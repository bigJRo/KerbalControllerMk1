/***************************************************************************************
   Screen_LNCH_Ascent.ino -- Ascent phase of the LNCH screen.

   Active when state.altitude is below ~6% of bodyR (with hysteresis at 5.5% on the
   way back down). This is the "graphical flight instrument" view used during the
   gravity-turn ascent.

   LEFT PANEL (x=0..452):
     - Altitude ladder (x=0..155): vertical altitude scale with vessel + apoapsis
       markers, ground/atmosphere/orbit reference lines
     - V.Vrt bar (x=180..220): vertical velocity bar, ±500 m/s scale
     - V.Orb bar (x=240..280): orbital velocity bar
     - FPA dial (x=287, R=80): flight path angle indicator with target marker
     - Atmosphere gauge (x=287..440, y=394..434): atmospheric density bar with
       triangle indicator showing current altitude's atmo fraction

   RIGHT PANEL (x=453..720):
     - 8 numeric readouts: Alt.SL, ApA, T+Ap, V.Srf/V.Orb, V.Vrt, Thrtl, T.Brn, ΔV.Stg
     - Grouped horizontal dividers between altitude / velocity / propulsion groups

   Phase membership (LNCH screen has three phases):
     - PRE-LAUNCH (Screen_LNCH_PreLaunch.ino)  — auto, when sit_PreLaunch is set
     - ASCENT     (this file)                   — when below switch altitude
     - CIRC       (Screen_LNCH_Circ.ino)       — when above switch altitude

   Top-level dispatcher Screen_LNCH.ino selects which of these to draw.

   Shared with the rest of the LNCH screen (defined in Screen_LNCH.ino):
     - LNCH_AS_LPANEL_X/W, LNCH_AS_PANEL_Y/H  (left graphics panel geometry)
     The ascent-specific code below references these freely (single Arduino TU).
     The rev-2 right (numeric readout) panel uses its own LNCH_AS2_* geometry and
     _lnchAs2RowY() — right-aligned to the content edge, 400 px wide, full height,
     larger fonts. Circularization still uses the shared LNCH_AS_RPANEL_* / ROW_H.

   Public-to-the-sketch entry points (called by the LNCH dispatcher):
     - _lnchAsResetState()              — clear all change-detection state
     - _lnchAsDrawLeftPanelChrome(tft)  — initial graphics chrome (ladder, dial, etc.)
     - _lnchAsDrawRightPanelChrome(tft) — initial labels + dividers for right panel
     - _lnchAsDrawLeftPanelValues(tft)  — per-frame left panel value updates
     - _lnchAsDrawRightPanelValues(tft) — per-frame right panel value updates
****************************************************************************************/

// ═══════════════════════════════════════════════════════════════════════════
//  ASCENT LAYOUT ANCHORS — resize the whole screen from here.
//  Every element below derives its position from these anchors, so changing the
//  readout width, the shared vertical band, or the panel split reflows all
//  gauges without touching their individual positioning code.
// ═══════════════════════════════════════════════════════════════════════════

// Right-hand numeric readout column (right-aligned to the content edge).
static const int16_t LNCH_AS_READOUT_W = 360;
static const int16_t LNCH_AS_READOUT_X = SCREEN_W - SIDEBAR_W - LNCH_AS_READOUT_W;   // 580

// Left graphics panel: everything left of the readout column.
static const int16_t LNCH_AS_LP_LEFT   = 0;
static const int16_t LNCH_AS_LP_RIGHT  = LNCH_AS_READOUT_X - 4;      // small gap before readout (576)

// Shared vertical band for the ladder and all gauges. Move them together here.
static const int16_t LNCH_AS_BAND_TOP  = TITLE_TOP + 18;            // 80  (~18px under title bar)
static const int16_t LNCH_AS_BAND_BOT  = SCREEN_H  - 18;            // 582 (~18px above screen bottom)

// Gauge label rows within the band (name label on top; endpoint labels flank
// the gauge body). The bars/FPA/atmosphere bodies sit between BODY_TOP/BOT; the
// ladder uses the full band (it carries its own inline labels).
static const int16_t LNCH_AS_GAUGE_NAME_H   = 24;   // name label height (Black_20)
static const int16_t LNCH_AS_GAUGE_ENDLBL_H = 18;   // endpoint label height (Black_16)
static const int16_t LNCH_AS_GAUGE_NAME_Y   = LNCH_AS_BAND_TOP;                                                     // 80
static const int16_t LNCH_AS_GAUGE_BODY_TOP = LNCH_AS_BAND_TOP + LNCH_AS_GAUGE_NAME_H + LNCH_AS_GAUGE_ENDLBL_H + 6; // 128
static const int16_t LNCH_AS_GAUGE_BODY_BOT = LNCH_AS_BAND_BOT - LNCH_AS_GAUGE_ENDLBL_H - 2;                        // 562
static const int16_t LNCH_AS_GAUGE_BODY_MIDY= (LNCH_AS_GAUGE_BODY_TOP + LNCH_AS_GAUGE_BODY_BOT) / 2;

// Horizontal spacing: COL_GAP = whitespace between element bounding boxes;
// LBL_OVERHANG = how far each bar's endpoint-label box extends beyond the bar.
static const int16_t LNCH_AS_COL_GAP      = 10;   // compact whitespace to fit the larger FPA
static const int16_t LNCH_AS_LBL_OVERHANG = 14;

// Left panel — altitude ladder (first column).
// Vertical strip showing altitude scale with tick labels, reference lines
// (ground/atmo/orbit), vessel marker, and apoapsis marker. Anchored at the left
// edge of the graphics panel; fills the full shared band vertically.
static const int16_t LNCH_AS_LADDER_LBL_X    = LNCH_AS_LP_LEFT;        // tick label column (0)
static const int16_t LNCH_AS_LADDER_LBL_W    = 66;    // tick label column width
static const int16_t LNCH_AS_LADDER_LINE_X   = LNCH_AS_LP_LEFT + 72;   // main vertical line
static const int16_t LNCH_AS_LADDER_TICK_W   = 10;    // major tick mark length (minor = half)
static const int16_t LNCH_AS_LADDER_MARKER_X = LNCH_AS_LP_LEFT + 82;   // tip x for markers
static const int16_t LNCH_AS_LADDER_MARKER_W = 14;    // marker base width
static const int16_t LNCH_AS_LADDER_REF_X2   = LNCH_AS_LP_LEFT + 150;  // right edge of reference lines
static const int16_t LNCH_AS_LADDER_Y_TOP    = LNCH_AS_BAND_TOP;       // top of ladder strip
static const int16_t LNCH_AS_LADDER_Y_BOT    = LNCH_AS_BAND_BOT;       // bottom of ladder strip (= ground line)
static const int16_t LNCH_AS_LADDER_ERASE_X2 = LNCH_AS_LP_LEFT + 156;  // right edge for scale-change erase
static const int16_t LNCH_AS_LADDER_H        = LNCH_AS_LADDER_Y_BOT - LNCH_AS_LADDER_Y_TOP;

// Column order, left → right:  ladder | ATMO | V.Vrt | V.Orb | FPA

// Atmosphere gauge geometry (second column, left-anchored after the ladder).
// VERTICAL bar: sea level (dense) at TOP, vacuum at BOTTOM, with a white triangle
// indicator on EACH side. Body width matches the velocity bars. Zone/tick
// parameters are defined further down.
static const int16_t LNCH_AS_ATMO_W          = 44;
static const int16_t LNCH_AS_ATMO_TRI_HALF_H = 7;    // triangle half height
static const int16_t LNCH_AS_ATMO_TRI_W      = 12;   // triangle base-to-tip depth
static const int16_t LNCH_AS_ATMO_TRI_GAP    = 2;    // gap between a tip and the bar edge
static const int16_t LNCH_AS_ATMO_X_LEFT     = LNCH_AS_LADDER_ERASE_X2 + LNCH_AS_COL_GAP
                                               + LNCH_AS_ATMO_TRI_GAP + LNCH_AS_ATMO_TRI_W;  // room for the left triangle
static const int16_t LNCH_AS_ATMO_X_RIGHT    = LNCH_AS_ATMO_X_LEFT + LNCH_AS_ATMO_W;
static const int16_t LNCH_AS_ATMO_Y_TOP      = LNCH_AS_GAUGE_BODY_TOP;   // sea level (dense)
static const int16_t LNCH_AS_ATMO_Y_BOT      = LNCH_AS_GAUGE_BODY_BOT;   // vacuum / no-atmosphere
// Right extent (right-triangle base) — the next column starts a COL_GAP after this.
static const int16_t LNCH_AS_ATMO_RIGHT_EXT  = LNCH_AS_ATMO_X_RIGHT + LNCH_AS_ATMO_TRI_GAP + LNCH_AS_ATMO_TRI_W;

// V.Vrt bar (third column) — vertical velocity, fixed ±500 m/s.
static const int16_t LNCH_AS_VVRT_W          = 44;   // narrow bar; label box overhangs for "±500 m/s"
static const int16_t LNCH_AS_VVRT_X_LEFT     = LNCH_AS_ATMO_RIGHT_EXT + LNCH_AS_COL_GAP + LNCH_AS_LBL_OVERHANG;
static const int16_t LNCH_AS_VVRT_Y_TOP      = LNCH_AS_GAUGE_BODY_TOP;
static const int16_t LNCH_AS_VVRT_Y_BOT      = LNCH_AS_GAUGE_BODY_BOT;
static const int16_t LNCH_AS_VVRT_Y_MID      = (LNCH_AS_VVRT_Y_TOP + LNCH_AS_VVRT_Y_BOT) / 2;
static const float   LNCH_AS_VVRT_SCALE_MS   = 500.0f;

// V.Orb bar (fourth column) — orbital velocity toward v_circ (body-aware).
static const int16_t LNCH_AS_VORB_W          = 44;
static const int16_t LNCH_AS_VORB_X_LEFT     = LNCH_AS_VVRT_X_LEFT + LNCH_AS_VVRT_W + 2 * LNCH_AS_LBL_OVERHANG + LNCH_AS_COL_GAP;
static const int16_t LNCH_AS_VORB_Y_TOP      = LNCH_AS_GAUGE_BODY_TOP;
static const int16_t LNCH_AS_VORB_Y_BOT      = LNCH_AS_GAUGE_BODY_BOT;

// Flight Path Angle dial (fifth column, rightmost) — semicircle (right half of a
// circle), flat side on the left at x=CX, a needle rotating from the center.
// -90°=diving (bottom), 0°=horizon (right), +90°=climbing (top). Top-aligned with
// the bars: the arc top sits near the gauge body top rather than centered.
static const int16_t LNCH_AS_FPA_LBL_MARGIN = 28;   // +90/-90 labels extend this far left of CX
static const int16_t LNCH_AS_FPA_R  = 96;           // radius (= width; height = 2R)
static const int16_t LNCH_AS_FPA_CX = LNCH_AS_VORB_X_LEFT + LNCH_AS_VORB_W + LNCH_AS_LBL_OVERHANG + LNCH_AS_COL_GAP + LNCH_AS_FPA_LBL_MARGIN;  // flat left side
static const int16_t LNCH_AS_FPA_CY = LNCH_AS_GAUGE_BODY_TOP + LNCH_AS_FPA_R + 16;   // top-aligned (arc top ≈ body top)
static const int16_t LNCH_AS_FPA_ARROW_R = 84;      // needle tip distance from center

// Radial tick extents (px inside / outside the radius).
static const int16_t LNCH_AS_FPA_MAJ_OUT = 8;
static const int16_t LNCH_AS_FPA_MAJ_IN  = 8;
static const int16_t LNCH_AS_FPA_MIN_OUT = 4;
static const int16_t LNCH_AS_FPA_MIN_IN  = 4;

// Needle geometry (rotated rectangle shaft + triangle head).
static const int16_t LNCH_AS_FPA_SHAFT_LEN = 70;    // shaft length from center
static const int16_t LNCH_AS_FPA_SHAFT_W   = 6;     // shaft width
static const int16_t LNCH_AS_FPA_HEAD_W    = 16;    // arrowhead base width
static const int16_t LNCH_AS_FPA_PIVOT_R   = 7;     // pivot circle radius

