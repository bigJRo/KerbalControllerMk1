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
static const int16_t  SCFT_CX        = 260;
static const int16_t  SCFT_CY        = 260;
static const int16_t  SCFT_R         = 164;
static const float    SCFT_SCALE     = (float)SCFT_R / 30.0f;   // 5.467 px/deg

static const int16_t  SCFT_BALL_Y0   = SCFT_CY - SCFT_R;         // 107
static const int16_t  SCFT_BALL_Y1   = SCFT_CY + SCFT_R;         // 435
static const uint16_t SCFT_SCANLINES = (uint16_t)(SCFT_R * 2 + 1); // 329

// ── Right panel geometry ───────────────────────────────────────────────────────────────
static const int16_t  SCFT_PANEL_X       = SCFT_CX - (SCFT_R*2+54)/2 + (SCFT_R*2+54) + 2; // 453 — flush with HDG tape right edge
static const int16_t  SCFT_PANEL_RIGHT   = 720;
static const int16_t  SCFT_PANEL_W       = SCFT_PANEL_RIGHT - SCFT_PANEL_X;
static const uint8_t  SCFT_PANEL_NR      = 8;

// ── Right panel state ──────────────────────────────────────────────────────────────────
// Cache managed by rowCache[screen_SCFT] / printState[screen_SCFT] slots 0-8.
// No additional state variables needed — printValue handles dirty detection.

// Colours
static const uint16_t SCFT_SKY      = TFT_ROYAL;
static const uint16_t SCFT_GND      = TFT_UPS_BROWN;
static const uint16_t SCFT_HORIZON  = TFT_WHITE;
static const uint16_t SCFT_WINGS    = TFT_YELLOW;
static const uint16_t SCFT_LADDER   = TFT_WHITE;

static const int16_t  SCFT_BX_ALLSKY = INT16_MIN;
static const int16_t  SCFT_BX_ALLGND = INT16_MAX;


// ── Per-frame state ───────────────────────────────────────────────────────────────────
static int16_t  _scftPrevBx[SCFT_SCANLINES];
static bool     _scftPrevGroundLeft;

static float    _scftPrevPitch   = -9999.0f;
static float    _scftPrevRoll    = -9999.0f;
bool            _scftPrevOrbMode = false;

// Active velocity vector — set each frame by drawScreen_SCFT based on orbMode.
// Both tapes and the panel V.Orb/V.Srf row read from these.
static float    _scftVelHdg   = 0.0f;
static float    _scftVelPitch = 0.0f;
static bool     _scftFullRedrawNeeded = true;

// Horizon line dirty band (small — just 1-2px tall after clipping)
static int16_t  _scftPrevHorizLo  = INT16_MAX;
static int16_t  _scftPrevHorizHi  = INT16_MIN;

// Per-scanline ladder dirty bitmap — 1 bit per scanline (329 bits = 42 bytes).
// Set during ladder draw, checked during delta fill. O(1) lookup per row.
static const uint16_t SCFT_DIRTY_WORDS = (SCFT_SCANLINES + 15) / 16;
static uint16_t _scftLadderDirty[SCFT_DIRTY_WORDS];   // current frame
static uint16_t _scftLadderDirtyPrev[SCFT_DIRTY_WORDS]; // previous frame

static inline void _scftLadderDirtySet(int16_t y) {
    int16_t i = y - SCFT_BALL_Y0;
    if (i < 0 || i >= (int16_t)SCFT_SCANLINES) return;
    _scftLadderDirty[i >> 4] |= (uint16_t)(1u << (i & 15));
}
static inline bool _scftLadderDirtyTest(uint16_t i) {
    return (_scftLadderDirtyPrev[i >> 4] >> (i & 15)) & 1u;
}

// Chord half-width lookup table — precomputed once, saves 4-iter sqrt per scanline.
static int16_t _scftChordTable[SCFT_SCANLINES];
static bool    _scftChordTableReady = false;

static void _scftBuildChordTable() {
    if (_scftChordTableReady) return;
    for (uint16_t i = 0; i < SCFT_SCANLINES; i++) {
        int16_t dy  = (int16_t)i - SCFT_R;   // dy = y - SCFT_CY
        int32_t rem = (int32_t)SCFT_R*SCFT_R - (int32_t)dy*dy;
        if (rem <= 0) { _scftChordTable[i] = 0; continue; }
        int32_t x = SCFT_R;
        x = (x + rem/x) >> 1; x = (x + rem/x) >> 1;
        x = (x + rem/x) >> 1; x = (x + rem/x) >> 1;
        _scftChordTable[i] = (int16_t)x;
    }
    _scftChordTableReady = true;
}


// ── Orbital mode helper ───────────────────────────────────────────────────────────────
static bool _scftOrbMode() {
    float bodyRad   = (currentBody.radius > 0.0f) ? currentBody.radius : 600000.0f;
    bool  ascending = (state.verticalVel >= 0.0f);
    float switchAlt = ascending ? (bodyRad * 0.06f) : (bodyRad * 0.055f);
    return state.altitude > switchAlt;
}

static inline float _scftSin(float deg) { return sinf(deg * DEG_TO_RAD); }
static inline float _scftCos(float deg) { return cosf(deg * DEG_TO_RAD); }




// ── Draw one scanline ─────────────────────────────────────────────────────────────────
static void _scftDrawScanline(RA8875 &tft,
                              int16_t y, int16_t x0, int16_t x1,
                              int16_t bx, bool groundLeft) {
    if (bx == SCFT_BX_ALLSKY) {
        tft.drawLine(x0, y, x1, y, SCFT_SKY);
    } else if (bx == SCFT_BX_ALLGND) {
        tft.drawLine(x0, y, x1, y, SCFT_GND);
    } else if (groundLeft) {
        tft.drawLine(x0,   y, bx,   y, SCFT_GND);
        tft.drawLine(bx+1, y, x1,   y, SCFT_SKY);
    } else {
        tft.drawLine(x0,   y, bx-1, y, SCFT_SKY);
        tft.drawLine(bx,   y, x1,   y, SCFT_GND);
    }
}


// ── Aircraft symbol (fixed, horizontal) ──────────────────────────────────────────────
// Centre dot: radius 7 (15px total diameter).
// Wings: inner gap WI=14 (7px clear of dot edge), outer WO=50, height WH=3 (7px total).
// Fin: 7px wide, 20px tall, starting 7px below dot edge (gap=7px).
// Drawn as the very last element so it is always on top of fill, horizon, and ladder.
static void _scftDrawAircraftSymbol(RA8875 &tft) {
    static const int16_t DOT_R  = 7;    // dot radius → 15px diameter
    static const int16_t WI     = 14;   // wing inner edge (DOT_R + 7px gap)
    static const int16_t WO     = 50;   // wing outer edge
    static const int16_t WH     = 2;    // wing half-height → 5px total
    static const int16_t FIN_GAP = 7;   // gap between dot bottom and fin top
    static const int16_t FIN_H  = 20;   // fin height
    static const int16_t FIN_W  = 2;    // fin half-width → 5px total

    // Left wing
    tft.fillRect(SCFT_CX - WO,    SCFT_CY - WH, WO - WI,    WH*2+1, SCFT_WINGS);
    // Right wing
    tft.fillRect(SCFT_CX + WI,    SCFT_CY - WH, WO - WI,    WH*2+1, SCFT_WINGS);
    // Fin
    tft.fillRect(SCFT_CX - FIN_W, SCFT_CY + DOT_R + FIN_GAP, FIN_W*2+1, FIN_H, SCFT_WINGS);
    // Centre dot — drawn last so it sits on top of wings/fin overlap at centre
    tft.fillCircle(SCFT_CX, SCFT_CY, DOT_R, SCFT_WINGS);
}


