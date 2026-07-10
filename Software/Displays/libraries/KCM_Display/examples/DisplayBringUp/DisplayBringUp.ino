/***************************************************************************************
   DisplayBringUp.ino — staged bring-up test for the Kerbal Controller Mk1
   7" TFT Display Driver board (RA8876 / Teensy 4.1, FlexIO3 16-bit 8080).

   PURPOSE
   Validate the KCM display stack in isolation, from the riskiest layer up, with
   verbose Serial logging so a blank panel still tells us how far init got.

   HOW TO USE
   1. Open the Serial Monitor at 115200 baud BEFORE the board finishes booting
      (or press the Teensy reset after connecting).
   2. Start with only STAGE 1 patterns. Confirm each stage on the panel + Serial,
      then enable the next #define.
   3. Report back: what the Serial log says and what the panel shows for each stage.

   STAGES (enable one at a time by uncommenting):
     STAGE 1  (always on): solid-colour fills — validates FlexIO bus + RA8876 init.
     STAGE 2  primitives — rects, lines, circles, triangle (GFX geometry path).
     TEST_KDC_FONTS       — text via the converted ILI9341_t3 fonts (needs
                            KerbalDisplayCommon + the RA8876 GFX/ILI9341_fonts libs).
     TEST_TOUCH           — FT5316 readout: crosshair at the touch point + Serial.
     TEST_SD              — draw a 24-bit BMP from the Teensy 4.1 on-board SD.

   Requires (install into your Arduino libraries path):
     wwatson4506/TeensyRA8876-8080, TeensyRA8876-GFX-Common, PaulStoffregen/ILI9341_fonts,
     plus this repo's KCM_Display, KCMk1_SystemConfig (and KCM_Touch / KerbalDisplayCommon
     for the optional stages).
****************************************************************************************/

// ---- Stage selection ----
#define STAGE_2_PRIMITIVES     // rects/lines/circles/triangle
//#define TEST_KDC_FONTS        // converted ILI9341_t3 fonts
//#define TEST_TOUCH            // FT5316 touch
//#define TEST_SD               // SD BMP

#include <KCM_Display.h>
#include <KCMk1_SystemConfig.h>
#ifdef TEST_KDC_FONTS
  #include <KerbalDisplayCommon.h>   // pulls in the converted fonts (fonts_ili/)
#endif
#ifdef TEST_TOUCH
  #include <KCM_Touch.h>
#endif
#ifdef TEST_SD
  #include <SD.h>
#endif

// Construct the display: (RS/DC, CS, RESET) — data bus + WR/RD owned by FlexIO3.
KCM_TFT tft(KCM_TFT_RS, KCM_TFT_CS, KCM_TFT_RESET);

static void banner(const char *msg) {
  Serial.print(F("[BRINGUP] ")); Serial.println(msg);
}

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && (millis() - t0 < 3000)) {}
  Serial.println();
  banner("KCMk1 7\" TFT bring-up");
  Serial.print(F("[BRINGUP] bus width ")); Serial.print(KCM_TFT_BUS_WIDTH);
  Serial.print(F(" @ ")); Serial.print(KCM_TFT_BUS_SPEED_MHZ); Serial.println(F(" MHz FlexIO3"));
  Serial.print(F("[BRINGUP] pins: CS=")); Serial.print(KCM_TFT_CS);
  Serial.print(F(" RS=")); Serial.print(KCM_TFT_RS);
  Serial.print(F(" RST=")); Serial.print(KCM_TFT_RESET);
  Serial.print(F(" WR=")); Serial.print(KCM_TFT_WR);
  Serial.print(F(" RD=")); Serial.print(KCM_TFT_RD);
  Serial.print(F(" BL=")); Serial.println(KCM_TFT_BL);

  banner("calling kcmDisplayBegin() ...");
  kcmDisplayBegin(tft, 0x0000);   // clear to black
  banner("kcmDisplayBegin() returned");
  Serial.print(F("[BRINGUP] width=")); Serial.print(tft.width());
  Serial.print(F(" height=")); Serial.println(tft.height());
  banner("If the panel is lit but blank/garbled, init reached the MPU but the");
  banner("PLL/SDRAM/timing or bus speed likely need tuning (see PORTING §8).");

