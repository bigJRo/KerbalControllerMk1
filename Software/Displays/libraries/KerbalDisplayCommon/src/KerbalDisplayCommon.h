#ifndef KERBAL_DISPLAY_COMMON_H
#define KERBAL_DISPLAY_COMMON_H

/***************************************************************************************
   KerbalDisplayCommon Library
   A UI toolkit for RA8875-based touchscreen displays used in Kerbal Controller Mk1.
   Provides button drawing, text rendering, value formatting, and threshold coloring.

   Dependencies:
    - RA8875 library by PaulStoffregen
    - tFont proportional fonts (located in src/fonts/, included automatically)

  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
  Version: 1.0.0
****************************************************************************************/
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <RA8875.h>
#include <Wire.h>


/***************************************************************************************
   DEFINES
****************************************************************************************/
// User-overrideable display settings
#ifndef RA8875_CS
  #define RA8875_CS 10
#endif
#ifndef RA8875_RESET
  #define RA8875_RESET 15
#endif
#ifndef RA8875_DISPLAY_SIZE
  #define RA8875_DISPLAY_SIZE RA8875_800x480
#endif

// User-overrideable SD card settings
#ifndef SD_DETECT_PIN
  #define SD_DETECT_PIN 4
#endif
#ifndef SD_CS_PIN
  #define SD_CS_PIN 5
#endif

// Font includes -- all sizes, located in src/fonts/
#include "fonts/Roboto_Black_12.c"
#include "fonts/Roboto_Black_16.c"
#include "fonts/Roboto_Black_20.c"
#include "fonts/Roboto_Black_24.c"
#include "fonts/Roboto_Black_36.c"
#include "fonts/Roboto_Black_40.c"
#include "fonts/Roboto_Black_48.c"
#include "fonts/Roboto_Black_72.c"
#include "fonts/TerminalFont_16.c"  // IBM CP437 VGA 8x16 -- fixed-width terminal aesthetic
#include "fonts/TerminalFont_32.c"  // IBM CP437 VGA 16x32 -- 2x scaled from TerminalFont_16


/***************************************************************************************
   NO_BORDER SENTINEL
   Use as borderColor parameter in any function to skip drawing a border.
   Value 0x0001 is not used as a real UI color (near-black, not in color palette).
****************************************************************************************/
#define NO_BORDER 0x0001


/***************************************************************************************
   COLOR DEFINITIONS - RGB565 format
   Bits 0..4   -> Blue  0..4
   Bits 5..10  -> Green 0..5
   Bits 11..15 -> Red   0..4
   Ref: https://rgbcolorpicker.com/565
   Ref: https://github.com/newdigate/rgb565_colors
****************************************************************************************/
#define TFT_BLACK        0x0000  /*   0,   0,   0 */
#define TFT_OFF_BLACK    0x2104  /*   4,   8,   4 */
#define TFT_DARK_GREY    0x39E7  /*   7,  15,   7 */
#define TFT_GREY         0x8410  /*  16,  16,  16 */
#define TFT_LIGHT_GREY   0xBDF7  /*  23,  47,  23 */
#define TFT_WHITE        0xFFFF  /*  31,  63,  31 */
#define TFT_GREEN        0x07E0  /*   0,  63,   0 */
#define TFT_DARK_GREEN   0x03E0  /*   0,  31,   0 */
#define TFT_JUNGLE       0x01E0  /*   0,  15,   0 */
#define TFT_RED          0xF800  /*  31,   0,   0 */
#define TFT_MAROON       0x7800  /*  15,   0,   0 */
#define TFT_CORNELL      0xB0E3  /*  22,   7,   3 */
#define TFT_DARK_RED     0x6000  /*  12,   0,   0 */
#define TFT_BLUE         0x001F  /*   0,   0,  31 */
#define TFT_SKY          0x761F  /*  14,  48,  31 */
#define TFT_ROYAL        0x010C  /*   0,   8,  12 */
#define TFT_AQUA         0x5D1C  /*  11,  40,  28 */
#define TFT_NAVY         0x000F  /*   0,   0,  15 */
#define TFT_CYAN         0x07FF  /*   0,  63,  31 */
#define TFT_FRENCH_BLUE  0x347C  /*   6,  35,  28 */
#define TFT_MAGENTA      0xF81F  /*  31,   0,  31 */
#define TFT_PURPLE       0x8010  /*  16,   0,  16 */
#define TFT_VIOLET       0x901A  /*  18,   0,  26 */
#define TFT_YELLOW       0xFDC2  /*  31,  46,   2 */
#define TFT_DULL_YELLOW  0xEEEB  /*  29,  55,  11 */
#define TFT_DARK_YELLOW  0xA500  /*  20,  40,   0 */
#define TFT_OLIVE        0x8400  /*  16,  32,   0 */
#define TFT_BROWN        0x8200  /*  16,  16,  40 */
#define TFT_SILVER       0xC618  /*  24,  48,  24 */
#define TFT_GOLD         0xD566  /*  26,  43,   6 */
#define TFT_ORANGE       0xFBE0  /*  31,  31,   0 */
#define TFT_AIR_SUP_BLUE 0x7517  /*  14,  40,  23 */
#define TFT_NEON_GREEN   0x3FE2  /*   7,  63,   2 */
#define TFT_SAP_GREEN    0x53E5  /*  10,  31,   5 */
#define TFT_INT_ORANGE   0xFA80  /*  31,  20,   0 */
#define TFT_UPS_BROWN    0x6203  /*  12,  16,   3 */


