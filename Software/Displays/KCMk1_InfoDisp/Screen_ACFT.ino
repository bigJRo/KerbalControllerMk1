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
static const int16_t  ACFT_CX        = 295;
static const int16_t  ACFT_CY        = 248;   // raised 12px vs SCFT to make room for slip ball
static const int16_t  ACFT_R         = 150;   // smaller than SCFT (164) for same reason
static const float    ACFT_SCALE     = (float)ACFT_R / 30.0f;   // 5.0 px/deg exactly

static const int16_t  ACFT_BALL_Y0   = ACFT_CY - ACFT_R;         // 98
static const int16_t  ACFT_BALL_Y1   = ACFT_CY + ACFT_R;         // 398
static const uint16_t ACFT_SCANLINES = (uint16_t)(ACFT_R * 2 + 1); // 301

// ── Right panel geometry ───────────────────────────────────────────────────────────────
// Panel left = HDG tape right + 2. HDG tape: x = CX - (R*2+54)/2 = 260-177 = 83, w = 354
static const int16_t  ACFT_PANEL_X       = ACFT_CX - (ACFT_R*2+54)/2 + (ACFT_R*2+54) + 2; // 439
static const int16_t  ACFT_PANEL_RIGHT   = 720;
static const int16_t  ACFT_PANEL_W       = ACFT_PANEL_RIGHT - ACFT_PANEL_X;  // 281
static const uint8_t  ACFT_PANEL_NR      = 8;

// ── Right panel state ──────────────────────────────────────────────────────────────────
// Cache managed by rowCache[screen_ACFT] / printState[screen_ACFT] slots 0-8.
// No additional state variables needed — printValue handles dirty detection.

// Colours
static const uint16_t ACFT_SKY      = TFT_ROYAL;
static const uint16_t ACFT_GND      = TFT_UPS_BROWN;
static const uint16_t ACFT_HORIZON  = TFT_WHITE;
static const uint16_t ACFT_WINGS    = TFT_YELLOW;
static const uint16_t ACFT_LADDER   = TFT_WHITE;

static const int16_t  ACFT_BX_ALLSKY = INT16_MIN;
static const int16_t  ACFT_BX_ALLGND = INT16_MAX;


// ── Per-frame state ───────────────────────────────────────────────────────────────────
static int16_t  _acftPrevBx[ACFT_SCANLINES];
static bool     _acftPrevGroundLeft;

static float    _acftPrevPitch   = -9999.0f;
static float    _acftPrevRoll    = -9999.0f;

// Aircraft always uses surface velocity — no orbital mode switching
static bool     _acftFullRedrawNeeded = true;

// Horizon line dirty band (small — just 1-2px tall after clipping)
static int16_t  _acftPrevHorizLo  = INT16_MAX;
static int16_t  _acftPrevHorizHi  = INT16_MIN;

// Per-scanline ladder dirty bitmap — 1 bit per scanline.
static const uint16_t ACFT_DIRTY_WORDS = (ACFT_SCANLINES + 15) / 16;
static uint16_t _acftLadderDirty[ACFT_DIRTY_WORDS];
static uint16_t _acftLadderDirtyPrev[ACFT_DIRTY_WORDS];

static inline void _acftLadderDirtySet(int16_t y) {
    int16_t i = y - ACFT_BALL_Y0;
    if (i < 0 || i >= (int16_t)ACFT_SCANLINES) return;
    _acftLadderDirty[i >> 4] |= (uint16_t)(1u << (i & 15));
}
static inline bool _acftLadderDirtyTest(uint16_t i) {
    return (_acftLadderDirtyPrev[i >> 4] >> (i & 15)) & 1u;
}

// Chord half-width lookup table — precomputed once.
static int16_t _acftChordTable[ACFT_SCANLINES];
static bool    _acftChordTableReady = false;

static void _acftBuildChordTable() {
    if (_acftChordTableReady) return;
    for (uint16_t i = 0; i < ACFT_SCANLINES; i++) {
        int16_t dy  = (int16_t)i - ACFT_R;
        int32_t rem = (int32_t)ACFT_R*ACFT_R - (int32_t)dy*dy;
        if (rem <= 0) { _acftChordTable[i] = 0; continue; }
        int32_t x = ACFT_R;
        x = (x + rem/x) >> 1; x = (x + rem/x) >> 1;
        x = (x + rem/x) >> 1; x = (x + rem/x) >> 1;
        _acftChordTable[i] = (int16_t)x;
    }
    _acftChordTableReady = true;
}

static inline float _acftSin(float deg) { return sinf(deg * DEG_TO_RAD); }
static inline float _acftCos(float deg) { return cosf(deg * DEG_TO_RAD); }




// ── Draw one scanline ─────────────────────────────────────────────────────────────────
static void _acftDrawScanline(RA8875 &tft,
                              int16_t y, int16_t x0, int16_t x1,
                              int16_t bx, bool groundLeft) {
    if (bx == ACFT_BX_ALLSKY) {
        tft.drawLine(x0, y, x1, y, ACFT_SKY);
    } else if (bx == ACFT_BX_ALLGND) {
        tft.drawLine(x0, y, x1, y, ACFT_GND);
    } else if (groundLeft) {
        tft.drawLine(x0,   y, bx,   y, ACFT_GND);
        tft.drawLine(bx+1, y, x1,   y, ACFT_SKY);
    } else {
        tft.drawLine(x0,   y, bx-1, y, ACFT_SKY);
        tft.drawLine(bx,   y, x1,   y, ACFT_GND);
    }
}


// ── Aircraft symbol (fixed, horizontal) ──────────────────────────────────────────────
// Centre dot: radius 7 (15px total diameter).
// Wings: inner gap WI=14 (7px clear of dot edge), outer WO=50, height WH=3 (7px total).
// Fin: 7px wide, 20px tall, starting 7px below dot edge (gap=7px).
// Drawn as the very last element so it is always on top of fill, horizon, and ladder.
static void _acftDrawAircraftSymbol(RA8875 &tft) {
    static const int16_t DOT_R  = 7;    // dot radius → 15px diameter
    static const int16_t WI     = 14;   // wing inner edge (DOT_R + 7px gap)
    static const int16_t WO     = 50;   // wing outer edge
    static const int16_t WH     = 2;    // wing half-height → 5px total
    static const int16_t FIN_GAP = 7;   // gap between dot bottom and fin top
    static const int16_t FIN_H  = 20;   // fin height
    static const int16_t FIN_W  = 2;    // fin half-width → 5px total

    // Left wing
    tft.fillRect(ACFT_CX - WO,    ACFT_CY - WH, WO - WI,    WH*2+1, ACFT_WINGS);
    // Right wing
    tft.fillRect(ACFT_CX + WI,    ACFT_CY - WH, WO - WI,    WH*2+1, ACFT_WINGS);
    // Fin
    tft.fillRect(ACFT_CX - FIN_W, ACFT_CY + DOT_R + FIN_GAP, FIN_W*2+1, FIN_H, ACFT_WINGS);
    // Centre dot — drawn last so it sits on top of wings/fin overlap at centre
    tft.fillCircle(ACFT_CX, ACFT_CY, DOT_R, ACFT_WINGS);
}


// ── Clip endpoint to disc ─────────────────────────────────────────────────────────────
static void _acftClipToDisk(float px, float py, float qx, float qy,
                            float &ox, float &oy) {
    float cx = qx - ACFT_CX, cy = qy - ACFT_CY;
    if (cx*cx + cy*cy <= (float)ACFT_R * ACFT_R) { ox = qx; oy = qy; return; }
    float dx = qx-px, dy = qy-py;
    float ax = px-ACFT_CX, ay = py-ACFT_CY;
    float a = dx*dx + dy*dy;
    float b = 2.0f*(ax*dx + ay*dy);
    float c = ax*ax + ay*ay - (float)ACFT_R*(float)ACFT_R;
    float disc = b*b - 4.0f*a*c;
    if (disc < 0.0f || a < 0.001f) { ox = qx; oy = qy; return; }
    float sq = sqrtf(disc);
    float t  = (-b - sq) / (2.0f*a);
    if (t < 0.0f || t > 1.0f) t = (-b + sq) / (2.0f*a);
    t = max(0.0f, min(1.0f, t));
    ox = px + t*dx; oy = py + t*dy;
}


