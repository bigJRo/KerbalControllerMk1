/***************************************************************************************
   board.h — the Ctrl Module's own hardware: reset cause, watchdog, on-board devices,
   display reset (design §12).

   Watchdog (§12.3): Teensy 4.1 WDOG1 through WDT_T4, 5 s timeout, a warning callback
   0.5 s before the reset. Off during setup() (a stalled boot stays on the Sys Info
   screen), started as the last boot step, fed once per loop pass (wdtFeed()).
   A breadcrumb (which loop call is running, and the module address it is talking to)
   is copied by the watchdog warning into an SNVS general-purpose register, which
   survives a watchdog reset, so the next boot can say where the master hung. Three
   watchdog resets within 60 s trip the reboot-loop guard: the master stops on the boot
   screen and does not start the watchdog, so the fault stays visible.
****************************************************************************************/
#pragma once
#include <Arduino.h>

// Loop stages for the breadcrumb (design §4.2 order).
enum LoopStage : uint8_t {
  LS_NONE = 0, LS_SETUP, LS_SIMPIT, LS_BOARD_FAST, LS_MODULES, LS_INTERLOCKS, LS_AUTOPILOT,
  LS_KSP_OUT, LS_LEDS, LS_CARRIERS, LS_SYSINFO, LS_BOARD_SLOW, LS_DEBUG, LS_COUNT
};

struct ResetInfo {
  uint32_t srsr;            // SRC_SRSR as read at boot
  bool     watchdog;        // this boot follows a watchdog reset
  uint8_t  crumbStage;      // LoopStage running when it fired
  uint8_t  crumbAddr;       // I2C address in use then (0 = none)
  uint8_t  streak;          // consecutive watchdog resets within the guard window
  bool     guardTripped;    // WDT_LOOP_GUARD_COUNT reached
};

void        boardEarly();                    // first thing in setup(): reset cause, breadcrumb, streak
const ResetInfo &boardResetInfo();
const char *boardResetCause();               // "POWER ON", "WATCHDOG", "SOFTWARE", …
const char *loopStageName(uint8_t s);

void        wdtStart();
void        wdtFeed();
bool        wdtRunning();
void        wdtCrumb(LoopStage s, uint8_t addr = 0);   // record where the loop is

bool        boardProbeDevices();             // boot step 4: INA228, EMC2101 present?
bool        boardHasPowerMon();
bool        boardHasFanCtrl();
void        boardDisplayReset();             // 10 ms low pulse on RST (7" displays)
void        boardService();                  // power (4 Hz), fan (1 Hz), the watchdog streak

// Power monitor and fan (design §12.1), as also sent to the Sys Info Display.
struct BoardStatus {
  bool     powerValid, tempValid, fanValid;
  float    busV, currentA, powerW;
  int8_t   tempC;                            // EMC2101 internal sensor (the board)
  uint8_t  fanDuty;                          // %
  uint16_t fanRpm;
};
void        boardServicesInit();             // before boardProbeDevices()
const BoardStatus &boardStatus();
