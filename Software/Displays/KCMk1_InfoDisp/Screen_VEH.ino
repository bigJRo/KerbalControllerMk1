/***************************************************************************************
   Screen_VEH.ino -- Vehicle Info screen — chromeScreen_VEH, drawScreen_VEH
****************************************************************************************/
#include "KCMk1_InfoDisp.h"


static void chromeScreen_VEH(RA8875 &tft) {
  static const tFont   *F      = &Roboto_Black_40;
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t AX     = ROW_PAD + SECT_W;
  static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
  static const uint16_t AHW    = AW / 2;
  uint16_t rowH = rowHFor(NR);

  // Section labels: INFO(rows0-2), CREW(rows3-5), PROP(rows6-7)
  drawVerticalText(tft, 0, TITLE_TOP,          SECT_W, rowH*3, &Roboto_Black_16, "INFO", TFT_LIGHT_GREY, TFT_BLACK);
  drawVerticalText(tft, 0, TITLE_TOP + rowH*3, SECT_W, rowH*3, &Roboto_Black_16, "CREW", TFT_LIGHT_GREY, TFT_BLACK);
  drawVerticalText(tft, 0, TITLE_TOP + rowH*6, SECT_W, rowH*2, &Roboto_Black_16, "PROP", TFT_LIGHT_GREY, TFT_BLACK);

  // Row labels (right of section strip)
  printDispChrome(tft, F, AX, rowYFor(0,NR), AW, rowH, "Name:",    COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(1,NR), AW, rowH, "Type:",    COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(2,NR), AW, rowH, "Status:",  COL_LABEL, COL_BACK, COL_NO_BDR);

  printDispChrome(tft, F, AX, rowYFor(3,NR), AW, rowH, "Control:",          COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(4,NR), AW, rowH, "Signal:",           COL_LABEL, COL_BACK, COL_NO_BDR);

  // Row 5 — split Crew | Cap (uses AX-based geometry)
  {
    uint16_t y = rowYFor(5, NR), h = rowH;
    printDispChrome(tft, F, AX,                   y, AHW - ROW_PAD, h, "Crew:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX + AHW + ROW_PAD,   y, AHW - ROW_PAD, h, "Cap:",  COL_LABEL, COL_BACK, COL_NO_BDR);
    for (int8_t dx = -1; dx <= 1; dx++)
      tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, rowYFor(6,NR) - 1, TFT_GREY);
  }
  printDispChrome(tft, F, AX, rowYFor(6,NR), AW, rowH, "\xCE\x94V.Stg:", COL_LABEL, COL_BACK, COL_NO_BDR);
  printDispChrome(tft, F, AX, rowYFor(7,NR), AW, rowH, "\xCE\x94V.Tot:", COL_LABEL, COL_BACK, COL_NO_BDR);

  // Dividers — drawn LAST, +1 offset to clear printValue fillRect boundary
  uint16_t d1 = TITLE_TOP + rowH*3 + 1;  // after Status  (row2/row3)
  uint16_t d2 = TITLE_TOP + rowH*6 + 1;  // after Signal  (row5/row6)
  tft.drawLine(0, d1,   CONTENT_W, d1,   TFT_GREY);
  tft.drawLine(0, d1+1, CONTENT_W, d1+1, TFT_GREY);
  tft.drawLine(0, d2,   CONTENT_W, d2,   TFT_GREY);
  tft.drawLine(0, d2+1, CONTENT_W, d2+1, TFT_GREY);
}

// LNDG mode toggle — persists across screen visits.
// Pilot sets this before the maneuver by tapping the title bar.
// false = Powered Descent (deorbit + propulsive landing, vacuum or atmo)
// true  = Re-entry (unpowered atmospheric re-entry + capsule recovery)


