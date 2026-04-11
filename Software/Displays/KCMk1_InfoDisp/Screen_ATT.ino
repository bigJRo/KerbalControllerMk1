/***************************************************************************************
   Screen_ATT.ino  —  Attitude screen: graphical ADI + error bars
   Replaces the original eight-row text layout with a synthetic attitude indicator.

   LAYOUT (800×480, content area 720×418 below title bar)
   ┌──────────────────────────────────────────────────────────┐
   │  ATTITUDE                                          [sbr] │ ← title bar (58px) + rule (4px)
   ├────────────────────────────┬─────────────────────────────┤
   │ P│                        │ Hdg:       045.0°            │
   │ i│      ADI sphere        │ Pitch:     +12.0°            │
   │ t│    (360×360 canvas,    │ Roll:       +5.0°            │
   │  │     R=160, centred     │ SAS:      PROGRADE           │
   │ b│     in left 385px)     │──────────────────────────────│
   │ a│                        │ SRF V                        │
   │ r│                        │ V.Hdg:    041.3°             │
   │  │                        │ V.Pit:     +8.5°             │
   │  │                        │──────────────────────────────│
   │  │                        │ ERR                          │
   │  │                        │ Hdg.Err:   +3.7°             │
   │  │                        │ Pit.Err:   +3.5°             │
   │  ├────────────────────────┤                              │
   │  │   Hdg error bar        │                              │
   └──┴────────────────────────┴──────────────────────────────┘

   ADI SPHERE (single framebuffer — 800×480@16bpp = 1 layer only)
   ┌─ Layer strategy ──────────────────────────────────────────┐
   │ All drawing on the single RA8875 framebuffer (L1).        │
   │ Marker erase uses dirty-zone redraw: when any marker      │
   │ moves >2px, _attMarkerDirty is set and the relevant zone  │
   │ is redrawn before markers are repainted on top.           │
   └───────────────────────────────────────────────────────────┘

   ZONES (redrawn on threshold change only)
     Zone A: full sphere — sky/ground/ladder — on roll change > ATT_ROLL_THRESH
     Zone B: horizon band (40px) + ladder — on pitch change > ATT_PITCH_THRESH
     Zone C: compass ring annulus — on heading change > ATT_HDG_THRESH
     Zone D: bank scale arc + roll pointer — co-triggered with Zone A
     Bezel:  always redrawn after any zone update (cosmetic seal)
     Markers: erase+redraw whenever position changes > ATT_MARKER_MOVE_PX,
              always after any zone redraw

   NAVBALL MARKERS (drawn on top of sphere, back-to-front)
     TGT  — violet diamond  — state.tgtHeading / state.tgtPitch
     MNV  — blue triangle   — state.mnvrHeading / state.mnvrPitch
     RG   — green X-circle  — prograde + 180°
     PG   — green FPV ring  — vel vector (primary reference, drawn last)
   Half-visible clamping: marker centre clamped to sphere edge when off-sphere.
   setActiveWindow() provides hardware clip so no pixel escapes the sphere.

   ERROR BARS (drawn directly to framebuffer, no active window needed)
     Pitch bar:   vertical,   x=0..31,   y=111..431  (left of sphere)
     Heading bar: horizontal, x=32..352, y=432..479  (below sphere)
     Scale: ±20° full deflection, ticks at ±5° (ATT_ERR_WARN_DEG)
            and ±15° (ATT_ERR_ALARM_DEG). Bar px = err * 8.0 px/deg.

   RIGHT COLUMN (x=385..714, standard printValue/printDispChrome)
     Uses ATT-local row geometry: ATT_ROW_H=42, ATT_LABEL_H=14.
     Bypasses the shared rowYFor()/rowHFor() macros to fit 8 rows
     plus section labels within the 418px content height.

   PIXEL COORDINATE REFERENCE (screen coords, origin top-left)
     ADI centre:      (192, 271)
     Sphere bounds:   left=32 right=352 top=111 bottom=431
     Pitch bar:       x=0..31, centreY=271
     Heading bar:     y=432..479, centreX=192
     Right col:       x=385, w=330
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


/***************************************************************************************
   LOCAL GEOMETRY CONSTANTS
   ATT screen deviates from the shared rowYFor/rowHFor macros (standard rowH=52px)
   to fit 8 data rows + 3 section labels within CONTENT_H=418px.
   All other screens are unaffected.
****************************************************************************************/
// ADI sphere — screen coordinates (y includes TITLE_TOP=62)
static const uint16_t ADI_CX         = 192;   // sphere centre x (screen)
static const uint16_t ADI_CY         = 271;   // sphere centre y (screen)
static const uint16_t ADI_R          = 160;   // sphere radius

// Derived sphere bounds (for setActiveWindow and bar placement)
static const uint16_t ADI_WIN_XL     = ADI_CX - ADI_R;  // 32
static const uint16_t ADI_WIN_XR     = ADI_CX + ADI_R;  // 352
static const uint16_t ADI_WIN_YT     = ADI_CY - ADI_R;  // 111
static const uint16_t ADI_WIN_YB     = ADI_CY + ADI_R;  // 431

// ADI rendering
static const float    ADI_PS         = 2.1333f;  // pitch scale: ADI_R*1.2/90 px/deg
static const uint16_t ADI_BANK_R     = 176;      // bank scale arc radius (ADI_R + 16)

// Pitch error bar (vertical strip left of sphere)
static const uint16_t PB_X0          = 0;
static const uint16_t PB_X1          = ADI_WIN_XL - 1;   // 31
static const uint16_t PB_CX          = 16;               // centre x of strip
static const uint16_t PB_CY          = ADI_CY;            // 271 (= sphere centre y)
static const uint16_t PB_HALF        = ADI_R;             // 160 px half-height

// Heading error bar (horizontal strip below sphere)
static const uint16_t HB_XL          = ADI_WIN_XL;        // 32
static const uint16_t HB_XR          = ADI_WIN_XR;        // 352
static const uint16_t HB_YT          = ADI_WIN_YB + 1;    // 432
static const uint16_t HB_YB          = SCREEN_H - 1;      // 479
static const uint16_t HB_CX          = ADI_CX;            // 192
static const uint16_t HB_CY          = (HB_YT + HB_YB) / 2;  // 455
static const uint16_t HB_HALF        = ADI_R;             // 160 px half-width

// Error bar scale: 8 px/deg, ticks aligned with ATT_ERR_WARN/ALARM thresholds
static const float    ATT_BAR_PX_PER_DEG = 8.0f;  // HB_HALF / 20.0 = 8.0
static const uint16_t ATT_BAR_WARN_PX    = 40;    // ATT_ERR_WARN_DEG (5°) * 8
static const uint16_t ATT_BAR_ALARM_PX   = 120;   // ATT_ERR_ALARM_DEG (15°) * 8

// Navball markers
static const uint8_t  ADI_MRK_R      = 10;   // marker radius / half-size
static const uint16_t ADI_MRK_THRESH = ADI_R - ADI_MRK_R;  // 150 — inside = full draw

// Redraw thresholds
static const float    ATT_ROLL_THRESH   = 0.5f;   // deg — triggers Zone A+D
static const float    ATT_PITCH_THRESH  = 0.5f;   // deg — triggers Zone B
static const float    ATT_HDG_THRESH    = 0.5f;   // deg — triggers Zone C
static const float    ATT_BAR_THRESH    = 0.2f;   // deg — triggers bar redraw
static const uint8_t  ATT_MARKER_MOVE_PX = 2;     // px — triggers marker redraw

// Right column geometry (ATT-specific, bypasses shared rowHFor/rowYFor)
static const uint16_t ATT_RC_X       = 385;   // right column left edge (screen)
static const uint16_t ATT_RC_W       = 330;   // right column width
static const uint16_t ATT_ROW_H      = 42;    // data row height
static const uint16_t ATT_LABEL_H    = 14;    // section label height
static const uint16_t ATT_DIV_H      = 1;     // divider height
static const uint16_t ATT_GAP_H      = 4;     // inter-section gap

