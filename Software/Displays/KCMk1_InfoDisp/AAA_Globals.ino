/***************************************************************************************
   AAA_Globals.ino -- Variable definitions for Kerbal Controller Mk1 Information Display
   All global variable instances are owned here.
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


/***************************************************************************************
   DISPLAY AND TOUCH
****************************************************************************************/
KCM_TFT     infoDisp  = KCM_TFT(KCM_TFT_RS, KCM_TFT_CS, KCM_TFT_RESET);
TouchResult lastTouch;
KCMDoubleBuffer infoDB;


/***************************************************************************************
   SIMPIT OBJECT
   Moved here from SimpitHandler.ino (#9) — owned in globals alongside other
   shared objects. SerialUSB1 is the second USB COM port, dedicated to KSP telemetry.
****************************************************************************************/
KerbalSimpit simpit(SerialUSB1);


/***************************************************************************************
   SCREEN STATE
****************************************************************************************/
ScreenType activeScreen = SCREEN_HOME;   // per-unit home screen (KCMk1_InfoDisp.h)
ScreenType prevScreen   = screen_COUNT;  // sentinel -- forces chrome on first loop


/***************************************************************************************
   FLIGHT STATE
****************************************************************************************/
bool simpitConnected = false;  // true after Simpit handshake succeeds
bool _pendingContextSwitch = false;  // set on vessel change; cleared when FLIGHT_STATUS arrives
bool _pendingDockCheck     = false;  // set after context switch; cleared when TARGETINFO arrives
bool flightScene     = false;  // true when KSP is in a flight scene
bool idleState       = false;  // true when master wants standby when not in flight


/***************************************************************************************
   CELESTIAL BODY (derived from gameSOI — not stored in AppState)
   Updated whenever state.gameSOI changes. Access radius via currentBody.radius.
   Initialised to Kerbin so screens work before first Simpit or demo SOI update.
****************************************************************************************/
BodyParams currentBody;


/***************************************************************************************
   APPLICATION STATE
****************************************************************************************/
AppState state;


/***************************************************************************************
   SWITCH TO SCREEN
   Sets activeScreen and forces a full chrome redraw on the next loop pass by
   resetting prevScreen to the sentinel value screen_COUNT.
   Always use this function — never set activeScreen directly.
****************************************************************************************/
void switchToScreen(ScreenType s) {
  // ORB (Apsides) and ORBADV (Advanced Elements) are now separate sidebar screens;
  // _orbAdvancedMode is derived from the target screen in drawStaticScreen(), so no
  // navigation-time reset is needed here.
  activeScreen     = s;
  prevScreen       = screen_COUNT;
}


/***************************************************************************************
   CONTEXT SCREEN SELECTION
   Returns the most operationally relevant screen for the current vessel state.
   Called on VESSEL_CHANGE_MESSAGE and on entering a flight scene.

   The single-display ladder this replaces interleaved two unrelated questions —
   "what am I flying?" (vessel type) and "what phase am I in?" (mission situation) —
   and had to rank one above the other. Vessel type won, so the phase rules below it
   were masked: a spaceplane on the pad never saw the pre-launch board, and neither a
   spaceplane nor a rover closing on a target ever auto-routed to DOCKING. That was
   not a bug in the ordering; it is what happens when one screen must answer both
   questions. With two panels each ladder gets its own display and neither masks the
   other, so both are complete.

     Info Display 1 (panel A1) — vehicle-type ladder. Holds the PFD family.
     Info Display 2 (panel B1) — mission-phase ladder. Holds the plan/target views.

   contextScreen() is the single entry point; it dispatches on INFO_DISP_UNIT so the
   call sites in SimpitHandler.ino stay unit-agnostic.
****************************************************************************************/

