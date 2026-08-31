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

   This file now holds the complete shared ball renderer: the stateless leaf primitives
   (scanline fill, disc clip, fixed aircraft symbol), the pitch ladder (eadiDrawLadder),
   the ADI markers (eadiDrawAdiMarker), and the full/delta horizon fill (eadiFullDraw /
   eadiDeltaDraw).
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

// ── Shared attitude-ball geometry + delta-fill state ─────────────────────────────────
// SCFT and ACFT never run simultaneously (mutually-exclusive screens) and share identical
// geometry, so one set of buffers serves both; each screen resets them in its chrome via
// eadiBallResetState(). ~1.8 KB total.
static const int16_t  EADI_BALL_Y0     = EADI_CY - EADI_R;            // 94 (top scanline of ball)
static const uint16_t EADI_SCANLINES   = (uint16_t)(EADI_R * 2 + 1);  // 413
static const float    EADI_SCALE       = (float)EADI_R / 30.0f;       // px per degree
static const uint16_t EADI_DIRTY_WORDS = (EADI_SCANLINES + 15) / 16;  // 26

static int16_t  _eadiChordTable[EADI_SCANLINES];   // chord half-widths (built once)
static bool     _eadiChordTableReady = false;
static int16_t  _eadiPrevBx[EADI_SCANLINES];       // prev-frame per-scanline sky/gnd split
static bool     _eadiPrevGroundLeft  = false;
static uint16_t _eadiLadderDirty[EADI_DIRTY_WORDS];      // current-frame occupied scanlines
static uint16_t _eadiLadderDirtyPrev[EADI_DIRTY_WORDS];  // previous frame (delta erase mask)
static int16_t  _eadiPrevHorizLo = INT16_MAX;      // prev horizon-line dirty band
static int16_t  _eadiPrevHorizHi = INT16_MIN;

// Mark scanline y occupied in the current-frame dirty bitmap (ladder/markers call this).
void eadiLadderDirtySet(int16_t y) {
    int16_t i = y - EADI_BALL_Y0;
    if (i < 0 || i >= (int16_t)EADI_SCANLINES) return;
    _eadiLadderDirty[i >> 4] |= (uint16_t)(1u << (i & 15));
}
// Was scanline index i occupied last frame? (delta fill uses this to erase old pixels.)
static inline bool eadiLadderDirtyTest(uint16_t i) {
    return (_eadiLadderDirtyPrev[i >> 4] >> (i & 15)) & 1u;
}

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
// The KSP "level indicator": gold wings with a centre dip and a nose dot at the ball
// centre. Drawn last so it sits above fill/horizon/ladder.
void eadiDrawAircraftSymbol(KCM_TFT &tft) {
    drawLevelIndicator(tft, EADI_CX, EADI_CY, 48, TFT_GOLD);
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
// ═══ Extreme-attitude chevrons ═══════════════════════════════════════════════════════
// Red chevrons pointing at the horizon once it has left the disc, on both attitude
// screens -- every modern PFD draws them (50 deg up / 30 deg down on the G1000, 60/40 on
// the G5) because at an extreme attitude the ball alone stops telling you which way is
// out.
//
// The onset is OUR geometry, not Garmin's numbers. The ball is scaled R/30, so the
// horizon leaves the disc at exactly +/-30 deg in both directions and the fill goes solid
// sky or solid ground -- the renderer already has EADI_BX_ALLSKY / ALLGND sentinels for
// that state. Symmetric, because our geometry is symmetric. The labelled ladder rungs
// still give the number; what the chevrons add is the direction of recovery at a glance,
// at the moment the pilot has least attention to spare. In KSP, where aeroplanes get
// flipped regularly, that is worth more than it is in a real cockpit.
//
// NOT decluttering past 30/20/65 the way a real PFD does. Our readout panel is a separate
// region rather than an overlay on the ball, so blanking it would remove numbers a KSP
// pilot still wants while inverted and gain nothing in legibility.
//
// Drawn inside eadiDrawLadder so they inherit its dirty-scanline bookkeeping and are
// erased by the same delta pass as the rungs.
static const float   EADI_CHEV_ON_DEG  = 32.0f;   // hysteresis, so the boundary cannot
static const float   EADI_CHEV_OFF_DEG = 28.0f;   //   flicker while levelling out
static const int16_t EADI_CHEV_D1      = 52;      // apex distance from disc centre
static const int16_t EADI_CHEV_D2      = 108;
static const int16_t EADI_CHEV_LEN     = 30;      // arm run-back along the pointing axis
static const int16_t EADI_CHEV_HW      = 30;      // arm half-width across it (45 deg)
static bool _eadiChevronOn = false;

void eadiDrawExtremeChevrons(KCM_TFT &tft, float BCX, float BCY, float sinR, float cosR) {
    // Distance from the disc centre to the horizon point IS the pitch, in pixels.
    const float dx = BCX - (float)EADI_CX, dy = BCY - (float)EADI_CY;
    const float distPx = sqrtf(dx*dx + dy*dy);
    const float pitchDeg = distPx / EADI_SCALE;

    if (_eadiChevronOn) { if (pitchDeg < EADI_CHEV_OFF_DEG) _eadiChevronOn = false; }
    else                { if (pitchDeg > EADI_CHEV_ON_DEG)  _eadiChevronOn = true;  }
    if (!_eadiChevronOn || distPx < 1.0f) return;

    // Unit vector from the disc centre toward the horizon: the way out.
    const float hx = dx / distPx, hy = dy / distPx;
    const float px = -hy, py = hx;              // perpendicular, for the arms
    (void)sinR; (void)cosR;                     // roll is already carried by (hx,hy)

    const float rr = (float)EADI_R * (float)EADI_R;
    const int16_t dists[2] = { EADI_CHEV_D1, EADI_CHEV_D2 };
    for (uint8_t i = 0; i < 2; i++) {
        const float ax = (float)EADI_CX + hx * (float)dists[i];
        const float ay = (float)EADI_CY + hy * (float)dists[i];
        // Arms run back from the apex, opening away from the horizon.
        const float bx = ax - hx * (float)EADI_CHEV_LEN;
        const float by = ay - hy * (float)EADI_CHEV_LEN;
        const float e1x = bx + px * (float)EADI_CHEV_HW, e1y = by + py * (float)EADI_CHEV_HW;
        const float e2x = bx - px * (float)EADI_CHEV_HW, e2y = by - py * (float)EADI_CHEV_HW;

        // Skip a chevron that would fall outside the disc -- clipping a V leaves a shape
        // that reads as something else.
        const float m1 = (e1x-EADI_CX)*(e1x-EADI_CX) + (e1y-EADI_CY)*(e1y-EADI_CY);
        const float m2 = (e2x-EADI_CX)*(e2x-EADI_CX) + (e2y-EADI_CY)*(e2y-EADI_CY);
        if (m1 >= rr || m2 >= rr) continue;

        int16_t yLo = (int16_t)min(min(ay, e1y), e2y) - 2;
        int16_t yHi = (int16_t)max(max(ay, e1y), e2y) + 2;
        for (int16_t yd = yLo; yd <= yHi; yd++) eadiLadderDirtySet(yd);

        // Two passes one pixel apart, the same way the ladder thickens its rungs.
        for (int8_t o = 0; o <= 1; o++) {
            const int16_t ox = (int16_t)(hx * (float)o), oy = (int16_t)(hy * (float)o);
            tft.drawLine((int16_t)ax + ox, (int16_t)ay + oy,
                         (int16_t)e1x + ox, (int16_t)e1y + oy, TFT_RED);
            tft.drawLine((int16_t)ax + ox, (int16_t)ay + oy,
                         (int16_t)e2x + ox, (int16_t)e2y + oy, TFT_RED);
        }
    }
}


// Draw the pitch ladder. Rungs/labels are marked dirty for next frame's delta erase.
void eadiDrawLadder(KCM_TFT &tft, float BCX, float BCY, float sinR, float cosR) {
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
            for (int16_t yd = y_lo; yd <= y_hi; yd++) eadiLadderDirtySet(yd);
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
                for (int16_t yd = ly; yd < ly + FONT_H; yd++) eadiLadderDirtySet(yd);
                tft.setCursor(lx, ly); tft.print(lbl);
            }
        };
        placeLabel(cx1, cy1);
        placeLabel(cx2, cy2);
    }

    eadiDrawExtremeChevrons(tft, BCX, BCY, sinR, cosR);
}


