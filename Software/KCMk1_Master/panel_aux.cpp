/***************************************************************************************
   panel_aux.cpp — AUX CTRL (0x26) (Inventory §10.1, design §8.9).

     [EVA] B0 LIGHTS U, B1 JET PACK R, B2 BOARD B, B3 CONST I, B4 JUMP / LET GO Space,
           B5 GRAB F, B6 EVA CHUTE P, B7 HELM O, B9 FLAG G: a key tap each; dark outside
           EVA, and ignored. KSP reports none of them, so each flashes on a press.
     B8    UNDOCK: CAG 37 toggle, lit while active.
     B10   [CTRL PT] PRI: CAG 35 } mutually exclusive: a press deactivates the other's
     B11   [CTRL PT] ALT: CAG 36 } group, then activates its own; the active one is lit,
                                   and pressing it does nothing.
   CAG numbers carry the control-group offset.
****************************************************************************************/
#include "panels.h"
#include "state.h"
#include "debug.h"
#include "ksp_out.h"
#include "keys.h"
#include "cag.h"

enum : uint8_t { B_UNDOCK = 8, B_PRI = 10, B_ALT = 11 };
static const uint8_t CAG_UNDOCK = 37, CAG_PRI = 35, CAG_ALT = 36;

// EVA key per button, 0 = not an EVA button.
static const int16_t kEvaKey[12] = { VK_U, VK_R, VK_B, VK_I, VK_SPACE, VK_F, VK_P, VK_O, 0, VK_G, 0, 0 };

static bool evaUsable() { return panelDemo() || g_vessel.eva; }

static void controlPoint(uint8_t own, uint8_t other, const char *name) {
  if (cagActive(cagNumber(own))) { dbg(DBG_STATE, "control point %s already active", name); return; }
  kspCag(CAG_DEACTIVATE, cagNumber(other));
  kspCag(CAG_ACTIVATE, cagNumber(own));
  dbg(DBG_STATE, "control point %s", name);
}

static void onEvent(const ModEvent &e) {
  panelNoteEvent(e);
  if (!panelsEnabled() || !panelLive()) return;
  uint32_t act = e.pressed | e.tapped;
  for (uint8_t b = 0; b < 12; b++) {
    if (!kEvaKey[b] || !(act & (1u << b))) continue;
    if (!g_vessel.eva) continue;                   // dark and ignored outside EVA
    keyTap(kEvaKey[b]);
    panelFlash(MOD_AUX, b);
  }
  if (act & (1u << B_UNDOCK)) kspCag(CAG_TOGGLE, cagNumber(CAG_UNDOCK));
  if (act & (1u << B_PRI)) controlPoint(CAG_PRI, CAG_ALT, "PRI");
  if (act & (1u << B_ALT)) controlPoint(CAG_ALT, CAG_PRI, "ALT");
}

void panelAuxService() {
  for (uint8_t b = 0; b < 12; b++) if (kEvaKey[b]) panelLed(MOD_AUX, b, evaUsable(), false);
  panelLed(MOD_AUX, B_UNDOCK, true, cagActive(cagNumber(CAG_UNDOCK)));
  panelLed(MOD_AUX, B_PRI, true, cagActive(cagNumber(CAG_PRI)));
  panelLed(MOD_AUX, B_ALT, true, cagActive(cagNumber(CAG_ALT)));
}

void panelAuxInit() { moduleBusSetHandler(MOD_AUX, onEvent); }