// ── Pitch ladder ──────────────────────────────────────────────────────────────────────
// Marks each scanline it touches in _acftLadderDirty[] so delta fill can erase old pixels.
static void _acftDrawLadder(RA8875 &tft,
                            float BCX, float BCY,
                            float sinR, float cosR) {
    static const int16_t HL_MAJ  = 36;
    static const int16_t HL_MIN  = 22;
    static const int16_t LBL_GAP = 8;
    static const uint8_t FONT_W  = 8;
    static const uint8_t FONT_H  = 14;

    tft.setFont(&Roboto_Black_12);
    tft.setTextColor(ACFT_LADDER);

    auto rnd = [](float v) -> int16_t {
        return (int16_t)(v + (v > 0.0f ? 0.5f : -0.5f));
    };
    int16_t spx = rnd(sinR), spy = rnd(-cosR);

    auto clampToDisc = [](int16_t &px, int16_t &py) {
        float dx = (float)px - ACFT_CX, dy = (float)py - ACFT_CY;
        float d2 = dx*dx + dy*dy;
        if (d2 > (float)(ACFT_R-1) * (float)(ACFT_R-1)) {
            float s = (float)(ACFT_R-1) / sqrtf(d2);
            px = ACFT_CX + (int16_t)(dx*s);
            py = ACFT_CY + (int16_t)(dy*s);
        }
    };

    auto boxInDisc = [](int16_t lx, int16_t ly, uint8_t lw, uint8_t lh) -> bool {
        for (int8_t cx = 0; cx <= 1; cx++)
            for (int8_t cy = 0; cy <= 1; cy++) {
                float dx = (float)(lx + cx*(int16_t)lw) - ACFT_CX;
                float dy = (float)(ly + cy*(int16_t)lh) - ACFT_CY;
                if (dx*dx + dy*dy >= (float)ACFT_R * ACFT_R) return false;
            }
        return true;
    };

    float R2 = (float)ACFT_R * (float)ACFT_R;

    // Pitch clamped ±90° in KSP. Step in 5° increments.
    for (int16_t lad_p_x2 = -180; lad_p_x2 <= 180; lad_p_x2 += 10) {
        if (lad_p_x2 == 0) continue;   // skip horizon (drawn separately)
        float  lad_p = (float)lad_p_x2 * 0.5f;
        float  delta = lad_p * ACFT_SCALE;

        // Rung foot = ball centre + lad_p offset along sky-ward (sinR,-cosR)
        // Equivalently: (BCX + delta*sinR, BCY - delta*cosR)
        float rfx = BCX + delta * sinR;
        float rfy = BCY - delta * cosR;

        float fd2 = (rfx-ACFT_CX)*(rfx-ACFT_CX) + (rfy-ACFT_CY)*(rfy-ACFT_CY);
        if (fd2 >= R2) continue;

        bool is_major = (lad_p_x2 % 20 == 0);   // divisible by 10°
        // else minor (5°)
        float hl = is_major ? (float)HL_MAJ : (float)HL_MIN;

        // Rung line along horizon direction (cosR, sinR)
        float rx1 = rfx - hl*cosR, ry1 = rfy - hl*sinR;
        float rx2 = rfx + hl*cosR, ry2 = rfy + hl*sinR;

        float cx1, cy1, cx2, cy2;
        _acftClipToDisk(rfx, rfy, rx1, ry1, cx1, cy1);
        _acftClipToDisk(rfx, rfy, rx2, ry2, cx2, cy2);

        int16_t lx1 = (int16_t)cx1, ly1 = (int16_t)cy1;
        int16_t lx2 = (int16_t)cx2, ly2 = (int16_t)cy2;
        int16_t lx1b = lx1+spx, ly1b = ly1+spy;
        int16_t lx2b = lx2+spx, ly2b = ly2+spy;
        clampToDisc(lx1b, ly1b);
        clampToDisc(lx2b, ly2b);

        // Mark every scanline the rung touches as dirty for next frame's delta erase.
        // drawLine touches all y values between its endpoints — mark that range.
        {
            int16_t y_lo = min(min(ly1, ly2), min(ly1b, ly2b));
            int16_t y_hi = max(max(ly1, ly2), max(ly1b, ly2b));
            for (int16_t yd = y_lo; yd <= y_hi; yd++) _acftLadderDirtySet(yd);
        }

        tft.drawLine(lx1,  ly1,  lx2,  ly2,  ACFT_LADDER);
        tft.drawLine(lx1b, ly1b, lx2b, ly2b, ACFT_LADDER);

        if (!is_major) continue;   // only label 10° multiples

        int16_t abs_p   = lad_p_x2 < 0 ? -lad_p_x2 : lad_p_x2;
        abs_p /= 2;   // back to degrees
        char    lbl[4];
        lbl[0] = '0' + (int8_t)(abs_p / 10);
        lbl[1] = '0' + (int8_t)(abs_p % 10);
        lbl[2] = '\0';
        uint8_t lw = (uint8_t)(strlen(lbl) * FONT_W);

        // Push each label away from the rung foot along the rung direction.
        // Then clamp lx so the label box edge never crosses back into the rung x range.
        float rung_cx = (cx1 + cx2) * 0.5f;  // rung foot screen x (midpoint of clipped ends)
        auto placeLabel = [&](float ex, float ey) {
            // Direction from rung midpoint to this endpoint
            float dx = ex - rung_cx;
            float sign = (dx >= 0.0f) ? 1.0f : -1.0f;
            int16_t lx = (int16_t)(ex + (float)LBL_GAP * sign);
            if (sign > 0.0f && lx < (int16_t)ex)  lx = (int16_t)ex + LBL_GAP;
            if (sign < 0.0f && lx + lw > (int16_t)ex) lx = (int16_t)ex - LBL_GAP - lw;
            int16_t ly = (int16_t)(ey) - FONT_H/2;
            if (boxInDisc(lx, ly, lw, FONT_H)) {
                // Mark all label rows dirty
                for (int16_t yd = ly; yd < ly + FONT_H; yd++) _acftLadderDirtySet(yd);
                tft.setCursor(lx, ly); tft.print(lbl);
            }
        };
        placeLabel(cx1, cy1);
        placeLabel(cx2, cy2);
    }
}


// ── Full ball draw ────────────────────────────────────────────────────────────────────
static void _acftFullDraw(RA8875 &tft, float sinR, float cosR, float K) {
    tft.fillCircle(ACFT_CX, ACFT_CY, ACFT_R, ACFT_SKY);

    // Incremental bx: bx(y) = (K + cosR*y)/sinR
    // bx(y+1) = bx(y) + cosR/sinR  → one float add per row instead of multiply+divide
    bool near_horiz = (fabsf(sinR) < 0.01f);
    float bx_f    = near_horiz ? 0.0f : (K + cosR * (float)ACFT_BALL_Y0) / sinR;
    float bx_step = near_horiz ? 0.0f : cosR / sinR;
    bool  gl0     = (sinR > 0.0f);

    for (uint16_t i = 0; i < ACFT_SCANLINES; i++) {
        int16_t chw = _acftChordTable[i];
        if (chw <= 0) { _acftPrevBx[i] = ACFT_BX_ALLSKY; bx_f += bx_step; continue; }
        int16_t y  = ACFT_BALL_Y0 + (int16_t)i;
        int16_t x0 = ACFT_CX - chw, x1 = ACFT_CX + chw;
        bool    gl;
        int16_t bx;
        if (near_horiz) {
            bool sky = (-cosR * (float)y > K);
            bx = sky ? ACFT_BX_ALLSKY : ACFT_BX_ALLGND;
            gl = !sky;
        } else {
            bx = (int16_t)bx_f;
            gl = gl0;
            if (sinR > 0.0f) {
                if (bx <= x0) { bx = ACFT_BX_ALLSKY;  gl = false; }
                else if (bx >= x1) { bx = ACFT_BX_ALLGND; }
            } else {
                if (bx >= x1) { bx = ACFT_BX_ALLSKY;  gl = false; }
                else if (bx <= x0) { bx = ACFT_BX_ALLGND; }
            }
        }
        _acftPrevBx[i] = bx;
        _acftDrawScanline(tft, y, x0, x1, bx, gl);
        bx_f += bx_step;
    }
    _acftPrevGroundLeft = gl0;
}


