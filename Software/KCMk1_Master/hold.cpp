/***************************************************************************************
   hold.cpp — HOLD.
****************************************************************************************/
#include "hold.h"
#include "config.h"
#include "state.h"
#include "debug.h"
#include "ksp_out.h"
#include "panels.h"
#include "switches.h"
#include "interlocks.h"
#include "control_links.h"
#include "hold_autopilot.h"
#include "autopilots.h"

static bool    g_engaged;
static uint8_t g_mode;                  // control mode it was engaged in
static bool    g_sasWasOn;
static uint8_t g_sasWasMode;            // 255 = unknown / off
static uint8_t g_sceneVessel;           // vessel-change count when engaged

static uint8_t g_vesselChanges;         // VESSEL_CHANGE messages seen

bool holdEngaged() { return g_engaged; }

bool holdAvailable() { return panelLive() && !g_vessel.eva; }

bool holdUsesHoldAutopilot() { return g_engaged && g_mode != CTRL_MODE_SPC; }

static void setEngaged(bool on) {
  g_engaged = on;
  g_panel.autopilot = on || autopilotsEngaged();
  switchErrSuppress(SW_SAS, SUPP_AUTOPILOT, on);
}

// SPC: SAS Stability Assist. PLN: the hold autopilot's ALT + HDG at the current values.
// RVR: its RHDG, and CRUISE at the current speed unless the ENGINE is SAFE.
static bool engage() {
  g_mode = g_panel.ctrlMode;
  g_sceneVessel = g_vesselChanges;
  if (autopilotsEngaged()) arbAllOffReason(HP_REASON_OTHER_AP);   // HOLD takes the vehicle as it is
  switch (g_mode) {
    case CTRL_MODE_SPC:
      g_sasWasOn = (g_vessel.actions & SAS_ACTION) != 0;
      g_sasWasMode = g_vessel.sasValid ? g_vessel.sasMode : 255;
      if (!g_sasWasOn) kspActionGroup(SAS_ACTION, true);
      kspSasMode(AP_STABILITYASSIST);
      break;
    case CTRL_MODE_PLN:
      if (!hpEngage(HP_MODE_ALT, true) || !hpEngage(HP_MODE_HDG, true)) {
        hpDisconnectAircraft(HP_REASON_PILOT);
        dbg(DBG_AP, "HOLD refused by the hold autopilot (%s)", hpReasonName(hpGetStatus().reason));
        return false;
      }
      break;
    default:   // RVR
      if (!hpEngage(HP_MODE_RHDG, true)) {
        dbg(DBG_AP, "HOLD refused by the hold autopilot (%s)", hpReasonName(hpGetStatus().roverReason));
        return false;
      }
      if (interlockThrottleLive()) hpEngage(HP_MODE_CRUISE, true);
      break;
  }
  setEngaged(true);
  static const char *const what[] = { "RVR: heading + speed", "PLN: altitude + heading", "SPC: SAS stability assist" };
  dbg(DBG_AP, "HOLD engaged (%s)", what[g_mode % 3]);
  return true;
}

void holdRelease(const char *why) {
  if (!g_engaged) return;
  setEngaged(false);
  if (g_mode == CTRL_MODE_PLN)      hpDisconnectAircraft(HP_REASON_PILOT);
  else if (g_mode == CTRL_MODE_RVR) hpDisconnectRover(HP_REASON_PILOT);
  else if (panelLive()) {               // SPC: put SAS back as it was
    if (!g_sasWasOn) kspActionGroup(SAS_ACTION, false);
    else if (g_sasWasMode != 255 && g_sasWasMode != AP_STABILITYASSIST) kspSasMode(g_sasWasMode);
  }
  dbg(DBG_AP, "HOLD released (%s)", why);
}

void holdToggle() {
  if (g_engaged) { holdRelease("HOLD button"); return; }
  if (!holdAvailable()) { dbg(DBG_AP, "HOLD not available (no flight, or EVA)"); return; }
  engage();
}

static void onDrop(DropReason why) {
  if (why == DROP_AP_INHIB || why == DROP_ENG_SAFE) return;   // HOLD is not affected (SPC uses no throttle)
  holdRelease(dropReasonName(why));
}

void holdNoteVesselChange() { g_vesselChanges++; }

void holdService() {
  if (!g_engaged) return;
  if (!panelLive())                       holdRelease("left the flight");
  else if (g_vessel.eva)                  holdRelease("EVA");
  else if (g_panel.ctrlMode != g_mode)    holdRelease("control mode changed");
  else if (g_vesselChanges != g_sceneVessel) holdRelease("vessel changed");
  else if (g_mode == CTRL_MODE_PLN && !hpAttitudeEngaged()) holdRelease("the hold autopilot let go");
  else if (g_mode == CTRL_MODE_RVR && !hpRoverEngaged())    holdRelease("the hold autopilot let go");
}

void holdInit() {
  g_engaged = false;
  g_vesselChanges = g_sceneVessel = 0;
  interlockOnDrop(onDrop);
}
