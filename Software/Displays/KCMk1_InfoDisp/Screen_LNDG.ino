/***************************************************************************************
   Screen_LNDG.ino -- Landing screen (Powered Descent + Re-entry) — chromeScreen_LNDG, drawScreen_LNDG
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


bool _lndgReentryMode = false;

static void chromeScreen_LNDG(RA8875 &tft) {
  static const tFont   *F      = &Roboto_Black_40;
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t AX     = ROW_PAD + SECT_W;
  static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
  static const uint16_t AHW    = AW / 2;
  uint16_t rowH = rowHFor(NR);

  // Title bar shows current mode — tapping it toggles (handled in TouchEvents)
  drawTitleBar(tft, _lndgReentryMode ? "RE-ENTRY" : "POWERED DESCENT");

  // STATE section label (rows 0-4, both modes)
  drawVerticalText(tft, 0, TITLE_TOP, SECT_W, rowH*5, &Roboto_Black_16, "STATE", TFT_LIGHT_GREY, TFT_BLACK);

  // STATE rows 0-4: mostly shared, row 3 differs by mode
  printDispChrome(tft, F, AX, rowYFor(0,NR), AW, rowH, "T.Grnd:",  COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(1,NR), AW, rowH, "Alt.Rdr:", COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(2,NR), AW, rowH, "V.Vrt:",   COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(4,NR), AW, rowH, "V.Srf:",   COL_LABEL, COL_BACK, COL_NO_BDR);

  if (!_lndgReentryMode) {
    // Row 3: Fwd | Lat split — heading-relative horizontal velocity components
    {
      uint16_t y = rowYFor(3, NR), h = rowH;
      printDispChrome(tft, F, AX,                 y, AHW - ROW_PAD, h, "Fwd:", COL_LABEL, COL_BACK, COL_NO_BDR);
      printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "Lat:", COL_LABEL, COL_BACK, COL_NO_BDR);
      for (int8_t dx = -1; dx <= 1; dx++)
        tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, rowYFor(4,NR) - 1, TFT_GREY);
    }
    // === POWERED DESCENT rows 5-7 ===

    // VEH section label (rows 6-7) drawn first so divider can overwrite its top
    drawVerticalText(tft, 0, TITLE_TOP + rowH*6, SECT_W, rowH*2, &Roboto_Black_16, "VEH", TFT_LIGHT_GREY, TFT_BLACK);

    // Row 5: ΔV.Stg row label
    printDispChrome(tft, F, AX, rowYFor(5,NR), AW, rowH, "\xCE\x94V.Stg:", COL_LABEL, COL_BACK, COL_NO_BDR);

    // Row 6: Throttle
    printDispChrome(tft, F, AX, rowYFor(6,NR), AW, rowH, "Throttle:", COL_LABEL, COL_BACK, COL_NO_BDR);

    // Row 7: Gear | Brakes split
    {
      uint16_t y = rowYFor(7, NR), h = rowH;
      printDispChrome(tft, F, AX,               y, AHW - ROW_PAD, h, "Gear:",   COL_LABEL, COL_BACK, COL_NO_BDR);
      printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "Brakes:", COL_LABEL, COL_BACK, COL_NO_BDR);
      for (int8_t dx = -1; dx <= 1; dx++)
        tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, rowYFor(8,NR) - 1, TFT_GREY);
    }

  } else {
    // Row 3: V.Hrz (re-entry — single value, parachutes handle drift)
    printDispChrome(tft, F, AX, rowYFor(3,NR), AW, rowH, "V.Hrz:", COL_LABEL, COL_BACK, COL_NO_BDR);

    // === RE-ENTRY rows 5-7 ===

    // VEH section label (rows 6-7) drawn first so divider can overwrite its top
    drawVerticalText(tft, 0, TITLE_TOP + rowH*6, SECT_W, rowH*2, &Roboto_Black_16, "VEH", TFT_LIGHT_GREY, TFT_BLACK);

    // Row 5: Mach | G split (ATM block — drawn after VEH so divider order is correct)
    {
      uint16_t y = rowYFor(5, NR), h = rowH;
      printDispChrome(tft, F, AX,               y, AHW - ROW_PAD, h, "Mach:", COL_LABEL, COL_BACK, COL_NO_BDR);
      printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "G:",    COL_LABEL, COL_BACK, COL_NO_BDR);
      for (int8_t dx = -1; dx <= 1; dx++)
        tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, rowYFor(6,NR) - 1, TFT_GREY);
    }

    // Row 6: Drogue | Main split
    {
      uint16_t y = rowYFor(6, NR), h = rowH;
      printDispChrome(tft, F, AX,               y, AHW - ROW_PAD, h, "Drogue:", COL_LABEL, COL_BACK, COL_NO_BDR);
      printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "Main:",   COL_LABEL, COL_BACK, COL_NO_BDR);
      for (int8_t dx = -1; dx <= 1; dx++)
        tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, rowYFor(7,NR) - 1, TFT_GREY);
    }

    // Row 7: Gear | Brakes split
    {
      uint16_t y = rowYFor(7, NR), h = rowH;
      printDispChrome(tft, F, AX,               y, AHW - ROW_PAD, h, "Gear:",   COL_LABEL, COL_BACK, COL_NO_BDR);
      printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "Brakes:", COL_LABEL, COL_BACK, COL_NO_BDR);
      for (int8_t dx = -1; dx <= 1; dx++)
        tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, rowYFor(8,NR) - 1, TFT_GREY);
    }
  }

  // Dividers — drawn LAST, +1 offset to clear printValue fillRect boundary
  // div1: after STATE block (row4/row5), div2: after single-row block (row5/row6)
  uint16_t d1 = TITLE_TOP + rowH*5 + 1;
  uint16_t d2 = TITLE_TOP + rowH*6 + 1;
  tft.drawLine(0, d1,   CONTENT_W, d1,   TFT_GREY);
  tft.drawLine(0, d1+1, CONTENT_W, d1+1, TFT_GREY);
  tft.drawLine(0, d2,   CONTENT_W, d2,   TFT_GREY);
  tft.drawLine(0, d2+1, CONTENT_W, d2+1, TFT_GREY);

  // Single-row block label drawn AFTER both bounding dividers to avoid overwrite
  // Powered descent: STG (row5 = ΔV.Stg); Re-entry: ATM (row5 = Mach|G)
  {
    uint16_t labelY = TITLE_TOP + rowH*5 + 2;  // 2px below d1
    uint16_t labelH = rowH - 4;                // stay clear of d2 at bottom
    const char *singleLabel = _lndgReentryMode ? "AT" : "ST";
    drawVerticalText(tft, 0, labelY, SECT_W, labelH, &Roboto_Black_16, singleLabel, TFT_LIGHT_GREY, TFT_BLACK);
  }
}


static void drawScreen_LNDG(RA8875 &tft) {
  static const tFont   *F      = &Roboto_Black_40;
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t AX     = ROW_PAD + SECT_W;
  static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
  static const uint16_t AHW    = AW / 2;
  uint16_t fg, bg;
  char buf[16];

  // Parachute state tracking — persists across loop() calls.
  static bool _drogueDeployed = false;
  static bool _mainDeployed   = false;
  static bool _drogueCut      = false;
  static bool _mainCut        = false;

  // Cache-checked draw helper using section-label-aware geometry
  auto lndgVal = [&](uint8_t row, const char *label, const String &val,
                     uint16_t fgc, uint16_t bgc) {
    RowCache &rc = rowCache[3][row];
    if (rc.value == val && rc.fg == fgc && rc.bg == bgc) return;
    printValue(tft, F, AX, rowYFor(row, NR), AW, rowHFor(NR),
               label, val, fgc, bgc, COL_BACK, printState[3][row]);
    rc.value = val; rc.fg = fgc; rc.bg = bgc;
  };

  // ── STATE block (rows 0-4): shared by both modes ──
  // Option C order: T.Grnd → Alt.Rdr → V.Vrt → V.Hrz → V.Srf
  // Most critical first (time to impact), supporting data below.

  // Precompute T.Grnd and V.Hrz — used by multiple rows
  float tGround = (state.verticalVel < 0.0f && state.radarAlt > 0.0f)
                  ? fabsf(state.radarAlt / state.verticalVel) : -1.0f;
  float vSq  = state.surfaceVel * state.surfaceVel - state.verticalVel * state.verticalVel;
  float hSpd = (vSq > 0.0f) ? sqrtf(vSq) : 0.0f;

  // Row 0 — T.Grnd: most critical — how long do I have
  // Gear-aware colouring:
  //   Gear UP:   T.Grnd <10s = red (matches annunciator CW_GROUND_PROX), <30s = yellow
  //   Gear DOWN: V.Vrt < -8 m/s = red (crash landing), < -5 m/s = yellow, else green
  //              T.Grnd <30s adds yellow caution regardless of speed
  if (tGround >= 0.0f) {
    if (!state.gear_on) {
      // Gear UP — time-based alarm matching annunciator
      fg = (tGround < LNDG_TGRND_ALARM_S) ? TFT_WHITE  :
           (tGround < LNDG_TGRND_WARN_S) ? TFT_YELLOW  : TFT_DARK_GREEN;
      bg = (tGround < LNDG_TGRND_ALARM_S) ? TFT_RED    : TFT_BLACK;
    } else {
      // Gear DOWN — speed-based with T.Grnd caution floor
      if      (state.verticalVel < LNDG_VVRT_ALARM_MS)  { fg = TFT_WHITE;     bg = TFT_RED;   }
      else if (state.verticalVel < LNDG_VVRT_WARN_MS ||
               tGround < LNDG_TGRND_WARN_S)   { fg = TFT_YELLOW;    bg = TFT_BLACK; }
      else                                 { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    }
    lndgVal(0, "T.Grnd:", formatTime(tGround), fg, bg);
  } else {
    lndgVal(0, "T.Grnd:", "---", TFT_DARK_GREEN, TFT_BLACK);
  }

  // Row 1 — Alt.Rdr: where am I
  fg = (state.radarAlt < ALT_RDR_ALARM_M)  ? TFT_WHITE  :
       (state.radarAlt < ALT_RDR_WARN_M) ? TFT_YELLOW  : TFT_DARK_GREEN;
  bg = (state.radarAlt < ALT_RDR_ALARM_M)  ? TFT_RED    : TFT_BLACK;
  lndgVal(1, "Alt.Rdr:", formatAlt(state.radarAlt), fg, bg);

  // Row 2 — V.Vrt: am I slowing the descent
  // Same gear-aware logic as T.Grnd for consistency with annunciator
  if (tGround >= 0.0f) {
    if (!state.gear_on) {
      fg = (tGround < LNDG_TGRND_ALARM_S) ? TFT_WHITE  :
           (tGround < LNDG_TGRND_WARN_S) ? TFT_YELLOW  : TFT_DARK_GREEN;
      bg = (tGround < LNDG_TGRND_ALARM_S) ? TFT_RED    : TFT_BLACK;
    } else {
      if      (state.verticalVel < LNDG_VVRT_ALARM_MS)  { fg = TFT_WHITE;     bg = TFT_RED;   }
      else if (state.verticalVel < LNDG_VVRT_WARN_MS ||
               tGround < LNDG_TGRND_WARN_S)   { fg = TFT_YELLOW;    bg = TFT_BLACK; }
      else                                 { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    }
  } else {
    fg = TFT_DARK_GREEN; bg = TFT_BLACK;
  }
  lndgVal(2, "V.Vrt:", fmtMs(state.verticalVel), fg, bg);

  // Row 4 — V.Srf: total speed reference (shared)
  fg = (state.surfaceVel < 0) ? TFT_RED : TFT_DARK_GREEN;
  lndgVal(4, "V.Srf:", fmtMs(state.surfaceVel), fg, TFT_BLACK);

  // Row 3 — mode-dependent horizontal velocity display
  if (!_lndgReentryMode) {
    // Powered descent: decompose into Fwd (heading-aligned) and Lat (perpendicular)
    // +Fwd = moving in heading direction, -Fwd = moving backward
    // +Lat = drifting right of heading, -Lat = drifting left
    float headRad    = state.heading      * 0.017453f;
    float velHdgRad  = state.srfVelHeading * 0.017453f;
    float velNorth   = state.surfaceVel * cosf(velHdgRad);
    float velEast    = state.surfaceVel * sinf(velHdgRad);
    float fwdNorth   = cosf(headRad);
    float fwdEast    = sinf(headRad);
    float vFwd = velNorth * fwdNorth  + velEast * fwdEast;   // along heading
    float vLat = velNorth * (-fwdEast) + velEast * fwdNorth; // right of heading

    // Context-dependent horizontal thresholds — tighten as ground approaches
    // tGround: >60s = loose, 30-60s = mid, 10-30s = tight, <10s = final
    auto horzColor = [&](float v, uint16_t &fg, uint16_t &bg) {
      float av = fabsf(v);
      float warnMs, alarmMs;
      if      (tGround < 0.0f || tGround > LNDG_HVEL_T_LOOSE_S) { warnMs = LNDG_HVEL_WARN_LOOSE_MS; alarmMs = LNDG_HVEL_ALARM_LOOSE_MS; }
      else if (tGround > LNDG_HVEL_T_MID_S)                   { warnMs = LNDG_HVEL_WARN_MID_MS;   alarmMs = LNDG_HVEL_ALARM_MID_MS;   }
      else if (tGround > LNDG_TGRND_ALARM_S)                   { warnMs = LNDG_HVEL_WARN_TIGHT_MS; alarmMs = LNDG_HVEL_ALARM_TIGHT_MS; }
      else                                         { warnMs = LNDG_HVEL_WARN_FINAL_MS; alarmMs = LNDG_HVEL_ALARM_FINAL_MS; }
      if      (av >= alarmMs) { fg = TFT_WHITE;     bg = TFT_RED;   }
      else if (av >= warnMs)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
      else                    { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    };

    uint16_t xL = AX,                wL = AHW - ROW_PAD;
    uint16_t xR = AX + AHW + ROW_PAD, wR = AHW - ROW_PAD;
    uint16_t y3 = rowYFor(3, NR), h3 = rowHFor(NR);
    char buf3[12];

    snprintf(buf3, sizeof(buf3), "%+.1f m/s", vFwd);
    uint16_t ffg, fbg; horzColor(vFwd, ffg, fbg);
    {
      String s = buf3;
      RowCache &rc = rowCache[3][12];
      if (rc.value != s || rc.fg != ffg || rc.bg != fbg) {
        printValue(tft, F, xL, y3, wL, h3, "Fwd:", s, ffg, fbg, COL_BACK, printState[3][12]);
        rc.value = s; rc.fg = ffg; rc.bg = fbg;
      }
    }

    snprintf(buf3, sizeof(buf3), "%+.1f m/s", vLat);
    uint16_t lfg, lbg; horzColor(vLat, lfg, lbg);
    {
      String s = buf3;
      RowCache &rc = rowCache[3][13];
      if (rc.value != s || rc.fg != lfg || rc.bg != lbg) {
        printValue(tft, F, xR, y3, wR, h3, "Lat:", s, lfg, lbg, COL_BACK, printState[3][13]);
        rc.value = s; rc.fg = lfg; rc.bg = lbg;
      }
    }

  } else {
    // Re-entry: single V.Hrz total (parachutes handle drift)
    if      (hSpd > LNDG_REENTRY_VHRZ_ALARM_MS) { fg = TFT_WHITE;     bg = TFT_RED;   }
    else if (hSpd > LNDG_REENTRY_VHRZ_WARN_MS)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
    else                   { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    lndgVal(3, "V.Hrz:", fmtMs(hSpd), fg, bg);
  }

  // ── Mode-specific rows 5-7 ──
  if (!_lndgReentryMode) {
    // ── POWERED DESCENT ──

    // Row 5 — ΔV.Stg (single-row STG block, indicator square in chrome)
    thresholdColor((uint16_t)constrain(state.stageDeltaV, 0, 65535),
                   DV_STG_ALARM_MS, TFT_WHITE,  TFT_RED,
                   DV_STG_WARN_MS,  TFT_YELLOW, TFT_BLACK,
                        TFT_DARK_GREEN, TFT_BLACK, fg, bg);
    lndgVal(5, "\xCE\x94V.Stg:", fmtMs(state.stageDeltaV), fg, bg);

    // Row 6 — Throttle (VEH block)
    {
      uint8_t thrPct = (uint8_t)constrain(state.throttle * 100.0f, 0.0f, 100.0f);
      if (thrPct == 0) { fg = TFT_WHITE;     bg = TFT_RED;   }
      else             { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
      lndgVal(6, "Throttle:", formatPerc(thrPct), fg, bg);
    }

    // Row 7 — Gear | Brakes (VEH block)
    // Slots [3][7] and [3][8] are shared with the re-entry branch — same row, same data.
    {
      uint16_t y = rowYFor(7, NR), h = rowHFor(NR);

      const char *gearStr = state.gear_on ? "DOWN" : "UP";
      uint16_t    gearFg  = state.gear_on ? TFT_DARK_GREEN : TFT_WHITE;
      uint16_t    gearBg  = state.gear_on ? TFT_BLACK      : TFT_RED;
      RowCache   &gc = rowCache[3][7];
      if (gc.value != gearStr || gc.fg != gearFg || gc.bg != gearBg) {
        printValue(tft, F, AX, y, AHW - ROW_PAD, h,
                   "Gear:", gearStr, gearFg, gearBg, COL_BACK,
                   printState[3][7]);
        gc.value = gearStr; gc.fg = gearFg; gc.bg = gearBg;
      }

      const char *brkStr = state.brakes_on ? "ON"  : "OFF";
      bool groundState = (state.situation == sit_Landed ||
                          state.situation == sit_Splashed ||
                          state.situation == sit_PreLaunch);
      uint16_t brkFg = state.brakes_on ? TFT_DARK_GREEN :
                       groundState     ? TFT_WHITE : TFT_DARK_GREY;
      uint16_t brkBg = state.brakes_on ? TFT_BLACK :
                       groundState     ? TFT_RED    : TFT_BLACK;
      RowCache   &bc = rowCache[3][8];
      if (bc.value != brkStr || bc.fg != brkFg || bc.bg != brkBg) {
        printValue(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h,
                   "Brakes:", brkStr, brkFg, brkBg, COL_BACK,
                   printState[3][8]);
        bc.value = brkStr; bc.fg = brkFg; bc.bg = brkBg;
      }
    }

  } else {
    // ── RE-ENTRY ──

    // Dynamic pressure for chute safety
    float q = 0.5f * state.airDensity * state.surfaceVel * state.surfaceVel;

    auto chuteLevel = [](float q, float safeLimit, float unsafeLimit) -> uint8_t {
      if (q >= unsafeLimit) return 2;
      if (q >= safeLimit)   return 1;
      return 0;
    };

    // Row 5 — Mach | G split (single-row AERO block, cache[3][9] and [3][10])
    {
      uint16_t xL = AX,               wL = AHW - ROW_PAD;
      uint16_t xR = AX + AHW + ROW_PAD, wR = AHW - ROW_PAD;

      float m = state.machNumber;
      snprintf(buf, sizeof(buf), "%.2f", m);
      bool transonic = (m >= 0.85f && m <= 1.2f);
      {
        String ms = buf;
        uint16_t mfg = transonic ? TFT_YELLOW : TFT_DARK_GREEN;
        RowCache &mc = rowCache[3][9];
        if (mc.value != ms || mc.fg != mfg || mc.bg != TFT_BLACK) {
          printValue(tft, F, xL, rowYFor(5,NR), wL, rowHFor(NR),
                     "Mach:", ms, mfg, TFT_BLACK, COL_BACK, printState[3][9]);
          mc.value = ms; mc.fg = mfg; mc.bg = TFT_BLACK;
        }
      }

      float g = state.gForce;
      snprintf(buf, sizeof(buf), "%.2f", g);
      if      (g > G_ALARM_POS || g < G_ALARM_NEG) { fg = TFT_WHITE;     bg = TFT_RED;   }
      else if (g > G_WARN_POS  || g < G_WARN_NEG)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
      else                             { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
      {
        String gs = buf;
        RowCache &gc = rowCache[3][10];
        if (gc.value != gs || gc.fg != fg || gc.bg != bg) {
          printValue(tft, F, xR, rowYFor(5,NR), wR, rowHFor(NR),
                     "G:", gs, fg, bg, COL_BACK, printState[3][10]);
          gc.value = gs; gc.fg = fg; gc.bg = bg;
        }
      }
    }

    // Parachute state tracking
    if (state.drogueDeploy && !_drogueDeployed) _drogueDeployed = true;
    if (state.drogueCut    && !_drogueCut)      { _drogueCut = true; _drogueDeployed = false; }
    if (state.mainDeploy   && !_mainDeployed)   _mainDeployed = true;
    if (state.mainCut      && !_mainCut)        { _mainCut = true;   _mainDeployed = false; }

    auto chuteColor = [&](bool deployed, bool cut,
                          float safeLimit, float unsafeLimit,
                          uint16_t &fg, uint16_t &bg) {
      if (cut) { fg = TFT_RED; bg = TFT_BLACK; return; }
      if (deployed) {
        uint8_t lvl = chuteLevel(q, safeLimit, unsafeLimit);
        fg = (lvl >= 1) ? TFT_RED : TFT_DARK_GREEN;
        bg = TFT_BLACK; return;
      }
      uint8_t lvl = chuteLevel(q, safeLimit, unsafeLimit);
      if      (lvl == 2) { fg = TFT_WHITE;     bg = TFT_RED;   }
      else if (lvl == 1) { fg = TFT_YELLOW;    bg = TFT_BLACK; }
      else               { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    };

    // Row 6 — Drogue | Main split (VEH block, cache[3][6] and [3][11])
    {
      uint16_t xL = AX,               wL = AHW - ROW_PAD;
      uint16_t xR = AX + AHW + ROW_PAD, wR = AHW - ROW_PAD;
      uint16_t y6 = rowYFor(6, NR), h6 = rowHFor(NR);

      const char *drogueVal = _drogueCut ? "CUT" : _drogueDeployed ? "OPEN" : "STOWED";
      uint16_t dfg, dbg;
      chuteColor(_drogueDeployed, _drogueCut, LNDG_DROGUE_SAFE_Q, LNDG_DROGUE_UNSAFE_Q, dfg, dbg);
      {
        String ds = drogueVal;
        RowCache &dc = rowCache[3][6];
        if (dc.value != ds || dc.fg != dfg || dc.bg != dbg) {
          printValue(tft, F, xL, y6, wL, h6, "Drogue:", ds, dfg, dbg, COL_BACK,
                     printState[3][6]);
          dc.value = ds; dc.fg = dfg; dc.bg = dbg;
        }
      }

      const char *mainVal = _mainCut ? "CUT" : _mainDeployed ? "OPEN" : "STOWED";
      uint16_t mfg, mbg;
      chuteColor(_mainDeployed, _mainCut, LNDG_MAIN_SAFE_Q, LNDG_MAIN_UNSAFE_Q, mfg, mbg);
      {
        String ms = mainVal;
        RowCache &mc = rowCache[3][11];
        if (mc.value != ms || mc.fg != mfg || mc.bg != mbg) {
          printValue(tft, F, xR, y6, wR, h6, "Main:", ms, mfg, mbg, COL_BACK,
                     printState[3][11]);
          mc.value = ms; mc.fg = mfg; mc.bg = mbg;
        }
      }
    }

    // Row 7 — Gear | Brakes (VEH block, cache[3][7] and [3][8])
    // Re-entry: gear UP is normal (capsule), show dark grey not alarm
    {
      uint16_t y = rowYFor(7, NR), h = rowHFor(NR);

      const char *gearStr = state.gear_on ? "DOWN" : "UP";
      uint16_t    gearFg  = state.gear_on ? TFT_DARK_GREEN : TFT_DARK_GREY;
      uint16_t    gearBg  = TFT_BLACK;  // never alarm for gear on re-entry
      RowCache   &gc = rowCache[3][7];
      if (gc.value != gearStr || gc.fg != gearFg || gc.bg != gearBg) {
        printValue(tft, F, AX, y, AHW - ROW_PAD, h,
                   "Gear:", gearStr, gearFg, gearBg, COL_BACK,
                   printState[3][7]);
        gc.value = gearStr; gc.fg = gearFg; gc.bg = gearBg;
      }

      const char *brkStr = state.brakes_on ? "ON"  : "OFF";
      bool groundState = (state.situation == sit_Landed ||
                          state.situation == sit_Splashed ||
                          state.situation == sit_PreLaunch);
      uint16_t brkFg = state.brakes_on ? TFT_DARK_GREEN :
                       groundState     ? TFT_WHITE : TFT_DARK_GREY;
      uint16_t brkBg = state.brakes_on ? TFT_BLACK :
                       groundState     ? TFT_RED    : TFT_BLACK;
      RowCache   &bc = rowCache[3][8];
      if (bc.value != brkStr || bc.fg != brkFg || bc.bg != brkBg) {
        printValue(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h,
                   "Brakes:", brkStr, brkFg, brkBg, COL_BACK,
                   printState[3][8]);
        bc.value = brkStr; bc.fg = brkFg; bc.bg = brkBg;
      }
    }
  }
}

