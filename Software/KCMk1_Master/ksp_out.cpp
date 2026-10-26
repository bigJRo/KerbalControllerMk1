/***************************************************************************************
   ksp_out.cpp — the send gate's counters and log.
****************************************************************************************/
#include "ksp_out.h"

static uint32_t g_suppressed, g_sent;

uint32_t kspSuppressed() { return g_suppressed; }
uint32_t kspSentCount()  { return g_sent; }

void kspNoteSent(byte type, uint8_t size) {
  g_sent++;
  dbg(DBG_TOKSP, "sent type %u, %u bytes", type, size);
}

bool kspActionGroup(uint8_t action, bool on) {
  byte a = action;
  dbg(DBG_TOKSP, "%s %02X", on ? "AGACTIVATE" : "AGDEACTIVATE", action);
  return kspSend(on ? AGACTIVATE_MESSAGE : AGDEACTIVATE_MESSAGE, a);
}

bool kspThrottle(int16_t value) {
  throttleMessage m;
  m.throttle = value;
  dbg(DBG_TOKSP, "THROTTLE %d", value);
  return kspSend(THROTTLE_MESSAGE, m);
}

bool kspCag(CagOp op, uint8_t group) {
  static const char *const name[] = { "CAGACTIVATE", "CAGDEACTIVATE", "CAGTOGGLE" };
  static const byte type[] = { CAGACTIVATE_MESSAGE, CAGDEACTIVATE_MESSAGE, CAGTOGGLE_MESSAGE };
  byte g = group;
  dbg(DBG_TOKSP, "%s %u", name[op], group);
  return kspSend(type[op], g);
}

bool kspActionToggle(uint8_t action) {
  byte a = action;
  dbg(DBG_TOKSP, "AGTOGGLE %02X", action);
  return kspSend(AGTOGGLE_MESSAGE, a);
}

bool kspSasMode(uint8_t mode) {
  byte m = mode;
  dbg(DBG_TOKSP, "SAS_MODE %u", mode);
  return kspSend(SAS_MODE_MESSAGE, m);
}

bool kspKey(int16_t vk, uint8_t modifier) {
  keyboardEmulatorMessage m(vk, modifier);
  return kspSend(KEYBOARD_EMULATOR, m);
}

void kspNoteSuppressed(byte type) {
  g_suppressed++;
  dbg(DBG_TOKSP, "not sent type %u (%s)", type, g_master.demo ? "demo" : "KSP link down");
}
