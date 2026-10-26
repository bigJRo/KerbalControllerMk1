/***************************************************************************************
   panel_vehicle.cpp — Vehicle Control (0x24), buttons B0–B11 (Inventory §10.3, design
   §8.3, §8.5). Switch Group 2 on B16–B23 is switches.cpp.

     B0  BRAKES        brakes while held; on release off only if BRAKE is on REL
     B1  LIGHTS        lights toggle            B3  LANDING GEAR   gear toggle
     B2  ANTENNA       CAG 13                   B4  FUEL CELL      CAG 14
     B5  SOLAR ARRAY   CAG 15                   B6  CARGO DOOR     CAG 16
     B7  RADIATOR      CAG 17                   B9  LADDER         CAG 18
     B8  HEAT SHIELD   state machine: deploy CAG 19, then release CAG 20
     B10 MAIN          state machine: deploy CAG 21, then cut CAG 22   (CHUTE ARM)
     B11 DROGUE        state machine: deploy CAG 23, then cut CAG 24   (CHUTE ARM)
   CAG numbers carry the control-group offset. The CAG buttons toggle; the state
   machines only ever activate, act on release, and a 2 s hold resets them (both groups
   deactivated). Their state is KSP's report of the two groups, never memory here.
****************************************************************************************/
#include "panels.h"
#include "state.h"
#include "debug.h"
#include "ksp_out.h"
#include "cag.h"
#include "switches.h"
#include <KerbalModuleCommon.h>

enum : uint8_t { B_BRAKES = 0, B_LIGHTS = 1, B_GEAR = 3, B_HEAT = 8, B_MAIN = 10, B_DROGUE = 11 };

// Base CAG per button, 0 = not a plain CAG button.
static const uint8_t kCagBase[12] = { 0, 0, 13, 0, 14, 15, 16, 17, 0, 18, 0, 0 };

struct StateMachine { uint8_t button, deploy, cut; bool chute; const char *name; };
static const StateMachine kSm[3] = {
  { B_HEAT,   19, 20, false, "HEAT SHIELD" },
  { B_MAIN,   21, 22, true,  "MAIN" },
  { B_DROGUE, 23, 24, true,  "DROGUE" },
};
enum SmState : uint8_t { SM_READY, SM_DEPLOYED, SM_CUT };

static uint8_t g_brakeSrc;               // BRAKE_SRC_* holding the brakes
static uint32_t g_smHeldFired;           // bit per button: its 2 s reset fired this press

static SmState smState(const StateMachine &m) {
  if (cagActive(cagNumber(m.cut))) return SM_CUT;
  if (cagActive(cagNumber(m.deploy))) return SM_DEPLOYED;
  return SM_READY;
}

static bool smUsable(const StateMachine &m) { return !m.chute || g_panel.chuteArm; }

static void smRelease(const StateMachine &m) {
  switch (smState(m)) {
    case SM_READY:    kspCag(CAG_ACTIVATE, cagNumber(m.deploy)); dbg(DBG_STATE, "%s deploy", m.name); break;
    case SM_DEPLOYED: kspCag(CAG_ACTIVATE, cagNumber(m.cut));    dbg(DBG_STATE, "%s %s", m.name, m.chute ? "cut" : "release"); break;
    case SM_CUT:      dbg(DBG_STATE, "%s already %s", m.name, m.chute ? "cut" : "released"); break;
  }
}

static void smReset(const StateMachine &m) {
  kspCag(CAG_DEACTIVATE, cagNumber(m.deploy));
  kspCag(CAG_DEACTIVATE, cagNumber(m.cut));
  dbg(DBG_STATE, "%s reset (2 s hold)", m.name);
}

void vehicleBrakeHold(uint8_t source, bool held) {
  uint8_t was = g_brakeSrc;
  if (held) g_brakeSrc |= source; else g_brakeSrc &= (uint8_t)~source;
  switchErrSuppress(SW_BRAKE, SUPP_BRAKE_HELD, g_brakeSrc != 0);
  if (!panelLive()) return;
  if (!was && g_brakeSrc) kspActionGroup(BRAKES_ACTION, true);
  else if (was && !g_brakeSrc && !g_panel.brakeLock) kspActionGroup(BRAKES_ACTION, false);
}

static void onEvent(const ModEvent &e) {
  panelNoteEvent(e);
  if (!panelsEnabled() || !panelLive()) return;
  uint32_t act = e.pressed | e.tapped;            // act on the press (a tap included)
  uint32_t up  = e.released | e.tapped;

  // BRAKES: on with the press, off with the release (unless LOCK). A tap does both.
  if (act & (1u << B_BRAKES)) vehicleBrakeHold(BRAKE_SRC_BUTTON, true);
  if (up & (1u << B_BRAKES))  vehicleBrakeHold(BRAKE_SRC_BUTTON, false);

  if (act & (1u << B_LIGHTS)) kspActionToggle(LIGHT_ACTION);
  if (act & (1u << B_GEAR))   kspActionToggle(GEAR_ACTION);
  for (uint8_t b = 0; b < 12; b++)
    if (kCagBase[b] && (act & (1u << b))) kspCag(CAG_TOGGLE, cagNumber(kCagBase[b]));

  for (auto &m : kSm) {
    uint32_t bit = 1u << m.button;
    if (!smUsable(m)) { g_smHeldFired &= ~bit; continue; }
    if (e.pressed & bit) g_smHeldFired &= ~bit;
    if (e.held & bit) { g_smHeldFired |= bit; smReset(m); }
    if (up & bit) {
      if (!(g_smHeldFired & bit)) smRelease(m);   // a release before 2 s is the press
      g_smHeldFired &= ~bit;
    }
  }
}

void panelVehicleService() {
  bool live = !panelDemo();
  uint8_t a = g_vessel.actions;
  panelLed(MOD_VEHICLE, B_BRAKES, true, live && (a & BRAKES_ACTION));
  panelLed(MOD_VEHICLE, B_LIGHTS, true, live && (a & LIGHT_ACTION));
  panelLed(MOD_VEHICLE, B_GEAR,   true, live && (a & GEAR_ACTION));
  for (uint8_t b = 0; b < 12; b++) if (kCagBase[b]) panelLed(MOD_VEHICLE, b, true, live && cagActive(cagNumber(kCagBase[b])));
  for (auto &m : kSm) {
    if (!smUsable(m)) { moduleSetLed(MOD_VEHICLE, m.button, KMC_LED_OFF); continue; }
    if (panelDemo())  { panelLed(MOD_VEHICLE, m.button, true, false); continue; }
    SmState s = smState(m);
    moduleSetLed(MOD_VEHICLE, m.button, s == SM_CUT ? KMC_LED_CUT : s == SM_DEPLOYED ? KMC_LED_ACTIVE : KMC_LED_ENABLED);
  }
  if (!panelLive() && g_brakeSrc) { g_brakeSrc = 0; switchErrSuppress(SW_BRAKE, SUPP_BRAKE_HELD, false); }
}

void panelVehicleInit() {
  g_brakeSrc = 0;
  g_smHeldFired = 0;
  moduleBusSetHandler(MOD_VEHICLE, onEvent);
}