// ── Clip endpoint to disc ─────────────────────────────────────────────────────────────
static void _scftClipToDisk(float px, float py, float qx, float qy,
                            float &ox, float &oy) {
    float cx = qx - SCFT_CX, cy = qy - SCFT_CY;
    if (cx*cx + cy*cy <= (float)SCFT_R * SCFT_R) { ox = qx; oy = qy; return; }
    float dx = qx-px, dy = qy-py;
    float ax = px-SCFT_CX, ay = py-SCFT_CY;
    float a = dx*dx + dy*dy;
    float b = 2.0f*(ax*dx + ay*dy);
    float c = ax*ax + ay*ay - (float)SCFT_R*(float)SCFT_R;
    float disc = b*b - 4.0f*a*c;
    if (disc < 0.0f || a < 0.001f) { ox = qx; oy = qy; return; }
    float sq = sqrtf(disc);
    float t  = (-b - sq) / (2.0f*a);
    if (t < 0.0f || t > 1.0f) t = (-b + sq) / (2.0f*a);
    t = max(0.0f, min(1.0f, t));
    ox = px + t*dx; oy = py + t*dy;
}


// ── Pitch ladder ──────────────────────────────────────────────────────────────────────
// Marks each scanline it touches in _scftLadderDirty[] so delta fill can erase old pixels.
static void _scftDrawLadder(RA8875 &tft,
                            float BCX, float BCY,
                            float sinR, float cosR) {
    static const int16_t HL_MAJ  = 36;
    static const int16_t HL_MIN  = 22;
    static const int16_t LBL_GAP = 8;
    static const uint8_t FONT_W  = 8;
    static const uint8_t FONT_H  = 14;

    tft.setFont(&Roboto_Black_12);
    tft.setTextColor(SCFT_LADDER);

    auto rnd = [](float v) -> int16_t {
        return (int16_t)(v + (v > 0.0f ? 0.5f : -0.5f));
    };
    int16_t spx = rnd(sinR), spy = rnd(-cosR);

    auto clampToDisc = [](int16_t &px, int16_t &py) {
        float dx = (float)px - SCFT_CX, dy = (float)py - SCFT_CY;
        float d2 = dx*dx + dy*dy;
        if (d2 > (float)(SCFT_R-1) * (float)(SCFT_R-1)) {
            float s = (float)(SCFT_R-1) / sqrtf(d2);
            px = SCFT_CX + (int16_t)(dx*s);
            py = SCFT_CY + (int16_t)(dy*s);
        }
    };

    auto boxInDisc = [](int16_t lx, int16_t ly, uint8_t lw, uint8_t lh) -> bool {
        for (int8_t cx = 0; cx <= 1; cx++)
            for (int8_t cy = 0; cy <= 1; cy++) {
                float dx = (float)(lx + cx*(int16_t)lw) - SCFT_CX;
                float dy = (float)(ly + cy*(int16_t)lh) - SCFT_CY;
                if (dx*dx + dy*dy >= (float)SCFT_R * SCFT_R) return false;
            }
        return true;
    };

    float R2 = (float)SCFT_R * (float)SCFT_R;

    // Pitch clamped ±90° in KSP. Step in 5° increments.
    for (int16_t lad_p_x2 = -180; lad_p_x2 <= 180; lad_p_x2 += 10) {
        if (lad_p_x2 == 0) continue;   // skip horizon (drawn separately)
        float  lad_p = (float)lad_p_x2 * 0.5f;
        float  delta = lad_p * SCFT_SCALE;

        // Rung foot = ball centre + lad_p offset along sky-ward (sinR,-cosR)
        // Equivalently: (BCX + delta*sinR, BCY - delta*cosR)
        float rfx = BCX + delta * sinR;
        float rfy = BCY - delta * cosR;

        float fd2 = (rfx-SCFT_CX)*(rfx-SCFT_CX) + (rfy-SCFT_CY)*(rfy-SCFT_CY);
        if (fd2 >= R2) continue;

        bool is_major = (lad_p_x2 % 20 == 0);   // divisible by 10°
        // else minor (5°)
        float hl = is_major ? (float)HL_MAJ : (float)HL_MIN;

        // Rung line along horizon direction (cosR, sinR)
        float rx1 = rfx - hl*cosR, ry1 = rfy - hl*sinR;
        float rx2 = rfx + hl*cosR, ry2 = rfy + hl*sinR;

        float cx1, cy1, cx2, cy2;
        _scftClipToDisk(rfx, rfy, rx1, ry1, cx1, cy1);
        _scftClipToDisk(rfx, rfy, rx2, ry2, cx2, cy2);

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
            for (int16_t yd = y_lo; yd <= y_hi; yd++) _scftLadderDirtySet(yd);
        }

        tft.drawLine(lx1,  ly1,  lx2,  ly2,  SCFT_LADDER);
        tft.drawLine(lx1b, ly1b, lx2b, ly2b, SCFT_LADDER);

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
                for (int16_t yd = ly; yd < ly + FONT_H; yd++) _scftLadderDirtySet(yd);
                tft.setCursor(lx, ly); tft.print(lbl);
            }
        };
        placeLabel(cx1, cy1);
        placeLabel(cx2, cy2);
    }
}


// ── Full ball draw ────────────────────────────────────────────────────────────────────
static void _scftFullDraw(RA8875 &tft, float sinR, float cosR, float K) {
    tft.fillCircle(SCFT_CX, SCFT_CY, SCFT_R, SCFT_SKY);

    // Incremental bx: bx(y) = (K + cosR*y)/sinR
    // bx(y+1) = bx(y) + cosR/sinR  → one float add per row instead of multiply+divide
    bool near_horiz = (fabsf(sinR) < 0.01f);
    float bx_f    = near_horiz ? 0.0f : (K + cosR * (float)SCFT_BALL_Y0) / sinR;
    float bx_step = near_horiz ? 0.0f : cosR / sinR;
    bool  gl0     = (sinR > 0.0f);

    for (uint16_t i = 0; i < SCFT_SCANLINES; i++) {
        int16_t chw = _scftChordTable[i];
        if (chw <= 0) { _scftPrevBx[i] = SCFT_BX_ALLSKY; bx_f += bx_step; continue; }
        int16_t y  = SCFT_BALL_Y0 + (int16_t)i;
        int16_t x0 = SCFT_CX - chw, x1 = SCFT_CX + chw;
        bool    gl;
        int16_t bx;
        if (near_horiz) {
            bool sky = (-cosR * (float)y > K);
            bx = sky ? SCFT_BX_ALLSKY : SCFT_BX_ALLGND;
            gl = !sky;
        } else {
            bx = (int16_t)bx_f;
            gl = gl0;
            if (sinR > 0.0f) {
                if (bx <= x0) { bx = SCFT_BX_ALLSKY;  gl = false; }
                else if (bx >= x1) { bx = SCFT_BX_ALLGND; }
            } else {
                if (bx >= x1) { bx = SCFT_BX_ALLSKY;  gl = false; }
                else if (bx <= x0) { bx = SCFT_BX_ALLGND; }
            }
        }
        _scftPrevBx[i] = bx;
        _scftDrawScanline(tft, y, x0, x1, bx, gl);
        bx_f += bx_step;
    }
    _scftPrevGroundLeft = gl0;
}


