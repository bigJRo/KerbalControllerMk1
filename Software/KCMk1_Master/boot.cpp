/***************************************************************************************
   boot.cpp — the boot sequence.

   Steps (design §5):
     1  Serial, Wire at 400 kHz, pins
     2  wait for the Sys Info Display (SYSINFO_PROBE_MS)
     3  banner: firmware version, reset cause (and where a watchdog reset happened)
     4  board devices: INA228, EMC2101
     5  INT map: stored (EEPROM) or default
     6  modules: identify, learn INT lines, LEDs off, DISABLE (module_bus); then the
        switch modules are read, so demo, the interlocks and the control mode are
        known before KSP is tried (switches)
     7  display carriers: PROCEED to each, wait for its status; learn their INT lines
     8  first Simpit handshake attempt
     9  start the watchdog (skipped, and the master halts, if the reboot-loop guard tripped)
****************************************************************************************/
#include "boot.h"
#include "config.h"
#include "state.h"
#include "debug.h"
#include "i2c_bus.h"
#include "board.h"
#include "carriers.h"
#include "simpit_link.h"
#include "module_bus.h"
#include "switches.h"
#include <stdarg.h>
#include <stdio.h>

static bool     g_halted;
static uint32_t g_waitStartMs, g_lastWaitLineMs;
static bool     g_hintShown;
static bool     g_connectedAtBoot;

bool bootHalted() { return g_halted; }

// shortText: what the Sys Info Display shows (nullptr = the same as Serial).
static void vline(BootSev sev, bool replace, const char *shortText, const char *fmt, va_list ap) {
  char text[SI_TEXT_MAX + 1];
  vsnprintf(text, sizeof text, fmt, ap);
  static const char *const tag[] = { "    ", " OK ", "WARN", "ERR " };
  conPrintf("[%6lu.%03lu] BOOT   %s %s", (unsigned long)(millis() / 1000), (unsigned long)(millis() % 1000),
            tag[sev & 3], text);
  siBootLine(sev, replace, shortText ? shortText : text);
}

void bootLine(BootSev sev, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt); vline(sev, false, nullptr, fmt, ap); va_end(ap);
}

void bootReplace(BootSev sev, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt); vline(sev, true, nullptr, fmt, ap); va_end(ap);
}

void bootLineS(BootSev sev, const char *shortText, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt); vline(sev, false, shortText, fmt, ap); va_end(ap);
}

void bootReplaceS(BootSev sev, const char *shortText, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt); vline(sev, true, shortText, fmt, ap); va_end(ap);
}

const char *bootFmt(const char *fmt, ...) {
  static char b[SI_TEXT_MAX + 1];
  va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
  return b;
}

static void stepConfig() {
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, HIGH);                   // displays run
  for (uint8_t i = 0; i < INT_LINE_COUNT; i++) pinMode(PIN_INT_LINE[i], INPUT);   // push-pull INT, no pull
  pinMode(PIN_SAFE_ABORT, INPUT);                // active-high safe lines: plain inputs
  pinMode(PIN_SAFE_A1, INPUT);
  pinMode(PIN_SAFE_STAGE, INPUT);
  pinMode(PIN_SAFE_B1, INPUT);
  pinMode(PIN_EXT_INT_1, INPUT);                 // expansion: reserved
  pinMode(PIN_EXT_INT_2, INPUT);
  busInit();
  const BusStats &st = busStats();
  if (st.recoveries) bootLineS(SEV_WARN, "I2C BUS RECOVERED", "I2C BUS WAS HELD LOW: RECOVERED");
  bootLineS(SEV_OK, bootFmt("I2C %lu kHz", (unsigned long)(I2C_CLOCK_HZ / 1000)),
            "CONFIG  I2C %lu kHz", (unsigned long)(I2C_CLOCK_HZ / 1000));
}

static void stepBanner() {
  const ResetInfo &r = boardResetInfo();
  bootLineS(SEV_INFO, bootFmt("MASTER v%s", KCM_MASTER_VERSION), "KCMk1 MASTER v%s", KCM_MASTER_VERSION);
  if (r.watchdog) {
    bootLineS(SEV_ERROR, bootFmt("RESET: WATCHDOG x%u", r.streak), "RESET: WATCHDOG (%u IN A ROW)", r.streak);
    if (r.crumbAddr) bootLineS(SEV_ERROR, bootFmt("HUNG IN %s @%02X", loopStageName(r.crumbStage), r.crumbAddr),
                               "HUNG IN %s @0x%02X", loopStageName(r.crumbStage), r.crumbAddr);
    else             bootLine(SEV_ERROR, "HUNG IN %s", loopStageName(r.crumbStage));
  } else {
    bootLine(SEV_INFO, "RESET: %s", boardResetCause());
  }
  dbgErr(DBG_BOARD, "boot: v%s, reset cause %s (SRSR %08lX)", KCM_MASTER_VERSION, boardResetCause(),
         (unsigned long)r.srsr);
}

