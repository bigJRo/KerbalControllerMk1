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
  // Backlight on at the configured PWM duty (dimmed from full-on to save power).
  analogWrite(KCM_TFT_BL, (KCM_BL_BRIGHTNESS_PCT * 255) / 100);
}


/***************************************************************************************
   DOUBLE BUFFERING — RA8876 hardware page flip
   The RA8876 has up to 16MB SDRAM; a 1024x600 RGB565 frame is 1,228,800 bytes, so
   two full pages fit with ~13x room to spare. Two independent pointers control it:
     - canvasImageStartAddress()  : where 2D drawing writes ("the canvas")
     - displayImageStartAddress() : which page the panel scans out ("the display")
   Flipping = pointing the display at the page you just finished drawing. The RA8876
   latches the new scan-out base at the next vertical sync, so the flip is tear-free.
   All three calls below are RA8876_common methods inherited by KCM_TFT — no raw
   register access needed.

   Usage (Model A — full redraw per frame):
     KCMDoubleBuffer db;
     kcmDisplayBegin(tft, TFT_BLACK);
     db.begin(tft);
     ... // one-time: draw static background into BOTH pages if desired
     // per frame:
     db.beginFrame(tft);     // draw target -> hidden back page
     drawWholeFrame(tft);    // fillScreen + all widgets (fully define the frame)
     db.flip(tft);           // present it; back/front swap

   NOTE: assumes a landscape rotation (0 or 2). At 90/270 the canvas width and the
   page-size factors would use the swapped dimensions.
****************************************************************************************/
static const uint32_t KCM_FB_PAGE_BYTES = (uint32_t)KCM_SCREEN_W * (uint32_t)KCM_SCREEN_H * 2UL;
static const uint32_t KCM_FB_PAGE0_ADDR = 0UL;
static const uint32_t KCM_FB_PAGE1_ADDR = KCM_FB_PAGE_BYTES;   // 1,228,800 = 0x12C000

class KCMDoubleBuffer {
public:
  // Call once, after kcmDisplayBegin(). Scans out page 0 and leaves the draw canvas
  // on page 0 too, so any pre-flip static setup lands on the visible page.
  void begin(KCM_TFT &tft) {
    _front = KCM_FB_PAGE0_ADDR;
    _back  = KCM_FB_PAGE1_ADDR;
    tft.displayImageWidth(KCM_SCREEN_W);
    tft.displayWindowStartXY(0, 0);
    tft.displayImageStartAddress(_front);
    canvasTo(tft, _front);
  }

  // Point the draw canvas at the hidden (back) page, then draw the frame and flip().
  // The RA8876 latches the scan-out base at the frame boundary, so after a flip the
  // just-freed page is still on screen until the next boundary. Wait out one frame
  // period since the last flip before drawing that page, or we'd be drawing onto the
  // live image (the cause of the continuous flicker). If the app naturally spends a
  // frame period between flips this never stalls.
  void beginFrame(KCM_TFT &tft) {
    while ((uint32_t)(micros() - _lastFlipUs) < KCM_FRAME_PERIOD_US) { /* spin */ }
    canvasTo(tft, _back);
  }

  // Incremental-frame begin: duplicate the currently-displayed (front) page into
  // the hidden (back) page with a hardware BTE block copy, then aim the canvas at
  // the back page. The back page then holds an EXACT copy of the live frame, so an
  // incremental redraw (change-detected against the last presented frame, plus its
  // erase/repair steps) lands on a correct base and the flip presents a complete,
  // tear-free frame — without re-rasterizing the unchanged chrome/text every frame.
  //
  // Same frame-boundary caveat as beginFrame(): wait out one frame period since the
  // last flip so the BTE isn't writing the page that's still scanning out.
  // lastCopyUs records the measured BTE copy time (spin excluded) for diagnostics.
  void beginFrameCopy(KCM_TFT &tft) {
    while ((uint32_t)(micros() - _lastFlipUs) < KCM_FRAME_PERIOD_US) { /* spin */ }
    uint32_t c0 = micros();
    tft.bteMemoryCopy(_front, KCM_SCREEN_W, 0, 0,
                      _back,  KCM_SCREEN_W, 0, 0,
                      KCM_SCREEN_W, KCM_SCREEN_H);
    tft.check2dBusy();          // BTE runs on the 2D engine — wait for the copy
    lastCopyUs = micros() - c0;
    canvasTo(tft, _back);
  }

  // Block until all drawing queued for the back page has actually finished. The
  // RA8876 draw ops (fillRect/drawSquareFill, writeRect) are ASYNC and return
  // before the engine completes, so without this a flip would present a
  // half-rendered page ("incomplete draws"). Call is also safe to use directly.
  void waitDrawComplete(KCM_TFT &tft) {
    tft.checkWriteFifoEmpty();   // memory-write FIFO drained (pixel blits done)
    tft.check2dBusy();           // 2D geometry engine idle (fills/lines done)
  }

  // Present the back page: it becomes the visible front, and the old front becomes
  // the new back. Waits for drawing to complete first so a complete frame is shown.
  void flip(KCM_TFT &tft) {
    waitDrawComplete(tft);
    tft.displayImageStartAddress(_back);
    _lastFlipUs = micros();
    uint32_t tmp = _front; _front = _back; _back = tmp;
  }

  // Redirect all subsequent drawing to `addr`, full-screen canvas + active window.
  void canvasTo(KCM_TFT &tft, uint32_t addr) {
    tft.canvasImageStartAddress(addr);
    tft.canvasImageWidth(KCM_SCREEN_W);
    tft.activeWindowXY(0, 0);
    tft.activeWindowWH(KCM_SCREEN_W, KCM_SCREEN_H);
  }

  uint32_t frontAddr() const { return _front; }
  uint32_t backAddr()  const { return _back;  }

  uint32_t lastCopyUs = 0;   // µs spent in the last beginFrameCopy() BTE copy (diagnostic)

private:
  uint32_t _front      = KCM_FB_PAGE0_ADDR;
  uint32_t _back       = KCM_FB_PAGE1_ADDR;
  uint32_t _lastFlipUs = 0;
};

#endif // KCM_DISPLAY_H
