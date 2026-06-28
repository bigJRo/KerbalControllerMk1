#ifndef KCM_TOUCH_H
#define KCM_TOUCH_H

/***************************************************************************************
   KCM_Touch — FT5316 (FT5x06-family) capacitive touch driver for the
   Kerbal Controller Mk1 7" TFT Display Driver board (KC-01-1911).

   Replaces the rev-1 GSL1680F driver that lived inside KerbalDisplayCommon. The
   public surface is kept API-compatible (TouchPoint / TouchResult / setupTouch /
   isTouched / readTouch / clearTouchISR / touchISRCount) so the panel sketches'
   TouchEvents code ports unchanged.

   BUS: the FT5316 hangs off the "local" I2C net (SCL_LOCAL/SDA_LOCAL = Teensy
   pins 4/5). Those are NOT a hardware I2C bus on the Teensy 4.1 (Wire=18/19,
   Wire1=16/17, Wire2=24/25 are all consumed by the parallel data bus and the
   module bus), so this driver bit-bangs an I2C master. The board provides 10k
   pull-ups (R3/R4), so the lines are driven open-drain (release-high / pull-low).

   FT5316 protocol (7-bit address 0x38):
     reg 0x02  TD_STATUS   — low nibble = number of active touch points (0..5)
     reg 0x03  P1_XH       — [7:6]=event, [3:0]=X[11:8]
     reg 0x04  P1_XL       — X[7:0]
     reg 0x05  P1_YH       — [7:4]=touch id, [3:0]=Y[11:8]
     reg 0x06  P1_YL       — Y[7:0]
     ...each further point is +6 bytes; registers auto-increment on read.

   Orientation: confirmed against the BuyDisplay ER-TFT070A2-6 vendor demo, which
   maps raw touch to display pixels as (LCD_W - x, LCD_H - y) — i.e. BOTH axes
   inverted, no swap. Defaults below reflect that. The vendor demo also confirms
   no FT5316 init writes are needed (reset pulse only) and that CTP_INT is
   active-LOW (low while a touch is present) — this driver polls TD_STATUS over
   I2C instead, so it does not depend on the INT line.

   Licensed under the GNU General Public License v3.0 (GPL-3.0).
   Written for Jeb's Controller Works.
****************************************************************************************/

#include <Arduino.h>
#include <KCMk1_SystemConfig.h>

#ifndef CTP_MAX_TOUCHES
  #define CTP_MAX_TOUCHES 5
#endif

// --- Orientation mapping (tune on hardware) ---
#ifndef KCM_CTP_SWAP_XY
  #define KCM_CTP_SWAP_XY 0
#endif
#ifndef KCM_CTP_INVERT_X
  #define KCM_CTP_INVERT_X 1   // confirmed from vendor demo (display = LCD_W - rawX)
#endif
#ifndef KCM_CTP_INVERT_Y
  #define KCM_CTP_INVERT_Y 1   // confirmed from vendor demo (display = LCD_H - rawY)
#endif

struct TouchPoint {
  uint16_t x  = 0;
  uint16_t y  = 0;
  uint8_t  id = 0;
};

struct TouchResult {
  uint8_t    count = 0;
  TouchPoint points[CTP_MAX_TOUCHES];
};

// Initialise the bit-banged I2C bus and reset the FT5316. Call once in setup().
void setupTouch();

// True if at least one touch point is currently active. Polls FT5316 TD_STATUS
// over I2C (does not rely on the INT pin polarity/mode being configured).
bool isTouched();

// Read all active touch points (coordinates already mapped to display space).
TouchResult readTouch();

// API-compat no-ops / diagnostics carried over from the ISR-based GSL1680 driver.
void     clearTouchISR();
uint32_t touchISRCount();

// Enable verbose touch debug prints to Serial (I2C probe, raw coords).
void setTouchDebug(bool enable);

#endif // KCM_TOUCH_H
