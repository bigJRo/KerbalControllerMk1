/***************************************************************************************
   Screen_LNDG_Powered.ino -- Powered descent (one of two modes of the LNDG screen).

   Active when _lndgReentryMode is FALSE (default). Pilot can toggle to re-entry
   mode via tap (handled in TouchEvents.ino).

   LAYOUT:
     - Altitude tape (x=4..34)         — vertical 0-500m / 0-50m altitude scale
     - X-Pointer square (x=120..350)   — horizontal velocity indicator (lat/fwd)
     - ATT indicator (x=111..227)      — attitude alignment dot in concentric rings
     - V.Vrt bar (x=283..313)          — vertical velocity 0-15 m/s gauge
     - Right panel (x=458..720)        — 15 numeric readouts

   Phase membership (LNDG screen has two modes):
     - POWERED DESCENT (this file)             — default
     - RE-ENTRY (Screen_LNDG_Reentry.ino)     — pilot toggle

   Top-level dispatcher Screen_LNDG.ino selects which to draw based on
   _lndgReentryMode.

   Public-to-the-sketch entry points (called by the LNDG dispatcher):
     - _lndgChromePowered(tft) — initial chrome (tape, XP, ATT, V.Vrt, panel labels)
     - _lndgDrawPowered(tft)   — per-frame value updates
****************************************************************************************/

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

        // Row 6: Throttle | RCS split — throttle keeps label, RCS is a button
        {
            uint16_t y = rowYFor(6, RNR), h = rowHFor(RNR);
            printDispChrome(tft, RCF, RX, y, RHW - ROW_PAD, h, "Thrtl:", COL_LABEL, COL_BACK, COL_NO_BDR);
            tft.drawLine(RX + RHW,     y, RX + RHW,     rowYFor(7,RNR) - 1, TFT_GREY);
            tft.drawLine(RX + RHW + 1, y, RX + RHW + 1, rowYFor(7,RNR) - 1, TFT_GREY);
        }

        // Row 7: Gear | SAS split — both are buttons, no chrome labels needed
        {
            uint16_t y = rowYFor(7, RNR);
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
    for (uint8_t r = 0; r <= 14; r++) rowCache[6][r].value = "\x01";
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
            // RCS button
            {
                bool rcsOn = state.rcs_on;
                String rv = rcsOn ? "ON" : "OFF";
                RowCache &rc2 = rowCache[6][8];
                if (rc2.value != rv) {
                    ButtonLabel btn = rcsOn
                        ? ButtonLabel{ "RCS", TFT_WHITE,     TFT_WHITE,     TFT_DARK_GREEN, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }
                        : ButtonLabel{ "RCS", TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK,  TFT_OFF_BLACK,  TFT_GREY, TFT_GREY };
                    drawButton(tft, RX + RHW, y, RHW, h, btn, &Roboto_Black_20, false);
                    rc2.value = rv;
                }
            }
        }

        // Row 7: Gear | SAS (split) — both as buttons, extend to screen bottom
        {
            uint16_t y  = rowYFor(7,RNR);
            uint16_t rh = SCREEN_H - y;
            // Gear button
            {
                bool gearDown = state.gear_on;
                String gv = gearDown ? "DOWN" : "UP";
                RowCache &gc = rowCache[6][9];
                if (gc.value != gv) {
                    ButtonLabel btn = gearDown
                        ? ButtonLabel{ "GEAR", TFT_WHITE,     TFT_WHITE,     TFT_DARK_GREEN, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }
                        : ButtonLabel{ "GEAR", TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK,  TFT_OFF_BLACK,  TFT_GREY, TFT_GREY };
                    drawButton(tft, RX - 2, y, RHW + 2, rh, btn, &Roboto_Black_20, false);
                    gc.value = gv;
                }
            }
            // SAS button — landing context: STAB and RETR acceptable, others questionable
            {
                const char *sv; uint16_t sfg, sbg;
                switch (state.sasMode) {
                    case 255: sv = "SAS";  sfg = TFT_WHITE;     sbg = TFT_RED;        break; // off — alarm
                    case 0:   sv = "STAB"; sfg = TFT_WHITE;     sbg = TFT_DARK_GREEN; break; // good for landing
                    case 2:   sv = "RETR"; sfg = TFT_WHITE;     sbg = TFT_DARK_GREEN; break; // good for landing
                    default:  sv = "SAS";  sfg = TFT_DARK_GREY; sbg = TFT_OFF_BLACK;  break; // other — neutral
                }
                RowCache &sc = rowCache[6][10];
                String ssv = sv;
                if (sc.value != ssv || sc.fg != sfg || sc.bg != sbg) {
                    ButtonLabel btn = { sv, sfg, sfg, sbg, sbg, TFT_GREY, TFT_GREY };
                    drawButton(tft, RX + RHW, y, RHW, rh, btn, &Roboto_Black_20, false);
                    sc.value = ssv; sc.fg = sfg; sc.bg = sbg;
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



