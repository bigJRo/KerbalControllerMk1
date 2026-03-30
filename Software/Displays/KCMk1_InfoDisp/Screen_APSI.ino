/***************************************************************************************
   Screen_APSI.ino -- Apsides screen — chromeScreen_APSI, drawScreen_APSI
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


static void chromeScreen_APSI(RA8875 &tft) {
  static const tFont   *F      = &Roboto_Black_40;
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t AX     = ROW_PAD + SECT_W;
  static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
  uint16_t rowH = rowHFor(NR);

  // Section labels: STATE(rows0-2), AP(rows3-4), PE(rows5-6)
  // PR(row7) drawn AFTER dividers to avoid overwrite
  drawVerticalText(tft, 0, TITLE_TOP,          SECT_W, rowH*3, &Roboto_Black_16, "STATE", TFT_LIGHT_GREY, TFT_BLACK);
  drawVerticalText(tft, 0, TITLE_TOP + rowH*3, SECT_W, rowH*2, &Roboto_Black_16, "AP",    TFT_LIGHT_GREY, TFT_BLACK);
  drawVerticalText(tft, 0, TITLE_TOP + rowH*5, SECT_W, rowH*2, &Roboto_Black_16, "PE",    TFT_LIGHT_GREY, TFT_BLACK);

  // Row labels (right of section strip)
  printDispChrome(tft, F, AX, rowYFor(0,NR), AW, rowH, "Alt.SL:", COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(1,NR), AW, rowH, "V.Orb:",  COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(2,NR), AW, rowH, "SOI:",    COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(3,NR), AW, rowH, "ApA:",    COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(4,NR), AW, rowH, "T+Ap:",   COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(5,NR), AW, rowH, "PeA:",    COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(6,NR), AW, rowH, "T+Pe:",   COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(7,NR), AW, rowH, "Period:", COL_LABEL, COL_BACK, COL_NO_BDR);

  // Dividers — drawn LAST, +1 offset to clear printValue fillRect boundary
  uint16_t d1 = TITLE_TOP + rowH*3 + 1;  // after STATE (row2/row3)
  uint16_t d2 = TITLE_TOP + rowH*5 + 1;  // after AP    (row4/row5)
  uint16_t d3 = TITLE_TOP + rowH*7 + 1;  // after PE    (row6/row7)
  tft.drawLine(0, d1,   CONTENT_W, d1,   TFT_GREY);
  tft.drawLine(0, d1+1, CONTENT_W, d1+1, TFT_GREY);
  tft.drawLine(0, d2,   CONTENT_W, d2,   TFT_GREY);
  tft.drawLine(0, d2+1, CONTENT_W, d2+1, TFT_GREY);
  tft.drawLine(0, d3,   CONTENT_W, d3,   TFT_GREY);
  tft.drawLine(0, d3+1, CONTENT_W, d3+1, TFT_GREY);
  // PR drawn after d3 so its fillRect doesn't overwrite the divider
  drawVerticalText(tft, 0, TITLE_TOP + rowH*7 + 2, SECT_W, rowH - 4,
                   &Roboto_Black_16, "PR", TFT_LIGHT_GREY, TFT_BLACK);
}



static void drawScreen_APSI(RA8875 &tft) {
  static const tFont   *F      = &Roboto_Black_40;
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t AX     = ROW_PAD + SECT_W;
  static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
  uint16_t fg;

  // Cache-checked draw helper using section-label-aware geometry
  auto apsiVal = [&](uint8_t row, const char *label, const String &val,
                     uint16_t fgc, uint16_t bgc) {
    RowCache &rc = rowCache[1][row];
    if (rc.value == val && rc.fg == fgc && rc.bg == bgc) return;
    printValue(tft, F, AX, rowYFor(row, NR), AW, rowHFor(NR),
               label, val, fgc, bgc, COL_BACK, printState[1][row]);
    rc.value = val; rc.fg = fgc; rc.bg = bgc;
  };

  // Shared thresholds
  float warnAlt = max(currentBody.minSafe, currentBody.flyHigh);

  // ── ORBIT block (rows 0-2): state, velocity, SOI ──

  // Row 0 — Altitude ASL: red if negative, green otherwise (orbital context)
  fg = (state.altitude < 0) ? TFT_RED : TFT_DARK_GREEN;
  apsiVal(0, "Alt.SL:", formatAlt(state.altitude), fg, TFT_BLACK);

  // Row 1 — Orbital velocity
  fg = (state.orbitalVel < 0) ? TFT_RED : TFT_DARK_GREEN;
  apsiVal(1, "V.Orb:", fmtMs(state.orbitalVel), fg, TFT_BLACK);

  // Row 2 — SOI body name (informational)
  apsiVal(2, "SOI:", state.gameSOI, COL_VALUE, COL_BACK);

  // ── ApA block (rows 3-4): apoapsis + time ──

  // Row 3 — Apoapsis
  {
    bool apaWarn = (warnAlt > 0 && state.apoapsis > 0 && state.apoapsis < warnAlt);
    if      (state.apoapsis < 0) fg = TFT_RED;
    else if (apaWarn)            fg = TFT_YELLOW;
    else                         fg = TFT_DARK_GREEN;
    apsiVal(3, "ApA:", formatAlt(state.apoapsis), fg, TFT_BLACK);
  }

  // Row 4 — Time to apoapsis: yellow <60s, red if past
  if      (state.timeToAp < 0)  fg = TFT_RED;
  else if (state.timeToAp < APSI_TIME_WARN_S) fg = TFT_YELLOW;
  else                          fg = TFT_DARK_GREEN;
  apsiVal(4, "T+Ap:", formatTime(state.timeToAp), fg, TFT_BLACK);

  // ── PeA block (rows 5-6): periapsis + time ──

  // Row 5 — Periapsis
  {
    bool peWarn = (warnAlt > 0 && state.periapsis > 0 && state.periapsis < warnAlt);
    if      (state.periapsis < 0) fg = TFT_RED;
    else if (peWarn)              fg = TFT_YELLOW;
    else                          fg = TFT_DARK_GREEN;
    apsiVal(5, "PeA:", formatAlt(state.periapsis), fg, TFT_BLACK);
  }

  // Row 6 — Time to periapsis: yellow <60s, red if past
  if      (state.timeToPe < 0)  fg = TFT_RED;
  else if (state.timeToPe < APSI_TIME_WARN_S) fg = TFT_YELLOW;
  else                          fg = TFT_DARK_GREEN;
  apsiVal(6, "T+Pe:", formatTime(state.timeToPe), fg, TFT_BLACK);

  // ── Period row (row 7): orbit summary ──

  // Row 7 — Orbital period (always green — no actionable threshold)
  apsiVal(7, "Period:", formatTime(state.orbitalPeriod), COL_VALUE, COL_BACK);
}