/***************************************************************************************
   BUTTON LABEL STRUCT
   Defines text and colors for a button in both off and on states.
   Note: Δ is stored at 0x94 in Roboto_Black fonts (non-standard encoding) — works
   correctly with these fonts but may break if fonts are regenerated.
****************************************************************************************/
struct ButtonLabel {
  const char *text;
  uint16_t fontColorOff;
  uint16_t fontColorOn;
  uint16_t backgroundColorOff;
  uint16_t backgroundColorOn;
  uint16_t borderColorOff;   // use NO_BORDER to skip border
  uint16_t borderColorOn;    // use NO_BORDER to skip border
};


/***************************************************************************************
   TEXT PRIMITIVE CONSTANTS
****************************************************************************************/
extern const byte TEXT_BORDER;  // horizontal padding from edge, default 8


/***************************************************************************************
   FUNCTION DECLARATIONS
****************************************************************************************/

// --- Setup helper ---
void setupDisplay(RA8875 &tft, uint16_t backColor);

// --- Font measurement ---
int16_t getFontCharWidth(const tFont *font, char c);
int16_t getFontStringWidth(const tFont *font, const char *str);

// --- Button ---
void drawButton(RA8875 &tft, int16_t x, int16_t y, int16_t w, int16_t h,
                const ButtonLabel &label, const tFont *font, bool isOn);

// --- Text primitives ---
void textLeft(RA8875 &tft, const tFont *font, uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
              String value, uint16_t foreColor, uint16_t backColor);
void textRight(RA8875 &tft, const tFont *font, uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
               String value, uint16_t foreColor, uint16_t backColor);
void textCenter(RA8875 &tft, const tFont *font, uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                String value, uint16_t foreColor, uint16_t backColor);

// --- Basic formatters ---
String formatInt(uint16_t value);
String formatFloat(float value, uint8_t decimals);
String formatPerc(uint16_t value);
String formatUnits(uint16_t value, String units);
String formatFloatUnits(float value, uint8_t decimals, String units);

// --- Advanced formatters (KSP telemetry) ---
// Note: formatSep() is a dependency of formatAlt() — keep together
// Note: formatTime() uses Kerbin day = 6 hours
String formatSep(float value);
String formatTime(float timeVal);
String formatAlt(float value);
String twString(uint8_t twIndex, bool physTW);

// --- Threshold color selector ---
void thresholdColor(uint16_t value,
                    uint16_t lowVal,  uint16_t lowColor,  uint16_t lowBack,
                    uint16_t midVal,  uint16_t midColor,  uint16_t midBack,
                                      uint16_t highColor, uint16_t highBack,
                    uint16_t &foreColor, uint16_t &backColor);

