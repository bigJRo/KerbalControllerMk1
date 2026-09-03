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
     - LNCH_AS_PANEL_Y, LNCH_AS_LP_LEFT       (left graphics panel geometry)
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
//
// Seven rows, not the eight this panel carried before. V.Srf and V.Vrt moved onto the
// two bars that draw them; Alt.SL and ApA were dropped outright. T+Ap, Thrtl, Q, Mach,
// G, Stg.Brn and dV.Stg are what is left, every one a value with no picture anywhere on
// this screen. Fewer rows, so each is taller. ASCENT is the only user of this panel --
// the circularisation phase replaced its copy with the apsis tape.
//
// Alt.SL and ApA are position-only on this screen now: the altitude ladder marks both
// against a labelled tick scale, and there is nowhere to put their digits. The left
// panel is packed to 573 px of 576; the ladder strip is erased out to x=156 (its marker
// labels reach x=124) and the atmosphere column's left triangle starts at x=166, so the
// free space between them is 10 px, where "101 km" needs about 60. The PFD on the other panel carries both in the great majority of
// ascent phases; the exception is a spaceplane still in the atmosphere, where that panel
// is AIRCRAFT and has neither -- a deliberate, accepted cost rather than an oversight.
//
// Declared up here with the other anchors because the PrintState array and the label
// table size themselves from it a thousand lines above the readout geometry block, and
// the Arduino builder hoists function prototypes but not constants.
static const uint8_t LNCH_AS2_NROWS    = 7;

// The readout column is LNCH_AS2_RPANEL_W/X, declared with the rest of the row table
// further down. A second, identical pair (LNCH_AS_READOUT_W/X) used to be declared here
// -- same 360 px, same CONTENT_W - 360 -- and nothing read it.

// Left graphics panel: everything left of the readout column.
static const int16_t LNCH_AS_LP_LEFT   = 0;

// Shared vertical band for the ladder and all gauges. Move them together here.
static const int16_t LNCH_AS_BAND_TOP  = TITLE_TOP + 18;            // 80  (~18px under title bar)
static const int16_t LNCH_AS_BAND_BOT  = SCREEN_H  - 18;            // 582 (~18px above screen bottom)

// Gauge label rows within the band (name label on top; endpoint labels flank
// the gauge body). The bars/FPA/atmosphere bodies sit between BODY_TOP/BOT; the
// ladder uses the full band (it carries its own inline labels).
static const int16_t LNCH_AS_GAUGE_NAME_H   = 24;   // name label height (Black_20)
static const int16_t LNCH_AS_GAUGE_ENDLBL_H = 18;   // endpoint label height (Black_16)
static const int16_t LNCH_AS_GAUGE_NAME_Y   = LNCH_AS_BAND_TOP;                                                     // 80

// Digital window row, between the gauge name and its top endpoint label. Each bar
// carries its own value here rather than in the readout column 400 px to the right:
// a tape whose digits are at the other end of the display is two half-instruments,
// and reading magnitude and precision costs a saccade every time. Shifting BODY_TOP
// is all it takes to make room -- every gauge derives from it, which is what the
// anchor block above exists for.
static const int16_t LNCH_AS_GAUGE_VAL_H    = 26;
static const int16_t LNCH_AS_GAUGE_VAL_Y    = LNCH_AS_BAND_TOP + LNCH_AS_GAUGE_NAME_H;                              // 104
static const int16_t LNCH_AS_GAUGE_BODY_TOP = LNCH_AS_BAND_TOP + LNCH_AS_GAUGE_NAME_H + LNCH_AS_GAUGE_VAL_H
                                              + LNCH_AS_GAUGE_ENDLBL_H + 6;                                         // 154
static const int16_t LNCH_AS_GAUGE_BODY_BOT = LNCH_AS_BAND_BOT - LNCH_AS_GAUGE_ENDLBL_H - 2;                        // 562

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
// -90°=diving (bottom), 0°=horizon (right), +90°=climbing (top).
//
// The rightmost column stacks two elements: the FPA dial at the top (name on the
// shared gauge name row, arc top-aligned with the bars) and the HDG tape at the
// bottom (its number box bottom flush with the ATMO bar bottom).
static const int16_t LNCH_AS_FPA_LBL_MARGIN = 28;   // +90/-90 labels extend this far left of CX
static const int16_t LNCH_AS_FPA_R  = 96;           // radius (= width; height = 2R)
static const int16_t LNCH_AS_FPA_CX = LNCH_AS_VORB_X_LEFT + LNCH_AS_VORB_W + LNCH_AS_LBL_OVERHANG + LNCH_AS_COL_GAP + LNCH_AS_FPA_LBL_MARGIN;  // flat left side
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

// FPA dial vertical placement — top-aligned with the bars. "FPA" name sits on the
// shared gauge name row (aligned with V.Vrt/V.Orb); the arc top sits just below it.
static const int16_t LNCH_AS_FPA_CY     = LNCH_AS_GAUGE_BODY_TOP + LNCH_AS_FPA_R + 16;   // arc top ≈ body top
static const int16_t LNCH_AS_FPA_LBL_H  = 20;   // Black_16 angle-label box height
static const int16_t LNCH_AS_FPA_P90_Y  = LNCH_AS_FPA_CY - LNCH_AS_FPA_R - LNCH_AS_FPA_MAJ_OUT - LNCH_AS_FPA_LBL_H - 2;  // +90 label top
static const int16_t LNCH_AS_FPA_M90_Y  = LNCH_AS_FPA_CY + LNCH_AS_FPA_R + LNCH_AS_FPA_MAJ_OUT + 4;                       // -90 label top
static const int16_t LNCH_AS_FPA_NAME_Y = LNCH_AS_GAUGE_NAME_Y;   // "FPA" on the shared name row (aligned with V.Orb)
// Numeric readout box below the dial (bordered like the HDG number box).
static const int16_t LNCH_AS_FPA_VAL_W  = 96;
static const int16_t LNCH_AS_FPA_VAL_H  = 34;
static const int16_t LNCH_AS_FPA_VAL_X  = LNCH_AS_FPA_CX + LNCH_AS_FPA_R / 2 - LNCH_AS_FPA_VAL_W / 2;
static const int16_t LNCH_AS_FPA_VAL_Y  = LNCH_AS_FPA_CY + LNCH_AS_FPA_R + 42;

// ── Heading tape (bottom of the rightmost column) ─────────────────────────────
// Horizontal scrolling compass centered on the current vessel heading, with a
// bug at 90° (due-east launch azimuth) and the surface velocity-vector heading
// marker. The number box bottom is flush with the ATMO bar bottom
// (GAUGE_BODY_BOT); the tape and name stack upward from there.
static const int16_t LNCH_AS_HDG_CX      = LNCH_AS_FPA_CX + LNCH_AS_FPA_R / 2;   // centered on the dial
static const int16_t LNCH_AS_HDG_W       = 170;
static const int16_t LNCH_AS_HDG_X       = LNCH_AS_HDG_CX - LNCH_AS_HDG_W / 2;
static const float   LNCH_AS_HDG_SCALE   = 2.6f;                                 // px per degree (±~33° visible)
static const int16_t LNCH_AS_HDG_TAPE_H  = 28;
static const int16_t LNCH_AS_HDG_BOX_W   = 60;
static const int16_t LNCH_AS_HDG_BOX_H   = 34;
static const int16_t LNCH_AS_HDG_BOX_X   = LNCH_AS_HDG_CX - LNCH_AS_HDG_BOX_W / 2;
static const int16_t LNCH_AS_HDG_BOX_Y   = LNCH_AS_GAUGE_BODY_BOT - LNCH_AS_HDG_BOX_H;   // box bottom = ATMO bar bottom
static const int16_t LNCH_AS_HDG_TAPE_Y  = LNCH_AS_HDG_BOX_Y;                            // box top = tape top (box overhangs below)
static const int16_t LNCH_AS_HDG_NAME_Y  = LNCH_AS_HDG_TAPE_Y - 26;                      // name row above the tape
static const int16_t LNCH_AS_HDG_SUPP_LO = LNCH_AS_HDG_BOX_X - 16;
static const int16_t LNCH_AS_HDG_SUPP_HI = LNCH_AS_HDG_BOX_X + LNCH_AS_HDG_BOX_W + 16;
static const float   LNCH_AS_HDG_LAUNCH_AZ  = 90.0f;   // due-east launch-azimuth bug
static const float   LNCH_AS_HDG_VEL_MIN_MS = 20.0f;   // hide velocity marker below this surface speed

