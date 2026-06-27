/***************************************************************************************
   AAA_Screens.ino -- Shared screen infrastructure for Kerbal Controller Mk1 Information Display
   Must compile before Screen_*.ino tabs (AAA_ prefix ensures correct sort order).

   Screen map (mission-phase order):
     0  LNCH  Launch Information (3 modes: Pre-launch, Ascent, Circularization)
     1  ORB   Orbit Information (Apsides + Advanced Elements modes)
     2  PFD   Spacecraft / Primary Flight Display (EADI ball)
     3  MNVR  Maneuver Information
     4  TGT   Target / Rendezvous Information
     5  DOCK  Docking Information
     6  LNDG  Landing Information (2 modes: Powered Descent, Re-entry)
     7  VEH   Vehicle Information
     8  ACFT  Aircraft Information
     9  ROVR  Rover Information

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
const uint16_t CONTENT_W = SCREEN_W - SIDEBAR_W;

const uint16_t TITLE_H = 58;
const uint16_t TITLE_RULE_H = 4;
const uint16_t TITLE_TOP = TITLE_H + TITLE_RULE_H;

const tFont *TITLE_FONT = &Roboto_Black_36;
const tFont *ROW_FONT = &Roboto_Black_40;  // all screen row labels
const uint16_t ROW_PAD = 2;

const uint16_t COL_LABEL = TFT_WHITE;
const uint16_t COL_VALUE = TFT_DARK_GREEN;
const uint16_t COL_BACK = TFT_BLACK;
const uint16_t COL_NO_BDR = TFT_BLACK;

const uint8_t SB_BTN_COUNT = SCREEN_COUNT;
inline uint16_t sbBtnH() {
  return SCREEN_H / SB_BTN_COUNT;
}
inline uint16_t sbBtnY(uint8_t btn) {
  return btn * sbBtnH();
}

const char *const SCREEN_TITLES[SCREEN_COUNT] = {
  "LAUNCH",
  "ORBIT",
  "SPACECRAFT",
  "MANEUVER",
  "TARGET",
  "DOCKING",
  "LANDING",
  "VEHICLE INFO",
  "AIRCRAFT",
  "ROVER"
};

const char *const SCREEN_IDS[SCREEN_COUNT] = {
  "LNCH", "ORB", "PFD", "MNVR", "TGT", "DOCK", "LNDG", "VEH", "ACFT", "ROVR"
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
RowCache rowCache[SCREEN_COUNT][ROW_COUNT];
PrintState printState[SCREEN_COUNT][ROW_COUNT];

/***************************************************************************************
   ROW GEOMETRY
   rowH/rowY computed from per-screen row count so rows fill the content area evenly.
****************************************************************************************/
const uint16_t CONTENT_H = SCREEN_H - TITLE_TOP;  // 420px

inline uint16_t rowHFor(uint8_t nRows) {
  return CONTENT_H / nRows;
}
inline uint16_t rowYFor(uint8_t row, uint8_t nRows) {
  return TITLE_TOP + row * rowHFor(nRows) + ROW_PAD;
}
inline uint16_t rowHInner(uint8_t nRows) {
  return rowHFor(nRows) - ROW_PAD * 2;
}
inline uint16_t rowX() {
  return ROW_PAD;
}
inline uint16_t rowW() {
  return CONTENT_W - ROW_PAD * 2;
}

// All screens use NR=8; call rowYFor(row, NR) and rowHFor(NR) explicitly.

// ACFT geometry: 8 rows, Roboto_Black_40 (48px font in 52px cells — 4px clearance).
// ACFT row geometry: uses rowHFor(8) with a +1 Y offset to match printValue trim behaviour.
inline uint16_t acftRowY(uint8_t row) {
  return TITLE_TOP + row * rowHFor(8) + 1;
}
inline uint16_t acftRowH() {
  return rowHFor(8);
}

/***************************************************************************************
   VALUE FORMATTERS
****************************************************************************************/
String fmtNum(float v) {
  if (fabsf(v) < 0.05f) v = 0.0f;  // snap -0.0 and sub-rounding noise to zero
  if (v >= 1000.0f || v <= -1000.0f) return formatSep(v);
  char buf[16];
  dtostrf(v, 1, 1, buf);
  return String(buf);
}
String fmtUnit(float v, const char *unit) {
  return fmtNum(v) + " " + unit;
}
String fmtMs(float v) {
  return fmtUnit(v, "m/s");
}

