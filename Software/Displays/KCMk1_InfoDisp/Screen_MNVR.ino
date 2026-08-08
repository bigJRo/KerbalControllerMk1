/***************************************************************************************
   Screen_MNVR.ino  —  Maneuver screen: burn alignment reticle + node data

   LAYOUT (1024×600, content area 940×538 below title bar; reticle R=210)
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

   RETICLE GEOMETRY (rev-2, 1024×600)
   ─────────────────
   Centre: (289, 295)  Radius: 225px  Scale: 11.25 px/deg (±20° full scale)
   Rings: 5°=56px  10°=112px  15°=168px  20°=225px. Centred in the left region
   x=[0,578]; ΔV bar centred under it.

   PANEL: 360 px wide at x=580 (matches ascent/circ), NR=8 rows of 67 px,
   labels Black_28, values Black_36
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


// ── Reticle geometry — centred and stretched to fill the left region ─────────────────
// The readout panel now sits on the far right (x=580, matching the ascent/circ
// panel), leaving x=[0,578] for the reticle. The reticle is centred in that band
// and enlarged; the ΔV bar is centred under it.
static const uint16_t MNVR_CX    = RETICLE_CX;   // centre of the left region x=[0,578]
static const uint16_t MNVR_CY    = RETICLE_CY;   // nudged down for more whitespace above
static const uint16_t MNVR_R     = RETICLE_R;    // slightly reduced so top gap grows to ~28 px
static const float    MNVR_SCALE = (float)MNVR_R / 20.0f;   // 10.5 px/deg, ±20° full scale

static const uint16_t MNVR_RING_5  = MNVR_R / 4;         // 56
static const uint16_t MNVR_RING_10 = MNVR_R / 2;         // 112
static const uint16_t MNVR_RING_15 = (MNVR_R * 3) / 4;   // 168
static const uint16_t MNVR_RING_20 = MNVR_R;             // 225

// Marker sizing — diamond and box scaled up with the larger reticle
static const uint8_t MNVR_MRK_DS  = 26;   // maneuver marker prong length
static const uint8_t MNVR_BOX_R   = 23;   // alignment box half-size (DS + ~7px clearance)
static const uint8_t MNVR_ERASE_R = 29;   // erase rect half-size (covers prong 26 + cap)

// Right panel geometry — matches the ascent/circ readout panel: 360 px wide,
// right-aligned to the content edge, labels Black_28, values Black_36.
static const uint16_t MNVR_RP_W   = RETICLE_RP_W;
static const uint16_t MNVR_RP_X   = SCREEN_W - SIDEBAR_W - MNVR_RP_W;   // 580
static const uint8_t  MNVR_RP_NR  = 8;
static const tFont   *MNVR_RP_LBL = &Roboto_Black_28;   // row labels (chrome)
static const tFont   *MNVR_RP_F   = &Roboto_Black_36;   // row values

// ΔV bar geometry — centred under the reticle
static const uint16_t MNVR_BAR_W = RETICLE_BAR_W;
static const uint16_t MNVR_BAR_X = MNVR_CX - MNVR_BAR_W / 2;   // 64
static const uint16_t MNVR_BAR_H = 26;

// Screen cache index
static const uint8_t MNVR_SC = (uint8_t)screen_MNVR;   // 3


// ── Previous marker state ─────────────────────────────────────────────────────────────
static int16_t _mnvrPrevMrkX    = 9999, _mnvrPrevMrkY = 9999;
static bool    _mnvrPrevAligned = false;
static float   _mnvrPrevDV      = -999.0f;   // ΔV-bar dedup; reset in chrome on re-entry


// ── Wrap heading error to ±180° ───────────────────────────────────────────────────────
static inline float _mnvrWrapErr(float e) { return eadiHdgDelta(e, 0.0f); }


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
static void _mnvrDrawReticleChrome(KCM_TFT &tft) {
    // Shared disc + rings + cardinals + crosshair + ticks + bezel (MNVR/DOCK gap 18, tick 14)
    reticleDrawBase(tft, MNVR_CX, MNVR_CY, MNVR_R, 18, 14);

    // Ring degree labels (NE quadrant, just inside each ring). Transparent bg.
    tft.setFont(Roboto_Black_16);
    tft.setTextColor(TFT_LIGHT_GREY);
    tft.setCursor(MNVR_CX + 3, MNVR_CY - MNVR_RING_5  + 3);  tft.print("5");
    tft.setCursor(MNVR_CX + 3, MNVR_CY - MNVR_RING_10 + 3);  tft.print("10");
    tft.setCursor(MNVR_CX + 3, MNVR_CY - MNVR_RING_15 + 3);  tft.print("15");
    tft.setCursor(MNVR_CX + 3, MNVR_CY - MNVR_RING_20 + 3);  tft.print("20");

    // Legend — top-left corner
    static const uint16_t LEG_X  = 6;
    static const uint16_t LEG_Y0 = TITLE_TOP + 6;
    tft.setFont(Roboto_Black_16);

    // MANEUVER — blue maneuver marker
    drawManeuverMarker(tft, LEG_X + 6, LEG_Y0 + 6, 5, TFT_BLUE);
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

    // ΔV bar chrome — label/value in Black_24, centred under the reticle. lblY
    // gives the Black_24 text (cap 29) ~5 px of clearance above the bar border.
    uint16_t barY = MNVR_CY + MNVR_R + 42;
    uint16_t lblY = barY - 34;
    tft.setFont(Roboto_Black_24);
    tft.setTextColor(TFT_LIGHT_GREY, TFT_BLACK);
    tft.setCursor(MNVR_BAR_X, lblY);
    tft.print("\xCE\x94V Burn");
    tft.drawRect(MNVR_BAR_X, barY, MNVR_BAR_W, MNVR_BAR_H, TFT_GREY);
}


// ── Draw right-panel chrome ───────────────────────────────────────────────────────────
static void _mnvrDrawRightChrome(KCM_TFT &tft) {
    uint8_t  NR = MNVR_RP_NR;
    uint16_t X  = MNVR_RP_X;
    uint16_t W  = MNVR_RP_W;

    printDispChrome(tft, MNVR_RP_LBL, X, rowYFor(0,NR), W, rowHFor(NR), "\xCE\x94V.Mnvr:",  COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, MNVR_RP_LBL, X, rowYFor(1,NR), W, rowHFor(NR), "\xCE\x94V.Plan:",  COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, MNVR_RP_LBL, X, rowYFor(2,NR), W, rowHFor(NR), "\xCE\x94V.Stg:",   COL_LABEL, COL_BACK, COL_NO_BDR);

    // Divider: ΔV block / timing block
    { uint16_t dy = rowYFor(3,NR) - 1;
      tft.drawLine(X, dy,   X+W, dy,   TFT_GREY);
      tft.drawLine(X, dy+1, X+W, dy+1, TFT_GREY); }

    printDispChrome(tft, MNVR_RP_LBL, X, rowYFor(3,NR), W, rowHFor(NR), "T+Ign:",   COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, MNVR_RP_LBL, X, rowYFor(4,NR), W, rowHFor(NR), "T+Mnvr:",  COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, MNVR_RP_LBL, X, rowYFor(5,NR), W, rowHFor(NR), "Burn Dur:",COL_LABEL, COL_BACK, COL_NO_BDR);

    // Divider: timing block / alignment block
    { uint16_t dy = rowYFor(6,NR) - 1;
      tft.drawLine(X, dy,   X+W, dy,   TFT_GREY);
      tft.drawLine(X, dy+1, X+W, dy+1, TFT_GREY); }

    // Row 6: Nos.Brg | Nos.Elv split
    {
        uint16_t y = rowYFor(6,NR), h = rowHFor(NR), hw = W / 2;
        printDispChrome(tft, MNVR_RP_LBL, X,      y, hw - ROW_PAD, h, "Brg:", COL_LABEL, COL_BACK, COL_NO_BDR);
        printDispChrome(tft, MNVR_RP_LBL, X + hw, y, hw - ROW_PAD, h, "Elv:", COL_LABEL, COL_BACK, COL_NO_BDR);
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
static void _mnvrRepairChrome(KCM_TFT &tft, int16_t bx, int16_t by, uint8_t bh) {
    int16_t boxX0=bx, boxX1=bx+2*bh, boxY0=by, boxY1=by+2*bh;
    float d = reticleRepair(tft, MNVR_CX, MNVR_CY, MNVR_R, 18, bx, by, bh);

    // Redraw the ring label(s) the erase box overlapped, plus the innermost one if
    // the good-zone refill (radius R/4) just painted over it (marker inside it).
    static const uint16_t lblR[]   = {MNVR_RING_5, MNVR_RING_10, MNVR_RING_15, MNVR_RING_20};
    static const char    *lblTxt[] = {"5", "10", "15", "20"};
    bool fontSet = false;
    for (uint8_t i = 0; i < 4; i++) {
        int16_t lx = MNVR_CX + 3, ly = MNVR_CY - lblR[i] + 3;
        bool boxHit      = (boxX1 >= lx && boxX0 <= lx + 26 && boxY1 >= ly && boxY0 <= ly + 20);
        bool goodZoneHit = (i == 0 && d <= (float)(MNVR_R / 4));
        if (boxHit || goodZoneHit) {
            if (!fontSet) { tft.setFont(Roboto_Black_16); tft.setTextColor(TFT_LIGHT_GREY); fontSet = true; }
            tft.setCursor(lx, ly);
            tft.print(lblTxt[i]);
        }
    }
}


// ── Update burn vector marker ─────────────────────────────────────────────────────────
// Diamond always TFT_BLUE. Neon-green alignment box appears when error < 5°.
static void _mnvrUpdateMarker(KCM_TFT &tft, float brgErr, float elvErr) {
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
        // KSP maneuver marker: dot + three T-capped prongs
        drawManeuverMarker(tft, sx, sy, DS, TFT_BLUE);
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
static void _mnvrDrawDVBar(KCM_TFT &tft, float dvNode, float dvStage) {
    // Layout matches chrome: barY = CY+R+42, lblY = barY-34 (Black_24 label).
    static const uint16_t barY = MNVR_CY + MNVR_R + 42;
    static const uint16_t lblY = barY - 34;

    if (fabsf(dvNode - _mnvrPrevDV) < 1.0f) return;
    _mnvrPrevDV = dvNode;

    float    maxDV    = fmaxf(dvStage, dvNode);
    bool     tight    = (dvStage < dvNode * MNVR_DV_MARGIN);
    uint16_t barCol   = tight ? TFT_YELLOW : TFT_DARK_GREEN;
    float    fraction = (maxDV > 0.0f) ? fminf(dvNode / maxDV, 1.0f) : 0.0f;
    uint16_t fillW    = (uint16_t)(fraction * (MNVR_BAR_W - 2));

    tft.fillRect(MNVR_BAR_X + 1, barY + 1, MNVR_BAR_W - 2, MNVR_BAR_H - 2, TFT_OFF_BLACK);
    if (fillW > 0)
        tft.fillRect(MNVR_BAR_X + 1, barY + 1, fillW, MNVR_BAR_H - 2, barCol);

    // Right-aligned value text in Black_24 (matches "ΔV Burn" label font).
    char buf[14];
    snprintf(buf, sizeof(buf), "%.0fm/s", dvNode);
    tft.setFont(Roboto_Black_24);
    tft.setTextColor(barCol, TFT_BLACK);
    // Clear the value-text region: right half of bar width, 30 px tall (Black_24).
    tft.fillRect(MNVR_BAR_X + MNVR_BAR_W/2, lblY, MNVR_BAR_W/2, 30, TFT_BLACK);
    int16_t tw = getFontStringWidth(&Roboto_Black_24, buf);
    tft.setCursor(MNVR_BAR_X + MNVR_BAR_W - tw, lblY);
    tft.print(buf);
}


// ── CHROME ────────────────────────────────────────────────────────────────────────────
void chromeScreen_MNVR(KCM_TFT &tft) {
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
    _mnvrPrevDV      = -999.0f;   // force ΔV bar + value to repaint on screen entry

    tft.fillRect(0, TITLE_TOP, MNVR_RP_X, SCREEN_H - TITLE_TOP, TFT_BLACK);
    _mnvrDrawReticleChrome(tft);
    _mnvrDrawRightChrome(tft);

    for (uint8_t r = 0; r < ROW_COUNT; r++) rowCache[MNVR_SC][r].value = "\x01";
    for (uint8_t r = 0; r < ROW_COUNT; r++) printState[MNVR_SC][r] = PrintState{};
}


// ── DRAW (called every loop) ──────────────────────────────────────────────────────────
void drawScreen_MNVR(KCM_TFT &tft) {
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
        drawPanelValue(tft, MNVR_SC, slot, row, X, W, label, val, fgc, bgc, MNVR_RP_F, NR, false);
    };

    auto angCol = [](float e, uint16_t &fg, uint16_t &bg) {
        // Original screen colour logic — plain value colour, no alarm states
        fg = COL_VALUE; bg = COL_BACK;
    };

    // Row 0 — ΔV.Mnvr
    mnvrVal(0, 0, "\xCE\x94V.Mnvr:", fmtMs(state.mnvrDeltaV), TFT_DARK_GREEN, TFT_BLACK);

    // Row 1 — ΔV.Plan (total ΔV across all planned nodes; distinct from vessel total)
    mnvrVal(1, 1, "\xCE\x94V.Plan:", fmtMs(state.mnvrTotalDeltaV), TFT_DARK_GREEN, TFT_BLACK);

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
        String val = (state.mnvrDeltaV > 0.0f) ? formatTimeCompact((int64_t)tIgn) : "---";
        mnvrVal(3, 3, "T+Ign:", val, fg, bg);
    }

    // Row 4 — T+Mnvr (time to the node itself; yellow once burn should have started)
    {
        if      (state.mnvrTime < 0.0f)                      { fg = TFT_WHITE;      bg = TFT_RED;   }
        else if (state.mnvrTime < state.mnvrDuration * 0.5f) { fg = TFT_YELLOW;     bg = TFT_BLACK; }
        else                                                   { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
        String val = (state.mnvrDeltaV > 0.0f) ? formatTimeCompact((int64_t)state.mnvrTime) : "---";
        mnvrVal(4, 4, "T+Mnvr:", val, fg, bg);
    }

    // Row 5 — Burn dur
    {
        String val = (state.mnvrDuration > 0.0f) ? formatTimeCompact((int64_t)state.mnvrDuration) : "---";
        mnvrVal(5, 5, "Burn Dur:", val, TFT_DARK_GREEN, TFT_BLACK);
    }

    // Row 6 — Nos.Brg | Nos.Elv split
    {
        uint16_t y6 = rowYFor(6, NR), h6 = rowHFor(NR), hw = W / 2;

        angCol(brgErr, fg, bg);
        // Integer degrees so the value fits the half-cell at the panel's Black_36
        // (a decimal would overflow at large angles).
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
                drawButton(tft, X, ry, hw, rh, btn, MNVR_RP_LBL, false);
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
                drawButton(tft, sasX, ry, sasW, rh, btn, MNVR_RP_LBL, false);
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

    if (debugMode) {
        uint32_t dt = micros() - _t0;
        Serial.print("MNVR total="); Serial.print((float)dt/1000.0f, 2);
        Serial.print("ms  brg=");    Serial.print(brgErr, 1);
        Serial.print("  elv=");      Serial.print(elvErr, 1);
        Serial.print("  tIgn=");     Serial.println(tIgn, 0);
    }
}
