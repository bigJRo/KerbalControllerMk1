/***************************************************************************************
   Screen_SCFT.ino  --  Spacecraft screen  (EADI style)

   EADI GEOMETRY
   -------------
   The aircraft symbol is FIXED at screen centre (CX, CY), always horizontal.
   The ball moves behind it:
     Ball centre:  BCX = CX - pitchPx*sinR
                   BCY = CY + pitchPx*cosR
   The horizon passes through (BCX, BCY), tilted at roll angle R.
   Horizon line equation:  sinR*x - cosR*y = K
     where K = sinR*CX - cosR*CY - pitchPx  (independent of roll)
   Sky side:    sinR*x - cosR*y > K
   Ground side: sinR*x - cosR*y <= K

   The perpendicular distance from (CX,CY) to the horizon = pitchPx always.
   At lad_p=pitch: rung foot = (CX,CY) exactly at all roll angles.

   PITCH LADDER
   ------------
   Rung at lad_p degrees:
     foot = (BCX + lad_p*SCALE*sinR,  BCY - lad_p*SCALE*cosR)
   Rung line direction = (cosR, sinR)  (parallel to horizon).
   Sky-ward normal = (sinR, -cosR).

   DISC
   ----
   Fixed on screen, centred at (CX, CY), radius R.
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


// ── Geometry ──────────────────────────────────────────────────────────────────────────
// ONE PFD, ONE SET OF NUMBERS -- see the same note at the head of Screen_ACFT.ino.
// This screen and AIRCRAFT draw the same instrument with the same renderer, and the
// geometry used to be written out in full three times over, each re-derived from its own
// copy of CX/CY/R. EADIBall.ino holds the single definition; these are aliases so the
// chrome below still reads in this screen's own vocabulary. SPACECRAFT differs from
// AIRCRAFT only in what it omits (VSI, slip, AoA) and in its row content, never in where
// anything sits.
static const int16_t  SCFT_CX        = EADI_CX;
static const int16_t  SCFT_CY        = EADI_CY;
static const int16_t  SCFT_R         = EADI_R;

// ── Right panel geometry ───────────────────────────────────────────────────────────────
static const int16_t  SCFT_PANEL_X       = SCFT_CX - (SCFT_R*2+54)/2 + (SCFT_R*2+54) + 2; // 580
static const int16_t  SCFT_PANEL_RIGHT   = CONTENT_W;   // 940 — abuts the sidebar divider
static const int16_t  SCFT_PANEL_W       = SCFT_PANEL_RIGHT - SCFT_PANEL_X;  // 360 (= reticle panel)
static const uint8_t  SCFT_PANEL_NR      = 8;

// ── Right panel state ──────────────────────────────────────────────────────────────────
// Cache managed by rowCache[screen_SCFT] / printState[screen_SCFT] slots 0-8.
// No additional state variables needed — printValue handles dirty detection.

// ── Per-frame state ───────────────────────────────────────────────────────────────────

static float    _scftPrevPitch   = -9999.0f;
static float    _scftPrevRoll    = -9999.0f;
bool            _scftPrevOrbMode = false;

// Active velocity vector — set each frame by drawScreen_SCFT based on orbMode.
// Both tapes and the panel V.Orb/V.Srf row read from these.
static float    _scftVelHdg   = 0.0f;
static float    _scftVelPitch = 0.0f;
static bool     _scftFullRedrawNeeded = true;



// ── Orbital mode helpers ──────────────────────────────────────────────────────────────
// One altitude test used to drive three different things: row 1's V.Orb/V.Srf, row 5's
// T+Ign/V.Vrt, and the prograde marker on the ball and both tapes. Only two of those are
// a REFERENCE choice. Row 5's swap is a PHASE decision -- am I near the ground, is
// descent rate the number being flown -- that merely happened to reuse the same test.
//
// The distinction did not matter while both were automatic, because they always agreed.
// It matters the moment the pilot can override the reference: forcing ORB at 10 km during
// a re-entry must not trade descent rate away for a meaningless ignition countdown. So
// the two are separate predicates, and only the reference one is overridable.
static bool _scftAutoOrb() {
    float bodyRad   = (currentBody.radius > 0.0f) ? currentBody.radius : DEFAULT_BODY_RADIUS_M;
    bool  ascending = (state.verticalVel >= 0.0f);
    float switchAlt = ascending ? (bodyRad * ORB_SWITCH_ALT_FRAC_ASC) : (bodyRad * ORB_SWITCH_ALT_FRAC_DESC);
    return state.altitude > switchAlt;
}

// The velocity reference actually in force: automatic unless the pilot has pinned one.
ModeOverride _scftVelRefOverride;
// Chip cache: -1 unknown, 0 auto-SRF, 1 auto-ORB, 2 held-SRF, 3 held-ORB. Declared here
// rather than beside its draw function because chromeScreen_SCFT resets it, and the
// Arduino builder hoists prototypes but not variables.
static int8_t _scftChipShown = -1;
static bool _scftVelRef() { return modeResolve(_scftVelRefOverride, _scftAutoOrb()); }

// ── EVA mode ──────────────────────────────────────────────────────────────────────────
// A Kerbal outside the craft is still a vessel as far as KSP and Simpit are concerned,
// so it lands here on the vehicle panel — and the attitude ball is genuinely the right
// instrument, because you do orient on EVA. The numeric panel beside it was not: a
// Kerbal has no stage, no burn and no apoapsis worth reading, so rows 2-6 were showing
// dV.Stg, ApA, PeA, T+Ap and T+Ign, none of which mean anything to a person holding a
// jetpack. Those five rows swap to what an EVA is actually flown on: how high above the
// surface, how fast relative to it, and how far from and how quickly closing on whatever
// you are trying to reach.
//
// Row 6 becomes suit charge. dV.Stg would read a flat green 0 m/s, and electric charge
// is the number that ends an EVA -- ROVER already reads the same field for the same
// reason.
static inline bool _scftEvaMode() { return state.vesselType == type_EVA; }
static bool _scftPrevEvaMode = false;


// ── Row 5: descent rate, ignition countdown, or the burn itself ───────────────────────
// Three states, not two. Below the mode switch the row is V.Vrt (see the chrome, which
// explains why altitude rate is phase-specific instrumentation on every real spacecraft
// that flies it). Above the switch it is T+Ign -- and, once that burn is actually under
// way, dV.Rem counting down.
//
// The countdown is Apollo's. The CSM Entry Monitor System's dV counter was set before a
// burn and counted to zero as the SPS thrust; the crew shut down on it, and it was the
// most-watched number in the burn. Nothing on this controller counted a burn down before
// this: MANEUVER carries Brg, Elv, Burn.Dur, T+Ign and T+Mnvr, and no dV at all.
//
// Row 5 is the right home because the fit is exact -- a countdown TO ignition is
// meaningless once ignition has happened, so the row is free precisely when the burn
// number is wanted. V.Vrt's claim below the switch is untouched: a noded burn under
// 36 km is rare, and descent rate still wins there if one happens.
//
// One difference from the EMS worth knowing. Apollo's counter integrated an accelerometer
// along the thrust axis, so it measured what the engine actually delivered and pointing
// error showed up as a residual that would not null. state.mnvrDeltaV is the guidance
// number -- what the node still needs -- which nulls correctly however the craft is
// pointed, and therefore hides that error. It is the better number to fly on; it is not
// the same instrument.
static const uint8_t SCFT_R5_VVRT  = 0;
static const uint8_t SCFT_R5_TIGN  = 1;
static const uint8_t SCFT_R5_DVREM = 2;

// Seconds until the burn should light, negative once it should already have.
static float _scftTIgn() {
    return (state.mnvrTime > 0.0f) ? (state.mnvrTime - state.mnvrDuration / 2.0f) : 1.0f;
}

static uint8_t _scftRow5Mode() {
    if (!_scftAutoOrb()) return SCFT_R5_VVRT;   // phase, not reference -- see above
    const bool hasNode  = (state.mnvrTime > 0.0f) && (state.mnvrDeltaV > 0.0f);
    const bool burning  = hasNode && (state.throttle > 0.01f) && (_scftTIgn() <= 0.0f);
    return burning ? SCFT_R5_DVREM : SCFT_R5_TIGN;
}

static const char *_scftRow5Label(uint8_t mode) {
    return (mode == SCFT_R5_VVRT)  ? "V.Vrt:"
         : (mode == SCFT_R5_DVREM) ? "\xCE\x94V.Rem:"
                                   : "T+Ign:";
}

// The label follows the throttle, so it cannot wait for a full chrome repaint the way
// the orbMode swap does. Only row 5's chrome is redrawn, and its cache slot is cleared
// so printValue repaints the value that printDispChrome just filled over.
static uint8_t _scftPrevRow5Mode = 255;





// Scanline fill, aircraft symbol, and disc clip are shared with ACFT — see EADIBall.ino
// (eadiDrawScanline / eadiDrawAircraftSymbol / eadiClipToDisk).








// ── ADI marker state (ball-side tracking — separate from tape-side state) ─────────────
// When any of these changes meaningfully, the ball must be redrawn so markers can be
// repositioned. Ball markers are drawn as the last layer of the ball (just before the
// aircraft symbol), so they're naturally wiped and re-rendered on any ball redraw.
static float _scftPrevHeadingBall    = -9999.0f;
static float _scftPrevVelHdgBall     = -9999.0f;
static float _scftPrevVelPitchBall   = -9999.0f;
static float _scftPrevMnvrHdgBall    = -9999.0f;
static float _scftPrevMnvrPitchBall  = -9999.0f;
static float _scftPrevTgtHdgBall     = -9999.0f;
static float _scftPrevTgtPitchBall   = -9999.0f;
static bool  _scftPrevMnvrActiveBall = false;
static bool  _scftPrevTgtAvailBall   = false;





// ── Roll indicator state ──────────────────────────────────────────────────────────────
static int16_t  _scftPrevRollReadout   = -9999;      // last drawn roll readout (integer degrees)
static uint16_t _scftPrevRollReadoutFg = 0;          // last drawn foreground colour

// ── Pitch readout state ───────────────────────────────────────────────────────────────
static int16_t  _scftPrevPitchReadout   = -9999;
static uint16_t _scftPrevPitchReadoutFg = 0;


// Roll readout — geometry is EADI_ROLL_* in EADIBall.ino, which is what actually draws
// it. This screen only chooses the colour (a fixed dark green -- no roll warnings).

// Update the roll numeric readout — spacecraft uses a fixed dark-green (no roll warnings).
// Scaffolding/geometry live in the shared eadiUpdateRollReadout(); see EADIBall.ino.
static void _scftUpdateRollReadout(KCM_TFT &tft, float roll) {
    uint16_t fg = TFT_DARK_GREEN;  // spacecraft — no roll warnings
    uint16_t bg = TFT_BLACK;
    eadiUpdateRollReadout(tft, roll, fg, bg,
                          _scftPrevRollReadout, _scftPrevRollReadoutFg);
}

// ── Pitch tape ────────────────────────────────────────────────────────────────────────
// Shared availability state (also used by heading tape)
static bool    _scftPrevTgtAvail   = false;
static bool    _scftPrevMnvrActive = false;
// Vertical tape on the left side of the disc. Same scale as the pitch ladder.
// ±30° visible, ticks at every 5° (minor) and 10° (major).
// Current value box centred on disc centre (pitch=0 line).
// Markers: left-pointing triangles on the right edge for vel/tgt/mnvr pitch.

// Only the outer frame and the value box are drawn here; the ticks, labels, markers and
// suppress zone belong to eadiDrawPitchTape.
static const int16_t  SCFT_PTAPE_W       = EADI_PTAPE_W;       // 36
static const int16_t  SCFT_PTAPE_X       = EADI_PTAPE_X;       // 76
static const int16_t  SCFT_PTAPE_Y       = EADI_PTAPE_Y;       // 94  — top of disc
static const int16_t  SCFT_PTAPE_H       = EADI_PTAPE_H;       // 420 — bottom meets the HDG tape
static const int16_t  SCFT_PTAPE_BOX_W   = EADI_PTAPE_BOX_W;   // 68
static const int16_t  SCFT_PTAPE_BOX_H   = EADI_PTAPE_BOX_H;   // 38
static const int16_t  SCFT_PTAPE_BOX_X   = EADI_PTAPE_BOX_X;   // 44
static const int16_t  SCFT_PTAPE_BOX_Y   = EADI_PTAPE_BOX_Y;   // 281 — centred on pitch = 0

// State
static float   _scftPrevPitch2      = -9999.0f;   // pitch tape (distinct from ball state)
static int16_t _scftPrevPitchBox    = -9999;
static float   _scftPrevVelPitch    = -9999.0f;
static float   _scftPrevTgtPitch    = -9999.0f;
static float   _scftPrevMnvrPitch   = -9999.0f;

// Draw/update the pitch value box — delegated to the shared helper (see EADIBall.ino).
static void _scftUpdatePitchBox(KCM_TFT &tft, float pitch) {
    eadiUpdatePitchBox(tft, pitch, _scftPrevPitchBox);
}

// Draw the full pitch tape. Spacecraft markers: velocity (prograde) always, target when
// available, maneuver when a node exists. Scaffolding lives in the shared helper.
static void _scftDrawPitchTape(KCM_TFT &tft, float pitch) {
    EadiTapeMarker mk[3];
    uint8_t n = 0;
    mk[n++] = { _scftVelPitch, TFT_NEON_GREEN };
    if (state.targetAvailable) mk[n++] = { state.tgtPitch,  TFT_VIOLET };
    if (state.mnvrTime > 0.0f) mk[n++] = { state.mnvrPitch, TFT_BLUE };
    eadiDrawPitchTape(tft, pitch, mk, n);
}

// Update pitch tape — redraws when pitch or any marker pitch changes.
static void _scftUpdatePitchTape(KCM_TFT &tft, float pitch) {
    bool mnvrActive = (state.mnvrTime > 0.0f);
    bool dirty = fabsf(pitch - _scftPrevPitch2)                   >= 0.2f
              || fabsf(_scftVelPitch         - _scftPrevVelPitch)  >= 0.2f
              || fabsf(state.tgtPitch       - _scftPrevTgtPitch)  >= 0.2f
              || fabsf(state.mnvrPitch      - _scftPrevMnvrPitch) >= 0.2f
              || state.targetAvailable != _scftPrevTgtAvail
              || mnvrActive           != _scftPrevMnvrActive;

    if (dirty) {
        _scftDrawPitchTape(tft, pitch);
        _scftPrevPitch2     = pitch;
        _scftPrevVelPitch   = _scftVelPitch;
        _scftPrevTgtPitch   = state.tgtPitch;
        _scftPrevMnvrPitch  = state.mnvrPitch;
        // Note: _scftPrevTgtAvail and _scftPrevMnvrActive shared with heading tape
    }
    _scftUpdatePitchBox(tft, pitch);
}

// ── Heading tape state ────────────────────────────────────────────────────────────────
static float   _scftPrevHeading    = -9999.0f;
static int16_t _scftPrevHdgBox     = -9999;
static float   _scftPrevVelHdg     = -9999.0f;
static float   _scftPrevTgtHdg     = -9999.0f;
static float   _scftPrevMnvrHdg    = -9999.0f;

// ── Heading tape geometry ─────────────────────────────────────────────────────────────
// As above: frame and box only. Ticks, labels, markers and the suppress zone are
// eadiDrawHeadingTape's.
static const int16_t  SCFT_HDG_TAPE_W    = EADI_HDG_TAPE_W;    // 466 — ±35° visible
static const int16_t  SCFT_HDG_TAPE_X    = EADI_HDG_TAPE_X;    // 112
static const int16_t  SCFT_HDG_TAPE_Y    = EADI_HDG_TAPE_Y;    // 514 — 8 px below the disc
static const int16_t  SCFT_HDG_TAPE_H    = EADI_HDG_TAPE_H;    // 32
static const int16_t  SCFT_HDG_BOX_W     = EADI_HDG_BOX_W;     // 72
static const int16_t  SCFT_HDG_BOX_H     = EADI_HDG_BOX_H;     // 40 — 8 px taller than the tape,
static const int16_t  SCFT_HDG_BOX_X     = EADI_HDG_BOX_X;     //   so its bottom border sits
static const int16_t  SCFT_HDG_BOX_Y     = EADI_HDG_BOX_Y;     //   outside the fill and can't flicker

// Draw/update the heading number box — delegated to the shared helper (see EADIBall.ino).
static void _scftUpdateHdgBox(KCM_TFT &tft, float hdg) {
    eadiUpdateHdgBox(tft, hdg, _scftPrevHdgBox);
}

// Draw the full heading tape. Spacecraft markers: velocity (prograde) always, target when
// available, maneuver when a node exists. Scaffolding lives in the shared helper, which
// also forces _scftPrevHdgBox to -1 (the fill blackens the box interior).
static void _scftDrawHeadingTape(KCM_TFT &tft, float hdg) {
    EadiTapeMarker mk[3];
    uint8_t n = 0;
    mk[n++] = { _scftVelHdg, TFT_NEON_GREEN };
    if (state.targetAvailable) mk[n++] = { state.tgtHeading,  TFT_VIOLET };
    if (state.mnvrTime > 0.0f) mk[n++] = { state.mnvrHeading, TFT_BLUE };
    eadiDrawHeadingTape(tft, hdg, _scftPrevHdgBox, mk, n);
}

// Update heading — tape redraws when heading or any marker heading changes.
static void _scftUpdateHeadingTape(KCM_TFT &tft, float hdg) {
    bool mnvrActive = (state.mnvrTime > 0.0f);
    bool dirty = fabsf(hdg - _scftPrevHeading)             >= 0.5f
              || fabsf(_scftVelHdg - _scftPrevVelHdg)       >= 0.5f
              || fabsf(state.tgtHeading - _scftPrevTgtHdg) >= 0.5f
              || fabsf(state.mnvrHeading - _scftPrevMnvrHdg) >= 0.5f
              || state.targetAvailable != _scftPrevTgtAvail
              || mnvrActive           != _scftPrevMnvrActive;

    if (dirty) {
        _scftDrawHeadingTape(tft, hdg);
        _scftPrevHeading    = hdg;
        _scftPrevVelHdg     = _scftVelHdg;
        _scftPrevTgtHdg     = state.tgtHeading;
        _scftPrevMnvrHdg    = state.mnvrHeading;
        _scftPrevTgtAvail   = state.targetAvailable;
        _scftPrevMnvrActive = mnvrActive;
    }
    _scftUpdateHdgBox(tft, hdg);
}




// ── Throttle bar (left strip — occupies the ACFT VSI slot) ────────────────────────────
// Vertical 0–100% gauge: 0% at the bottom, 100% at the top, filled bottom-up. No
// danger thresholds — throttle is a control input, so it uses a single amber fill.
static const int16_t THR_X       = 2;
static const int16_t THR_BAR_W   = 18;
static const int16_t THR_TICK_X0 = THR_X + THR_BAR_W;      // 20
static const int16_t THR_LABEL_H = 62;                     // rotated "THR" label at bottom
static const int16_t THR_TOP_Y   = SCFT_PTAPE_Y + 6;       // 100 — 100% mark sits below the
                                                           //   "Pitch:" label (bottom y=92)
static const int16_t THR_BOT_Y   = SCREEN_H - THR_LABEL_H - 14; // 524 — 0% mark; range chosen so the
                                                           //   20% ticks bracket (not overlap) the
                                                           //   pitch value box (y 281..319)
static const int16_t THR_TRACK_H = THR_BOT_Y - THR_TOP_Y;       // 424
static float _scftPrevThrottle   = -9999.0f;

static void _scftDrawThrottleChrome(KCM_TFT &tft) {
    // Right border of the strip
    tft.drawLine(THR_TICK_X0 + 1, TITLE_TOP, THR_TICK_X0 + 1, SCREEN_H - 1, TFT_GREY);
    tft.setFont(Roboto_Black_12);
    for (int16_t p = 0; p <= 100; p += 5) {
        int16_t ty    = THR_BOT_Y - (int16_t)((float)p / 100.0f * THR_TRACK_H);
        bool    major = (p % 20 == 0);
        // Major ticks (every 20%) — long, light grey, labelled. Minor ticks
        // (every 5%) — short, dark grey, no label. Matches the VSI/pitch-tape style.
        tft.drawLine(THR_TICK_X0, ty, THR_TICK_X0 + (major ? 10 : 6), ty,
                     major ? TFT_LIGHT_GREY : TFT_GREY);
        if (major) {
            tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
            tft.setCursor(THR_TICK_X0 + 12, ty - 6);
            char lbl[4]; snprintf(lbl, sizeof(lbl), "%d", p);
            tft.print(lbl);
        }
    }
    // "THR" label — vertical, at the bottom, matching the Pitch/Hdg label style
    drawVerticalText(tft, THR_X, SCREEN_H - THR_LABEL_H, THR_BAR_W, THR_LABEL_H,
                     &Roboto_Black_16, "THR", TFT_WHITE, TFT_BLACK);
}

static void _scftUpdateThrottle(KCM_TFT &tft, float throttle) {
    if (fabsf(throttle - _scftPrevThrottle) < 0.005f) return;
    _scftPrevThrottle = throttle;
    float   clamped = constrain(throttle, 0.0f, 1.0f);
    int16_t fillH   = (int16_t)(clamped * THR_TRACK_H);
    tft.fillRect(THR_X, THR_TOP_Y, THR_BAR_W, THR_TRACK_H, TFT_BLACK);
    if (fillH > 0)
        tft.fillRect(THR_X, THR_BOT_Y - fillH, THR_BAR_W, fillH, TFT_ORANGE);
}


// ── Vitals strip (bottom — occupies the ACFT slip slot) ───────────────────────────────
// Three horizontal bars: Electric Charge %, hottest core temp %, hottest skin temp %.
// EC is green when high (low = bad); temps are green when low (high = bad).
static const int16_t VIT_Y         = SCFT_HDG_BOX_Y + SCFT_HDG_BOX_H + 2; // just below the HDG box
static const int16_t VIT_ROW_H     = 14;
static const int16_t VIT_BAR_H     = 10;
static const int16_t VIT_LBL_RIGHT = SCFT_PTAPE_X + SCFT_PTAPE_W;      // 112 — labels right-align here
                                                                      //   (= pitch tape right edge)
static const int16_t VIT_BAR_X     = VIT_LBL_RIGHT + 4;               // 116 — bars start just right of it
static const int16_t VIT_RIGHT     = SCFT_HDG_TAPE_X + SCFT_HDG_TAPE_W; // 578 — strip right edge (divider)
static const int16_t VIT_VAL_W     = 46;                              // value column ("100%")
static const int16_t VIT_BAR_RIGHT = VIT_RIGHT - VIT_VAL_W;           // 532 — bar track right edge
static int16_t _scftPrevEC   = -9999;
static int16_t _scftPrevCore = -9999;
static int16_t _scftPrevSkin = -9999;

static inline int16_t _scftVitBarX() { return VIT_BAR_X; }
static inline int16_t _scftVitBarW() { return VIT_BAR_RIGHT - VIT_BAR_X; }
static inline int16_t _scftVitBarY(uint8_t row) {
    return VIT_Y + row * VIT_ROW_H + (VIT_ROW_H - VIT_BAR_H) / 2;
}

static void _scftVitalsChrome(KCM_TFT &tft) {
    tft.setFont(Roboto_Black_12);
    static const char *labels[] = {"EC", "CORE", "SKIN"};
    for (uint8_t i = 0; i < 3; i++) {
        int16_t ry = VIT_Y + i * VIT_ROW_H;
        int16_t lw = getFontStringWidth(&Roboto_Black_12, labels[i]);
        tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
        tft.setCursor(VIT_LBL_RIGHT - lw, ry);   // right-aligned to the pitch tape edge
        tft.print(labels[i]);
        tft.drawRect(_scftVitBarX(), _scftVitBarY(i), _scftVitBarW(), VIT_BAR_H, TFT_GREY);
    }
}

// Draw one vitals bar fill + value. lowIsBad=true → low % is the alarm (EC).
// Thresholds are the shared annunciator-aligned values (EC: 20%/5%, temp: 75/90).
static void _scftDrawVitalBar(KCM_TFT &tft, uint8_t row, int16_t pct, bool lowIsBad) {
    pct = constrain(pct, (int16_t)0, (int16_t)100);
    uint16_t col;
    if (lowIsBad)   // electric charge — low charge is the alarm
        col = (pct < (int16_t)(EC_LOW_ALARM_FRAC * 100.0f)) ? TFT_RED :
              (pct < (int16_t)(EC_LOW_WARN_FRAC  * 100.0f)) ? TFT_YELLOW : TFT_DARK_GREEN;
    else            // temperature — high temp is the alarm
        col = (pct > TEMP_ALARM_PCT) ? TFT_RED :
              (pct > TEMP_WARN_PCT)  ? TFT_YELLOW : TFT_DARK_GREEN;
    int16_t bx = _scftVitBarX(), bw = _scftVitBarW(), by = _scftVitBarY(row);
    int16_t fillW = (int16_t)((float)pct / 100.0f * (bw - 2));
    tft.fillRect(bx + 1, by + 1, bw - 2, VIT_BAR_H - 2, TFT_OFF_BLACK);
    if (fillW > 0) tft.fillRect(bx + 1, by + 1, fillW, VIT_BAR_H - 2, col);
    // Warn/alarm threshold ticks — same treatment as the re-entry temp bars.
    float warnFrac  = lowIsBad ? EC_LOW_WARN_FRAC  : TEMP_WARN_PCT  / 100.0f;
    float alarmFrac = lowIsBad ? EC_LOW_ALARM_FRAC : TEMP_ALARM_PCT / 100.0f;
    int16_t xw = bx + 1 + (int16_t)(warnFrac  * (bw - 2));
    int16_t xa = bx + 1 + (int16_t)(alarmFrac * (bw - 2));
    tft.drawLine(xw, by + 1, xw, by + VIT_BAR_H - 2, TFT_YELLOW);
    tft.drawLine(xa, by + 1, xa, by + VIT_BAR_H - 2, TFT_RED);
    // Value text — right-aligned in the value column (right of the bar track)
    int16_t ry = VIT_Y + row * VIT_ROW_H;
    char buf[6]; snprintf(buf, sizeof(buf), "%d%%", pct);
    int16_t vw = getFontStringWidth(&Roboto_Black_12, buf);
    tft.fillRect(VIT_BAR_RIGHT, ry, VIT_VAL_W, VIT_ROW_H, TFT_BLACK);
    tft.setFont(Roboto_Black_12);
    tft.setTextColor(col, TFT_BLACK);
    tft.setCursor(VIT_RIGHT - vw - 2, ry);
    tft.print(buf);
}

static void _scftUpdateVitals(KCM_TFT &tft) {
    int16_t ec   = (int16_t)roundf(state.electricChargePercent);
    int16_t core = (int16_t)state.coreTempPct;
    int16_t skin = (int16_t)state.skinTempPct;
    if (ec   != _scftPrevEC)   { _scftDrawVitalBar(tft, 0, ec,   true);  _scftPrevEC   = ec; }
    if (core != _scftPrevCore) { _scftDrawVitalBar(tft, 1, core, false); _scftPrevCore = core; }
    if (skin != _scftPrevSkin) { _scftDrawVitalBar(tft, 2, skin, false); _scftPrevSkin = skin; }
}


// ── Attitude rate pointers ────────────────────────────────────────────────────────────
// The one conspicuous thing both real attitude balls carry that this one did not. The
// Shuttle ADI has three rate pointers on the ball's edges (full scale selectable +/-1,
// +/-5 or +/-10 deg/s on the ADI RATE switch) and the Apollo FDAI has the same three
// needles, because a spacecraft is flown on rates: an RCS pulse is judged by the rate it
// produces, not by where the ball settles ten seconds later.
//
// Rates come from kcmRateUpdate (KerbalDisplayCommon), which differentiates the ROTATION
// rather than the Euler angles so it stays well conditioned near vertical -- see the
// header there, including the bench test that is still outstanding.
//
// PLACEMENT, and why it is not the Shuttle's. The Shuttle puts roll along the top of the
// ball and yaw along the bottom, so each pointer moves the way its axis moves. There is
// no room for that here, and the numbers say so rather than the eye: the throttle scale
// runs to x~54, the pitch tape occupies 75..112, the disc bezel is 137..553, the bank
// labels reach x=130 and x=556 at y=174..193, the heading tape starts at y=514 and the
// panel divider is at x=578. That leaves three usable columns -- 18 px, 23 px and 20 px
// wide -- and nothing horizontal wider than 23 px anywhere on the screen.
//
// So all three share one column to the right of the ball, stacked, all vertical, all
// reading the same way: UP IS POSITIVE, with positive meaning roll right, nose up, nose
// right. Consistency between the three is worth more than matching a layout we cannot
// fit, and keeping them together is how a rate cluster is actually scanned.
static const float   SCFT_RATE_FS     = 10.0f;   // deg/s at either end of every bar

static const int16_t SCFT_RATE_X      = 557;     // clear of the "60" bank label (556)
static const int16_t SCFT_RATE_W      = 20;      //   and the panel divider (578)
static const int16_t SCFT_RATE_LBL_H  = 14;      // single-letter name above each bar
// VERTICAL BUDGET. The column is boxed in at both ends and both numbers are measured
// against the real glyph data rather than guessed:
//   TOP  -- the roll readout is right-justified into x 497..583, so it lands squarely on
//           this column. Its value row paints y 97..130 ("+180" at Roboto_Black_28,
//           cap 33, centred in the 38 px value row below the 30 px label row from
//           TITLE_TOP=62). The first version started the stack at y=96, which put the "P"
//           label and the top 20 px of the pitch bar inside that box -- so every time the
//           roll digits changed they were erased. Start at 136 to clear it with margin.
//   BOTTOM-- the TRIM flag: "TRIM" is 59 px at Roboto_Black_24 (cap 29), right-aligned to
//           the heading tape's right edge (578) less 8, so x 511..570 -- squarely on this
//           column too -- and its erase rect starts at y=473. The stack must end above it.
// That leaves 136..471, i.e. 335 px for 3*(14 + BAR_H) + 2*8, so BAR_H <= 92.
static const int16_t SCFT_RATE_BAR_H  = 92;
static const int16_t SCFT_RATE_PITCH_Y = 136;                                   // R
static const int16_t SCFT_RATE_ROLL_Y  = SCFT_RATE_PITCH_Y + SCFT_RATE_LBL_H + SCFT_RATE_BAR_H + 8;
static const int16_t SCFT_RATE_YAW_Y   = SCFT_RATE_ROLL_Y  + SCFT_RATE_LBL_H + SCFT_RATE_BAR_H + 8;

static const uint16_t SCFT_RATE_FILL  = TFT_WHITE;

static KcmRateTracker _scftRates;
static const int16_t  SCFT_RATE_RESET = INT16_MIN;
static int16_t _scftPrevRollPx  = SCFT_RATE_RESET;
static int16_t _scftPrevPitchPx = SCFT_RATE_RESET;
static int16_t _scftPrevYawPx   = SCFT_RATE_RESET;

// Signed pixel offset from a bar's zero line, clamped to the track.
static int16_t _scftRatePx(float degPerSec) {
    const int16_t halfSpan = SCFT_RATE_BAR_H / 2 - 1;
    float f = degPerSec / SCFT_RATE_FS;
    if (f >  1.0f) f =  1.0f;
    if (f < -1.0f) f = -1.0f;
    return (int16_t)lroundf(f * (float)halfSpan);
}

static inline int16_t _scftRateBarY(int16_t rowY) { return rowY + SCFT_RATE_LBL_H; }

// Track outline, zero tick and single-letter name for all three bars.
static void _scftDrawRateChrome(KCM_TFT &tft) {
    static const char *names[3] = { "P", "R", "Y" };
    const int16_t rowY[3] = { SCFT_RATE_PITCH_Y, SCFT_RATE_ROLL_Y, SCFT_RATE_YAW_Y };
    tft.setFont(Roboto_Black_12);
    tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
    for (uint8_t i = 0; i < 3; i++) {
        const int16_t lw = getFontStringWidth(&Roboto_Black_12, names[i]);
        tft.setCursor(SCFT_RATE_X + (SCFT_RATE_W - lw) / 2, rowY[i]);
        tft.print(names[i]);
        const int16_t by = _scftRateBarY(rowY[i]);
        tft.drawRect(SCFT_RATE_X, by, SCFT_RATE_W, SCFT_RATE_BAR_H, TFT_GREY);
        const int16_t zy = by + SCFT_RATE_BAR_H / 2;
        tft.drawLine(SCFT_RATE_X - 3, zy, SCFT_RATE_X + SCFT_RATE_W + 2, zy, TFT_LIGHT_GREY);
    }
}

// Repaint one bar's fill. Only the union of the old and new extents is touched, so a
// pointer trembling around zero costs a few pixels rather than a full-track fill --
// these move on almost every frame, unlike the vitals bars they sit near.
static void _scftDrawRateFill(KCM_TFT &tft, int16_t rowY, int16_t oldPx, int16_t newPx) {
    const int16_t lo = (int16_t)min((int)(oldPx < 0 ? oldPx : 0), (int)(newPx < 0 ? newPx : 0));
    const int16_t hi = (int16_t)max((int)(oldPx > 0 ? oldPx : 0), (int)(newPx > 0 ? newPx : 0));
    if (lo == 0 && hi == 0) return;
    const int16_t zy = _scftRateBarY(rowY) + SCFT_RATE_BAR_H / 2;   // + is up
    tft.fillRect(SCFT_RATE_X + 1, zy - hi, SCFT_RATE_W - 2, (hi - lo), TFT_BLACK);
    if (newPx > 0)      tft.fillRect(SCFT_RATE_X + 1, zy - newPx, SCFT_RATE_W - 2, newPx,  SCFT_RATE_FILL);
    else if (newPx < 0) tft.fillRect(SCFT_RATE_X + 1, zy,         SCFT_RATE_W - 2, -newPx, SCFT_RATE_FILL);
}

static void _scftUpdateRates(KCM_TFT &tft) {
    const bool first = (_scftPrevPitchPx == SCFT_RATE_RESET);
    if (!kcmRateUpdate(_scftRates, state.heading, state.pitch, state.roll, millis()) && !first)
        return;

    const int16_t pp = _scftRatePx(_scftRates.pitch);
    const int16_t rp = _scftRatePx(_scftRates.roll);
    const int16_t yp = _scftRatePx(_scftRates.yaw);
    const int16_t pPrev = first ? 0 : _scftPrevPitchPx;
    const int16_t rPrev = first ? 0 : _scftPrevRollPx;
    const int16_t yPrev = first ? 0 : _scftPrevYawPx;

    if (pp != pPrev || first) _scftDrawRateFill(tft, SCFT_RATE_PITCH_Y, pPrev, pp);
    if (rp != rPrev || first) _scftDrawRateFill(tft, SCFT_RATE_ROLL_Y,  rPrev, rp);
    if (yp != yPrev || first) _scftDrawRateFill(tft, SCFT_RATE_YAW_Y,   yPrev, yp);

    _scftPrevPitchPx = pp; _scftPrevRollPx = rp; _scftPrevYawPx = yp;
}


// ── Screen chrome ─────────────────────────────────────────────────────────────────────
static void chromeScreen_SCFT(KCM_TFT &tft) {
    eadiBallResetState();
    _scftFullRedrawNeeded      = true;
    eadiResetRollIndicator();
    _scftPrevRollReadout       = -9999;
    _scftPrevRollReadoutFg     = 0;
    _scftPrevPitchReadout      = -9999;
    _scftPrevPitchReadoutFg    = 0;
    _scftPrevPitch2            = -9999.0f;
    _scftPrevPitchBox          = -9999;
    _scftPrevVelPitch          = -9999.0f;
    _scftPrevTgtPitch          = -9999.0f;
    _scftPrevMnvrPitch         = -9999.0f;
    _scftPrevHeading           = -9999.0f;
    _scftPrevHdgBox            = -9999;
    _scftPrevVelHdg            = -9999.0f;
    _scftPrevTgtHdg            = -9999.0f;
    _scftPrevMnvrHdg           = -9999.0f;
    _scftPrevTgtAvail          = false;
    _scftPrevMnvrActive        = false;
    // Ball-side marker trackers (distinct from tape-side; see _scftDrawAdiMarker)
    _scftPrevHeadingBall       = -9999.0f;
    _scftPrevVelHdgBall        = -9999.0f;
    _scftPrevVelPitchBall      = -9999.0f;
    _scftPrevMnvrHdgBall       = -9999.0f;
    _scftPrevMnvrPitchBall     = -9999.0f;
    _scftPrevTgtHdgBall        = -9999.0f;
    _scftPrevTgtPitchBall      = -9999.0f;
    _scftPrevMnvrActiveBall    = false;
    _scftPrevTgtAvailBall      = false;
    // Panel values reset via rowCache invalidation (row cache cleared on screen switch)
    // Ball delta-fill state (chord table, dirty bitmaps, prev horizon) reset above.
    _scftPrevPitch        = -9999.0f;
    _scftPrevRoll         = -9999.0f;
    _scftPrevThrottle     = -9999.0f;
    _scftPrevEC           = -9999;
    _scftPrevCore         = -9999;
    _scftPrevSkin         = -9999;
    _scftChipShown = -1;
    kcmRateReset(_scftRates);
    _scftPrevRollPx       = SCFT_RATE_RESET;
    _scftPrevPitchPx      = SCFT_RATE_RESET;
    _scftPrevYawPx        = SCFT_RATE_RESET;

    // Bezel ring
    tft.drawCircle(SCFT_CX, SCFT_CY, SCFT_R,     TFT_LIGHT_GREY);
    tft.drawCircle(SCFT_CX, SCFT_CY, SCFT_R + 1, TFT_WHITE);
    tft.drawCircle(SCFT_CX, SCFT_CY, SCFT_R + 2, TFT_DARK_GREY);

    // Bank scale tick marks outside the disc circumference (upper arc)
    static const int16_t ticks[] = {-60,-45,-30,-20,-10,0,10,20,30,45,60};
    for (uint8_t i = 0; i < 11; i++) eadiDrawBankTick(tft, ticks[i]);

    // Labels at ±30° and ±60° — drawn at R+28 along the tick radial (Roboto_Black_16)
    static const int16_t labelTicks[] = {-60, -30, 30, 60};
    for (uint8_t i = 0; i < 4; i++) eadiDrawBankLabel(tft, labelTicks[i]);

    // Heading box border
    tft.drawRect(SCFT_HDG_BOX_X, SCFT_HDG_BOX_Y, SCFT_HDG_BOX_W, SCFT_HDG_BOX_H, TFT_LIGHT_GREY);

    // Pitch tape chrome — value box border and three-sided tape border (top, right, bottom)
    tft.drawRect(SCFT_PTAPE_BOX_X, SCFT_PTAPE_BOX_Y, SCFT_PTAPE_BOX_W, SCFT_PTAPE_BOX_H, TFT_LIGHT_GREY);
    // Top, right, bottom lines — right and bottom drawn 1px inward to align with HDG tape borders
    tft.drawLine(SCFT_PTAPE_X - 1,                  SCFT_PTAPE_Y - 1,
                 SCFT_PTAPE_X + SCFT_PTAPE_W - 1,    SCFT_PTAPE_Y - 1, TFT_LIGHT_GREY);
    tft.drawLine(SCFT_PTAPE_X + SCFT_PTAPE_W - 1,    SCFT_PTAPE_Y - 1,
                 SCFT_PTAPE_X + SCFT_PTAPE_W - 1,    SCFT_PTAPE_Y + SCFT_PTAPE_H - 1, TFT_GREY);
    tft.drawLine(SCFT_PTAPE_X - 1,                  SCFT_PTAPE_Y + SCFT_PTAPE_H - 1,
                 SCFT_PTAPE_X + SCFT_PTAPE_W - 1,    SCFT_PTAPE_Y + SCFT_PTAPE_H - 1, TFT_LIGHT_GREY);

    // "Pitch:" right-justified to right edge of pitch value box (box widened left
    // so the larger _24 label — 75px — fits without clipping the pitch value box)
    textRight(tft, &Roboto_Black_24,
              SCFT_PTAPE_BOX_X - 24, SCFT_PTAPE_Y - 32,
              SCFT_PTAPE_BOX_W + 24, 30,
              "Pitch:", TFT_WHITE, TFT_BLACK);

    // Heading tape border — top (light grey) and two sides (darker), open at bottom
    tft.drawLine(SCFT_HDG_TAPE_X - 1, SCFT_HDG_TAPE_Y - 1,
                 SCFT_HDG_TAPE_X + SCFT_HDG_TAPE_W, SCFT_HDG_TAPE_Y - 1, TFT_LIGHT_GREY);
    tft.drawLine(SCFT_HDG_TAPE_X - 1, SCFT_HDG_TAPE_Y - 1,
                 SCFT_HDG_TAPE_X - 1, SCFT_HDG_TAPE_Y + SCFT_HDG_TAPE_H, TFT_GREY);
    tft.drawLine(SCFT_HDG_TAPE_X + SCFT_HDG_TAPE_W, SCFT_HDG_TAPE_Y - 1,
                 SCFT_HDG_TAPE_X + SCFT_HDG_TAPE_W, SCFT_HDG_TAPE_Y + SCFT_HDG_TAPE_H, TFT_GREY);

    // "Hdg:" right-justified to same right edge as "Pitch:" label
    textRight(tft, &Roboto_Black_24,
              SCFT_PTAPE_BOX_X, SCFT_HDG_BOX_Y + SCFT_HDG_BOX_H / 2 - 15,
              SCFT_PTAPE_BOX_W, 30,
              "Hdg:", TFT_WHITE, TFT_BLACK);

    // ── Throttle bar (left) + vitals strip (bottom) — fill the ACFT VSI/slip slots ─────
    _scftDrawThrottleChrome(tft);
    _scftVitalsChrome(tft);
    _scftDrawRateChrome(tft);

    // ── Right panel chrome ─────────────────────────────────────────────────────────────
    // Vertical divider (2px) between ADI and panel
    tft.drawLine(SCFT_PANEL_X - 2, TITLE_TOP, SCFT_PANEL_X - 2, SCREEN_H - 1, TFT_GREY);
    tft.drawLine(SCFT_PANEL_X - 1, TITLE_TOP, SCFT_PANEL_X - 1, SCREEN_H - 1, TFT_GREY);

    static const tFont *PF = &Roboto_Black_28;   // label font — matches reticle/launch panels

    // Rows 0-6: single-width rows. Rows 1 and 5 depend on orbMode (see below).
    static const char *panelLabels[] = {
        "Alt.SL:", nullptr, "ApA:", "PeA:", "T+Ap:", nullptr, "\xCE\x94V.Stg:"
    };
    // On EVA the same seven rows carry a different set — see _scftEvaMode(). Row 1 is
    // pinned to V.Orb here rather than following orbMode, so that V.Orb and V.Srf are
    // both always on the panel: the pair is how you tell station-keeping from drifting.
    static const char *evaPanelLabels[] = {
        "Alt.SL:", "V.Orb:", "Alt.Rdr:", "V.Srf:", "Dist:", "V.Close:", "EC:"
    };
    const bool evaChrome = _scftEvaMode();
    for (uint8_t r = 0; r < 7; r++) {
        // Row 5 is T+Ign in orbit and V.Vrt below the mode-switch altitude. Real
        // spacecraft treat altitude rate as a phase-specific instrument rather than a
        // permanent one: the Shuttle's PFD carries a vertical-speed tape only in its
        // entry configuration (on orbit it shows the ADI ball alone) and its AVVI's
        // radar portion is live only in MM305; Apollo gave the LM a dedicated ALT RATE
        // tapemeter because the LM lands, while the CM had none and read altitude rate
        // from the DSKY on demand (V06 N62); Orion puts altitude rate on its entry
        // format. This row does the same thing with the switch the screen already runs.
        //
        // The trade is exact. Above the switch a node countdown is the useful half --
        // and the PFD is the ONLY place it appears outside MANEUVER's ten-minute window,
        // so it must not simply be dropped. Below the switch a node countdown is close
        // to meaningless and descent rate is the number being flown.
        const char *lbl = evaChrome ? evaPanelLabels[r]
                        : (r == 1) ? (_scftVelRef() ? "V.Orb:" : "V.Srf:")
                        : (r == 5) ? _scftRow5Label(_scftRow5Mode())
                        : panelLabels[r];
        printDispChrome(tft, PF, SCFT_PANEL_X, rowYFor(r, SCFT_PANEL_NR),
                        SCFT_PANEL_W, rowHFor(SCFT_PANEL_NR),
                        lbl, COL_LABEL, COL_BACK, COL_NO_BDR);
    }

    // Chrome has just painted row 5 with whichever label was current; record it so the
    // update below only relabels when it actually changes.
    _scftPrevRow5Mode = evaChrome ? (uint8_t)255 : _scftRow5Mode();

    // Row 7 — split divider (2px)
    {
        uint16_t hw = SCFT_PANEL_W / 2;
        tft.drawLine(SCFT_PANEL_X + hw,     TITLE_TOP + 7 * rowHFor(SCFT_PANEL_NR),
                     SCFT_PANEL_X + hw,     SCREEN_H - 1, TFT_GREY);
        tft.drawLine(SCFT_PANEL_X + hw + 1, TITLE_TOP + 7 * rowHFor(SCFT_PANEL_NR),
                     SCFT_PANEL_X + hw + 1, SCREEN_H - 1, TFT_GREY);
    }
}


// ── Right panel update ─────────────────────────────────────────────────────────────────
// Uses printValue + rowCache[screen_SCFT] exactly as LNDG/ORB screens do.
// Cache slots 0-9: Alt.SL, V.Orb, ApA, PeA, T+Ap/Pe, T+Ign, ΔV.Stg, RCS, SAS label, SAS value
static void _scftUpdatePanel(KCM_TFT &tft, bool orbMode) {
    static const tFont *VF  = &Roboto_Black_36;   // value font — matches reticle/launch panels
    static const uint8_t SC = (uint8_t)screen_SCFT;

    bool hasMnvr  = (state.mnvrTime > 0.0f);
    bool hasOrbit = (state.apoapsis > 0.0f || state.periapsis > 0.0f);
    bool evaMode  = _scftEvaMode();
    bool hasTgt   = state.targetAvailable;
    uint16_t fw = SCFT_PANEL_W;
    uint16_t hw = SCFT_PANEL_W / 2;

    auto attPanelVal = [&](uint8_t row, uint8_t slot, const char *label,
                           const String &val, uint16_t fg, uint16_t bg) {
        drawPanelValue(tft, SC, slot, row, SCFT_PANEL_X, fw, label, val, fg, bg, VF, SCFT_PANEL_NR, true);
    };

    // Row 0 — Alt.SL
    {
        uint16_t fg = TFT_DARK_GREEN;
        String val = hasOrbit ? formatAlt(state.altitude) : "---";
        attPanelVal(0, 0, "Alt.SL:", val, fg, TFT_BLACK);
    }

    // Row 1 — V.Orb or V.Srf depending on orbital mode; pinned to V.Orb on EVA, where
    // row 3 carries V.Srf and the two are read as a pair.
    {
        uint16_t fg = TFT_DARK_GREEN;
        const char *lbl = (evaMode || orbMode) ? "V.Orb:" : "V.Srf:";
        float vel = (evaMode || orbMode) ? state.orbitalVel : state.surfaceVel;
        String val = hasOrbit ? fmtMs(vel) : "---";
        attPanelVal(1, 1, lbl, val, fg, TFT_BLACK);
    }

    // Rows 2-6 on EVA — height above the surface, drift, and the approach numbers.
    // Returns early: the split row 7 (RCS/SAS) and the dividers below are shared, so
    // they are drawn by the common tail, not duplicated here.
    if (evaMode) {
        // Row 2 — Alt.Rdr. The one altitude that matters when a Kerbal is near a surface.
        attPanelVal(2, 2, "Alt.Rdr:", formatAlt(state.radarAlt), TFT_DARK_GREEN, TFT_BLACK);

        // Row 3 — V.Srf. Beside V.Orb above: station-keeping vs drifting.
        attPanelVal(3, 3, "V.Srf:", fmtMs(state.surfaceVel), TFT_DARK_GREEN, TFT_BLACK);

        // Row 4 — Dist to target, dashed when nothing is selected.
        attPanelVal(4, 4, "Dist:", hasTgt ? formatAlt(state.tgtDistance) : String("---"),
                    hasTgt ? TFT_DARK_GREEN : TFT_DARK_GREY, TFT_BLACK);

        // Row 5 — V.Close. Same sign convention as TGT/DOCK: negative is closing.
        attPanelVal(5, 5, "V.Close:", hasTgt ? fmtMs(state.tgtVelocity) : String("---"),
                    hasTgt ? TFT_DARK_GREEN : TFT_DARK_GREY, TFT_BLACK);

        // Row 6 — suit charge, on ROVER's thresholds (5% alarm, 20% caution).
        {
            float ec = constrain(state.electricChargePercent, 0.0f, 100.0f);
            uint16_t fg, bg;
            thresholdColor(ec,
                           EC_PCT_ALARM, TFT_WHITE,  TFT_RED,
                           EC_PCT_WARN,  TFT_YELLOW, TFT_BLACK,
                           TFT_DARK_GREEN, TFT_BLACK, fg, bg);
            char buf[8];
            snprintf(buf, sizeof(buf), "%d%%", (int)lroundf(ec));
            attPanelVal(6, 6, "EC:", String(buf), fg, bg);
        }
    } else {

    // Row 2 — ApA
    {
        // Escape trajectory: apoapsis undefined (KSP reports it negative) -> infinity
        bool escape = hasOrbit && (state.apoapsis < 0.0f);
        String val  = !hasOrbit ? String("---") : (escape ? String("\x80") : formatAlt(state.apoapsis));
        attPanelVal(2, 2, "ApA:", val, TFT_DARK_GREEN, TFT_BLACK);
    }

    // Row 3 — PeA
    {
        // Suborbital periapsis is informational here (not a launch/descent screen) -> neutral
        String val = hasOrbit ? formatAlt(state.periapsis) : "---";
        attPanelVal(3, 3, "PeA:", val, TFT_DARK_GREEN, TFT_BLACK);
    }

    // Row 4 — T+Ap or T+Pe (whichever is sooner and positive)
    {
        float tAp = state.timeToAp, tPe = state.timeToPe;
        float tNext; const char *lbl;
        if (!hasOrbit)                        { tNext = -1.0f; lbl = "T+Ap:"; }
        else if (tAp >= 0.0f && tPe >= 0.0f) { tNext = min(tAp,tPe); lbl = (tAp < tPe) ? "T+Ap:" : "T+Pe:"; }
        else if (tAp >= 0.0f)                 { tNext = tAp; lbl = "T+Ap:"; }
        else                                  { tNext = tPe; lbl = "T+Pe:"; }
        String val = (tNext >= 0.0f) ? formatTimeCompact((int64_t)tNext) : "---";
        attPanelVal(4, 4, lbl, val, TFT_DARK_GREEN, TFT_BLACK);
    }

    // Row 5 — V.Vrt below the mode switch, T+Ign above it, dV.Rem once that burn is
    // actually under way. See _scftRow5Mode above for why all three live on one row.
    {
        const uint8_t r5 = _scftRow5Mode();
        // The label follows the throttle, so unlike the orbMode swap it cannot wait for
        // a screen re-entry. Repaint just this row's chrome and clear its cache slot,
        // since printDispChrome has filled over the value printValue last wrote.
        if (r5 != _scftPrevRow5Mode) {
            printDispChrome(tft, &Roboto_Black_28, SCFT_PANEL_X,
                            rowYFor(5, SCFT_PANEL_NR), SCFT_PANEL_W,
                            rowHFor(SCFT_PANEL_NR), _scftRow5Label(r5),
                            COL_LABEL, COL_BACK, COL_NO_BDR);
            rowCache[SC][5] = RowCache();
            printState[SC][5] = PrintState();
            _scftPrevRow5Mode = r5;
        }

        uint16_t fg = TFT_DARK_GREEN, bg = TFT_BLACK;
        String val;
        if (r5 == SCFT_R5_VVRT) {
            // Reported, not alarmed. LNDG's thresholds are LANDING thresholds -- alarm at
            // -8 m/s, caution at -5 -- and reusing them here would paint the row red for
            // the whole of a normal descent, which is the always-on alarm that teaches a
            // pilot to ignore the colour. POWERED DESCENT owns that alarm and has the
            // radar altitude to justify it; the Shuttle's AVVI is likewise unalarmed.
            val = fmtMs(state.verticalVel);
        } else if (r5 == SCFT_R5_DVREM) {
            // Unalarmed for the same reason the EMS counter was: it is watched, not
            // warned on. The pilot shuts down when it reaches zero.
            val = fmtMs(state.mnvrDeltaV);
        } else {
            const float tIgn = hasMnvr ? _scftTIgn() : -1.0f;
            if (!hasMnvr)                     { fg = TFT_DARK_GREY; val = "---"; }
            else if (tIgn < 0.0f)             { fg = TFT_WHITE; bg = TFT_RED;
                                                val = formatTimeCompact((int64_t)tIgn); }
            else if (tIgn < MNVR_TIGN_WARN_S) { fg = TFT_YELLOW;
                                                val = formatTimeCompact((int64_t)tIgn); }
            else                              { val = formatTimeCompact((int64_t)tIgn); }
        }
        attPanelVal(5, 5, _scftRow5Label(r5), val, fg, bg);
    }

    // Row 6 — ΔV.Stg (low-stage-fuel warning, matches VEH/LNCH)
    {
        uint16_t fg, bg;
        thresholdColor(state.stageDeltaV,
                       DV_STG_ALARM_MS, TFT_WHITE,  TFT_RED,
                       DV_STG_WARN_MS,  TFT_YELLOW, TFT_BLACK,
                       TFT_DARK_GREEN, TFT_BLACK, fg, bg);
        attPanelVal(6, 6, "\xCE\x94V.Stg:", fmtMs(state.stageDeltaV), fg, bg);
    }
    }   // end of the non-EVA rows 2-6

    // Row 7 split — RCS button (left half) | SAS button (right half).
    // Shared: RCS and SAS are exactly as meaningful for a Kerbal on a jetpack.
    {
        uint16_t ry  = TITLE_TOP + 7 * rowHFor(SCFT_PANEL_NR);  // full row top (no ROW_PAD)
        uint16_t rh  = SCREEN_H - ry - 1;                        // bottom border on row 598 (599 overscanned)
        uint16_t sasX = SCFT_PANEL_X + hw;
        uint16_t sasW = SCFT_PANEL_RIGHT - sasX;                 // fills remainder exactly

        // RCS button (slot 7)
        {
            bool rcsOn = state.rcs_on;
            String rcsStr = rcsOn ? "ON" : "OFF";
            RowCache &rc = rowCache[SC][7];
            if (rc.value != rcsStr) {
                ButtonLabel btn = rcsOn
                    ? ButtonLabel{ "RCS", TFT_WHITE,     TFT_WHITE,     TFT_DARK_GREEN, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }
                    : ButtonLabel{ "RCS", TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK,  TFT_OFF_BLACK,  TFT_GREY, TFT_GREY };
                drawButton(tft, SCFT_PANEL_X, ry, hw, rh, btn, &Roboto_Black_28, false);
                rc.value = rcsStr;
            }
        }

        // SAS button (slot 8)
        {
            const char *v; uint16_t fg, bg;
            sasNavballLabel(state.sasMode, v, fg, bg);
            RowCache &rc = rowCache[SC][8];
            if (rc.value != v || rc.fg != fg || rc.bg != bg) {
                ButtonLabel btn = { v, fg, fg, bg, bg, TFT_GREY, TFT_GREY };
                drawButton(tft, sasX, ry, sasW, rh, btn, &Roboto_Black_28, false);
                rc.value = v; rc.fg = fg; rc.bg = bg;
            }
        }
    }

    // Redraw horizontal dividers last — printValue fillRects erase them on value changes.
    // Same pattern as LNDG powered descent screen.
    static const uint8_t divRows[] = { 2, 4, 5, 6 };
    for (uint8_t i = 0; i < 4; i++) {
        uint16_t dy = TITLE_TOP + divRows[i] * rowHFor(SCFT_PANEL_NR);
        tft.drawLine(SCFT_PANEL_X, dy,     SCFT_PANEL_RIGHT - 1, dy,     TFT_GREY);
        tft.drawLine(SCFT_PANEL_X, dy + 1, SCFT_PANEL_RIGHT - 1, dy + 1, TFT_GREY);
    }
}


// ── Screen update ─────────────────────────────────────────────────────────────────────
// "TRIM" annunciation (cyan) in the graphical area's bottom-right corner: right-aligned to
// the heading-tape right edge, bottom just above the heading-tape top. Redrawn each frame
// while trim is enabled; erased once when it clears.
// ── Velocity reference chip ───────────────────────────────────────────────────────────
// SRF or ORB, mirrored from TRIM at the heading tape's left edge. See drawRefChip in
// AAA_Screens.ino for the colour rule and the hit box.
//
// It annunciates the VELOCITY reference only -- the prograde marker on the ball and both
// tapes, and row 1. It deliberately does not cover altitude: SRF and ORB differ by a
// rotation about the body's spin axis, and altitude is radial and unchanged by that
// rotation, so Alt.SL is the same number in both frames and there is no such thing as an
// orbital altitude. (The Shuttle mixed them freely for the same reason: its AMI showed
// Earth-relative velocity beside the AVVI's geodetic altitude, with nothing to
// annunciate.) Nor does it cover row 5, which is a phase choice -- see _scftAutoOrb.

static void _scftUpdateRefChip(KCM_TFT &tft) {
    const bool orb  = _scftVelRef();
    const bool held = _scftVelRefOverride.manual;
    const int8_t want = (int8_t)((held ? 2 : 0) + (orb ? 1 : 0));
    if (want == _scftChipShown) return;
    _scftChipShown = want;
    drawRefChip(tft, orb ? "ORB" : "SRF", held);
}

// One tap pins the other reference, or drops back to auto if that is what auto wants.
void scftToggleVelRef() {
    modeToggle(_scftVelRefOverride, _scftAutoOrb());
    _scftChipShown = -1;   // repaint on the next frame
}

static void _scftDrawTrim(KCM_TFT &tft) {
    static bool prev = false;
    int16_t tw = getFontStringWidth(&Roboto_Black_24, "TRIM");
    int16_t x  = (SCFT_HDG_TAPE_X + SCFT_HDG_TAPE_W) - tw - 8;
    int16_t y  = SCFT_HDG_TAPE_Y - (int16_t)Roboto_Black_24.cap_height - 2 - 8;
    if (state.trimEnabled) {
        tft.setFont(Roboto_Black_24);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setCursor(x, y);
        tft.print("TRIM");
    } else if (prev) {
        tft.fillRect(x - 2, y - 2, tw + 4, (int16_t)Roboto_Black_24.cap_height + 6, TFT_BLACK);
    }
    prev = state.trimEnabled;
}

static void drawScreen_SCFT(KCM_TFT &tft) {
    // Boarding or leaving a craft changes which seven rows the panel carries, and the
    // row labels are chrome. Re-enter the screen so the chrome redraws with them --
    // exactly how the V.Orb/V.Srf swap below has always been handled.
    bool evaMode = _scftEvaMode();
    if (evaMode != _scftPrevEvaMode) {
        _scftPrevEvaMode      = evaMode;
        _scftFullRedrawNeeded = true;
        switchToScreen(screen_SCFT);
        return;
    }

    bool orbMode = _scftVelRef();
    if (orbMode != _scftPrevOrbMode) {
        _scftPrevOrbMode      = orbMode;
        _scftFullRedrawNeeded = true;
        switchToScreen(screen_SCFT);
        return;
    }

    // Select active velocity vector based on orbital mode — computed BEFORE the
    // ball dirty check since the ball's ADI markers depend on it.
    _scftVelHdg   = orbMode ? state.orbVelHeading : state.srfVelHeading;
    _scftVelPitch = orbMode ? state.orbVelPitch   : state.srfVelPitch;

    bool mnvrActive = (state.mnvrTime > 0.0f);
    bool tgtAvail   = state.targetAvailable;

    // Attitude dirty — pitch/roll as before, plus heading (which now affects ball
    // because ADI markers project relative to it).
    bool ballDirty = _scftFullRedrawNeeded                                        ||
                     fabsf(state.pitch - _scftPrevPitch)                  >= 0.2f ||
                     fabsf(state.roll  - _scftPrevRoll)                   >= 0.2f ||
                     fabsf(eadiHdgDelta(state.heading, _scftPrevHeadingBall)) >= 0.5f;

    // Marker dirty — any visible marker's world position changed, or a marker's
    // availability toggled (target acquired/lost, maneuver set/cleared).
    if (!ballDirty) {
        ballDirty = fabsf(eadiHdgDelta(_scftVelHdg, _scftPrevVelHdgBall)) >= 0.5f ||
                    fabsf(_scftVelPitch - _scftPrevVelPitchBall)           >= 0.5f ||
                    mnvrActive != _scftPrevMnvrActiveBall                         ||
                    tgtAvail   != _scftPrevTgtAvailBall;
    }
    if (!ballDirty && mnvrActive) {
        ballDirty = fabsf(eadiHdgDelta(state.mnvrHeading, _scftPrevMnvrHdgBall)) >= 0.5f ||
                    fabsf(state.mnvrPitch - _scftPrevMnvrPitchBall)               >= 0.5f;
    }
    if (!ballDirty && tgtAvail) {
        ballDirty = fabsf(eadiHdgDelta(state.tgtHeading, _scftPrevTgtHdgBall)) >= 0.5f ||
                    fabsf(state.tgtPitch - _scftPrevTgtPitchBall)               >= 0.5f;
    }

    if (ballDirty) {
        bool full = _scftFullRedrawNeeded;
        uint32_t t0 = micros();
        eadiDrawBall(tft, full, _scftVelHdg, _scftVelPitch, true);   // orbital-frame markers on
        // Snapshot ball-dirty trackers for next frame's dirty check.
        _scftPrevPitch = state.pitch;
        _scftPrevRoll  = state.roll;
        _scftPrevHeadingBall    = state.heading;
        _scftPrevVelHdgBall     = _scftVelHdg;
        _scftPrevVelPitchBall   = _scftVelPitch;
        _scftPrevMnvrHdgBall    = state.mnvrHeading;
        _scftPrevMnvrPitchBall  = state.mnvrPitch;
        _scftPrevTgtHdgBall     = state.tgtHeading;
        _scftPrevTgtPitchBall   = state.tgtPitch;
        _scftPrevMnvrActiveBall = (state.mnvrTime > 0.0f);
        _scftPrevTgtAvailBall   = state.targetAvailable;
        _scftFullRedrawNeeded = false;
        uint32_t dt = micros() - t0;
        if (debugMode) {
            Serial.print(full ? "SCFT_FULL total=" : "SCFT_DELTA total=");
            Serial.print((float)dt / 1000.0f, 2);
            Serial.print("ms  pitch="); Serial.print(state.pitch, 1);
            Serial.print("  roll=");    Serial.println(state.roll, 1);
        }
    }

    // Roll indicator — update whenever roll changes, independent of ball redraw
    eadiUpdateRollIndicator(tft, state.roll);
    _scftUpdateRollReadout(tft, state.roll);
    _scftUpdatePitchTape(tft, state.pitch);
    _scftUpdateHeadingTape(tft, state.heading);
    _scftUpdateThrottle(tft, state.throttle);
    _scftUpdateVitals(tft);
    _scftUpdateRates(tft);
    _scftUpdatePanel(tft, orbMode);
    _scftUpdateRefChip(tft);
    _scftDrawTrim(tft);
}

