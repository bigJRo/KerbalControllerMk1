// Host Arduino core for tools/render_screen.py: the syntax-only stub from
// ../host_stubs plus the few things that only matter once the code is LINKED
// and RUN rather than just parsed.
#pragma once
// Standard headers first: the stub defines min/max as macros, which would break
// these if they were pulled in afterwards.
#include <vector>
#include <algorithm>
#include <string>
#include "../host_stubs/Arduino.h"

// Audio library: tone() on the master-alarm pin (silent here).
inline void tone(int, unsigned int, unsigned long = 0) {}
inline void noTone(int) {}

// Teensy 4.1 system registers touched by the reboot helpers (never called here).
inline volatile uint32_t &kcmStubReg() { static volatile uint32_t r = 0; return r; }
#define SCB_AIRCR    kcmStubReg()
#define USB1_USBCMD  kcmStubReg()
#define USB1_USBSTS  kcmStubReg()
#define USB1_USBINTR kcmStubReg()
#define USB1_PORTSC1 kcmStubReg()
#define USB1_USBMODE kcmStubReg()
#define CCM_CCGR6    kcmStubReg()
#define asm(x)

// Pins read HIGH. The FT5316 software-I2C lines idle high (the stub's constant 0
// looks like a slave clock-stretching forever, and micros() never advances to time
// it out), and its INT line is active-low, so an idle bus with no touch is what the
// driver sees.
#define digitalReadFast(p) 1
#define digitalRead(p) 1