// ═══ ADI ball markers (prograde / target / maneuver) ═════════════════════════════════
static const int16_t EADI_ADI_MRK_HD = 32;   // marker extent (prograde ring 18 + spoke 12)

// eadiHdgDelta (shortest-arc heading delta) now lives in KerbalDisplayCommon.

// Draw one KSP navball marker on the ADI ball at the given world-space heading/pitch
// (relative to the current vessel attitude, read from `state`). Skips the draw if it
// would fall outside the visible cone. Marks the marker's scanlines into the caller's
// `dirty` bitmap so the next delta fill repaints them (prevents trails).
void eadiDrawAdiMarker(KCM_TFT &tft, float markerHdg, float markerPitch,
                       uint16_t fillCol, uint8_t kind) {
    // True boresight projection into the cockpit frame, shared with the reticles and the
    // re-entry retro ball (KerbalDisplayCommon). The ball's own horizon line and pitch
    // ladder are still drawn on the older flat model, so a marker can sit up to ~4 px
    // off the drawn horizon at the widest visible lateral offset — within the ±25° the
    // marker clip allows. Making the ball itself spherical is a separate job.
    const KspBodyAxes ax = kspBodyAxes(state.heading, state.pitch, state.roll);
    float degRight, degUp;
    kspBoresightAngles(ax, markerHdg, markerPitch, degRight, degUp);
    int16_t sx = (int16_t)(EADI_CX + degRight * EADI_SCALE);
    int16_t sy = (int16_t)(EADI_CY - degUp    * EADI_SCALE);

    // Clip: entire marker must fit inside ball. Use (R - HD) so outline doesn't cross rim.
    int16_t dx = sx - EADI_CX, dy = sy - EADI_CY;
    int32_t rInner = (int32_t)EADI_R - EADI_ADI_MRK_HD;
    if ((int32_t)dx*dx + (int32_t)dy*dy > rInner * rInner) return;

    // KSP navball symbol — dispatch to the shared KDC glyph for this kind.
    switch (kind) {
      case KSP_MK_TARGET:      drawTargetMarker(tft, sx, sy, 22, fillCol);     break;
      case KSP_MK_ANTITARGET:  drawAntiTargetMarker(tft, sx, sy, 20, fillCol); break;
      case KSP_MK_MANEUVER:    drawManeuverMarker(tft, sx, sy, 19, fillCol);   break;
      case KSP_MK_RETROGRADE:  drawRetrogradeMarker(tft, sx, sy, 18, fillCol); break;
      case KSP_MK_NORMAL:      drawNormalMarker(tft, sx, sy, 18, fillCol);     break;
      case KSP_MK_ANTINORMAL:  drawAntiNormalMarker(tft, sx, sy, 18, fillCol); break;
      case KSP_MK_RADIAL_IN:   drawRadialInMarker(tft, sx, sy, 20, fillCol);   break;
      case KSP_MK_RADIAL_OUT:  drawRadialOutMarker(tft, sx, sy, 20, fillCol);  break;
      case KSP_MK_LEVEL:       drawLevelIndicator(tft, sx, sy, 18, fillCol);   break;
      default:                 drawProgradeMarker(tft, sx, sy, 18, fillCol);   break;
    }

    // Tell next frame's delta fill to repaint these scanlines.
    for (int16_t y = sy - EADI_ADI_MRK_HD; y <= sy + EADI_ADI_MRK_HD; y++) {
        eadiLadderDirtySet(y);
    }
}


