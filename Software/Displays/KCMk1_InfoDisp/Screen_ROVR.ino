/***************************************************************************************
   Screen_ROVR.ino -- Rover screen — chromeScreen_ROVR, drawScreen_ROVR
   Dedicated display for type_Rover vessels. contextScreen() routes here automatically.
   rowCache index: [9] (screen_ROVR = 9)

   Layout (800 × 480 content area, sidebar at x=720..799):
     LEFT COLUMN (x=0..148, 5 stacked boxes):
       V.Srf    — signed surface speed (m/s), 1 decimal, direction-colored
       EC%      — electric charge %, threshold-colored (green/yellow/red)
       BRAKES   — button: white on dark green when on, muted when off
       GEAR     — button: white on dark green when deployed, muted when up
       SAS      — button: navball-palette mode colors (STAB/PRO/RETR/...)

     THROTTLE BAR (x=149..184, full screen height):
       Binary on/off: FWD half green, REV half yellow, both black when neutral
       "FWD" in white and "REV" in dark grey stacked vertically in each half

     COMPASS (centered at 360, 302, R=165):
       Rotating ring with cardinal letters (N/E/S/W) and numeric labels (03/06/...)
       Fixed nose triangle at top, rover icon at centre
       Heading readout box above (boxed "XXX°" in Roboto_Black_28)
       Target bearing triangle inside ring when state.targetAvailable
       "Tgt:" label and formatted distance value in bottom corners outside
       the ring, shown only when state.targetAvailable

     RIGHT COLUMN (x=540..719, 3 stacked boxes):
       Elev     — elevation (altitude ASL - radarAlt AGL), formatted via
                  formatAlt (auto-scales m/km/Mm/Gm with thousands separator)
       Pitch    — side-view tilting silhouette + 1-decimal signed angle
       Roll     — rear-view tilting silhouette + 1-decimal signed angle
                  Silhouettes always drawn in chrome colors; numeric values
                  take threshold color (green/yellow/red per AAA_Config).

   Anti-flicker: each sub-element has its own dirty threshold and incremental
   update. Stationary chrome (borders, labels, ring, nose, rover icon, centre
   line) is drawn once in chromeScreen_ROVR and never redrawn.

****************************************************************************************/
#include "KCMk1_InfoDisp.h"


// ── Geometry ──────────────────────────────────────────────────────────────────────────
// Compass ring shrunk slightly from R=168 to R=165 to accommodate a taller heading
// readout box (needed for Roboto_Black_28, font_height=33). CY moved down to 302
// to maintain roughly equal margins above the heading box (12 px) and below the
// ring (13 px).
static const int16_t ROVR_CX             = 360;
static const int16_t ROVR_CY             = 302;
static const int16_t ROVR_R              = 165;   // outer ring radius
static const int16_t ROVR_R_TICK_OUTER   = 164;   // tick outer end (1 px inside outer ring)
                                                  //   so erase-in-black doesn't nibble ring
static const int16_t ROVR_R_TICK_INNER   = 147;   // major tick inner end  (scaled from 150)
static const int16_t ROVR_R_MINOR_INNER  = 153;   // minor tick inner end  (scaled from 156)
static const int16_t ROVR_R_LETTER       = 130;   // cardinal letter centre (scaled from 132)
static const int16_t ROVR_R_NUMLABEL     = 130;   // numeric label centre — same

// Vessel heading indicator — triangle ABOVE the ring at 12 o'clock, pointing INWARD
// (toward the compass center). Tip closer to centre, base farther out.
static const int16_t ROVR_NOSE_R_TIP     = 169;   // inward tip (4 px outside ring)
static const int16_t ROVR_NOSE_R_BASE    = 184;   // outward base
static const int16_t ROVR_NOSE_HALF_W    = 10;

// Target bearing indicator — triangle INSIDE the ring, positioned just inside
// the compass labels. Tip R=113 leaves ~4 px visual gap to the label inward
// glyph edge (~R=117 for Roboto_Black_24), with enough margin that the 6-px
// erase dilation doesn't overlap the labels when the target bearing aligns
// with a label position. Tip points OUTWARD (toward the ring); when target is
// at same heading as vessel, triangle sits at 12 o'clock with tip pointing up.
// Uses TFT_VIOLET to match target markers on SCFT/ACFT screens.
static const int16_t ROVR_TGT_R_TIP      = 113;   // outward tip
static const int16_t ROVR_TGT_R_BASE     = 98;    // inward base (15 px tall triangle)
static const int16_t ROVR_TGT_HALF_W     = 10;   // same half-width as nose

// Target distance readout — shown only when a target is selected (targetAvailable).
// Label "Tgt:" is flush to the bottom-left of the compass area (left-aligned,
// between the throttle bar and the ring), and the formatted distance value is
// flush to the bottom-right (right-aligned, between the ring and the right
// column). Both sit at the screen bottom edge, outside the compass ring.
// At y=451 the ring left edge is at x≈285 and right edge at x≈435, leaving
// room on each side for the text without encroaching on the ring.
static const int16_t ROVR_TGTD_LBL_X     = 185;
static const int16_t ROVR_TGTD_LBL_W     = 100;
static const int16_t ROVR_TGTD_VAL_X     = 440;
static const int16_t ROVR_TGTD_VAL_W     = 100;
static const int16_t ROVR_TGTD_Y         = 451;
static const int16_t ROVR_TGTD_H         = 28;    // hugs screen bottom (y=479)

// Heading readout — single-line boxed value above the nose triangle. The box
// border is stationary chrome; only the numeric value is redrawn on changes.
// Format: zero-padded 3-digit heading with ° symbol (e.g. "045°").
// Box is 38 tall to give the 28pt Roboto Black font (33 px font_height) room
// to render without the glyph cell's black background overwriting the border.
static const int16_t ROVR_HDG_BOX_W      = 80;   // widened slightly for the larger font
static const int16_t ROVR_HDG_BOX_H      = 38;
static const int16_t ROVR_HDG_BOX_X      = ROVR_CX - (ROVR_HDG_BOX_W / 2);
static const int16_t ROVR_HDG_BOX_Y      = ROVR_CY - ROVR_NOSE_R_BASE - 6 - ROVR_HDG_BOX_H;

// Rover silhouette (top-down view, stationary at compass centre — represents the
// vessel itself). Forward points up, matching the heading indicator at 12 o'clock.
//
//    ┌─────────┐
//    │  chassis │ ← front (with small notch)
//   [wh]       [wh]
//    │         │
//   [wh]       [wh]
//    └─────────┘
//
// All coordinates are offsets from (ROVR_CX, ROVR_CY).
static const int16_t ROVR_ICON_CHASS_W   = 44;   // chassis rect width
static const int16_t ROVR_ICON_CHASS_H   = 72;   // chassis rect height
static const int16_t ROVR_ICON_WHEEL_W   = 10;   // single wheel rect width
static const int16_t ROVR_ICON_WHEEL_H   = 20;   // single wheel rect height
static const int16_t ROVR_ICON_WHEEL_GAP = 10;   // wheel outer edge extends this far beyond chassis
static const int16_t ROVR_ICON_WHEEL_INSET = 6;  // wheel vertical inset from chassis top/bottom
static const int16_t ROVR_ICON_NOSE_W    = 16;   // nose notch base width (across chassis front)
static const int16_t ROVR_ICON_NOSE_H    = 10;   // nose notch height (extends above chassis)

// ── Throttle bar ──────────────────────────────────────────────────────────────────────
// Vertical bar positioned just left of the compass. Binary on/off fill:
// dark-green upper half when forward, yellow lower half when reverse, both
// black when neutral. "FWD" in white stacked vertically in the upper half,
// "REV" in dark grey in the lower half — both re-rendered with the matching
// background color on each state transition.
//
// Bar right edge is at x=193, giving 1 px clearance to the compass ring
// leftmost pixel at x=195, and 2 px clearance to the 9-o'clock major tick
// which extends to x=213 only when the ring rotates — the ring itself never
// crosses x=195.
static const int16_t ROVR_THR_W          = 36;    // bar width
static const int16_t ROVR_THR_X          = 149;   // bar left edge (directly against left column right edge at x=148)
static const int16_t ROVR_THR_Y_TOP      = 63;    // bar top edge (just below header)
static const int16_t ROVR_THR_Y_BOT      = 479;   // bar bottom edge (screen bottom)
static const int16_t ROVR_THR_Y_MID      = (ROVR_THR_Y_TOP + ROVR_THR_Y_BOT) / 2;
                                                  // centre line (neutral position, y=271)
static const int16_t ROVR_THR_HALF_H     = (ROVR_THR_Y_BOT - ROVR_THR_Y_TOP) / 2;
                                                  // half-height
static const float   ROVR_THR_THRESH     = 0.01f; // deadband — |wt| under this reads as neutral

// ── Left column: V.Srf / EC% / BRAKE / GEAR / SAS ─────────────────────────────────────
// Five stacked blocks filling the entire left column from just below the header
// (y=63) to the bottom of the screen (y=480). Total height 417 px, divided as:
//   V.Srf  85 px (slightly taller — gets the 2 leftover px after 5-way split)
//   EC%    83 px
//   BRAKE  83 px
//   GEAR   83 px
//   SAS    83 px
// Column x=0..148 (width 149) flush with the left edge of the screen. The
// throttle bar's left edge at x=149 is directly adjacent to the column's right
// edge at x=148, so their borders touch without overlap.
static const int16_t ROVR_LCOL_X         = 0;
static const int16_t ROVR_LCOL_W         = 149;
static const int16_t ROVR_LCOL_Y_TOP     = 63;    // just below header

static const int16_t ROVR_VSRF_H         = 85;
static const int16_t ROVR_EC_H           = 83;
static const int16_t ROVR_BRAKE_H        = 83;
static const int16_t ROVR_GEAR_H         = 83;
static const int16_t ROVR_SAS_H          = 83;

