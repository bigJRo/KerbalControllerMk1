/***************************************************************************************
   EADIBall.ino -- Shared EADI attitude-ball renderer for the SPACECRAFT (SCFT) and
   AIRCRAFT (ACFT) screens.

   SCFT and ACFT draw a pixel-identical attitude ball (same centre, radius, colours);
   historically each carried its own private copy of the whole renderer (~1000 duplicate
   lines). These functions are the shared implementation. Because every .ino compiles into
   one translation unit, non-static functions here are callable from both screens, and the
   EADI_* constants below are visible throughout the sketch.

   Geometry MUST stay identical to the per-screen SCFT_ / ACFT_ constants (all == the
   values below). The screens keep their own SCFT_ / ACFT_ copies for their tapes/panels;
   these EADI_ values are the single source of truth for the shared ball code.

   Extraction is staged (each stage compiles on its own). Stage 1 (this file, initial):
   the pure, stateless leaf primitives — scanline fill, disc clip, fixed aircraft symbol.
   Later stages add the pitch ladder, ADI markers, and the horizon full/delta fill.
****************************************************************************************/
#include "KCMk1_InfoDisp.h"

// ── Shared ball geometry / colours (mirror SCFT_*/ACFT_*) ────────────────────────────
static const int16_t  EADI_CX        = 345;
static const int16_t  EADI_CY        = 300;
static const int16_t  EADI_R         = 206;
static const uint16_t EADI_SKY       = TFT_ROYAL;
static const uint16_t EADI_GND       = TFT_UPS_BROWN;
static const uint16_t EADI_HORIZON   = TFT_WHITE;
static const uint16_t EADI_WINGS     = TFT_YELLOW;
static const uint16_t EADI_LADDER    = TFT_WHITE;
static const int16_t  EADI_BX_ALLSKY = INT16_MIN;   // scanline is entirely sky
static const int16_t  EADI_BX_ALLGND = INT16_MAX;   // scanline is entirely ground

// ── Draw one horizon scanline ────────────────────────────────────────────────────────
// bx is the sky/ground split x for row y (or the ALLSKY/ALLGND sentinel). groundLeft
// selects which side of bx is ground.
void eadiDrawScanline(KCM_TFT &tft, int16_t y, int16_t x0, int16_t x1,
                      int16_t bx, bool groundLeft) {
    if (bx == EADI_BX_ALLSKY) {
        tft.drawLine(x0, y, x1, y, EADI_SKY);
    } else if (bx == EADI_BX_ALLGND) {
        tft.drawLine(x0, y, x1, y, EADI_GND);
    } else if (groundLeft) {
        tft.drawLine(x0,   y, bx,   y, EADI_GND);
        tft.drawLine(bx+1, y, x1,   y, EADI_SKY);
    } else {
        tft.drawLine(x0,   y, bx-1, y, EADI_SKY);
        tft.drawLine(bx,   y, x1,   y, EADI_GND);
    }
}

// ── Aircraft symbol (fixed, horizontal) ──────────────────────────────────────────────
// Centre dot on top of yellow wings + fin. Drawn last so it sits above fill/horizon/ladder.
void eadiDrawAircraftSymbol(KCM_TFT &tft) {
    static const int16_t DOT_R   = 9;    // dot radius → 19px diameter
    static const int16_t WI      = 17;   // wing inner edge (DOT_R + gap)
    static const int16_t WO      = 60;   // wing outer edge
    static const int16_t WH      = 2;    // wing half-height → 5px total
    static const int16_t FIN_GAP = 9;    // gap between dot bottom and fin top
    static const int16_t FIN_H   = 24;   // fin height
    static const int16_t FIN_W   = 2;    // fin half-width → 5px total

    // Left wing
    tft.fillRect(EADI_CX - WO,    EADI_CY - WH, WO - WI,    WH*2+1, EADI_WINGS);
    // Right wing
    tft.fillRect(EADI_CX + WI,    EADI_CY - WH, WO - WI,    WH*2+1, EADI_WINGS);
    // Fin
    tft.fillRect(EADI_CX - FIN_W, EADI_CY + DOT_R + FIN_GAP, FIN_W*2+1, FIN_H, EADI_WINGS);
    // Centre dot — drawn last so it sits on top of wings/fin overlap at centre
    tft.fillCircle(EADI_CX, EADI_CY, DOT_R, EADI_WINGS);
}

