/***************************************************************************************
   CautionWarningPanel Example
   Demonstrates a 4x4 grid of caution/warning buttons using KerbalDisplayCommon.
   Cycles through three states: all off, mixed, inverted.

   Hardware: Teensy 4.0 + RA8875 800x480 display
   Wiring:
    - RA8875 CS    -> Pin 10 (default, override with #define RA8875_CS before include)
    - RA8875 RESET -> Pin 15 (default, override with #define RA8875_RESET before include)
****************************************************************************************/

#include <KerbalDisplayCommon.h>

RA8875 infoDisp = RA8875(RA8875_CS, RA8875_RESET);

// Button layout constants
const int16_t CAUTWARN_W = 126;
const int16_t CAUTWARN_H = 96;
const int16_t GAP_X      = 0;
const int16_t GAP_Y      = 0;
const int16_t OFFSET_X   = 0;
const int16_t OFFSET_Y   = 0;

// Caution/Warning button definitions
// Note: Δ stored at 0x94 in Roboto_Black fonts (non-standard encoding)
ButtonLabel cautWarn[] = {
  { "HIGH SPACE",  TFT_DARK_GREY, TFT_WHITE,      TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY },
  { "LOW SPACE",   TFT_DARK_GREY, TFT_WHITE,      TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY },
  { "FLYING HIGH", TFT_DARK_GREY, TFT_WHITE,      TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY },
  { "FLYING LOW",  TFT_DARK_GREY, TFT_WHITE,      TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY },
  { "ALT",         TFT_DARK_GREY, TFT_DARK_GREY,  TFT_OFF_BLACK, TFT_YELLOW,     TFT_GREY, TFT_GREY },
  { "DESCENT",     TFT_DARK_GREY, TFT_DARK_GREY,  TFT_OFF_BLACK, TFT_YELLOW,     TFT_GREY, TFT_GREY },
  { "GROUND PROX", TFT_DARK_GREY, TFT_WHITE,      TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY },
  { "MECO",        TFT_DARK_GREY, TFT_WHITE,      TFT_OFF_BLACK, TFT_DARK_GREEN, TFT_GREY, TFT_GREY },
  { "HIGH G",      TFT_DARK_GREY, TFT_WHITE,      TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY },
  { "BUS VOLTAGE", TFT_DARK_GREY, TFT_WHITE,      TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY },
  { "HIGH TEMP",   TFT_DARK_GREY, TFT_WHITE,      TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY },
  { "LOW ΔV",      TFT_DARK_GREY, TFT_WHITE,      TFT_OFF_BLACK, TFT_RED,        TFT_GREY, TFT_GREY },
  { "WARP",        TFT_DARK_GREY, TFT_DARK_GREY,  TFT_OFF_BLACK, TFT_YELLOW,     TFT_GREY, TFT_GREY },
  { "ATMO",        TFT_DARK_GREY, TFT_DARK_GREY,  TFT_OFF_BLACK, TFT_YELLOW,     TFT_GREY, TFT_GREY },
  { "O2 PRESENT",  TFT_DARK_GREY, TFT_WHITE,      TFT_OFF_BLACK, TFT_BLUE,       TFT_GREY, TFT_GREY },
  { "CONTACT",     TFT_DARK_GREY, TFT_WHITE,      TFT_OFF_BLACK, TFT_BLUE,       TFT_GREY, TFT_GREY },
};

// Helper to draw all 16 buttons from a state bitmask
void drawCautWarnPanel(uint16_t state) {
  for (int col = 0; col < 4; col++) {
    for (int row = 0; row < 4; row++) {
      int16_t x = col * (CAUTWARN_W + GAP_X) + OFFSET_X;
      int16_t y = row * (CAUTWARN_H + GAP_Y) + OFFSET_Y;
      drawButton(tft, x, y, CAUTWARN_W, CAUTWARN_H,
                 cautWarn[col * 4 + row], &Roboto_Black_24,
                 bitRead(state, col * 4 + row));
    }
  }
}

void setup() {
  setupDisplay(infoDisp, TFT_BLACK);

  // State 1: all off
  drawCautWarnPanel(0x0000);
  delay(2000);

  // State 2: mixed
  drawCautWarnPanel(0b1010010110100101);
  delay(2000);

  // State 3: inverted
  drawCautWarnPanel(~0b1010010110100101);
}

void loop() {
}