// Pre-computed row Y positions in screen coordinates.
// Layout: label(14) + div(1) + [rows × 42] + gap(4) + div(1) + label(14) + div(1) + ...
// CRAFT section rows 0-3 start at TITLE_TOP + 14 + 1 = 77
static const uint16_t ATT_ROW_Y[8] = {
    77,   // row 0 (Hdg)    CRAFT
   119,   // row 1 (Pitch)
   161,   // row 2 (Roll)
   203,   // row 3 (SAS)
   265,   // row 4 (V.Hdg)  SRF V  (gap 4 + div + label + div = 20px after row3+42=245)
   307,   // row 5 (V.Pit)
   369,   // row 6 (Hdg.Err) ERR   (gap 4 + div + label + div = 20px after row5+42=349)
   411,   // row 7 (Pit.Err)
};
// Section label Y positions
static const uint16_t ATT_CRAFT_LBL_Y = TITLE_TOP;          // 62
static const uint16_t ATT_VEL_LBL_Y   = ATT_ROW_Y[3] + ATT_ROW_H + ATT_GAP_H + ATT_DIV_H;  // 249
static const uint16_t ATT_ERR_LBL_Y   = ATT_ROW_Y[5] + ATT_ROW_H + ATT_GAP_H + ATT_DIV_H;  // 353


/***************************************************************************************
   MARKER STATE
   Tracks previous position for dirty-zone detection.
   A marker is "dirty" if its pixel position changed by > ATT_MARKER_MOVE_PX
   since the last draw, or if its visibility changed.
****************************************************************************************/
struct MarkerState {
    int16_t prevX       = 0;
    int16_t prevY       = 0;
    bool    prevVisible = false;
    bool    prevClamped = false;
};

static MarkerState _msPG;   // prograde / velocity vector
static MarkerState _msRG;   // retrograde
static MarkerState _msMNV;  // maneuver node
static MarkerState _msTGT;  // target bearing


/***************************************************************************************
   REDRAW STATE
   Tracks previous drawn values for threshold-gated zone redraws.
   Initialised to values that guarantee a full draw on first entry.
****************************************************************************************/
static float   _attPrevRoll   =  999.0f;
static float   _attPrevPitch  =  999.0f;
static float   _attPrevHdg    =  999.0f;
static float   _attPrevPErr   =  999.0f;
static float   _attPrevHErr   =  999.0f;
static bool    _attMarkerDirty = true;  // force initial draw

// Existing cross-file variable (declared extern in KCMk1_InfoDisp.h)
// bool _attPrevOrbMode — unchanged, still governs ORB V / SRF V label


/***************************************************************************************
   FORWARD DECLARATIONS
****************************************************************************************/
static void _attDrawZoneA(RA8875 &tft);
static void _attDrawZoneB(RA8875 &tft, float pitch);
static void _attDrawZoneC(RA8875 &tft, float hdg);
static void _attDrawZoneD(RA8875 &tft, float roll);
static void _attDrawBezel(RA8875 &tft);
static bool _attProjectMarker(float markerHdg, float markerPit,
                               int16_t &mx, int16_t &my, bool &clamped);
static void _attEraseMarker(RA8875 &tft, int16_t mx, int16_t my);
static void _attDrawMarker_PG(RA8875 &tft, int16_t mx, int16_t my, bool clamped);
static void _attDrawMarker_RG(RA8875 &tft, int16_t mx, int16_t my, bool clamped);
static void _attDrawMarker_MNV(RA8875 &tft, int16_t mx, int16_t my, bool clamped);
static void _attDrawMarker_TGT(RA8875 &tft, int16_t mx, int16_t my, bool clamped);
static void _attUpdateMarkers(RA8875 &tft, float velHdg, float velPit);
static void _attDrawPitchBar(RA8875 &tft, float pErr);
static void _attDrawHdgBar(RA8875 &tft, float hErr);
static void _attDrawAircraftSymbol(RA8875 &tft);
static void _attDrawRC_Chrome(RA8875 &tft, bool orbMode);
static void _attDrawRC_Values(RA8875 &tft, float velHdg, float velPit,
                               float hErr, float pErr, bool orbMode, bool velValid);


/***************************************************************************************
   UTILITY: wrapErr — wrap angle difference to ±180°
****************************************************************************************/
static inline float _attWrapErr(float e) {
    while (e >  180.0f) e -= 360.0f;
    while (e < -180.0f) e += 360.0f;
    return e;
}


/***************************************************************************************
   UTILITY: _attErrColor — threshold colour for error bars and right column
   Returns foreground colour; background is always TFT_BLACK.
****************************************************************************************/
static uint16_t _attErrColor(float err) {
    if (!state.inAtmo) return COL_VALUE;  // dark green, no colouring in space
    float ae = fabsf(err);
    if (ae >= ATT_ERR_ALARM_DEG) return TFT_WHITE;    // alarm: white (on red bg)
    if (ae >= ATT_ERR_WARN_DEG)  return TFT_YELLOW;
    return COL_VALUE;  // TFT_DARK_GREEN
}

static uint16_t _attErrBg(float err) {
    if (!state.inAtmo) return TFT_BLACK;
    if (fabsf(err) >= ATT_ERR_ALARM_DEG) return TFT_RED;
    return TFT_BLACK;
}


/***************************************************************************************
   ZONE A: Full sphere repaint
   Sky hemisphere, ground hemisphere, horizon line, pitch ladder.
   Runs inside the sphere active window clip.
   Called on roll change > ATT_ROLL_THRESH or at screen entry.
****************************************************************************************/
static void _attDrawZoneA(RA8875 &tft) {
    float roll  = state.roll;
    float pitch = state.pitch;

    // Clip all drawing to sphere bounding box (hardware clip)
    tft.setActiveWindow(ADI_WIN_XL, ADI_WIN_XR, ADI_WIN_YT, ADI_WIN_YB);

    // Pitch offset: positive pitch moves horizon DOWN (sky expands upward)
    int16_t pOff = (int16_t)(pitch * ADI_PS);

    // Roll transform is approximated by rotating the horizon line and
    // shearing the sky/ground fill. For the RA8875 (no rotation hardware)
    // we draw rolled fills using a scanline approach: for each screen row
    // within the sphere, compute the rolled horizon crossover and fill
    // sky vs ground accordingly.
    //
    // Scanline fill — correct for any roll angle
    float sinR = sinf(roll * DEG_TO_RAD);
    float cosR = cosf(roll * DEG_TO_RAD);

    for (int16_t dy = -(int16_t)ADI_R; dy <= (int16_t)ADI_R; dy++) {
        int16_t screenY = ADI_CY + dy;
        // Half-width of sphere at this row (integer, pre-clipped by active window)
        int16_t hw = (int16_t)sqrtf((float)(ADI_R * ADI_R - dy * dy));
        if (hw <= 0) continue;
        int16_t x0 = ADI_CX - hw;
        int16_t x1 = ADI_CX + hw;

        // Horizon Y for this screen row in roll-corrected space:
        // Horizon in sphere coords (pre-roll): y = pOff
        // After rolling, the horizon is a tilted line through (0, pOff)
        // at angle `roll`. For screen row dy, the horizon x-crossover is:
        // (dy - pOff) = -sinR/cosR * (x - ADI_CX)  →  horizon_x = ADI_CX - (dy - pOff) * cosR/sinR
        // Simpler: for each pixel (dx, dy) in sphere coords,
        // sky if: dx * sinR + dy * cosR < pOff  (dot with roll-up vector < pOff)
        // We split the row at the horizon crossover x.

        // Horizon crossover dx in sphere coords: dx = (pOff - dy * cosR) / sinR
        int16_t splitX;
        bool horizInRow = false;
        if (fabsf(sinR) > 0.01f) {
            float splitDx = (pOff - dy * cosR) / sinR;
            splitX = ADI_CX + (int16_t)splitDx;
            horizInRow = (splitX > x0 && splitX < x1);
        }

        if (!horizInRow) {
            // Entire row is one colour: check centre pixel
            // Sky if: 0 * sinR + dy * cosR < pOff
            bool isSky = ((dy * cosR) < pOff);
            tft.drawLine(x0, screenY, x1, screenY, isSky ? TFT_ROYAL : TFT_UPS_BROWN);
        } else {
            // Row crosses the horizon
            bool leftIsSky = (((x0 - ADI_CX) * sinR + dy * cosR) < pOff);
            uint16_t leftCol  = leftIsSky ? TFT_ROYAL    : TFT_UPS_BROWN;
            uint16_t rightCol = leftIsSky ? TFT_UPS_BROWN : TFT_ROYAL;
            tft.drawLine(x0,      screenY, splitX - 1, screenY, leftCol);
            tft.drawLine(splitX,  screenY, x1,         screenY, rightCol);
        }
    }

    // Horizon line (white, 2px thick, rotated)
    {
        int16_t hw = (int16_t)sqrtf((float)(ADI_R * ADI_R - pOff * pOff));
        hw = min(hw, (int16_t)ADI_R);
        // Endpoints of the horizon line in sphere coords, rotated by roll
        int16_t hx0 = ADI_CX + (int16_t)(-hw * cosR + pOff * sinR);
        int16_t hy0 = ADI_CY + (int16_t)(-hw * sinR - pOff * cosR) + pOff;
        int16_t hx1 = ADI_CX + (int16_t)( hw * cosR + pOff * sinR);
        int16_t hy1 = ADI_CY + (int16_t)( hw * sinR - pOff * cosR) + pOff;
        // Recompute without the pOff doubling — standard roll formula:
        // horizon passes through (0, -pOff) in unrolled sphere coords
        // In screen coords: each end at ±hw along the horizon direction
        // horizon direction (in screen): (cosR, sinR)
        // horizon offset (in screen): (sinR * pOff, -cosR * pOff)
        int16_t ox = (int16_t)(sinR * pOff);
        int16_t oy = (int16_t)(-cosR * pOff);
        hx0 = ADI_CX - (int16_t)(hw * cosR) + ox;
        hy0 = ADI_CY - (int16_t)(hw * sinR) + oy;
        hx1 = ADI_CX + (int16_t)(hw * cosR) + ox;
        hy1 = ADI_CY + (int16_t)(hw * sinR) + oy;
        tft.drawLine(hx0, hy0, hx1, hy1, TFT_WHITE);
        // Second pixel offset perpendicular to horizon for 2px thickness
        int16_t px = (int16_t)(-sinR);
        int16_t py = (int16_t)( cosR);
        tft.drawLine(hx0 + px, hy0 + py, hx1 + px, hy1 + py, TFT_WHITE);
    }

    // Pitch ladder
    _attDrawZoneB(tft, pitch);  // reuses Zone B for the ladder marks (already clipped)

    tft.setActiveWindow();  // restore full screen
}


