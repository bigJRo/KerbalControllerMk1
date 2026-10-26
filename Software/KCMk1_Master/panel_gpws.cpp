/***************************************************************************************
   panel_gpws.cpp — GPWS Input Panel (0x2A), module-managed (Inventory §10.8, I2C spec
   §9.3, §15.3). The master relays its state byte (bits 1:0 mode OFF / ACTIVE / PROX,
   bit 2 proximity alarm, bit 3 rendezvous radar) and its value (the altitude threshold,
   metres) to the Annunciator, which uses the same layout. The module manages its own
   LEDs and buttons; the master never acts on them.
****************************************************************************************/
#include "panels.h"
#include "carriers.h"
#include "debug.h"
#include <KerbalModuleCommon.h>

static uint8_t  g_cfg;
static int16_t  g_thr;
static bool     g_seen;

void panelGpwsService() {
  uint8_t p[KMC_DISPLAY_PACKET_SIZE];
  if (!moduleLastPacket(MOD_GPWS, p)) return;
  uint8_t cfg = p[5] & 0x0F;
  int16_t thr = (int16_t)(p[6] << 8 | p[7]);
  if (g_seen && cfg == g_cfg && thr == g_thr) return;
  g_seen = true; g_cfg = cfg; g_thr = thr;
  carrierSetGpws(cfg, thr);
  dbg(DBG_STATE, "GPWS mode %u%s%s, threshold %d m", cfg & 3, (cfg & 4) ? " prox" : "", (cfg & 8) ? " radar" : "", thr);
}

void panelGpwsInit() { g_seen = false; g_cfg = 0; g_thr = 0; }
