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
static const uint32_t KCM_FB_PAGE1_ADDR = KCM_FB_PAGE_BYTES;         // 1,228,800 = 0x12C000
static const uint32_t KCM_FB_PAGE2_ADDR = KCM_FB_PAGE_BYTES * 2UL;   // 2,457,600 = 0x258000

// TRIPLE-BUFFERED. Three framebuffer pages (3.5 MB of the 16 MB SDRAM). Each frame
// draws to the page that was displayed TWO flips ago — which, because our frame time
// (~30+ ms: BTE copy + redraw) is well over one panel refresh period, has certainly
// finished scanning out. That lets us drop the fixed "wait one refresh period after a
// flip" spin that a two-page flip needs (to avoid drawing the page still being scanned):
// with a third page there is always an idle page ready, so no wait is required and the
// achievable rate is bounded by the actual redraw cost, not the refresh period.
//
// Kept the class name KCMDoubleBuffer and the begin/beginFrame/beginFrameCopy/flip API
// unchanged so callers don't change.
class KCMDoubleBuffer {
public:
  // Call once, after kcmDisplayBegin(). Displays page 0 and draws to it too, so any
  // pre-flip static setup (boot text) lands on the visible page.
  void begin(KCM_TFT &tft) {
    _page[0] = KCM_FB_PAGE0_ADDR;
    _page[1] = KCM_FB_PAGE1_ADDR;
    _page[2] = KCM_FB_PAGE2_ADDR;
    _dispIdx = 0;
    _drawIdx = 0;
    tft.displayImageWidth(KCM_SCREEN_W);
    tft.displayWindowStartXY(0, 0);
    tft.displayImageStartAddress(_page[_dispIdx]);
    canvasTo(tft, _page[_dispIdx]);
  }

  // The next draw page: the one displayed two flips ago (idle). Never the currently
  // displayed page and never the one from the previous flip.
  uint8_t nextDrawIdx() const { return (uint8_t)((_dispIdx + 1) % 3); }

  // Full-paint frame begin (used on screen entry): aim the canvas at an idle page.
  // The caller then paints the entire screen (fillScreen + chrome + values) and flips.
  // No copy — the whole page is overwritten. No spin — the page is already idle.
  void beginFrame(KCM_TFT &tft) {
    _drawIdx = nextDrawIdx();
    canvasTo(tft, _page[_drawIdx]);
  }

  // Incremental frame begin: BTE-copy the currently displayed page into an idle page,
  // then aim the canvas there. That page then holds an exact copy of the live frame, so
  // a change-detected incremental redraw (plus its erase/repair) lands on a correct base
  // and the flip presents a complete, tear-free frame — without re-rasterizing unchanged
  // chrome/text. No spin: the destination page finished scanning out two flips ago.
  // lastCopyUs records the measured BTE copy time for diagnostics.
  void beginFrameCopy(KCM_TFT &tft) {
    _drawIdx = nextDrawIdx();
    uint32_t c0 = micros();
    tft.bteMemoryCopy(_page[_dispIdx], KCM_SCREEN_W, 0, 0,
                      _page[_drawIdx], KCM_SCREEN_W, 0, 0,
                      KCM_SCREEN_W, KCM_SCREEN_H);
    tft.check2dBusy();          // BTE runs on the 2D engine — wait for the copy
    lastCopyUs = micros() - c0;
    canvasTo(tft, _page[_drawIdx]);
  }

  // Block until all drawing queued for the draw page has actually finished. The RA8876
  // draw ops (fillRect/drawSquareFill, writeRect) are ASYNC and return before the engine
  // completes, so without this a flip would present a half-rendered page.
  void waitDrawComplete(KCM_TFT &tft) {
    tft.checkWriteFifoEmpty();   // memory-write FIFO drained (pixel blits done)
    tft.check2dBusy();           // 2D geometry engine idle (fills/lines done)
  }

  // Present the freshly-drawn page. Waits for drawing to complete first so a complete
  // frame is shown. The RA8876 latches the new scan-out base at the next vsync (tear-
  // free); we don't wait for that latch because the next frame draws a different (idle)
  // page.
  void flip(KCM_TFT &tft) {
    waitDrawComplete(tft);
    tft.displayImageStartAddress(_page[_drawIdx]);
    _dispIdx = _drawIdx;
  }

  // Redirect all subsequent drawing to `addr`, full-screen canvas + active window.
  //
  // NOTE: the RA8876 driver's writeRect()/text-blit path writes to its own public
  // `currentPage` base address (via the BTE MPU-write), NOT to canvasImageStartAddress
  // — that page is normally maintained only by selectScreen(), which we bypass. So we
  // must point currentPage at the same page here, or writeRect-based text would land on
  // a stale page while fillRect/drawLine geometry goes to `addr`, desyncing the buffers
  // (symptom: text flickers every frame while geometry is stable).
  void canvasTo(KCM_TFT &tft, uint32_t addr) {
    tft.canvasImageStartAddress(addr);
    tft.canvasImageWidth(KCM_SCREEN_W);
    tft.currentPage = addr;   // keep writeRect()/text blits on the same page as geometry
    tft.activeWindowXY(0, 0);
    tft.activeWindowWH(KCM_SCREEN_W, KCM_SCREEN_H);
  }

  uint32_t frontAddr() const { return _page[_dispIdx]; }
  uint32_t backAddr()  const { return _page[_drawIdx]; }

  uint32_t lastCopyUs = 0;   // µs spent in the last beginFrameCopy() BTE copy (diagnostic)

private:
  uint32_t _page[3] = { KCM_FB_PAGE0_ADDR, KCM_FB_PAGE1_ADDR, KCM_FB_PAGE2_ADDR };
  uint8_t  _dispIdx = 0;    // page currently latched for scan-out
  uint8_t  _drawIdx = 0;    // page being drawn this frame
};

#endif // KCM_DISPLAY_H