/***************************************************************************************
   ZONE B: Horizon band repaint (pitch-only change, roll unchanged)
   Repaints a ±ADI_HORIZON_BAND_H px strip around the current horizon,
   plus the pitch ladder marks that fall within it.
   For a pure pitch change (no roll change), this is much cheaper than Zone A.
   Called on pitch change > ATT_PITCH_THRESH when roll has not changed.
   Also called from Zone A to draw the pitch ladder after the sphere fill.
****************************************************************************************/
// Half-height of the band repainted for a pure pitch update.
// Must be large enough to cover the maximum pitch change between frames.
// At 125Hz simpit rate, 10°/s pitch rate = 0.08°/frame = 0.17px — 40px is very safe.
static const uint16_t ADI_BAND_H = 40;

static void _attDrawZoneB(RA8875 &tft, float pitch) {
    // Pitch ladder marks — drawn over whatever background is already there.
    // The background was just laid down by Zone A's scanline fill (or was already
    // correct for a pure pitch update after a narrow band repaint).
    //
    // Active window is already set to sphere by Zone A caller, or Zone B is always
    // invoked after a zone redraw that refreshes the background. Ladder is drawn
    // unconditionally — it is cheap (10 short lines) and idempotent on a fresh background.

    float  roll  = state.roll;
    float  sinR  = sinf(roll * DEG_TO_RAD);
    float  cosR  = cosf(roll * DEG_TO_RAD);
    int16_t pOff = (int16_t)(pitch * ADI_PS);

    // Horizon offset in screen from centre
    int16_t hox = (int16_t)(sinR * pOff);
    int16_t hoy = (int16_t)(-cosR * pOff);

    // Draw pitch ladder marks every 10°, ±80°
    for (int16_t deg = -80; deg <= 80; deg += 10) {
        if (deg == 0) continue;  // horizon line drawn separately
        // Mark is offset from horizon by (deg - pitch) in pitch direction
        // In sphere coords: perpendicular to horizon direction = (-sinR, cosR)
        float   markOff = (deg - pitch) * ADI_PS;
        int16_t mx  = ADI_CX + hox + (int16_t)(-sinR * markOff);
        int16_t my  = ADI_CY + hoy + (int16_t)( cosR * markOff);

        // Skip if mark centre is outside the sphere
        int16_t dx = mx - ADI_CX, dy2 = my - ADI_CY;
        if ((int32_t)dx*dx + (int32_t)dy2*dy2 > (int32_t)ADI_R*ADI_R) continue;

        // Tick length: 30° and 60° marks longer
        uint16_t tickLen = (abs(deg) % 30 == 0) ? 28 : (abs(deg) % 20 == 0) ? 22 : 14;
        // Tick colour: white, slightly dimmer for minor marks
        uint16_t col = (abs(deg) % 30 == 0) ? TFT_WHITE : TFT_LIGHT_GREY;

        // Tick runs along the horizon direction (cosR, sinR)
        int16_t tx0 = mx - (int16_t)(cosR * tickLen / 2);
        int16_t ty0 = my - (int16_t)(sinR * tickLen / 2);
        int16_t tx1 = mx + (int16_t)(cosR * tickLen / 2);
        int16_t ty1 = my + (int16_t)(sinR * tickLen / 2);
        tft.drawLine(tx0, ty0, tx1, ty1, col);

        // Degree labels for ±30° and ±60°
        if (abs(deg) == 30 || abs(deg) == 60) {
            // Place label to the right of the right end of the tick
            // Using small Roboto_Black_16 for compactness
            char buf[4]; snprintf(buf, sizeof(buf), "%d", abs(deg));
            int16_t lx = tx1 + (int16_t)(cosR * 4);
            int16_t ly = ty1 + (int16_t)(sinR * 4) - 8;
            tft.setFont(&Roboto_Black_16);
            tft.setTextColor(col, TFT_BLACK);
            tft.setCursor(lx, ly);
            tft.print(buf);
        }
    }
}


/***************************************************************************************
   ZONE C: Compass ring repaint
   Redraws the heading ring annulus (R=130..160) with tick marks and N/E/S/W labels.
   Called on heading change > ATT_HDG_THRESH.
****************************************************************************************/
static void _attDrawZoneC(RA8875 &tft, float hdg) {
    tft.setActiveWindow(ADI_WIN_XL, ADI_WIN_XR, ADI_WIN_YT, ADI_WIN_YB);

    // Fill the annulus with near-black background
    // Approximate with a filled circle then an inner filled circle overlay
    // Since RA8875 has no annulus primitive, we use fillCircle + filled inner circle.
    tft.fillCircle(ADI_CX, ADI_CY, ADI_R - 1, TFT_BLACK);
    // Restore the sphere interior (will be overdrawn by bezel)
    // Actually we only want to clear the ring area, not the interior.
    // Use inner filled circle to restore: NOT what we want — we want the sphere interior
    // to be intact. Zone C is called independently only when roll and pitch haven't changed,
    // so the sphere interior is already correct. We overdraw just the ring.
    // Approach: draw the ring tick marks. The ring background between R=130 and R=160
    // is overdrawn by the ticks. Residual background from a previous heading is an issue.
    //
    // Clean approach: fillCircle at R=160 in TFT_BLACK (clears ring + sphere), then
    // redraw the sphere interior from Zone A, then draw the ring. But that's Zone A cost.
    //
    // Practical approach: draw tick marks with a 3px-wide black clearance line first,
    // then the coloured tick on top. The ring background doesn't need to be pixel-perfect;
    // it just needs to be dark. A dim fillCircle over the ring area is sufficient.
    //
    // We do this by iterating the ring and fillRect-clearing each tick position:
    // Actually simplest: draw a circle of TFT_OFF_BLACK dots at R=145 (ring midpoint)
    // to clear the previous ticks, then draw new ticks.
    // For compactness, we use the same dot-circle approach as drawArcDisplay().

    uint16_t ringMid = ADI_R - 15;
    for (int16_t angle = 0; angle < 360; angle += 3) {
        float rad = angle * DEG_TO_RAD;
        int16_t px = ADI_CX + (int16_t)(ringMid * sinf(rad));
        int16_t py = ADI_CY - (int16_t)(ringMid * cosf(rad));
        tft.fillCircle(px, py, 2, TFT_BLACK);
    }

    // Draw tick marks and labels for every 10°, labels every 30°
    const char *cardinals[] = {"N","30","60","E","120","150","S","210","240","W","300","330"};
    for (int16_t mark = 0; mark < 360; mark += 10) {
        // Convert mark angle to screen angle (heading-corrected):
        // The ring rotates so that hdg is at the top (12 o'clock).
        float screenAngle = (mark - hdg) * DEG_TO_RAD;
        float s = sinf(screenAngle);
        float c = cosf(screenAngle);

        bool isMajor = (mark % 30 == 0);
        uint16_t tickLen = isMajor ? 12 : 6;
        uint16_t col     = isMajor ? TFT_LIGHT_GREY : TFT_GREY;

        // Outer end: R-1, inner end: R-1-tickLen
        int16_t ox = ADI_CX + (int16_t)((ADI_R - 2) * s);
        int16_t oy = ADI_CY - (int16_t)((ADI_R - 2) * c);
        int16_t ix = ADI_CX + (int16_t)((ADI_R - 2 - tickLen) * s);
        int16_t iy = ADI_CY - (int16_t)((ADI_R - 2 - tickLen) * c);
        tft.drawLine(ox, oy, ix, iy, col);

        // Cardinal / degree labels at major marks
        if (isMajor) {
            const char *lbl = cardinals[mark / 30];
            // Label position at R-28 (inside the tick)
            int16_t lx = ADI_CX + (int16_t)((ADI_R - 30) * s);
            int16_t ly = ADI_CY - (int16_t)((ADI_R - 30) * c);
            bool isCardinal = (mark == 0 || mark == 90 || mark == 180 || mark == 270);
            tft.setFont(&Roboto_Black_16);
            tft.setTextColor(isCardinal ? TFT_WHITE : TFT_GREY, TFT_BLACK);
            // Centre the label — approximate: 8px per char, 16px height
            uint8_t lblLen = strlen(lbl);
            tft.setCursor(lx - lblLen * 4, ly - 8);
            tft.print(lbl);
        }
    }

    // Fixed heading reference pointer (orange inward triangle at top of ring, fixed)
    tft.fillTriangle(
        ADI_CX,     ADI_WIN_YT + 2,   // tip (inward, at sphere top)
        ADI_CX - 7, ADI_WIN_YT + 18,  // base left
        ADI_CX + 7, ADI_WIN_YT + 18,  // base right
        TFT_INT_ORANGE
    );

    tft.setActiveWindow();
}