#ifdef TEST_TOUCH
  setTouchDebug(true);
  setupTouch();
  banner("touch init done (watch for FT5316 probe result above)");
#endif
#ifdef TEST_SD
  if (SD.begin(KCM_SD_CS)) banner("SD ready");
  else                     banner("SD begin FAILED");
#endif

  banner("setup complete — entering loop");
}

// RGB565 helper so this sketch needs no colour header.
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void loop() {
  struct { const char *name; uint16_t col; } fills[] = {
    { "RED",   rgb565(255,0,0) }, { "GREEN", rgb565(0,255,0) },
    { "BLUE",  rgb565(0,0,255) }, { "WHITE", 0xFFFF }, { "BLACK", 0x0000 },
  };

  // ---- STAGE 1: solid fills ----
  for (auto &f : fills) {
    Serial.print(F("[BRINGUP] fillScreen ")); Serial.println(f.name);
    tft.fillScreen(f.col);
    delay(800);
  }

#ifdef STAGE_2_PRIMITIVES
  banner("STAGE 2: primitives");
  tft.fillScreen(0x0000);
  tft.drawRect(2, 2, tft.width() - 4, tft.height() - 4, 0xFFFF);         // full-screen frame
  for (int i = 0; i < 10; i++)
    tft.fillRect(20 + i * 90, 20, 80, 80, rgb565(25 * i, 0, 255 - 25 * i));
  tft.drawLine(0, 0, tft.width() - 1, tft.height() - 1, rgb565(0,255,0));
  tft.drawLine(tft.width() - 1, 0, 0, tft.height() - 1, rgb565(0,255,0));
  tft.drawCircle(tft.width() / 2, tft.height() / 2, 120, 0xFFFF);
  tft.fillCircle(tft.width() / 2, tft.height() / 2, 60, rgb565(255,128,0));
  tft.fillTriangle(500, 450, 620, 590, 380, 590, rgb565(255,255,0));
  delay(3000);
#endif

#ifdef TEST_KDC_FONTS
  banner("STAGE: fonts");
  tft.fillScreen(0x0000);
  tft.setFont(Roboto_Black_24);
  tft.setTextColor(0xFFFF);
  tft.setCursor(20, 20);
  tft.print("KCMk1 RA8876 1024x600");
  tft.setFont(Roboto_Black_48);
  tft.setTextColor(rgb565(255,0,0));
  tft.setCursor(20, 70);
  tft.print("MASTER ALARM");
  tft.setFont(Roboto_Black_20);
  tft.setTextColor(rgb565(0,255,0));
  tft.setCursor(20, 160);
  tft.print("abcdefg 0123456789 %.-");   // baseline/ascender/descender check
  tft.setFont(TerminalFont_16);
  tft.setTextColor(0xFFFF);
  tft.setCursor(20, 220);
  tft.print("Terminal 8x16: The quick brown fox");
  delay(4000);
#endif

#ifdef TEST_TOUCH
  banner("STAGE: touch — tap the panel (crosshair should track your finger)");
  tft.fillScreen(0x0000);
  tft.drawRect(0, 0, tft.width() - 1, tft.height() - 1, rgb565(0,80,0));
  uint32_t until = millis() + 8000;
  while (millis() < until) {
    if (isTouched()) {
      TouchResult r = readTouch();
      for (uint8_t i = 0; i < r.count; i++) {
        int16_t x = r.points[i].x, y = r.points[i].y;
        tft.drawFastHLine(x - 15, y, 31, 0xFFFF);
        tft.drawFastVLine(x, y - 15, 31, 0xFFFF);
        tft.fillCircle(x, y, 4, rgb565(255,0,0));
        Serial.print(F("[TOUCH] ")); Serial.print(x); Serial.print(','); Serial.println(y);
      }
    }
  }
#endif

#ifdef TEST_SD
  banner("STAGE: SD BMP (/bringup.bmp, 24-bit)");
  tft.fillScreen(0x0000);
  // Uses KerbalDisplayCommon's drawBMP if available; otherwise skip.
  #ifdef TEST_KDC_FONTS
    setupSD();
    drawBMP(tft, "/bringup.bmp", 0, 0);
  #endif
  delay(4000);
#endif
}
