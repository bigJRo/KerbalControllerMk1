/***************************************************************************************
   board.cpp — reset cause, watchdog and breadcrumb, on-board device probes.

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
  "unknown (interrupts off?)", "setup", "simpitService", "boardServiceFast", "moduleBusService", "interlockService",
  "autopilotService", "kspOutService", "moduleBusFlushLeds", "carrierService", "sysInfoService",
  "boardServiceSlow", "dbgService"
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

void boardService() {
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

bool boardProbeDevices() {
  g_hasPower = probeIna228();
  g_hasFan   = probeEmc2101();
  return g_hasPower && g_hasFan;
}

void boardDisplayReset() {
  digitalWrite(PIN_RST, LOW);
  delay(10);
  digitalWrite(PIN_RST, HIGH);
  dbg(DBG_BOARD, "display reset pulse");
}