// ── Info Display 1: vehicle-type ladder ─────────────────────────────────────────────
// Answers "what am I flying?" and nothing else. This is pfdContextScreen() plus the
// recoverable-vessel rule: a piece of debris or a spent probe has no useful attitude
// display, so VEHICLE INFO is the right PFD-family member for it.
//
// The pre-launch exclusion is load-bearing. KSP reports a vessel on the pad as
// recoverable — you can recover it without launching — so a bare isRecoverable test
// puts VEHICLE INFO up on the pad, which is exactly where the pilot wants the PFD.
// The old combined ladder never hit this because its pre-launch rule sat above its
// recoverable rule and shielded it; splitting the ladders moved the pre-launch rule
// to the other panel and left this one exposed. The shield is restored explicitly
// rather than by ordering, since there is no longer a pre-launch rule here to hide
// behind. Landed-and-recoverable after a flight still routes to VEHICLE INFO, which
// is the case the rule was written for.
ScreenType vehicleContextScreen() {
  // This panel answers one question and admits no exceptions: what am I flying? The
  // answer is always an instrument for the vehicle — rover, aircraft or spacecraft.
  //
  // VEHICLE INFO used to be routed here for a recoverable vessel, and it is a status
  // summary rather than an instrument, so it belongs to the mission panel — which now
  // routes to it from the surface rule. Keeping it on both produced the one pairing
  // where the two panels showed the same screen (a capsule down and awaiting
  // recovery), and, until the precedence fix, replaced the rover screen with a static
  // summary while the rover was being driven. It remains one press away on this
  // panel's own VEH key.
  return pfdContextScreen();   // rover -> ROVR, plane in atmosphere -> ACFT, else SCFT
}