// ── Delta update ──────────────────────────────────────────────────────────────────────
static void _acftDeltaDraw(RA8875 &tft, float sinR, float cosR, float K) {
    bool  newGL      = (sinR > 0.0f);
    bool  near_horiz = (fabsf(sinR) < 0.01f);
    float bx_f       = near_horiz ? 0.0f : (K + cosR * (float)ACFT_BALL_Y0) / sinR;
    float bx_step    = near_horiz ? 0.0f : cosR / sinR;

    // Analytically bound the split-row band using horizon chord endpoints.
    // Outside this band every row is all-sky or all-gnd; we only need to repaint
    // them if they transition (bx_changed) or are in the horizon/ladder dirty bands.
    // The chord y-range in scanline index space is ly_lo_i..ly_hi_i.
    // We also track the previous and current horizon bands here.
    int16_t split_lo = 0, split_hi = (int16_t)(ACFT_SCANLINES - 1);
    if (!near_horiz) {
        float pitchPx = -(K - sinR * (float)ACFT_CX + cosR * (float)ACFT_CY);
        float hc2 = (float)ACFT_R * ACFT_R - pitchPx * pitchPx;
        if (hc2 >= 0.0f) {
            float BCY = (float)ACFT_CY + pitchPx * cosR;
            float hc  = sqrtf(hc2);
            float ly1 = BCY - hc * sinR, ly2 = BCY + hc * sinR;
            float ylo = (ly1 < ly2 ? ly1 : ly2) - 2.0f;
            float yhi = (ly1 > ly2 ? ly1 : ly2) + 1.0f;
            split_lo = (int16_t)(ylo - (float)ACFT_BALL_Y0);
            split_hi = (int16_t)(yhi - (float)ACFT_BALL_Y0);
            if (split_lo < 0) split_lo = 0;
            if (split_hi >= (int16_t)ACFT_SCANLINES) split_hi = (int16_t)(ACFT_SCANLINES - 1);
        }
    }

    for (uint16_t i = 0; i < ACFT_SCANLINES; i++) {
        bool in_split   = ((int16_t)i >= split_lo && (int16_t)i <= split_hi);
        bool in_prev_horiz  = ((int16_t)(ACFT_BALL_Y0 + i) >= _acftPrevHorizLo &&
                                (int16_t)(ACFT_BALL_Y0 + i) <= _acftPrevHorizHi);
        bool in_prev_ladder = _acftLadderDirtyTest(i);

        int16_t chw = _acftChordTable[i];

        // For rows outside the split band: compute the sentinel bx quickly
        // and only skip if it hasn't changed. This catches the case where
        // the horizon sweeps into a previously all-sky/all-gnd row.
        if (!in_split && !in_prev_horiz && !in_prev_ladder) {
            if (chw <= 0) { bx_f += bx_step; continue; }
            // Compute sentinel for this row
            int16_t bx_new_s;
            if (near_horiz) {
                int16_t y = ACFT_BALL_Y0 + (int16_t)i;
                bx_new_s = (-cosR * (float)y > K) ? ACFT_BX_ALLSKY : ACFT_BX_ALLGND;
            } else {
                int16_t x0 = ACFT_CX - chw, x1 = ACFT_CX + chw;
                int16_t bx = (int16_t)bx_f;
                if (sinR > 0.0f)
                    bx_new_s = (bx <= x0) ? ACFT_BX_ALLSKY : (bx >= x1) ? ACFT_BX_ALLGND : bx;
                else
                    bx_new_s = (bx >= x1) ? ACFT_BX_ALLSKY : (bx <= x0) ? ACFT_BX_ALLGND : bx;
            }
            bx_f += bx_step;
            if (bx_new_s == _acftPrevBx[i] && newGL == _acftPrevGroundLeft) continue;
            // Sentinel changed — fall through to full repaint below by re-entering loop
            // We already advanced bx_f, so use bx_new_s directly
            int16_t y  = ACFT_BALL_Y0 + (int16_t)i;
            int16_t x0 = ACFT_CX - chw, x1 = ACFT_CX + chw;
            bool gl = (bx_new_s == ACFT_BX_ALLGND) ? newGL :
                      (bx_new_s == ACFT_BX_ALLSKY)  ? false : newGL;
            _acftDrawScanline(tft, y, x0, x1, bx_new_s, gl);
            _acftPrevBx[i] = bx_new_s;
            continue;
        }

        if (chw <= 0) { _acftPrevBx[i] = ACFT_BX_ALLSKY; bx_f += bx_step; continue; }

        int16_t y  = ACFT_BALL_Y0 + (int16_t)i;
        int16_t x0 = ACFT_CX - chw, x1 = ACFT_CX + chw;
        bool    gl;
        int16_t bx_new;

        if (near_horiz) {
            bool sky = (-cosR * (float)y > K);
            bx_new = sky ? ACFT_BX_ALLSKY : ACFT_BX_ALLGND;
            gl = !sky;
        } else {
            bx_new = (int16_t)bx_f;
            gl = newGL;
            if (sinR > 0.0f) {
                if (bx_new <= x0) { bx_new = ACFT_BX_ALLSKY;  gl = false; }
                else if (bx_new >= x1) { bx_new = ACFT_BX_ALLGND; }
            } else {
                if (bx_new >= x1) { bx_new = ACFT_BX_ALLSKY;  gl = false; }
                else if (bx_new <= x0) { bx_new = ACFT_BX_ALLGND; }
            }
        }
        bx_f += bx_step;

        bool bx_changed = (bx_new != _acftPrevBx[i] || newGL != _acftPrevGroundLeft);

        if (!bx_changed && !in_prev_horiz && !in_prev_ladder) continue;

        _acftDrawScanline(tft, y, x0, x1, bx_new, gl);
        _acftPrevBx[i] = bx_new;
    }
    _acftPrevGroundLeft = newGL;
}


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

// Marker size — half-diagonal in pixels. Diamond is 2*ADI_MRK+1 pixels tip-to-tip.
static const int16_t ACFT_ADI_MRK_HD = 8;   // 17 px total diagonal

