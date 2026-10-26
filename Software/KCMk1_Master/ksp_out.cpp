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

void kspNoteSuppressed(byte type) {
  g_suppressed++;
  dbg(DBG_TOKSP, "not sent type %u (%s)", type, g_master.demo ? "demo" : "KSP link down");
}