// ── Clip endpoint to disc ─────────────────────────────────────────────────────────────
// Given a segment p->q, return in (ox,oy) the point q clipped to the ball disc boundary.
void eadiClipToDisk(float px, float py, float qx, float qy,
                    float &ox, float &oy) {
    float cx = qx - EADI_CX, cy = qy - EADI_CY;
    if (cx*cx + cy*cy <= (float)EADI_R * EADI_R) { ox = qx; oy = qy; return; }
    float dx = qx-px, dy = qy-py;
    float ax = px-EADI_CX, ay = py-EADI_CY;
    float a = dx*dx + dy*dy;
    float b = 2.0f*(ax*dx + ay*dy);
    float c = ax*ax + ay*ay - (float)EADI_R*(float)EADI_R;
    float disc = b*b - 4.0f*a*c;
    if (disc < 0.0f || a < 0.001f) { ox = qx; oy = qy; return; }
    float sq = sqrtf(disc);
    float t  = (-b - sq) / (2.0f*a);
    if (t < 0.0f || t > 1.0f) t = (-b + sq) / (2.0f*a);
    t = max(0.0f, min(1.0f, t));
    ox = px + t*dx; oy = py + t*dy;
}


// ═══ Bank / roll arc ═════════════════════════════════════════════════════════════════
// The bank pointer + scale ticks/labels sit just outside the bezel. Shared by SCFT/ACFT.
static const int16_t EADI_PTR_TIP_R  = EADI_R + 3;    // tip clear of bezel (bezel outer = R+2)
static const int16_t EADI_PTR_BASE_R = EADI_R + 22;   // base beyond tick outer (R+16), below labels
static const int16_t EADI_PTR_W      = 12;            // half-width of pointer base

static float _eadiPrevRollIndicator = -9999.0f;       // last drawn pointer angle
void eadiResetRollIndicator() { _eadiPrevRollIndicator = -9999.0f; }

// Draw the roll pointer triangle for a given roll angle.
void eadiDrawRollPointer(KCM_TFT &tft, float roll, uint16_t colour) {
    float a    = (roll - 90.0f) * (float)DEG_TO_RAD;
    float cosA = cosf(a), sinA = sinf(a);
    int16_t tx  = (int16_t)(EADI_CX + EADI_PTR_TIP_R  * cosA);
    int16_t ty  = (int16_t)(EADI_CY + EADI_PTR_TIP_R  * sinA);
    int16_t bcx = (int16_t)(EADI_CX + EADI_PTR_BASE_R * cosA);
    int16_t bcy = (int16_t)(EADI_CY + EADI_PTR_BASE_R * sinA);
    int16_t b1x = bcx + (int16_t)(-sinA * EADI_PTR_W);
    int16_t b1y = bcy + (int16_t)( cosA * EADI_PTR_W);
    int16_t b2x = bcx - (int16_t)(-sinA * EADI_PTR_W);
    int16_t b2y = bcy - (int16_t)( cosA * EADI_PTR_W);
    tft.fillTriangle(tx, ty, b1x, b1y, b2x, b2y, colour);
}

// Erase the roll pointer — a generously expanded triangle to catch stray rotated-edge
// pixels. The wider erase reaches the R+28 bank labels, so the caller redraws any within.
void eadiEraseRollPointer(KCM_TFT &tft, float roll) {
    float a    = (roll - 90.0f) * (float)DEG_TO_RAD;
    float cosA = cosf(a), sinA = sinf(a);
    int16_t tx  = (int16_t)(EADI_CX + (EADI_PTR_TIP_R  - 1) * cosA);
    int16_t ty  = (int16_t)(EADI_CY + (EADI_PTR_TIP_R  - 1) * sinA);
    int16_t bcx = (int16_t)(EADI_CX + (EADI_PTR_BASE_R + 3) * cosA);
    int16_t bcy = (int16_t)(EADI_CY + (EADI_PTR_BASE_R + 3) * sinA);
    int16_t b1x = bcx + (int16_t)(-sinA * (EADI_PTR_W + 9));
    int16_t b1y = bcy + (int16_t)( cosA * (EADI_PTR_W + 9));
    int16_t b2x = bcx - (int16_t)(-sinA * (EADI_PTR_W + 9));
    int16_t b2y = bcy - (int16_t)( cosA * (EADI_PTR_W + 9));
    tft.fillTriangle(tx, ty, b1x, b1y, b2x, b2y, TFT_BLACK);
}