// ── STAGE indicator button (in the gap between the FPA readout and the HDG tape) ─
// Off = standard indicator look (dark-grey text on off-black, grey border); when
// the current stage's ΔV is spent it lights red with white text — "time to stage".
// Centered on the dial/HDG column, vertically midway between the FPA value box
// bottom and the HDG name row.
static const int16_t LNCH_AS_STAGE_W = 134;   // 40% wider than the original 96
static const int16_t LNCH_AS_STAGE_H = 43;    // 20% taller than the original 36
static const int16_t LNCH_AS_STAGE_X = LNCH_AS_HDG_CX - LNCH_AS_STAGE_W / 2;
// Vertically centered in the whitespace between the FPA value box bottom and the
// HDG name row top (recomputes if either moves or the button height changes).
static const int16_t LNCH_AS_STAGE_Y = (LNCH_AS_FPA_VAL_Y + LNCH_AS_FPA_VAL_H + LNCH_AS_HDG_NAME_Y) / 2
                                       - LNCH_AS_STAGE_H / 2;
static const float   LNCH_AS_STAGE_EMPTY_MS = 0.5f;    // stage ΔV at/below this reads as spent

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
// Alt.SL, ApA, V.Srf and V.Vrt moved onto their gauges, and their caches with them --
// the gauge windows keep last-drawn strings instead. What is left is the readout
// column's own seven rows.
static int32_t _lnchAsPrevTimeToAp   = -1 << 30;  // seconds
static int16_t _lnchAsPrevThrottle   = -1;        // percent (0-100)
static int32_t _lnchAsPrevQ          = -1 << 30;  // tenths of a kPa
static int32_t _lnchAsPrevMach       = -9999;     // hundredths, -9999 = "---"
static int32_t _lnchAsPrevG          = -1 << 30;  // tenths of a g
static int32_t _lnchAsPrevTBurn      = -1 << 30;  // seconds
static int32_t _lnchAsPrevDVStg      = -1 << 30;  // m/s (as tenths? or int)
// Track colors too (threshold changes don't alter the numeric value but still require redraw)
static uint16_t _lnchAsPrevTimeToApFg= 0xFFFF;
static uint16_t _lnchAsPrevThrFg     = 0xFFFF; static uint16_t _lnchAsPrevThrBg  = 0xFFFF;
static uint16_t _lnchAsPrevQFg       = 0xFFFF; static uint16_t _lnchAsPrevQBg    = 0xFFFF;
static uint16_t _lnchAsPrevGFg       = 0xFFFF; static uint16_t _lnchAsPrevGBg    = 0xFFFF;
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

// FPA dial state.
static int16_t _lnchAsPrevFpaReadout   = -9999;  // last FPA value shown (integer degrees)
static int16_t _lnchAsPrevFpaTarget    = -9999;  // last target FPA marker position (integer degrees)

// Atmosphere gauge state.
// Tracks previous triangle indicator center x-coordinate for flicker-free
// incremental updates. Sentinel -1 = "not yet drawn" (first frame).
// With the four-zone design (including OFF_BLACK parking), the triangle
// position alone encodes whether we're in a no-atmosphere or atmospheric
// state — no separate NoAtm flag is needed.
static int16_t _lnchAsPrevAtmoTriY   = -1;

// Heading tape state.
static float   _lnchAsPrevHdg    = -9999.0f;   // last tape-center heading
static int16_t _lnchAsPrevHdgBox = -9999;      // last integer heading in the box
static float   _lnchAsPrevVelHdg = -9999.0f;   // last velocity-vector heading

// STAGE indicator button state (-1 = not yet drawn).
static int8_t  _lnchAsPrevStageActive = -1;

// ── Ascent phase helpers ──────────────────────────────────────────────────────────────
//
// Right panel uses library printDispChrome (for labels/borders in chrome) and
// printValue (for the value in updates), matching the pattern used by every other
// screen. PrintState tracking handles flicker-free redraws and correct erase of
// prior values (including when string width or background colour changes).
//
// Each row is a full-width cell in the right panel; the label is drawn on the
// left and the value is right-aligned. Labels are short ("V.SRF", "ΔV.STG")
// and don't carry units since the unit ("m/s") is implicit from KSP convention.

static PrintState _lnchAsPs[LNCH_AS2_NROWS];   // PrintState tracking, one per readout row


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

// Draw an altitude-marker triangle pointing left at altitude y, with a short label to
// the right. 'filled' selects vessel (filled, "ALT") vs apoapsis (hollow, "Ap") style;
// the label is drawn at Black_16 in the same color as the triangle.
// Tip at LADDER_LINE_X+3, base at MARKER_X + MARKER_W, label just past the base.
static const int16_t LNCH_AS_MARKER_LBL_X    = 100;   // x of the label (past the base)
static const int16_t LNCH_AS_MARKER_LBL_W    = 32;    // width reserved for the marker label
                                                     // "ALT" is 30 px at Black_16, so the
                                                     // cell is 32. This also sizes the
                                                     // marker ERASE, which is why it has
                                                     // to be measured rather than guessed.
// A marker's drawn extent and the erase box are both 19 rows tall, so erasing one
// marker clips the other whenever their centres are within 19 - 1 + 19 - 1 = 18 px.
// (That 19 was also spelled out as a constant here; nothing read it, and a number no
// code depends on is one that drifts from the geometry it claims to describe.)

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
    // Label to the right of the triangle. Drawn opaquely on black, so where a marker
    // sits on a reference line its label covers that line's label rather than
    // interleaving with it; the repair below puts the reference label back when the
    // marker moves off.
    if (!label || !*label) return;
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

// Reference-line labels (GND / ATM / TGT+ORB). Their glyphs start at x=118 and the
// marker erase box reaches x=132, so a marker passing a reference line takes a bite out
// of its label. The lines were already repaired after an erase; the labels were not, and
// the bite stayed on screen until the next ladder scale change. Both the chrome and the
// repair go through these two helpers now, so the two cannot drift apart.
static const int16_t LNCH_AS_REF_LBL_X = LNCH_AS_LADDER_REF_X2 - 40;
static const int16_t LNCH_AS_REF_LBL_W = 44;
static const int16_t LNCH_AS_REF_LBL_H = 16;

static void _lnchAsDrawRefLabel(KCM_TFT &tft, int16_t y0, const char *s, uint16_t color) {
    textLeft(tft, &Roboto_Black_16,
             LNCH_AS_REF_LBL_X, y0, LNCH_AS_REF_LBL_W, LNCH_AS_REF_LBL_H,
             s, color, TFT_BLACK);
}

