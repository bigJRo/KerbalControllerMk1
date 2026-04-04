/***************************************************************************************
   Screen_LNDG.ino -- Landing screen (Powered Descent + Re-entry) — chromeScreen_LNDG, drawScreen_LNDG
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


bool _lndgReentryMode    = false;
bool _lndgReentryRow3PeA  = true;   // true = row 3 shows PeA (alt > 20km); false = V.Hrz
bool _lndgReentryRow0TPe  = false;  // true = row 0 shows T+Atm; false = T.Grnd or ---
bool _lndgReentryRow1SL   = false;  // true = row 1 shows Alt.SL (above atmo); false = Alt.Rdr

// Parachute deployment state — file-scope so SimpitHandler can reset on vessel switch
bool _drogueDeployed  = false;
bool _mainDeployed    = false;
bool _drogueCut       = false;
bool _mainCut         = false;
// armedSafe: set when a chute is fired above atmosphere at safe conditions.
// While true, skip the in-atmosphere speed check — the chute committed safely.
bool _drogueArmedSafe = false;
bool _mainArmedSafe   = false;

static void chromeScreen_LNDG(RA8875 &tft) {
  static const tFont   *F      = &Roboto_Black_40;
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t AX     = ROW_PAD + SECT_W;
  static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
  static const uint16_t AHW    = AW / 2;
  uint16_t rowH = rowHFor(NR);

  // Title bar is drawn by drawStaticScreen before calling this function.
  // Toggle indicator is added there too — do not call drawTitleBar here.

  // STATE section label (rows 0-4, both modes)
  drawVerticalText(tft, 0, TITLE_TOP, SECT_W, rowH*5, &Roboto_Black_16, "STATE", TFT_LIGHT_GREY, TFT_BLACK);

  // STATE rows 0-4: mostly shared, row 3 differs by mode.
  // Rows 0 and 1 also change label in re-entry mode based on flight phase.
  // Labels are drawn here from current state vars; draw function updates the vars
  // and triggers switchToScreen when they change (same pattern as row 3).
  const char *row0Label = (!_lndgReentryMode) ? "T.Grnd:"
                          : (_lndgReentryRow0TPe ? "T+Atm:" : "T.Grnd:");
  const char *row1Label = (!_lndgReentryMode || !_lndgReentryRow1SL) ? "Alt.Rdr:" : "Alt.SL:";
  printDispChrome(tft, F, AX, rowYFor(0,NR), AW, rowH, row0Label,  COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(1,NR), AW, rowH, row1Label,  COL_LABEL, COL_BACK, COL_NO_BDR);

  if (!_lndgReentryMode) {
    // Powered descent: V.Srf row 2, V.Vrt row 3
    printDispChrome(tft, F, AX, rowYFor(2,NR), AW, rowH, "V.Srf:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(3,NR), AW, rowH, "V.Vrt:", COL_LABEL, COL_BACK, COL_NO_BDR);
  } else {
    // Re-entry: V.Vrt row 2, PeA or V.Hrz row 3 (altitude-dependent)
    printDispChrome(tft, F, AX, rowYFor(2,NR), AW, rowH, "V.Vrt:", COL_LABEL, COL_BACK, COL_NO_BDR);
  }

  if (!_lndgReentryMode) {
    // Row 4: Fwd | Lat split — heading-relative horizontal velocity components
    {
      uint16_t y = rowYFor(4, NR), h = rowH;
      printDispChrome(tft, F, AX,                 y, AHW - ROW_PAD, h, "Fwd:", COL_LABEL, COL_BACK, COL_NO_BDR);
      printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "Lat:", COL_LABEL, COL_BACK, COL_NO_BDR);
      for (int8_t dx = -1; dx <= 1; dx++)
        tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, rowYFor(5,NR) - 1, TFT_GREY);
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

    // Row 7: Gear | SAS split
    {
      uint16_t y = rowYFor(7, NR), h = rowH;
      printDispChrome(tft, F, AX,               y, AHW - ROW_PAD, h, "Gear:", COL_LABEL, COL_BACK, COL_NO_BDR);
      printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, "SAS:",  COL_LABEL, COL_BACK, COL_NO_BDR);
      for (int8_t dx = -1; dx <= 1; dx++)
        tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, rowYFor(8,NR) - 1, TFT_GREY);
    }

  } else {
    // Row 3: label is altitude-dependent — PeA above 20km (de-orbit context),
    // V.Hrz below 20km (landing context). _lndgReentryRow3PeA tracks current mode.
    printDispChrome(tft, F, AX, rowYFor(3,NR), AW, rowH,
                    _lndgReentryRow3PeA ? "PeA:" : "V.Hrz:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(4,NR), AW, rowH, "V.Srf:", COL_LABEL, COL_BACK, COL_NO_BDR);

    // === RE-ENTRY rows 5-7 ===

    // VEH section label (rows 6-7): start 2px below d2 to avoid overwriting AT label.
    drawVerticalText(tft, 0, TITLE_TOP + rowH*6 + 2, SECT_W, rowH*2 - 4, &Roboto_Black_16, "VEH", TFT_LIGHT_GREY, TFT_BLACK);

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

  // Cache-checked draw helper using section-label-aware geometry
  // lndgVal -> drawValue() split overload with AX/AW section geometry (#6B)
  auto lndgVal = [&](uint8_t row, const char *label, const String &val,
                     uint16_t fgc, uint16_t bgc) {
    drawValue(tft, 6, row, AX, AW, label, val, fgc, bgc, F, NR);
  };

  // ── STATE block (rows 0-4) ──

  // Precompute T.Grnd and V.Hrz — used by multiple rows
  bool inOrbitOrEscape = (state.situation == sit_Orbit || state.situation == sit_Escaping);
  float tGround = (!inOrbitOrEscape && state.verticalVel < -0.05f && state.radarAlt > 0.0f)
                  ? fabsf(state.radarAlt / state.verticalVel) : -1.0f;
  float vSq  = state.surfaceVel * state.surfaceVel - state.verticalVel * state.verticalVel;
  float hSpd = (vSq > 0.0f) ? sqrtf(vSq) : 0.0f;

  // Helper: draw T.Grnd with standard gear-aware colouring
  auto drawTGrnd = [&]() {
    if (tGround >= 0.0f) {
      if (!state.gear_on) {
        fg = (tGround < LNDG_TGRND_ALARM_S) ? TFT_WHITE  :
             (tGround < LNDG_TGRND_WARN_S)  ? TFT_YELLOW : TFT_DARK_GREEN;
        bg = (tGround < LNDG_TGRND_ALARM_S) ? TFT_RED    : TFT_BLACK;
      } else {
        if      (state.verticalVel < LNDG_VVRT_ALARM_MS)            { fg = TFT_WHITE;     bg = TFT_RED;   }
        else if (state.verticalVel < LNDG_VVRT_WARN_MS ||
                 tGround < LNDG_TGRND_WARN_S)                       { fg = TFT_YELLOW;    bg = TFT_BLACK; }
        else                                                         { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
      }
      lndgVal(0, "T.Grnd:", formatTime(tGround), fg, bg);
    } else {
      lndgVal(0, "T.Grnd:", "---", TFT_DARK_GREY, TFT_BLACK);
    }
  };

  if (_lndgReentryMode) {
    // ── RE-ENTRY: 6-state phase logic ──
    // currentBody.lowSpace is the atmosphere ceiling (70km Kerbin, 50km Duna, etc.).
    // flyHigh is the science altitude boundary (~18km) — do NOT use for atmo detection.
    float atmoAlt       = (currentBody.lowSpace > 0.0f) ? currentBody.lowSpace : 70000.0f;
    bool peaBelowAtmo   = (state.periapsis < atmoAlt);

    // Atmosphere boundary detection.
    // state.inAtmo (from isVesselInAtmosphere()) is the authoritative source for
    // whether the vessel is inside the atmosphere. flyHigh is the science altitude
    // boundary (~18km for Kerbin), NOT the atmosphere ceiling — do not use it here.
    // For T+Atm calculation we still need a numeric atmosphere ceiling; use the
    // ATMO_CONDITIONS air density as proxy — but since we don't have atmo height
    // directly, derive it: if above atmosphere, inAtmo=false and altitude is our alt.
    // We use inAtmo for the state machine and altitude for T+Atm computation,
    // with the atmosphere ceiling estimated as the altitude where inAtmo last flipped.
    // Simplest correct approach: use inAtmo for state, and for T+Atm use a per-body
    // hardcoded atmosphere height constant stored in AAA_Config.
    bool aboveAtmo  = !state.inAtmo;
    bool wantTPe    = (aboveAtmo && peaBelowAtmo);
    if (wantTPe != _lndgReentryRow0TPe) {
      _lndgReentryRow0TPe = wantTPe;
      rowCache[6][0].value = "\x01";
      switchToScreen(screen_LNDG);
      return;
    }

    // Row 1 label: Alt.SL when above atmosphere; Alt.Rdr when in atmosphere
    bool wantSL = aboveAtmo;
    if (wantSL != _lndgReentryRow1SL) {
      _lndgReentryRow1SL = wantSL;
      rowCache[6][1].value = "\x01";
      switchToScreen(screen_LNDG);
      return;
    }

    // ── Row 0 ──
    if (wantTPe) {
      // States 2 & 3: above atmo, Pe inside atmosphere.
      // Show T+Atm when descending fast enough to be meaningful (V.Vrt < -5 m/s).
      // Show --- dark green when coasting (near apoapsis) — value is not yet meaningful
      // but the label stays so the pilot knows the field is active.
      // Suppress entirely (--- dark grey) only if actively ascending (V.Vrt > +5 m/s).
      float tAtmo = -1.0f;
      if (state.verticalVel < -5.0f)
        tAtmo = fabsf((state.altitude - atmoAlt) / state.verticalVel);

      if (tAtmo >= 0.0f)
        lndgVal(0, "T+Atm:", formatTime(tAtmo), TFT_DARK_GREEN, TFT_BLACK);
      else if (state.verticalVel <= 5.0f)
        lndgVal(0, "T+Atm:", "---", TFT_DARK_GREEN, TFT_BLACK);   // coasting — green
      else
        lndgVal(0, "T+Atm:", "---", TFT_DARK_GREY, TFT_BLACK);    // ascending — grey
    } else if (!aboveAtmo) {
      // States 4, 5, 6: in atmosphere — T.Grnd
      drawTGrnd();
    } else {
      // State 1: above atmo, Pe still orbital — nothing useful
      lndgVal(0, "T.Grnd:", "---", TFT_DARK_GREY, TFT_BLACK);
    }

    // ── Row 1 ──
    if (wantSL) {
      // Above atmosphere — Alt.SL so pilot can watch altitude vs atmosphere boundary
      lndgVal(1, "Alt.SL:", formatAlt(state.altitude), TFT_DARK_GREEN, TFT_BLACK);
    } else {
      // In atmosphere — Alt.Rdr with proximity colouring
      fg = (state.radarAlt < ALT_RDR_ALARM_M) ? TFT_WHITE  :
           (state.radarAlt < ALT_RDR_WARN_M)  ? TFT_YELLOW : TFT_DARK_GREEN;
      bg = (state.radarAlt < ALT_RDR_ALARM_M) ? TFT_RED    : TFT_BLACK;
      lndgVal(1, "Alt.Rdr:", formatAlt(state.radarAlt), fg, bg);
    }

  } else {
    // ── POWERED DESCENT: rows 0 and 1 ──
    drawTGrnd();

    fg = (state.radarAlt < ALT_RDR_ALARM_M) ? TFT_WHITE  :
         (state.radarAlt < ALT_RDR_WARN_M)  ? TFT_YELLOW : TFT_DARK_GREEN;
    bg = (state.radarAlt < ALT_RDR_ALARM_M) ? TFT_RED    : TFT_BLACK;
    lndgVal(1, "Alt.Rdr:", formatAlt(state.radarAlt), fg, bg);
  }

  // V.Vrt colouring (used by both modes, different row)
  auto drawVVrt = [&](uint8_t row) {
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
    lndgVal(row, "V.Vrt:", fmtMs(state.verticalVel), fg, bg);
  };

  if (!_lndgReentryMode) {
    // Powered descent: V.Srf row 2, V.Vrt row 3
    fg = (state.surfaceVel < 0) ? TFT_RED : TFT_DARK_GREEN;
    lndgVal(2, "V.Srf:", fmtMs(state.surfaceVel), fg, TFT_BLACK);
    drawVVrt(3);
  } else {
    // Re-entry: V.Vrt row 2 (row 3 handled below by PeA/V.Hrz branch)
    drawVVrt(2);
  }

  // Row 4 — mode-dependent horizontal velocity display
  if (!_lndgReentryMode) {
    // Decompose horizontal surface velocity into craft heading-relative axes.
    // Craft control direction is UP during vertical descent:
    //   Fwd (W/S): component of horizontal drift along the craft's heading direction
    //   Lat (A/D): component of horizontal drift perpendicular to heading (right = positive)
    //
    // Use hSpd (already computed from surfaceVel/verticalVel via Pythagoras) as the
    // horizontal speed magnitude, and srfVelHeading as its compass direction.
    // This avoids srfVelPitch which is unstable near-vertical and introduces noise.
    float headRad    = (state.heading + state.roll) * 0.017453f;
    float svelHdgRad = state.srfVelHeading * 0.017453f;
    float dHeadRad   = svelHdgRad - headRad;  // angle of drift relative to roll-adjusted heading

    // vFwd: positive = drifting toward nose; correct with W (pitch nose toward retrograde)
    // vLat: positive = drifting right of nose; correct with D (yaw nose toward retrograde)
    // sin gives the along-heading component (W/S axis) when craft control direction is UP
    // cos gives the perpendicular component (A/D axis)
    float vFwd = -hSpd * cosf(dHeadRad);
    float vLat = -hSpd * sinf(dHeadRad);

    // Snap near-zero to +0.0 to prevent +0.0/-0.0 flicker
    if (fabsf(vFwd) < 0.05f) vFwd = 0.0f;
    if (fabsf(vLat) < 0.05f) vLat = 0.0f;

    // Context-dependent horizontal thresholds — tighten as ground approaches
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
    uint16_t y4 = rowYFor(4, NR), h4 = rowHFor(NR);
    char buf4[12];

    snprintf(buf4, sizeof(buf4), "%+.1f m/s", vFwd);
    uint16_t ffg, fbg; horzColor(vFwd, ffg, fbg);
    {
      String s = buf4;
      RowCache &rc = rowCache[6][12];
      if (rc.value != s || rc.fg != ffg || rc.bg != fbg) {
        printValue(tft, F, xL, y4, wL, h4, "Fwd:", s, ffg, fbg, COL_BACK, printState[6][12]);
        rc.value = s; rc.fg = ffg; rc.bg = fbg;
      }
    }

    snprintf(buf4, sizeof(buf4), "%+.1f m/s", vLat);
    uint16_t lfg, lbg; horzColor(vLat, lfg, lbg);
    {
      String s = buf4;
      RowCache &rc = rowCache[6][13];
      if (rc.value != s || rc.fg != lfg || rc.bg != lbg) {
        printValue(tft, F, xR, y4, wR, h4, "Lat:", s, lfg, lbg, COL_BACK, printState[6][13]);
        rc.value = s; rc.fg = lfg; rc.bg = lbg;
      }
    }

  } else {
    // Re-entry row 3: PeA above 20km radar alt (or above atmo); V.Hrz below 20km in atmo
    float atmoAlt3  = (currentBody.lowSpace > 0.0f) ? currentBody.lowSpace : 70000.0f;
    bool wantPeA = (state.radarAlt > 20000.0f || !state.inAtmo);
    if (wantPeA != _lndgReentryRow3PeA) {
      _lndgReentryRow3PeA = wantPeA;
      rowCache[6][3].value = "\x01";
      switchToScreen(screen_LNDG);
      return;
    }

    if (wantPeA) {
      // 3-state PeA colouring matching the 6-state flight phase model:
      //   PeA < 0           → white-on-green  (states 3 & 6: landing inevitable)
      //   0 < PeA < atmoAlt → yellow          (states 2 & 5: committed to atmo, uncertain landing)
      //   PeA > atmoAlt     → dark green      (states 1 & 4: safely orbital / going back to space)
      if      (state.periapsis < 0.0f)           { fg = TFT_WHITE;     bg = TFT_DARK_GREEN; }
      else if (state.periapsis < atmoAlt3)       { fg = TFT_YELLOW;    bg = TFT_BLACK;      }
      else                                       { fg = TFT_DARK_GREEN; bg = TFT_BLACK;     }
      lndgVal(3, "PeA:", formatAlt(state.periapsis), fg, bg);
    } else {
      // V.Hrz — total horizontal speed
      if      (hSpd > LNDG_REENTRY_VHRZ_ALARM_MS) { fg = TFT_WHITE;     bg = TFT_RED;   }
      else if (hSpd > LNDG_REENTRY_VHRZ_WARN_MS)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
      else                                         { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
      lndgVal(3, "V.Hrz:", fmtMs(hSpd), fg, bg);
    }

    // Row 4 — V.Srf: total surface speed (re-entry)
    fg = (state.surfaceVel < 0) ? TFT_RED : TFT_DARK_GREEN;
    lndgVal(4, "V.Srf:", fmtMs(state.surfaceVel), fg, TFT_BLACK);
  }

  // ── Mode-specific rows 5-7 ──
  if (!_lndgReentryMode) {
    // ── POWERED DESCENT ──

    // Row 5 — ΔV.Stg (single-row STG block, indicator square in chrome)
    thresholdColor(state.stageDeltaV,
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
        RowCache &tc = rowCache[6][6];
        if (tc.value != ts || tc.fg != fg || tc.bg != bg) {
          printValue(tft, F, AX, y6, AHW - ROW_PAD, h6,
                     "Throttle:", ts, fg, bg, COL_BACK, printState[6][6]);
          tc.value = ts; tc.fg = fg; tc.bg = bg;
        }
      }

      const char *rcsStr = state.rcs_on ? "ON" : "OFF";
      uint16_t rcsFg = state.rcs_on ? TFT_DARK_GREEN : TFT_DARK_GREY;
      RowCache &rc = rowCache[6][9];
      if (rc.value != rcsStr || rc.fg != rcsFg || rc.bg != COL_BACK) {
        printValue(tft, F, AX + AHW + ROW_PAD, y6, AHW - ROW_PAD, h6,
                   "RCS:", rcsStr, rcsFg, COL_BACK, COL_BACK, printState[6][9]);
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
      RowCache   &gc = rowCache[6][7];
      if (gc.value != gearStr || gc.fg != gearFg || gc.bg != gearBg) {
        printValue(tft, F, AX, y, AHW - ROW_PAD, h,
                   "Gear:", gearStr, gearFg, gearBg, COL_BACK,
                   printState[6][7]);
        gc.value = gearStr; gc.fg = gearFg; gc.bg = gearBg;
      }

      // SAS: RETRO(2) or STAB(0) = green; OFF(255) = dark grey; anything else = red
      const char *sasStr;
      uint16_t sasFg, sasBg = TFT_BLACK;
      if (state.sasMode == 255) {
        sasStr = "OFF";   sasFg = TFT_DARK_GREY;
      } else if (state.sasMode == 0) {
        sasStr = "STAB";  sasFg = TFT_DARK_GREEN;
      } else if (state.sasMode == 2) {
        sasStr = "RETRO"; sasFg = TFT_DARK_GREEN;
      } else {
        switch (state.sasMode) {
          case 1: sasStr = "PROGRADE"; break; case 3: sasStr = "NORMAL";   break;
          case 4: sasStr = "ANTI-NRM"; break; case 5: sasStr = "RAD-OUT";  break;
          case 6: sasStr = "RAD-IN";   break; case 7: sasStr = "TARGET";   break;
          case 8: sasStr = "ANTI-TGT"; break; case 9: sasStr = "MANEUVER"; break;
          default: sasStr = "OTHER";   break;
        }
        sasFg = TFT_RED; sasBg = TFT_BLACK;
      }
      RowCache &sc = rowCache[6][8];
      if (sc.value != sasStr || sc.fg != sasFg || sc.bg != sasBg) {
        printValue(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h,
                   "SAS:", sasStr, sasFg, sasBg, COL_BACK, printState[6][8]);
        sc.value = sasStr; sc.fg = sasFg; sc.bg = sasBg;
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
        RowCache &mc = rowCache[6][9];
        if (mc.value != ms || mc.fg != mfg || mc.bg != TFT_BLACK) {
          printValue(tft, F, xL, rowYFor(5,NR), wL, rowHFor(NR),
                     "Mach:", ms, mfg, TFT_BLACK, COL_BACK, printState[6][9]);
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
        RowCache &gc = rowCache[6][10];
        if (gc.value != gs || gc.fg != fg || gc.bg != bg) {
          printValue(tft, F, xR, rowYFor(5,NR), wR, rowHFor(NR),
                     "G:", gs, fg, bg, COL_BACK, printState[6][10]);
          gc.value = gs; gc.fg = fg; gc.bg = bg;
        }
      }
    }

    // Parachute state tracking
    if (state.drogueDeploy && !_drogueDeployed) {
      _drogueDeployed = true;
      // Record if armed safely (above atmosphere or at safe speed) — if so, skip
      // the dangerous-speed check when atmosphere is later entered.
      _drogueArmedSafe = (!state.inAtmo || spd <= LNDG_DROGUE_RISKY_MS);
    }
    if (state.drogueCut && !_drogueCut) {
      _drogueCut = true; _drogueDeployed = false; _drogueArmedSafe = false;
    }
    if (state.mainDeploy && !_mainDeployed) {
      _mainDeployed = true;
      _mainArmedSafe = (!state.inAtmo || spd <= LNDG_MAIN_RISKY_MS);
    }
    if (state.mainCut && !_mainCut) {
      _mainCut = true; _mainDeployed = false; _mainArmedSafe = false;
    }

    // Chute state machine — states per chute, priority order:
    //   CUT     — cut CAG fired                                  → red on black
    //   OPEN*   — deploy CAG fired in atmo at unsafe speed       → white on red
    //             (skipped if armedSafe — chute was committed safely before atmo)
    //   ARMED   — deploy CAG fired, airDensity too low           → cyan on black
    //   OPEN    — density ok, alt > fullAlt                      → yellow on black
    //   OPEN    — density ok, alt ≤ fullAlt                      → green on black
    //   STOWED  — deploy CAG not fired                           → speed-coloured
    //   STOWED  — deploy CAG not fired                    → speed-coloured: green/yellow/white-on-red
    auto chuteState = [&](bool deployed, bool cut, bool armedSafe,
                          float safeSpeed, float riskySpeed, float fullAlt,
                          const char *&label, uint16_t &fg, uint16_t &bg) {
      if (cut) {
        label = "CUT";  fg = TFT_RED;   bg = TFT_BLACK;  return;
      }
      if (deployed) {
        // Unsafe speed check: only if in atmosphere AND chute was not already
        // armed safely before atmosphere entry. If armedSafe, skip this branch —
        // the chute was committed at a safe moment and should proceed normally.
        if (!armedSafe && state.inAtmo && spd > riskySpeed) {
          label = "OPEN"; fg = TFT_WHITE;     bg = TFT_RED;   return;
        }
        if (state.airDensity < LNDG_CHUTE_SEMI_DENSITY) {
          // Not enough atmosphere yet — armed, waiting
          label = "ARMED"; fg = TFT_SKY;  bg = TFT_BLACK;  return;
        }
        if (state.radarAlt > fullAlt) {
          // Atmosphere present, above full-deploy altitude — semi-deploying
          label = "OPEN"; fg = TFT_YELLOW;    bg = TFT_BLACK;  return;
        }
        // Atmosphere present, below full-deploy altitude — fully open
        label = "OPEN"; fg = TFT_DARK_GREEN;  bg = TFT_BLACK;  return;
      }
      // Not fired — show STOWED.
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
      chuteState(_drogueDeployed, _drogueCut, _drogueArmedSafe,
                 LNDG_DROGUE_SAFE_MS, LNDG_DROGUE_RISKY_MS, LNDG_DROGUE_FULL_ALT,
                 drogueVal, dfg, dbg);
      {
        String ds = drogueVal;
        RowCache &dc = rowCache[6][6];
        if (dc.value != ds || dc.fg != dfg || dc.bg != dbg) {
          printValue(tft, F, xL, y6, wL, h6, "Drogue:", ds, dfg, dbg, COL_BACK,
                     printState[6][6]);
          dc.value = ds; dc.fg = dfg; dc.bg = dbg;
        }
      }

      const char *mainVal; uint16_t mfg, mbg;
      chuteState(_mainDeployed, _mainCut, _mainArmedSafe,
                 LNDG_MAIN_SAFE_MS, LNDG_MAIN_RISKY_MS, LNDG_MAIN_FULL_ALT,
                 mainVal, mfg, mbg);
      {
        String ms = mainVal;
        RowCache &mc = rowCache[6][11];
        if (mc.value != ms || mc.fg != mfg || mc.bg != mbg) {
          printValue(tft, F, xR, y6, wR, h6, "Main:", ms, mfg, mbg, COL_BACK,
                     printState[6][11]);
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
      RowCache   &gc = rowCache[6][7];
      if (gc.value != gearStr || gc.fg != gearFg || gc.bg != gearBg) {
        printValue(tft, F, AX, y, AHW - ROW_PAD, h,
                   "Gear:", gearStr, gearFg, gearBg, COL_BACK,
                   printState[6][7]);
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
      RowCache &sc = rowCache[6][8];
      if (sc.value != sasStr || sc.fg != sasFg || sc.bg != sasBg) {
        printValue(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h,
                   "SAS:", sasStr, sasFg, sasBg, COL_BACK,
                   printState[6][8]);
        sc.value = sasStr; sc.fg = sasFg; sc.bg = sasBg;
      }
    }
  }
}
