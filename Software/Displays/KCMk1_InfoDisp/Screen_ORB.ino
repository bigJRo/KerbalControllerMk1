/***************************************************************************************
   Screen_ORB.ino -- Orbit screen (combined) — chromeScreen_ORB, drawScreen_ORB
   Default view: APSIDES — mission snapshot: state, apses, next node, ΔV reserve, control
   Title bar tap: ADVANCED ELEMENTS — full orbital elements reference
   rowCache index: [1] (screen_ORB = 1)
   Split row cache slots: PROP ΔV.Stg=[8] ΔV.Tot=[9] | CT RCS=[10] SAS=[11]
****************************************************************************************/
#include "KCMk1_InfoDisp.h"

bool _orbAdvancedMode = false;
bool _prevShowAp = false;  // non-static so SimpitHandler can reset on vessel switch

// ── APSIDES chrome ────────────────────────────────────────────────────────────────────

static void chromeOrb_Apsides(RA8875 &tft) {
  static const tFont   *F      = &Roboto_Black_40;
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t AX     = ROW_PAD + SECT_W;
  static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
  static const uint16_t AHW    = AW / 2;
  uint16_t rowH = rowHFor(NR);

  // Section labels: STATE(0-1), APSI(2-3), TIME(4), BURN(5), PROP(6), CT(7)
  // TIME, BURN, PROP, CT drawn AFTER their bounding dividers
  drawVerticalText(tft, 0, TITLE_TOP,          SECT_W, rowH*2, &Roboto_Black_16, "STATE", TFT_LIGHT_GREY, TFT_BLACK);
  drawVerticalText(tft, 0, TITLE_TOP + rowH*2, SECT_W, rowH*2, &Roboto_Black_16, "APSI",  TFT_LIGHT_GREY, TFT_BLACK);

  // Row labels: STATE and APSI blocks
  printDispChrome(tft, F, AX, rowYFor(0,NR), AW, rowH, "Alt.SL:", COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(1,NR), AW, rowH, "V.Orb:",  COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(2,NR), AW, rowH, "ApA:",    COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(3,NR), AW, rowH, "PeA:",    COL_LABEL, COL_BACK, COL_NO_BDR);

  // Dividers — drawn before single-row section labels so labels sit on top
  uint16_t d1 = TITLE_TOP + rowH*2 + 1;  // after STATE
  uint16_t d2 = TITLE_TOP + rowH*4 + 1;  // after APSI
  uint16_t d3 = TITLE_TOP + rowH*5 + 1;  // after TIME
  uint16_t d4 = TITLE_TOP + rowH*6 + 1;  // after BURN
  uint16_t d5 = TITLE_TOP + rowH*7 + 1;  // after PROP
  tft.drawLine(0, d1,   CONTENT_W, d1,   TFT_GREY);
  tft.drawLine(0, d1+1, CONTENT_W, d1+1, TFT_GREY);
  tft.drawLine(0, d2,   CONTENT_W, d2,   TFT_GREY);
  tft.drawLine(0, d2+1, CONTENT_W, d2+1, TFT_GREY);
  tft.drawLine(0, d3,   CONTENT_W, d3,   TFT_GREY);
  tft.drawLine(0, d3+1, CONTENT_W, d3+1, TFT_GREY);
  tft.drawLine(0, d4,   CONTENT_W, d4,   TFT_GREY);
  tft.drawLine(0, d4+1, CONTENT_W, d4+1, TFT_GREY);
  tft.drawLine(0, d5,   CONTENT_W, d5,   TFT_GREY);
  tft.drawLine(0, d5+1, CONTENT_W, d5+1, TFT_GREY);

  // Single-row section labels drawn after dividers (+2px offset)
  drawVerticalText(tft, 0, TITLE_TOP + rowH*4 + 2, SECT_W, rowH-4, &Roboto_Black_16, "TM", TFT_LIGHT_GREY, TFT_BLACK);
  drawVerticalText(tft, 0, TITLE_TOP + rowH*5 + 2, SECT_W, rowH-4, &Roboto_Black_16, "BN", TFT_LIGHT_GREY, TFT_BLACK);
  // ΔV: Δ is a 2-byte UTF-8 sequence counted as 2 chars by drawVerticalText centering,
  // so shift the y-origin up by ~8px (half a char height) to re-centre it visually.
  drawVerticalText(tft, 0, TITLE_TOP + rowH*6 - 12, SECT_W, rowH,   &Roboto_Black_16, "\xCE\x94V", TFT_LIGHT_GREY, TFT_BLACK);
  drawVerticalText(tft, 0, TITLE_TOP + rowH*7 + 2, SECT_W, rowH-4, &Roboto_Black_16, "CT",   TFT_LIGHT_GREY, TFT_BLACK);

  // Row 4 — T+Ap or T+Pe: label reflects _prevShowAp so chrome is always consistent
  // Row 5 — T+Ign
  printDispChrome(tft, F, AX, rowYFor(4,NR), AW, rowH, _prevShowAp ? "T+Ap:" : "T+Pe:", COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(5,NR), AW, rowH, "T+Ign:", COL_LABEL, COL_BACK, COL_NO_BDR);

  // Row 6 — split Stg: | Tot:
  {
    uint16_t y = rowYFor(6, NR), h = rowH;
    printDispChrome(tft, F, AX,                 y, AHW - ROW_PAD, h, "Stg:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "Tot:", COL_LABEL, COL_BACK, COL_NO_BDR);
    for (int8_t dx = -1; dx <= 1; dx++)
      tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, rowYFor(7,NR) - 1, TFT_GREY);
  }

  // Row 7 — split RCS | SAS
  {
    uint16_t y = rowYFor(7, NR), h = rowH;
    printDispChrome(tft, F, AX,                 y, AHW - ROW_PAD, h, "RCS:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "SAS:", COL_LABEL, COL_BACK, COL_NO_BDR);
    for (int8_t dx = -1; dx <= 1; dx++)
      tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, rowYFor(8,NR) - 1, TFT_GREY);
  }
}