// ═══ Horizon fill: chord table, full draw, per-scanline delta fill ═══════════════════
// Build the chord half-width lookup (integer sqrt, 4 Newton iters). Idempotent.
void eadiBuildChordTable() {
    if (_eadiChordTableReady) return;
    for (uint16_t i = 0; i < EADI_SCANLINES; i++) {
        int16_t dy  = (int16_t)i - EADI_R;
        int32_t rem = (int32_t)EADI_R*EADI_R - (int32_t)dy*dy;
        if (rem <= 0) { _eadiChordTable[i] = 0; continue; }
        int32_t x = EADI_R;
        x = (x + rem/x) >> 1; x = (x + rem/x) >> 1;
        x = (x + rem/x) >> 1; x = (x + rem/x) >> 1;
        _eadiChordTable[i] = (int16_t)x;
    }
    _eadiChordTableReady = true;
}

// Full ball sky/ground fill. Records each row's split into _eadiPrevBx for the delta fill.
void eadiFullDraw(KCM_TFT &tft, float sinR, float cosR, float K) {
    tft.fillCircle(EADI_CX, EADI_CY, EADI_R, EADI_SKY);
    bool near_horiz = (fabsf(sinR) < 0.01f);
    float bx_f    = near_horiz ? 0.0f : (K + cosR * (float)EADI_BALL_Y0) / sinR;
    float bx_step = near_horiz ? 0.0f : cosR / sinR;
    bool  gl0     = (sinR > 0.0f);
    for (uint16_t i = 0; i < EADI_SCANLINES; i++) {
        int16_t chw = _eadiChordTable[i];
        if (chw <= 0) { _eadiPrevBx[i] = EADI_BX_ALLSKY; bx_f += bx_step; continue; }
        int16_t y  = EADI_BALL_Y0 + (int16_t)i;
        int16_t x0 = EADI_CX - chw, x1 = EADI_CX + chw;
        bool    gl;
        int16_t bx;
        if (near_horiz) {
            bool sky = (-cosR * (float)y > K);
            bx = sky ? EADI_BX_ALLSKY : EADI_BX_ALLGND;
            gl = !sky;
        } else {
            bx = (int16_t)bx_f;
            gl = gl0;
            if (sinR > 0.0f) {
                if (bx <= x0) { bx = EADI_BX_ALLSKY;  gl = false; }
                else if (bx >= x1) { bx = EADI_BX_ALLGND; }
            } else {
                if (bx >= x1) { bx = EADI_BX_ALLSKY;  gl = false; }
                else if (bx <= x0) { bx = EADI_BX_ALLGND; }
            }
        }
        _eadiPrevBx[i] = bx;
        eadiDrawScanline(tft, y, x0, x1, bx, gl);
        bx_f += bx_step;
    }
    _eadiPrevGroundLeft = gl0;
}

// Per-scanline delta fill. Repaints only rows whose split changed or that fall in the
// previous horizon-line / ladder dirty bands. Reads/writes _eadiPrevBx/_eadiPrevGroundLeft
// and _eadiPrevHoriz*/dirty-prev.
void eadiDeltaDraw(KCM_TFT &tft, float sinR, float cosR, float K) {
    bool  newGL      = (sinR > 0.0f);
    bool  near_horiz = (fabsf(sinR) < 0.01f);
    float bx_f       = near_horiz ? 0.0f : (K + cosR * (float)EADI_BALL_Y0) / sinR;
    float bx_step    = near_horiz ? 0.0f : cosR / sinR;

    int16_t split_lo = 0, split_hi = (int16_t)(EADI_SCANLINES - 1);
    if (!near_horiz) {
        float pitchPx = -(K - sinR * (float)EADI_CX + cosR * (float)EADI_CY);
        float hc2 = (float)EADI_R * EADI_R - pitchPx * pitchPx;
        if (hc2 >= 0.0f) {
            float BCY = (float)EADI_CY + pitchPx * cosR;
            float hc  = sqrtf(hc2);
            float ly1 = BCY - hc * sinR, ly2 = BCY + hc * sinR;
            float ylo = (ly1 < ly2 ? ly1 : ly2) - 2.0f;
            float yhi = (ly1 > ly2 ? ly1 : ly2) + 1.0f;
            split_lo = (int16_t)(ylo - (float)EADI_BALL_Y0);
            split_hi = (int16_t)(yhi - (float)EADI_BALL_Y0);
            if (split_lo < 0) split_lo = 0;
            if (split_hi >= (int16_t)EADI_SCANLINES) split_hi = (int16_t)(EADI_SCANLINES - 1);
        }
    }

    for (uint16_t i = 0; i < EADI_SCANLINES; i++) {
        bool in_split   = ((int16_t)i >= split_lo && (int16_t)i <= split_hi);
        bool in_prev_horiz  = ((int16_t)(EADI_BALL_Y0 + i) >= _eadiPrevHorizLo &&
                                (int16_t)(EADI_BALL_Y0 + i) <= _eadiPrevHorizHi);
        bool in_prev_ladder = eadiLadderDirtyTest(i);

        int16_t chw = _eadiChordTable[i];

        if (!in_split && !in_prev_horiz && !in_prev_ladder) {
            if (chw <= 0) { bx_f += bx_step; continue; }
            int16_t bx_new_s;
            if (near_horiz) {
                int16_t y = EADI_BALL_Y0 + (int16_t)i;
                bx_new_s = (-cosR * (float)y > K) ? EADI_BX_ALLSKY : EADI_BX_ALLGND;
            } else {
                int16_t x0 = EADI_CX - chw, x1 = EADI_CX + chw;
                int16_t bx = (int16_t)bx_f;
                if (sinR > 0.0f)
                    bx_new_s = (bx <= x0) ? EADI_BX_ALLSKY : (bx >= x1) ? EADI_BX_ALLGND : bx;
                else
                    bx_new_s = (bx >= x1) ? EADI_BX_ALLSKY : (bx <= x0) ? EADI_BX_ALLGND : bx;
            }
            bx_f += bx_step;
            if (bx_new_s == _eadiPrevBx[i] && newGL == _eadiPrevGroundLeft) continue;
            int16_t y  = EADI_BALL_Y0 + (int16_t)i;
            int16_t x0 = EADI_CX - chw, x1 = EADI_CX + chw;
            bool gl = (bx_new_s == EADI_BX_ALLGND) ? newGL :
                      (bx_new_s == EADI_BX_ALLSKY)  ? false : newGL;
            eadiDrawScanline(tft, y, x0, x1, bx_new_s, gl);
            _eadiPrevBx[i] = bx_new_s;
            continue;
        }

        if (chw <= 0) { _eadiPrevBx[i] = EADI_BX_ALLSKY; bx_f += bx_step; continue; }

        int16_t y  = EADI_BALL_Y0 + (int16_t)i;
        int16_t x0 = EADI_CX - chw, x1 = EADI_CX + chw;
        bool    gl;
        int16_t bx_new;

        if (near_horiz) {
            bool sky = (-cosR * (float)y > K);
            bx_new = sky ? EADI_BX_ALLSKY : EADI_BX_ALLGND;
            gl = !sky;
        } else {
            bx_new = (int16_t)bx_f;
            gl = newGL;
            if (sinR > 0.0f) {
                if (bx_new <= x0) { bx_new = EADI_BX_ALLSKY;  gl = false; }
                else if (bx_new >= x1) { bx_new = EADI_BX_ALLGND; }
            } else {
                if (bx_new >= x1) { bx_new = EADI_BX_ALLSKY;  gl = false; }
                else if (bx_new <= x0) { bx_new = EADI_BX_ALLGND; }
            }
        }
        bx_f += bx_step;

        bool bx_changed = (bx_new != _eadiPrevBx[i] || newGL != _eadiPrevGroundLeft);

        if (!bx_changed && !in_prev_horiz && !in_prev_ladder) continue;

        eadiDrawScanline(tft, y, x0, x1, bx_new, gl);
        _eadiPrevBx[i] = bx_new;
    }
    _eadiPrevGroundLeft = newGL;
}