// formatTime() removed — formatting improvements merged into library formatTime() (#5C)
// All call sites now call formatTime() directly.

/***************************************************************************************
   KSP BODY COLORS — single source of truth, used by ORB and LNCH (Circularization)

   Both screens render a top-down orbit diagram with body and atmosphere rings, so
   they need to agree on per-body colors. These functions are the canonical lookup;
   callers should never inline their own table. If KSP ever adds a body, update only
   here and both screens update together.

   Returns TFT_GREY for unknown bodies (safe fallback). atmoColor returns 0 (no draw)
   for bodies with no significant atmosphere.
****************************************************************************************/
uint16_t kspBodyColor(const char *name) {
  if (!name || name[0] == '\0') return TFT_GREY;
  if      (strcmp(name, "Kerbol") == 0) return TFT_YELLOW;
  else if (strcmp(name, "Moho")   == 0) return TFT_UPS_BROWN;
  else if (strcmp(name, "Eve")    == 0) return TFT_PURPLE;
  else if (strcmp(name, "Gilly")  == 0) return TFT_TAN;
  else if (strcmp(name, "Kerbin") == 0) return TFT_OCEAN;
  else if (strcmp(name, "Mun")    == 0) return TFT_SILVER;
  else if (strcmp(name, "Minmus") == 0) return TFT_MINT;
  else if (strcmp(name, "Duna")   == 0) return TFT_CORNELL;
  else if (strcmp(name, "Ike")    == 0) return TFT_DARK_GREY;
  else if (strcmp(name, "Dres")   == 0) return TFT_GREY;
  else if (strcmp(name, "Jool")   == 0) return TFT_SAP_GREEN;
  else if (strcmp(name, "Laythe") == 0) return TFT_FRENCH_BLUE;
  else if (strcmp(name, "Vall")   == 0) return TFT_AQUA;
  else if (strcmp(name, "Tylo")   == 0) return TFT_BROWN;
  else if (strcmp(name, "Bop")    == 0) return TFT_DARK_RED;
  else if (strcmp(name, "Pol")    == 0) return TFT_OLIVE;
  else if (strcmp(name, "Eeloo")  == 0) return TFT_WHITE;
  return TFT_GREY;
}

uint16_t kspAtmoColor(const char *name) {
  if (!name || name[0] == '\0') return 0;
  if      (strcmp(name, "Eve")    == 0) return TFT_VIOLET;
  else if (strcmp(name, "Kerbin") == 0) return TFT_AQUA;
  else if (strcmp(name, "Duna")   == 0) return TFT_RED;
  else if (strcmp(name, "Jool")   == 0) return TFT_NEON_GREEN;
  else if (strcmp(name, "Laythe") == 0) return TFT_SKY;
  return 0;
}

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

// Draws a dark-green right-pointing triangle on the left of the title bar.
// Call after drawTitleBar() on any screen with a title-bar tap action.
// Triangle: 20px wide × 24px tall, vertically centred in TITLE_H (58px), x=6.
// Uses ceiling division so every scanline including the tip is at least 1px wide.
static void drawTitleToggleIndicator(RA8875 &tft) {
  const uint16_t tx = 6;
  const uint16_t tw = 20;
  const uint16_t th = 24;
  const uint16_t ty = (TITLE_H - th) / 2;
  const uint16_t half = th / 2;
  for (uint16_t row = 0; row < th; row++) {
    // Ceiling division: (tw * row + half - 1) / half gives at least 1px from row 1
    uint16_t extent = (row <= half)
                        ? (tw * row + half - 1) / half
                        : (tw * (th - 1 - row) + half - 1) / half;
    if (row == 0) extent = 1;  // tip: single pixel
    tft.drawLine(tx, ty + row, tx + extent, ty + row, TFT_DARK_GREEN);
  }
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

// Full-width row overload (uses rowX()/rowW() geometry)
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
  c.fg = fg;
  c.bg = bg;
}

