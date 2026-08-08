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
// Geometry mirrors the ACFT (aircraft) screen exactly — same ball centre, radius,
// and readout panel — so the two attitude screens are visually identical. SCFT
// omits the VSI, slip, and AoA indicators.
static const int16_t  SCFT_CX        = 345;
static const int16_t  SCFT_CY        = 300;
static const int16_t  SCFT_R         = 206;
static const float    SCFT_SCALE     = (float)SCFT_R / 30.0f;   // 6.867 px/deg
// (ball extents/scanlines + the sky/ground/horizon/wings/ladder colours and the
//  SCFT_BX_ALLSKY/ALLGND sentinels now live in the shared EADIBall.ino renderer.)

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



// ── Orbital mode helper ───────────────────────────────────────────────────────────────
static bool _scftOrbMode() {
    float bodyRad   = (currentBody.radius > 0.0f) ? currentBody.radius : DEFAULT_BODY_RADIUS_M;
    bool  ascending = (state.verticalVel >= 0.0f);
    float switchAlt = ascending ? (bodyRad * ORB_SWITCH_ALT_FRAC_ASC) : (bodyRad * ORB_SWITCH_ALT_FRAC_DESC);
    return state.altitude > switchAlt;
}





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


// Roll readout — two lines, right-justified toward the panel divider (matches ACFT).
// Label: Roboto_Black_24, Value: Roboto_Black_28.
static const int16_t  SCFT_ROLL_ANCHOR_X  = SCFT_CX + SCFT_R - 54; // right edge tucked by the divider
static const int16_t  SCFT_ROLL_ANCHOR_Y  = TITLE_TOP;             // pinned just below the title rule
static const int16_t  SCFT_ROLL_W         = 80;   // block width ("+180°" _28 = 74px fits)
static const int16_t  SCFT_ROLL_TXT_W     = SCFT_ROLL_W + 6;   // text-justify reference (matches ACFT)
static const int16_t  SCFT_ROLL_LABEL_H   = 30;   // label line height (Roboto_Black_24, cap 29)
static const int16_t  SCFT_ROLL_VALUE_H   = 38;   // value line height (Roboto_Black_28, cap 33)
static const int16_t  SCFT_ROLL_GAP       = 3;    // gap between lines

// Update the roll numeric readout.
static void _scftUpdateRollReadout(KCM_TFT &tft, float roll) {
    int16_t  iRoll = (int16_t)roundf(roll);
    uint16_t fg    = TFT_DARK_GREEN;  // spacecraft — no roll warnings
    uint16_t bg    = TFT_BLACK;

    if (iRoll == _scftPrevRollReadout && fg == _scftPrevRollReadoutFg) return;

    // Erase previous value — right-justified glyph box (matches textRight below).
    if (_scftPrevRollReadout > -9000) {
        char oldBuf[8];
        snprintf(oldBuf, sizeof(oldBuf), "%+d\xB0", _scftPrevRollReadout);
        int16_t ow   = getFontStringWidth(&Roboto_Black_28, oldBuf);
        int16_t capH = (int16_t)Roboto_Black_28.cap_height;
        int16_t ex   = SCFT_ROLL_ANCHOR_X + SCFT_ROLL_TXT_W - ow - TEXT_BORDER;
        int16_t ey   = (SCFT_ROLL_ANCHOR_Y + SCFT_ROLL_LABEL_H + SCFT_ROLL_GAP)
                       + (SCFT_ROLL_VALUE_H - capH) / 2;
        tft.fillRect(ex - 1, ey, ow + 2, capH, TFT_BLACK);
    }

    // Line 1: "Roll:" — label row, right-justified toward the panel divider
    textRight(tft, &Roboto_Black_24,
              SCFT_ROLL_ANCHOR_X, SCFT_ROLL_ANCHOR_Y,
              SCFT_ROLL_TXT_W, SCFT_ROLL_LABEL_H,
              "Roll:", TFT_WHITE, TFT_BLACK);

    // Line 2: signed value — larger font, right-justified in value row
    char buf[8];
    snprintf(buf, sizeof(buf), "%+d\xB0", iRoll);
    textRight(tft, &Roboto_Black_28,
              SCFT_ROLL_ANCHOR_X, SCFT_ROLL_ANCHOR_Y + SCFT_ROLL_LABEL_H + SCFT_ROLL_GAP,
              SCFT_ROLL_TXT_W, SCFT_ROLL_VALUE_H,
              buf, fg, bg);

    _scftPrevRollReadout   = iRoll;
    _scftPrevRollReadoutFg = fg;
}

