/***************************************************************************************
   Screen_LNDG.ino -- Landing screen (stub: altitude tape + X-Pointer only)
   Re-entry mode: unchanged text-only original (preserved below).
   Powered descent: altitude tape + X-Pointer square. Right panel TBD.
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


bool _lndgReentryMode    = false;
bool _lndgReentryRow3PeA  = true;
bool _lndgReentryRow0TPe  = false;
bool _lndgReentryRow1SL   = false;

bool _drogueDeployed  = false;
bool _mainDeployed    = false;
bool _drogueCut       = false;
bool _mainCut         = false;
bool _drogueArmedSafe = false;
bool _mainArmedSafe   = false;


/***************************************************************************************
   LAYOUT CONSTANTS
   Altitude tape: x=4..34, y=TITLE_TOP..SCREEN_H  (full content height = 418px = 0-500m)
   X-Pointer square: 410×410, x=48..458, y=TITLE_TOP..TITLE_TOP+410
   Right panel: x=48..CONTENT_W — reserved for later
****************************************************************************************/
static const uint16_t LNDG_TAPE_X    = 40;
static const uint16_t LNDG_TAPE_W    = 30;
static const uint16_t LNDG_TAPE_Y    = TITLE_TOP + 8;
static const uint16_t LNDG_TAPE_H    = SCREEN_H - LNDG_TAPE_Y;
static const float    LNDG_TAPE_PPM  = (float)LNDG_TAPE_H / 500.0f;

static const uint16_t LNDG_XP_X     = 120;
static const uint16_t LNDG_XP_Y     = LNDG_TAPE_Y + 20;
static const uint16_t LNDG_XP_SIDE  = 230;
static const uint16_t LNDG_XP_CX    = LNDG_XP_X + LNDG_XP_SIDE / 2;  // 235
static const uint16_t LNDG_XP_CY    = LNDG_XP_Y + LNDG_XP_SIDE / 2;  // 205
static const float    LNDG_XP_SCALE = 7.000f;

// X-Pointer field colours
static const uint16_t LNDG_XP_BG    = TFT_DARK_GREY;
static const uint16_t LNDG_XP_GRID  = TFT_LIGHT_GREY;
static const uint16_t LNDG_XP_INNER = TFT_GREY;

// Needle state
static int16_t _lndgPrevLatX  = -999;
static int16_t _lndgPrevFwdY  = -999;
static float   _lndgPrevAlt   = -1.0f;
static bool    _lndgLowAltMode = false;  // true when radarAlt < 50m (high-res tape)


