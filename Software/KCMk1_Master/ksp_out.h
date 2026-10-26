/***************************************************************************************
   ksp_out.h — the only code that sends to KSP (design §7).

   Every Simpit command the master sends goes through kspSend(), which
     - sends nothing in demo (design §10.1) and nothing while the link is down;
     - logs each message (debug category TOKSP).
   Stage 5 adds the per-frame stick / throttle arbitration on top; stage 1 has only the
   gate.
****************************************************************************************/
#pragma once
#include <Arduino.h>
#include "simpit_link.h"
#include "state.h"
#include "debug.h"

uint32_t kspSuppressed();                 // sends refused by the gate (demo / link down)
uint32_t kspSentCount();
void     kspNoteSent(byte type, uint8_t size);
void     kspNoteSuppressed(byte type);

inline bool kspAllowed() { return !g_master.demo && g_master.kspConnected; }

template <class T> bool kspSend(byte type, T &msg) {
  if (!kspAllowed()) { kspNoteSuppressed(type); return false; }
  simpitClient().send(type, msg);
  kspNoteSent(type, (uint8_t)sizeof(T));
  return true;
}

// Standard action groups (*_ACTION bits): AGACTIVATE / AGDEACTIVATE.
bool kspActionGroup(uint8_t action, bool on);
// Main throttle, 0..INT16_MAX. Stage 5 adds the per-frame owner merge; until then only
// the ENGINE interlock sends it (0 %).
bool kspThrottle(int16_t value);
// Custom action groups 1..255: CAGACTIVATE / CAGDEACTIVATE / CAGTOGGLE.
enum CagOp : uint8_t { CAG_ACTIVATE, CAG_DEACTIVATE, CAG_TOGGLE };
bool kspCag(CagOp op, uint8_t group);
bool kspActionToggle(uint8_t action);     // AGTOGGLE (LIGHTS, LANDING GEAR buttons)
bool kspSasMode(uint8_t mode);            // AutopilotMode
bool kspKey(int16_t vk, uint8_t modifier = 0);   // KEYBOARD_EMULATOR (keys.h tracks held keys)
bool kspTimewarp(uint8_t command);        // TIMEWARP (Timewarp enum)
bool kspTimewarpTo(uint8_t instant, float delayS);   // TIMEWARP_TO (TimewarpTo enum), delay < 0 = before
bool kspNavballCycle();                   // NAVBALLMODE
void kspPrint(const char *text);          // a message on KSP's screen (the autopilots' notices)
