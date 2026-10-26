/***************************************************************************************
   boot.cpp — the boot sequence.

   Steps (design §5). The module steps (INT map, discovery) arrive with stage 2.
     1  Serial, Wire at 400 kHz, pins
     2  wait for the Sys Info Display (SYSINFO_PROBE_MS)
     3  banner: firmware version, reset cause (and where a watchdog reset happened)
     4  board devices: INA228, EMC2101
     7  display carriers: PROCEED to each, wait for its status
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
#include <stdarg.h>
#include <stdio.h>

static bool     g_halted;
static uint32_t g_waitStartMs, g_lastWaitLineMs;
static bool     g_hintShown;
static bool     g_connectedAtBoot;

bool bootHalted() { return g_halted; }

static void vline(BootSev sev, bool replace, const char *fmt, va_list ap) {
  char text[SI_TEXT_MAX + 1];
  vsnprintf(text, sizeof text, fmt, ap);
  static const char *const tag[] = { "    ", " OK ", "WARN", "ERR " };
  conPrintf("[%6lu.%03lu] BOOT   %s %s", (unsigned long)(millis() / 1000), (unsigned long)(millis() % 1000),
            tag[sev & 3], text);
  siBootLine(sev, replace, text);
}

void bootLine(BootSev sev, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt); vline(sev, false, fmt, ap); va_end(ap);
}

void bootReplace(BootSev sev, const char *fmt, ...) {
  va_list ap; va_start(ap, fmt); vline(sev, true, fmt, ap); va_end(ap);
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
  if (st.recoveries) bootLine(SEV_WARN, "I2C BUS WAS HELD LOW: RECOVERED");
  bootLine(SEV_OK, "CONFIG  I2C %lu kHz", (unsigned long)(I2C_CLOCK_HZ / 1000));
}

static void stepBanner() {
  const ResetInfo &r = boardResetInfo();
  bootLine(SEV_INFO, "KCMk1 MASTER v%s", KCM_MASTER_VERSION);
  if (r.watchdog) {
    bootLine(SEV_ERROR, "RESET: WATCHDOG (%u IN A ROW)", r.streak);
    if (r.crumbAddr) bootLine(SEV_ERROR, "HUNG IN %s @0x%02X", loopStageName(r.crumbStage), r.crumbAddr);
    else             bootLine(SEV_ERROR, "HUNG IN %s", loopStageName(r.crumbStage));
  } else {
    bootLine(SEV_INFO, "RESET: %s", boardResetCause());
  }
  dbgErr(DBG_BOARD, "boot: v%s, reset cause %s (SRSR %08lX)", KCM_MASTER_VERSION, boardResetCause(),
         (unsigned long)r.srsr);
}

static void stepDevices() {
  boardProbeDevices();
  if (boardHasPowerMon()) bootLine(SEV_OK, "POWER MON  INA228   OK");
  else                    bootLine(SEV_ERROR, "POWER MON  INA228   MISSING");
  if (boardHasFanCtrl())  bootLine(SEV_OK, "FAN CTRL   EMC2101  OK");
  else                    bootLine(SEV_ERROR, "FAN CTRL   EMC2101  MISSING");
}

static void stepSimpit() {
  if (g_master.demo) { bootLine(SEV_INFO, "KSP: NOT CONTACTED (DEMO)"); return; }
  bootLine(SEV_INFO, "KSP: CONNECTING ...");
  g_connectedAtBoot = simpitAttempt();
  if (g_connectedAtBoot) bootReplace(SEV_OK, "KSP: CONNECTED");
  else                   bootReplace(SEV_WARN, "KSP: NOT RUNNING, RETRYING");
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
  carriersBoot();                                                   // 7
  const ResetInfo &r = boardResetInfo();
  if (r.guardTripped) {
    bootLine(SEV_ERROR, "%u WATCHDOG RESETS IN %lu S: HALTED", r.streak, (unsigned long)WDT_LOOP_GUARD_S);
    bootLine(SEV_ERROR, "FIX THE FAULT, THEN POWER CYCLE");
    dbgErr(DBG_BOARD, "reboot-loop guard tripped: halted, watchdog not started");
    g_halted = true;
    g_master.bootPhase = BOOT_PHASE_HALT;
    return;
  }
  stepSimpit();                                                     // 8
  wdtStart();                                                       // 9
  bootLine(SEV_OK, "WATCHDOG ON  %u S", (unsigned)WDT_TIMEOUT_S);
  g_master.setupDoneMs = millis();
  g_waitStartMs = g_lastWaitLineMs = millis();
  g_master.bootPhase = BOOT_PHASE_WAIT_KSP;
  if (!g_master.kspConnected && !g_master.demo) bootLine(SEV_INFO, "WAITING FOR KSP  0 S");
}

void bootService() {
  if (g_master.bootPhase != BOOT_PHASE_WAIT_KSP) return;
  uint32_t now = millis();
  if (g_master.kspConnected || g_master.demo) {
    if (!g_master.kspConnected) bootLine(SEV_OK, "DEMO MODE");
    else if (!g_connectedAtBoot) bootLine(SEV_OK, "KSP: CONNECTED");
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
    bootLine(SEV_WARN, "KSP RUNNING? SIMPIT COM PORT? USB?");
    bootLine(SEV_INFO, "WAITING FOR KSP  %lu S  TRY %lu", (unsigned long)s, (unsigned long)simpitAttempts());
    return;
  }
  bootReplace(SEV_INFO, "WAITING FOR KSP  %lu S  TRY %lu", (unsigned long)s, (unsigned long)simpitAttempts());
}
