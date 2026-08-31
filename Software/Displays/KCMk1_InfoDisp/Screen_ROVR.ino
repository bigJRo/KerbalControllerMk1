/***************************************************************************************
   Screen_ROVR.ino -- Rover screen — chromeScreen_ROVR, drawScreen_ROVR
   Dedicated display for type_Rover vessels. contextScreen() routes here automatically.
   rowCache index: [9] (screen_ROVR = 9)

   Layout (1024 × 600, content area x=0..939, sidebar at x=940..1023):
     LEFT COLUMN (x=0..190, 5 stacked boxes):
       V.Srf    — signed surface speed (m/s), 1 decimal, direction-colored
       EC%      — electric charge %, threshold-colored (green/yellow/red)
       BRAKES   — button: white on dark green when on, muted when off
       GEAR     — button: white on dark green when deployed, muted when up
       SAS      — button: navball-palette mode colors (STAB/PRO/RETR/...)

     FWD / REV BLOCKS (top corners of the compass area, flanking the heading box):
       Binary on/off: FWD block green when forward, REV block yellow when reverse,
       both muted (off-black) when neutral

     COMPASS (centered at 470, 344, R=200 — centred on the ROVER title):
       Rotating ring with cardinal letters (N/E/S/W) and numeric labels (03/06/...)
       Fixed nose triangle at top, rover icon at centre
       Heading readout box above (boxed "XXX°" in Roboto_Black_36)
       Target bearing triangle inside ring when state.targetAvailable

     TGT STATUS STRIP (within the compass region, along its bottom, y=552..600):
       "Dist:" label flush-left against the left column, formatted distance value
       flush-right against the right column, shown only when state.targetAvailable

     RIGHT COLUMN (x=750..940, 3 stacked boxes):
       Elev     — elevation (altitude ASL - radarAlt AGL), formatted via
                  formatAlt (auto-scales m/km/Mm/Gm with thousands separator)
       Pitch    — side-view tilting silhouette + 1-decimal signed angle
       Roll     — rear-view tilting silhouette + 1-decimal signed angle
                  Silhouettes always drawn in chrome colors; numeric values
                  take threshold color (green/yellow/red per AAA_Config).

   Anti-flicker: each sub-element has its own dirty threshold and incremental
   update. Stationary chrome (borders, labels, ring, nose, rover icon) is drawn
   once in chromeScreen_ROVR and never redrawn.

****************************************************************************************/
#include "KCMk1_InfoDisp.h"


// ── Geometry ──────────────────────────────────────────────────────────────────────────
// Compass ring sized to R=200 and centred at CX=470 (the ROVER title centre,
// CONTENT_W/2) with a taller heading readout box (Roboto_Black_36, cap_height=43).
// CY at 344 places the ring erase floor at y=551, just above the Tgt status strip
// to maintain roughly equal margins above the heading box (12 px) and below the
// ring (13 px).
static const int16_t ROVR_CX             = CONTENT_W / 2;  // centred on the title
static const int16_t ROVR_CY             = 344;
static const int16_t ROVR_R              = 200;   // outer ring radius
static const int16_t ROVR_R_TICK_OUTER   = 196;   // tick outer end (4 px inside outer ring)
                                                  //   so tick-erase can't reach the ring — lets
                                                  //   us skip re-stamping the ring every frame
static const int16_t ROVR_R_TICK_INNER   = 178;   // major tick inner end
static const int16_t ROVR_R_MINOR_INNER  = 185;   // minor tick inner end
static const int16_t ROVR_R_LETTER       = 157;   // cardinal letter centre
static const int16_t ROVR_R_NUMLABEL     = 157;   // numeric label centre — same

// Vessel heading indicator — triangle ABOVE the ring at 12 o'clock, pointing INWARD
// (toward the compass center). Tip closer to centre, base farther out.
static const int16_t ROVR_NOSE_R_TIP     = 204;   // inward tip (4 px outside ring)
static const int16_t ROVR_NOSE_R_BASE    = 219;   // outward base
static const int16_t ROVR_NOSE_HALF_W    = 12;

