/***************************************************************************************
   panel_stability.cpp — Stability Control (0x23) (Inventory §10.2, design §8.4).

     B0–B9  SAS modes. SAS switch OFF: all dark, presses ignored. ENAB: KSP's current
            mode lit, available modes backlit, unavailable modes dark and ignored
            (SAS_MODE_INFO; until it arrives every mode counts as available, and KSP
            ignores a mode it does not allow).
     B10    HOLD (hold.cpp): dark where it is not available.
     B11    INVERT: holds F while held (KSP's SAS invert); SWITCH ERR does not check SAS
            meanwhile. Dark on EVA, where F is GRAB.
****************************************************************************************/
#include "panels.h"
#include "state.h"
#include "debug.h"
#include "ksp_out.h"
#include "keys.h"
#include "hold.h"
#include "switches.h"
#include <KerbalModuleCommon.h>

enum : uint8_t { B_HOLD = 10, B_INVERT = 11 };

// Button → KSP AutopilotMode (Inventory §10.2).
static const uint8_t kSasMode[10] = {
  AP_TARGET, AP_ANTITARGET, AP_RADIALIN, AP_RADIALOUT, AP_NORMAL,
  AP_ANTINORMAL, AP_PROGRADE, AP_RETROGRADE, AP_STABILITYASSIST, AP_MANEUVER
};

static bool sasButtonUsable(uint8_t b) {
  if (!switchOn(SW_SAS)) return false;            // also dark while the switch is unread
  if (panelDemo() || !g_vessel.sasValid) return true;
  return (g_vessel.sasAvail >> kSasMode[b]) & 1;
}

static bool invertUsable() { return panelDemo() || !g_vessel.eva; }

static void setInvert(bool held) {
  if (held && !invertUsable()) return;
  keyHold(VK_F, held);
  switchErrSuppress(SW_SAS, SUPP_INVERT, held && keyHeld(VK_F));
}

static void onEvent(const ModEvent &e) {
  panelNoteEvent(e);
  if (!panelsEnabled() || !panelLive()) return;
  uint32_t act = e.pressed | e.tapped;
  for (uint8_t b = 0; b < 10; b++) {
    if (!(act & (1u << b))) continue;
    if (!sasButtonUsable(b)) { dbg(DBG_STATE, "SAS mode %u ignored (%s)", kSasMode[b], switchOn(SW_SAS) ? "not available" : "SAS OFF"); continue; }
    kspSasMode(kSasMode[b]);
  }
  if (act & (1u << B_HOLD)) holdToggle();
  if (e.pressed & (1u << B_INVERT)) setInvert(true);
  if ((e.released | e.tapped) & (1u << B_INVERT)) setInvert(false);
}

void panelStabilityService() {
  bool sasOn = g_vessel.sasValid ? g_vessel.sasMode != 255 : (g_vessel.actions & SAS_ACTION) != 0;
  for (uint8_t b = 0; b < 10; b++)
    panelLed(MOD_STABILITY, b, sasButtonUsable(b), sasOn && g_vessel.sasValid && g_vessel.sasMode == kSasMode[b]);
  panelLed(MOD_STABILITY, B_HOLD, panelDemo() || holdAvailable() || holdEngaged(), holdEngaged());
  panelLed(MOD_STABILITY, B_INVERT, invertUsable(), keyHeld(VK_F));
  if (!keyHeld(VK_F)) switchErrSuppress(SW_SAS, SUPP_INVERT, false);   // keys released elsewhere
  holdService();
}

void panelStabilityInit() {
  holdInit();
  moduleBusSetHandler(MOD_STABILITY, onEvent);
}