/***************************************************************************************
   DISPLAY CACHE
   Hold one DispCache per display block to enable flicker-free updates.
   Pass it to the printDisp overload and the function will skip the redraw entirely
   if the content, colors, and position are unchanged since the last draw.
   Declare in your sketch like:  DispCache altCache;
****************************************************************************************/
struct DispCache {
  String    param      = "";
  String    value      = "";
  uint16_t  paramColor = 0xFFFF;
  uint16_t  valColor   = 0xFFFF;
  uint16_t  valBack    = 0x0000;
  uint16_t  backColor  = 0x0000;
  uint16_t  borderColor= 0x0001;
  uint16_t  x0 = 0, y0 = 0, w = 0, h = 0;
  bool      valid      = false;  // false until first draw
};

// --- Display block functions ---

// Full block draw — use once at setup to draw label, value, and border.
void printDisp(RA8875 &tft, const tFont *font,
               uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
               String param, String value,
               uint16_t paramColor, uint16_t valColor, uint16_t valBack,
               uint16_t backColor, uint16_t borderColor);

// Cached overload — skips redraw entirely if content and colors are unchanged.
void printDisp(RA8875 &tft, const tFont *font,
               uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
               String param, String value,
               uint16_t paramColor, uint16_t valColor, uint16_t valBack,
               uint16_t backColor, uint16_t borderColor,
               DispCache &cache);

// Value-only redraw — use on updates. Clears and redraws only the right-hand
// value region, leaving the param label and border completely untouched.
void printDispChrome(RA8875 &tft, const tFont *font,
                     uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                     String label,
                     uint16_t labelColor, uint16_t backColor,
                     uint16_t borderColor);

void printValue(RA8875 &tft, const tFont *font,
                uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                String param, String value,
                uint16_t valColor, uint16_t valBack,
                uint16_t backColor);

void printName(RA8875 &tft, const tFont *font,
               uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
               String value, uint16_t color, uint16_t backColor,
               uint16_t borderColor, byte maxLength = 30);

void printTitle(RA8875 &tft, const tFont *font,
                uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                String value, uint16_t color, uint16_t backColor,
                uint16_t borderColor);

// --- SD card BMP drawing ---

// Enable verbose library debug output to Serial (I2C scan, touch init steps, etc.).
// Error messages (SD failures, BMP errors) are always printed regardless of this setting.
// Call setKDCDebugMode(debugMode) in setup() after Serial.begin().
void setKDCDebugMode(bool enable);

// Draw a vertical bar graph (bottom-fill). prevVal and newVal in range 0..scale.
// Erases the delta between old and new bar rather than redrawing the full area.
// drawBorder=true draws a white outline around the bar area.
void drawVertBarGraph(RA8875 &tft,
                      uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                      int32_t prevVal, int32_t newVal,
                      uint16_t barColor, bool drawBorder,
                      int32_t scale = 1000);

// Draw a semicircular arc indicator with an erasable needle.
// Arc spans ±90° (left to right). prevVal is erased, curVal is drawn.
// The arc track is redrawn on every call — call from updateScreen*() only on value change.
void drawArcDisplay(RA8875 &tft,
                    int16_t cx, int16_t cy,
                    uint16_t radius, uint16_t needleW,
                    float minVal, float maxVal,
                    float prevVal, float curVal,
                    uint16_t color);

// Draw a vertical percentage axis with major/minor ticks and right-justified labels.
// 0% is at barBottom, 100% is at barTop. axisW px are reserved for labels and ticks.
// The axis line is drawn at x0 + axisW - 1.
void drawLabelledAxis(RA8875 &tft,
                      uint16_t x0, uint16_t axisW,
                      uint16_t barTop, uint16_t barBottom,
                      const tFont *font,
                      uint16_t axisColor, uint16_t backColor);

// Draw a string one character per line within a rectangle — vertical label strip.
// Text is centred horizontally within w and vertically within h.
// The strip is filled with backColor before drawing.
// Use where text rotation is needed but the RA8875 has no native rotation support.
void drawVerticalText(RA8875 &tft,
                      uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                      const tFont *font,
                      const char *text,
                      uint16_t color, uint16_t backColor);

