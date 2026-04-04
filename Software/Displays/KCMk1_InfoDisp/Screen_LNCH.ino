/***************************************************************************************
   Screen_LNCH.ino -- Launch / Circularization screen — chromeScreen_LNCH, drawScreen_LNCH
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


bool _lnchOrbitalMode      = false;
bool _lnchManualOverride   = false;  // true = pilot has overridden auto phase switch
bool _lnchPrelaunchMode    = false;  // true = sit_PreLaunch board is showing
bool _lnchPrelaunchDismissed = false; // true = pilot tapped to dismiss; don't re-enter

static void chromeScreen_LNCH(RA8875 &tft) {
  static const tFont   *F      = &Roboto_Black_40;
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t AX     = ROW_PAD + SECT_W;
  static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
  uint16_t rowH = rowHFor(NR);  // hoisted — used in both phases

  // _lnchOrbitalMode is set by drawScreen_LNCH via hysteresis before chrome is called.
  if (_lnchPrelaunchMode) {
    // ── PRE-LAUNCH board ──
    // No section labels — pure status board. 8 rows, all split except rows 0 and 1.
    drawTitleBar(tft, "PRE-LAUNCH");  // no toggle indicator

    printDispChrome(tft, F, AX, rowYFor(0,NR), AW, rowH, "Vessel:",  COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(1,NR), AW, rowH, "Type:",    COL_LABEL, COL_BACK, COL_NO_BDR);

    // Split rows 2-7: vertical dividers and left/right labels
    uint16_t AHW = AW / 2;
    auto plSplit = [&](uint8_t row, const char *lLabel, const char *rLabel) {
      uint16_t y = rowYFor(row, NR), h = rowH;
      printDispChrome(tft, F, AX,                 y, AHW - ROW_PAD, h, lLabel, COL_LABEL, COL_BACK, COL_NO_BDR);
      printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, rLabel, COL_LABEL, COL_BACK, COL_NO_BDR);
      uint16_t nextY = (row < NR - 1) ? rowYFor(row + 1, NR) : SCREEN_H;
      for (int8_t dx = -1; dx <= 1; dx++)
        tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, nextY - 1, TFT_GREY);
    };
    plSplit(2, "SAS:",      "RCS:");
    plSplit(3, "Throttle:", "EC%:");
    plSplit(4, "Crew:",     "Comm:");
    printDispChrome(tft, F, AX, rowYFor(5,NR), AW, rowH, "\xCE\x94V.Tot:", COL_LABEL, COL_BACK, COL_NO_BDR);
    plSplit(6, "Drogue:",   "Main:");
    plSplit(7, "D.Cut:",    "M.Cut:");

    // Horizontal dividers between every row — makes board easier to read as a checklist
    for (uint8_t row = 1; row < NR; row++) {
      uint16_t dy = TITLE_TOP + rowHFor(NR) * row;
      tft.drawLine(0, dy,   CONTENT_W, dy,   TFT_GREY);
      tft.drawLine(0, dy+1, CONTENT_W, dy+1, TFT_GREY);
    }
    return;
  }

  if (!_lnchOrbitalMode) {
    // ── Ascent phase ──
    // Section labels: STATE(rows0-2), TRAJ(rows3-4), PROP(rows5-7)
    drawVerticalText(tft, 0, TITLE_TOP,              SECT_W, rowH*3, &Roboto_Black_16, "STATE", TFT_LIGHT_GREY, TFT_BLACK);
    drawVerticalText(tft, 0, TITLE_TOP + rowH*3,     SECT_W, rowH*2, &Roboto_Black_16, "TRAJ",  TFT_LIGHT_GREY, TFT_BLACK);
    drawVerticalText(tft, 0, TITLE_TOP + rowH*5,     SECT_W, rowH*3, &Roboto_Black_16, "PROP",  TFT_LIGHT_GREY, TFT_BLACK);

    // Row labels (offset right of section strip)
    printDispChrome(tft, F, AX, rowYFor(0,NR), AW, rowHFor(NR), "Alt.SL:",   COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(1,NR), AW, rowHFor(NR), "V.Srf:",    COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(2,NR), AW, rowHFor(NR), "V.Vrt:",    COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(3,NR), AW, rowHFor(NR), "ApA:",      COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(4,NR), AW, rowHFor(NR), "T+Ap:",     COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(5,NR), AW, rowHFor(NR), "Throttle:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(6,NR), AW, rowHFor(NR), "T.Burn:",   COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(7,NR), AW, rowHFor(NR), "\xCE\x94V.Stg:", COL_LABEL, COL_BACK, COL_NO_BDR);

    // Dividers — drawn LAST, offset +1 to clear printValue fillRect boundary
    uint16_t d1 = TITLE_TOP + rowH*3 + 1;
    uint16_t d2 = TITLE_TOP + rowH*5 + 1;
    tft.drawLine(0, d1,   CONTENT_W, d1,   TFT_GREY);
    tft.drawLine(0, d1+1, CONTENT_W, d1+1, TFT_GREY);
    tft.drawLine(0, d2,   CONTENT_W, d2,   TFT_GREY);
    tft.drawLine(0, d2+1, CONTENT_W, d2+1, TFT_GREY);

  } else {
    // ── Orbital (circularization) phase ──
    // Section labels: STATE(rows0-1), TRAJ(rows2-4), PROP(rows5-7)
    drawVerticalText(tft, 0, TITLE_TOP,              SECT_W, rowH*2, &Roboto_Black_16, "STATE", TFT_LIGHT_GREY, TFT_BLACK);
    drawVerticalText(tft, 0, TITLE_TOP + rowH*2,     SECT_W, rowH*3, &Roboto_Black_16, "TRAJ",  TFT_LIGHT_GREY, TFT_BLACK);
    drawVerticalText(tft, 0, TITLE_TOP + rowH*5,     SECT_W, rowH*3, &Roboto_Black_16, "PROP",  TFT_LIGHT_GREY, TFT_BLACK);

    // Row labels
    printDispChrome(tft, F, AX, rowYFor(0,NR), AW, rowHFor(NR), "Alt.SL:",   COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(1,NR), AW, rowHFor(NR), "V.Orb:",    COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(2,NR), AW, rowHFor(NR), "ApA:",      COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(3,NR), AW, rowHFor(NR), "PeA:",      COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(4,NR), AW, rowHFor(NR), "T+Ap:",     COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(5,NR), AW, rowHFor(NR), "Throttle:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(6,NR), AW, rowHFor(NR), "T.Burn:",   COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(7,NR), AW, rowHFor(NR), "\xCE\x94V.Stg:", COL_LABEL, COL_BACK, COL_NO_BDR);

    // Dividers — drawn LAST, offset +1 to clear printValue fillRect boundary
    uint16_t d1 = TITLE_TOP + rowH*2 + 1;
    uint16_t d2 = TITLE_TOP + rowH*5 + 1;
    tft.drawLine(0, d1,   CONTENT_W, d1,   TFT_GREY);
    tft.drawLine(0, d1+1, CONTENT_W, d1+1, TFT_GREY);
    tft.drawLine(0, d2,   CONTENT_W, d2,   TFT_GREY);
    tft.drawLine(0, d2+1, CONTENT_W, d2+1, TFT_GREY);
  }
}


static void drawScreen_LNCH(RA8875 &tft) {
  static const tFont   *F      = &Roboto_Black_40;
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t AX     = ROW_PAD + SECT_W;
  static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
  static const uint16_t AHW    = AW / 2;
  uint16_t fg, bg;

  // ── PRE-LAUNCH board ──
  if (_lnchPrelaunchMode) {
    auto plVal = [&](uint8_t row, uint8_t slot, const char *label, const String &val,
                     uint16_t fgc, uint16_t bgc) {
      RowCache &rc = rowCache[0][slot];
      if (rc.value == val && rc.fg == fgc && rc.bg == bgc) return;
      printValue(tft, F, AX, rowYFor(row, NR), AW, rowHFor(NR),
                 label, val, fgc, bgc, COL_BACK, printState[0][slot]);
      rc.value = val; rc.fg = fgc; rc.bg = bgc;
    };

    auto plValL = [&](uint8_t row, uint8_t slot, const char *label, const String &val,
                      uint16_t fgc, uint16_t bgc) {
      uint16_t xL = AX, wL = AHW - ROW_PAD;
      RowCache &rc = rowCache[0][slot];
      if (rc.value == val && rc.fg == fgc && rc.bg == bgc) return;
      printValue(tft, F, xL, rowYFor(row, NR), wL, rowHFor(NR),
                 label, val, fgc, bgc, COL_BACK, printState[0][slot]);
      rc.value = val; rc.fg = fgc; rc.bg = bgc;
    };

    auto plValR = [&](uint8_t row, uint8_t slot, const char *label, const String &val,
                      uint16_t fgc, uint16_t bgc) {
      uint16_t xR = AX + AHW + ROW_PAD, wR = AHW - ROW_PAD;
      RowCache &rc = rowCache[0][slot];
      if (rc.value == val && rc.fg == fgc && rc.bg == bgc) return;
      printValue(tft, F, xR, rowYFor(row, NR), wR, rowHFor(NR),
                 label, val, fgc, bgc, COL_BACK, printState[0][slot]);
      rc.value = val; rc.fg = fgc; rc.bg = bgc;
    };

    // Row 0 — Vessel name (full width, always dark green)
    plVal(0, 14, "Vessel:", state.vesselName, TFT_DARK_GREEN, TFT_BLACK);

    // Row 1 — Vessel type (full width, always dark green — informational confirmation)
    {
      const char *typeName;
      switch (state.vesselType) {
        case type_Ship:     typeName = "Ship";      break;
        case type_Probe:    typeName = "Probe";     break;
        case type_Relay:    typeName = "Relay";     break;
        case type_Rover:    typeName = "Rover";     break;
        case type_Lander:   typeName = "Lander";    break;
        case type_Plane:    typeName = "Plane";     break;
        case type_Station:  typeName = "Station";   break;
        case type_Base:     typeName = "Base";      break;
        case type_EVA:      typeName = "EVA";       break;
        default:            typeName = "Unknown";   break;
      }
      plVal(1, 15, "Type:", typeName, TFT_DARK_GREEN, TFT_BLACK);
    }

    // Row 2 — SAS (left) | RCS (right)
    {
      // SAS: green=STAB, yellow=other mode on, red=OFF
      const char *sasStr;
      if (state.sasMode == 255)     { sasStr = "OFF";  fg = TFT_WHITE;     bg = TFT_RED;   }
      else if (state.sasMode == 0)  { sasStr = "STAB"; fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
      else {
        // Any other SAS mode — show mode name, yellow
        switch (state.sasMode) {
          case 1: sasStr = "PROGRADE"; break; case 2: sasStr = "RETRO";    break;
          case 3: sasStr = "NORMAL";   break; case 4: sasStr = "ANTI-NRM"; break;
          case 5: sasStr = "RAD-OUT";  break; case 6: sasStr = "RAD-IN";   break;
          case 7: sasStr = "TARGET";   break; case 8: sasStr = "ANTI-TGT"; break;
          case 9: sasStr = "MANEUVER"; break; default: sasStr = "OTHER";   break;
        }
        fg = TFT_YELLOW; bg = TFT_BLACK;
      }
      plValL(2, 2, "SAS:", sasStr, fg, bg);

      // RCS: green=OFF, red=ON (no RCS during launch)
      const char *rcsStr = state.rcs_on ? "ON" : "OFF";
      uint16_t rcsFg     = state.rcs_on ? TFT_WHITE     : TFT_DARK_GREEN;
      uint16_t rcsBg     = state.rcs_on ? TFT_RED        : TFT_BLACK;
      plValR(2, 3, "RCS:", rcsStr, rcsFg, rcsBg);
    }

    // Row 3 — Throttle (left) | EC% (right)
    {
      // Throttle: green=0%, red=any non-zero
      uint8_t thrPct = (uint8_t)constrain(state.throttle * 100.0f, 0.0f, 100.0f);
      String thrStr = formatPerc(thrPct);
      if (thrPct == 0) { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
      else             { fg = TFT_WHITE;       bg = TFT_RED;   }
      plValL(3, 4, "Throttle:", thrStr, fg, bg);

      // EC%: green≥90%, yellow=75-89%, red=<75%
      uint8_t ecPct = (uint8_t)constrain(state.electricChargePercent, 0.0f, 100.0f);
      String ecStr = formatPerc(ecPct);
      if      (ecPct >= 90) { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
      else if (ecPct >= 75) { fg = TFT_YELLOW;     bg = TFT_BLACK; }
      else                  { fg = TFT_WHITE;       bg = TFT_RED;   }
      plValR(3, 5, "EC%:", ecStr, fg, bg);
    }

    // Row 4 — Crew (left) | CommNet (right)
    {
      // Crew: always dark green — pilot reads and confirms count
      char crewBuf[10];
      snprintf(crewBuf, sizeof(crewBuf), "%d / %d", state.crewCount, state.crewCapacity);
      plValL(4, 6, "Crew:", crewBuf, TFT_DARK_GREEN, TFT_BLACK);

      // CommNet: green=>50%, yellow=10-50%, red=<10%
      uint8_t sig = (uint8_t)constrain(state.commNetSignal, 0, 100);
      String sigStr = formatPerc(sig);
      if      (sig >= 50) { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
      else if (sig >= 10) { fg = TFT_YELLOW;     bg = TFT_BLACK; }
      else                { fg = TFT_WHITE;       bg = TFT_RED;   }
      plValR(4, 7, "CommNet:", sigStr, fg, bg);
    }

    // Row 5 — ΔV.Tot (full width) — total mission delta-V at a glance
    {
      thresholdColor((uint16_t)constrain(state.totalDeltaV, 0, 65535),
                     DV_STG_ALARM_MS, TFT_WHITE, TFT_RED,
                     DV_TOT_WARN_MS,  TFT_YELLOW, TFT_BLACK,
                          TFT_DARK_GREEN, TFT_BLACK, fg, bg);
      plVal(5, 16, "\xCE\x94V.Tot:", fmtMs(state.totalDeltaV), fg, bg);
    }

    // Row 6 — Drogue deploy (CAG 1) | Main deploy (CAG 3)
    {
      // Green = STOWED (CAG off), Red = ARMED! (CAG on)
      const char *dStr = state.drogueDeploy ? "ARMED!" : "STOWED";
      uint16_t dFg = state.drogueDeploy ? TFT_WHITE : TFT_DARK_GREEN;
      uint16_t dBg = state.drogueDeploy ? TFT_RED   : TFT_BLACK;
      plValL(6, 10, "Drogue:", dStr, dFg, dBg);

      const char *mStr = state.mainDeploy ? "ARMED!" : "STOWED";
      uint16_t mFg = state.mainDeploy ? TFT_WHITE : TFT_DARK_GREEN;
      uint16_t mBg = state.mainDeploy ? TFT_RED   : TFT_BLACK;
      plValR(6, 11, "Main:", mStr, mFg, mBg);
    }

    // Row 7 — Drogue cut (CAG 2) | Main cut (CAG 4)
    {
      // Green = SAFE (CAG off), Red = FIRED! (CAG on)
      const char *dcStr = state.drogueCut ? "FIRED!" : "SAFE";
      uint16_t dcFg = state.drogueCut ? TFT_WHITE : TFT_DARK_GREEN;
      uint16_t dcBg = state.drogueCut ? TFT_RED   : TFT_BLACK;
      plValL(7, 12, "D.Cut:", dcStr, dcFg, dcBg);

      const char *mcStr = state.mainCut ? "FIRED!" : "SAFE";
      uint16_t mcFg = state.mainCut ? TFT_WHITE : TFT_DARK_GREEN;
      uint16_t mcBg = state.mainCut ? TFT_RED   : TFT_BLACK;
      plValR(7, 13, "M.Cut:", mcStr, mcFg, mcBg);
    }

    // Redraw horizontal dividers every frame — printValue fillRect overwrites them.
    // Same pattern as ATT and ROVR screens.
    {
      uint16_t rH = rowHFor(NR);
      for (uint8_t row = 1; row < NR; row++) {
        uint16_t dy = TITLE_TOP + rH * row;
        tft.drawLine(0, dy,   CONTENT_W, dy,   TFT_GREY);
        tft.drawLine(0, dy+1, CONTENT_W, dy+1, TFT_GREY);
      }
    }

    return;  // pre-launch draw complete
  }

  // Phase detection with hysteresis — ascending uses higher threshold to prevent
  // rapid switching near the boundary.
  float bodyRad   = (currentBody.radius > 0.0f) ? currentBody.radius : 600000.0f;
  bool  ascending = (state.verticalVel >= 0.0f);
  float switchAlt = ascending ? (bodyRad * 0.06f) : (bodyRad * 0.055f);
  bool  orbMode   = (state.altitude > switchAlt);

  // Phase switch — auto only if not manually overridden
  if (!_lnchManualOverride && orbMode != _lnchOrbitalMode) {
    _lnchOrbitalMode = orbMode;
    for (uint8_t r = 0; r < NR; r++) rowCache[0][r].value = "\x01";
    switchToScreen(screen_LNCH);
    return;
  }

  // Shared precompute
  float warnAlt  = max(currentBody.minSafe, currentBody.flyHigh);
  float altYellow = bodyRad * 0.0015f;  // body-relative: ~900m Kerbin, ~300m Mun

  // Manual override indicator — red dot on right of title bar.
  // Drawn every loop: red when overridden, black (erase) when auto.
  // Position matches drawStaticScreen: x=706, y=29, r=6.
  {
    uint16_t indCol = _lnchManualOverride ? TFT_RED : TFT_BLACK;
    tft.fillCircle(706, 29, 6, indCol);
  }

  // Cache-checked draw helper using section-label-aware geometry
  auto lnchVal = [&](uint8_t row, const char *label, const String &val,
                     uint16_t fgc, uint16_t bgc) {
    RowCache &rc = rowCache[0][row];
    if (rc.value == val && rc.fg == fgc && rc.bg == bgc) return;
    printValue(tft, F, AX, rowYFor(row, NR), AW, rowHFor(NR),
               label, val, fgc, bgc, COL_BACK, printState[0][row]);
    rc.value = val; rc.fg = fgc; rc.bg = bgc;
  };

  // --- Row 0: Alt.SL — always present ---
  fg = (state.altitude < 0)         ? TFT_RED    :
       (state.altitude < altYellow)  ? TFT_YELLOW  : TFT_DARK_GREEN;
  lnchVal(0, "Alt.SL:", formatAlt(state.altitude), fg, TFT_BLACK);

  if (!orbMode) {
    // =========================================================
    // ASCENT PHASE — below switchAlt
    // Focus: building trajectory, monitoring propellant
    // =========================================================

    // Row 1 — Surface velocity (red if negative)
    fg = (state.surfaceVel < 0) ? TFT_RED : TFT_DARK_GREEN;
    lnchVal(1, "V.Srf:", fmtMs(state.surfaceVel), fg, TFT_BLACK);

    // Row 2 — Vertical velocity: suppress on ground to avoid spurious alarms.
    // Use same 0.05 m/s dead-band as T.Grnd computation.
    if (fabsf(state.verticalVel) < 0.05f ||
        (state.situation & sit_PreLaunch) || (state.situation & sit_Landed)) {
      lnchVal(2, "V.Vrt:", "---", TFT_DARK_GREY, TFT_BLACK);
    } else {
      if (state.verticalVel < 0) { fg = TFT_WHITE;     bg = TFT_RED;   }
      else                       { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
      lnchVal(2, "V.Vrt:", fmtMs(state.verticalVel), fg, bg);
    }

    // Row 3 — Apoapsis: suppress on ground (same condition as T+Ap below)
    {
      bool onGround = (state.situation & sit_PreLaunch) || (state.situation & sit_Landed);
      if (onGround || state.apoapsis <= 0.0f) {
        lnchVal(3, "ApA:", "---", TFT_DARK_GREY, TFT_BLACK);
      } else {
        bool apaWarn = (warnAlt > 0 && state.apoapsis > 0 && state.apoapsis < warnAlt);
        if      (state.apoapsis < 0) { fg = TFT_RED;        }
        else if (apaWarn)            { fg = TFT_YELLOW;      }
        else                         { fg = TFT_DARK_GREEN;  }
        lnchVal(3, "ApA:", formatAlt(state.apoapsis), fg, TFT_BLACK);
      }
    }

    // Row 4 — Time to apoapsis: suppress on ground and when no valid apoapsis yet
    if (state.apoapsis <= 0.0f ||
        (state.situation & sit_PreLaunch) || (state.situation & sit_Landed)) {
      lnchVal(4, "T+Ap:", "---", TFT_DARK_GREY, TFT_BLACK);
    } else {
      if      (state.timeToAp < 0)              { fg = TFT_RED;       }
      else if (state.timeToAp < LNCH_TOAPO_WARN_S) { fg = TFT_YELLOW;    }
      else                                      { fg = TFT_DARK_GREEN; }
      lnchVal(4, "T+Ap:", fmtTime(state.timeToAp), fg, TFT_BLACK);
    }

    // Row 5 — Throttle: white-on-red at 0% (engine out = abort condition during ascent)
    {
      uint8_t thrPct = (uint8_t)constrain(state.throttle * 100.0f, 0.0f, 100.0f);
      if (thrPct == 0) { fg = TFT_WHITE; bg = TFT_RED;   }
      else             { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
      lnchVal(5, "Throttle:", formatPerc(thrPct), fg, bg);
    }

  } else {
    // =========================================================
    // ORBITAL PHASE — above switchAlt (coast + circularisation)
    // Focus: apoapsis/periapsis targets, burn resources
    // =========================================================

    // Row 1 — Orbital velocity (red if negative)
    fg = (state.orbitalVel < 0) ? TFT_RED : TFT_DARK_GREEN;
    lnchVal(1, "V.Orb:", fmtMs(state.orbitalVel), fg, TFT_BLACK);

    // Row 2 — Apoapsis: red if negative, yellow if below safe orbit, green if safe
    {
      bool apaWarn = (warnAlt > 0 && state.apoapsis > 0 && state.apoapsis < warnAlt);
      if      (state.apoapsis < 0) { fg = TFT_RED;        }
      else if (apaWarn)            { fg = TFT_YELLOW;      }
      else                         { fg = TFT_DARK_GREEN;  }
      lnchVal(2, "ApA:", formatAlt(state.apoapsis), fg, TFT_BLACK);
    }

    // Row 3 — Periapsis: same convention as ApA
    {
      bool peWarn = (warnAlt > 0 && state.periapsis > 0 && state.periapsis < warnAlt);
      if      (state.periapsis < 0) { fg = TFT_RED;        }
      else if (peWarn)              { fg = TFT_YELLOW;      }
      else                          { fg = TFT_DARK_GREEN;  }
      lnchVal(3, "PeA:", formatAlt(state.periapsis), fg, TFT_BLACK);
    }

    // Row 4 — Time to apoapsis: red if negative (past burn point)
    fg = (state.timeToAp < 0) ? TFT_RED : TFT_DARK_GREEN;
    lnchVal(4, "T+Ap:", fmtTime(state.timeToAp), fg, TFT_BLACK);

    // Row 5 — Throttle: plain green at 0% (coasting to apoapsis is normal)
    {
      uint8_t thrPct = (uint8_t)constrain(state.throttle * 100.0f, 0.0f, 100.0f);
      lnchVal(5, "Throttle:", formatPerc(thrPct), TFT_DARK_GREEN, TFT_BLACK);
    }
  }

  // =========================================================
  // Rows 6-7: same in both phases — stage resources
  // =========================================================

  // Row 6 — Stage burn time: white-on-red <60s, yellow <120s, green
  thresholdColor((uint16_t)constrain(state.stageBurnTime, 0, 65535),
                 LNCH_BURNTIME_ALARM_S, TFT_WHITE,  TFT_RED,
                 LNCH_BURNTIME_WARN_S,  TFT_YELLOW, TFT_BLACK,
                      TFT_DARK_GREEN, TFT_BLACK, fg, bg);
  lnchVal(6, "T.Burn:", fmtTime(state.stageBurnTime), fg, bg);

  // Row 7 — Stage ΔV: white-on-red <150 m/s, yellow <300 m/s, green
  thresholdColor((uint16_t)constrain(state.stageDeltaV, 0, 65535),
                 DV_STG_ALARM_MS, TFT_WHITE,  TFT_RED,
                 DV_STG_WARN_MS,  TFT_YELLOW, TFT_BLACK,
                      TFT_DARK_GREEN, TFT_BLACK, fg, bg);
  lnchVal(7, "\xCE\x94V.Stg:", fmtMs(state.stageDeltaV), fg, bg);
}
