/***************************************************************************************
   Screen_ATT.ino -- Attitude screen — chromeScreen_ATT, drawScreen_ATT
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


bool _attOrbMode() {
  float bodyRad   = (currentBody.radius > 0.0f) ? currentBody.radius : 600000.0f;
  bool  ascending = (state.verticalVel >= 0.0f);
  float switchAlt = ascending ? (bodyRad * 0.06f) : (bodyRad * 0.055f);
  return state.altitude > switchAlt;
}
bool _attPrevOrbMode = false;

static void chromeScreen_ATT(RA8875 &tft) {
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t RX     = ROW_PAD + SECT_W;
  static const uint16_t RW     = CONTENT_W - RX - ROW_PAD;
  static const tFont   *F      = &Roboto_Black_40;

  // Group heights
  uint16_t craftH = rowHFor(NR) * 4;   // CRAFT: rows 0-3
  uint16_t velH   = rowHFor(NR) * 2;   // velocity: rows 4-5
  uint16_t errH   = rowHFor(NR) * 2;   // error:    rows 6-7

  uint16_t g0y = TITLE_TOP;
  uint16_t g1y = g0y + craftH;
  uint16_t g2y = g1y + velH;

  // Context: which velocity reference are we showing?
  bool orbMode = _attOrbMode();
  const char *velLabel = orbMode ? "ORB V" : "SRF V";

  // Section labels (drawn first, dividers overwrite their edges)
  drawVerticalText(tft, 0, g0y, SECT_W, craftH,
                   &Roboto_Black_16, "CRAFT",  TFT_LIGHT_GREY, TFT_BLACK);
  drawVerticalText(tft, 0, g1y, SECT_W, velH,
                   &Roboto_Black_16, velLabel, TFT_LIGHT_GREY, TFT_BLACK);
  drawVerticalText(tft, 0, g2y, SECT_W, errH,
                   &Roboto_Black_16, "ERR",    TFT_LIGHT_GREY, TFT_BLACK);

  // Dividers — 2px wide for visual distinction
  tft.drawLine(0, g1y,   CONTENT_W, g1y,   TFT_GREY);
  tft.drawLine(0, g1y+1, CONTENT_W, g1y+1, TFT_GREY);
  tft.drawLine(0, g2y,   CONTENT_W, g2y,   TFT_GREY);
  tft.drawLine(0, g2y+1, CONTENT_W, g2y+1, TFT_GREY);

  // CRAFT row labels (rows 0-3)
  const char *craftLabels[4] = { "Hdg:", "Pitch:", "Roll:", "SAS:" };
  for (uint8_t r = 0; r < 4; r++)
    printDispChrome(tft, F, RX, rowYFor(r, NR), RW, rowHFor(NR),
                    craftLabels[r], COL_LABEL, COL_BACK, COL_NO_BDR);

  // Velocity group row labels (rows 4-5) — velocity vector heading and pitch
  printDispChrome(tft, F, RX, rowYFor(4, NR), RW, rowHFor(NR),
                  "V.Hdg:", COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, RX, rowYFor(5, NR), RW, rowHFor(NR),
                  "V.Pit:", COL_LABEL, COL_BACK, COL_NO_BDR);

  // Error group row labels (rows 6-7) — angle between velocity vector and craft nose
  printDispChrome(tft, F, RX, rowYFor(6, NR), RW, rowHFor(NR),
                  "Hdg.Err:", COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, RX, rowYFor(7, NR), RW, rowHFor(NR),
                  "Pit.Err:", COL_LABEL, COL_BACK, COL_NO_BDR);
}

/***************************************************************************************
   SCREEN VALUE UPDATE FUNCTIONS — called every loop, draw only on change
****************************************************************************************/