// setupSD() must be called once in setup() before any drawBMP() calls.
// Returns true if the SD card was found and initialised successfully.
// drawBMP() will silently skip and return BMP_ERR_SD_INIT if this was not called
// or if it returned false.
bool setupSD();

// Return codes for drawBMP — check against BMP_OK for success.
// BMP_ERR_NOT_24BIT, BMP_ERR_COMPRESSED, BMP_ERR_DIB, and bit-depth codes are
// retained for compatibility but only 24-bit uncompressed BMPs are supported.
enum BMPResult : uint8_t {
  BMP_OK             = 0,  // success
  BMP_ERR_NO_CARD    = 1,  // SD card not detected at SD_DETECT_PIN
  BMP_ERR_SD_INIT    = 2,  // SD not initialised — call setupSD() in setup()
  BMP_ERR_FILE       = 3,  // file not found or could not be opened
  BMP_ERR_SIGNATURE  = 4,  // not a valid BMP file (missing "BM" header)
  BMP_ERR_DIB        = 5,  // DIB header version too old or unsupported
  BMP_ERR_COMPRESSED = 6,  // compressed BMPs are not supported
  BMP_ERR_DIMENSIONS = 7,  // invalid image dimensions (zero or negative width)
  BMP_ERR_READ       = 8,  // unexpected end of file during read
  BMP_ERR_NOT_24BIT  = 9,  // only 24-bit uncompressed BMPs are supported
};

// Draw a 24-bit uncompressed BMP from the SD card at screen position (x, y).
// setupSD() must have been called and returned true before calling this.
// Errors are logged to Serial with the filename and error code.
BMPResult drawBMP(RA8875 &tft, const char *filename, uint16_t x, uint16_t y);

// =============================================================================
// --- Vessel enums ---
// Ref: https://www.kerbalspaceprogram.com/ksp/api/_vessel_8cs.html
// Ref: https://www.kerbalspaceprogram.com/ksp/api/class_vessel.html
// =============================================================================

enum VesselType : uint8_t {
  type_Debris   =  0,
  type_Object   =  1,
  type_Unknown  =  2,
  type_Probe    =  3,
  type_Relay    =  4,
  type_Rover    =  5,
  type_Lander   =  6,
  type_Ship     =  7,
  type_Plane    =  8,
  type_Station  =  9,
  type_Base     = 10,
  type_EVA      = 11,
  type_Flag     = 12,
  type_SciCtrlr = 13,
  type_SciPart  = 14,
  type_Part     = 15,
  type_GndPart  = 16
};

// Situation flags — bitmask, multiple can be set simultaneously
enum VesselSituation : uint8_t {
  sit_Landed    =   1,
  sit_Splashed  =   2,
  sit_PreLaunch =   4,
  sit_Flying    =   8,
  sit_SubOrb    =  16,
  sit_Orbit     =  32,
  sit_Escaping  =  64,
  sit_Docked    = 128
};

// =============================================================================
// --- Celestial body parameters ---
// =============================================================================
//
// Usage:
//   BodyParams currentBody = getBodyParams("Kerbin");
//   // then access: currentBody.radius, currentBody.surfGrav, etc.
//
//   Call getBodyParams() again whenever Simpit reports a new SOI.
//   If the SOI string is not recognised, all numeric fields are 0 and
//   all string fields are empty — check currentBody.soiName[0] != '\0'
//   to detect a valid result.
//
// All altitude/radius values are in metres.
// surfGrav is in g (1.0 = Kerbin surface gravity).
// surfGrav == 0.0 means the body is not landable (e.g. Kerbol, Jool).
// flyHigh, lowSpace == 0 means the body has no atmosphere.

struct BodyParams {
  const char* soiName;    // Simpit SOI string — matches what Simpit sends
  const char* dispName;   // Display name, uppercase, max 6 chars + null
  float       minSafe;    // Minimum safe altitude (m)
  float       flyHigh;    // Top of "Fly High" biome / atmosphere boundary (m)
  float       lowSpace;   // Low space boundary (m)
  float       highSpace;  // High space boundary (m)
  float       surfGrav;   // Surface gravity in g; 0.0 if not landable
  float       radius;     // Mean body radius (m)
  const char* image;      // SD card BMP path, e.g. "/Kerbin-Display_240x140.bmp"
  const char* cond;       // Atmosphere condition: "Vacuum", "Atmosphere",
                          //   "Breathable", or "Plasma" (Kerbol only)
};