// The ORBIT line's label is "TGT" over "ORB" straddling the line, so the pilot reads it
// as an altitude being aimed for; it falls back to a single centred "ORB" when the line
// is too close to either end of the ladder for two rows.
static void _lnchAsDrawOrbRefLabel(KCM_TFT &tft, int16_t refY) {
    if (refY > LNCH_AS_LADDER_Y_TOP + 18 && refY < LNCH_AS_LADDER_Y_BOT - 18) {
        _lnchAsDrawRefLabel(tft, refY - 17, "TGT", TFT_DARK_GREEN);
        _lnchAsDrawRefLabel(tft, refY +  1, "ORB", TFT_DARK_GREEN);
    } else {
        _lnchAsDrawRefLabel(tft, refY -  8, "ORB", TFT_DARK_GREEN);
    }
}

// After erasing a marker, repair the reference lines (GND/ATM/ORB) and their labels
// where the erased region crossed them. markerY is the y-center of the erased bounding
// box (height 19, so the box spans y-9 to y+9).
//
// The line and the label have different reach. A line is one row, so it needs repair
// only when it falls inside the box. A label is 19 rows tall and the two-row ORB label
// straddles its line by 18, so a label is repainted whenever its line is within 28 rows
// -- generous on purpose: the labels are opaque and identical every time, so a redundant
// repaint costs nothing and a missed one leaves a hole.
static const int16_t LNCH_AS_REF_LBL_REACH = 28;

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
        if (abs(refY - markerY) <= LNCH_AS_REF_LBL_REACH)
            _lnchAsDrawRefLabel(tft, refY - 8, "GND", TFT_LIGHT_GREY);
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
        if (refY < LNCH_AS_LADDER_Y_BOT - 2 && refY > LNCH_AS_LADDER_Y_TOP + 2 &&
            abs(refY - markerY) <= LNCH_AS_REF_LBL_REACH)
            _lnchAsDrawRefLabel(tft, refY - 8, "ATM", TFT_SKY);
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
        if (refY < LNCH_AS_LADDER_Y_BOT - 2 && refY > LNCH_AS_LADDER_Y_TOP + 2 &&
            abs(refY - markerY) <= LNCH_AS_REF_LBL_REACH)
            _lnchAsDrawOrbRefLabel(tft, refY);
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
        // Label vertically centered on the ground line (matches ATM/ORB). There
        // is room below now that the ladder bottom sits ~18px above the screen edge.
        _lnchAsDrawRefLabel(tft, y - 8, "GND", TFT_LIGHT_GREY);
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
            _lnchAsDrawRefLabel(tft, y - 8, "ATM", TFT_SKY);
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
            // "TGT" over "ORB" straddling the line, or a single centred "ORB" when
            // there is not room for two rows -- see _lnchAsDrawOrbRefLabel().
            _lnchAsDrawOrbRefLabel(tft, y);
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

    // Vessel and apoapsis markers.
    //
    // Both are erased, then both are redrawn, whenever either has moved. The version
    // this replaces erased one marker and redrew the other only "if it was within 6 px",
    // which was a guess: the erase box is 19 rows tall and a marker's own drawn extent is
    // 19 rows, so one marker's erase clips the other up to 18 px away, and the reference-
    // label repair above reaches 28. A miss left a marker half erased on the ladder until
    // the next scale change, which is what the bench saw. Two triangles and two short
    // labels cost nothing per frame; the proximity arithmetic cost correctness.
    int16_t    vesselY   = _lnchAsAltToY(state.altitude, scaleTop);
    const bool apaValid  = (state.apoapsis > 0.0f);
    int16_t    apaY      = apaValid ? _lnchAsAltToY(state.apoapsis, scaleTop) : (int16_t)-1;

    const bool moved = (vesselY != _lnchAsPrevVesselPx) ||
                       (apaValid != _lnchAsPrevApAValid) ||
                       (apaValid && apaY != _lnchAsPrevApAPx);
    if (!moved) return;

    if (_lnchAsPrevVesselPx >= 0) {
        _lnchAsEraseAltMarker(tft, _lnchAsPrevVesselPx);
        _lnchAsRepairRefLines(tft, _lnchAsPrevVesselPx);
    }
    if (_lnchAsPrevApAValid && _lnchAsPrevApAPx >= 0) {
        _lnchAsEraseAltMarker(tft, _lnchAsPrevApAPx);
        _lnchAsRepairRefLines(tft, _lnchAsPrevApAPx);
    }

    // Apoapsis first, so where the two coincide the vessel marker is the one on top.
    if (apaValid) _lnchAsDrawAltMarker(tft, apaY, TFT_YELLOW, false, "Ap");
    _lnchAsDrawAltMarker(tft, vesselY, TFT_DARK_GREEN, true, "ALT");

    _lnchAsPrevVesselPx = vesselY;
    _lnchAsPrevApAPx    = apaValid ? apaY : (int16_t)-1;
    _lnchAsPrevApAValid = apaValid;
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

// Angle labels (Black_16, matching the V.Vrt "±500 m/s" endpoint labels), placed
// clear of the tick marks: +90 above the top tick, -90 below the bottom tick, 0 to
// the right of the arc edge. Split out so the target-marker erase can repair the
// label it sits on (the +90 marker overlaps the "+90°" text).
static void _lnchAsDrawDialLabels(KCM_TFT &tft) {
    textCenter(tft, &Roboto_Black_16,
               LNCH_AS_FPA_CX - 32, LNCH_AS_FPA_P90_Y,
               64, LNCH_AS_FPA_LBL_H, "+90\xB0", TFT_LIGHT_GREY, TFT_BLACK);
    textCenter(tft, &Roboto_Black_16,
               LNCH_AS_FPA_CX - 32, LNCH_AS_FPA_M90_Y,
               64, LNCH_AS_FPA_LBL_H, "-90\xB0", TFT_LIGHT_GREY, TFT_BLACK);
    textLeft(tft, &Roboto_Black_16,
             LNCH_AS_FPA_CX + LNCH_AS_FPA_R + LNCH_AS_FPA_MAJ_OUT + 4, LNCH_AS_FPA_CY - 10,
             30, LNCH_AS_FPA_LBL_H, "0\xB0", TFT_LIGHT_GREY, TFT_BLACK);
}

