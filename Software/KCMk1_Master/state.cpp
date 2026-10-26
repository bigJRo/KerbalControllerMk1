/***************************************************************************************
   state.cpp — the shared state and its power-on values.
****************************************************************************************/
#include "state.h"
#include "config.h"

VesselState g_vessel;
PanelState  g_panel;
MasterState g_master;

void stateReset() {
  memset(&g_vessel, 0, sizeof g_vessel);
  memset(&g_panel, 0, sizeof g_panel);
  memset(&g_master, 0, sizeof g_master);

  g_panel.ctrlMode = CTRL_MODE_SPC;   // until the mode selector is read
  g_panel.ctrlGrp  = 1;
  g_panel.audio    = true;            // LIVE until the AUDIO switch is read

  g_master.bootPhase = BOOT_PHASE_SETUP;
}