// Draw a single bank-scale angle label ("30" / "60") at R+28 along the bank radial.
void eadiDrawBankLabel(KCM_TFT &tft, int16_t bankDeg) {
    const char *txt = (bankDeg == 60 || bankDeg == -60) ? "60" : "30";
    float   a    = (bankDeg - 90.0f) * (float)DEG_TO_RAD;
    int16_t lc_x = (int16_t)(EADI_CX + (EADI_R + 28) * cosf(a));
    int16_t lc_y = (int16_t)(EADI_CY + (EADI_R + 28) * sinf(a));
    int16_t lw   = getFontStringWidth(&Roboto_Black_16, txt);
    tft.setFont(Roboto_Black_16);
    tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
    tft.setCursor(lc_x - lw / 2, lc_y - 9);   // 9 ~ cap-height/2 for Roboto_Black_16
    tft.print(txt);
}

// Draw a single bank scale tick at the given bank angle.
void eadiDrawBankTick(KCM_TFT &tft, int16_t bankDeg) {
    bool isMajor = (bankDeg == 0 || bankDeg == 30 || bankDeg == -30 ||
                                     bankDeg == 60 || bankDeg == -60);
    int16_t tOuter = EADI_R + 16;
    int16_t tInner = EADI_R + (isMajor ? 2 : 6);
    uint16_t col = (bankDeg == 0 || bankDeg == 60 || bankDeg == -60)
                   ? TFT_WHITE : TFT_LIGHT_GREY;   // 0 and +/-60 white, rest light grey
    float a    = (bankDeg - 90.0f) * (float)DEG_TO_RAD;
    float cosA = cosf(a), sinA = sinf(a);
    int16_t ox = (int16_t)(EADI_CX + tOuter * cosA);
    int16_t oy = (int16_t)(EADI_CY + tOuter * sinA);
    int16_t ix = (int16_t)(EADI_CX + tInner * cosA);
    int16_t iy = (int16_t)(EADI_CY + tInner * sinA);
    tft.drawLine(ox, oy, ix, iy, col);
}

// Update the roll pointer: erase old, redraw any tick/label it covered, draw new.
void eadiUpdateRollIndicator(KCM_TFT &tft, float roll) {
    if (fabsf(roll - _eadiPrevRollIndicator) < 0.2f) return;

    static const int16_t ticks[] = {-60,-45,-30,-20,-10,0,10,20,30,45,60};

    // Erase old pointer with expanded triangle to catch stray pixels
    if (_eadiPrevRollIndicator > -9000.0f) {
        float prevClamped = _eadiPrevRollIndicator;
        if      (prevClamped >  60.0f) prevClamped =  60.0f;
        else if (prevClamped < -60.0f) prevClamped = -60.0f;
        eadiEraseRollPointer(tft, prevClamped);
        // Redraw any bank label the (wider) erase reached into (+/-30/+/-60, within a
        // 12deg window) BEFORE the ticks, so a label's opaque box can't clip a tick.
        static const int16_t labelBanks[] = {-60, -30, 30, 60};
        for (uint8_t i = 0; i < 4; i++) {
            if (fabsf(_eadiPrevRollIndicator - labelBanks[i]) < 12.0f) {
                eadiDrawBankLabel(tft, labelBanks[i]);
            }
        }
        // Redraw every tick the (wider) erase region may have covered (within 7deg —
        // do NOT break, the enlarged erase can span two adjacent ticks).
        for (uint8_t i = 0; i < 11; i++) {
            if (fabsf(_eadiPrevRollIndicator - ticks[i]) < 7.0f) {
                eadiDrawBankTick(tft, ticks[i]);
            }
        }
    }

    // Draw new pointer — clamped to +/-60deg so it stays within the scale marks
    float clampedRoll = roll;
    if      (clampedRoll >  60.0f) clampedRoll =  60.0f;
    else if (clampedRoll < -60.0f) clampedRoll = -60.0f;
    eadiDrawRollPointer(tft, clampedRoll, TFT_YELLOW);
    _eadiPrevRollIndicator = roll;
}


// ═══ Pitch ladder ════════════════════════════════════════════════════════════════════
static const int16_t  EADI_BALL_Y0   = EADI_CY - EADI_R;             // 94 (top scanline of ball)
static const uint16_t EADI_SCANLINES = (uint16_t)(EADI_R * 2 + 1);   // 413
static const float    EADI_SCALE     = (float)EADI_R / 30.0f;        // px per degree