// Target bearing indicator — triangle INSIDE the ring, positioned just inside
// the compass labels. Labels (Roboto_Black_28) are centred at R=157; their
// opaque glyph boxes reach inward to R≈136. The tip is held at R=128 so that
// even with the 3-px erase-rect dilation (R≈131) the triangle never reaches the
// label boxes — previously the tip at R=135 let a label (e.g. "15") clip the
// triangle when the target bearing lined up with it. Tip points OUTWARD (toward
// the ring); when target heading == vessel heading the triangle sits at 12
// o'clock with its tip pointing up. TFT_VIOLET matches SCFT/ACFT target markers.
static const int16_t ROVR_TGT_R_TIP      = 128;   // outward tip (clears label boxes at R≈136)
static const int16_t ROVR_TGT_R_BASE     = 110;   // inward base (18 px tall triangle)
static const int16_t ROVR_TGT_HALF_W     = 12;   // same half-width as nose

// Target distance readout — shown only when a target is selected (targetAvailable).
// Status strip along the bottom of the compass region, between the two side
// columns and below the ring. Label "Dist:" is flush-left against the left column
// (region left edge x=190) and the formatted distance value is flush-right against
// the right column (region right edge x=750). The strip sits just below the
// compass erase region (which ends at y=551), so the two never collide.
static const int16_t ROVR_TGTD_LBL_X     = 190;
static const int16_t ROVR_TGTD_LBL_W     = 250;
static const int16_t ROVR_TGTD_VAL_X     = 500;
static const int16_t ROVR_TGTD_VAL_W     = 250;  // right edge = 500+250 = 750
static const int16_t ROVR_TGTD_Y         = 552;
static const int16_t ROVR_TGTD_H         = 48;   // Roboto_Black_36 cap 43 + padding

// Heading readout — single-line boxed value above the nose triangle. The box
// border is stationary chrome; only the numeric value is redrawn on changes.
// Format: zero-padded 3-digit heading with ° symbol (e.g. "045°").
// Box is 38 tall to give the 28pt Roboto Black font (33 px font_height) room
// to render without the glyph cell's black background overwriting the border.
static const int16_t ROVR_HDG_BOX_W      = 110;  // widened slightly for the larger font
static const int16_t ROVR_HDG_BOX_H      = 52;   // Roboto_Black_36 cap 43 + padding
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
static const int16_t ROVR_ICON_CHASS_W   = 54;   // chassis rect width
static const int16_t ROVR_ICON_CHASS_H   = 90;   // chassis rect height
static const int16_t ROVR_ICON_WHEEL_W   = 12;   // single wheel rect width
static const int16_t ROVR_ICON_WHEEL_H   = 24;   // single wheel rect height
static const int16_t ROVR_ICON_WHEEL_GAP = 12;   // wheel outer edge extends this far beyond chassis
static const int16_t ROVR_ICON_WHEEL_INSET = 8;  // wheel vertical inset from chassis top/bottom
static const int16_t ROVR_ICON_NOSE_W    = 20;   // nose notch base width (across chassis front)
static const int16_t ROVR_ICON_NOSE_H    = 12;   // nose notch height (extends above chassis)

// ── FWD / REV drive-state blocks ──────────────────────────────────────────────────────
// Two button-style blocks in the top-left and top-right corners of the compass
// area, flanking the heading readout box. Binary drive-state indicators:
//   FWD block — dark-green fill + white text when wheelThrottle > deadband
//   REV block — yellow fill + dark-grey text when wheelThrottle < -deadband
//   both muted (off-black fill) when neutral.
// Both sit at the top corners of the compass region, top-aligned with the title-bar
// rule (y=62, same as the side columns). FWD's left edge is flush with the left
// column's right edge (x=190); REV's right edge is flush with the right column's
// left edge (x=750, so REV_X = 750-168 = 582). The heading box sits between them.
// Boxes clear the ring: at their inner x-edges the ring peaks near y=178, below
// the box bottom at y=139.
static const int16_t ROVR_THRBLK_W       = 168;   // block width (20% larger than 140)
static const int16_t ROVR_THRBLK_H        = 77;   // block height (20% larger than 64)
static const int16_t ROVR_THRBLK_Y        = TITLE_TOP;  // top edge (flush with title-bar rule)
static const int16_t ROVR_FWD_BLK_X       = 190;  // left edge flush with left column
static const int16_t ROVR_REV_BLK_X       = 582;  // right edge flush with right column (582+168 = 750)
static const float   ROVR_THR_THRESH     = 0.01f; // deadband — |wt| under this reads as neutral

