/***************************************************************************************
   KCMk1_Master — Kerbal Controller Mk1 master firmware (Teensy 4.1, Ctrl Module
   KC-01-1701 V2.0).

   Design: Documents/Developer/Master_Firmware_Design.md
   Hardware: Documents/Developer/Hardware_Inventory.md

   Board settings: Teensy 4.1, USB Type "Dual Serial" (Serial = debug console,
   SerialUSB1 = KerbalSimpit), CPU 600 MHz.
   Libraries: KerbalSimpit 2.4.0, WDT_T4 and EEPROM (Teensyduino), KerbalModuleCommon
   (Software/Modules/libraries).

   This tab holds setup() and loop() only. Each part of the firmware is its own
   .cpp / .h pair; nothing relies on the IDE's prototype generation.

   Stage 1 (skeleton and boot): state, I2C, boot sequence and the Sys Info Display link,
   the four display carriers, the master's Simpit link, the KSP send gate, the debug log
   and console, the watchdog.
   Stage 2 (module bus): module table, INT line learning, start-up, reads and events,
   LEDs, lifecycle.
   Stage 3 (switches and interlocks): Switch Groups 1 and 2, SWITCH ERR, ENGINE SAFE /
   ARM, A/P INHIB / ARM, the mode selector, ABORT and STAGE, demo and debug.
   Stage 4 (flight panels): Vehicle, Stability (SAS modes, HOLD, INVERT), Action, AUX,
   Dual Encoder; the CAG numbering and keyboard events.
   Stage 5 (sticks and throttle): the per-frame control channels and the autopilot
   interface (ksp_frame), stick mapping, trim, camera, EVA keys, the lever and its
   buttons.
   Stage 6 (remaining panels): UI Control, Function Control, Time Control with the
   Pre-Warp lead, the GPWS relay.
   Stage 7 (autopilots): ascent, hold, burn and landing ported from Controller_Main, the
   Info Display 2 consoles, the A/P INHIB / ENGINE SAFE / disconnect rules, HOLD in PLN
   and RVR.
   Stage 8 (board): the 12 V power monitor, the fan from the board temperature, the Sys
   Info Display's status screen.

   Licensed under the GNU General Public License v3.0 (GPL-3.0).
   Written for Jeb's Controller Works.
****************************************************************************************/
#include "config.h"
#include "state.h"
#include "debug.h"
#include "i2c_bus.h"
#include "board.h"
#include "boot.h"
#include "sysinfo.h"
#include "carriers.h"
#include "simpit_link.h"
#include "ksp_out.h"
#include "module_bus.h"
#include "switches.h"
#include "interlocks.h"
#include "panel_safe.h"
#include "panels.h"
#include "ksp_frame.h"
#include "autopilots.h"
#include <strings.h>

static void cmdStatus(const char *) {
  static const char *const phase[] = { "setup", "waiting for KSP", "running", "HALTED" };
  conPrintf("KCMk1 master v%s, up %lu s, %s", KCM_MASTER_VERSION, (unsigned long)(millis() / 1000),
            phase[g_master.bootPhase & 3]);
  conPrintf("  reset %s, watchdog %s", boardResetCause(), wdtRunning() ? "on" : "off");
  conPrintf("  control mode %s, ENGINE %s, A/P %s, SWITCH ERR %s",
            g_panel.ctrlMode == CTRL_MODE_SPC ? "SPC" : g_panel.ctrlMode == CTRL_MODE_PLN ? "PLN" : "RVR",
            interlockEngineState() == ENG_ARMED ? "ARMED" : interlockEngineState() == ENG_ARMING ? "ARMING" : "SAFE",
            g_panel.apArm ? "ARM" : "INHIB", g_panel.switchErr ? "on" : "off");
  conPrintf("  demo %d, debug %d, KSP %s%s, displays known %02X links %02X, Sys Info %s",
            g_master.demo, g_master.debug, g_master.kspConnected ? "connected" : "down",
            g_master.simpitLost ? " (LOST)" : "", g_master.displaysKnown, g_master.displayLinks,
            g_master.sysInfoPresent ? "present" : "absent");
  conPrintf("  mode %u, group %u, annunciator flags %04X", g_panel.ctrlMode, g_panel.ctrlGrp,
            carrierModeFlags());
  const BusStats &b = busStats();
  conPrintf("  I2C: %lu writes, %lu reads, %lu no answer, %lu NACK, %lu short, %lu other, %lu recoveries",
            (unsigned long)b.writes, (unsigned long)b.reads, (unsigned long)b.nackAddr,
            (unsigned long)b.nackData, (unsigned long)b.shortReads, (unsigned long)b.other,
            (unsigned long)b.recoveries);
  conPrintf("  KSP sends: %lu sent, %lu held back; debug lines dropped %lu", (unsigned long)kspSentCount(),
            (unsigned long)kspSuppressed(), (unsigned long)dbgDropped());
}

