/***************************************************************************************
   board.cpp — reset cause, watchdog and breadcrumb, on-board devices: power monitor
   (INA228) and fan controller (EMC2101), and the Sys Info Display's status frame.

   Retained state lives in the four SNVS_LPGPR registers (i.MX RT1062 SNVS, 0x400D4100),
   which keep their contents through a watchdog or software reset while the board stays
   powered:
     R0  magic 0x4B43 << 16 | streak << 8
     R1  breadcrumb: stage << 8 | I2C address (written by the watchdog warning)
     R2  RTC seconds when the current watchdog streak started
   Bench check: that these registers are untouched by the Teensy core and survive a
   WDOG1 reset (design §18).
****************************************************************************************/
#include "board.h"
#include "config.h"
#include "debug.h"
#include "i2c_bus.h"
#include "sysinfo.h"
#include <Watchdog_t4.h>

#if defined(KCM_HOST)
#define RETAIN(i) (stubRetain()[i])   // tools/host/Arduino.h: retained registers, SRC_SRSR, rtc_get()
#else
static volatile uint32_t *const kRetain = (volatile uint32_t *)0x400D4100;   // SNVS_LPGPR0..3
#define RETAIN(i) (kRetain[i])
#endif

static const uint32_t RETAIN_MAGIC = 0x4B43u;
// SRC_SRSR bits (i.MX RT1060 RM, SRC chapter); write 1 to clear.
static const uint32_t SRSR_POR      = 1u << 0;
static const uint32_t SRSR_LOCKUP   = 1u << 1;   // includes a software reset (SCB_AIRCR)
static const uint32_t SRSR_USER     = 1u << 3;
static const uint32_t SRSR_WDOG     = 1u << 4;
static const uint32_t SRSR_JTAG     = (1u << 5) | (1u << 6);
static const uint32_t SRSR_WDOG3    = 1u << 7;
static const uint32_t SRSR_TEMP     = 1u << 8;

static WDT_T4<WDT1> g_wdt;
static bool      g_wdtRunning;
static ResetInfo g_reset;
static uint8_t   g_crumbStage, g_crumbAddr;
static bool      g_hasPower, g_hasFan;
static bool      g_streakCleared;

static const char *const kStageName[LS_COUNT] = {
  "unknown (interrupts off?)", "setup", "simpitService", "safeLinesService", "moduleBusService",
  "switches / interlocks", "autopilotsService", "panels / frameService", "moduleBusFlushLeds", "carrierService",
  "boot / siService", "boardService", "dbgService"
};

const char *loopStageName(uint8_t s) { return s < LS_COUNT ? kStageName[s] : "?"; }
const ResetInfo &boardResetInfo() { return g_reset; }
bool wdtRunning() { return g_wdtRunning; }
bool boardHasPowerMon() { return g_hasPower; }
bool boardHasFanCtrl() { return g_hasFan; }

const char *boardResetCause() {
  uint32_t s = g_reset.srsr;
  if (s & (SRSR_WDOG | SRSR_WDOG3)) return "WATCHDOG";
  if (s & SRSR_TEMP)   return "OVER TEMPERATURE";
  if (s & SRSR_JTAG)   return "DEBUGGER";
  if (s & SRSR_USER)   return "RESET BUTTON";
  if (s & SRSR_LOCKUP) return "SOFTWARE";
  if (s & SRSR_POR)    return "POWER ON";
  return "UNKNOWN";
}