// Returns a copy of the BodyParams for the given Simpit SOI string.
// Returns a zeroed/empty BodyParams if the SOI is not in the table.
//
// NOTE: The const char* fields (soiName, dispName, image, cond) in the returned
// struct are pointers into string literals in the static _bodyTable array.
// They remain valid for the lifetime of the program. Do NOT assign these pointers
// to stack-allocated char arrays or reassign them to point to other strings —
// treat them as read-only. If you need a mutable copy of a string field, use
// strcpy() into a local char buffer of sufficient size.
BodyParams getBodyParams(const String& SOI);

// =============================================================================
// --- Capacitive touch (GSL1680F via I2C on Wire1) ---
// =============================================================================
//
// Usage:
//   // In setup() — one call handles Wire1 init, firmware upload, and chip start:
//   setupTouch();
//
//   // In loop():
//   if (isTouched()) {
//     TouchResult t = readTouch();
//     // t.count = number of active points (1-5)
//     // t.points[0].x, t.points[0].y = first touch coordinates
//   }
//
// Pin defaults (override before including this header if needed):
//   CTP_INT_PIN  22  — GSL1680 interrupt pin (goes HIGH when touched)
//   CTP_WAKE_PIN  3  — GSL1680 wake pin
//   CTP_SDA_PIN  17  — Wire1 SDA (Teensy 4.0 native Wire1 pins)
//   CTP_SCL_PIN  16  — Wire1 SCL (Teensy 4.0 native Wire1 pins)
//
// Wire (pins 18/19) is left free for other I2C devices.
// No external library required — driver is built into KerbalDisplayCommon.

#ifndef CTP_INT_PIN
  #define CTP_INT_PIN   22
#endif
#ifndef CTP_WAKE_PIN
  #define CTP_WAKE_PIN   3
#endif
#ifndef CTP_SDA_PIN
  #define CTP_SDA_PIN   17
#endif
#ifndef CTP_SCL_PIN
  #define CTP_SCL_PIN   16
#endif
#ifndef CTP_MAX_TOUCHES
  #define CTP_MAX_TOUCHES 5
#endif

// Single touch point — x, y pixel coordinates
struct TouchPoint {
  uint16_t x  = 0;
  uint16_t y  = 0;
  uint8_t  id = 0;
};

// Full touch read result — number of active points and their coordinates
struct TouchResult {
  uint8_t    count = 0;
  TouchPoint points[CTP_MAX_TOUCHES];
};


// Initialise Wire1 and the GSL1680F touch controller.
// Uploads the panel firmware over I2C and starts the chip.
// Call once from setup() — no parameters needed.
void setupTouch();

// Returns true if the GSL1680 INT pin is currently HIGH (touch active).
// Polls GPIO directly — no ISR. The INT pin stays HIGH for the full touch duration.
bool isTouched();

// No-op — retained for API compatibility with ISR-based callers.
// Polling has no flag to clear; call sites can be left unchanged.
void clearTouchISR();

// Returns the raw number of times the INT pin ISR has fired since boot.
// Use for diagnostics — confirms whether interrupts are reaching the MCU at all.
uint32_t touchISRCount();

// Reads all active touch points from the GSL1680F.
// Returns a TouchResult with count and coordinates.
// Call when isTouched() returns true.
TouchResult readTouch();


// =============================================================================
// SYSTEM UTILITIES — Teensy 4.0 (IMXRT1062) specific
// Uses hardware registers only — safe to call from library code.
// Do not call from within an ISR.
// =============================================================================

// Performs a soft reboot via the ARM AIRCR register. Does not return.
void executeReboot();

// Shuts down and resets the USB1 controller. Call immediately before
// executeReboot() to ensure a clean USB re-enumeration on the host after reboot.
// Note: USB reconnection without a full reboot is not supported on this hardware.
void disconnectUSB();

#endif // KERBAL_DISPLAY_COMMON_H
