#ifndef KCM_DISPLAY_H
#define KCM_DISPLAY_H

/***************************************************************************************
   KCM_Display.h — display driver glue for the Kerbal Controller Mk1 7" TFT
   Display Driver carrier board (KC-01-1911/1912): Teensy 4.1 + RA8876 (1024x600)
   over the FlexIO3 16-bit 8080 parallel bus.

   The carrier board's data bus and /WR,/RD lines are the fixed Teensy 4.1 FlexIO3
   parallel pin set, so the board is driven by the purpose-built library:

       wwatson4506/TeensyRA8876-8080        (FlexIO3 8080 parallel driver: RA8876_t41_p)
       wwatson4506/TeensyRA8876-GFX-Common  (Adafruit_GFX-style graphics API)
       PaulStoffregen/ILI9341_fonts         (ILI9341_t3 proportional fonts)

   Install/vendor those three libraries into the Arduino libraries path before
   building (see PORTING_7inch_TFT.md "Dependencies"). This header gives the rest
   of the firmware a single, controller-agnostic name (KCM_TFT) plus one begin
   helper, so a future controller swap touches this file only.

   The data/WR/RD pins are owned by FlexIO3 and configured inside the driver via
   RA8876_Config_8080.h (defaults already match this board: D0=19, /WR=36, /RD=37).
   Only /CS, RS(=DC) and /RESET are passed in — they were remapped to 34/33/35.

       // In the sketch's AAA_Globals:
       KCM_TFT infoDisp(KCM_TFT_RS, KCM_TFT_CS, KCM_TFT_RESET);

       // In setup():
       kcmDisplayBegin(infoDisp, TFT_BLACK);

   Licensed under the GNU General Public License v3.0 (GPL-3.0).
   Written for Jeb's Controller Works.
****************************************************************************************/

#include <Arduino.h>
#include <KCMk1_SystemConfig.h>

// FlexIO 8080 RA8876 driver (provides RA8876_t41_p, which derives from the
// GFX-Common base and therefore exposes the full Adafruit_GFX-style surface:
// fillScreen/fillRect/drawRect/drawLine/drawCircle/fillCircle/fillTriangle/
// drawPixel/setRotation/width/height, ILI9341_t3 setFont/print/setTextColor/
// setCursor, measureTextWidth/getTextBounds, and writeRect() for BMP blits).
#include <RA8876_t41_p.h>

// Controller-agnostic display type used throughout the firmware.
using KCM_TFT = RA8876_t41_p;

// Bring the panel up: 16-bit FlexIO bus, init at the configured speed, set
// rotation, drive the backlight on, and clear to backColor.
// Construct the KCM_TFT object with (RS/DC, CS, RESET) before calling this.
inline void kcmDisplayBegin(KCM_TFT &tft, uint16_t backColor,
                            uint8_t rotation = KCM_DEFAULT_DISPLAY_ROTATION) {
  pinMode(KCM_TFT_BL, OUTPUT);
  digitalWrite(KCM_TFT_BL, LOW);              // keep backlight off during init
  tft.setBusWidth(KCM_TFT_BUS_WIDTH);        // MUST precede begin()
  tft.begin(KCM_TFT_BUS_SPEED_MHZ);
  tft.setRotation(rotation);
  tft.fillScreen(backColor);
  digitalWrite(KCM_TFT_BL, HIGH);            // backlight on once the panel is up
}

#endif // KCM_DISPLAY_H
