/***************************************************************************************
   Screen_LNCH_Circ.ino -- Circularization (orbital) phase of the LNCH screen.

   Active when state.altitude exceeds ~6% of bodyR (with hysteresis), i.e. once the
   vessel has reached its apoapsis-stretching phase and the pilot is preparing to
   circularize. Layout summary:

   LEFT PANEL (x=0..452):
     - Cluster (x≈0..117) — ATT indicator (alignment disc), IGN button (burn-active
       lamp), Burn Dur readout
     - Orbit diagram (x≈120..440) — body, atmosphere ring, dashed target circle,
       current orbit curve, Pe/Ap markers, vessel dot, direction chevron
     - Below diagram: ΔV Burn bar + value text, T+Ign countdown row

   RIGHT PANEL (x=453..720):
     - 8 numeric readouts: Alt.SL, V.Orb, ApA, PeA, T+Ap, Thrtl, T.Brn, ΔV.Stg
     - Grouped horizontal dividers between logical row groups

   Phase membership (LNCH screen has three phases):
     - PRE-LAUNCH (Screen_LNCH_PreLaunch.ino)  — auto, when sit_PreLaunch is set
     - ASCENT     (Screen_LNCH_Ascent.ino)     — when below switch altitude
     - CIRC       (this file)                   — when above switch altitude

   Top-level dispatcher Screen_LNCH.ino selects which of these to draw.

   Public-to-the-sketch entry points (called by the LNCH dispatcher):
     - _lnchOrResetState()              — clear all change-detection state
     - _lnchOrDrawRightPanelChrome(tft) — initial labels + dividers for right panel
     - _lnchOrDrawLeftPanelValues(tft)  — per-frame left panel updates (orbit + bar + ATT/IGN/BD)
     - _lnchOrDrawRightPanelValues(tft) — per-frame right panel value updates
****************************************************************************************/

// ── Orbital-phase left panel graphic ──────────────────────────────────────────────────
//
// Centred orbit curve with body at one focus, vessel marker (chevron pointing
// in direction of motion), Ap/Pe markers, and a dashed reference circle
// showing the target circular orbit altitude. A "Time to Apoapsis" progress
// bar sits below the diagram to give an at-a-glance countdown to the
// circularization burn point.
//
// Layout summary (panel is x=0..452, y=63..479):
//   y=131..411: orbit diagram (MAX_R=140 centered on CY=271)
//   y=440..459: T+Ap progress bar (20 px tall, labeled on the left)
//
// The vessel chevron points along the tangent of the drawn orbit curve at the
// vessel's true-anomaly position. Direction of travel (CCW vs CW) is inferred
// from `state.inclination` (< 90° = prograde/CCW, > 90° = retrograde/CW).
static const int16_t LNCH_OR_DIAG_CX    = 280;   // diagram center X (right side; ATT/IGN/Burn-Dur cluster on left)
static const int16_t LNCH_OR_DIAG_CY    = 234;   // diagram center Y (centered between content top and bar label row)
static const int16_t LNCH_OR_DIAG_MAX_R = 140;   // max orbit half-extent (px)

// Maneuver ΔV bar layout. Matches the visual format of the MNVR screen's
// "ΔV Burn" bar: label "ΔV Burn" above-left, right-aligned ΔV value in m/s
// above-right, grey border, off-black background, green fill (yellow when
// stage ΔV is tight). Bar fills proportionally to mnvrDeltaV and drains
// from the cached arm-time max as the burn consumes ΔV.
//
// Vertical layout:
//   y = 397..420: bar label row (24 px tall, Black_20 — was Black_12 at y=407)
//   y = 423..444: bar (22 px tall including border)
//   y = 448..478: T+Ign row (31 px tall — Black_24 is 29 px, +1 px margin
//                             above/below so printValue's h-2 fillRect fully
//                             covers the font's glyph rows)
// Horizontal: bar centered in the 453-wide left panel, ~same width as MNVR.
static const int16_t LNCH_OR_BAR_X      =  53;   // bar left edge
static const int16_t LNCH_OR_BAR_W      = 346;   // bar width (matches MNVR)
static const int16_t LNCH_OR_BAR_H      =  22;   // bar height (matches MNVR)
static const int16_t LNCH_OR_BAR_Y      = 423;   // bar top
static const int16_t LNCH_OR_BAR_LBL_Y  = 397;   // bar label row top (barY - 26 to fit Black_20)
static const int16_t LNCH_OR_TIGN_Y     = 448;   // T+Ign row top (barY + barH + 3)
static const int16_t LNCH_OR_TIGN_H     =  31;   // T+Ign row height (Black_24=29 + 2 margin)

// Attitude (ATT) indicator: a maneuver-alignment disc positioned to the
// right of the orbit diagram. Shows the vessel's current attitude error vs
// the maneuver's planned attitude (mnvrHeading, mnvrPitch) as a coloured
// dot within concentric rings.
//
// Scaled down from the LNDG attitude indicator: outer ring = 15° scale,
// rings at 5° / 10° / 15°, crosshairs at cardinal axes, dot clamped to
// outer ring. Dot colour: green (< 5°), yellow (< 15°), red (>= 15°).
// Inactive dark grey when no maneuver node is planned.
//
// The ATT/IGN/Burn-dur cluster sits on the LEFT side of the panel; the
// orbit diagram (LNCH_OR_DIAG_*) is on the RIGHT. Cluster CX=60 places
// the IGN button (widest at 104 px) at x=8..112, leaving ~28 px clearance
// to the orbit diagram's leftmost extent at x=140.
//
// ATT outer-ring radius matches the LNDG_Powered ATT indicator (R=52) for
// cross-screen visual consistency. CY=150 places the disc with a 7 px top
// margin below the panel, giving room for the Black_20 "ATT" label above.
static const int16_t LNCH_OR_ATT_CX    = 60;                     // indicator center X (left side)
static const int16_t LNCH_OR_ATT_CY    = 150;                    // indicator center Y (was 140; shifted to fit R=52)
static const int16_t LNCH_OR_ATT_R     = 52;                     // outer ring radius (= ±15°, matches LNDG_Powered)
static const float   LNCH_OR_ATT_SCALE = (float)LNCH_OR_ATT_R / 15.0f;  // ~3.47 px/deg
static const float   LNCH_OR_ALIGN_GREEN_DEG  =  5.0f;   // < 5° = aligned (dot green)
static const float   LNCH_OR_ALIGN_YELLOW_DEG = 15.0f;   // < 15° = close (dot yellow)

// IGN button: replaces the previous "burn-active bar border colour change"
// as the visible cue that the engine is firing. Sits below the ATT disc.
// Width matches the ATT disc diameter (2×R=104 px) for visual unity within
// the cluster. Height (83 px) preserves the prior 84:67 aspect ratio for a
// vertically-prominent shape. OFF state = light grey label on off-black;
// ON state = black label on International Orange.
static const int16_t LNCH_OR_IGN_W     = 104;          // button width (matches ATT disc diameter)
static const int16_t LNCH_OR_IGN_H     =  83;          // button height (preserves 84:67 aspect ratio)
static const int16_t LNCH_OR_IGN_X     = LNCH_OR_ATT_CX - LNCH_OR_IGN_W / 2;  // centered under ATT CX (=8)
static const int16_t LNCH_OR_IGN_Y     = 228;                                  // centered between ATT bottom (202) and Burn Dur top (336)

// Burn-duration readout: a centered label+value block placed below the IGN
// button. Both label and value horizontally-centered around LNCH_OR_ATT_CX.
//   Label: "Burn Dur:" in Black_20 light grey
//   Value: formatTime(mnvrDuration) in Black_24 dark green
//          ("---" in dark grey when no maneuver node)
static const int16_t LNCH_OR_BDUR_LBL_Y = 336;                                  // fixed position (shifted from 311 for R=52 cluster)
static const int16_t LNCH_OR_BDUR_VAL_Y = LNCH_OR_BDUR_LBL_Y + 27;             // 24 px label + 3 px gap



// ── Orbital (circularization) phase change-detection state ────────────────────────────
// Mirrors the ascent state structure. Row order for orbital phase:
//   0: Alt.SL, 1: V.Orb, 2: ApA, 3: PeA, 4: T+Ap, 5: Thrtl, 6: T.Brn, 7: ΔV.Stg
static int32_t _lnchOrPrevAlt        = -1 << 30;  // m
static int16_t _lnchOrPrevVOrb       = -9999;     // tenths m/s
static int32_t _lnchOrPrevApA        = -1 << 30;  // m
static int32_t _lnchOrPrevPeA        = -1 << 30;  // m
static int32_t _lnchOrPrevTimeToAp   = -1 << 30;  // seconds
static int16_t _lnchOrPrevThrottle   = -1;        // percent
static int32_t _lnchOrPrevTBurn      = -1 << 30;  // seconds
static int32_t _lnchOrPrevDVStg      = -1 << 30;  // m/s

static uint16_t _lnchOrPrevAltFg     = 0xFFFF;
static uint16_t _lnchOrPrevVOrbFg    = 0xFFFF;
static uint16_t _lnchOrPrevApAFg     = 0xFFFF;
static uint16_t _lnchOrPrevPeAFg     = 0xFFFF;
static uint16_t _lnchOrPrevTimeToApFg= 0xFFFF;
static uint16_t _lnchOrPrevThrFg     = 0xFFFF; static uint16_t _lnchOrPrevThrBg  = 0xFFFF;
static uint16_t _lnchOrPrevTBurnFg   = 0xFFFF; static uint16_t _lnchOrPrevTBurnBg = 0xFFFF;
static uint16_t _lnchOrPrevDVStgFg   = 0xFFFF; static uint16_t _lnchOrPrevDVStgBg = 0xFFFF;

static PrintState _lnchOrPs[8];   // PrintState tracking for each orbital row

// Throttle state for the orbital graphic — full-redraw is expensive so we
// limit to ~5 Hz (200 ms between redraws).
static uint32_t _lnchOrLastDiagDraw = 0;