// Bench: `debug on|off` without the UPTLM switch; the next UPTLM flip takes over again.
static void cmdDebug(const char *args) {
  while (*args == ' ') args++;
  if (strncasecmp(args, "on", 2) == 0) g_master.debug = true;
  else if (strncasecmp(args, "off", 3) == 0) g_master.debug = false;
  conPrintf("debug %s", g_master.debug ? "on" : "off");
}

// Bench: hang the loop so the watchdog fires (design §18, bench check 5). Needs "yes".
static void cmdWdTest(const char *args) {
  while (*args == ' ') args++;
  if (strncasecmp(args, "yes", 3) != 0) { conPrintf("wdtest yes: hang the loop; the watchdog resets in 5 s"); return; }
  if (!wdtRunning()) { conPrintf("watchdog not running"); return; }
  conPrintf("hanging in dbgService ...");
  wdtCrumb(LS_DEBUG);
  for (;;) { asm volatile(""); }   // asm: the compiler may not assume the loop ends
}

void setup() {
  stateReset();
  Serial.begin(115200);
  boardEarly();                     // reset cause and breadcrumb, before anything overwrites them
  dbgInit();
  boardServicesInit();
  consoleRegister("status", "modes, links, bus and send counters", cmdStatus);
  consoleRegister("debug", "debug on|off (until the next UPTLM flip)", cmdDebug);
  consoleRegister("wdtest", "wdtest yes — hang the loop to test the watchdog", cmdWdTest);
  siInit();
  carriersInit();
  moduleBusInit();
  interlocksInit();
  switchesInit();
  panelsInit();
  frameInit();
  autopilotsInit();
  simpitInit();
  bootSequence();
  safeLinesInit();                  // after boot step 1 set the pins up; the level now is no edge
}

void loop() {
  uint32_t t0 = micros();
  wdtFeed();
  if (bootHalted()) {               // reboot-loop guard: console and Sys Info heartbeat only
    wdtCrumb(LS_DEBUG);   dbgService();
    wdtCrumb(LS_SYSINFO); siService();
    return;
  }
  wdtCrumb(LS_SIMPIT);     simpitService();
  wdtCrumb(LS_BOARD_FAST); safeLinesService();
  wdtCrumb(LS_MODULES);    moduleBusService();
  wdtCrumb(LS_INTERLOCKS); switchesService(); interlockService();
  wdtCrumb(LS_AUTOPILOT);  autopilotsService();
  wdtCrumb(LS_KSP_OUT);    panelsService(); frameService();
  wdtCrumb(LS_LEDS);       moduleBusFlushLeds();
  wdtCrumb(LS_CARRIERS);   carrierService();
  wdtCrumb(LS_SYSINFO);    bootService(); siService();
  wdtCrumb(LS_BOARD_SLOW); boardService();
  wdtCrumb(LS_DEBUG);      dbgService();
  dbgLoopTime(micros() - t0);
}
