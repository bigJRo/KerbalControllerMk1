/***************************************************************************************
   Screen_RNDZ.ino -- Rendezvous screen — chromeScreen_RNDZ, drawScreen_RNDZ
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


bool _rndzChromDrawn = false;

static void chromeScreen_RNDZ(RA8875 &tft) {
  if (state.targetAvailable) {
    _rndzChromDrawn = true;
    static const tFont   *F      = &Roboto_Black_40;
    static const uint8_t  NR     = 8;
    static const uint16_t SECT_W = 26;
    static const uint16_t AX     = ROW_PAD + SECT_W;
    static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
    uint16_t rowH = rowHFor(NR);

    // Section labels: STATE(0-1), TGT(2-3), INT1(4-5), INT2(6-7)
    drawVerticalText(tft, 0, TITLE_TOP,          SECT_W, rowH*2, &Roboto_Black_16, "STATE", TFT_LIGHT_GREY, TFT_BLACK);
    drawVerticalText(tft, 0, TITLE_TOP + rowH*2, SECT_W, rowH*2, &Roboto_Black_16, "TGT",   TFT_LIGHT_GREY, TFT_BLACK);
    drawVerticalText(tft, 0, TITLE_TOP + rowH*4, SECT_W, rowH*2, &Roboto_Black_16, "INT1",  TFT_LIGHT_GREY, TFT_BLACK);
    drawVerticalText(tft, 0, TITLE_TOP + rowH*6, SECT_W, rowH*2, &Roboto_Black_16, "INT2",  TFT_LIGHT_GREY, TFT_BLACK);

    // Row labels (right of section strip)
    printDispChrome(tft, F, AX, rowYFor(0,NR), AW, rowH, "Alt.SL:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(1,NR), AW, rowH, "V.Orb:",  COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(2,NR), AW, rowH, "Dist:",   COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(3,NR), AW, rowH, "V.Tgt:",  COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(4,NR), AW, rowH, "Time:",   COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(5,NR), AW, rowH, "Dist:",   COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(6,NR), AW, rowH, "Time:",   COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(7,NR), AW, rowH, "Dist:",   COL_LABEL, COL_BACK, COL_NO_BDR);

    // Dividers — drawn LAST, +1 offset
    uint16_t d1 = TITLE_TOP + rowH*2 + 1;  // after STATE (row1/row2)
    uint16_t d2 = TITLE_TOP + rowH*4 + 1;  // after TGT   (row3/row4)
    uint16_t d3 = TITLE_TOP + rowH*6 + 1;  // after INT1  (row5/row6)
    tft.drawLine(0, d1,   CONTENT_W, d1,   TFT_GREY);
    tft.drawLine(0, d1+1, CONTENT_W, d1+1, TFT_GREY);
    tft.drawLine(0, d2,   CONTENT_W, d2,   TFT_GREY);
    tft.drawLine(0, d2+1, CONTENT_W, d2+1, TFT_GREY);
    tft.drawLine(0, d3,   CONTENT_W, d3,   TFT_GREY);
    tft.drawLine(0, d3+1, CONTENT_W, d3+1, TFT_GREY);

  } else {
    _rndzChromDrawn = false;
    // Clear content area — no chrome needed for NO TARGET state
    tft.fillRect(0, TITLE_TOP, CONTENT_W, SCREEN_H - TITLE_TOP, TFT_BLACK);
    textCenter(tft, &Roboto_Black_72, 0, TITLE_TOP, CONTENT_W, SCREEN_H - TITLE_TOP,
               "NO TARGET", TFT_WHITE, TFT_RED);
  }
}



static void drawScreen_RNDZ(RA8875 &tft) {

  if (!state.targetAvailable) {
    if (_rndzChromDrawn) {
      _rndzChromDrawn = false;
      switchToScreen(screen_RNDZ);
    }
    return;
  }

  if (!_rndzChromDrawn) {
    switchToScreen(screen_RNDZ);
    return;
  }

  static const tFont   *F      = &Roboto_Black_40;
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t AX     = ROW_PAD + SECT_W;
  static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
  uint16_t fg, bg;

  // Cache-checked draw helper using section-label-aware geometry
  auto rndzVal = [&](uint8_t row, const char *label, const String &val,
                     uint16_t fgc, uint16_t bgc) {
    RowCache &rc = rowCache[4][row];
    if (rc.value == val && rc.fg == fgc && rc.bg == bgc) return;
    printValue(tft, F, AX, rowYFor(row, NR), AW, rowHFor(NR),
               label, val, fgc, bgc, COL_BACK, printState[4][row]);
    rc.value = val; rc.fg = fgc; rc.bg = bgc;
  };

  // ── STATE block (rows 0-1) ──

  // Row 0 — Own altitude: red if negative, green otherwise
  fg = (state.altitude < 0) ? TFT_RED : TFT_DARK_GREEN;
  rndzVal(0, "Alt.SL:", formatAlt(state.altitude), fg, TFT_BLACK);

  // Row 1 — Orbital velocity: red if negative, green otherwise
  fg = (state.orbitalVel < 0) ? TFT_RED : TFT_DARK_GREEN;
  rndzVal(1, "V.Orb:", fmtMs(state.orbitalVel), fg, TFT_BLACK);

  // ── TGT block (rows 2-3) ──

  // Row 2 — Distance: yellow <5km, red <500m (tightened from 10km/1km)
  if      (state.tgtDistance < RNDZ_DIST_ALARM_M)  { fg = TFT_WHITE;     bg = TFT_RED;   }
  else if (state.tgtDistance < RNDZ_DIST_WARN_M)   { fg = TFT_YELLOW;    bg = TFT_BLACK; }
  else                                   { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
  rndzVal(2, "Dist:", formatAlt(state.tgtDistance), fg, bg);

  // Row 3 — Closure rate: yellow >5 m/s closing, red >10 m/s within 2km
  {
    bool closing     = (state.tgtVelocity < 0);
    bool fastClosure = closing && (state.tgtDistance < RNDZ_VCLOSURE_ALARM_DIST_M) &&
                       (fabsf(state.tgtVelocity) > RNDZ_VCLOSURE_ALARM_MS);
    bool moderateClosure = closing && fabsf(state.tgtVelocity) > RNDZ_VCLOSURE_WARN_MS;
    if      (fastClosure)      { fg = TFT_WHITE;     bg = TFT_RED;   }
    else if (moderateClosure)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
    else                       { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    rndzVal(3, "V.Tgt:", fmtMs(state.tgtVelocity), fg, bg);
  }

  // ── INT1 block (rows 4-5) ──

  // Row 4 — Time to first intercept: yellow <120s, red if past
  if (state.intercept1Time < 0) {
    rndzVal(4, "Time:", "---", TFT_DARK_GREEN, TFT_BLACK);
  } else {
    fg = (state.intercept1Time == 0)                    ? TFT_RED    :
         (state.intercept1Time < RNDZ_INT_WARN_S) ? TFT_YELLOW  : TFT_DARK_GREEN;
    rndzVal(4, "Time:", formatTime((float)state.intercept1Time), fg, TFT_BLACK);
  }

  // Row 5 — Distance at first intercept:
  //   green <1km (good intercept), yellow <5km (workable), red >=5km (poor — needs correction)
  if (state.intercept1Dist < 0) {
    rndzVal(5, "Dist:", "---", TFT_DARK_GREEN, TFT_BLACK);
  } else {
    if      (state.intercept1Dist < RNDZ_INTDIST_GOOD_M) { fg = TFT_DARK_GREEN; }
    else if (state.intercept1Dist < RNDZ_INTDIST_WARN_M) { fg = TFT_YELLOW;     }
    else                                     { fg = TFT_RED;         }
    rndzVal(5, "Dist:", formatAlt(state.intercept1Dist), fg, TFT_BLACK);
  }

  // ── INT2 block (rows 6-7) ──

  // Row 6 — Time to second intercept
  if (state.intercept2Time < 0) {
    rndzVal(6, "Time:", "---", TFT_DARK_GREEN, TFT_BLACK);
  } else {
    fg = (state.intercept2Time == 0)                    ? TFT_RED    :
         (state.intercept2Time < RNDZ_INT_WARN_S) ? TFT_YELLOW  : TFT_DARK_GREEN;
    rndzVal(6, "Time:", formatTime((float)state.intercept2Time), fg, TFT_BLACK);
  }

  // Row 7 — Distance at second intercept (same logic as INT1)
  if (state.intercept2Dist < 0) {
    rndzVal(7, "Dist:", "---", TFT_DARK_GREEN, TFT_BLACK);
  } else {
    if      (state.intercept2Dist < RNDZ_INTDIST_GOOD_M) { fg = TFT_DARK_GREEN; }
    else if (state.intercept2Dist < RNDZ_INTDIST_WARN_M) { fg = TFT_YELLOW;     }
    else                                     { fg = TFT_RED;         }
    rndzVal(7, "Dist:", formatAlt(state.intercept2Dist), fg, TFT_BLACK);
  }
}