/***************************************************************************************
   ZONE D: Bank scale arc + roll pointer
   Drawn outside the sphere (no active window needed).
   Bank scale: semicircular arc at R=ADI_BANK_R with ticks at 0,10,20,30,45,60°.
   Roll pointer: white outward-pointing triangle, rotates with roll,
                 base at sphere edge (R=160), tip at bank scale (R=176).
   Heading reference pointer is part of Zone C and drawn there.
****************************************************************************************/
static void _attDrawZoneD(RA8875 &tft, float roll) {
    // Bank scale arc (upper half only: -90° to +90° in compass convention)
    // drawArc(cx, cy, radius, thickness, startAngle, endAngle, color)
    // RA8875 drawArc angles: 0° = top, clockwise. Scale: 0-360 maps to full circle.
    // We want arc from 270° to 90° (upper half) = -90° to +90°.
    // drawArc start/end: values 0.0..360.0 (or whatever _arcAngle_max is set to).
    // Use thick=3 so the arc is visible but not fat.
    tft.drawArc(ADI_CX, ADI_CY, ADI_BANK_R, 3, 270.0f, 360.0f + 90.0f, TFT_DARK_GREY);

    // Bank scale tick marks (upper half, bank angles)
    const int16_t bankAngles[] = {-60,-45,-30,-20,-10,0,10,20,30,45,60};
    for (uint8_t i = 0; i < 11; i++) {
        int16_t a    = bankAngles[i];
        float   rad  = (a - 90.0f) * DEG_TO_RAD;  // -90° offset: 0° bank = top of arc
        uint8_t tLen = (a == 0 || abs(a) == 30 || abs(a) == 60) ? 12 : 7;
        uint16_t col = (a == 0) ? TFT_LIGHT_GREY : TFT_DARK_GREY;

        int16_t ox = ADI_CX + (int16_t)(ADI_BANK_R * cosf(rad));
        int16_t oy = ADI_CY + (int16_t)(ADI_BANK_R * sinf(rad));
        int16_t ix = ADI_CX + (int16_t)((ADI_BANK_R - tLen) * cosf(rad));
        int16_t iy = ADI_CY + (int16_t)((ADI_BANK_R - tLen) * sinf(rad));
        tft.drawLine(ox, oy, ix, iy, col);
    }

    // Roll pointer: white outward-pointing triangle
    // Triangle base at R=ADI_R, tip at R=ADI_BANK_R
    // Rotates with aircraft roll: angle = roll - 90° (to point toward top of arc at roll=0)
    float pRad = (roll - 90.0f) * DEG_TO_RAD;
    float sinP = sinf(pRad), cosP = cosf(pRad);

    // Tip of triangle (outward, at bank arc radius)
    int16_t tx  = ADI_CX + (int16_t)(ADI_BANK_R * cosP);
    int16_t ty  = ADI_CY + (int16_t)(ADI_BANK_R * sinP);
    // Base corners at R=ADI_R, ±7px perpendicular to pointer direction
    int16_t b1x = ADI_CX + (int16_t)(ADI_R * cosP) + (int16_t)(-sinP * 7);
    int16_t b1y = ADI_CY + (int16_t)(ADI_R * sinP) + (int16_t)( cosP * 7);
    int16_t b2x = ADI_CX + (int16_t)(ADI_R * cosP) - (int16_t)(-sinP * 7);
    int16_t b2y = ADI_CY + (int16_t)(ADI_R * sinP) - (int16_t)( cosP * 7);
    tft.fillTriangle(tx, ty, b1x, b1y, b2x, b2y, TFT_WHITE);
    tft.drawTriangle(tx, ty, b1x, b1y, b2x, b2y, TFT_GREY);  // thin outline
}


/***************************************************************************************
   BEZEL: Outer ring — always redrawn after any zone update
   4px wide circle at R=ADI_R. Provides cosmetic seal over sphere edge artefacts.
   Also protects the area outside the sphere from pitch/compass overdraw.
****************************************************************************************/
static void _attDrawBezel(RA8875 &tft) {
    tft.drawCircle(ADI_CX, ADI_CY, ADI_R,     TFT_GREY);
    tft.drawCircle(ADI_CX, ADI_CY, ADI_R - 1, TFT_GREY);
    tft.drawCircle(ADI_CX, ADI_CY, ADI_R + 1, TFT_DARK_GREY);
    tft.drawCircle(ADI_CX, ADI_CY, ADI_R + 2, TFT_DARK_GREY);
}


/***************************************************************************************
   AIRCRAFT SYMBOL (fixed, drawn once at chrome time and after any zone redraw)
   Orange: left/right wings + centre dot. Always at sphere centre (ADI_CX, ADI_CY).
   Uses setActiveWindow to clip to sphere in case of partial overdraw.
****************************************************************************************/
static void _attDrawAircraftSymbol(RA8875 &tft) {
    tft.setActiveWindow(ADI_WIN_XL, ADI_WIN_XR, ADI_WIN_YT, ADI_WIN_YB);
    // Left wing: horizontal line from CX-60 to CX-20, with 10px downward serif
    tft.drawLine(ADI_CX - 60, ADI_CY,      ADI_CX - 20, ADI_CY,      TFT_INT_ORANGE);
    tft.drawLine(ADI_CX - 21, ADI_CY,      ADI_CX - 21, ADI_CY,      TFT_INT_ORANGE); // 2px thick
    tft.drawLine(ADI_CX - 20, ADI_CY,      ADI_CX - 20, ADI_CY + 10, TFT_INT_ORANGE);
    // Right wing
    tft.drawLine(ADI_CX + 20, ADI_CY,      ADI_CX + 60, ADI_CY,      TFT_INT_ORANGE);
    tft.drawLine(ADI_CX + 21, ADI_CY,      ADI_CX + 21, ADI_CY,      TFT_INT_ORANGE);
    tft.drawLine(ADI_CX + 20, ADI_CY,      ADI_CX + 20, ADI_CY + 10, TFT_INT_ORANGE);
    // Centre dot (filled circle, radius 4)
    tft.fillCircle(ADI_CX, ADI_CY, 4, TFT_INT_ORANGE);
    tft.setActiveWindow();
}