void boardEarly() {
  memset(&g_reset, 0, sizeof g_reset);
  g_wdtRunning = false;
  g_streakCleared = false;
  g_reset.srsr = SRC_SRSR;
  SRC_SRSR = g_reset.srsr;                     // clear what was read (write 1 to clear)
  g_reset.watchdog = (g_reset.srsr & (SRSR_WDOG | SRSR_WDOG3)) != 0;

  bool valid = (RETAIN(0) >> 16) == RETAIN_MAGIC;
  uint8_t streak = valid ? (uint8_t)((RETAIN(0) >> 8) & 0xFF) : 0;
  uint32_t start = valid ? RETAIN(2) : 0;
  if (g_reset.watchdog && valid) {
    g_reset.crumbStage = (uint8_t)((RETAIN(1) >> 8) & 0xFF);
    g_reset.crumbAddr  = (uint8_t)(RETAIN(1) & 0xFF);
  }
  uint32_t now = rtc_get();
  if (g_reset.watchdog) {
    if (streak == 0 || now - start > WDT_LOOP_GUARD_S) { streak = 1; start = now; }
    else if (streak < 255) streak++;
  } else {
    streak = 0;
  }
  g_reset.streak = streak;
  g_reset.guardTripped = streak >= WDT_LOOP_GUARD_COUNT;
  RETAIN(0) = (RETAIN_MAGIC << 16) | ((uint32_t)streak << 8);
  RETAIN(1) = 0;
  RETAIN(2) = start;
}

// The breadcrumb is kept in ordinary RAM (cheap, written several times a pass) and
// copied to the retained register by the warning interrupt, 0.5 s before the reset.
// SNVS low-power registers are slow to write, so they are not written every pass. A
// hang with interrupts off never runs the warning: the next boot then reports the
// stage as unknown.
void wdtCrumb(LoopStage s, uint8_t addr) {
  g_crumbStage = s; g_crumbAddr = addr;
}

// Runs from the WDOG1 interrupt 0.5 s before the reset. If the hang is inside a Serial
// write, the log line may not get out; the breadcrumb and the reset happen regardless.
static void wdtWarning() {
  RETAIN(1) = ((uint32_t)g_crumbStage << 8) | g_crumbAddr;
  uint32_t warnMs = (uint32_t)(WDT_WARN_BEFORE_S * 1000.0f);
  dbgErr(DBG_BOARD, "watchdog: no feed, reset in %lu ms (in %s)", (unsigned long)warnMs,
         loopStageName(g_crumbStage));
}

void wdtStart() {
  WDT_timings_t cfg;
  cfg.timeout  = WDT_TIMEOUT_S;
  cfg.trigger  = WDT_WARN_BEFORE_S;            // WDT_T4 / WDOG1 WICT: callback this long before the reset (0.5 s steps)
  cfg.callback = wdtWarning;
  g_wdt.begin(cfg);
  g_wdtRunning = true;
}

void wdtFeed() {
  if (g_wdtRunning) g_wdt.feed();
}

static void powerService(uint32_t now);
static void fanService(uint32_t now);

void boardService() {
  uint32_t now = millis();
  powerService(now);
  fanService(now);
  // A clean run of WDT_STREAK_CLEAR_MS since boot ends a watchdog streak.
  if (!g_streakCleared && g_wdtRunning && millis() >= WDT_STREAK_CLEAR_MS) {
    g_streakCleared = true;
    if (((RETAIN(0) >> 8) & 0xFF) != 0) {
      RETAIN(0) = RETAIN_MAGIC << 16;
      dbg(DBG_BOARD, "watchdog streak cleared after a clean run");
    }
  }
}

/***************************************************************************************
   Devices. Presence is checked by ID registers, so a wrong part at the address shows:
     INA228  0x3E MANUFACTURER_ID = 0x5449 ("TI"), 0x3F DEVICE_ID[15:4] = 0x228
     EMC2101 0xFE manufacturer = 0x5D, 0xFD product = 0x16 (EMC2101) or 0x28 (EMC2101-R)
****************************************************************************************/
static bool probeIna228() {
  uint8_t b[2];
  if (busReadReg(ADDR_INA228, 0x3E, b, 2) != BUS_OK) return false;
  if (((uint16_t)b[0] << 8 | b[1]) != 0x5449) return false;
  if (busReadReg(ADDR_INA228, 0x3F, b, 2) != BUS_OK) return false;
  return (((uint16_t)b[0] << 8 | b[1]) >> 4) == 0x228;
}

static bool probeEmc2101() {
  uint8_t m, p;
  if (busReadReg(ADDR_EMC2101, 0xFE, &m, 1) != BUS_OK || m != 0x5D) return false;
  if (busReadReg(ADDR_EMC2101, 0xFD, &p, 1) != BUS_OK) return false;
  return p == 0x16 || p == 0x28;
}