// ── Delta update ──────────────────────────────────────────────────────────────────────
static void _scftDeltaDraw(RA8875 &tft, float sinR, float cosR, float K) {
    bool  newGL      = (sinR > 0.0f);
    bool  near_horiz = (fabsf(sinR) < 0.01f);
    float bx_f       = near_horiz ? 0.0f : (K + cosR * (float)SCFT_BALL_Y0) / sinR;
    float bx_step    = near_horiz ? 0.0f : cosR / sinR;

    // Analytically bound the split-row band using horizon chord endpoints.
    // Outside this band every row is all-sky or all-gnd; we only need to repaint
    // them if they transition (bx_changed) or are in the horizon/ladder dirty bands.
    // The chord y-range in scanline index space is ly_lo_i..ly_hi_i.
    // We also track the previous and current horizon bands here.
    int16_t split_lo = 0, split_hi = (int16_t)(SCFT_SCANLINES - 1);
    if (!near_horiz) {
        float pitchPx = -(K - sinR * (float)SCFT_CX + cosR * (float)SCFT_CY);
        float hc2 = (float)SCFT_R * SCFT_R - pitchPx * pitchPx;
        if (hc2 >= 0.0f) {
            float BCY = (float)SCFT_CY + pitchPx * cosR;
            float hc  = sqrtf(hc2);
            float ly1 = BCY - hc * sinR, ly2 = BCY + hc * sinR;
            float ylo = (ly1 < ly2 ? ly1 : ly2) - 2.0f;
            float yhi = (ly1 > ly2 ? ly1 : ly2) + 1.0f;
            split_lo = (int16_t)(ylo - (float)SCFT_BALL_Y0);
            split_hi = (int16_t)(yhi - (float)SCFT_BALL_Y0);
            if (split_lo < 0) split_lo = 0;
            if (split_hi >= (int16_t)SCFT_SCANLINES) split_hi = (int16_t)(SCFT_SCANLINES - 1);
        }
    }

    for (uint16_t i = 0; i < SCFT_SCANLINES; i++) {
        bool in_split   = ((int16_t)i >= split_lo && (int16_t)i <= split_hi);
        bool in_prev_horiz  = ((int16_t)(SCFT_BALL_Y0 + i) >= _scftPrevHorizLo &&
                                (int16_t)(SCFT_BALL_Y0 + i) <= _scftPrevHorizHi);
        bool in_prev_ladder = _scftLadderDirtyTest(i);

        int16_t chw = _scftChordTable[i];

        // For rows outside the split band: compute the sentinel bx quickly
        // and only skip if it hasn't changed. This catches the case where
        // the horizon sweeps into a previously all-sky/all-gnd row.
        if (!in_split && !in_prev_horiz && !in_prev_ladder) {
            if (chw <= 0) { bx_f += bx_step; continue; }
            // Compute sentinel for this row
            int16_t bx_new_s;
            if (near_horiz) {
                int16_t y = SCFT_BALL_Y0 + (int16_t)i;
                bx_new_s = (-cosR * (float)y > K) ? SCFT_BX_ALLSKY : SCFT_BX_ALLGND;
            } else {
                int16_t x0 = SCFT_CX - chw, x1 = SCFT_CX + chw;
                int16_t bx = (int16_t)bx_f;
                if (sinR > 0.0f)
                    bx_new_s = (bx <= x0) ? SCFT_BX_ALLSKY : (bx >= x1) ? SCFT_BX_ALLGND : bx;
                else
                    bx_new_s = (bx >= x1) ? SCFT_BX_ALLSKY : (bx <= x0) ? SCFT_BX_ALLGND : bx;
            }
            bx_f += bx_step;
            if (bx_new_s == _scftPrevBx[i] && newGL == _scftPrevGroundLeft) continue;
            // Sentinel changed — fall through to full repaint below by re-entering loop
            // We already advanced bx_f, so use bx_new_s directly
            int16_t y  = SCFT_BALL_Y0 + (int16_t)i;
            int16_t x0 = SCFT_CX - chw, x1 = SCFT_CX + chw;
            bool gl = (bx_new_s == SCFT_BX_ALLGND) ? newGL :
                      (bx_new_s == SCFT_BX_ALLSKY)  ? false : newGL;
            _scftDrawScanline(tft, y, x0, x1, bx_new_s, gl);
            _scftPrevBx[i] = bx_new_s;
            continue;
        }

        if (chw <= 0) { _scftPrevBx[i] = SCFT_BX_ALLSKY; bx_f += bx_step; continue; }

        int16_t y  = SCFT_BALL_Y0 + (int16_t)i;
        int16_t x0 = SCFT_CX - chw, x1 = SCFT_CX + chw;
        bool    gl;
        int16_t bx_new;

        if (near_horiz) {
            bool sky = (-cosR * (float)y > K);
            bx_new = sky ? SCFT_BX_ALLSKY : SCFT_BX_ALLGND;
            gl = !sky;
        } else {
            bx_new = (int16_t)bx_f;
            gl = newGL;
            if (sinR > 0.0f) {
                if (bx_new <= x0) { bx_new = SCFT_BX_ALLSKY;  gl = false; }
                else if (bx_new >= x1) { bx_new = SCFT_BX_ALLGND; }
            } else {
                if (bx_new >= x1) { bx_new = SCFT_BX_ALLSKY;  gl = false; }
                else if (bx_new <= x0) { bx_new = SCFT_BX_ALLGND; }
            }
        }
        bx_f += bx_step;

        bool bx_changed = (bx_new != _scftPrevBx[i] || newGL != _scftPrevGroundLeft);

        if (!bx_changed && !in_prev_horiz && !in_prev_ladder) continue;

        _scftDrawScanline(tft, y, x0, x1, bx_new, gl);
        _scftPrevBx[i] = bx_new;
    }
    _scftPrevGroundLeft = newGL;
}


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

// Marker size — half-diagonal in pixels. Diamond is 2*ADI_MRK+1 pixels tip-to-tip.
static const int16_t SCFT_ADI_MRK_HD = 8;   // 17 px total diagonal