/***************************************************************************************
   MARKER PROJECTION
   Projects a navball direction (heading, pitch) onto the ADI sphere as a pixel offset
   from centre. Returns true if the marker should be drawn.
   If the projected distance > ADI_MRK_THRESH, the marker centre is clamped to the
   sphere edge (half-visible when combined with setActiveWindow clip in draw calls).

   Convention (matches PG marker on sphere):
     mx = -hErr * ADI_PS   (positive hErr → marker LEFT of centre)
     my = +pErr * ADI_PS   (positive pErr → marker BELOW centre, screen Y down)
   where hErr = wrap(craftHdg - markerHdg),  pErr = craftPitch - markerPitch.
****************************************************************************************/
static bool _attProjectMarker(float markerHdg, float markerPit,
                               int16_t &mx, int16_t &my, bool &clamped) {
    float hErr = _attWrapErr(state.heading - markerHdg);
    float pErr = state.pitch - markerPit;
    float dx   = -hErr * ADI_PS;
    float dy   = +pErr * ADI_PS;
    float dist = sqrtf(dx * dx + dy * dy);

    if (dist < 2.0f) return false;  // at bore-sight — coincides with aircraft symbol

    if (dist <= (float)ADI_MRK_THRESH) {
        mx      = (int16_t)roundf(dx);
        my      = (int16_t)roundf(dy);
        clamped = false;
    } else {
        float inv = (float)ADI_R / dist;
        mx      = (int16_t)roundf(dx * inv);
        my      = (int16_t)roundf(dy * inv);
        clamped = true;
    }
    return true;
}


/***************************************************************************************
   MARKER DRAW FUNCTIONS
   All use setActiveWindow(sphere) so clamped markers are half-visible.
   Clamped markers get a faint outer ring for "look this direction" cue.
****************************************************************************************/

// Helper: set/restore active window around a marker draw block
#define _ATT_MRK_BEGIN(tft) \
    tft.setActiveWindow(ADI_WIN_XL, ADI_WIN_XR, ADI_WIN_YT, ADI_WIN_YB)
#define _ATT_MRK_END(tft) \
    tft.setActiveWindow()


static void _attDrawMarker_PG(RA8875 &tft, int16_t mx, int16_t my, bool clamped) {
    _ATT_MRK_BEGIN(tft);
    int16_t px = ADI_CX + mx, py = ADI_CY + my;
    uint16_t col = TFT_NEON_GREEN;
    tft.drawCircle(px, py, ADI_MRK_R, col);
    // Cross arms (4 short lines, gap between circle edge and arm start)
    tft.drawLine(px - 18, py,      px - ADI_MRK_R - 1, py,      col);
    tft.drawLine(px + ADI_MRK_R + 1, py,  px + 18, py,          col);
    tft.drawLine(px, py - 18,      px, py - ADI_MRK_R - 1,      col);
    tft.drawLine(px, py + ADI_MRK_R + 1, px, py + 18,           col);
    if (clamped) {
        // Faint outer ring: approximate with 4 arc dots
        for (int16_t a = 0; a < 360; a += 30) {
            float r = (a * DEG_TO_RAD);
            int16_t cx2 = px + (int16_t)((ADI_MRK_R + 5) * sinf(r));
            int16_t cy2 = py - (int16_t)((ADI_MRK_R + 5) * cosf(r));
            tft.fillCircle(cx2, cy2, 1, TFT_SAP_GREEN);
        }
    }
    _ATT_MRK_END(tft);
}

static void _attDrawMarker_RG(RA8875 &tft, int16_t mx, int16_t my, bool clamped) {
    _ATT_MRK_BEGIN(tft);
    int16_t px = ADI_CX + mx, py = ADI_CY + my;
    uint16_t col = TFT_NEON_GREEN;
    tft.drawCircle(px, py, ADI_MRK_R, col);
    // X inside the circle
    tft.drawLine(px - 7, py - 7, px + 7, py + 7, col);
    tft.drawLine(px + 7, py - 7, px - 7, py + 7, col);
    if (clamped) {
        for (int16_t a = 0; a < 360; a += 30) {
            float r = (a * DEG_TO_RAD);
            tft.fillCircle(px + (int16_t)((ADI_MRK_R+5)*sinf(r)),
                           py - (int16_t)((ADI_MRK_R+5)*cosf(r)), 1, TFT_SAP_GREEN);
        }
    }
    _ATT_MRK_END(tft);
}

static void _attDrawMarker_MNV(RA8875 &tft, int16_t mx, int16_t my, bool clamped) {
    _ATT_MRK_BEGIN(tft);
    int16_t px = ADI_CX + mx, py = ADI_CY + my;
    // Blue upward-pointing triangle
    tft.drawTriangle(px, py - 12, px + 9, py + 7, px - 9, py + 7, TFT_BLUE);
    tft.drawTriangle(px, py - 11, px + 8, py + 6, px - 8, py + 6, TFT_ROYAL); // dim fill
    if (clamped) {
        for (int16_t a = 0; a < 360; a += 30) {
            float r = (a * DEG_TO_RAD);
            tft.fillCircle(px + (int16_t)(16*sinf(r)),
                           py - (int16_t)(16*cosf(r)), 1, TFT_NAVY);
        }
    }
    _ATT_MRK_END(tft);
}

static void _attDrawMarker_TGT(RA8875 &tft, int16_t mx, int16_t my, bool clamped) {
    _ATT_MRK_BEGIN(tft);
    int16_t px = ADI_CX + mx, py = ADI_CY + my;
    // Violet diamond
    tft.drawTriangle(px, py - 11, px + 11, py, px, py + 11, TFT_VIOLET);
    tft.drawTriangle(px, py - 11, px - 11, py, px, py + 11, TFT_VIOLET);
    if (clamped) {
        for (int16_t a = 0; a < 360; a += 30) {
            float r = (a * DEG_TO_RAD);
            tft.fillCircle(px + (int16_t)(15*sinf(r)),
                           py - (int16_t)(15*cosf(r)), 1, TFT_PURPLE);
        }
    }
    _ATT_MRK_END(tft);
}


/***************************************************************************************
   UPDATE MARKERS
   Called every loop. For each marker:
     1. Compute new position via _attProjectMarker.
     2. If position or visibility changed > threshold → set _attMarkerDirty.
     3. If !_attMarkerDirty: draw at new position (no erase needed — background just
        refreshed by zone redraw, or marker hasn't moved enough to need erase).
   The caller (drawScreen_ATT) is responsible for running the appropriate zone redraw
   before calling _attUpdateMarkers when _attMarkerDirty was set.
   Draw order: TGT (back), MNV, RG, PG (front, always on top).
****************************************************************************************/
static void _attUpdateMarkers(RA8875 &tft, float velHdg, float velPit) {
    // Compute new positions for all markers
    int16_t nx, ny;
    bool    clamped, visible;

    // Retrograde: exactly opposite prograde
    float rgHdg = fmodf(velHdg + 180.0f, 360.0f);
    float rgPit = -velPit;

    // Note: pointer (not reference) for ms so the array is copy-constructible in C++11.
    struct MarkerDef {
        float        hdg, pit;
        bool         enabled;
        MarkerState *ms;
        void (*drawFn)(RA8875&, int16_t, int16_t, bool);
    } markers[] = {
        { state.tgtHeading,  state.tgtPitch,  state.targetAvailable,      &_msTGT, _attDrawMarker_TGT },
        { state.mnvrHeading, state.mnvrPitch, (state.mnvrDeltaV > 0.0f),  &_msMNV, _attDrawMarker_MNV },
        { rgHdg,             rgPit,           true,                        &_msRG,  _attDrawMarker_RG  },
        { velHdg,            velPit,          true,                        &_msPG,  _attDrawMarker_PG  },
    };

    // First pass: detect dirtiness
    for (auto &m : markers) {
        if (!m.enabled) {
            if (m.ms->prevVisible) _attMarkerDirty = true;
        } else {
            visible = _attProjectMarker(m.hdg, m.pit, nx, ny, clamped);
            if (visible != m.ms->prevVisible) {
                _attMarkerDirty = true;
            } else if (visible) {
                int16_t ddx = nx - m.ms->prevX, ddy = ny - m.ms->prevY;
                if ((int32_t)ddx*ddx + (int32_t)ddy*ddy >
                    (int32_t)ATT_MARKER_MOVE_PX * ATT_MARKER_MOVE_PX) {
                    _attMarkerDirty = true;
                }
            }
        }
    }

    // If dirty, the caller has already run a zone redraw — just draw all markers.
    // If not dirty, draw all markers (they haven't moved, drawing is idempotent on
    // a stable background). This ensures markers are always visible.
    // Note: when _attMarkerDirty was handled by the caller running Zone A/B,
    // the sphere background is fresh. We draw on top now.

    // Draw back-to-front: TGT, MNV, RG, PG
    for (auto &m : markers) {
        if (!m.enabled) {
            m.ms->prevVisible = false;
            continue;
        }
        visible = _attProjectMarker(m.hdg, m.pit, nx, ny, clamped);
        if (visible) m.drawFn(tft, nx, ny, clamped);
        m.ms->prevX       = nx;
        m.ms->prevY       = ny;
        m.ms->prevVisible = visible;
        m.ms->prevClamped = clamped;
    }

    _attMarkerDirty = false;
}