// ── Left column: V.Srf / EC% / BRAKE / GEAR / SAS ─────────────────────────────────────
// Five stacked blocks filling the full available height of the left column, from
// the title-bar rule (y=62) down to the screen bottom (y=600). Total height 538 px,
// split into five blocks (108/108/108/107/107). Column x=0..190 (width 190),
// matching the right column width so the compass centres cleanly between them.
static const int16_t ROVR_LCOL_X         = 0;
static const int16_t ROVR_LCOL_W         = 190;
static const int16_t ROVR_LCOL_Y_TOP     = TITLE_TOP;  // flush with the title-bar rule

static const int16_t ROVR_VSRF_H         = 108;
static const int16_t ROVR_EC_H           = 108;
static const int16_t ROVR_BRAKE_H        = 108;
static const int16_t ROVR_GEAR_H         = 107;
static const int16_t ROVR_SAS_H          = 107;

static const int16_t ROVR_VSRF_Y         = ROVR_LCOL_Y_TOP;                   // 62
static const int16_t ROVR_EC_Y           = ROVR_VSRF_Y  + ROVR_VSRF_H;        // 170
static const int16_t ROVR_BRAKE_Y        = ROVR_EC_Y    + ROVR_EC_H;          // 278
static const int16_t ROVR_GEAR_Y         = ROVR_BRAKE_Y + ROVR_BRAKE_H;       // 386
static const int16_t ROVR_SAS_Y          = ROVR_GEAR_Y  + ROVR_GEAR_H;        // 493
                                                                               //   end = 600

// Within V.Srf and EC% blocks: label on top, numeric value below. Both use the
// same internal layout metrics. textCenter is called with the block slot (x, y,
// w, h) so the two lines are centered within each block vertically.
static const int16_t ROVR_LBL_H          = 32;    // label strip height (Roboto_Black_24 cap=29)
static const int16_t ROVR_VAL_H          = 48;    // value strip height (Roboto_Black_36 cap=43)
static const int16_t ROVR_LBL_VAL_GAP    = 2;     // gap between label and value

// ── Right column: elevation / pitch / roll ───────────────────────────────────────────
// Three stacked boxes on the right side of the screen:
//   Elevation — surface ASL altitude (Alt.SL - Alt.Rdr), same block style as V.Srf
//   Pitch     — side-view tilt silhouette + numeric
//   Roll      — rear-view tilt silhouette + numeric
//
// Column x=750..940 (width 190), matching the left column width. Runs the full
// available height from the title-bar rule (y=62) down to the screen bottom (y=600).
static const int16_t ROVR_RCOL_X         = 750;
static const int16_t ROVR_RCOL_W         = 190;
static const int16_t ROVR_ELEV_Y         = TITLE_TOP;  // flush with the title-bar rule
static const int16_t ROVR_ELEV_H         = 108;   // matches left-column block height
static const int16_t ROVR_PITCH_Y        = ROVR_ELEV_Y + ROVR_ELEV_H;   // adjacent to Elev bottom
static const int16_t ROVR_PITCH_H        = 215;
static const int16_t ROVR_ROLL_Y         = ROVR_PITCH_Y + ROVR_PITCH_H;  // adjacent to Pitch bottom
static const int16_t ROVR_ROLL_H         = 215;   // ends at y=600 (screen bottom)

// Within each indicator box:
//   label at top (Roboto_Black_24, height 32)
//   silhouette area in middle (approx 130 px tall)
//   numeric value at bottom (Roboto_Black_36, height 48)
static const int16_t ROVR_TILT_LBL_H     = 32;
static const int16_t ROVR_TILT_VAL_H     = 48;
static const int16_t ROVR_TILT_TOP_PAD   = 4;
static const int16_t ROVR_TILT_GAP       = 2;