// ── Orbit graphic prev-frame state (for erase-then-redraw, no full wipe) ─────────────
// The static layer (body, atmosphere ring, dashed target circle) is drawn once
// at chrome time and doesn't need per-frame redraw unless the SOI changes.
// Dynamic elements (orbit curve, Pe/Ap markers, vessel) are erased individually
// at their prev pixel positions and redrawn in new positions each frame. This
// avoids the visible flicker caused by a fullRect black wipe every frame.
//
// ORBIT_CURVE_N = number of line segments in the orbit curve (same as N in the
// drawing loop). Array size = ORBIT_CURVE_N + 1 points (segments are between
// consecutive points).
static const uint8_t LNCH_OR_CURVE_N = 96;
static int16_t _lnchOrPrevCurveX[LNCH_OR_CURVE_N + 1];
static int16_t _lnchOrPrevCurveY[LNCH_OR_CURVE_N + 1];
static bool    _lnchOrPrevCurveValid = false;
static int16_t _lnchOrPrevPeX = -1;
static int16_t _lnchOrPrevPeY = -1;
static int16_t _lnchOrPrevPeLblX = -1;
static int16_t _lnchOrPrevPeLblY = -1;
static bool    _lnchOrPrevPeValid = false;
static int16_t _lnchOrPrevApX = -1;
static int16_t _lnchOrPrevApY = -1;
static int16_t _lnchOrPrevApLblX = -1;
static int16_t _lnchOrPrevApLblY = -1;
static bool    _lnchOrPrevApValid = false;
static int16_t _lnchOrPrevVslX = -1;
static int16_t _lnchOrPrevVslY = -1;
static bool    _lnchOrPrevVslValid = false;
// Direction-indicator chevron (embedded on orbit curve at ν=90° for prograde,
// ν=270° for retrograde). Shares lifecycle with the orbit curve — redrawn
// when orbit curve changes.
static int16_t _lnchOrPrevDirX = -1;
static int16_t _lnchOrPrevDirY = -1;
static bool    _lnchOrPrevDirValid = false;
// Previously-drawn orbit curve colour (for detecting colour-only changes).
static uint16_t _lnchOrPrevOrbitCol = 0;
// Previously-drawn static layer geometry. When atmo_px or target_px change
// (due to compressed-scaling recomputation as orbit drifts), we need to
// erase the OLD static layer rings first before redrawing at new radii —
// otherwise the old ring stays on screen and successive draws accumulate
// into a visibly thick line.
static int16_t _lnchOrPrevAtmoPx   = -1;
static int16_t _lnchOrPrevTargetPx = -1;
static int16_t _lnchOrPrevBodyPx   = -1;
// T+Ap progress bar prev-state (fill width in pixels, fill colour)
// Maneuver bar prev-state. Fill width in pixels, fill colour, and the
// cached "max ΔV" used to compute fill fraction (burn starts → lock max;
// burn progresses → mnvrDeltaV drains; ratio = current/max = fill fraction).
// Max is reset to 0 when the maneuver node disappears.
//
// prevBarDvRounded: mnvrDeltaV rounded to the nearest whole m/s for the
// right-aligned value text ("%.0fm/s"). When this changes (or colour
// changes), we re-render the value text. -9999 = not yet drawn.
static int16_t  _lnchOrPrevBarFill = -1;    // -1 = not drawn yet
static uint16_t _lnchOrPrevBarCol  = 0;
static int16_t  _lnchOrPrevBarDvRounded = -9999;
// Text colour of the value text (distinct from fill_col — the value text uses
// dim light grey in estimate mode even though the bar fill is dark grey).
// 0 = not yet drawn.
static uint16_t _lnchOrPrevBarTextCol = 0;
static bool     _lnchOrPrevBarEstimate = false;
static float    _lnchOrMnvrMaxDV   = 0.0f;  // cached maximum ΔV seen, m/s
// T+Ign countdown row (below the bar). Shows time-until-ignition for the
// upcoming maneuver burn (mnvrTime - mnvrDuration/2), with the same colour
// thresholds as the MNVR screen. Uses the library printValue helper (which
// manages its own change-detection via PrintState). prevTignSec is an
// additional rounded-seconds cache so we can skip the printValue call
// entirely when the rounded value and colours are unchanged (-9999 = not
// yet drawn, -9998 = currently showing "---" placeholder).
static int32_t   _lnchOrPrevTignSec = -9999;
static uint16_t  _lnchOrPrevTignFg  = 0;
static uint16_t  _lnchOrPrevTignBg  = 0;
static PrintState _lnchOrTignPs;
// Burn-active indicator: the ΔV Burn bar's border turns TFT_ORANGE while the
// engine is firing (state.throttle > 0). -1 = not yet evaluated, triggers a
// first-frame draw.
static int8_t    _lnchOrPrevBurnActive = -1;
// ATT indicator prev-frame state (dot position + colour). The chrome (rings,
// crosshairs) is drawn once and not retracked — touch-up on dot move
// repairs any damage. 9999 = not drawn yet / needs first-frame init.
static int16_t   _lnchOrPrevAttDotX = 9999;
static int16_t   _lnchOrPrevAttDotY = 9999;
static uint16_t  _lnchOrPrevAttDotCol = 0;
// Burn-duration readout prev state: rounded seconds (-9999 = not drawn,
// -9998 = "---" placeholder). Skip redraw when value rounds the same.
static int32_t   _lnchOrPrevBurnDurSec = -9999;
// SOI name we last drew the static layer for. If SOI changes mid-session, we
// need to redraw the static layer.
static char _lnchOrPrevSoi[24] = {0};

// Reset all orbital-phase change-detection state + PrintState. Called at
// chrome time to force a full redraw.
static void _lnchOrResetState() {
    _lnchOrPrevAlt        = -1 << 30;
    _lnchOrPrevVOrb       = -9999;
    _lnchOrPrevApA        = -1 << 30;
    _lnchOrPrevPeA        = -1 << 30;
    _lnchOrPrevTimeToAp   = -1 << 30;
    _lnchOrPrevThrottle   = -1;
    _lnchOrPrevTBurn      = -1 << 30;
    _lnchOrPrevDVStg      = -1 << 30;
    _lnchOrPrevAltFg      = 0xFFFF;
    _lnchOrPrevVOrbFg     = 0xFFFF;
    _lnchOrPrevApAFg      = 0xFFFF;
    _lnchOrPrevPeAFg      = 0xFFFF;
    _lnchOrPrevTimeToApFg = 0xFFFF;
    _lnchOrPrevThrFg      = 0xFFFF; _lnchOrPrevThrBg  = 0xFFFF;
    _lnchOrPrevTBurnFg    = 0xFFFF; _lnchOrPrevTBurnBg = 0xFFFF;
    _lnchOrPrevDVStgFg    = 0xFFFF; _lnchOrPrevDVStgBg = 0xFFFF;
    for (uint8_t i = 0; i < 8; i++) {
        _lnchOrPs[i].prevWidth  = 0;
        _lnchOrPs[i].prevBg     = 0x0001;
        _lnchOrPs[i].prevHeight = 0;
    }
    // Invalidate global printState slots (shared across phases).
    for (uint8_t i = 0; i < ROW_COUNT; i++) {
        printState[screen_LNCH][i].prevWidth  = 0;
        printState[screen_LNCH][i].prevBg     = 0x0001;
        printState[screen_LNCH][i].prevHeight = 0;
    }
    // Force immediate first-frame redraw of the orbit graphic
    _lnchOrLastDiagDraw = 0;
    // Invalidate orbit-graphic prev-frame cache so the first frame does a
    // full draw (static layer + all dynamic elements) without trying to
    // erase stale previous pixels.
    _lnchOrPrevCurveValid = false;
    _lnchOrPrevPeValid    = false;
    _lnchOrPrevApValid    = false;
    _lnchOrPrevVslValid   = false;
    _lnchOrPrevDirValid   = false;
    _lnchOrPrevOrbitCol   = 0;
    _lnchOrPrevAtmoPx     = -1;
    _lnchOrPrevTargetPx   = -1;
    _lnchOrPrevBodyPx     = -1;
    _lnchOrPrevBarFill    = -1;
    _lnchOrPrevBarCol     = 0;
    _lnchOrPrevBarDvRounded = -9999;
    _lnchOrPrevBarTextCol = 0;
    _lnchOrPrevBarEstimate = false;
    _lnchOrPrevTignSec    = -9999;
    _lnchOrPrevTignFg     = 0;
    _lnchOrPrevTignBg     = 0;
    _lnchOrTignPs.prevWidth  = 0;
    _lnchOrTignPs.prevBg     = 0x0001;
    _lnchOrTignPs.prevHeight = 0;
    _lnchOrPrevBurnActive = -1;
    _lnchOrPrevAttDotX    = 9999;
    _lnchOrPrevAttDotY    = 9999;
    _lnchOrPrevAttDotCol  = 0;
    _lnchOrPrevBurnDurSec = -9999;
    _lnchOrMnvrMaxDV      = 0.0f;
    _lnchOrPrevSoi[0]     = '\0';
}

// ── Orbital (circularization) phase — right panel ─────────────────────────────────────
//
// Mirrors the ascent right panel structure: 8 rows, Roboto_Black_24, label on
// the left and value right-aligned, dividers between rows. Uses a separate
// row-label set and different change-detection state.
//
// Row order / labels:
//   0: Alt.SL  (same as ascent)
//   1: V.Orb   (permanent in orbital phase — no V.Srf swap)
//   2: ApA     (apoapsis altitude)
//   3: PeA     (periapsis altitude — NEW for orbital, replaces V.Vrt)
//   4: T+Ap    (time to apoapsis)
//   5: Thrtl   (throttle percent)
//   6: T.Brn   (stage burn time remaining)
//   7: ΔV.Stg  (stage delta-V remaining)
static const char *_lnchOrLabels[8] = {
    "Alt.SL:", "V.Orb:", "ApA:", "PeA:",
    "T+Ap:", "Thrtl:", "T.Brn:", "\xCE\x94V.Stg:"
};