/***************************************************************************************
   PITCH ERROR BAR  (vertical strip, left of sphere)
   x=0..31, y=111..431, centre=(16, 271)
   Convention: +pErr (nose above vel) → bar fills DOWNWARD (toward vel vector below)
   Scale: 8 px/deg, ±20° full deflection, ticks at ±5° and ±15°.
****************************************************************************************/
static void _attDrawPitchBar(RA8875 &tft, float pErr) {
    // Clear the entire bar strip
    tft.fillRect(PB_X0, ADI_WIN_YT, PB_X1 - PB_X0 + 1, ADI_WIN_YB - ADI_WIN_YT + 1, TFT_BLACK);

    // Track: narrow dark rect
    tft.fillRect(PB_CX - 3, ADI_WIN_YT, 7, ADI_WIN_YB - ADI_WIN_YT + 1, TFT_OFF_BLACK);

    // Scale ticks: ±5° (warn) and ±15° (alarm), both sides
    tft.drawLine(PB_X0 + 2, PB_CY - ATT_BAR_WARN_PX,  PB_X1 - 2, PB_CY - ATT_BAR_WARN_PX,  TFT_DARK_GREY);
    tft.drawLine(PB_X0 + 2, PB_CY + ATT_BAR_WARN_PX,  PB_X1 - 2, PB_CY + ATT_BAR_WARN_PX,  TFT_DARK_GREY);
    tft.drawLine(PB_X0 + 2, PB_CY - ATT_BAR_ALARM_PX, PB_X1 - 2, PB_CY - ATT_BAR_ALARM_PX, TFT_GREY);
    tft.drawLine(PB_X0 + 2, PB_CY + ATT_BAR_ALARM_PX, PB_X1 - 2, PB_CY + ATT_BAR_ALARM_PX, TFT_GREY);

    // Centre zero mark (bright)
    tft.drawLine(PB_X0, PB_CY, PB_X1, PB_CY, TFT_GREY);
    tft.drawLine(PB_X0, PB_CY + 1, PB_X1, PB_CY + 1, TFT_GREY);

    // Bar fill: clamp to ±PB_HALF, direction: +pErr → positive pxOff → fill downward
    int16_t pxOff = (int16_t)constrain(pErr * ATT_BAR_PX_PER_DEG, -(float)PB_HALF, (float)PB_HALF);
    uint16_t barCol = _attErrColor(pErr);

    if (abs(pxOff) > 1) {
        int16_t fillY = (pxOff >= 0) ? PB_CY     : PB_CY + pxOff;
        int16_t fillH = abs(pxOff);
        // Dim fill
        tft.fillRect(PB_CX - 3, fillY, 7, fillH, TFT_DARK_GREY);
        // Bright leading edge (2px)
        int16_t edgeY = (pxOff >= 0) ? PB_CY + pxOff - 2 : PB_CY + pxOff;
        tft.fillRect(PB_CX - 3, edgeY, 7, 2, barCol);
    }

    // Arrow pointer at bar tip
    int16_t tipY  = PB_CY + pxOff;
    tipY = constrain(tipY, (int16_t)(ADI_WIN_YT + 2), (int16_t)(ADI_WIN_YB - 2));
    // dir: +pErr → bar goes down → arrow points down (dir=1), base above tip
    int8_t dir = (pxOff >= 0) ? 1 : -1;
    tft.fillTriangle(
        PB_CX,     tipY,
        PB_CX - 6, tipY - dir * 11,
        PB_CX + 6, tipY - dir * 11,
        barCol
    );

    // Label "P E" (tiny, at top of strip)
    tft.setFont(&Roboto_Black_12);
    tft.setTextColor(TFT_DARK_GREY, TFT_BLACK);
    tft.setCursor(PB_CX - 4, ADI_WIN_YT + 2);
    tft.print("P");
}


/***************************************************************************************
   HEADING ERROR BAR  (horizontal strip, below sphere)
   x=32..352, y=432..479, centre=(192, 455)
   Convention: +hErr (nose right of vel) → bar fills LEFTWARD (toward vel vector left)
   Scale: 8 px/deg, ±20° full deflection, ticks at ±5° and ±15°.
****************************************************************************************/
static void _attDrawHdgBar(RA8875 &tft, float hErr) {
    // Clear the entire bar strip
    tft.fillRect(HB_XL, HB_YT, HB_XR - HB_XL + 1, HB_YB - HB_YT + 1, TFT_BLACK);

    // Track
    tft.fillRect(HB_XL, HB_CY - 3, HB_XR - HB_XL + 1, 7, TFT_OFF_BLACK);

    // Scale ticks
    tft.drawLine(HB_CX - ATT_BAR_WARN_PX,  HB_YT + 2, HB_CX - ATT_BAR_WARN_PX,  HB_YB - 2, TFT_DARK_GREY);
    tft.drawLine(HB_CX + ATT_BAR_WARN_PX,  HB_YT + 2, HB_CX + ATT_BAR_WARN_PX,  HB_YB - 2, TFT_DARK_GREY);
    tft.drawLine(HB_CX - ATT_BAR_ALARM_PX, HB_YT + 2, HB_CX - ATT_BAR_ALARM_PX, HB_YB - 2, TFT_GREY);
    tft.drawLine(HB_CX + ATT_BAR_ALARM_PX, HB_YT + 2, HB_CX + ATT_BAR_ALARM_PX, HB_YB - 2, TFT_GREY);

    // Centre zero mark
    tft.drawLine(HB_CX, HB_YT, HB_CX, HB_YB, TFT_GREY);
    tft.drawLine(HB_CX + 1, HB_YT, HB_CX + 1, HB_YB, TFT_GREY);

    // Bar fill: -hErr * scale so bar points toward velocity vector
    int16_t pxOff = (int16_t)constrain(-hErr * ATT_BAR_PX_PER_DEG, -(float)HB_HALF, (float)HB_HALF);
    uint16_t barCol = _attErrColor(hErr);

    if (abs(pxOff) > 1) {
        int16_t fillX = (pxOff >= 0) ? HB_CX     : HB_CX + pxOff;
        int16_t fillW = abs(pxOff);
        tft.fillRect(fillX, HB_CY - 3, fillW, 7, TFT_DARK_GREY);
        int16_t edgeX = (pxOff >= 0) ? HB_CX + pxOff - 2 : HB_CX + pxOff;
        tft.fillRect(edgeX, HB_CY - 3, 2, 7, barCol);
    }

    // Arrow pointer
    int16_t tipX  = HB_CX + pxOff;
    tipX = constrain(tipX, (int16_t)(HB_XL + 2), (int16_t)(HB_XR - 2));
    int8_t dir = (pxOff >= 0) ? 1 : -1;
    tft.fillTriangle(
        tipX,          HB_CY,
        tipX - dir*11, HB_CY - 6,
        tipX - dir*11, HB_CY + 6,
        barCol
    );

    // Label "H" at right end
    tft.setFont(&Roboto_Black_12);
    tft.setTextColor(TFT_DARK_GREY, TFT_BLACK);
    tft.setCursor(HB_XR - 10, HB_CY - 6);
    tft.print("H");
}