// Zone boundaries as fractions of the atmospheric portion (0 = vacuum end/bottom,
// 1 = sea level/top). Bottom 10% is the OFF_BLACK "no atmosphere" parking segment.
static const float   LNCH_AS_ATMO_ZONE1_FRAC   = 0.35f;  // NAVY ↔ FRENCH_BLUE
static const float   LNCH_AS_ATMO_ZONE2_FRAC   = 0.75f;  // FRENCH_BLUE ↔ SKY
static const float   LNCH_AS_ATMO_NOATM_H_FRAC = 0.10f;  // bottom 10% OFF_BLACK parking

// Tick marks: horizontal lines at 10% steps of the atmospheric portion, on the
// LEFT of the bar. Majors span 40% of the width, minors 20%.
static const float   LNCH_AS_ATMO_TICK_W_FRAC_MAJOR = 0.40f;
static const float   LNCH_AS_ATMO_TICK_W_FRAC_MINOR = 0.20f;
static const int16_t LNCH_AS_ATMO_TICK_COUNT_MAJOR  = 10;   // 0%, 10%, ..., 90%
static const int16_t LNCH_AS_ATMO_TICK_COUNT_MINOR  = 10;   // 5%, 15%, ..., 95%

// Tick color on the bright SKY (top) zone — darker than LIGHT_GREY so the ticks
// don't wash out against the light background.
static const uint16_t LNCH_AS_ATMO_TICK_SKY_COLOR = TFT_DARK_GREY;

// ── Ascent phase change-detection state ───────────────────────────────────────────────
// Cached last-drawn values for each row. Re-draw only on change.
static int32_t _lnchAsPrevAlt        = -1 << 30;  // m
static int32_t _lnchAsPrevApA        = -1 << 30;  // m
static int32_t _lnchAsPrevTimeToAp   = -1 << 30;  // seconds
static int16_t _lnchAsPrevVSrf       = -9999;     // tenths m/s
static int16_t _lnchAsPrevVVrt       = -9999;     // tenths m/s
static int16_t _lnchAsPrevThrottle   = -1;        // percent (0-100)
static int32_t _lnchAsPrevTBurn      = -1 << 30;  // seconds
static int32_t _lnchAsPrevDVStg      = -1 << 30;  // m/s (as tenths? or int)
// Track colors too (threshold changes don't alter the numeric value but still require redraw)
static uint16_t _lnchAsPrevAltFg     = 0xFFFF;
static uint16_t _lnchAsPrevApAFg     = 0xFFFF;
static uint16_t _lnchAsPrevTimeToApFg= 0xFFFF;
static uint16_t _lnchAsPrevVSrfFg    = 0xFFFF;
static uint16_t _lnchAsPrevVVrtFg    = 0xFFFF; static uint16_t _lnchAsPrevVVrtBg = 0xFFFF;
static uint16_t _lnchAsPrevThrFg     = 0xFFFF; static uint16_t _lnchAsPrevThrBg  = 0xFFFF;
static uint16_t _lnchAsPrevTBurnFg   = 0xFFFF; static uint16_t _lnchAsPrevTBurnBg = 0xFFFF;
static uint16_t _lnchAsPrevDVStgFg   = 0xFFFF; static uint16_t _lnchAsPrevDVStgBg = 0xFFFF;

// Ladder / V.Vrt bar state.
// Auto-scaling: scale top grows as vessel climbs. When it changes, the entire
// ladder is redrawn (chrome re-runs). _lnchAsLastDrawnScaleTop tracks what
// value was last rendered, so we know when to redraw.
static int16_t _lnchAsPrevVesselPx   = -1;       // last vessel marker y-pixel
static int16_t _lnchAsPrevApAPx      = -1;       // last apoapsis marker y-pixel
static bool    _lnchAsPrevApAValid   = false;    // was apoapsis marker drawn last frame?
static int16_t _lnchAsPrevVVrtFillPx = 0;        // last V.Vrt bar fill height (signed)
static int16_t _lnchAsPrevVOrbFillPx = 0;        // last V.Orb bar fill height (unsigned, pixels from bottom)
static float   _lnchAsLastDrawnScaleTop = 0.0f;  // scale top last rendered on screen
static float   _lnchAsCurrScaleTop      = 0.0f;  // current (grow-only) scale top target (m)
static bool    _lnchAsPrevVRow3IsOrb    = false; // row 3: true = "V.Orb" label, false = "V.Srf"

// FPA dial state. Arrow tracked by its endpoint pixel so it can be erased
// cleanly on change. Sentinel -9999 = "arrow not yet drawn" (first frame).
static int16_t _lnchAsPrevFpaArrowEndX = -9999;
static int16_t _lnchAsPrevFpaArrowEndY = -9999;
static int16_t _lnchAsPrevFpaReadout   = -9999;  // last FPA value shown (integer degrees)
static int16_t _lnchAsPrevFpaTarget    = -9999;  // last target FPA marker position (integer degrees)

// Atmosphere gauge state.
// Tracks previous triangle indicator center x-coordinate for flicker-free
// incremental updates. Sentinel -1 = "not yet drawn" (first frame).
// With the four-zone design (including OFF_BLACK parking), the triangle
// position alone encodes whether we're in a no-atmosphere or atmospheric
// state — no separate NoAtm flag is needed.
static int16_t _lnchAsPrevAtmoTriY   = -1;

// ── Ascent phase helpers ──────────────────────────────────────────────────────────────
//
// Right panel uses library printDispChrome (for labels/borders in chrome) and
// printValue (for the value in updates), matching the pattern used by every other
// screen. PrintState tracking handles flicker-free redraws and correct erase of
// prior values (including when string width or background colour changes).
//
// Each row is a full-width cell in the right panel; the label is drawn on the
// left and the value is right-aligned. Labels are short ("V.Srf:", "ΔV.Stg:")
// and don't carry units since the unit ("m/s") is implicit from KSP convention.

static PrintState _lnchAsPs[8];   // PrintState tracking for each of 8 rows


// ── Altitude ladder ───────────────────────────────────────────────────────────────────
//
// AUTO-SCALED, GROW-ONLY: the scale top grows through a fixed ladder of snap
// levels as the vessel climbs (or apoapsis grows). Once a level is reached, the
// scale stays there or grows higher — never shrinks. This gives fine-grained
// visibility of low altitudes early in the ascent (e.g. gravity-turn zone at
// 1-2 km) while still showing the full orbital range later.
//
// Snap levels (km): 2, 5, 10, 20, 40, 80, max
//   where max = target orbit altitude + 30 km (body-aware ceiling).
//
// Body parameter interpretation:
//   currentBody.flyHigh   — "Fly High" biome boundary (~18 km on Kerbin), low in atmosphere
//   currentBody.lowSpace  — atmosphere top / space boundary (~70 km on Kerbin)
//   currentBody.minSafe   — minimum safe orbital altitude (~72 km on Kerbin)
//
// For the ladder: ATMO line at lowSpace, ORBIT line at max(minSafe, lowSpace + 20 km).
//
// (_lnchAsCurrScaleTop is declared earlier in the state section so _lnchAsResetState
//  can reference it.)

// Compute the ceiling — the highest scale-top value we ever grow to for this body.
static float _lnchAsScaleCeiling() {
    float targetOrbit = max(currentBody.minSafe, currentBody.lowSpace + 20000.0f);
    return targetOrbit + 30000.0f;
}

// Given a "desired minimum" scale top (m), snap UP to the next level in the
// fixed ladder. Returns the selected level in meters.
static float _lnchAsSnapScale(float desired) {
    static const float levels[] = {
        2000.0f, 5000.0f, 10000.0f, 20000.0f, 40000.0f, 80000.0f
    };
    static const uint8_t N = sizeof(levels) / sizeof(levels[0]);

    float ceiling = _lnchAsScaleCeiling();
    for (uint8_t i = 0; i < N; i++) {
        if (desired <= levels[i] && levels[i] < ceiling) return levels[i];
    }
    return ceiling;
}

// Compute the scale top we WANT right now, based on current vessel altitude
// and apoapsis. Called every frame; compared against the stored "current"
// scale top to decide if we need to redraw at a new scale.
static float _lnchAsDesiredScaleTop() {
    // Target: altitude × 2 so vessel sits roughly at mid-ladder as it climbs,
    //         apoapsis × 1.2 so it's always visible with headroom.
    float target = 0.0f;
    if (state.altitude * 2.0f > target) target = state.altitude * 2.0f;
    if (state.apoapsis > 0.0f && state.apoapsis * 1.2f > target) {
        target = state.apoapsis * 1.2f;
    }
    return _lnchAsSnapScale(target);
}

// Return the current effective scale top, growing as needed. GROW-ONLY: never
// returns a value smaller than the previous value stored in _lnchAsCurrScaleTop.
static float _lnchAsLadderScaleTop() {
    float desired = _lnchAsDesiredScaleTop();
    if (desired > _lnchAsCurrScaleTop) {
        _lnchAsCurrScaleTop = desired;
    }
    // Initial: if current is 0 (first call after reset), use at least the
    // smallest snap level so we have a sensible scale on the launch pad.
    if (_lnchAsCurrScaleTop < 2000.0f) {
        _lnchAsCurrScaleTop = _lnchAsSnapScale(2000.0f);
    }
    return _lnchAsCurrScaleTop;
}

// Map an altitude (m) to a Y pixel on the ladder. 0 -> Y_BOT, scaleTop -> Y_TOP.
// Values outside [0, scaleTop] are clamped.
static int16_t _lnchAsAltToY(float alt, float scaleTop) {
    if (scaleTop <= 0) return LNCH_AS_LADDER_Y_BOT;
    float frac = alt / scaleTop;
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    int16_t y = LNCH_AS_LADDER_Y_BOT -
                (int16_t)roundf(frac * (float)LNCH_AS_LADDER_H);
    return y;
}

// Pick a nice tick interval based on scale range.
static float _lnchAsLadderTickInterval(float scaleTop) {
    if (scaleTop <=  3000)  return 500.0f;    // every 0.5 km  (2 km scale)
    if (scaleTop <=  6000)  return 1000.0f;   // every 1 km    (5 km scale)
    if (scaleTop <= 12000)  return 2000.0f;   // every 2 km    (10 km scale)
    if (scaleTop <= 25000)  return 5000.0f;   // every 5 km    (20 km scale)
    if (scaleTop <= 50000)  return 10000.0f;  // every 10 km   (40 km scale)
    if (scaleTop <= 100000) return 20000.0f;  // every 20 km   (80 km scale)
    if (scaleTop <= 150000) return 20000.0f;  // every 20 km   (120 km scale)
    if (scaleTop <= 500000) return 50000.0f;  // every 50 km
    return 100000.0f;
}

// Draw an altitude-marker triangle pointing left at altitude y, with a small
// letter label to the right. 'filled' selects vessel (filled) vs apoapsis
// (hollow) style; 'label' is a one-letter tag ("V" or "A") drawn at Black_16
// in the same color as the triangle.
// Tip at LADDER_LINE_X+3, base at MARKER_X + MARKER_W, label just past the base.
static const int16_t LNCH_AS_MARKER_LBL_X    = 100;   // x of label letter (past base)
static const int16_t LNCH_AS_MARKER_LBL_W    = 16;    // width reserved for label letter

static void _lnchAsDrawAltMarker(KCM_TFT &tft, int16_t y, uint16_t color,
                                 bool filled, const char *label) {
    int16_t tipX  = LNCH_AS_LADDER_LINE_X + 3;
    int16_t baseX = LNCH_AS_LADDER_MARKER_X + LNCH_AS_LADDER_MARKER_W;
    int16_t halfH = 5;
    if (filled) {
        tft.fillTriangle(tipX, y,
                         baseX, y - halfH,
                         baseX, y + halfH,
                         color);
    } else {
        tft.drawLine(tipX, y, baseX, y - halfH, color);
        tft.drawLine(tipX, y, baseX, y + halfH, color);
        tft.drawLine(baseX, y - halfH, baseX, y + halfH, color);
    }
    // One-letter label to the right of the triangle
    textLeft(tft, &Roboto_Black_16,
             LNCH_AS_MARKER_LBL_X - 8, y - 9,   // x0-8 because textLeft adds TEXT_BORDER=8
             LNCH_AS_MARKER_LBL_W, 18,
             label, color, TFT_BLACK);
}

