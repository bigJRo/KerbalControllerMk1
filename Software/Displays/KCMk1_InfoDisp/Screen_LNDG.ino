/***************************************************************************************
   Screen_LNDG.ino -- Landing screen (Powered Descent + Re-entry) — chromeScreen_LNDG, drawScreen_LNDG
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


bool _lndgReentryMode    = false;
bool _lndgReentryRow3PeA = true;   // true = row 3 shows PeA (alt > 2km); false = V.Hrz

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

    // Row 6: Throttle | RCS split
    {
      uint16_t y = rowYFor(6, NR), h = rowH;
      printDispChrome(tft, F, AX,                 y, AHW - ROW_PAD, h, "Throttle:", COL_LABEL, COL_BACK, COL_NO_BDR);
      printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "RCS:",      COL_LABEL, COL_BACK, COL_NO_BDR);
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

  } else {
    // Row 3: label is altitude-dependent — PeA above 2km (de-orbit context),
    // V.Hrz below 2km (landing context). _lndgReentryRow3PeA tracks current mode.
    printDispChrome(tft, F, AX, rowYFor(3,NR), AW, rowH,
                    _lndgReentryRow3PeA ? "PeA:" : "V.Hrz:", COL_LABEL, COL_BACK, COL_NO_BDR);

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

    // Row 7: Gear | SAS split (re-entry — SAS replaces Brakes)
    {
      uint16_t y = rowYFor(7, NR), h = rowH;
      printDispChrome(tft, F, AX,               y, AHW - ROW_PAD, h, "Gear:",   COL_LABEL, COL_BACK, COL_NO_BDR);
      printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "SAS:", COL_LABEL, COL_BACK, COL_NO_BDR);
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
}  // end chromeScreen_LNDG


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
    RowCache &rc = rowCache[8][row];
    if (rc.value == val && rc.fg == fgc && rc.bg == bgc) return;
    printValue(tft, F, AX, rowYFor(row, NR), AW, rowHFor(NR),
               label, val, fgc, bgc, COL_BACK, printState[8][row]);
    rc.value = val; rc.fg = fgc; rc.bg = bgc;
  };

  // ── STATE block (rows 0-4): shared by both modes ──
  // Option C order: T.Grnd → Alt.Rdr → V.Vrt → V.Hrz → V.Srf
  // Most critical first (time to impact), supporting data below.

  // Precompute T.Grnd and V.Hrz — used by multiple rows
  // Guard: only compute T.Grnd when descending faster than 0.1 m/s to avoid
  // a near-zero denominator producing a huge/noisy quotient on the pad.
  float tGround = (state.verticalVel < -0.05f && state.radarAlt > 0.0f)
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
      RowCache &rc = rowCache[8][12];
      if (rc.value != s || rc.fg != ffg || rc.bg != fbg) {
        printValue(tft, F, xL, y3, wL, h3, "Fwd:", s, ffg, fbg, COL_BACK, printState[8][12]);
        rc.value = s; rc.fg = ffg; rc.bg = fbg;
      }
    }

    snprintf(buf3, sizeof(buf3), "%+.1f m/s", vLat);
    uint16_t lfg, lbg; horzColor(vLat, lfg, lbg);
    {
      String s = buf3;
      RowCache &rc = rowCache[8][13];
      if (rc.value != s || rc.fg != lfg || rc.bg != lbg) {
        printValue(tft, F, xR, y3, wR, h3, "Lat:", s, lfg, lbg, COL_BACK, printState[8][13]);
        rc.value = s; rc.fg = lfg; rc.bg = lbg;
      }
    }

  } else {
    // Re-entry row 3: PeA above 2km (de-orbit context), V.Hrz below 2km (terminal)
    bool wantPeA = (state.radarAlt > 2000.0f || !state.inAtmo);
    if (wantPeA != _lndgReentryRow3PeA) {
      // Altitude crossed the 2km boundary — redraw chrome to relabel row 3
      _lndgReentryRow3PeA = wantPeA;
      rowCache[8][3].value = "\x01";  // invalidate row 3 cache to force redraw
      switchToScreen(screen_LNDG);
      return;
    }

    if (wantPeA) {
      // PeA with same colour convention as APSI screen
      float warnAlt = (currentBody.radius > 0.0f)
                      ? max(currentBody.minSafe, currentBody.flyHigh) : 0.0f;
      bool peWarn = (warnAlt > 0 && state.periapsis > 0 && state.periapsis < warnAlt);
      if      (state.periapsis < 0) fg = TFT_RED;
      else if (peWarn)              fg = TFT_YELLOW;
      else                          fg = TFT_DARK_GREEN;
      lndgVal(3, "PeA:", formatAlt(state.periapsis), fg, TFT_BLACK);
    } else {
      // V.Hrz — total horizontal speed
      if      (hSpd > LNDG_REENTRY_VHRZ_ALARM_MS) { fg = TFT_WHITE;     bg = TFT_RED;   }
      else if (hSpd > LNDG_REENTRY_VHRZ_WARN_MS)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
      else                                         { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
      lndgVal(3, "V.Hrz:", fmtMs(hSpd), fg, bg);
    }
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

    // Row 6 — Throttle | RCS split (VEH block)
    // Throttle: left half, slot 6. RCS: right half, slot 9 (free in powered-descent branch).
    {
      uint16_t y6 = rowYFor(6, NR), h6 = rowHFor(NR);

      uint8_t thrPct = (uint8_t)constrain(state.throttle * 100.0f, 0.0f, 100.0f);
      if (thrPct == 0) { fg = TFT_WHITE;     bg = TFT_RED;   }
      else             { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
      {
        String ts = formatPerc(thrPct);
        RowCache &tc = rowCache[8][6];
        if (tc.value != ts || tc.fg != fg || tc.bg != bg) {
          printValue(tft, F, AX, y6, AHW - ROW_PAD, h6,
                     "Throttle:", ts, fg, bg, COL_BACK, printState[8][6]);
          tc.value = ts; tc.fg = fg; tc.bg = bg;
        }
      }

      const char *rcsStr = state.rcs_on ? "ON" : "OFF";
      uint16_t rcsFg = state.rcs_on ? TFT_DARK_GREEN : TFT_DARK_GREY;
      RowCache &rc = rowCache[8][9];
      if (rc.value != rcsStr || rc.fg != rcsFg || rc.bg != COL_BACK) {
        printValue(tft, F, AX + AHW + ROW_PAD, y6, AHW - ROW_PAD, h6,
                   "RCS:", rcsStr, rcsFg, COL_BACK, COL_BACK, printState[8][9]);
        rc.value = rcsStr; rc.fg = rcsFg; rc.bg = COL_BACK;
      }
    }

    // Row 7 — Gear | Brakes (VEH block)
    // Slots [3][7] and [3][8] are shared with the re-entry branch — same row, same data.
    {
      uint16_t y = rowYFor(7, NR), h = rowHFor(NR);

      const char *gearStr = state.gear_on ? "DOWN" : "UP";
      uint16_t    gearFg  = state.gear_on ? TFT_DARK_GREEN : TFT_WHITE;
      uint16_t    gearBg  = state.gear_on ? TFT_BLACK      : TFT_RED;
      RowCache   &gc = rowCache[8][7];
      if (gc.value != gearStr || gc.fg != gearFg || gc.bg != gearBg) {
        printValue(tft, F, AX, y, AHW - ROW_PAD, h,
                   "Gear:", gearStr, gearFg, gearBg, COL_BACK,
                   printState[8][7]);
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
      RowCache   &bc = rowCache[8][8];
      if (bc.value != brkStr || bc.fg != brkFg || bc.bg != brkBg) {
        printValue(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h,
                   "Brakes:", brkStr, brkFg, brkBg, COL_BACK,
                   printState[8][8]);
        bc.value = brkStr; bc.fg = brkFg; bc.bg = brkBg;
      }
    }

  } else {
    // ── RE-ENTRY ──

    // Chute safety is based on surface velocity, matching KSP's own safe/risky/unsafe
    // indicator. See chuteState lambda below for full state machine.
    float spd = state.surfaceVel;

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
        RowCache &mc = rowCache[8][9];
        if (mc.value != ms || mc.fg != mfg || mc.bg != TFT_BLACK) {
          printValue(tft, F, xL, rowYFor(5,NR), wL, rowHFor(NR),
                     "Mach:", ms, mfg, TFT_BLACK, COL_BACK, printState[8][9]);
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
        RowCache &gc = rowCache[8][10];
        if (gc.value != gs || gc.fg != fg || gc.bg != bg) {
          printValue(tft, F, xR, rowYFor(5,NR), wR, rowHFor(NR),
                     "G:", gs, fg, bg, COL_BACK, printState[8][10]);
          gc.value = gs; gc.fg = fg; gc.bg = bg;
        }
      }
    }

    // Parachute state tracking
    if (state.drogueDeploy && !_drogueDeployed) _drogueDeployed = true;
    if (state.drogueCut    && !_drogueCut)      { _drogueCut = true; _drogueDeployed = false; }
    if (state.mainDeploy   && !_mainDeployed)   _mainDeployed = true;
    if (state.mainCut      && !_mainCut)        { _mainCut = true;   _mainDeployed = false; }

    // Chute state machine — six states per chute, priority order:
    //   CUT     — cut CAG fired                           → red on black
    //   OPEN*   — deploy CAG fired, speed > riskyLimit    → white on red (likely destroyed)
    //   ARMED   — deploy CAG fired, airDensity too low    → cyan on black (waiting for atmo)
    //   OPEN    — deploy CAG fired, density ok, alt > fullAlt → yellow on black (semi-deploying)
    //   OPEN    — deploy CAG fired, density ok, alt ≤ fullAlt → green on black (fully open)
    //   STOWED  — deploy CAG not fired                    → speed-coloured: green/yellow/white-on-red
    auto chuteState = [&](bool deployed, bool cut,
                          float safeSpeed, float riskySpeed,
                          const char *&label, uint16_t &fg, uint16_t &bg) {
      if (cut) {
        label = "CUT";  fg = TFT_RED;   bg = TFT_BLACK;  return;
      }
      if (deployed) {
        if (spd > riskySpeed) {
          // Fired at unsafe speed — chute likely destroyed
          label = "OPEN"; fg = TFT_WHITE;     bg = TFT_RED;   return;
        }
        if (state.airDensity < LNDG_CHUTE_SEMI_DENSITY) {
          // Fired but not enough atmosphere yet — armed, waiting
          label = "ARMED"; fg = TFT_SKY;  bg = TFT_BLACK;  return;
        }
        if (state.radarAlt > LNDG_CHUTE_FULL_ALT) {
          // Atmosphere present, above full-deploy altitude — semi-deploying
          label = "OPEN"; fg = TFT_YELLOW;    bg = TFT_BLACK;  return;
        }
        // Atmosphere present, below full-deploy altitude — fully open
        label = "OPEN"; fg = TFT_DARK_GREEN;  bg = TFT_BLACK;  return;
      }
      // Not fired — show STOWED.
      // Above atmosphere: always green (speed is irrelevant, safe to arm any time).
      // In atmosphere: speed-coloured so pilot knows whether it's safe to fire.
      label = "STOWED";
      if (!state.inAtmo) {
        fg = TFT_DARK_GREEN; bg = TFT_BLACK;
      } else if (spd > riskySpeed) { fg = TFT_WHITE;     bg = TFT_RED;   }
      else if (spd > safeSpeed)    { fg = TFT_YELLOW;    bg = TFT_BLACK; }
      else                         { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    };

    // Row 6 — Drogue | Main split (VEH block, cache[8][6] and [8][11])
    {
      uint16_t xL = AX,               wL = AHW - ROW_PAD;
      uint16_t xR = AX + AHW + ROW_PAD, wR = AHW - ROW_PAD;
      uint16_t y6 = rowYFor(6, NR), h6 = rowHFor(NR);

      const char *drogueVal; uint16_t dfg, dbg;
      chuteState(_drogueDeployed, _drogueCut, LNDG_DROGUE_SAFE_MS, LNDG_DROGUE_RISKY_MS, drogueVal, dfg, dbg);
      {
        String ds = drogueVal;
        RowCache &dc = rowCache[8][6];
        if (dc.value != ds || dc.fg != dfg || dc.bg != dbg) {
          printValue(tft, F, xL, y6, wL, h6, "Drogue:", ds, dfg, dbg, COL_BACK,
                     printState[8][6]);
          dc.value = ds; dc.fg = dfg; dc.bg = dbg;
        }
      }

      const char *mainVal; uint16_t mfg, mbg;
      chuteState(_mainDeployed, _mainCut, LNDG_MAIN_SAFE_MS, LNDG_MAIN_RISKY_MS, mainVal, mfg, mbg);
      {
        String ms = mainVal;
        RowCache &mc = rowCache[8][11];
        if (mc.value != ms || mc.fg != mfg || mc.bg != mbg) {
          printValue(tft, F, xR, y6, wR, h6, "Main:", ms, mfg, mbg, COL_BACK,
                     printState[8][11]);
          mc.value = ms; mc.fg = mfg; mc.bg = mbg;
        }
      }
    }

    // Row 7 — Gear | SAS (VEH block, cache[8][7] and [8][8])
    // Re-entry: gear UP is normal (capsule), show dark grey not alarm.
    // SAS replaces Brakes — attitude control matters far more than brakes during re-entry.
    {
      uint16_t y = rowYFor(7, NR), h = rowHFor(NR);

      const char *gearStr = state.gear_on ? "DOWN" : "UP";
      uint16_t    gearFg  = state.gear_on ? TFT_DARK_GREEN : TFT_DARK_GREY;
      uint16_t    gearBg  = TFT_BLACK;  // never alarm for gear on re-entry
      RowCache   &gc = rowCache[8][7];
      if (gc.value != gearStr || gc.fg != gearFg || gc.bg != gearBg) {
        printValue(tft, F, AX, y, AHW - ROW_PAD, h,
                   "Gear:", gearStr, gearFg, gearBg, COL_BACK,
                   printState[8][7]);
        gc.value = gearStr; gc.fg = gearFg; gc.bg = gearBg;
      }

      // SAS indicator for re-entry:
      //   RETRO  → dark green (correct — retrograde for re-entry)
      //   STAB   → sky blue (valid hold)
      //   OFF above REENTRY_SAS_AERO_STABLE_MACH → white-on-red (no attitude control, hypersonic)
      //   OFF below REENTRY_SAS_AERO_STABLE_MACH → dark grey (aero forces stabilise, acceptable)
      //   Anything else → red on black (wrong mode)
      const char *sasStr;
      uint16_t    sasFg, sasBg;
      if (state.sasMode == 255) {
        // SAS OFF — alarm only above the aero-stable Mach threshold
        if (state.machNumber > REENTRY_SAS_AERO_STABLE_MACH) {
          sasStr = "OFF";  sasFg = TFT_WHITE;     sasBg = TFT_RED;
        } else {
          sasStr = "OFF";  sasFg = TFT_DARK_GREY;  sasBg = TFT_BLACK;
        }
      } else if (state.sasMode == 2) {   // RETRO
        sasStr = "RETRO";  sasFg = TFT_DARK_GREEN;  sasBg = TFT_BLACK;
      } else if (state.sasMode == 0) {   // STAB
        sasStr = "STAB";   sasFg = TFT_SKY;          sasBg = TFT_BLACK;
      } else {
        // Any other mode is wrong for re-entry
        switch (state.sasMode) {
          case 1:  sasStr = "PROGRADE"; break;
          case 3:  sasStr = "NORMAL";   break;
          case 4:  sasStr = "ANTI-NRM"; break;
          case 5:  sasStr = "RAD-OUT";  break;
          case 6:  sasStr = "RAD-IN";   break;
          case 7:  sasStr = "TARGET";   break;
          case 8:  sasStr = "ANTI-TGT"; break;
          case 9:  sasStr = "MANEUVER"; break;
          default: sasStr = "---";      break;
        }
        sasFg = TFT_RED;  sasBg = TFT_BLACK;
      }
      RowCache &sc = rowCache[8][8];
      if (sc.value != sasStr || sc.fg != sasFg || sc.bg != sasBg) {
        printValue(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h,
                   "SAS:", sasStr, sasFg, sasBg, COL_BACK,
                   printState[8][8]);
        sc.value = sasStr; sc.fg = sasFg; sc.bg = sasBg;
      }
    }
  }
}