// Mark scanline y occupied in the caller's per-scanline dirty bitmap (so the delta fill
// erases the rung/label next frame). `dirty` is the caller-owned array (>= 26 words).
void eadiLadderDirtySet(uint16_t *dirty, int16_t y) {
    int16_t i = y - EADI_BALL_Y0;
    if (i < 0 || i >= (int16_t)EADI_SCANLINES) return;
    dirty[i >> 4] |= (uint16_t)(1u << (i & 15));
}

// Draw the pitch ladder. Rungs are marked into `dirty` for next frame's delta erase.
void eadiDrawLadder(KCM_TFT &tft, float BCX, float BCY, float sinR, float cosR,
                    uint16_t *dirty) {
    static const int16_t HL_MAJ  = 47;   // major rung half-length
    static const int16_t HL_MIN  = 29;   // minor rung half-length
    static const int16_t LBL_GAP = 8;
    static const uint8_t FONT_W  = 9;    // Roboto_Black_16 digit advance
    static const uint8_t FONT_H  = 19;   // Roboto_Black_16 cap height

    tft.setFont(Roboto_Black_16);
    tft.setTextColor(EADI_LADDER);

    auto rnd = [](float v) -> int16_t {
        return (int16_t)(v + (v > 0.0f ? 0.5f : -0.5f));
    };
    int16_t spx = rnd(sinR), spy = rnd(-cosR);

    auto clampToDisc = [](int16_t &px, int16_t &py) {
        float dx = (float)px - EADI_CX, dy = (float)py - EADI_CY;
        float d2 = dx*dx + dy*dy;
        if (d2 > (float)(EADI_R-1) * (float)(EADI_R-1)) {
            float s = (float)(EADI_R-1) / sqrtf(d2);
            px = EADI_CX + (int16_t)(dx*s);
            py = EADI_CY + (int16_t)(dy*s);
        }
    };

    auto boxInDisc = [](int16_t lx, int16_t ly, uint8_t lw, uint8_t lh) -> bool {
        for (int8_t cx = 0; cx <= 1; cx++)
            for (int8_t cy = 0; cy <= 1; cy++) {
                float dx = (float)(lx + cx*(int16_t)lw) - EADI_CX;
                float dy = (float)(ly + cy*(int16_t)lh) - EADI_CY;
                if (dx*dx + dy*dy >= (float)EADI_R * EADI_R) return false;
            }
        return true;
    };

    float R2 = (float)EADI_R * (float)EADI_R;

    // Pitch clamped +/-90deg in KSP. Step in 5deg increments.
    for (int16_t lad_p_x2 = -180; lad_p_x2 <= 180; lad_p_x2 += 10) {
        if (lad_p_x2 == 0) continue;   // skip horizon (drawn separately)
        float  lad_p = (float)lad_p_x2 * 0.5f;
        float  delta = lad_p * EADI_SCALE;

        // Rung foot = ball centre + lad_p offset along sky-ward (sinR,-cosR)
        float rfx = BCX + delta * sinR;
        float rfy = BCY - delta * cosR;

        float fd2 = (rfx-EADI_CX)*(rfx-EADI_CX) + (rfy-EADI_CY)*(rfy-EADI_CY);
        if (fd2 >= R2) continue;

        bool is_major = (lad_p_x2 % 20 == 0);   // divisible by 10deg
        float hl = is_major ? (float)HL_MAJ : (float)HL_MIN;

        // Rung line along horizon direction (cosR, sinR)
        float rx1 = rfx - hl*cosR, ry1 = rfy - hl*sinR;
        float rx2 = rfx + hl*cosR, ry2 = rfy + hl*sinR;

        float cx1, cy1, cx2, cy2;
        eadiClipToDisk(rfx, rfy, rx1, ry1, cx1, cy1);
        eadiClipToDisk(rfx, rfy, rx2, ry2, cx2, cy2);

        int16_t lx1 = (int16_t)cx1, ly1 = (int16_t)cy1;
        int16_t lx2 = (int16_t)cx2, ly2 = (int16_t)cy2;
        int16_t lx1b = lx1+spx, ly1b = ly1+spy;
        int16_t lx2b = lx2+spx, ly2b = ly2+spy;
        clampToDisc(lx1b, ly1b);
        clampToDisc(lx2b, ly2b);

        // Mark every scanline the rung touches as dirty for next frame's delta erase.
        {
            int16_t y_lo = min(min(ly1, ly2), min(ly1b, ly2b));
            int16_t y_hi = max(max(ly1, ly2), max(ly1b, ly2b));
            for (int16_t yd = y_lo; yd <= y_hi; yd++) eadiLadderDirtySet(dirty, yd);
        }

        tft.drawLine(lx1,  ly1,  lx2,  ly2,  EADI_LADDER);
        tft.drawLine(lx1b, ly1b, lx2b, ly2b, EADI_LADDER);

        if (!is_major) continue;   // only label 10deg multiples

        int16_t abs_p   = lad_p_x2 < 0 ? -lad_p_x2 : lad_p_x2;
        abs_p /= 2;   // back to degrees
        char    lbl[4];
        lbl[0] = '0' + (int8_t)(abs_p / 10);
        lbl[1] = '0' + (int8_t)(abs_p % 10);
        lbl[2] = '\0';
        uint8_t lw = (uint8_t)(strlen(lbl) * FONT_W);

        // Push each label away from the rung foot along the rung direction.
        float rung_cx = (cx1 + cx2) * 0.5f;  // rung foot screen x (midpoint of clipped ends)
        auto placeLabel = [&](float ex, float ey) {
            float dx = ex - rung_cx;
            float sign = (dx >= 0.0f) ? 1.0f : -1.0f;
            int16_t lx = (int16_t)(ex + (float)LBL_GAP * sign);
            if (sign > 0.0f && lx < (int16_t)ex)  lx = (int16_t)ex + LBL_GAP;
            if (sign < 0.0f && lx + lw > (int16_t)ex) lx = (int16_t)ex - LBL_GAP - lw;
            int16_t ly = (int16_t)(ey) - FONT_H/2;
            if (boxInDisc(lx, ly, lw, FONT_H)) {
                for (int16_t yd = ly; yd < ly + FONT_H; yd++) eadiLadderDirtySet(dirty, yd);
                tft.setCursor(lx, ly); tft.print(lbl);
            }
        };
        placeLabel(cx1, cy1);
        placeLabel(cx2, cy2);
    }
}