// Erase a previously-drawn marker (triangle bounding box + label area).
static void _lnchAsEraseAltMarker(KCM_TFT &tft, int16_t y) {
    int16_t tipX  = LNCH_AS_LADDER_LINE_X + 3;
    int16_t rightX = LNCH_AS_MARKER_LBL_X + LNCH_AS_MARKER_LBL_W;
    tft.fillRect(tipX, y - 9, rightX - tipX + 1, 19, TFT_BLACK);
}

// After erasing a marker, repair any reference line segments (GND/ATM/ORB)
// that were passing through the erased region. Called from marker update logic.
// markerY is the y-center of the erased bounding box (height 19, so the box
// spans y-9 to y+9).
static void _lnchAsRepairRefLines(KCM_TFT &tft, int16_t markerY) {
    int16_t tipX   = LNCH_AS_LADDER_LINE_X + 3;
    int16_t rightX = LNCH_AS_MARKER_LBL_X + LNCH_AS_MARKER_LBL_W;
    int16_t top    = markerY - 9;
    int16_t bot    = markerY + 9;

    float scaleTop = _lnchAsLadderScaleTop();

    // Check GND (solid white at y = Y_BOT). Endpoint matches chrome (REF_X2-2)
    // to avoid the stray bright pixel near the GND label.
    {
        int16_t refY = LNCH_AS_LADDER_Y_BOT;
        if (refY >= top && refY <= bot) {
            int16_t x0 = max(tipX, (int16_t)(LNCH_AS_LADDER_LINE_X + 1));
            int16_t x1 = min(rightX, (int16_t)(LNCH_AS_LADDER_REF_X2 - 2));
            if (x1 >= x0) tft.drawLine(x0, refY, x1, refY, TFT_WHITE);
        }
    }

    // Check ATM (dashed sky-blue at y = lowSpace altitude)
    if (currentBody.lowSpace > 0) {
        int16_t refY = _lnchAsAltToY(currentBody.lowSpace, scaleTop);
        if (refY >= top && refY <= bot &&
            refY < LNCH_AS_LADDER_Y_BOT - 2 && refY > LNCH_AS_LADDER_Y_TOP + 2) {
            int16_t x0 = max(tipX, (int16_t)(LNCH_AS_LADDER_LINE_X + 1));
            int16_t x1 = min(rightX, (int16_t)(LNCH_AS_LADDER_REF_X2 - 1));
            // Redraw dashes only in the overlap x-range. Walk the same 6-px
            // pattern used in chrome so dashes align.
            for (int16_t x = LNCH_AS_LADDER_LINE_X + 1; x < LNCH_AS_LADDER_REF_X2; x += 6) {
                int16_t sx = x;
                int16_t ex = x + 3;
                // Clip dash to repair region
                if (ex < x0 || sx > x1) continue;
                int16_t csx = max(sx, x0);
                int16_t cex = min(ex, x1);
                tft.drawLine(csx, refY, cex, refY, TFT_SKY);
            }
        }
    }

    // Check ORB (dashed dark-green at target orbit altitude)
    {
        float targetOrbit = max(currentBody.minSafe, currentBody.lowSpace + 20000.0f);
        int16_t refY = _lnchAsAltToY(targetOrbit, scaleTop);
        if (refY >= top && refY <= bot &&
            refY < LNCH_AS_LADDER_Y_BOT - 2 && refY > LNCH_AS_LADDER_Y_TOP + 2) {
            int16_t x0 = max(tipX, (int16_t)(LNCH_AS_LADDER_LINE_X + 1));
            int16_t x1 = min(rightX, (int16_t)(LNCH_AS_LADDER_REF_X2 - 1));
            for (int16_t x = LNCH_AS_LADDER_LINE_X + 1; x < LNCH_AS_LADDER_REF_X2; x += 6) {
                int16_t sx = x;
                int16_t ex = x + 3;
                if (ex < x0 || sx > x1) continue;
                int16_t csx = max(sx, x0);
                int16_t cex = min(ex, x1);
                tft.drawLine(csx, refY, cex, refY, TFT_DARK_GREEN);
            }
        }
    }

    // Also repair the main vertical ladder line if it crosses the erase region.
    // The line is at LINE_X; marker tipX = LINE_X + 3, so the line isn't inside
    // the erase region horizontally. Skip.
}

// Draw static chrome for the altitude ladder: main vertical line, tick marks,
// tick labels, and reference lines (ground, atmo, orbit) with their labels.
// Fixed scale — called once per chrome cycle.
static void _lnchAsDrawLadderChrome(KCM_TFT &tft) {
    float scaleTop = _lnchAsLadderScaleTop();
    float tickInt  = _lnchAsLadderTickInterval(scaleTop);

    // Main vertical line
    tft.drawLine(LNCH_AS_LADDER_LINE_X, LNCH_AS_LADDER_Y_TOP,
                 LNCH_AS_LADDER_LINE_X, LNCH_AS_LADDER_Y_BOT,
                 TFT_LIGHT_GREY);

    // Tick marks and labels. Major ticks (at each `tickInt`) get a full-length
    // mark + label. Minor ticks (4 between majors, at tickInt/5 spacing) get a
    // half-length mark with no label, providing visual subdivision.
    float minorInt = tickInt / 5.0f;
    int16_t minorTickW = LNCH_AS_LADDER_TICK_W / 2;  // 3 px
    for (float a = 0; a <= scaleTop + 0.5f * minorInt; a += minorInt) {
        int16_t y = _lnchAsAltToY(a, scaleTop);
        if (y < LNCH_AS_LADDER_Y_TOP - 1) break;
        if (y > LNCH_AS_LADDER_Y_BOT) continue;

        // Is this a major tick? (altitude is an integer multiple of tickInt)
        // Use rounding since floating-point stepping accumulates error.
        float major_mul = a / tickInt;
        bool isMajor = (fabsf(major_mul - roundf(major_mul)) < 0.05f);

        int16_t tickLen = isMajor ? LNCH_AS_LADDER_TICK_W : minorTickW;
        tft.drawLine(LNCH_AS_LADDER_LINE_X - tickLen, y,
                     LNCH_AS_LADDER_LINE_X - 1,        y,
                     TFT_LIGHT_GREY);

        if (!isMajor) continue;

        // Label at each major tick. Skip 0 — "GND" label drawn separately.
        if (a < 500.0f) continue;
        char buf[12];
        if (a < 1000.0f) {
            snprintf(buf, sizeof(buf), "%dm", (int)roundf(a));
        } else if (tickInt < 1000.0f) {
            // Sub-km tick spacing — show decimal
            snprintf(buf, sizeof(buf), "%.1fkm", a / 1000.0f);
        } else {
            snprintf(buf, sizeof(buf), "%d km", (int)roundf(a / 1000.0f));
        }
        textRight(tft, &Roboto_Black_16,
                  LNCH_AS_LADDER_LBL_X, y - 9,
                  LNCH_AS_LADDER_LBL_W, 18,
                  buf, TFT_LIGHT_GREY, TFT_BLACK);
    }

    // Reference lines — use short labels (GND/ATM/ORB) so they don't overflow
    // into the V.Vrt bar area.

    // GROUND at altitude 0 (bottom of scale) — solid white line.
    // End matches the dashed-line endpoint pattern used for ATM/ORB
    // (the loop "x += 6" from LINE_X+1 ends the last dash at x=148 for REF_X2=150),
    // avoiding a stray bright pixel at x=150 which would sit 2 px diagonally below
    // the "D" of the "GND" label.
    {
        int16_t y = LNCH_AS_LADDER_Y_BOT;
        tft.drawLine(LNCH_AS_LADDER_LINE_X + 1, y,
                     LNCH_AS_LADDER_REF_X2 - 2, y,
                     TFT_WHITE);
        // Label ABOVE the ground line (below would push off-screen past y=479)
        textLeft(tft, &Roboto_Black_16,
                 LNCH_AS_LADDER_REF_X2 - 40, y - 17,
                 44, 16,
                 "GND", TFT_LIGHT_GREY, TFT_BLACK);
    }

    // ATMO — only for atmospheric bodies (lowSpace > 0). Drawn at the actual
    // atmosphere top (lowSpace), not the fly-high biome boundary (flyHigh).
    // Color is sky blue for high contrast against the dark ladder background.
    if (currentBody.lowSpace > 0) {
        int16_t y = _lnchAsAltToY(currentBody.lowSpace, scaleTop);
        if (y < LNCH_AS_LADDER_Y_BOT - 2 && y > LNCH_AS_LADDER_Y_TOP + 2) {
            // Dashed line
            for (int16_t x = LNCH_AS_LADDER_LINE_X + 1; x < LNCH_AS_LADDER_REF_X2; x += 6) {
                tft.drawLine(x, y, x + 3, y, TFT_SKY);
            }
            // Label vertically centered on the reference line
            textLeft(tft, &Roboto_Black_16,
                     LNCH_AS_LADDER_REF_X2 - 40, y - 8,
                     44, 16,
                     "ATM", TFT_SKY, TFT_BLACK);
        }
    }

    // ORBIT — target orbit altitude (minSafe or lowSpace + 20 km, whichever higher).
    // This is also the target apoapsis altitude during ascent, labeled as "TGT /
    // ORB" stacked on two lines to emphasize the pilot is AIMING for this altitude.
    {
        float targetOrbit = max(currentBody.minSafe, currentBody.lowSpace + 20000.0f);
        int16_t y = _lnchAsAltToY(targetOrbit, scaleTop);
        if (y < LNCH_AS_LADDER_Y_BOT - 2 && y > LNCH_AS_LADDER_Y_TOP + 2) {
            for (int16_t x = LNCH_AS_LADDER_LINE_X + 1; x < LNCH_AS_LADDER_REF_X2; x += 6) {
                tft.drawLine(x, y, x + 3, y, TFT_DARK_GREEN);
            }
            // Two-line label "TGT" / "ORB" vertically centered on the ref line:
            // TGT sits just above the line, ORB just below, so the line runs in
            // the 2-px gap between the two words. Needs ~18 px clearance on each
            // side; otherwise fall back to a single-line "ORB" centered on it.
            if (y > LNCH_AS_LADDER_Y_TOP + 18 && y < LNCH_AS_LADDER_Y_BOT - 18) {
                textLeft(tft, &Roboto_Black_16,
                         LNCH_AS_LADDER_REF_X2 - 40, y - 17,
                         44, 16,
                         "TGT", TFT_DARK_GREEN, TFT_BLACK);
                textLeft(tft, &Roboto_Black_16,
                         LNCH_AS_LADDER_REF_X2 - 40, y + 1,
                         44, 16,
                         "ORB", TFT_DARK_GREEN, TFT_BLACK);
            } else {
                // Insufficient room — single-line "ORB" centered on the line
                textLeft(tft, &Roboto_Black_16,
                         LNCH_AS_LADDER_REF_X2 - 40, y - 8,
                         44, 16,
                         "ORB", TFT_DARK_GREEN, TFT_BLACK);
            }
        }
    }
}

