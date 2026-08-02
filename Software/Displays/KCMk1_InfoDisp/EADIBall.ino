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