// ── Info Display 2: mission-phase ladder ────────────────────────────────────────────
// Answers "what phase am I in?" and nothing else, highest priority first:
//   1. Pre-launch                        -> LAUNCH (pre-launch board)
//   2. Descending into an atmosphere     -> RE-ENTRY
//   3. Descending close to the ground    -> POWERED DESCENT
//   4. Target inside docking range       -> DOCKING
//   5. Landed or splashed                -> TARGET if one is set, else VEHICLE INFO
//   6. Burn imminent (or running)        -> MANEUVER
//   7. Target in the approach window     -> TARGET
//   8. On EVA                            -> TARGET
//   9. Flying in an atmosphere, not climbing out -> NAVIGATION
//  10. Everything else                   -> ORBIT
//
// ORBIT as the resting state is the change that the second panel pays for. The
// single-display ladder deliberately never auto-selected it ("Orbit is manual-select
// from the sidebar") because doing so would have stolen the pilot's attitude
// reference. Info Display 1 now holds that permanently, so the plan view is free to
// be where this panel sits when nothing more urgent is happening.
//
// Rules 4 and 5 are new for the same reason, and both are bounded rather than bare
// existence tests — see TGT_CONTEXT_MAX_M / MNVR_CONTEXT_LEAD_S in AAA_Config.ino.
ScreenType missionContextScreen() {
  // 1. Pre-launch -> launch screen (shows the pre-launch board). Unlike the old
  //    combined ladder this is reached by planes and rovers too: their vessel-type
  //    routing now happens on the other panel and no longer masks the phase.
  if (state.situation & sit_PreLaunch)
    return screen_LNCH;

  // 2. Coming down under an atmosphere -> RE-ENTRY. The test is the RE-ENTRY screen's
  //    own corridor classifier, so the rule and the screen cannot disagree about what
  //    counts as a re-entry, and it fires before entry interface — which is when the
  //    pilot wants the corridor tape, not after. The speed gate is what keeps it off
  //    an aircraft in level flight, whose periapsis is far underground and therefore
  //    "in the corridor" by the bare test.
  if (currentBody.hasAtmo && state.verticalVel < 0.0f) {
    ReCorridor corr = _reCorridor();
    int8_t reg = _rePeRegime(corr, state.periapsis);
    bool comingIn = (!state.inAtmo || state.machNumber > REENTRY_CTX_MACH);
    if ((reg == 0 || reg == 1) && comingIn) return screen_LNDGRE;
  }

  // 3. Powered terminal descent -> POWERED DESCENT. Gated on proximity and a real
  //    descent rate rather than on vessel type: the old `type_Lander && sit_SubOrb`
  //    test missed every Ship landing on the Mun, and sub-orbital is also what a
  //    rocket looks like on the way up. A plane in atmosphere is excluded — its
  //    approach instrument is the AIRCRAFT screen on the other panel.
  {
    const float lndgAlt = (activeScreen == screen_LNDG) ? LNDG_CTX_ALT_RELEASE_M
                                                        : LNDG_CTX_ALT_M;
    const bool planeInAir = (state.vesselType == type_Plane && state.inAtmo);
    if (!planeInAir && state.verticalVel < LNDG_CTX_VVERT_MS &&
        state.radarAlt > 0.0f && state.radarAlt < lndgAlt)
      return screen_LNDG;
  }

  // 3. Target within docking range -> docking screen.
  //    Use tgtDistance alone — KSP may report targetAvailable=false even while
  //    actively sending TARGETINFO with a valid distance (observed in KSP1).
  //    The limit widens once DOCKING owns the screen: holding station at exactly
  //    200 m must not oscillate the panel. Same pattern in rules 4 and 5.
  const float dockLim = (activeScreen == screen_DOCK) ? DOCK_CTX_RELEASE_M : DOCK_DIST_WARN_M;
  if (state.tgtDistance > 0.0f && state.tgtDistance <= dockLim)
    return screen_DOCK;

  // On the surface -> TARGET when one is set, otherwise VEHICLE INFO. ORBIT was the
  // fallback for everything, which meant a rover parked on Duna got apoapsis,
  // periapsis, inclination and period — every one of them meaningless on the ground.
  // This sits above the MANEUVER and approach-window rules deliberately: a landed
  // vessel is not flying a burn, and a rover's target is a waypoint at any range, so
  // it wants TARGET whether or not the range falls in the flight approach window.
  if (state.situation & (sit_Landed | sit_Splashed))
    return state.targetAvailable ? screen_TGT : screen_VEH;

  // 4. Maneuver node with an imminent burn -> MANEUVER. Time-to-ignition is measured
  //    to the start of the burn (half the burn duration ahead of the node), matching
  //    the T+Ign the MANEUVER screen itself shows. A negative value means the burn
  //    should already be running, which still belongs on this screen; KSP clears the
  //    node when the burn completes, which drops the rule.
  bool hasMnvr = (state.mnvrTime > 0.0f || state.mnvrDeltaV > 0.0f);
  const float mnvrLead = (activeScreen == screen_MNVR) ? MNVR_CTX_RELEASE_S : MNVR_CONTEXT_LEAD_S;
  if (hasMnvr && (state.mnvrTime - state.mnvrDuration * 0.5f) < mnvrLead)
    return screen_MNVR;

  // 5. Target in the approach window -> TARGET (the RPOD scope). Bounded at both
  //    ends: inside DOCK_DIST_WARN_M rule 3 has already taken it, and past
  //    TGT_CONTEXT_MAX_M the scope has nothing useful to show yet.
  const bool tgtActive = (activeScreen == screen_TGT);
  const float tgtLo = tgtActive ? TGT_CTX_RELEASE_MIN_M : DOCK_DIST_WARN_M;
  const float tgtHi = tgtActive ? TGT_CTX_RELEASE_MAX_M : TGT_CONTEXT_MAX_M;
  if (state.targetAvailable && state.tgtDistance > tgtLo && state.tgtDistance < tgtHi)
    return screen_TGT;

  // On EVA -> TARGET. A Kerbal outside the craft is doing exactly one thing: getting to
  // something. The rules above already cover it once a target is set and close (DOCKING
  // inside 200 m, TARGET out to 2 km); this catches the rest, including the case that
  // matters most -- no target selected, where TARGET's "NO TARGET SET" fullscreen is
  // honest advice rather than a dead end. ORBIT was the alternative, and a Kerbal has an
  // apoapsis in the same sense a thrown wrench does.
  //
  // Below the surface rule deliberately: a Kerbal standing on the Mun is not on an
  // approach, and rule 5 already answers for them.
  if (state.vesselType == type_EVA)
    return screen_TGT;

  // Flying an aircraft inside an atmosphere -> NAVIGATION. ORBIT is the right resting
  // state for something in orbit and says nothing to an aircraft: apoapsis, periapsis,
  // inclination and period are not numbers you fly a jet on.
  //
  // NAVIGATION is a navigation display, and a navigation display exists to pair with an
  // attitude display. The condition is therefore exactly the one the other panel uses to
  // put AIRCRAFT up: NAV appears if and only if its partner PFD is the aircraft PFD, and
  // the two panels are the standard airliner pair or they are not paired at all.
  //
  // Stating it that way rather than as "in an atmosphere" is what keeps a rocket out.
  // A booster a kilometre off the pad is also in an atmosphere with a trivial apoapsis,
  // and the first form of this rule handed it a compass rose during ascent -- then
  // swapped to ORBIT mid-burn, the moment apoapsis crossed the top of the atmosphere.
  //
  // The apoapsis test stays, and does the remaining work: a spaceplane on the way up is
  // still an aircraft by type and still in the atmosphere, but it has already thrown its
  // apoapsis into space and wants exactly what ORBIT shows.
  if (state.vesselType == type_Plane && state.inAtmo &&
      currentBody.hasAtmo && currentBody.lowSpace > 0.0f &&
      state.apoapsis < currentBody.lowSpace)
    return screen_NAV;

  // Everything else -> ORBIT (Apsides).
  return screen_ORB;
}