// Dynamic update: check for scale change first (redraw entire ladder if so),
// then update vessel and apoapsis markers. On each marker erase, repair any
// reference line segments that were crossing the erased region.
static void _lnchAsUpdateLadderMarkers(KCM_TFT &tft) {
    float scaleTop = _lnchAsLadderScaleTop();

    // Scale-change detection: redraw entire ladder strip if the scale differs
    // from what was last drawn. This erases tick labels, tick marks, reference
    // lines, and markers — all of which will be re-chromed/redrawn below.
    if (scaleTop != _lnchAsLastDrawnScaleTop) {
        // Erase full ladder region (wider than any single label/mark).
        int16_t eraseTop = max((int16_t)(LNCH_AS_LADDER_Y_TOP - 12), (int16_t)LNCH_AS_PANEL_Y);
        int16_t eraseBot = min((int16_t)(LNCH_AS_LADDER_Y_BOT + 2),
                               (int16_t)(SCREEN_H - 1));
        // Erase from x=0 to LADDER_ERASE_X2, covering tick labels, ticks, line,
        // markers, and reference-line labels.
        tft.fillRect(LNCH_AS_LADDER_LBL_X, eraseTop,
                     LNCH_AS_LADDER_ERASE_X2 - LNCH_AS_LADDER_LBL_X + 1,
                     eraseBot - eraseTop + 1,
                     TFT_BLACK);

        // Redraw chrome and invalidate marker cache so they redraw below.
        _lnchAsDrawLadderChrome(tft);
        _lnchAsPrevVesselPx = -1;
        _lnchAsPrevApAPx    = -1;
        _lnchAsPrevApAValid = false;
        _lnchAsLastDrawnScaleTop = scaleTop;
    }

    // Vessel marker (always shown)
    int16_t vesselY = _lnchAsAltToY(state.altitude, scaleTop);
    if (vesselY != _lnchAsPrevVesselPx) {
        if (_lnchAsPrevVesselPx >= 0) {
            _lnchAsEraseAltMarker(tft, _lnchAsPrevVesselPx);
            _lnchAsRepairRefLines(tft, _lnchAsPrevVesselPx);
        }
        _lnchAsDrawAltMarker(tft, vesselY, TFT_DARK_GREEN, true, "V");
        _lnchAsPrevVesselPx = vesselY;

        // If ApA was on top of vessel's old position, it may have been erased — redraw.
        if (_lnchAsPrevApAValid && _lnchAsPrevApAPx >= 0 &&
            abs(_lnchAsPrevApAPx - vesselY) <= 6) {
            _lnchAsDrawAltMarker(tft, _lnchAsPrevApAPx, TFT_YELLOW, false, "A");
        }
    }

    // Apoapsis marker (only when valid — apoapsis > 0)
    bool apaValid = (state.apoapsis > 0.0f);
    if (apaValid) {
        int16_t apaY = _lnchAsAltToY(state.apoapsis, scaleTop);
        if (apaY != _lnchAsPrevApAPx || !_lnchAsPrevApAValid) {
            if (_lnchAsPrevApAValid && _lnchAsPrevApAPx >= 0) {
                _lnchAsEraseAltMarker(tft, _lnchAsPrevApAPx);
                _lnchAsRepairRefLines(tft, _lnchAsPrevApAPx);
                // If vessel was on top of ApA's old position, redraw vessel
                if (abs(_lnchAsPrevApAPx - vesselY) <= 6) {
                    _lnchAsDrawAltMarker(tft, vesselY, TFT_DARK_GREEN, true, "V");
                }
            }
            _lnchAsDrawAltMarker(tft, apaY, TFT_YELLOW, false, "A");
            _lnchAsPrevApAPx    = apaY;
            _lnchAsPrevApAValid = true;
        }
    } else {
        // Apoapsis not valid — erase any previously drawn marker
        if (_lnchAsPrevApAValid && _lnchAsPrevApAPx >= 0) {
            _lnchAsEraseAltMarker(tft, _lnchAsPrevApAPx);
            _lnchAsRepairRefLines(tft, _lnchAsPrevApAPx);
            if (abs(_lnchAsPrevApAPx - vesselY) <= 6) {
                _lnchAsDrawAltMarker(tft, vesselY, TFT_DARK_GREEN, true, "V");
            }
            _lnchAsPrevApAValid = false;
            _lnchAsPrevApAPx    = -1;
        }
    }
}

// ── V.Vrt bar ─────────────────────────────────────────────────────────────────────────
//
// Vertical bar with zero at middle. Positive V.Vrt fills upward (green),
// negative fills downward (red). Scale is fixed at ±LNCH_AS_VVRT_SCALE_MS m/s,
// saturates at the ends.
static void _lnchAsDrawVVrtChrome(KCM_TFT &tft) {
    // Label box spans LBL_OVERHANG px beyond each bar edge so the "±500 m/s"
    // endpoint labels fit around the widened bar.
    const int16_t LBL_X0 = LNCH_AS_VVRT_X_LEFT - LNCH_AS_LBL_OVERHANG;
    const int16_t LBL_W  = LNCH_AS_VVRT_W + 2 * LNCH_AS_LBL_OVERHANG;

    // Bar name "V.Vrt" (Black_20)
    textCenter(tft, &Roboto_Black_20,
               LBL_X0, LNCH_AS_GAUGE_NAME_Y,
               LBL_W, LNCH_AS_GAUGE_NAME_H,
               "V.Vrt", TFT_LIGHT_GREY, TFT_BLACK);

    // Endpoint labels (Black_16), a few px clear of the bar top/bottom borders.
    textCenter(tft, &Roboto_Black_16,
               LBL_X0, LNCH_AS_VVRT_Y_TOP - LNCH_AS_GAUGE_ENDLBL_H - 6,
               LBL_W, LNCH_AS_GAUGE_ENDLBL_H,
               "+500 m/s", TFT_LIGHT_GREY, TFT_BLACK);
    textCenter(tft, &Roboto_Black_16,
               LBL_X0, LNCH_AS_VVRT_Y_BOT + 5,
               LBL_W, LNCH_AS_GAUGE_ENDLBL_H,
               "-500 m/s", TFT_LIGHT_GREY, TFT_BLACK);

    // Border + zero line drawn LAST so a label glyph can never nibble the borders.
    tft.drawRect(LNCH_AS_VVRT_X_LEFT, LNCH_AS_VVRT_Y_TOP,
                 LNCH_AS_VVRT_W,
                 LNCH_AS_VVRT_Y_BOT - LNCH_AS_VVRT_Y_TOP + 1,
                 TFT_LIGHT_GREY);
    tft.drawLine(LNCH_AS_VVRT_X_LEFT + 1, LNCH_AS_VVRT_Y_MID,
                 LNCH_AS_VVRT_X_LEFT + LNCH_AS_VVRT_W - 2, LNCH_AS_VVRT_Y_MID,
                 TFT_LIGHT_GREY);
}

// Update V.Vrt bar fill using incremental drawing (no full-clear + redraw, so
// no flicker). Handles positive/negative fill separately. When signs change
// between frames, clears the old side and draws the new side.
//
// Positive fill: green, grows upward from the zero line (y=MID-1, MID-2, ...).
// Negative fill: red, grows downward from the zero line (y=MID+1, MID+2, ...).
// fillPx encodes both magnitude and direction as a signed pixel count.
static void _lnchAsUpdateVVrtBar(KCM_TFT &tft) {
    float vv = state.verticalVel;
    if (vv > LNCH_AS_VVRT_SCALE_MS)  vv = LNCH_AS_VVRT_SCALE_MS;
    if (vv < -LNCH_AS_VVRT_SCALE_MS) vv = -LNCH_AS_VVRT_SCALE_MS;

    int16_t halfH = LNCH_AS_VVRT_Y_MID - LNCH_AS_VVRT_Y_TOP - 1;
    int16_t fillPx = (int16_t)roundf((vv / LNCH_AS_VVRT_SCALE_MS) * (float)halfH);

    int16_t prev = _lnchAsPrevVVrtFillPx;
    if (fillPx == prev) return;

    int16_t innerX = LNCH_AS_VVRT_X_LEFT + 1;
    int16_t innerW = LNCH_AS_VVRT_W - 2;

    // Sign change: clear the opposite side entirely before drawing new fill.
    bool signChanged = (fillPx >= 0) != (prev >= 0);
    if (signChanged) {
        if (prev > 0) {
            // Was climbing, now zero or falling — clear the green area above zero
            tft.fillRect(innerX, LNCH_AS_VVRT_Y_MID - prev,
                         innerW, prev, TFT_BLACK);
        } else if (prev < 0) {
            // Was falling, now zero or climbing — clear the red area below zero
            tft.fillRect(innerX, LNCH_AS_VVRT_Y_MID + 1,
                         innerW, -prev, TFT_BLACK);
        }
        prev = 0;  // effective "old" fill on the new sign's side is zero
    }

    if (fillPx > 0) {
        if (fillPx > prev) {
            // Extend green upward — fill only the newly-covered strip
            tft.fillRect(innerX, LNCH_AS_VVRT_Y_MID - fillPx,
                         innerW, fillPx - prev, TFT_DARK_GREEN);
        } else {
            // Retract — clear the top strip that's no longer filled
            tft.fillRect(innerX, LNCH_AS_VVRT_Y_MID - prev,
                         innerW, prev - fillPx, TFT_BLACK);
        }
    } else if (fillPx < 0) {
        int16_t absNew  = -fillPx;
        int16_t absPrev = -prev;
        if (absNew > absPrev) {
            // Extend red downward
            tft.fillRect(innerX, LNCH_AS_VVRT_Y_MID + absPrev + 1,
                         innerW, absNew - absPrev, TFT_RED);
        } else {
            // Retract — clear the bottom strip
            tft.fillRect(innerX, LNCH_AS_VVRT_Y_MID + absNew + 1,
                         innerW, absPrev - absNew, TFT_BLACK);
        }
    } else {
        // fillPx == 0: clear any remaining fill on the previously-active side
        if (prev > 0) {
            tft.fillRect(innerX, LNCH_AS_VVRT_Y_MID - prev,
                         innerW, prev, TFT_BLACK);
        } else if (prev < 0) {
            tft.fillRect(innerX, LNCH_AS_VVRT_Y_MID + 1,
                         innerW, -prev, TFT_BLACK);
        }
    }

    _lnchAsPrevVVrtFillPx = fillPx;
}

// ── V.Orb bar ─────────────────────────────────────────────────────────────────────────
//
// Shows orbital velocity (state.orbitalVel) as a fill from the bottom up, with
// 100% = target circular-orbit velocity. The pilot can glance to see "how close
// am I to orbital speed" without having to know the specific target number.

// Circular orbit velocity at target altitude (m/s).
// v_circ = sqrt(μ / r) where μ = g_surf × radius² (classic two-body).
static float _lnchAsCircularOrbitVelocity() {
    if (currentBody.radius <= 0.0f || currentBody.gravity <= 0.0f) return 1.0f;
    float targetOrbit = max(currentBody.minSafe, currentBody.lowSpace + 20000.0f);
    float r    = currentBody.radius + targetOrbit;
    float g    = currentBody.gravity;            // rev-2: gravity is m/s² (was surfGrav in g)
    float mu   = g * currentBody.radius * currentBody.radius;
    return sqrtf(mu / r);
}

// V.Orb bar chrome — border + label. Fill is dynamic.
static void _lnchAsDrawVOrbChrome(KCM_TFT &tft) {
    const int16_t LBL_X0 = LNCH_AS_VORB_X_LEFT - LNCH_AS_LBL_OVERHANG;
    const int16_t LBL_W  = LNCH_AS_VORB_W + 2 * LNCH_AS_LBL_OVERHANG;

    // Bar name "V.Orb" (Black_20)
    textCenter(tft, &Roboto_Black_20,
               LBL_X0, LNCH_AS_GAUGE_NAME_Y,
               LBL_W, LNCH_AS_GAUGE_NAME_H,
               "V.Orb", TFT_LIGHT_GREY, TFT_BLACK);

    // Top endpoint label — v_circ at target orbit (body-aware), with unit.
    float vCirc = _lnchAsCircularOrbitVelocity();
    char topBuf[16];
    snprintf(topBuf, sizeof(topBuf), "%d m/s", (int)roundf(vCirc));
    textCenter(tft, &Roboto_Black_16,
               LBL_X0, LNCH_AS_VORB_Y_TOP - LNCH_AS_GAUGE_ENDLBL_H - 6,
               LBL_W, LNCH_AS_GAUGE_ENDLBL_H,
               topBuf, TFT_LIGHT_GREY, TFT_BLACK);

    // Bottom endpoint label "0 m/s"
    textCenter(tft, &Roboto_Black_16,
               LBL_X0, LNCH_AS_VORB_Y_BOT + 5,
               LBL_W, LNCH_AS_GAUGE_ENDLBL_H,
               "0 m/s", TFT_LIGHT_GREY, TFT_BLACK);

    // Border drawn LAST so a label glyph can never nibble the top/bottom borders.
    tft.drawRect(LNCH_AS_VORB_X_LEFT, LNCH_AS_VORB_Y_TOP,
                 LNCH_AS_VORB_W,
                 LNCH_AS_VORB_Y_BOT - LNCH_AS_VORB_Y_TOP + 1,
                 TFT_LIGHT_GREY);
}

