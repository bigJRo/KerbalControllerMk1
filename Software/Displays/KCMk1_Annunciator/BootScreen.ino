/***************************************************************************************
   BootScreen.ino -- Boot simulation screen for Kerbal Controller Mk1 Annunciator
   Renders a terminal-aesthetic BIOS POST sequence using the Terminalia monospace font (OFL).

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
   COL1_X   -- x position for check label
   COL2_X   -- x position for status word, computed at runtime from a fixed-width slot
   BS_FONT  -- fine-print metadata font (version / attribution)
   BS_BIG   -- title / check / summary font
****************************************************************************************/
// Tuning aid: when true, the sequence runs slowly and then FREEZES on the finished
// screen until the operator taps, so the layout can be inspected at leisure. Set
// false for the fast production auto-advance.
static const bool     BS_TUNE_PAUSE = false;
static const uint16_t BS_HOLD  = BS_TUNE_PAUSE ? 400 : 110;  // per-line reveal pause
static const uint16_t BIG_ROW  = 40;   // 32px glyph + 8px leading (checks/title/summary)
static const uint16_t DIAG_ROW = 22;   // 16px glyph + 6px leading (diagnostics)
static const uint16_t COL1_X   = 10;
static const ILI9341_t3_font_t *BS_FONT = &Terminalia_16;
static const ILI9341_t3_font_t *BS_BIG  = &Terminalia_32;

// Status columns: start of the label slot + a gap. Computed at runtime since they
// depend on each font's glyph advance. _bs_col2 = big checks, _bs_col2sm = small
// diagnostics.
static uint16_t _bs_col2   = 720;
static uint16_t _bs_col2sm = 300;


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
   INTERNAL HELPERS -- live diagnostic readouts (real hardware values)
****************************************************************************************/
static uint32_t _bs_freeRAM() {
#if defined(__IMXRT1062__)
  extern unsigned long _heap_end;
  extern char *__brkval;
  return (uint32_t)((char *)&_heap_end - __brkval);   // Teensy 4.x RAM2 heap free
#else
  return 0;
#endif
}

