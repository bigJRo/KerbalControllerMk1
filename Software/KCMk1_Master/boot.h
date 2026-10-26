/***************************************************************************************
   boot.h — start-up sequence and boot reporting (design §5).

   bootSequence() runs the setup() steps in order. Each step reports a line to the Sys
   Info Display and to Serial, and each has its own timeout: a step that times out is
   reported in red and start-up carries on.

   After setup() the controller runs normally, but the Sys Info Display stays on its
   boot screen, with a WAITING FOR KSP line, until the Simpit link connects or demo is
   switched on (bootService()). If the watchdog reboot-loop guard has tripped, the
   master halts instead: only the console and the Sys Info heartbeat run (bootHalted()).
****************************************************************************************/
#pragma once
#include <Arduino.h>
#include "sysinfo.h"

void bootSequence();                                     // the whole of setup() after boardEarly()
void bootService();                                      // in loop(): WAITING FOR KSP, release
bool bootHalted();
void bootLine(BootSev sev, const char *fmt, ...);        // a new boot line
void bootReplace(BootSev sev, const char *fmt, ...);     // overwrite the previous boot line