// V.Orb bar update — incremental fill update (no full-clear + redraw, so no
// flicker). Fill grows upward from the bottom; we only modify the pixels that
// change between frames.
static void _lnchAsUpdateVOrbBar(KCM_TFT &tft) {
    float vo = state.orbitalVel;
    float vCirc = _lnchAsCircularOrbitVelocity();

    // Clamp to [0, v_circ] — negative orbital velocities saturate at zero,
    // overshoot saturates at full bar.
    float frac;
    if (vo <= 0.0f)         frac = 0.0f;
    else if (vo >= vCirc)   frac = 1.0f;
    else                    frac = vo / vCirc;

    int16_t innerX = LNCH_AS_VORB_X_LEFT + 1;
    int16_t innerW = LNCH_AS_VORB_W - 2;
    int16_t innerYBot = LNCH_AS_VORB_Y_BOT - 1;
    int16_t innerH = innerYBot - (LNCH_AS_VORB_Y_TOP + 1) + 1;

    int16_t fillPx = (int16_t)roundf(frac * (float)innerH);
    int16_t prev = _lnchAsPrevVOrbFillPx;

    if (fillPx == prev) return;

    if (fillPx > prev) {
        // Grow — fill only the newly-added strip at the top
        int16_t strip = fillPx - prev;
        tft.fillRect(innerX, innerYBot - fillPx + 1,
                     innerW, strip,
                     TFT_DARK_GREEN);
    } else {
        // Shrink — clear the strip that's no longer filled
        int16_t strip = prev - fillPx;
        tft.fillRect(innerX, innerYBot - prev + 1,
                     innerW, strip,
                     TFT_BLACK);
    }

    _lnchAsPrevVOrbFillPx = fillPx;
}

// ── FPA (Flight Path Angle) dial ──────────────────────────────────────────────────────
//
// Half-circle dial showing velocity vector orientation relative to the horizon.
// The horizon is a horizontal line across the middle; the arrow points from
// the center at the current FPA. Tick marks at 30° increments.
//
// FPA is computed from surface velocity components:
//     FPA = arctan2(V.Vrt, V.Hrz)  where V.Hrz = sqrt(V.Srf² - V.Vrt²)
//
// Conventions:
//     FPA =  0° → horizontal (prograde / right)
//     FPA = +90° → straight up
//     FPA = -90° → straight down
//
// Special cases:
//     On pad / stationary (|V.Srf| < 0.1) → arrow straight up (90°), no motion
//     V.Vrt > V.Srf (numerical error) → clamp to ±90°

// Compute FPA in degrees from state.
// Returns +90° when stationary (pre-launch) so the arrow defaults to "pointing up".
static float _lnchAsComputeFPA() {
    float vs = state.surfaceVel;
    float vv = state.verticalVel;

    if (vs < 0.1f) return 90.0f;   // stationary: arrow straight up

    // V.Hrz² = V.Srf² - V.Vrt²  (can go slightly negative from rounding)
    float vHrzSq = vs * vs - vv * vv;
    if (vHrzSq < 0.0f) {
        // Vertical component dominates — arrow is ±90° aligned with V.Vrt sign
        return (vv >= 0.0f) ? 90.0f : -90.0f;
    }
    float vHrz = sqrtf(vHrzSq);
    if (vHrz < 0.01f) return (vv >= 0.0f) ? 90.0f : -90.0f;
    return atan2f(vv, vHrz) / DEG_TO_RAD;  // radians → degrees
}

// Compute the target FPA for the current altitude — the "where should I be
// pointing" value for an optimal gravity turn. Starts at +90° (vertical) on
// the pad, smoothly decreases to 0° (horizontal) at target orbit altitude.
//
// Curve: target_FPA = 90 × (1 − √(alt / target_alt))
// Steeper pitch-over at low altitude, flatter near orbit. Matches the shape
// of a typical Kerbin gravity turn — most of the tilt happens in the first
// third of the ascent.
//
// Below 1500 m altitude: hold at 90° (vehicle is still clearing the pad and
// shouldn't tilt yet — matches real rocket launch profiles).
static float _lnchAsComputeTargetFPA() {
    float alt = state.altitude;
    if (alt < 1500.0f) return 90.0f;

    float targetOrbit = max(currentBody.minSafe, currentBody.lowSpace + 20000.0f);
    if (targetOrbit <= 0.0f) return 0.0f;

    float frac = alt / targetOrbit;
    if (frac >= 1.0f) return 0.0f;   // already at/above target, aim horizontal

    return 90.0f * (1.0f - sqrtf(frac));
}

// Draw a radial tick at angle fpaDeg. tickOut = px outside R, tickIn = px inside.
static void _lnchAsDrawDialTick(KCM_TFT &tft, float fpaDeg,
                                int16_t tickOut, int16_t tickIn, uint16_t color) {
    float rad = fpaDeg * DEG_TO_RAD;
    float cs = cosf(rad), sn = sinf(rad);
    int16_t x0 = LNCH_AS_FPA_CX + (int16_t)roundf((LNCH_AS_FPA_R - tickIn)  * cs);
    int16_t y0 = LNCH_AS_FPA_CY - (int16_t)roundf((LNCH_AS_FPA_R - tickIn)  * sn);
    int16_t x1 = LNCH_AS_FPA_CX + (int16_t)roundf((LNCH_AS_FPA_R + tickOut) * cs);
    int16_t y1 = LNCH_AS_FPA_CY - (int16_t)roundf((LNCH_AS_FPA_R + tickOut) * sn);
    tft.drawLine(x0, y0, x1, y1, color);
}

// Draw the semicircle arc outline (right half, -90°..+90°).
static void _lnchAsDrawDialArc(KCM_TFT &tft, uint16_t color) {
    const int16_t STEP = 2;
    int16_t prevX = LNCH_AS_FPA_CX + LNCH_AS_FPA_R, prevY = LNCH_AS_FPA_CY;
    for (int16_t deg = STEP; deg <= 90; deg += STEP) {
        float rad = deg * DEG_TO_RAD;
        int16_t x = LNCH_AS_FPA_CX + (int16_t)roundf(LNCH_AS_FPA_R * cosf(rad));
        int16_t y = LNCH_AS_FPA_CY - (int16_t)roundf(LNCH_AS_FPA_R * sinf(rad));
        tft.drawLine(prevX, prevY, x, y, color);
        prevX = x; prevY = y;
    }
    prevX = LNCH_AS_FPA_CX + LNCH_AS_FPA_R; prevY = LNCH_AS_FPA_CY;
    for (int16_t deg = -STEP; deg >= -90; deg -= STEP) {
        float rad = deg * DEG_TO_RAD;
        int16_t x = LNCH_AS_FPA_CX + (int16_t)roundf(LNCH_AS_FPA_R * cosf(rad));
        int16_t y = LNCH_AS_FPA_CY - (int16_t)roundf(LNCH_AS_FPA_R * sinf(rad));
        tft.drawLine(prevX, prevY, x, y, color);
        prevX = x; prevY = y;
    }
}

// Draw the doubled (2 px) major ticks + single minor ticks in one place, reused
// by chrome and repair.
static void _lnchAsDrawDialTicks(KCM_TFT &tft) {
    for (int16_t deg = -90; deg <= 90; deg += 30) {
        if (deg == 0) continue;   // 0° is the horizon line
        _lnchAsDrawDialTick(tft, (float)deg, LNCH_AS_FPA_MAJ_OUT, LNCH_AS_FPA_MAJ_IN, TFT_LIGHT_GREY);
        float rad = (float)deg * DEG_TO_RAD, cs = cosf(rad), sn = sinf(rad);
        int16_t dx = (int16_t)roundf(sn), dy = (int16_t)roundf(cs);
        int16_t x0 = LNCH_AS_FPA_CX + (int16_t)roundf((LNCH_AS_FPA_R - LNCH_AS_FPA_MAJ_IN) * cs) + dx;
        int16_t y0 = LNCH_AS_FPA_CY - (int16_t)roundf((LNCH_AS_FPA_R - LNCH_AS_FPA_MAJ_IN) * sn) + dy;
        int16_t x1 = LNCH_AS_FPA_CX + (int16_t)roundf((LNCH_AS_FPA_R + LNCH_AS_FPA_MAJ_OUT) * cs) + dx;
        int16_t y1 = LNCH_AS_FPA_CY - (int16_t)roundf((LNCH_AS_FPA_R + LNCH_AS_FPA_MAJ_OUT) * sn) + dy;
        tft.drawLine(x0, y0, x1, y1, TFT_LIGHT_GREY);
    }
    static const int8_t minorDegs[] = {-75, -45, -15, 15, 45, 75};
    for (uint8_t i = 0; i < sizeof(minorDegs); i++)
        _lnchAsDrawDialTick(tft, (float)minorDegs[i], LNCH_AS_FPA_MIN_OUT, LNCH_AS_FPA_MIN_IN, TFT_LIGHT_GREY);
}

// Dial chrome: arc, horizon, ticks, pivot, "FPA" name, angle labels.
static void _lnchAsDrawDialChrome(KCM_TFT &tft) {
    _lnchAsDrawDialArc(tft, TFT_LIGHT_GREY);
    tft.drawLine(LNCH_AS_FPA_CX, LNCH_AS_FPA_CY,
                 LNCH_AS_FPA_CX + LNCH_AS_FPA_R, LNCH_AS_FPA_CY, TFT_LIGHT_GREY);   // horizon
    _lnchAsDrawDialTicks(tft);
    tft.fillCircle(LNCH_AS_FPA_CX, LNCH_AS_FPA_CY, LNCH_AS_FPA_PIVOT_R, TFT_DARK_GREEN);

    // "FPA" name on the shared name row, centered over the dial span.
    textCenter(tft, &Roboto_Black_20,
               LNCH_AS_FPA_CX - LNCH_AS_FPA_LBL_MARGIN, LNCH_AS_GAUGE_NAME_Y,
               LNCH_AS_FPA_R + LNCH_AS_FPA_LBL_MARGIN + 20, LNCH_AS_GAUGE_NAME_H,
               "FPA", TFT_LIGHT_GREY, TFT_BLACK);

    // Angle labels (Black_20), placed clear of the tick marks: +90 above the top
    // tick, -90 below the bottom tick, 0 to the right of the arc edge.
    textCenter(tft, &Roboto_Black_20,
               LNCH_AS_FPA_CX - 32, LNCH_AS_FPA_CY - LNCH_AS_FPA_R - LNCH_AS_FPA_MAJ_OUT - 28,
               64, 24, "+90\xB0", TFT_LIGHT_GREY, TFT_BLACK);
    textCenter(tft, &Roboto_Black_20,
               LNCH_AS_FPA_CX - 32, LNCH_AS_FPA_CY + LNCH_AS_FPA_R + LNCH_AS_FPA_MAJ_OUT + 4,
               64, 24, "-90\xB0", TFT_LIGHT_GREY, TFT_BLACK);
    textLeft(tft, &Roboto_Black_20,
             LNCH_AS_FPA_CX + LNCH_AS_FPA_R + LNCH_AS_FPA_MAJ_OUT + 4, LNCH_AS_FPA_CY - 12,
             30, 24, "0\xB0", TFT_LIGHT_GREY, TFT_BLACK);
}

// Repair chrome the needle erase may have crossed (horizon + ticks + pivot).
static void _lnchAsRepairDialChrome(KCM_TFT &tft) {
    tft.drawLine(LNCH_AS_FPA_CX, LNCH_AS_FPA_CY,
                 LNCH_AS_FPA_CX + LNCH_AS_FPA_R, LNCH_AS_FPA_CY, TFT_LIGHT_GREY);
    _lnchAsDrawDialTicks(tft);
    tft.fillCircle(LNCH_AS_FPA_CX, LNCH_AS_FPA_CY, LNCH_AS_FPA_PIVOT_R, TFT_DARK_GREEN);
}