// Draw static chrome for the orbital-phase right panel. Matches the ascent
// chrome style exactly — same fonts (Black_20 labels / Black_24 values via
// update functions), same 2-px dividers, same RPANEL geometry — just with
// orbital row labels.
static void _lnchOrDrawRightPanelChrome(RA8875 &tft) {
    // Vertical divider in the 2-px gap before the right panel
    tft.drawLine(LNCH_AS_RPANEL_X - 2, LNCH_AS_PANEL_Y,
                 LNCH_AS_RPANEL_X - 2, LNCH_AS_PANEL_Y + LNCH_AS_PANEL_H - 1,
                 TFT_GREY);
    tft.drawLine(LNCH_AS_RPANEL_X - 1, LNCH_AS_PANEL_Y,
                 LNCH_AS_RPANEL_X - 1, LNCH_AS_PANEL_Y + LNCH_AS_PANEL_H - 1,
                 TFT_GREY);

    for (uint8_t i = 0; i < 8; i++) {
        printDispChrome(tft, &Roboto_Black_20,
                        LNCH_AS_RPANEL_X, _lnchAsRowY(i),
                        LNCH_AS_RPANEL_W, LNCH_AS_ROW_H,
                        _lnchOrLabels[i], COL_LABEL, TFT_BLACK, COL_NO_BDR);
    }

    // Horizontal group dividers — 2 px in TFT_GREY, matching SCFT pattern.
    // Rows grouped logically:
    //   rows 0-1: Alt.SL, V.Orb         (position + velocity)
    //   rows 2-3: ApA, PeA              (trajectory)
    //   row  4:   T+Ap                   (timing to apoapsis)
    //   row  5:   Thrtl                  (current engine state)
    //   rows 6-7: T.Brn, ΔV.Stg         (stage resources)
    // Dividers sit in the 2-px gap between row groups (at y=dy-1 and y=dy,
    // where dy = rowY(2), rowY(4), rowY(5), rowY(6)). Same rationale as the
    // ascent-phase dividers: placed outside both adjacent rows' fillRect
    // clear regions so bg-colour changes (alarms) don't nibble them.
    static const uint8_t divRows[] = { 2, 4, 5, 6 };
    for (uint8_t i = 0; i < sizeof(divRows); i++) {
        int16_t dy = _lnchAsRowY(divRows[i]);
        tft.drawLine(LNCH_AS_RPANEL_X, dy - 1,
                     LNCH_AS_RPANEL_X + LNCH_AS_RPANEL_W - 1, dy - 1,
                     TFT_GREY);
        tft.drawLine(LNCH_AS_RPANEL_X, dy,
                     LNCH_AS_RPANEL_X + LNCH_AS_RPANEL_W - 1, dy,
                     TFT_GREY);
    }
}

// Helper: draw a value in an orbital-phase row using printValue. The label was
// drawn by chrome; value is right-aligned in the cell.
static void _lnchOrDrawRowValue(RA8875 &tft, uint8_t row, const String &val,
                                 uint16_t fg, uint16_t bg) {
    printValue(tft, &Roboto_Black_24,
               LNCH_AS_RPANEL_X, _lnchAsRowY(row),
               LNCH_AS_RPANEL_W, LNCH_AS_ROW_H,
               _lnchOrLabels[row], val,
               fg, bg, TFT_BLACK,
               _lnchOrPs[row]);
}

// ── Orbital row update functions ──────────────────────────────────────────────────────
// Each checks its own change detection and returns early if unchanged.
// Order: Alt, V.Orb, ApA, PeA, T+Ap, Throttle, T.Burn, ΔV.Stg.

static void _lnchOrUpdateAlt(RA8875 &tft) {
    float alt = state.altitude;
    int32_t iAlt = (int32_t)roundf(alt);

    float bodyRad = (currentBody.radius > 0.0f) ? currentBody.radius : 600000.0f;
    float altYellow = bodyRad * 0.0015f;  // ~900m Kerbin

    uint16_t fg = (alt < 0)         ? TFT_RED
                : (alt < altYellow)  ? TFT_YELLOW
                : TFT_DARK_GREEN;

    if (iAlt == _lnchOrPrevAlt && fg == _lnchOrPrevAltFg) return;

    _lnchOrDrawRowValue(tft, 0, formatAlt((float)iAlt), fg, TFT_BLACK);
    _lnchOrPrevAlt = iAlt;
    _lnchOrPrevAltFg = fg;
}

static void _lnchOrUpdateVOrb(RA8875 &tft) {
    float v = state.orbitalVel;
    int16_t iV = (int16_t)roundf(v * 10.0f);  // tenths m/s

    uint16_t fg = (v < 0) ? TFT_RED : TFT_DARK_GREEN;

    if (iV == _lnchOrPrevVOrb && fg == _lnchOrPrevVOrbFg) return;

    _lnchOrDrawRowValue(tft, 1, _lnchAsFmtMs1(v), fg, TFT_BLACK);
    _lnchOrPrevVOrb = iV;
    _lnchOrPrevVOrbFg = fg;
}

// ApA / PeA share the same threshold logic: red if negative (suborbital /
// impact trajectory), yellow if positive but below safe orbital altitude,
// green otherwise. warnAlt uses lowSpace (atmosphere top) not flyHigh —
// flyHigh is a biome boundary (~18km Kerbin), not an orbital threshold.
static void _lnchOrUpdateApA(RA8875 &tft) {
    float apa = state.apoapsis;
    int32_t iApA = (int32_t)roundf(apa);

    float warnAlt = max(currentBody.minSafe, currentBody.lowSpace);
    uint16_t fg;
    if      (apa < 0)                                      fg = TFT_RED;
    else if (warnAlt > 0 && apa > 0 && apa < warnAlt)      fg = TFT_YELLOW;
    else                                                   fg = TFT_DARK_GREEN;

    if (iApA == _lnchOrPrevApA && fg == _lnchOrPrevApAFg) return;

    _lnchOrDrawRowValue(tft, 2, formatAlt((float)iApA), fg, TFT_BLACK);
    _lnchOrPrevApA = iApA;
    _lnchOrPrevApAFg = fg;
}

static void _lnchOrUpdatePeA(RA8875 &tft) {
    float pea = state.periapsis;
    int32_t iPeA = (int32_t)roundf(pea);

    float warnAlt = max(currentBody.minSafe, currentBody.lowSpace);
    uint16_t fg;
    if      (pea < 0)                                      fg = TFT_RED;
    else if (warnAlt > 0 && pea > 0 && pea < warnAlt)      fg = TFT_YELLOW;
    else                                                   fg = TFT_DARK_GREEN;

    if (iPeA == _lnchOrPrevPeA && fg == _lnchOrPrevPeAFg) return;

    _lnchOrDrawRowValue(tft, 3, formatAlt((float)iPeA), fg, TFT_BLACK);
    _lnchOrPrevPeA = iPeA;
    _lnchOrPrevPeAFg = fg;
}

static void _lnchOrUpdateTimeToAp(RA8875 &tft) {
    float ttAp = state.timeToAp;
    int32_t iTtAp = (int32_t)roundf(ttAp);

    // Suppress display when no apoapsis (suborbital below surface) or vessel
    // is on the pad / landed — same logic as ascent.
    bool suppress = (state.apoapsis <= 0.0f) ||
                    (state.situation & sit_PreLaunch) ||
                    (state.situation & sit_Landed);

    uint16_t fg;
    String val;
    if (suppress) {
        val = "---";
        fg = TFT_DARK_GREY;
        iTtAp = -1 << 29;
    } else {
        if      (ttAp < 0)                   fg = TFT_RED;
        else if (ttAp < LNCH_TOAPO_WARN_S)   fg = TFT_YELLOW;
        else                                 fg = TFT_DARK_GREEN;
        val = formatTime(ttAp);
    }

    if (iTtAp == _lnchOrPrevTimeToAp && fg == _lnchOrPrevTimeToApFg) return;

    _lnchOrDrawRowValue(tft, 4, val, fg, TFT_BLACK);
    _lnchOrPrevTimeToAp = iTtAp;
    _lnchOrPrevTimeToApFg = fg;
}

static void _lnchOrUpdateThrottle(RA8875 &tft) {
    uint8_t thrPct = (uint8_t)constrain(state.throttle * 100.0f, 0.0f, 100.0f);

    // Coasting (throttle=0) is normal during orbital phase, so no warning;
    // active burn is always green.
    uint16_t fg = TFT_DARK_GREEN;
    uint16_t bg = TFT_BLACK;

    if ((int16_t)thrPct == _lnchOrPrevThrottle &&
        fg == _lnchOrPrevThrFg && bg == _lnchOrPrevThrBg) return;

    _lnchOrDrawRowValue(tft, 5, formatPerc(thrPct), fg, bg);
    _lnchOrPrevThrottle = (int16_t)thrPct;
    _lnchOrPrevThrFg = fg; _lnchOrPrevThrBg = bg;
}

static void _lnchOrUpdateTBurn(RA8875 &tft) {
    float tb = state.stageBurnTime;
    int32_t iTb = (int32_t)roundf(tb);

    uint16_t fg, bg;
    thresholdColor(tb,
                   LNCH_BURNTIME_ALARM_S, TFT_WHITE,  TFT_RED,
                   LNCH_BURNTIME_WARN_S,  TFT_YELLOW, TFT_BLACK,
                        TFT_DARK_GREEN, TFT_BLACK, fg, bg);

    if (iTb == _lnchOrPrevTBurn &&
        fg == _lnchOrPrevTBurnFg && bg == _lnchOrPrevTBurnBg) return;

    _lnchOrDrawRowValue(tft, 6, formatTime(tb), fg, bg);
    _lnchOrPrevTBurn = iTb;
    _lnchOrPrevTBurnFg = fg; _lnchOrPrevTBurnBg = bg;
}

static void _lnchOrUpdateDVStg(RA8875 &tft) {
    float dv = state.stageDeltaV;
    int32_t iDv = (int32_t)roundf(dv);

    uint16_t fg, bg;
    thresholdColor(dv,
                   DV_STG_ALARM_MS, TFT_WHITE,  TFT_RED,
                   DV_STG_WARN_MS,  TFT_YELLOW, TFT_BLACK,
                        TFT_DARK_GREEN, TFT_BLACK, fg, bg);

    if (iDv == _lnchOrPrevDVStg &&
        fg == _lnchOrPrevDVStgFg && bg == _lnchOrPrevDVStgBg) return;

    _lnchOrDrawRowValue(tft, 7, fmtMs(dv), fg, bg);
    _lnchOrPrevDVStg = iDv;
    _lnchOrPrevDVStgFg = fg; _lnchOrPrevDVStgBg = bg;
}

static void _lnchOrDrawRightPanelValues(RA8875 &tft) {
    _lnchOrUpdateAlt(tft);
    _lnchOrUpdateVOrb(tft);
    _lnchOrUpdateApA(tft);
    _lnchOrUpdatePeA(tft);
    _lnchOrUpdateTimeToAp(tft);
    _lnchOrUpdateThrottle(tft);
    _lnchOrUpdateTBurn(tft);
    _lnchOrUpdateDVStg(tft);
}

