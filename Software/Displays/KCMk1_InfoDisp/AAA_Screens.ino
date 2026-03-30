/***************************************************************************************
   AAA_Screens.ino -- Shared screen infrastructure for Kerbal Controller Mk1 Information Display
   Must compile before Screen_*.ino tabs (AAA_ prefix ensures correct sort order).

   Screen map (mission-phase order):
     0  LNCH  Launch Information
     1  APSI  Apsides Information
     2  ORB   Orbit Information
     3  ATT   Attitude Information
     4  MNVR  Maneuver Information
     5  RNDZ  Rendezvous Information
     6  DOCK  Docking Information
     7  VEH   Vehicle Information
     8  LNDG  Landing Information
     9  ACFT  Aircraft Information

   Layout (800x480):
     Title bar  : 62px (58px text + 4px rule)
     Data rows  : 8 rows x 52px each = 416px
     Sidebar    : 80px right-hand column, 10 labelled buttons

   Update pattern (mirrors Annunciator):
     Chrome (labels)  : printDispChrome() — called once per screen transition.
     Values           : printValue()      — called only when state != prev.
                        Draws only the right-hand value region, label untouched.
     Colour changes   : tracked via prev colour fields per row.
     This avoids all unnecessary SPI traffic and eliminates label flicker entirely.
****************************************************************************************/
#include "KCMk1_InfoDisp.h"

/***************************************************************************************
   LAYOUT CONSTANTS
****************************************************************************************/
const uint16_t CONTENT_W    = SCREEN_W - SIDEBAR_W;

const uint16_t TITLE_H      = 58;
const uint16_t TITLE_RULE_H = 4;
const uint16_t TITLE_TOP    = TITLE_H + TITLE_RULE_H;

const tFont   *TITLE_FONT   = &Roboto_Black_36;
const tFont   *ROW_FONT     = &Roboto_Black_40;  // all screen row labels
const uint16_t ROW_PAD      = 2;

const uint16_t COL_LABEL    = TFT_WHITE;
const uint16_t COL_VALUE    = TFT_DARK_GREEN;
const uint16_t COL_BACK     = TFT_BLACK;
const uint16_t COL_NO_BDR   = TFT_BLACK;

const uint8_t  SB_BTN_COUNT = SCREEN_COUNT;
inline uint16_t sbBtnH()             { return SCREEN_H / SB_BTN_COUNT; }
inline uint16_t sbBtnY(uint8_t btn)  { return btn * sbBtnH(); }

const char * const SCREEN_TITLES[SCREEN_COUNT] = {
  "LAUNCH",
  "APSIDES",
  "ORBIT",
  "ATTITUDE",
  "MANEUVER",
  "RENDEZVOUS",
  "DOCKING",
  "VEHICLE INFO",
  "LANDING",
  "AIRCRAFT"
};

const char * const SCREEN_IDS[SCREEN_COUNT] = {
  "LNCH", "APSI", "ORB", "ATT", "MNVR", "RNDZ", "DOCK", "VEH", "LNDG", "ACFT"
};

const ButtonLabel btnScreenOff = {
  "", TFT_WHITE, TFT_WHITE, TFT_OFF_BLACK, TFT_NAVY, TFT_GREY, TFT_GREY
};
const ButtonLabel btnScreenOn = {
  "", TFT_WHITE, TFT_WHITE, TFT_CORNELL, TFT_CORNELL, TFT_GREY, TFT_WHITE
};

/***************************************************************************************
   PREV STATE SHADOW
   Mirrors the fields actually displayed. Values are redrawn only when changed.
   Colour fields track the last-drawn colour so a colour change triggers a redraw
   even if the formatted string is identical.
   Initialised to sentinel values in drawStaticScreen() to force first-draw.
****************************************************************************************/
// RowCache struct defined in KCMk1_InfoDisp.h (shared with TouchEvents.ino)
RowCache  rowCache  [SCREEN_COUNT][ROW_COUNT];
PrintState printState[SCREEN_COUNT][ROW_COUNT];

/***************************************************************************************
   ROW GEOMETRY
   rowH/rowY computed from per-screen row count so rows fill the content area evenly.
****************************************************************************************/
const uint16_t CONTENT_H = SCREEN_H - TITLE_TOP;  // 420px