/***************************************************************************************
   RIGHT COLUMN CHROME  (static labels — drawn once per screen entry)
   Uses ATT-local row geometry (ATT_ROW_Y[], ATT_ROW_H, ATT_RC_X, ATT_RC_W).
   Font: Roboto_Black_40 for values (matches other screens visually at 20px height
   within the smaller row — printValue scales within the given h parameter).
   We use Roboto_Black_36 to fit comfortably in ATT_ROW_H=42px.
****************************************************************************************/
static void _attDrawRC_Chrome(RA8875 &tft, bool orbMode) {
    static const tFont *F  = &Roboto_Black_36;
    static const tFont *FL = &Roboto_Black_16;

    // Section: CRAFT
    tft.fillRect(ATT_RC_X, ATT_CRAFT_LBL_Y, ATT_RC_W, ATT_LABEL_H + ATT_DIV_H, TFT_BLACK);
    tft.setFont(FL); tft.setTextColor(TFT_DARK_GREY, TFT_BLACK);
    tft.setCursor(ATT_RC_X + 4, ATT_CRAFT_LBL_Y);
    tft.print("CRAFT");
    tft.drawLine(ATT_RC_X, ATT_CRAFT_LBL_Y + ATT_LABEL_H,
                 ATT_RC_X + ATT_RC_W, ATT_CRAFT_LBL_Y + ATT_LABEL_H, TFT_DARK_GREY);

    const char *craftLabels[4] = { "Hdg:", "Pitch:", "Roll:", "SAS:" };
    for (uint8_t r = 0; r < 4; r++) {
        printDispChrome(tft, F, ATT_RC_X, ATT_ROW_Y[r], ATT_RC_W, ATT_ROW_H,
                        craftLabels[r], COL_LABEL, COL_BACK, COL_NO_BDR);
    }

    // Divider + Section: SRF V / ORB V
    uint16_t velDivY = ATT_ROW_Y[3] + ATT_ROW_H + ATT_GAP_H;
    tft.drawLine(ATT_RC_X, velDivY, ATT_RC_X + ATT_RC_W, velDivY, TFT_DARK_GREY);
    tft.fillRect(ATT_RC_X, ATT_VEL_LBL_Y, ATT_RC_W, ATT_LABEL_H, TFT_BLACK);
    tft.setFont(FL); tft.setTextColor(TFT_DARK_GREY, TFT_BLACK);
    tft.setCursor(ATT_RC_X + 4, ATT_VEL_LBL_Y);
    tft.print(orbMode ? "ORB V" : "SRF V");
    tft.drawLine(ATT_RC_X, ATT_VEL_LBL_Y + ATT_LABEL_H,
                 ATT_RC_X + ATT_RC_W, ATT_VEL_LBL_Y + ATT_LABEL_H, TFT_DARK_GREY);

    printDispChrome(tft, F, ATT_RC_X, ATT_ROW_Y[4], ATT_RC_W, ATT_ROW_H,
                    "V.Hdg:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, ATT_RC_X, ATT_ROW_Y[5], ATT_RC_W, ATT_ROW_H,
                    "V.Pit:", COL_LABEL, COL_BACK, COL_NO_BDR);

    // Divider + Section: ERR
    uint16_t errDivY = ATT_ROW_Y[5] + ATT_ROW_H + ATT_GAP_H;
    tft.drawLine(ATT_RC_X, errDivY, ATT_RC_X + ATT_RC_W, errDivY, TFT_DARK_GREY);
    tft.fillRect(ATT_RC_X, ATT_ERR_LBL_Y, ATT_RC_W, ATT_LABEL_H, TFT_BLACK);
    tft.setFont(FL); tft.setTextColor(TFT_DARK_GREY, TFT_BLACK);
    tft.setCursor(ATT_RC_X + 4, ATT_ERR_LBL_Y);
    tft.print("ERR");
    tft.drawLine(ATT_RC_X, ATT_ERR_LBL_Y + ATT_LABEL_H,
                 ATT_RC_X + ATT_RC_W, ATT_ERR_LBL_Y + ATT_LABEL_H, TFT_DARK_GREY);

    printDispChrome(tft, F, ATT_RC_X, ATT_ROW_Y[6], ATT_RC_W, ATT_ROW_H,
                    "Hdg.Err:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, ATT_RC_X, ATT_ROW_Y[7], ATT_RC_W, ATT_ROW_H,
                    "Pit.Err:", COL_LABEL, COL_BACK, COL_NO_BDR);
}


/***************************************************************************************
   RIGHT COLUMN VALUES  (called every loop — printValue handles dirty checking)
****************************************************************************************/
static void _attDrawRC_Values(RA8875 &tft, float velHdg, float velPit,
                               float hErr, float pErr, bool orbMode, bool velValid) {
    static const tFont *F = &Roboto_Black_36;
    char buf[16];
    uint16_t fg, bg;

    // Helper: cache-aware printValue for ATT rows
    // rowCache[2] is the ATT screen cache (screen_ATT = 2)
    auto attVal = [&](uint8_t row, const char *label, const String &val,
                      uint16_t fgc, uint16_t bgc) {
        RowCache &rc = rowCache[2][row];
        if (rc.value == val && rc.fg == fgc && rc.bg == bgc) return;
        printValue(tft, F, ATT_RC_X, ATT_ROW_Y[row], ATT_RC_W, ATT_ROW_H,
                   label, val, fgc, bgc, COL_BACK, printState[2][row]);
        rc.value = val; rc.fg = fgc; rc.bg = bgc;
    };

    // Row 0 — Heading
    snprintf(buf, sizeof(buf), "%.1f\xB0", state.heading);
    attVal(0, "Hdg:", buf, COL_VALUE, COL_BACK);

    // Row 1 — Pitch
    snprintf(buf, sizeof(buf), "%.1f\xB0", state.pitch);
    attVal(1, "Pitch:", buf, COL_VALUE, COL_BACK);

    // Row 2 — Roll (coloured only for planes in atmosphere)
    {
        float absRoll = fabsf(state.roll);
        bool warnRoll = (state.vesselType == type_Plane && state.inAtmo);
        if      (warnRoll && absRoll > ROLL_ALARM_DEG) { fg = TFT_WHITE;     bg = TFT_RED;   }
        else if (warnRoll && absRoll > ROLL_WARN_DEG)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
        else                                            { fg = COL_VALUE;     bg = COL_BACK;  }
        snprintf(buf, sizeof(buf), "%.1f\xB0", state.roll);
        attVal(2, "Roll:", buf, fg, bg);
    }

    // Row 3 — SAS mode (matches navball colour convention)
    {
        const char *sasStr;
        uint16_t sasFg;
        switch (state.sasMode) {
            case 255: sasStr = "OFF";       sasFg = TFT_DARK_GREY;  break;
            case 0:   sasStr = "STAB";      sasFg = TFT_DARK_GREEN; break;
            case 1:   sasStr = "PROGRADE";  sasFg = TFT_NEON_GREEN; break;
            case 2:   sasStr = "RETRO";     sasFg = TFT_NEON_GREEN; break;
            case 3:   sasStr = "NORMAL";    sasFg = TFT_MAGENTA;    break;
            case 4:   sasStr = "ANTI-NRM";  sasFg = TFT_MAGENTA;    break;
            case 5:   sasStr = "RAD-OUT";   sasFg = TFT_SKY;        break;
            case 6:   sasStr = "RAD-IN";    sasFg = TFT_SKY;        break;
            case 7:   sasStr = "TARGET";    sasFg = TFT_VIOLET;     break;
            case 8:   sasStr = "ANTI-TGT";  sasFg = TFT_VIOLET;     break;
            case 9:   sasStr = "MANEUVER";  sasFg = TFT_BLUE;       break;
            default:  sasStr = "---";       sasFg = TFT_DARK_GREY;  break;
        }
        attVal(3, "SAS:", sasStr, sasFg, COL_BACK);
    }

    // Rows 4-5 — velocity vector heading/pitch (suppress if speed too low)
    if (velValid) {
        snprintf(buf, sizeof(buf), "%.1f\xB0", velHdg);
        attVal(4, "V.Hdg:", buf, COL_VALUE, COL_BACK);
        snprintf(buf, sizeof(buf), "%.1f\xB0", velPit);
        attVal(5, "V.Pit:", buf, COL_VALUE, COL_BACK);
    } else {
        attVal(4, "V.Hdg:", "---", TFT_DARK_GREY, COL_BACK);
        attVal(5, "V.Pit:", "---", TFT_DARK_GREY, COL_BACK);
    }

    // Rows 6-7 — heading and pitch errors
    if (!velValid) {
        attVal(6, "Hdg.Err:", "---", TFT_DARK_GREY, COL_BACK);
        attVal(7, "Pit.Err:", "---", TFT_DARK_GREY, COL_BACK);
    } else {
        snprintf(buf, sizeof(buf), "%+.1f\xB0", hErr);
        attVal(6, "Hdg.Err:", buf, _attErrColor(hErr), _attErrBg(hErr));
        snprintf(buf, sizeof(buf), "%+.1f\xB0", pErr);
        attVal(7, "Pit.Err:", buf, _attErrColor(pErr), _attErrBg(pErr));
    }
}