// ── Pitch tape ────────────────────────────────────────────────────────────────────────
// Shared availability state (also used by heading tape)
static bool    _scftPrevTgtAvail   = false;
static bool    _scftPrevMnvrActive = false;
// Vertical tape on the left side of the disc. Same scale as the pitch ladder.
// ±30° visible, ticks at every 5° (minor) and 10° (major).
// Current value box centred on disc centre (pitch=0 line).
// Markers: left-pointing triangles on the right edge for vel/tgt/mnvr pitch.

static const int16_t  SCFT_PTAPE_W       = 36;
static const int16_t  SCFT_PTAPE_GAP     = 27;                          // right edge aligns with HDG tape left
static const int16_t  SCFT_PTAPE_X       = SCFT_CX - SCFT_R - SCFT_PTAPE_GAP - SCFT_PTAPE_W; // 133
static const int16_t  SCFT_PTAPE_Y       = SCFT_CY - SCFT_R;              // 96 — top of disc
static const int16_t  SCFT_PTAPE_H       = SCFT_CY + SCFT_R + 8 - (SCFT_CY - SCFT_R); // 336 — bottom aligns with HDG tape top (CY+R+8)
static const float    SCFT_PTAPE_SCALE   = SCFT_SCALE;

// Current value box — centred vertically on disc centre (pitch=0)
static const int16_t  SCFT_PTAPE_BOX_W   = 68;
static const int16_t  SCFT_PTAPE_BOX_H   = 38;                          // taller for comfortable text margin
static const int16_t  SCFT_PTAPE_BOX_X   = SCFT_PTAPE_X + SCFT_PTAPE_W - 68; // right edge flush with tape right
static const int16_t  SCFT_PTAPE_BOX_Y   = SCFT_CY - SCFT_PTAPE_BOX_H / 2; // 241

// Suppress zone — ticks/labels suppressed near the value box
static const int16_t  SCFT_PTAPE_SUPP_LO = SCFT_PTAPE_BOX_Y - 10;
static const int16_t  SCFT_PTAPE_SUPP_HI = SCFT_PTAPE_BOX_Y + SCFT_PTAPE_BOX_H + 10;

// Markers — left-pointing triangles on right edge of tape
static const int16_t  SCFT_PTAPE_MRK_BASE_X = SCFT_PTAPE_X + SCFT_PTAPE_W - 2;
static const int16_t  SCFT_PTAPE_MRK_TIP_X  = SCFT_PTAPE_X + SCFT_PTAPE_W - 22; // 20px (matches ACFT)
static const int16_t  SCFT_PTAPE_MRK_HW     = 9;

// State
static float   _scftPrevPitch2      = -9999.0f;   // pitch tape (distinct from ball state)
static int16_t _scftPrevPitchBox    = -9999;
static float   _scftPrevVelPitch    = -9999.0f;
static float   _scftPrevTgtPitch    = -9999.0f;
static float   _scftPrevMnvrPitch   = -9999.0f;

// Draw/update the pitch value box — cached on integer change.
static void _scftUpdatePitchBox(KCM_TFT &tft, float pitch) {
    int16_t iPitch = (int16_t)roundf(pitch);
    if (iPitch == _scftPrevPitchBox) return;

    char newBuf[8];
    snprintf(newBuf, sizeof(newBuf), "%+d\xB0", iPitch);

    if (_scftPrevPitchBox > -9000) {
        char oldBuf[8];
        snprintf(oldBuf, sizeof(oldBuf), "%+d\xB0", _scftPrevPitchBox);
        eraseCenteredValue(tft, &Roboto_Black_28,
                   SCFT_PTAPE_BOX_X, SCFT_PTAPE_BOX_Y + 1,
                   SCFT_PTAPE_BOX_W, SCFT_PTAPE_BOX_H - 2,
                   oldBuf, TFT_BLACK);
    }
    textCenter(tft, &Roboto_Black_28,
               SCFT_PTAPE_BOX_X, SCFT_PTAPE_BOX_Y + 1,
               SCFT_PTAPE_BOX_W, SCFT_PTAPE_BOX_H - 2,
               newBuf, TFT_DARK_GREEN, TFT_BLACK);

    _scftPrevPitchBox = iPitch;
}

