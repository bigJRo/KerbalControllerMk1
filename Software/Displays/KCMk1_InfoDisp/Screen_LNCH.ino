/***************************************************************************************
   Screen_LNCH.ino -- Launch / Circularization screen — chromeScreen_LNCH, drawScreen_LNCH
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


bool _lnchOrbitalMode   = false;
bool _lnchManualOverride = false;  // true = pilot has overridden auto phase switch

static void chromeScreen_LNCH(RA8875 &tft) {
  static const tFont   *F      = &Roboto_Black_40;
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t AX     = ROW_PAD + SECT_W;
  static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
  uint16_t rowH = rowHFor(NR);  // hoisted — used in both phases

  // _lnchOrbitalMode is set by drawScreen_LNCH via hysteresis before chrome is called.
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
  uint16_t fg, bg;

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

  // Manual override indicator — red filled circle on right of title bar
  // Drawn every loop: fills with red when overridden, black when auto
  {
    uint16_t indCol = _lnchManualOverride ? TFT_RED : TFT_BLACK;
    tft.fillCircle(700, 29, 8, indCol);
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

    // Row 3 — Apoapsis: red if negative, yellow if below safe orbit, green if safe
    {
      bool apaWarn = (warnAlt > 0 && state.apoapsis > 0 && state.apoapsis < warnAlt);
      if      (state.apoapsis < 0) { fg = TFT_RED;        }
      else if (apaWarn)            { fg = TFT_YELLOW;      }
      else                         { fg = TFT_DARK_GREEN;  }
      lnchVal(3, "ApA:", formatAlt(state.apoapsis), fg, TFT_BLACK);
    }

    // Row 4 — Time to apoapsis: suppress on ground and when no valid apoapsis yet
    if (state.apoapsis <= 0.0f ||
        (state.situation & sit_PreLaunch) || (state.situation & sit_Landed)) {
      lnchVal(4, "T+Ap:", "---", TFT_DARK_GREY, TFT_BLACK);
    } else {
      if      (state.timeToAp < 0)              { fg = TFT_RED;       }
      else if (state.timeToAp < LNCH_TOAPO_WARN_S) { fg = TFT_YELLOW;    }
      else                                      { fg = TFT_DARK_GREEN; }
      lnchVal(4, "T+Ap:", formatTime(state.timeToAp), fg, TFT_BLACK);
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
    lnchVal(4, "T+Ap:", formatTime(state.timeToAp), fg, TFT_BLACK);

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
  lnchVal(6, "T.Burn:", formatTime(state.stageBurnTime), fg, bg);

  // Row 7 — Stage ΔV: white-on-red <150 m/s, yellow <300 m/s, green
  thresholdColor((uint16_t)constrain(state.stageDeltaV, 0, 65535),
                 DV_STG_ALARM_MS, TFT_WHITE,  TFT_RED,
                 DV_STG_WARN_MS,  TFT_YELLOW, TFT_BLACK,
                      TFT_DARK_GREEN, TFT_BLACK, fg, bg);
  lnchVal(7, "\xCE\x94V.Stg:", fmtMs(state.stageDeltaV), fg, bg);
}