static const int16_t ROVR_VSRF_Y         = ROVR_LCOL_Y_TOP;                   // 63
static const int16_t ROVR_EC_Y           = ROVR_VSRF_Y  + ROVR_VSRF_H;        // 148
static const int16_t ROVR_BRAKE_Y        = ROVR_EC_Y    + ROVR_EC_H;          // 231
static const int16_t ROVR_GEAR_Y         = ROVR_BRAKE_Y + ROVR_BRAKE_H;       // 314
static const int16_t ROVR_SAS_Y          = ROVR_GEAR_Y  + ROVR_GEAR_H;        // 397
                                                                               //   end = 480

// Within V.Srf and EC% blocks: label on top, numeric value below. Both use the
// same internal layout metrics. textCenter is called with the block slot (x, y,
// w, h) so the two lines are centered within each block vertically.
static const int16_t ROVR_LBL_H          = 26;    // label strip height (Roboto_Black_20 fh=24)
static const int16_t ROVR_VAL_H          = 38;    // value strip height (Roboto_Black_28 fh=33)
static const int16_t ROVR_LBL_VAL_GAP    = 2;     // gap between label and value

// ── Right column: elevation / pitch / roll ───────────────────────────────────────────
// Three stacked boxes on the right side of the screen:
//   Elevation — surface ASL altitude (Alt.SL - Alt.Rdr), same block style as V.Srf
//   Pitch     — side-view tilt silhouette + numeric
//   Roll      — rear-view tilt silhouette + numeric
//
// Column x=540..719 (width 180), flush with the sidebar's left edge at x=720.
static const int16_t ROVR_RCOL_X         = 540;
static const int16_t ROVR_RCOL_W         = 180;
static const int16_t ROVR_ELEV_Y         = 63;
static const int16_t ROVR_ELEV_H         = 85;    // matches V.Srf block height
static const int16_t ROVR_PITCH_Y        = 148;   // adjacent to Elev bottom
static const int16_t ROVR_PITCH_H        = 166;
static const int16_t ROVR_ROLL_Y         = 314;   // adjacent to Pitch bottom
static const int16_t ROVR_ROLL_H         = 166;   // reaches screen bottom (y=479)

// Within each indicator box:
//   label at top (Roboto_Black_20, height 26)
//   silhouette area in middle (approx 130 px tall)
//   numeric value at bottom (Roboto_Black_28, height 38)
static const int16_t ROVR_TILT_LBL_H     = 26;
static const int16_t ROVR_TILT_VAL_H     = 38;
static const int16_t ROVR_TILT_TOP_PAD   = 4;
static const int16_t ROVR_TILT_GAP       = 2;

// Side-view silhouette dimensions (used for Pitch indicator) — base, unrotated.
// Wide low body with two round wheels beneath, like the side profile of a rover.
static const int16_t ROVR_SIL_BODY_W     = 80;    // pitch-view body width
static const int16_t ROVR_SIL_BODY_H     = 28;    // pitch-view body height
static const int16_t ROVR_SIL_WHEEL_R    = 10;    // pitch-view wheel radius (circle)
static const int16_t ROVR_SIL_WHEEL_DX   = 28;    // horiz offset of wheel centre from body centre
static const int16_t ROVR_SIL_WHEEL_DY   = 20;    // vert offset (below body centre)

// Rear-view silhouette dimensions (used for Roll indicator) — base, unrotated.
// Body height matches pitch silhouette body height (28) so the vehicle feels
// consistently sized across the two indicators. Body width is narrower (50)
// to convey rear-view perspective. Tires appear as rectangles on the SIDES of
// the body (visible tread face), with about half the tire height extending
// below the body.
static const int16_t ROVR_REAR_BODY_W    = 50;    // rear-view body width
static const int16_t ROVR_REAR_BODY_H    = 28;    // rear-view body height (matches pitch body)
static const int16_t ROVR_REAR_TIRE_W    = 10;    // each tire rect width (extends OUTWARD)
static const int16_t ROVR_REAR_TIRE_H    = 20;    // each tire rect height (≈pitch wheel dia)

// Dirty thresholds
static const float   ROVR_HDG_THRESH_DEG = 0.5f;  // compass rotates
static const float   ROVR_TGT_THRESH_DEG = 1.0f;  // target triangle screen position
static const float   ROVR_TILT_THRESH_DEG = 1.0f; // pitch/roll silhouette redraws

// ── State ─────────────────────────────────────────────────────────────────────────────
static float      _rovrPrevHeading      = -9999.0f;
static int16_t    _rovrPrevHdgVal       = -9999;    // last-drawn integer heading (readout)
static bool       _rovrPrevTgtAvail     = false;    // whether target triangle was last drawn
static float      _rovrPrevTgtScreenDeg = -9999.0f; // last-drawn target screen bearing
                                                    //   (targetHdg - vesselHdg wrapped to ±180°)
static int16_t    _rovrPrevThrFill      = -9999;    // last-drawn throttle state code
                                                    //   (+1 = forward, 0 = neutral, -1 = reverse)
static int16_t    _rovrPrevVSrf         = -9999;    // last-drawn signed speed in tenths m/s
                                                    //   (i.e. roundf(speed * 10.0f))
static int16_t    _rovrPrevEcPct        = -9999;    // last-drawn integer EC%
static uint16_t   _rovrPrevEcFg         = 0xFFFF;   // last-drawn EC% fg color (for threshold change)
static uint16_t   _rovrPrevEcBg         = 0xFFFF;   // last-drawn EC% bg color
static int8_t     _rovrPrevBrake        = -1;       // -1=never drawn, 0=off, 1=on
static int8_t     _rovrPrevGear         = -1;
static int16_t    _rovrPrevSasMode      = -1;       // -1=never drawn, else stored value
static float      _rovrPrevPitch        = -9999.0f; // last-drawn pitch angle
static float      _rovrPrevRoll         = -9999.0f; // last-drawn roll angle
static int16_t    _rovrPrevPitchVal     = -9999;    // last-drawn integer pitch (for readout)
static int16_t    _rovrPrevRollVal      = -9999;    // last-drawn integer roll (for readout)
static uint16_t   _rovrPrevPitchColor   = 0;        // last-drawn numeric-value color
static uint16_t   _rovrPrevRollColor    = 0;        // (silhouettes always drawn in chrome colors)
static int32_t    _rovrPrevElev         = -99999;   // last-drawn integer elevation (m ASL)
static bool       _rovrPrevTgtDistAvail  = false;    // whether the target-distance label/value were drawn
static int32_t    _rovrPrevTgtDistVal    = -1;       // last-drawn integer target distance in metres