// ── Orbital phase — left-panel orbit graphic ──────────────────────────────────────────
//
// Shows the vessel's current orbit as a closed curve with the body at one
// focus, plus a dashed reference circle at the target circular orbit altitude.
// The vessel, apoapsis, and periapsis are marked.
//
// SCALING NOTE: A realistic 1:1 scale would make the body dominate the panel
// (e.g. a 90 km orbit around Kerbin's 600 km radius leaves a thin 15 % ring).
// Instead we use COMPRESSED RADIAL SCALING: body surface maps to 55 % of
// MAX_R, and the altitude band (surface..max radius) maps to the remaining
// 45 %. This exaggerates the visible orbit ring so Ap/Pe distances and
// eccentricity are much easier to read. The body still draws as a true circle
// (radial compression is symmetric); only the orbit curve is distorted from
// a true ellipse.
//
// Orbit radius as a function of true anomaly ν:
//   r(ν) = a·(1-e²) / (1 + e·cos(ν))
// At ν=0:  r = a·(1-e) = rPe  (periapsis)
// At ν=π:  r = a·(1+e) = rAp  (apoapsis)
// Vessel angle from body center = argOfPe + ν

// Body and atmosphere colors come from the shared lookups in AAA_Screens.ino
// (kspBodyColor, kspAtmoColor) so ORB and LNCH (Circularization) cannot drift
// apart on per-body visuals.

// Draw the full orbital graphic: body, atmosphere ring, target circle, orbit
// curve, vessel, Ap marker, Pe marker. Uses COMPRESSED RADIAL SCALING so the
// orbit ring (where all the interesting action is for circularization) gets
// most of the panel pixels: body is capped at 55 % of MAX_R, the altitude
// band gets the remaining 45 %. Orbit curve is no longer a true ellipse — it's
// a polar-parametric closed curve using r(nu) = a(1-e²)/(1+e·cos(nu)), with
// each radius remapped through the compression function. Markers and vessel
// use the same compression so they stay on the curve.
//
// For simplicity this does a full-redraw on each call rather than incremental
// updates. The caller throttles update rate via _lnchOrLastDiagDraw.
// Draw the static layer of the orbit graphic: body, atmosphere ring, dashed
// target circle. These do NOT change frame-to-frame (they depend only on the
// SOI) and don't need to be erased/redrawn every frame. Called once at entry
// to orbital mode, and again only if the SOI changes.
//
// Note: the drawing ORDER is atmosphere → target-circle → body so the body
// disc overlaps (hides) any atmosphere ring pixels that fall inside it (they
// shouldn't — atmo_px > body_px — but belt-and-braces).
static void _lnchOrDrawStaticLayer(RA8875 &tft,
                                   int16_t CX, int16_t CY,
                                   int16_t body_px, int16_t atmo_px, int16_t target_px,
                                   uint16_t bodyCol, uint16_t atmoCol) {
    // Atmosphere ring
    if (atmo_px > body_px && atmoCol != 0) {
        tft.drawCircle(CX, CY, atmo_px, atmoCol);
    }
    // Body disc
    tft.fillCircle(CX, CY, body_px, bodyCol);
    // Dashed target circle (40 segments, draw 1 of every 2 → dashed pattern)
    const int N = 40;
    const int skip = 2;
    float dtheta = 2.0f * (float)PI / N;
    int16_t px = CX + target_px;
    int16_t py = CY;
    for (int i = 1; i <= N; i++) {
        float theta = i * dtheta;
        int16_t nx = CX + (int16_t)roundf((float)target_px * cosf(theta));
        int16_t ny = CY - (int16_t)roundf((float)target_px * sinf(theta));
        if ((i % skip) == 0) {
            tft.drawLine(px, py, nx, ny, TFT_LIGHT_GREY);
        }
        px = nx; py = ny;
    }
}

// Check whether the vessel's current SOI differs from the last-drawn SOI.
// When it does (rare mid-flight event), we'll need to redraw the static layer.
static bool _lnchOrSoiChanged() {
    const char *name = currentBody.soiName;
    if (!name) name = "";
    return strncmp(_lnchOrPrevSoi, name, sizeof(_lnchOrPrevSoi)) != 0;
}

// Draw a small filled chevron (triangle) pointing along a heading. Used as
// the orbit-direction indicator — a compact arrow embedded on the orbit
// curve at ν=90° that shows which way the vessel travels.
// dx, dy = unit vector in direction of travel (screen coordinates, y grows down).
// Triangle is 12 px long × 10 px wide base, pointing forward.
static void _lnchOrDrawChevron(RA8875 &tft, int16_t cx, int16_t cy,
                                float dx, float dy, uint16_t col) {
    // Perpendicular (rotated 90° CW in screen coords: px=+dy, py=-dx)
    float px = dy;
    float py = -dx;
    // Tip: 7 px forward from (cx, cy)
    int16_t tip_x  = cx + (int16_t)roundf(7.0f * dx);
    int16_t tip_y  = cy + (int16_t)roundf(7.0f * dy);
    // Base corners: 5 px backward, ±5 px perpendicular
    int16_t bl_x   = cx - (int16_t)roundf(5.0f * dx + 5.0f * px);
    int16_t bl_y   = cy - (int16_t)roundf(5.0f * dy + 5.0f * py);
    int16_t br_x   = cx - (int16_t)roundf(5.0f * dx - 5.0f * px);
    int16_t br_y   = cy - (int16_t)roundf(5.0f * dy - 5.0f * py);
    tft.fillTriangle(tip_x, tip_y, bl_x, bl_y, br_x, br_y, col);
    // Dark outline for contrast against any background color. Drawn as three
    // drawLine calls rather than drawTriangle for library portability.
    tft.drawLine(tip_x, tip_y, bl_x, bl_y, TFT_BLACK);
    tft.drawLine(bl_x,  bl_y,  br_x, br_y, TFT_BLACK);
    tft.drawLine(br_x,  br_y,  tip_x, tip_y, TFT_BLACK);
}

// Draw the maneuver-bar chrome: "ΔV Burn" label above the bar (left-aligned),
// grey rectangular border, off-black interior background. Also draws the
// T+Ign row label ("T+Ign:") via printDispChrome. Bar fill, bar ΔV value,
// and T+Ign time value are all handled by per-frame update functions with
// change detection.
static void _lnchOrDrawProgressBarChrome(RA8875 &tft) {
    // "ΔV Burn" label above the bar, left-aligned with the bar's left edge.
    // Black_20 (24 px) — matches the "ATT" and "Burn Dur:" labels in the
    // left cluster for cluster/screen-wide label-hierarchy consistency.
    tft.setFont(&Roboto_Black_20);
    tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
    tft.setCursor(LNCH_OR_BAR_X, LNCH_OR_BAR_LBL_Y);
    tft.print("\xCE\x94V Burn");  // UTF-8 for "ΔV Burn"
    // Grey border + off-black interior (matches MNVR screen)
    tft.drawRect(LNCH_OR_BAR_X, LNCH_OR_BAR_Y,
                 LNCH_OR_BAR_W, LNCH_OR_BAR_H, TFT_GREY);
    tft.fillRect(LNCH_OR_BAR_X + 1, LNCH_OR_BAR_Y + 1,
                 LNCH_OR_BAR_W - 2, LNCH_OR_BAR_H - 2, TFT_OFF_BLACK);

    // T+Ign row label — use library printDispChrome so the cell geometry and
    // padding match the rest of the right-panel label convention. No border
    // (COL_NO_BDR). Background is black — the value's alarm background is
    // applied only to the value region by printValue.
    printDispChrome(tft, &Roboto_Black_24,
                    LNCH_OR_BAR_X, LNCH_OR_TIGN_Y,
                    LNCH_OR_BAR_W, LNCH_OR_TIGN_H,
                    "T+Ign:", TFT_LIGHT_GREY, TFT_BLACK, COL_NO_BDR);
}

