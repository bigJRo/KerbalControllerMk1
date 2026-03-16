/***************************************************************************************
   BootScreen.ino -- Boot simulation screen for Kerbal Controller Mk1 Annunciator
   Renders a terminal-aesthetic BIOS POST sequence using the IBM CP437 TerminalFont.

   Uses tft.setFont / setCursor / print (GFX graphics mode calls) exclusively.
   The RA8875 setFontScale/print API puts the chip into internal text mode and the
   PaulStoffregen v0.7.11 library does not expose a public graphics-mode restore call,
   so we avoid text mode entirely and stay in graphics mode throughout.

   Call bootSimText() from setup() after setupDisplay(), before initSimpit()/initDemoMode().
****************************************************************************************/
#include "KCMk1_Annunciator.h"


/***************************************************************************************
   CONSTANTS
   ROW_H  -- pixel height per text row (font height + leading)
   COL1_X -- x position for check number + label
   COL2_X -- x position for status word (right-aligned area)
   BS_HOLD -- pause between printing a check label and its status word
****************************************************************************************/
static const uint16_t BS_HOLD  = 300;
static const uint16_t ROW_H    = 18;   // 16px glyph + 2px leading
static const uint16_t COL1_X   = 4;
static const uint16_t COL2_X   = 560;
static const tFont   *BS_FONT  = &TerminalFont_16;
static const tFont   *BS_BIG   = &TerminalFont_32;  // 2x scaled for summary lines


/***************************************************************************************
   INTERNAL HELPER -- delay without any skip logic (no touch sensing per spec)
****************************************************************************************/
static void _bs_wait(uint16_t ms) {
  delay(ms);
}


/***************************************************************************************
   INTERNAL HELPER -- print a boot check row
   Prints label at COL1_X, waits BS_HOLD, then prints status at COL2_X.
   Uses tft.setFont / setCursor / print directly -- stays in graphics mode throughout.
****************************************************************************************/
static void _bs_check(RA8875 &tft, uint16_t y,
                       const char *label, uint16_t labelCol,
                       const char *status, uint16_t statusCol) {
  tft.setFont(BS_FONT);
  tft.setTextColor(labelCol, TFT_BLACK);
  tft.setCursor(COL1_X, y);
  tft.print(label);
  _bs_wait(BS_HOLD);
  tft.setTextColor(statusCol, TFT_BLACK);
  tft.setCursor(COL2_X, y);
  tft.print(status);
}


/***************************************************************************************
   INTERNAL HELPER -- print a single line at x,y with given font and color
****************************************************************************************/
static void _bs_print(RA8875 &tft, const tFont *font, uint16_t x, uint16_t y,
                       const char *text, uint16_t col) {
  tft.setFont(font);
  tft.setTextColor(col, TFT_BLACK);
  tft.setCursor(x, y);
  tft.print(text);
}


/***************************************************************************************
   BOOT SIM TEXT
   All rendering via _bs_print() / _bs_check() -- stays in RA8875 graphics mode throughout.
****************************************************************************************/
void bootSimText(RA8875 &tft) {

  tft.fillScreen(TFT_BLACK);

  uint16_t y = 4;

  // - Header -
  tft.fillRect(0, y, 800, 2, TFT_GREY);
  y += 6;
  _bs_print(tft, BS_FONT, COL1_X, y,
            "KCMk1-ANNUNC  //  Jeb's Controller Works  //  BIOS v1.0.003", TFT_GREY);
  y += ROW_H;
  tft.fillRect(0, y, 800, 2, TFT_GREY);
  y += 10;
  _bs_wait(BS_HOLD);

  // - Check 1: Boot Loader -
  _bs_check(tft, y, "1) Boot Loader Complete...",  TFT_WHITE, "OK!",   TFT_GREEN);  y += ROW_H;
  _bs_check(tft, y, "     EEPROM Bank...",         TFT_GREY,  "1 of 4", TFT_WHITE); y += ROW_H + 4;

  // - Check 2: OS Initialisation -
  _bs_check(tft, y, "2) OS Initialization Complete...", TFT_WHITE, "OK!",   TFT_GREEN);  y += ROW_H;
  _bs_check(tft, y, "     Application Copy...",         TFT_GREY,  "1 of 4", TFT_WHITE); y += ROW_H + 4;

  // - Check 3: Memory -
  _bs_check(tft, y, "3) Memory Check Complete...", TFT_WHITE, "OK!", TFT_GREEN); y += ROW_H + 4;

  // - Check 4: Memory Scrub -
  _bs_check(tft, y, "4) Memory Scrub Enabled...", TFT_WHITE, "OK!", TFT_GREEN); y += ROW_H + 4;

  // - Check 5: BIST -
  _bs_check(tft, y, "5) BIST Complete...", TFT_WHITE, "OK!", TFT_GREEN); y += ROW_H + 4;

  // - Check 6: Display Systems -
  _bs_check(tft, y, "6) Display Systems Online...",   TFT_WHITE, "OK!",   TFT_GREEN);  y += ROW_H;
  _bs_check(tft, y, "     RA8875 800x480 @ SPI...",   TFT_GREY,  "READY", TFT_WHITE);  y += ROW_H + 4;

  // - Check 7: Touch Controller -
  _bs_check(tft, y, "7) Touch Controller Online...", TFT_WHITE, "OK!",   TFT_GREEN); y += ROW_H;
  _bs_check(tft, y, "     GSL1680F Capacitive...",   TFT_GREY,  "READY", TFT_WHITE); y += ROW_H + 4;

  // - Check 8: KSP Interface -
  _bs_check(tft, y, "8) KSP Simpit Interface...", TFT_WHITE, "STANDBY", TFT_YELLOW); y += ROW_H + 12;

  // - Summary -
  tft.fillRect(0, y, 800, 2, TFT_GREY);
  y += 10;
  _bs_print(tft, BS_BIG, COL1_X, y, "All Checks Complete...", TFT_WHITE);
  y += 38;   // 32px font + 6px leading
  _bs_wait(BS_HOLD);
  _bs_print(tft, BS_BIG, COL1_X, y, "Initializing...", TFT_BLUE);
  y += 44;   // 32px font + 12px gap before attribution

  // - Attribution -
  tft.fillRect(0, y, 800, 2, TFT_GREY);
  y += 8;
  _bs_print(tft, BS_FONT, COL1_X, y,
            "Jeb's Controller Works  //  C-2026", TFT_GREY);

  _bs_wait(2000);
}