static bool inaSetup();
static bool emcSetup();

bool boardProbeDevices() {
  g_hasPower = probeIna228() && inaSetup();
  g_hasFan   = probeEmc2101() && emcSetup();
  return g_hasPower && g_hasFan;
}

/***************************************************************************************
   Power monitor — INA228 on the 12 V rail (design §12.1). 15 mΩ shunt (KC-01-1701;
   bench check), current LSB = 10 A / 2^19, so SHUNT_CAL = 13107.2e6 × LSB × R = 3750.
     0x02 SHUNT_CAL   0x05 VBUS   20 bits in 23:4, 195.3125 µV per LSB
     0x07 CURRENT     20 bits signed in 23:4, × LSB    0x08 POWER  24 bits × 3.2 × LSB
   Read 4 times a second; a warning below POWER_LOW_V.
****************************************************************************************/
static const float    INA_CURRENT_LSB = 10.0f / 524288.0f;
static const uint16_t INA_SHUNT_CAL   = 3750;
static const uint32_t POWER_MS        = 250;
static const float    POWER_LOW_V     = 11.0f;

static BoardStatus g_bs;
static uint32_t g_lastPowerMs, g_lastFanMs;
static bool     g_lowWarned;
static uint8_t  g_fanLevel;

const BoardStatus &boardStatus() { return g_bs; }

static bool inaSetup() {
  uint8_t b[3] = { 0x02, (uint8_t)(INA_SHUNT_CAL >> 8), (uint8_t)(INA_SHUNT_CAL & 0xFF) };
  return busWrite(ADDR_INA228, b, 3) == BUS_OK;
}

static bool read24(uint8_t reg, uint32_t *v) {
  uint8_t b[3];
  if (busReadReg(ADDR_INA228, reg, b, 3) != BUS_OK) return false;
  *v = (uint32_t)b[0] << 16 | (uint32_t)b[1] << 8 | b[2];
  return true;
}

static void pushStatus() {
  SiStatus s = {};
  s.powerValid = g_bs.powerValid; s.tempValid = g_bs.tempValid; s.fanValid = g_bs.fanValid;
  s.busV = g_bs.busV; s.currentA = g_bs.currentA; s.powerW = g_bs.powerW;
  s.tempC = g_bs.tempC; s.fanRpm = g_bs.fanRpm; s.fanDuty = g_bs.fanDuty;
  siSetStatus(s);
}

static void powerService(uint32_t now) {
  if (!g_hasPower || now - g_lastPowerMs < POWER_MS) return;
  g_lastPowerMs = now;
  uint32_t v, c, p;
  bool ok = read24(0x05, &v) && read24(0x07, &c) && read24(0x08, &p);
  g_bs.powerValid = ok;
  if (ok) {
    int32_t ci = (int32_t)(c << 8) >> 12;          // sign-extend bit 23, drop the 4 reserved bits
    g_bs.busV     = (float)(v >> 4) * 195.3125e-6f;
    g_bs.currentA = (float)ci * INA_CURRENT_LSB;
    g_bs.powerW   = (float)p * 3.2f * INA_CURRENT_LSB;
    bool low = g_bs.busV < POWER_LOW_V;
    if (low && !g_lowWarned) dbgErr(DBG_BOARD, "supply low: %s V", fstr(g_bs.busV, 2));
    if (!low && g_lowWarned) dbg(DBG_BOARD, "supply back to %s V", fstr(g_bs.busV, 2));
    g_lowWarned = low;
  }
  pushStatus();
}

/***************************************************************************************
   Fan — EMC2101 (design §12.1). Its INTERNAL sensor is the board temperature (nothing
   is fitted on H4, so the remote channel is unused). Direct fan setting, LUT off:
     0x00 internal temperature (°C, signed)   0x03 CONFIG bit 2 = TACH input on
     0x46 / 0x47 TACH low / high: rpm = 5 400 000 / count (0xFFFF: stopped / none)
     0x4A FAN_CONFIG bit 5 = direct setting   0x4C FAN_SETTING 0..63 (duty × 63 / 100)
     0x4D PWM_F = 0x1F
   Duty 30 / 40 / 50 / 70 / 100 % from 25 / 30 / 40 / 50 °C, 2 °C hysteresis on the way
   down, once a second. Bench check: duty and rpm on the Ctrl Module fan.
****************************************************************************************/
static const uint32_t FAN_MS = 1000;
static const int8_t   FAN_HYST_C = 2;
static const int8_t   kFanTemp[]  = { -128, 25, 30, 40, 50 };   // level n from this temperature
static const uint8_t  kFanDuty[]  = { 30, 40, 50, 70, 100 };
static const uint8_t  FAN_LEVELS  = sizeof kFanDuty;