ScreenType contextScreen() {
#if INFO_DISP_IS_PFD_UNIT
  return vehicleContextScreen();
#else
  return missionContextScreen();
#endif
}


/***************************************************************************************
   MANUAL SELECTION LATCH
   Set by a sidebar press that changes the screen (TouchEvents.ino). An override means
   "not this, now" — not "never again" — so it releases three ways:

     - the situation it was set against passes. _latchedAgainst records what the ladder
       was recommending at the moment of the press; once the ladder's answer changes,
       the pilot's objection is about a situation that no longer exists. Parking on
       ORBIT during a rendezvous therefore holds until the target actually comes inside
       docking range, and then the panel takes over again.
     - the pilot presses the button owning the screen the ladder currently wants, which
       is an explicit "back to auto".
     - vessel change or flight-scene entry, as before.

   Without a release rule, continuous evaluation would mean one exploratory press
   disables automatic routing for the rest of the flight.
****************************************************************************************/
bool       _manualScreenLatch = false;
ScreenType _latchedAgainst    = screen_COUNT;   // ladder's answer when the pilot pressed

void clearManualScreenLatch() {
  _manualScreenLatch = false;
  _latchedAgainst    = screen_COUNT;
}

void setManualScreenLatch() {
  _manualScreenLatch = true;
  _latchedAgainst    = contextScreen();
}

bool contextSwitchAllowed() {
  return !_manualScreenLatch;
}


/***************************************************************************************
   CONTINUOUS CONTEXT ROUTING
   Called once per frame. Previously the ladders ran only at vessel/scene boundaries,
   which meant the panels did not follow the mission: liftoff, reaching orbit, a node
   coming due, a target closing and re-entry all passed without either panel
   reconsidering, and the MANEUVER and TARGET rules were effectively unreachable.

   The dwell is deliberately checked after the latch release, so a change in the
   ladder's answer frees a held override immediately even if the switch itself waits.
****************************************************************************************/
static uint32_t _lastAutoSwitchMs = 0;