// Draw (or erase) the needle at angle fpaDeg: rotated-rectangle shaft + triangle head.
static void _lnchAsDrawDialArrow(KCM_TFT &tft, float fpaDeg, uint16_t color) {
    float rad = fpaDeg * DEG_TO_RAD;
    float cs = cosf(rad), sn = sinf(rad);
    float alongX = cs, alongY = -sn, perpX = sn, perpY = cs;
    float CX = (float)LNCH_AS_FPA_CX, CY = (float)LNCH_AS_FPA_CY;
    float hw = (float)LNCH_AS_FPA_SHAFT_W / 2.0f;
    float L  = (float)LNCH_AS_FPA_SHAFT_LEN;
    int16_t blX = (int16_t)roundf(CX + perpX * hw),        blY = (int16_t)roundf(CY + perpY * hw);
    int16_t brX = (int16_t)roundf(CX - perpX * hw),        brY = (int16_t)roundf(CY - perpY * hw);
    int16_t tlX = (int16_t)roundf(CX + alongX * L + perpX * hw), tlY = (int16_t)roundf(CY + alongY * L + perpY * hw);
    int16_t trX = (int16_t)roundf(CX + alongX * L - perpX * hw), trY = (int16_t)roundf(CY + alongY * L - perpY * hw);
    tft.fillTriangle(blX, blY, brX, brY, trX, trY, color);
    tft.fillTriangle(blX, blY, trX, trY, tlX, tlY, color);
    float R = (float)LNCH_AS_FPA_ARROW_R, hhw = (float)LNCH_AS_FPA_HEAD_W / 2.0f;
    int16_t tipX = (int16_t)roundf(CX + alongX * R),           tipY = (int16_t)roundf(CY + alongY * R);
    int16_t hblX = (int16_t)roundf(CX + alongX * L + perpX * hhw), hblY = (int16_t)roundf(CY + alongY * L + perpY * hhw);
    int16_t hbrX = (int16_t)roundf(CX + alongX * L - perpX * hhw), hbrY = (int16_t)roundf(CY + alongY * L - perpY * hhw);
    tft.fillTriangle(hblX, hblY, hbrX, hbrY, tipX, tipY, color);
}

// Draw (or erase) the target-FPA marker: small triangle just outside the arc,
// pointing inward.
static void _lnchAsDrawDialTargetMarker(KCM_TFT &tft, float fpaDeg, uint16_t color) {
    float rad = fpaDeg * DEG_TO_RAD;
    float cs = cosf(rad), sn = sinf(rad);
    float alongX = cs, alongY = -sn, perpX = sn, perpY = cs;
    const float OUT = 11.0f, DEPTH = 11.0f, HALF = 7.0f;   // enlarged target marker
    float CX = (float)LNCH_AS_FPA_CX, CY = (float)LNCH_AS_FPA_CY;
    float rTip = (float)LNCH_AS_FPA_R + OUT, rBase = rTip + DEPTH;
    int16_t tipX = (int16_t)roundf(CX + alongX * rTip),  tipY = (int16_t)roundf(CY + alongY * rTip);
    int16_t blX  = (int16_t)roundf(CX + alongX * rBase + perpX * HALF), blY = (int16_t)roundf(CY + alongY * rBase + perpY * HALF);
    int16_t brX  = (int16_t)roundf(CX + alongX * rBase - perpX * HALF), brY = (int16_t)roundf(CY + alongY * rBase - perpY * HALF);
    tft.fillTriangle(tipX, tipY, blX, blY, brX, brY, color);
}

// Update: needle, target marker, numeric readout (below the dial).
static void _lnchAsUpdateFpaDial(KCM_TFT &tft) {
    float fpa = _lnchAsComputeFPA();
    if (fpa >  90.0f) fpa =  90.0f;
    if (fpa < -90.0f) fpa = -90.0f;
    int16_t iFpa    = (int16_t)roundf(fpa);
    int16_t iTarget = (int16_t)roundf(_lnchAsComputeTargetFPA());
    bool arrowChanged  = (iFpa != _lnchAsPrevFpaReadout);
    bool targetChanged = (iTarget != _lnchAsPrevFpaTarget);
    if (!arrowChanged && !targetChanged) return;

    if (targetChanged && _lnchAsPrevFpaTarget != -9999)
        _lnchAsDrawDialTargetMarker(tft, (float)_lnchAsPrevFpaTarget, TFT_BLACK);
    if (arrowChanged && _lnchAsPrevFpaReadout != -9999) {
        _lnchAsDrawDialArrow(tft, (float)_lnchAsPrevFpaReadout, TFT_BLACK);
        _lnchAsRepairDialChrome(tft);
    }
    if (arrowChanged) {
        _lnchAsDrawDialArrow(tft, (float)iFpa, TFT_DARK_GREEN);
        tft.fillCircle(LNCH_AS_FPA_CX, LNCH_AS_FPA_CY, LNCH_AS_FPA_PIVOT_R, TFT_DARK_GREEN);
    }
    if (targetChanged || arrowChanged)
        _lnchAsDrawDialTargetMarker(tft, (float)iTarget, TFT_YELLOW);

    // Numeric readout centered under the dial.
    if (arrowChanged) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%+d\xB0", iFpa);
        int16_t rx0 = LNCH_AS_FPA_CX + LNCH_AS_FPA_R / 2 - 48;
        int16_t ry  = LNCH_AS_FPA_CY + LNCH_AS_FPA_R + 42;   // below the -90 label
        tft.fillRect(rx0, ry, 96, 32, TFT_BLACK);
        textCenter(tft, &Roboto_Black_28, rx0, ry, 96, 32, buf, TFT_DARK_GREEN, TFT_BLACK);
    }
    _lnchAsPrevFpaReadout = iFpa;
    _lnchAsPrevFpaTarget  = iTarget;
}

// ── Atmosphere gauge ──────────────────────────────────────────────────────────────────
//
// Vertical bar showing current atmospheric density as a fraction of the body's
// sea-level density (0..1). Sea level (dense) at the top, vacuum at the bottom;
// a white indicator slides down as the vessel climbs out of the atmosphere. On
// bodies with no atmosphere the indicator parks in the OFF_BLACK bottom segment.
//
// Data inputs from AppState:
//   state.airDensity   — current air density in kg/m³ (0 in vacuum)
//   state.inAtmo       — true if vessel is within body atmosphere
//   currentBody        — name-keyed lookup for sea-level reference density

// Body sea-level density lookup (kg/m³). Only atmosphere bodies listed; bodies
// not in the table are treated as non-atmospheric (gauge shows "NO ATM").
//
// Values are KSP stock sea-level densities per the Kerbal Space Program wiki
// entries. Simpit's atmoConditionsMessage doesn't expose sea-level reference
// density directly, so we maintain this per-body lookup. Atmosphere-gauge
// depth is then computed as (currentDensity / sealevelDensity)^0.25 to match
// KSP's in-game gauge.
static float _lnchAsBodySurfaceDensity() {
    // Early bail for bodies the body table explicitly flags as non-atmosphere.
    if (currentBody.cond && strcmp(currentBody.cond, "Vacuum") == 0) return 0.0f;

    const char *name = currentBody.soiName;
    if (!name || name[0] == '\0') return 0.0f;

    // Case-sensitive exact match against SOI name reported by Simpit.
    if      (strcmp(name, "Kerbin") == 0) return 1.225f;
    else if (strcmp(name, "Eve")    == 0) return 6.150f;
    else if (strcmp(name, "Duna")   == 0) return 0.0678f;
    else if (strcmp(name, "Jool")   == 0) return 14.00f;
    else if (strcmp(name, "Laythe") == 0) return 1.700f;

    // Non-atmosphere bodies (Mun, Minmus, Ike, Gilly, Dres, Moho, Eeloo,
    // Bop, Pol, Vall, Tylo) → return 0 to signal "no atmosphere".
    return 0.0f;
}

// Compute atmosphere fill fraction (0..1). Returns <0 to signal "no atmosphere"
// on this body, which the caller uses to draw the "NO ATM" overlay.
//
// Formula matches KSP's stock atmosphere gauge: depth = (ρ / ρ_SL)^0.25
// This is a 4th-root relationship (not linear) which makes the gauge emphasize
// the high end — e.g. on Kerbin ~90% at 1.8 km, ~50% at 11 km, 10% at ~55 km.
// Source: KerbalSimpit forum discussion (wile1411, Aug 2019) confirmed against
// KSP's in-game gauge.
static float _lnchAsAtmoFraction() {
    float maxD = _lnchAsBodySurfaceDensity();
    if (maxD <= 0.0f) return -1.0f;   // no atmosphere
    float ratio = state.airDensity / maxD;
    if (ratio <= 0.0f) return 0.0f;
    if (ratio >= 1.0f) return 1.0f;
    // depth = ratio^0.25 = sqrt(sqrt(ratio))  — two sqrtfs are cheaper than powf
    return sqrtf(sqrtf(ratio));
}

// Vertical atmospheric-scale geometry: atmYBot = the y at frac 0 (bottom of the
// atmospheric portion, just above the OFF_BLACK parking segment); atmH = scale
// height in px. Sea level (frac 1) is at the top interior row.
static void _lnchAsAtmoScaleGeom(int16_t &atmYBot, int16_t &atmH) {
    int16_t innerY0 = LNCH_AS_ATMO_Y_TOP + 1;
    int16_t innerY1 = LNCH_AS_ATMO_Y_BOT - 1;
    int16_t innerH  = innerY1 - innerY0 + 1;
    int16_t noAtmH  = (int16_t)roundf(LNCH_AS_ATMO_NOATM_H_FRAC * innerH);
    atmYBot = innerY1 - noAtmH;
    atmH    = atmYBot - innerY0 + 1;
}

// Map an atmospheric fraction (0..1) to a y-pixel on the vertical scale
// (0 = bottom / vacuum end, 1 = top / sea level).
static int16_t _lnchAsAtmoFracToY(float frac) {
    int16_t atmYBot, atmH;
    _lnchAsAtmoScaleGeom(atmYBot, atmH);
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    return atmYBot - (int16_t)roundf(frac * (float)(atmH - 1));
}

// Draw the stable vertical zone fill (SKY top → NAVY, OFF_BLACK parking bottom)
// plus horizontal tick marks along the left of the bar.
static void _lnchAsDrawAtmoBackground(KCM_TFT &tft) {
    int16_t innerX0 = LNCH_AS_ATMO_X_LEFT + 1;
    int16_t innerY0 = LNCH_AS_ATMO_Y_TOP + 1;
    int16_t innerY1 = LNCH_AS_ATMO_Y_BOT - 1;
    int16_t innerW  = LNCH_AS_ATMO_W - 1;
    int16_t atmYBot, atmH;
    _lnchAsAtmoScaleGeom(atmYBot, atmH);

    int16_t y75 = _lnchAsAtmoFracToY(LNCH_AS_ATMO_ZONE2_FRAC);  // SKY ↔ FRENCH_BLUE
    int16_t y35 = _lnchAsAtmoFracToY(LNCH_AS_ATMO_ZONE1_FRAC);  // FRENCH_BLUE ↔ NAVY

    tft.fillRect(innerX0, innerY0, innerW, y75 - innerY0,        TFT_SKY);          // dense (top)
    tft.fillRect(innerX0, y75,     innerW, y35 - y75,            TFT_FRENCH_BLUE);  // medium
    tft.fillRect(innerX0, y35,     innerW, atmYBot - y35 + 1,    TFT_NAVY);         // near vacuum
    if (atmYBot + 1 <= innerY1)
        tft.fillRect(innerX0, atmYBot + 1, innerW, innerY1 - atmYBot, TFT_OFF_BLACK);  // parking

    int16_t tickWMajor = (int16_t)roundf(LNCH_AS_ATMO_TICK_W_FRAC_MAJOR * innerW);
    int16_t tickWMinor = (int16_t)roundf(LNCH_AS_ATMO_TICK_W_FRAC_MINOR * innerW);
    if (tickWMajor < 1) tickWMajor = 1;
    if (tickWMinor < 1) tickWMinor = 1;
    // Ticks in the bright SKY zone (above y75) use a darker color so they don't
    // wash out; NAVY/FRENCH_BLUE zones keep light-grey ticks.
    for (int16_t i = 0; i < LNCH_AS_ATMO_TICK_COUNT_MAJOR; i++) {
        int16_t ty = _lnchAsAtmoFracToY((float)i / (float)LNCH_AS_ATMO_TICK_COUNT_MAJOR);
        uint16_t tc = (ty < y75) ? LNCH_AS_ATMO_TICK_SKY_COLOR : TFT_LIGHT_GREY;
        tft.drawLine(innerX0, ty, innerX0 + tickWMajor - 1, ty, tc);
    }
    for (int16_t i = 0; i < LNCH_AS_ATMO_TICK_COUNT_MINOR; i++) {
        int16_t ty = _lnchAsAtmoFracToY(((float)i + 0.5f) / (float)LNCH_AS_ATMO_TICK_COUNT_MINOR);
        uint16_t tc = (ty < y75) ? LNCH_AS_ATMO_TICK_SKY_COLOR : TFT_LIGHT_GREY;
        tft.drawLine(innerX0, ty, innerX0 + tickWMinor - 1, ty, tc);
    }
}

