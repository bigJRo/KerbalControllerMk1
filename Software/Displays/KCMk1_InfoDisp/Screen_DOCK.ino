/***************************************************************************************
   Screen_DOCK.ino -- Docking screen — chromeScreen_DOCK, drawScreen_DOCK
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


bool _dockChromDrawn = false;
bool _vesselDocked   = false;

static void chromeScreen_DOCK(RA8875 &tft) {
  if (_vesselDocked) {
    // ── DOCKED splash ── white text on dark green background, matching NO TARGET style
    _dockChromDrawn = false;
    tft.fillRect(0, TITLE_TOP, CONTENT_W, SCREEN_H - TITLE_TOP, TFT_BLACK);
    textCenter(tft, &Roboto_Black_72, 0, TITLE_TOP, CONTENT_W, SCREEN_H - TITLE_TOP,
               "DOCKED", TFT_WHITE, TFT_DARK_GREEN);
  } else if (state.targetAvailable) {
    _dockChromDrawn = true;
    static const tFont   *F      = &Roboto_Black_40;
    static const uint8_t  NR     = 8;
    static const uint16_t SECT_W = 26;
    static const uint16_t AX     = ROW_PAD + SECT_W;
    static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
    static const uint16_t AHW    = AW / 2;
    uint16_t rowH = rowHFor(NR);

    // Section labels: APCH(0-3), BRG(5-6)
    // DR(row4) and CT(row7) drawn AFTER their bounding dividers
    drawVerticalText(tft, 0, TITLE_TOP,          SECT_W, rowH*4, &Roboto_Black_16, "APCH", TFT_LIGHT_GREY, TFT_BLACK);
    drawVerticalText(tft, 0, TITLE_TOP + rowH*5, SECT_W, rowH*2, &Roboto_Black_16, "BRG",  TFT_LIGHT_GREY, TFT_BLACK);

    // APCH row labels (rows 0-3)
    printDispChrome(tft, F, AX, rowYFor(0,NR), AW, rowH, "Alt.SL:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(1,NR), AW, rowH, "Dist:",   COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(2,NR), AW, rowH, "V.Tgt:",  COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(3,NR), AW, rowH, "V.Drft:", COL_LABEL, COL_BACK, COL_NO_BDR);

    // Divider after APCH (row3/row4)
    {
      uint16_t d = rowYFor(4, NR) - 1;
      tft.drawLine(0, d - 1, CONTENT_W, d - 1, TFT_GREY);
      tft.drawLine(0, d,     CONTENT_W, d,     TFT_GREY);
    }
    // DR label drawn after its bounding dividers
    drawVerticalText(tft, 0, rowYFor(4,NR)+1, SECT_W, rowH-3, &Roboto_Black_16, "DR", TFT_LIGHT_GREY, TFT_BLACK);

    // Row 4: split Drft.H | Drft.V
    {
      uint16_t y = rowYFor(4, NR), h = rowH;
      printDispChrome(tft, F, AX,               y, AHW - ROW_PAD, h, "Drft.H:", COL_LABEL, COL_BACK, COL_NO_BDR);
      printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "Drft.V:", COL_LABEL, COL_BACK, COL_NO_BDR);
      for (int8_t dx = -1; dx <= 1; dx++)
        tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, rowYFor(5, NR) - 1, TFT_GREY);
    }

    // Divider after DR (row4/row5)
    {
      uint16_t d = rowYFor(5, NR) - 1;
      tft.drawLine(0, d - 1, CONTENT_W, d - 1, TFT_GREY);
      tft.drawLine(0, d,     CONTENT_W, d,     TFT_GREY);
    }

    // Row 5: split Brg.Err | Elv.Err (angle between target bearing and velocity bearing)
    {
      uint16_t y = rowYFor(5, NR), h = rowH;
      printDispChrome(tft, F, AX,               y, AHW - ROW_PAD, h, "Brg.Err:", COL_LABEL, COL_BACK, COL_NO_BDR);
      printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "Elv.Err:", COL_LABEL, COL_BACK, COL_NO_BDR);
      for (int8_t dx = -1; dx <= 1; dx++)
        tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, rowYFor(6, NR) - 1, TFT_GREY);
    }

    // Row 6: split Brg | Elev (raw absolute bearing/elevation — reference only)
    {
      uint16_t y = rowYFor(6, NR), h = rowH;
      printDispChrome(tft, F, AX,               y, AHW - ROW_PAD, h, "Brg:",  COL_LABEL, COL_BACK, COL_NO_BDR);
      printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "Elev:", COL_LABEL, COL_BACK, COL_NO_BDR);
      for (int8_t dx = -1; dx <= 1; dx++)
        tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, rowYFor(7, NR) - 1, TFT_GREY);
    }

    // Divider after BRG (row6/row7)
    {
      uint16_t d = rowYFor(7, NR) - 1;
      tft.drawLine(0, d - 1, CONTENT_W, d - 1, TFT_GREY);
      tft.drawLine(0, d,     CONTENT_W, d,     TFT_GREY);
    }
    // CT label drawn after its bounding dividers
    drawVerticalText(tft, 0, rowYFor(7,NR)+1, SECT_W, rowH-3, &Roboto_Black_16, "CT", TFT_LIGHT_GREY, TFT_BLACK);

    // Row 7: split RCS | SAS
    {
      uint16_t y = rowYFor(7, NR), h = rowH;
      printDispChrome(tft, F, AX,               y, AHW - ROW_PAD, h, "RCS:", COL_LABEL, COL_BACK, COL_NO_BDR);
      printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "SAS:", COL_LABEL, COL_BACK, COL_NO_BDR);
      for (int8_t dx = -1; dx <= 1; dx++)
        tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, rowYFor(8, NR) - 1, TFT_GREY);
    }
  } else {
    _dockChromDrawn = false;
    // Clear content area — no chrome needed for NO TARGET state
    tft.fillRect(0, TITLE_TOP, CONTENT_W, SCREEN_H - TITLE_TOP, TFT_BLACK);
    textCenter(tft, &Roboto_Black_72, 0, TITLE_TOP, CONTENT_W, SCREEN_H - TITLE_TOP,
               "NO TARGET", TFT_WHITE, TFT_RED);
  }
}

static void drawScreen_DOCK(RA8875 &tft) {

  // DOCKED splash is static — chrome drew it, nothing to update each frame
  if (_vesselDocked) return;

  if (!state.targetAvailable) {
    if (_dockChromDrawn) {
      _dockChromDrawn = false;
      switchToScreen(screen_DOCK);
    }
    return;
  }

  if (!_dockChromDrawn) {
    switchToScreen(screen_DOCK);
    return;
  }

  static const tFont   *F      = &Roboto_Black_40;
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t AX     = ROW_PAD + SECT_W;
  static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
  static const uint16_t AHW    = AW / 2;
  uint16_t fg, bg;
  char buf[16];
  uint16_t xL = AX,               wL = AHW - ROW_PAD;
  uint16_t xR = AX + AHW + ROW_PAD, wR = AHW - ROW_PAD;

  // Redraw dividers every loop to survive value updates
  // div1: between V.Drft (row 3) and Drft.H|Drft.V (row 4)
  // div2: between Drft.H|Drft.V (row 4) and Brg|Elev (row 5)
  // div3: between Vel.Brg|Vel.Elv (row 6) and RCS|SAS (row 7)
  {
    uint16_t d1 = rowYFor(4, NR) - 1;
    tft.drawLine(0, d1 - 1, CONTENT_W, d1 - 1, TFT_GREY);
    tft.drawLine(0, d1,     CONTENT_W, d1,     TFT_GREY);
    uint16_t d2 = rowYFor(5, NR) - 1;
    tft.drawLine(0, d2 - 1, CONTENT_W, d2 - 1, TFT_GREY);
    tft.drawLine(0, d2,     CONTENT_W, d2,     TFT_GREY);
    uint16_t d3 = rowYFor(7, NR) - 1;
    tft.drawLine(0, d3 - 1, CONTENT_W, d3 - 1, TFT_GREY);
    tft.drawLine(0, d3,     CONTENT_W, d3,     TFT_GREY);
  }

  // Cache-checked draw helper using section-label-aware geometry
  auto dockVal = [&](uint8_t row, const char *label, const String &val,
                     uint16_t fgc, uint16_t bgc) {
    RowCache &rc = rowCache[6][row];
    if (rc.value == val && rc.fg == fgc && rc.bg == bgc) return;
    printValue(tft, F, AX, rowYFor(row, NR), AW, rowHFor(NR),
               label, val, fgc, bgc, COL_BACK, printState[6][row]);
    rc.value = val; rc.fg = fgc; rc.bg = bgc;
  };

  // Row 0 — Own altitude: simple red/green
  fg = (state.altitude < 0) ? TFT_RED : TFT_DARK_GREEN;
  dockVal(0, "Alt.SL:", formatAlt(state.altitude), fg, TFT_BLACK);

  // Row 1 — Distance: yellow <200m, white-on-red <50m
  if      (state.tgtDistance < DOCK_DIST_ALARM_M)  { fg = TFT_WHITE;     bg = TFT_RED;   }
  else if (state.tgtDistance < DOCK_DIST_WARN_M)   { fg = TFT_YELLOW;    bg = TFT_BLACK; }
  else                                 { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
  dockVal(1, "Dist:", formatAlt(state.tgtDistance), fg, bg);

  // Row 2 — V.Tgt: KSP1 targetMessage.velocity is always positive (magnitude only).
  // No closure direction is available, so show as plain informational green.
  dockVal(2, "V.Tgt:", fmtMs(state.tgtVelocity), TFT_DARK_GREEN, TFT_BLACK);

  // Compute drift decomposition (used by rows 3 and 4)
  float v_yaw, v_pitch, v_lat_mag;
  {
    auto toUnit = [](float hdg_deg, float pit_deg, float out[3]) {
      float h = hdg_deg * 0.017453f, p = pit_deg * 0.017453f;
      out[0] = cosf(p)*sinf(h);
      out[1] = cosf(p)*cosf(h);
      out[2] = sinf(p);
    };

    float vel_unit[3], tgt_unit[3];
    toUnit(state.tgtVelHeading, state.tgtVelPitch, vel_unit);
    toUnit(state.tgtHeading,    state.tgtPitch,    tgt_unit);

    float speed   = fabsf(state.tgtVelocity);
    float vel_vec[3] = { vel_unit[0]*speed, vel_unit[1]*speed, vel_unit[2]*speed };

    float v_app = vel_vec[0]*tgt_unit[0] + vel_vec[1]*tgt_unit[1] + vel_vec[2]*tgt_unit[2];
    float v_lat[3] = { vel_vec[0]-v_app*tgt_unit[0],
                       vel_vec[1]-v_app*tgt_unit[1],
                       vel_vec[2]-v_app*tgt_unit[2] };

    float world_up[3] = {0, 0, 1};
    float right[3] = {
      tgt_unit[1]*world_up[2] - tgt_unit[2]*world_up[1],
      tgt_unit[2]*world_up[0] - tgt_unit[0]*world_up[2],
      tgt_unit[0]*world_up[1] - tgt_unit[1]*world_up[0]
    };
    float r_mag = sqrtf(right[0]*right[0]+right[1]*right[1]+right[2]*right[2]);
    if (r_mag > 0.001f) { right[0]/=r_mag; right[1]/=r_mag; right[2]/=r_mag; }
    else                { right[0]=1; right[1]=0; right[2]=0; }

    float up[3] = {
      right[1]*tgt_unit[2] - right[2]*tgt_unit[1],
      right[2]*tgt_unit[0] - right[0]*tgt_unit[2],
      right[0]*tgt_unit[1] - right[1]*tgt_unit[0]
    };

    v_yaw     = v_lat[0]*right[0] + v_lat[1]*right[1] + v_lat[2]*right[2];
    v_pitch   = v_lat[0]*up[0]    + v_lat[1]*up[1]    + v_lat[2]*up[2];
    v_lat_mag = sqrtf(v_lat[0]*v_lat[0]+v_lat[1]*v_lat[1]+v_lat[2]*v_lat[2]);
  }

  // Drift colour helper: green <0.1, yellow <0.5, white-on-red >=0.5
  auto driftColor = [](float v, uint16_t &fg, uint16_t &bg) {
    float av = fabsf(v);
    if      (av >= DOCK_DRIFT_ALARM_MS) { fg = TFT_WHITE;     bg = TFT_RED;   }
    else if (av >= DOCK_DRIFT_WARN_MS)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
    else                 { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
  };

  // Row 3 — V.Drft: total lateral drift magnitude
  snprintf(buf, sizeof(buf), "%.2f m/s", v_lat_mag);
  driftColor(v_lat_mag, fg, bg);
  dockVal(3, "V.Drft:", buf, fg, bg);

  // Row 4 — Split: Drft.H (left, cache[9][4]) | Drft.V (right, cache[9][9])
  {
    uint16_t y4 = rowYFor(4, NR), h4 = rowHFor(NR);

    snprintf(buf, sizeof(buf), "%+.2f m/s", v_yaw);
    driftColor(v_yaw, fg, bg);
    {
      String s = buf;
      RowCache &rc = rowCache[6][4];
      if (rc.value != s || rc.fg != fg || rc.bg != bg) {
        printValue(tft, F, xL, y4, wL, h4, "Drft.H:", s, fg, bg, COL_BACK,
                   printState[6][4]);
        rc.value = s; rc.fg = fg; rc.bg = bg;
      }
    }

    snprintf(buf, sizeof(buf), "%+.2f m/s", v_pitch);
    driftColor(v_pitch, fg, bg);
    {
      String s = buf;
      RowCache &rc = rowCache[6][9];
      if (rc.value != s || rc.fg != fg || rc.bg != bg) {
        printValue(tft, F, xR, y4, wR, h4, "Drft.V:", s, fg, bg, COL_BACK,
                   printState[6][9]);
        rc.value = s; rc.fg = fg; rc.bg = bg;
      }
    }
  }

  // Row 5 — Approach angle errors: angle between target bearing and velocity bearing.
  // This tells the pilot how well they are flying toward the docking port.
  // Green < 2.5°, yellow < 5°, white-on-red >= 5°.
  // Slots: Brg.Err = cache[6][5], Elv.Err = cache[6][10]
  {
    uint16_t y5 = rowYFor(5, NR), h5 = rowHFor(NR);

    float brgErr = state.tgtHeading - state.tgtVelHeading;
    if (brgErr >  180.0f) brgErr -= 360.0f;
    if (brgErr < -180.0f) brgErr += 360.0f;

    float elvErr = state.tgtPitch - state.tgtVelPitch;

    auto errColor = [](float e, uint16_t &fg, uint16_t &bg) {
      float ae = fabsf(e);
      if      (ae >= DOCK_BRG_ALARM_DEG) { fg = TFT_WHITE;     bg = TFT_RED;   }
      else if (ae >= DOCK_BRG_WARN_DEG)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
      else                               { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    };

    errColor(brgErr, fg, bg);
    snprintf(buf, sizeof(buf), "%+.1f\xB0", brgErr);
    {
      String s = buf;
      RowCache &rc = rowCache[6][5];
      if (rc.value != s || rc.fg != fg || rc.bg != bg) {
        printValue(tft, F, xL, y5, wL, h5, "Brg.Err:", s, fg, bg, COL_BACK,
                   printState[6][5]);
        rc.value = s; rc.fg = fg; rc.bg = bg;
      }
    }

    errColor(elvErr, fg, bg);
    snprintf(buf, sizeof(buf), "%+.1f\xB0", elvErr);
    {
      String s = buf;
      RowCache &rc = rowCache[6][10];
      if (rc.value != s || rc.fg != fg || rc.bg != bg) {
        printValue(tft, F, xR, y5, wR, h5, "Elv.Err:", s, fg, bg, COL_BACK,
                   printState[6][10]);
        rc.value = s; rc.fg = fg; rc.bg = bg;
      }
    }
  }

  // Row 6 — Raw bearing/elevation to target: reference only (dark grey, no thresholds).
  // Slots: Brg = cache[6][6], Elev = cache[6][7]
  {
    uint16_t y6 = rowYFor(6, NR), h6 = rowHFor(NR);

    snprintf(buf, sizeof(buf), "%.1f\xB0", state.tgtHeading);
    {
      String s = buf;
      RowCache &rc = rowCache[6][6];
      if (rc.value != s || rc.fg != TFT_DARK_GREY || rc.bg != COL_BACK) {
        printValue(tft, F, xL, y6, wL, h6, "Brg:", s, TFT_DARK_GREY, COL_BACK, COL_BACK,
                   printState[6][6]);
        rc.value = s; rc.fg = TFT_DARK_GREY; rc.bg = COL_BACK;
      }
    }

    snprintf(buf, sizeof(buf), "%.1f\xB0", state.tgtPitch);
    {
      String s = buf;
      RowCache &rc = rowCache[6][7];
      if (rc.value != s || rc.fg != TFT_DARK_GREY || rc.bg != COL_BACK) {
        printValue(tft, F, xR, y6, wR, h6, "Elev:", s, TFT_DARK_GREY, COL_BACK, COL_BACK,
                   printState[6][7]);
        rc.value = s; rc.fg = TFT_DARK_GREY; rc.bg = COL_BACK;
      }
    }
  }

  // Row 7 — Split: RCS (left, cache[9][8]) | SAS (right, cache[9][12])
  {
    uint16_t y7 = rowYFor(7, NR), h7 = rowHFor(NR);

    const char *rcsStr = state.rcs_on ? "ON"  : "OFF";
    uint16_t    rcsFg  = state.rcs_on ? TFT_DARK_GREEN : TFT_WHITE;
    uint16_t    rcsBg  = state.rcs_on ? TFT_BLACK      : TFT_RED;
    {
      RowCache &rc = rowCache[6][8];
      if (rc.value != rcsStr || rc.fg != rcsFg || rc.bg != rcsBg) {
        printValue(tft, F, xL, y7, wL, h7, "RCS:", rcsStr, rcsFg, rcsBg, COL_BACK,
                   printState[6][8]);
        rc.value = rcsStr; rc.fg = rcsFg; rc.bg = rcsBg;
      }
    }

    const char *sasStr;
    uint16_t    sasFg, sasBg;
    switch (state.sasMode) {
      case 255: sasStr = "OFF";      sasFg = TFT_WHITE;       sasBg = TFT_RED;    break;  // SAS disabled — always alarm
      case 0:   sasStr = "STAB";     sasFg = TFT_SKY;         sasBg = TFT_BLACK;  break;  // valid hold but not ideal for docking
      case 1:   sasStr = "PROGRADE"; sasFg = TFT_RED;         sasBg = TFT_BLACK;  break;  // wrong mode for docking
      case 2:   sasStr = "RETRO";    sasFg = TFT_RED;         sasBg = TFT_BLACK;  break;  // wrong mode for docking
      case 3:   sasStr = "NORMAL";   sasFg = TFT_RED;         sasBg = TFT_BLACK;  break;  // wrong mode for docking
      case 4:   sasStr = "ANTI-NRM"; sasFg = TFT_RED;         sasBg = TFT_BLACK;  break;  // wrong mode for docking
      case 5:   sasStr = "RAD-OUT";  sasFg = TFT_RED;         sasBg = TFT_BLACK;  break;  // wrong mode for docking
      case 6:   sasStr = "RAD-IN";   sasFg = TFT_RED;         sasBg = TFT_BLACK;  break;  // wrong mode for docking
      case 7:   sasStr = "TARGET";   sasFg = TFT_DARK_GREEN;  sasBg = TFT_BLACK;  break;  // correct for docking approach
      case 8:   sasStr = "ANTI-TGT"; sasFg = TFT_RED;         sasBg = TFT_BLACK;  break;  // pointing away — wrong for docking
      case 9:   sasStr = "MANEUVER"; sasFg = TFT_RED;         sasBg = TFT_BLACK;  break;  // wrong mode for docking
      default:  sasStr = "---";      sasFg = TFT_DARK_GREY;   sasBg = TFT_BLACK; break;
    }
    {
      RowCache &rc = rowCache[6][12];
      if (rc.value != sasStr || rc.fg != sasFg || rc.bg != sasBg) {
        printValue(tft, F, xR, y7, wR, h7, "SAS:", sasStr, sasFg, sasBg, COL_BACK,
                   printState[6][12]);
        rc.value = sasStr; rc.fg = sasFg; rc.bg = sasBg;
      }
    }
  }
}

/***************************************************************************************
   PUBLIC INTERFACE — called from loop()
****************************************************************************************/

