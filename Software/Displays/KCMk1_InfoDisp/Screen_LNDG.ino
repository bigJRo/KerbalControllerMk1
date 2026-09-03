/***************************************************************************************
   Screen_LNDG.ino -- Landing screen — chromeScreen_LNDG, drawScreen_LNDG

   Two modes:
     - POWERED DESCENT (Screen_LNDG_Powered.ino) — graphical altitude tape + X-Pointer +
       ATT + V.Vrt bar (default)
     - RE-ENTRY (Screen_LNDG_Reentry.ino)        — graphical: corridor tape, heat-shield/
       retrograde alignment ball, chute deploy envelope, VSI/G/thermal gauges + text panel

   The two are sibling sidebar screens (screen_LNDG / screen_LNDGRE): a repeat press
   on the LNDG key cycles them, and AAA_Screens.ino derives _lndgReentryMode from the
   active screen before dispatching here.

   On vessel switch, all mode flags + parachute state are reset (SimpitHandler.ino).
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


// ── Mode flags + re-entry row-label toggles ────────────────────────────────────────────
// Externally accessible (declared extern in KCMk1_InfoDisp.h). _lndgReentryMode is
// the master toggle; row toggles drive label-swapping in re-entry mode.
bool _lndgReentryMode    = false;
bool _lndgReentryRow0TPe  = false;
bool _lndgReentryRow1SL   = false;

// ── Parachute deploy/cut latched state ─────────────────────────────────────────────────
// These are EXTERN globals (declared in KCMk1_InfoDisp.h) because SimpitHandler resets
// them on vessel switch. They're only consumed by the re-entry mode for chute status
// rendering, but the canonical definitions live here so the dispatcher file owns them.
bool _drogueDeployed  = false;
bool _mainDeployed    = false;
bool _drogueCut       = false;
bool _mainCut         = false;
bool _drogueArmedSafe = false;
bool _mainArmedSafe   = false;


// ── Top-level dispatchers ──────────────────────────────────────────────────────────────
// chromeScreen_LNDG is called once on screen entry / mode change, drawScreen_LNDG on
// every frame. Both delegate to the active mode's draw functions, which live in:
//   - Screen_LNDG_Powered.ino (when _lndgReentryMode is false)
//   - Screen_LNDG_Reentry.ino (when _lndgReentryMode is true)

static void chromeScreen_LNDG(KCM_TFT &tft) {
    if (!_lndgReentryMode) _lndgChromePowered(tft);
    else                   _lndgChromeReentry(tft);
}


static void drawScreen_LNDG(KCM_TFT &tft) {
    if (!_lndgReentryMode) _lndgDrawPowered(tft);
    else                   _lndgDrawReentry(tft);
}