static void drawScreen_VEH(RA8875 &tft) {
  static const tFont   *F      = &Roboto_Black_40;
  static const uint8_t  NR     = 8;
  static const uint16_t SECT_W = 26;
  static const uint16_t AX     = ROW_PAD + SECT_W;
  static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
  static const uint16_t AHW    = AW / 2;
  uint16_t fg, bg;

  // Cache-checked draw helper using section-label-aware geometry
  auto vehVal = [&](uint8_t row, const char *label, const String &val,
                    uint16_t fgc, uint16_t bgc) {
    RowCache &rc = rowCache[2][row];
    if (rc.value == val && rc.fg == fgc && rc.bg == bgc) return;
    printValue(tft, F, AX, rowYFor(row, NR), AW, rowHFor(NR),
               label, val, fgc, bgc, COL_BACK, printState[2][row]);
    rc.value = val; rc.fg = fgc; rc.bg = bgc;
  };

  // ── INFO block (rows 0-2): identity ──

  // Row 0 — Vessel name
  vehVal(0, "Name:", state.vesselName, COL_VALUE, COL_BACK);

  // Row 1 — Vessel type with colour coding
  const char *typeName;
  uint16_t typeColor;
  switch (state.vesselType) {
    case type_Ship:     typeName = "Ship";      typeColor = TFT_DARK_GREEN; break;
    case type_Probe:    typeName = "Probe";     typeColor = TFT_MAGENTA;    break;
    case type_Relay:    typeName = "Relay";     typeColor = TFT_VIOLET;     break;
    case type_Rover:    typeName = "Rover";     typeColor = TFT_OLIVE;      break;
    case type_Lander:   typeName = "Lander";    typeColor = TFT_NAVY;       break;
    case type_Plane:    typeName = "Plane";     typeColor = TFT_SKY;        break;
    case type_Station:  typeName = "Station";   typeColor = TFT_MAROON;     break;
    case type_Base:     typeName = "Base";      typeColor = TFT_BROWN;      break;
    case type_EVA:      typeName = "EVA";       typeColor = TFT_JUNGLE;     break;
    case type_Debris:   typeName = "Debris";    typeColor = TFT_CORNELL;    break;
    case type_Object:   typeName = "Object";    typeColor = TFT_CORNELL;    break;
    case type_Unknown:  typeName = "Unknown";   typeColor = TFT_CORNELL;    break;
    case type_Flag:     typeName = "Flag";      typeColor = TFT_CORNELL;    break;
    case type_SciCtrlr: typeName = "Sci Ctrlr"; typeColor = TFT_CORNELL;   break;
    case type_SciPart:  typeName = "Sci Part";  typeColor = TFT_CORNELL;    break;
    case type_Part:     typeName = "Part";      typeColor = TFT_CORNELL;    break;
    case type_GndPart:  typeName = "Gnd Part";  typeColor = TFT_CORNELL;    break;
    default:            typeName = "---";       typeColor = TFT_DARK_GREY;  break;
  }
  vehVal(1, "Type:", typeName, typeColor, TFT_BLACK);

  // Row 2 — Flight status / situation
  const char *condName;
  uint16_t condColor;
  if (state.isRecoverable) {
    condName  = "RECOVERABLE";
    condColor = TFT_SKY;
  } else {
    condColor = TFT_DARK_GREEN;
    switch (state.situation) {
      case sit_PreLaunch: condName = "PRE-LAUNCH";   break;
      case sit_Flying:    condName = "IN FLIGHT";    break;
      case sit_SubOrb:    condName = "SUB-ORBITAL";  break;
      case sit_Orbit:     condName = "IN ORBIT";     break;
      case sit_Escaping:  condName = "ESCAPE ORBIT"; break;
      case sit_Landed:    condName = "LANDED";       break;
      case sit_Splashed:  condName = "SPLASHED";     break;
      case sit_Docked:    condName = "DOCKED";       break;
      default:            condName = "---";          break;
    }
  }
  vehVal(2, "Status:", condName, condColor, TFT_BLACK);

  // ── CREW block (rows 3-5): crew, control, comms ──

  // Row 3 — Control level
  const char *ctrlName;
  uint16_t ctrlColor;
  switch (state.ctrlLevel) {
    case 0:  ctrlName = "No Control";     ctrlColor = TFT_WHITE;       bg = TFT_RED;   break;
    case 1:  ctrlName = "Limited Probe";  ctrlColor = TFT_INT_ORANGE;  bg = TFT_BLACK; break;
    case 2:  ctrlName = "Limited Manned"; ctrlColor = TFT_DULL_YELLOW; bg = TFT_BLACK; break;
    default: ctrlName = "Full Control";   ctrlColor = TFT_DARK_GREEN;  bg = TFT_BLACK; break;
  }
  vehVal(3, "Control:", ctrlName, ctrlColor, bg);

  // Row 4 — CommNet signal: white-on-red 0%, yellow <50%, green
  {
    uint8_t sig = state.commNetSignal;
    char sigStr[8];
    snprintf(sigStr, sizeof(sigStr), "%d%%", sig);
    if      (sig == 0) { fg = TFT_WHITE;     bg = TFT_RED;   }
    else if (sig < VEH_SIGNAL_WARN_PCT) { fg = TFT_YELLOW;    bg = TFT_BLACK; }
    else               { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
    vehVal(4, "Signal:", sigStr, fg, bg);
  }

  // Row 5 — Split: Crew (left, cache[2][9]) | Cap (right, cache[2][10])
  {
    uint16_t y = rowYFor(5, NR), h = rowHFor(NR);
    char crewStr[8], capStr[8];
    snprintf(crewStr, sizeof(crewStr), "%d", state.crewCount);
    snprintf(capStr,  sizeof(capStr),  "%d", state.crewCapacity);

    RowCache &cc = rowCache[2][9];
    String crewS = crewStr;
    if (cc.value != crewS || cc.fg != COL_VALUE || cc.bg != COL_BACK) {
      printValue(tft, F, AX, y, AHW - ROW_PAD, h,
                 "Crew:", crewS, COL_VALUE, COL_BACK, COL_BACK,
                 printState[2][9]);
      cc.value = crewS; cc.fg = COL_VALUE; cc.bg = COL_BACK;
    }

    RowCache &ca = rowCache[2][10];
    String capS = capStr;
    if (ca.value != capS || ca.fg != COL_VALUE || ca.bg != COL_BACK) {
      printValue(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h,
                 "Cap:", capS, COL_VALUE, COL_BACK, COL_BACK,
                 printState[2][10]);
      ca.value = capS; ca.fg = COL_VALUE; ca.bg = COL_BACK;
    }
  }

  // ── PROP block (rows 6-7): delta-V ──

  // Row 6 — Stage ΔV: white-on-red <150, yellow <300, green
  thresholdColor((uint16_t)constrain(state.stageDeltaV, 0, 65535),
                 DV_STG_ALARM_MS, TFT_WHITE,  TFT_RED,
                 DV_STG_WARN_MS,  TFT_YELLOW, TFT_BLACK,
                      TFT_DARK_GREEN, TFT_BLACK, fg, bg);
  vehVal(6, "\xCE\x94V.Stg:", fmtMs(state.stageDeltaV), fg, bg);

  // Row 7 — Total ΔV: white-on-red if < stage ΔV, yellow <500, green
  if      (state.totalDeltaV < state.stageDeltaV) { fg = TFT_WHITE;     bg = TFT_RED;   }
  else if (state.totalDeltaV < DV_TOT_WARN_MS)    { fg = TFT_YELLOW;    bg = TFT_BLACK; }
  else                                            { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
  vehVal(7, "\xCE\x94V.Tot:", fmtMs(state.totalDeltaV), fg, bg);
}