/***************************************************************************************
   HORIZONTAL VELOCITY COLOUR
****************************************************************************************/
static void _lndgHorzColor(float v, float tGround, uint16_t &fg, uint16_t &bg) {
    float av = fabsf(v);
    float warnMs, alarmMs;
    if      (tGround < 0.0f || tGround > LNDG_HVEL_T_LOOSE_S)
        { warnMs = LNDG_HVEL_WARN_LOOSE_MS; alarmMs = LNDG_HVEL_ALARM_LOOSE_MS; }
    else if (tGround > LNDG_HVEL_T_MID_S)
        { warnMs = LNDG_HVEL_WARN_MID_MS;   alarmMs = LNDG_HVEL_ALARM_MID_MS;   }
    else if (tGround > LNDG_TGRND_ALARM_S)
        { warnMs = LNDG_HVEL_WARN_TIGHT_MS; alarmMs = LNDG_HVEL_ALARM_TIGHT_MS; }
    else
        { warnMs = LNDG_HVEL_WARN_FINAL_MS; alarmMs = LNDG_HVEL_ALARM_FINAL_MS; }
    if      (av >= alarmMs) { fg = TFT_WHITE;     bg = TFT_RED;   }
    else if (av >= warnMs)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
    else                    { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
}


/***************************************************************************************
   X-POINTER CHROME ELEMENTS
   Drawn at chrome time and re-applied after needle erase.
****************************************************************************************/
static void _lndgDrawXpChrome(RA8875 &tft) {
    uint16_t g5  = (uint16_t)(5.0f  * LNDG_XP_SCALE);
    uint16_t g10 = (uint16_t)(10.0f * LNDG_XP_SCALE);
    uint16_t g2  = (uint16_t)(2.0f  * LNDG_XP_SCALE);

    // Centre crosshairs (dim)
    tft.drawLine(LNDG_XP_CX, LNDG_XP_Y + 1,
                 LNDG_XP_CX, LNDG_XP_Y + LNDG_XP_SIDE - 1, LNDG_XP_GRID);
    tft.drawLine(LNDG_XP_X + 1, LNDG_XP_CY,
                 LNDG_XP_X + LNDG_XP_SIDE - 1, LNDG_XP_CY, LNDG_XP_GRID);

    // ±5 m/s dashed reference lines
    for (uint16_t y = LNDG_XP_Y + 2; y < LNDG_XP_Y + LNDG_XP_SIDE - 2; y += 8) {
        tft.drawPixel(LNDG_XP_CX - g5, y, LNDG_XP_GRID);
        tft.drawPixel(LNDG_XP_CX + g5, y, LNDG_XP_GRID);
    }
    for (uint16_t x = LNDG_XP_X + 2; x < LNDG_XP_X + LNDG_XP_SIDE - 2; x += 8) {
        tft.drawPixel(x, LNDG_XP_CY - g5, LNDG_XP_GRID);
        tft.drawPixel(x, LNDG_XP_CY + g5, LNDG_XP_GRID);
    }

    // ±10 m/s dashed reference lines
    for (uint16_t y = LNDG_XP_Y + 2; y < LNDG_XP_Y + LNDG_XP_SIDE - 2; y += 8) {
        tft.drawPixel(LNDG_XP_CX - g10, y, LNDG_XP_GRID);
        tft.drawPixel(LNDG_XP_CX + g10, y, LNDG_XP_GRID);
    }
    for (uint16_t x = LNDG_XP_X + 2; x < LNDG_XP_X + LNDG_XP_SIDE - 2; x += 8) {
        tft.drawPixel(x, LNDG_XP_CY - g10, LNDG_XP_GRID);
        tft.drawPixel(x, LNDG_XP_CY + g10, LNDG_XP_GRID);
    }

    // Inner ±2 m/s zone (slightly lighter fill)
    tft.fillRect(LNDG_XP_CX - g2, LNDG_XP_CY - g2, g2 * 2, g2 * 2, LNDG_XP_INNER);

    // Centre reference dot (fixed)
    tft.fillCircle(LNDG_XP_CX, LNDG_XP_CY, 5, TFT_BLACK);
    tft.fillCircle(LNDG_XP_CX, LNDG_XP_CY, 2, TFT_DARK_GREY);
}


// Low-altitude tape: covers 0-50m, same geometry as normal tape
static const float LNDG_TAPE_PPM_LOW = (float)LNDG_TAPE_H / 50.0f;

static void _lndgDrawTapeChrome(RA8875 &tft, bool lowAlt) {
    float ppm = lowAlt ? LNDG_TAPE_PPM_LOW : LNDG_TAPE_PPM;

    // Clear only the numeric label area (x=14..TAPE_X-1), preserving the ALTITUDE strip (x=0..13)
    tft.fillRect(14, LNDG_TAPE_Y, LNDG_TAPE_X - 14, LNDG_TAPE_H, TFT_BLACK);

    if (lowAlt) {
        // Low-alt: dark grey upper (unlit), light grey lower (lit) — draw function updates this
        // Chrome just paints the full tape dark grey as the baseline unlit state
        tft.fillRect(LNDG_TAPE_X, LNDG_TAPE_Y, LNDG_TAPE_W, LNDG_TAPE_H, TFT_DARK_GREY);
    } else {
        // Normal: three zone dim backgrounds
        uint16_t y200 = SCREEN_H - (uint16_t)(200.0f * ppm);
        uint16_t y50  = SCREEN_H - (uint16_t)( 50.0f * ppm);
        tft.fillRect(LNDG_TAPE_X, LNDG_TAPE_Y, LNDG_TAPE_W,
                     y200 - LNDG_TAPE_Y, TFT_JUNGLE);
        tft.fillRect(LNDG_TAPE_X, y200, LNDG_TAPE_W, y50 - y200,      TFT_OLIVE);
        tft.fillRect(LNDG_TAPE_X, y50,  LNDG_TAPE_W, SCREEN_H - y50,  TFT_DARK_RED);
    }

    // Ticks on both sides, inside border
    // Chrome paints the full tape as dim, so all ticks start light grey
    uint16_t tickCol = TFT_LIGHT_GREY;

    auto tapeTick = [&](uint16_t altM, uint8_t tickLen) {
        uint16_t ty = SCREEN_H - (uint16_t)(altM * ppm);
        if (ty < LNDG_TAPE_Y || ty >= (uint16_t)SCREEN_H) return;
        tft.drawLine(LNDG_TAPE_X + 1,                         ty,
                     LNDG_TAPE_X + tickLen,                    ty, tickCol);
        tft.drawLine(LNDG_TAPE_X + LNDG_TAPE_W - 1 - tickLen, ty,
                     LNDG_TAPE_X + LNDG_TAPE_W - 2,            ty, tickCol);
    };

    if (lowAlt) {
        // Major every 10m, minor every 5m
        for (uint16_t m = 10; m <= 50; m += 10) tapeTick(m, 8);
        for (uint16_t m =  5; m <= 45; m += 10) tapeTick(m, 4);
    } else {
        // Major every 100m, minor every 25m
        for (uint16_t m = 100; m <= 500; m += 100) tapeTick(m, 8);
        for (uint16_t m =  25; m <= 475; m +=  25)
            if (m % 100 != 0) tapeTick(m, 4);
    }

    tft.drawRect(LNDG_TAPE_X, LNDG_TAPE_Y, LNDG_TAPE_W, LNDG_TAPE_H, TFT_GREY);

    // Scale labels — right-aligned to x = LNDG_TAPE_X - 2
    // Normal: every 50m, range 50..450 (skip 0m=off-screen, 500m=clips top)
    // Low-alt: every 5m,  range 5..45  (skip 0m=off-screen, 50m=clips top)
    tft.setFont(&Roboto_Black_12);
    tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
    uint16_t labelStep  = lowAlt ? 5  : 50;
    uint16_t labelStart = lowAlt ? 0  : 0;    // include 0m (clamped to bottom)
    uint16_t labelEnd   = lowAlt ? 50 : 500;  // include 50m/500m (clamped to top)
    for (uint16_t m = labelStart; m <= labelEnd; m += labelStep) {
        char buf[5];  snprintf(buf, sizeof(buf), "%u", m);
        uint16_t lw = strlen(buf) * 7;
        uint16_t lx = LNDG_TAPE_X - 2 - lw;
        // 0m would give ty=SCREEN_H (off screen) — clamp to SCREEN_H-1
        uint16_t ty = (m == 0) ? (uint16_t)(SCREEN_H - 1)
                                : (uint16_t)(SCREEN_H - (uint16_t)(m * ppm));
        uint16_t ly = (uint16_t)max((int)LNDG_TAPE_Y, min((int)(SCREEN_H - 14), (int)ty - 7));
        tft.setCursor(lx, ly);
        tft.print(buf);
    }
    // Redraw ALTITUDE vertical label — must come after label area clear
    drawVerticalText(tft, 0, 219, 14, 112,
                     &Roboto_Black_12, "ALTITUDE", TFT_LIGHT_GREY, TFT_BLACK);
}


/***************************************************************************************
   ATTITUDE BULLSEYE — constants and state
   Field: 120×120px, centred horizontally under X-Pointer
   Scale: ±15° full deflection, 3 reference rings at 5°/10°/15°
****************************************************************************************/
static const uint16_t LNDG_ATT_X     = 111;   // field left edge
static const uint16_t LNDG_ATT_Y     = 362;   // field top edge (below XP tick/label rows)
static const uint16_t LNDG_ATT_SIDE  = 116;
static const uint16_t LNDG_ATT_CX    = LNDG_ATT_X + LNDG_ATT_SIDE / 2;  // 169
static const uint16_t LNDG_ATT_CY    = LNDG_ATT_Y + LNDG_ATT_SIDE / 2;  // 420
static const uint8_t  LNDG_ATT_R     = 52;    // outer ring radius (= ±15°)
static const float    LNDG_ATT_SCALE = (float)LNDG_ATT_R / 15.0f;

static int16_t _lndgPrevAttX = -999;
static int16_t _lndgPrevAttY = -999;


static void _lndgDrawAttChrome(RA8875 &tft) {
    // Black disc background (no bounding square)
    tft.fillCircle(LNDG_ATT_CX, LNDG_ATT_CY, LNDG_ATT_R + 2, TFT_BLACK);

    // Inner good-zone fill (subtle, inside ±5° ring)
    uint8_t r5  = (uint8_t)(5.0f  * LNDG_ATT_SCALE);
    uint8_t r10 = (uint8_t)(10.0f * LNDG_ATT_SCALE);
    tft.fillCircle(LNDG_ATT_CX, LNDG_ATT_CY, r5, TFT_OFF_BLACK);

    // Reference rings at 5°, 10°, 15°
    tft.drawCircle(LNDG_ATT_CX, LNDG_ATT_CY, r5,         TFT_DARK_GREEN);
    tft.drawCircle(LNDG_ATT_CX, LNDG_ATT_CY, r10,        TFT_DARK_GREY);
    tft.drawCircle(LNDG_ATT_CX, LNDG_ATT_CY, LNDG_ATT_R, TFT_GREY);

    // Centre crosshairs (cardinal lines with gap)
    uint8_t gap = 6, arm = LNDG_ATT_R - 1;
    tft.drawLine(LNDG_ATT_CX, LNDG_ATT_CY - arm, LNDG_ATT_CX, LNDG_ATT_CY - gap, TFT_DARK_GREY);
    tft.drawLine(LNDG_ATT_CX, LNDG_ATT_CY + gap, LNDG_ATT_CX, LNDG_ATT_CY + arm, TFT_DARK_GREY);
    tft.drawLine(LNDG_ATT_CX - arm, LNDG_ATT_CY, LNDG_ATT_CX - gap, LNDG_ATT_CY, TFT_DARK_GREY);
    tft.drawLine(LNDG_ATT_CX + gap, LNDG_ATT_CY, LNDG_ATT_CX + arm, LNDG_ATT_CY, TFT_DARK_GREY);

    // 30° tick marks on outer ring (skip cardinals 0/90/180/270)
    for (uint16_t deg = 0; deg < 360; deg += 30) {
        if (deg % 90 == 0) continue;
        float rad = (deg - 90) * DEG_TO_RAD;
        int16_t ox = LNDG_ATT_CX + (int16_t)(LNDG_ATT_R * cosf(rad));
        int16_t oy = LNDG_ATT_CY + (int16_t)(LNDG_ATT_R * sinf(rad));
        int16_t ix = LNDG_ATT_CX + (int16_t)((LNDG_ATT_R - 6) * cosf(rad));
        int16_t iy = LNDG_ATT_CY + (int16_t)((LNDG_ATT_R - 6) * sinf(rad));
        tft.drawLine(ox, oy, ix, iy, TFT_DARK_GREY);
    }

    // Bezel ring
    tft.drawCircle(LNDG_ATT_CX, LNDG_ATT_CY, LNDG_ATT_R,     TFT_GREY);
    tft.drawCircle(LNDG_ATT_CX, LNDG_ATT_CY, LNDG_ATT_R + 1, TFT_DARK_GREY);

    // Range label "15°" outside outer ring, lower-right — 68px from centre (ring edge at 53px)
    tft.setFont(&Roboto_Black_12);
    tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
    tft.setCursor(207, 461);
    tft.print("15\xb0");

    // "ATT" vertical label to the left of the circle, centred on bar height
    drawVerticalText(tft, 97, 363 + (116 - 42) / 2, 14, 42,
                     &Roboto_Black_12, "ATT", TFT_LIGHT_GREY, TFT_BLACK);
}


static void _lndgDrawAtt(RA8875 &tft) {
    // Deviation of nose from vertical in body frame.
    // tilt = total angular distance from vertical (0 = pointing straight up).
    // Decomposed by roll into screen axes:
    //   dot_y (up on screen  = pitch fwd) = -tilt * cos(roll)  [−ve: nose fwd → dot up]
    //   dot_x (right on screen = yaw right) = tilt * sin(roll)
    float tilt    = 90.0f - state.pitch;
    float rollRad = state.roll * DEG_TO_RAD;
    int16_t dotX = (int16_t)(LNDG_ATT_CX +  tilt * sinf(rollRad) * LNDG_ATT_SCALE);
    int16_t dotY = (int16_t)(LNDG_ATT_CY -  tilt * cosf(rollRad) * LNDG_ATT_SCALE);

    // Clamp to outer ring
    float dx = (float)(dotX - (int16_t)LNDG_ATT_CX);
    float dy = (float)(dotY - (int16_t)LNDG_ATT_CY);
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist > (float)LNDG_ATT_R) {
        dotX = (int16_t)(LNDG_ATT_CX + dx * LNDG_ATT_R / dist);
        dotY = (int16_t)(LNDG_ATT_CY + dy * LNDG_ATT_R / dist);
    }

    // Dot colour by tilt magnitude
    float absTilt = fabsf(tilt);
    uint16_t dotCol = (absTilt >= 5.0f) ? TFT_RED :
                      (absTilt >= 2.0f) ? TFT_YELLOW : TFT_DARK_GREEN;

    if (dotX != _lndgPrevAttX || dotY != _lndgPrevAttY) {
        // Erase old dot: fillCircle with black to restore background
        if (_lndgPrevAttX >= 0) {
            tft.fillCircle(_lndgPrevAttX, _lndgPrevAttY, 8, TFT_BLACK);
            _lndgDrawAttChrome(tft);  // restore rings/ticks erased by circle fill
        }

        // Draw new dot
        tft.fillCircle(dotX, dotY, 6, dotCol);
        tft.fillCircle(dotX, dotY, 2, TFT_WHITE);

        _lndgPrevAttX = dotX;
        _lndgPrevAttY = dotY;
    }
}