static void stepDevices() {
  boardProbeDevices();
  if (boardHasPowerMon()) bootLineS(SEV_OK, "POWER MON OK", "POWER MON  INA228   OK");
  else                    bootLineS(SEV_ERROR, "POWER MON MISSING", "POWER MON  INA228   MISSING");
  if (boardHasFanCtrl())  bootLineS(SEV_OK, "FAN CTRL OK", "FAN CTRL   EMC2101  OK");
  else                    bootLineS(SEV_ERROR, "FAN CTRL MISSING", "FAN CTRL   EMC2101  MISSING");
}

static void stepSimpit() {
  if (g_master.demo) { bootLineS(SEV_INFO, "KSP: DEMO", "KSP: NOT CONTACTED (DEMO)"); return; }
  bootLineS(SEV_INFO, "KSP CONNECTING", "KSP: CONNECTING ...");
  g_connectedAtBoot = simpitAttempt();
  if (g_connectedAtBoot) bootReplaceS(SEV_OK, "KSP CONNECTED", "KSP: CONNECTED");
  else                   bootReplaceS(SEV_WARN, "KSP NOT RUNNING", "KSP: NOT RUNNING, RETRYING");
}

void bootSequence() {
  g_halted = false;
  g_hintShown = false;
  g_connectedAtBoot = false;
  wdtCrumb(LS_SETUP);
  stepConfig();                                                     // 1
  if (siProbeBoot(SYSINFO_PROBE_MS)) dbg(DBG_DISPLAY, "Sys Info Display answered");   // 2
  else dbgErr(DBG_DISPLAY, "Sys Info Display not answering; boot report on Serial only");
  stepBanner();                                                     // 3
  stepDevices();                                                    // 4
  moduleBusBoot();                                                  // 5, 6
  moduleBusReadSwitches();
  switchesBoot();
  carriersBoot();                                                   // 7
  moduleBusLearnCarriers();
  const ResetInfo &r = boardResetInfo();
  if (r.guardTripped) {
    bootLineS(SEV_ERROR, bootFmt("HALTED: %u WDT RESETS", r.streak),
              "%u WATCHDOG RESETS IN %lu S: HALTED", r.streak, (unsigned long)WDT_LOOP_GUARD_S);
    bootLineS(SEV_ERROR, "FIX, THEN POWER CYCLE", "FIX THE FAULT, THEN POWER CYCLE");
    dbgErr(DBG_BOARD, "reboot-loop guard tripped: halted, watchdog not started");
    g_halted = true;
    g_master.bootPhase = BOOT_PHASE_HALT;
    return;
  }
  stepSimpit();                                                     // 8
  wdtStart();                                                       // 9
  bootLineS(SEV_OK, bootFmt("WATCHDOG %u S", (unsigned)WDT_TIMEOUT_S), "WATCHDOG ON  %u S", (unsigned)WDT_TIMEOUT_S);
  g_master.setupDoneMs = millis();
  g_waitStartMs = g_lastWaitLineMs = millis();
  g_master.bootPhase = BOOT_PHASE_WAIT_KSP;
  if (!g_master.kspConnected && !g_master.demo) bootLineS(SEV_INFO, "KSP WAIT 0 S", "WAITING FOR KSP  0 S");
}

void bootService() {
  if (g_master.bootPhase != BOOT_PHASE_WAIT_KSP) return;
  uint32_t now = millis();
  if (g_master.kspConnected || g_master.demo) {
    if (!g_master.kspConnected) bootLine(SEV_OK, "DEMO MODE");
    else if (!g_connectedAtBoot) bootLineS(SEV_OK, "KSP CONNECTED", "KSP: CONNECTED");
    siBootDone();
    g_master.bootPhase = BOOT_PHASE_RUN;
    dbg(DBG_BOARD, "boot screen released (%s)", g_master.kspConnected ? "KSP connected" : "demo");
    return;
  }
  if (now - g_lastWaitLineMs < BOOT_WAIT_UPDATE_MS) return;
  g_lastWaitLineMs = now;
  uint32_t s = (now - g_waitStartMs) / 1000;
  if (!g_hintShown && now - g_waitStartMs >= BOOT_KSP_HINT_MS) {
    g_hintShown = true;
    bootLineS(SEV_WARN, "CHECK KSP, SIMPIT, USB", "KSP RUNNING? SIMPIT COM PORT? USB?");
    bootLineS(SEV_INFO, bootFmt("KSP WAIT %lu S", (unsigned long)s),
              "WAITING FOR KSP  %lu S  TRY %lu", (unsigned long)s, (unsigned long)simpitAttempts());
    return;
  }
  bootReplaceS(SEV_INFO, bootFmt("KSP WAIT %lu S", (unsigned long)s),
               "WAITING FOR KSP  %lu S  TRY %lu", (unsigned long)s, (unsigned long)simpitAttempts());
}