// Shortest-arc delta between two headings, result in [-180, 180].
static inline float _scftHdgDelta(float a, float b) {
    float d = a - b;
    while (d >  180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return d;
}

// Draw a single diamond marker on the ADI ball at the given world-space heading/pitch.
// Skips the draw if the computed position falls outside the ball's visible cone.
// Uses the horizontal-split two-triangle diamond pattern (matching MNVR/DOCK).
//
// Marks occupied scanlines in _scftLadderDirty (same mechanism ladder uses) so that
// the next frame's delta fill repaints those rows — prevents trails when the marker
// moves but the horizon doesn't.
static void _scftDrawAdiMarker(RA8875 &tft, float markerHdg, float markerPitch,
                               uint16_t fillCol) {
    // Delta from current vessel attitude
    float dh = _scftHdgDelta(markerHdg, state.heading);
    float dp = markerPitch - state.pitch;

    // Ball uses negated roll (matches KerbalSimpit convention)
    float cosR = _scftCos(-state.roll);
    float sinR = _scftSin(-state.roll);

    // Unrolled-frame offset: +dh degrees rightward, +dp degrees upward (so -y)
    float ux = dh * SCFT_SCALE;
    float uy = -dp * SCFT_SCALE;

    // Apply roll rotation (angle = -state.roll, so using cosR/sinR from above).
    // Marker is treated as a 2D point on the pitch/roll-rolled ball — both dh
    // and dp combine to determine its screen position after roll rotation.
    // Horizontal marker x will not match the heading tape's x when roll != 0;
    // the ball shows the combined attitude-error in its own rolled frame, while
    // the tape shows only the heading axis.
    int16_t sx = (int16_t)(SCFT_CX + ux * cosR - uy * sinR);
    int16_t sy = (int16_t)(SCFT_CY + ux * sinR + uy * cosR);

    // Clip: entire marker must fit inside ball. Use (R - HD) so outline doesn't cross rim.
    int16_t dx = sx - SCFT_CX, dy = sy - SCFT_CY;
    int32_t rInner = (int32_t)SCFT_R - SCFT_ADI_MRK_HD;
    if ((int32_t)dx*dx + (int32_t)dy*dy > rInner * rInner) return;

    // Filled diamond (horizontal split — two triangles sharing the waist row)
    tft.fillTriangle(sx - SCFT_ADI_MRK_HD, sy,
                     sx + SCFT_ADI_MRK_HD, sy,
                     sx,                   sy - SCFT_ADI_MRK_HD,
                     fillCol);
    tft.fillTriangle(sx - SCFT_ADI_MRK_HD, sy,
                     sx + SCFT_ADI_MRK_HD, sy,
                     sx,                   sy + SCFT_ADI_MRK_HD,
                     fillCol);

    // White 1-px outline for contrast against both sky and ground fills
    tft.drawLine(sx,                   sy - SCFT_ADI_MRK_HD,
                 sx + SCFT_ADI_MRK_HD, sy,                   TFT_WHITE);
    tft.drawLine(sx + SCFT_ADI_MRK_HD, sy,
                 sx,                   sy + SCFT_ADI_MRK_HD, TFT_WHITE);
    tft.drawLine(sx,                   sy + SCFT_ADI_MRK_HD,
                 sx - SCFT_ADI_MRK_HD, sy,                   TFT_WHITE);
    tft.drawLine(sx - SCFT_ADI_MRK_HD, sy,
                 sx,                   sy - SCFT_ADI_MRK_HD, TFT_WHITE);

    // Tell next frame's delta fill to repaint these scanlines. Prevents trails when
    // the marker moves without the horizon line or ladder touching the same rows.
    for (int16_t y = sy - SCFT_ADI_MRK_HD; y <= sy + SCFT_ADI_MRK_HD; y++) {
        _scftLadderDirtySet(y);
    }
}


// ── Master ball update ────────────────────────────────────────────────────────────────
static void _scftDrawBall(RA8875 &tft, bool fullRedraw) {
    _scftBuildChordTable();   // no-op after first call

    float pitch = state.pitch;
    float roll  = -state.roll;   // negate: KerbalSimpit sign convention

    float cosR    = _scftCos(roll);
    float sinR    = _scftSin(roll);
    float pitchPx = pitch * SCFT_SCALE;

    float K   = sinR * (float)SCFT_CX - cosR * (float)SCFT_CY - pitchPx;
    float BCX = (float)SCFT_CX - pitchPx * sinR;
    float BCY = (float)SCFT_CY + pitchPx * cosR;
    float hc2 = (float)SCFT_R * SCFT_R - pitchPx * pitchPx;

    uint32_t _t0, _t1;

    // ── 1. Sky/ground fill ────────────────────────────────────────────────────────────
    _t0 = micros();
    if (fullRedraw) _scftFullDraw(tft, sinR, cosR, K);
    else            _scftDeltaDraw(tft, sinR, cosR, K);
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
        tft.drawLine(lx1, ly1, lx2, ly2, SCFT_HORIZON);
        auto rnd = [](float v) -> int16_t {
            return (int16_t)(v + (v > 0.0f ? 0.5f : -0.5f));
        };
        int16_t px = rnd(sinR), py = rnd(-cosR);
        tft.drawLine(lx1+px, ly1+py, lx2+px, ly2+py, SCFT_HORIZON);
        new_horiz_lo = min(min(ly1, ly2), min((int16_t)(ly1+py), (int16_t)(ly2+py)));
        new_horiz_hi = max(max(ly1, ly2), max((int16_t)(ly1+py), (int16_t)(ly2+py)));
    }
    _scftPrevHorizLo = new_horiz_lo;
    _scftPrevHorizHi = new_horiz_hi;
    _t1 = micros();
    Serial.print("  horiz="); Serial.print((_t1-_t0)/1000.0f, 2); Serial.print("ms");

    // ── 3. Pitch ladder ───────────────────────────────────────────────────────────────
    // Swap bitmaps: prev = last frame's dirty set (used by delta fill above),
    // current = cleared for this frame's ladder draw.
    _t0 = micros();
    memcpy(_scftLadderDirtyPrev, _scftLadderDirty, sizeof(_scftLadderDirty));
    memset(_scftLadderDirty, 0, sizeof(_scftLadderDirty));
    _scftDrawLadder(tft, BCX, BCY, sinR, cosR);
    _t1 = micros();
    Serial.print("  ladder="); Serial.print((_t1-_t0)/1000.0f, 2); Serial.print("ms");

    // ── 4. ADI markers — drawn on top of ball, under the aircraft reference ───────────
    //    Prograde always drawn; target if available; maneuver if active.
    //    Each call self-clips to the ball's visible cone.
    _scftDrawAdiMarker(tft, _scftVelHdg, _scftVelPitch, TFT_NEON_GREEN);
    if (state.targetAvailable)
        _scftDrawAdiMarker(tft, state.tgtHeading, state.tgtPitch, TFT_VIOLET);
    if (state.mnvrTime > 0.0f)
        _scftDrawAdiMarker(tft, state.mnvrHeading, state.mnvrPitch, TFT_BLUE);

    // ── 5. Aircraft symbol — drawn last so it is always on top ────────────────────────
    _scftDrawAircraftSymbol(tft);

    _scftPrevPitch = state.pitch;
    _scftPrevRoll  = state.roll;

    // Ball-side marker trackers — snapshot current values so the next frame's
    // dirty check sees no change unless something actually moved.
    _scftPrevHeadingBall    = state.heading;
    _scftPrevVelHdgBall     = _scftVelHdg;
    _scftPrevVelPitchBall   = _scftVelPitch;
    _scftPrevMnvrHdgBall    = state.mnvrHeading;
    _scftPrevMnvrPitchBall  = state.mnvrPitch;
    _scftPrevTgtHdgBall     = state.tgtHeading;
    _scftPrevTgtPitchBall   = state.tgtPitch;
    _scftPrevMnvrActiveBall = (state.mnvrTime > 0.0f);
    _scftPrevTgtAvailBall   = state.targetAvailable;
}


