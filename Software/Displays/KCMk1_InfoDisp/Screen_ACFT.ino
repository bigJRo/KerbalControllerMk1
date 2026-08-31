/***************************************************************************************
   Screen_ACFT.ino  --  Aircraft screen  (EADI style)

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
static const int16_t  ACFT_CX        = 345;   // 1024x600: PFD sits left, data panel right
static const int16_t  ACFT_CY        = 300;   // ball centred so bank ticks clear the title rule
                                              //   (top ~84) and the slip strip reaches the bottom
static const int16_t  ACFT_R         = 206;   // sized so the derived panel (PANEL_X = CX+R+29)
                                              //   lands at x=580, giving a 360px readout panel
                                              //   matching the reticle/launch screens
static const float    ACFT_SCALE     = (float)ACFT_R / 30.0f;   // R/30 px/deg
// (ball extents/scanlines + sky/ground/horizon/wings/ladder colours and the
//  ACFT_BX_ALLSKY/ALLGND sentinels now live in the shared EADIBall.ino renderer.)

// ── Right panel geometry ───────────────────────────────────────────────────────────────
// Panel left = HDG tape right + 2. HDG tape right = CX + (R*2+54)/2 = 345+233 = 578.
static const int16_t  ACFT_PANEL_X       = ACFT_CX - (ACFT_R*2+54)/2 + (ACFT_R*2+54) + 2; // 580
static const int16_t  ACFT_PANEL_RIGHT   = CONTENT_W;   // 940 — dividers/buttons reach x=939,
                                                        //   abutting the sidebar divider at x=940
static const int16_t  ACFT_PANEL_W       = ACFT_PANEL_RIGHT - ACFT_PANEL_X;  // 360 (= reticle panel)
static const uint8_t  ACFT_PANEL_NR      = 8;

// ── Right panel state ──────────────────────────────────────────────────────────────────
// Cache managed by rowCache[screen_ACFT] / printState[screen_ACFT] slots 0-8.
// No additional state variables needed — printValue handles dirty detection.



// ── Per-frame state ───────────────────────────────────────────────────────────────────

static float    _acftPrevPitch   = -9999.0f;
static float    _acftPrevRoll    = -9999.0f;

// Aircraft always uses surface velocity — no orbital mode switching
static bool     _acftFullRedrawNeeded = true;






// Scanline fill, aircraft symbol, and disc clip are shared with SCFT — see EADIBall.ino
// (eadiDrawScanline / eadiDrawAircraftSymbol / eadiClipToDisk).








// ── ADI marker state (ball-side tracking — separate from tape-side state) ─────────────
// When any of these changes meaningfully, the ball must be redrawn so markers can be
// repositioned. Ball markers are drawn as the last layer of the ball (just before the
// aircraft symbol), so they're naturally wiped and re-rendered on any ball redraw.
static float _acftPrevHeadingBall    = -9999.0f;
static float _acftPrevVelHdgBall     = -9999.0f;
static float _acftPrevVelPitchBall   = -9999.0f;
static float _acftPrevMnvrHdgBall    = -9999.0f;
static float _acftPrevMnvrPitchBall  = -9999.0f;
static float _acftPrevTgtHdgBall     = -9999.0f;
static float _acftPrevTgtPitchBall   = -9999.0f;
static bool  _acftPrevMnvrActiveBall = false;
static bool  _acftPrevTgtAvailBall   = false;





// ── Roll indicator state ──────────────────────────────────────────────────────────────
static int16_t  _acftPrevRollReadout   = -9999;      // last drawn roll readout (integer degrees)
static uint16_t _acftPrevRollReadoutFg = 0;          // last drawn foreground colour

// ── Pitch readout state ───────────────────────────────────────────────────────────────


// Roll readout — two lines centred in fixed-width block
// Label: Roboto_Black_24, Value: Roboto_Black_28 (enlarged)
static const int16_t  ACFT_ROLL_ANCHOR_X  = ACFT_CX + ACFT_R - 54; // right edge tucked against
                                                                   //   the panel divider
static const int16_t  ACFT_ROLL_ANCHOR_Y  = TITLE_TOP;             // pinned just below the title
                                                                   //   rule (y58–61); independent of
                                                                   //   R so a bigger ball can't push
                                                                   //   the readout into the title bar
static const int16_t  ACFT_ROLL_W         = 80;   // block width ("+180°" _28 = 74px fits)
// Label/value are right-justified toward the panel divider. textRight() insets by
// TEXT_BORDER(8) from the box's right edge, so extend the justify reference 6px
// past ACFT_ROLL_W: text right edge lands at x=569, 2px clear of the divider (571),
// ~10px right of the old centred position for typical narrow roll values.
static const int16_t  ACFT_ROLL_TXT_W     = ACFT_ROLL_W + 6;   // 86 — text-justify reference
static const int16_t  ACFT_ROLL_LABEL_H   = 30;   // label line height (Roboto_Black_24, cap 29)
static const int16_t  ACFT_ROLL_VALUE_H   = 38;   // value line height (Roboto_Black_28, cap 33)
static const int16_t  ACFT_ROLL_GAP       = 3;    // gap between lines

// Update the roll numeric readout — aircraft applies roll warn/alarm colouring (unlike
// spacecraft). Scaffolding/geometry live in the shared eadiUpdateRollReadout().
static void _acftUpdateRollReadout(KCM_TFT &tft, float roll) {
    float    absRoll = fabsf(roll);
    uint16_t fg = (absRoll > ROLL_ALARM_DEG) ? TFT_WHITE      :
                  (absRoll > ROLL_WARN_DEG)  ? TFT_YELLOW     : TFT_DARK_GREEN;
    uint16_t bg = (absRoll > ROLL_ALARM_DEG) ? TFT_RED        : TFT_BLACK;
    eadiUpdateRollReadout(tft, roll, fg, bg,
                          _acftPrevRollReadout, _acftPrevRollReadoutFg);
}

// ── Pitch tape ────────────────────────────────────────────────────────────────────────
// Shared availability state (also used by heading tape)
// Vertical tape on the left side of the disc. Same scale as the pitch ladder.
// ±30° visible, ticks at every 5° (minor) and 10° (major).
// Current value box centred on disc centre (pitch=0 line).
// Markers: left-pointing triangles on the right edge for vel/tgt/mnvr pitch.

static const int16_t  ACFT_PTAPE_W       = 36;
static const int16_t  ACFT_PTAPE_GAP     = 27;                          // right edge aligns with HDG tape left
static const int16_t  ACFT_PTAPE_X       = ACFT_CX - ACFT_R - ACFT_PTAPE_GAP - ACFT_PTAPE_W; // 133
static const int16_t  ACFT_PTAPE_Y       = ACFT_CY - ACFT_R;              // 96 — top of disc
static const int16_t  ACFT_PTAPE_H       = ACFT_CY + ACFT_R + 8 - (ACFT_CY - ACFT_R); // 336 — bottom aligns with HDG tape top (CY+R+8)
static const float    ACFT_PTAPE_SCALE   = ACFT_SCALE;

// Current value box — centred vertically on disc centre (pitch=0)
static const int16_t  ACFT_PTAPE_BOX_W   = 68;
static const int16_t  ACFT_PTAPE_BOX_H   = 38;                          // taller for comfortable text margin
static const int16_t  ACFT_PTAPE_BOX_X   = ACFT_PTAPE_X + ACFT_PTAPE_W - 68; // right edge flush with tape right
static const int16_t  ACFT_PTAPE_BOX_Y   = ACFT_CY - ACFT_PTAPE_BOX_H / 2; // 241

// Suppress zone — ticks/labels suppressed near the value box
static const int16_t  ACFT_PTAPE_SUPP_LO = ACFT_PTAPE_BOX_Y - 10;
static const int16_t  ACFT_PTAPE_SUPP_HI = ACFT_PTAPE_BOX_Y + ACFT_PTAPE_BOX_H + 10;

// Markers — left-pointing triangles on right edge of tape
static const int16_t  ACFT_PTAPE_MRK_BASE_X = ACFT_PTAPE_X + ACFT_PTAPE_W - 2;
static const int16_t  ACFT_PTAPE_MRK_TIP_X  = ACFT_PTAPE_X + ACFT_PTAPE_W - 22; // 20px — enlarged
static const int16_t  ACFT_PTAPE_MRK_HW     = 9;                                // enlarged from 6

// State
static float   _acftPrevPitch2      = -9999.0f;   // pitch tape (distinct from ball state)
static int16_t _acftPrevPitchBox    = -9999;
static float   _acftPrevVelPitch    = -9999.0f;

// Draw/update the pitch value box — delegated to the shared helper (see EADIBall.ino).
static void _acftUpdatePitchBox(KCM_TFT &tft, float pitch) {
    eadiUpdatePitchBox(tft, pitch, _acftPrevPitchBox);
}

// Draw the full pitch tape. Aircraft markers: surface velocity pitch only — no target or
// maneuver markers. Scaffolding lives in the shared helper.
static void _acftDrawPitchTape(KCM_TFT &tft, float pitch) {
    EadiTapeMarker mk[1] = { { state.srfVelPitch, TFT_NEON_GREEN } };
    eadiDrawPitchTape(tft, pitch, mk, 1);
}

// Update pitch tape — redraws when pitch or velocity pitch changes.
static void _acftUpdatePitchTape(KCM_TFT &tft, float pitch) {
    bool dirty = fabsf(pitch - _acftPrevPitch2)                        >= 0.2f
              || fabsf(state.srfVelPitch - _acftPrevVelPitch)          >= 0.2f;

    if (dirty) {
        _acftDrawPitchTape(tft, pitch);
        _acftPrevPitch2   = pitch;
        _acftPrevVelPitch = state.srfVelPitch;
    }
    _acftUpdatePitchBox(tft, pitch);
}

// ── Heading tape state ────────────────────────────────────────────────────────────────
static float   _acftPrevHeading    = -9999.0f;
static int16_t _acftPrevHdgBox     = -9999;
static float   _acftPrevVelHdg     = -9999.0f;

// ── Heading tape geometry ─────────────────────────────────────────────────────────────
static const int16_t  ACFT_HDG_TAPE_W    = (ACFT_R * 2) + 54;              // 354 — ±35° visible
static const int16_t  ACFT_HDG_TAPE_X    = ACFT_CX - (ACFT_HDG_TAPE_W / 2); // 83
static const int16_t  ACFT_HDG_TAPE_Y    = ACFT_CY + ACFT_R + 8;    // 406 — 8px below disc bottom
static const int16_t  ACFT_HDG_TAPE_H    = 32;
static const float    ACFT_HDG_SCALE     = (float)(ACFT_R * 2) / 60.0f;
static const int16_t  ACFT_HDG_LABEL_LO  = ACFT_HDG_TAPE_X + 8;
static const int16_t  ACFT_HDG_LABEL_HI  = ACFT_HDG_TAPE_X + ACFT_HDG_TAPE_W - 8;

// Box — top aligned with tape top, extends 8px BELOW tape bottom so its
// bottom border is outside the fillRect zone and never flickers
static const int16_t  ACFT_HDG_BOX_W     = 72;
static const int16_t  ACFT_HDG_BOX_H     = ACFT_HDG_TAPE_H + 8;    // = 40
static const int16_t  ACFT_HDG_BOX_X     = ACFT_CX - (ACFT_HDG_BOX_W / 2);
static const int16_t  ACFT_HDG_BOX_Y     = ACFT_HDG_TAPE_Y;

// Suppress zone — covers box + max label half-width (12px)
static const int16_t  ACFT_HDG_SUPP_LO   = ACFT_HDG_BOX_X - 18;
static const int16_t  ACFT_HDG_SUPP_HI   = ACFT_HDG_BOX_X + ACFT_HDG_BOX_W + 18;

// Heading markers — long thin downward triangles fully inside the tape
static const int16_t  ACFT_HDG_MRK_BASE_Y = ACFT_HDG_TAPE_Y + 2;   // 2px below tape top
static const int16_t  ACFT_HDG_MRK_TIP_Y  = ACFT_HDG_TAPE_Y + 24;  // 22px tall (enlarged)
static const int16_t  ACFT_HDG_MRK_HW     = 9;                     // half-width → 19px wide (enlarged)

// Draw/update the heading number box — delegated to the shared helper (see EADIBall.ino).
static void _acftUpdateHdgBox(KCM_TFT &tft, float hdg) {
    eadiUpdateHdgBox(tft, hdg, _acftPrevHdgBox);
}

// Draw the full heading tape. Aircraft markers: surface velocity heading only — no target
// or maneuver markers. Scaffolding lives in the shared helper, which also forces
// _acftPrevHdgBox to -1 (the fill blackens the box interior).
static void _acftDrawHeadingTape(KCM_TFT &tft, float hdg) {
    EadiTapeMarker mk[1] = { { state.srfVelHeading, TFT_NEON_GREEN } };
    eadiDrawHeadingTape(tft, hdg, _acftPrevHdgBox, mk, 1);
}

// Update heading — tape redraws when heading or velocity heading changes.
static void _acftUpdateHeadingTape(KCM_TFT &tft, float hdg) {
    bool dirty = fabsf(hdg - _acftPrevHeading)                     >= 0.5f
              || fabsf(state.srfVelHeading - _acftPrevVelHdg)      >= 0.5f;

    if (dirty) {
        _acftDrawHeadingTape(tft, hdg);
        _acftPrevHeading = hdg;
        _acftPrevVelHdg  = state.srfVelHeading;
    }
    _acftUpdateHdgBox(tft, hdg);
}




// ── Screen chrome ─────────────────────────────────────────────────────────────────────

// ── VSI, slip ball, AoA arc ───────────────────────────────────────────────────────────

// VSI tape — vertical velocity bar, left strip, zero at ACFT_CY
static const int16_t VSI_X       = 2;
static const int16_t VSI_BAR_W   = 18;
static const int16_t VSI_ZERO_Y  = ACFT_CY;
static const int16_t VSI_TICK_X0 = VSI_X + VSI_BAR_W;
static float   _acftPrevVSI      = -9999.0f;

static const int16_t VSI_LABEL_H  = 62;   // height reserved at bottom for "VSI" label (Roboto_Black_16 rotated)

static void _acftDrawVSIChrome(KCM_TFT &tft) {
    tft.drawLine(VSI_TICK_X0 + 1, TITLE_TOP, VSI_TICK_X0 + 1, SCREEN_H - 1, TFT_GREY);
    tft.drawLine(VSI_X, VSI_ZERO_Y,     VSI_TICK_X0 + 10, VSI_ZERO_Y,     TFT_LIGHT_GREY);
    tft.drawLine(VSI_X, VSI_ZERO_Y + 1, VSI_TICK_X0 + 10, VSI_ZERO_Y + 1, TFT_LIGHT_GREY);
    tft.setFont(Roboto_Black_12);
    for (int16_t v = -30; v <= 30; v += 5) {
        if (v == 0) continue;
        int16_t ty = VSI_ZERO_Y - (int16_t)(v * ACFT_SCALE);
        if (ty < TITLE_TOP + 2 || ty > SCREEN_H - VSI_LABEL_H - 4) continue;
        bool major = (v % 10 == 0);
        tft.drawLine(VSI_TICK_X0, ty, VSI_TICK_X0 + (major ? 10 : 6), ty,
                     major ? TFT_LIGHT_GREY : TFT_GREY);
        if (major) {
            tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);  // grey, matching pitch/hdg tapes
            tft.setCursor(VSI_TICK_X0 + 12, ty - 6);
            char lbl[4]; snprintf(lbl, sizeof(lbl), "%d", abs(v));
            tft.print(lbl);
        }
    }
    // "VSI" label — vertical, in bar at bottom, matching Pitch/Hdg label style
    drawVerticalText(tft, VSI_X, SCREEN_H - VSI_LABEL_H, VSI_BAR_W, VSI_LABEL_H,
                     &Roboto_Black_16, "VSI", TFT_WHITE, TFT_BLACK);
}

static void _acftUpdateVSI(KCM_TFT &tft, float vVrt) {
    if (fabsf(vVrt - _acftPrevVSI) < 0.1f) return;
    _acftPrevVSI = vVrt;
    float clamped = constrain(vVrt, -30.0f, 30.0f);
    int16_t fillH = (int16_t)fabsf(clamped * ACFT_SCALE);
    // Clear bar area — stop VSI_LABEL_H px short of bottom to preserve "VSI" label
    int16_t barTop = TITLE_TOP;
    int16_t barH   = SCREEN_H - TITLE_TOP - VSI_LABEL_H;
    tft.fillRect(VSI_X, barTop, VSI_BAR_W, barH, TFT_BLACK);
    if (fillH > 0) {
        uint16_t col = (vVrt < LNDG_VVRT_ALARM_MS) ? TFT_RED :
                       (vVrt < LNDG_VVRT_WARN_MS)  ? TFT_YELLOW : TFT_DARK_GREEN;
        int16_t fillY = (clamped > 0.0f) ? VSI_ZERO_Y - fillH : VSI_ZERO_Y + 2;
        // Clamp fill to bar area (don't paint over label)
        int16_t fillBot = fillY + fillH;
        int16_t barBot  = SCREEN_H - VSI_LABEL_H;
        if (fillBot > barBot) fillH = barBot - fillY;
        if (fillH > 0) tft.fillRect(VSI_X, fillY, VSI_BAR_W, fillH, col);
    }
    tft.drawLine(VSI_X, VSI_ZERO_Y,     VSI_TICK_X0 + 10, VSI_ZERO_Y,     TFT_LIGHT_GREY);
    tft.drawLine(VSI_X, VSI_ZERO_Y + 1, VSI_TICK_X0 + 10, VSI_ZERO_Y + 1, TFT_LIGHT_GREY);
}

// Slip ball — short strip pinned to the bottom of the screen, same x/width as the
// heading tape and the same HEIGHT as the heading tape. Centred on ACFT_CX.
static const int16_t SLIP_X       = ACFT_HDG_TAPE_X;
static const int16_t SLIP_W       = ACFT_HDG_TAPE_W;
static const int16_t SLIP_H       = ACFT_HDG_TAPE_H;                     // 32 — matches HDG tape
static const int16_t SLIP_Y       = SCREEN_H - 1 - SLIP_H;               // 567 — bottom border on row 598
                                                                         //   (row 599 is lost to overscan)
static const int16_t SLIP_BALL_R  = SLIP_H * 2 / 5;                      // 12 → 25px dia ≈ 80% of height
static const int16_t SLIP_CY      = SLIP_Y + SLIP_H / 2;
static const float   SLIP_SCALE   = (float)(SLIP_W / 2 - SLIP_BALL_R - 4) / 20.0f;
static int16_t _acftPrevSlipX     = 9999;

static void _acftDrawSlipChrome(KCM_TFT &tft) {
    tft.drawRect(SLIP_X, SLIP_Y, SLIP_W, SLIP_H, TFT_GREY);

    int16_t m5  = (int16_t)(SLIP_WARN_DEG  * SLIP_SCALE);
    int16_t m10 = (int16_t)(10.0f          * SLIP_SCALE);
    int16_t m15 = (int16_t)(SLIP_ALARM_DEG * SLIP_SCALE);

    // Range reference ticks at ±5, ±10, ±15 — drawn in the light-grey label colour
    // (matching the numeric labels), inset 1px from the top/bottom borders.
    const int16_t TLEN = 7;   // 80% of the previous 9px height
    int16_t topY0 = SLIP_Y + 1,              topY1 = SLIP_Y + 1 + TLEN;
    int16_t botY1 = SLIP_Y + SLIP_H - 2,     botY0 = botY1 - TLEN;
    const int markDx[] = { -m15, -m10, -m5, m5, m10, m15 };  // int: unary minus promotes
    for (uint8_t i = 0; i < 6; i++) {
        int16_t mx = ACFT_CX + markDx[i];
        tft.drawLine(mx, topY0, mx, topY1, TFT_LIGHT_GREY);
        tft.drawLine(mx, botY0, mx, botY1, TFT_LIGHT_GREY);
    }

    // Signed numeric labels centred above each tick, in the clear space above the
    // strip. All drawn in the standard light-grey label colour (not the tick colour).
    tft.setFont(Roboto_Black_12);
    const char *markTxt[] = { "-15", "-10", "-5", "+5", "+10", "+15" };
    int16_t lblY = SLIP_Y - 16;
    tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
    for (uint8_t i = 0; i < 6; i++) {
        int16_t mx = ACFT_CX + markDx[i];
        int16_t w  = getFontStringWidth(&Roboto_Black_12, markTxt[i]);
        tft.setCursor(mx - w / 2, lblY);
        tft.print(markTxt[i]);
    }

    // "Slip:" label — right-justified in the space left of the strip, matching the
    // Pitch/Hdg label style
    textRight(tft, &Roboto_Black_24,
               VSI_TICK_X0 + 2, SLIP_Y, SLIP_X - VSI_TICK_X0 - 2, SLIP_H,
               "Slip:", TFT_WHITE, TFT_BLACK);
}

static void _acftUpdateSlipBall(KCM_TFT &tft, float slip) {
    float sc = constrain(slip, -20.0f, 20.0f);
    int16_t ballX = ACFT_CX + (int16_t)(sc * SLIP_SCALE);
    ballX = constrain(ballX, SLIP_X + SLIP_BALL_R + 2, SLIP_X + SLIP_W - SLIP_BALL_R - 2);
    if (_acftPrevSlipX != 9999 && abs(ballX - _acftPrevSlipX) <= 1) return;
    if (_acftPrevSlipX != 9999)
        tft.fillCircle(_acftPrevSlipX, SLIP_CY, SLIP_BALL_R + 1, TFT_BLACK);
    _acftDrawSlipChrome(tft);
    float absSlip = fabsf(slip);
    uint16_t col = (absSlip > SLIP_ALARM_DEG) ? TFT_RED :
                   (absSlip > SLIP_WARN_DEG)  ? TFT_YELLOW : TFT_DARK_GREEN;
    tft.fillCircle(ballX, SLIP_CY, SLIP_BALL_R, col);
    tft.drawCircle(ballX, SLIP_CY, SLIP_BALL_R, TFT_LIGHT_GREY);
    _acftPrevSlipX = ballX;
}

// ── AoA arc indicator ────────────────────────────────────────────────────────────────
// Solid arc on the left bezel. Zero AoA = 9 o'clock (180°).
// Arc angle = AOA_ZERO_DEG + aoa (see _acftUpdateAoAArc), so POSITIVE AoA (nose above
// velocity) INCREASES the screen angle from 180° toward 205°.
// Arc spans AOA_ANG_LO..AOA_ANG_HI in screen-angle degrees.
// Safe range: stays clear of bank=±60 roll ticks (which live at 210° and 330°).
// Drawn as solid colour bands using fillTriangle pairs — no float drift, no gaps.
static const int16_t  AOA_R_INNER  = ACFT_R + 4;    // 154px — just outside bezel
static const int16_t  AOA_R_OUTER  = ACFT_R + 18;   // 168px
static const float    AOA_ZERO_DEG = 180.0f;         // 9 o'clock
static const int16_t  AOA_ANG_LO   = 155;            // low screen angle  (= AoA -25°)
static const int16_t  AOA_ANG_HI   = 205;            // high screen angle (= AoA +25°)
// AoA thresholds in arc-angle space (1:1 scale: 1° AoA = 1° arc)
// Positive AoA → arc angle increases from 180°
// warn zone:  arc 190°..200°  (AoA +10°..+20°)
// alarm zone: arc 200°..205°  (AoA +20°..+25°)
// (negative AoA occupies 180°..155° symmetrically)

// Precomputed integer-degree cos/sin table for the arc range (155..206 inclusive)
static const uint8_t  AOA_STEPS     = (uint8_t)(AOA_ANG_HI - AOA_ANG_LO + 2); // 52 entries
static int16_t _aoaIX[52];   // inner x at each degree
static int16_t _aoaIY[52];   // inner y
static int16_t _aoaOX[52];   // outer x
static int16_t _aoaOY[52];   // outer y
static bool    _aoaTableReady = false;

static void _aoaBuildTable() {
    if (_aoaTableReady) return;
    for (uint8_t i = 0; i < AOA_STEPS; i++) {
        float rad = (AOA_ANG_LO + (int16_t)i) * DEG_TO_RAD;
        float ca = cosf(rad), sa = sinf(rad);
        _aoaIX[i] = (int16_t)(ACFT_CX + AOA_R_INNER * ca);
        _aoaIY[i] = (int16_t)(ACFT_CY + AOA_R_INNER * sa);
        _aoaOX[i] = (int16_t)(ACFT_CX + AOA_R_OUTER * ca);
        _aoaOY[i] = (int16_t)(ACFT_CY + AOA_R_OUTER * sa);
    }
    _aoaTableReady = true;
}

// Fill a solid arc segment [degLo..degHi] with colour col.
// Uses fillTriangle pairs on precomputed integer-degree vertices — no gaps, no drift.
static void _aoaFillSegment(KCM_TFT &tft, int16_t degLo, int16_t degHi, uint16_t col) {
    for (int16_t d = degLo; d < degHi; d++) {
        uint8_t i = (uint8_t)(d - AOA_ANG_LO);
        tft.fillTriangle(_aoaIX[i], _aoaIY[i], _aoaOX[i], _aoaOY[i],
                         _aoaOX[i+1], _aoaOY[i+1], col);
        tft.fillTriangle(_aoaIX[i], _aoaIY[i], _aoaIX[i+1], _aoaIY[i+1],
                         _aoaOX[i+1], _aoaOY[i+1], col);
    }
}

// Draw the full arc background (chrome). Call once on screen entry.
// Orientation: 155° = lower arc (7 o'clock) = NEGATIVE AoA
//              180° = exact left  (9 o'clock) = zero AoA
//              205° = upper arc (11 o'clock)  = HIGH POSITIVE AoA
// So warn/alarm zones must be at the HIGH end of the angle range.
static void _acftDrawAoAChrome(KCM_TFT &tft) {
    _aoaBuildTable();
    // Zone boundaries derived from config thresholds — stays in sync with text field colours
    int16_t warnAng  = (int16_t)(AOA_ZERO_DEG + AOA_WARN_DEG);   // 180+10 = 190
    int16_t alarmAng = (int16_t)(AOA_ZERO_DEG + AOA_ALARM_DEG);  // 180+20 = 200
    // Grey base covers full arc, coloured zones painted on top
    _aoaFillSegment(tft, AOA_ANG_LO,  AOA_ANG_HI, TFT_DARK_GREY);
    _aoaFillSegment(tft, warnAng,      alarmAng,    TFT_YELLOW);
    _aoaFillSegment(tft, alarmAng,     AOA_ANG_HI,  TFT_RED);
    // Zero tick — white radial at 180°, drawn last
    uint8_t zi = (uint8_t)(180 - AOA_ANG_LO);
    tft.drawLine(_aoaIX[zi], _aoaIY[zi], _aoaOX[zi], _aoaOY[zi], TFT_WHITE);

    // "AoA" label centred just below the arc's lower end. The arc spans 155..205°
    // at radius R+18, so its lowest point is at y = CY + (R+18)*sin(25°) ≈ CY+92.
    // Place the label ~4px below that, in the clear slot between the pitch tape
    // (right edge 118) and the ball's left rim.
    textCenter(tft, &Roboto_Black_20,
               ACFT_CX - ACFT_R - 25, ACFT_CY + 100, 50, 26,
               "AoA", TFT_WHITE, TFT_BLACK);
}

// Pointer state — tracked in integer degrees to avoid float drift
static int16_t _acftPrevAoADeg = INT16_MIN;

// Restore background colour at a ±1° band around arcDeg
static void _aoaErasePointer(KCM_TFT &tft, int16_t arcDeg) {
    int16_t lo = max((int16_t)AOA_ANG_LO, (int16_t)(arcDeg - 1));
    int16_t hi = min((int16_t)(AOA_ANG_HI - 1), (int16_t)(arcDeg + 1));
    for (int16_t d = lo; d < hi; d++) {
        // Positive AoA = higher screen angle (d > 180). Only positive side has warn/alarm.
        int16_t aoaDeg = d - (int16_t)AOA_ZERO_DEG;
        uint16_t col = (aoaDeg >= (int16_t)AOA_ALARM_DEG) ? TFT_RED :
                       (aoaDeg >= (int16_t)AOA_WARN_DEG)  ? TFT_YELLOW : TFT_DARK_GREY;
        _aoaFillSegment(tft, d, d + 1, col);
    }
    // Restore zero tick if erased
    if (arcDeg >= 179 && arcDeg <= 181) {
        uint8_t zi = (uint8_t)(180 - AOA_ANG_LO);
        tft.drawLine(_aoaIX[zi], _aoaIY[zi], _aoaOX[zi], _aoaOY[zi], TFT_WHITE);
    }
}

// Draw pointer — 1° wide (≈3px at outer edge), bold white fill
static void _aoaDrawPointer(KCM_TFT &tft, int16_t arcDeg) {
    int16_t lo = max((int16_t)AOA_ANG_LO,       arcDeg);
    int16_t hi = min((int16_t)(AOA_ANG_HI - 1), (int16_t)(arcDeg + 1));
    _aoaFillSegment(tft, lo, hi, TFT_WHITE);
}

static void _acftUpdateAoAArc(KCM_TFT &tft, float aoa) {
    _aoaBuildTable();
    // Positive AoA → higher screen angle (toward 205° = 11 o'clock = upper arc)
    float arcF = AOA_ZERO_DEG + constrain(aoa, (float)(AOA_ANG_LO - 180), (float)(AOA_ANG_HI - 180));
    int16_t arcDeg = (int16_t)roundf(arcF);
    arcDeg = (int16_t)constrain((int32_t)arcDeg, (int32_t)AOA_ANG_LO, (int32_t)(AOA_ANG_HI - 1));

    if (arcDeg == _acftPrevAoADeg) return;

    // Erase old pointer
    if (_acftPrevAoADeg != INT16_MIN)
        _aoaErasePointer(tft, _acftPrevAoADeg);

    // Draw new pointer
    _aoaDrawPointer(tft, arcDeg);
    _acftPrevAoADeg = arcDeg;
}




// ── Screen update ─────────────────────────────────────────────────────────────────────

static void chromeScreen_ACFT(KCM_TFT &tft) {
    eadiBallResetState();
    _acftFullRedrawNeeded      = true;
    eadiResetRollIndicator();
    _acftPrevRollReadout       = -9999;
    _acftPrevRollReadoutFg     = 0;
    _acftPrevPitch2            = -9999.0f;
    _acftPrevPitchBox          = -9999;
    _acftPrevVelPitch          = -9999.0f;
    _acftPrevHeading           = -9999.0f;
    _acftPrevHdgBox            = -9999;
    _acftPrevVelHdg            = -9999.0f;
    // Panel values reset via rowCache invalidation (row cache cleared on screen switch)
    // Ball delta-fill state (chord table, dirty bitmaps, prev horizon) reset above.
    _acftPrevPitch        = -9999.0f;
    _acftPrevRoll         = -9999.0f;
    _acftPrevVSI          = -9999.0f;
    _acftPrevSlipX        = 9999;
    _acftPrevAoADeg       = INT16_MIN;

    // Ball-side marker trackers (distinct from tape-side; see _acftDrawAdiMarker)
    _acftPrevHeadingBall       = -9999.0f;
    _acftPrevVelHdgBall        = -9999.0f;
    _acftPrevVelPitchBall      = -9999.0f;
    _acftPrevMnvrHdgBall       = -9999.0f;
    _acftPrevMnvrPitchBall     = -9999.0f;
    _acftPrevTgtHdgBall        = -9999.0f;
    _acftPrevTgtPitchBall      = -9999.0f;
    _acftPrevMnvrActiveBall    = false;
    _acftPrevTgtAvailBall      = false;

    // Bezel ring
    tft.drawCircle(ACFT_CX, ACFT_CY, ACFT_R,     TFT_LIGHT_GREY);
    tft.drawCircle(ACFT_CX, ACFT_CY, ACFT_R + 1, TFT_WHITE);
    tft.drawCircle(ACFT_CX, ACFT_CY, ACFT_R + 2, TFT_DARK_GREY);

    // Bank scale tick marks outside the disc circumference (upper arc)
    static const int16_t ticks[] = {-60,-45,-30,-20,-10,0,10,20,30,45,60};
    for (uint8_t i = 0; i < 11; i++) eadiDrawBankTick(tft, ticks[i]);

    // Labels at ±30° and ±60° — drawn at R+28 along the tick radial (Roboto_Black_16)
    static const int16_t labelTicks[] = {-60, -30, 30, 60};
    for (uint8_t i = 0; i < 4; i++) eadiDrawBankLabel(tft, labelTicks[i]);

    // Heading box border
    tft.drawRect(ACFT_HDG_BOX_X, ACFT_HDG_BOX_Y, ACFT_HDG_BOX_W, ACFT_HDG_BOX_H, TFT_LIGHT_GREY);

    // Pitch tape chrome — value box border and three-sided tape border (top, right, bottom)
    tft.drawRect(ACFT_PTAPE_BOX_X, ACFT_PTAPE_BOX_Y, ACFT_PTAPE_BOX_W, ACFT_PTAPE_BOX_H, TFT_LIGHT_GREY);
    // Top, right, bottom lines — right and bottom drawn 1px inward to align with HDG tape borders
    tft.drawLine(ACFT_PTAPE_X - 1,                  ACFT_PTAPE_Y - 1,
                 ACFT_PTAPE_X + ACFT_PTAPE_W - 1,    ACFT_PTAPE_Y - 1, TFT_LIGHT_GREY);
    tft.drawLine(ACFT_PTAPE_X + ACFT_PTAPE_W - 1,    ACFT_PTAPE_Y - 1,
                 ACFT_PTAPE_X + ACFT_PTAPE_W - 1,    ACFT_PTAPE_Y + ACFT_PTAPE_H - 1, TFT_GREY);
    tft.drawLine(ACFT_PTAPE_X - 1,                  ACFT_PTAPE_Y + ACFT_PTAPE_H - 1,
                 ACFT_PTAPE_X + ACFT_PTAPE_W - 1,    ACFT_PTAPE_Y + ACFT_PTAPE_H - 1, TFT_LIGHT_GREY);

    // "Pitch:" right-justified to right edge of pitch value box (box widened left
    // so the larger _24 label — 75px — fits without clipping the pitch value box)
    textRight(tft, &Roboto_Black_24,
              ACFT_PTAPE_BOX_X - 24, ACFT_PTAPE_Y - 32,
              ACFT_PTAPE_BOX_W + 24, 30,
              "Pitch:", TFT_WHITE, TFT_BLACK);

    // Heading tape border — top (light grey) and two sides (darker), open at bottom
    tft.drawLine(ACFT_HDG_TAPE_X - 1, ACFT_HDG_TAPE_Y - 1,
                 ACFT_HDG_TAPE_X + ACFT_HDG_TAPE_W, ACFT_HDG_TAPE_Y - 1, TFT_LIGHT_GREY);
    tft.drawLine(ACFT_HDG_TAPE_X - 1, ACFT_HDG_TAPE_Y - 1,
                 ACFT_HDG_TAPE_X - 1, ACFT_HDG_TAPE_Y + ACFT_HDG_TAPE_H, TFT_GREY);
    tft.drawLine(ACFT_HDG_TAPE_X + ACFT_HDG_TAPE_W, ACFT_HDG_TAPE_Y - 1,
                 ACFT_HDG_TAPE_X + ACFT_HDG_TAPE_W, ACFT_HDG_TAPE_Y + ACFT_HDG_TAPE_H, TFT_GREY);

    // "Hdg:" right-justified to same right edge as "Pitch:" label
    textRight(tft, &Roboto_Black_24,
              ACFT_PTAPE_BOX_X, ACFT_HDG_BOX_Y + ACFT_HDG_BOX_H / 2 - 15,
              ACFT_PTAPE_BOX_W, 30,
              "Hdg:", TFT_WHITE, TFT_BLACK);

    // ── VSI tape chrome ───────────────────────────────────────────────────────────────
    _acftDrawVSIChrome(tft);

    // ── Slip ball chrome ──────────────────────────────────────────────────────────────
    _acftDrawSlipChrome(tft);

    // ── AoA arc chrome ────────────────────────────────────────────────────────────────
    _acftDrawAoAChrome(tft);

    // ── Right panel chrome ─────────────────────────────────────────────────────────────
    // Vertical divider (2px) between ADI and panel
    tft.drawLine(ACFT_PANEL_X - 2, TITLE_TOP, ACFT_PANEL_X - 2, SCREEN_H - 1, TFT_GREY);
    tft.drawLine(ACFT_PANEL_X - 1, TITLE_TOP, ACFT_PANEL_X - 1, SCREEN_H - 1, TFT_GREY);

    static const tFont *PF = &Roboto_Black_28;   // label font — matches reticle/launch panels

    // Rows 0-3: single-width labels
    printDispChrome(tft, PF, ACFT_PANEL_X, rowYFor(0, ACFT_PANEL_NR), ACFT_PANEL_W, rowHFor(ACFT_PANEL_NR), "Alt.Rdr:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, PF, ACFT_PANEL_X, rowYFor(1, ACFT_PANEL_NR), ACFT_PANEL_W, rowHFor(ACFT_PANEL_NR), "V.Srf:",   COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, PF, ACFT_PANEL_X, rowYFor(2, ACFT_PANEL_NR), ACFT_PANEL_W, rowHFor(ACFT_PANEL_NR), "IAS:",     COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, PF, ACFT_PANEL_X, rowYFor(3, ACFT_PANEL_NR), ACFT_PANEL_W, rowHFor(ACFT_PANEL_NR), "V.Vrt:",   COL_LABEL, COL_BACK, COL_NO_BDR);

    // Row 4: Ma | G split.
    // "Ma:", not "Mach:" as ASCENT and RE-ENTRY use, and this is measured rather than
    // careless: these split cells are 180 px, "Mach:" is 101 px at the value font and the
    // widest Mach reading is 95, which overflows by 26. Dropping a decimal does not save
    // it either. The abbreviation is forced by the cell, and it is the one place in the
    // sketch where a quantity carries a second name.
    {
        uint16_t hw = ACFT_PANEL_W / 2;
        uint16_t y  = rowYFor(4, ACFT_PANEL_NR), h = rowHFor(ACFT_PANEL_NR);
        printDispChrome(tft, PF, ACFT_PANEL_X,      y, hw, h, "Ma:", COL_LABEL, COL_BACK, COL_NO_BDR);
        printDispChrome(tft, PF, ACFT_PANEL_X + hw, y, hw, h, "G:",  COL_LABEL, COL_BACK, COL_NO_BDR);
        tft.drawLine(ACFT_PANEL_X + hw,     y, ACFT_PANEL_X + hw,     y + h - 1, TFT_GREY);
        tft.drawLine(ACFT_PANEL_X + hw + 1, y, ACFT_PANEL_X + hw + 1, y + h - 1, TFT_GREY);
    }

    // Row 5: AoA | Slip split. "Slip:" in full, matching the slip-ball label on the same
    // screen -- the panel row used to say "Sl:" while the ball said "Slip:", one quantity
    // with two names three inches apart. It fits: the value is already whole degrees, so
    // the widest case "+180deg" is 96 px against a 74 px label in a 174 px usable cell.
    {
        uint16_t hw = ACFT_PANEL_W / 2;
        uint16_t y  = rowYFor(5, ACFT_PANEL_NR), h = rowHFor(ACFT_PANEL_NR);
        printDispChrome(tft, PF, ACFT_PANEL_X,      y, hw, h, "AoA:",  COL_LABEL, COL_BACK, COL_NO_BDR);
        printDispChrome(tft, PF, ACFT_PANEL_X + hw, y, hw, h, "Slip:", COL_LABEL, COL_BACK, COL_NO_BDR);
        tft.drawLine(ACFT_PANEL_X + hw,     y, ACFT_PANEL_X + hw,     y + h - 1, TFT_GREY);
        tft.drawLine(ACFT_PANEL_X + hw + 1, y, ACFT_PANEL_X + hw + 1, y + h - 1, TFT_GREY);
    }

    // Row 6: Gear | Airbrk — buttons draw own labels, just draw the split divider.
    {
        uint16_t hw = ACFT_PANEL_W / 2;
        uint16_t y6 = TITLE_TOP + 6 * rowHFor(ACFT_PANEL_NR);
        uint16_t y7 = TITLE_TOP + 7 * rowHFor(ACFT_PANEL_NR);
        tft.drawLine(ACFT_PANEL_X + hw,     y6, ACFT_PANEL_X + hw,     y7 - 1, TFT_GREY);
        tft.drawLine(ACFT_PANEL_X + hw + 1, y6, ACFT_PANEL_X + hw + 1, y7 - 1, TFT_GREY);
    }

    // Row 7: Brakes | SAS split divider (buttons drawn in update)
    {
        uint16_t hw = ACFT_PANEL_W / 2;
        tft.drawLine(ACFT_PANEL_X + hw,     TITLE_TOP + 7 * rowHFor(ACFT_PANEL_NR),
                     ACFT_PANEL_X + hw,     SCREEN_H - 1, TFT_GREY);
        tft.drawLine(ACFT_PANEL_X + hw + 1, TITLE_TOP + 7 * rowHFor(ACFT_PANEL_NR),
                     ACFT_PANEL_X + hw + 1, SCREEN_H - 1, TFT_GREY);
    }
}



// ── Right panel update ─────────────────────────────────────────────────────────────────
// Row order: Alt.Rdr(0), V.Srf(1), IAS(2), V.Vrt(3), Ma|G(4), AoA|Slip(5), Gear|Airbrk(6), Brakes|SAS(7)
// Cache slots: 0=Alt.Rdr, 1=V.Srf, 2=IAS, 3=V.Vrt, 4=Ma, 5=G, 6=AoA, 7=Slip,
//              8=Gear, 9=Airbrk, 10=Brakes, 11=SAS
static void _acftUpdatePanel(KCM_TFT &tft) {
    static const tFont  *VF = &Roboto_Black_36;   // value font — matches reticle/launch panels
    static const uint8_t SC = (uint8_t)screen_ACFT;
    uint16_t fw = ACFT_PANEL_W;
    uint16_t hw = ACFT_PANEL_W / 2;
    uint16_t fg, bg;
    char buf[20];

    auto acftVal = [&](uint8_t row, uint8_t slot, const char *label,
                       const String &val, uint16_t fgc, uint16_t bgc) {
        drawPanelValue(tft, SC, slot, row, ACFT_PANEL_X, fw, label, val, fgc, bgc, VF, ACFT_PANEL_NR, true);
    };

    // Row 0 — Alt.Rdr
    {
        fg = (state.radarAlt < ALT_RDR_ALARM_M) ? TFT_WHITE  :
             (state.radarAlt < ALT_RDR_WARN_M)  ? TFT_YELLOW : TFT_DARK_GREEN;
        bg = (state.radarAlt < ALT_RDR_ALARM_M) ? TFT_RED    : TFT_BLACK;
        acftVal(0, 0, "Alt.Rdr:", formatAlt(state.radarAlt), fg, bg);
    }

    // Row 1 — V.Srf (surfaceVel is a magnitude, always >= 0)
    {
        acftVal(1, 1, "V.Srf:", fmtMs(state.surfaceVel), TFT_DARK_GREEN, TFT_BLACK);
    }

    // Row 2 — IAS with stall warning
    {
        float ias = state.IAS;
        if (STALL_SPEED_MS > 0.0f) {
            if      (ias < STALL_SPEED_MS * 0.5f) { fg = TFT_WHITE;     bg = TFT_RED;   }
            else if (ias < STALL_SPEED_MS)         { fg = TFT_YELLOW;    bg = TFT_BLACK; }
            else                                   { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
        } else { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
        acftVal(2, 2, "IAS:", fmtMs(ias), fg, bg);
    }

    // Row 3 — V.Vrt — colours match VSI bar thresholds
    {
        float vv = state.verticalVel;
        if      (vv < LNDG_VVRT_ALARM_MS) { fg = TFT_WHITE;     bg = TFT_RED;   }
        else if (vv < LNDG_VVRT_WARN_MS)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
        else                               { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
        acftVal(3, 3, "V.Vrt:", fmtMs(vv), fg, bg);
    }

    // Row 4 — Ma | G split
    {
        uint16_t y4 = rowYFor(4, ACFT_PANEL_NR), h4 = rowHFor(ACFT_PANEL_NR);
        float m = state.machNumber;
        snprintf(buf, sizeof(buf), "%.2f", m);
        bool transonic = (m >= 0.85f && m <= 1.2f);
        uint16_t mfg = transonic ? TFT_YELLOW : TFT_DARK_GREEN;
        {
            String ms = buf; RowCache &mc = rowCache[SC][4];
            if (mc.value != ms || mc.fg != mfg) {
                printValue(tft, VF, ACFT_PANEL_X, y4, hw, h4,
                           "Ma:", ms, mfg, TFT_BLACK, COL_BACK, printState[SC][4]);
                mc.value = ms; mc.fg = mfg; mc.bg = TFT_BLACK;
            }
        }
        float g = state.gForce;
        snprintf(buf, sizeof(buf), "%.2f", g);
        fg = (g > G_ALARM_POS || g < G_ALARM_NEG) ? TFT_WHITE  :
             (g > G_WARN_POS  || g < G_WARN_NEG)  ? TFT_YELLOW : TFT_DARK_GREEN;
        bg = (g > G_ALARM_POS || g < G_ALARM_NEG) ? TFT_RED    : TFT_BLACK;
        {
            String gs = buf; RowCache &gc = rowCache[SC][5];
            if (gc.value != gs || gc.fg != fg || gc.bg != bg) {
                printValue(tft, VF, ACFT_PANEL_X + hw, y4, hw, h4,
                           "G:", gs, fg, bg, COL_BACK, printState[SC][5]);
                gc.value = gs; gc.fg = fg; gc.bg = bg;
            }
        }
    }

    // Row 5 — AoA | Slip split
    {
        uint16_t y5 = rowYFor(5, ACFT_PANEL_NR), h5 = rowHFor(ACFT_PANEL_NR);

        // AoA (slot 6) — warn/alarm only on positive AoA (stall risk), matching the arc indicator
        float aoa = (state.surfaceVel > 0.5f) ? (state.pitch - state.srfVelPitch) : 0.0f;
        if (state.surfaceVel < 0.5f)          { fg = TFT_DARK_GREY; bg = TFT_BLACK; }
        else if (aoa >= AOA_ALARM_DEG)        { fg = TFT_WHITE;     bg = TFT_RED;   }
        else if (aoa >= AOA_WARN_DEG)         { fg = TFT_YELLOW;    bg = TFT_BLACK; }
        else                                  { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
        snprintf(buf, sizeof(buf), "%+.0f\xB0", aoa);
        {
            String av = (state.surfaceVel < 0.5f) ? String("---") : String(buf);
            RowCache &ac = rowCache[SC][6];
            if (ac.value != av || ac.fg != fg || ac.bg != bg) {
                printValue(tft, VF, ACFT_PANEL_X, y5, hw, h5,
                           "AoA:", av, fg, bg, COL_BACK, printState[SC][6]);
                ac.value = av; ac.fg = fg; ac.bg = bg;
            }
        }

        // Slip (slot 7) — computed in drawScreen_ACFT and passed via panel
        // Re-derive here to keep panel self-contained
        float slip = 0.0f;
        if (state.surfaceVel > 0.5f) {
            slip = state.heading - state.srfVelHeading;
            while (slip >  180.0f) slip -= 360.0f;
            while (slip < -180.0f) slip += 360.0f;
        }
        float absSlip = fabsf(slip);
        if (state.surfaceVel < 0.5f)           { fg = TFT_DARK_GREY; bg = TFT_BLACK; }
        else if (absSlip >= SLIP_ALARM_DEG)    { fg = TFT_WHITE;     bg = TFT_RED;   }
        else if (absSlip >= SLIP_WARN_DEG)     { fg = TFT_YELLOW;    bg = TFT_BLACK; }
        else                                   { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
        snprintf(buf, sizeof(buf), "%+.0f\xB0", slip);
        {
            String sv = (state.surfaceVel < 0.5f) ? String("---") : String(buf);
            RowCache &sc2 = rowCache[SC][7];
            if (sc2.value != sv || sc2.fg != fg || sc2.bg != bg) {
                printValue(tft, VF, ACFT_PANEL_X + hw, y5, hw, h5,
                           "Slip:", sv, fg, bg, COL_BACK, printState[SC][7]);
                sc2.value = sv; sc2.fg = fg; sc2.bg = bg;
            }
        }
    }

    // Row 6 — Gear | Airbrk split buttons.
    // Buttons tile on the raw row grid (no ROW_PAD) so they abut row 7 seamlessly:
    // row 6 spans [6*rowH .. 7*rowH] and row 7 spans [7*rowH .. SCREEN_H].
    {
        uint16_t y6 = TITLE_TOP + 6 * rowHFor(ACFT_PANEL_NR);
        uint16_t h6 = (TITLE_TOP + 7 * rowHFor(ACFT_PANEL_NR)) - y6 + 1;  // shares bottom border with row 7 top
        bool gearGroundState = (state.situation == sit_Landed  ||
                                state.situation == sit_Splashed ||
                                state.situation == sit_PreLaunch);
        // Gear (slot 8) — label always "GEAR", state expressed through colour
        {
            const char *gv = state.gear_on ? "DOWN" : "UP";  // cache key only
            uint16_t gfg, gbg;
            if (gearGroundState) {
                gfg = TFT_WHITE;
                gbg = state.gear_on ? TFT_DARK_GREEN : TFT_RED;
            } else if (state.gear_on) {
                gfg = TFT_WHITE;
                gbg = (state.surfaceVel > GEAR_MAX_SPEED_MS) ? TFT_YELLOW : TFT_DARK_GREEN;
            } else { gfg = TFT_DARK_GREY; gbg = TFT_OFF_BLACK; }
            RowCache &gc = rowCache[SC][8]; String gs = gv;
            if (gc.value != gs || gc.fg != gfg || gc.bg != gbg) {
                ButtonLabel btn = { "GEAR", gfg, gfg, gbg, gbg, TFT_GREY, TFT_GREY };
                drawButton(tft, ACFT_PANEL_X - 2, y6, hw + 2, h6, btn, &Roboto_Black_28, false);
                gc.value = gs; gc.fg = gfg; gc.bg = gbg;
            }
        }
        // Airbrake (slot 9) — Custom Action Group AIRBRAKE_CAG (base 38, Function Control B4;
        // see Documents/Developer/Module_UI_Reference.md). Cyan when deployed, grey when stowed.
        {
            uint16_t afg, abg;
            if (state.airbrake_on) { afg = TFT_DARK_GREY; abg = TFT_CYAN; }
            else                   { afg = TFT_DARK_GREY; abg = TFT_OFF_BLACK; }
            RowCache &ac = rowCache[SC][9]; String as = state.airbrake_on ? "OUT" : "IN";  // cache key
            if (ac.value != as || ac.fg != afg || ac.bg != abg) {
                ButtonLabel btn = { "AIRBRK", afg, afg, abg, abg, TFT_GREY, TFT_GREY };
                drawButton(tft, ACFT_PANEL_X + hw, y6, hw, h6, btn, &Roboto_Black_28, false);
                ac.value = as; ac.fg = afg; ac.bg = abg;
            }
        }
    }

    // Row 7 — Brakes | SAS buttons
    {
        uint16_t ry  = TITLE_TOP + 7 * rowHFor(ACFT_PANEL_NR);
        uint16_t rh  = SCREEN_H - ry - 1;   // bottom border on row 598 (599 is lost to overscan)
        uint16_t sasX = ACFT_PANEL_X + hw;
        uint16_t sasW = ACFT_PANEL_RIGHT - sasX;

        // Brakes (slot 10) — context-aware
        {
            bool gearGroundState = (state.situation == sit_Landed  ||
                                    state.situation == sit_Splashed ||
                                    state.situation == sit_PreLaunch);
            const char *bv = state.brakes_on ? "ON" : "OFF";
            uint16_t bfg, bbg;
            if (state.brakes_on)       { bfg = TFT_WHITE;     bbg = TFT_DARK_GREEN; }
            else if (gearGroundState)  { bfg = TFT_WHITE;     bbg = TFT_RED;        }
            else                       { bfg = TFT_DARK_GREY; bbg = TFT_OFF_BLACK;  }
            RowCache &bc = rowCache[SC][10]; String bs = bv;
            if (bc.value != bs || bc.fg != bfg || bc.bg != bbg) {
                ButtonLabel btn = { "BRAKES", bfg, bfg, bbg, bbg, TFT_GREY, TFT_GREY };
                drawButton(tft, ACFT_PANEL_X - 2, ry, hw + 2, rh, btn, &Roboto_Black_28, false);
                bc.value = bs; bc.fg = bfg; bc.bg = bbg;
            }
        }

        // SAS (slot 11) — SCFT generic colour scheme
        {
            const char *v; uint16_t sfg, sbg;
            sasNavballLabel(state.sasMode, v, sfg, sbg);
            RowCache &rc = rowCache[SC][11];
            if (rc.value != v || rc.fg != sfg || rc.bg != sbg) {
                ButtonLabel btn = { v, sfg, sfg, sbg, sbg, TFT_GREY, TFT_GREY };
                drawButton(tft, sasX, ry, sasW, rh, btn, &Roboto_Black_28, false);
                rc.value = v; rc.fg = sfg; rc.bg = sbg;
            }
        }
    }

    // Redraw horizontal dividers last — printValue fillRects erase them
    // Between V.Vrt(3)/Ma|G(4), Ma|G(4)/AoA|Slip(5), AoA|Slip(5)/Gear(6)
    static const uint8_t divRows[] = { 4, 5, 6 };
    for (uint8_t i = 0; i < 3; i++) {
        uint16_t dy = TITLE_TOP + divRows[i] * rowHFor(ACFT_PANEL_NR);
        tft.drawLine(ACFT_PANEL_X, dy,     ACFT_PANEL_RIGHT - 1, dy,     TFT_GREY);
        tft.drawLine(ACFT_PANEL_X, dy + 1, ACFT_PANEL_RIGHT - 1, dy + 1, TFT_GREY);
    }
    // Ma|G split divider (row 4)
    {
        uint16_t y4 = rowYFor(4, ACFT_PANEL_NR);
        tft.drawLine(ACFT_PANEL_X + hw,     y4, ACFT_PANEL_X + hw,     y4 + rowHFor(ACFT_PANEL_NR) - 1, TFT_GREY);
        tft.drawLine(ACFT_PANEL_X + hw + 1, y4, ACFT_PANEL_X + hw + 1, y4 + rowHFor(ACFT_PANEL_NR) - 1, TFT_GREY);
    }
    // AoA|Slip split divider (row 5)
    {
        uint16_t y5 = rowYFor(5, ACFT_PANEL_NR);
        tft.drawLine(ACFT_PANEL_X + hw,     y5, ACFT_PANEL_X + hw,     y5 + rowHFor(ACFT_PANEL_NR) - 1, TFT_GREY);
        tft.drawLine(ACFT_PANEL_X + hw + 1, y5, ACFT_PANEL_X + hw + 1, y5 + rowHFor(ACFT_PANEL_NR) - 1, TFT_GREY);
    }
}



// "TRIM" annunciation (cyan) in the graphical area's bottom-right corner: right-aligned to
// the heading-tape right edge, bottom just above the heading-tape top. Redrawn each frame
// while trim is enabled; erased once when it clears.
static void _acftDrawTrim(KCM_TFT &tft) {
    static bool prev = false;
    int16_t tw = getFontStringWidth(&Roboto_Black_24, "TRIM");
    int16_t x  = (ACFT_HDG_TAPE_X + ACFT_HDG_TAPE_W) - tw - 8;
    int16_t y  = ACFT_HDG_TAPE_Y - (int16_t)Roboto_Black_24.cap_height - 2 - 8;
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

static void drawScreen_ACFT(KCM_TFT &tft) {
    uint32_t _t0 = micros();

    bool mnvrActive = (state.mnvrTime > 0.0f);
    bool tgtAvail   = state.targetAvailable;

    // Attitude dirty — pitch/roll as before, plus heading (which now affects ball
    // because ADI markers project relative to it).
    bool ballDirty = _acftFullRedrawNeeded                                         ||
                     fabsf(state.pitch - _acftPrevPitch)                   >= 0.2f ||
                     fabsf(state.roll  - _acftPrevRoll)                    >= 0.2f ||
                     fabsf(eadiHdgDelta(state.heading, _acftPrevHeadingBall)) >= 0.5f;

    // Marker dirty — any visible marker's world position changed, or a marker's
    // availability toggled (target acquired/lost, maneuver set/cleared).
    if (!ballDirty) {
        ballDirty = fabsf(eadiHdgDelta(state.srfVelHeading, _acftPrevVelHdgBall)) >= 0.5f ||
                    fabsf(state.srfVelPitch - _acftPrevVelPitchBall)               >= 0.5f ||
                    mnvrActive != _acftPrevMnvrActiveBall                                  ||
                    tgtAvail   != _acftPrevTgtAvailBall;
    }
    if (!ballDirty && mnvrActive) {
        ballDirty = fabsf(eadiHdgDelta(state.mnvrHeading, _acftPrevMnvrHdgBall)) >= 0.5f ||
                    fabsf(state.mnvrPitch - _acftPrevMnvrPitchBall)               >= 0.5f;
    }
    if (!ballDirty && tgtAvail) {
        ballDirty = fabsf(eadiHdgDelta(state.tgtHeading, _acftPrevTgtHdgBall)) >= 0.5f ||
                    fabsf(state.tgtPitch - _acftPrevTgtPitchBall)               >= 0.5f;
    }

    if (ballDirty) {
        bool full = _acftFullRedrawNeeded;
        uint32_t t0 = micros();
        eadiDrawBall(tft, full, state.srfVelHeading, state.srfVelPitch, false);  // atmospheric: no orbital markers
        // Snapshot ball-dirty trackers for next frame's dirty check.
        _acftPrevPitch = state.pitch;
        _acftPrevRoll  = state.roll;
        _acftPrevHeadingBall    = state.heading;
        _acftPrevVelHdgBall     = state.srfVelHeading;
        _acftPrevVelPitchBall   = state.srfVelPitch;
        _acftPrevMnvrHdgBall    = state.mnvrHeading;
        _acftPrevMnvrPitchBall  = state.mnvrPitch;
        _acftPrevTgtHdgBall     = state.tgtHeading;
        _acftPrevTgtPitchBall   = state.tgtPitch;
        _acftPrevMnvrActiveBall = (state.mnvrTime > 0.0f);
        _acftPrevTgtAvailBall   = state.targetAvailable;
        _acftFullRedrawNeeded = false;
        if (debugMode) {
            uint32_t dt = micros() - t0;
            Serial.print(full ? "ACFT_FULL total=" : "ACFT_DELTA total=");
            Serial.print((float)dt / 1000.0f, 2);
            Serial.print("ms  pitch="); Serial.print(state.pitch, 1);
            Serial.print("  roll=");    Serial.println(state.roll, 1);
        }
    }

    // Compute AoA and slip — suppress below 0.5 m/s
    float aoa = 0.0f, slip = 0.0f;
    if (state.surfaceVel > 0.5f) {
        aoa  = state.pitch - state.srfVelPitch;
        slip = state.heading - state.srfVelHeading;
        while (slip >  180.0f) slip -= 360.0f;
        while (slip < -180.0f) slip += 360.0f;
    }

    eadiUpdateRollIndicator(tft, state.roll);
    _acftUpdateRollReadout(tft, state.roll);
    _acftUpdatePitchTape(tft, state.pitch);
    _acftUpdateHeadingTape(tft, state.heading);
    _acftUpdateVSI(tft, state.verticalVel);
    _acftUpdateSlipBall(tft, slip);
    _acftUpdateAoAArc(tft, aoa);
    _acftUpdatePanel(tft);

    _acftDrawTrim(tft);

    if (debugMode) {
        uint32_t _dt = micros() - _t0;
        Serial.print("ACFT frame=");
        Serial.print((float)_dt / 1000.0f, 2);
        Serial.print("ms  hdg=");  Serial.print(state.heading, 1);
        Serial.print("  vvrt=");   Serial.print(state.verticalVel, 1);
        Serial.print("  slip=");   Serial.print(slip, 2);
        Serial.print("  aoa=");    Serial.println(aoa, 2);
    }
}
