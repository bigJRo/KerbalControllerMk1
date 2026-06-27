/***************************************************************************************
   Screen_LNCH.ino -- Launch / Circularization screen — chromeScreen_LNCH, drawScreen_LNCH

   Three modes:
     - PRE-LAUNCH: static checklist board (text-style, unchanged)
     - ASCENT: graphical altitude ladder + V.Vrt bar + velocity dial (left panel),
               numeric readouts (right panel). Auto-enters when sit_PreLaunch ends.
     - ORBITAL (circularization): orbit-diagram view with Ap/Pe markers and vessel
               position (left panel), numeric readouts (right panel). Auto-switches
               at altitude > 6% of body radius (matches KSP navball auto-switch).

   Phase switching:
     - sit_PreLaunch → PRE-LAUNCH (auto, via SimpitHandler)
     - sit_PreLaunch clears → ASCENT
     - alt > bodyRad * 0.06 → ORBITAL (with hysteresis at 0.055 descending)
     - alt < hysteresis → ASCENT
     - Pilot tap: toggle manual override (stops auto-switching, flips current mode)

****************************************************************************************/
#include "KCMk1_InfoDisp.h"


bool _lnchOrbitalMode      = false;
bool _lnchManualOverride   = false;  // true = pilot has overridden auto phase switch
bool _lnchPrelaunchMode    = false;  // true = sit_PreLaunch board is showing
bool _lnchPrelaunchDismissed = false; // true = pilot tapped to dismiss; don't re-enter

// ── Ascent phase geometry ─────────────────────────────────────────────────────────────
// Content area: x=0..719, y=63..479 (717×417 usable)
// Left panel (graphics): x=0..452 (453 wide)
// Right panel (numeric readouts): x=453..719 (267 wide)
// Panel split matches the SCFT screen convention: right panel starts at x=453,
// with a 2-px vertical divider in the gap (at x=451, 452). Right-panel fonts
// also match SCFT: Roboto_Black_20 labels, Roboto_Black_24 values.
static const int16_t LNCH_AS_LPANEL_X    = 0;
static const int16_t LNCH_AS_LPANEL_W    = 453;
static const int16_t LNCH_AS_RPANEL_X    = 453;
static const int16_t LNCH_AS_RPANEL_W    = 267;
static const int16_t LNCH_AS_PANEL_Y     = 63;   // just below title bar
static const int16_t LNCH_AS_PANEL_H     = 417;  // content below title, to bottom

// Right panel: 8 stacked numeric readouts, 52 px each (8 × 52 = 416, fits in 417).
// Each row uses library printDispChrome for label+border and printValue for value.
// Font: Roboto_Black_24 — large enough to read easily, small enough that worst-case
// launch values like "+3,234" fit within the value region after label+padding.
static const int16_t LNCH_AS_ROW_H       = 52;

// Row Y positions
static inline int16_t _lnchAsRowY(uint8_t row) {
    return LNCH_AS_PANEL_Y + row * LNCH_AS_ROW_H;
}


// ── Top-level dispatchers ──────────────────────────────────────────────────────────────
// chromeScreen_LNCH is called once on screen entry / SOI change, drawScreen_LNCH on
// every frame. Both delegate to the active phase's draw functions, which live in:
//   - Screen_LNCH_PreLaunch.ino (when sit_PreLaunch is set)
//   - Screen_LNCH_Ascent.ino    (when below switch altitude)
//   - Screen_LNCH_Circ.ino      (when above switch altitude)
// The phase-mode flags _lnchPrelaunchMode and _lnchOrbitalMode are set by SimpitHandler
// (pre-launch transitions) and by drawScreen_LNCH itself (altitude hysteresis).

static void chromeScreen_LNCH(RA8875 &tft) {
  // _lnchOrbitalMode is set by drawScreen_LNCH via hysteresis before chrome is called.
  if (_lnchPrelaunchMode) {
    // ── PRE-LAUNCH board (see Screen_LNCH_PreLaunch.ino) ──
    _lnchPrelaunchDrawChrome(tft);
    return;
  }

  if (!_lnchOrbitalMode) {
    // ── Ascent phase (graphical) ──
    // Left panel: altitude ladder + V.Vrt bar + V.Orb bar + FPA dial + atmosphere gauge
    // Right panel: 8 numeric readouts stacked vertically
    _lnchAsResetState();
    _lnchAsDrawLeftPanelChrome(tft);
    _lnchAsDrawRightPanelChrome(tft);

  } else {
    // ── Orbital (circularization) phase ──
    // Left panel: BLANK (placeholder for Stage 6 — orbit diagram with Ap/Pe).
    //   drawStaticScreen already did fillScreen(TFT_BLACK) before calling this,
    //   so nothing to draw for now.
    // Right panel: 8 numeric readouts — same layout style as ascent but with
    //   orbital-specific row set (Alt.SL / V.Orb / ApA / PeA / T+Ap / Thrtl /
    //   T.Brn / ΔV.Stg) and orbital change-detection state.
    _lnchOrResetState();
    _lnchOrDrawRightPanelChrome(tft);
  }
}


static void drawScreen_LNCH(RA8875 &tft) {
  static const uint8_t NR = 8;

  // ── PRE-LAUNCH board (see Screen_LNCH_PreLaunch.ino) ──
  if (_lnchPrelaunchMode) {
    _lnchPrelaunchDrawValues(tft);
    return;
  }

  // Phase detection with hysteresis — ascending uses higher threshold to prevent
  // rapid switching near the boundary.
  float bodyRad   = (currentBody.radius > 0.0f) ? currentBody.radius : 600000.0f;
  bool  ascending = (state.verticalVel >= 0.0f);
  float switchAlt = ascending ? (bodyRad * 0.06f) : (bodyRad * 0.055f);
  bool  orbMode   = (state.altitude > switchAlt);

  // Phase switch — auto only if not manually overridden
  if (!_lnchManualOverride && orbMode != _lnchOrbitalMode) {
    _lnchOrbitalMode = orbMode;
    for (uint8_t r = 0; r < NR; r++) rowCache[0][r].value = "\x01";
    switchToScreen(screen_LNCH);
    return;
  }

  // Manual override indicator — red dot on right of title bar.
  // Drawn every loop: red when overridden, black (erase) when auto.
  // Position matches drawStaticScreen: x=706, y=29, r=6.
  {
    uint16_t indCol = _lnchManualOverride ? TFT_RED : TFT_BLACK;
    tft.fillCircle(706, 29, 6, indCol);
  }

  if (!_lnchOrbitalMode) {
    // =========================================================
    // ASCENT PHASE (graphical)
    // Left panel: altitude ladder + V.Vrt + V.Orb bars + FPA dial + atmosphere gauge
    // Right panel: 8 numeric readouts, each with its own change detection
    //
    // Uses _lnchOrbitalMode (persistent state) rather than the freshly-computed
    // `orbMode` (altitude-based), so that manual override mode correctly draws
    // the ascent visuals even at altitudes above the auto-switch threshold.
    // =========================================================
    _lnchAsDrawLeftPanelValues(tft);
    _lnchAsDrawRightPanelValues(tft);
    return;
  }

  // =========================================================
  // ORBITAL PHASE (circularization)
  // Left panel: orbit graphic (ellipse + body + Ap/Pe markers + target circle)
  // Right panel: 8 numeric readouts with orbital-specific row set
  // =========================================================
  _lnchOrDrawLeftPanelValues(tft);
  _lnchOrDrawRightPanelValues(tft);
}
