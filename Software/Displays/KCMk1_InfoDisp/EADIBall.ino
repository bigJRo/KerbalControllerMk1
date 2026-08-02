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