static bool emcWrite(uint8_t reg, uint8_t v) { uint8_t b[2] = { reg, v }; return busWrite(ADDR_EMC2101, b, 2) == BUS_OK; }

static bool emcSetup() {
  uint8_t cfg, fc;
  if (busReadReg(ADDR_EMC2101, 0x03, &cfg, 1) != BUS_OK || busReadReg(ADDR_EMC2101, 0x4A, &fc, 1) != BUS_OK) return false;
  return emcWrite(0x03, cfg | 0x04) && emcWrite(0x4A, fc | 0x20) && emcWrite(0x4D, 0x1F) &&
         emcWrite(0x4C, (uint8_t)(kFanDuty[0] * 63 / 100));
}

static uint8_t fanLevelFor(int t) {
  uint8_t l = 0;
  while (l + 1 < FAN_LEVELS && t >= kFanTemp[l + 1]) l++;
  return l;
}

static void fanService(uint32_t now) {
  if (!g_hasFan || now - g_lastFanMs < FAN_MS) return;
  g_lastFanMs = now;
  uint8_t t, lo, hi;
  g_bs.tempValid = busReadReg(ADDR_EMC2101, 0x00, &t, 1) == BUS_OK;
  if (g_bs.tempValid) {
    g_bs.tempC = (int8_t)t;
    uint8_t up = fanLevelFor(g_bs.tempC), down = fanLevelFor(g_bs.tempC + FAN_HYST_C);
    uint8_t lvl = up > g_fanLevel ? up : down < g_fanLevel ? down : g_fanLevel;
    if (lvl != g_fanLevel) {
      dbg(DBG_BOARD, "fan %u %% (%d C)", kFanDuty[lvl], g_bs.tempC);
      g_fanLevel = lvl;
    }
    emcWrite(0x4C, (uint8_t)(kFanDuty[g_fanLevel] * 63 / 100));
  }
  g_bs.fanDuty = kFanDuty[g_fanLevel];
  g_bs.fanValid = busReadReg(ADDR_EMC2101, 0x46, &lo, 1) == BUS_OK && busReadReg(ADDR_EMC2101, 0x47, &hi, 1) == BUS_OK;
  if (g_bs.fanValid) {
    uint16_t count = (uint16_t)(hi << 8 | lo);
    g_bs.fanRpm = (count == 0 || count == 0xFFFF) ? 0 : (uint16_t)(5400000UL / count);
  }
  pushStatus();
}

static void cmdBoard(const char *) {
  if (g_hasPower) conPrintf("power: %s V  %s A  %s W%s", fstr(g_bs.busV, 2), fstr(g_bs.currentA, 2), fstr(g_bs.powerW, 1),
                            g_bs.powerValid ? "" : " (read failed)");
  else            conPrintf("power: no INA228");
  if (g_hasFan) conPrintf("fan: board %d C, duty %u %%, %u rpm%s", g_bs.tempC, g_bs.fanDuty, g_bs.fanRpm,
                          g_bs.tempValid ? "" : " (read failed)");
  else          conPrintf("fan: no EMC2101");
}

void boardServicesInit() {
  memset(&g_bs, 0, sizeof g_bs);
  g_lastPowerMs = g_lastFanMs = millis();
  g_lowWarned = false;
  g_fanLevel = 0;
  consoleRegister("board", "12 V supply, board temperature, fan", cmdBoard);
}

void boardDisplayReset() {
  digitalWrite(PIN_RST, LOW);
  delay(10);
  digitalWrite(PIN_RST, HIGH);
  dbg(DBG_BOARD, "display reset pulse");
}