// Update the maneuver-bar fill. Called each frame during orbital phase.
//
// Visual format matches the MNVR screen's "ΔV Burn" bar: grey border, off-
// black interior background, green fill (yellow when stage ΔV is tight —
// less than 110 % of node ΔV), with a right-aligned "%.0fm/s" value readout
// in the label row above the bar.
//
// Fill logic (intentionally different from MNVR — drains cleanly during the
// actual burn):
//   - No maneuver node (mnvrTime==0 && mnvrDeltaV==0): bar empty, value text
//     cleared. Cached max is reset to 0.
//   - Fresh maneuver node: cache mnvrDeltaV as max, draw bar full.
//   - mnvrDeltaV grows above cached max: re-arm (handles user adjusting node).
//   - mnvrDeltaV shrinks (burn in progress): fill fraction = current / max,
//     bar drains toward empty.
//
// Change detection on fill width, colour, and rounded value — skips redraws
// when nothing visually changed.
static void _lnchOrUpdateProgressBar(RA8875 &tft) {
    bool hasMnvr = (state.mnvrTime > 0.0f || state.mnvrDeltaV > 0.0f);

    // Compute target fill width, colour, and value-text state.
    int16_t  fill_px;
    uint16_t fill_col;
    int16_t  dv_rounded;   // -9999 = no value text
    bool     estimate_mode = false;  // true → prefix "est " and use dim colour
    if (!hasMnvr) {
        // No maneuver planned. Reset the drain cache, show an empty inactive
        // bar, but compute an ESTIMATE of the ΔV needed to circularize at
        // apoapsis — useful as a pre-planning hint. Formula (vis-viva):
        //   μ       = surfGrav × bodyR²  (standard gravitational parameter)
        //   rAp     = apoapsis + bodyR
        //   sma     = (rAp + (periapsis + bodyR)) / 2
        //   v_circ  = sqrt(μ / rAp)                 (target circular v at Ap)
        //   v_curr  = sqrt(μ × (2/rAp - 1/sma))     (current v at Ap, vis-viva)
        //   ΔV_circ = v_circ - v_curr
        _lnchOrMnvrMaxDV = 0.0f;
        fill_px    = 0;
        fill_col   = TFT_DARK_GREY;
        dv_rounded = -9999;

        float bodyR = currentBody.radius;
        float g     = currentBody.surfGrav;
        if (bodyR > 0.0f && g > 0.0f && state.apoapsis > 0.0f) {
            float mu   = g * bodyR * bodyR;
            float rAp  = state.apoapsis  + bodyR;
            float rPe  = fmaxf(state.periapsis + bodyR, bodyR);  // clamp to surface
            float sma  = (rAp + rPe) * 0.5f;
            if (sma > 0.0f && rAp > 0.0f) {
                float v_circ_sq = mu / rAp;
                float v_curr_sq = mu * (2.0f / rAp - 1.0f / sma);
                if (v_circ_sq > 0.0f && v_curr_sq > 0.0f) {
                    float dvEst = sqrtf(v_circ_sq) - sqrtf(v_curr_sq);
                    // Guard against nonsense (e.g. sub-orbital, negative values).
                    if (dvEst > 0.0f && dvEst < 100000.0f) {
                        dv_rounded = (int16_t)roundf(dvEst);
                        estimate_mode = true;
                    }
                }
            }
        }
    } else {
        // Maneuver node active. Update cached max if this is the first time
        // we see the node, or if mnvrDeltaV has grown (user adjusted node).
        // Small tolerance (0.5 m/s) to avoid thrashing on noise.
        if (_lnchOrMnvrMaxDV <= 0.0f ||
            state.mnvrDeltaV > _lnchOrMnvrMaxDV + 0.5f) {
            _lnchOrMnvrMaxDV = state.mnvrDeltaV;
        }
        float frac = (_lnchOrMnvrMaxDV > 0.001f)
                     ? state.mnvrDeltaV / _lnchOrMnvrMaxDV : 0.0f;
        if (frac < 0.0f) frac = 0.0f;
        if (frac > 1.0f) frac = 1.0f;
        fill_px = (int16_t)roundf(frac * (float)(LNCH_OR_BAR_W - 2));
        // Tight-stage warning: if stageDeltaV < mnvrDeltaV * 1.1, the pilot
        // may run out of stage ΔV before completing the maneuver — flag with
        // yellow instead of green.
        bool tight = (state.stageDeltaV < state.mnvrDeltaV * 1.1f);
        fill_col   = tight ? TFT_YELLOW : TFT_DARK_GREEN;
        dv_rounded = (int16_t)roundf(state.mnvrDeltaV);
    }

    // Colour used for the VALUE TEXT — in estimate mode use a dim light-grey
    // so the estimate is distinct from a live maneuver's bright fill colour.
    uint16_t text_col = estimate_mode ? TFT_LIGHT_GREY : fill_col;

    bool fill_changed  = (fill_px != _lnchOrPrevBarFill) ||
                         (fill_col != _lnchOrPrevBarCol);
    bool value_changed = (dv_rounded != _lnchOrPrevBarDvRounded) ||
                         (text_col  != _lnchOrPrevBarTextCol)   ||
                         (estimate_mode != _lnchOrPrevBarEstimate);

    // ── Fill update ─────────────────────────────────────────────────────────────
    if (fill_changed) {
        int16_t inner_x = LNCH_OR_BAR_X + 1;
        int16_t inner_y = LNCH_OR_BAR_Y + 1;
        int16_t inner_w = LNCH_OR_BAR_W - 2;
        int16_t inner_h = LNCH_OR_BAR_H - 2;
        if (fill_col != _lnchOrPrevBarCol) {
            // Colour changed: repaint whole interior — off-black background,
            // then fill in new colour.
            tft.fillRect(inner_x, inner_y, inner_w, inner_h, TFT_OFF_BLACK);
            if (fill_px > 0) {
                tft.fillRect(inner_x, inner_y, fill_px, inner_h, fill_col);
            }
        } else {
            // Same colour — delta draw only.
            if (fill_px > _lnchOrPrevBarFill) {
                int16_t dx = _lnchOrPrevBarFill;
                int16_t dw = fill_px - _lnchOrPrevBarFill;
                tft.fillRect(inner_x + dx, inner_y, dw, inner_h, fill_col);
            } else if (fill_px < _lnchOrPrevBarFill) {
                int16_t dx = fill_px;
                int16_t dw = _lnchOrPrevBarFill - fill_px;
                tft.fillRect(inner_x + dx, inner_y, dw, inner_h, TFT_OFF_BLACK);
            }
        }
        _lnchOrPrevBarFill = fill_px;
    }

    // ── Value text (right-aligned in label row) ─────────────────────────────────
    // Normal mode:   "245m/s" in bar colour.
    // Estimate mode: "est 145m/s" in dim light grey.
    // No value:      blank (e.g. no mnvr and no valid orbital data).
    if (value_changed) {
        // Clear the value-text region: right half of the bar width, same
        // row as the label. Height 24 px to fit Black_20 glyphs.
        int16_t valRegionX = LNCH_OR_BAR_X + LNCH_OR_BAR_W / 2;
        int16_t valRegionW = LNCH_OR_BAR_W / 2;
        tft.fillRect(valRegionX, LNCH_OR_BAR_LBL_Y, valRegionW, 24, TFT_BLACK);
        if (dv_rounded != -9999) {
            char buf[18];
            if (estimate_mode) {
                snprintf(buf, sizeof(buf), "est %dm/s", dv_rounded);
            } else {
                snprintf(buf, sizeof(buf), "%dm/s", dv_rounded);
            }
            // Black_20 — matches the "ΔV Burn" label font for visual unity.
            tft.setFont(&Roboto_Black_20);
            tft.setTextColor(text_col, TFT_BLACK);
            int16_t tw = getFontStringWidth(&Roboto_Black_20, buf);
            tft.setCursor(LNCH_OR_BAR_X + LNCH_OR_BAR_W - tw, LNCH_OR_BAR_LBL_Y);
            tft.print(buf);
        }
        _lnchOrPrevBarDvRounded = dv_rounded;
        _lnchOrPrevBarTextCol   = text_col;
        _lnchOrPrevBarEstimate  = estimate_mode;
    }

    _lnchOrPrevBarCol = fill_col;
}

// Update the T+Ign (time-to-ignition) countdown row below the bar.
//
// The "T+Ign:" label was drawn at chrome time via printDispChrome; this
// function only updates the time VALUE, using the library printValue helper
// so region clearing, padding, and right-alignment are all handled
// consistently with the rest of the app's label+value rows.
//
// Formula: T+Ign = mnvrTime - mnvrDuration/2 (burn midpoint at node).
// Colours follow MNVR thresholds (value only — label stays light grey):
//   tIgn > 60 s       → dark green on black
//   tIgn 10..60 s     → yellow on black
//   tIgn < 10 s / neg → white on red (alarm)
//   No maneuver node  → "---" in dark grey on black
//
// A rounded-seconds cache + colour-pair cache skip the printValue call when
// nothing visibly changed. printValue's own PrintState still handles the
// bg-change repaint when the alarm red turns on/off.
static void _lnchOrUpdateTignRow(RA8875 &tft) {
    bool hasMnvr = (state.mnvrTime > 0.0f || state.mnvrDeltaV > 0.0f);

    int32_t  newSec;       // rounded seconds value, or sentinel
    uint16_t valFg, valBg;
    String   val;
    if (!hasMnvr) {
        newSec = -9998;  // "---" placeholder sentinel
        valFg  = TFT_DARK_GREY;
        valBg  = TFT_BLACK;
        val    = "---";
    } else {
        float tIgn = state.mnvrTime - state.mnvrDuration * 0.5f;
        newSec = (int32_t)roundf(tIgn);
        if      (tIgn < MNVR_TIGN_ALARM_S) { valFg = TFT_WHITE;      valBg = TFT_RED;   }
        else if (tIgn < MNVR_TIGN_WARN_S)  { valFg = TFT_YELLOW;     valBg = TFT_BLACK; }
        else                               { valFg = TFT_DARK_GREEN; valBg = TFT_BLACK; }
        val = formatTime((int64_t)tIgn);
    }

    if (newSec == _lnchOrPrevTignSec &&
        valFg  == _lnchOrPrevTignFg  &&
        valBg  == _lnchOrPrevTignBg) return;

    // Delegate to the library helper. printValue handles region clearing
    // (using valBg for the value area, without touching the label on the
    // left), right-aligns the value within the cell, and tracks shrink
    // via PrintState so old wider strings get cleaned up.
    printValue(tft, &Roboto_Black_24,
               LNCH_OR_BAR_X, LNCH_OR_TIGN_Y,
               LNCH_OR_BAR_W, LNCH_OR_TIGN_H,
               "T+Ign:", val,
               valFg, valBg, TFT_BLACK,
               _lnchOrTignPs);

    _lnchOrPrevTignSec = newSec;
    _lnchOrPrevTignFg  = valFg;
    _lnchOrPrevTignBg  = valBg;
}

// Update the IGN button: a rectangular button below the ATT indicator that
// shows whether the engine is actively firing. Replaces the older approach
// of flipping the ΔV Burn bar's border colour — a dedicated button is more
// visually distinct and keeps the bar's chrome stable.
//   OFF (throttle == 0): "IGN" in light-grey on TFT_OFF_BLACK with a grey
//                        border — visually quiet, matches other inactive
//                        chrome.
//   ON  (throttle  > 0): "IGN" in TFT_BLACK on TFT_ORANGE with a grey
//                        border — high-contrast, catches the eye during a
//                        burn.
// Uses the library drawButton helper so styling is consistent with any
// future buttons and word-wrap/truncation behaviour matches the rest of
// the app.
//
// Only redraws when burn state changes.
static void _lnchOrUpdateIgnButton(RA8875 &tft) {
    int8_t active = (state.throttle > 0.0f) ? 1 : 0;
    if (active == _lnchOrPrevBurnActive) return;

    static const ButtonLabel IGN_LBL = {
        "IGN",
        TFT_LIGHT_GREY,   // fontColorOff
        TFT_BLACK,        // fontColorOn
        TFT_OFF_BLACK,    // backgroundColorOff
        TFT_INT_ORANGE,   // backgroundColorOn (International Orange — classic "lit" signal)
        TFT_GREY,         // borderColorOff
        TFT_GREY,         // borderColorOn
    };
    drawButton(tft, LNCH_OR_IGN_X, LNCH_OR_IGN_Y,
               LNCH_OR_IGN_W, LNCH_OR_IGN_H,
               IGN_LBL, &Roboto_Black_28, active != 0);
    _lnchOrPrevBurnActive = active;
}