// ── ADVANCED ELEMENTS chrome ──────────────────────────────────────────────────────────

static void chromeOrb_Advanced(RA8875 &tft) {
  static const tFont   *F      = &Roboto_Black_40;
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t AX     = ROW_PAD + SECT_W;
  static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
  static const uint16_t AHW    = AW / 2;
  uint16_t rowH = rowHFor(NR);

  drawVerticalText(tft, 0, TITLE_TOP,          SECT_W, rowH*2, &Roboto_Black_16, "SHAPE", TFT_LIGHT_GREY, TFT_BLACK);
  drawVerticalText(tft, 0, TITLE_TOP + rowH*2, SECT_W, rowH*2, &Roboto_Black_16, "APSE",  TFT_LIGHT_GREY, TFT_BLACK);
  drawVerticalText(tft, 0, TITLE_TOP + rowH*4, SECT_W, rowH*2, &Roboto_Black_16, "PLANE", TFT_LIGHT_GREY, TFT_BLACK);

  printDispChrome(tft, F, AX, rowYFor(0,NR), AW, rowH, "Ecc:",    COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(1,NR), AW, rowH, "SMA:",    COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(2,NR), AW, rowH, "ApA:",    COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(3,NR), AW, rowH, "PeA:",    COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(4,NR), AW, rowH, "Arg.Pe:", COL_LABEL, COL_BACK, COL_NO_BDR);
  {
    uint16_t y = rowYFor(5, NR), h = rowH;
    printDispChrome(tft, F, AX,                 y, AHW - ROW_PAD, h, "Inc:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "LAN:", COL_LABEL, COL_BACK, COL_NO_BDR);
    for (int8_t dx = -1; dx <= 1; dx++)
      tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, rowYFor(6,NR) - 1, TFT_GREY);
  }
  {
    uint16_t y = rowYFor(6, NR), h = rowH;
    printDispChrome(tft, F, AX,                 y, AHW - ROW_PAD, h, "True:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "Mean:", COL_LABEL, COL_BACK, COL_NO_BDR);
    for (int8_t dx = -1; dx <= 1; dx++)
      tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, rowYFor(7,NR) - 1, TFT_GREY);
  }
  printDispChrome(tft, F, AX, rowYFor(7,NR), AW, rowH, "Period:", COL_LABEL, COL_BACK, COL_NO_BDR);

  uint16_t d1    = TITLE_TOP + rowH*2 + 1;
  uint16_t d2    = TITLE_TOP + rowH*4 + 1;
  uint16_t d_mid = TITLE_TOP + rowH*6 + 1;
  uint16_t d3    = TITLE_TOP + rowH*7 + 1;
  tft.drawLine(0, d1,      CONTENT_W, d1,      TFT_GREY);
  tft.drawLine(0, d1+1,    CONTENT_W, d1+1,    TFT_GREY);
  tft.drawLine(0, d2,      CONTENT_W, d2,      TFT_GREY);
  tft.drawLine(0, d2+1,    CONTENT_W, d2+1,    TFT_GREY);
  tft.drawLine(0, d_mid,   CONTENT_W, d_mid,   TFT_GREY);
  tft.drawLine(0, d_mid+1, CONTENT_W, d_mid+1, TFT_GREY);
  tft.drawLine(0, d3,      CONTENT_W, d3,      TFT_GREY);
  tft.drawLine(0, d3+1,    CONTENT_W, d3+1,    TFT_GREY);
  drawVerticalText(tft, 0, TITLE_TOP + rowH*6 + 2, SECT_W, rowH - 4, &Roboto_Black_16, "AN", TFT_LIGHT_GREY, TFT_BLACK);
  drawVerticalText(tft, 0, TITLE_TOP + rowH*7 + 4, SECT_W, rowH - 4, &Roboto_Black_16, "PR", TFT_LIGHT_GREY, TFT_BLACK);
}