// ── Shortest-arc delta helper ─────────────────────────────────────────────────────────
static inline float _rovrHdgDelta(float a, float b) {
    float d = a - b;
    while (d >  180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return d;
}

// ── Polar → screen conversion ─────────────────────────────────────────────────────────
//
// Compass screen angle: measured from 12 o'clock (up), increasing clockwise.
// A world-frame bearing `worldDeg` (N=0, E=90, S=180, W=270) appears on-compass at
// screenAngle = worldDeg - headingDeg. Screen coords:
//   x = cx + r * sin(screenDeg * π/180)
//   y = cy - r * cos(screenDeg * π/180)
static inline void _rovrPolar(float screenDeg, int16_t r, int16_t &x, int16_t &y) {
    float rad = screenDeg * (float)DEG_TO_RAD;
    x = (int16_t)(ROVR_CX + (float)r * sinf(rad));
    y = (int16_t)(ROVR_CY - (float)r * cosf(rad));
}

// ── Compass drawing ───────────────────────────────────────────────────────────────────

// Erase the compass bounding box before redrawing. fillRect is hardware-accelerated
// on the RA8875 so this is cheap. Bounds cover outer ring, tick marks, labels,
// nose triangle, and the heading readout box above the triangle.
static void _rovrEraseCompass(RA8875 &tft) {
    int16_t x0 = ROVR_CX - ROVR_R - 8;
    int16_t y0 = ROVR_HDG_BOX_Y - 4;               // reach above heading box
    int16_t x1 = ROVR_CX + ROVR_R + 8;
    int16_t y1 = ROVR_CY + ROVR_R + 8;
    tft.fillRect(x0, y0, x1 - x0, y1 - y0, TFT_BLACK);
}

// Tick marks every 5°; majors every 30°. Minors (all non-major positions) are
// drawn in LIGHT_GREY (same as majors) for visibility, but are shorter. When
// `erase` is true, all ticks are drawn in black to wipe the previous frame's
// ticks before drawing the new frame's ticks at the new heading.
static void _rovrDrawTicks(RA8875 &tft, float headingDeg, bool erase) {
    for (int16_t worldDeg = 0; worldDeg < 360; worldDeg += 5) {
        float screenDeg = (float)worldDeg - headingDeg;
        int16_t x0, y0, x1, y1;
        if (worldDeg % 30 == 0) {
            // Major tick — long, light grey
            _rovrPolar(screenDeg, ROVR_R_TICK_OUTER, x1, y1);
            _rovrPolar(screenDeg, ROVR_R_TICK_INNER, x0, y0);
            tft.drawLine(x0, y0, x1, y1, erase ? TFT_BLACK : TFT_LIGHT_GREY);
        } else {
            // Minor tick — short, TFT_GREY (dimmer than the light-grey majors)
            _rovrPolar(screenDeg, ROVR_R_TICK_OUTER,  x1, y1);
            _rovrPolar(screenDeg, ROVR_R_MINOR_INNER, x0, y0);
            tft.drawLine(x0, y0, x1, y1, erase ? TFT_BLACK : TFT_GREY);
        }
    }
}

// Cardinal letters (N/E/S/W) at world 0/90/180/270 and numeric labels at other 30°s.
// Text drawn with top-left cursor; glyph metrics estimated empirically (RA8875 has
// no text-metric API in this library). See inside the loop for per-font numbers.
//
// When `erase` is true, all labels are drawn in black with a black background —
// this wipes any glyph pixels from the previous draw at this angle.
static void _rovrDrawLabels(RA8875 &tft, float headingDeg, bool erase) {
    tft.setFont(&Roboto_Black_24);

    struct LabelSpec { int16_t worldDeg; const char *text; uint16_t color; int16_t r; };
    static const LabelSpec labels[] = {
        {   0, "N",  TFT_YELLOW,     ROVR_R_LETTER },
        {  30, "03", TFT_LIGHT_GREY, ROVR_R_NUMLABEL },
        {  60, "06", TFT_LIGHT_GREY, ROVR_R_NUMLABEL },
        {  90, "E",  TFT_WHITE,      ROVR_R_LETTER },
        { 120, "12", TFT_LIGHT_GREY, ROVR_R_NUMLABEL },
        { 150, "15", TFT_LIGHT_GREY, ROVR_R_NUMLABEL },
        { 180, "S",  TFT_WHITE,      ROVR_R_LETTER },
        { 210, "21", TFT_LIGHT_GREY, ROVR_R_NUMLABEL },
        { 240, "24", TFT_LIGHT_GREY, ROVR_R_NUMLABEL },
        { 270, "W",  TFT_WHITE,      ROVR_R_LETTER },
        { 300, "30", TFT_LIGHT_GREY, ROVR_R_NUMLABEL },
        { 330, "33", TFT_LIGHT_GREY, ROVR_R_NUMLABEL },
    };

    for (uint8_t i = 0; i < sizeof(labels) / sizeof(labels[0]); i++) {
        float screenDeg = (float)labels[i].worldDeg - headingDeg;
        int16_t x, y;
        _rovrPolar(screenDeg, labels[i].r, x, y);

        // Roboto_Black_24 metrics (empirical): glyph ~14 px wide, ~20 px tall
        uint8_t textLen = strlen(labels[i].text);
        int16_t textW = (int16_t)(textLen * 14);
        int16_t textH = 20;
        int16_t cursorX = x - textW / 2;
        int16_t cursorY = y - textH / 2;

        uint16_t fg = erase ? TFT_BLACK : labels[i].color;
        tft.setTextColor(fg, TFT_BLACK);
        tft.setCursor(cursorX, cursorY);
        tft.print(labels[i].text);
    }
}

// Vessel heading indicator — white triangle above the ring, pointing INWARD
// toward the compass centre. Marks the 12 o'clock position which always
// corresponds to vessel heading (ring rotates around it).
static void _rovrDrawNose(RA8875 &tft) {
    int16_t tipX, tipY, blX, blY, brX, brY;
    _rovrPolar(0.0f, ROVR_NOSE_R_TIP, tipX, tipY);
    float angOffset = (float)ROVR_NOSE_HALF_W / (float)ROVR_NOSE_R_BASE * (180.0f / (float)PI);
    _rovrPolar(-angOffset, ROVR_NOSE_R_BASE, blX, blY);
    _rovrPolar( angOffset, ROVR_NOSE_R_BASE, brX, brY);
    tft.fillTriangle(tipX, tipY, blX, blY, brX, brY, TFT_WHITE);
}

// Target bearing indicator — violet triangle inside the ring at the target's
// relative bearing on the compass (screenDeg = targetHeading - vesselHeading,
// wrapped to ±180°). Tip points outward (toward the ring); base points toward
// centre. When the target is dead ahead, screenDeg=0 and the triangle sits at
// 12 o'clock with its tip pointing up.
//
// When `erase` is true, a bounding rectangle around the triangle is filled in
// black rather than drawing a black triangle. This guarantees complete pixel
// coverage regardless of rasterizer edge-pixel rules (triangle-based erase
// with vertex dilation was leaving trails at certain angles). The bounding
// rect is padded a few px beyond the triangle extents for safety. The target
// triangle sits well inside the annular band with 25+ px clearance from both
// the rover icon and the compass labels, so over-painting with a bounding
// rect is safe.
static void _rovrDrawTargetAt(RA8875 &tft, float screenDeg, bool erase) {
    int16_t tipX, tipY, blX, blY, brX, brY;
    _rovrPolar(screenDeg, ROVR_TGT_R_TIP, tipX, tipY);
    float angOffset = (float)ROVR_TGT_HALF_W / (float)ROVR_TGT_R_BASE * (180.0f / (float)PI);
    _rovrPolar(screenDeg - angOffset, ROVR_TGT_R_BASE, blX, blY);
    _rovrPolar(screenDeg + angOffset, ROVR_TGT_R_BASE, brX, brY);

    if (erase) {
        // Bounding rect, padded 3 px on all sides. Guaranteed to cover the
        // drawn triangle completely, no rasterization edge issues.
        int16_t xMin = tipX;
        int16_t xMax = tipX;
        if (blX < xMin) xMin = blX;
        if (brX < xMin) xMin = brX;
        if (blX > xMax) xMax = blX;
        if (brX > xMax) xMax = brX;
        int16_t yMin = tipY;
        int16_t yMax = tipY;
        if (blY < yMin) yMin = blY;
        if (brY < yMin) yMin = brY;
        if (blY > yMax) yMax = blY;
        if (brY > yMax) yMax = brY;
        const int16_t pad = 3;
        tft.fillRect(xMin - pad, yMin - pad,
                     (xMax - xMin) + 2 * pad + 1,
                     (yMax - yMin) + 2 * pad + 1,
                     TFT_BLACK);
    } else {
        tft.fillTriangle(tipX, tipY, blX, blY, brX, brY, TFT_VIOLET);
    }
}

// Target triangle update — called every frame. Decides whether the target
// triangle needs to be erased, redrawn, or both. Independent of the main
// compass redraw because the target triangle sits in an annular band (R=100..
// 115) that doesn't overlap with the ticks/labels/ring, so it can update on
// its own cadence.
//
// Triggers:
//   - target availability toggled — erase old if was visible, draw new if now
//   - target screen-bearing changed ≥ ROVR_TGT_THRESH_DEG — erase old, draw new
static void _rovrUpdateTarget(RA8875 &tft) {
    if (!state.targetAvailable) {
        // Target is not available. If one was previously drawn, erase it.
        if (_rovrPrevTgtAvail) {
            _rovrDrawTargetAt(tft, _rovrPrevTgtScreenDeg, true);
            _rovrPrevTgtAvail = false;
        }
        return;
    }

    // Target IS available. Compute new screen bearing.
    float screenDeg = _rovrHdgDelta(state.tgtHeading, state.heading);

    if (!_rovrPrevTgtAvail) {
        // Target just appeared — draw fresh, no erase needed.
        _rovrDrawTargetAt(tft, screenDeg, false);
        _rovrPrevTgtAvail     = true;
        _rovrPrevTgtScreenDeg = screenDeg;
        return;
    }

    // Target was available and still is — redraw only if the screen position
    // changed meaningfully.
    float deltaScreenDeg = _rovrHdgDelta(screenDeg, _rovrPrevTgtScreenDeg);
    if (fabsf(deltaScreenDeg) < ROVR_TGT_THRESH_DEG) return;

    // Erase at the old screen position, draw at the new one.
    _rovrDrawTargetAt(tft, _rovrPrevTgtScreenDeg, true);
    _rovrDrawTargetAt(tft, screenDeg, false);
    _rovrPrevTgtScreenDeg = screenDeg;
}

// Target-distance readout — "Tgt:" label on bottom-left of the compass area,
// formatted distance value on bottom-right. Both are visible only when a target
// is selected; when no target, both strips are erased to black.
//
// Uses formatAlt() from the shared library for auto-scaled units (m/km/Mm/Gm)
// and thousands separators. Value is rendered in TFT_VIOLET to match the target
// triangle and the convention used on SCFT/ACFT target markers.
static void _rovrUpdateTgtDist(RA8875 &tft) {
    if (!state.targetAvailable) {
        // Target is not available. Erase the label/value if they were drawn.
        if (_rovrPrevTgtDistAvail) {
            tft.fillRect(ROVR_TGTD_LBL_X, ROVR_TGTD_Y,
                         ROVR_TGTD_LBL_W, ROVR_TGTD_H, TFT_BLACK);
            tft.fillRect(ROVR_TGTD_VAL_X, ROVR_TGTD_Y,
                         ROVR_TGTD_VAL_W, ROVR_TGTD_H, TFT_BLACK);
            _rovrPrevTgtDistAvail = false;
            _rovrPrevTgtDistVal   = -1;
        }
        return;
    }

    // Target IS available. Draw the label if this is the first frame since it
    // appeared (the label is static, so we only draw it once per availability
    // transition).
    if (!_rovrPrevTgtDistAvail) {
        textLeft(tft, &Roboto_Black_20,
                 ROVR_TGTD_LBL_X, ROVR_TGTD_Y,
                 ROVR_TGTD_LBL_W, ROVR_TGTD_H,
                 "Tgt:", TFT_WHITE, TFT_BLACK);
        _rovrPrevTgtDistAvail = true;
        _rovrPrevTgtDistVal   = -1;   // force value redraw below
    }

    // Distance value — redraw only on integer-metre change to match the
    // precision of formatAlt's output.
    int32_t iDist = (int32_t)roundf(state.tgtDistance);
    if (iDist == _rovrPrevTgtDistVal) return;

    // Erase previous value by fillRect over the value strip. Using fillRect
    // rather than text-overdraw because textRight's x position depends on
    // string length — the old and new strings may not cover the same pixels.
    if (_rovrPrevTgtDistVal >= 0) {
        tft.fillRect(ROVR_TGTD_VAL_X, ROVR_TGTD_Y,
                     ROVR_TGTD_VAL_W, ROVR_TGTD_H, TFT_BLACK);
    }

    String newStr = formatAlt((float)iDist);
    textRight(tft, &Roboto_Black_20,
              ROVR_TGTD_VAL_X, ROVR_TGTD_Y,
              ROVR_TGTD_VAL_W, ROVR_TGTD_H,
              newStr, TFT_VIOLET, TFT_BLACK);

    _rovrPrevTgtDistVal = iDist;
}

// Rover silhouette (top-down) at compass centre. Stationary — the rover always
// points "up" (12 o'clock) since it represents the vessel's own frame, and the
// ring rotates around it to show world-frame cardinal directions.
//
// Drawing order: chassis first, then wheels on top (wheels overhang the chassis
// sides slightly), then nose notch at the front.
static void _rovrDrawIcon(RA8875 &tft) {
    // Chassis — centred rectangle
    int16_t chX = ROVR_CX - ROVR_ICON_CHASS_W / 2;
    int16_t chY = ROVR_CY - ROVR_ICON_CHASS_H / 2;
    tft.fillRect(chX, chY, ROVR_ICON_CHASS_W, ROVR_ICON_CHASS_H, TFT_GREY);
    tft.drawRect(chX, chY, ROVR_ICON_CHASS_W, ROVR_ICON_CHASS_H, TFT_LIGHT_GREY);

    // Wheels — 4 rectangles at corners, extending ROVR_ICON_WHEEL_GAP beyond chassis
    // sides. Each wheel is inset vertically from the chassis top/bottom.
    // Wheel outer edges at chX - WHEEL_GAP and chX + CHASS_W + WHEEL_GAP - WHEEL_W.
    int16_t wLX = chX - ROVR_ICON_WHEEL_GAP;
    int16_t wRX = chX + ROVR_ICON_CHASS_W + ROVR_ICON_WHEEL_GAP - ROVR_ICON_WHEEL_W;
    int16_t wTY = chY + ROVR_ICON_WHEEL_INSET;
    int16_t wBY = chY + ROVR_ICON_CHASS_H - ROVR_ICON_WHEEL_INSET - ROVR_ICON_WHEEL_H;

    // Front-left
    tft.fillRect(wLX, wTY, ROVR_ICON_WHEEL_W, ROVR_ICON_WHEEL_H, TFT_DARK_GREY);
    tft.drawRect(wLX, wTY, ROVR_ICON_WHEEL_W, ROVR_ICON_WHEEL_H, TFT_GREY);
    // Front-right
    tft.fillRect(wRX, wTY, ROVR_ICON_WHEEL_W, ROVR_ICON_WHEEL_H, TFT_DARK_GREY);
    tft.drawRect(wRX, wTY, ROVR_ICON_WHEEL_W, ROVR_ICON_WHEEL_H, TFT_GREY);
    // Rear-left
    tft.fillRect(wLX, wBY, ROVR_ICON_WHEEL_W, ROVR_ICON_WHEEL_H, TFT_DARK_GREY);
    tft.drawRect(wLX, wBY, ROVR_ICON_WHEEL_W, ROVR_ICON_WHEEL_H, TFT_GREY);
    // Rear-right
    tft.fillRect(wRX, wBY, ROVR_ICON_WHEEL_W, ROVR_ICON_WHEEL_H, TFT_DARK_GREY);
    tft.drawRect(wRX, wBY, ROVR_ICON_WHEEL_W, ROVR_ICON_WHEEL_H, TFT_GREY);

    // Nose notch — small triangle at the front-centre, pointing forward (up).
    // Base sits on chassis top edge; tip extends ROVR_ICON_NOSE_H above it.
    int16_t noseTipX = ROVR_CX;
    int16_t noseTipY = chY - ROVR_ICON_NOSE_H;
    int16_t noseBLX  = ROVR_CX - ROVR_ICON_NOSE_W / 2;
    int16_t noseBLY  = chY;
    int16_t noseBRX  = ROVR_CX + ROVR_ICON_NOSE_W / 2;
    int16_t noseBRY  = chY;
    tft.fillTriangle(noseTipX, noseTipY, noseBLX, noseBLY, noseBRX, noseBRY, TFT_LIGHT_GREY);
}

// Throttle bar — vertical bar on the far left. Stationary frame drawn once in
// chrome (outer border + centre line). The fill region (dark green upward for
// forward, yellow downward for reverse) is updated separately via
// _rovrUpdateThrottle to avoid redrawing the frame every time throttle changes.
static void _rovrDrawThrottleFrame(RA8875 &tft) {
    // Outer border
    tft.drawRect(ROVR_THR_X, ROVR_THR_Y_TOP,
                 ROVR_THR_W, ROVR_THR_Y_BOT - ROVR_THR_Y_TOP + 1,
                 TFT_LIGHT_GREY);

    // Centre (neutral) line — a thin light-grey horizontal line at 0% throttle
    // position. Drawn 1 px inside the border to avoid touching it.
    tft.drawLine(ROVR_THR_X + 1, ROVR_THR_Y_MID,
                 ROVR_THR_X + ROVR_THR_W - 2, ROVR_THR_Y_MID,
                 TFT_LIGHT_GREY);
}

// Throttle fill update — binary on/off. Shows one of three states:
//   FWD (throttle > deadband)     — upper half dark-green, "FWD" white-on-green
//   REV (throttle < -deadband)    — lower half yellow,     "REV" dark-grey-on-yellow
//   NEUTRAL (|throttle| < deadband) — both halves black,   "FWD" / "REV" dim-on-black
//
// drawVerticalText fills its strip with backColor before drawing the text, so
// passing the same backColor as the half's fill color in a single call handles
// both the fill and the text in one operation per half. "FWD" stays white and
// "REV" stays dark grey regardless of throttle state (fg colors are constant).
//
// Only redraws on state transitions (throttle state changes). State code:
//   +1 = forward, 0 = neutral, -1 = reverse
static void _rovrUpdateThrottle(RA8875 &tft) {
    float wt = state.wheelThrottle;
    int16_t newState;
    if      (wt >  ROVR_THR_THRESH) newState = +1;
    else if (wt < -ROVR_THR_THRESH) newState = -1;
    else                            newState =  0;

    if (newState == _rovrPrevThrFill) return;

    // Interior region of the bar (inside the outer border, excluding the centre line)
    int16_t xL = ROVR_THR_X + 1;
    int16_t xW = ROVR_THR_W - 2;
    int16_t upperY = ROVR_THR_Y_TOP + 1;
    int16_t lowerY = ROVR_THR_Y_MID + 1;
    int16_t halfH  = ROVR_THR_HALF_H - 1;

    // Upper half ("FWD" in white) — backColor is dark green if forward, else black.
    uint16_t upperBg = (newState == +1) ? TFT_DARK_GREEN : TFT_BLACK;
    drawVerticalText(tft, xL, upperY, xW, halfH,
                     &Roboto_Black_20, "FWD",
                     TFT_WHITE, upperBg);

    // Lower half ("REV" in dark grey) — backColor is yellow if reverse, else black.
    uint16_t lowerBg = (newState == -1) ? TFT_YELLOW : TFT_BLACK;
    drawVerticalText(tft, xL, lowerY, xW, halfH,
                     &Roboto_Black_20, "REV",
                     TFT_DARK_GREY, lowerBg);

    _rovrPrevThrFill = newState;
}

// Surface velocity readout — "V.Srf:" label over signed speed (m/s) with 1 decimal.
// Label drawn once in chrome. Value updated via cached dirty check. Sign derived
// from comparing state.srfVelHeading to state.heading (>90° off-nose = reversing).
//
// The label and value are centered within the V.Srf block (y=ROVR_VSRF_Y, height
// ROVR_VSRF_H). Both strip rects are exactly `ROVR_LBL_H` and `ROVR_VAL_H` tall.
static inline int16_t _rovrVSrfLabelY() {
    int16_t totalContent = ROVR_LBL_H + ROVR_LBL_VAL_GAP + ROVR_VAL_H;
    return ROVR_VSRF_Y + (ROVR_VSRF_H - totalContent) / 2;
}
static inline int16_t _rovrVSrfValueY() {
    return _rovrVSrfLabelY() + ROVR_LBL_H + ROVR_LBL_VAL_GAP;
}

// Draw the stationary V.Srf chrome: bounding box border and label. Called once
// in chromeScreen_ROVR. The box border is drawn in light grey (matches the
// heading readout box on the compass).
static void _rovrDrawVSrfChrome(RA8875 &tft) {
    // Bounding box around the full V.Srf block
    tft.drawRect(ROVR_LCOL_X, ROVR_VSRF_Y,
                 ROVR_LCOL_W, ROVR_VSRF_H,
                 TFT_LIGHT_GREY);

    // Label centered in its strip
    textCenter(tft, &Roboto_Black_20,
               ROVR_LCOL_X, _rovrVSrfLabelY(),
               ROVR_LCOL_W, ROVR_LBL_H,
               "V.Srf:", TFT_WHITE, TFT_BLACK);
}

static void _rovrUpdateVSrf(RA8875 &tft) {
    // Magnitude (may be signed from demo mode; treat as unsigned magnitude)
    float mag = fabsf(state.surfaceVel);
    float speed;
    if (mag < 0.05f) {
        speed = 0.0f;   // below 1-decimal visibility
    } else {
        float d = _rovrHdgDelta(state.srfVelHeading, state.heading);
        speed = (fabsf(d) > 90.0f) ? -mag : mag;
    }

    // Clamp to ±99.9 m/s so the displayed value always fits the column width
    // ("+99.9 m/s" is the widest renderable string).
    if (speed >  99.9f) speed =  99.9f;
    if (speed < -99.9f) speed = -99.9f;

    // Cache by tenths (integer rounding of speed * 10). This triggers redraw
    // whenever the displayed 1-decimal value changes.
    int16_t iSpeed = (int16_t)roundf(speed * 10.0f);
    if (iSpeed == _rovrPrevVSrf) return;

    int16_t valueY = _rovrVSrfValueY();

    // Erase previous value (black-on-black)
    if (_rovrPrevVSrf > -9000) {
        char oldBuf[16];
        snprintf(oldBuf, sizeof(oldBuf), "%+.1f m/s", (float)_rovrPrevVSrf / 10.0f);
        textCenter(tft, &Roboto_Black_28,
                   ROVR_LCOL_X, valueY,
                   ROVR_LCOL_W, ROVR_VAL_H,
                   oldBuf, TFT_BLACK, TFT_BLACK);
    }

    // Color: green forward, yellow reverse, grey neutral
    uint16_t fg;
    if      (iSpeed >  0) fg = TFT_DARK_GREEN;
    else if (iSpeed <  0) fg = TFT_YELLOW;
    else                   fg = TFT_DARK_GREY;

    char buf[16];
    snprintf(buf, sizeof(buf), "%+.1f m/s", speed);
    textCenter(tft, &Roboto_Black_28,
               ROVR_LCOL_X, valueY,
               ROVR_LCOL_W, ROVR_VAL_H,
               buf, fg, TFT_BLACK);

    _rovrPrevVSrf = iSpeed;
}

// ── EC% readout ───────────────────────────────────────────────────────────────────────
// "EC%:" label on top, integer percentage below with threshold-based color.
// Using ROVER_EC_WARN_PCT / ROVER_EC_ALARM_PCT from AAA_Config.ino:
//   < ALARM (25%): WHITE-on-RED (critical)
//   < WARN (50%):  YELLOW-on-BLACK
//   otherwise:     DARK_GREEN-on-BLACK
static inline int16_t _rovrEcLabelY() {
    int16_t totalContent = ROVR_LBL_H + ROVR_LBL_VAL_GAP + ROVR_VAL_H;
    return ROVR_EC_Y + (ROVR_EC_H - totalContent) / 2;
}
static inline int16_t _rovrEcValueY() {
    return _rovrEcLabelY() + ROVR_LBL_H + ROVR_LBL_VAL_GAP;
}

// Draw the stationary EC% chrome: bounding box border and label.
static void _rovrDrawEcChrome(RA8875 &tft) {
    tft.drawRect(ROVR_LCOL_X, ROVR_EC_Y,
                 ROVR_LCOL_W, ROVR_EC_H,
                 TFT_LIGHT_GREY);

    textCenter(tft, &Roboto_Black_20,
               ROVR_LCOL_X, _rovrEcLabelY(),
               ROVR_LCOL_W, ROVR_LBL_H,
               "EC%:", TFT_WHITE, TFT_BLACK);
}

static void _rovrUpdateEc(RA8875 &tft) {
    float ec = state.electricChargePercent;
    if (ec < 0.0f) ec = 0.0f; else if (ec > 100.0f) ec = 100.0f;
    int16_t iEc = (int16_t)roundf(ec);

    // Compute threshold colors
    uint16_t fg, bg;
    if      (ec < ROVER_EC_ALARM_PCT) { fg = TFT_WHITE;      bg = TFT_RED;   }
    else if (ec < ROVER_EC_WARN_PCT)  { fg = TFT_YELLOW;     bg = TFT_BLACK; }
    else                               { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }

    if (iEc == _rovrPrevEcPct && fg == _rovrPrevEcFg && bg == _rovrPrevEcBg) return;

    int16_t valueY = _rovrEcValueY();

    // Erase previous value using its own previous bg color (important: if the
    // threshold transitioned into/out of the RED alarm state, the bg is different)
    if (_rovrPrevEcPct > -9000) {
        char oldBuf[8];
        snprintf(oldBuf, sizeof(oldBuf), "%d%%", _rovrPrevEcPct);
        textCenter(tft, &Roboto_Black_28,
                   ROVR_LCOL_X, valueY,
                   ROVR_LCOL_W, ROVR_VAL_H,
                   oldBuf, _rovrPrevEcBg, _rovrPrevEcBg);
    }

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", iEc);
    textCenter(tft, &Roboto_Black_28,
               ROVR_LCOL_X, valueY,
               ROVR_LCOL_W, ROVR_VAL_H,
               buf, fg, bg);

    _rovrPrevEcPct = iEc;
    _rovrPrevEcFg  = fg;
    _rovrPrevEcBg  = bg;
}

// ── BRAKE / GEAR / SAS buttons ────────────────────────────────────────────────────────
// Button-style indicators using drawButton(). Each button fills its entire block
// slot — no padding — so adjacent buttons share borders for a tight stack.
// Uses Roboto_Black_24 to match the compass labels. SAS mode labels use the
// abbreviated short-form convention shared with SCFT and ACFT (STAB / PRO / RETR /
// NRM / ANRM / RAD+ / RAD- / TGT / ATGT / MNVR / SAS).

static void _rovrUpdateBrake(RA8875 &tft) {
    int8_t newState = state.brakes_on ? 1 : 0;
    if (newState == _rovrPrevBrake) return;

    ButtonLabel btn = state.brakes_on
        ? ButtonLabel{ "BRAKES", TFT_WHITE,     TFT_WHITE,     TFT_DARK_GREEN, TFT_DARK_GREEN, TFT_LIGHT_GREY, TFT_LIGHT_GREY }
        : ButtonLabel{ "BRAKES", TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK,  TFT_OFF_BLACK,  TFT_LIGHT_GREY, TFT_LIGHT_GREY };
    drawButton(tft,
               ROVR_LCOL_X, ROVR_BRAKE_Y,
               ROVR_LCOL_W, ROVR_BRAKE_H,
               btn, &Roboto_Black_24, false);

    _rovrPrevBrake = newState;
}

static void _rovrUpdateGear(RA8875 &tft) {
    int8_t newState = state.gear_on ? 1 : 0;
    if (newState == _rovrPrevGear) return;

    // Rover wheels are deployed (gear_on) for normal driving — so "GEAR DOWN"
    // is the safe/active state (green bg), "GEAR UP" is unusual (muted).
    ButtonLabel btn = state.gear_on
        ? ButtonLabel{ "GEAR", TFT_WHITE,     TFT_WHITE,     TFT_DARK_GREEN, TFT_DARK_GREEN, TFT_LIGHT_GREY, TFT_LIGHT_GREY }
        : ButtonLabel{ "GEAR", TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK,  TFT_OFF_BLACK,  TFT_LIGHT_GREY, TFT_LIGHT_GREY };
    drawButton(tft,
               ROVR_LCOL_X, ROVR_GEAR_Y,
               ROVR_LCOL_W, ROVR_GEAR_H,
               btn, &Roboto_Black_24, false);

    _rovrPrevGear = newState;
}

// SAS colors follow the navball palette (matches SCFT/ACFT usage):
//   OFF        — dark grey on off-black (muted, expected state for rover driving)
//   STAB (0)   — white on dark green
//   PRO (1)    — dark grey on neon green
//   RETR (2)   — dark grey on neon green
//   NRM (3)    — white on magenta
//   ANRM (4)   — white on magenta
//   RAD+ (5)   — dark grey on sky
//   RAD- (6)   — dark grey on sky
//   TGT (7)    — white on violet
//   ATGT (8)   — white on violet
//   MNVR (9)   — white on blue
static void _rovrUpdateSas(RA8875 &tft) {
    int16_t newMode = (int16_t)state.sasMode;
    if (newMode == _rovrPrevSasMode) return;

    const char *v; uint16_t fg, bg;
    switch (state.sasMode) {
        case 255: v = "SAS";  fg = TFT_DARK_GREY; bg = TFT_OFF_BLACK;  break;
        case 0:   v = "STAB"; fg = TFT_WHITE;     bg = TFT_DARK_GREEN; break;
        case 1:   v = "PRO";  fg = TFT_DARK_GREY; bg = TFT_NEON_GREEN; break;
        case 2:   v = "RETR"; fg = TFT_DARK_GREY; bg = TFT_NEON_GREEN; break;
        case 3:   v = "NRM";  fg = TFT_WHITE;     bg = TFT_MAGENTA;    break;
        case 4:   v = "ANRM"; fg = TFT_WHITE;     bg = TFT_MAGENTA;    break;
        case 5:   v = "RAD+"; fg = TFT_DARK_GREY; bg = TFT_SKY;        break;
        case 6:   v = "RAD-"; fg = TFT_DARK_GREY; bg = TFT_SKY;        break;
        case 7:   v = "TGT";  fg = TFT_WHITE;     bg = TFT_VIOLET;     break;
        case 8:   v = "ATGT"; fg = TFT_WHITE;     bg = TFT_VIOLET;     break;
        case 9:   v = "MNVR"; fg = TFT_WHITE;     bg = TFT_BLUE;       break;
        default:  v = "SAS";  fg = TFT_DARK_GREY; bg = TFT_OFF_BLACK;  break;
    }

    ButtonLabel btn = { v, fg, fg, bg, bg, TFT_LIGHT_GREY, TFT_LIGHT_GREY };
    drawButton(tft,
               ROVR_LCOL_X, ROVR_SAS_Y,
               ROVR_LCOL_W, ROVR_SAS_H,
               btn, &Roboto_Black_24, false);

    _rovrPrevSasMode = newMode;
}

// ── Elevation readout ─────────────────────────────────────────────────────────────────
// "Elev:" label over integer surface-altitude value in meters. Elevation is
// computed as (altitude_ASL - radarAlt_AGL), giving the altitude of the terrain
// surface below the vessel — i.e. how high up the current terrain is above
// sea level. Same block style as V.Srf on the left side.
//
// Clamped to ≥ 0 (negative values would indicate telemetry anomalies, not real
// terrain below sea level, and KSP bodies don't have meaningful negative
// elevation in the rover-driving domain).
static inline int16_t _rovrElevLabelY() {
    int16_t totalContent = ROVR_LBL_H + ROVR_LBL_VAL_GAP + ROVR_VAL_H;
    return ROVR_ELEV_Y + (ROVR_ELEV_H - totalContent) / 2;
}
static inline int16_t _rovrElevValueY() {
    return _rovrElevLabelY() + ROVR_LBL_H + ROVR_LBL_VAL_GAP;
}

static void _rovrDrawElevChrome(RA8875 &tft) {
    tft.drawRect(ROVR_RCOL_X, ROVR_ELEV_Y,
                 ROVR_RCOL_W, ROVR_ELEV_H,
                 TFT_LIGHT_GREY);
    textCenter(tft, &Roboto_Black_20,
               ROVR_RCOL_X, _rovrElevLabelY(),
               ROVR_RCOL_W, ROVR_LBL_H,
               "Elev:", TFT_WHITE, TFT_BLACK);
}

static void _rovrUpdateElev(RA8875 &tft) {
    // Elevation = surface altitude above sea level = altitude (ASL) - radarAlt (AGL).
    // formatAlt() handles the sign and auto-scales units (m/km/Mm/Gm) for large values.
    float elev = state.altitude - state.radarAlt;
    int32_t iElev = (int32_t)roundf(elev);

    if (iElev == _rovrPrevElev) return;

    int16_t valueY = _rovrElevValueY();

    // Erase previous value by regenerating its formatted string from the cached int
    // and drawing it black-on-black.
    if (_rovrPrevElev > -90000) {
        String oldStr = formatAlt((float)_rovrPrevElev);
        textCenter(tft, &Roboto_Black_28,
                   ROVR_RCOL_X, valueY,
                   ROVR_RCOL_W, ROVR_VAL_H,
                   oldStr, TFT_BLACK, TFT_BLACK);
    }

    String newStr = formatAlt((float)iElev);
    textCenter(tft, &Roboto_Black_28,
               ROVR_RCOL_X, valueY,
               ROVR_RCOL_W, ROVR_VAL_H,
               newStr, TFT_DARK_GREEN, TFT_BLACK);

    _rovrPrevElev = iElev;
}


// ── Pitch / Roll tilt indicators ──────────────────────────────────────────────────────
//
// Two independent rover silhouettes that tilt with vessel attitude:
//   Pitch — side-view profile (wide body, round wheels below) rotated by -pitch.
//           Conventionally: nose up = right side of silhouette up on screen.
//   Roll  — rear-view profile (narrow tall body, rectangular tires on the sides)
//           rotated by +roll. Conventionally: bank right = right side down.
//
// The silhouettes themselves are always drawn in chrome colors (TFT_GREY body,
// TFT_DARK_GREY wheels/tires). Threshold state (good/warn/alarm) shows in the
// numeric value color below the silhouette:
//   normal  → TFT_DARK_GREEN
//   warn    → TFT_YELLOW
//   alarm   → TFT_RED
// This keeps the silhouette visually stable under all conditions while the
// text carries the warning semantics.


// Pitch silhouette — side-view rover profile (wide body, round wheels below).
// Colors match the compass-center rover icon: TFT_GREY body with
// TFT_LIGHT_GREY outline, TFT_DARK_GREY wheels with TFT_GREY outline.
// Sign convention: positive angle rotates silhouette counter-clockwise on
// screen (y-down coords: CCW = right-side-up). Caller passes -pitch for the
// conventional "nose up = right side up" orientation.
static void _rovrDrawPitchSilhouette(RA8875 &tft, int16_t cx, int16_t cy,
                                     float angleDeg) {
    float a = angleDeg * (float)DEG_TO_RAD;
    float ca = cosf(a);
    float sa = sinf(a);

    auto rot = [&](float px, float py, int16_t &outX, int16_t &outY) {
        float rx = px * ca - py * sa;
        float ry = px * sa + py * ca;
        outX = cx + (int16_t)roundf(rx);
        outY = cy + (int16_t)roundf(ry);
    };

    // Wheels first (drawn below body so body draws on top near overlap)
    int16_t wL_X, wL_Y, wR_X, wR_Y;
    rot(-(float)ROVR_SIL_WHEEL_DX, (float)ROVR_SIL_WHEEL_DY, wL_X, wL_Y);
    rot( (float)ROVR_SIL_WHEEL_DX, (float)ROVR_SIL_WHEEL_DY, wR_X, wR_Y);
    tft.fillCircle(wL_X, wL_Y, ROVR_SIL_WHEEL_R, TFT_DARK_GREY);
    tft.drawCircle(wL_X, wL_Y, ROVR_SIL_WHEEL_R, TFT_GREY);
    tft.fillCircle(wR_X, wR_Y, ROVR_SIL_WHEEL_R, TFT_DARK_GREY);
    tft.drawCircle(wR_X, wR_Y, ROVR_SIL_WHEEL_R, TFT_GREY);

    // Body rectangle — drawn as two filled triangles (fill), then edge lines (outline)
    int16_t halfW = ROVR_SIL_BODY_W / 2;
    int16_t halfH = ROVR_SIL_BODY_H / 2;
    int16_t tlX, tlY, trX, trY, brX, brY, blX, blY;
    rot(-halfW, -halfH, tlX, tlY);
    rot( halfW, -halfH, trX, trY);
    rot( halfW,  halfH, brX, brY);
    rot(-halfW,  halfH, blX, blY);
    tft.fillTriangle(tlX, tlY, trX, trY, brX, brY, TFT_GREY);
    tft.fillTriangle(tlX, tlY, brX, brY, blX, blY, TFT_GREY);
    // Outline the body (matching compass icon chassis outline)
    tft.drawLine(tlX, tlY, trX, trY, TFT_LIGHT_GREY);
    tft.drawLine(trX, trY, brX, brY, TFT_LIGHT_GREY);
    tft.drawLine(brX, brY, blX, blY, TFT_LIGHT_GREY);
    tft.drawLine(blX, blY, tlX, tlY, TFT_LIGHT_GREY);
}

// Roll silhouette — rear-view rover profile: body matching pitch body height,
// narrower width, with rectangular tires on the sides (visible tread face).
// Tires are centered on the body's bottom edge so about half of each tire
// extends below the body.
//
// Colors match the compass-center rover icon: TFT_GREY body with TFT_LIGHT_GREY
// outline, TFT_DARK_GREY tires with TFT_GREY outline.
//
// Unrotated layout (origin at silhouette centre):
//      ┌──────────┐    body: W=50, H=28 (same H as pitch body)
//      │          │
//    ┌─┤          ├─┐  tires: W=10, H=20, centered on body bottom edge
//    │ │          │ │
//    └─┴──────────┴─┘
//
// Sign convention: positive angle = bank right = right side goes down on screen.
// Caller passes +roll directly.
static void _rovrDrawRollSilhouette(RA8875 &tft, int16_t cx, int16_t cy,
                                    float angleDeg) {
    float a = angleDeg * (float)DEG_TO_RAD;
    float ca = cosf(a);
    float sa = sinf(a);

    auto rot = [&](float px, float py, int16_t &outX, int16_t &outY) {
        float rx = px * ca - py * sa;
        float ry = px * sa + py * ca;
        outX = cx + (int16_t)roundf(rx);
        outY = cy + (int16_t)roundf(ry);
    };

    int16_t halfBW = ROVR_REAR_BODY_W / 2;
    int16_t halfBH = ROVR_REAR_BODY_H / 2;
    int16_t halfTH = ROVR_REAR_TIRE_H / 2;

    // Tires first (drawn behind body edges where they overlap).
    // Left tire: x range [-halfBW - TIRE_W, -halfBW], y centered on body bottom
    // so y range is [halfBH - halfTH, halfBH + halfTH].
    auto drawTire = [&](float xL, float xR) {
        float yT = (float)(halfBH - halfTH);
        float yB = (float)(halfBH + halfTH);
        int16_t tlX, tlY, trX, trY, brX, brY, blX, blY;
        rot(xL, yT, tlX, tlY);
        rot(xR, yT, trX, trY);
        rot(xR, yB, brX, brY);
        rot(xL, yB, blX, blY);
        tft.fillTriangle(tlX, tlY, trX, trY, brX, brY, TFT_DARK_GREY);
        tft.fillTriangle(tlX, tlY, brX, brY, blX, blY, TFT_DARK_GREY);
        tft.drawLine(tlX, tlY, trX, trY, TFT_GREY);
        tft.drawLine(trX, trY, brX, brY, TFT_GREY);
        tft.drawLine(brX, brY, blX, blY, TFT_GREY);
        tft.drawLine(blX, blY, tlX, tlY, TFT_GREY);
    };
    drawTire((float)(-halfBW - ROVR_REAR_TIRE_W), (float)(-halfBW));
    drawTire((float)halfBW, (float)(halfBW + ROVR_REAR_TIRE_W));

    // Body rectangle on top
    int16_t btlX, btlY, btrX, btrY, bbrX, bbrY, bblX, bblY;
    rot(-halfBW, -halfBH, btlX, btlY);
    rot( halfBW, -halfBH, btrX, btrY);
    rot( halfBW,  halfBH, bbrX, bbrY);
    rot(-halfBW,  halfBH, bblX, bblY);
    tft.fillTriangle(btlX, btlY, btrX, btrY, bbrX, bbrY, TFT_GREY);
    tft.fillTriangle(btlX, btlY, bbrX, bbrY, bblX, bblY, TFT_GREY);
    tft.drawLine(btlX, btlY, btrX, btrY, TFT_LIGHT_GREY);
    tft.drawLine(btrX, btrY, bbrX, bbrY, TFT_LIGHT_GREY);
    tft.drawLine(bbrX, bbrY, bblX, bblY, TFT_LIGHT_GREY);
    tft.drawLine(bblX, bblY, btlX, btlY, TFT_LIGHT_GREY);
}

// Erase a previously-drawn silhouette by fillRect over a conservative bounding
// square. The pitch and roll silhouettes have different extents, but using the
// larger of the two guarantees coverage for either.
static void _rovrEraseSilhouette(RA8875 &tft, int16_t cx, int16_t cy) {
    // Pitch silhouette extent: wheel extreme at sqrt(WHEEL_DX² + (WHEEL_DY+WHEEL_R)²)
    float pHalfW  = (float)ROVR_SIL_BODY_W / 2.0f;
    float pHalfH  = (float)ROVR_SIL_BODY_H / 2.0f;
    float pBodyR  = sqrtf(pHalfW*pHalfW + pHalfH*pHalfH);
    float pWheelR = sqrtf((float)(ROVR_SIL_WHEEL_DX*ROVR_SIL_WHEEL_DX) +
                          (float)((ROVR_SIL_WHEEL_DY+ROVR_SIL_WHEEL_R) *
                                  (ROVR_SIL_WHEEL_DY+ROVR_SIL_WHEEL_R)));
    float pitchR = (pBodyR > pWheelR) ? pBodyR : pWheelR;

    // Roll silhouette extent: tire outer-bottom corner is farthest point.
    // Tire now centered on body bottom edge, so bottom extends to halfBH + halfTH.
    float rHalfOuterX = (float)(ROVR_REAR_BODY_W/2 + ROVR_REAR_TIRE_W);
    float rHalfOuterY = (float)(ROVR_REAR_BODY_H/2 + ROVR_REAR_TIRE_H/2);
    float rollR = sqrtf(rHalfOuterX*rHalfOuterX + rHalfOuterY*rHalfOuterY);

    float maxR = (pitchR > rollR) ? pitchR : rollR;
    int16_t R = (int16_t)ceilf(maxR + 2.0f);
    tft.fillRect(cx - R, cy - R, 2 * R, 2 * R, TFT_BLACK);
}

// Choose numeric-value color based on angle magnitude and threshold config.
//   below warn  → dark green (good)
//   warn..alarm → yellow
//   >= alarm    → red
static uint16_t _rovrTiltValueColor(float angleDeg, float warnDeg, float alarmDeg) {
    float a = fabsf(angleDeg);
    if      (a >= alarmDeg) return TFT_RED;
    else if (a >= warnDeg)  return TFT_YELLOW;
    else                     return TFT_DARK_GREEN;
}

// Silhouette layout within each tilt box:
//   top_pad(4) + label(26) + gap(2) + silhouette_area + value(38) + bot_pad(4) = box_H
// Silhouette area height is (box_H - 74); silhouette centre is in the middle of that.
static const int16_t ROVR_PITCH_SIL_AREA_TOP = ROVR_PITCH_Y + ROVR_TILT_TOP_PAD +
                                               ROVR_TILT_LBL_H + ROVR_TILT_GAP;
static const int16_t ROVR_PITCH_SIL_AREA_H   = ROVR_PITCH_H - 2 * ROVR_TILT_TOP_PAD -
                                               ROVR_TILT_LBL_H - ROVR_TILT_GAP -
                                               ROVR_TILT_VAL_H;
static const int16_t ROVR_PITCH_SIL_CY = ROVR_PITCH_SIL_AREA_TOP + ROVR_PITCH_SIL_AREA_H / 2;
static const int16_t ROVR_PITCH_SIL_CX = ROVR_RCOL_X + ROVR_RCOL_W / 2;
static const int16_t ROVR_PITCH_VAL_Y  = ROVR_PITCH_Y + ROVR_PITCH_H -
                                         ROVR_TILT_TOP_PAD - ROVR_TILT_VAL_H;

static const int16_t ROVR_ROLL_SIL_AREA_TOP  = ROVR_ROLL_Y + ROVR_TILT_TOP_PAD +
                                               ROVR_TILT_LBL_H + ROVR_TILT_GAP;
static const int16_t ROVR_ROLL_SIL_AREA_H    = ROVR_ROLL_H - 2 * ROVR_TILT_TOP_PAD -
                                               ROVR_TILT_LBL_H - ROVR_TILT_GAP -
                                               ROVR_TILT_VAL_H;
static const int16_t ROVR_ROLL_SIL_CY  = ROVR_ROLL_SIL_AREA_TOP + ROVR_ROLL_SIL_AREA_H / 2;
static const int16_t ROVR_ROLL_SIL_CX  = ROVR_RCOL_X + ROVR_RCOL_W / 2;
static const int16_t ROVR_ROLL_VAL_Y   = ROVR_ROLL_Y + ROVR_ROLL_H -
                                         ROVR_TILT_TOP_PAD - ROVR_TILT_VAL_H;

// Horizontal reference line dimensions — short ticks on left and right of the
// silhouette at its vertical centre, providing a visual "true horizontal"
// reference to compare the tilted silhouette against.
static const int16_t ROVR_TILT_REF_MARGIN = 4;    // inset from box inner edge
static const int16_t ROVR_TILT_REF_LEN    = 20;   // length of each reference line

static void _rovrDrawPitchChrome(RA8875 &tft) {
    tft.drawRect(ROVR_RCOL_X, ROVR_PITCH_Y,
                 ROVR_RCOL_W, ROVR_PITCH_H,
                 TFT_LIGHT_GREY);
    textCenter(tft, &Roboto_Black_20,
               ROVR_RCOL_X, ROVR_PITCH_Y + ROVR_TILT_TOP_PAD,
               ROVR_RCOL_W, ROVR_TILT_LBL_H,
               "Pitch:", TFT_WHITE, TFT_BLACK);

    // Horizontal reference lines at silhouette centre Y, inside the box edges
    int16_t refY  = ROVR_PITCH_SIL_CY;
    int16_t lxL   = ROVR_RCOL_X + ROVR_TILT_REF_MARGIN;
    int16_t lxR   = lxL + ROVR_TILT_REF_LEN - 1;
    int16_t rxR   = ROVR_RCOL_X + ROVR_RCOL_W - 1 - ROVR_TILT_REF_MARGIN;
    int16_t rxL   = rxR - ROVR_TILT_REF_LEN + 1;
    tft.drawLine(lxL, refY, lxR, refY, TFT_LIGHT_GREY);
    tft.drawLine(rxL, refY, rxR, refY, TFT_LIGHT_GREY);
}

static void _rovrDrawRollChrome(RA8875 &tft) {
    tft.drawRect(ROVR_RCOL_X, ROVR_ROLL_Y,
                 ROVR_RCOL_W, ROVR_ROLL_H,
                 TFT_LIGHT_GREY);
    textCenter(tft, &Roboto_Black_20,
               ROVR_RCOL_X, ROVR_ROLL_Y + ROVR_TILT_TOP_PAD,
               ROVR_RCOL_W, ROVR_TILT_LBL_H,
               "Roll:", TFT_WHITE, TFT_BLACK);

    int16_t refY  = ROVR_ROLL_SIL_CY;
    int16_t lxL   = ROVR_RCOL_X + ROVR_TILT_REF_MARGIN;
    int16_t lxR   = lxL + ROVR_TILT_REF_LEN - 1;
    int16_t rxR   = ROVR_RCOL_X + ROVR_RCOL_W - 1 - ROVR_TILT_REF_MARGIN;
    int16_t rxL   = rxR - ROVR_TILT_REF_LEN + 1;
    tft.drawLine(lxL, refY, lxR, refY, TFT_LIGHT_GREY);
    tft.drawLine(rxL, refY, rxR, refY, TFT_LIGHT_GREY);
}

// Clamp angle to ±45° for display purposes (actual sign preserved). Beyond this
// range the silhouette is drawn at the clamped angle but the color indicates
// the real magnitude (which will be in the alarm zone by this point).
static inline float _rovrClampTilt(float a) {
    if (a >  45.0f) return  45.0f;
    if (a < -45.0f) return -45.0f;
    return a;
}

// Pitch update — redraws the side-view silhouette on significant angle change,
// and the numeric value on integer-degree change. The silhouette itself is
// always drawn in chrome colors; only the numeric value takes the threshold
// color (green normal / yellow warn / red alarm).
static void _rovrUpdatePitch(RA8875 &tft) {
    float pitch = state.pitch;
    float pitchClamped = _rovrClampTilt(pitch);

    // Silhouette redraw: only on sufficient angle change, NOT on threshold change
    // (since silhouette color is fixed).
    bool silDirty =
        (fabsf(pitch - _rovrPrevPitch) >= ROVR_TILT_THRESH_DEG) ||
        (_rovrPrevPitch < -9000.0f);

    if (silDirty) {
        _rovrEraseSilhouette(tft, ROVR_PITCH_SIL_CX, ROVR_PITCH_SIL_CY);
        // Nose-up pitch rotates the silhouette CCW on screen: pass -pitch.
        _rovrDrawPitchSilhouette(tft, ROVR_PITCH_SIL_CX, ROVR_PITCH_SIL_CY,
                                 -pitchClamped);
        _rovrPrevPitch = pitch;
    }

    // Numeric readout — threshold-colored, redraws on tenths change OR color change.
    // Cache by iPitch = round(pitch * 10) so the displayed value (1 decimal)
    // determines the dirty boundary.
    uint16_t valueColor = _rovrTiltValueColor(pitch, ROVER_PITCH_WARN_DEG,
                                                     ROVER_PITCH_ALARM_DEG);
    int16_t iPitch = (int16_t)roundf(pitch * 10.0f);
    if (iPitch != _rovrPrevPitchVal || valueColor != _rovrPrevPitchColor) {
        if (_rovrPrevPitchVal > -9000) {
            char oldBuf[12];
            snprintf(oldBuf, sizeof(oldBuf), "%+.1f\xB0", (float)_rovrPrevPitchVal / 10.0f);
            textCenter(tft, &Roboto_Black_28,
                       ROVR_RCOL_X, ROVR_PITCH_VAL_Y,
                       ROVR_RCOL_W, ROVR_TILT_VAL_H,
                       oldBuf, TFT_BLACK, TFT_BLACK);
        }
        char buf[12];
        snprintf(buf, sizeof(buf), "%+.1f\xB0", pitch);
        textCenter(tft, &Roboto_Black_28,
                   ROVR_RCOL_X, ROVR_PITCH_VAL_Y,
                   ROVR_RCOL_W, ROVR_TILT_VAL_H,
                   buf, valueColor, TFT_BLACK);
        _rovrPrevPitchVal   = iPitch;
        _rovrPrevPitchColor = valueColor;
    }
}

// Roll update — rear-view silhouette. Positive roll (bank right) → right side
// of silhouette down on screen (rotation angle = +roll directly).
static void _rovrUpdateRoll(RA8875 &tft) {
    float roll = state.roll;
    float rollClamped = _rovrClampTilt(roll);

    bool silDirty =
        (fabsf(roll - _rovrPrevRoll) >= ROVR_TILT_THRESH_DEG) ||
        (_rovrPrevRoll < -9000.0f);

    if (silDirty) {
        _rovrEraseSilhouette(tft, ROVR_ROLL_SIL_CX, ROVR_ROLL_SIL_CY);
        _rovrDrawRollSilhouette(tft, ROVR_ROLL_SIL_CX, ROVR_ROLL_SIL_CY,
                                rollClamped);
        _rovrPrevRoll = roll;
    }

    uint16_t valueColor = _rovrTiltValueColor(roll, ROVER_ROLL_WARN_DEG,
                                                    ROVER_ROLL_ALARM_DEG);
    int16_t iRoll = (int16_t)roundf(roll * 10.0f);
    if (iRoll != _rovrPrevRollVal || valueColor != _rovrPrevRollColor) {
        if (_rovrPrevRollVal > -9000) {
            char oldBuf[12];
            snprintf(oldBuf, sizeof(oldBuf), "%+.1f\xB0", (float)_rovrPrevRollVal / 10.0f);
            textCenter(tft, &Roboto_Black_28,
                       ROVR_RCOL_X, ROVR_ROLL_VAL_Y,
                       ROVR_RCOL_W, ROVR_TILT_VAL_H,
                       oldBuf, TFT_BLACK, TFT_BLACK);
        }
        char buf[12];
        snprintf(buf, sizeof(buf), "%+.1f\xB0", roll);
        textCenter(tft, &Roboto_Black_28,
                   ROVR_RCOL_X, ROVR_ROLL_VAL_Y,
                   ROVR_RCOL_W, ROVR_TILT_VAL_H,
                   buf, valueColor, TFT_BLACK);
        _rovrPrevRollVal   = iRoll;
        _rovrPrevRollColor = valueColor;
    }
}

// Heading readout — single-line value inside a bordered box above the nose
// triangle. Box border is drawn once in chrome; this function only touches
// the value text when the integer heading changes. Value is erased black-on-
// black before the new value is drawn to handle digit-width changes cleanly.
//
// Format: zero-padded 3-digit unsigned heading with ° symbol (e.g. "045°").
static void _rovrUpdateHdgReadout(RA8875 &tft, float hdg) {
    int16_t iHdg = (int16_t)roundf(hdg) % 360;
    if (iHdg < 0) iHdg += 360;
    if (iHdg == _rovrPrevHdgVal) return;

    // Erase previous value (black-on-black) if one was drawn.
    // Inset by 2 px from the border on top and bottom so the glyph cell's
    // background fill doesn't overwrite the box border pixels.
    if (_rovrPrevHdgVal > -9000) {
        char oldBuf[8];
        snprintf(oldBuf, sizeof(oldBuf), "%03d\xB0", _rovrPrevHdgVal);
        textCenter(tft, &Roboto_Black_28,
                   ROVR_HDG_BOX_X, ROVR_HDG_BOX_Y + 2,
                   ROVR_HDG_BOX_W, ROVR_HDG_BOX_H - 4,
                   oldBuf, TFT_BLACK, TFT_BLACK);
    }

    // Draw new value, zero-padded to 3 digits
    char buf[8];
    snprintf(buf, sizeof(buf), "%03d\xB0", iHdg);
    textCenter(tft, &Roboto_Black_28,
               ROVR_HDG_BOX_X, ROVR_HDG_BOX_Y + 2,
               ROVR_HDG_BOX_W, ROVR_HDG_BOX_H - 4,
               buf, TFT_DARK_GREEN, TFT_BLACK);

    _rovrPrevHdgVal = iHdg;
}

// Full chrome draw — called once on screen entry. Erases the compass area (which
// includes the heading readout box region) and draws all stationary elements:
// outer ring, nose indicator, heading box border, rover silhouette. Then does the
// initial dynamic pass (ticks, labels, heading value, target triangle).
static void _rovrChromeCompass(RA8875 &tft) {
    _rovrEraseCompass(tft);

    // Stationary: outer ring
    tft.drawCircle(ROVR_CX, ROVR_CY, ROVR_R, TFT_LIGHT_GREY);

    // Stationary: vessel heading indicator (triangle at top of ring, pointing inward)
    _rovrDrawNose(tft);

    // Stationary: heading readout box border (rectangle above the triangle).
    // The border is drawn here ONCE — _rovrUpdateHdgReadout only writes the value
    // text inside the box, never the border, so the border doesn't flicker.
    tft.drawRect(ROVR_HDG_BOX_X, ROVR_HDG_BOX_Y,
                 ROVR_HDG_BOX_W, ROVR_HDG_BOX_H, TFT_LIGHT_GREY);

    // Stationary: rover silhouette at compass centre
    _rovrDrawIcon(tft);

    // Initial dynamic pass
    _rovrDrawTicks(tft, state.heading, false);
    _rovrDrawLabels(tft, state.heading, false);
    _rovrPrevHdgVal = -9999;   // force readout to redraw from scratch
    _rovrUpdateHdgReadout(tft, state.heading);

    _rovrPrevHeading = state.heading;

    // Target triangle — use the independent update function so the previous-
    // position state variables are kept consistent.
    _rovrPrevTgtAvail     = false;
    _rovrPrevTgtScreenDeg = -9999.0f;
    _rovrUpdateTarget(tft);
}

// Incremental compass update — erases ticks and labels at the *previous* heading
// (by drawing them in black), then redraws them at the new heading. Stationary
// elements (outer ring, nose, rover icon, heading box border) are not touched by
// the erase, and the ring is re-stamped at the end as a cleanup against any
// tick-erase nibbles. Heading readout updates via its own integer-change
// detection. Target triangle is handled separately by _rovrUpdateTarget because
// it sits in an annular band that doesn't overlap with ticks or labels.
//
// This avoids the large fillRect on every heading change that would otherwise
// cause visible flicker during continuous yaw.
static void _rovrUpdateCompass(RA8875 &tft) {
    // Erase old ticks and labels at previous heading
    _rovrDrawTicks(tft, _rovrPrevHeading, true);
    _rovrDrawLabels(tft, _rovrPrevHeading, true);

    // Draw new ticks and labels at current heading
    _rovrDrawTicks(tft, state.heading, false);
    _rovrDrawLabels(tft, state.heading, false);

    // Redraw outer ring. Tick-erase can nibble a pixel or two of the ring on some
    // diagonal angles, so re-stamping the ring here keeps it clean.
    tft.drawCircle(ROVR_CX, ROVR_CY, ROVR_R, TFT_LIGHT_GREY);

    // Update heading readout
    _rovrUpdateHdgReadout(tft, state.heading);

    _rovrPrevHeading = state.heading;
}


// ── Screen entry points ───────────────────────────────────────────────────────────────
static void chromeScreen_ROVR(RA8875 &tft) {
    // Reset all change-detection state.
    _rovrPrevHeading      = -9999.0f;
    _rovrPrevTgtAvail     = false;
    _rovrPrevTgtScreenDeg = -9999.0f;
    _rovrPrevThrFill      = -9999;
    _rovrPrevVSrf         = -9999;
    _rovrPrevEcPct        = -9999;
    _rovrPrevEcFg         = 0xFFFF;
    _rovrPrevEcBg         = 0xFFFF;
    _rovrPrevBrake        = -1;
    _rovrPrevGear         = -1;
    _rovrPrevSasMode      = -1;
    _rovrPrevPitch        = -9999.0f;
    _rovrPrevRoll         = -9999.0f;
    _rovrPrevPitchVal     = -9999;
    _rovrPrevRollVal      = -9999;
    _rovrPrevPitchColor   = 0;
    _rovrPrevRollColor    = 0;
    _rovrPrevElev         = -99999;
    _rovrPrevTgtDistAvail = false;
    _rovrPrevTgtDistVal   = -1;

    // Compass chrome + initial dynamic pass (erase compass area, draw stationary
    // elements, do first tick/label/heading/target pass).
    _rovrChromeCompass(tft);

    // Throttle bar — draw stationary frame + initial fill+text. (The throttle bar
    // region is outside the compass erase area, so we need our own chrome.)
    _rovrDrawThrottleFrame(tft);
    _rovrUpdateThrottle(tft);

    // Left column labels (stationary, drawn once) + initial value passes.
    _rovrDrawVSrfChrome(tft);
    _rovrUpdateVSrf(tft);
    _rovrDrawEcChrome(tft);
    _rovrUpdateEc(tft);
    _rovrUpdateBrake(tft);
    _rovrUpdateGear(tft);
    _rovrUpdateSas(tft);

    // Right column (elevation / pitch / roll)
    _rovrDrawElevChrome(tft);
    _rovrUpdateElev(tft);
    _rovrDrawPitchChrome(tft);
    _rovrUpdatePitch(tft);
    _rovrDrawRollChrome(tft);
    _rovrUpdateRoll(tft);

    // Target distance readout — below the compass, shown only when target is active.
    _rovrUpdateTgtDist(tft);
}

static void drawScreen_ROVR(RA8875 &tft) {
    // Heading readout updates whenever the integer degree value changes — may fire
    // at sub-threshold heading movements. Its own cache check is near-free.
    _rovrUpdateHdgReadout(tft, state.heading);

    // Compass redraw (ticks/labels/ring) only when heading has changed past threshold.
    if (fabsf(_rovrHdgDelta(state.heading, _rovrPrevHeading)) >= ROVR_HDG_THRESH_DEG) {
        _rovrUpdateCompass(tft);
    }

    // Target triangle — update on its own cadence, independent of compass redraw.
    _rovrUpdateTarget(tft);

    // Target distance — gated on targetAvailable internally, cheap when no target.
    _rovrUpdateTgtDist(tft);

    // Left column + throttle bar — each with internal change detection.
    _rovrUpdateThrottle(tft);
    _rovrUpdateVSrf(tft);
    _rovrUpdateEc(tft);
    _rovrUpdateBrake(tft);
    _rovrUpdateGear(tft);
    _rovrUpdateSas(tft);

    // Right column
    _rovrUpdateElev(tft);
    _rovrUpdatePitch(tft);
    _rovrUpdateRoll(tft);
}