/***************************************************************************************
   V.VRT BAR — 0-15 m/s descent rate gauge
   Geometry: equal left/right margins (~27px) with distinct label zones
   x=304..334 bar, numeric labels right-align to x=293, "V.Vrt" strip x=250..264
   Colours: green <5, yellow 5-8, red >8 m/s (matches text panel thresholds)
****************************************************************************************/
static const uint16_t LNDG_VV_X    = 283;
static const uint16_t LNDG_VV_Y    = 363;
static const uint16_t LNDG_VV_W    = 30;
static const uint16_t LNDG_VV_H    = 116;
static const float    LNDG_VV_MAX  = 15.0f;
static const float    LNDG_VV_PPM  = (float)LNDG_VV_H / LNDG_VV_MAX;  // 7.73 px/m/s
static const uint16_t LNDG_VV_BOT  = LNDG_VV_Y + LNDG_VV_H;           // 479

static const uint16_t LNDG_VV_Y5   = LNDG_VV_BOT - (uint16_t)(5.0f  * LNDG_VV_PPM);
static const uint16_t LNDG_VV_Y8   = LNDG_VV_BOT - (uint16_t)(8.0f  * LNDG_VV_PPM);
static const uint16_t LNDG_VV_LBRT = 281;  // numeric labels right-align to this x

static float _lndgPrevVV = -9999.0f;


static void _lndgDrawVvChrome(RA8875 &tft) {
    // Dim zone backgrounds
    tft.fillRect(LNDG_VV_X, LNDG_VV_Y,  LNDG_VV_W, LNDG_VV_Y8 - LNDG_VV_Y,  TFT_DARK_RED);
    tft.fillRect(LNDG_VV_X, LNDG_VV_Y8, LNDG_VV_W, LNDG_VV_Y5 - LNDG_VV_Y8, TFT_OLIVE);
    tft.fillRect(LNDG_VV_X, LNDG_VV_Y5, LNDG_VV_W, LNDG_VV_BOT - LNDG_VV_Y5, TFT_JUNGLE);

    // Ticks both sides — major 8px every 5 m/s, minor 4px every 1 m/s
    auto vvTick = [&](uint16_t vMs, uint8_t tickLen) {
        uint16_t ty = LNDG_VV_BOT - (uint16_t)(vMs * LNDG_VV_PPM);
        if (ty < LNDG_VV_Y || ty > LNDG_VV_BOT) return;
        tft.drawLine(LNDG_VV_X + 1,                         ty,
                     LNDG_VV_X + tickLen,                    ty, TFT_LIGHT_GREY);
        tft.drawLine(LNDG_VV_X + LNDG_VV_W - 1 - tickLen,   ty,
                     LNDG_VV_X + LNDG_VV_W - 2,             ty, TFT_LIGHT_GREY);
    };
    for (uint16_t v = 0; v <= 15; v += 5) vvTick(v, 8);
    for (uint16_t v = 1; v <= 14; v++)
        if (v % 5 != 0) vvTick(v, 4);

    tft.drawRect(LNDG_VV_X, LNDG_VV_Y, LNDG_VV_W, LNDG_VV_H, TFT_GREY);

    // Numeric labels every 5 m/s, right-aligned
    tft.setFont(&Roboto_Black_12);
    tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
    for (uint16_t v = 0; v <= 15; v += 5) {
        char buf[4]; snprintf(buf, sizeof(buf), "%u", v);
        uint16_t lw = strlen(buf) * 7;
        uint16_t ty = LNDG_VV_BOT - (uint16_t)(v * LNDG_VV_PPM);
        uint16_t ly = (uint16_t)max((int)LNDG_VV_Y, min((int)(SCREEN_H - 14), (int)ty - 7));
        tft.setCursor(LNDG_VV_LBRT - lw, ly);
        tft.print(buf);
    }

    // "V.Vrt" vertical label — centred on bar height, 15px gap left of numeric labels
    drawVerticalText(tft, 250, LNDG_VV_Y + (LNDG_VV_H - 70) / 2, 14, 70,
                     &Roboto_Black_12, "V.Vrt", TFT_LIGHT_GREY, TFT_BLACK);
}


static void _lndgDrawVv(RA8875 &tft) {
    float vv     = constrain(-state.verticalVel, 0.0f, LNDG_VV_MAX);
    uint16_t fillH = (uint16_t)(vv * LNDG_VV_PPM);
    uint16_t fillY = LNDG_VV_BOT - fillH;

    if (fabsf(vv - _lndgPrevVV) < 0.1f) return;
    _lndgPrevVV = vv;

    uint16_t tx = LNDG_VV_X + 1, tw = LNDG_VV_W - 2;

    // Full repaint: dim zones
    tft.fillRect(tx, LNDG_VV_Y + 1,   tw, LNDG_VV_Y8 - LNDG_VV_Y - 1,  TFT_DARK_RED);
    tft.fillRect(tx, LNDG_VV_Y8,       tw, LNDG_VV_Y5 - LNDG_VV_Y8,      TFT_OLIVE);
    tft.fillRect(tx, LNDG_VV_Y5,       tw, LNDG_VV_BOT - LNDG_VV_Y5 - 1, TFT_JUNGLE);

    // Bright fill zone-split from bottom up
    if (fillH > 0) {
        auto seg = [&](uint16_t top, uint16_t bot, uint16_t col) {
            top = (uint16_t)max((int)fillY,                (int)top);
            bot = (uint16_t)min((int)(LNDG_VV_BOT - 1),   (int)bot);
            if (top < bot) tft.fillRect(tx, top, tw, bot - top, col);
        };
        seg(LNDG_VV_Y + 1, LNDG_VV_Y8, TFT_RED);
        seg(LNDG_VV_Y8,    LNDG_VV_Y5, TFT_YELLOW);
        seg(LNDG_VV_Y5,    LNDG_VV_BOT, TFT_NEON_GREEN);
    }

    // Zone-aware ticks
    auto vvTick = [&](uint16_t vMs, uint8_t tickLen) {
        uint16_t ty  = LNDG_VV_BOT - (uint16_t)(vMs * LNDG_VV_PPM);
        uint16_t col = (ty >= fillY) ? TFT_DARK_GREY : TFT_LIGHT_GREY;
        tft.drawLine(LNDG_VV_X + 1,                         ty,
                     LNDG_VV_X + tickLen,                    ty, col);
        tft.drawLine(LNDG_VV_X + LNDG_VV_W - 1 - tickLen,   ty,
                     LNDG_VV_X + LNDG_VV_W - 2,             ty, col);
    };
    for (uint16_t v = 0; v <= 15; v += 5) vvTick(v, 8);
    for (uint16_t v = 1; v <= 14; v++)
        if (v % 5 != 0) vvTick(v, 4);

    // 3px white marker
    if (fillH > 0 && fillY > LNDG_VV_Y && fillY < LNDG_VV_BOT)
        tft.fillRect(LNDG_VV_X, fillY, LNDG_VV_W, 3, TFT_WHITE);

    tft.drawRect(LNDG_VV_X, LNDG_VV_Y, LNDG_VV_W, LNDG_VV_H, TFT_GREY);
}