// Update the Burn Duration readout — a centered label+value block below the
// IGN button. Shows the planned burn length (state.mnvrDuration). Helps the
// pilot mentally prepare before ignition and gauge progress during the burn.
//
// Label "Burn dur" stays light-grey on black (chrome — drawn once on first
// frame). Value formatTime(mnvrDuration) in dark green (or "---" / dark
// grey when no maneuver). Change detection on rounded seconds skips the
// redraw when nothing visible changed.
//
// Centered horizontally on LNCH_OR_ATT_CX. Width budget ~80 px (matches
// IGN button); typical formatTime values like "1m 23s" fit easily at
// Roboto_Black_24.
static void _lnchOrUpdateBurnDurReadout(RA8875 &tft) {
    bool hasMnvr = (state.mnvrTime > 0.0f || state.mnvrDeltaV > 0.0f) &&
                   state.mnvrDuration > 0.0f;

    int32_t  newSec;
    uint16_t valFg;
    String   val;
    if (!hasMnvr) {
        newSec = -9998;  // "---" placeholder sentinel
        valFg  = TFT_DARK_GREY;
        val    = "---";
    } else {
        newSec = (int32_t)roundf(state.mnvrDuration);
        valFg  = TFT_DARK_GREEN;
        val    = formatTime((int64_t)state.mnvrDuration);
    }

    bool first_draw = (_lnchOrPrevBurnDurSec == -9999);
    if (newSec == _lnchOrPrevBurnDurSec && !first_draw) return;

    // Draw the label "Burn Dur:" once on first frame — its colour and text
    // never change, so subsequent updates skip it. Black_20 (24 px).
    if (first_draw) {
        tft.setFont(&Roboto_Black_20);
        tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
        int16_t lblW = getFontStringWidth(&Roboto_Black_20, "Burn Dur:");
        tft.setCursor(LNCH_OR_ATT_CX - lblW / 2, LNCH_OR_BDUR_LBL_Y);
        tft.print("Burn Dur:");
    }

    // Clear the value region (a fixed-width rect centered on ATT_CX, tall
    // enough for Black_24's 29-px font).
    const int16_t valRegionW = 140;
    const int16_t valRegionX = LNCH_OR_ATT_CX - valRegionW / 2;
    tft.fillRect(valRegionX, LNCH_OR_BDUR_VAL_Y, valRegionW, 30, TFT_BLACK);

    // Draw the value, horizontally centered. Black_24 (29 px) — matches
    // the T+Ign value font for visual consistency across time displays.
    tft.setFont(&Roboto_Black_24);
    tft.setTextColor(valFg, TFT_BLACK);
    int16_t valW = getFontStringWidth(&Roboto_Black_24, val.c_str());
    tft.setCursor(LNCH_OR_ATT_CX - valW / 2, LNCH_OR_BDUR_VAL_Y);
    tft.print(val.c_str());

    _lnchOrPrevBurnDurSec = newSec;
}

// Wrap a heading error into ±180° (for bearing differences on a 0..360°
// compass). Duplicated from Screen_MNVR.ino (which declares it static).
static inline float _lnchOrWrapBrgErr(float e) {
    while (e >  180.0f) e -= 360.0f;
    while (e < -180.0f) e += 360.0f;
    return e;
}

// Draw the ATT indicator chrome: concentric rings at 5°, 10°, 15°, plus
// centre crosshairs. Drawn once on first-draw / SOI change; the dot is
// updated dynamically by _lnchOrUpdateAttIndicator. Also called to "repair"
// the chrome when a moving dot passes over it (same approach as LNDG).
static void _lnchOrDrawAttChrome(RA8875 &tft) {
    const int16_t CX = LNCH_OR_ATT_CX;
    const int16_t CY = LNCH_OR_ATT_CY;
    const int16_t R  = LNCH_OR_ATT_R;

    // Black disc (slightly larger than the outer ring — clears any old dot
    // pixels that may have been drawn outside the outer ring area).
    tft.fillCircle(CX, CY, R + 2, TFT_BLACK);

    // Good-zone inner fill (subtle off-black highlight inside the 5° ring).
    int16_t r5  = (int16_t)(5.0f  * LNCH_OR_ATT_SCALE);  // ≈ 17 px (5° at SCALE 3.47)
    int16_t r10 = (int16_t)(10.0f * LNCH_OR_ATT_SCALE);  // ≈ 35 px
    tft.fillCircle(CX, CY, r5, TFT_OFF_BLACK);

    // Reference rings: 5° green, 10° dark grey, 15° grey bezel.
    tft.drawCircle(CX, CY, r5, TFT_DARK_GREEN);
    tft.drawCircle(CX, CY, r10, TFT_DARK_GREY);
    tft.drawCircle(CX, CY, R,   TFT_GREY);

    // Centre crosshairs (cardinal axes with small centre gap so the dot at
    // centre isn't obscured by lines).
    const int16_t gap = 4;
    tft.drawLine(CX, CY - R + 1, CX, CY - gap, TFT_DARK_GREY);
    tft.drawLine(CX, CY + gap,   CX, CY + R - 1, TFT_DARK_GREY);
    tft.drawLine(CX - R + 1, CY, CX - gap, CY,   TFT_DARK_GREY);
    tft.drawLine(CX + gap,   CY, CX + R - 1, CY, TFT_DARK_GREY);

    // "ATT" label above the disc, centered horizontally on the disc.
    // Black_20 (24 px tall) — matches the "Burn Dur:" label below for
    // cluster-internal label-hierarchy consistency. Offset of 28 px above
    // disc keeps a 4 px gap between label bottom and disc top.
    tft.setFont(&Roboto_Black_20);
    tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
    int16_t lblW = getFontStringWidth(&Roboto_Black_20, "ATT");
    tft.setCursor(CX - lblW / 2, CY - R - 28);
    tft.print("ATT");
}

// Update the ATT indicator dot: shows attitude error vs the planned
// maneuver. Dot position follows MNVR's convention (dot = where to point;
// crosshair at centre = current attitude):
//   sx = CX + (-brgErr × SCALE)     — dot moves toward target heading
//   sy = CY + ( elvErr × SCALE)     — dot moves toward target pitch
// Dot clamped to outer ring (at 15° max). Colour by error magnitude.
//   errMag < 5°   → green
//   errMag < 15°  → yellow
//   errMag >= 15° → red
//   no maneuver   → inactive — dot erased (or drawn in dark grey at centre)
//
// Uses erase-prev-then-redraw with chrome touch-up (same pattern as LNDG).
static void _lnchOrUpdateAttIndicator(RA8875 &tft) {
    bool hasMnvr = (state.mnvrTime > 0.0f || state.mnvrDeltaV > 0.0f);

    int16_t newX, newY;
    uint16_t newCol;
    if (!hasMnvr) {
        // Inactive — park the dot at centre in dark grey.
        newX = LNCH_OR_ATT_CX;
        newY = LNCH_OR_ATT_CY;
        newCol = TFT_DARK_GREY;
    } else {
        float brgErr = _lnchOrWrapBrgErr(state.heading - state.mnvrHeading);
        float elvErr = state.pitch - state.mnvrPitch;
        float errMag = sqrtf(brgErr * brgErr + elvErr * elvErr);

        // Raw dot position (MNVR convention: dot is where you need to point).
        float fx = (float)LNCH_OR_ATT_CX + (-brgErr * LNCH_OR_ATT_SCALE);
        float fy = (float)LNCH_OR_ATT_CY + ( elvErr * LNCH_OR_ATT_SCALE);
        // Clamp to outer ring.
        float dx = fx - (float)LNCH_OR_ATT_CX;
        float dy = fy - (float)LNCH_OR_ATT_CY;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist > (float)LNCH_OR_ATT_R) {
            fx = (float)LNCH_OR_ATT_CX + dx * (float)LNCH_OR_ATT_R / dist;
            fy = (float)LNCH_OR_ATT_CY + dy * (float)LNCH_OR_ATT_R / dist;
        }
        newX = (int16_t)roundf(fx);
        newY = (int16_t)roundf(fy);

        if      (errMag < LNCH_OR_ALIGN_GREEN_DEG)  newCol = TFT_DARK_GREEN;
        else if (errMag < LNCH_OR_ALIGN_YELLOW_DEG) newCol = TFT_YELLOW;
        else                                        newCol = TFT_RED;
    }

    // Skip if nothing moved and colour is unchanged.
    if (newX == _lnchOrPrevAttDotX &&
        newY == _lnchOrPrevAttDotY &&
        newCol == _lnchOrPrevAttDotCol) return;

    // On first draw, paint the chrome (rings, crosshairs, label). On later
    // frames, erase the old dot by black-filling a slightly-larger circle
    // then repair the chrome over the erased pixels. Same approach as LNDG.
    if (_lnchOrPrevAttDotX == 9999) {
        _lnchOrDrawAttChrome(tft);
    } else {
        tft.fillCircle(_lnchOrPrevAttDotX, _lnchOrPrevAttDotY, 8, TFT_BLACK);
        _lnchOrDrawAttChrome(tft);
    }

    // Draw new dot: 6-px filled circle in the error-magnitude colour, with a
    // small bright centre pip for visibility.
    tft.fillCircle(newX, newY, 6, newCol);
    tft.fillCircle(newX, newY, 2, TFT_WHITE);

    _lnchOrPrevAttDotX   = newX;
    _lnchOrPrevAttDotY   = newY;
    _lnchOrPrevAttDotCol = newCol;
}

