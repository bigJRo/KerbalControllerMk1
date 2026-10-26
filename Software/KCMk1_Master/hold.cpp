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

static bool    g_engaged;
static uint8_t g_mode;                  // control mode it was engaged in
static bool    g_sasWasOn;
static uint8_t g_sasWasMode;            // 255 = unknown / off
static uint8_t g_sceneVessel;           // vessel-change count when engaged

static uint8_t g_vesselChanges;         // VESSEL_CHANGE messages seen

bool holdEngaged() { return g_engaged; }

bool holdAvailable() { return panelLive() && !g_vessel.eva && g_panel.ctrlMode == CTRL_MODE_SPC; }

static void setEngaged(bool on) {
  g_engaged = on;
  g_panel.autopilot = on;               // stage 7 ORs in the console autopilots
  switchErrSuppress(SW_SAS, SUPP_AUTOPILOT, on);
}

static void engage() {
  g_mode = g_panel.ctrlMode;
  g_sasWasOn = (g_vessel.actions & SAS_ACTION) != 0;
  g_sasWasMode = g_vessel.sasValid ? g_vessel.sasMode : 255;
  g_sceneVessel = g_vesselChanges;
  if (!g_sasWasOn) kspActionGroup(SAS_ACTION, true);
  kspSasMode(AP_STABILITYASSIST);
  setEngaged(true);
  dbg(DBG_AP, "HOLD engaged (SPC: SAS stability assist)");
}

void holdRelease(const char *why) {
  if (!g_engaged) return;
  setEngaged(false);
  if (panelLive()) {                    // put SAS back as it was
    if (!g_sasWasOn) kspActionGroup(SAS_ACTION, false);
    else if (g_sasWasMode != 255 && g_sasWasMode != AP_STABILITYASSIST) kspSasMode(g_sasWasMode);
  }
  dbg(DBG_AP, "HOLD released (%s)", why);
}

void holdToggle() {
  if (g_engaged) { holdRelease("HOLD button"); return; }
  if (!holdAvailable()) {
    dbg(DBG_AP, "HOLD not available (%s)", g_panel.ctrlMode != CTRL_MODE_SPC ? "PLN / RVR need the hold autopilot" : "no flight");
    return;
  }
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
}

void holdInit() {
  g_engaged = false;
  g_panel.autopilot = false;
  g_vesselChanges = g_sceneVessel = 0;
  interlockOnDrop(onDrop);
}