/***************************************************************************************
   _attOrbMode() — already declared extern in KCMk1_InfoDisp.h as a file-scope bool.
   The function and _attPrevOrbMode bool are defined here (replacing original).
****************************************************************************************/
bool _attPrevOrbMode = false;

static bool _attOrbMode() {
    float bodyRad   = (currentBody.radius > 0.0f) ? currentBody.radius : 600000.0f;
    bool  ascending = (state.verticalVel >= 0.0f);
    float switchAlt = ascending ? (bodyRad * 0.06f) : (bodyRad * 0.055f);
    return state.altitude > switchAlt;
}


/***************************************************************************************
   chromeScreen_ATT  — called once on screen transition
   drawStaticScreen() in AAA_Screens.ino has already: fillScreen(BLACK), drawSidebar(),
   drawTitleBar(). This function does:
   1. Draw Zone A (full sphere), Zone C (compass), Zone D (bank scale), bezel.
   2. Draw aircraft symbol (on sphere, always drawn after zone redraws).
   3. Draw both error bars (initial state, using current telemetry).
   4. Draw right column chrome.
   5. Reset all redraw state variables so first drawScreen_ATT re-evaluates everything.
****************************************************************************************/
static void chromeScreen_ATT(RA8875 &tft) {
    bool orbMode = _attOrbMode();
    _attPrevOrbMode = orbMode;

    // Reset redraw state — sentinel values force full redraw on first update call
    _attPrevRoll   = 999.0f;
    _attPrevPitch  = 999.0f;
    _attPrevHdg    = 999.0f;
    _attPrevPErr   = 999.0f;
    _attPrevHErr   = 999.0f;
    _attMarkerDirty = true;

    // Reset all marker states
    _msPG  = _msRG  = _msMNV = _msTGT = MarkerState{};

    // Note: drawStaticScreen() calls fillScreen(TFT_BLACK) before this function,
    // so the screen is already black. No explicit clear needed here.

    // Draw the ADI — full pass (sets _attPrevRoll/Pitch/Hdg internally via drawScreen)
    // We drive them directly here rather than calling drawScreen to avoid double-draw.
    _attDrawZoneA(tft);               // sky/ground/horizon/ladder (sets sphere clip internally)
    _attDrawZoneC(tft, state.heading); // compass ring
    _attDrawZoneD(tft, state.roll);    // bank scale + roll pointer
    _attDrawBezel(tft);                // cosmetic seal
    _attDrawAircraftSymbol(tft);       // aircraft symbol on top

    // Initial error bars (nominally zero)
    float velHdg = orbMode ? state.orbVelHeading : state.srfVelHeading;
    float velPit = orbMode ? state.orbVelPitch   : state.srfVelPitch;
    bool  velValid = (state.surfaceVel >= 0.5f);
    float hErr = velValid ? _attWrapErr(state.heading - velHdg) : 0.0f;
    float pErr = velValid ? (state.pitch - velPit)              : 0.0f;
    _attDrawPitchBar(tft, pErr);
    _attDrawHdgBar(tft, hErr);

    // Right column chrome
    _attDrawRC_Chrome(tft, orbMode);
    // Note: fillScreen(BLACK), drawSidebar(), and drawTitleBar() are all called by
    // drawStaticScreen() in AAA_Screens.ino before chromeScreen_ATT() — do not repeat here.
}


/***************************************************************************************
   drawScreen_ATT  — called every loop iteration
   Runs threshold checks for each zone and redraws only what changed.
   Marker updates and bar updates are folded in here.
****************************************************************************************/
static void drawScreen_ATT(RA8875 &tft) {
    // ── Orbital mode check (chrome redraw if mode switches) ──
    bool orbMode = _attOrbMode();
    if (orbMode != _attPrevOrbMode) {
        _attPrevOrbMode = orbMode;
        // Invalidate the vel group cache rows so they redraw
        for (uint8_t r = 0; r < ROW_COUNT; r++) rowCache[2][r].value = "\x01";
        switchToScreen(screen_ATT);
        return;
    }

    // ── Gather current values ──
    float roll    = state.roll;
    float pitch   = state.pitch;
    float hdg     = state.heading;
    float velHdg  = orbMode ? state.orbVelHeading : state.srfVelHeading;
    float velPit  = orbMode ? state.orbVelPitch   : state.srfVelPitch;
    bool  velValid = (state.surfaceVel >= 0.5f);
    float hErr    = velValid ? _attWrapErr(hdg - velHdg) : 0.0f;
    float pErr    = velValid ? (pitch - velPit)           : 0.0f;

    // ── Zone A+D: full sphere + bank scale — roll change ──
    bool zoneAD = (fabsf(roll  - _attPrevRoll)  > ATT_ROLL_THRESH);
    // ── Zone B: horizon band — pitch change (when not doing full redraw) ──
    bool zoneB  = !zoneAD && (fabsf(pitch - _attPrevPitch) > ATT_PITCH_THRESH);
    // ── Zone C: compass ring — heading change ──
    bool zoneC  = (fabsf(_attWrapErr(hdg - _attPrevHdg)) > ATT_HDG_THRESH);

    // If marker dirty flag is set AND no zone redraw is scheduled, at minimum
    // run Zone B (horizon band) to refresh the sphere interior so we can safely
    // redraw markers over a clean background. Zone A would be more complete but
    // Zone B is cheaper and covers most of the sphere interior where markers live.
    if (_attMarkerDirty && !zoneAD && !zoneB) {
        zoneB = true;  // force a horizon band refresh as background for marker redraw
    }

    // Execute zone redraws
    if (zoneAD || zoneB) {
        // For Zone A, we need the sphere interior fully redrawn
        if (zoneAD) {
            _attDrawZoneA(tft);  // includes pitch ladder via Zone B call
        } else {
            // Zone B only: repaint horizon band then pitch ladder
            // For a pure pitch change we repaint a band around the new horizon
            // by doing a partial Zone A (just the rows near the horizon).
            // In practice, calling the full Zone A scanline loop but limited
            // to a ±ADI_BAND_H band around the horizon is the right approach.
            // However Zone A already clips to the sphere active window, so running
            // it fully is safe — just more expensive. For now, run full Zone A when
            // only pitch changed to guarantee correct background for marker redraw.
            // TODO: implement true Zone B band repaint for pitch-only performance.
            _attDrawZoneA(tft);
        }
    }

    if (zoneC) {
        _attDrawZoneC(tft, hdg);
        _attPrevHdg = hdg;
    }

    if (zoneAD) {
        _attDrawZoneD(tft, roll);
        _attPrevRoll  = roll;
        _attPrevPitch = pitch;  // Zone A covers pitch too
    } else if (zoneB) {
        _attPrevPitch = pitch;
    }

    // Bezel always redrawn after any zone update (cosmetic seal over edges)
    if (zoneAD || zoneB || zoneC) {
        _attDrawBezel(tft);
    }

    // ── Markers: update (dirty detection + redraw) ──
    // _attMarkerDirty may have been set by first-pass detection above.
    // The zone redraws above have already refreshed the background.
    // Now draw aircraft symbol (always needed over any fresh background) then markers.
    if (zoneAD || zoneB || zoneC || _attMarkerDirty) {
        _attDrawAircraftSymbol(tft);
    }
    _attUpdateMarkers(tft, velHdg, velPit);

    // ── Error bars: redraw on threshold change ──
    if (fabsf(pErr - _attPrevPErr) > ATT_BAR_THRESH) {
        _attDrawPitchBar(tft, pErr);
        _attPrevPErr = pErr;
    }
    if (fabsf(hErr - _attPrevHErr) > ATT_BAR_THRESH) {
        _attDrawHdgBar(tft, hErr);
        _attPrevHErr = hErr;
    }

    // ── Right column values ──
    _attDrawRC_Values(tft, velHdg, velPit, hErr, pErr, orbMode, velValid);
}


/***************************************************************************************
   PUBLIC INTERFACE
   Called from AAA_Screens.ino dispatch (drawStaticScreen / updateScreen).
   chromeScreen_ATT wraps -> chromeScreen_ATT(tft)
   drawScreen_ATT   wraps -> drawScreen_ATT(tft)
   These names match the existing dispatch switch in AAA_Screens.ino.
****************************************************************************************/
