/***************************************************************************************
   panel_time.cpp — Time Control (0x25), buttons B0–B11 (Inventory §10.4, design §8.8),
   and the Pre-Warp lead (0x2B). The mode selector on B12 / B13 is switches.cpp.

     [WARP TO], less the Pre-Warp lead (minutes):
       B0 ApA   apoapsis           available: orbit closed (e < 1), not landed
       B2 PeA   periapsis          available: not landed
       B4 MNVR  start of the burn  available: a maneuver node (Δv > 0)
       B6 SOI   next SOI change    available: in flight, outside the atmosphere
       B8 MORN  next morning       available: landed or splashed
       Dark, and ignored, when not available. Until KSP has reported what a rule
       needs, the button counts as available (KSP ignores a warp it cannot do) —
       except MNVR, which needs a node reported.
     [TIME WARP]:
       B1 STOP  cancel any warp, including a warp-to, back to 1×
       B3 PHYS  held as a modifier: PHYS + > / < steps physics warp 1×–4×
       B5 >     warp up              B7 <   warp down
     B9  LOAD   F9 held while held (KSP's quick load needs it held)
     B10 PAUSE  Esc                  B11 SAVE  F5
   The Pre-Warp lead is the Pre-Warp display's value (I2C spec §9.3, bytes 6..7).
****************************************************************************************/
#include "panels.h"
#include "config.h"
#include "state.h"
#include "debug.h"
#include "ksp_out.h"
#include "keys.h"
#include <KerbalModuleCommon.h>

enum : uint8_t {
  B_APA = 0, B_STOP = 1, B_PEA = 2, B_PHYS = 3, B_MNVR = 4, B_FASTER = 5, B_SOI = 6, B_SLOWER = 7,
  B_MORN = 8, B_LOAD = 9, B_PAUSE = 10, B_SAVE = 11
};

static uint16_t g_leadMin;               // Pre-Warp minutes

static bool landed() { return (g_vessel.situation & (KSP_SIT_LANDED | KSP_SIT_SPLASHED | KSP_SIT_PRELAUNCH)) != 0; }

static bool available(uint8_t b) {
  if (panelDemo()) return true;
  switch (b) {
    case B_APA:  return !landed() && (!g_vessel.orbitValid || g_vessel.eccentricity < 1.0f);
    case B_PEA:  return !landed();
    case B_MNVR: return g_vessel.nodeValid && g_vessel.nodeDv > 0.0f;
    case B_SOI:  return g_vessel.inFlight && !landed() && !(g_vessel.atmoValid && g_vessel.inAtmosphere);
    case B_MORN: return landed();
  }
  return true;
}

static void warpTo(uint8_t b, uint8_t instant, const char *name) {
  if (!available(b)) { dbg(DBG_STATE, "WARP TO %s: not available", name); return; }
  float lead = -60.0f * (float)g_leadMin;
  kspTimewarpTo(instant, lead);
  panelFlash(MOD_TIME, b);
}

static void physStep(int8_t dir) {
  int idx = g_vessel.twIndex + dir;              // KSP keeps one rate index for both modes
  if (idx < 0) idx = 0;
  if (idx > 3) idx = 3;
  kspTimewarp((uint8_t)(TIMEWARP_X1_PHYSICAL + idx));
}

static void onEvent(const ModEvent &e) {
  panelNoteEvent(e);
  if (!panelsEnabled() || !panelLive()) return;
  uint32_t act = e.pressed | e.tapped;
  bool phys = (e.down & (1u << B_PHYS)) != 0;
  if (act & (1u << B_APA))  warpTo(B_APA, TIMEWARP_TO_APOAPSIS, "ApA");
  if (act & (1u << B_PEA))  warpTo(B_PEA, TIMEWARP_TO_PERIAPSIS, "PeA");
  if (act & (1u << B_MNVR)) warpTo(B_MNVR, TIMEWARP_TO_NEXT_BURN, "MNVR");
  if (act & (1u << B_SOI))  warpTo(B_SOI, TIMEWARP_TO_NEXT_SOI, "SOI");
  if (act & (1u << B_MORN)) warpTo(B_MORN, TIMEWARP_TO_NEXT_MORNING, "MORN");
  if (act & (1u << B_STOP)) { kspTimewarp(TIMEWARP_CANCEL_AUTOWARP); kspTimewarp(TIMEWARP_X1); panelFlash(MOD_TIME, B_STOP); }
  if (act & (1u << B_FASTER)) { if (phys) physStep(+1); else kspTimewarp(TIMEWARP_UP);   panelFlash(MOD_TIME, B_FASTER); }
  if (act & (1u << B_SLOWER)) { if (phys) physStep(-1); else kspTimewarp(TIMEWARP_DOWN); panelFlash(MOD_TIME, B_SLOWER); }
  if (e.pressed & (1u << B_LOAD)) keyHold(VK_F9, true);
  if ((e.released | e.tapped) & (1u << B_LOAD)) { if (e.tapped & (1u << B_LOAD)) keyHold(VK_F9, true); keyHold(VK_F9, false); }
  if (act & (1u << B_PAUSE)) { keyTap(VK_ESCAPE); panelFlash(MOD_TIME, B_PAUSE); }
  if (act & (1u << B_SAVE))  { keyTap(VK_F5); panelFlash(MOD_TIME, B_SAVE); }
}

static void readLead() {
  uint8_t p[KMC_DISPLAY_PACKET_SIZE];
  if (!moduleLastPacket(MOD_PREWARP, p)) return;
  uint16_t v = (uint16_t)(p[6] << 8 | p[7]);
  if (v > 9999) v = 9999;
  if (v != g_leadMin) dbg(DBG_STATE, "Pre-Warp lead %u min", v);
  g_leadMin = v;
}

uint16_t timePreWarpMinutes() { return g_leadMin; }

void panelTimeService() {
  readLead();
  static const uint8_t kWarpTo[] = { B_APA, B_PEA, B_MNVR, B_SOI, B_MORN };
  for (uint8_t b : kWarpTo) panelLed(MOD_TIME, b, available(b), false);
  uint32_t down = panelDown(MOD_TIME);
  panelLed(MOD_TIME, B_PHYS, true, (down & (1u << B_PHYS)) != 0);
  panelLed(MOD_TIME, B_LOAD, true, keyHeld(VK_F9));
  static const uint8_t kTaps[] = { B_STOP, B_FASTER, B_SLOWER, B_PAUSE, B_SAVE };
  for (uint8_t b : kTaps) panelLed(MOD_TIME, b, true, false);
}

void panelTimeInit() {
  g_leadMin = 0;
  moduleBusSetHandler(MOD_TIME, onEvent);
}