// Shortest-arc delta between two headings, result in [-180, 180].
static inline float _acftHdgDelta(float a, float b) {
    float d = a - b;
    while (d >  180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return d;
}

// Draw a single diamond marker on the ADI ball at the given world-space heading/pitch.
// Skips the draw if the computed position falls outside the ball's visible cone.
// Uses the horizontal-split two-triangle diamond pattern (matching MNVR/DOCK).
//
// Marks occupied scanlines in _acftLadderDirty (same mechanism ladder uses) so that
// the next frame's delta fill repaints those rows — prevents trails when the marker
// moves but the horizon doesn't.
static void _acftDrawAdiMarker(RA8875 &tft, float markerHdg, float markerPitch,
                               uint16_t fillCol) {
    // Delta from current vessel attitude
    float dh = _acftHdgDelta(markerHdg, state.heading);
    float dp = markerPitch - state.pitch;

    // Ball uses negated roll (matches KerbalSimpit convention)
    float cosR = _acftCos(-state.roll);
    float sinR = _acftSin(-state.roll);

    // Unrolled-frame offset: +dh degrees rightward, +dp degrees upward (so -y)
    float ux = dh * ACFT_SCALE;
    float uy = -dp * ACFT_SCALE;

    // Apply roll rotation (angle = -state.roll, so using cosR/sinR from above).
    // Marker is treated as a 2D point on the pitch/roll-rolled ball — both dh
    // and dp combine to determine its screen position after roll rotation.
    // Horizontal marker x will not match the heading tape's x when roll != 0;
    // the ball shows the combined attitude-error in its own rolled frame, while
    // the tape shows only the heading axis.
    int16_t sx = (int16_t)(ACFT_CX + ux * cosR - uy * sinR);
    int16_t sy = (int16_t)(ACFT_CY + ux * sinR + uy * cosR);

    // Clip: entire marker must fit inside ball. Use (R - HD) so outline doesn't cross rim.
    int16_t dx = sx - ACFT_CX, dy = sy - ACFT_CY;
    int32_t rInner = (int32_t)ACFT_R - ACFT_ADI_MRK_HD;
    if ((int32_t)dx*dx + (int32_t)dy*dy > rInner * rInner) return;

    // Filled diamond (horizontal split — two triangles sharing the waist row)
    tft.fillTriangle(sx - ACFT_ADI_MRK_HD, sy,
                     sx + ACFT_ADI_MRK_HD, sy,
                     sx,                   sy - ACFT_ADI_MRK_HD,
                     fillCol);
    tft.fillTriangle(sx - ACFT_ADI_MRK_HD, sy,
                     sx + ACFT_ADI_MRK_HD, sy,
                     sx,                   sy + ACFT_ADI_MRK_HD,
                     fillCol);

    // White 1-px outline for contrast against both sky and ground fills
    tft.drawLine(sx,                   sy - ACFT_ADI_MRK_HD,
                 sx + ACFT_ADI_MRK_HD, sy,                   TFT_WHITE);
    tft.drawLine(sx + ACFT_ADI_MRK_HD, sy,
                 sx,                   sy + ACFT_ADI_MRK_HD, TFT_WHITE);
    tft.drawLine(sx,                   sy + ACFT_ADI_MRK_HD,
                 sx - ACFT_ADI_MRK_HD, sy,                   TFT_WHITE);
    tft.drawLine(sx - ACFT_ADI_MRK_HD, sy,
                 sx,                   sy - ACFT_ADI_MRK_HD, TFT_WHITE);

    // Tell next frame's delta fill to repaint these scanlines. Prevents trails when
    // the marker moves without the horizon line or ladder touching the same rows.
    for (int16_t y = sy - ACFT_ADI_MRK_HD; y <= sy + ACFT_ADI_MRK_HD; y++) {
        _acftLadderDirtySet(y);
    }
}


// ── Master ball update ────────────────────────────────────────────────────────────────
static void _acftDrawBall(RA8875 &tft, bool fullRedraw) {
    _acftBuildChordTable();   // no-op after first call

    float pitch = state.pitch;
    float roll  = -state.roll;   // negate: KerbalSimpit sign convention

    float cosR    = _acftCos(roll);
    float sinR    = _acftSin(roll);
    float pitchPx = pitch * ACFT_SCALE;

    float K   = sinR * (float)ACFT_CX - cosR * (float)ACFT_CY - pitchPx;
    float BCX = (float)ACFT_CX - pitchPx * sinR;
    float BCY = (float)ACFT_CY + pitchPx * cosR;
    float hc2 = (float)ACFT_R * ACFT_R - pitchPx * pitchPx;

    uint32_t _t0, _t1;

    // ── 1. Sky/ground fill ────────────────────────────────────────────────────────────
    _t0 = micros();
    if (fullRedraw) _acftFullDraw(tft, sinR, cosR, K);
    else            _acftDeltaDraw(tft, sinR, cosR, K);
    _t1 = micros();
    Serial.print(fullRedraw ? "  fill(FULL)=" : "  fill(DELTA)=");
    Serial.print((_t1-_t0)/1000.0f, 2); Serial.print("ms");

    // ── 2. Horizon line ───────────────────────────────────────────────────────────────
    _t0 = micros();
    int16_t new_horiz_lo = INT16_MAX, new_horiz_hi = INT16_MIN;
    if (hc2 >= 0.0f) {
        float hc  = max(0.0f, sqrtf(hc2) - 3.0f);
        int16_t lx1 = (int16_t)(BCX - hc * cosR);
        int16_t ly1 = (int16_t)(BCY - hc * sinR);
        int16_t lx2 = (int16_t)(BCX + hc * cosR);
        int16_t ly2 = (int16_t)(BCY + hc * sinR);
        tft.drawLine(lx1, ly1, lx2, ly2, ACFT_HORIZON);
        auto rnd = [](float v) -> int16_t {
            return (int16_t)(v + (v > 0.0f ? 0.5f : -0.5f));
        };
        int16_t px = rnd(sinR), py = rnd(-cosR);
        tft.drawLine(lx1+px, ly1+py, lx2+px, ly2+py, ACFT_HORIZON);
        new_horiz_lo = min(min(ly1, ly2), min((int16_t)(ly1+py), (int16_t)(ly2+py)));
        new_horiz_hi = max(max(ly1, ly2), max((int16_t)(ly1+py), (int16_t)(ly2+py)));
    }
    _acftPrevHorizLo = new_horiz_lo;
    _acftPrevHorizHi = new_horiz_hi;
    _t1 = micros();
    Serial.print("  horiz="); Serial.print((_t1-_t0)/1000.0f, 2); Serial.print("ms");

    // ── 3. Pitch ladder ───────────────────────────────────────────────────────────────
    // Swap bitmaps: prev = last frame's dirty set (used by delta fill above),
    // current = cleared for this frame's ladder draw.
    _t0 = micros();
    memcpy(_acftLadderDirtyPrev, _acftLadderDirty, sizeof(_acftLadderDirty));
    memset(_acftLadderDirty, 0, sizeof(_acftLadderDirty));
    _acftDrawLadder(tft, BCX, BCY, sinR, cosR);
    _t1 = micros();
    Serial.print("  ladder="); Serial.print((_t1-_t0)/1000.0f, 2); Serial.print("ms");

    // ── 4. ADI markers — drawn on top of ball, under the aircraft reference ───────────
    //    Prograde (surface velocity) always drawn; target if available; maneuver if
    //    active. Each call self-clips to the ball's visible cone.
    _acftDrawAdiMarker(tft, state.srfVelHeading, state.srfVelPitch, TFT_NEON_GREEN);
    if (state.targetAvailable)
        _acftDrawAdiMarker(tft, state.tgtHeading, state.tgtPitch, TFT_VIOLET);
    if (state.mnvrTime > 0.0f)
        _acftDrawAdiMarker(tft, state.mnvrHeading, state.mnvrPitch, TFT_BLUE);

    // ── 5. Aircraft symbol — drawn last so it is always on top ────────────────────────
    _acftDrawAircraftSymbol(tft);

    _acftPrevPitch = state.pitch;
    _acftPrevRoll  = state.roll;

    // Ball-side marker trackers — snapshot current values so the next frame's
    // dirty check sees no change unless something actually moved.
    _acftPrevHeadingBall    = state.heading;
    _acftPrevVelHdgBall     = state.srfVelHeading;
    _acftPrevVelPitchBall   = state.srfVelPitch;
    _acftPrevMnvrHdgBall    = state.mnvrHeading;
    _acftPrevMnvrPitchBall  = state.mnvrPitch;
    _acftPrevTgtHdgBall     = state.tgtHeading;
    _acftPrevTgtPitchBall   = state.tgtPitch;
    _acftPrevMnvrActiveBall = (state.mnvrTime > 0.0f);
    _acftPrevTgtAvailBall   = state.targetAvailable;
}


// ── Roll indicator state ──────────────────────────────────────────────────────────────
static float    _acftPrevRollIndicator = -9999.0f;   // last drawn pointer angle
static int16_t  _acftPrevRollReadout   = -9999;      // last drawn roll readout (integer degrees)
static uint16_t _acftPrevRollReadoutFg = 0;          // last drawn foreground colour

// ── Pitch readout state ───────────────────────────────────────────────────────────────

// ── Roll indicator geometry ───────────────────────────────────────────────────────────
static const int16_t  ACFT_PTR_TIP_R  = ACFT_R + 3;   // tip clear of bezel (bezel outer = R+2)
static const int16_t  ACFT_PTR_BASE_R = ACFT_R + 20;  // base beyond tick outer edge (R+16)
static const int16_t  ACFT_PTR_W      = 8;            // half-width of pointer base

// Roll readout — two lines centred in fixed-width block
// Label: Roboto_Black_20 (smaller), Value: Roboto_Black_24 (larger)
static const int16_t  ACFT_ROLL_ANCHOR_X  = ACFT_CX + ACFT_R - 56; // moves with CX
static const int16_t  ACFT_ROLL_ANCHOR_Y  = ACFT_CY - ACFT_R - 24; // aligned with Pitch: label top
static const int16_t  ACFT_ROLL_W         = 80;   // block width (unchanged)
static const int16_t  ACFT_ROLL_LABEL_H   = 20;   // label line height (Roboto_Black_20)
static const int16_t  ACFT_ROLL_VALUE_H   = 26;   // value line height (Roboto_Black_24)
static const int16_t  ACFT_ROLL_GAP       = 3;    // gap between lines

// Update the roll numeric readout.
static void _acftUpdateRollReadout(RA8875 &tft, float roll) {
    int16_t  iRoll   = (int16_t)roundf(roll);
    float    absRoll = fabsf(roll);
    // Aircraft: roll warn/alarm active (unlike spacecraft)
    uint16_t fg = (absRoll > ROLL_ALARM_DEG) ? TFT_WHITE      :
                  (absRoll > ROLL_WARN_DEG)  ? TFT_YELLOW     : TFT_DARK_GREEN;
    uint16_t bg = (absRoll > ROLL_ALARM_DEG) ? TFT_RED        : TFT_BLACK;

    if (iRoll == _acftPrevRollReadout && fg == _acftPrevRollReadoutFg) return;

    // Erase previous value in black-on-black using the same font/box
    if (_acftPrevRollReadout > -9000) {
        char oldBuf[8];
        snprintf(oldBuf, sizeof(oldBuf), "%+d\xB0", _acftPrevRollReadout);
        textCenter(tft, &Roboto_Black_24,
                   ACFT_ROLL_ANCHOR_X, ACFT_ROLL_ANCHOR_Y + ACFT_ROLL_LABEL_H + ACFT_ROLL_GAP,
                   ACFT_ROLL_W, ACFT_ROLL_VALUE_H,
                   oldBuf, TFT_BLACK, TFT_BLACK);
    }

    // Line 1: "Roll:" — small font, centred in label row
    textCenter(tft, &Roboto_Black_20,
               ACFT_ROLL_ANCHOR_X, ACFT_ROLL_ANCHOR_Y,
               ACFT_ROLL_W, ACFT_ROLL_LABEL_H,
               "Roll:", TFT_WHITE, TFT_BLACK);

    // Line 2: signed value — larger font, centred in value row
    char buf[8];
    snprintf(buf, sizeof(buf), "%+d\xB0", iRoll);
    textCenter(tft, &Roboto_Black_24,
               ACFT_ROLL_ANCHOR_X, ACFT_ROLL_ANCHOR_Y + ACFT_ROLL_LABEL_H + ACFT_ROLL_GAP,
               ACFT_ROLL_W, ACFT_ROLL_VALUE_H,
               buf, fg, bg);

    _acftPrevRollReadout   = iRoll;
    _acftPrevRollReadoutFg = fg;
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
static const int16_t  ACFT_PTAPE_MRK_TIP_X  = ACFT_PTAPE_X + ACFT_PTAPE_W - 20; // 18px — matches HDG tape
static const int16_t  ACFT_PTAPE_MRK_HW     = 6;

// State
static float   _acftPrevPitch2      = -9999.0f;   // pitch tape (distinct from ball state)
static int16_t _acftPrevPitchBox    = -9999;
static float   _acftPrevVelPitch    = -9999.0f;

// Draw/update the pitch value box — cached on integer change.
static void _acftUpdatePitchBox(RA8875 &tft, float pitch) {
    int16_t iPitch = (int16_t)roundf(pitch);
    if (iPitch == _acftPrevPitchBox) return;

    char newBuf[8];
    snprintf(newBuf, sizeof(newBuf), "%+d\xB0", iPitch);

    if (_acftPrevPitchBox > -9000) {
        char oldBuf[8];
        snprintf(oldBuf, sizeof(oldBuf), "%+d\xB0", _acftPrevPitchBox);
        textCenter(tft, &Roboto_Black_24,
                   ACFT_PTAPE_BOX_X, ACFT_PTAPE_BOX_Y + 1,
                   ACFT_PTAPE_BOX_W, ACFT_PTAPE_BOX_H - 2,
                   oldBuf, TFT_BLACK, TFT_BLACK);
    }
    textCenter(tft, &Roboto_Black_24,
               ACFT_PTAPE_BOX_X, ACFT_PTAPE_BOX_Y + 1,
               ACFT_PTAPE_BOX_W, ACFT_PTAPE_BOX_H - 2,
               newBuf, TFT_DARK_GREEN, TFT_BLACK);

    _acftPrevPitchBox = iPitch;
}

// Draw the full pitch tape for the given pitch.
static void _acftDrawPitchTape(RA8875 &tft, float pitch) {
    // Clear tape in two passes, skipping the box area and staying 1px inside borders
    int16_t fillW  = ACFT_PTAPE_W - 1;  // stop 1px short of right border
    int16_t aboveH = ACFT_PTAPE_BOX_Y - ACFT_PTAPE_Y;
    int16_t belowY = ACFT_PTAPE_BOX_Y + ACFT_PTAPE_BOX_H;
    int16_t belowH = (ACFT_PTAPE_Y + ACFT_PTAPE_H - 1) - belowY;  // stop 1px short of bottom border
    tft.fillRect(ACFT_PTAPE_X, ACFT_PTAPE_Y, fillW, aboveH, TFT_BLACK);
    tft.fillRect(ACFT_PTAPE_X, belowY,       fillW, belowH, TFT_BLACK);

    // Redraw box border (sides may have been touched by above/below fills)
    tft.drawRect(ACFT_PTAPE_BOX_X, ACFT_PTAPE_BOX_Y, ACFT_PTAPE_BOX_W, ACFT_PTAPE_BOX_H, TFT_LIGHT_GREY);
    // Box interior not erased — no need to reset _acftPrevPitchBox

    tft.setFont(&Roboto_Black_12);

    // Draw ticks from pitch-32 to pitch+32 (slightly beyond ±30° visible range)
    for (int16_t dp = -32; dp <= 32; dp++) {
        float deg = pitch + (float)dp;
        if (deg < -90.0f || deg > 90.0f) continue;  // KSP pitch clamped ±90°

        // Pixel y: current pitch stays at centre (ACFT_CY), offset by dp degrees
        int16_t py = (int16_t)(ACFT_CY - (float)dp * ACFT_PTAPE_SCALE);

        // Clip to tape interior
        if (py <= ACFT_PTAPE_Y || py >= ACFT_PTAPE_Y + ACFT_PTAPE_H) continue;

        // Suppress near value box
        if (py >= ACFT_PTAPE_SUPP_LO && py <= ACFT_PTAPE_SUPP_HI) continue;

        int16_t ideg = (int16_t)roundf(deg);

        if (ideg % 10 == 0) {
            // Major tick — right-aligned, stopping 1px short of right border
            int16_t tx0 = ACFT_PTAPE_X + ACFT_PTAPE_W - 11;
            int16_t tx1 = ACFT_PTAPE_X + ACFT_PTAPE_W - 2;
            tft.drawLine(tx0, py, tx1, py, TFT_LIGHT_GREY);

            // Label — left of tick, clamped to tape
            char lbl[8];
            snprintf(lbl, sizeof(lbl), "%+d", ideg);
            int16_t lx = ACFT_PTAPE_X + 2;
            int16_t ly = py - 6;
            if (ly < ACFT_PTAPE_Y + 1) ly = ACFT_PTAPE_Y + 1;
            if (ly + 12 > ACFT_PTAPE_Y + ACFT_PTAPE_H - 3)
                ly = ACFT_PTAPE_Y + ACFT_PTAPE_H - 15;
            // Only draw if label y is not in suppress zone
            if (!(ly + 6 >= ACFT_PTAPE_SUPP_LO && ly + 6 <= ACFT_PTAPE_SUPP_HI)) {
                tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
                tft.setCursor(lx, ly);
                tft.print(lbl);
            }
        } else if (ideg % 2 == 0) {
            // Minor tick (every 2°) — stopping 1px short of right border
            int16_t tx0 = ACFT_PTAPE_X + ACFT_PTAPE_W - 7;
            int16_t tx1 = ACFT_PTAPE_X + ACFT_PTAPE_W - 2;
            tft.drawLine(tx0, py, tx1, py, TFT_DARK_GREY);
        }
    }

    // Draw pitch markers (left-pointing triangles on right edge)
    auto drawPitchMarker = [&](float markerPitch, uint16_t col) {
        float diff = markerPitch - pitch;
        int16_t py = (int16_t)(ACFT_CY - diff * ACFT_PTAPE_SCALE);
        // Peg to tape edges rather than hiding
        int16_t pyMin = ACFT_PTAPE_Y + ACFT_PTAPE_MRK_HW + 1;
        int16_t pyMax = ACFT_PTAPE_Y + ACFT_PTAPE_H - ACFT_PTAPE_MRK_HW - 2;
        if (py < pyMin) py = pyMin;
        if (py > pyMax) py = pyMax;
        if (py >= ACFT_PTAPE_SUPP_LO && py <= ACFT_PTAPE_SUPP_HI) return;
        tft.fillTriangle(ACFT_PTAPE_MRK_TIP_X,  py,
                         ACFT_PTAPE_MRK_BASE_X,  py - ACFT_PTAPE_MRK_HW,
                         ACFT_PTAPE_MRK_BASE_X,  py + ACFT_PTAPE_MRK_HW,
                         col);
    };

    // Aircraft: surface velocity pitch only — no target or maneuver markers
    drawPitchMarker(state.srfVelPitch, TFT_NEON_GREEN);
}

// Update pitch tape — redraws when pitch or velocity pitch changes.
static void _acftUpdatePitchTape(RA8875 &tft, float pitch) {
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
static const int16_t  ACFT_HDG_BOX_H     = 40;                     // TAPE_H + 8 = 40
static const int16_t  ACFT_HDG_BOX_X     = ACFT_CX - (ACFT_HDG_BOX_W / 2);
static const int16_t  ACFT_HDG_BOX_Y     = ACFT_HDG_TAPE_Y;

// Suppress zone — covers box + max label half-width (12px)
static const int16_t  ACFT_HDG_SUPP_LO   = ACFT_HDG_BOX_X - 18;
static const int16_t  ACFT_HDG_SUPP_HI   = ACFT_HDG_BOX_X + ACFT_HDG_BOX_W + 18;

// Heading markers — long thin downward triangles fully inside the tape
static const int16_t  ACFT_HDG_MRK_BASE_Y = ACFT_HDG_TAPE_Y + 2;   // 2px below tape top
static const int16_t  ACFT_HDG_MRK_TIP_Y  = ACFT_HDG_TAPE_Y + 20;  // 18px tall
static const int16_t  ACFT_HDG_MRK_HW     = 6;                     // half-width → 13px wide

// Draw/update the heading number box — cached, only redraws when integer heading changes.
// Uses textCenter for flicker-free rendering: erase old value with black-on-black first.
static void _acftUpdateHdgBox(RA8875 &tft, float hdg) {
    int16_t iHdg = (int16_t)roundf(hdg) % 360;
    if (iHdg < 0) iHdg += 360;
    if (iHdg == _acftPrevHdgBox) return;

    char oldBuf[8], newBuf[8];
    snprintf(newBuf, sizeof(newBuf), "%03d\xB0", iHdg);

    // Erase previous value with black-on-black
    if (_acftPrevHdgBox >= 0) {
        snprintf(oldBuf, sizeof(oldBuf), "%03d\xB0", _acftPrevHdgBox);
        textCenter(tft, &Roboto_Black_24,
                   ACFT_HDG_BOX_X, ACFT_HDG_BOX_Y + 1,
                   ACFT_HDG_BOX_W, ACFT_HDG_BOX_H - 2,
                   oldBuf, TFT_BLACK, TFT_BLACK);
    }

    // Draw new value
    textCenter(tft, &Roboto_Black_24,
               ACFT_HDG_BOX_X, ACFT_HDG_BOX_Y + 1,
               ACFT_HDG_BOX_W, ACFT_HDG_BOX_H - 2,
               newBuf, TFT_DARK_GREEN, TFT_BLACK);

    _acftPrevHdgBox = iHdg;
}

// Draw the full heading tape. Only the tape strip — box is handled separately.
static void _acftDrawHeadingTape(RA8875 &tft, float hdg) {
    while (hdg <   0.0f) hdg += 360.0f;
    while (hdg >= 360.0f) hdg -= 360.0f;

    tft.fillRect(ACFT_HDG_TAPE_X, ACFT_HDG_TAPE_Y, ACFT_HDG_TAPE_W, ACFT_HDG_TAPE_H, TFT_BLACK);

    // Redraw box border after fill (fill erases box sides where they overlap)
    tft.drawRect(ACFT_HDG_BOX_X, ACFT_HDG_BOX_Y, ACFT_HDG_BOX_W, ACFT_HDG_BOX_H, TFT_LIGHT_GREY);

    // Force box number to redraw — fill blackened the interior
    _acftPrevHdgBox = -1;

    tft.setFont(&Roboto_Black_12);

    for (int16_t d = -32; d <= 32; d++) {
        float deg = hdg + (float)d;
        while (deg <   0.0f) deg += 360.0f;
        while (deg >= 360.0f) deg -= 360.0f;

        int16_t px  = (int16_t)(ACFT_CX + d * ACFT_HDG_SCALE);
        // Strict clip — exclude boundary pixels to prevent residual at edges
        if (px <= ACFT_HDG_TAPE_X || px >= ACFT_HDG_TAPE_X + ACFT_HDG_TAPE_W) continue;

        // Suppress elements near the box (expanded to cover label text extents)
        if (px >= ACFT_HDG_SUPP_LO && px <= ACFT_HDG_SUPP_HI) continue;

        int16_t ideg = (int16_t)roundf(deg);
        if (ideg == 360) ideg = 0;

        if (ideg % 10 == 0) {
            tft.drawLine(px, ACFT_HDG_TAPE_Y, px, ACFT_HDG_TAPE_Y + 10, TFT_LIGHT_GREY);

            if (px >= ACFT_HDG_LABEL_LO && px <= ACFT_HDG_LABEL_HI) {
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
                if (cx < ACFT_HDG_TAPE_X + 1) cx = ACFT_HDG_TAPE_X + 1;
                if (cx + lw > ACFT_HDG_TAPE_X + ACFT_HDG_TAPE_W - 1)
                    cx = ACFT_HDG_TAPE_X + ACFT_HDG_TAPE_W - 1 - lw;
                tft.setCursor(cx, ACFT_HDG_TAPE_Y + 12);
                tft.print(lbl);
            }
        } else if (ideg % 2 == 0) {
            tft.drawLine(px, ACFT_HDG_TAPE_Y, px, ACFT_HDG_TAPE_Y + 6, TFT_DARK_GREY);
        }
    }

    // Draw heading markers after ticks so they render on top
    auto drawMarker = [&](float markerHdg, uint16_t col) {
        // Find angular offset with wrap
        float diff = markerHdg - hdg;
        while (diff >  180.0f) diff -= 360.0f;
        while (diff < -180.0f) diff += 360.0f;
        int16_t px = (int16_t)(ACFT_CX + diff * ACFT_HDG_SCALE);
        // Peg to tape edges (leave room for half-width) rather than hiding
        int16_t pxMin = ACFT_HDG_TAPE_X + ACFT_HDG_MRK_HW + 1;
        int16_t pxMax = ACFT_HDG_TAPE_X + ACFT_HDG_TAPE_W - ACFT_HDG_MRK_HW - 1;
        if (px < pxMin) px = pxMin;
        if (px > pxMax) px = pxMax;
        // Skip if in suppress zone
        if (px >= ACFT_HDG_SUPP_LO && px <= ACFT_HDG_SUPP_HI) return;
        tft.fillTriangle(px,                    ACFT_HDG_MRK_TIP_Y,
                         px - ACFT_HDG_MRK_HW,  ACFT_HDG_MRK_BASE_Y,
                         px + ACFT_HDG_MRK_HW,  ACFT_HDG_MRK_BASE_Y,
                         col);
    };

    // Aircraft: surface velocity heading only — no target or maneuver markers
    drawMarker(state.srfVelHeading, TFT_NEON_GREEN);
}

// Update heading — tape redraws when heading or velocity heading changes.
static void _acftUpdateHeadingTape(RA8875 &tft, float hdg) {
    bool dirty = fabsf(hdg - _acftPrevHeading)                     >= 0.5f
              || fabsf(state.srfVelHeading - _acftPrevVelHdg)      >= 0.5f;

    if (dirty) {
        _acftDrawHeadingTape(tft, hdg);
        _acftPrevHeading = hdg;
        _acftPrevVelHdg  = state.srfVelHeading;
    }
    _acftUpdateHdgBox(tft, hdg);
}


// Draw the roll pointer triangle for a given roll angle.
static void _acftDrawRollPointer(RA8875 &tft, float roll, uint16_t colour) {
    float a    = (roll - 90.0f) * (float)DEG_TO_RAD;
    float cosA = cosf(a), sinA = sinf(a);
    int16_t tx  = (int16_t)(ACFT_CX + ACFT_PTR_TIP_R  * cosA);
    int16_t ty  = (int16_t)(ACFT_CY + ACFT_PTR_TIP_R  * sinA);
    int16_t bcx = (int16_t)(ACFT_CX + ACFT_PTR_BASE_R * cosA);
    int16_t bcy = (int16_t)(ACFT_CY + ACFT_PTR_BASE_R * sinA);
    int16_t b1x = bcx + (int16_t)(-sinA * ACFT_PTR_W);
    int16_t b1y = bcy + (int16_t)( cosA * ACFT_PTR_W);
    int16_t b2x = bcx - (int16_t)(-sinA * ACFT_PTR_W);
    int16_t b2y = bcy - (int16_t)( cosA * ACFT_PTR_W);
    tft.fillTriangle(tx, ty, b1x, b1y, b2x, b2y, colour);
}

// Erase the roll pointer — uses a 2px expanded triangle to catch any stray pixels
// left by integer rounding in the draw pass.
static void _acftEraseRollPointer(RA8875 &tft, float roll) {
    float a    = (roll - 90.0f) * (float)DEG_TO_RAD;
    float cosA = cosf(a), sinA = sinf(a);
    // Expand tip inward by 1px along radial, base outward by 1px, width +2px
    int16_t tx  = (int16_t)(ACFT_CX + (ACFT_PTR_TIP_R  - 1) * cosA);
    int16_t ty  = (int16_t)(ACFT_CY + (ACFT_PTR_TIP_R  - 1) * sinA);
    int16_t bcx = (int16_t)(ACFT_CX + (ACFT_PTR_BASE_R + 1) * cosA);
    int16_t bcy = (int16_t)(ACFT_CY + (ACFT_PTR_BASE_R + 1) * sinA);
    int16_t b1x = bcx + (int16_t)(-sinA * (ACFT_PTR_W + 2));
    int16_t b1y = bcy + (int16_t)( cosA * (ACFT_PTR_W + 2));
    int16_t b2x = bcx - (int16_t)(-sinA * (ACFT_PTR_W + 2));
    int16_t b2y = bcy - (int16_t)( cosA * (ACFT_PTR_W + 2));
    tft.fillTriangle(tx, ty, b1x, b1y, b2x, b2y, TFT_BLACK);
}

// Draw a single bank scale tick at the given bank angle.
static void _acftDrawBankTick(RA8875 &tft, int16_t bankDeg) {
    bool isMajor = (bankDeg == 0 || bankDeg == 30 || bankDeg == -30 ||
                                     bankDeg == 60 || bankDeg == -60);
    int16_t tOuter = ACFT_R + 16;
    int16_t tInner = ACFT_R + (isMajor ? 2 : 6);
    // 0° and ±60° white, all others light grey
    uint16_t col = (bankDeg == 0 || bankDeg == 60 || bankDeg == -60)
                   ? TFT_WHITE : TFT_LIGHT_GREY;
    float a    = (bankDeg - 90.0f) * (float)DEG_TO_RAD;
    float cosA = cosf(a), sinA = sinf(a);
    int16_t ox = (int16_t)(ACFT_CX + tOuter * cosA);
    int16_t oy = (int16_t)(ACFT_CY + tOuter * sinA);
    int16_t ix = (int16_t)(ACFT_CX + tInner * cosA);
    int16_t iy = (int16_t)(ACFT_CY + tInner * sinA);
    tft.drawLine(ox, oy, ix, iy, col);
}

// Update the roll pointer: erase old, redraw any tick it covered, draw new.
static void _acftUpdateRollIndicator(RA8875 &tft, float roll) {
    if (fabsf(roll - _acftPrevRollIndicator) < 0.2f) return;

    static const int16_t ticks[] = {-60,-45,-30,-20,-10,0,10,20,30,45,60};

    // Erase old pointer with expanded triangle to catch stray pixels
    if (_acftPrevRollIndicator > -9000.0f) {
        float prevClamped = _acftPrevRollIndicator;
        if      (prevClamped >  60.0f) prevClamped =  60.0f;
        else if (prevClamped < -60.0f) prevClamped = -60.0f;
        _acftEraseRollPointer(tft, prevClamped);
        // Redraw any tick the erase region may have covered (within 4° — slightly
        // wider than before to account for the expanded erase)
        for (uint8_t i = 0; i < 11; i++) {
            if (fabsf(_acftPrevRollIndicator - ticks[i]) < 4.0f) {
                _acftDrawBankTick(tft, ticks[i]);
                break;
            }
        }
    }

    // Draw new pointer — clamped to ±60° so it stays within the scale marks
    float clampedRoll = roll;
    if      (clampedRoll >  60.0f) clampedRoll =  60.0f;
    else if (clampedRoll < -60.0f) clampedRoll = -60.0f;
    _acftDrawRollPointer(tft, clampedRoll, TFT_YELLOW);
    _acftPrevRollIndicator = roll;
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

static void _acftDrawVSIChrome(RA8875 &tft) {
    tft.drawLine(VSI_TICK_X0 + 1, TITLE_TOP, VSI_TICK_X0 + 1, SCREEN_H - 1, TFT_GREY);
    tft.drawLine(VSI_X, VSI_ZERO_Y,     VSI_TICK_X0 + 10, VSI_ZERO_Y,     TFT_LIGHT_GREY);
    tft.drawLine(VSI_X, VSI_ZERO_Y + 1, VSI_TICK_X0 + 10, VSI_ZERO_Y + 1, TFT_LIGHT_GREY);
    tft.setFont(&Roboto_Black_12);
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

static void _acftUpdateVSI(RA8875 &tft, float vVrt) {
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

// Slip ball — strip below heading tape, same x/width as heading tape
// Starts below heading BOX bottom (box extends 8px below tape) to avoid overlap with hdg number
static const int16_t SLIP_X      = ACFT_HDG_TAPE_X;
static const int16_t SLIP_W      = ACFT_HDG_TAPE_W;
static const int16_t SLIP_Y      = ACFT_HDG_TAPE_Y + ACFT_HDG_BOX_H;    // 1px higher than before
static const int16_t SLIP_H      = SCREEN_H - SLIP_Y;
static const int16_t SLIP_BALL_R = SLIP_H / 2 - 2;
static const int16_t SLIP_CY     = SLIP_Y + SLIP_H / 2;
static const float   SLIP_SCALE  = (float)(SLIP_W / 2 - SLIP_BALL_R - 4) / 20.0f;
static int16_t _acftPrevSlipX    = 9999;

static void _acftDrawSlipChrome(RA8875 &tft) {
    tft.drawRect(SLIP_X, SLIP_Y, SLIP_W, SLIP_H, TFT_GREY);

    // ±5° reference ticks — drawn on border only (top+bottom 2px) to stay clear of ball
    int16_t m5 = (int16_t)(5.0f * SLIP_SCALE);
    tft.drawLine(ACFT_CX - m5, SLIP_Y,          ACFT_CX - m5, SLIP_Y + 2,          TFT_NEON_GREEN);
    tft.drawLine(ACFT_CX + m5, SLIP_Y,          ACFT_CX + m5, SLIP_Y + 2,          TFT_NEON_GREEN);
    tft.drawLine(ACFT_CX - m5, SLIP_Y+SLIP_H-3, ACFT_CX - m5, SLIP_Y+SLIP_H-1,    TFT_NEON_GREEN);
    tft.drawLine(ACFT_CX + m5, SLIP_Y+SLIP_H-3, ACFT_CX + m5, SLIP_Y+SLIP_H-1,    TFT_NEON_GREEN);

    // "SLIP" label — centred in the space left of the tube, matching Pitch/Hdg label style
    textRight(tft, &Roboto_Black_20,
               VSI_TICK_X0 + 2, SLIP_Y, SLIP_X - VSI_TICK_X0 - 2, SLIP_H,
               "Slip:", TFT_WHITE, TFT_BLACK);
}

static void _acftUpdateSlipBall(RA8875 &tft, float slip) {
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
// Positive AoA (nose above velocity) = arc toward 11 o'clock (decreasing angle).
// Arc spans AOA_ANG_LO..AOA_ANG_HI in screen-angle degrees.
// Safe range: stays clear of bank=±60 roll ticks (which live at 210° and 330°).
// Drawn as solid colour bands using fillTriangle pairs — no float drift, no gaps.
static const int16_t  AOA_R_INNER  = ACFT_R + 4;    // 154px — just outside bezel
static const int16_t  AOA_R_OUTER  = ACFT_R + 18;   // 168px
static const float    AOA_ZERO_DEG = 180.0f;         // 9 o'clock
static const int16_t  AOA_ANG_LO   = 155;            // low screen angle (= AoA +25°)
static const int16_t  AOA_ANG_HI   = 205;            // high screen angle (= AoA -25°)
// AoA thresholds in arc-angle space (1:1 scale: 1° AoA = 1° arc)
// Positive AoA → arc angle decreases from 180°
// warn zone:  arc 170°..160°  (AoA 10°..20°)
// alarm zone: arc 160°..155°  (AoA 20°..25°)
// same zones mirrored below zero

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
static void _aoaFillSegment(RA8875 &tft, int16_t degLo, int16_t degHi, uint16_t col) {
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
static void _acftDrawAoAChrome(RA8875 &tft) {
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
}

// Pointer state — tracked in integer degrees to avoid float drift
static int16_t _acftPrevAoADeg = INT16_MIN;

// Restore background colour at a ±1° band around arcDeg
static void _aoaErasePointer(RA8875 &tft, int16_t arcDeg) {
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
static void _aoaDrawPointer(RA8875 &tft, int16_t arcDeg) {
    int16_t lo = max((int16_t)AOA_ANG_LO,       arcDeg);
    int16_t hi = min((int16_t)(AOA_ANG_HI - 1), (int16_t)(arcDeg + 1));
    _aoaFillSegment(tft, lo, hi, TFT_WHITE);
}

static void _acftUpdateAoAArc(RA8875 &tft, float aoa) {
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

static void chromeScreen_ACFT(RA8875 &tft) {
    _acftBuildChordTable();
    _acftFullRedrawNeeded      = true;
    _acftPrevHorizLo           = INT16_MAX;
    _acftPrevHorizHi           = INT16_MIN;
    _acftPrevRollIndicator     = -9999.0f;
    _acftPrevRollReadout       = -9999;
    _acftPrevRollReadoutFg     = 0;
    _acftPrevPitch2            = -9999.0f;
    _acftPrevPitchBox          = -9999;
    _acftPrevVelPitch          = -9999.0f;
    _acftPrevHeading           = -9999.0f;
    _acftPrevHdgBox            = -9999;
    _acftPrevVelHdg            = -9999.0f;
    // Panel values reset via rowCache invalidation (row cache cleared on screen switch)
    memset(_acftLadderDirty,     0, sizeof(_acftLadderDirty));
    memset(_acftLadderDirtyPrev, 0, sizeof(_acftLadderDirtyPrev));
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
    for (uint8_t i = 0; i < 11; i++) _acftDrawBankTick(tft, ticks[i]);

    // Labels at ±30° and ±60° — drawn at R+28 along the tick radial
    tft.setFont(&Roboto_Black_12);
    tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
    static const int16_t labelTicks[] = {-60, -30, 30, 60};
    static const char *  labelText[]  = {"60", "30", "30", "60"};
    static const uint8_t FONT_W = 8, FONT_H = 14;
    for (uint8_t i = 0; i < 4; i++) {
        int16_t bank = labelTicks[i];
        float   a    = (bank - 90.0f) * (float)DEG_TO_RAD;
        int16_t lc_x = (int16_t)(ACFT_CX + (ACFT_R + 28) * cosf(a));
        int16_t lc_y = (int16_t)(ACFT_CY + (ACFT_R + 28) * sinf(a));
        uint8_t lw   = strlen(labelText[i]) * FONT_W;
        tft.setCursor(lc_x - lw / 2, lc_y - FONT_H / 2);
        tft.print(labelText[i]);
    }

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

    // "Pitch:" right-justified to right edge of pitch value box
    textRight(tft, &Roboto_Black_20,
              ACFT_PTAPE_BOX_X, ACFT_PTAPE_Y - 24,
              ACFT_PTAPE_BOX_W, 24,
              "Pitch:", TFT_WHITE, TFT_BLACK);

    // Heading tape border — top (light grey) and two sides (darker), open at bottom
    tft.drawLine(ACFT_HDG_TAPE_X - 1, ACFT_HDG_TAPE_Y - 1,
                 ACFT_HDG_TAPE_X + ACFT_HDG_TAPE_W, ACFT_HDG_TAPE_Y - 1, TFT_LIGHT_GREY);
    tft.drawLine(ACFT_HDG_TAPE_X - 1, ACFT_HDG_TAPE_Y - 1,
                 ACFT_HDG_TAPE_X - 1, ACFT_HDG_TAPE_Y + ACFT_HDG_TAPE_H, TFT_GREY);
    tft.drawLine(ACFT_HDG_TAPE_X + ACFT_HDG_TAPE_W, ACFT_HDG_TAPE_Y - 1,
                 ACFT_HDG_TAPE_X + ACFT_HDG_TAPE_W, ACFT_HDG_TAPE_Y + ACFT_HDG_TAPE_H, TFT_GREY);

    // "Hdg:" right-justified to same right edge as "Pitch:" label
    textRight(tft, &Roboto_Black_20,
              ACFT_PTAPE_BOX_X, ACFT_HDG_BOX_Y + ACFT_HDG_BOX_H / 2 - 10,
              ACFT_PTAPE_BOX_W, 24,
              "Hdg:", TFT_WHITE, TFT_BLACK);

    // ── VSI tape chrome ───────────────────────────────────────────────────────────────
    _acftDrawVSIChrome(tft);

    // ── Slip ball chrome ──────────────────────────────────────────────────────────────
    _acftDrawSlipChrome(tft);

    // ── AoA arc chrome ────────────────────────────────────────────────────────────────
    _acftDrawAoAChrome(tft);

    // ── Right panel chrome ─────────────────────────────────────────────────────────────
    // Vertical divider (2px) between ADI and panel
    tft.drawLine(ACFT_PANEL_X - 2, TITLE_TOP, ACFT_PANEL_X - 2, 479, TFT_GREY);
    tft.drawLine(ACFT_PANEL_X - 1, TITLE_TOP, ACFT_PANEL_X - 1, 479, TFT_GREY);

    static const tFont *PF = &Roboto_Black_20;

    // Rows 0-3: single-width labels
    printDispChrome(tft, PF, ACFT_PANEL_X, rowYFor(0, ACFT_PANEL_NR), ACFT_PANEL_W, rowHFor(ACFT_PANEL_NR), "Alt.Rdr:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, PF, ACFT_PANEL_X, rowYFor(1, ACFT_PANEL_NR), ACFT_PANEL_W, rowHFor(ACFT_PANEL_NR), "V.Srf:",   COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, PF, ACFT_PANEL_X, rowYFor(2, ACFT_PANEL_NR), ACFT_PANEL_W, rowHFor(ACFT_PANEL_NR), "IAS:",     COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, PF, ACFT_PANEL_X, rowYFor(3, ACFT_PANEL_NR), ACFT_PANEL_W, rowHFor(ACFT_PANEL_NR), "V.Vrt:",   COL_LABEL, COL_BACK, COL_NO_BDR);

    // Row 4: Ma | G split
    {
        uint16_t hw = ACFT_PANEL_W / 2;
        uint16_t y  = rowYFor(4, ACFT_PANEL_NR), h = rowHFor(ACFT_PANEL_NR);
        printDispChrome(tft, PF, ACFT_PANEL_X,      y, hw, h, "Ma:", COL_LABEL, COL_BACK, COL_NO_BDR);
        printDispChrome(tft, PF, ACFT_PANEL_X + hw, y, hw, h, "G:",  COL_LABEL, COL_BACK, COL_NO_BDR);
        tft.drawLine(ACFT_PANEL_X + hw,     y, ACFT_PANEL_X + hw,     y + h - 1, TFT_GREY);
        tft.drawLine(ACFT_PANEL_X + hw + 1, y, ACFT_PANEL_X + hw + 1, y + h - 1, TFT_GREY);
    }

    // Row 5: AoA | Slip split
    {
        uint16_t hw = ACFT_PANEL_W / 2;
        uint16_t y  = rowYFor(5, ACFT_PANEL_NR), h = rowHFor(ACFT_PANEL_NR);
        printDispChrome(tft, PF, ACFT_PANEL_X,      y, hw, h, "AoA:",  COL_LABEL, COL_BACK, COL_NO_BDR);
        printDispChrome(tft, PF, ACFT_PANEL_X + hw, y, hw, h, "Sl:", COL_LABEL, COL_BACK, COL_NO_BDR);
        tft.drawLine(ACFT_PANEL_X + hw,     y, ACFT_PANEL_X + hw,     y + h - 1, TFT_GREY);
        tft.drawLine(ACFT_PANEL_X + hw + 1, y, ACFT_PANEL_X + hw + 1, y + h - 1, TFT_GREY);
    }

    // Row 6: Gear | Airbrk — buttons draw own labels, just draw split divider
    {
        uint16_t hw = ACFT_PANEL_W / 2;
        uint16_t y6 = rowYFor(6, ACFT_PANEL_NR), y7 = rowYFor(7, ACFT_PANEL_NR);
        tft.drawLine(ACFT_PANEL_X + hw,     y6, ACFT_PANEL_X + hw,     y7 - 1, TFT_GREY);
        tft.drawLine(ACFT_PANEL_X + hw + 1, y6, ACFT_PANEL_X + hw + 1, y7 - 1, TFT_GREY);
    }

    // Row 7: Brakes | SAS split divider (buttons drawn in update)
    {
        uint16_t hw = ACFT_PANEL_W / 2;
        tft.drawLine(ACFT_PANEL_X + hw,     TITLE_TOP + 7 * rowHFor(ACFT_PANEL_NR),
                     ACFT_PANEL_X + hw,     479, TFT_GREY);
        tft.drawLine(ACFT_PANEL_X + hw + 1, TITLE_TOP + 7 * rowHFor(ACFT_PANEL_NR),
                     ACFT_PANEL_X + hw + 1, 479, TFT_GREY);
    }
}



// ── Right panel update ─────────────────────────────────────────────────────────────────
// Row order: Alt.Rdr(0), V.Srf(1), IAS(2), V.Vrt(3), Ma|G(4), AoA|Slip(5), Gear|Airbrk(6), Brakes|SAS(7)
// Cache slots: 0=Alt.Rdr, 1=V.Srf, 2=IAS, 3=V.Vrt, 4=Ma, 5=G, 6=AoA, 7=Slip,
//              8=Gear, 9=Airbrk, 10=Brakes, 11=SAS
static void _acftUpdatePanel(RA8875 &tft) {
    static const tFont  *VF = &Roboto_Black_24;
    static const uint8_t SC = (uint8_t)screen_ACFT;
    uint16_t fw = ACFT_PANEL_W;
    uint16_t hw = ACFT_PANEL_W / 2;
    uint16_t fg, bg;
    char buf[20];

    auto acftVal = [&](uint8_t row, uint8_t slot, const char *label,
                       const String &val, uint16_t fgc, uint16_t bgc) {
        uint16_t drawFg = (val == "---") ? TFT_DARK_GREY : fgc;
        printValue(tft, VF,
                   ACFT_PANEL_X, rowYFor(row, ACFT_PANEL_NR),
                   fw, rowHFor(ACFT_PANEL_NR),
                   label, val, drawFg, bgc, COL_BACK, printState[SC][slot]);
        RowCache &rc = rowCache[SC][slot];
        rc.value = val; rc.fg = drawFg; rc.bg = bgc;
    };

    // Row 0 — Alt.Rdr
    {
        fg = (state.radarAlt < ALT_RDR_ALARM_M) ? TFT_WHITE  :
             (state.radarAlt < ALT_RDR_WARN_M)  ? TFT_YELLOW : TFT_DARK_GREEN;
        bg = (state.radarAlt < ALT_RDR_ALARM_M) ? TFT_RED    : TFT_BLACK;
        acftVal(0, 0, "Alt.Rdr:", formatAlt(state.radarAlt), fg, bg);
    }

    // Row 1 — V.Srf
    {
        fg = (state.surfaceVel < 0.0f) ? TFT_RED : TFT_DARK_GREEN;
        acftVal(1, 1, "V.Srf:", fmtMs(state.surfaceVel), fg, TFT_BLACK);
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
                           "Sl:", sv, fg, bg, COL_BACK, printState[SC][7]);
                sc2.value = sv; sc2.fg = fg; sc2.bg = bg;
            }
        }
    }

    // Row 6 — Gear | Airbrk split buttons
    {
        uint16_t y6 = rowYFor(6, ACFT_PANEL_NR), h6 = rowHFor(ACFT_PANEL_NR);
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
                drawButton(tft, ACFT_PANEL_X - 2, y6, hw + 2, h6, btn, &Roboto_Black_20, false);
                gc.value = gs; gc.fg = gfg; gc.bg = gbg;
            }
        }
        // Airbrake (slot 9) — custom action group, placeholder
        {
            // TODO: wire to action group when defined
            RowCache &ac = rowCache[SC][9]; String as = "---";
            if (ac.value != as) {
                ButtonLabel btn = { "AIRBRK", TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK, TFT_OFF_BLACK, TFT_GREY, TFT_GREY };
                drawButton(tft, ACFT_PANEL_X + hw, y6, hw, h6, btn, &Roboto_Black_20, false);
                ac.value = as;
            }
        }
    }

    // Row 7 — Brakes | SAS buttons
    {
        uint16_t ry  = TITLE_TOP + 7 * rowHFor(ACFT_PANEL_NR);
        uint16_t rh  = 480 - ry;
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
                drawButton(tft, ACFT_PANEL_X - 2, ry, hw + 2, rh, btn, &Roboto_Black_20, false);
                bc.value = bs; bc.fg = bfg; bc.bg = bbg;
            }
        }

        // SAS (slot 11) — SCFT generic colour scheme
        {
            const char *v; uint16_t sfg, sbg;
            switch (state.sasMode) {
                case 255: v = "SAS";  sfg = TFT_DARK_GREY;  sbg = TFT_OFF_BLACK;  break;
                case 0:   v = "STAB"; sfg = TFT_WHITE;       sbg = TFT_DARK_GREEN; break;
                case 1:   v = "PRO";  sfg = TFT_DARK_GREY;   sbg = TFT_NEON_GREEN; break;
                case 2:   v = "RETR"; sfg = TFT_DARK_GREY;   sbg = TFT_NEON_GREEN; break;
                case 3:   v = "NRM";  sfg = TFT_WHITE;       sbg = TFT_MAGENTA;    break;
                case 4:   v = "ANRM"; sfg = TFT_WHITE;       sbg = TFT_MAGENTA;    break;
                case 5:   v = "RAD+"; sfg = TFT_DARK_GREY;   sbg = TFT_SKY;        break;
                case 6:   v = "RAD-"; sfg = TFT_DARK_GREY;   sbg = TFT_SKY;        break;
                case 7:   v = "TGT";  sfg = TFT_WHITE;       sbg = TFT_VIOLET;     break;
                case 8:   v = "ATGT"; sfg = TFT_WHITE;       sbg = TFT_VIOLET;     break;
                case 9:   v = "MNVR"; sfg = TFT_WHITE;       sbg = TFT_BLUE;       break;
                default:  v = "SAS";  sfg = TFT_DARK_GREY;   sbg = TFT_OFF_BLACK;  break;
            }
            RowCache &rc = rowCache[SC][11];
            if (rc.value != v || rc.fg != sfg || rc.bg != sbg) {
                ButtonLabel btn = { v, sfg, sfg, sbg, sbg, TFT_GREY, TFT_GREY };
                drawButton(tft, sasX, ry, sasW, rh, btn, &Roboto_Black_20, false);
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



static void drawScreen_ACFT(RA8875 &tft) {
    uint32_t _t0 = micros();

    bool mnvrActive = (state.mnvrTime > 0.0f);
    bool tgtAvail   = state.targetAvailable;

    // Attitude dirty — pitch/roll as before, plus heading (which now affects ball
    // because ADI markers project relative to it).
    bool ballDirty = _acftFullRedrawNeeded                                         ||
                     fabsf(state.pitch - _acftPrevPitch)                   >= 0.2f ||
                     fabsf(state.roll  - _acftPrevRoll)                    >= 0.2f ||
                     fabsf(_acftHdgDelta(state.heading, _acftPrevHeadingBall)) >= 0.5f;

    // Marker dirty — any visible marker's world position changed, or a marker's
    // availability toggled (target acquired/lost, maneuver set/cleared).
    if (!ballDirty) {
        ballDirty = fabsf(_acftHdgDelta(state.srfVelHeading, _acftPrevVelHdgBall)) >= 0.5f ||
                    fabsf(state.srfVelPitch - _acftPrevVelPitchBall)               >= 0.5f ||
                    mnvrActive != _acftPrevMnvrActiveBall                                  ||
                    tgtAvail   != _acftPrevTgtAvailBall;
    }
    if (!ballDirty && mnvrActive) {
        ballDirty = fabsf(_acftHdgDelta(state.mnvrHeading, _acftPrevMnvrHdgBall)) >= 0.5f ||
                    fabsf(state.mnvrPitch - _acftPrevMnvrPitchBall)               >= 0.5f;
    }
    if (!ballDirty && tgtAvail) {
        ballDirty = fabsf(_acftHdgDelta(state.tgtHeading, _acftPrevTgtHdgBall)) >= 0.5f ||
                    fabsf(state.tgtPitch - _acftPrevTgtPitchBall)               >= 0.5f;
    }

    if (ballDirty) {
        bool full = _acftFullRedrawNeeded;
        uint32_t t0 = micros();
        _acftDrawBall(tft, full);
        _acftFullRedrawNeeded = false;
        uint32_t dt = micros() - t0;
        Serial.print(full ? "ACFT_FULL total=" : "ACFT_DELTA total=");
        Serial.print((float)dt / 1000.0f, 2);
        Serial.print("ms  pitch="); Serial.print(state.pitch, 1);
        Serial.print("  roll=");    Serial.println(state.roll, 1);
    }

    // Compute AoA and slip — suppress below 0.5 m/s
    float aoa = 0.0f, slip = 0.0f;
    if (state.surfaceVel > 0.5f) {
        aoa  = state.pitch - state.srfVelPitch;
        slip = state.heading - state.srfVelHeading;
        while (slip >  180.0f) slip -= 360.0f;
        while (slip < -180.0f) slip += 360.0f;
    }

    _acftUpdateRollIndicator(tft, state.roll);
    _acftUpdateRollReadout(tft, state.roll);
    _acftUpdatePitchTape(tft, state.pitch);
    _acftUpdateHeadingTape(tft, state.heading);
    _acftUpdateVSI(tft, state.verticalVel);
    _acftUpdateSlipBall(tft, slip);
    _acftUpdateAoAArc(tft, aoa);
    _acftUpdatePanel(tft);

    uint32_t _dt = micros() - _t0;
    Serial.print("ACFT frame=");
    Serial.print((float)_dt / 1000.0f, 2);
    Serial.print("ms  hdg=");  Serial.print(state.heading, 1);
    Serial.print("  vvrt=");   Serial.print(state.verticalVel, 1);
    Serial.print("  slip=");   Serial.print(slip, 2);
    Serial.print("  aoa=");    Serial.println(aoa, 2);
}