// Swap the ladder dirty bitmaps: prev = last frame's set (for the delta fill), current
// cleared for this frame's ladder/marker marks. Call once per frame before drawing them.
void eadiBallSwapDirty() {
    memcpy(_eadiLadderDirtyPrev, _eadiLadderDirty, sizeof(_eadiLadderDirty));
    memset(_eadiLadderDirty, 0, sizeof(_eadiLadderDirty));
}

// Record this frame's horizon-line dirty band (read by next frame's delta fill).
void eadiBallSetPrevHoriz(int16_t lo, int16_t hi) {
    _eadiPrevHorizLo = lo;
    _eadiPrevHorizHi = hi;
}

// Reset all shared ball delta-fill state. Each screen calls this in its chrome on entry.
void eadiBallResetState() {
    eadiBuildChordTable();
    memset(_eadiLadderDirty,     0, sizeof(_eadiLadderDirty));
    memset(_eadiLadderDirtyPrev, 0, sizeof(_eadiLadderDirtyPrev));
    _eadiChevronOn   = false;
    _eadiPrevHorizLo = INT16_MAX;
    _eadiPrevHorizHi = INT16_MIN;
}


// ═══ Master ball update ══════════════════════════════════════════════════════════════
// Renders one attitude-ball frame: sky/ground fill (full or delta), horizon line, pitch
// ladder, ADI markers, aircraft symbol. Vessel attitude and target/maneuver come from
// `state`; the prograde source differs per screen (SCFT orbital vs ACFT surface velocity)
// so it is passed in. The caller snapshots its ball-dirty trackers after this returns.
// Compute the orbital normal and radial-out marker directions (compass heading + pitch)
// from the velocity direction, using the local vertical as the radial reference:
//   normal    = up x velocity  (horizontal, perpendicular to the ground track)
//   radial-out= up orthogonalised to velocity  (points away from the body)
// Anti-normal / radial-in are the antipodes. Only meaningful for an orbital velocity.
static void eadiOrbitalDirs(float velHdg, float velPitch,
                            float &normHdg, float &normPitch, float &radHdg, float &radPitch) {
    float th = velHdg * (float)DEG_TO_RAD, ph = velPitch * (float)DEG_TO_RAD, cph = cosf(ph);
    float vE = cph * sinf(th), vN = cph * cosf(th), vU = sinf(ph);   // velocity in East/North/Up
    normHdg   = atan2f(-vN, vE) * (float)RAD_TO_DEG;                 // up x v (horizontal)
    normPitch = 0.0f;                                               // normal is always on the horizon
    float rE = -vU * vE, rN = -vU * vN, rU = 1.0f - vU * vU;         // up - (up.v) v
    float rlen = sqrtf(rE*rE + rN*rN + rU*rU); if (rlen < 1e-4f) rlen = 1e-4f;
    rU /= rlen; if (rU > 1.0f) rU = 1.0f; else if (rU < -1.0f) rU = -1.0f;
    radHdg   = atan2f(rE, rN) * (float)RAD_TO_DEG;
    radPitch = asinf(rU) * (float)RAD_TO_DEG;
}