// Dial chrome: arc, horizon, ticks, pivot, "FPA" name, angle labels.
static void _lnchAsDrawDialChrome(KCM_TFT &tft) {
    _lnchAsDrawDialArc(tft, TFT_LIGHT_GREY);
    tft.drawLine(LNCH_AS_FPA_CX, LNCH_AS_FPA_CY,
                 LNCH_AS_FPA_CX + LNCH_AS_FPA_R, LNCH_AS_FPA_CY, TFT_LIGHT_GREY);   // horizon
    _lnchAsDrawDialTicks(tft);
    tft.fillCircle(LNCH_AS_FPA_CX, LNCH_AS_FPA_CY, LNCH_AS_FPA_PIVOT_R, TFT_DARK_GREEN);

    // "FPA" name (Black_20), centered over the dial, above the +90 label.
    textCenter(tft, &Roboto_Black_20,
               LNCH_AS_HDG_CX - 50, LNCH_AS_FPA_NAME_Y,
               100, LNCH_AS_GAUGE_NAME_H,
               "FPA", TFT_LIGHT_GREY, TFT_BLACK);

    _lnchAsDrawDialLabels(tft);

    // Numeric readout box border (matches the HDG number box aesthetic). The
    // value itself is painted inside it by _lnchAsUpdateFpaDial.
    tft.drawRect(LNCH_AS_FPA_VAL_X, LNCH_AS_FPA_VAL_Y,
                 LNCH_AS_FPA_VAL_W, LNCH_AS_FPA_VAL_H, TFT_LIGHT_GREY);
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

    if (targetChanged && _lnchAsPrevFpaTarget != -9999) {
        _lnchAsDrawDialTargetMarker(tft, (float)_lnchAsPrevFpaTarget, TFT_BLACK);
        // The marker sits just outside the arc where the +90/0/-90 labels live, so
        // its black erase can nibble a label — repair them (cheap, target moves rarely).
        _lnchAsDrawDialLabels(tft);
    }
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

    // Numeric readout inside the bordered box under the dial.
    if (arrowChanged) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%+d\xB0", iFpa);
        tft.fillRect(LNCH_AS_FPA_VAL_X + 1, LNCH_AS_FPA_VAL_Y + 1,
                     LNCH_AS_FPA_VAL_W - 2, LNCH_AS_FPA_VAL_H - 2, TFT_BLACK);
        textCenter(tft, &Roboto_Black_24,
                   LNCH_AS_FPA_VAL_X, LNCH_AS_FPA_VAL_Y + 1,
                   LNCH_AS_FPA_VAL_W, LNCH_AS_FPA_VAL_H - 2, buf, TFT_DARK_GREEN, TFT_BLACK);
    }
    _lnchAsPrevFpaReadout = iFpa;
    _lnchAsPrevFpaTarget  = iTarget;
}

// ── Heading tape ──────────────────────────────────────────────────────────────────────

// Heading number box (cached) — current vessel heading, "NNN°".
static void _lnchAsUpdateHdgBox(KCM_TFT &tft, float hdg) {
    int16_t iHdg = (int16_t)roundf(hdg) % 360;
    if (iHdg < 0) iHdg += 360;
    if (iHdg == _lnchAsPrevHdgBox) return;

    char buf[8];
    if (_lnchAsPrevHdgBox >= 0) {           // erase previous value (black-on-black)
        snprintf(buf, sizeof(buf), "%03d\xB0", _lnchAsPrevHdgBox);
        eraseCenteredValue(tft, &Roboto_Black_24, LNCH_AS_HDG_BOX_X, LNCH_AS_HDG_BOX_Y + 1,
                   LNCH_AS_HDG_BOX_W, LNCH_AS_HDG_BOX_H - 2, buf, TFT_BLACK);
    }
    snprintf(buf, sizeof(buf), "%03d\xB0", iHdg);
    textCenter(tft, &Roboto_Black_24, LNCH_AS_HDG_BOX_X, LNCH_AS_HDG_BOX_Y + 1,
               LNCH_AS_HDG_BOX_W, LNCH_AS_HDG_BOX_H - 2, buf, TFT_DARK_GREEN, TFT_BLACK);
    _lnchAsPrevHdgBox = iHdg;
}

// Redraw the heading tape strip: ticks, N/E/S/W + degree labels, and markers
// (90° launch bug + velocity-vector heading). Tape center = current vessel heading.
static void _lnchAsDrawHdgTape(KCM_TFT &tft, float hdg) {
    while (hdg <   0.0f) hdg += 360.0f;
    while (hdg >= 360.0f) hdg -= 360.0f;

    tft.fillRect(LNCH_AS_HDG_X, LNCH_AS_HDG_TAPE_Y, LNCH_AS_HDG_W, LNCH_AS_HDG_TAPE_H, TFT_BLACK);
    tft.drawRect(LNCH_AS_HDG_BOX_X, LNCH_AS_HDG_BOX_Y, LNCH_AS_HDG_BOX_W, LNCH_AS_HDG_BOX_H, TFT_LIGHT_GREY);
    _lnchAsPrevHdgBox = -1;   // fill blackened the box interior — force number redraw

    int16_t halfDeg = (int16_t)(LNCH_AS_HDG_W / (2.0f * LNCH_AS_HDG_SCALE)) + 1;
    tft.setFont(Roboto_Black_12);
    for (int16_t d = -halfDeg; d <= halfDeg; d++) {
        float deg = hdg + (float)d;
        while (deg <   0.0f) deg += 360.0f;
        while (deg >= 360.0f) deg -= 360.0f;
        int16_t px = LNCH_AS_HDG_CX + (int16_t)roundf((float)d * LNCH_AS_HDG_SCALE);
        if (px <= LNCH_AS_HDG_X || px >= LNCH_AS_HDG_X + LNCH_AS_HDG_W) continue;
        if (px >= LNCH_AS_HDG_SUPP_LO && px <= LNCH_AS_HDG_SUPP_HI) continue;
        int16_t ideg = (int16_t)roundf(deg); if (ideg == 360) ideg = 0;

        if (ideg % 10 == 0) {
            tft.drawLine(px, LNCH_AS_HDG_TAPE_Y, px, LNCH_AS_HDG_TAPE_Y + 9, TFT_LIGHT_GREY);
            const char *lbl; uint16_t col; char nb[8];
            if      (ideg ==   0) { lbl = "N"; col = TFT_YELLOW; }
            else if (ideg ==  90) { lbl = "E"; col = TFT_WHITE;  }
            else if (ideg == 180) { lbl = "S"; col = TFT_WHITE;  }
            else if (ideg == 270) { lbl = "W"; col = TFT_WHITE;  }
            else { snprintf(nb, sizeof(nb), "%d", ideg); lbl = nb; col = TFT_LIGHT_GREY; }
            tft.setTextColor(col, TFT_BLACK);
            uint8_t lw = strlen(lbl) * 8;
            int16_t cx = px - (int16_t)(lw / 2);
            // Skip the label entirely if it would run off either edge — clamping it
            // to the edge makes labels stall and bunch up as the tape scrolls.
            if (cx >= LNCH_AS_HDG_X + 1 && cx + lw <= LNCH_AS_HDG_X + LNCH_AS_HDG_W - 1) {
                tft.setCursor(cx, LNCH_AS_HDG_TAPE_Y + 11);
                tft.print(lbl);
            }
        } else if (ideg % 2 == 0) {
            tft.drawLine(px, LNCH_AS_HDG_TAPE_Y, px, LNCH_AS_HDG_TAPE_Y + 5, TFT_DARK_GREY);
        }
    }

    // Markers — downward triangles inside the tape (pegged to the edges, hidden
    // behind the center box's suppress zone).
    auto drawMarker = [&](float mHdg, uint16_t col) {
        float diff = mHdg - hdg;
        while (diff >  180.0f) diff -= 360.0f;
        while (diff < -180.0f) diff += 360.0f;
        int16_t px = LNCH_AS_HDG_CX + (int16_t)roundf(diff * LNCH_AS_HDG_SCALE);
        int16_t pxMin = LNCH_AS_HDG_X + 7, pxMax = LNCH_AS_HDG_X + LNCH_AS_HDG_W - 7;
        if (px < pxMin) px = pxMin;
        if (px > pxMax) px = pxMax;
        if (px >= LNCH_AS_HDG_SUPP_LO && px <= LNCH_AS_HDG_SUPP_HI) return;
        tft.fillTriangle(px,     LNCH_AS_HDG_TAPE_Y + 18,
                         px - 6, LNCH_AS_HDG_TAPE_Y + 2,
                         px + 6, LNCH_AS_HDG_TAPE_Y + 2, col);
    };
    // Velocity-vector heading (green), only when actually moving.
    if (state.surfaceVel >= LNCH_AS_HDG_VEL_MIN_MS)
        drawMarker(state.srfVelHeading, TFT_NEON_GREEN);
    // 90° launch-azimuth bug (amber) drawn last so it sits on top.
    drawMarker(LNCH_AS_HDG_LAUNCH_AZ, TFT_YELLOW);
}