static float _bs_coreTemp() {
#if defined(__IMXRT1062__)
  return tempmonGetTemp();                             // Teensy 4.x on-die temp sensor
#else
  return 0.0f;
#endif
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
   INTERNAL HELPER -- check/readout row: label at COL1_X, staged pause, status at col2.
   Status colour conveys the result (green OK / red FAIL / yellow non-fatal / info).
****************************************************************************************/
static void _bs_check(KCM_TFT &tft, uint16_t y, const ILI9341_t3_font_t *font, uint16_t col2,
                       const char *label, const char *status, uint16_t statusCol) {
  tft.setFont(*font);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setCursor(COL1_X, y);
  tft.print(label);
  _bs_wait(BS_HOLD);
  tft.setTextColor(statusCol, TFT_BLACK);
  tft.setCursor(col2, y);
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

  // Status column origins: big checks use a 19-char slot; small diagnostics an
  // 11-char slot. Both depend on the font's glyph advance, so compute at runtime.
  _bs_col2   = COL1_X + getFontStringWidth(BS_BIG,  "0000000000000000000") + 20;
  _bs_col2sm = COL1_X + getFontStringWidth(BS_FONT, "00000000000")         + 12;

  uint16_t y = 6;
  char buf[96];

  // - Title (big) + version (small, terminal, with branding restored) -
  _bs_print(tft, BS_BIG, COL1_X, y, "KCMk1 ANNUNCIATOR", TFT_WHITE);
  y += BIG_ROW;
  snprintf(buf, sizeof(buf),
           "Jeb's Controller Works   v%d.%d.%d   KDC %d.%d.%d   KDA %d.%d.%d",
           SKETCH_VERSION_MAJOR,               SKETCH_VERSION_MINOR,               SKETCH_VERSION_PATCH,
           KDC_VERSION_MAJOR,                  KDC_VERSION_MINOR,                  KDC_VERSION_PATCH,
           KERBAL_DISPLAY_AUDIO_VERSION_MAJOR, KERBAL_DISPLAY_AUDIO_VERSION_MINOR, KERBAL_DISPLAY_AUDIO_VERSION_PATCH);
  _bs_print(tft, BS_FONT, COL1_X, y, buf, TFT_GREY);
  y += 24;
  tft.fillRect(0, y, KCM_SCREEN_W, 2, TFT_GREY);
  y += 12;
  _bs_wait(BS_HOLD);

  // - Real subsystem checks (big) -
  _bs_check(tft, y, BS_BIG, _bs_col2, "1) DISPLAY  RA8876", "OK",                        TFT_GREEN);   y += BIG_ROW;
  _bs_check(tft, y, BS_BIG, _bs_col2, "2) TOUCH  FT5316",    touchOK ? "OK"   : "FAIL",  touchOK ? TFT_GREEN : TFT_RED);    y += BIG_ROW;
  _bs_check(tft, y, BS_BIG, _bs_col2, "3) SD CARD  eMMC",    sdOK    ? "OK"   : "NONE",  sdOK    ? TFT_GREEN : TFT_YELLOW); y += BIG_ROW;
  _bs_check(tft, y, BS_BIG, _bs_col2, "4) AUDIO  TONE/DFP", "OK",                        TFT_GREEN);   y += BIG_ROW;
  _bs_check(tft, y, BS_BIG, _bs_col2, "5) I2C SLAVE  0x10", "OK",                        TFT_GREEN);   y += BIG_ROW;
  _bs_check(tft, y, BS_BIG, _bs_col2, "6) KSP LINK  SIMPIT",
            demoMode ? "DEMO" : (standaloneMode ? "LOCAL" : "STANDBY"),
            demoMode ? TFT_BLUE : (standaloneMode ? TFT_AQUA : TFT_YELLOW));          y += BIG_ROW + 6;

  tft.fillRect(0, y, KCM_SCREEN_W, 2, TFT_GREY);
  y += 14;

  // - Live diagnostics (real hardware values, small font -- subordinate to checks) -
  snprintf(buf, sizeof(buf), "%lu MHz",
#ifdef F_CPU_ACTUAL
           (unsigned long)(F_CPU_ACTUAL / 1000000UL));
#else
           (unsigned long)(F_CPU / 1000000UL));
#endif
  _bs_check(tft, y, BS_FONT, _bs_col2sm, "CPU  CLOCK", buf, TFT_AQUA); y += DIAG_ROW;

  snprintf(buf, sizeof(buf), "%lu KB", (unsigned long)(_bs_freeRAM() / 1024UL));
  _bs_check(tft, y, BS_FONT, _bs_col2sm, "RAM  FREE",  buf, TFT_AQUA); y += DIAG_ROW;

  {
    float tc = _bs_coreTemp();
    int   ti = (int)tc;
    snprintf(buf, sizeof(buf), "%d.%d C", ti, (int)((tc - ti) * 10.0f));
    _bs_check(tft, y, BS_FONT, _bs_col2sm, "CORE TEMP", buf, TFT_AQUA); y += DIAG_ROW;
  }

  _bs_check(tft, y, BS_FONT, _bs_col2sm, "FW  BUILD", __DATE__, TFT_AQUA); y += DIAG_ROW;

  tft.fillRect(0, y + 2, KCM_SCREEN_W, 2, TFT_GREY);
  y += 14;

  // - Summary - (touch is the only hard failure; a missing SD card is non-fatal)
  if (touchOK) _bs_print(tft, BS_BIG, COL1_X, y, "SYSTEMS NOMINAL", TFT_GREEN);
  else         _bs_print(tft, BS_BIG, COL1_X, y, "TOUCH FAULT",     TFT_RED);
  y += BIG_ROW + 4;

  // - Copyright watermark (always shown) + tap prompt in dev mode -
  _bs_print(tft, BS_FONT, COL1_X, y, "Jeb's Controller Works  //  C-2026", TFT_GREY);
  if (BS_TUNE_PAUSE) {
    _bs_print(tft, BS_BIG, _bs_col2, y - 4, "TAP TO CONTINUE", TFT_YELLOW);
    _bs_holdForTouch();
  } else {
    _bs_wait(700);   // production: fast hand-off to the app
  }
}