void eadiDrawBall(KCM_TFT &tft, bool fullRedraw, float progradeHdg, float progradePitch, bool orbital) {
    eadiBuildChordTable();   // no-op after first call

    float pitch = state.pitch;
    float roll  = -state.roll;   // negate: KerbalSimpit sign convention

    float cosR    = cosf(roll * (float)DEG_TO_RAD);
    float sinR    = sinf(roll * (float)DEG_TO_RAD);
    float pitchPx = pitch * EADI_SCALE;

    float K   = sinR * (float)EADI_CX - cosR * (float)EADI_CY - pitchPx;
    float BCX = (float)EADI_CX - pitchPx * sinR;
    float BCY = (float)EADI_CY + pitchPx * cosR;
    float hc2 = (float)EADI_R * EADI_R - pitchPx * pitchPx;

    uint32_t _t0, _t1;

    // ── 1. Sky/ground fill ────────────────────────────────────────────────────────────
    _t0 = micros();
    if (fullRedraw) eadiFullDraw(tft, sinR, cosR, K);
    else            eadiDeltaDraw(tft, sinR, cosR, K);
    _t1 = micros();
    if (debugMode) { Serial.print(fullRedraw ? "  fill(FULL)=" : "  fill(DELTA)=");
                     Serial.print((_t1-_t0)/1000.0f, 2); Serial.print("ms"); }

    // ── 2. Horizon line ───────────────────────────────────────────────────────────────
    _t0 = micros();
    int16_t new_horiz_lo = INT16_MAX, new_horiz_hi = INT16_MIN;
    if (hc2 >= 0.0f) {
        float hc  = max(0.0f, sqrtf(hc2) - 3.0f);
        int16_t lx1 = (int16_t)(BCX - hc * cosR);
        int16_t ly1 = (int16_t)(BCY - hc * sinR);
        int16_t lx2 = (int16_t)(BCX + hc * cosR);
        int16_t ly2 = (int16_t)(BCY + hc * sinR);
        tft.drawLine(lx1, ly1, lx2, ly2, EADI_HORIZON);
        auto rnd = [](float v) -> int16_t {
            return (int16_t)(v + (v > 0.0f ? 0.5f : -0.5f));
        };
        int16_t px = rnd(sinR), py = rnd(-cosR);
        tft.drawLine(lx1+px, ly1+py, lx2+px, ly2+py, EADI_HORIZON);
        new_horiz_lo = min(min(ly1, ly2), min((int16_t)(ly1+py), (int16_t)(ly2+py)));
        new_horiz_hi = max(max(ly1, ly2), max((int16_t)(ly1+py), (int16_t)(ly2+py)));
    }
    eadiBallSetPrevHoriz(new_horiz_lo, new_horiz_hi);
    _t1 = micros();
    if (debugMode) { Serial.print("  horiz="); Serial.print((_t1-_t0)/1000.0f, 2); Serial.print("ms"); }

    // ── 3. Pitch ladder ───────────────────────────────────────────────────────────────
    // Swap bitmaps: prev = last frame's dirty set (used by delta fill above),
    // current = cleared for this frame's ladder draw.
    _t0 = micros();
    eadiBallSwapDirty();
    eadiDrawLadder(tft, BCX, BCY, sinR, cosR);
    _t1 = micros();
    if (debugMode) { Serial.print("  ladder="); Serial.print((_t1-_t0)/1000.0f, 2); Serial.print("ms"); }

    // ── 4. ADI markers — prograde + retrograde always; target if available; maneuver if
    //       active. (Retrograde is antipodal to prograde; it clips out of the visible cone
    //       when the nose is prograde-side, like the KSP navball.) ─────────────────────
    eadiDrawAdiMarker(tft, progradeHdg,          progradePitch,  TFT_NEON_GREEN, KSP_MK_PROGRADE);
    eadiDrawAdiMarker(tft, progradeHdg + 180.0f, -progradePitch, TFT_NEON_GREEN, KSP_MK_RETROGRADE);
    if (state.targetAvailable) {
        eadiDrawAdiMarker(tft, state.tgtHeading,          state.tgtPitch,  TFT_VIOLET, KSP_MK_TARGET);
        eadiDrawAdiMarker(tft, state.tgtHeading + 180.0f, -state.tgtPitch, TFT_VIOLET, KSP_MK_ANTITARGET);
    }
    if (state.mnvrTime > 0.0f)
        eadiDrawAdiMarker(tft, state.mnvrHeading, state.mnvrPitch, TFT_BLUE, KSP_MK_MANEUVER);

    // Orbital-frame markers (SCFT only): normal/anti-normal (magenta) and radial-out/in
    // (cyan), computed from the orbital velocity. Each clips to the visible cone.
    if (orbital) {
        float nH, nP, rH, rP;
        eadiOrbitalDirs(progradeHdg, progradePitch, nH, nP, rH, rP);
        eadiDrawAdiMarker(tft, nH,          nP,  TFT_MAGENTA, KSP_MK_NORMAL);
        eadiDrawAdiMarker(tft, nH + 180.0f, -nP, TFT_MAGENTA, KSP_MK_ANTINORMAL);
        eadiDrawAdiMarker(tft, rH,          rP,  TFT_CYAN,    KSP_MK_RADIAL_OUT);
        eadiDrawAdiMarker(tft, rH + 180.0f, -rP, TFT_CYAN,    KSP_MK_RADIAL_IN);
    }

    // ── 5. Aircraft symbol — drawn last so it is always on top ────────────────────────
    eadiDrawAircraftSymbol(tft);
}


// ═══ Shared PFD tapes / boxes / roll readout (SCFT + ACFT) ═══════════════════════════
// SCFT and ACFT draw pixel-identical pitch/heading tapes, pitch/heading value boxes, and
// a roll readout. Historically each screen carried its own near-identical copy; the only
// real differences are (a) which markers each tape draws and (b) the roll warn/alarm
// colouring. The scaffolding lives here; the differences are passed in as parameters.
// Geometry MUST equal the per-screen SCFT_*/ACFT_* constants — it is re-derived below
// from the same EADI_CX/CY/R single source of truth, using the identical formulas.

