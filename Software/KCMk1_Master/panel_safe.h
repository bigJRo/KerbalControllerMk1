/***************************************************************************************
   panel_safe.h — the safe-switch lines (design §8.6).

     INT_SAFE   (A2 Panel Hub, Teensy 31)   ABORT: rising edge → the ABORT action group
     B_INT_SAFE (B2 Panel Hub, Teensy 35)   STAGE: rising edge → STAGE, flight scene only,
                                            then a SAFE_STAGE_LOCKOUT_MS lockout
   Each button's arming switch is wired in series, so a press only reaches the master
   while armed. The lines are active-high plain inputs (10k / 20k divider on the hub),
   debounced SAFE_DEBOUNCE_MS. The level found at start-up is not an edge. INT_SAFE2 and
   B_INT_SAFE2 have nothing wired (Inventory §7) and are not read.
****************************************************************************************/
#pragma once
#include <Arduino.h>

static const uint32_t SAFE_DEBOUNCE_MS       = 10;
static const uint32_t SAFE_STAGE_LOCKOUT_MS  = 1000;

void safeLinesInit();
void safeLinesService();
bool safeLineLevel(bool stage);          // debounced level (false = ABORT, true = STAGE)
