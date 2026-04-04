/***************************************************************************************
   BootScreen.ino -- Boot simulation screen for Kerbal Controller Mk1 Resource Display
   Jurassic Park-themed terminal boot sequence.

   Uses tft.setFont / setCursor / print (GFX graphics mode calls) exclusively.
   The RA8875 setFontScale/print API puts the chip into internal text mode and the
   PaulStoffregen v0.7.11 library does not expose a public graphics-mode restore call,
   so we avoid text mode entirely and stay in graphics mode throughout.

   Call bootSimText() from setup() after setupDisplay(), before initSimpit()/initDemoMode().
****************************************************************************************/
#include "KCMk1_ResourceDisp.h"


/***************************************************************************************
   CONSTANTS
****************************************************************************************/
static const uint16_t BS_HOLD  = 300;
static const uint16_t BS_ROW_H = 18;   // 16px glyph + 2px leading
static const uint16_t BS_COL_X = 4;
static const tFont   *BS_FONT  = &TerminalFont_16;
static const tFont   *BS_BIG   = &TerminalFont_32;


/***************************************************************************************
   INTERNAL HELPERS (#5E — delegate to KDC library)
   Signatures unchanged so call sites in bootSimText() are unmodified.
****************************************************************************************/
static void _bs_wait(uint16_t ms) { delay(ms); }

static void _bs_print(RA8875 &tft, const tFont *font, uint16_t x, uint16_t y,
                      const char *text, uint16_t col) {
  bsPrint(tft, font, x, y, text, col);
}

static uint16_t _bs_line(RA8875 &tft, uint16_t y, const char *text, uint16_t col) {
  return bsLine(tft, BS_FONT, BS_COL_X, y, BS_ROW_H, text, col);
}

static uint16_t _bs_blank(uint16_t y) { return bsBlank(y, BS_ROW_H); }

static uint16_t _bs_big(RA8875 &tft, uint16_t y, const char *text, uint16_t col) {
  return bsBig(tft, BS_BIG, BS_COL_X, y, text, col);
}

static uint16_t _bs_wrap(RA8875 &tft, uint16_t y, const char *text,
                          uint16_t col, uint16_t maxW) {
  return bsWrap(tft, BS_FONT, BS_COL_X, y, BS_ROW_H, text, col, maxW);
}


/***************************************************************************************
   BOOT SIM TEXT
   Jurassic Park-themed terminal sequence.
   All rendering via helpers -- stays in RA8875 graphics mode throughout.
****************************************************************************************/
void bootSimText(RA8875 &tft) {

  tft.fillScreen(TFT_BLACK);
  uint16_t y = 0;
  uint16_t wrapW = 792;  // 800px minus left margin

  // - Header -
  tft.fillRect(0, y, 800, 2, TFT_GREY);
  y += 4;
  {
    char buf[128];   // #4B version string
    snprintf(buf, sizeof(buf),
             "KCMk1-RESDISP  //  Jeb's Controller Works  //  v%d.%d.%d"
             " / KDC %d.%d.%d",
             SKETCH_VERSION_MAJOR, SKETCH_VERSION_MINOR, SKETCH_VERSION_PATCH,
             KDC_VERSION_MAJOR,    KDC_VERSION_MINOR,    KDC_VERSION_PATCH);
    _bs_print(tft, BS_FONT, BS_COL_X, y, buf, TFT_GREY);
  }
  y += BS_ROW_H;
  tft.fillRect(0, y, 800, 2, TFT_GREY);
  y += 4;
  _bs_wait(BS_HOLD);

  // Opening line -- big, white
  y = _bs_big(tft, y, "Pretty sure this is going to work...", TFT_WHITE);
  _bs_wait(BS_HOLD);

  // Access sequence
  y = _bs_line(tft, y, "Access main program...",      TFT_WHITE);  _bs_wait(BS_HOLD);
  y = _bs_line(tft, y, "Access main security...",     TFT_WHITE);  _bs_wait(BS_HOLD);
  y = _bs_line(tft, y, "Access main program grid...", TFT_WHITE);  _bs_wait(BS_HOLD);
  y += 14;  // slightly tighter blank before Nedry

  // Nedry interlude
  y = _bs_line(tft, y, "Uh uh uh! You didn't say the magic word!", TFT_YELLOW);  _bs_wait(BS_HOLD);
  y = _bs_line(tft, y, "Uh uh uh!",                                TFT_YELLOW);  _bs_wait(BS_HOLD);
  y = _bs_line(tft, y, "Uh uh uh! You didn't say the magic word!", TFT_YELLOW);  _bs_wait(BS_HOLD);
  y = _bs_line(tft, y, "Uh uh uh!",                                TFT_YELLOW);  _bs_wait(BS_HOLD);

  y = _bs_line(tft, y, "Damn I hate this hacker crap!", TFT_WHITE);  _bs_wait(BS_HOLD);

  y = _bs_line(tft, y, "Uh uh uh! You didn't say the magic word!", TFT_YELLOW);
  y = _bs_line(tft, y, "Uh uh uh!",                                TFT_YELLOW);  _bs_wait(BS_HOLD);

  // White rabbit
  y = _bs_line(tft, y, "White rabbit object...", TFT_RED);  _bs_wait(BS_HOLD);

  // Access restored
  y = _bs_line(tft, y, "ACCESS RESTORED!", TFT_GREEN);
  y = _bs_blank(y);
  y = _bs_line(tft, y, "Hold on to your butts!", TFT_WHITE);
  y = _bs_wrap(tft, y,
    "We have all the problems of a major theme park and a major zoo and the computer's not even on its feet yet.",
    TFT_WHITE, wrapW);
  y = _bs_wrap(tft, y,
    "Yeah but Jeb, when Pirates of the Caribbean breaks down the pirates don't eat the tourists.",
    TFT_CYAN, wrapW);
  _bs_wait(BS_HOLD);

  // System ready -- big green
  y = _bs_blank(y);
  _bs_big(tft, y, "System Ready", TFT_GREEN);

  _bs_wait(2000);
}