// ── Pitch tape geometry (== SCFT_PTAPE_* == ACFT_PTAPE_*) ────────────────────────────
static const int16_t EADI_PTAPE_W        = 36;
static const int16_t EADI_PTAPE_GAP      = 27;
static const int16_t EADI_PTAPE_X        = EADI_CX - EADI_R - EADI_PTAPE_GAP - EADI_PTAPE_W;
static const int16_t EADI_PTAPE_Y        = EADI_CY - EADI_R;
static const int16_t EADI_PTAPE_H        = EADI_CY + EADI_R + 8 - (EADI_CY - EADI_R);
static const float   EADI_PTAPE_SCALE    = EADI_SCALE;                       // R/30 px/deg
static const int16_t EADI_PTAPE_BOX_W    = 68;
static const int16_t EADI_PTAPE_BOX_H    = 38;
static const int16_t EADI_PTAPE_BOX_X    = EADI_PTAPE_X + EADI_PTAPE_W - 68;
static const int16_t EADI_PTAPE_BOX_Y    = EADI_CY - EADI_PTAPE_BOX_H / 2;
static const int16_t EADI_PTAPE_SUPP_LO  = EADI_PTAPE_BOX_Y - 10;
static const int16_t EADI_PTAPE_SUPP_HI  = EADI_PTAPE_BOX_Y + EADI_PTAPE_BOX_H + 10;
static const int16_t EADI_PTAPE_MRK_BASE_X = EADI_PTAPE_X + EADI_PTAPE_W - 2;
static const int16_t EADI_PTAPE_MRK_TIP_X  = EADI_PTAPE_X + EADI_PTAPE_W - 22;
static const int16_t EADI_PTAPE_MRK_HW     = 9;

// ── Heading tape geometry (== SCFT_HDG_* == ACFT_HDG_*) ──────────────────────────────
static const int16_t EADI_HDG_TAPE_W     = (EADI_R * 2) + 54;
static const int16_t EADI_HDG_TAPE_X     = EADI_CX - (EADI_HDG_TAPE_W / 2);
static const int16_t EADI_HDG_TAPE_Y     = EADI_CY + EADI_R + 8;
static const int16_t EADI_HDG_TAPE_H     = 32;
static const float   EADI_HDG_SCALE      = (float)(EADI_R * 2) / 60.0f;
static const int16_t EADI_HDG_LABEL_LO   = EADI_HDG_TAPE_X + 8;
static const int16_t EADI_HDG_LABEL_HI   = EADI_HDG_TAPE_X + EADI_HDG_TAPE_W - 8;
static const int16_t EADI_HDG_BOX_W      = 72;
static const int16_t EADI_HDG_BOX_H      = EADI_HDG_TAPE_H + 8;
static const int16_t EADI_HDG_BOX_X      = EADI_CX - (EADI_HDG_BOX_W / 2);
static const int16_t EADI_HDG_BOX_Y      = EADI_HDG_TAPE_Y;
static const int16_t EADI_HDG_SUPP_LO    = EADI_HDG_BOX_X - 18;
static const int16_t EADI_HDG_SUPP_HI    = EADI_HDG_BOX_X + EADI_HDG_BOX_W + 18;
static const int16_t EADI_HDG_MRK_BASE_Y = EADI_HDG_TAPE_Y + 2;
static const int16_t EADI_HDG_MRK_TIP_Y  = EADI_HDG_TAPE_Y + 24;
static const int16_t EADI_HDG_MRK_HW     = 9;

// ── Roll readout geometry (== SCFT_ROLL_* == ACFT_ROLL_*) ────────────────────────────
static const int16_t EADI_ROLL_ANCHOR_X  = EADI_CX + EADI_R - 54;
static const int16_t EADI_ROLL_ANCHOR_Y  = TITLE_TOP;
static const int16_t EADI_ROLL_W         = 80;
static const int16_t EADI_ROLL_TXT_W     = EADI_ROLL_W + 6;
static const int16_t EADI_ROLL_LABEL_H   = 30;
static const int16_t EADI_ROLL_VALUE_H   = 38;
static const int16_t EADI_ROLL_GAP       = 3;

// Draw/update the pitch value box — cached on integer change. Caller owns `prevBox`.
void eadiUpdatePitchBox(KCM_TFT &tft, float pitch, int16_t &prevBox) {
    int16_t iPitch = (int16_t)roundf(pitch);
    if (iPitch == prevBox) return;

    char newBuf[8];
    snprintf(newBuf, sizeof(newBuf), "%+d\xB0", iPitch);

    if (prevBox > -9000) {
        char oldBuf[8];
        snprintf(oldBuf, sizeof(oldBuf), "%+d\xB0", prevBox);
        eraseCenteredValue(tft, &Roboto_Black_28,
                   EADI_PTAPE_BOX_X, EADI_PTAPE_BOX_Y + 1,
                   EADI_PTAPE_BOX_W, EADI_PTAPE_BOX_H - 2,
                   oldBuf, TFT_BLACK);
    }
    textCenter(tft, &Roboto_Black_28,
               EADI_PTAPE_BOX_X, EADI_PTAPE_BOX_Y + 1,
               EADI_PTAPE_BOX_W, EADI_PTAPE_BOX_H - 2,
               newBuf, TFT_DARK_GREEN, TFT_BLACK);

    prevBox = iPitch;
}

