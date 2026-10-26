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