// Chrome: name label + stable zone fill + border.
static void _lnchAsDrawAtmoChrome(KCM_TFT &tft) {
    int16_t cx = LNCH_AS_ATMO_X_LEFT + LNCH_AS_ATMO_W / 2;
    textCenter(tft, &Roboto_Black_20,
               cx - 50, LNCH_AS_GAUGE_NAME_Y, 100, LNCH_AS_GAUGE_NAME_H,
               "ATMO", TFT_LIGHT_GREY, TFT_BLACK);
    _lnchAsDrawAtmoBackground(tft);
    tft.drawRect(LNCH_AS_ATMO_X_LEFT, LNCH_AS_ATMO_Y_TOP,
                 LNCH_AS_ATMO_W + 1,
                 LNCH_AS_ATMO_Y_BOT - LNCH_AS_ATMO_Y_TOP + 1,
                 TFT_LIGHT_GREY);
}

// Twin triangle indicators: one each side of the bar, pointing toward it, at
// centerY. Pass TFT_BLACK to erase (both live on the black background outside the
// bar, so no zone/tick reconstruction is needed).
static void _lnchAsDrawAtmoTriangle(KCM_TFT &tft, int16_t centerY, uint16_t color) {
    // Left triangle — points right, tip just left of the bar.
    int16_t lTipX  = LNCH_AS_ATMO_X_LEFT - LNCH_AS_ATMO_TRI_GAP;
    int16_t lBaseX = lTipX - LNCH_AS_ATMO_TRI_W;
    tft.fillTriangle(lTipX,  centerY,
                     lBaseX, centerY - LNCH_AS_ATMO_TRI_HALF_H,
                     lBaseX, centerY + LNCH_AS_ATMO_TRI_HALF_H,
                     color);
    // Right triangle — points left, tip just right of the bar.
    int16_t rTipX  = LNCH_AS_ATMO_X_RIGHT + LNCH_AS_ATMO_TRI_GAP;
    int16_t rBaseX = rTipX + LNCH_AS_ATMO_TRI_W;
    tft.fillTriangle(rTipX,  centerY,
                     rBaseX, centerY - LNCH_AS_ATMO_TRI_HALF_H,
                     rBaseX, centerY + LNCH_AS_ATMO_TRI_HALF_H,
                     color);
}

// Update: slide the indicator to the current density fraction (or park it in the
// OFF_BLACK segment on a non-atmosphere body). The bar background is stable
// chrome; only the triangle moves.
static void _lnchAsUpdateAtmoGauge(KCM_TFT &tft) {
    float frac = _lnchAsAtmoFraction();
    bool  noAtm = (frac < 0.0f);

    int16_t innerY1 = LNCH_AS_ATMO_Y_BOT - 1;
    int16_t atmYBot, atmH;
    _lnchAsAtmoScaleGeom(atmYBot, atmH);

    int16_t triY = noAtm ? (int16_t)((atmYBot + 1 + innerY1) / 2)
                         : _lnchAsAtmoFracToY(frac);
    if (triY == _lnchAsPrevAtmoTriY) return;

    if (_lnchAsPrevAtmoTriY >= 0) {
        _lnchAsDrawAtmoTriangle(tft, _lnchAsPrevAtmoTriY, TFT_BLACK);
    }
    _lnchAsDrawAtmoTriangle(tft, triY, TFT_WHITE);
    _lnchAsPrevAtmoTriY = triY;
}

// Left panel chrome — draw ladder (dynamic, dependent on current state),
// V.Vrt / V.Orb bars, FPA dial, and atmosphere gauge (static chrome).
static void _lnchAsDrawLeftPanelChrome(KCM_TFT &tft) {
    _lnchAsDrawLadderChrome(tft);
    _lnchAsDrawVVrtChrome(tft);
    _lnchAsDrawVOrbChrome(tft);
    _lnchAsDrawDialChrome(tft);
    _lnchAsDrawAtmoChrome(tft);
    // Record scale that was just drawn, so UpdateLadderMarkers doesn't trigger
    // an immediate spurious redraw on the first frame after chrome.
    _lnchAsLastDrawnScaleTop = _lnchAsLadderScaleTop();
}

// Called every frame: update the ladder markers, bars, dial, and atmo gauge.
static void _lnchAsDrawLeftPanelValues(KCM_TFT &tft) {
    _lnchAsUpdateLadderMarkers(tft);
    _lnchAsUpdateVVrtBar(tft);
    _lnchAsUpdateVOrbBar(tft);
    _lnchAsUpdateFpaDial(tft);
    _lnchAsUpdateAtmoGauge(tft);
}


// Format a velocity in m/s with 1 decimal place and "m/s" unit, no thousands
// separator. At Roboto_Black_24, worst case "-3234.5 m/s" = 135 px, well under
// the ~140 px value-region limit for the widest label ("ΔV.Stg:").
static String _lnchAsFmtMs1(float v) {
    if (fabsf(v) < 0.05f) v = 0.0f;  // snap sub-rounding noise and -0.0 to zero
    char buf[16];
    dtostrf(v, 1, 1, buf);
    return String(buf) + " m/s";
}

// Reset all ascent-phase change-detection state + PrintState. Called at chrome
// time to force a full redraw.
static void _lnchAsResetState() {
    _lnchAsPrevAlt        = -1 << 30;
    _lnchAsPrevApA        = -1 << 30;
    _lnchAsPrevTimeToAp   = -1 << 30;
    _lnchAsPrevVSrf       = -9999;
    _lnchAsPrevVVrt       = -9999;
    _lnchAsPrevThrottle   = -1;
    _lnchAsPrevTBurn      = -1 << 30;
    _lnchAsPrevDVStg      = -1 << 30;
    _lnchAsPrevAltFg      = 0xFFFF;
    _lnchAsPrevApAFg      = 0xFFFF;
    _lnchAsPrevTimeToApFg = 0xFFFF;
    _lnchAsPrevVSrfFg     = 0xFFFF;
    _lnchAsPrevVVrtFg     = 0xFFFF; _lnchAsPrevVVrtBg = 0xFFFF;
    _lnchAsPrevThrFg      = 0xFFFF; _lnchAsPrevThrBg  = 0xFFFF;
    _lnchAsPrevTBurnFg    = 0xFFFF; _lnchAsPrevTBurnBg = 0xFFFF;
    _lnchAsPrevDVStgFg    = 0xFFFF; _lnchAsPrevDVStgBg = 0xFFFF;
    for (uint8_t i = 0; i < 8; i++) {
        _lnchAsPs[i].prevWidth  = 0;
        _lnchAsPs[i].prevBg     = 0x0001;  // sentinel
        _lnchAsPs[i].prevHeight = 0;
    }
    // Also invalidate the global printState[screen_LNCH][*] slots — these are
    // shared with the orbital phase's drawValue() calls. Stale state from a
    // prior orbital-mode chrome (which uses Roboto_Black_40) can otherwise
    // cause the first ascent-mode draw to paint an incorrect-height region.
    for (uint8_t i = 0; i < ROW_COUNT; i++) {
        printState[screen_LNCH][i].prevWidth  = 0;
        printState[screen_LNCH][i].prevBg     = 0x0001;
        printState[screen_LNCH][i].prevHeight = 0;
    }
    _lnchAsPrevVesselPx       = -1;
    _lnchAsPrevApAPx          = -1;
    _lnchAsPrevApAValid       = false;
    _lnchAsPrevVVrtFillPx     = 0;
    _lnchAsPrevVOrbFillPx     = 0;
    _lnchAsCurrScaleTop       = 0.0f;  // forces grow-from-zero on next query
    _lnchAsLastDrawnScaleTop  = 0.0f;
    _lnchAsPrevVRow3IsOrb     = false; // chrome draws V.Srf initially — match that
    _lnchAsPrevFpaArrowEndX   = -9999;
    _lnchAsPrevFpaArrowEndY   = -9999;
    _lnchAsPrevFpaReadout     = -9999;
    _lnchAsPrevFpaTarget      = -9999;
    _lnchAsPrevAtmoTriY       = -1;
}


// Row labels. The unit ("m/s") is included in labels for velocity rows to make
// the row purpose unambiguous. Other units follow KSP convention (formatAlt for
// altitudes, formatTime for time-based values, formatPerc for throttle).
// Row 3 ("V.Srf" / "V.Orb") is label-swapped by the update function based on
// altitude — at high altitude the value switches from surface velocity to
// orbital velocity.
static const char *_lnchAsLabels[8] = {
    "Alt.SL:", "ApA:", "T+Ap:", "V.Srf:",
    "V.Vrt:", "Thrtl:", "T.Brn:", "\xCE\x94V.Stg:"
};

// Draw static chrome for the ascent-phase right panel: a border separating
// Draw static chrome for the ascent-phase right panel. One printDispChrome
// call per row draws the label + clears the cell interior. Label font is
// Roboto_Black_20 (matches SCFT right-panel standard); value font in the
// update functions is Roboto_Black_24. 52 px row height holds the 24-tall
// value comfortably.
//
// The vertical divider between panels sits at x=RPANEL_X-2..-1 (in the 2-px
// gap between the left graphics area and the right text area), matching the
// SCFT screen layout. Because it's outside the RPANEL_X..RPANEL_X+W-1 rect
// painted by printDispChrome's row borders, it isn't overwritten and can be
// drawn in any order.
// ── rev-2 Ascent right-panel geometry ──────────────────────────────────────
// Phase 2 redesign: the numeric readout column is right-aligned to the content
// edge (flush against the sidebar), widened to 400 px, and stretched to fill the
// full height below the title bar, with larger fonts than the letterboxed
// original. Ascent-only — the Circularization right panel keeps the shared
// LNCH_AS_* geometry until its own redesign pass.
static const int16_t LNCH_AS2_RPANEL_W = 360;
static const int16_t LNCH_AS2_RPANEL_X = SCREEN_W - SIDEBAR_W - LNCH_AS2_RPANEL_W;  // 580
static const int16_t LNCH_AS2_RPANEL_Y = LNCH_AS_PANEL_Y;                            // 63
static const int16_t LNCH_AS2_ROW_H    = (SCREEN_H - LNCH_AS_PANEL_Y) / 8;           // 67
static const int16_t LNCH_AS2_RPANEL_H = LNCH_AS2_ROW_H * 8;                         // 536
static const tFont  *LNCH_AS2_LBL_FONT = &Roboto_Black_28;
static const tFont  *LNCH_AS2_VAL_FONT = &Roboto_Black_36;
static inline int16_t _lnchAs2RowY(uint8_t row) {
    return LNCH_AS2_RPANEL_Y + row * LNCH_AS2_ROW_H;
}