// Draw the full pitch tape. `markers` (value = pitch°, colour) draws the per-screen
// marker set as left-pointing triangles on the tape's right edge.
void eadiDrawPitchTape(KCM_TFT &tft, float pitch,
                       const EadiTapeMarker *markers, uint8_t nMarkers) {
    // Clear tape in two passes, skipping the box area and staying 1px inside borders
    int16_t fillW  = EADI_PTAPE_W - 1;  // stop 1px short of right border
    int16_t aboveH = EADI_PTAPE_BOX_Y - EADI_PTAPE_Y;
    int16_t belowY = EADI_PTAPE_BOX_Y + EADI_PTAPE_BOX_H;
    int16_t belowH = (EADI_PTAPE_Y + EADI_PTAPE_H - 1) - belowY;  // stop 1px short of bottom border
    tft.fillRect(EADI_PTAPE_X, EADI_PTAPE_Y, fillW, aboveH, TFT_BLACK);
    tft.fillRect(EADI_PTAPE_X, belowY,       fillW, belowH, TFT_BLACK);

    // Redraw box border (sides may have been touched by above/below fills)
    tft.drawRect(EADI_PTAPE_BOX_X, EADI_PTAPE_BOX_Y, EADI_PTAPE_BOX_W, EADI_PTAPE_BOX_H, TFT_LIGHT_GREY);
    // Box interior not erased — no need to reset the caller's pitch-box cache

    tft.setFont(Roboto_Black_12);

    // Draw ticks from pitch-32 to pitch+32 (slightly beyond ±30° visible range)
    for (int16_t dp = -32; dp <= 32; dp++) {
        float deg = pitch + (float)dp;
        if (deg < -90.0f || deg > 90.0f) continue;  // KSP pitch clamped ±90°

        // Pixel y: current pitch stays at centre (EADI_CY), offset by dp degrees
        int16_t py = (int16_t)(EADI_CY - (float)dp * EADI_PTAPE_SCALE);

        // Clip to tape interior
        if (py <= EADI_PTAPE_Y || py >= EADI_PTAPE_Y + EADI_PTAPE_H) continue;

        // Suppress near value box
        if (py >= EADI_PTAPE_SUPP_LO && py <= EADI_PTAPE_SUPP_HI) continue;

        int16_t ideg = (int16_t)roundf(deg);

        if (ideg % 10 == 0) {
            // Major tick — right-aligned, stopping 1px short of right border
            int16_t tx0 = EADI_PTAPE_X + EADI_PTAPE_W - 11;
            int16_t tx1 = EADI_PTAPE_X + EADI_PTAPE_W - 2;
            tft.drawLine(tx0, py, tx1, py, TFT_LIGHT_GREY);

            // Label — left of tick, clamped to tape
            char lbl[8];
            snprintf(lbl, sizeof(lbl), "%+d", ideg);
            int16_t lx = EADI_PTAPE_X + 2;
            int16_t ly = py - 6;
            if (ly < EADI_PTAPE_Y + 1) ly = EADI_PTAPE_Y + 1;
            if (ly + 12 > EADI_PTAPE_Y + EADI_PTAPE_H - 3)
                ly = EADI_PTAPE_Y + EADI_PTAPE_H - 15;
            // Only draw if label y is not in suppress zone
            if (!(ly + 6 >= EADI_PTAPE_SUPP_LO && ly + 6 <= EADI_PTAPE_SUPP_HI)) {
                tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
                tft.setCursor(lx, ly);
                tft.print(lbl);
            }
        } else if (ideg % 2 == 0) {
            // Minor tick (every 2°) — stopping 1px short of right border
            int16_t tx0 = EADI_PTAPE_X + EADI_PTAPE_W - 7;
            int16_t tx1 = EADI_PTAPE_X + EADI_PTAPE_W - 2;
            tft.drawLine(tx0, py, tx1, py, TFT_DARK_GREY);
        }
    }

    // Redraw the tape's bottom border — the lowest number labels' opaque black
    // background can paint over it, and it is otherwise only drawn once in chrome.
    tft.drawLine(EADI_PTAPE_X - 1,                EADI_PTAPE_Y + EADI_PTAPE_H - 1,
                 EADI_PTAPE_X + EADI_PTAPE_W - 1, EADI_PTAPE_Y + EADI_PTAPE_H - 1, TFT_LIGHT_GREY);

    // Draw pitch markers (left-pointing triangles on right edge)
    auto drawPitchMarker = [&](float markerPitch, uint16_t col) {
        float diff = markerPitch - pitch;
        int16_t py = (int16_t)(EADI_CY - diff * EADI_PTAPE_SCALE);
        // Peg to tape edges rather than hiding
        int16_t pyMin = EADI_PTAPE_Y + EADI_PTAPE_MRK_HW + 1;
        int16_t pyMax = EADI_PTAPE_Y + EADI_PTAPE_H - EADI_PTAPE_MRK_HW - 2;
        if (py < pyMin) py = pyMin;
        if (py > pyMax) py = pyMax;
        if (py >= EADI_PTAPE_SUPP_LO && py <= EADI_PTAPE_SUPP_HI) return;
        tft.fillTriangle(EADI_PTAPE_MRK_TIP_X,  py,
                         EADI_PTAPE_MRK_BASE_X,  py - EADI_PTAPE_MRK_HW,
                         EADI_PTAPE_MRK_BASE_X,  py + EADI_PTAPE_MRK_HW,
                         col);
    };

    for (uint8_t i = 0; i < nMarkers; i++)
        drawPitchMarker(markers[i].value, markers[i].colour);
}

// Draw/update the heading number box — cached, only redraws when integer heading changes.
// Uses textCenter for flicker-free rendering: erase old value with black-on-black first.
void eadiUpdateHdgBox(KCM_TFT &tft, float hdg, int16_t &prevBox) {
    int16_t iHdg = (int16_t)roundf(hdg) % 360;
    if (iHdg < 0) iHdg += 360;
    if (iHdg == prevBox) return;

    char oldBuf[8], newBuf[8];
    snprintf(newBuf, sizeof(newBuf), "%03d\xB0", iHdg);

    // Erase previous value with black-on-black
    if (prevBox >= 0) {
        snprintf(oldBuf, sizeof(oldBuf), "%03d\xB0", prevBox);
        eraseCenteredValue(tft, &Roboto_Black_28,
                   EADI_HDG_BOX_X, EADI_HDG_BOX_Y + 1,
                   EADI_HDG_BOX_W, EADI_HDG_BOX_H - 2,
                   oldBuf, TFT_BLACK);
    }

    // Draw new value
    textCenter(tft, &Roboto_Black_28,
               EADI_HDG_BOX_X, EADI_HDG_BOX_Y + 1,
               EADI_HDG_BOX_W, EADI_HDG_BOX_H - 2,
               newBuf, TFT_DARK_GREEN, TFT_BLACK);

    prevBox = iHdg;
}

