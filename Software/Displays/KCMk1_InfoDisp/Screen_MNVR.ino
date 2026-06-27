/***************************************************************************************
   Screen_MNVR.ino  —  Maneuver screen: burn alignment reticle + node data

   LAYOUT (800×480, content area 720×418 below title bar)
   ┌────────────────────────────────┬──────────────────────────────────────┐
   │                                │ ΔV.Mnvr:      847 m/s                │
   │   Alignment Reticle            │ ΔV.Plan:     1204 m/s               │
   │   (black disc, R=170)          │ ΔV.Stg:       923 m/s                │
   │                                │ T+Ign:        4:32                   │
   │   ◆ maneuver marker (blue)     │ T+Mnvr:       5:19                   │
   │   □ green box when aligned     │ Burn dur:     1:47                   │
   │   + fixed crosshair (grey)     │ Brg: +2.1°      Elv: -1.4°          │
   │                                │ [RCS]        [SAS: MNVR]             │
   └────────────────────────────────┴──────────────────────────────────────┘

   RETICLE SEMANTICS
   ─────────────────
   Fixed crosshair  = your nose direction (always at centre)
   Marker (MNVR)    = where the burn vector points relative to your nose.
                      At centre → aligned for the burn.
   Marker colour    = always maneuver blue (TFT_BLUE).
   Alignment box    = neon-green square drawn around diamond when
                      error magnitude < ATT_ERR_WARN_DEG (5°).

   RETICLE GEOMETRY
   ─────────────────
   Centre: (192, 251)  Radius: 170px  Scale: 8.5 px/deg (±20° full scale)
   Rings: 5°=42px  10°=85px  15°=127px  20°=170px

   PANEL: NR=8 rows (rowH=52px, btn_h=54px — matches DOCK)
   Row 0: ΔV.Mnvr        Row 1: ΔV.Plan
   Row 2: ΔV.Stg         Row 3: T+Ign
   Row 4: T+Mnvr         Row 5: Burn dur
   Row 6: Brg | Elv split
   Row 7: RCS | SAS buttons (to screen bottom)

   SAS COLOUR SCHEME
   ──────────────────
   MNVR (9)  = dark green  ← correct mode for burn
   STAB (0)  = cyan/sky    ← acceptable (holding attitude manually)
   SAS  (255)= grey        ← off
   All others= red         ← wrong for a burn
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


// ── State flags ───────────────────────────────────────────────────────────────────────
static bool _mnvrChromDrawn  = false;


// ── Reticle geometry ─────────────────────────────────────────────────────────────────
static const uint16_t MNVR_CX    = 192;
static const uint16_t MNVR_CY    = 251;
static const uint16_t MNVR_R     = 170;
static const float    MNVR_SCALE = (float)MNVR_R / 20.0f;   // 8.5 px/deg, ±20° full scale

static const uint16_t MNVR_RING_5  =  42;
static const uint16_t MNVR_RING_10 =  85;
static const uint16_t MNVR_RING_15 = 127;
static const uint16_t MNVR_RING_20 = MNVR_R;

// Marker sizing — diamond and box larger than DOCK for better visibility
static const uint8_t MNVR_MRK_DS  = 13;   // diamond half-size (was 9 on DOCK)
static const uint8_t MNVR_BOX_R   = 19;   // alignment box half-size (DS + 6px clearance)
static const uint8_t MNVR_ERASE_R = 22;   // erase rect half-size (covers box + 2px margin)

// Right panel geometry
static const uint16_t MNVR_RP_X  = 385;
static const uint16_t MNVR_RP_W  = 335;
static const uint8_t  MNVR_RP_NR = 8;     // 8 rows: rowH=52px, btn_h=54px — matches DOCK
static const tFont   *MNVR_RP_F  = &Roboto_Black_28;

// ΔV bar geometry
static const uint16_t MNVR_BAR_X = 19;
static const uint16_t MNVR_BAR_W = 346;
static const uint16_t MNVR_BAR_H = 22;

// Screen cache index
static const uint8_t MNVR_SC = (uint8_t)screen_MNVR;   // 3


// ── Previous marker state ─────────────────────────────────────────────────────────────
static int16_t _mnvrPrevMrkX    = 9999, _mnvrPrevMrkY = 9999;
static bool    _mnvrPrevAligned = false;


// ── Wrap heading error to ±180° ───────────────────────────────────────────────────────
static inline float _mnvrWrapErr(float e) {
    while (e >  180.0f) e -= 360.0f;
    while (e < -180.0f) e += 360.0f;
    return e;
}


// ── Clamp marker to reticle boundary ─────────────────────────────────────────────────
static void _mnvrClampMrk(int16_t &sx, int16_t &sy) {
    float dx = sx - MNVR_CX, dy = sy - MNVR_CY;
    float dist = sqrtf(dx*dx + dy*dy);
    float maxR = (float)(MNVR_R - MNVR_MRK_DS - 2);
    if (dist > maxR && dist > 0.5f) {
        float scale = maxR / dist;
        sx = MNVR_CX + (int16_t)(dx * scale);
        sy = MNVR_CY + (int16_t)(dy * scale);
    }
}


// ── Draw reticle chrome ───────────────────────────────────────────────────────────────
static void _mnvrDrawReticleChrome(RA8875 &tft) {
    // Black disc + inner good-zone fill
    tft.fillCircle(MNVR_CX, MNVR_CY, MNVR_R,     TFT_BLACK);
    tft.fillCircle(MNVR_CX, MNVR_CY, MNVR_RING_5, TFT_OFF_BLACK);

    // Concentric rings
    tft.drawCircle(MNVR_CX, MNVR_CY, MNVR_RING_5,  TFT_DARK_GREEN);
    tft.drawCircle(MNVR_CX, MNVR_CY, MNVR_RING_10, TFT_DARK_GREY);
    tft.drawCircle(MNVR_CX, MNVR_CY, MNVR_RING_15, TFT_DARK_GREY);
    tft.drawCircle(MNVR_CX, MNVR_CY, MNVR_RING_20, TFT_GREY);

    // Cardinal lines with gap for crosshair
    uint16_t gap = 18, arm = MNVR_R - 1;
    tft.drawLine(MNVR_CX,       MNVR_CY - arm, MNVR_CX,       MNVR_CY - gap, TFT_DARK_GREY);
    tft.drawLine(MNVR_CX,       MNVR_CY + gap, MNVR_CX,       MNVR_CY + arm, TFT_DARK_GREY);
    tft.drawLine(MNVR_CX - arm, MNVR_CY,       MNVR_CX - gap, MNVR_CY,       TFT_DARK_GREY);
    tft.drawLine(MNVR_CX + gap, MNVR_CY,       MNVR_CX + arm, MNVR_CY,       TFT_DARK_GREY);

    // Crosshair symbol at centre
    tft.drawLine(MNVR_CX - gap + 2, MNVR_CY,       MNVR_CX - 4,       MNVR_CY,       TFT_GREY);
    tft.drawLine(MNVR_CX + 4,       MNVR_CY,       MNVR_CX + gap - 2, MNVR_CY,       TFT_GREY);
    tft.drawLine(MNVR_CX,       MNVR_CY - gap + 2, MNVR_CX,       MNVR_CY - 4,       TFT_GREY);
    tft.drawLine(MNVR_CX,       MNVR_CY + 4,       MNVR_CX,       MNVR_CY + gap - 2, TFT_GREY);
    tft.fillCircle(MNVR_CX, MNVR_CY, 2, TFT_GREY);

    // Minor ticks at 30° increments on outer ring (skip cardinals)
    for (uint16_t deg = 30; deg < 360; deg += 30) {
        if (deg % 90 == 0) continue;
        float rad = (deg - 90) * DEG_TO_RAD;
        int16_t ox = MNVR_CX + (int16_t)(MNVR_R * cosf(rad));
        int16_t oy = MNVR_CY + (int16_t)(MNVR_R * sinf(rad));
        int16_t ix = MNVR_CX + (int16_t)((MNVR_R - 14) * cosf(rad));
        int16_t iy = MNVR_CY + (int16_t)((MNVR_R - 14) * sinf(rad));
        tft.drawLine(ox, oy, ix, iy, TFT_DARK_GREY);
    }

    // Ring degree labels (NE quadrant)
    // Ring degree labels (NE quadrant, just inside each ring)
    // Single-arg setTextColor = transparent background.
    tft.setFont(&Roboto_Black_12);
    tft.setTextColor(TFT_LIGHT_GREY);
    tft.setCursor(MNVR_CX + 3, MNVR_CY - MNVR_RING_5  + 3);  tft.print("5");
    tft.setCursor(MNVR_CX + 3, MNVR_CY - MNVR_RING_10 + 3);  tft.print("10");
    tft.setCursor(MNVR_CX + 3, MNVR_CY - MNVR_RING_15 + 3);  tft.print("15");
    tft.setCursor(MNVR_CX + 3, MNVR_CY - MNVR_RING_20 + 3);  tft.print("20");

    // Bezel ring
    tft.drawCircle(MNVR_CX, MNVR_CY, MNVR_R,     TFT_GREY);
    tft.drawCircle(MNVR_CX, MNVR_CY, MNVR_R + 1, TFT_DARK_GREY);

    // Legend — top-left corner
    static const uint16_t LEG_X  = 6;
    static const uint16_t LEG_Y0 = TITLE_TOP + 6;
    tft.setFont(&Roboto_Black_12);

    // MANEUVER — solid blue diamond (horizontal split, no fill gaps)
    tft.fillTriangle(LEG_X,   LEG_Y0+6, LEG_X+12, LEG_Y0+6, LEG_X+6, LEG_Y0,    TFT_BLUE);  // top half
    tft.fillTriangle(LEG_X,   LEG_Y0+6, LEG_X+12, LEG_Y0+6, LEG_X+6, LEG_Y0+12, TFT_BLUE);  // bottom half
    tft.setTextColor(TFT_SKY, TFT_BLACK);
    tft.setCursor(LEG_X + 16, LEG_Y0);
    tft.print("MANEUVER");

    // NOSE — crosshair symbol
    tft.drawLine(LEG_X+1,  LEG_Y0+26, LEG_X+4,  LEG_Y0+26, TFT_GREY);
    tft.drawLine(LEG_X+8,  LEG_Y0+26, LEG_X+11, LEG_Y0+26, TFT_GREY);
    tft.drawLine(LEG_X+6,  LEG_Y0+21, LEG_X+6,  LEG_Y0+24, TFT_GREY);
    tft.drawLine(LEG_X+6,  LEG_Y0+28, LEG_X+6,  LEG_Y0+31, TFT_GREY);
    tft.fillCircle(LEG_X+6, LEG_Y0+26, 1, TFT_GREY);
    tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
    tft.setCursor(LEG_X + 16, LEG_Y0 + 20);
    tft.print("NOSE");

    // ΔV bar chrome — bar label/value font matches LNCH_Circ ΔV Burn bar (Black_20).
    // Bar shifted 8 px down (barY: CY+R+20 → CY+R+28) to accommodate the taller
    // 24 px label between the reticle bottom and the bar.
    uint16_t barY = MNVR_CY + MNVR_R + 28;
    uint16_t lblY = barY - 24;
    tft.setFont(&Roboto_Black_20);
    tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
    tft.setCursor(MNVR_BAR_X, lblY);
    tft.print("\xCE\x94V Burn");
    tft.drawRect(MNVR_BAR_X, barY, MNVR_BAR_W, MNVR_BAR_H, TFT_GREY);
}


// ── Draw right-panel chrome ───────────────────────────────────────────────────────────
static void _mnvrDrawRightChrome(RA8875 &tft) {
    uint8_t  NR = MNVR_RP_NR;
    uint16_t X  = MNVR_RP_X;
    uint16_t W  = MNVR_RP_W;

    printDispChrome(tft, &Roboto_Black_20, X, rowYFor(0,NR), W, rowHFor(NR), "\xCE\x94V.Mnvr:",  COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, &Roboto_Black_20, X, rowYFor(1,NR), W, rowHFor(NR), "\xCE\x94V.Plan:",  COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, &Roboto_Black_20, X, rowYFor(2,NR), W, rowHFor(NR), "\xCE\x94V.Stg:",   COL_LABEL, COL_BACK, COL_NO_BDR);

    // Divider: ΔV block / timing block
    { uint16_t dy = rowYFor(3,NR) - 1;
      tft.drawLine(X, dy,   X+W, dy,   TFT_GREY);
      tft.drawLine(X, dy+1, X+W, dy+1, TFT_GREY); }

    printDispChrome(tft, &Roboto_Black_20, X, rowYFor(3,NR), W, rowHFor(NR), "T+Ign:",   COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, &Roboto_Black_20, X, rowYFor(4,NR), W, rowHFor(NR), "T+Mnvr:",  COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, &Roboto_Black_20, X, rowYFor(5,NR), W, rowHFor(NR), "Burn dur:",COL_LABEL, COL_BACK, COL_NO_BDR);

    // Divider: timing block / alignment block
    { uint16_t dy = rowYFor(6,NR) - 1;
      tft.drawLine(X, dy,   X+W, dy,   TFT_GREY);
      tft.drawLine(X, dy+1, X+W, dy+1, TFT_GREY); }

    // Row 6: Nos.Brg | Nos.Elv split
    {
        uint16_t y = rowYFor(6,NR), h = rowHFor(NR), hw = W / 2;
        printDispChrome(tft, &Roboto_Black_20, X,      y, hw - ROW_PAD, h, "Brg:", COL_LABEL, COL_BACK, COL_NO_BDR);
        printDispChrome(tft, &Roboto_Black_20, X + hw, y, hw - ROW_PAD, h, "Elv:", COL_LABEL, COL_BACK, COL_NO_BDR);
        tft.drawLine(X + hw,     y, X + hw,     y + h - 1, TFT_GREY);
        tft.drawLine(X + hw + 1, y, X + hw + 1, y + h - 1, TFT_GREY);
    }

    // Divider: alignment block / buttons
    { uint16_t dy = rowYFor(7,NR) - 1;
      tft.drawLine(X, dy,   X+W, dy,   TFT_GREY);
      tft.drawLine(X, dy+1, X+W, dy+1, TFT_GREY); }

    // RCS | SAS button split line
    uint16_t hw = W / 2;
    tft.drawLine(X + hw,     rowYFor(7,NR), X + hw,     SCREEN_H - 1, TFT_GREY);
    tft.drawLine(X + hw + 1, rowYFor(7,NR), X + hw + 1, SCREEN_H - 1, TFT_GREY);

    // Panel left border
    tft.drawLine(X - 2, TITLE_TOP, X - 2, SCREEN_H, TFT_GREY);
    tft.drawLine(X - 1, TITLE_TOP, X - 1, SCREEN_H, TFT_GREY);
}


// ── Repair chrome after marker erase ─────────────────────────────────────────────────
static void _mnvrRepairChrome(RA8875 &tft, int16_t bx, int16_t by, uint8_t bh) {
    int16_t boxX0=bx, boxX1=bx+2*bh, boxY0=by, boxY1=by+2*bh;

    float cx = (float)constrain((int)MNVR_CX, (int)boxX0, (int)boxX1);
    float cy = (float)constrain((int)MNVR_CY, (int)boxY0, (int)boxY1);
    float distToCentre = sqrtf((cx-MNVR_CX)*(cx-MNVR_CX) + (cy-MNVR_CY)*(cy-MNVR_CY));
    float ddx = fmaxf(fabsf(boxX0-MNVR_CX), fabsf(boxX1-MNVR_CX));
    float ddy = fmaxf(fabsf(boxY0-MNVR_CY), fabsf(boxY1-MNVR_CY));
    float distFar = sqrtf(ddx*ddx + ddy*ddy);

    static const uint16_t rings[] = {MNVR_RING_5, MNVR_RING_10, MNVR_RING_15, MNVR_RING_20};
    static const uint16_t rcols[] = {TFT_DARK_GREEN, TFT_DARK_GREY, TFT_DARK_GREY, TFT_GREY};
    for (uint8_t i = 0; i < 4; i++) {
        float r = (float)rings[i];
        if (distToCentre <= r + 1.5f && distFar >= r - 1.5f)
            tft.drawCircle(MNVR_CX, MNVR_CY, rings[i], rcols[i]);
    }

    static const uint16_t cgap = 18, carm = MNVR_R - 1;
    if (boxY0 <= MNVR_CY && MNVR_CY <= boxY1) {
        if (boxX0 < (int16_t)(MNVR_CX - cgap))
            tft.drawLine(MNVR_CX - carm, MNVR_CY, MNVR_CX - cgap, MNVR_CY, TFT_DARK_GREY);
        if (boxX1 > (int16_t)(MNVR_CX + cgap))
            tft.drawLine(MNVR_CX + cgap, MNVR_CY, MNVR_CX + carm, MNVR_CY, TFT_DARK_GREY);
        if (boxX0 <= MNVR_CX || boxX1 >= MNVR_CX) {
            tft.drawLine(MNVR_CX - cgap + 2, MNVR_CY, MNVR_CX - 4,       MNVR_CY, TFT_GREY);
            tft.drawLine(MNVR_CX + 4,       MNVR_CY, MNVR_CX + cgap - 2, MNVR_CY, TFT_GREY);
        }
    }
    if (boxX0 <= MNVR_CX && MNVR_CX <= boxX1) {
        if (boxY0 < (int16_t)(MNVR_CY - cgap))
            tft.drawLine(MNVR_CX, MNVR_CY - carm, MNVR_CX, MNVR_CY - cgap, TFT_DARK_GREY);
        if (boxY1 > (int16_t)(MNVR_CY + cgap))
            tft.drawLine(MNVR_CX, MNVR_CY + cgap, MNVR_CX, MNVR_CY + carm, TFT_DARK_GREY);
        if (boxY0 <= MNVR_CY || boxY1 >= MNVR_CY) {
            tft.drawLine(MNVR_CX, MNVR_CY - cgap + 2, MNVR_CX, MNVR_CY - 4,        TFT_GREY);
            tft.drawLine(MNVR_CX, MNVR_CY + 4,        MNVR_CX, MNVR_CY + cgap - 2, TFT_GREY);
        }
    }
    if (boxX0 <= MNVR_CX && MNVR_CX <= boxX1 && boxY0 <= MNVR_CY && MNVR_CY <= boxY1)
        tft.fillCircle(MNVR_CX, MNVR_CY, 2, TFT_GREY);

    if (distToCentre <= MNVR_RING_5 - 1) {
        tft.fillCircle(MNVR_CX, MNVR_CY, MNVR_RING_5 - 1, TFT_OFF_BLACK);
        tft.drawCircle(MNVR_CX, MNVR_CY, MNVR_RING_5,     TFT_DARK_GREEN);
    }
}


// ── Update burn vector marker ─────────────────────────────────────────────────────────
// Diamond always TFT_BLUE. Neon-green alignment box appears when error < 5°.
static void _mnvrUpdateMarker(RA8875 &tft, float brgErr, float elvErr) {
    float errMag  = sqrtf(brgErr*brgErr + elvErr*elvErr);
    bool  aligned = (errMag < ATT_ERR_WARN_DEG);

    int16_t sx = MNVR_CX + (int16_t)(-brgErr * MNVR_SCALE);
    int16_t sy = MNVR_CY + (int16_t)( elvErr * MNVR_SCALE);
    _mnvrClampMrk(sx, sy);

    static const uint8_t EH = MNVR_ERASE_R;
    static const uint8_t DS = MNVR_MRK_DS;
    static const uint8_t BX = MNVR_BOX_R;

    bool moved        = (_mnvrPrevMrkX == 9999 ||
                         abs(sx - _mnvrPrevMrkX) > 1 ||
                         abs(sy - _mnvrPrevMrkY) > 1);
    bool alignChanged = (aligned != _mnvrPrevAligned);

    if (moved || alignChanged) {
        if (_mnvrPrevMrkX != 9999) {
            tft.fillRect(_mnvrPrevMrkX - EH, _mnvrPrevMrkY - EH, EH*2+1, EH*2+1, TFT_BLACK);
            _mnvrRepairChrome(tft, _mnvrPrevMrkX - EH, _mnvrPrevMrkY - EH, EH);
        }
        // Diamond — horizontal split: shared edge is scanline-aligned, no raster gaps
        tft.fillTriangle(sx-DS, sy, sx+DS, sy, sx, sy-DS, TFT_BLUE);  // top half
        tft.fillTriangle(sx-DS, sy, sx+DS, sy, sx, sy+DS, TFT_BLUE);  // bottom half
        // Alignment box — neon green when within 5°
        if (aligned) {
            tft.drawRect(sx - BX,     sy - BX,     BX*2+1, BX*2+1, TFT_NEON_GREEN);
            tft.drawRect(sx - BX - 1, sy - BX - 1, BX*2+3, BX*2+3, TFT_DARK_GREEN);
        }
        _mnvrPrevMrkX    = sx;
        _mnvrPrevMrkY    = sy;
        _mnvrPrevAligned = aligned;
    }

    // Crosshair always on top
    static const uint16_t g = 18;
    tft.drawLine(MNVR_CX - g + 2, MNVR_CY,       MNVR_CX - 4,       MNVR_CY,       TFT_GREY);
    tft.drawLine(MNVR_CX + 4,     MNVR_CY,       MNVR_CX + g - 2,   MNVR_CY,       TFT_GREY);
    tft.drawLine(MNVR_CX,         MNVR_CY - g+2, MNVR_CX,           MNVR_CY - 4,   TFT_GREY);
    tft.drawLine(MNVR_CX,         MNVR_CY + 4,   MNVR_CX,           MNVR_CY + g-2, TFT_GREY);
    tft.fillCircle(MNVR_CX, MNVR_CY, 2, TFT_GREY);
}


// ── ΔV remaining bar ──────────────────────────────────────────────────────────────────
static void _mnvrDrawDVBar(RA8875 &tft, float dvNode, float dvStage) {
    // Layout matches chrome: barY = CY+R+28, lblY = barY-24 (Black_20 label).
    static const uint16_t barY = MNVR_CY + MNVR_R + 28;
    static const uint16_t lblY = barY - 24;

    static float prevDV = -999.0f;
    if (fabsf(dvNode - prevDV) < 1.0f) return;
    prevDV = dvNode;

    float    maxDV    = fmaxf(dvStage, dvNode);
    bool     tight    = (dvStage < dvNode * MNVR_DV_MARGIN);
    uint16_t barCol   = tight ? TFT_YELLOW : TFT_DARK_GREEN;
    float    fraction = (maxDV > 0.0f) ? fminf(dvNode / maxDV, 1.0f) : 0.0f;
    uint16_t fillW    = (uint16_t)(fraction * (MNVR_BAR_W - 2));

    tft.fillRect(MNVR_BAR_X + 1, barY + 1, MNVR_BAR_W - 2, MNVR_BAR_H - 2, TFT_OFF_BLACK);
    if (fillW > 0)
        tft.fillRect(MNVR_BAR_X + 1, barY + 1, fillW, MNVR_BAR_H - 2, barCol);

    // Right-aligned value text in Black_20 (matches "ΔV Burn" label font).
    char buf[14];
    snprintf(buf, sizeof(buf), "%.0fm/s", dvNode);
    tft.setFont(&Roboto_Black_20);
    tft.setTextColor(barCol, TFT_BLACK);
    // Clear the value-text region: right half of bar width, 24 px tall (Black_20).
    tft.fillRect(MNVR_BAR_X + MNVR_BAR_W/2, lblY, MNVR_BAR_W/2, 24, TFT_BLACK);
    int16_t tw = getFontStringWidth(&Roboto_Black_20, buf);
    tft.setCursor(MNVR_BAR_X + MNVR_BAR_W - tw, lblY);
    tft.print(buf);
}


// ── CHROME ────────────────────────────────────────────────────────────────────────────
void chromeScreen_MNVR(RA8875 &tft) {
    bool hasMnvr = (state.mnvrTime > 0.0f || state.mnvrDeltaV > 0.0f);

    if (!hasMnvr) {
        _mnvrChromDrawn = false;
        tft.fillRect(0, TITLE_TOP, CONTENT_W, SCREEN_H - TITLE_TOP, TFT_BLACK);
        textCenter(tft, &Roboto_Black_72, 0, TITLE_TOP, CONTENT_W, SCREEN_H - TITLE_TOP,
                   "NO MANEUVER", TFT_WHITE, TFT_RED);
        return;
    }

    _mnvrChromDrawn  = true;
    _mnvrPrevMrkX    = 9999; _mnvrPrevMrkY = 9999;
    _mnvrPrevAligned = false;

    tft.fillRect(0, TITLE_TOP, MNVR_RP_X, SCREEN_H - TITLE_TOP, TFT_BLACK);
    _mnvrDrawReticleChrome(tft);
    _mnvrDrawRightChrome(tft);

    for (uint8_t r = 0; r < ROW_COUNT; r++) rowCache[MNVR_SC][r].value = "\x01";
    for (uint8_t r = 0; r < ROW_COUNT; r++) printState[MNVR_SC][r] = PrintState{};
}


// ── DRAW (called every loop) ──────────────────────────────────────────────────────────
void drawScreen_MNVR(RA8875 &tft) {
    uint32_t _t0 = micros();

    bool hasMnvr = (state.mnvrTime > 0.0f || state.mnvrDeltaV > 0.0f);

    if (!hasMnvr) {
        if (_mnvrChromDrawn) { _mnvrChromDrawn = false; switchToScreen(screen_MNVR); }
        return;
    }
    if (!_mnvrChromDrawn) { switchToScreen(screen_MNVR); return; }

    // ── Derived values ────────────────────────────────────────────────────────────────
    float brgErr = _mnvrWrapErr(state.heading - state.mnvrHeading);
    float elvErr = state.pitch - state.mnvrPitch;
    float tIgn   = state.mnvrTime - state.mnvrDuration * 0.5f;

    // ── Update reticle marker ─────────────────────────────────────────────────────────
    _mnvrUpdateMarker(tft, brgErr, elvErr);

    // ── Right panel values ────────────────────────────────────────────────────────────
    uint8_t  NR = MNVR_RP_NR;
    uint16_t X  = MNVR_RP_X;
    uint16_t W  = MNVR_RP_W;
    char buf[20];
    uint16_t fg, bg;

    auto mnvrVal = [&](uint8_t row, uint8_t slot, const char *label, const String &val,
                       uint16_t fgc, uint16_t bgc) {
        RowCache &rc = rowCache[MNVR_SC][slot];
        if (rc.value == val && rc.fg == fgc && rc.bg == bgc) return;
        printValue(tft, MNVR_RP_F, X, rowYFor(row, NR), W, rowHFor(NR),
                   label, val, fgc, bgc, COL_BACK, printState[MNVR_SC][slot]);
        rc.value = val; rc.fg = fgc; rc.bg = bgc;
    };

    auto angCol = [](float e, uint16_t &fg, uint16_t &bg) {
        // Original screen colour logic — plain value colour, no alarm states
        fg = COL_VALUE; bg = COL_BACK;
    };

    // Row 0 — ΔV.Mnvr
    mnvrVal(0, 0, "\xCE\x94V.Mnvr:", fmtMs(state.mnvrDeltaV), TFT_DARK_GREEN, TFT_BLACK);

    // Row 1 — ΔV.Plan (total ΔV across all planned nodes; distinct from vessel total)
    mnvrVal(1, 1, "\xCE\x94V.Plan:", fmtMs(state.totalDeltaV), TFT_DARK_GREEN, TFT_BLACK);

    // Row 2 — ΔV.Stg (yellow if stage ΔV is tight relative to node)
    {
        bool tight = (state.stageDeltaV < state.mnvrDeltaV * MNVR_DV_MARGIN);
        mnvrVal(2, 2, "\xCE\x94V.Stg:", fmtMs(state.stageDeltaV),
                tight ? TFT_YELLOW : TFT_DARK_GREEN, TFT_BLACK);
    }

    // Row 3 — T+Ign (time to ignition = node time minus half burn duration)
    {
        if      (tIgn < 0.0f)              { fg = TFT_WHITE;      bg = TFT_RED;   }
        else if (tIgn < MNVR_TIGN_ALARM_S) { fg = TFT_WHITE;      bg = TFT_RED;   }
        else if (tIgn < MNVR_TIGN_WARN_S)  { fg = TFT_YELLOW;     bg = TFT_BLACK; }
        else                                { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
        String val = (state.mnvrDeltaV > 0.0f) ? formatTime((int64_t)tIgn) : "---";
        mnvrVal(3, 3, "T+Ign:", val, fg, bg);
    }

    // Row 4 — T+Mnvr (time to the node itself; yellow once burn should have started)
    {
        if      (state.mnvrTime < 0.0f)                      { fg = TFT_WHITE;      bg = TFT_RED;   }
        else if (state.mnvrTime < state.mnvrDuration * 0.5f) { fg = TFT_YELLOW;     bg = TFT_BLACK; }
        else                                                   { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
        String val = (state.mnvrDeltaV > 0.0f) ? formatTime((int64_t)state.mnvrTime) : "---";
        mnvrVal(4, 4, "T+Mnvr:", val, fg, bg);
    }

    // Row 5 — Burn dur
    {
        String val = (state.mnvrDuration > 0.0f) ? formatTime((int64_t)state.mnvrDuration) : "---";
        mnvrVal(5, 5, "Burn dur:", val, TFT_DARK_GREEN, TFT_BLACK);
    }

    // Row 6 — Nos.Brg | Nos.Elv split
    {
        uint16_t y6 = rowYFor(6, NR), h6 = rowHFor(NR), hw = W / 2;

        angCol(brgErr, fg, bg);
        snprintf(buf, sizeof(buf), "%+.0f\xB0", brgErr);
        {
            RowCache &rc = rowCache[MNVR_SC][6];
            String sv = buf;
            if (rc.value != sv || rc.fg != fg || rc.bg != bg) {
                printValue(tft, MNVR_RP_F, X,      y6, hw - ROW_PAD, h6,
                           "Brg:", sv, fg, bg, COL_BACK, printState[MNVR_SC][6]);
                rc.value = sv; rc.fg = fg; rc.bg = bg;
            }
        }

        angCol(elvErr, fg, bg);
        snprintf(buf, sizeof(buf), "%+.0f\xB0", elvErr);
        {
            RowCache &rc = rowCache[MNVR_SC][7];
            String sv = buf;
            if (rc.value != sv || rc.fg != fg || rc.bg != bg) {
                printValue(tft, MNVR_RP_F, X + hw, y6, hw - ROW_PAD, h6,
                           "Elv:", sv, fg, bg, COL_BACK, printState[MNVR_SC][7]);
                rc.value = sv; rc.fg = fg; rc.bg = bg;
            }
        }
    }

    // Row 7 — RCS | SAS buttons (to screen bottom, same height as DOCK)
    {
        uint16_t ry   = rowYFor(7, NR);
        uint16_t rh   = SCREEN_H - ry;
        uint16_t hw   = W / 2;
        uint16_t sasX = X + hw;
        uint16_t sasW = X + W - sasX;

        // RCS (slot 8)
        {
            bool rcsOn = state.rcs_on;
            String rcsStr = rcsOn ? "ON" : "OFF";
            RowCache &rc = rowCache[MNVR_SC][8];
            if (rc.value != rcsStr) {
                ButtonLabel btn = rcsOn
                    ? ButtonLabel{ "RCS", TFT_WHITE,     TFT_WHITE,     TFT_DARK_GREEN, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }
                    : ButtonLabel{ "RCS", TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK,  TFT_OFF_BLACK,  TFT_GREY, TFT_GREY };
                drawButton(tft, X, ry, hw, rh, btn, &Roboto_Black_20, false);
                rc.value = rcsStr;
            }
        }

        // SAS (slot 9)
        {
            const char *v; uint16_t sasFg, sasBg;
            switch (state.sasMode) {
                case 255: v = "SAS";  sasFg = TFT_DARK_GREY; sasBg = TFT_OFF_BLACK;  break;
                case 0:   v = "STAB"; sasFg = TFT_BLACK;     sasBg = TFT_SKY;        break;  // acceptable not ideal
                case 1:   v = "PRO";  sasFg = TFT_WHITE;     sasBg = TFT_RED;        break;
                case 2:   v = "RETR"; sasFg = TFT_WHITE;     sasBg = TFT_RED;        break;
                case 3:   v = "NRM";  sasFg = TFT_WHITE;     sasBg = TFT_RED;        break;
                case 4:   v = "ANRM"; sasFg = TFT_WHITE;     sasBg = TFT_RED;        break;
                case 5:   v = "RAD+"; sasFg = TFT_WHITE;     sasBg = TFT_RED;        break;
                case 6:   v = "RAD-"; sasFg = TFT_WHITE;     sasBg = TFT_RED;        break;
                case 7:   v = "TGT";  sasFg = TFT_WHITE;     sasBg = TFT_RED;        break;
                case 8:   v = "ATGT"; sasFg = TFT_WHITE;     sasBg = TFT_RED;        break;
                case 9:   v = "MNVR"; sasFg = TFT_WHITE;     sasBg = TFT_DARK_GREEN; break;  // correct
                default:  v = "SAS";  sasFg = TFT_DARK_GREY; sasBg = TFT_OFF_BLACK;  break;
            }
            RowCache &rc = rowCache[MNVR_SC][9];
            String sv = v;
            if (rc.value != sv || rc.fg != sasFg || rc.bg != sasBg) {
                ButtonLabel btn = { v, sasFg, sasFg, sasBg, sasBg, TFT_GREY, TFT_GREY };
                drawButton(tft, sasX, ry, sasW, rh, btn, &Roboto_Black_20, false);
                rc.value = sv; rc.fg = sasFg; rc.bg = sasBg;
            }
        }
    }

    // ΔV burn remaining bar
    _mnvrDrawDVBar(tft, state.mnvrDeltaV, state.stageDeltaV);

    // Redraw dividers last (printValue fillRects can erase them)
    { uint16_t dy = rowYFor(3,NR) - 1;   // after ΔV block
      tft.drawLine(X, dy,   X+W, dy,   TFT_GREY);
      tft.drawLine(X, dy+1, X+W, dy+1, TFT_GREY); }
    { uint16_t dy = rowYFor(6,NR) - 1;
      tft.drawLine(X, dy,   X+W, dy,   TFT_GREY);
      tft.drawLine(X, dy+1, X+W, dy+1, TFT_GREY); }
    { uint16_t dy = rowYFor(7,NR) - 1;
      tft.drawLine(X, dy,   X+W, dy,   TFT_GREY);
      tft.drawLine(X, dy+1, X+W, dy+1, TFT_GREY); }
    // Nos.Brg | Nos.Elv vertical split
    { uint16_t hw = W / 2, y6 = rowYFor(6,NR);
      tft.drawLine(X + hw,     y6, X + hw,     y6 + rowHFor(NR) - 1, TFT_GREY);
      tft.drawLine(X + hw + 1, y6, X + hw + 1, y6 + rowHFor(NR) - 1, TFT_GREY); }
    // Panel left border
    tft.drawLine(X - 2, TITLE_TOP, X - 2, SCREEN_H, TFT_GREY);
    tft.drawLine(X - 1, TITLE_TOP, X - 1, SCREEN_H, TFT_GREY);

    uint32_t dt = micros() - _t0;
    Serial.print("MNVR total="); Serial.print((float)dt/1000.0f, 2);
    Serial.print("ms  brg=");    Serial.print(brgErr, 1);
    Serial.print("  elv=");      Serial.print(elvErr, 1);
    Serial.print("  tIgn=");     Serial.println(tIgn, 0);
}
