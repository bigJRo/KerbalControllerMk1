/***************************************************************************************
   Screen_MNVR.ino -- Maneuver screen — chromeScreen_MNVR, drawScreen_MNVR
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


static void chromeScreen_MNVR(RA8875 &tft) {
  static const tFont   *F      = &Roboto_Black_40;
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t AX     = ROW_PAD + SECT_W;
  static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
  static const uint16_t AHW    = AW / 2;
  uint16_t rowH = rowHFor(NR);

  // Section labels: STATE(0-1), TIME(2-3), BURN(4-6)
  // HDG(7) drawn AFTER dividers so d3 isn't overwritten by its fillRect
  drawVerticalText(tft, 0, TITLE_TOP,          SECT_W, rowH*2, &Roboto_Black_16, "STATE", TFT_LIGHT_GREY, TFT_BLACK);
  drawVerticalText(tft, 0, TITLE_TOP + rowH*2, SECT_W, rowH*2, &Roboto_Black_16, "TIME",  TFT_LIGHT_GREY, TFT_BLACK);
  drawVerticalText(tft, 0, TITLE_TOP + rowH*4, SECT_W, rowH*3, &Roboto_Black_16, "BURN",  TFT_LIGHT_GREY, TFT_BLACK);

  // Row labels (right of section strip)
  printDispChrome(tft, F, AX, rowYFor(0,NR), AW, rowH, "Alt.SL:",          COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(1,NR), AW, rowH, "V.Orb:",           COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(2,NR), AW, rowH, "T+Ign:",           COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(3,NR), AW, rowH, "T+Mnvr:",          COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(4,NR), AW, rowH, "\xCE\x94V.Mnvr:", COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(5,NR), AW, rowH, "T.Burn:",          COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(6,NR), AW, rowH, "\xCE\x94V.Tot:",  COL_LABEL, COL_BACK, COL_NO_BDR);

  // Row 7 — split M.Hdg | M.Pit (AX-based geometry)
  {
    uint16_t y = rowYFor(7, NR), h = rowH;
    printDispChrome(tft, F, AX,                 y, AHW - ROW_PAD, h, "M.Hdg:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "M.Pitch:", COL_LABEL, COL_BACK, COL_NO_BDR);
    for (int8_t dx = -1; dx <= 1; dx++)
      tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, rowYFor(8,NR) - 1, TFT_GREY);
  }

  // Dividers — drawn LAST, +1 offset to clear printValue fillRect boundary
  uint16_t d1 = TITLE_TOP + rowH*2 + 1;  // after STATE (row1/row2)
  uint16_t d2 = TITLE_TOP + rowH*4 + 1;  // after TIME  (row3/row4)
  uint16_t d3 = TITLE_TOP + rowH*7 + 1;  // after BURN  (row6/row7)
  tft.drawLine(0, d1,   CONTENT_W, d1,   TFT_GREY);
  tft.drawLine(0, d1+1, CONTENT_W, d1+1, TFT_GREY);
  tft.drawLine(0, d2,   CONTENT_W, d2,   TFT_GREY);
  tft.drawLine(0, d2+1, CONTENT_W, d2+1, TFT_GREY);
  tft.drawLine(0, d3,   CONTENT_W, d3,   TFT_GREY);
  tft.drawLine(0, d3+1, CONTENT_W, d3+1, TFT_GREY);
  // HDG drawn last — start 2px below d3 to avoid overwriting the divider
  uint16_t hdgY = TITLE_TOP + rowH*7 + 2;
  uint16_t hdgH = rowH - 2;
  drawVerticalText(tft, 0, hdgY, SECT_W, hdgH, &Roboto_Black_16, "HD", TFT_LIGHT_GREY, TFT_BLACK);
}



static void drawScreen_MNVR(RA8875 &tft) {
  static const tFont   *F      = &Roboto_Black_40;
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t AX     = ROW_PAD + SECT_W;
  static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
  static const uint16_t AHW    = AW / 2;
  uint16_t fg, bg;

  // Cache-checked draw helper using section-label-aware geometry
  auto mnvrVal = [&](uint8_t row, const char *label, const String &val,
                     uint16_t fgc, uint16_t bgc) {
    RowCache &rc = rowCache[4][row];
    if (rc.value == val && rc.fg == fgc && rc.bg == bgc) return;
    printValue(tft, F, AX, rowYFor(row, NR), AW, rowHFor(NR),
               label, val, fgc, bgc, COL_BACK, printState[4][row]);
    rc.value = val; rc.fg = fgc; rc.bg = bgc;
  };

  // hasMnvr: a maneuver node exists only when Simpit reports a non-zero burn.
  // All rows show "---" when no node is planned — Alt.SL and V.Orb included,
  // since they are only meaningful as situational context during a burn.
  bool hasMnvr = (state.mnvrDeltaV > 0.0f);

  // ── STATE block (rows 0-1) ──

  // Row 0 — Altitude ASL
  if (!hasMnvr) {
    mnvrVal(0, "Alt.SL:", "---", TFT_DARK_GREY, TFT_BLACK);
  } else {
    fg = (state.altitude < 0) ? TFT_RED : TFT_DARK_GREEN;
    mnvrVal(0, "Alt.SL:", formatAlt(state.altitude), fg, TFT_BLACK);
  }

  // Row 1 — Orbital velocity
  if (!hasMnvr) {
    mnvrVal(1, "V.Orb:", "---", TFT_DARK_GREY, TFT_BLACK);
  } else {
    fg = (state.orbitalVel < 0) ? TFT_RED : TFT_DARK_GREEN;
    mnvrVal(1, "V.Orb:", fmtMs(state.orbitalVel), fg, TFT_BLACK);
  }

  // ── TIME block (rows 2-3) ──

  // Row 2 — Time to ignition: T+Mnvr minus half burn duration
  //   white-on-red if negative (should have lit already)
  //   yellow if < 60s (get ready to light)
  if (!hasMnvr) {
    mnvrVal(2, "T+Ign:",  "---", TFT_DARK_GREY, TFT_BLACK);
  } else {
    float tIgn = state.mnvrTime - (state.mnvrDuration / 2.0f);
    if      (tIgn < 0.0f)             { fg = TFT_WHITE;     bg = TFT_RED;   }
    else if (tIgn < MNVR_TIGN_WARN_S) { fg = TFT_YELLOW;    bg = TFT_BLACK; }
    else                              { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    mnvrVal(2, "T+Ign:", formatTime(tIgn), fg, bg);
  }

  // Row 3 — Time to maneuver node:
  //   white-on-red if negative (past node)
  //   yellow if < half burn duration (burn should have started)
  if (!hasMnvr) {
    mnvrVal(3, "T+Mnvr:", "---", TFT_DARK_GREY, TFT_BLACK);
  } else {
    float halfBurn = state.mnvrDuration / 2.0f;
    if      (state.mnvrTime < 0)        { fg = TFT_WHITE;     bg = TFT_RED;   }
    else if (state.mnvrTime < halfBurn) { fg = TFT_YELLOW;    bg = TFT_BLACK; }
    else                                { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    mnvrVal(3, "T+Mnvr:", formatTime(state.mnvrTime), fg, bg);
  }

  // ── BURN block (rows 4-6) ──

  // Row 4 — Maneuver ΔV required
  mnvrVal(4, "\xCE\x94V.Mnvr:", hasMnvr ? fmtMs(state.mnvrDeltaV) : "---",
          hasMnvr ? TFT_DARK_GREEN : TFT_DARK_GREY, TFT_BLACK);

  // Row 5 — Burn duration
  mnvrVal(5, "T.Burn:", hasMnvr ? formatTime(state.mnvrDuration) : "---",
          hasMnvr ? TFT_DARK_GREEN : TFT_DARK_GREY, TFT_BLACK);

  // Row 6 — Total ΔV remaining: suppress when no maneuver planned.
  if (!hasMnvr) {
    mnvrVal(6, "\xCE\x94V.Tot:", "---", TFT_DARK_GREY, TFT_BLACK);
  } else {
    if      (state.totalDeltaV < state.mnvrDeltaV)                    { fg = TFT_WHITE;     bg = TFT_RED;   }
    else if (state.totalDeltaV < state.mnvrDeltaV * MNVR_DV_MARGIN)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
    else                                                               { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    mnvrVal(6, "\xCE\x94V.Tot:", fmtMs(state.totalDeltaV), fg, bg);
  }

  // ── HDG block (row 7): burn direction reference ──
  // Split: M.Hdg (left, cache[4][7]) | M.Pitch (right, cache[4][8])
  {
    uint16_t y7 = rowYFor(7, NR), h7 = rowHFor(NR);
    char buf[10];

    if (!hasMnvr) {
      RowCache &hc = rowCache[4][7];
      if (hc.value != "---") {
        printValue(tft, F, AX, y7, AHW - ROW_PAD, h7,
                   "M.Hdg:", "---", TFT_DARK_GREY, COL_BACK, COL_BACK,
                   printState[4][7]);
        hc.value = "---"; hc.fg = TFT_DARK_GREY; hc.bg = COL_BACK;
      }
      RowCache &pc = rowCache[4][8];
      if (pc.value != "---") {
        printValue(tft, F, AX + AHW + ROW_PAD, y7, AHW - ROW_PAD, h7,
                   "M.Pitch:", "---", TFT_DARK_GREY, COL_BACK, COL_BACK,
                   printState[4][8]);
        pc.value = "---"; pc.fg = TFT_DARK_GREY; pc.bg = COL_BACK;
      }
    } else {
      snprintf(buf, sizeof(buf), "%.1f\xB0", state.mnvrHeading);
      String hdgStr = buf;
      RowCache &hc = rowCache[4][7];
      if (hc.value != hdgStr || hc.fg != COL_VALUE || hc.bg != COL_BACK) {
        printValue(tft, F, AX, y7, AHW - ROW_PAD, h7,
                   "M.Hdg:", hdgStr, COL_VALUE, COL_BACK, COL_BACK,
                   printState[4][7]);
        hc.value = hdgStr; hc.fg = COL_VALUE; hc.bg = COL_BACK;
      }

      snprintf(buf, sizeof(buf), "%.1f\xB0", state.mnvrPitch);
      String pitStr = buf;
      RowCache &pc = rowCache[4][8];
      if (pc.value != pitStr || pc.fg != COL_VALUE || pc.bg != COL_BACK) {
        printValue(tft, F, AX + AHW + ROW_PAD, y7, AHW - ROW_PAD, h7,
                   "M.Pitch:", pitStr, COL_VALUE, COL_BACK, COL_BACK,
                   printState[4][8]);
        pc.value = pitStr; pc.fg = COL_VALUE; pc.bg = COL_BACK;
      }
    }
  }
}

