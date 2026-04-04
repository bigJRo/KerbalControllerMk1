/***************************************************************************************
   Screen_ACFT.ino -- Aircraft screen — drawScreen_ACFT (chrome is inline in drawStaticScreen)
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


static void drawScreen_ACFT(RA8875 &tft) {
  static const tFont   *F       = &Roboto_Black_40;
  static const uint16_t SECT_W  = 26;
  static const uint16_t AX      = ROW_PAD + SECT_W;
  static const uint16_t AW      = CONTENT_W - AX - ROW_PAD;
  static const uint16_t THIRD_W = AW / 3;
  static const uint16_t HALF_W  = AW / 2;
  uint16_t fg, bg;
  char buf[16];

  // Cache-checked draw for full-width rows
  // acftVal: kept as local lambda — ACFT uses acftRowY() (+1 offset) which
  // differs from rowYFor() (+2 ROW_PAD), so drawValue() cannot be used here
  // without introducing a 1px visual difference. (#6C intentional exception)
  auto acftVal = [&](uint8_t slot, uint8_t row, const char *label,
                     const String &val, uint16_t fgc, uint16_t bgc) {
    RowCache &cache = rowCache[8][slot];
    if (cache.value == val && cache.fg == fgc && cache.bg == bgc) return;
    printValue(tft, F, AX, acftRowY(row), AW, acftRowH(),
               label, val, fgc, bgc, COL_BACK, printState[8][slot]);
    cache.value = val; cache.fg = fgc; cache.bg = bgc;
  };

  // Cache-checked draw for split cells
  // splitVal: same acftRowY() exception as acftVal (#6C)
  auto splitVal = [&](uint8_t slot, uint16_t x, uint16_t w, uint8_t row,
                      const char *label, const String &val,
                      uint16_t fgc, uint16_t bgc) {
    RowCache &cache = rowCache[8][slot];
    if (cache.value == val && cache.fg == fgc && cache.bg == bgc) return;
    printValue(tft, F, x, acftRowY(row), w, acftRowH(),
               label, val, fgc, bgc, COL_BACK, printState[8][slot]);
    cache.value = val; cache.fg = fgc; cache.bg = bgc;
  };

  // Redraw section dividers every loop (value updates can overwrite them)
  tft.drawLine(0, acftRowY(4)-2, CONTENT_W, acftRowY(4)-2, TFT_GREY);
  tft.drawLine(0, acftRowY(4)-1, CONTENT_W, acftRowY(4)-1, TFT_GREY);
  tft.drawLine(0, acftRowY(5)-2, CONTENT_W, acftRowY(5)-2, TFT_GREY);
  tft.drawLine(0, acftRowY(5)-1, CONTENT_W, acftRowY(5)-1, TFT_GREY);
  tft.drawLine(0, acftRowY(7)-2, CONTENT_W, acftRowY(7)-2, TFT_GREY);
  tft.drawLine(0, acftRowY(7)-1, CONTENT_W, acftRowY(7)-1, TFT_GREY);

  // ── STATE block (rows 0-3) ──

  // Row 0 — Radar altitude
  fg = (state.radarAlt < ALT_RDR_ALARM_M)  ? TFT_WHITE  :
       (state.radarAlt < ALT_RDR_WARN_M) ? TFT_YELLOW  : TFT_DARK_GREEN;
  bg = (state.radarAlt < ALT_RDR_ALARM_M)  ? TFT_RED    : TFT_BLACK;
  acftVal(0, 0, "Alt.Rdr:", formatAlt(state.radarAlt), fg, bg);

  // Row 1 — Surface velocity
  fg = (state.surfaceVel < 0) ? TFT_RED : TFT_DARK_GREEN;
  acftVal(1, 1, "V.Srf:", fmtMs(state.surfaceVel), fg, TFT_BLACK);

  // Row 2 — IAS with configurable stall warning
  if (STALL_SPEED_MS > 0.0f) {
    float ias = state.IAS;
    if      (ias < STALL_SPEED_MS * 0.5f) { fg = TFT_WHITE;     bg = TFT_RED;   }
    else if (ias < STALL_SPEED_MS)        { fg = TFT_YELLOW;    bg = TFT_BLACK; }
    else                                  { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    acftVal(2, 2, "IAS:", fmtMs(state.IAS), fg, bg);
  } else {
    acftVal(2, 2, "IAS:", fmtMs(state.IAS), TFT_DARK_GREEN, TFT_BLACK);
  }

  // Row 3 — Vertical velocity with T.Grnd colouring
  if (state.verticalVel < 0.0f && state.radarAlt > 0.0f) {
    float tGround = fabsf(state.radarAlt / state.verticalVel);
    fg = (tGround < 10) ? TFT_WHITE  :
         (tGround < 30) ? TFT_YELLOW  : TFT_DARK_GREEN;
    bg = (tGround < 10) ? TFT_RED    : TFT_BLACK;
  } else {
    fg = TFT_DARK_GREEN; bg = TFT_BLACK;
  }
  acftVal(3, 3, "V.Vrt:", fmtMs(state.verticalVel), fg, bg);

  // ── AT block (row 4): triple split Hdg | Pitch | Roll ──
  // Cache slots: [4]=Hdg, [9]=Pitch, [10]=Roll
  // Use 0 decimal places so a 3-digit negative value (e.g. -180°) fits in its cell.
  {
    uint16_t xL = AX,              wL = THIRD_W;
    uint16_t xM = AX + THIRD_W,   wM = THIRD_W;
    uint16_t xR = AX + THIRD_W*2, wR = AW - THIRD_W*2;

    snprintf(buf, sizeof(buf), "%.0f\xB0", state.heading);
    splitVal(4, xL, wL, 4, "Hdg:", buf, COL_VALUE, COL_BACK);

    snprintf(buf, sizeof(buf), "%.0f\xB0", state.pitch);
    splitVal(9, xM, wM, 4, "Pitch:", buf, COL_VALUE, COL_BACK);

    float absRoll = fabsf(state.roll);
    bool warnRoll = (state.vesselType == type_Plane && state.inAtmo);
    if      (warnRoll && absRoll > ROLL_ALARM_DEG) { fg = TFT_WHITE;     bg = TFT_RED;   }
    else if (warnRoll && absRoll > ROLL_WARN_DEG)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
    else                                  { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    snprintf(buf, sizeof(buf), "%.0f\xB0", state.roll);
    splitVal(10, xR, wR, 4, "Roll:", buf, fg, bg);
  }

  // ── AERO block (rows 5-6) ──

  // Row 5: half split AoA | Slip
  // Cache slots: [5]=AoA, [11]=Slip
  // Suppress when surface velocity is too low — velocity vector is undefined/noisy below 0.5 m/s
  {
    uint16_t xL = AX,               wL = HALF_W - ROW_PAD;
    uint16_t xR = AX + HALF_W + ROW_PAD, wR = HALF_W - ROW_PAD;

    if (state.surfaceVel < 0.5f) {
      splitVal(5,  xL, wL, 5, "AoA:",  "---", TFT_DARK_GREY, TFT_BLACK);
      splitVal(11, xR, wR, 5, "Slip:", "---", TFT_DARK_GREY, TFT_BLACK);
    } else {
      float aoa  = state.pitch - state.srfVelPitch;
      float slip = state.heading - state.srfVelHeading;
      if (slip >  180.0f) slip -= 360.0f;
      if (slip < -180.0f) slip += 360.0f;

      auto angColor = [](float v, float warn, float alarm,
                         uint16_t &fg, uint16_t &bg) {
        float av = fabsf(v);
        if      (av > alarm) { fg = TFT_WHITE;     bg = TFT_RED;   }
        else if (av > warn)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
        else                 { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
      };

      angColor(aoa,  AOA_WARN_DEG,  AOA_ALARM_DEG,  fg, bg);
      snprintf(buf, sizeof(buf), "%+.1f\xB0", aoa);
      splitVal(5, xL, wL, 5, "AoA:", buf, fg, bg);

      angColor(slip, SLIP_WARN_DEG, SLIP_ALARM_DEG, fg, bg);
      snprintf(buf, sizeof(buf), "%+.1f\xB0", slip);
      splitVal(11, xR, wR, 5, "Slip:", buf, fg, bg);
    }
  }

  // Row 6: half split Mach | G
  // Cache slots: [6]=Mach, [12]=G
  {
    uint16_t xL = AX,               wL = HALF_W - ROW_PAD;
    uint16_t xR = AX + HALF_W + ROW_PAD, wR = HALF_W - ROW_PAD;

    float m = state.machNumber;
    snprintf(buf, sizeof(buf), "%.2f", m);
    bool transonic = (m >= 0.85f && m <= 1.2f);
    splitVal(6, xL, wL, 6, "Mach:", buf,
             transonic ? TFT_YELLOW : TFT_DARK_GREEN, TFT_BLACK);

    float g = state.gForce;
    snprintf(buf, sizeof(buf), "%.2f", g);
    if      (g > G_ALARM_POS || g < G_ALARM_NEG) { fg = TFT_WHITE;     bg = TFT_RED;   }
    else if (g < G_WARN_NEG  || g > G_WARN_POS)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
    else                             { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    splitVal(12, xR, wR, 6, "G:", buf, fg, bg);
  }

  // ── CT block (row 7): half split Gear | Brakes ──
  // Cache slots: [7]=Gear, [8]=Brakes
  {
    uint16_t xL = AX,               wL = HALF_W - ROW_PAD;
    uint16_t xR = AX + HALF_W + ROW_PAD, wR = HALF_W - ROW_PAD;
    uint16_t y  = acftRowY(7), h = acftRowH();

    // Gear: context-dependent
    //   Ground (Landed/Splashed/PreLaunch) gear UP → white-on-red (wrong for ground ops)
    //   Ground gear DOWN → green
    //   Flying gear DOWN at speed > 160 m/s → yellow (structural risk)
    //   Flying gear DOWN at speed <= 160 m/s → green (normal approach/low speed)
    //   Flying gear UP → dark grey (retracted in flight is normal)
    const char *gearStr = state.gear_on ? "DOWN" : "UP";
    uint16_t    gearFg, gearBg;
    bool gearGroundState = (state.situation == sit_Landed ||
                            state.situation == sit_Splashed ||
                            state.situation == sit_PreLaunch);
    if (gearGroundState) {
      // On ground: UP is wrong, DOWN is correct
      gearFg = state.gear_on ? TFT_DARK_GREEN : TFT_WHITE;
      gearBg = state.gear_on ? TFT_BLACK      : TFT_RED;
    } else if (state.gear_on) {
      // Flying with gear down: warn if over speed threshold
      gearFg = (state.surfaceVel > GEAR_MAX_SPEED_MS) ? TFT_YELLOW : TFT_DARK_GREEN;
      gearBg = TFT_BLACK;
    } else {
      // Flying with gear up: normal, informational
      gearFg = TFT_DARK_GREY;
      gearBg = TFT_BLACK;
    }
    RowCache   &gc = rowCache[8][7];
    if (gc.value != gearStr || gc.fg != gearFg || gc.bg != gearBg) {
      printValue(tft, F, xL, y, wL, h, "Gear:", gearStr, gearFg, gearBg,
                 COL_BACK, printState[8][7]);
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
      printValue(tft, F, xR, y, wR, h, "Brakes:", brkStr, brkFg, brkBg,
                 COL_BACK, printState[8][8]);
      bc.value = brkStr; bc.fg = brkFg; bc.bg = brkBg;
    }
  }
}

