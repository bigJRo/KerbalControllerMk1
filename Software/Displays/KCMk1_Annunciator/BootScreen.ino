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
   BS_HOLD  -- pause between printing a check label and its status word (faster pacing)
   BIG_ROW  -- pixel pitch per big (check/summary) row
   SM_ROW   -- pixel pitch per small (metadata) row
   COL1_X   -- x position for check label
   COL2_X   -- x position for status word, computed at runtime from a fixed-width slot
   BS_FONT  -- fine-print metadata font (version / attribution)
   BS_BIG   -- title / check / summary font
****************************************************************************************/
// Tuning aid: when true, the sequence runs slowly and then FREEZES on the finished
// screen until the operator taps, so the layout can be inspected at leisure. Set
// false for the fast production auto-advance.
static const bool     BS_TUNE_PAUSE = true;
static const uint16_t BS_HOLD  = BS_TUNE_PAUSE ? 400 : 110;  // per-line reveal pause
static const uint16_t BIG_ROW  = 40;   // 32px glyph + 8px leading
static const uint16_t SM_ROW   = 20;   // 16px glyph + 4px leading
static const uint16_t COL1_X   = 10;
static const ILI9341_t3_font_t *BS_FONT = &TerminalFont_16;
static const ILI9341_t3_font_t *BS_BIG  = &TerminalFont_32;

// Status column: start of the widest label slot (19 monospace chars) + a gap.
// Computed once in bootSimText() since it depends on the font's glyph advance.
static uint16_t _bs_col2 = 720;


/***************************************************************************************
   INTERNAL HELPER -- delay without any skip logic (no touch sensing per spec)
****************************************************************************************/
static void _bs_wait(uint16_t ms) {
  delay(ms);
}


/***************************************************************************************
   INTERNAL HELPER -- freeze on the finished screen until a deliberate tap (tuning).
   Releases any held/boot-phantom touch first, waits for a fresh press, then waits
   for its release so the same tap isn't consumed as a gesture on the Main screen.
****************************************************************************************/
static void _bs_holdForTouch() {
  while (isTouched())  { delay(10); }   // clear any lingering / boot-phantom touch
  while (!isTouched()) { delay(10); }   // wait for a deliberate press
  while (isTouched())  { delay(10); }   // wait for release
}


/***************************************************************************************
   INTERNAL HELPER -- print a single line at x,y with given font and color
****************************************************************************************/
static void _bs_print(KCM_TFT &tft, const ILI9341_t3_font_t *font, uint16_t x, uint16_t y,
                       const char *text, uint16_t col) {
  tft.setFont(*font);
  tft.setTextColor(col, TFT_BLACK);
  tft.setCursor(x, y);
  tft.print(text);
}


/***************************************************************************************
   INTERNAL HELPER -- big check row: label at COL1_X, staged pause, status at _bs_col2.
   Status colour conveys the result (green OK / red FAIL / yellow non-fatal).
****************************************************************************************/
static void _bs_check(KCM_TFT &tft, uint16_t y,
                       const char *label, const char *status, uint16_t statusCol) {
  tft.setFont(*BS_BIG);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(COL1_X, y);
  tft.print(label);
  _bs_wait(BS_HOLD);
  tft.setTextColor(statusCol, TFT_BLACK);
  tft.setCursor(_bs_col2, y);
  tft.print(status);
}


/***************************************************************************************
   BOOT SIM TEXT
   Terminal-aesthetic POST sequence, enlarged for 1024x600, with real subsystem
   status. sdOK / touchOK come from the actual init calls in setup(). All rendering
   via graphics-mode calls (setFont / setCursor / print).
****************************************************************************************/
void bootSimText(KCM_TFT &tft, bool sdOK, bool touchOK) {

  tft.fillScreen(TFT_BLACK);

  // Status column origin: fixed 19-char label slot in the big monospace font.
  _bs_col2 = COL1_X + getFontStringWidth(BS_BIG, "0000000000000000000") + 20;

  uint16_t y = 8;

  // - Title -
  _bs_print(tft, BS_BIG, COL1_X, y, "KCMk1 ANNUNCIATOR", TFT_WHITE);
  y += BIG_ROW;
  {
    char buf[128];
    snprintf(buf, sizeof(buf),
             "Jeb's Controller Works   v%d.%d.%d   KDC %d.%d.%d   KDA %d.%d.%d",
             SKETCH_VERSION_MAJOR,               SKETCH_VERSION_MINOR,               SKETCH_VERSION_PATCH,
             KDC_VERSION_MAJOR,                  KDC_VERSION_MINOR,                  KDC_VERSION_PATCH,
             KERBAL_DISPLAY_AUDIO_VERSION_MAJOR, KERBAL_DISPLAY_AUDIO_VERSION_MINOR, KERBAL_DISPLAY_AUDIO_VERSION_PATCH);
    _bs_print(tft, BS_FONT, COL1_X, y, buf, TFT_GREY);
  }
  y += SM_ROW + 2;
  tft.fillRect(0, y, KCM_SCREEN_W, 2, TFT_GREY);
  y += 10;
  _bs_wait(BS_HOLD);

  // - Real subsystem checks -
  _bs_check(tft, y, "1) DISPLAY  RA8876",  "OK",                        TFT_GREEN);   y += BIG_ROW;
  _bs_check(tft, y, "2) TOUCH  FT5316",     touchOK ? "OK"   : "FAIL",  touchOK ? TFT_GREEN : TFT_RED);    y += BIG_ROW;
  _bs_check(tft, y, "3) SD CARD  eMMC",     sdOK    ? "OK"   : "NONE",  sdOK    ? TFT_GREEN : TFT_YELLOW); y += BIG_ROW;
  _bs_check(tft, y, "4) AUDIO  TONE/DFP",  "OK",                        TFT_GREEN);   y += BIG_ROW;
  _bs_check(tft, y, "5) I2C SLAVE  0x10",  "OK",                        TFT_GREEN);   y += BIG_ROW;
  _bs_check(tft, y, "6) KSP LINK  SIMPIT",
            demoMode ? "DEMO" : (standaloneMode ? "LOCAL" : "STANDBY"),
            demoMode ? TFT_BLUE : (standaloneMode ? TFT_AQUA : TFT_YELLOW));          y += BIG_ROW + 4;

  tft.fillRect(0, y, KCM_SCREEN_W, 2, TFT_GREY);
  y += 12;

  // - Summary - (touch is the only hard failure; a missing SD card is non-fatal)
  if (touchOK) _bs_print(tft, BS_BIG, COL1_X, y, "SYSTEMS NOMINAL", TFT_GREEN);
  else         _bs_print(tft, BS_BIG, COL1_X, y, "TOUCH FAULT",     TFT_RED);
  y += BIG_ROW;
  _bs_wait(BS_HOLD);
  _bs_print(tft, BS_BIG, COL1_X, y, "Initializing...", TFT_BLUE);
  y += BIG_ROW + 6;

  // - Attribution -
  tft.fillRect(0, y, KCM_SCREEN_W, 2, TFT_GREY);
  y += 8;
  _bs_print(tft, BS_FONT, COL1_X, y, "Jeb's Controller Works  //  C-2026", TFT_GREY);

  if (BS_TUNE_PAUSE) {
    // Tuning: add a "TAP TO CONTINUE" prompt and hold until touched.
    y += SM_ROW + 6;
    _bs_print(tft, BS_BIG, COL1_X, y, "TAP TO CONTINUE", TFT_YELLOW);
    _bs_holdForTouch();
  } else {
    _bs_wait(700);   // production: fast hand-off to the app
  }
}