/***************************************************************************************
   CHROME: POWERED DESCENT
****************************************************************************************/
static void _lndgChromePowered(RA8875 &tft) {



    // ── Altitude tape chrome ──
    _lndgDrawTapeChrome(tft, _lndgLowAltMode);

    tft.fillRect(LNDG_XP_X, LNDG_XP_Y, LNDG_XP_SIDE, LNDG_XP_SIDE, LNDG_XP_BG);
    _lndgDrawXpChrome(tft);
    tft.drawRect(LNDG_XP_X, LNDG_XP_Y, LNDG_XP_SIDE, LNDG_XP_SIDE, TFT_GREY);


    // ── Perimeter tick marks and labels outside XP boundary ──
    // Left edge  = Fwd/Aft axis (positive = forward = up)
    // Bottom edge = Lateral axis (positive = right)
    // Major ticks every 5 m/s (5px), minor every 1 m/s (2px)
    // Numeric labels every 5 m/s; axis name labels on both axes
    {
        static const uint8_t  MAJ = 5, MIN = 2;
        static const uint16_t COL = TFT_LIGHT_GREY;
        uint16_t botY = LNDG_XP_Y + LNDG_XP_SIDE;

        // ── FWD / AFT axis name labels — vertical strips on left ──
        // x=80..94: 10px gap from tape right (70), 4px gap to numeric labels (x=98)
        // FWD: top-aligned with XP top edge
        // AFT: bottom-aligned with XP bottom edge
        drawVerticalText(tft, 80, LNDG_XP_Y,      14, 42,
                         &Roboto_Black_12, "FWD", TFT_LIGHT_GREY, TFT_BLACK);
        drawVerticalText(tft, 80, botY - 42,        14, 42,
                         &Roboto_Black_12, "AFT", TFT_LIGHT_GREY, TFT_BLACK);

        // ── LATERAL axis label + L/R — all on the same row below bottom axis ──
        // L: left-aligned with XP left edge
        // LATERAL: centred on XP
        // R: right-aligned with XP right edge
        tft.setFont(&Roboto_Black_12);
        tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
        tft.setCursor(LNDG_XP_X, botY + 24);                            tft.print("L");
        tft.setCursor(LNDG_XP_CX - 24, botY + 24);                      tft.print("LATERAL");
        tft.setCursor(LNDG_XP_X + LNDG_XP_SIDE - 7, botY + 24);        tft.print("R");

        for (int8_t v = -15; v <= 15; v++) {
            if (v == 0) continue;
            bool isMajor = (v % 5 == 0);
            uint8_t tlen = isMajor ? MAJ : MIN;

            // Left edge tick (fwd/aft): positive v = up
            int16_t ty = (int16_t)(LNDG_XP_CY - v * LNDG_XP_SCALE);
            if (ty >= LNDG_XP_Y && ty <= botY) {
                tft.drawLine(LNDG_XP_X - 1,       ty,
                             LNDG_XP_X - 1 - tlen, ty, COL);
            }

            // Bottom edge tick (lat): positive v = right
            int16_t tx = (int16_t)(LNDG_XP_CX + v * LNDG_XP_SCALE);
            if (tx >= LNDG_XP_X && tx <= LNDG_XP_X + LNDG_XP_SIDE) {
                tft.drawLine(tx, botY + 1,
                             tx, botY + 1 + tlen, COL);
            }

            // Numeric labels every 5 m/s
            if (isMajor) {
                char buf[4];
                snprintf(buf, sizeof(buf), "%d", abs(v));
                uint8_t  nch = strlen(buf);
                uint16_t lw  = nch * 7;

                // Left axis: right-aligned, leaving room for vertical axis label strip
                uint16_t lx = LNDG_XP_X - 8 - lw;
                uint16_t ly = (uint16_t)max((int16_t)LNDG_XP_Y, (int16_t)(ty - 7));
                tft.setFont(&Roboto_Black_12);
                tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
                tft.setCursor(lx, ly);
                tft.print(buf);

                // Bottom axis: centred on tick
                uint16_t blx = (uint16_t)max(0, (int)(tx - (int)(lw / 2)));
                tft.setCursor(blx, botY + 8);
                tft.print(buf);
            }
        }

        // Zero tick + label on left axis
        tft.drawLine(LNDG_XP_X - 1, LNDG_XP_CY,
                     LNDG_XP_X - 1 - MAJ, LNDG_XP_CY, COL);
        tft.setFont(&Roboto_Black_12);
        tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
        tft.setCursor(LNDG_XP_X - 8 - 7, LNDG_XP_CY - 7);
        tft.print("0");

        // Zero tick + label on bottom axis
        tft.drawLine(LNDG_XP_CX, botY + 1,
                     LNDG_XP_CX, botY + 1 + MAJ, COL);
        tft.setCursor(LNDG_XP_CX - 3, botY + 8);
        tft.print("0");
    }

    // ── Instrument title labels ──
    // "SURF DRIFT" — horizontal, centred above the XP field
    tft.setFont(&Roboto_Black_12);
    tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
    tft.setCursor(200, 70);
    tft.print("SURF DRIFT");

    // ── Right panel chrome ──
    // 8 rows using Roboto_Black_20 labels, Roboto_Black_28 values
    // x=360..720, y=TITLE_TOP..SCREEN_H
    {
        static const tFont   *RF  = &Roboto_Black_28;
        static const tFont   *RCF = &Roboto_Black_20;
        static const uint16_t RX  = 364;
        static const uint16_t RW  = CONTENT_W - RX;           // 360px
        static const uint8_t  RNR = 8;
        static const uint16_t RHW = RW / 2;                   // 180px — half width for split rows

        printDispChrome(tft, RCF, RX, rowYFor(0,RNR), RW,  rowHFor(RNR), "V.Vrt:",   COL_LABEL, COL_BACK, COL_NO_BDR);
        printDispChrome(tft, RCF, RX, rowYFor(1,RNR), RW,  rowHFor(RNR), "T.Grnd:",  COL_LABEL, COL_BACK, COL_NO_BDR);
        printDispChrome(tft, RCF, RX, rowYFor(2,RNR), RW,  rowHFor(RNR), "Alt.Rdr:", COL_LABEL, COL_BACK, COL_NO_BDR);
        printDispChrome(tft, RCF, RX, rowYFor(3,RNR), RW,  rowHFor(RNR), "V.Srf:",   COL_LABEL, COL_BACK, COL_NO_BDR);

        // Row 4: Fwd | Lat split
        {
            uint16_t y = rowYFor(4, RNR), h = rowHFor(RNR);
            printDispChrome(tft, RCF, RX,        y, RHW - ROW_PAD, h, "Fwd:", COL_LABEL, COL_BACK, COL_NO_BDR);
            printDispChrome(tft, RCF, RX + RHW,  y, RHW - ROW_PAD, h, "Lat:", COL_LABEL, COL_BACK, COL_NO_BDR);
            tft.drawLine(RX + RHW,     y, RX + RHW,     rowYFor(5,RNR) - 1, TFT_GREY);
            tft.drawLine(RX + RHW + 1, y, RX + RHW + 1, rowYFor(5,RNR) - 1, TFT_GREY);
        }

        printDispChrome(tft, RCF, RX, rowYFor(5,RNR), RW, rowHFor(RNR), "\xCE\x94V.Stg:", COL_LABEL, COL_BACK, COL_NO_BDR);

        // Divider before VEH section
        uint16_t divY = rowYFor(6, RNR) - 1;
        // (VEH section horizontal divider kept, label omitted — sits in graphical area)
        tft.drawLine(360, divY,   CONTENT_W, divY,   TFT_GREY);
        tft.drawLine(360, divY+1, CONTENT_W, divY+1, TFT_GREY);

        // ── All 2px borders ──
        // VERTICAL: graphical | text section divider (x=360..361, full height)
        tft.drawLine(360, TITLE_TOP, 360, SCREEN_H - 1, TFT_GREY);
        tft.drawLine(361, TITLE_TOP, 361, SCREEN_H - 1, TFT_GREY);

        // Horizontal borders drawn in draw function (after value updates) to avoid being overwritten

        // Row 6: Throttle | RCS split
        {
            uint16_t y = rowYFor(6, RNR), h = rowHFor(RNR);
            printDispChrome(tft, RCF, RX,        y, RHW - ROW_PAD, h, "Thrtl:", COL_LABEL, COL_BACK, COL_NO_BDR);
            printDispChrome(tft, RCF, RX + RHW,  y, RHW - ROW_PAD, h, "RCS:",   COL_LABEL, COL_BACK, COL_NO_BDR);
            tft.drawLine(RX + RHW,     y, RX + RHW,     rowYFor(7,RNR) - 1, TFT_GREY);
            tft.drawLine(RX + RHW + 1, y, RX + RHW + 1, rowYFor(7,RNR) - 1, TFT_GREY);
        }

        // Row 7: Gear | SAS split
        {
            uint16_t y = rowYFor(7, RNR), h = rowHFor(RNR);
            printDispChrome(tft, RCF, RX,        y, RHW - ROW_PAD, h, "Gear:", COL_LABEL, COL_BACK, COL_NO_BDR);
            printDispChrome(tft, RCF, RX + RHW,  y, RHW - ROW_PAD, h, "SAS:",  COL_LABEL, COL_BACK, COL_NO_BDR);
            tft.drawLine(RX + RHW,     y, RX + RHW,     SCREEN_H - 1, TFT_GREY);
            tft.drawLine(RX + RHW + 1, y, RX + RHW + 1, SCREEN_H - 1, TFT_GREY);
        }
    }

    // Attitude bullseye
    _lndgDrawAttChrome(tft);

    // Redraw LATERAL row labels — bullseye fillCircle may overdraw them
    {
        uint16_t botY = LNDG_XP_Y + LNDG_XP_SIDE;
        tft.setFont(&Roboto_Black_12);
        tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
        tft.setCursor(LNDG_XP_X,                           botY + 24); tft.print("L");
        tft.setCursor(LNDG_XP_CX - 24,                     botY + 24); tft.print("LATERAL");
        tft.setCursor(LNDG_XP_X + LNDG_XP_SIDE - 7,        botY + 24); tft.print("R");
    }

    // V.Vrt bar
    _lndgDrawVvChrome(tft);

    // Reset dirty flags — use SCREEN_H+1 as sentinel (invalid y, forces full first draw)
    _lndgPrevLatX   = -999;
    _lndgPrevFwdY   = -999;
    _lndgPrevAlt    = (float)(SCREEN_H + 1);
    _lndgPrevAttX   = -999;
    _lndgPrevAttY   = -999;
    _lndgPrevVV     = -9999.0f;
    _lndgLowAltMode = (state.radarAlt < 50.0f);
    // Clear right panel row caches so values redraw on first update
    for (uint8_t r = 0; r <= 10; r++) rowCache[6][r].value = "\x01";
}