// Draw the full pitch tape for the given pitch.
static void _scftDrawPitchTape(KCM_TFT &tft, float pitch) {
    // Clear tape in two passes, skipping the box area and staying 1px inside borders
    int16_t fillW  = SCFT_PTAPE_W - 1;  // stop 1px short of right border
    int16_t aboveH = SCFT_PTAPE_BOX_Y - SCFT_PTAPE_Y;
    int16_t belowY = SCFT_PTAPE_BOX_Y + SCFT_PTAPE_BOX_H;
    int16_t belowH = (SCFT_PTAPE_Y + SCFT_PTAPE_H - 1) - belowY;  // stop 1px short of bottom border
    tft.fillRect(SCFT_PTAPE_X, SCFT_PTAPE_Y, fillW, aboveH, TFT_BLACK);
    tft.fillRect(SCFT_PTAPE_X, belowY,       fillW, belowH, TFT_BLACK);

    // Redraw box border (sides may have been touched by above/below fills)
    tft.drawRect(SCFT_PTAPE_BOX_X, SCFT_PTAPE_BOX_Y, SCFT_PTAPE_BOX_W, SCFT_PTAPE_BOX_H, TFT_LIGHT_GREY);
    // Box interior not erased — no need to reset _scftPrevPitchBox

    tft.setFont(Roboto_Black_12);

    // Draw ticks from pitch-32 to pitch+32 (slightly beyond ±30° visible range)
    for (int16_t dp = -32; dp <= 32; dp++) {
        float deg = pitch + (float)dp;
        if (deg < -90.0f || deg > 90.0f) continue;  // KSP pitch clamped ±90°

        // Pixel y: current pitch stays at centre (SCFT_CY), offset by dp degrees
        int16_t py = (int16_t)(SCFT_CY - (float)dp * SCFT_PTAPE_SCALE);

        // Clip to tape interior
        if (py <= SCFT_PTAPE_Y || py >= SCFT_PTAPE_Y + SCFT_PTAPE_H) continue;

        // Suppress near value box
        if (py >= SCFT_PTAPE_SUPP_LO && py <= SCFT_PTAPE_SUPP_HI) continue;

        int16_t ideg = (int16_t)roundf(deg);

        if (ideg % 10 == 0) {
            // Major tick — right-aligned, stopping 1px short of right border
            int16_t tx0 = SCFT_PTAPE_X + SCFT_PTAPE_W - 11;
            int16_t tx1 = SCFT_PTAPE_X + SCFT_PTAPE_W - 2;
            tft.drawLine(tx0, py, tx1, py, TFT_LIGHT_GREY);

            // Label — left of tick, clamped to tape
            char lbl[8];
            snprintf(lbl, sizeof(lbl), "%+d", ideg);
            int16_t lx = SCFT_PTAPE_X + 2;
            int16_t ly = py - 6;
            if (ly < SCFT_PTAPE_Y + 1) ly = SCFT_PTAPE_Y + 1;
            if (ly + 12 > SCFT_PTAPE_Y + SCFT_PTAPE_H - 3)
                ly = SCFT_PTAPE_Y + SCFT_PTAPE_H - 15;
            // Only draw if label y is not in suppress zone
            if (!(ly + 6 >= SCFT_PTAPE_SUPP_LO && ly + 6 <= SCFT_PTAPE_SUPP_HI)) {
                tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
                tft.setCursor(lx, ly);
                tft.print(lbl);
            }
        } else if (ideg % 2 == 0) {
            // Minor tick (every 2°) — stopping 1px short of right border
            int16_t tx0 = SCFT_PTAPE_X + SCFT_PTAPE_W - 7;
            int16_t tx1 = SCFT_PTAPE_X + SCFT_PTAPE_W - 2;
            tft.drawLine(tx0, py, tx1, py, TFT_DARK_GREY);
        }
    }

    // Redraw the tape's bottom border — the lowest number labels' opaque black
    // background can paint over it, and it is otherwise only drawn once in chrome.
    tft.drawLine(SCFT_PTAPE_X - 1,                SCFT_PTAPE_Y + SCFT_PTAPE_H - 1,
                 SCFT_PTAPE_X + SCFT_PTAPE_W - 1, SCFT_PTAPE_Y + SCFT_PTAPE_H - 1, TFT_LIGHT_GREY);

    // Draw pitch markers (left-pointing triangles on right edge)
    auto drawPitchMarker = [&](float markerPitch, uint16_t col) {
        float diff = markerPitch - pitch;
        int16_t py = (int16_t)(SCFT_CY - diff * SCFT_PTAPE_SCALE);
        // Peg to tape edges rather than hiding
        int16_t pyMin = SCFT_PTAPE_Y + SCFT_PTAPE_MRK_HW + 1;
        int16_t pyMax = SCFT_PTAPE_Y + SCFT_PTAPE_H - SCFT_PTAPE_MRK_HW - 2;
        if (py < pyMin) py = pyMin;
        if (py > pyMax) py = pyMax;
        if (py >= SCFT_PTAPE_SUPP_LO && py <= SCFT_PTAPE_SUPP_HI) return;
        tft.fillTriangle(SCFT_PTAPE_MRK_TIP_X,  py,
                         SCFT_PTAPE_MRK_BASE_X,  py - SCFT_PTAPE_MRK_HW,
                         SCFT_PTAPE_MRK_BASE_X,  py + SCFT_PTAPE_MRK_HW,
                         col);
    };

    drawPitchMarker(_scftVelPitch, TFT_NEON_GREEN);
    if (state.targetAvailable)
        drawPitchMarker(state.tgtPitch, TFT_VIOLET);
    if (state.mnvrTime > 0.0f)
        drawPitchMarker(state.mnvrPitch, TFT_BLUE);
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
static const int16_t  SCFT_HDG_TAPE_W    = (SCFT_R * 2) + 54;              // 382 — ±35° visible
static const int16_t  SCFT_HDG_TAPE_X    = SCFT_CX - (SCFT_HDG_TAPE_W / 2); // 169
static const int16_t  SCFT_HDG_TAPE_Y    = SCFT_CY + SCFT_R + 8;    // 432 — 8px below disc
static const int16_t  SCFT_HDG_TAPE_H    = 32;
static const float    SCFT_HDG_SCALE     = (float)(SCFT_R * 2) / 60.0f;
static const int16_t  SCFT_HDG_LABEL_LO  = SCFT_HDG_TAPE_X + 8;
static const int16_t  SCFT_HDG_LABEL_HI  = SCFT_HDG_TAPE_X + SCFT_HDG_TAPE_W - 8;

// Box — top aligned with tape top, extends 8px BELOW tape bottom so its
// bottom border is outside the fillRect zone and never flickers
static const int16_t  SCFT_HDG_BOX_W     = 72;
static const int16_t  SCFT_HDG_BOX_H     = 40;                     // TAPE_H + 8 = 40
static const int16_t  SCFT_HDG_BOX_X     = SCFT_CX - (SCFT_HDG_BOX_W / 2);
static const int16_t  SCFT_HDG_BOX_Y     = SCFT_HDG_TAPE_Y;

// Suppress zone — covers box + max label half-width (12px)
static const int16_t  SCFT_HDG_SUPP_LO   = SCFT_HDG_BOX_X - 18;
static const int16_t  SCFT_HDG_SUPP_HI   = SCFT_HDG_BOX_X + SCFT_HDG_BOX_W + 18;

// Heading markers — long thin downward triangles fully inside the tape
static const int16_t  SCFT_HDG_MRK_BASE_Y = SCFT_HDG_TAPE_Y + 2;   // 2px below tape top
static const int16_t  SCFT_HDG_MRK_TIP_Y  = SCFT_HDG_TAPE_Y + 24;  // 22px tall (matches ACFT)
static const int16_t  SCFT_HDG_MRK_HW     = 9;                     // half-width → 19px wide

// Draw/update the heading number box — cached, only redraws when integer heading changes.
// Uses textCenter for flicker-free rendering: erase old value with black-on-black first.
static void _scftUpdateHdgBox(KCM_TFT &tft, float hdg) {
    int16_t iHdg = (int16_t)roundf(hdg) % 360;
    if (iHdg < 0) iHdg += 360;
    if (iHdg == _scftPrevHdgBox) return;

    char oldBuf[8], newBuf[8];
    snprintf(newBuf, sizeof(newBuf), "%03d\xB0", iHdg);

    // Erase previous value with black-on-black
    if (_scftPrevHdgBox >= 0) {
        snprintf(oldBuf, sizeof(oldBuf), "%03d\xB0", _scftPrevHdgBox);
        eraseCenteredValue(tft, &Roboto_Black_28,
                   SCFT_HDG_BOX_X, SCFT_HDG_BOX_Y + 1,
                   SCFT_HDG_BOX_W, SCFT_HDG_BOX_H - 2,
                   oldBuf, TFT_BLACK);
    }

    // Draw new value
    textCenter(tft, &Roboto_Black_28,
               SCFT_HDG_BOX_X, SCFT_HDG_BOX_Y + 1,
               SCFT_HDG_BOX_W, SCFT_HDG_BOX_H - 2,
               newBuf, TFT_DARK_GREEN, TFT_BLACK);

    _scftPrevHdgBox = iHdg;
}

// Draw the full heading tape. Only the tape strip — box is handled separately.
static void _scftDrawHeadingTape(KCM_TFT &tft, float hdg) {
    while (hdg <   0.0f) hdg += 360.0f;
    while (hdg >= 360.0f) hdg -= 360.0f;

    tft.fillRect(SCFT_HDG_TAPE_X, SCFT_HDG_TAPE_Y, SCFT_HDG_TAPE_W, SCFT_HDG_TAPE_H, TFT_BLACK);

    // Redraw box border after fill (fill erases box sides where they overlap)
    tft.drawRect(SCFT_HDG_BOX_X, SCFT_HDG_BOX_Y, SCFT_HDG_BOX_W, SCFT_HDG_BOX_H, TFT_LIGHT_GREY);

    // Force box number to redraw — fill blackened the interior
    _scftPrevHdgBox = -1;

    tft.setFont(Roboto_Black_12);

    for (int16_t d = -32; d <= 32; d++) {
        float deg = hdg + (float)d;
        while (deg <   0.0f) deg += 360.0f;
        while (deg >= 360.0f) deg -= 360.0f;

        int16_t px  = (int16_t)(SCFT_CX + d * SCFT_HDG_SCALE);
        // Strict clip — exclude boundary pixels to prevent residual at edges
        if (px <= SCFT_HDG_TAPE_X || px >= SCFT_HDG_TAPE_X + SCFT_HDG_TAPE_W) continue;

        // Suppress elements near the box (expanded to cover label text extents)
        if (px >= SCFT_HDG_SUPP_LO && px <= SCFT_HDG_SUPP_HI) continue;

        int16_t ideg = (int16_t)roundf(deg);
        if (ideg == 360) ideg = 0;

        if (ideg % 10 == 0) {
            tft.drawLine(px, SCFT_HDG_TAPE_Y, px, SCFT_HDG_TAPE_Y + 10, TFT_LIGHT_GREY);

            if (px >= SCFT_HDG_LABEL_LO && px <= SCFT_HDG_LABEL_HI) {
                const char *lbl;
                uint16_t    col;
                char        numbuf[8];
                if      (ideg ==   0) { lbl = "N";  col = TFT_YELLOW;  }
                else if (ideg ==  90) { lbl = "E";  col = TFT_WHITE;   }
                else if (ideg == 180) { lbl = "S";  col = TFT_WHITE;   }
                else if (ideg == 270) { lbl = "W";  col = TFT_WHITE;   }
                else {
                    snprintf(numbuf, sizeof(numbuf), "%d", ideg);
                    lbl = numbuf;
                    col = TFT_LIGHT_GREY;
                }
                tft.setTextColor(col, TFT_BLACK);
                uint8_t  lw  = strlen(lbl) * 8;
                // Clamp cursor so label never bleeds outside the tape area
                int16_t  cx  = px - (int16_t)(lw / 2);
                if (cx < SCFT_HDG_TAPE_X + 1) cx = SCFT_HDG_TAPE_X + 1;
                if (cx + lw > SCFT_HDG_TAPE_X + SCFT_HDG_TAPE_W - 1)
                    cx = SCFT_HDG_TAPE_X + SCFT_HDG_TAPE_W - 1 - lw;
                tft.setCursor(cx, SCFT_HDG_TAPE_Y + 12);
                tft.print(lbl);
            }
        } else if (ideg % 2 == 0) {
            tft.drawLine(px, SCFT_HDG_TAPE_Y, px, SCFT_HDG_TAPE_Y + 6, TFT_DARK_GREY);
        }
    }

    // Draw heading markers after ticks so they render on top
    auto drawMarker = [&](float markerHdg, uint16_t col) {
        // Find angular offset with wrap
        float diff = markerHdg - hdg;
        while (diff >  180.0f) diff -= 360.0f;
        while (diff < -180.0f) diff += 360.0f;
        int16_t px = (int16_t)(SCFT_CX + diff * SCFT_HDG_SCALE);
        // Peg to tape edges (leave room for half-width) rather than hiding
        int16_t pxMin = SCFT_HDG_TAPE_X + SCFT_HDG_MRK_HW + 1;
        int16_t pxMax = SCFT_HDG_TAPE_X + SCFT_HDG_TAPE_W - SCFT_HDG_MRK_HW - 1;
        if (px < pxMin) px = pxMin;
        if (px > pxMax) px = pxMax;
        // Skip if in suppress zone
        if (px >= SCFT_HDG_SUPP_LO && px <= SCFT_HDG_SUPP_HI) return;
        tft.fillTriangle(px,                    SCFT_HDG_MRK_TIP_Y,
                         px - SCFT_HDG_MRK_HW,  SCFT_HDG_MRK_BASE_Y,
                         px + SCFT_HDG_MRK_HW,  SCFT_HDG_MRK_BASE_Y,
                         col);
    };

    // Velocity heading — always drawn (TFT_NEON_GREEN = prograde colour)
    drawMarker(_scftVelHdg, TFT_NEON_GREEN);

    // Target heading — only when target available (TFT_VIOLET = target colour)
    if (state.targetAvailable)
        drawMarker(state.tgtHeading, TFT_VIOLET);

    // Maneuver heading — only when maneuver node exists (TFT_BLUE = maneuver colour)
    if (state.mnvrTime > 0.0f)
        drawMarker(state.mnvrHeading, TFT_BLUE);
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

    // ── Right panel chrome ─────────────────────────────────────────────────────────────
    // Vertical divider (2px) between ADI and panel
    tft.drawLine(SCFT_PANEL_X - 2, TITLE_TOP, SCFT_PANEL_X - 2, SCREEN_H - 1, TFT_GREY);
    tft.drawLine(SCFT_PANEL_X - 1, TITLE_TOP, SCFT_PANEL_X - 1, SCREEN_H - 1, TFT_GREY);

    static const tFont *PF = &Roboto_Black_28;   // label font — matches reticle/launch panels

    // Rows 0-6: single-width rows with label (row 1 label depends on orbMode)
    static const char *panelLabels[] = {
        "Alt.SL:", nullptr, "ApA:", "PeA:", "T+Ap:", "T+Ign:", "\xCE\x94V.Stg:"
    };
    for (uint8_t r = 0; r < 7; r++) {
        const char *lbl = (r == 1) ? (_scftOrbMode() ? "V.Orb:" : "V.Srf:") : panelLabels[r];
        printDispChrome(tft, PF, SCFT_PANEL_X, rowYFor(r, SCFT_PANEL_NR),
                        SCFT_PANEL_W, rowHFor(SCFT_PANEL_NR),
                        lbl, COL_LABEL, COL_BACK, COL_NO_BDR);
    }

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

    // Row 1 — V.Orb or V.Srf depending on orbital mode
    {
        uint16_t fg = TFT_DARK_GREEN;
        const char *lbl = orbMode ? "V.Orb:" : "V.Srf:";
        float vel = orbMode ? state.orbitalVel : state.surfaceVel;
        String val = hasOrbit ? fmtMs(vel) : "---";
        attPanelVal(1, 1, lbl, val, fg, TFT_BLACK);
    }

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

    // Row 5 — T+Ign
    {
        float tIgn = hasMnvr ? state.mnvrTime - state.mnvrDuration / 2.0f : -1.0f;
        uint16_t fg, bg = TFT_BLACK;
        String val;
        if (!hasMnvr)        { fg = TFT_DARK_GREY; val = "---"; }
        else if (tIgn < 0.0f){ fg = TFT_WHITE; bg = TFT_RED; val = formatTimeCompact((int64_t)tIgn); }
        else if (tIgn < MNVR_TIGN_WARN_S){ fg = TFT_YELLOW; val = formatTimeCompact((int64_t)tIgn); }
        else                  { fg = TFT_DARK_GREEN; val = formatTimeCompact((int64_t)tIgn); }
        attPanelVal(5, 5, "T+Ign:", val, fg, bg);
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

    // Row 7 split — RCS button (left half) | SAS button (right half)
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
    bool orbMode = _scftOrbMode();
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
    _scftUpdatePanel(tft, orbMode);
    _scftDrawTrim(tft);
}