inline uint16_t rowHFor(uint8_t nRows) { return CONTENT_H / nRows; }
inline uint16_t rowYFor(uint8_t row, uint8_t nRows) {
  return TITLE_TOP + row * rowHFor(nRows) + ROW_PAD;
}
inline uint16_t rowHInner(uint8_t nRows) { return rowHFor(nRows) - ROW_PAD * 2; }
inline uint16_t rowX()  { return ROW_PAD; }
inline uint16_t rowW()  { return CONTENT_W - ROW_PAD * 2; }

// All screens use NR=8; call rowYFor(row, NR) and rowHFor(NR) explicitly.

// ACFT geometry: 8 rows, Roboto_Black_40 (48px font in 52px cells — 4px clearance).
// ACFT row geometry: uses rowHFor(8) with a +1 Y offset to match printValue trim behaviour.
inline uint16_t acftRowY(uint8_t row) { return TITLE_TOP + row * rowHFor(8) + 1; }
inline uint16_t acftRowH()            { return rowHFor(8); }

/***************************************************************************************
   VALUE FORMATTERS
****************************************************************************************/
String fmtNum(float v) {
  if (v >= 1000.0f || v <= -1000.0f) return formatSep(v);
  char buf[16]; dtostrf(v, 1, 1, buf); return String(buf);
}
String fmtUnit(float v, const char *unit) { return fmtNum(v) + " " + unit; }
String fmtMs(float v)   { return fmtUnit(v, "m/s"); }

/***************************************************************************************
   DRAW SIDEBAR
****************************************************************************************/
void drawSidebar(RA8875 &tft) {
  tft.drawLine(SCREEN_W - SIDEBAR_W, 0, SCREEN_W - SIDEBAR_W, SCREEN_H, TFT_GREY);
  uint16_t bx = SCREEN_W - SIDEBAR_W + 1;
  uint16_t bw = SIDEBAR_W - 1;
  uint16_t bh = sbBtnH();
  for (uint8_t i = 0; i < SB_BTN_COUNT; i++) {
    ButtonLabel btn = (i == (uint8_t)activeScreen) ? btnScreenOn : btnScreenOff;
    btn.text = SCREEN_IDS[i];
    drawButton(tft, bx, sbBtnY(i), bw, bh, btn, &Roboto_Black_20, true);
  }
}

/***************************************************************************************
   DRAW TITLE BAR (chrome — once per transition)
****************************************************************************************/
void drawTitleBar(RA8875 &tft, ScreenType s) {
  tft.fillRect(0, 0, CONTENT_W, TITLE_TOP, TFT_BLACK);
  textCenter(tft, TITLE_FONT, 0, 0, CONTENT_W, TITLE_H,
             SCREEN_TITLES[(uint8_t)s], TFT_WHITE, TFT_BLACK);
  tft.fillRect(0, TITLE_H, CONTENT_W, TITLE_RULE_H, TFT_GREY);
}

void drawTitleBar(RA8875 &tft, const String &title) {
  tft.fillRect(0, 0, CONTENT_W, TITLE_TOP, TFT_BLACK);
  textCenter(tft, TITLE_FONT, 0, 0, CONTENT_W, TITLE_H,
             title, TFT_WHITE, TFT_BLACK);
  tft.fillRect(0, TITLE_H, CONTENT_W, TITLE_RULE_H, TFT_GREY);
}

/***************************************************************************************
   ROW PRIMITIVES
   Core versions accept explicit font and nRows for screens with non-standard layout.
   Convenience wrappers use ROW_FONT. All screens use NR=8 rows.
****************************************************************************************/
void drawChrome(RA8875 &tft, uint8_t row, const char *label,
                       const tFont *font, uint8_t nRows) {
  printDispChrome(tft, font,
                  rowX(), rowYFor(row, nRows), rowW(), rowHFor(nRows),
                  label, COL_LABEL, COL_BACK, COL_NO_BDR);
}

void drawChrome(RA8875 &tft, uint8_t row, const char *label) {
  drawChrome(tft, row, label, ROW_FONT, 8);
}

void drawValue(RA8875 &tft, uint8_t screen, uint8_t row,
                      const char *label, String value,
                      uint16_t fg, uint16_t bg,
                      const tFont *font, uint8_t nRows) {
  RowCache &c = rowCache[screen][row];
  if (c.value == value && c.fg == fg && c.bg == bg) return;
  printValue(tft, font,
             rowX(), rowYFor(row, nRows), rowW(), rowHFor(nRows),
             label, value, fg, bg, COL_BACK,
             printState[screen][row]);
  c.value = value;
  c.fg    = fg;
  c.bg    = bg;
}

