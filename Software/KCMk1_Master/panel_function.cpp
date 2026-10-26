/***************************************************************************************
   panel_function.cpp — Function Control (0x21), buttons B0–B11 (Inventory §10.5).
   Switch Group 1 on B16–B23 is switches.cpp.

     B0  FAIRING JETT   CAG 26        B1  LES               CAG 25
     B2  LOCK SURFACES  CAG 34        B3  AIR INTAKE        CAG 33
     B4  FLAPS          CAG 39        B5  AIR BRAKE         CAG 38
     B6  [SCIENCE] COLLECT CAG 28     B7  [ENGINE] ALT      CAG 27
     B8  [SCIENCE] GROUP 1 CAG 30     B9  [ENGINE] GROUP 1  CAG 29
     B10 [SCIENCE] GROUP 2 CAG 32     B11 [ENGINE] GROUP 2  CAG 31
   Each toggles its group (with the control-group offset), lit while KSP reports it.
****************************************************************************************/
#include "panels.h"
#include "ksp_out.h"
#include "cag.h"

static const uint8_t kCagBase[12] = { 26, 25, 34, 33, 39, 38, 28, 27, 30, 29, 32, 31 };

static void onEvent(const ModEvent &e) {
  panelNoteEvent(e);
  if (!panelsEnabled() || !panelLive()) return;
  uint32_t act = e.pressed | e.tapped;
  for (uint8_t b = 0; b < 12; b++) if (act & (1u << b)) kspCag(CAG_TOGGLE, cagNumber(kCagBase[b]));
}

void panelFunctionService() {
  for (uint8_t b = 0; b < 12; b++) panelLed(MOD_FUNCTION, b, true, cagActive(cagNumber(kCagBase[b])));
}

void panelFunctionInit() { moduleBusSetHandler(MOD_FUNCTION, onEvent); }
