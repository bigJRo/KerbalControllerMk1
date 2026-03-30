/***************************************************************************************
   Screen_ORB.ino -- Orbit screen — chromeScreen_ORB, drawScreen_ORB
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


static void chromeScreen_ORB(RA8875 &tft) {
  static const tFont   *F      = &Roboto_Black_40;
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t AX     = ROW_PAD + SECT_W;
  static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
  static const uint16_t AHW    = AW / 2;
  uint16_t rowH = rowHFor(NR);

  // Section labels: SHAPE(0-1), APSE(2-3), PLANE(4-6)
  // AN(7) and PR(after d3) drawn after dividers
  drawVerticalText(tft, 0, TITLE_TOP,          SECT_W, rowH*2, &Roboto_Black_16, "SHAPE", TFT_LIGHT_GREY, TFT_BLACK);
  drawVerticalText(tft, 0, TITLE_TOP + rowH*2, SECT_W, rowH*2, &Roboto_Black_16, "APSE",  TFT_LIGHT_GREY, TFT_BLACK);
  drawVerticalText(tft, 0, TITLE_TOP + rowH*4, SECT_W, rowH*2, &Roboto_Black_16, "PLANE", TFT_LIGHT_GREY, TFT_BLACK);

  // Row labels (right of section strip)
  // SHAPE
  printDispChrome(tft, F, AX, rowYFor(0,NR), AW, rowH, "Ecc:",    COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(1,NR), AW, rowH, "SMA:",    COL_LABEL, COL_BACK, COL_NO_BDR);
  // APSE
  printDispChrome(tft, F, AX, rowYFor(2,NR), AW, rowH, "ApA:",    COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(3,NR), AW, rowH, "PeA:",    COL_LABEL, COL_BACK, COL_NO_BDR);
  // PLANE: Inc|LAN split then Arg.Pe
  {
    uint16_t y = rowYFor(4, NR), h = rowH;
    printDispChrome(tft, F, AX,                 y, AHW - ROW_PAD, h, "Inc:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "LAN:", COL_LABEL, COL_BACK, COL_NO_BDR);
    for (int8_t dx = -1; dx <= 1; dx++)
      tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, rowYFor(5,NR) - 1, TFT_GREY);
  }
  printDispChrome(tft, F, AX, rowYFor(5,NR), AW, rowH, "Arg.Pe:", COL_LABEL, COL_BACK, COL_NO_BDR);
  // AN: T.Anom|M.Anom split
  {
    uint16_t y = rowYFor(6, NR), h = rowH;
    printDispChrome(tft, F, AX,                 y, AHW - ROW_PAD, h, "True:",   COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "Mean:",   COL_LABEL, COL_BACK, COL_NO_BDR);
    for (int8_t dx = -1; dx <= 1; dx++)
      tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, rowYFor(7,NR) - 1, TFT_GREY);
  }
  // PR
  printDispChrome(tft, F, AX, rowYFor(7,NR), AW, rowH, "Period:", COL_LABEL, COL_BACK, COL_NO_BDR);

  // Dividers — drawn LAST, +1 offset
  uint16_t d1   = TITLE_TOP + rowH*2 + 1;  // after SHAPE  (row1/row2)
  uint16_t d2   = TITLE_TOP + rowH*4 + 1;  // after APSE   (row3/row4)
  uint16_t d_mid = TITLE_TOP + rowH*6 + 1; // after PLANE  (row5/row6)
  uint16_t d3   = TITLE_TOP + rowH*7 + 1;  // after AN     (row6/row7)
  tft.drawLine(0, d1,     CONTENT_W, d1,     TFT_GREY);
  tft.drawLine(0, d1+1,   CONTENT_W, d1+1,   TFT_GREY);
  tft.drawLine(0, d2,     CONTENT_W, d2,     TFT_GREY);
  tft.drawLine(0, d2+1,   CONTENT_W, d2+1,   TFT_GREY);
  tft.drawLine(0, d_mid,  CONTENT_W, d_mid,  TFT_GREY);
  tft.drawLine(0, d_mid+1,CONTENT_W, d_mid+1,TFT_GREY);
  tft.drawLine(0, d3,     CONTENT_W, d3,     TFT_GREY);
  tft.drawLine(0, d3+1,   CONTENT_W, d3+1,   TFT_GREY);
  // AN and PR labels drawn after their bounding dividers
  drawVerticalText(tft, 0, TITLE_TOP + rowH*6 + 2, SECT_W, rowH - 4, &Roboto_Black_16, "AN", TFT_LIGHT_GREY, TFT_BLACK);
  drawVerticalText(tft, 0, TITLE_TOP + rowH*7 + 2, SECT_W, rowH - 4, &Roboto_Black_16, "PR", TFT_LIGHT_GREY, TFT_BLACK);
}

// ATT screen orbital mode helper — defined here so chromeScreen_ATT and drawScreen_ATT
// can both call it. Uses same hysteresis thresholds as LNCH screen.


static void drawScreen_ORB(RA8875 &tft) {
  static const tFont   *F      = &Roboto_Black_40;
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t AX     = ROW_PAD + SECT_W;
  static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
  static const uint16_t AHW    = AW / 2;
  char buf[16];
  uint16_t fg, bg;

  // Cache-checked draw helper using section-label-aware geometry
  auto orbVal = [&](uint8_t row, const char *label, const String &val,
                    uint16_t fgc, uint16_t bgc) {
    RowCache &rc = rowCache[6][row];
    if (rc.value == val && rc.fg == fgc && rc.bg == bgc) return;
    printValue(tft, F, AX, rowYFor(row, NR), AW, rowHFor(NR),
               label, val, fgc, bgc, COL_BACK, printState[6][row]);
    rc.value = val; rc.fg = fgc; rc.bg = bgc;
  };

  float warnAlt = max(currentBody.minSafe, currentBody.flyHigh);

  // ── SHAPE block (rows 0-1) ──

  // Row 0 — Eccentricity: yellow >0.9 (highly elliptical), red >1.0 (escape)
  snprintf(buf, sizeof(buf), "%.4f", state.eccentricity);
  if      (state.eccentricity > ORB_ECC_ALARM) { fg = TFT_WHITE;     bg = TFT_RED;   }
  else if (state.eccentricity > ORB_ECC_WARN)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
  else                                { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
  orbVal(0, "Ecc:", buf, fg, bg);

  // Row 1 — Semi-major axis (always green)
  orbVal(1, "SMA:", formatAlt(state.semiMajorAxis), TFT_DARK_GREEN, TFT_BLACK);

  // ── APSE block (rows 2-3) ──

  // Row 2 — Apoapsis
  {
    bool apaWarn = (warnAlt > 0 && state.apoapsis > 0 && state.apoapsis < warnAlt);
    if      (state.apoapsis < 0) fg = TFT_RED;
    else if (apaWarn)            fg = TFT_YELLOW;
    else                         fg = TFT_DARK_GREEN;
    orbVal(2, "ApA:", formatAlt(state.apoapsis), fg, TFT_BLACK);
  }

  // Row 3 — Periapsis
  {
    bool peWarn = (warnAlt > 0 && state.periapsis > 0 && state.periapsis < warnAlt);
    if      (state.periapsis < 0) fg = TFT_RED;
    else if (peWarn)              fg = TFT_YELLOW;
    else                          fg = TFT_DARK_GREEN;
    orbVal(3, "PeA:", formatAlt(state.periapsis), fg, TFT_BLACK);
  }

  // ── PLANE block (rows 4-6): Inc|LAN split, Arg.Pe, then AN (T.Anom|M.Anom) ──

  // Row 4 — Split: Inc (left, cache[6][5]) | LAN (right, cache[6][9])
  {
    uint16_t y = rowYFor(4, NR), h = rowHFor(NR);

    snprintf(buf, sizeof(buf), "%.2f\xB0", state.inclination);
    String incStr = buf;
    RowCache &ic = rowCache[6][11];
    if (ic.value != incStr || ic.fg != COL_VALUE || ic.bg != COL_BACK) {
      printValue(tft, F, AX, y, AHW - ROW_PAD, h,
                 "Inc:", incStr, COL_VALUE, COL_BACK, COL_BACK,
                 printState[6][11]);
      ic.value = incStr; ic.fg = COL_VALUE; ic.bg = COL_BACK;
    }

    snprintf(buf, sizeof(buf), "%.2f\xB0", state.LAN);
    String lanStr = buf;
    RowCache &lc = rowCache[6][9];
    if (lc.value != lanStr || lc.fg != COL_VALUE || lc.bg != COL_BACK) {
      printValue(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h,
                 "LAN:", lanStr, COL_VALUE, COL_BACK, COL_BACK,
                 printState[6][9]);
      lc.value = lanStr; lc.fg = COL_VALUE; lc.bg = COL_BACK;
    }
  }

  // Row 5 — Argument of periapsis
  snprintf(buf, sizeof(buf), "%.2f\xB0", state.argOfPe);
  orbVal(5, "Arg.Pe:", buf, TFT_DARK_GREEN, TFT_BLACK);

  // Row 6 — Split: T.Anom (left, cache[6][6]) | M.Anom (right, cache[6][10])
  {
    uint16_t y = rowYFor(6, NR), h = rowHFor(NR);

    snprintf(buf, sizeof(buf), "%.2f\xB0", state.trueAnomaly);
    String taStr = buf;
    RowCache &tc = rowCache[6][6];
    if (tc.value != taStr || tc.fg != COL_VALUE || tc.bg != COL_BACK) {
      printValue(tft, F, AX, y, AHW - ROW_PAD, h,
                 "True:", taStr, COL_VALUE, COL_BACK, COL_BACK,
                 printState[6][6]);
      tc.value = taStr; tc.fg = COL_VALUE; tc.bg = COL_BACK;
    }

    snprintf(buf, sizeof(buf), "%.2f\xB0", state.meanAnomaly);
    String maStr = buf;
    RowCache &mc = rowCache[6][10];
    if (mc.value != maStr || mc.fg != COL_VALUE || mc.bg != COL_BACK) {
      printValue(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h,
                 "Mean:", maStr, COL_VALUE, COL_BACK, COL_BACK,
                 printState[6][10]);
      mc.value = maStr; mc.fg = COL_VALUE; mc.bg = COL_BACK;
    }
  }

  // Row 7 — Orbital period (PR block, always green)
  orbVal(7, "Period:", formatTime(state.orbitalPeriod), TFT_DARK_GREEN, TFT_BLACK);
}