// Side-view silhouette dimensions (used for Pitch indicator) — base, unrotated.
// Wide low body with two round wheels beneath, like the side profile of a rover.
static const int16_t ROVR_SIL_BODY_W     = 100;    // pitch-view body width
static const int16_t ROVR_SIL_BODY_H     = 36;    // pitch-view body height
static const int16_t ROVR_SIL_WHEEL_R    = 13;    // pitch-view wheel radius (circle)
static const int16_t ROVR_SIL_WHEEL_DX   = 35;    // horiz offset of wheel centre from body centre
static const int16_t ROVR_SIL_WHEEL_DY   = 26;    // vert offset (below body centre)

// Rear-view silhouette dimensions (used for Roll indicator) — base, unrotated.
// Body height matches pitch silhouette body height (28) so the vehicle feels
// consistently sized across the two indicators. Body width is narrower (50)
// to convey rear-view perspective. Tires appear as rectangles on the SIDES of
// the body (visible tread face), with about half the tire height extending
// below the body.
static const int16_t ROVR_REAR_BODY_W    = 64;    // rear-view body width
static const int16_t ROVR_REAR_BODY_H    = 36;    // rear-view body height (matches pitch body)
static const int16_t ROVR_REAR_TIRE_W    = 13;    // each tire rect width (extends OUTWARD)
static const int16_t ROVR_REAR_TIRE_H    = 26;    // each tire rect height (≈pitch wheel dia)

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

// ── Shared compass card ───────────────────────────────────────────────────────────────
// The rose (ring, ticks, labels, nose, bearing marker) moved to Compass.ino when the
// NAV screen was added, so the two navigation screens run one implementation rather
// than two hand-maintained copies — the same consolidation the reticle layer and the
// EADI tape already went through. Every radius below is ROVER's original value; only
// the code that consumes them is now shared.
static const CompassGeom ROVR_GEOM = {
  ROVR_CX, ROVR_CY,
  ROVR_R,
  ROVR_R_TICK_OUTER, ROVR_R_TICK_INNER, ROVR_R_MINOR_INNER,
  ROVR_R_LETTER, ROVR_R_NUMLABEL,
  ROVR_NOSE_R_TIP, ROVR_NOSE_R_BASE, ROVR_NOSE_HALF_W,
  ROVR_TGT_R_TIP,  ROVR_TGT_R_BASE,  ROVR_TGT_HALF_W
};

// ── Shortest-arc delta helper ─────────────────────────────────────────────────────────
static inline float _rovrHdgDelta(float a, float b) { return eadiHdgDelta(a, b); }

// ── Compass drawing ───────────────────────────────────────────────────────────────────

// Erase the compass bounding box before redrawing. fillRect is hardware-accelerated
// on the RA8876 so this is cheap. Bounds cover outer ring, tick marks, labels,
// nose triangle, and the heading readout box above the triangle.
static void _rovrEraseCompass(KCM_TFT &tft) {
    int16_t x0 = ROVR_CX - ROVR_R - 8;
    int16_t y0 = ROVR_HDG_BOX_Y - 4;               // reach above heading box
    int16_t x1 = ROVR_CX + ROVR_R + 8;
    int16_t y1 = ROVR_CY + ROVR_R + 8;
    tft.fillRect(x0, y0, x1 - x0, y1 - y0, TFT_BLACK);
}

// Compass card, nose and target marker — thin adapters onto the shared renderer in
// Compass.ino. The geometry, colours and erase strategy are unchanged; ROVR_GEOM
// carries this screen's original radii, and each screen keeps its own prev-drawn
// cache so the redraw gating stays independent. Equivalence with the previous
// hand-rolled versions was verified on the host: every draw call, at every heading
// and every marker bearing, is identical.
static inline void _rovrDrawTicks(KCM_TFT &tft, float headingDeg, bool erase) {
    compassDrawTicks(tft, ROVR_GEOM, headingDeg, erase);
}

static inline void _rovrDrawLabels(KCM_TFT &tft, float headingDeg, bool erase) {
    compassDrawLabels(tft, ROVR_GEOM, headingDeg, erase);
}

static inline void _rovrDrawNose(KCM_TFT &tft) {
    compassDrawNose(tft, ROVR_GEOM);
}