// ═══ ADI ball markers (prograde / target / maneuver) ═════════════════════════════════
static const int16_t EADI_ADI_MRK_HD = 28;   // marker extent (prograde ring 18 + spoke 9)

// Shortest-arc delta between two headings, result in [-180, 180].
float eadiHdgDelta(float a, float b) {
    float d = a - b;
    while (d >  180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return d;
}

// Draw one KSP navball marker on the ADI ball at the given world-space heading/pitch
// (relative to the current vessel attitude, read from `state`). Skips the draw if it
// would fall outside the visible cone. Marks the marker's scanlines into the caller's
// `dirty` bitmap so the next delta fill repaints them (prevents trails).
void eadiDrawAdiMarker(KCM_TFT &tft, float markerHdg, float markerPitch,
                       uint16_t fillCol, uint8_t kind, uint16_t *dirty) {
    // Delta from current vessel attitude
    float dh = eadiHdgDelta(markerHdg, state.heading);
    float dp = markerPitch - state.pitch;

    // Ball uses negated roll (matches KerbalSimpit convention)
    float cosR = cosf(-state.roll * (float)DEG_TO_RAD);
    float sinR = sinf(-state.roll * (float)DEG_TO_RAD);

    // Unrolled-frame offset: +dh degrees rightward, +dp degrees upward (so -y)
    float ux = dh * EADI_SCALE;
    float uy = -dp * EADI_SCALE;

    // Apply roll rotation (angle = -state.roll)
    int16_t sx = (int16_t)(EADI_CX + ux * cosR - uy * sinR);
    int16_t sy = (int16_t)(EADI_CY + ux * sinR + uy * cosR);

    // Clip: entire marker must fit inside ball. Use (R - HD) so outline doesn't cross rim.
    int16_t dx = sx - EADI_CX, dy = sy - EADI_CY;
    int32_t rInner = (int32_t)EADI_R - EADI_ADI_MRK_HD;
    if ((int32_t)dx*dx + (int32_t)dy*dy > rInner * rInner) return;

    // KSP navball symbol — prograde (velocity) / target / maneuver
    switch (kind) {
      case KSP_MK_TARGET:   drawTargetMarker(tft, sx, sy, 22, fillCol);   break;
      case KSP_MK_MANEUVER: drawManeuverMarker(tft, sx, sy, 19, fillCol); break;
      default:              drawProgradeMarker(tft, sx, sy, 18, fillCol); break;
    }

    // Tell next frame's delta fill to repaint these scanlines.
    for (int16_t y = sy - EADI_ADI_MRK_HD; y <= sy + EADI_ADI_MRK_HD; y++) {
        eadiLadderDirtySet(dirty, y);
    }
}