/***************************************************************************************
   SCREEN CHROME FUNCTIONS — labels drawn once on transition
****************************************************************************************/


void drawStaticScreen(RA8875 &tft, ScreenType s) {
  tft.fillScreen(TFT_BLACK);
  drawSidebar(tft);

  // Dynamic titles
  if (s == screen_ORB) {
    String orbTitle = String("ORBIT - ") + state.gameSOI;
    drawTitleBar(tft, orbTitle);
  } else if (s == screen_LNDG) {
    drawTitleBar(tft, _lndgReentryMode ? "RE-ENTRY" : "POWERED DESCENT");
  } else if (s == screen_LNCH) {
    drawTitleBar(tft, _lnchOrbitalMode ? "CIRCULARIZATION" : "ASCENT");
    // Draw override indicator in chrome (redrawn every loop in draw function too)
    if (_lnchManualOverride)
      tft.fillCircle(700, 29, 8, TFT_RED);
  } else {
    drawTitleBar(tft, s);
  }

  switch (s) {
    case screen_LNCH: chromeScreen_LNCH(tft); break;
    case screen_APSI: chromeScreen_APSI(tft); break;
    case screen_ORB:  chromeScreen_ORB(tft);  break;
    case screen_ATT:  chromeScreen_ATT(tft);  break;
    case screen_MNVR: chromeScreen_MNVR(tft); break;
    case screen_RNDZ: _rndzChromDrawn = false; chromeScreen_RNDZ(tft); break;
    case screen_DOCK: _dockChromDrawn = false; chromeScreen_DOCK(tft); break;
    case screen_VEH:  chromeScreen_VEH(tft);  break;
    case screen_LNDG: chromeScreen_LNDG(tft); break;
    case screen_ACFT: {
      // ACFT: 8 rows, Roboto_Black_40, section-label-aware geometry
      static const tFont   *F       = &Roboto_Black_40;
      static const uint16_t SECT_W  = 26;
      static const uint16_t AX      = ROW_PAD + SECT_W;
      static const uint16_t AW      = CONTENT_W - AX - ROW_PAD;
      static const uint16_t THIRD_W = AW / 3;
      static const uint16_t HALF_W  = AW / 2;
      uint16_t rowH = acftRowH();

      // Section labels: STATE(rows0-3), AERO(rows5-6)
      // AT(row4) and CT(row7) drawn AFTER their bounding dividers
      drawVerticalText(tft, 0, TITLE_TOP,           SECT_W, rowH*4, &Roboto_Black_16, "STATE", TFT_LIGHT_GREY, TFT_BLACK);
      drawVerticalText(tft, 0, TITLE_TOP + rowH*5,  SECT_W, rowH*2, &Roboto_Black_16, "AERO",  TFT_LIGHT_GREY, TFT_BLACK);

      // Rows 0-3: STATE labels
      const char *stdLabels[4] = { "Alt.Rdr:", "V.Srf:", "IAS:", "V.Vrt:" };
      for (uint8_t r = 0; r < 4; r++)
        printDispChrome(tft, F, AX, acftRowY(r), AW, rowH,
                        stdLabels[r], COL_LABEL, COL_BACK, COL_NO_BDR);

      // Divider after STATE (rows 0-3) — drawn before AT label
      tft.drawLine(0, acftRowY(4)-2, CONTENT_W, acftRowY(4)-2, TFT_GREY);
      tft.drawLine(0, acftRowY(4)-1, CONTENT_W, acftRowY(4)-1, TFT_GREY);
      // AT label drawn after its bounding dividers (+2px offset)
      drawVerticalText(tft, 0, acftRowY(4)+1, SECT_W, rowH-3, &Roboto_Black_16, "AT", TFT_LIGHT_GREY, TFT_BLACK);

      // Row 4: triple split — Hdg | Pitch | Roll
      {
        uint16_t y = acftRowY(4), h = rowH;
        printDispChrome(tft, F, AX,                    y, THIRD_W,   h, "Hdg:",   COL_LABEL, COL_BACK, COL_NO_BDR);
        printDispChrome(tft, F, AX + THIRD_W,          y, THIRD_W,   h, "Pitch:", COL_LABEL, COL_BACK, COL_NO_BDR);
        printDispChrome(tft, F, AX + THIRD_W*2,        y, AW-THIRD_W*2, h, "Roll:", COL_LABEL, COL_BACK, COL_NO_BDR);
        for (int8_t dx = 0; dx <= 1; dx++) {
          tft.drawLine(AX + THIRD_W     + dx, y, AX + THIRD_W     + dx, acftRowY(5) - 1, TFT_GREY);
          tft.drawLine(AX + THIRD_W * 2 + dx, y, AX + THIRD_W * 2 + dx, acftRowY(5) - 1, TFT_GREY);
        }
      }

      // Divider after AT (row 4)
      tft.drawLine(0, acftRowY(5)-2, CONTENT_W, acftRowY(5)-2, TFT_GREY);
      tft.drawLine(0, acftRowY(5)-1, CONTENT_W, acftRowY(5)-1, TFT_GREY);

      // Row 5: half split — AoA | Slip
      {
        uint16_t y = acftRowY(5), h = rowH;
        printDispChrome(tft, F, AX,               y, HALF_W - ROW_PAD, h, "AoA:",  COL_LABEL, COL_BACK, COL_NO_BDR);
        printDispChrome(tft, F, AX + HALF_W + ROW_PAD, y, HALF_W - ROW_PAD, h, "Slip:", COL_LABEL, COL_BACK, COL_NO_BDR);
        for (int8_t dx = -1; dx <= 1; dx++)
          tft.drawLine(AX + HALF_W + dx, y, AX + HALF_W + dx, acftRowY(6) - 1, TFT_GREY);
      }

      // Row 6: half split — Mach | G
      {
        uint16_t y = acftRowY(6), h = rowH;
        printDispChrome(tft, F, AX,               y, HALF_W - ROW_PAD, h, "Mach:", COL_LABEL, COL_BACK, COL_NO_BDR);
        printDispChrome(tft, F, AX + HALF_W + ROW_PAD, y, HALF_W - ROW_PAD, h, "G:",    COL_LABEL, COL_BACK, COL_NO_BDR);
        for (int8_t dx = -1; dx <= 1; dx++)
          tft.drawLine(AX + HALF_W + dx, y, AX + HALF_W + dx, acftRowY(7) - 1, TFT_GREY);
      }

      // Divider after AERO (rows 5-6) — drawn before CT label
      tft.drawLine(0, acftRowY(7)-2, CONTENT_W, acftRowY(7)-2, TFT_GREY);
      tft.drawLine(0, acftRowY(7)-1, CONTENT_W, acftRowY(7)-1, TFT_GREY);
      // CT label drawn after its bounding dividers (+2px offset)
      drawVerticalText(tft, 0, acftRowY(7)+1, SECT_W, rowH-3, &Roboto_Black_16, "CT", TFT_LIGHT_GREY, TFT_BLACK);

      // Row 7: half split — Gear | Brakes
      {
        uint16_t y = acftRowY(7), h = rowH;
        printDispChrome(tft, F, AX,               y, HALF_W - ROW_PAD, h, "Gear:",   COL_LABEL, COL_BACK, COL_NO_BDR);
        printDispChrome(tft, F, AX + HALF_W + ROW_PAD, y, HALF_W - ROW_PAD, h, "Brakes:", COL_LABEL, COL_BACK, COL_NO_BDR);
        for (int8_t dx = -1; dx <= 1; dx++)
          tft.drawLine(AX + HALF_W + dx, y, AX + HALF_W + dx, acftRowY(8) - 1, TFT_GREY);
      }
      break;
    }
    default: break;
  }

  // Invalidate all row cache slots so first updateScreen() draws everything fresh
  for (uint8_t r = 0; r < ROW_COUNT; r++) {
    rowCache[(uint8_t)s][r].value = "\x01";
    rowCache[(uint8_t)s][r].fg    = 0x0001;
    rowCache[(uint8_t)s][r].bg    = 0x0001;
  }
}

void updateScreen(RA8875 &tft, ScreenType s) {
  switch (s) {
    case screen_LNCH: drawScreen_LNCH(tft); break;
    case screen_APSI: drawScreen_APSI(tft); break;
    case screen_ORB:  drawScreen_ORB(tft);  break;
    case screen_ATT:  drawScreen_ATT(tft);  break;
    case screen_MNVR: drawScreen_MNVR(tft); break;
    case screen_RNDZ: drawScreen_RNDZ(tft); break;
    case screen_DOCK: drawScreen_DOCK(tft); break;
    case screen_VEH:  drawScreen_VEH(tft);  break;
    case screen_LNDG: drawScreen_LNDG(tft); break;
    case screen_ACFT: drawScreen_ACFT(tft); break;
    default: break;
  }
}