// ── PUBLIC CHROME ENTRY POINT ─────────────────────────────────────────────────────────

static void chromeScreen_ORB(RA8875 &tft) {
  if (_orbAdvancedMode) chromeOrb_Advanced(tft);
  else                  chromeOrb_Apsides(tft);
}

// ── APSIDES DRAW ──────────────────────────────────────────────────────────────────────

static void drawOrb_Apsides(RA8875 &tft) {
  static const tFont   *F      = &Roboto_Black_40;
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t AX     = ROW_PAD + SECT_W;
  static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
  static const uint16_t AHW    = AW / 2;
  uint16_t fg, bg;

  // Redraw lower dividers every loop — value updates can overwrite them
  {
    uint16_t rowH = rowHFor(NR);
    uint16_t d3 = TITLE_TOP + rowH*5 + 1;
    uint16_t d4 = TITLE_TOP + rowH*6 + 1;
    uint16_t d5 = TITLE_TOP + rowH*7 + 1;
    tft.drawLine(0, d3,   CONTENT_W, d3,   TFT_GREY);
    tft.drawLine(0, d3+1, CONTENT_W, d3+1, TFT_GREY);
    tft.drawLine(0, d4,   CONTENT_W, d4,   TFT_GREY);
    tft.drawLine(0, d4+1, CONTENT_W, d4+1, TFT_GREY);
    tft.drawLine(0, d5,   CONTENT_W, d5,   TFT_GREY);
    tft.drawLine(0, d5+1, CONTENT_W, d5+1, TFT_GREY);
  }

  // orbVal -> drawValue() split overload with AX/AW section geometry (#6C)
  auto orbVal = [&](uint8_t row, const char *label, const String &val,
                    uint16_t fgc, uint16_t bgc) {
    drawValue(tft, 1, row, AX, AW, label, val, fgc, bgc, F, NR);
  };

  float warnAlt = max(currentBody.minSafe, currentBody.flyHigh);
  bool hasOrbit = (state.situation & sit_SubOrb)  ||
                  (state.situation & sit_Orbit)    ||
                  (state.situation & sit_Escaping);
  bool hasMnvr  = (state.mnvrDeltaV > 0.0f);

  // ── STATE (rows 0-1) ──

  fg = (state.altitude < 0) ? TFT_RED : TFT_DARK_GREEN;
  orbVal(0, "Alt.SL:", formatAlt(state.altitude), fg, TFT_BLACK);

  fg = (state.orbitalVel < 0) ? TFT_RED : TFT_DARK_GREEN;
  orbVal(1, "V.Orb:", fmtMs(state.orbitalVel), fg, TFT_BLACK);

  // ── APSI (rows 2-3) ──

  if (!hasOrbit) {
    orbVal(2, "ApA:", "---", TFT_DARK_GREY, TFT_BLACK);
  } else {
    bool apaWarn = (warnAlt > 0 && state.apoapsis > 0 && state.apoapsis < warnAlt);
    if      (state.apoapsis < 0) fg = TFT_RED;
    else if (apaWarn)            fg = TFT_YELLOW;
    else                         fg = TFT_DARK_GREEN;
    orbVal(2, "ApA:", formatAlt(state.apoapsis), fg, TFT_BLACK);
  }

  if (!hasOrbit) {
    orbVal(3, "PeA:", "---", TFT_DARK_GREY, TFT_BLACK);
  } else {
    bool peWarn = (warnAlt > 0 && state.periapsis > 0 && state.periapsis < warnAlt);
    if      (state.periapsis < 0) fg = TFT_RED;
    else if (peWarn)              fg = TFT_YELLOW;
    else                          fg = TFT_DARK_GREEN;
    orbVal(3, "PeA:", formatAlt(state.periapsis), fg, TFT_BLACK);
  }

  // ── TIME (row 4): T+Ap or T+Pe, whichever is sooner ──
  // Near-circular guard: if ApA and PeA are within ORB_CIRCULAR_PCT% of each other,
  // suppress T+ to --- (dark green) — the value jumps rapidly on a near-circular orbit.
  // When showAp changes, redraw the chrome label directly without a full switchToScreen.
  if (!hasOrbit) {
    if (_prevShowAp) {
      _prevShowAp = false;
      printDispChrome(tft, &Roboto_Black_40, AX, rowYFor(4,NR), AW, rowHFor(NR),
                      "T+Pe:", COL_LABEL, COL_BACK, COL_NO_BDR);
    }
    orbVal(4, "T+Pe:", "---", TFT_DARK_GREY, TFT_BLACK);
  } else {
    // Near-circular: ApA and PeA within ORB_CIRCULAR_PCT % of each other
    float apsiMid = (fabsf(state.apoapsis) + fabsf(state.periapsis)) / 2.0f;
    bool nearCircular = (apsiMid > 0.0f) &&
                        (fabsf(state.apoapsis - state.periapsis) / apsiMid * 100.0f < ORB_CIRCULAR_PCT);
    if (nearCircular) {
      // Near-circular — suppress to avoid meaningless jumping value
      if (_prevShowAp) {
        _prevShowAp = false;
        printDispChrome(tft, &Roboto_Black_40, AX, rowYFor(4,NR), AW, rowHFor(NR),
                        "T+Pe:", COL_LABEL, COL_BACK, COL_NO_BDR);
      }
      orbVal(4, "T+Pe:", "---", TFT_DARK_GREEN, TFT_BLACK);
    } else {
      bool showAp = (state.timeToAp >= 0.0f) && (state.timeToAp < state.timeToPe);
      if (showAp != _prevShowAp) {
        _prevShowAp = showAp;
        printDispChrome(tft, &Roboto_Black_40, AX, rowYFor(4,NR), AW, rowHFor(NR),
                        showAp ? "T+Ap:" : "T+Pe:", COL_LABEL, COL_BACK, COL_NO_BDR);
        rowCache[1][4].value = "\x01";
      }
      float      tNext  = showAp ? state.timeToAp : state.timeToPe;
      const char *tLabel = showAp ? "T+Ap:"        : "T+Pe:";
      if      (tNext < 0)                fg = TFT_RED;
      else if (tNext < APSI_TIME_WARN_S) fg = TFT_YELLOW;
      else                               fg = TFT_DARK_GREEN;
      orbVal(4, tLabel, formatTime(tNext), fg, TFT_BLACK);
    }
  }

  // ── BURN (row 5): T+Ignition ──
  if (!hasMnvr) {
    orbVal(5, "T+Ign:", "---", TFT_DARK_GREY, TFT_BLACK);
  } else {
    float tIgn = state.mnvrTime - (state.mnvrDuration / 2.0f);
    if      (tIgn < 0.0f)             { fg = TFT_WHITE;     bg = TFT_RED;   }
    else if (tIgn < MNVR_TIGN_WARN_S) { fg = TFT_YELLOW;    bg = TFT_BLACK; }
    else                              { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    orbVal(5, "T+Ign:", formatTime(tIgn), fg, bg);
  }

  // ── PROP (row 6): split ΔV.Stg | ΔV.Tot ──  cache [1][8]=Stg [1][9]=Tot
  {
    uint16_t y6 = rowYFor(6, NR), h6 = rowHFor(NR);
    uint16_t xL = AX,                 wL = AHW - ROW_PAD;
    uint16_t xR = AX + AHW + ROW_PAD, wR = AHW - ROW_PAD;

    // ΔV.Stg — left
    thresholdColor(state.stageDeltaV,
                   DV_STG_ALARM_MS, TFT_WHITE, TFT_RED,
                   DV_STG_WARN_MS,  TFT_YELLOW, TFT_BLACK,
                        TFT_DARK_GREEN, TFT_BLACK, fg, bg);
    {
      String s = fmtMs(state.stageDeltaV);
      RowCache &sc = rowCache[1][8];
      if (sc.value != s || sc.fg != fg || sc.bg != bg) {
        printValue(tft, F, xL, y6, wL, h6, "Stg:", s, fg, bg, COL_BACK, printState[1][8]);
        sc.value = s; sc.fg = fg; sc.bg = bg;
      }
    }

    // ΔV.Tot — right (uses DV_STG_WARN_MS to match Stg: so colours are directly comparable)
    thresholdColor(state.totalDeltaV,
                   DV_STG_ALARM_MS, TFT_WHITE, TFT_RED,
                   DV_STG_WARN_MS,  TFT_YELLOW, TFT_BLACK,
                        TFT_DARK_GREEN, TFT_BLACK, fg, bg);
    {
      String s = fmtMs(state.totalDeltaV);
      RowCache &tc = rowCache[1][9];
      if (tc.value != s || tc.fg != fg || tc.bg != bg) {
        printValue(tft, F, xR, y6, wR, h6, "Tot:", s, fg, bg, COL_BACK, printState[1][9]);
        tc.value = s; tc.fg = fg; tc.bg = bg;
      }
    }
  }

  // ── CT (row 7): split RCS | SAS ──  cache [1][10] and [1][11]
  {
    uint16_t y7 = rowYFor(7, NR), h7 = rowHFor(NR);
    uint16_t xL = AX,                 wL = AHW - ROW_PAD;
    uint16_t xR = AX + AHW + ROW_PAD, wR = AHW - ROW_PAD;

    // RCS
    const char *rcsStr = state.rcs_on ? "ON" : "OFF";
    uint16_t    rcsFg  = state.rcs_on ? TFT_DARK_GREEN : TFT_DARK_GREY;
    {
      RowCache &rc = rowCache[1][10];
      if (rc.value != rcsStr || rc.fg != rcsFg || rc.bg != TFT_BLACK) {
        printValue(tft, F, xL, y7, wL, h7, "RCS:", rcsStr, rcsFg, TFT_BLACK, COL_BACK, printState[1][10]);
        rc.value = rcsStr; rc.fg = rcsFg; rc.bg = TFT_BLACK;
      }
    }

    // SAS — navball colours; OFF = dark grey (not an alarm in cruise context)
    const char *sasStr; uint16_t sasFg;
    switch (state.sasMode) {
      case 255: sasStr = "OFF";       sasFg = TFT_DARK_GREY;  break;
      case 0:   sasStr = "STAB";      sasFg = TFT_DARK_GREEN; break;
      case 1:   sasStr = "PROGRADE";  sasFg = TFT_NEON_GREEN; break;
      case 2:   sasStr = "RETRO";     sasFg = TFT_NEON_GREEN; break;
      case 3:   sasStr = "NORMAL";    sasFg = TFT_MAGENTA;    break;
      case 4:   sasStr = "ANTI-NRM";  sasFg = TFT_MAGENTA;    break;
      case 5:   sasStr = "RAD-OUT";   sasFg = TFT_SKY;        break;
      case 6:   sasStr = "RAD-IN";    sasFg = TFT_SKY;        break;
      case 7:   sasStr = "TARGET";    sasFg = TFT_VIOLET;     break;
      case 8:   sasStr = "ANTI-TGT";  sasFg = TFT_VIOLET;     break;
      case 9:   sasStr = "MANEUVER";  sasFg = TFT_BLUE;       break;
      default:  sasStr = "---";       sasFg = TFT_DARK_GREY;  break;
    }
    {
      RowCache &sc = rowCache[1][11];
      if (sc.value != sasStr || sc.fg != sasFg || sc.bg != TFT_BLACK) {
        printValue(tft, F, xR, y7, wR, h7, "SAS:", sasStr, sasFg, TFT_BLACK, COL_BACK, printState[1][11]);
        sc.value = sasStr; sc.fg = sasFg; sc.bg = TFT_BLACK;
      }
    }
  }
}

static void drawOrb_Advanced(RA8875 &tft) {
  static const tFont   *F      = &Roboto_Black_40;
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t AX     = ROW_PAD + SECT_W;
  static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
  static const uint16_t AHW    = AW / 2;
  char buf[16];
  uint16_t fg, bg;

  // orbVal -> drawValue() split overload with AX/AW section geometry (#6C)
  auto orbVal = [&](uint8_t row, const char *label, const String &val,
                    uint16_t fgc, uint16_t bgc) {
    drawValue(tft, 1, row, AX, AW, label, val, fgc, bgc, F, NR);
  };

  bool hasOrbit = (state.situation & sit_SubOrb)  ||
                  (state.situation & sit_Orbit)    ||
                  (state.situation & sit_Escaping);
  float warnAlt = max(currentBody.minSafe, currentBody.flyHigh);

  // Row 0 — Eccentricity
  if (!hasOrbit) {
    orbVal(0, "Ecc:", "---", TFT_DARK_GREY, TFT_BLACK);
  } else {
    snprintf(buf, sizeof(buf), "%.4f", state.eccentricity);
    if      (state.eccentricity > ORB_ECC_ALARM) { fg = TFT_WHITE;     bg = TFT_RED;   }
    else if (state.eccentricity > ORB_ECC_WARN)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
    else                                          { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    orbVal(0, "Ecc:", buf, fg, bg);
  }

  // Row 1 — Semi-major axis
  orbVal(1, "SMA:", hasOrbit ? formatAlt(state.semiMajorAxis) : "---",
         hasOrbit ? TFT_DARK_GREEN : TFT_DARK_GREY, TFT_BLACK);

  // Row 2 — Apoapsis
  if (!hasOrbit) {
    orbVal(2, "ApA:", "---", TFT_DARK_GREY, TFT_BLACK);
  } else {
    bool apaWarn = (warnAlt > 0 && state.apoapsis > 0 && state.apoapsis < warnAlt);
    if      (state.apoapsis < 0) fg = TFT_RED;
    else if (apaWarn)            fg = TFT_YELLOW;
    else                         fg = TFT_DARK_GREEN;
    orbVal(2, "ApA:", formatAlt(state.apoapsis), fg, TFT_BLACK);
  }

  // Row 3 — Periapsis
  if (!hasOrbit) {
    orbVal(3, "PeA:", "---", TFT_DARK_GREY, TFT_BLACK);
  } else {
    bool peWarn = (warnAlt > 0 && state.periapsis > 0 && state.periapsis < warnAlt);
    if      (state.periapsis < 0) fg = TFT_RED;
    else if (peWarn)              fg = TFT_YELLOW;
    else                          fg = TFT_DARK_GREEN;
    orbVal(3, "PeA:", formatAlt(state.periapsis), fg, TFT_BLACK);
  }

  // Row 4 — Argument of periapsis
  if (hasOrbit) { snprintf(buf, sizeof(buf), "%.2f\xB0", state.argOfPe); }
  orbVal(4, "Arg.Pe:", hasOrbit ? buf : "---",
         hasOrbit ? TFT_DARK_GREEN : TFT_DARK_GREY, TFT_BLACK);

  // Row 5 — Split: Inc | LAN
  {
    uint16_t y = rowYFor(5, NR), h = rowHFor(NR);
    String incStr = hasOrbit ? (snprintf(buf, sizeof(buf), "%.2f\xB0", state.inclination), String(buf)) : "---";
    uint16_t incFg = hasOrbit ? COL_VALUE : TFT_DARK_GREY;
    RowCache &ic = rowCache[1][11];
    if (ic.value != incStr || ic.fg != incFg || ic.bg != COL_BACK) {
      printValue(tft, F, AX, y, AHW - ROW_PAD, h, "Inc:", incStr, incFg, COL_BACK, COL_BACK, printState[1][11]);
      ic.value = incStr; ic.fg = incFg; ic.bg = COL_BACK;
    }
    String lanStr = hasOrbit ? (snprintf(buf, sizeof(buf), "%.2f\xB0", state.LAN), String(buf)) : "---";
    uint16_t lanFg = hasOrbit ? COL_VALUE : TFT_DARK_GREY;
    RowCache &lc = rowCache[1][9];
    if (lc.value != lanStr || lc.fg != lanFg || lc.bg != COL_BACK) {
      printValue(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "LAN:", lanStr, lanFg, COL_BACK, COL_BACK, printState[1][9]);
      lc.value = lanStr; lc.fg = lanFg; lc.bg = COL_BACK;
    }
  }

  // Row 6 — Split: True anomaly | Mean anomaly
  {
    uint16_t y = rowYFor(6, NR), h = rowHFor(NR);
    String taStr = hasOrbit ? (snprintf(buf, sizeof(buf), "%.2f\xB0", state.trueAnomaly), String(buf)) : "---";
    uint16_t taFg = hasOrbit ? COL_VALUE : TFT_DARK_GREY;
    RowCache &tc = rowCache[1][6];
    if (tc.value != taStr || tc.fg != taFg || tc.bg != COL_BACK) {
      printValue(tft, F, AX, y, AHW - ROW_PAD, h, "True:", taStr, taFg, COL_BACK, COL_BACK, printState[1][6]);
      tc.value = taStr; tc.fg = taFg; tc.bg = COL_BACK;
    }
    String maStr = hasOrbit ? (snprintf(buf, sizeof(buf), "%.2f\xB0", state.meanAnomaly), String(buf)) : "---";
    uint16_t maFg = hasOrbit ? COL_VALUE : TFT_DARK_GREY;
    RowCache &mc = rowCache[1][10];
    if (mc.value != maStr || mc.fg != maFg || mc.bg != COL_BACK) {
      printValue(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "Mean:", maStr, maFg, COL_BACK, COL_BACK, printState[1][10]);
      mc.value = maStr; mc.fg = maFg; mc.bg = COL_BACK;
    }
  }

  // Row 7 — Orbital period
  orbVal(7, "Period:", hasOrbit ? formatTime(state.orbitalPeriod) : "---",
         hasOrbit ? TFT_DARK_GREEN : TFT_DARK_GREY, TFT_BLACK);
}

// ── PUBLIC DRAW ENTRY POINT ───────────────────────────────────────────────────────────

static void drawScreen_ORB(RA8875 &tft) {
  if (_orbAdvancedMode) drawOrb_Advanced(tft);
  else                  drawOrb_Apsides(tft);
}