// Draw the full heading tape. Only the tape strip — box is handled separately. The fill
// blackens the box interior, so the caller's `prevHdgBox` cache is forced to -1 to make
// the next box update redraw. `markers` (value = heading°, colour) is the per-screen set.
void eadiDrawHeadingTape(KCM_TFT &tft, float hdg, int16_t &prevHdgBox,
                         const EadiTapeMarker *markers, uint8_t nMarkers) {
    while (hdg <   0.0f) hdg += 360.0f;
    while (hdg >= 360.0f) hdg -= 360.0f;

    tft.fillRect(EADI_HDG_TAPE_X, EADI_HDG_TAPE_Y, EADI_HDG_TAPE_W, EADI_HDG_TAPE_H, TFT_BLACK);

    // Redraw box border after fill (fill erases box sides where they overlap)
    tft.drawRect(EADI_HDG_BOX_X, EADI_HDG_BOX_Y, EADI_HDG_BOX_W, EADI_HDG_BOX_H, TFT_LIGHT_GREY);

    // Force box number to redraw — fill blackened the interior
    prevHdgBox = -1;

    tft.setFont(Roboto_Black_12);

    for (int16_t d = -32; d <= 32; d++) {
        float deg = hdg + (float)d;
        while (deg <   0.0f) deg += 360.0f;
        while (deg >= 360.0f) deg -= 360.0f;

        int16_t px  = (int16_t)(EADI_CX + d * EADI_HDG_SCALE);
        // Strict clip — exclude boundary pixels to prevent residual at edges
        if (px <= EADI_HDG_TAPE_X || px >= EADI_HDG_TAPE_X + EADI_HDG_TAPE_W) continue;

        // Suppress elements near the box (expanded to cover label text extents)
        if (px >= EADI_HDG_SUPP_LO && px <= EADI_HDG_SUPP_HI) continue;

        int16_t ideg = (int16_t)roundf(deg);
        if (ideg == 360) ideg = 0;

        if (ideg % 10 == 0) {
            tft.drawLine(px, EADI_HDG_TAPE_Y, px, EADI_HDG_TAPE_Y + 10, TFT_LIGHT_GREY);

            if (px >= EADI_HDG_LABEL_LO && px <= EADI_HDG_LABEL_HI) {
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
                if (cx < EADI_HDG_TAPE_X + 1) cx = EADI_HDG_TAPE_X + 1;
                if (cx + lw > EADI_HDG_TAPE_X + EADI_HDG_TAPE_W - 1)
                    cx = EADI_HDG_TAPE_X + EADI_HDG_TAPE_W - 1 - lw;
                tft.setCursor(cx, EADI_HDG_TAPE_Y + 12);
                tft.print(lbl);
            }
        } else if (ideg % 2 == 0) {
            tft.drawLine(px, EADI_HDG_TAPE_Y, px, EADI_HDG_TAPE_Y + 6, TFT_DARK_GREY);
        }
    }

    // Draw heading markers after ticks so they render on top
    auto drawMarker = [&](float markerHdg, uint16_t col) {
        // Find angular offset with wrap
        float diff = markerHdg - hdg;
        while (diff >  180.0f) diff -= 360.0f;
        while (diff < -180.0f) diff += 360.0f;
        int16_t px = (int16_t)(EADI_CX + diff * EADI_HDG_SCALE);
        // Peg to tape edges (leave room for half-width) rather than hiding
        int16_t pxMin = EADI_HDG_TAPE_X + EADI_HDG_MRK_HW + 1;
        int16_t pxMax = EADI_HDG_TAPE_X + EADI_HDG_TAPE_W - EADI_HDG_MRK_HW - 1;
        if (px < pxMin) px = pxMin;
        if (px > pxMax) px = pxMax;
        // Skip if in suppress zone
        if (px >= EADI_HDG_SUPP_LO && px <= EADI_HDG_SUPP_HI) return;
        tft.fillTriangle(px,                    EADI_HDG_MRK_TIP_Y,
                         px - EADI_HDG_MRK_HW,  EADI_HDG_MRK_BASE_Y,
                         px + EADI_HDG_MRK_HW,  EADI_HDG_MRK_BASE_Y,
                         col);
    };

    for (uint8_t i = 0; i < nMarkers; i++)
        drawMarker(markers[i].value, markers[i].colour);
}

// Update the roll numeric readout — two lines right-justified toward the panel divider.
// `fg`/`bg` are computed per screen (SCFT: fixed dark-green; ACFT: roll warn/alarm).
// Caller owns `prevReadout`/`prevFg`.
void eadiUpdateRollReadout(KCM_TFT &tft, float roll, uint16_t fg, uint16_t bg,
                           int16_t &prevReadout, uint16_t &prevFg) {
    int16_t iRoll = (int16_t)roundf(roll);

    if (iRoll == prevReadout && fg == prevFg) return;

    // Erase previous value — right-justified glyph box (matches textRight below).
    if (prevReadout > -9000) {
        char oldBuf[8];
        snprintf(oldBuf, sizeof(oldBuf), "%+d\xB0", prevReadout);
        int16_t ow   = getFontStringWidth(&Roboto_Black_28, oldBuf);
        int16_t capH = (int16_t)Roboto_Black_28.cap_height;
        int16_t ex   = EADI_ROLL_ANCHOR_X + EADI_ROLL_TXT_W - ow - TEXT_BORDER;
        int16_t ey   = (EADI_ROLL_ANCHOR_Y + EADI_ROLL_LABEL_H + EADI_ROLL_GAP)
                       + (EADI_ROLL_VALUE_H - capH) / 2;
        tft.fillRect(ex - 1, ey, ow + 2, capH, TFT_BLACK);
    }

    // Line 1: "Roll:" — label row, right-justified toward the panel divider
    textRight(tft, &Roboto_Black_24,
              EADI_ROLL_ANCHOR_X, EADI_ROLL_ANCHOR_Y,
              EADI_ROLL_TXT_W, EADI_ROLL_LABEL_H,
              "Roll:", TFT_WHITE, TFT_BLACK);

    // Line 2: signed value — larger font, right-justified in value row
    char buf[8];
    snprintf(buf, sizeof(buf), "%+d\xB0", iRoll);
    textRight(tft, &Roboto_Black_28,
              EADI_ROLL_ANCHOR_X, EADI_ROLL_ANCHOR_Y + EADI_ROLL_LABEL_H + EADI_ROLL_GAP,
              EADI_ROLL_TXT_W, EADI_ROLL_VALUE_H,
              buf, fg, bg);

    prevReadout = iRoll;
    prevFg      = fg;
}
