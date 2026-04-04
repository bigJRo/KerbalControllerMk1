/***************************************************************************************
   Screen_MISC.ino -- Rover screen — chromeScreen_MISC, drawScreen_MISC
   Dedicated display for type_Rover vessels. contextScreen() routes here automatically.
   rowCache index: [2] (screen_MISC = 2)

   Layout:
     STATE (rows 0-1): V.Srf, Heading
     NAV   (row  2):   Brg | Err  — bearing to target and heading error (--- if no target)
     TERR  (rows 3-4): Alt.Rdr, Pitch | Roll
     CTRL  (rows 5-7): Throttle (wheel), Gear | Brakes, EC% | SAS

   Notes:
     - Wheel throttle (state.wheelThrottle) comes from WHEEL_MESSAGE (outbound telemetry
       echo of combined wheel input including keyboard + Simpit). Field is w.throttle.
       *** TEST REQUIRED: verify value matches in-game wheel throttle indicator. ***
     - EC% comes from ELECTRIC_MESSAGE resource channel.
     - Roll and Pitch share a split row — both are rollover risk indicators.
     - Brg/Err both show --- when no target is selected.
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


static void chromeScreen_MISC(RA8875 &tft) {
  static const tFont   *F      = &Roboto_Black_40;
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t AX     = ROW_PAD + SECT_W;
  static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
  static const uint16_t AHW    = AW / 2;
  uint16_t rowH = rowHFor(NR);

  // Section labels spanning multiple rows — drawn BEFORE dividers so dividers overwrite their edges
  drawVerticalText(tft, 0, TITLE_TOP,          SECT_W, rowH*2, &Roboto_Black_16, "STATE", TFT_LIGHT_GREY, TFT_BLACK);
  drawVerticalText(tft, 0, TITLE_TOP + rowH*3, SECT_W, rowH*2, &Roboto_Black_16, "TERR",  TFT_LIGHT_GREY, TFT_BLACK);

  // STATE rows
  printDispChrome(tft, F, AX, rowYFor(0,NR), AW, rowH, "V.Srf:", COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(1,NR), AW, rowH, "Hdg:",   COL_LABEL, COL_BACK, COL_NO_BDR);

  // TERR rows
  printDispChrome(tft, F, AX, rowYFor(3,NR), AW, rowH, "Alt.Rdr:", COL_LABEL, COL_BACK, COL_NO_BDR);

  // Dividers at rowH*N — lands in the ROW_PAD gap, not overwritten by printValue.
  // Also redrawn every frame in drawScreen_MISC (same pattern as ATT screen).
  uint16_t d1a = TITLE_TOP + rowH*2;   // STATE / NAV
  uint16_t d2a = TITLE_TOP + rowH*3;   // NAV / TERR
  uint16_t d3a = TITLE_TOP + rowH*5;   // TERR / CTRL
  tft.drawLine(0, d1a,   CONTENT_W, d1a,   TFT_GREY);
  tft.drawLine(0, d1a+1, CONTENT_W, d1a+1, TFT_GREY);
  tft.drawLine(0, d2a,   CONTENT_W, d2a,   TFT_GREY);
  tft.drawLine(0, d2a+1, CONTENT_W, d2a+1, TFT_GREY);
  tft.drawLine(0, d3a,   CONTENT_W, d3a,   TFT_GREY);
  tft.drawLine(0, d3a+1, CONTENT_W, d3a+1, TFT_GREY);

  // Single-row section labels drawn AFTER their bounding dividers (+2px offset)
  drawVerticalText(tft, 0, TITLE_TOP + rowH*2 + 2, SECT_W, rowH-4,   &Roboto_Black_16, "NV",   TFT_LIGHT_GREY, TFT_BLACK);
  drawVerticalText(tft, 0, TITLE_TOP + rowH*5 + 2, SECT_W, rowH*3-4, &Roboto_Black_16, "CTRL", TFT_LIGHT_GREY, TFT_BLACK);

  // NAV row 2: Brg | Err split — drawn after dividers so split divider doesn't get overwritten
  {
    uint16_t y = rowYFor(2, NR), h = rowH;
    printDispChrome(tft, F, AX,                 y, AHW - ROW_PAD, h, "Brg:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "Err:", COL_LABEL, COL_BACK, COL_NO_BDR);
    for (int8_t dx = -1; dx <= 1; dx++)
      tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, d2a - 1, TFT_GREY);
  }

  // TERR row 4: Pitch | Roll split
  {
    uint16_t y = rowYFor(4, NR), h = rowH;
    printDispChrome(tft, F, AX,                 y, AHW - ROW_PAD, h, "Pitch:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "Roll:",  COL_LABEL, COL_BACK, COL_NO_BDR);
    for (int8_t dx = -1; dx <= 1; dx++)
      tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, d3a - 1, TFT_GREY);
  }

  // CTRL row 5: Throttle (full width)
  printDispChrome(tft, F, AX, rowYFor(5,NR), AW, rowH, "Throttle:", COL_LABEL, COL_BACK, COL_NO_BDR);

  // CTRL row 6: Gear | Brakes split
  {
    uint16_t y = rowYFor(6, NR), h = rowH;
    printDispChrome(tft, F, AX,                 y, AHW - ROW_PAD, h, "Gear:",   COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "Brakes:", COL_LABEL, COL_BACK, COL_NO_BDR);
    for (int8_t dx = -1; dx <= 1; dx++)
      tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, rowYFor(7,NR) - 1, TFT_GREY);
  }

  // CTRL row 7: EC% | SAS split
  {
    uint16_t y = rowYFor(7, NR), h = rowH;
    printDispChrome(tft, F, AX,                 y, AHW - ROW_PAD, h, "EC%:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "SAS:", COL_LABEL, COL_BACK, COL_NO_BDR);
    for (int8_t dx = -1; dx <= 1; dx++)
      tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, rowYFor(8,NR) - 1, TFT_GREY);
  }
}


static void drawScreen_MISC(RA8875 &tft) {
  static const tFont   *F      = &Roboto_Black_40;
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t AX     = ROW_PAD + SECT_W;
  static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
  static const uint16_t AHW    = AW / 2;
  uint16_t fg, bg;
  char buf[16];

  auto rovVal = [&](uint8_t row, const char *label, const String &val,
                    uint16_t fgc, uint16_t bgc) {
    RowCache &rc = rowCache[9][row];
    if (rc.value == val && rc.fg == fgc && rc.bg == bgc) return;
    printValue(tft, F, AX, rowYFor(row, NR), AW, rowHFor(NR),
               label, val, fgc, bgc, COL_BACK, printState[9][row]);
    rc.value = val; rc.fg = fgc; rc.bg = bgc;
  };

  // Redraw dividers every frame — same pattern as ATT screen.
  // printValue fillRect at boundary rows overwrites them on value changes.
  {
    uint16_t rowH = rowHFor(NR);
    uint16_t d1 = TITLE_TOP + rowH*2;
    uint16_t d2 = TITLE_TOP + rowH*3;
    uint16_t d3 = TITLE_TOP + rowH*5;
    tft.drawLine(0, d1,   CONTENT_W, d1,   TFT_GREY);
    tft.drawLine(0, d1+1, CONTENT_W, d1+1, TFT_GREY);
    tft.drawLine(0, d2,   CONTENT_W, d2,   TFT_GREY);
    tft.drawLine(0, d2+1, CONTENT_W, d2+1, TFT_GREY);
    tft.drawLine(0, d3,   CONTENT_W, d3,   TFT_GREY);
    tft.drawLine(0, d3+1, CONTENT_W, d3+1, TFT_GREY);
  }

  // ── STATE (rows 0-1) ──

  // Row 0 — V.Srf: surface speed. Alarm threshold reuses LNDG powered descent convention.
  fg = (state.surfaceVel < 0) ? TFT_RED : TFT_DARK_GREEN;
  rovVal(0, "V.Srf:", fmtMs(state.surfaceVel), fg, TFT_BLACK);

  // Row 1 — Heading
  snprintf(buf, sizeof(buf), "%.1f\xB0", state.heading);
  rovVal(1, "Hdg:", buf, TFT_DARK_GREEN, TFT_BLACK);

  // ── NAV (row 2): Brg | Err split ──
  // cache [2][8] = Brg, [2][9] = Err
  {
    uint16_t y2 = rowYFor(2, NR), h2 = rowHFor(NR);
    uint16_t xL = AX,                 wL = AHW - ROW_PAD;
    uint16_t xR = AX + AHW + ROW_PAD, wR = AHW - ROW_PAD;

    if (!state.targetAvailable) {
      // No target — show --- for both
      {
        RowCache &rc = rowCache[9][8];
        String s = "---";
        if (rc.value != s || rc.fg != TFT_DARK_GREY || rc.bg != TFT_BLACK) {
          printValue(tft, F, xL, y2, wL, h2, "Brg:", s, TFT_DARK_GREY, TFT_BLACK, COL_BACK, printState[9][8]);
          rc.value = s; rc.fg = TFT_DARK_GREY; rc.bg = TFT_BLACK;
        }
      }
      {
        RowCache &rc = rowCache[9][9];
        String s = "---";
        if (rc.value != s || rc.fg != TFT_DARK_GREY || rc.bg != TFT_BLACK) {
          printValue(tft, F, xR, y2, wR, h2, "Err:", s, TFT_DARK_GREY, TFT_BLACK, COL_BACK, printState[9][9]);
          rc.value = s; rc.fg = TFT_DARK_GREY; rc.bg = TFT_BLACK;
        }
      }
    } else {
      // Bearing to target
      snprintf(buf, sizeof(buf), "%.1f\xB0", state.tgtHeading);
      {
        String s = buf;
        RowCache &rc = rowCache[9][8];
        if (rc.value != s || rc.fg != TFT_DARK_GREEN || rc.bg != TFT_BLACK) {
          printValue(tft, F, xL, y2, wL, h2, "Brg:", s, TFT_DARK_GREEN, TFT_BLACK, COL_BACK, printState[9][8]);
          rc.value = s; rc.fg = TFT_DARK_GREEN; rc.bg = TFT_BLACK;
        }
      }

      // Heading error: target bearing minus own heading, wrapped ±180°
      float brgErr = state.tgtHeading - state.heading;
      if (brgErr >  180.0f) brgErr -= 360.0f;
      if (brgErr < -180.0f) brgErr += 360.0f;
      float ae = fabsf(brgErr);
      if      (ae >= ROVER_BRG_ALARM_DEG) { fg = TFT_WHITE;     bg = TFT_RED;   }
      else if (ae >= ROVER_BRG_WARN_DEG)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
      else                                { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
      snprintf(buf, sizeof(buf), "%+.1f\xB0", brgErr);
      {
        String s = buf;
        RowCache &rc = rowCache[9][9];
        if (rc.value != s || rc.fg != fg || rc.bg != bg) {
          printValue(tft, F, xR, y2, wR, h2, "Err:", s, fg, bg, COL_BACK, printState[9][9]);
          rc.value = s; rc.fg = fg; rc.bg = bg;
        }
      }
    }
  }

  // ── TERR (rows 3-4) ──

  // Row 3 — Alt.Rdr: inverted logic for rover — close to ground is GOOD
  // Green < 5m (wheels on surface), yellow < 10m (lightly airborne), red >= 10m
  if      (state.radarAlt < ROVER_ALT_RDR_GOOD_M) { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
  else if (state.radarAlt < ROVER_ALT_RDR_WARN_M) { fg = TFT_YELLOW;    bg = TFT_BLACK; }
  else                                             { fg = TFT_WHITE;     bg = TFT_RED;   }
  rovVal(3, "Alt.Rdr:", formatAlt(state.radarAlt), fg, bg);

  // Row 4 — Pitch | Roll split  cache [2][10]=Pitch, [2][11]=Roll
  {
    uint16_t y4 = rowYFor(4, NR), h4 = rowHFor(NR);
    uint16_t xL = AX,                 wL = AHW - ROW_PAD;
    uint16_t xR = AX + AHW + ROW_PAD, wR = AHW - ROW_PAD;

    auto angleColor = [](float deg, float warnDeg, float alarmDeg,
                         uint16_t &fg, uint16_t &bg) {
      float ad = fabsf(deg);
      if      (ad >= alarmDeg) { fg = TFT_WHITE;     bg = TFT_RED;   }
      else if (ad >= warnDeg)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
      else                     { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    };

    angleColor(state.pitch, ROVER_PITCH_WARN_DEG, ROVER_PITCH_ALARM_DEG, fg, bg);
    snprintf(buf, sizeof(buf), "%+.1f\xB0", state.pitch);
    {
      String s = buf;
      RowCache &rc = rowCache[9][10];
      if (rc.value != s || rc.fg != fg || rc.bg != bg) {
        printValue(tft, F, xL, y4, wL, h4, "Pitch:", s, fg, bg, COL_BACK, printState[9][10]);
        rc.value = s; rc.fg = fg; rc.bg = bg;
      }
    }

    angleColor(state.roll, ROVER_ROLL_WARN_DEG, ROVER_ROLL_ALARM_DEG, fg, bg);
    snprintf(buf, sizeof(buf), "%+.1f\xB0", state.roll);
    {
      String s = buf;
      RowCache &rc = rowCache[9][11];
      if (rc.value != s || rc.fg != fg || rc.bg != bg) {
        printValue(tft, F, xR, y4, wR, h4, "Roll:", s, fg, bg, COL_BACK, printState[9][11]);
        rc.value = s; rc.fg = fg; rc.bg = bg;
      }
    }
  }

  // ── CTRL (rows 5-7) ──

  // Row 5 — Wheel throttle: FWD (green) / NEUT (dark grey) / REV (yellow)
  {
    float wt = state.wheelThrottle;
    const char *thrStr;
    uint16_t    thrFg;
    if      (wt >  0.01f) { thrStr = "FWD";  thrFg = TFT_DARK_GREEN; }
    else if (wt < -0.01f) { thrStr = "REV";  thrFg = TFT_YELLOW;     }
    else                  { thrStr = "NEUT"; thrFg = TFT_DARK_GREY;  }
    rovVal(5, "Throttle:", thrStr, thrFg, TFT_BLACK);
  }

  // Row 6 — Gear | Brakes split  cache [2][6]=Gear, [2][7]=Brakes
  {
    uint16_t y6 = rowYFor(6, NR), h6 = rowHFor(NR);
    uint16_t xL = AX,                 wL = AHW - ROW_PAD;
    uint16_t xR = AX + AHW + ROW_PAD, wR = AHW - ROW_PAD;

    const char *gearStr = state.gear_on   ? "DOWN" : "UP";
    uint16_t    gearFg  = state.gear_on   ? TFT_DARK_GREEN : TFT_DARK_GREY;
    {
      RowCache &rc = rowCache[9][6];
      if (rc.value != gearStr || rc.fg != gearFg || rc.bg != TFT_BLACK) {
        printValue(tft, F, xL, y6, wL, h6, "Gear:", gearStr, gearFg, TFT_BLACK, COL_BACK, printState[9][6]);
        rc.value = gearStr; rc.fg = gearFg; rc.bg = TFT_BLACK;
      }
    }

    // Brakes: ON = green (engaged = intentional stop), OFF = dark grey (released)
    const char *brkStr = state.brakes_on ? "ON"   : "OFF";
    uint16_t    brkFg  = state.brakes_on ? TFT_DARK_GREEN : TFT_DARK_GREY;
    {
      RowCache &rc = rowCache[9][7];
      if (rc.value != brkStr || rc.fg != brkFg || rc.bg != TFT_BLACK) {
        printValue(tft, F, xR, y6, wR, h6, "Brakes:", brkStr, brkFg, TFT_BLACK, COL_BACK, printState[9][7]);
        rc.value = brkStr; rc.fg = brkFg; rc.bg = TFT_BLACK;
      }
    }
  }

  // Row 7 — EC% | SAS split  cache [2][12]=EC, [2][13]=SAS
  {
    uint16_t y7 = rowYFor(7, NR), h7 = rowHFor(NR);
    uint16_t xL = AX,                 wL = AHW - ROW_PAD;
    uint16_t xR = AX + AHW + ROW_PAD, wR = AHW - ROW_PAD;

    // EC%
    snprintf(buf, sizeof(buf), "%.0f%%", state.electricChargePercent);
    if      (state.electricChargePercent < ROVER_EC_ALARM_PCT) { fg = TFT_WHITE;     bg = TFT_RED;   }
    else if (state.electricChargePercent < ROVER_EC_WARN_PCT)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
    else                                                        { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    {
      String s = buf;
      RowCache &rc = rowCache[9][12];
      if (rc.value != s || rc.fg != fg || rc.bg != bg) {
        printValue(tft, F, xL, y7, wL, h7, "EC%:", s, fg, bg, COL_BACK, printState[9][12]);
        rc.value = s; rc.fg = fg; rc.bg = bg;
      }
    }

    // SAS — navball colours; OFF = dark grey (rover SAS off during drive is normal)
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
      RowCache &rc = rowCache[9][13];
      if (rc.value != sasStr || rc.fg != sasFg || rc.bg != TFT_BLACK) {
        printValue(tft, F, xR, y7, wR, h7, "SAS:", sasStr, sasFg, TFT_BLACK, COL_BACK, printState[9][13]);
        rc.value = sasStr; rc.fg = sasFg; rc.bg = TFT_BLACK;
      }
    }
  }
}