// Static chrome: "HDG" label + initial tape/box. Resets change-detection state.
static void _lnchAsDrawHdgTapeChrome(KCM_TFT &tft) {
    _lnchAsPrevHdg = -9999.0f; _lnchAsPrevHdgBox = -9999; _lnchAsPrevVelHdg = -9999.0f;
    textCenter(tft, &Roboto_Black_20, LNCH_AS_HDG_X, LNCH_AS_HDG_NAME_Y,
               LNCH_AS_HDG_W, 24, "HDG", TFT_LIGHT_GREY, TFT_BLACK);
    _lnchAsDrawHdgTape(tft, state.heading);
    _lnchAsUpdateHdgBox(tft, state.heading);
}

// Per-frame update — redraw the strip when heading or velocity heading changes.
static void _lnchAsUpdateHdgTape(KCM_TFT &tft) {
    float hdg = state.heading;
    bool dirty = fabsf(hdg - _lnchAsPrevHdg) >= 0.3f
              || fabsf(state.srfVelHeading - _lnchAsPrevVelHdg) >= 0.3f;
    if (dirty) {
        _lnchAsDrawHdgTape(tft, hdg);
        _lnchAsPrevHdg    = hdg;
        _lnchAsPrevVelHdg = state.srfVelHeading;
    }
    _lnchAsUpdateHdgBox(tft, hdg);
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

// Which velocity is the meaningful one. Surface speed low down; orbital once the
// vessel is leaving the air, where that is the number an ascent is flown on. Altitude
// is the right axis for this question, with hysteresis so the value does not flicker
// at the boundary.
//
// This was once the same formula as the ASCENT/CIRCULARISATION phase switch. It no
// longer is -- that switch keys off the engine, since altitude could not tell a
// spaceplane still burning from one coasting -- and the two are independent decisions:
// which velocity to show, and which phase of the arc this is.
//
// It used to drive a label swap on a readout row. That row is now the V.Orb gauge's own
// digital window, and this drives the window instead.
static bool _lnchAsShowOrbitalVelocity() {
    float bodyRad  = (currentBody.radius > 0.0f) ? currentBody.radius : DEFAULT_BODY_RADIUS_M;
    bool  ascending = (state.verticalVel >= 0.0f);
    float switchAlt = ascending ? (bodyRad * ORB_SWITCH_ALT_FRAC_ASC)
                                : (bodyRad * ORB_SWITCH_ALT_FRAC_DESC);
    return (state.altitude > switchAlt);
}


// ── Gauge digital windows ─────────────────────────────────────────────────────────────
// One value per bar, centred in the row between the gauge name and its top endpoint
// label. Each keeps its own last-drawn string so an unchanged value costs nothing; the
// box is cleared and redrawn rather than tracked per glyph, which is cheap here because
// the row is 72 px wide and hardware fillRect is the fastest thing this driver does.
// Widest that fits the 72 px bar window at Black_20 is five digits and a sign
// ("-1,234" is 62 px); six digits ("123,456", 77 px) would not. That is 100 km/s, and
// this screen is only up below the coast latch, where V.Orb tops out near 3,000.
static String _lnchAsPrevVVrtVal, _lnchAsPrevVOrbVal;

static void _lnchAsGaugeValue(KCM_TFT &tft, int16_t lblX0, int16_t lblW,
                              const String &val, uint16_t fg, String &prev) {
    if (val == prev) return;
    // Boxed, so the window reads as part of its gauge rather than as loose text
    // floating above it. Border drawn every time: the fill that clears the old value
    // covers the inside only, so the frame is never touched, but drawing it here keeps
    // the window whole after a chrome repaint without a separate code path.
    tft.fillRect(lblX0 + 1, LNCH_AS_GAUGE_VAL_Y + 1,
                 lblW - 2, LNCH_AS_GAUGE_VAL_H - 2, TFT_BLACK);
    tft.drawRect(lblX0, LNCH_AS_GAUGE_VAL_Y, lblW, LNCH_AS_GAUGE_VAL_H, TFT_GREY);
    textCenter(tft, &Roboto_Black_20, lblX0, LNCH_AS_GAUGE_VAL_Y, lblW,
               LNCH_AS_GAUGE_VAL_H, val, fg, TFT_BLACK);
    prev = val;
}

// Dynamic pressure, from the density the ATMO gauge is already drawing and surface
// speed: q = 0.5 * rho * v^2, in pascals. Shown in kPa, which keeps a Kerbin ascent's
// 10-15 kPa peak to three characters instead of five.
//
// This is the number the ATMO gauge cannot show. Density falls monotonically from the
// pad; q rises to a peak around 5-8 km and then falls, and that peak is the one
// structural event of an ascent.
static float _lnchAsDynPressureKPa() {
    const float v = state.surfaceVel;
    return 0.5f * state.airDensity * v * v / 1000.0f;
}

// Chrome: name label + stable zone fill + border.
static void _lnchAsDrawAtmoChrome(KCM_TFT &tft) {
    // Name box matches the column's true extent (triangle to triangle), not a fixed
    // 100 px centred on the bar: the wider box reached x=252 and overlapped the V.Vrt
    // window at 248, so once both carried a border the two drew in the same four columns
    // and the shared edge flickered between them every frame.
    //
    // This column has a name and no value window. A percentage restating where the
    // triangle already points said nothing, and the time-to-vacuum that briefly replaced
    // it was a different quantity from the one the bar plots, which forced the column
    // name to label the number instead of the gauge. The bar is the instrument here --
    // sky-to-navy with a triangle on each side -- and it does not need a number under it
    // to be read. Q, on the right column, is the atmospheric number worth digits, and it
    // is the one the bar genuinely cannot show.
    const int16_t nameX0 = LNCH_AS_ATMO_X_LEFT - LNCH_AS_ATMO_TRI_GAP - LNCH_AS_ATMO_TRI_W;
    const int16_t nameW  = LNCH_AS_ATMO_RIGHT_EXT - nameX0;
    textCenter(tft, &Roboto_Black_20,
               nameX0, LNCH_AS_GAUGE_NAME_Y, nameW, LNCH_AS_GAUGE_NAME_H,
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
    // In vacuum (airDensity 0, frac == 0) or on a non-atmosphere body (frac < 0),
    // park the indicators centered in the dark OFF_BLACK segment at the bottom
    // rather than pegging them at the frac=0 line at the top of that segment.
    bool  vacuum = (frac <= 0.0f);

    int16_t innerY1 = LNCH_AS_ATMO_Y_BOT - 1;
    int16_t atmYBot, atmH;
    _lnchAsAtmoScaleGeom(atmYBot, atmH);

    int16_t triY = vacuum ? (int16_t)((atmYBot + 1 + innerY1) / 2)
                          : _lnchAsAtmoFracToY(frac);
    if (triY == _lnchAsPrevAtmoTriY) return;

    if (_lnchAsPrevAtmoTriY >= 0) {
        _lnchAsDrawAtmoTriangle(tft, _lnchAsPrevAtmoTriY, TFT_BLACK);
    }
    _lnchAsDrawAtmoTriangle(tft, triY, TFT_WHITE);
    _lnchAsPrevAtmoTriY = triY;
}

// ── STAGE indicator button ────────────────────────────────────────────────────
// Sits in the whitespace between the FPA readout and the HDG tape. Lights red
// (white text) when the current stage's ΔV is spent — a "time to stage" cue —
// and otherwise shows the standard off-look. Change-detected: redraws only when
// the active state flips.
static void _lnchAsUpdateStageButton(KCM_TFT &tft) {
    int8_t active = (state.stageDeltaV <= LNCH_AS_STAGE_EMPTY_MS) ? 1 : 0;
    if (active == _lnchAsPrevStageActive) return;

    static const ButtonLabel STAGE_LBL = {
        "STAGE",
        TFT_DARK_GREY,    // fontColorOff
        TFT_WHITE,        // fontColorOn
        TFT_OFF_BLACK,    // backgroundColorOff
        TFT_RED,          // backgroundColorOn
        TFT_GREY,         // borderColorOff
        TFT_GREY,         // borderColorOn
    };
    drawButton(tft, LNCH_AS_STAGE_X, LNCH_AS_STAGE_Y,
               LNCH_AS_STAGE_W, LNCH_AS_STAGE_H,
               STAGE_LBL, &Roboto_Black_28, active != 0);
    _lnchAsPrevStageActive = active;
}

// Left panel chrome — draw ladder (dynamic, dependent on current state),
// V.Vrt / V.Orb bars, FPA dial, heading tape, and atmosphere (static chrome).
static void _lnchAsDrawLeftPanelChrome(KCM_TFT &tft) {
    _lnchAsDrawLadderChrome(tft);
    _lnchAsDrawVVrtChrome(tft);
    _lnchAsDrawVOrbChrome(tft);
    _lnchAsDrawDialChrome(tft);
    _lnchAsDrawHdgTapeChrome(tft);
    _lnchAsDrawAtmoChrome(tft);
    // Record scale that was just drawn, so UpdateLadderMarkers doesn't trigger
    // an immediate spurious redraw on the first frame after chrome.
    _lnchAsLastDrawnScaleTop = _lnchAsLadderScaleTop();
}

// Called every frame: update the ladder markers, bars, dial, heading tape, atmo.
// The three bar gauges' digital windows. Their values used to be rows 0-4 of the
// readout column; they now sit on the gauges they belong to.
static void _lnchAsUpdateGaugeValues(KCM_TFT &tft) {
    // Two windows, not three: the atmosphere column carries a name and a bar only.
    // See _lnchAsDrawAtmoChrome() for why.
    // V.Vrt — signed, red descending, matching the bar's own fill colours.
    //
    // Bare number, no unit. The window is 72 px and fmtMs() carries " m/s", which makes
    // even "-12.0 m/s" 90 px wide; the bar's own endpoint labels already say +/-500 m/s
    // directly above and below it, so repeating the unit is what would not fit AND what
    // says nothing.
    {
        const int16_t x0 = LNCH_AS_VVRT_X_LEFT - LNCH_AS_LBL_OVERHANG;
        const int16_t w  = LNCH_AS_VVRT_W + 2 * LNCH_AS_LBL_OVERHANG;
        _lnchAsGaugeValue(tft, x0, w,
                          formatSepI64((int64_t)lroundf(state.verticalVel)),
                          (state.verticalVel < 0.0f) ? TFT_RED : TFT_DARK_GREEN,
                          _lnchAsPrevVVrtVal);
    }
    // V.Orb — surface speed low down, orbital once that is the meaningful number.
    // Same swap the readout row used to make, on the same test.
    {
        const int16_t x0 = LNCH_AS_VORB_X_LEFT - LNCH_AS_LBL_OVERHANG;
        const int16_t w  = LNCH_AS_VORB_W + 2 * LNCH_AS_LBL_OVERHANG;
        const bool orb = _lnchAsShowOrbitalVelocity();
        _lnchAsGaugeValue(tft, x0, w,
                          formatSepI64((int64_t)lroundf(orb ? state.orbitalVel
                                                            : state.surfaceVel)),
                          TFT_DARK_GREEN, _lnchAsPrevVOrbVal);
    }
}

static void _lnchAsDrawLeftPanelValues(KCM_TFT &tft) {
    _lnchAsUpdateLadderMarkers(tft);
    _lnchAsUpdateGaugeValues(tft);
    _lnchAsUpdateVVrtBar(tft);
    _lnchAsUpdateVOrbBar(tft);
    _lnchAsUpdateFpaDial(tft);
    _lnchAsUpdateHdgTape(tft);
    _lnchAsUpdateAtmoGauge(tft);
    _lnchAsUpdateStageButton(tft);
}


// Ascent/Circ velocity readouts use the shared fmtMs() (thousands-separated at
// >=1000, 1 decimal below) for cross-screen consistency. fmtMs is never wider
// than the old 1-decimal form here (it drops the decimal and adds a comma at
// >=1000), so it stays within the tight ~140 px value region for "ΔV.STG".

// Reset all ascent-phase change-detection state + PrintState. Called at chrome
// time to force a full redraw.
static void _lnchAsResetState() {
    _lnchAsPrevTimeToAp   = -1 << 30;
    _lnchAsPrevThrottle   = -1;
    _lnchAsPrevQ          = -1 << 30;
    _lnchAsPrevMach       = -9999;
    _lnchAsPrevG          = -1 << 30;
    _lnchAsPrevTBurn      = -1 << 30;
    _lnchAsPrevDVStg      = -1 << 30;
    _lnchAsPrevTimeToApFg = 0xFFFF;
    _lnchAsPrevThrFg      = 0xFFFF; _lnchAsPrevThrBg  = 0xFFFF;
    _lnchAsPrevQFg        = 0xFFFF; _lnchAsPrevQBg    = 0xFFFF;
    _lnchAsPrevGFg        = 0xFFFF; _lnchAsPrevGBg    = 0xFFFF;
    _lnchAsPrevTBurnFg    = 0xFFFF; _lnchAsPrevTBurnBg = 0xFFFF;
    _lnchAsPrevDVStgFg    = 0xFFFF; _lnchAsPrevDVStgBg = 0xFFFF;
    // Gauge digital windows: forget the last string so each redraws with its chrome.
    _lnchAsPrevVVrtVal = ""; _lnchAsPrevVOrbVal = "";
    for (uint8_t i = 0; i < LNCH_AS2_NROWS; i++) {
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
    _lnchAsPrevFpaReadout     = -9999;
    _lnchAsPrevFpaTarget      = -9999;
    _lnchAsPrevAtmoTriY       = -1;
    _lnchAsPrevStageActive    = -1;
}


// Row labels. The unit ("m/s") is included in labels for velocity rows to make
// the row purpose unambiguous. Other units follow KSP convention (formatAlt for
// altitudes, formatTime for time-based values, formatPerc for throttle).
// Row 3 ("V.Srf" / "V.Orb") is label-swapped by the update function based on
// altitude — at high altitude the value switches from surface velocity to
// orbital velocity.
// Stg.Brn, not T.Brn: it is Simpit's BURNTIME_MESSAGE -- seconds of thrust left in the
// stage, not a countdown of any particular burn. The old label read like a time-to.
// The circularisation phase uses the same name for the same field.
static const char *_lnchAsLabels[LNCH_AS2_NROWS] = {
    "T+AP", "THRTL", "Q", "MACH", "G", "STG.BRN", "\xCE\x94V.STG"
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
static const int16_t LNCH_AS2_RPANEL_X = CONTENT_W - LNCH_AS2_RPANEL_W;             // 580
static const int16_t LNCH_AS2_RPANEL_Y = LNCH_AS_PANEL_Y;                            // 63
static const int16_t LNCH_AS2_ROW_H    = (SCREEN_H - LNCH_AS_PANEL_Y) / LNCH_AS2_NROWS;  // 76
static const int16_t LNCH_AS2_RPANEL_H = LNCH_AS2_ROW_H * LNCH_AS2_NROWS;                 // 532
static const tFont  *LNCH_AS2_LBL_FONT = &Roboto_Black_28;
static const tFont  *LNCH_AS2_VAL_FONT = &Roboto_Black_36;
static inline int16_t _lnchAs2RowY(uint8_t row) {
    return LNCH_AS2_RPANEL_Y + row * LNCH_AS2_ROW_H;
}

// ── Shared LNCH right-panel readout plumbing ───────────────────────────────
// Both LNCH phases (Ascent + Circularization) render an identical 8-row numeric
// readout panel: same LNCH_AS2_* geometry, same Black_28 labels / Black_36
// values, same vertical divider, same 2-px horizontal group dividers. Only the
// row LABELS and the set of group-divider rows differ between the phases. These
// shared helpers hold the single implementation; each phase's public entry
// points delegate to them, passing their own label array, PrintState array, and
// divider-row list.
//
// Defined here in Screen_LNCH_Ascent.ino, which the Arduino build concatenates
// BEFORE Screen_LNCH_Circ.ino (alphabetical order), so these file-scope statics
// are visible to the Circularization phase's code as well.

// Shared right-panel chrome: vertical divider, one label cell per row, and the
// horizontal group dividers named in divRows. Font is LNCH_AS2_LBL_FONT
// (Roboto_Black_28) — identical in both phases.
static void _lnchAs2DrawPanelChrome(KCM_TFT &tft, const char *const *labels,
                                    const uint8_t *divRows, uint8_t divCount) {
    // Vertical divider in the 2-px gap before the right panel
    tft.drawLine(LNCH_AS2_RPANEL_X - 2, LNCH_AS2_RPANEL_Y,
                 LNCH_AS2_RPANEL_X - 2, LNCH_AS2_RPANEL_Y + LNCH_AS2_RPANEL_H - 1,
                 TFT_GREY);
    tft.drawLine(LNCH_AS2_RPANEL_X - 1, LNCH_AS2_RPANEL_Y,
                 LNCH_AS2_RPANEL_X - 1, LNCH_AS2_RPANEL_Y + LNCH_AS2_RPANEL_H - 1,
                 TFT_GREY);

    for (uint8_t i = 0; i < LNCH_AS2_NROWS; i++) {
        printDispChrome(tft, LNCH_AS2_LBL_FONT,
                        LNCH_AS2_RPANEL_X, _lnchAs2RowY(i),
                        LNCH_AS2_RPANEL_W, LNCH_AS2_ROW_H,
                        labels[i], COL_LABEL, TFT_BLACK, COL_NO_BDR);
    }

    // Horizontal group dividers — 2 px in TFT_GREY. Each divider sits in the
    // 2-px gap between row groups (at y=dy-1 and y=dy). These rows sit OUTSIDE
    // both adjacent rows' fillRect clear regions (printValue / printDispChrome
    // clear y0+1..y0+h-2 inclusive), so the divider can't be nibbled when a
    // value cell changes background colour (e.g. alarm state toggle).
    for (uint8_t i = 0; i < divCount; i++) {
        int16_t dy = _lnchAs2RowY(divRows[i]);
        tft.drawLine(LNCH_AS2_RPANEL_X, dy - 1,
                     LNCH_AS2_RPANEL_X + LNCH_AS2_RPANEL_W - 1, dy - 1,
                     TFT_GREY);
        tft.drawLine(LNCH_AS2_RPANEL_X, dy,
                     LNCH_AS2_RPANEL_X + LNCH_AS2_RPANEL_W - 1, dy,
                     TFT_GREY);
    }
}

static void _lnchAsDrawRightPanelChrome(KCM_TFT &tft) {
    // Groups: row 0 trajectory, row 1 the throttle setting, rows 2-4 air data
    // (Q / Mach / G rise and fall together through the ascent), rows 5-6 what is
    // left in the stage → dividers above rows 1, 2 and 5.
    static const uint8_t divRows[] = { 1, 2, 5 };
    _lnchAs2DrawPanelChrome(tft, _lnchAsLabels, divRows, sizeof(divRows));
}

// Shared: draw a value in a readout row using library printValue. The value is
// right-aligned in the cell; the label (already drawn by chrome) is used only
// for paramW calculation so the value region doesn't overlap the label. Font is
// LNCH_AS2_VAL_FONT (Roboto_Black_36) — identical in both LNCH phases.
static void _lnchAs2DrawRowValue(KCM_TFT &tft, uint8_t row, const String &val,
                                 uint16_t fg, uint16_t bg,
                                 const char *const *labels, PrintState *ps) {
    printValue(tft, LNCH_AS2_VAL_FONT,
               LNCH_AS2_RPANEL_X, _lnchAs2RowY(row),
               LNCH_AS2_RPANEL_W, LNCH_AS2_ROW_H,
               labels[row], val,
               fg, bg, TFT_BLACK,
               ps[row]);
}

// ── Shared LNCH row updaters ───────────────────────────────────────────────
// T+Ap and Stg.Brn use byte-identical threshold + formatter + suppress logic wherever
// they appear; only the target row index, label array, PrintState array, and
// change-detection caches differ, so those are passed as parameters.
//
// Alt and ApA had shared updaters here too. Both are gone: the circularisation phase
// replaced its readout column with the apsis tape, and on ascent both rows were dropped
// in favour of the altitude ladder's markers, so neither had a caller left.
//
// NOTE: Throttle and ΔV.Stg are deliberately NOT shared — they genuinely differ between
// the phases (Ascent flags a zero-throttle alarm while Circ treats coasting as normal;
// Ascent change-detects ΔV.Stg at tenths precision while Circ uses whole m/s). See
// their per-phase definitions.

static void _lnchAs2UpdateTimeToAp(KCM_TFT &tft, uint8_t row,
                                   const char *const *labels, PrintState *ps,
                                   int32_t &prevT, uint16_t &prevFg) {
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
        val = formatTimeCompact(ttAp);
    }

    if (iTtAp == prevT && fg == prevFg) return;

    _lnchAs2DrawRowValue(tft, row, val, fg, TFT_BLACK, labels, ps);
    prevT  = iTtAp;
    prevFg = fg;
}

static void _lnchAs2UpdateTBurn(KCM_TFT &tft, uint8_t row,
                                const char *const *labels, PrintState *ps,
                                int32_t &prevTB, uint16_t &prevFg, uint16_t &prevBg) {
    float tb = state.stageBurnTime;
    int32_t iTb = (int32_t)roundf(tb);

    uint16_t fg, bg;
    thresholdColor(tb,
                   LNCH_BURNTIME_ALARM_S, TFT_WHITE,  TFT_RED,
                   LNCH_BURNTIME_WARN_S,  TFT_YELLOW, TFT_BLACK,
                        TFT_DARK_GREEN, TFT_BLACK, fg, bg);

    if (iTb == prevTB && fg == prevFg && bg == prevBg) return;

    _lnchAs2DrawRowValue(tft, row, formatTimeCompact(tb), fg, bg, labels, ps);
    prevTB  = iTb;
    prevFg  = fg;
    prevBg  = bg;
}

// Per-phase wrapper: draws an Ascent readout value using the Ascent label and
// PrintState arrays. Kept so the non-shared updaters (V.Srf/V.Vrt/Thrtl/ΔV.Stg)
// call sites are unchanged.
static void _lnchAsDrawRowValue(KCM_TFT &tft, uint8_t row, const String &val,
                                 uint16_t fg, uint16_t bg) {
    _lnchAs2DrawRowValue(tft, row, val, fg, bg, _lnchAsLabels, _lnchAsPs);
}

// Update each row. Each checks its own change detection and returns early if
// unchanged. Order: Alt, ApA, T+Ap, V.Srf, V.Vrt, Throttle, T.Burn, ΔV.Stg.

// ── Readout column updaters ───────────────────────────────────────────────────────────
// Rows 0-4 of this column used to be Alt.SL, ApA, T+Ap, V.Srf and V.Vrt. Four of those
// five were the digits for a gauge on this same screen and now live on it; only T+Ap
// had no picture, and it stays. Q, Mach and G take the freed rows -- none of the three
// was anywhere on this screen, and all three are already in the telemetry.

// Row 0 — T+Ap. Shared with the circularisation phase.
static void _lnchAsUpdateTimeToAp(KCM_TFT &tft) {
    _lnchAs2UpdateTimeToAp(tft, 0, _lnchAsLabels, _lnchAsPs,
                           _lnchAsPrevTimeToAp, _lnchAsPrevTimeToApFg);
}

// Row 1 — throttle. White-on-red at zero: on an ascent a closed throttle is an event,
// not a resting state (the circularisation phase treats coasting as normal instead).
static void _lnchAsUpdateThrottle(KCM_TFT &tft) {
    uint8_t thrPct = (uint8_t)constrain(state.throttle * 100.0f, 0.0f, 100.0f);

    uint16_t fg, bg;
    if (thrPct == 0) { fg = TFT_WHITE;      bg = TFT_RED;   }
    else             { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }

    if ((int16_t)thrPct == _lnchAsPrevThrottle &&
        fg == _lnchAsPrevThrFg && bg == _lnchAsPrevThrBg) return;

    _lnchAsDrawRowValue(tft, 1, formatPerc(thrPct), fg, bg);
    _lnchAsPrevThrottle = thrPct;
    _lnchAsPrevThrFg = fg; _lnchAsPrevThrBg = bg;
}

// Row 2 — dynamic pressure. Yellow approaching the structural band, white-on-red past
// it: max Q is the one moment of an ascent where the airframe, not the trajectory, is
// the limit, and nothing else on either panel annunciates it.
static void _lnchAsUpdateQ(KCM_TFT &tft) {
    const float q = _lnchAsDynPressureKPa();
    const int32_t iQ = (int32_t)lroundf(q * 10.0f);          // tenths of a kPa

    const bool haveAir = (state.inAtmo && state.airDensity > 0.0f);

    uint16_t fg, bg;
    char buf[16];
    if (!haveAir) {
        strcpy(buf, "---");
        fg = TFT_DARK_GREY; bg = TFT_BLACK;
    } else {
        snprintf(buf, sizeof(buf), "%.1f kPa", q);
        if      (q >= LNCH_Q_ALARM_KPA) { fg = TFT_WHITE;      bg = TFT_RED;   }
        else if (q >= LNCH_Q_WARN_KPA)  { fg = TFT_YELLOW;     bg = TFT_BLACK; }
        else                            { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    }

    // Colours settled before the change test: deciding them from the thresholds and then
    // overriding for the placeholder meant an out-of-atmosphere frame compared a green
    // threshold colour against a stored grey one and repainted every single frame.
    if (iQ == _lnchAsPrevQ && fg == _lnchAsPrevQFg && bg == _lnchAsPrevQBg) return;

    _lnchAsDrawRowValue(tft, 2, String(buf), fg, bg);
    _lnchAsPrevQ = iQ; _lnchAsPrevQFg = fg; _lnchAsPrevQBg = bg;
}

// Row 3 — Mach. Dashed outside an atmosphere, where it means nothing.
static void _lnchAsUpdateMach(KCM_TFT &tft) {
    const bool inAir = state.inAtmo;
    const int32_t iM = inAir ? (int32_t)lroundf(state.machNumber * 100.0f) : -9999;
    if (iM == _lnchAsPrevMach) return;

    char buf[12];
    if (!inAir) strcpy(buf, "---");
    else        snprintf(buf, sizeof(buf), "%.2f", state.machNumber);
    _lnchAsDrawRowValue(tft, 3, String(buf),
                        inAir ? TFT_DARK_GREEN : TFT_DARK_GREY, TFT_BLACK);
    _lnchAsPrevMach = iM;
}

// Row 4 — load factor. Yellow then white-on-red on the crew limits the Annunciator
// already uses, so the two panels cannot disagree about what a high-G ascent is.
static void _lnchAsUpdateG(KCM_TFT &tft) {
    const float g = state.gForce;
    const int32_t iG = (int32_t)lroundf(g * 10.0f);

    uint16_t fg, bg;
    if      (g >= LNCH_G_ALARM) { fg = TFT_WHITE;      bg = TFT_RED;   }
    else if (g >= LNCH_G_WARN)  { fg = TFT_YELLOW;     bg = TFT_BLACK; }
    else                        { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }

    if (iG == _lnchAsPrevG && fg == _lnchAsPrevGFg && bg == _lnchAsPrevGBg) return;

    char buf[12];
    snprintf(buf, sizeof(buf), "%.1f", g);
    _lnchAsDrawRowValue(tft, 4, String(buf), fg, bg);
    _lnchAsPrevG = iG; _lnchAsPrevGFg = fg; _lnchAsPrevGBg = bg;
}

// Row 5 — stage endurance. Shared with the circularisation phase.
static void _lnchAsUpdateTBurn(KCM_TFT &tft) {
    _lnchAs2UpdateTBurn(tft, 5, _lnchAsLabels, _lnchAsPs,
                        _lnchAsPrevTBurn, _lnchAsPrevTBurnFg, _lnchAsPrevTBurnBg);
}

// Row 6 — stage delta-V.
static void _lnchAsUpdateDVStg(KCM_TFT &tft) {
    float dv = state.stageDeltaV;
    int32_t iDv = (int32_t)roundf(dv);

    uint16_t fg, bg;
    thresholdColor(dv,
                   DV_STG_ALARM_MS, TFT_WHITE,  TFT_RED,
                   DV_STG_WARN_MS,  TFT_YELLOW, TFT_BLACK,
                   TFT_DARK_GREEN, TFT_BLACK, fg, bg);

    if (iDv == _lnchAsPrevDVStg &&
        fg == _lnchAsPrevDVStgFg && bg == _lnchAsPrevDVStgBg) return;

    _lnchAsDrawRowValue(tft, 6, fmtMs(dv), fg, bg);
    _lnchAsPrevDVStg = iDv;
    _lnchAsPrevDVStgFg = fg; _lnchAsPrevDVStgBg = bg;
}

static void _lnchAsDrawRightPanelValues(KCM_TFT &tft) {
    _lnchAsUpdateTimeToAp(tft);
    _lnchAsUpdateThrottle(tft);
    _lnchAsUpdateQ(tft);
    _lnchAsUpdateMach(tft);
    _lnchAsUpdateG(tft);
    _lnchAsUpdateTBurn(tft);
    _lnchAsUpdateDVStg(tft);
}