// ── Roll indicator state ──────────────────────────────────────────────────────────────
static float    _scftPrevRollIndicator = -9999.0f;   // last drawn pointer angle
static int16_t  _scftPrevRollReadout   = -9999;      // last drawn roll readout (integer degrees)
static uint16_t _scftPrevRollReadoutFg = 0;          // last drawn foreground colour

// ── Pitch readout state ───────────────────────────────────────────────────────────────
static int16_t  _scftPrevPitchReadout   = -9999;
static uint16_t _scftPrevPitchReadoutFg = 0;

// ── Roll indicator geometry ───────────────────────────────────────────────────────────
static const int16_t  SCFT_PTR_TIP_R  = SCFT_R + 3;   // tip clear of bezel (bezel outer = R+2)
static const int16_t  SCFT_PTR_BASE_R = SCFT_R + 20;  // base beyond tick outer edge (R+16)
static const int16_t  SCFT_PTR_W      = 8;            // half-width of pointer base

// Roll readout — two lines centred in fixed-width block
// Label: Roboto_Black_20 (smaller), Value: Roboto_Black_24 (larger)
static const int16_t  SCFT_ROLL_ANCHOR_X  = SCFT_CX + SCFT_R - 56; // moves with CX
static const int16_t  SCFT_ROLL_ANCHOR_Y  = SCFT_CY - SCFT_R - 24; // aligned with Pitch: label top
static const int16_t  SCFT_ROLL_W         = 80;   // block width (unchanged)
static const int16_t  SCFT_ROLL_LABEL_H   = 20;   // label line height (Roboto_Black_20)
static const int16_t  SCFT_ROLL_VALUE_H   = 26;   // value line height (Roboto_Black_24)
static const int16_t  SCFT_ROLL_GAP       = 3;    // gap between lines