// Split-column overload (#51) — explicit x, w for left/right half-row cells
void drawValue(RA8875 &tft, uint8_t screen, uint8_t row,
               uint16_t x, uint16_t w,
               const char *label, String value,
               uint16_t fg, uint16_t bg,
               const tFont *font, uint8_t nRows) {
  RowCache &c = rowCache[screen][row];
  if (c.value == value && c.fg == fg && c.bg == bg) return;
  printValue(tft, font,
             x, rowYFor(row, nRows), w, rowHFor(nRows),
             label, value, fg, bg, COL_BACK,
             printState[screen][row]);
  c.value = value;
  c.fg = fg;
  c.bg = bg;
}

/***************************************************************************************
   SCREEN CHROME FUNCTIONS — labels drawn once on transition
****************************************************************************************/


void drawStaticScreen(RA8875 &tft, ScreenType s) {
  tft.fillScreen(TFT_BLACK);
  drawSidebar(tft);

  // Dynamic titles
  if (s == screen_ORB) {
    drawTitleBar(tft, _orbAdvancedMode ? "ORBIT ADVANCED" : "ORBIT");
    drawTitleToggleIndicator(tft);
  } else if (s == screen_LNDG) {
    drawTitleBar(tft, _lndgReentryMode ? "RE-ENTRY" : "POWERED DESCENT");
    drawTitleToggleIndicator(tft);
  } else if (s == screen_LNCH) {
    drawTitleBar(tft, _lnchOrbitalMode ? "CIRCULARIZATION" : "ASCENT");
    drawTitleToggleIndicator(tft);
    // Red dot indicates manual override of auto phase switch
    if (_lnchManualOverride)
      tft.fillCircle(706, 29, 6, TFT_RED);
  } else if (s == screen_DOCK) {
    // Vessel name from Simpit reflects the active/combined vessel after docking.
    String dockTitle = String("DOCKING [ ") + state.vesselName + " ]";
    drawTitleBar(tft, dockTitle);
  } else {
    drawTitleBar(tft, s);
  }

  switch (s) {
    case screen_LNCH: chromeScreen_LNCH(tft); break;
    case screen_ORB:
      if (_orbAdvancedMode) chromeScreen_OrbAdv(tft);
      else                  chromeScreen_ORB(tft);
      break;
    case screen_ROVR: chromeScreen_ROVR(tft); break;
    case screen_SCFT: chromeScreen_SCFT(tft); break;
    case screen_MNVR: chromeScreen_MNVR(tft); break;
    case screen_TGT:
      _tgtChromDrawn = false;
      chromeScreen_TGT(tft);
      break;
    case screen_DOCK:
      _dockChromDrawn = false;
      chromeScreen_DOCK(tft);
      break;
    case screen_VEH: chromeScreen_VEH(tft); break;
    case screen_LNDG: chromeScreen_LNDG(tft); break;
    case screen_ACFT: chromeScreen_ACFT(tft); break;
    default: break;
  }

  // Invalidate all row cache slots so first updateScreen() draws everything fresh
  for (uint8_t r = 0; r < ROW_COUNT; r++) {
    rowCache[(uint8_t)s][r].value = "\x01";
    rowCache[(uint8_t)s][r].fg = 0x0001;
    rowCache[(uint8_t)s][r].bg = 0x0001;
  }
}

void updateScreen(RA8875 &tft, ScreenType s) {
  switch (s) {
    case screen_LNCH: drawScreen_LNCH(tft); break;
    case screen_ORB:
      if (_orbAdvancedMode) drawScreen_OrbAdv(tft);
      else                  drawScreen_ORB(tft);
      break;
    case screen_ROVR: drawScreen_ROVR(tft); break;
    case screen_SCFT: drawScreen_SCFT(tft); break;
    case screen_MNVR: drawScreen_MNVR(tft); break;
    case screen_TGT: drawScreen_TGT(tft); break;
    case screen_DOCK: drawScreen_DOCK(tft); break;
    case screen_VEH: drawScreen_VEH(tft); break;
    case screen_LNDG: drawScreen_LNDG(tft); break;
    case screen_ACFT: drawScreen_ACFT(tft); break;
    default: break;
  }
}
