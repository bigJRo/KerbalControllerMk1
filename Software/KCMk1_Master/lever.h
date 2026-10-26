/***************************************************************************************
   lever.h — the Throttle Module's motorised lever (0x2C, I2C spec §9.4), as the master
   sees it: position, touch and precision from its last packet, and the one command
   that moves it. Used by the ENGINE interlock (lever to 0 %), the throttle buttons and
   the autopilots' throttle (panel_throttle.cpp, ksp_frame.cpp).

   Packet: [3] flags bit 0 enabled, bit 1 precision, bit 2 touched, bit 3 motor moving;
           [4] button rising edges bit 0 100 %, bit 1 UP, bit 2 DOWN, bit 3 0 %;
           [5..6] value, big-endian 0..INT16_MAX (in precision mode the module's fine
           output, not the lever position).
   The module refuses CMD_SET_THROTTLE while DISABLED or touched.
****************************************************************************************/
#pragma once
#include <Arduino.h>

static const uint16_t LEVER_FULL = 32767;          // INT16_MAX

struct LeverState {
  bool     known;          // a packet has been read
  bool     active;         // the module is ACTIVE (it takes lever commands)
  bool     precision, touched;
  uint16_t pos;            // 0..LEVER_FULL
  uint32_t ms;             // when the packet was read
};

LeverState leverState();
bool       leverCommand(uint16_t pos);             // CMD_SET_THROTTLE; false if not sent