// Update the roll numeric readout.
static void _scftUpdateRollReadout(RA8875 &tft, float roll) {
    int16_t  iRoll = (int16_t)roundf(roll);
    uint16_t fg    = TFT_DARK_GREEN;  // spacecraft — no roll warnings
    uint16_t bg    = TFT_BLACK;

    if (iRoll == _scftPrevRollReadout && fg == _scftPrevRollReadoutFg) return;

    // Erase previous value in black-on-black using the same font/box
    if (_scftPrevRollReadout > -9000) {
        char oldBuf[8];
        snprintf(oldBuf, sizeof(oldBuf), "%+d\xB0", _scftPrevRollReadout);
        textCenter(tft, &Roboto_Black_24,
                   SCFT_ROLL_ANCHOR_X, SCFT_ROLL_ANCHOR_Y + SCFT_ROLL_LABEL_H + SCFT_ROLL_GAP,
                   SCFT_ROLL_W, SCFT_ROLL_VALUE_H,
                   oldBuf, TFT_BLACK, TFT_BLACK);
    }

    // Line 1: "Roll:" — small font, centred in label row
    textCenter(tft, &Roboto_Black_20,
               SCFT_ROLL_ANCHOR_X, SCFT_ROLL_ANCHOR_Y,
               SCFT_ROLL_W, SCFT_ROLL_LABEL_H,
               "Roll:", TFT_WHITE, TFT_BLACK);

    // Line 2: signed value — larger font, centred in value row
    char buf[8];
    snprintf(buf, sizeof(buf), "%+d\xB0", iRoll);
    textCenter(tft, &Roboto_Black_24,
               SCFT_ROLL_ANCHOR_X, SCFT_ROLL_ANCHOR_Y + SCFT_ROLL_LABEL_H + SCFT_ROLL_GAP,
               SCFT_ROLL_W, SCFT_ROLL_VALUE_H,
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
static const int16_t  SCFT_PTAPE_MRK_TIP_X  = SCFT_PTAPE_X + SCFT_PTAPE_W - 20; // 18px — matches HDG tape
static const int16_t  SCFT_PTAPE_MRK_HW     = 6;

// State
static float   _scftPrevPitch2      = -9999.0f;   // pitch tape (distinct from ball state)
static int16_t _scftPrevPitchBox    = -9999;
static float   _scftPrevVelPitch    = -9999.0f;
static float   _scftPrevTgtPitch    = -9999.0f;
static float   _scftPrevMnvrPitch   = -9999.0f;

// Draw/update the pitch value box — cached on integer change.
static void _scftUpdatePitchBox(RA8875 &tft, float pitch) {
    int16_t iPitch = (int16_t)roundf(pitch);
    if (iPitch == _scftPrevPitchBox) return;

    char newBuf[8];
    snprintf(newBuf, sizeof(newBuf), "%+d\xB0", iPitch);

    if (_scftPrevPitchBox > -9000) {
        char oldBuf[8];
        snprintf(oldBuf, sizeof(oldBuf), "%+d\xB0", _scftPrevPitchBox);
        textCenter(tft, &Roboto_Black_24,
                   SCFT_PTAPE_BOX_X, SCFT_PTAPE_BOX_Y + 1,
                   SCFT_PTAPE_BOX_W, SCFT_PTAPE_BOX_H - 2,
                   oldBuf, TFT_BLACK, TFT_BLACK);
    }
    textCenter(tft, &Roboto_Black_24,
               SCFT_PTAPE_BOX_X, SCFT_PTAPE_BOX_Y + 1,
               SCFT_PTAPE_BOX_W, SCFT_PTAPE_BOX_H - 2,
               newBuf, TFT_DARK_GREEN, TFT_BLACK);

    _scftPrevPitchBox = iPitch;
}

// Draw the full pitch tape for the given pitch.
static void _scftDrawPitchTape(RA8875 &tft, float pitch) {
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

    tft.setFont(&Roboto_Black_12);

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
static void _scftUpdatePitchTape(RA8875 &tft, float pitch) {
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
static const int16_t  SCFT_HDG_MRK_TIP_Y  = SCFT_HDG_TAPE_Y + 20;  // 18px tall
static const int16_t  SCFT_HDG_MRK_HW     = 6;                     // half-width → 13px wide

// Draw/update the heading number box — cached, only redraws when integer heading changes.
// Uses textCenter for flicker-free rendering: erase old value with black-on-black first.
static void _scftUpdateHdgBox(RA8875 &tft, float hdg) {
    int16_t iHdg = (int16_t)roundf(hdg) % 360;
    if (iHdg < 0) iHdg += 360;
    if (iHdg == _scftPrevHdgBox) return;

    char oldBuf[8], newBuf[8];
    snprintf(newBuf, sizeof(newBuf), "%03d\xB0", iHdg);

    // Erase previous value with black-on-black
    if (_scftPrevHdgBox >= 0) {
        snprintf(oldBuf, sizeof(oldBuf), "%03d\xB0", _scftPrevHdgBox);
        textCenter(tft, &Roboto_Black_24,
                   SCFT_HDG_BOX_X, SCFT_HDG_BOX_Y + 1,
                   SCFT_HDG_BOX_W, SCFT_HDG_BOX_H - 2,
                   oldBuf, TFT_BLACK, TFT_BLACK);
    }

    // Draw new value
    textCenter(tft, &Roboto_Black_24,
               SCFT_HDG_BOX_X, SCFT_HDG_BOX_Y + 1,
               SCFT_HDG_BOX_W, SCFT_HDG_BOX_H - 2,
               newBuf, TFT_DARK_GREEN, TFT_BLACK);

    _scftPrevHdgBox = iHdg;
}

// Draw the full heading tape. Only the tape strip — box is handled separately.
static void _scftDrawHeadingTape(RA8875 &tft, float hdg) {
    while (hdg <   0.0f) hdg += 360.0f;
    while (hdg >= 360.0f) hdg -= 360.0f;

    tft.fillRect(SCFT_HDG_TAPE_X, SCFT_HDG_TAPE_Y, SCFT_HDG_TAPE_W, SCFT_HDG_TAPE_H, TFT_BLACK);

    // Redraw box border after fill (fill erases box sides where they overlap)
    tft.drawRect(SCFT_HDG_BOX_X, SCFT_HDG_BOX_Y, SCFT_HDG_BOX_W, SCFT_HDG_BOX_H, TFT_LIGHT_GREY);

    // Force box number to redraw — fill blackened the interior
    _scftPrevHdgBox = -1;

    tft.setFont(&Roboto_Black_12);

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
static void _scftUpdateHeadingTape(RA8875 &tft, float hdg) {
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


// Draw the roll pointer triangle for a given roll angle.
static void _scftDrawRollPointer(RA8875 &tft, float roll, uint16_t colour) {
    float a    = (roll - 90.0f) * (float)DEG_TO_RAD;
    float cosA = cosf(a), sinA = sinf(a);
    int16_t tx  = (int16_t)(SCFT_CX + SCFT_PTR_TIP_R  * cosA);
    int16_t ty  = (int16_t)(SCFT_CY + SCFT_PTR_TIP_R  * sinA);
    int16_t bcx = (int16_t)(SCFT_CX + SCFT_PTR_BASE_R * cosA);
    int16_t bcy = (int16_t)(SCFT_CY + SCFT_PTR_BASE_R * sinA);
    int16_t b1x = bcx + (int16_t)(-sinA * SCFT_PTR_W);
    int16_t b1y = bcy + (int16_t)( cosA * SCFT_PTR_W);
    int16_t b2x = bcx - (int16_t)(-sinA * SCFT_PTR_W);
    int16_t b2y = bcy - (int16_t)( cosA * SCFT_PTR_W);
    tft.fillTriangle(tx, ty, b1x, b1y, b2x, b2y, colour);
}

// Erase the roll pointer — uses a 2px expanded triangle to catch any stray pixels
// left by integer rounding in the draw pass.
static void _scftEraseRollPointer(RA8875 &tft, float roll) {
    float a    = (roll - 90.0f) * (float)DEG_TO_RAD;
    float cosA = cosf(a), sinA = sinf(a);
    // Expand tip inward by 1px along radial, base outward by 1px, width +2px
    int16_t tx  = (int16_t)(SCFT_CX + (SCFT_PTR_TIP_R  - 1) * cosA);
    int16_t ty  = (int16_t)(SCFT_CY + (SCFT_PTR_TIP_R  - 1) * sinA);
    int16_t bcx = (int16_t)(SCFT_CX + (SCFT_PTR_BASE_R + 1) * cosA);
    int16_t bcy = (int16_t)(SCFT_CY + (SCFT_PTR_BASE_R + 1) * sinA);
    int16_t b1x = bcx + (int16_t)(-sinA * (SCFT_PTR_W + 2));
    int16_t b1y = bcy + (int16_t)( cosA * (SCFT_PTR_W + 2));
    int16_t b2x = bcx - (int16_t)(-sinA * (SCFT_PTR_W + 2));
    int16_t b2y = bcy - (int16_t)( cosA * (SCFT_PTR_W + 2));
    tft.fillTriangle(tx, ty, b1x, b1y, b2x, b2y, TFT_BLACK);
}

// Draw a single bank scale tick at the given bank angle.
static void _scftDrawBankTick(RA8875 &tft, int16_t bankDeg) {
    bool isMajor = (bankDeg == 0 || bankDeg == 30 || bankDeg == -30 ||
                                     bankDeg == 60 || bankDeg == -60);
    int16_t tOuter = SCFT_R + 16;
    int16_t tInner = SCFT_R + (isMajor ? 2 : 6);
    // 0° and ±60° white, all others light grey
    uint16_t col = (bankDeg == 0 || bankDeg == 60 || bankDeg == -60)
                   ? TFT_WHITE : TFT_LIGHT_GREY;
    float a    = (bankDeg - 90.0f) * (float)DEG_TO_RAD;
    float cosA = cosf(a), sinA = sinf(a);
    int16_t ox = (int16_t)(SCFT_CX + tOuter * cosA);
    int16_t oy = (int16_t)(SCFT_CY + tOuter * sinA);
    int16_t ix = (int16_t)(SCFT_CX + tInner * cosA);
    int16_t iy = (int16_t)(SCFT_CY + tInner * sinA);
    tft.drawLine(ox, oy, ix, iy, col);
}

// Update the roll pointer: erase old, redraw any tick it covered, draw new.
static void _scftUpdateRollIndicator(RA8875 &tft, float roll) {
    if (fabsf(roll - _scftPrevRollIndicator) < 0.2f) return;

    static const int16_t ticks[] = {-60,-45,-30,-20,-10,0,10,20,30,45,60};

    // Erase old pointer with expanded triangle to catch stray pixels
    if (_scftPrevRollIndicator > -9000.0f) {
        float prevClamped = _scftPrevRollIndicator;
        if      (prevClamped >  60.0f) prevClamped =  60.0f;
        else if (prevClamped < -60.0f) prevClamped = -60.0f;
        _scftEraseRollPointer(tft, prevClamped);
        // Redraw any tick the erase region may have covered (within 4° — slightly
        // wider than before to account for the expanded erase)
        for (uint8_t i = 0; i < 11; i++) {
            if (fabsf(_scftPrevRollIndicator - ticks[i]) < 4.0f) {
                _scftDrawBankTick(tft, ticks[i]);
                break;
            }
        }
    }

    // Draw new pointer — clamped to ±60° so it stays within the scale marks
    float clampedRoll = roll;
    if      (clampedRoll >  60.0f) clampedRoll =  60.0f;
    else if (clampedRoll < -60.0f) clampedRoll = -60.0f;
    _scftDrawRollPointer(tft, clampedRoll, TFT_YELLOW);
    _scftPrevRollIndicator = roll;
}


// ── Screen chrome ─────────────────────────────────────────────────────────────────────
static void chromeScreen_SCFT(RA8875 &tft) {
    _scftBuildChordTable();
    _scftFullRedrawNeeded      = true;
    _scftPrevHorizLo           = INT16_MAX;
    _scftPrevHorizHi           = INT16_MIN;
    _scftPrevRollIndicator     = -9999.0f;
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
    memset(_scftLadderDirty,     0, sizeof(_scftLadderDirty));
    memset(_scftLadderDirtyPrev, 0, sizeof(_scftLadderDirtyPrev));
    _scftPrevPitch        = -9999.0f;
    _scftPrevRoll         = -9999.0f;

    // Bezel ring
    tft.drawCircle(SCFT_CX, SCFT_CY, SCFT_R,     TFT_LIGHT_GREY);
    tft.drawCircle(SCFT_CX, SCFT_CY, SCFT_R + 1, TFT_WHITE);
    tft.drawCircle(SCFT_CX, SCFT_CY, SCFT_R + 2, TFT_DARK_GREY);

    // Bank scale tick marks outside the disc circumference (upper arc)
    static const int16_t ticks[] = {-60,-45,-30,-20,-10,0,10,20,30,45,60};
    for (uint8_t i = 0; i < 11; i++) _scftDrawBankTick(tft, ticks[i]);

    // Labels at ±30° and ±60° — drawn at R+28 along the tick radial
    tft.setFont(&Roboto_Black_12);
    tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
    static const int16_t labelTicks[] = {-60, -30, 30, 60};
    static const char *  labelText[]  = {"60", "30", "30", "60"};
    static const uint8_t FONT_W = 8, FONT_H = 14;
    for (uint8_t i = 0; i < 4; i++) {
        int16_t bank = labelTicks[i];
        float   a    = (bank - 90.0f) * (float)DEG_TO_RAD;
        int16_t lc_x = (int16_t)(SCFT_CX + (SCFT_R + 28) * cosf(a));
        int16_t lc_y = (int16_t)(SCFT_CY + (SCFT_R + 28) * sinf(a));
        uint8_t lw   = strlen(labelText[i]) * FONT_W;
        tft.setCursor(lc_x - lw / 2, lc_y - FONT_H / 2);
        tft.print(labelText[i]);
    }

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

    // "Pitch:" right-justified to right edge of pitch value box
    textRight(tft, &Roboto_Black_20,
              SCFT_PTAPE_BOX_X, SCFT_PTAPE_Y - 24,
              SCFT_PTAPE_BOX_W, 24,
              "Pitch:", TFT_WHITE, TFT_BLACK);

    // Heading tape border — top (light grey) and two sides (darker), open at bottom
    tft.drawLine(SCFT_HDG_TAPE_X - 1, SCFT_HDG_TAPE_Y - 1,
                 SCFT_HDG_TAPE_X + SCFT_HDG_TAPE_W, SCFT_HDG_TAPE_Y - 1, TFT_LIGHT_GREY);
    tft.drawLine(SCFT_HDG_TAPE_X - 1, SCFT_HDG_TAPE_Y - 1,
                 SCFT_HDG_TAPE_X - 1, SCFT_HDG_TAPE_Y + SCFT_HDG_TAPE_H, TFT_GREY);
    tft.drawLine(SCFT_HDG_TAPE_X + SCFT_HDG_TAPE_W, SCFT_HDG_TAPE_Y - 1,
                 SCFT_HDG_TAPE_X + SCFT_HDG_TAPE_W, SCFT_HDG_TAPE_Y + SCFT_HDG_TAPE_H, TFT_GREY);

    // "Hdg:" right-justified to same right edge as "Pitch:" label
    textRight(tft, &Roboto_Black_20,
              SCFT_PTAPE_BOX_X, SCFT_HDG_BOX_Y + SCFT_HDG_BOX_H / 2 - 10,
              SCFT_PTAPE_BOX_W, 24,
              "Hdg:", TFT_WHITE, TFT_BLACK);

    // ── Right panel chrome ─────────────────────────────────────────────────────────────
    // Vertical divider (2px) between ADI and panel
    tft.drawLine(SCFT_PANEL_X - 2, TITLE_TOP, SCFT_PANEL_X - 2, 479, TFT_GREY);
    tft.drawLine(SCFT_PANEL_X - 1, TITLE_TOP, SCFT_PANEL_X - 1, 479, TFT_GREY);

    static const tFont *PF = &Roboto_Black_20;

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
                     SCFT_PANEL_X + hw,     479, TFT_GREY);
        tft.drawLine(SCFT_PANEL_X + hw + 1, TITLE_TOP + 7 * rowHFor(SCFT_PANEL_NR),
                     SCFT_PANEL_X + hw + 1, 479, TFT_GREY);
    }
}


// ── Right panel update ─────────────────────────────────────────────────────────────────
// Uses printValue + rowCache[screen_SCFT] exactly as LNDG/ORB screens do.
// Cache slots 0-9: Alt.SL, V.Orb, ApA, PeA, T+Ap/Pe, T+Ign, ΔV.Stg, RCS, SAS label, SAS value
static void _scftUpdatePanel(RA8875 &tft, bool orbMode) {
    static const tFont *VF  = &Roboto_Black_24;   // value font
    static const uint8_t SC = (uint8_t)screen_SCFT;

    bool hasMnvr  = (state.mnvrTime > 0.0f);
    bool hasOrbit = (state.apoapsis > 0.0f || state.periapsis > 0.0f);
    uint16_t fw = SCFT_PANEL_W;
    uint16_t hw = SCFT_PANEL_W / 2;

    auto attPanelVal = [&](uint8_t row, uint8_t slot, const char *label,
                           const String &val, uint16_t fg, uint16_t bg) {
        // Suppress no-data indicator to dark grey regardless of requested colour
        uint16_t drawFg = (val == "---") ? TFT_DARK_GREY : fg;
        printValue(tft, VF,
                   SCFT_PANEL_X, rowYFor(row, SCFT_PANEL_NR),
                   fw,          rowHFor(SCFT_PANEL_NR),
                   label, val, drawFg, bg, COL_BACK, printState[SC][slot]);
        RowCache &rc = rowCache[SC][slot];
        rc.value = val; rc.fg = drawFg; rc.bg = bg;
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
        uint16_t fg = (state.apoapsis < 0.0f) ? TFT_YELLOW : TFT_DARK_GREEN;
        String val = hasOrbit ? formatAlt(state.apoapsis) : "---";
        attPanelVal(2, 2, "ApA:", val, fg, TFT_BLACK);
    }

    // Row 3 — PeA
    {
        uint16_t fg = (state.periapsis < 0.0f) ? TFT_YELLOW : TFT_DARK_GREEN;
        String val = hasOrbit ? formatAlt(state.periapsis) : "---";
        attPanelVal(3, 3, "PeA:", val, fg, TFT_BLACK);
    }

    // Row 4 — T+Ap or T+Pe (whichever is sooner and positive)
    {
        float tAp = state.timeToAp, tPe = state.timeToPe;
        float tNext; const char *lbl;
        if (!hasOrbit)                        { tNext = -1.0f; lbl = "T+Ap:"; }
        else if (tAp >= 0.0f && tPe >= 0.0f) { tNext = min(tAp,tPe); lbl = (tAp < tPe) ? "T+Ap:" : "T+Pe:"; }
        else if (tAp >= 0.0f)                 { tNext = tAp; lbl = "T+Ap:"; }
        else                                  { tNext = tPe; lbl = "T+Pe:"; }
        String val = (tNext >= 0.0f) ? formatTime((int64_t)tNext) : "---";
        attPanelVal(4, 4, lbl, val, TFT_DARK_GREEN, TFT_BLACK);
    }

    // Row 5 — T+Ign
    {
        float tIgn = hasMnvr ? state.mnvrTime - state.mnvrDuration / 2.0f : -1.0f;
        uint16_t fg, bg = TFT_BLACK;
        String val;
        if (!hasMnvr)        { fg = TFT_DARK_GREY; val = "---"; }
        else if (tIgn < 0.0f){ fg = TFT_WHITE; bg = TFT_RED; val = formatTime((int64_t)tIgn); }
        else if (tIgn < 60.0f){ fg = TFT_YELLOW; val = formatTime((int64_t)tIgn); }
        else                  { fg = TFT_DARK_GREEN; val = formatTime((int64_t)tIgn); }
        attPanelVal(5, 5, "T+Ign:", val, fg, bg);
    }

    // Row 6 — ΔV.Stg
    {
        String val = fmtMs(state.stageDeltaV);
        attPanelVal(6, 6, "\xCE\x94V.Stg:", val, TFT_DARK_GREEN, TFT_BLACK);
    }

    // Row 7 split — RCS button (left half) | SAS button (right half)
    {
        uint16_t ry  = TITLE_TOP + 7 * rowHFor(SCFT_PANEL_NR);  // full row top (no ROW_PAD)
        uint16_t rh  = 480 - ry;                                // extend to screen bottom
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
                drawButton(tft, SCFT_PANEL_X, ry, hw, rh, btn, &Roboto_Black_20, false);
                rc.value = rcsStr;
            }
        }

        // SAS button (slot 8)
        {
            const char *v; uint16_t fg, bg;
            switch (state.sasMode) {
                case 255: v = "SAS";  fg = TFT_DARK_GREY;  bg = TFT_OFF_BLACK;  break;
                case 0:   v = "STAB"; fg = TFT_WHITE;       bg = TFT_DARK_GREEN; break;
                case 1:   v = "PRO";  fg = TFT_DARK_GREY;   bg = TFT_NEON_GREEN; break;
                case 2:   v = "RETR"; fg = TFT_DARK_GREY;   bg = TFT_NEON_GREEN; break;
                case 3:   v = "NRM";  fg = TFT_WHITE;       bg = TFT_MAGENTA;    break;
                case 4:   v = "ANRM"; fg = TFT_WHITE;       bg = TFT_MAGENTA;    break;
                case 5:   v = "RAD+"; fg = TFT_DARK_GREY;   bg = TFT_SKY;        break;
                case 6:   v = "RAD-"; fg = TFT_DARK_GREY;   bg = TFT_SKY;        break;
                case 7:   v = "TGT";  fg = TFT_WHITE;       bg = TFT_VIOLET;     break;
                case 8:   v = "ATGT"; fg = TFT_WHITE;       bg = TFT_VIOLET;     break;
                case 9:   v = "MNVR"; fg = TFT_WHITE;       bg = TFT_BLUE;       break;
                default:  v = "SAS";  fg = TFT_DARK_GREY;   bg = TFT_OFF_BLACK;  break;
            }
            RowCache &rc = rowCache[SC][8];
            if (rc.value != v || rc.fg != fg || rc.bg != bg) {
                ButtonLabel btn = { v, fg, fg, bg, bg, TFT_GREY, TFT_GREY };
                drawButton(tft, sasX, ry, sasW, rh, btn, &Roboto_Black_20, false);
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
static void drawScreen_SCFT(RA8875 &tft) {
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
                     fabsf(_scftHdgDelta(state.heading, _scftPrevHeadingBall)) >= 0.5f;

    // Marker dirty — any visible marker's world position changed, or a marker's
    // availability toggled (target acquired/lost, maneuver set/cleared).
    if (!ballDirty) {
        ballDirty = fabsf(_scftHdgDelta(_scftVelHdg, _scftPrevVelHdgBall)) >= 0.5f ||
                    fabsf(_scftVelPitch - _scftPrevVelPitchBall)           >= 0.5f ||
                    mnvrActive != _scftPrevMnvrActiveBall                         ||
                    tgtAvail   != _scftPrevTgtAvailBall;
    }
    if (!ballDirty && mnvrActive) {
        ballDirty = fabsf(_scftHdgDelta(state.mnvrHeading, _scftPrevMnvrHdgBall)) >= 0.5f ||
                    fabsf(state.mnvrPitch - _scftPrevMnvrPitchBall)               >= 0.5f;
    }
    if (!ballDirty && tgtAvail) {
        ballDirty = fabsf(_scftHdgDelta(state.tgtHeading, _scftPrevTgtHdgBall)) >= 0.5f ||
                    fabsf(state.tgtPitch - _scftPrevTgtPitchBall)               >= 0.5f;
    }

    if (ballDirty) {
        bool full = _scftFullRedrawNeeded;
        uint32_t t0 = micros();
        _scftDrawBall(tft, full);
        _scftFullRedrawNeeded = false;
        uint32_t dt = micros() - t0;
        Serial.print(full ? "SCFT_FULL total=" : "SCFT_DELTA total=");
        Serial.print((float)dt / 1000.0f, 2);
        Serial.print("ms  pitch="); Serial.print(state.pitch, 1);
        Serial.print("  roll=");    Serial.println(state.roll, 1);
    }

    // Roll indicator — update whenever roll changes, independent of ball redraw
    _scftUpdateRollIndicator(tft, state.roll);
    _scftUpdateRollReadout(tft, state.roll);
    _scftUpdatePitchTape(tft, state.pitch);
    _scftUpdateHeadingTape(tft, state.heading);
    _scftUpdatePanel(tft, orbMode);
}