void updateContextScreen() {
  const ScreenType want = contextScreen();

  // Release an override whose situation has passed (see the latch notes above).
  if (_manualScreenLatch && want != _latchedAgainst) clearManualScreenLatch();

  if (!contextSwitchAllowed()) return;
  if (want == activeScreen) return;

  const uint32_t now = millis();
  if (now - _lastAutoSwitchMs < CONTEXT_DWELL_MS) return;
  _lastAutoSwitchMs = now;
  switchToScreen(want);
}


/***************************************************************************************
   PFD BUTTON (SPACECRAFT / AIRCRAFT / ROVER / VEHICLE)
   The single PFD sidebar button shows one of four screens. By default the choice is
   made from vessel context; cycling the PFD sidebar button steps through them and
   latches a manual override (mirrors the LNCH ASCENT/CIRC sidebar toggle). VEH is
   only reachable by manual cycle — it is never chosen by context.
****************************************************************************************/
bool    _pfdManualOverride = false;   // true once the pilot cycles PFD via sidebar
uint8_t _pfdManualSel      = 0;       // 0 = SCFT, 1 = ACFT, 2 = ROVR, 3 = VEH

// Context-appropriate PFD screen: rover → ROVR, plane in atmosphere → ACFT,
// everything else → SCFT (spacecraft — the default).
ScreenType pfdContextScreen() {
  if (state.vesselType == type_Rover) return screen_ROVR;
  if (state.vesselType == type_Plane && state.inAtmo) return screen_ACFT;
  return screen_SCFT;
}

// Map a PFD sub-selection index to its screen.
ScreenType pfdScreenForSel(uint8_t sel) {
  return (sel == 3) ? screen_VEH  :
         (sel == 2) ? screen_ROVR :
         (sel == 1) ? screen_ACFT : screen_SCFT;
}

// The PFD screen to show when the PFD button is tapped: the manual selection if the
// pilot has cycled it via title touch, otherwise the context screen.
ScreenType pfdSelectedScreen() {
  return _pfdManualOverride ? pfdScreenForSel(_pfdManualSel) : pfdContextScreen();
}


/***************************************************************************************
   STANDBY SCREEN
   Shown when not in a flight scene (menus, tracking station, etc.).
   Displays the shared splash BMP used by all KCMk1 panels.
****************************************************************************************/
void drawStandbyScreen(KCM_TFT &tft) {
  // Present the full-screen splash through the double buffer: draw it to the
  // hidden page, then flip. During standby the loop returns early (no further
  // flips), so this held image stays on screen.
  infoDB.beginFrame(tft);
  drawStandbySplash(tft);   // #5A delegates to KDC library (subsumes #11 setXY fix)
  infoDB.flip(tft);
}


/***************************************************************************************
   TIME-TO-GROUND ESTIMATION
   Shared by the powered-descent and re-entry landing screens. Replaces the naive
   radarAlt/|verticalVel| estimate with a regime-aware calculation:
     - coasting in vacuum / thin atmosphere -> Keplerian time to the ground radius
     - under thrust or in dense atmosphere  -> kinematic projection using the
       measured vertical acceleration (captures thrust braking and drag)
     - degenerate / near-surface            -> naive constant-velocity fallback
   Returns seconds to ground, or -1.0 when not applicable (caller shows "---").
****************************************************************************************/

// Low-pass-filtered vertical acceleration (m/s^2), owned at module scope so the
// estimate functions can READ it without advancing it.
static float _ttgAccel = 0.0f;

// Advance the vertical-acceleration filter by one sample. MUST be called exactly
// once per frame, before any estimate*() call that reads _ttgAccel. Advancing it
// more than once per frame corrupts the estimate: the extra call sees an unchanged
// verticalVel over a real dt, computes dv/dt = 0, and decays the filter toward
// zero — which made T+Grnd oscillate and its colour flicker green<->yellow.
void ttgAdvanceAccel() {
  static float    prevVv = 0.0f;
  static uint32_t prevMs = 0;
  static bool     have   = false;
  // Only take a new derivative sample when the velocity telemetry actually changed.
  // KSP updates verticalVel slower than our frame rate; sampling on every frame
  // injected dv/dt = 0 on the stale frames, which made the filter ring (decay then
  // spike) and the kinematic time-to-ground jitter. Sampling on change gives a clean
  // dv/dt over the real inter-sample interval.
  if (state.verticalVel != prevVv) {
    uint32_t now = millis();
    if (have) {
      float dt = (float)(now - prevMs) / 1000.0f;
      if (dt > 0.01f && dt < 2.0f)
        _ttgAccel += 0.15f * ((state.verticalVel - prevVv) / dt - _ttgAccel);  // heavy smoothing
    }
    prevVv = state.verticalVel;
    prevMs = now;
    have   = true;
  }
}