/***************************************************************************************
   CHROME: RE-ENTRY (text-only, unchanged from original)
****************************************************************************************/
static void _lndgChromeReentry(RA8875 &tft) {
    static const tFont   *F      = &Roboto_Black_40;
    static const uint8_t  NR     = 8;
    static const uint16_t SECT_W = 26;
    static const uint16_t AX     = ROW_PAD + SECT_W;
    static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
    static const uint16_t AHW    = AW / 2;
    uint16_t rowH = rowHFor(NR);

    drawVerticalText(tft, 0, TITLE_TOP, SECT_W, rowH*5,
                     &Roboto_Black_16, "STATE", TFT_LIGHT_GREY, TFT_BLACK);

    const char *row0Label = _lndgReentryRow0TPe ? "T+Atm:" : "T.Grnd:";
    const char *row1Label = _lndgReentryRow1SL  ? "Alt.SL:" : "Alt.Rdr:";
    printDispChrome(tft, F, AX, rowYFor(0,NR), AW, rowH, row0Label,  COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(1,NR), AW, rowH, row1Label,  COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(2,NR), AW, rowH, "V.Vrt:",   COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(3,NR), AW, rowH,
                    _lndgReentryRow3PeA ? "PeA:" : "V.Hrz:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(4,NR), AW, rowH, "V.Srf:", COL_LABEL, COL_BACK, COL_NO_BDR);

    drawVerticalText(tft, 0, TITLE_TOP + rowH*6 + 2, SECT_W, rowH*2 - 4,
                     &Roboto_Black_16, "VEH", TFT_LIGHT_GREY, TFT_BLACK);

    auto halfRow = [&](uint8_t row, const char *lbl, const char *rbl) {
        uint16_t y = rowYFor(row, NR), h = rowH;
        printDispChrome(tft, F, AX,             y, AHW-ROW_PAD, h, lbl, COL_LABEL, COL_BACK, COL_NO_BDR);
        printDispChrome(tft, F, AX+AHW+ROW_PAD, y, AHW-ROW_PAD, h, rbl, COL_LABEL, COL_BACK, COL_NO_BDR);
        for (int8_t dx = -1; dx <= 1; dx++)
            tft.drawLine(AX+AHW+dx, y, AX+AHW+dx, rowYFor(row+1,NR)-1, TFT_GREY);
    };
    halfRow(5, "Mach:", "G:");
    halfRow(6, "Drogue:", "Main:");
    halfRow(7, "Gear:", "SAS:");

    uint16_t d1 = TITLE_TOP + rowH*5 + 1;
    uint16_t d2 = TITLE_TOP + rowH*6 + 1;
    tft.drawLine(0, d1,   CONTENT_W, d1,   TFT_GREY);
    tft.drawLine(0, d1+1, CONTENT_W, d1+1, TFT_GREY);
    tft.drawLine(0, d2,   CONTENT_W, d2,   TFT_GREY);
    tft.drawLine(0, d2+1, CONTENT_W, d2+1, TFT_GREY);

    drawVerticalText(tft, 0, TITLE_TOP + rowH*5 + 2, SECT_W, rowH - 4,
                     &Roboto_Black_16, "AT", TFT_LIGHT_GREY, TFT_BLACK);
}


static void chromeScreen_LNDG(RA8875 &tft) {
    if (!_lndgReentryMode) _lndgChromePowered(tft);
    else                   _lndgChromeReentry(tft);
}


/***************************************************************************************
   DRAW: POWERED DESCENT
****************************************************************************************/
static void _lndgDrawPowered(RA8875 &tft) {

    // Shared precompute
    bool inOrbitOrEscape = (state.situation == sit_Orbit || state.situation == sit_Escaping);
    float tGround = (!inOrbitOrEscape && state.verticalVel < -0.05f && state.radarAlt > 0.0f)
                    ? fabsf(state.radarAlt / state.verticalVel) : -1.0f;
    float vSq  = state.surfaceVel * state.surfaceVel - state.verticalVel * state.verticalVel;
    float hSpd = (vSq > 0.0f) ? sqrtf(vSq) : 0.0f;

    float headRad    = (state.heading + state.roll) * 0.017453f;
    float svelHdgRad = state.srfVelHeading * 0.017453f;
    float dHeadRad   = svelHdgRad - headRad;
    float vFwd = -hSpd * cosf(dHeadRad);
    float vLat = -hSpd * sinf(dHeadRad);
    if (fabsf(vFwd) < 0.05f) vFwd = 0.0f;
    if (fabsf(vLat) < 0.05f) vLat = 0.0f;

    // ── X-Pointer needles ──
    float latClamped = constrain(vLat, -15.0f, 15.0f);
    float fwdClamped = constrain(vFwd, -15.0f, 15.0f);
    int16_t latX = (int16_t)(LNDG_XP_CX + latClamped * LNDG_XP_SCALE);
    int16_t fwdY = (int16_t)(LNDG_XP_CY - fwdClamped * LNDG_XP_SCALE);

    // Needle colour: dark green=safe, yellow=warn, red=alarm
    float driftMag = sqrtf(vLat * vLat + vFwd * vFwd) / 1.414f;
    uint16_t needleFg, needleBg;
    _lndgHorzColor(driftMag, tGround, needleFg, needleBg);
    uint16_t needleCol = (needleBg == TFT_RED) ? TFT_RED :
                         (needleFg == TFT_YELLOW) ? TFT_YELLOW : TFT_DARK_GREEN;

    if (latX != _lndgPrevLatX || fwdY != _lndgPrevFwdY) {
        // Erase old needles — 3px wide lines + arrowhead bounding boxes + circle
        if (_lndgPrevLatX >= 0) {
            tft.fillRect(_lndgPrevLatX - 1, LNDG_XP_Y + 1,
                         3, LNDG_XP_SIDE - 2, LNDG_XP_BG);
            tft.fillRect(_lndgPrevLatX - 6, LNDG_XP_Y + 1,
                         13, 18, LNDG_XP_BG);
            tft.fillRect(_lndgPrevLatX - 6, LNDG_XP_Y + LNDG_XP_SIDE - 18,
                         13, 18, LNDG_XP_BG);
        }
        if (_lndgPrevFwdY >= 0) {
            tft.fillRect(LNDG_XP_X + 1, _lndgPrevFwdY - 1,
                         LNDG_XP_SIDE - 2, 3, LNDG_XP_BG);
            tft.fillRect(LNDG_XP_X + 1,              _lndgPrevFwdY - 6,
                         18, 13, LNDG_XP_BG);
            tft.fillRect(LNDG_XP_X + LNDG_XP_SIDE - 18, _lndgPrevFwdY - 6,
                         18, 13, LNDG_XP_BG);
        }
        // Erase old intersection circle (15×15 box)
        if (_lndgPrevLatX >= 0 && _lndgPrevFwdY >= 0) {
            tft.fillRect(_lndgPrevLatX - 7, _lndgPrevFwdY - 7,
                         15, 15, LNDG_XP_BG);
        }

        // Restore chrome elements erased above
        _lndgDrawXpChrome(tft);

        // Draw new needles — 3px thick, clamped 2px inside border
        uint16_t nx1 = LNDG_XP_X + 2, nx2 = LNDG_XP_X + LNDG_XP_SIDE - 2;
        uint16_t ny1 = LNDG_XP_Y + 2, ny2 = LNDG_XP_Y + LNDG_XP_SIDE - 2;
        for (int8_t d = -1; d <= 1; d++) {
            int16_t lx = latX + d;
            if (lx >= (int16_t)nx1 && lx <= (int16_t)nx2)
                tft.drawLine(lx, ny1, lx, ny2, needleCol);
            int16_t fy = fwdY + d;
            if (fy >= (int16_t)ny1 && fy <= (int16_t)ny2)
                tft.drawLine(nx1, fy, nx2, fy, needleCol);
        }

        // Arrowheads — lateral needle (top + bottom)
        tft.fillTriangle(latX,     LNDG_XP_Y + 4,
                         latX - 6, LNDG_XP_Y + 16,
                         latX + 6, LNDG_XP_Y + 16, needleCol);
        tft.fillTriangle(latX,     LNDG_XP_Y + LNDG_XP_SIDE - 4,
                         latX - 6, LNDG_XP_Y + LNDG_XP_SIDE - 16,
                         latX + 6, LNDG_XP_Y + LNDG_XP_SIDE - 16, needleCol);
        // Arrowheads — fwd needle (left + right)
        tft.fillTriangle(LNDG_XP_X + 4,              fwdY,
                         LNDG_XP_X + 16,             fwdY - 6,
                         LNDG_XP_X + 16,             fwdY + 6, needleCol);
        tft.fillTriangle(LNDG_XP_X + LNDG_XP_SIDE - 4,  fwdY,
                         LNDG_XP_X + LNDG_XP_SIDE - 16, fwdY - 6,
                         LNDG_XP_X + LNDG_XP_SIDE - 16, fwdY + 6, needleCol);

        // Intersection circle at needle crossing — solid fill, white centre dot
        tft.fillCircle(latX, fwdY, 7, needleCol);
        tft.fillCircle(latX, fwdY, 3, TFT_WHITE);

        // Redraw border on top — needles and arrowheads may touch it
        tft.drawRect(LNDG_XP_X, LNDG_XP_Y, LNDG_XP_SIDE, LNDG_XP_SIDE, TFT_GREY);

        // Centre reference dot always on top
        tft.fillCircle(LNDG_XP_CX, LNDG_XP_CY, 5, TFT_BLACK);
        tft.fillCircle(LNDG_XP_CX, LNDG_XP_CY, 2, TFT_DARK_GREY);

        _lndgPrevLatX = latX;
        _lndgPrevFwdY = fwdY;
    }


    // ── Altitude tape — incremental strip update ──
    // Detect low-alt mode transition (hysteresis: enter <50m, exit >55m)
    float rawAlt = state.radarAlt;
    bool wantLow = _lndgLowAltMode ? (rawAlt < 55.0f) : (rawAlt < 50.0f);
    if (wantLow != _lndgLowAltMode) {
        _lndgLowAltMode = wantLow;
        _lndgPrevAlt    = (float)(SCREEN_H + 1);  // force full redraw
        _lndgDrawTapeChrome(tft, _lndgLowAltMode);
    }

    bool   lowAlt = _lndgLowAltMode;
    float  ppm    = lowAlt ? LNDG_TAPE_PPM_LOW : LNDG_TAPE_PPM;
    float  alt    = constrain(rawAlt, 0.0f, lowAlt ? 50.0f : 500.0f);
    uint16_t newFillY = SCREEN_H - (uint16_t)(alt * ppm);

    if (newFillY != (uint16_t)_lndgPrevAlt) {
        uint16_t prevFillY = (uint16_t)_lndgPrevAlt;
        uint16_t tx = LNDG_TAPE_X + 1, tw = LNDG_TAPE_W - 2;
        static const uint8_t MRK = 5;

        uint16_t stripTop = min(newFillY, prevFillY);
        uint16_t stripBot = max(newFillY, prevFillY) + MRK;
        stripTop = max(stripTop, (uint16_t)(LNDG_TAPE_Y + 1));
        stripBot = min(stripBot, (uint16_t)(SCREEN_H - 1));

        if (lowAlt) {
            // Low-alt: dark grey = unlit (above marker), light grey = lit (below marker)
            uint16_t dimTop = stripTop;
            uint16_t dimBot = min(newFillY, stripBot);
            uint16_t litTop = max(newFillY, stripTop);
            uint16_t litBot = stripBot;
            if (dimTop < dimBot) tft.fillRect(tx, dimTop, tw, dimBot - dimTop, TFT_DARK_GREY);
            if (litTop < litBot) tft.fillRect(tx, litTop, tw, litBot - litTop, TFT_LIGHT_GREY);

            // Ticks — light grey in dim (upper) zone, dark grey in lit (lower) zone
            auto tapeTick = [&](uint16_t altM, uint8_t tickLen) {
                uint16_t ty = SCREEN_H - (uint16_t)(altM * ppm);
                if (ty < stripTop || ty > stripBot) return;
                uint16_t col = (ty < newFillY) ? TFT_LIGHT_GREY : TFT_DARK_GREY;
                tft.drawLine(LNDG_TAPE_X + 1,                         ty,
                             LNDG_TAPE_X + tickLen,                    ty, col);
                tft.drawLine(LNDG_TAPE_X + LNDG_TAPE_W - 1 - tickLen, ty,
                             LNDG_TAPE_X + LNDG_TAPE_W - 2,            ty, col);
            };
            for (uint16_t m = 10; m <= 50; m += 10) tapeTick(m, 8);
            for (uint16_t m =  5; m <= 45; m += 10) tapeTick(m, 4);

        } else {
            // Normal mode: three-zone dim/bright split
            uint16_t y200 = SCREEN_H - (uint16_t)(200.0f * ppm);
            uint16_t y50  = SCREEN_H - (uint16_t)( 50.0f * ppm);

            auto zoneCol = [&](uint16_t y, bool bright) -> uint16_t {
                if (bright) return (y < y200) ? TFT_NEON_GREEN : (y < y50) ? TFT_YELLOW : TFT_RED;
                else        return (y < y200) ? TFT_JUNGLE      : (y < y50) ? TFT_OLIVE  : TFT_DARK_RED;
            };

            uint16_t boundaries[] = {stripTop, y200, y50, stripBot};
            for (int i = 0; i < 3; i++) {
                uint16_t segTop = max(boundaries[i],   stripTop);
                uint16_t segBot = min(boundaries[i+1], stripBot);
                if (segTop >= segBot) continue;
                bool segBright = (segTop >= newFillY);
                tft.fillRect(tx, segTop, tw, segBot - segTop, zoneCol(segTop, segBright));
            }

            auto tapeTick = [&](uint16_t altM, uint8_t tickLen) {
                uint16_t ty = SCREEN_H - (uint16_t)(altM * ppm);
                if (ty < stripTop || ty > stripBot) return;
                uint16_t col = (ty < newFillY) ? TFT_LIGHT_GREY : TFT_DARK_GREY;
                tft.drawLine(LNDG_TAPE_X + 1,                         ty,
                             LNDG_TAPE_X + tickLen,                    ty, col);
                tft.drawLine(LNDG_TAPE_X + LNDG_TAPE_W - 1 - tickLen, ty,
                             LNDG_TAPE_X + LNDG_TAPE_W - 2,            ty, col);
            };
            for (uint16_t m = 100; m <= 500; m += 100) tapeTick(m, 8);
            for (uint16_t m =  25; m <= 475; m +=  25)
                if (m % 100 != 0) tapeTick(m, 4);
        }

        // Marker: 5px bar, inside borders
        // Low-alt: RED marker  Normal: WHITE marker
        uint16_t mrkCol = lowAlt ? TFT_RED : TFT_WHITE;
        tft.fillRect(LNDG_TAPE_X + 1, newFillY, LNDG_TAPE_W - 2, MRK, mrkCol);

        _lndgPrevAlt = (float)newFillY;
    }

    // Attitude bullseye + V.Vrt bar
    _lndgDrawAtt(tft);
    _lndgDrawVv(tft);

    // ── Right panel values ──
    {
        static const tFont   *RF  = &Roboto_Black_28;
        static const uint16_t RX  = 364;
        static const uint16_t RW  = CONTENT_W - RX;
        static const uint8_t  RNR = 8;
        static const uint16_t RHW = RW / 2;

        // Shared precompute (some already done above for XP — recompute cleanly here)
        bool   inOrb    = (state.situation == sit_Orbit || state.situation == sit_Escaping);
        float  tGround  = (!inOrb && state.verticalVel < -0.05f && state.radarAlt > 0.0f)
                          ? fabsf(state.radarAlt / state.verticalVel) : -1.0f;
        float  vSq2     = state.surfaceVel * state.surfaceVel - state.verticalVel * state.verticalVel;
        float  hSpd2    = (vSq2 > 0.0f) ? sqrtf(vSq2) : 0.0f;
        float  headRad2 = (state.heading + state.roll) * 0.017453f;
        float  svelRad2 = state.srfVelHeading * 0.017453f;
        float  dHR2     = svelRad2 - headRad2;
        float  vFwd2    = -hSpd2 * cosf(dHR2);
        float  vLat2    = -hSpd2 * sinf(dHR2);
        if (fabsf(vFwd2) < 0.05f) vFwd2 = 0.0f;
        if (fabsf(vLat2) < 0.05f) vLat2 = 0.0f;

        uint16_t fg, bg;
        char buf[16];

        // Helper: draw a right-panel value with row-cache
        auto rpVal = [&](uint8_t row, const char *label, const String &val,
                         uint16_t fgc, uint16_t bgc, uint8_t cacheIdx) {
            RowCache &rc = rowCache[6][cacheIdx];
            if (rc.value == val && rc.fg == fgc && rc.bg == bgc) return;
            printValue(tft, RF, RX, rowYFor(row,RNR), RW, rowHFor(RNR),
                       label, val, fgc, bgc, COL_BACK, printState[6][cacheIdx]);
            rc.value = val; rc.fg = fgc; rc.bg = bgc;
        };

        // Row 0: V.Vrt
        {
            fg = (state.verticalVel < LNDG_VVRT_ALARM_MS) ? TFT_WHITE  :
                 (state.verticalVel < LNDG_VVRT_WARN_MS)  ? TFT_YELLOW : TFT_DARK_GREEN;
            bg = (state.verticalVel < LNDG_VVRT_ALARM_MS) ? TFT_RED    : TFT_BLACK;
            rpVal(0, "V.Vrt:", fmtMs(state.verticalVel), fg, bg, 0);
        }

        // Row 1: T.Grnd
        {
            if (tGround >= 0.0f) {
                fg = (tGround < LNDG_TGRND_ALARM_S) ? TFT_WHITE  :
                     (tGround < LNDG_TGRND_WARN_S)  ? TFT_YELLOW : TFT_DARK_GREEN;
                bg = (tGround < LNDG_TGRND_ALARM_S) ? TFT_RED    : TFT_BLACK;
                rpVal(1, "T.Grnd:", formatTime(tGround), fg, bg, 1);
            } else {
                rpVal(1, "T.Grnd:", "---", TFT_DARK_GREY, TFT_BLACK, 1);
            }
        }

        // Row 2: Alt.Rdr
        {
            fg = (state.radarAlt < ALT_RDR_ALARM_M) ? TFT_WHITE  :
                 (state.radarAlt < 200.0f)          ? TFT_YELLOW : TFT_DARK_GREEN;
            bg = (state.radarAlt < ALT_RDR_ALARM_M) ? TFT_RED    : TFT_BLACK;
            rpVal(2, "Alt.Rdr:", formatAlt(state.radarAlt), fg, bg, 2);
        }

        // Row 3: V.Srf
        rpVal(3, "V.Srf:", fmtMs(state.surfaceVel), TFT_DARK_GREEN, TFT_BLACK, 3);

        // Row 4: Fwd | Lat (split)
        {
            uint16_t ffg, fbg, lfg, lbg;
            _lndgHorzColor(vFwd2, tGround, ffg, fbg);
            _lndgHorzColor(vLat2, tGround, lfg, lbg);
            uint16_t y = rowYFor(4,RNR), h = rowHFor(RNR);
            // Always show sign. One decimal for |v|<10, integer for |v|>=10.
            // Roboto_Black_24 used for value font — fits "+99.9 m/s" in split column.
            static const tFont *RFS = &Roboto_Black_24;
            // Clamp to ±99.9 m/s so fmtMs always fits in the split column
            float vFwdC = constrain(vFwd2, -99.9f, 99.9f);
            float vLatC = constrain(vLat2, -99.9f, 99.9f);
            {
                String fs = fmtMs(vFwdC);
                RowCache &fc = rowCache[6][4];
                if (fc.value != fs || fc.fg != ffg || fc.bg != fbg) {
                    printValue(tft, RFS, RX, y, RHW-ROW_PAD, h, "Fwd:", fs, ffg, fbg, COL_BACK, printState[6][4]);
                    fc.value = fs; fc.fg = ffg; fc.bg = fbg;
                }
            }
            {
                String ls = fmtMs(vLatC);
                RowCache &lc = rowCache[6][5];
                if (lc.value != ls || lc.fg != lfg || lc.bg != lbg) {
                    printValue(tft, RFS, RX+RHW, y, RHW-ROW_PAD, h, "Lat:", ls, lfg, lbg, COL_BACK, printState[6][5]);
                    lc.value = ls; lc.fg = lfg; lc.bg = lbg;
                }
            }
        }

        // Row 5: ΔV.Stg
        {
            snprintf(buf, sizeof(buf), "%.0f", state.stageDeltaV);
            rpVal(5, "\xCE\x94V.Stg:", String(buf) + "m/s", TFT_DARK_GREEN, TFT_BLACK, 6);
        }

        // Row 6: Throttle | RCS (split)
        {
            snprintf(buf, sizeof(buf), "%.0f%%", state.throttle * 100.0f);
            uint16_t y = rowYFor(6,RNR), h = rowHFor(RNR);
            {
                String ts = buf;
                RowCache &tc = rowCache[6][7];
                if (tc.value != ts) {
                    printValue(tft, RF, RX, y, RHW-ROW_PAD, h, "Thrtl:", ts, TFT_DARK_GREEN, TFT_BLACK, COL_BACK, printState[6][7]);
                    tc.value = ts; tc.fg = TFT_DARK_GREEN; tc.bg = TFT_BLACK;
                }
            }
            {
                const char *rv = state.rcs_on ? "ON" : "OFF";
                uint16_t rfg = state.rcs_on ? TFT_DARK_GREEN : TFT_DARK_GREY;
                RowCache &rc2 = rowCache[6][8];
                if (rc2.value != rv || rc2.fg != rfg) {
                    printValue(tft, RF, RX+RHW, y, RHW-ROW_PAD, h, "RCS:", rv, rfg, TFT_BLACK, COL_BACK, printState[6][8]);
                    rc2.value = rv; rc2.fg = rfg; rc2.bg = TFT_BLACK;
                }
            }
        }

        // Row 7: Gear | SAS (split)
        {
            uint16_t y = rowYFor(7,RNR), h = rowHFor(RNR);
            {
                const char *gv = state.gear_on ? "DOWN" : "UP";
                uint16_t gfg = state.gear_on ? TFT_DARK_GREEN : TFT_DARK_GREY;
                RowCache &gc = rowCache[6][9];
                if (gc.value != gv || gc.fg != gfg) {
                    printValue(tft, RF, RX, y, RHW-ROW_PAD, h, "Gear:", gv, gfg, TFT_BLACK, COL_BACK, printState[6][9]);
                    gc.value = gv; gc.fg = gfg; gc.bg = TFT_BLACK;
                }
            }
            {
                const char *sv; uint16_t sfg, sbg = TFT_BLACK;
                switch (state.sasMode) {
                    case 255: sv="OFF";  sfg=TFT_DARK_GREY;  break;
                    case 0:   sv="STAB"; sfg=TFT_SKY;        break;
                    case 2:   sv="RETR"; sfg=TFT_DARK_GREEN; break;
                    default:  sv="----"; sfg=TFT_DARK_GREY;  break;
                }
                RowCache &sc = rowCache[6][10];
                if (sc.value != sv || sc.fg != sfg) {
                    printValue(tft, RF, RX+RHW, y, RHW-ROW_PAD, h, "SAS:", sv, sfg, sbg, COL_BACK, printState[6][10]);
                    sc.value = sv; sc.fg = sfg; sc.bg = sbg;
                }
            }
        }

        // Horizontal borders redrawn last — after all value updates so they're never overwritten
        // Alt.Rdr | V.Srf (rows 2→3)
        tft.drawLine(360, rowYFor(3,RNR) - 2, CONTENT_W, rowYFor(3,RNR) - 2, TFT_GREY);
        tft.drawLine(360, rowYFor(3,RNR) - 1, CONTENT_W, rowYFor(3,RNR) - 1, TFT_GREY);
        // Fwd/Lat | ΔV.Stg (rows 4→5)
        tft.drawLine(360, rowYFor(5,RNR) - 2, CONTENT_W, rowYFor(5,RNR) - 2, TFT_GREY);
        tft.drawLine(360, rowYFor(5,RNR) - 1, CONTENT_W, rowYFor(5,RNR) - 1, TFT_GREY);
        // ΔV.Stg | Thrtl/RCS (rows 5→6)
        tft.drawLine(360, rowYFor(6,RNR) - 2, CONTENT_W, rowYFor(6,RNR) - 2, TFT_GREY);
        tft.drawLine(360, rowYFor(6,RNR) - 1, CONTENT_W, rowYFor(6,RNR) - 1, TFT_GREY);
    }
}



/***************************************************************************************
   DRAW: RE-ENTRY (unchanged from original)
****************************************************************************************/
static void _lndgDrawReentry(RA8875 &tft) {
    static const tFont   *F      = &Roboto_Black_40;
    static const uint8_t  NR     = 8;
    static const uint16_t SECT_W = 26;
    static const uint16_t AX     = ROW_PAD + SECT_W;
    static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
    static const uint16_t AHW    = AW / 2;
    uint16_t fg, bg;
    char buf[16];

    auto lndgVal = [&](uint8_t row, const char *label, const String &val,
                       uint16_t fgc, uint16_t bgc) {
        drawValue(tft, 6, row, AX, AW, label, val, fgc, bgc, F, NR);
    };

    bool inOrbitOrEscape = (state.situation == sit_Orbit || state.situation == sit_Escaping);
    float tGround = (!inOrbitOrEscape && state.verticalVel < -0.05f && state.radarAlt > 0.0f)
                    ? fabsf(state.radarAlt / state.verticalVel) : -1.0f;
    float vSq  = state.surfaceVel * state.surfaceVel - state.verticalVel * state.verticalVel;
    float hSpd = (vSq > 0.0f) ? sqrtf(vSq) : 0.0f;
    float atmoAlt = (currentBody.lowSpace > 0.0f) ? currentBody.lowSpace : 70000.0f;
    bool peaBelowAtmo = (state.periapsis < atmoAlt);
    bool aboveAtmo    = !state.inAtmo;
    bool wantTPe      = (aboveAtmo && peaBelowAtmo);
    if (wantTPe != _lndgReentryRow0TPe) {
        _lndgReentryRow0TPe = wantTPe; rowCache[6][0].value = "\x01";
        switchToScreen(screen_LNDG); return;
    }
    bool wantSL = aboveAtmo;
    if (wantSL != _lndgReentryRow1SL) {
        _lndgReentryRow1SL = wantSL; rowCache[6][1].value = "\x01";
        switchToScreen(screen_LNDG); return;
    }

    if (wantTPe) {
        float tAtmo = (state.verticalVel < -5.0f)
                      ? fabsf((state.altitude - atmoAlt) / state.verticalVel) : -1.0f;
        if      (tAtmo >= 0.0f)             lndgVal(0, "T+Atm:", formatTime(tAtmo), TFT_DARK_GREEN, TFT_BLACK);
        else if (state.verticalVel <= 5.0f) lndgVal(0, "T+Atm:", "---", TFT_DARK_GREEN, TFT_BLACK);
        else                                lndgVal(0, "T+Atm:", "---", TFT_DARK_GREY,  TFT_BLACK);
    } else if (!aboveAtmo) {
        if (tGround >= 0.0f) {
            fg = (tGround < LNDG_TGRND_ALARM_S) ? TFT_WHITE  :
                 (tGround < LNDG_TGRND_WARN_S)  ? TFT_YELLOW : TFT_DARK_GREEN;
            bg = (tGround < LNDG_TGRND_ALARM_S) ? TFT_RED    : TFT_BLACK;
            lndgVal(0, "T.Grnd:", formatTime(tGround), fg, bg);
        } else { lndgVal(0, "T.Grnd:", "---", TFT_DARK_GREY, TFT_BLACK); }
    } else { lndgVal(0, "T.Grnd:", "---", TFT_DARK_GREY, TFT_BLACK); }

    if (wantSL) {
        lndgVal(1, "Alt.SL:", formatAlt(state.altitude), TFT_DARK_GREEN, TFT_BLACK);
    } else {
        fg = (state.radarAlt < ALT_RDR_ALARM_M) ? TFT_WHITE  :
             (state.radarAlt < ALT_RDR_WARN_M)  ? TFT_YELLOW : TFT_DARK_GREEN;
        bg = (state.radarAlt < ALT_RDR_ALARM_M) ? TFT_RED    : TFT_BLACK;
        lndgVal(1, "Alt.Rdr:", formatAlt(state.radarAlt), fg, bg);
    }

    {
        if (tGround >= 0.0f) {
            fg = (state.verticalVel < LNDG_VVRT_ALARM_MS) ? TFT_WHITE  :
                 (state.verticalVel < LNDG_VVRT_WARN_MS)  ? TFT_YELLOW : TFT_DARK_GREEN;
            bg = (state.verticalVel < LNDG_VVRT_ALARM_MS) ? TFT_RED    : TFT_BLACK;
        } else { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
        lndgVal(2, "V.Vrt:", fmtMs(state.verticalVel), fg, bg);
    }

    {
        bool wantPeA = (state.radarAlt > 20000.0f || !state.inAtmo);
        if (wantPeA != _lndgReentryRow3PeA) {
            _lndgReentryRow3PeA = wantPeA; rowCache[6][3].value = "\x01";
            switchToScreen(screen_LNDG); return;
        }
        if (wantPeA) {
            if      (state.periapsis < 0.0f)     { fg = TFT_WHITE;     bg = TFT_DARK_GREEN; }
            else if (state.periapsis < atmoAlt)   { fg = TFT_YELLOW;    bg = TFT_BLACK; }
            else                                  { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
            lndgVal(3, "PeA:", formatAlt(state.periapsis), fg, bg);
        } else {
            if      (hSpd > LNDG_REENTRY_VHRZ_ALARM_MS) { fg = TFT_WHITE;     bg = TFT_RED; }
            else if (hSpd > LNDG_REENTRY_VHRZ_WARN_MS)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
            else                                         { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
            lndgVal(3, "V.Hrz:", fmtMs(hSpd), fg, bg);
        }
    }

    fg = (state.surfaceVel < 0) ? TFT_RED : TFT_DARK_GREEN;
    lndgVal(4, "V.Srf:", fmtMs(state.surfaceVel), fg, TFT_BLACK);

    float spd = state.surfaceVel;
    {
        uint16_t xL=AX, wL=AHW-ROW_PAD, xR=AX+AHW+ROW_PAD, wR=AHW-ROW_PAD;
        float m = state.machNumber;
        snprintf(buf, sizeof(buf), "%.2f", m);
        bool transonic = (m >= 0.85f && m <= 1.2f);
        { String ms=buf; uint16_t mfg = transonic ? TFT_YELLOW : TFT_DARK_GREEN;
          RowCache &mc=rowCache[6][9];
          if (mc.value!=ms||mc.fg!=mfg||mc.bg!=TFT_BLACK) {
              printValue(tft,F,xL,rowYFor(5,NR),wL,rowHFor(NR),"Mach:",ms,mfg,TFT_BLACK,COL_BACK,printState[6][9]);
              mc.value=ms;mc.fg=mfg;mc.bg=TFT_BLACK; } }
        float g=state.gForce; snprintf(buf,sizeof(buf),"%.2f",g);
        fg=(g>G_ALARM_POS||g<G_ALARM_NEG)?TFT_WHITE:(g>G_WARN_POS||g<G_WARN_NEG)?TFT_YELLOW:TFT_DARK_GREEN;
        bg=(g>G_ALARM_POS||g<G_ALARM_NEG)?TFT_RED:TFT_BLACK;
        { String gs=buf; RowCache &gc=rowCache[6][10];
          if (gc.value!=gs||gc.fg!=fg||gc.bg!=bg) {
              printValue(tft,F,xR,rowYFor(5,NR),wR,rowHFor(NR),"G:",gs,fg,bg,COL_BACK,printState[6][10]);
              gc.value=gs;gc.fg=fg;gc.bg=bg; } }
    }

    if (state.drogueDeploy&&!_drogueDeployed){_drogueDeployed=true;_drogueArmedSafe=(!state.inAtmo||spd<=LNDG_DROGUE_RISKY_MS);}
    if (state.drogueCut   &&!_drogueCut)     {_drogueCut=true;_drogueDeployed=false;_drogueArmedSafe=false;}
    if (state.mainDeploy  &&!_mainDeployed)  {_mainDeployed=true;_mainArmedSafe=(!state.inAtmo||spd<=LNDG_MAIN_RISKY_MS);}
    if (state.mainCut     &&!_mainCut)       {_mainCut=true;_mainDeployed=false;_mainArmedSafe=false;}

    auto chuteState=[&](bool dep,bool cut,bool safe,float safeSpd,float riskySpd,float fullAlt,
                        const char*&lbl,uint16_t&cfg,uint16_t&cbg){
        if(cut){lbl="CUT";cfg=TFT_RED;cbg=TFT_BLACK;return;}
        if(dep){
            if(!safe&&state.inAtmo&&spd>riskySpd){lbl="OPEN";cfg=TFT_WHITE;cbg=TFT_RED;return;}
            if(state.airDensity<LNDG_CHUTE_SEMI_DENSITY){lbl="ARMED";cfg=TFT_SKY;cbg=TFT_BLACK;return;}
            lbl="OPEN";cfg=(state.radarAlt>fullAlt)?TFT_YELLOW:TFT_DARK_GREEN;cbg=TFT_BLACK;return;
        }
        lbl="STOWED";
        if(!state.inAtmo){cfg=TFT_DARK_GREEN;cbg=TFT_BLACK;}
        else if(spd>riskySpd){cfg=TFT_WHITE;cbg=TFT_RED;}
        else if(spd>safeSpd) {cfg=TFT_YELLOW;cbg=TFT_BLACK;}
        else                 {cfg=TFT_DARK_GREEN;cbg=TFT_BLACK;}
    };

    {
        uint16_t xL=AX,wL=AHW-ROW_PAD,xR=AX+AHW+ROW_PAD,wR=AHW-ROW_PAD;
        uint16_t y6=rowYFor(6,NR),h6=rowHFor(NR);
        const char*dv;uint16_t dfg,dbg;
        chuteState(_drogueDeployed,_drogueCut,_drogueArmedSafe,LNDG_DROGUE_SAFE_MS,LNDG_DROGUE_RISKY_MS,LNDG_DROGUE_FULL_ALT,dv,dfg,dbg);
        {String ds=dv;RowCache&dc=rowCache[6][6];
         if(dc.value!=ds||dc.fg!=dfg||dc.bg!=dbg){printValue(tft,F,xL,y6,wL,h6,"Drogue:",ds,dfg,dbg,COL_BACK,printState[6][6]);dc.value=ds;dc.fg=dfg;dc.bg=dbg;}}
        const char*mv;uint16_t mfg,mbg;
        chuteState(_mainDeployed,_mainCut,_mainArmedSafe,LNDG_MAIN_SAFE_MS,LNDG_MAIN_RISKY_MS,LNDG_MAIN_FULL_ALT,mv,mfg,mbg);
        {String ms=mv;RowCache&mc=rowCache[6][11];
         if(mc.value!=ms||mc.fg!=mfg||mc.bg!=mbg){printValue(tft,F,xR,y6,wR,h6,"Main:",ms,mfg,mbg,COL_BACK,printState[6][11]);mc.value=ms;mc.fg=mfg;mc.bg=mbg;}}
    }
    {
        uint16_t y=rowYFor(7,NR),h=rowHFor(NR);
        uint16_t xL=AX,wL=AHW-ROW_PAD,xR=AX+AHW+ROW_PAD,wR=AHW-ROW_PAD;
        const char*gs=state.gear_on?"DOWN":"UP";
        uint16_t gfg=state.gear_on?TFT_DARK_GREEN:TFT_DARK_GREY;
        RowCache&gc=rowCache[6][7];
        if(gc.value!=gs||gc.fg!=gfg||gc.bg!=TFT_BLACK){
            printValue(tft,F,xL,y,wL,h,"Gear:",gs,gfg,TFT_BLACK,COL_BACK,printState[6][7]);
            gc.value=gs;gc.fg=gfg;gc.bg=TFT_BLACK;}
        const char*ss;uint16_t sfg,sbg;
        if(state.sasMode==255){ss="OFF";sfg=(state.machNumber>REENTRY_SAS_AERO_STABLE_MACH)?TFT_WHITE:TFT_DARK_GREY;sbg=(state.machNumber>REENTRY_SAS_AERO_STABLE_MACH)?TFT_RED:TFT_BLACK;}
        else if(state.sasMode==2){ss="RETR";sfg=TFT_DARK_GREEN;sbg=TFT_BLACK;}
        else if(state.sasMode==0){ss="STAB";sfg=TFT_SKY;sbg=TFT_BLACK;}
        else{switch(state.sasMode){case 1:ss="PROG";break;case 3:ss="NORM";break;case 4:ss="ANRM";break;case 5:ss="ROUT";break;case 6:ss="RINX";break;case 7:ss="TGT";break;case 8:ss="ATGT";break;case 9:ss="MNV";break;default:ss="---";break;}sfg=TFT_RED;sbg=TFT_BLACK;}
        RowCache&sc=rowCache[6][8];
        if(sc.value!=ss||sc.fg!=sfg||sc.bg!=sbg){
            printValue(tft,F,xR,y,wR,h,"SAS:",ss,sfg,sbg,COL_BACK,printState[6][8]);
            sc.value=ss;sc.fg=sfg;sc.bg=sbg;}
    }
}


/***************************************************************************************
   DRAW ENTRY POINT
****************************************************************************************/
static void drawScreen_LNDG(RA8875 &tft) {
    if (!_lndgReentryMode) _lndgDrawPowered(tft);
    else                   _lndgDrawReentry(tft);
}
