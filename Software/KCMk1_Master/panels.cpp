/***************************************************************************************
   panels.cpp — the shared panel rules and the tab list.
****************************************************************************************/
#include "panels.h"
#include "state.h"
#include "keys.h"
#include <KerbalModuleCommon.h>

static bool     g_enabled;
static uint32_t g_down[MOD_COUNT];
static uint32_t g_flashUntil[MOD_COUNT][16];
static bool     g_wasLive;

bool panelsEnabled() { return g_enabled; }
bool panelLive()     { return !g_master.demo && g_master.kspConnected && g_vessel.flightScene; }
bool panelDemo()     { return g_master.demo; }

uint32_t panelDown(ModId id) { return id < MOD_COUNT ? g_down[id] : 0; }

void panelNoteEvent(const ModEvent &e) {
  if (e.id < MOD_COUNT && e.pkt) g_down[e.id] = e.down;
}

void panelFlash(ModId id, uint8_t b) {
  if (id < MOD_COUNT && b < 16) g_flashUntil[id][b] = millis() + PANEL_FLASH_MS;
}

bool panelFlashing(ModId id, uint8_t b) {
  return id < MOD_COUNT && b < 16 && (int32_t)(g_flashUntil[id][b] - millis()) > 0;
}

void panelLed(ModId id, uint8_t b, bool usable, bool kspOn) {
  uint8_t s;
  if (!usable)                      s = KMC_LED_OFF;
  else if (panelDemo())             s = (g_down[id] & (1UL << b)) ? KMC_LED_ACTIVE : KMC_LED_ENABLED;
  else if (kspOn || panelFlashing(id, b)) s = KMC_LED_ACTIVE;
  else                              s = KMC_LED_ENABLED;
  moduleSetLed(id, b, s);
}

void panelsSetEnabled(bool on) { g_enabled = on; }

void panelsInit() {
  g_enabled = true;
  memset(g_down, 0, sizeof g_down);
  memset(g_flashUntil, 0, sizeof g_flashUntil);
  g_wasLive = false;
  keysInit();
  panelVehicleInit();
  panelStabilityInit();
  panelActionInit();
  panelAuxInit();
  panelEncodersInit();
}

void panelsService() {
  bool live = panelLive();
  if (g_wasLive && !live) keysReleaseAll();       // no key stays down past a flight
  g_wasLive = live;
  if (!g_enabled) return;
  for (uint8_t m = 0; m < MOD_COUNT; m++) if (moduleLifecycle((ModId)m) != KMC_STATUS_ACTIVE) g_down[m] = 0;
  panelVehicleService();
  panelStabilityService();
  panelActionService();
  panelAuxService();
}