// Target bearing marker — violet, matching the target colour used on SCFT/ACFT.
static inline void _rovrDrawTargetAt(KCM_TFT &tft, float screenDeg, bool erase) {
    compassDrawMarker(tft, ROVR_GEOM, screenDeg, TFT_VIOLET, erase);
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
static void _rovrUpdateTarget(KCM_TFT &tft) {
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

// Target-distance readout — status strip along the bottom of the compass region.
// "Dist:" label flush-left against the left column, formatted distance value flush-
// right against the right column. Both are visible only when a target is selected;
// when no target, both strips are erased to black.
//
// Uses formatAlt() from the shared library for auto-scaled units (m/km/Mm/Gm)
// and thousands separators. Value is rendered in TFT_VIOLET to match the target
// triangle and the convention used on SCFT/ACFT target markers.
static void _rovrUpdateTgtDist(KCM_TFT &tft) {
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
        textLeft(tft, &Roboto_Black_36,
                 ROVR_TGTD_LBL_X, ROVR_TGTD_Y,
                 ROVR_TGTD_LBL_W, ROVR_TGTD_H,
                 "Dist:", TFT_WHITE, TFT_BLACK);
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
    textRight(tft, &Roboto_Black_36,
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
static void _rovrDrawIcon(KCM_TFT &tft) {
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

// FWD / REV drive-state blocks — two button-style boxes in the top corners of the
// compass area, flanking the heading readout. Binary on/off, one of three states:
//   FWD (throttle > deadband)       — FWD block dark-green + white text, REV muted
//   REV (throttle < -deadband)      — REV block yellow + dark-grey text, FWD muted
//   NEUTRAL (|throttle| < deadband) — both blocks muted (off-black fill)
//
// Both blocks are redrawn together on every state transition (a change in one
// implies the other returns to muted). drawButton fills, borders, and centre-
// labels each block in a single call. State code: +1 forward, 0 neutral, -1 rev.
static void _rovrUpdateThrottle(KCM_TFT &tft) {
    float wt = state.wheelThrottle;
    int16_t newState;
    if      (wt >  ROVR_THR_THRESH) newState = +1;
    else if (wt < -ROVR_THR_THRESH) newState = -1;
    else                            newState =  0;

    if (newState == _rovrPrevThrFill) return;

    ButtonLabel fwd = (newState == +1)
        ? ButtonLabel{ "FWD", TFT_WHITE,     TFT_WHITE,     TFT_DARK_GREEN, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }
        : ButtonLabel{ "FWD", TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK,  TFT_OFF_BLACK,  TFT_GREY, TFT_GREY };
    drawButton(tft, ROVR_FWD_BLK_X, ROVR_THRBLK_Y,
               ROVR_THRBLK_W, ROVR_THRBLK_H, fwd, &Roboto_Black_36, false);

    ButtonLabel rev = (newState == -1)
        ? ButtonLabel{ "REV", TFT_DARK_GREY, TFT_DARK_GREY, TFT_YELLOW,    TFT_YELLOW,    TFT_GREY, TFT_GREY }
        : ButtonLabel{ "REV", TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_OFF_BLACK, TFT_GREY, TFT_GREY };
    drawButton(tft, ROVR_REV_BLK_X, ROVR_THRBLK_Y,
               ROVR_THRBLK_W, ROVR_THRBLK_H, rev, &Roboto_Black_36, false);

    _rovrPrevThrFill = newState;
}

// Surface velocity readout — "V.Srf:" label over signed speed (m/s) with 1 decimal.
// Label drawn once in chrome. Value updated via cached dirty check. Sign derived
// from comparing state.srfVelHeading to state.heading (>90° off-nose = reversing).
//
// The label and value are centered within the V.Srf block (y=ROVR_VSRF_Y, height
// ROVR_VSRF_H). Both strip rects are exactly `ROVR_LBL_H` and `ROVR_VAL_H` tall.
// (value-erase now uses the shared eraseCenteredValue() from KerbalDisplayCommon)

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
static void _rovrDrawVSrfChrome(KCM_TFT &tft) {
    // Bounding box around the full V.Srf block
    tft.drawRect(ROVR_LCOL_X, ROVR_VSRF_Y,
                 ROVR_LCOL_W, ROVR_VSRF_H,
                 TFT_GREY);

    // Label centered in its strip
    textCenter(tft, &Roboto_Black_24,
               ROVR_LCOL_X, _rovrVSrfLabelY(),
               ROVR_LCOL_W, ROVR_LBL_H,
               "V.Srf:", TFT_WHITE, TFT_BLACK);
}

static void _rovrUpdateVSrf(KCM_TFT &tft) {
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
        eraseCenteredValue(tft, &Roboto_Black_36,
                                ROVR_LCOL_X, valueY, ROVR_LCOL_W, ROVR_VAL_H,
                                oldBuf, TFT_BLACK);
    }

    // Color: green forward, yellow reverse, grey neutral
    uint16_t fg;
    if      (iSpeed >  0) fg = TFT_DARK_GREEN;
    else if (iSpeed <  0) fg = TFT_YELLOW;
    else                   fg = TFT_DARK_GREY;

    char buf[16];
    snprintf(buf, sizeof(buf), "%+.1f m/s", speed);
    textCenter(tft, &Roboto_Black_36,
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
static void _rovrDrawEcChrome(KCM_TFT &tft) {
    tft.drawRect(ROVR_LCOL_X, ROVR_EC_Y,
                 ROVR_LCOL_W, ROVR_EC_H,
                 TFT_GREY);

    textCenter(tft, &Roboto_Black_24,
               ROVR_LCOL_X, _rovrEcLabelY(),
               ROVR_LCOL_W, ROVR_LBL_H,
               "EC%:", TFT_WHITE, TFT_BLACK);
}

static void _rovrUpdateEc(KCM_TFT &tft) {
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
        eraseCenteredValue(tft, &Roboto_Black_36,
                                ROVR_LCOL_X, valueY, ROVR_LCOL_W, ROVR_VAL_H,
                                oldBuf, _rovrPrevEcBg);
    }

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", iEc);
    textCenter(tft, &Roboto_Black_36,
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

static void _rovrUpdateBrake(KCM_TFT &tft) {
    int8_t newState = state.brakes_on ? 1 : 0;
    if (newState == _rovrPrevBrake) return;

    ButtonLabel btn = state.brakes_on
        ? ButtonLabel{ "BRAKES", TFT_WHITE,     TFT_WHITE,     TFT_DARK_GREEN, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }
        : ButtonLabel{ "BRAKES", TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK,  TFT_OFF_BLACK,  TFT_GREY, TFT_GREY };
    drawButton(tft,
               ROVR_LCOL_X, ROVR_BRAKE_Y,
               ROVR_LCOL_W, ROVR_BRAKE_H,
               btn, &Roboto_Black_28, false);

    _rovrPrevBrake = newState;
}

static void _rovrUpdateGear(KCM_TFT &tft) {
    int8_t newState = state.gear_on ? 1 : 0;
    if (newState == _rovrPrevGear) return;

    // Rover wheels are deployed (gear_on) for normal driving — so "GEAR DOWN"
    // is the safe/active state (green bg), "GEAR UP" is unusual (muted).
    ButtonLabel btn = state.gear_on
        ? ButtonLabel{ "GEAR", TFT_WHITE,     TFT_WHITE,     TFT_DARK_GREEN, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }
        : ButtonLabel{ "GEAR", TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK,  TFT_OFF_BLACK,  TFT_GREY, TFT_GREY };
    drawButton(tft,
               ROVR_LCOL_X, ROVR_GEAR_Y,
               ROVR_LCOL_W, ROVR_GEAR_H,
               btn, &Roboto_Black_28, false);

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
static void _rovrUpdateSas(KCM_TFT &tft) {
    int16_t newMode = (int16_t)state.sasMode;
    if (newMode == _rovrPrevSasMode) return;

    const char *v; uint16_t fg, bg;
    sasNavballLabel(state.sasMode, v, fg, bg);

    ButtonLabel btn = { v, fg, fg, bg, bg, TFT_GREY, TFT_GREY };
    drawButton(tft,
               ROVR_LCOL_X, ROVR_SAS_Y,
               ROVR_LCOL_W, ROVR_SAS_H,
               btn, &Roboto_Black_28, false);

    _rovrPrevSasMode = newMode;
}

// ── Elevation readout ─────────────────────────────────────────────────────────────────
// "Alt.Trn:" label over integer surface-altitude value in meters. Named for the Alt
// family (Alt.SL, Alt.Rdr) it belongs to, not "Elev:", which sat one letter away from
// the "Elv:" that DOCKING, TARGET and MANEUVER use for an elevation ANGLE -- a different
// quantity in different units. Terrain elevation is
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

static void _rovrDrawElevChrome(KCM_TFT &tft) {
    tft.drawRect(ROVR_RCOL_X, ROVR_ELEV_Y,
                 ROVR_RCOL_W, ROVR_ELEV_H,
                 TFT_GREY);
    textCenter(tft, &Roboto_Black_24,
               ROVR_RCOL_X, _rovrElevLabelY(),
               ROVR_RCOL_W, ROVR_LBL_H,
               "Alt.Trn:", TFT_WHITE, TFT_BLACK);
}

static void _rovrUpdateElev(KCM_TFT &tft) {
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
        eraseCenteredValue(tft, &Roboto_Black_36,
                                ROVR_RCOL_X, valueY, ROVR_RCOL_W, ROVR_VAL_H,
                                oldStr.c_str(), TFT_BLACK);
    }

    String newStr = formatAlt((float)iElev);
    textCenter(tft, &Roboto_Black_36,
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
static void _rovrDrawPitchSilhouette(KCM_TFT &tft, int16_t cx, int16_t cy,
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
static void _rovrDrawRollSilhouette(KCM_TFT &tft, int16_t cx, int16_t cy,
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
static void _rovrEraseSilhouette(KCM_TFT &tft, int16_t cx, int16_t cy) {
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
//   top_pad(4) + label(32) + gap(2) + silhouette_area + value(48) + bot_pad(4) = box_H
// Silhouette area height is (box_H - 90); silhouette centre is in the middle of that.
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

static void _rovrDrawPitchChrome(KCM_TFT &tft) {
    tft.drawRect(ROVR_RCOL_X, ROVR_PITCH_Y,
                 ROVR_RCOL_W, ROVR_PITCH_H,
                 TFT_GREY);
    textCenter(tft, &Roboto_Black_24,
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

static void _rovrDrawRollChrome(KCM_TFT &tft) {
    tft.drawRect(ROVR_RCOL_X, ROVR_ROLL_Y,
                 ROVR_RCOL_W, ROVR_ROLL_H,
                 TFT_GREY);
    textCenter(tft, &Roboto_Black_24,
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
static void _rovrUpdatePitch(KCM_TFT &tft) {
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
            eraseCenteredValue(tft, &Roboto_Black_36,
                                    ROVR_RCOL_X, ROVR_PITCH_VAL_Y, ROVR_RCOL_W, ROVR_TILT_VAL_H,
                                    oldBuf, TFT_BLACK);
        }
        char buf[12];
        snprintf(buf, sizeof(buf), "%+.1f\xB0", pitch);
        textCenter(tft, &Roboto_Black_36,
                   ROVR_RCOL_X, ROVR_PITCH_VAL_Y,
                   ROVR_RCOL_W, ROVR_TILT_VAL_H,
                   buf, valueColor, TFT_BLACK);
        _rovrPrevPitchVal   = iPitch;
        _rovrPrevPitchColor = valueColor;
    }
}

// Roll update — rear-view silhouette. Positive roll (bank right) → right side
// of silhouette down on screen (rotation angle = +roll directly).
static void _rovrUpdateRoll(KCM_TFT &tft) {
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
            eraseCenteredValue(tft, &Roboto_Black_36,
                                    ROVR_RCOL_X, ROVR_ROLL_VAL_Y, ROVR_RCOL_W, ROVR_TILT_VAL_H,
                                    oldBuf, TFT_BLACK);
        }
        char buf[12];
        snprintf(buf, sizeof(buf), "%+.1f\xB0", roll);
        textCenter(tft, &Roboto_Black_36,
                   ROVR_RCOL_X, ROVR_ROLL_VAL_Y,
                   ROVR_RCOL_W, ROVR_TILT_VAL_H,
                   buf, valueColor, TFT_BLACK);
        _rovrPrevRollVal   = iRoll;
        _rovrPrevRollColor = valueColor;
    }
}

// Heading readout — single-line value inside a bordered box above the nose
// triangle. Box border is drawn once in chrome; this function only touches
// the value text when the integer heading changes. The old value is erased with
// a hardware fillRect before the new value is drawn to handle digit-width changes.
//
// Format: zero-padded 3-digit unsigned heading with ° symbol (e.g. "045°").
static void _rovrUpdateHdgReadout(KCM_TFT &tft, float hdg) {
    int16_t iHdg = (int16_t)roundf(hdg) % 360;
    if (iHdg < 0) iHdg += 360;
    if (iHdg == _rovrPrevHdgVal) return;

    // Erase previous value (black-on-black) if one was drawn.
    // Inset by 2 px from the border on top and bottom so the glyph cell's
    // background fill doesn't overwrite the box border pixels.
    if (_rovrPrevHdgVal > -9000) {
        char oldBuf[8];
        snprintf(oldBuf, sizeof(oldBuf), "%03d\xB0", _rovrPrevHdgVal);
        eraseCenteredValue(tft, &Roboto_Black_36,
                                ROVR_HDG_BOX_X, ROVR_HDG_BOX_Y + 2,
                                ROVR_HDG_BOX_W, ROVR_HDG_BOX_H - 4,
                                oldBuf, TFT_BLACK);
    }

    // Draw new value, zero-padded to 3 digits
    char buf[8];
    snprintf(buf, sizeof(buf), "%03d\xB0", iHdg);
    textCenter(tft, &Roboto_Black_36,
               ROVR_HDG_BOX_X, ROVR_HDG_BOX_Y + 2,
               ROVR_HDG_BOX_W, ROVR_HDG_BOX_H - 4,
               buf, TFT_DARK_GREEN, TFT_BLACK);

    _rovrPrevHdgVal = iHdg;
}

// Full chrome draw — called once on screen entry. Erases the compass area (which
// includes the heading readout box region) and draws all stationary elements:
// outer ring, nose indicator, heading box border, rover silhouette. Then does the
// initial dynamic pass (ticks, labels, heading value, target triangle).
static void _rovrChromeCompass(KCM_TFT &tft) {
    _rovrEraseCompass(tft);

    // Stationary: outer ring
    compassDrawRing(tft, ROVR_GEOM);

    // Stationary: vessel heading indicator (triangle at top of ring, pointing inward)
    _rovrDrawNose(tft);

    // Stationary: heading readout box border (rectangle above the triangle).
    // The border is drawn here ONCE — _rovrUpdateHdgReadout only writes the value
    // text inside the box, never the border, so the border doesn't flicker.
    tft.drawRect(ROVR_HDG_BOX_X, ROVR_HDG_BOX_Y,
                 ROVR_HDG_BOX_W, ROVR_HDG_BOX_H, TFT_GREY);

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
// the erase — the tick ends sit 4 px inside the ring so it is never nibbled and
// needs no re-stamp. Heading readout updates via its own integer-change
// detection. Target triangle is handled separately by _rovrUpdateTarget because
// it sits in an annular band that doesn't overlap with ticks or labels.
//
// This avoids the large fillRect on every heading change that would otherwise
// cause visible flicker during continuous yaw.
static void _rovrUpdateCompass(KCM_TFT &tft) {
    // Erase old ticks and labels at previous heading
    _rovrDrawTicks(tft, _rovrPrevHeading, true);
    _rovrDrawLabels(tft, _rovrPrevHeading, true);

    // Draw new ticks and labels at current heading
    _rovrDrawTicks(tft, state.heading, false);
    _rovrDrawLabels(tft, state.heading, false);

    // (The outer ring is NOT re-stamped here: the tick ends sit 4 px inside it
    // (R_TICK_OUTER=196 vs R=200), so the tick-erase can't reach the ring. Skipping
    // the full software drawCircle every heading change saves ~1100 pixel writes.)

    // Update heading readout
    _rovrUpdateHdgReadout(tft, state.heading);

    _rovrPrevHeading = state.heading;
}


// ── Screen entry points ───────────────────────────────────────────────────────────────
static void chromeScreen_ROVR(KCM_TFT &tft) {
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

    // FWD / REV drive-state blocks — drawn from scratch (both blocks) on the first
    // update since _rovrPrevThrFill was reset to a sentinel above.
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

static void drawScreen_ROVR(KCM_TFT &tft) {
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