// Smallest positive root of  0.5*a*t^2 + v*t + h = 0  (h>0 AGL, v<0 descending).
// Returns -1 if the vessel would arrest its descent before reaching the ground.
static float _ttgKinematic(float h, float v, float a) {
  if (fabsf(a) < 0.05f) return (v < 0.0f) ? (h / -v) : -1.0f;   // ~constant velocity
  float disc = v * v - 2.0f * a * h;
  if (disc < 0.0f) return -1.0f;
  float sq = sqrtf(disc);
  float t1 = (-v - sq) / a, t2 = (-v + sq) / a, t = -1.0f;
  if (t1 > 0.0f)                          t = t1;
  if (t2 > 0.0f && (t < 0.0f || t2 < t))  t = t2;
  return t;
}

// Keplerian time from the current (descending) point to a target orbital radius,
// from the received orbit elements. Valid for elliptical coasting arcs (0 <= e < 1).
// Returns -1 if periapsis is above the target (won't reach it) or elements unusable.
static float _ttgKeplerianToRadius(float r_target) {
  float e = state.eccentricity, a = state.semiMajorAxis, period = state.orbitalPeriod;
  if (a <= 0.0f || period <= 0.0f || e < 0.0f || e >= 1.0f) return -1.0f;
  if (a * (1.0f - e) > r_target) return -1.0f;             // periapsis above target

  float p      = a * (1.0f - e * e);
  float cosNu  = constrain((p / r_target - 1.0f) / e, -1.0f, 1.0f);
  float nu     = acosf(cosNu);                             // [0, PI]
  float Ecc    = 2.0f * atan2f(sqrtf(1.0f - e) * sinf(nu * 0.5f),
                               sqrtf(1.0f + e) * cosf(nu * 0.5f));
  float M      = Ecc - e * sinf(Ecc);                      // mean anomaly from periapsis
  float dtToPe = M / (TWO_PI / period);                    // target crossing -> periapsis
  float t      = state.timeToPe - dtToPe;                  // now -> target crossing
  return (t > 0.0f) ? t : -1.0f;
}

// Sticky display value: hold the previously shown value until the smoothed input
// has moved by at least ~1 s, so a residual sub-second wobble can't make the
// 1-second-resolution readout dither between adjacent values. `shown` is the
// caller's retained state (seeded to <0 for "no value yet").
static float _ttgSticky(float value, float &shown) {
  if (shown < 0.0f || fabsf(value - shown) >= 1.0f) shown = value;
  return shown;
}