static void drawScreen_ATT(RA8875 &tft) {
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t RX     = ROW_PAD + SECT_W;
  static const uint16_t RW     = CONTENT_W - RX - ROW_PAD;
  static const tFont   *F      = &Roboto_Black_40;
  char buf[16];

  // If orbital mode changed, force a full chrome redraw (section label changes)
  bool orbMode = _attOrbMode();
  if (orbMode != _attPrevOrbMode) {
    _attPrevOrbMode = orbMode;
    for (uint8_t r = 0; r < ROW_COUNT; r++) rowCache[2][r].value = "\x01";
    switchToScreen(screen_ATT);
    return;
  }

  // Redraw group dividers every loop — printValue fillRect at the group boundary
  // rows (row 3 bottom = g1y, row 5 bottom = g2y) overwrites them on value changes.
  uint16_t g1y = TITLE_TOP + rowHFor(NR) * 4;
  uint16_t g2y = TITLE_TOP + rowHFor(NR) * 6;
  tft.drawLine(0, g1y,   CONTENT_W, g1y,   TFT_GREY);
  tft.drawLine(0, g1y+1, CONTENT_W, g1y+1, TFT_GREY);
  tft.drawLine(0, g2y,   CONTENT_W, g2y,   TFT_GREY);
  tft.drawLine(0, g2y+1, CONTENT_W, g2y+1, TFT_GREY);

  // Cache-checked draw helper
  // attVal -> drawValue() split overload with RX/RW section geometry (#6B)
  auto attVal = [&](uint8_t row, const char *label,
                    const String &val, uint16_t fg, uint16_t bg) {
    drawValue(tft, 2, row, RX, RW, label, val, fg, bg, F, NR);
  };

  // --- CRAFT group (rows 0-3) ---

  snprintf(buf, sizeof(buf), "%.1f\xB0", state.heading);
  attVal(0, "Hdg:", buf, COL_VALUE, COL_BACK);

  snprintf(buf, sizeof(buf), "%.1f\xB0", state.pitch);
  attVal(1, "Pitch:", buf, COL_VALUE, COL_BACK);

  // Roll: warnings only when Plane type AND in atmosphere
  {
    float absRoll = fabsf(state.roll);
    uint16_t rfg, rbg;
    bool warnRoll = (state.vesselType == type_Plane && state.inAtmo);
    if      (warnRoll && absRoll > ROLL_ALARM_DEG) { rfg = TFT_WHITE;     rbg = TFT_RED;   }
    else if (warnRoll && absRoll > ROLL_WARN_DEG)  { rfg = TFT_YELLOW;    rbg = TFT_BLACK; }
    else                                  { rfg = TFT_DARK_GREEN; rbg = TFT_BLACK; }
    snprintf(buf, sizeof(buf), "%.1f\xB0", state.roll);
    attVal(2, "Roll:", buf, rfg, rbg);
  }

  // SAS mode
  {
    const char *sasStr;
    uint16_t   sasFg, sasBg;
    switch (state.sasMode) {
      case 255: sasStr = "OFF";        sasFg = TFT_DARK_GREY;   sasBg = COL_BACK;   break;
      case 0:   sasStr = "STAB";       sasFg = TFT_DARK_GREEN;  sasBg = COL_BACK;   break;
      case 1:   sasStr = "PROGRADE";   sasFg = TFT_NEON_GREEN;  sasBg = COL_BACK;   break;  // navball prograde green
      case 2:   sasStr = "RETRO";      sasFg = TFT_NEON_GREEN;  sasBg = COL_BACK;   break;  // navball prograde green
      case 3:   sasStr = "NORMAL";     sasFg = TFT_MAGENTA;     sasBg = COL_BACK;   break;  // navball magenta
      case 4:   sasStr = "ANTI-NRM";   sasFg = TFT_MAGENTA;     sasBg = COL_BACK;   break;  // navball magenta
      case 5:   sasStr = "RAD-OUT";    sasFg = TFT_SKY;         sasBg = COL_BACK;   break;  // navball cyan
      case 6:   sasStr = "RAD-IN";     sasFg = TFT_SKY;         sasBg = COL_BACK;   break;  // navball cyan
      case 7:   sasStr = "TARGET";     sasFg = TFT_VIOLET;      sasBg = COL_BACK;   break;  // navball purple
      case 8:   sasStr = "ANTI-TGT";   sasFg = TFT_VIOLET;      sasBg = COL_BACK;   break;  // navball purple
      case 9:   sasStr = "MANEUVER";   sasFg = TFT_BLUE;        sasBg = COL_BACK;   break;  // navball blue
      default:  sasStr = "---";        sasFg = TFT_DARK_GREY;   sasBg = COL_BACK;   break;
    }
    attVal(3, "SAS:", sasStr, sasFg, sasBg);
  }

  // --- Velocity group (rows 4-5): ORB V in orbit, SRF V in atmosphere ---
  // Suppress velocity vector heading/pitch when surface speed is too low to be meaningful.
  // Below 0.5 m/s the velocity vector is noise; show --- to avoid erratic values.
  float velHdg, velPitch;
  bool velValid = (state.surfaceVel >= 0.5f);
  if (velValid) {
    velHdg   = orbMode ? state.orbVelHeading : state.srfVelHeading;
    velPitch = orbMode ? state.orbVelPitch   : state.srfVelPitch;
    snprintf(buf, sizeof(buf), "%.1f\xB0", velHdg);
    attVal(4, "V.Hdg:", buf, COL_VALUE, COL_BACK);
    snprintf(buf, sizeof(buf), "%.1f\xB0", velPitch);
    attVal(5, "V.Pit:", buf, COL_VALUE, COL_BACK);
  } else {
    velHdg   = state.heading;  // use craft heading so error = 0 when shown
    velPitch = state.pitch;
    attVal(4, "V.Hdg:", "---", TFT_DARK_GREY, COL_BACK);
    attVal(5, "V.Pit:", "---", TFT_DARK_GREY, COL_BACK);
  }

  // --- Error group (rows 6-7): error to active velocity vector ---
  // Suppress when velocity vector is invalid (low speed) — derived values are meaningless.
  {
    if (!velValid) {
      attVal(6, "Hdg.Err:", "---", TFT_DARK_GREY, COL_BACK);
      attVal(7, "Pit.Err:", "---", TFT_DARK_GREY, COL_BACK);
    } else {
      // pErr: positive = nose above velocity vector (conventional AoA sign)
      // hErr: positive = nose to the right of velocity vector (consistent with pErr convention)
      float pErr = state.pitch - velPitch;
      float hErr = state.heading - velHdg;
      if (hErr >  180.0f) hErr -= 360.0f;
      if (hErr < -180.0f) hErr += 360.0f;

      // Only colour errors as warnings/alarms when in atmosphere — in space
      // there is no meaningful connection between attitude and velocity vector,
      // so alarm colours are operational noise.
      auto errColor = [](float e, uint16_t &fg, uint16_t &bg) {
        float ae = fabsf(e);
        if (!state.inAtmo)               { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
        else if (ae > ATT_ERR_ALARM_DEG) { fg = TFT_WHITE;      bg = TFT_RED;   }
        else if (ae > ATT_ERR_WARN_DEG)  { fg = TFT_YELLOW;     bg = TFT_BLACK; }
        else                             { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
      };

      uint16_t fg, bg;
      errColor(hErr, fg, bg);
      snprintf(buf, sizeof(buf), "%+.1f\xB0", hErr);
      attVal(6, "Hdg.Err:", buf, fg, bg);

      errColor(pErr, fg, bg);
      snprintf(buf, sizeof(buf), "%+.1f\xB0", pErr);
      attVal(7, "Pit.Err:", buf, fg, bg);
    }
  }
}

/***************************************************************************************
   PUBLIC INTERFACE — called from loop()
****************************************************************************************/
