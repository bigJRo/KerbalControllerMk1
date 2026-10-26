/***************************************************************************************
   panel_action.cpp — Action Control (0x22): AG 1–12 (Inventory §10.6). Each button
   toggles its custom action group, lit while KSP reports it active. These are the
   only CAG buttons without the control-group offset: they always send CAG 1–12.
****************************************************************************************/
#include "panels.h"
#include "ksp_out.h"
#include "cag.h"

// Button → CAG (two columns: AG 6..1 on the even buttons, AG 12..7 on the odd).
static const uint8_t kCag[12] = { 6, 12, 5, 11, 4, 10, 3, 9, 2, 8, 1, 7 };

static void onEvent(const ModEvent &e) {
  panelNoteEvent(e);
  if (!panelsEnabled() || !panelLive()) return;
  uint32_t act = e.pressed | e.tapped;
  for (uint8_t b = 0; b < 12; b++) if (act & (1u << b)) kspCag(CAG_TOGGLE, cagFixed(kCag[b]));
}

void panelActionService() {
  for (uint8_t b = 0; b < 12; b++) panelLed(MOD_ACTION, b, true, cagActive(cagFixed(kCag[b])));
}

void panelActionInit() { moduleBusSetHandler(MOD_ACTION, onEvent); }