// Draw the full orbital graphic using erase-then-redraw on the dynamic layer
// (orbit curve, Pe/Ap markers, vessel) while leaving the static layer (body,
// atmosphere, target circle) untouched unless the SOI has changed. This
// eliminates the visible flicker that a full-rectangle black wipe every frame
// would cause.
//
// Order of operations:
//   1. Compute all new geometry.
//   2. If SOI changed (or first draw), clear the diagram area and draw the
//      static layer.
//   3. Erase previous dynamic elements (orbit curve segments, markers, vessel)
//      by redrawing them in black at their saved pixel positions.
//   4. Touch up the static layer over regions the erase may have blackened.
//   5. Draw new dynamic elements, saving pixel positions for next frame's erase.
static void _lnchOrDrawOrbitGraphic(RA8875 &tft) {
    const int16_t CX = LNCH_OR_DIAG_CX;
    const int16_t CY = LNCH_OR_DIAG_CY;
    const int16_t MAX_R = LNCH_OR_DIAG_MAX_R;

    // ── Body parameters ─────────────────────────────────────────────────────────
    float bodyR    = (currentBody.radius   > 0.0f) ? currentBody.radius   : 600000.0f;
    float atmoTop  = (currentBody.lowSpace > 0.0f) ? currentBody.lowSpace : 0.0f;
    float targetAlt = max(currentBody.minSafe, currentBody.lowSpace + 20000.0f);
    float rTarget  = targetAlt + bodyR;

    // ── Current orbit parameters ────────────────────────────────────────────────
    float apaAlt = fmaxf(0.0f, state.apoapsis);
    float peaAlt = fmaxf(0.0f, state.periapsis);
    float rAp    = apaAlt + bodyR;
    float rPe    = fmaxf(peaAlt + bodyR, bodyR);
    float sma    = (rAp + rPe) * 0.5f;
    float ecc    = (rAp > rPe + 1.0f) ? ((rAp - rPe) / (rAp + rPe)) : 0.0f;

    float argOfPe_rad = state.argOfPe * (float)(PI / 180.0f);
    float cos_w = cosf(argOfPe_rad);
    float sin_w = sinf(argOfPe_rad);
    float nu_rad = state.trueAnomaly * (float)(PI / 180.0f);

    // ── Compressed radial scaling (see header comment) ──────────────────────────
    const float BODY_CAP_FRAC = 0.55f;
    float rMax       = fmaxf(fmaxf(rAp, rTarget), bodyR + 1000.0f);
    float body_cap   = (float)MAX_R * BODY_CAP_FRAC;
    float alt_extent = rMax - bodyR;
    float alt_scale  = ((float)MAX_R - body_cap) / alt_extent;

    auto r_to_px = [&](float r) -> float {
        if (r <= bodyR) return (r / bodyR) * body_cap;
        return body_cap + (r - bodyR) * alt_scale;
    };

    int16_t body_px   = (int16_t)fmaxf(3.0f, body_cap);
    int16_t atmo_px   = (atmoTop > 0.0f) ? (int16_t)r_to_px(bodyR + atmoTop) : 0;
    int16_t target_px = (int16_t)r_to_px(rTarget);
    uint16_t bodyCol  = kspBodyColor(currentBody.soiName);
    uint16_t atmoCol  = kspAtmoColor(currentBody.soiName);

    // ── Static layer: draw once (or redraw if SOI changed) ──────────────────────
    bool soi_changed = _lnchOrSoiChanged();
    bool first_draw  = !_lnchOrPrevCurveValid;
    if (soi_changed || first_draw) {
        // Full clear of diagram area + progress-bar area, then draw static
        // elements. This is the ONE PLACE we use fillRect — and only on rare
        // events (entry to orbital mode, SOI change).
        const int16_t clearMargin = 20;
        tft.fillRect(CX - MAX_R - clearMargin, CY - MAX_R - clearMargin,
                     2 * (MAX_R + clearMargin), 2 * (MAX_R + clearMargin),
                     TFT_BLACK);
        // Clear the bar + label + T+Ign area. Width stops at RPANEL_X - 2
        // so it doesn't touch the vertical divider at x=RPANEL_X-2..-1
        // (451..452). Vertical span covers the bar label row, the bar itself,
        // and the T+Ign row below the bar.
        tft.fillRect(0, LNCH_OR_BAR_LBL_Y - 2, LNCH_AS_RPANEL_X - 2,
                     (LNCH_OR_TIGN_Y + LNCH_OR_TIGN_H + 1) - (LNCH_OR_BAR_LBL_Y - 2),
                     TFT_BLACK);
        _lnchOrDrawStaticLayer(tft, CX, CY, body_px, atmo_px, target_px,
                               bodyCol, atmoCol);
        _lnchOrDrawProgressBarChrome(tft);
        // Record SOI we drew static layer for
        const char *soi = currentBody.soiName ? currentBody.soiName : "";
        strncpy(_lnchOrPrevSoi, soi, sizeof(_lnchOrPrevSoi) - 1);
        _lnchOrPrevSoi[sizeof(_lnchOrPrevSoi) - 1] = '\0';
        // Invalidate prev-frame dynamic state — nothing to erase (we just
        // blanked the area).
        _lnchOrPrevCurveValid = false;
        _lnchOrPrevPeValid    = false;
        _lnchOrPrevApValid    = false;
        _lnchOrPrevVslValid   = false;
        _lnchOrPrevDirValid   = false;
        _lnchOrPrevBarFill    = -1;
        _lnchOrPrevBarCol     = 0;
        _lnchOrPrevBarDvRounded = -9999;
        _lnchOrPrevBarTextCol = 0;
        _lnchOrPrevBarEstimate = false;
        _lnchOrPrevTignSec    = -9999;
        _lnchOrPrevTignFg     = 0;
        _lnchOrPrevTignBg     = 0;
        _lnchOrTignPs.prevWidth  = 0;
        _lnchOrTignPs.prevBg     = 0x0001;
        _lnchOrTignPs.prevHeight = 0;
        _lnchOrPrevBurnActive = -1;
        _lnchOrPrevAttDotX    = 9999;
        _lnchOrPrevAttDotY    = 9999;
        _lnchOrPrevAttDotCol  = 0;
        _lnchOrPrevBurnDurSec = -9999;
        // Record the static-layer geometry we just drew so that on future
        // frames we can detect radius drift (due to compressed-scaling
        // recomputation as orbit elements change) and cleanly erase before
        // redrawing.
        _lnchOrPrevAtmoPx   = atmo_px;
        _lnchOrPrevTargetPx = target_px;
        _lnchOrPrevBodyPx   = body_px;
    }

    // ── Compute new dynamic element positions ───────────────────────────────────
    // Orbit curve points (N+1 points, N segments between them)
    int16_t newCurveX[LNCH_OR_CURVE_N + 1];
    int16_t newCurveY[LNCH_OR_CURVE_N + 1];
    for (uint8_t i = 0; i <= LNCH_OR_CURVE_N; i++) {
        float nu = 2.0f * (float)PI * (float)i / (float)LNCH_OR_CURVE_N;
        float denom = 1.0f + ecc * cosf(nu);
        float r_m = (fabsf(denom) > 1e-5f) ? sma * (1.0f - ecc * ecc) / denom : sma;
        float r_px = r_to_px(r_m);
        float angle = argOfPe_rad + nu;
        newCurveX[i] = CX + (int16_t)roundf(r_px * cosf(angle));
        newCurveY[i] = CY - (int16_t)roundf(r_px * sinf(angle));
    }

    // Pe/Ap marker positions. The DOT sits ON the orbit curve at the
    // compressed pixel radius. The text LABEL is offset outward along the
    // major axis by LNCH_OR_LABEL_OFFSET_PX pixels so it sits beyond the
    // curve rather than overlapping the dot or the curve itself. This
    // matches the ORB screen's label-placement convention.
    const float LABEL_OFFSET_PX = 12.0f;
    float rPe_px = r_to_px(rPe);                       // dot radius (on curve)
    float rAp_px = r_to_px(rAp);                       // dot radius (on curve)
    float rPe_lbl_px = rPe_px + LABEL_OFFSET_PX;       // label radius (outboard)
    float rAp_lbl_px = rAp_px + LABEL_OFFSET_PX;
    int16_t pe_x = CX + (int16_t)roundf(rPe_px * cos_w);
    int16_t pe_y = CY - (int16_t)roundf(rPe_px * sin_w);
    int16_t ap_x = CX - (int16_t)roundf(rAp_px * cos_w);
    int16_t ap_y = CY + (int16_t)roundf(rAp_px * sin_w);
    // Label anchor points (center of the 2-char label). We offset the cursor
    // from this by half the text width/height below for centering.
    int16_t pe_lbl_cx = CX + (int16_t)roundf(rPe_lbl_px * cos_w);
    int16_t pe_lbl_cy = CY - (int16_t)roundf(rPe_lbl_px * sin_w);
    int16_t ap_lbl_cx = CX - (int16_t)roundf(rAp_lbl_px * cos_w);
    int16_t ap_lbl_cy = CY + (int16_t)roundf(rAp_lbl_px * sin_w);

    // Vessel position (dot on the orbit curve).
    bool vessel_valid = false;
    int16_t vsl_x = 0, vsl_y = 0;
    if (apaAlt > 0.0f || peaAlt > 0.0f) {
        float denom = 1.0f + ecc * cosf(nu_rad);
        if (fabsf(denom) > 1e-5f) {
            float rVsl_m  = sma * (1.0f - ecc * ecc) / denom;
            float rVsl_px = r_to_px(rVsl_m);
            float angle   = argOfPe_rad + nu_rad;
            vsl_x = CX + (int16_t)roundf(rVsl_px * cosf(angle));
            vsl_y = CY - (int16_t)roundf(rVsl_px * sinf(angle));
            vessel_valid = true;
        }
    }

    // Direction indicator: small chevron embedded on the orbit curve at ν=90°
    // (a quarter orbit past Pe, on the way toward Ap). Points along the
    // tangent of the curve in the direction of travel:
    //   Prograde  (inclination < 90°): chevron at ν=90° pointing toward ν>90°
    //   Retrograde (inclination > 90°): chevron at ν=270° pointing toward ν<270°
    // Using different reference ν for the two cases so the chevron always sits
    // on the "far side" of the orbit from the vessel's approach direction,
    // making the visual meaning unambiguous.
    bool retrograde = (state.inclination > 90.0f);
    float dir_nu = retrograde ? (3.0f * (float)PI / 2.0f) : ((float)PI / 2.0f);
    // Position on compressed orbit curve at dir_nu.
    float dir_denom = 1.0f + ecc * cosf(dir_nu);
    float dir_r_m = (fabsf(dir_denom) > 1e-5f) ? sma * (1.0f - ecc * ecc) / dir_denom : sma;
    float dir_r_px = r_to_px(dir_r_m);
    float dir_angle = argOfPe_rad + dir_nu;
    int16_t dir_x = CX + (int16_t)roundf(dir_r_px * cosf(dir_angle));
    int16_t dir_y = CY - (int16_t)roundf(dir_r_px * sinf(dir_angle));
    // Tangent direction at dir_nu: small forward step, take vector.
    float dir_step = retrograde ? -0.03f : 0.03f;
    float nu2 = dir_nu + dir_step;
    float den2 = 1.0f + ecc * cosf(nu2);
    float r2_m = (fabsf(den2) > 1e-5f) ? sma * (1.0f - ecc * ecc) / den2 : sma;
    float r2_px = r_to_px(r2_m);
    float ang2  = argOfPe_rad + nu2;
    float dir_dx = ((float)CX + r2_px * cosf(ang2)) - (float)dir_x;
    float dir_dy = ((float)CY - r2_px * sinf(ang2)) - (float)dir_y;
    float dir_mag = sqrtf(dir_dx * dir_dx + dir_dy * dir_dy);
    if (dir_mag > 0.01f) {
        dir_dx /= dir_mag;
        dir_dy /= dir_mag;
    } else {
        dir_dx = 1.0f; dir_dy = 0.0f;
    }

    // Determine orbit colour: green if Pe safely above atmo, yellow if Pe in
    // atmosphere (orbit will decay), red if Pe at/below surface (impact).
    uint16_t orbitCol;
    if (peaAlt <= 0.0f)           orbitCol = TFT_RED;
    else if (peaAlt < atmoTop)    orbitCol = TFT_YELLOW;
    else                          orbitCol = TFT_DARK_GREEN;

    // ── Change detection: only erase+redraw elements that actually moved ────────
    // Comparing new positions to saved prev positions. Orbit curve comparison
    // uses max pixel deviation; if all points are within 0-1 px of prev, the
    // curve is visually unchanged and doesn't need to be erased/redrawn.
    bool orbit_changed = !_lnchOrPrevCurveValid;
    if (!orbit_changed) {
        // Check if any segment endpoint moved by more than 0 pixels (strict).
        // Even 1px would be visible as flicker, so only skip if exactly equal.
        for (uint8_t i = 0; i <= LNCH_OR_CURVE_N; i++) {
            if (newCurveX[i] != _lnchOrPrevCurveX[i] ||
                newCurveY[i] != _lnchOrPrevCurveY[i]) {
                orbit_changed = true;
                break;
            }
        }
    }
    // Also redraw the orbit if its colour changed (peaAlt crossed atmo threshold).
    if (orbitCol != _lnchOrPrevOrbitCol) {
        orbit_changed = true;
        _lnchOrPrevOrbitCol = orbitCol;
    }

    bool pe_changed = !_lnchOrPrevPeValid ||
                      pe_x != _lnchOrPrevPeX || pe_y != _lnchOrPrevPeY;
    bool ap_changed = !_lnchOrPrevApValid ||
                      ap_x != _lnchOrPrevApX || ap_y != _lnchOrPrevApY;
    bool vsl_changed = !_lnchOrPrevVslValid ||
                       (vessel_valid && (vsl_x != _lnchOrPrevVslX ||
                                         vsl_y != _lnchOrPrevVslY)) ||
                       (!vessel_valid && _lnchOrPrevVslValid);
    // Direction indicator position depends on argOfPe + eccentricity. Its
    // orientation depends on those + inclination sign. Any orbit-shape change
    // moves the indicator, so it shares the orbit_changed lifecycle.
    bool dir_changed = orbit_changed;

    // ── Erase previous dynamic elements (only if they changed) ──────────────────
    // Draw previous shapes in black to erase. Drawing a line in black over
    // exactly the same path is a clean erase (line algorithm is deterministic).
    if (orbit_changed && _lnchOrPrevCurveValid) {
        for (uint8_t i = 0; i < LNCH_OR_CURVE_N; i++) {
            tft.drawLine(_lnchOrPrevCurveX[i],     _lnchOrPrevCurveY[i],
                         _lnchOrPrevCurveX[i + 1], _lnchOrPrevCurveY[i + 1],
                         TFT_BLACK);
        }
    }
    if (pe_changed && _lnchOrPrevPeValid) {
        // Erase prev dot
        tft.fillCircle(_lnchOrPrevPeX, _lnchOrPrevPeY, 4, TFT_BLACK);
        // Erase prev label — centered at _lnchOrPrevPeLbl{X,Y}, extents
        // ~10×8 from center (Black_12 is 14 px tall, "Pe" is ~14 px wide).
        tft.fillRect(_lnchOrPrevPeLblX - 10, _lnchOrPrevPeLblY - 9,
                     20, 18, TFT_BLACK);
    }
    if (ap_changed && _lnchOrPrevApValid) {
        tft.fillCircle(_lnchOrPrevApX, _lnchOrPrevApY, 4, TFT_BLACK);
        tft.fillRect(_lnchOrPrevApLblX - 10, _lnchOrPrevApLblY - 9,
                     20, 18, TFT_BLACK);
    }
    if (vsl_changed && _lnchOrPrevVslValid) {
        // Erase prev vessel dot (r=4 filled, outline r=4 → extent 5 for safety).
        tft.fillCircle(_lnchOrPrevVslX, _lnchOrPrevVslY, 5, TFT_BLACK);
    }
    if (dir_changed && _lnchOrPrevDirValid) {
        // Erase prev direction chevron. Triangle tip 7 px forward, base ±5 px
        // perpendicular 5 back → max extent from center ≈ 7.1 px. Use 9 px
        // radius for safety incl. 1-px outline.
        tft.fillCircle(_lnchOrPrevDirX, _lnchOrPrevDirY, 9, TFT_BLACK);
    }

    // ── Touch-up static layer (only if erase happened) ─────────────────────────
    // If nothing was erased, the static layer is untouched and we don't need
    // to redraw it. This is the main flicker-avoidance optimisation.
    //
    // Special case: if the static-layer GEOMETRY (atmo_px, target_px, body_px)
    // drifted from the previously-drawn values — this happens because
    // compressed scaling depends on rMax which depends on the current orbit's
    // apoapsis — we must erase the OLD rings first, otherwise overdrawing at
    // the new radius leaves the old rings visible and the lines appear to
    // thicken over time.
    bool static_geom_changed = (atmo_px   != _lnchOrPrevAtmoPx)   ||
                               (target_px != _lnchOrPrevTargetPx) ||
                               (body_px   != _lnchOrPrevBodyPx);
    bool any_erase = (orbit_changed && _lnchOrPrevCurveValid) ||
                     (pe_changed    && _lnchOrPrevPeValid)    ||
                     (ap_changed    && _lnchOrPrevApValid)    ||
                     (vsl_changed   && _lnchOrPrevVslValid)   ||
                     (dir_changed   && _lnchOrPrevDirValid);
    if ((any_erase || static_geom_changed) && !first_draw && !soi_changed) {
        // If the static-layer geometry drifted, erase old rings at their
        // previous radii before redrawing at new radii.
        if (static_geom_changed) {
            if (_lnchOrPrevAtmoPx > 0 && atmoCol != 0) {
                tft.drawCircle(CX, CY, _lnchOrPrevAtmoPx, TFT_BLACK);
            }
            if (_lnchOrPrevTargetPx > 0) {
                // Erase dashed target circle by redrawing its 20 drawn
                // segments in black using the same loop logic.
                const int N = 40;
                const int skip = 2;
                float dtheta = 2.0f * (float)PI / N;
                int16_t px = CX + _lnchOrPrevTargetPx;
                int16_t py = CY;
                for (int i = 1; i <= N; i++) {
                    float theta = i * dtheta;
                    int16_t nx = CX + (int16_t)roundf((float)_lnchOrPrevTargetPx * cosf(theta));
                    int16_t ny = CY - (int16_t)roundf((float)_lnchOrPrevTargetPx * sinf(theta));
                    if ((i % skip) == 0) {
                        tft.drawLine(px, py, nx, ny, TFT_BLACK);
                    }
                    px = nx; py = ny;
                }
            }
            // Body rarely changes radius (body_cap is MAX_R * 0.55 constant),
            // but handle for completeness.
            if (_lnchOrPrevBodyPx > 0 && _lnchOrPrevBodyPx != body_px) {
                tft.fillCircle(CX, CY, _lnchOrPrevBodyPx, TFT_BLACK);
            }
        }
        _lnchOrDrawStaticLayer(tft, CX, CY, body_px, atmo_px, target_px,
                               bodyCol, atmoCol);
        _lnchOrPrevAtmoPx   = atmo_px;
        _lnchOrPrevTargetPx = target_px;
        _lnchOrPrevBodyPx   = body_px;
    }

    // ── Draw new dynamic elements (only if they changed) ────────────────────────
    if (orbit_changed) {
        for (uint8_t i = 0; i < LNCH_OR_CURVE_N; i++) {
            tft.drawLine(newCurveX[i],     newCurveY[i],
                         newCurveX[i + 1], newCurveY[i + 1],
                         orbitCol);
        }
        memcpy(_lnchOrPrevCurveX, newCurveX, sizeof(newCurveX));
        memcpy(_lnchOrPrevCurveY, newCurveY, sizeof(newCurveY));
        _lnchOrPrevCurveValid = true;
    }

    // Direction indicator — drawn after the curve so the chevron sits ON TOP.
    // Colour matches the orbit curve so it reads as a natural extension of it.
    if (dir_changed) {
        _lnchOrDrawChevron(tft, dir_x, dir_y, dir_dx, dir_dy, orbitCol);
        _lnchOrPrevDirX = dir_x;
        _lnchOrPrevDirY = dir_y;
        _lnchOrPrevDirValid = true;
    }

    if (pe_changed) {
        // Dot ON the orbit curve
        tft.fillCircle(pe_x, pe_y, 3, TFT_MAGENTA);
        // Label centered at outboard anchor. setCursor sets top-left of text;
        // "Pe" at Black_12 is ~14 px wide × 14 px tall → offset (-7, -7).
        tft.setFont(&Roboto_Black_12);
        tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
        tft.setCursor(pe_lbl_cx - 7, pe_lbl_cy - 7);
        tft.print("Pe");
        _lnchOrPrevPeX = pe_x; _lnchOrPrevPeY = pe_y;
        _lnchOrPrevPeLblX = pe_lbl_cx; _lnchOrPrevPeLblY = pe_lbl_cy;
        _lnchOrPrevPeValid = true;
    }
    if (ap_changed) {
        tft.fillCircle(ap_x, ap_y, 3, TFT_CYAN);
        tft.setFont(&Roboto_Black_12);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setCursor(ap_lbl_cx - 7, ap_lbl_cy - 7);
        tft.print("Ap");
        _lnchOrPrevApX = ap_x; _lnchOrPrevApY = ap_y;
        _lnchOrPrevApLblX = ap_lbl_cx; _lnchOrPrevApLblY = ap_lbl_cy;
        _lnchOrPrevApValid = true;
    }
    // Vessel: filled neon-green dot with black outline for contrast.
    if (vsl_changed) {
        if (vessel_valid) {
            tft.fillCircle(vsl_x, vsl_y, 4, TFT_NEON_GREEN);
            tft.drawCircle(vsl_x, vsl_y, 4, TFT_BLACK);
            _lnchOrPrevVslX = vsl_x;
            _lnchOrPrevVslY = vsl_y;
            _lnchOrPrevVslValid = true;
        } else {
            _lnchOrPrevVslValid = false;
        }
    }
}

// Dispatcher: call this each frame during orbital phase. Throttled redraw of
// the orbit graphic and progress bar.
static void _lnchOrDrawLeftPanelValues(RA8875 &tft) {
    uint32_t now = millis();
    if (now - _lnchOrLastDiagDraw < 200) return;  // ~5 Hz cap
    _lnchOrLastDiagDraw = now;
    _lnchOrDrawOrbitGraphic(tft);
    _lnchOrUpdateProgressBar(tft);
    _lnchOrUpdateTignRow(tft);
    _lnchOrUpdateIgnButton(tft);
    _lnchOrUpdateAttIndicator(tft);
    _lnchOrUpdateBurnDurReadout(tft);
}