// Seconds to the ground, or -1 when not applicable.
// Reads the shared vertical-accel filter; call ttgAdvanceAccel() once per frame first.
float estimateTimeToGround() {
  float aVert = _ttgAccel;
  static float ema = -1.0f;   // smoothed output; <0 means "no valid sample yet"

  static float shown = -1.0f;   // sticky display value (deadbanded)

  // situation is a bitmask (multiple bits can be set) — test with & like contextScreen()
  bool inOrbitOrEscape = (state.situation & (sit_Orbit | sit_Escaping)) != 0;
  if (inOrbitOrEscape || state.radarAlt <= 0.0f || state.verticalVel >= -0.05f) {
    ema = -1.0f; shown = -1.0f;
    return -1.0f;
  }

  bool powered = (state.throttle > 0.02f);
  bool draggy  = (state.inAtmo && state.airDensity > 0.05f);
  float raw = (powered || draggy)
                ? _ttgKinematic(state.radarAlt, state.verticalVel, aVert)
                : _ttgKeplerianToRadius(currentBody.radius + (state.altitude - state.radarAlt));
  if (raw < 0.0f) raw = fabsf(state.radarAlt / state.verticalVel);   // naive fallback

  // Low-pass the final estimate. The kinematic term is sensitive to the (noisy)
  // acceleration, and it can jump ~2x when the arrest boundary flips it to the
  // naive fallback; smoothing keeps the displayed value and its colour band from
  // flickering frame-to-frame. Call ttgAdvanceAccel() once per frame so this
  // advances at the frame rate.
  ema = (ema < 0.0f) ? raw : ema + 0.25f * (raw - ema);
  return _ttgSticky(ema, shown);
}

// Seconds until the vessel descends into the atmosphere (crosses the atmosphere
// top), or -1 when not applicable (airless body, already in atmo, not descending,
// or periapsis above the atmosphere so it won't enter). Same regime split as
// estimateTimeToGround, minus the drag branch (we are above the atmosphere here).
float estimateTimeToAtmosphere() {
  float atmoAlt = currentBody.lowSpace;                    // atmosphere top (0 = airless)
  static float ema   = -1.0f;
  static float shown = -1.0f;
  if (atmoAlt <= 0.0f || state.inAtmo ||
      state.verticalVel >= -0.05f ||                       // not descending
      state.altitude <= atmoAlt ||                         // already at/below the boundary
      state.periapsis > atmoAlt) {                         // periapsis above atmo -> won't enter
    ema = -1.0f; shown = -1.0f;
    return -1.0f;
  }

  float aVert = _ttgAccel;
  bool  powered = (state.throttle > 0.02f);
  float raw = powered
                ? _ttgKinematic(state.altitude - atmoAlt, state.verticalVel, aVert)
                : _ttgKeplerianToRadius(currentBody.radius + atmoAlt);
  if (raw < 0.0f) raw = fabsf((state.altitude - atmoAlt) / state.verticalVel);   // naive fallback

  ema = (ema < 0.0f) ? raw : ema + 0.25f * (raw - ema);
  return _ttgSticky(ema, shown);
}

// Map a time-to-ground (s) to T+Grnd foreground/background colours with hysteresis.
// Escalates to a more urgent band immediately, but relaxes to a calmer band only
// once the estimate clears the threshold by LNDG_TGRND_HYST_S — so an estimate that
// hovers on a boundary can't flip-flop the colour (e.g. green<->yellow) every frame.
// t < 0 => not applicable: returns grey and resets the retained band.
//   band 0 = alarm (white on red), 1 = warn (yellow), 2 = safe (dark green)
void lndgTGroundColors(float t, uint16_t &fg, uint16_t &bg) {
  static uint8_t band = 2;
  const float A = LNDG_TGRND_ALARM_S, W = LNDG_TGRND_WARN_S, H = LNDG_TGRND_HYST_S;

  if (t < 0.0f) { band = 2; fg = TFT_DARK_GREY; bg = TFT_BLACK; return; }

  if (band == 0) {                                  // alarm -> relax needs +H
    band = (t >= A + H) ? ((t >= W + H) ? 2 : 1) : 0;
  } else if (band == 1) {                           // warn
    if      (t < A)      band = 0;                  // escalate immediately
    else if (t >= W + H) band = 2;                  // relax to green needs +H
  } else {                                          // safe / green
    if      (t < A) band = 0;
    else if (t < W) band = 1;                       // escalate immediately
  }

  switch (band) {
    case 0:  fg = TFT_WHITE;      bg = TFT_RED;   break;
    case 1:  fg = TFT_YELLOW;     bg = TFT_BLACK; break;
    default: fg = TFT_DARK_GREEN; bg = TFT_BLACK; break;
  }
}
