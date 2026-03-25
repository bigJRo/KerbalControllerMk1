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
   INTERNAL HELPERS
****************************************************************************************/
static void _bs_wait(uint16_t ms) { delay(ms); }

// Print one line at explicit x,y with given font and color (no y advance)
static void _bs_print(RA8875 &tft, const tFont *font, uint16_t x, uint16_t y,
                      const char *text, uint16_t col) {
  tft.setFont(font);
  tft.setTextColor(col, TFT_BLACK);
  tft.setCursor(x, y);
  tft.print(text);
}

// Print one line and advance y by BS_ROW_H
static uint16_t _bs_line(RA8875 &tft, uint16_t y, const char *text, uint16_t col) {
  tft.setFont(BS_FONT);
  tft.setTextColor(col, TFT_BLACK);
  tft.setCursor(BS_COL_X, y);
  tft.print(text);
  return y + BS_ROW_H;
}

// Print a blank line (just advance y)
static uint16_t _bs_blank(uint16_t y) { return y + BS_ROW_H; }

// Print with big font (TerminalFont_32) and advance y by 38
static uint16_t _bs_big(RA8875 &tft, uint16_t y, const char *text, uint16_t col) {
  tft.setFont(BS_BIG);
  tft.setTextColor(col, TFT_BLACK);
  tft.setCursor(BS_COL_X, y);
  tft.print(text);
  return y + 38;
}

// Word-wrap a long string across multiple lines at a given pixel width, advancing y
static uint16_t _bs_wrap(RA8875 &tft, uint16_t y, const char *text,
                          uint16_t col, uint16_t maxW) {
  tft.setFont(BS_FONT);
  tft.setTextColor(col, TFT_BLACK);

  char word[32];
  char line[128] = "";
  uint8_t wi = 0;
  const char *p = text;

  while (true) {
    char c = *p++;
    bool end = (c == '\0');
    bool space = (c == ' ') || end;

    if (space || end) {
      word[wi] = '\0';
      wi = 0;

      // Would appending word overflow the line?
      char test[128];
      if (line[0]) {
        snprintf(test, sizeof(test), "%s %s", line, word);
      } else {
        snprintf(test, sizeof(test), "%s", word);
      }

      if (getFontStringWidth(BS_FONT, test) > maxW) {
        // Flush current line, start new one with this word
        tft.setCursor(BS_COL_X, y);
        tft.print(line);
        y += BS_ROW_H;
        snprintf(line, sizeof(line), "%s", word);
      } else {
        snprintf(line, sizeof(line), "%s", test);
      }

      if (end) {
        if (line[0]) {
          tft.setCursor(BS_COL_X, y);
          tft.print(line);
          y += BS_ROW_H;
        }
        break;
      }
    } else {
      if (wi < (uint8_t)(sizeof(word) - 1)) word[wi++] = c;
    }
  }
  return y;
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
  _bs_print(tft, BS_FONT, BS_COL_X, y,
            "KCMk1-RESDISP  //  Jeb's Controller Works  //  BIOS v1.0.003", TFT_GREY);
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
