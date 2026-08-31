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
bool _lnchCoastLatched     = false;  // true once the ascent burn has ended (see below)
bool _lnchManualOverride   = false;  // true = pilot has overridden auto phase switch
bool _lnchPrelaunchMode    = false;  // true = sit_PreLaunch board is showing
bool _lnchPrelaunchDismissed = false; // true = pilot tapped to dismiss; don't re-enter

// ── Ascent phase geometry ─────────────────────────────────────────────────────────────
// Six panel constants used to live here describing a 717x417 content area split at
// x=453 -- the rev-1 720x480 screen. This panel has been 1024x600 since rev-2 and the
// live ASCENT layout is in Screen_LNCH_Ascent.ino, so every one of them was both unused
// and wrong. Only the title offset survives, and it is still correct.
static const int16_t LNCH_AS_PANEL_Y     = 63;   // just below the title bar


// ── Top-level dispatchers ──────────────────────────────────────────────────────────────
// chromeScreen_LNCH is called once on screen entry / SOI change, drawScreen_LNCH on
// every frame. Both delegate to the active phase's draw functions, which live in:
//   - Screen_LNCH_PreLaunch.ino (when sit_PreLaunch is set)
//   - Screen_LNCH_Ascent.ino    (when below switch altitude)
//   - Screen_LNCH_Circ.ino      (when above switch altitude)
// The phase-mode flags _lnchPrelaunchMode and _lnchOrbitalMode are set by SimpitHandler
// (pre-launch transitions) and by drawScreen_LNCH itself (altitude hysteresis).

static void chromeScreen_LNCH(KCM_TFT &tft) {
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
    // Left panel: orbit diagram (Ap/Pe) — drawn per-frame by _lnchOrDrawLeftPanelValues,
    //   so it needs no static chrome here (drawStaticScreen already cleared to black).
    // Right panel: 8 numeric readouts — same layout style as ascent but with
    //   orbital-specific row set (Alt.SL / V.Orb / ApA / PeA / T+Ap / Thrtl /
    //   T.Brn / ΔV.Stg) and orbital change-detection state.
    _lnchOrResetState();
    _lnchOrDrawRightPanelChrome(tft);
  }
}


static void drawScreen_LNCH(KCM_TFT &tft) {
  static const uint8_t NR = 8;

  // ── PRE-LAUNCH board (see Screen_LNCH_PreLaunch.ino) ──
  if (_lnchPrelaunchMode) {
    _lnchPrelaunchDrawValues(tft);
    return;
  }

  // ── Phase detection: ASCENT until the coast begins, CIRCULARISATION after ──────────
  //
  // The launch arc has three ordered stages and this screen has two modes, so ASCENT is
  // the powered ascent and CIRCULARISATION is everything from the coast onward.
  //
  // This used to switch on altitude, at 6% of body radius — 36 km on Kerbin. Altitude
  // is the wrong axis for it. A spaceplane can have its apoapsis parked above the
  // atmosphere while still burning at 35 km, and a Mun ascent finishes below the Kerbin
  // threshold entirely, so one number cannot serve both. What actually separates the
  // stages is the engine: you are ascending while you are pushing apoapsis up, and
  // coasting once you stop.
  //
  // Two conditions, because throttle alone is not enough:
  //
  //   Throttle closed. The direct signal, and the only one that works on an airless
  //   body, where there is no atmosphere boundary to cross.
  //
  //   Apoapsis already at or above the orbit-safe altitude — the ascent burn has done
  //   its job. Without this a throttle-down through max Q, or a staging gap, would read
  //   as a coast a minute after liftoff.
  //
  // And it latches. The circularisation burn re-opens the throttle, so a live test would
  // drop back to ASCENT for the one burn CIRCULARISATION exists to fly. Once the coast
  // has started the ascent is over.
  //
  // It clears on the surface and whenever apoapsis falls back below the line. The
  // surface test is what makes a Mun ascent start in ASCENT: SimpitHandler clears the
  // latch on leaving the pad, which covers a launch from KSC but not a departure from
  // another body's surface, where the vessel is sit_Landed rather than sit_PreLaunch --
  // and a lander parked high on the Mun can have an apoapsis above minSafe while
  // standing still. Sitting on the ground is not a coast.
  const float orbitSafeAlt = (currentBody.minSafe > currentBody.lowSpace)
                               ? currentBody.minSafe : currentBody.lowSpace;
  const bool  apoapsisParked = (orbitSafeAlt > 0.0f && state.apoapsis >= orbitSafeAlt);
  const bool  onSurface      = (state.situation & (sit_Landed | sit_Splashed | sit_PreLaunch)) != 0;

  if (onSurface || !apoapsisParked)               _lnchCoastLatched = false;
  else if (state.throttle <= LNCH_COAST_THROTTLE) _lnchCoastLatched = true;

  bool orbMode = _lnchCoastLatched;

  // Phase switch — auto only if not manually overridden
  if (!_lnchManualOverride && orbMode != _lnchOrbitalMode) {
    _lnchOrbitalMode = orbMode;
    for (uint8_t r = 0; r < NR; r++) rowCache[0][r].value = "\x01";
    switchToScreen(screen_LNCH);
    return;
  }

  // The per-frame manual-override dot that lived here is gone — the panel-level
  // AUTO/MAN chip (updateModeChip) annunciates this screen's phase override too,
  // and it occupies the same corner of the title bar.

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