static void _lnchAsDrawRightPanelChrome(KCM_TFT &tft) {
    // Vertical divider in the 2-px gap before the right panel
    tft.drawLine(LNCH_AS2_RPANEL_X - 2, LNCH_AS2_RPANEL_Y,
                 LNCH_AS2_RPANEL_X - 2, LNCH_AS2_RPANEL_Y + LNCH_AS2_RPANEL_H - 1,
                 TFT_GREY);
    tft.drawLine(LNCH_AS2_RPANEL_X - 1, LNCH_AS2_RPANEL_Y,
                 LNCH_AS2_RPANEL_X - 1, LNCH_AS2_RPANEL_Y + LNCH_AS2_RPANEL_H - 1,
                 TFT_GREY);

    for (uint8_t i = 0; i < 8; i++) {
        printDispChrome(tft, LNCH_AS2_LBL_FONT,
                        LNCH_AS2_RPANEL_X, _lnchAs2RowY(i),
                        LNCH_AS2_RPANEL_W, LNCH_AS2_ROW_H,
                        _lnchAsLabels[i], COL_LABEL, TFT_BLACK, COL_NO_BDR);
    }

    // Horizontal group dividers — 2 px in TFT_GREY. Three logical groups:
    //   rows 0-2: Alt.SL, ApA, T+Ap       (altitude trajectory)
    //   rows 3-4: V.Srf/V.Orb, V.Vrt      (velocity)
    //   rows 5-7: Thrtl, T.Brn, ΔV.Stg    (propulsion)
    // Dividers sit in the 2-px gap between row groups (at y=dy-1 and y=dy,
    // where dy = rowY(3) or rowY(5)). These rows sit OUTSIDE both adjacent
    // rows' fillRect clear regions (printValue / printDispChrome clear
    // y0+1..y0+h-2 inclusive), so the divider can't be nibbled when a
    // value cell changes background colour (e.g. alarm state toggle).
    static const uint8_t divRows[] = { 3, 5 };
    for (uint8_t i = 0; i < sizeof(divRows); i++) {
        int16_t dy = _lnchAs2RowY(divRows[i]);
        tft.drawLine(LNCH_AS2_RPANEL_X, dy - 1,
                     LNCH_AS2_RPANEL_X + LNCH_AS2_RPANEL_W - 1, dy - 1,
                     TFT_GREY);
        tft.drawLine(LNCH_AS2_RPANEL_X, dy,
                     LNCH_AS2_RPANEL_X + LNCH_AS2_RPANEL_W - 1, dy,
                     TFT_GREY);
    }
}

// Helper: draw a value in a row using library printValue. The value is
// right-aligned in the cell; the label (already drawn by chrome) is used
// only for paramW calculation so the value region doesn't overlap the label.
static void _lnchAsDrawRowValue(KCM_TFT &tft, uint8_t row, const String &val,
                                 uint16_t fg, uint16_t bg) {
    printValue(tft, LNCH_AS2_VAL_FONT,
               LNCH_AS2_RPANEL_X, _lnchAs2RowY(row),
               LNCH_AS2_RPANEL_W, LNCH_AS2_ROW_H,
               _lnchAsLabels[row], val,
               fg, bg, TFT_BLACK,
               _lnchAsPs[row]);
}

// Update each row. Each checks its own change detection and returns early if
// unchanged. Order: Alt, ApA, T+Ap, V.Srf, V.Vrt, Throttle, T.Burn, ΔV.Stg.

static void _lnchAsUpdateAlt(KCM_TFT &tft) {
    float alt = state.altitude;
    int32_t iAlt = (int32_t)roundf(alt);

    // Threshold: red if negative, yellow near ground, green otherwise
    float bodyRad = (currentBody.radius > 0.0f) ? currentBody.radius : 600000.0f;
    float altYellow = bodyRad * 0.0015f;
    uint16_t fg = (alt < 0)         ? TFT_RED
                : (alt < altYellow)  ? TFT_YELLOW
                : TFT_DARK_GREEN;

    if (iAlt == _lnchAsPrevAlt && fg == _lnchAsPrevAltFg) return;

    _lnchAsDrawRowValue(tft, 0, formatAlt((float)iAlt), fg, TFT_BLACK);
    _lnchAsPrevAlt = iAlt;
    _lnchAsPrevAltFg = fg;
}

static void _lnchAsUpdateApA(KCM_TFT &tft) {
    float apa = state.apoapsis;
    int32_t iApA = (int32_t)roundf(apa);

    float warnAlt = max(currentBody.minSafe, currentBody.lowSpace);
    uint16_t fg;
    if      (apa < 0)                                       fg = TFT_RED;
    else if (warnAlt > 0 && apa > 0 && apa < warnAlt)       fg = TFT_YELLOW;
    else                                                    fg = TFT_DARK_GREEN;

    if (iApA == _lnchAsPrevApA && fg == _lnchAsPrevApAFg) return;

    _lnchAsDrawRowValue(tft, 1, formatAlt((float)iApA), fg, TFT_BLACK);
    _lnchAsPrevApA = iApA;
    _lnchAsPrevApAFg = fg;
}

static void _lnchAsUpdateTimeToAp(KCM_TFT &tft) {
    float ttAp = state.timeToAp;
    int32_t iTtAp = (int32_t)roundf(ttAp);

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

    if (iTtAp == _lnchAsPrevTimeToAp && fg == _lnchAsPrevTimeToApFg) return;

    _lnchAsDrawRowValue(tft, 2, val, fg, TFT_BLACK);
    _lnchAsPrevTimeToAp = iTtAp;
    _lnchAsPrevTimeToApFg = fg;
}

// Threshold for switching row 3 label from "V.Srf" to "V.Orb". Based on body
// radius (same formula as the auto-switch to circularization mode, except the
// label swap happens slightly earlier so the pilot sees orbital velocity as
// soon as it becomes the more meaningful number).
static bool _lnchAsShowOrbitalVelocity() {
    float bodyRad  = (currentBody.radius > 0.0f) ? currentBody.radius : 600000.0f;
    // Same as the circularization phase-switch threshold with hysteresis so
    // the label doesn't flicker near the boundary.
    bool ascending = (state.verticalVel >= 0.0f);
    float switchAlt = ascending ? (bodyRad * 0.06f) : (bodyRad * 0.055f);
    return (state.altitude > switchAlt);
}

// Row 3 dynamic label redraw. Called when the label needs to change from
// "V.Srf:" to "V.Orb:" (or vice versa). Clears the row cell and re-chromes
// it with the new label. The value is redrawn on the next update cycle.
static void _lnchAsUpdateRow3Label(KCM_TFT &tft, bool showOrb) {
    const char *label = showOrb ? "V.Orb:" : "V.Srf:";
    printDispChrome(tft, LNCH_AS2_LBL_FONT,
                    LNCH_AS2_RPANEL_X, _lnchAs2RowY(3),
                    LNCH_AS2_RPANEL_W, LNCH_AS2_ROW_H,
                    label, COL_LABEL, TFT_BLACK, COL_NO_BDR);
    // Redraw the group divider above row 3 (re-painted at y=dy-1 and y=dy,
    // matching the chrome's updated divider placement). Before the position
    // move, printDispChrome's fillRect nibbled the divider's bottom pixel —
    // now the divider sits OUTSIDE the fill region so it's preserved
    // automatically. We still repair here because printDispChrome itself
    // paints over the row above's last pixel (which is y=dy-1 in the new
    // scheme), so we need to restore both pixels.
    //
    // The vertical divider at x=RPANEL_X-2..-1 sits OUTSIDE the row rect so
    // doesn't need repair.
    int16_t dy = _lnchAs2RowY(3);
    tft.drawLine(LNCH_AS2_RPANEL_X, dy - 1,
                 LNCH_AS2_RPANEL_X + LNCH_AS2_RPANEL_W - 1, dy - 1,
                 TFT_GREY);
    tft.drawLine(LNCH_AS2_RPANEL_X, dy,
                 LNCH_AS2_RPANEL_X + LNCH_AS2_RPANEL_W - 1, dy,
                 TFT_GREY);
    // Reset printState for row 3 so the next value redraws cleanly
    _lnchAsPs[3].prevWidth  = 0;
    _lnchAsPs[3].prevBg     = 0x0001;
    _lnchAsPs[3].prevHeight = 0;
}

static void _lnchAsUpdateVSrf(KCM_TFT &tft) {
    bool showOrb = _lnchAsShowOrbitalVelocity();
    float v = showOrb ? state.orbitalVel : state.surfaceVel;
    int16_t iV = (int16_t)roundf(v * 10.0f);  // tenths m/s

    uint16_t fg = (v < 0) ? TFT_RED : TFT_DARK_GREEN;

    // Label swap triggers chrome redraw for row 3.
    if (showOrb != _lnchAsPrevVRow3IsOrb) {
        _lnchAsUpdateRow3Label(tft, showOrb);
        _lnchAsPrevVRow3IsOrb = showOrb;
        _lnchAsPrevVSrf = -9999;  // force value redraw below
    }

    if (iV == _lnchAsPrevVSrf && fg == _lnchAsPrevVSrfFg) return;

    _lnchAsDrawRowValue(tft, 3, _lnchAsFmtMs1(v), fg, TFT_BLACK);
    _lnchAsPrevVSrf = iV;
    _lnchAsPrevVSrfFg = fg;
}

static void _lnchAsUpdateVVrt(KCM_TFT &tft) {
    float vv = state.verticalVel;
    int16_t iVv = (int16_t)roundf(vv * 10.0f);  // tenths m/s

    bool suppress = (fabsf(vv) < 0.05f) ||
                    (state.situation & sit_PreLaunch) ||
                    (state.situation & sit_Landed);

    uint16_t fg, bg;
    String val;
    if (suppress) {
        val = "---";
        fg = TFT_DARK_GREY; bg = TFT_BLACK;
        iVv = -9000;
    } else {
        if (vv < 0) { fg = TFT_WHITE;      bg = TFT_RED;   }
        else        { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
        val = _lnchAsFmtMs1(vv);
    }

    if (iVv == _lnchAsPrevVVrt && fg == _lnchAsPrevVVrtFg && bg == _lnchAsPrevVVrtBg) return;

    _lnchAsDrawRowValue(tft, 4, val, fg, bg);
    _lnchAsPrevVVrt = iVv;
    _lnchAsPrevVVrtFg = fg; _lnchAsPrevVVrtBg = bg;
}

static void _lnchAsUpdateThrottle(KCM_TFT &tft) {
    uint8_t thrPct = (uint8_t)constrain(state.throttle * 100.0f, 0.0f, 100.0f);

    uint16_t fg, bg;
    if (thrPct == 0) { fg = TFT_WHITE;      bg = TFT_RED;   }
    else             { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }

    if ((int16_t)thrPct == _lnchAsPrevThrottle &&
        fg == _lnchAsPrevThrFg && bg == _lnchAsPrevThrBg) return;

    _lnchAsDrawRowValue(tft, 5, formatPerc(thrPct), fg, bg);
    _lnchAsPrevThrottle = thrPct;
    _lnchAsPrevThrFg = fg; _lnchAsPrevThrBg = bg;
}

static void _lnchAsUpdateTBurn(KCM_TFT &tft) {
    float tb = state.stageBurnTime;
    int32_t iTb = (int32_t)roundf(tb);

    uint16_t fg, bg;
    thresholdColor(tb,
                   LNCH_BURNTIME_ALARM_S, TFT_WHITE,  TFT_RED,
                   LNCH_BURNTIME_WARN_S,  TFT_YELLOW, TFT_BLACK,
                        TFT_DARK_GREEN, TFT_BLACK, fg, bg);

    if (iTb == _lnchAsPrevTBurn && fg == _lnchAsPrevTBurnFg && bg == _lnchAsPrevTBurnBg) return;

    _lnchAsDrawRowValue(tft, 6, formatTime(tb), fg, bg);
    _lnchAsPrevTBurn = iTb;
    _lnchAsPrevTBurnFg = fg; _lnchAsPrevTBurnBg = bg;
}

static void _lnchAsUpdateDVStg(KCM_TFT &tft) {
    float dv = state.stageDeltaV;
    int32_t iDv = (int32_t)roundf(dv * 10.0f);  // tenths m/s

    uint16_t fg, bg;
    thresholdColor(dv,
                   DV_STG_ALARM_MS, TFT_WHITE,  TFT_RED,
                   DV_STG_WARN_MS,  TFT_YELLOW, TFT_BLACK,
                        TFT_DARK_GREEN, TFT_BLACK, fg, bg);

    if (iDv == _lnchAsPrevDVStg && fg == _lnchAsPrevDVStgFg && bg == _lnchAsPrevDVStgBg) return;

    _lnchAsDrawRowValue(tft, 7, _lnchAsFmtMs1(dv), fg, bg);
    _lnchAsPrevDVStg = iDv;
    _lnchAsPrevDVStgFg = fg; _lnchAsPrevDVStgBg = bg;
}

static void _lnchAsDrawRightPanelValues(KCM_TFT &tft) {
    _lnchAsUpdateAlt(tft);
    _lnchAsUpdateApA(tft);
    _lnchAsUpdateTimeToAp(tft);
    _lnchAsUpdateVSrf(tft);
    _lnchAsUpdateVVrt(tft);
    _lnchAsUpdateThrottle(tft);
    _lnchAsUpdateTBurn(tft);
    _lnchAsUpdateDVStg(tft);
}

