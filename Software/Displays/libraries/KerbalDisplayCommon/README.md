# KerbalDisplayCommon

**Kerbal Controller Mk1 — Shared Display Library** · v2.1.0
UI toolkit for RA8875-based touchscreen display panels used in KSP controller builds.
Part of the KCMk1 controller system.

---

## Overview

KerbalDisplayCommon provides the shared display firmware core for KCMk1 display panels. It abstracts the RA8875 display driver into higher-level UI primitives — buttons, text blocks, value formatters, threshold colouring, BMP drawing, and capacitive touch — so that sketch code can focus on layout and telemetry logic rather than display housekeeping.

The library is designed to be used alongside KerbalDisplayAudio and KerbalSimpit in multi-tab Arduino sketches targeting the Teensy 4.0.

---

## Hardware

| Component | Part | Interface |
|-----------|------|-----------|
| Microcontroller | Teensy 4.0 | — |
| Display | RA8875 800×480 TFT | SPI |
| Touch controller | GSL1680F capacitive | Wire1 (I2C) |
| SD card | SD (on RA8875 board) | SPI |

### Pin Assignments

Default pin assignments are defined in the library header and can be overridden in the sketch before the `#include`.

| Pin | Define | Default | Function |
|-----|--------|---------|----------|
| 10 | `RA8875_CS` | `10` | RA8875 SPI chip select |
| 15 | `RA8875_RESET` | `15` | RA8875 reset |
| 5 | `SD_CS_PIN` | `5` | SD card SPI chip select |
| 4 | `SD_DETECT_PIN` | `4` | SD card detect (active-LOW) |
| 16 | `CTP_SCL_PIN` | `16` | GSL1680F SCL (Wire1) |
| 17 | `CTP_SDA_PIN` | `17` | GSL1680F SDA (Wire1) |
| 3 | `CTP_WAKE_PIN` | `3` | GSL1680F wake |
| 22 | `CTP_INT_PIN` | `22` | GSL1680F interrupt (HIGH when touched) |

To override any of these, define them before including the library:

```cpp
#define RA8875_CS 9
#define RA8875_RESET 14
#include <KerbalDisplayCommon.h>
```

---

## Dependencies

| Library | Version | Notes |
|---------|---------|-------|
| RA8875 (PaulStoffregen) | 0.7.11 | Display driver — do not upgrade without testing; text mode API changed in later versions |
| SPI | — | Included automatically with RA8875 |
| SD | — | Teensy bundled version |
| Wire | — | Teensy bundled version |

---

## Configuration

| Constant | Default | Description |
|----------|---------|-------------|
| `RA8875_DISPLAY_SIZE` | `RA8875_800x480` | Display resolution passed to `tft.begin()` |
| `SD_DETECT_PIN` | `4` | Card detect pin — assumed active-LOW |
| `SD_CS_PIN` | `5` | SD SPI chip select |
| `CTP_MAX_TOUCHES` | `5` | Maximum simultaneous touch points reported |
| `NO_BORDER` | `0x0001` | Pass as `borderColor` to skip border drawing |
| `TEXT_BORDER` | `8` | Horizontal pixel padding used by all text functions |

---

## Features

### Display Setup

`setupDisplay(tft, backColor)` — initialises the RA8875, fills the screen with `backColor`, and prints a ready message to Serial if debug mode is enabled. Call once from `setup()` before any drawing functions.

`setKDCDebugMode(bool)` — enables or disables verbose Serial output from the library (touch init steps, I2C scan, BMP success messages). Error messages (SD failures, BMP errors) are always printed regardless of this setting. Call after `Serial.begin()`.

### SD Card and BMP Drawing

`setupSD()` — initialises the SD card. Must be called once from `setup()` before any `drawBMP()` calls. Checks `SD_DETECT_PIN` (active-LOW), then calls `SD.begin()`. Returns `true` on success.

`drawBMP(tft, filename, x, y)` — draws a 24-bit uncompressed BMP from the SD card at screen position `(x, y)`. Only 24-bit BI_RGB (uncompressed) files are supported. Both bottom-up (standard) and top-down BMPs are handled. On `malloc` success all rows are read sequentially for maximum throughput; on `malloc` failure rows are read individually. Image dimensions are clamped to 800×480 before stack allocation. Returns a `BMPResult` error code on failure, also printed to Serial with the filename. On any mid-read error the RA8875 active window is restored before returning.

**BMPResult codes:** `BMP_OK`, `BMP_ERR_NO_CARD`, `BMP_ERR_SD_INIT`, `BMP_ERR_FILE`, `BMP_ERR_SIGNATURE`, `BMP_ERR_DIB`, `BMP_ERR_COMPRESSED`, `BMP_ERR_DIMENSIONS`, `BMP_ERR_READ`, `BMP_ERR_NOT_24BIT`.

`drawStandbySplash(tft)` — convenience wrapper: calls `setXY(0,0)`, `fillScreen(BLACK)`, then `drawBMP("/StandbySplash_800x480.bmp", 0, 0)`. Used by all KCMk1 panels for their shared standby screen. `setupSD()` must have been called first.

### Capacitive Touch

`setupTouch()` — initialises Wire1 and the GSL1680F touch controller. Uploads the panel firmware over I2C (required on every power cycle) and starts the chip. Call once from `setup()`.

`isTouched()` — polls the `CTP_INT_PIN` GPIO directly and returns `true` if the pin is HIGH. The GSL1680F holds INT HIGH for the full duration of a touch, so polling is reliable without an ISR. No I2C read is performed.

`readTouch()` — reads all active touch points from the GSL1680F. Returns a `TouchResult` struct with `count` and `points[]` (each point has `x`, `y`, `id`). Call when `isTouched()` returns `true`.

`clearTouchISR()` — no-op retained for API compatibility. The library previously used a falling-edge ISR; it now uses GPIO polling. Call sites can be left unchanged.

`touchISRCount()` — returns a running count of `isTouched()` calls that returned `true`. Retained for diagnostics.

### Buttons

`drawButton(tft, x, y, w, h, label, font, isOn)` — draws a filled rectangle with centred, word-wrapped text in on or off state. Colours and text are taken from a `ButtonLabel` struct. After drawing, the RA8875 active window is explicitly restored to full-screen so subsequent drawing calls are not clipped to the button bounds.

**`ButtonLabel` struct:**

```cpp
struct ButtonLabel {
  const char *text;
  uint16_t fontColorOff;
  uint16_t fontColorOn;
  uint16_t backgroundColorOff;
  uint16_t backgroundColorOn;
  uint16_t borderColorOff;   // use NO_BORDER to skip border
  uint16_t borderColorOn;    // use NO_BORDER to skip border
};
```

**Note:** `Δ` is stored at `0x94` in the bundled Roboto Black fonts (non-standard encoding). This works correctly with the included fonts but may break if fonts are regenerated from a different tool.

### Text Primitives

Three core rendering functions — all other display block functions are built on these. All `value` parameters are `const String &`.

`textLeft(tft, font, x0, y0, w, h, value, foreColor, backColor)` — left-justified, vertically centred.

`textRight(tft, font, x0, y0, w, h, value, foreColor, backColor)` — right-justified, vertically centred.

`textCenter(tft, font, x0, y0, w, h, value, foreColor, backColor)` — horizontally and vertically centred.

### Display Blocks

Higher-level functions that combine text rendering, background fill, and optional border into a single call. All `param` and `value` string parameters are `const String &`.

`printDisp(tft, font, x0, y0, w, h, param, value, paramColor, valColor, valBack, backColor, borderColor, ps)` — draws a parameter label left-justified and a value right-justified in a rectangle, with flicker-free rendering via `PrintState &ps`. A cached overload is available: pass a `DispCache &cache` before `PrintState &ps` to skip the redraw entirely when content and colours are unchanged since the last draw.

`printDispChrome(tft, font, x0, y0, w, h, label, labelColor, backColor, borderColor)` — draws only the static label and border, leaving the value region untouched. Call once at screen entry; follow with `printValue()` on each update.

`printValue(tft, font, x0, y0, w, h, param, value, valColor, valBack, backColor, ps)` — redraws only the right-hand value region, leaving the param label and border completely untouched. Uses `PrintState &ps` to render new text first then erase any trailing pixels from a previously wider value — eliminates the blank-frame flicker of a full clear-before-draw approach. Use on per-frame updates.

`printName(tft, font, x0, y0, w, h, value, color, backColor, borderColor, maxLength=30)` — left-justified label, truncated to `maxLength` characters.

`printTitle(tft, font, x0, y0, w, h, value, color, backColor, borderColor)` — horizontally centred title string.

**`PrintState` struct** — tracks the pixel width, background colour, and font height of the previous render so `printValue` and `printDisp` can draw text first then clean up trailing pixels, avoiding a blank frame. Declare one `PrintState` per logical display slot:

```cpp
PrintState altState;                          // single slot
PrintState rowState[SCREEN_COUNT][ROW_COUNT]; // array for multi-slot layouts
```

**`DispCache` struct** — holds a full snapshot of the last drawn content (value string, all colours, position). Pass to the cached `printDisp` overload to skip the redraw entirely when nothing has changed. Declare alongside `PrintState`:

```cpp
DispCache  altCache;
PrintState altState;
```

### Threshold Colour

Two overloads select foreground and background colours based on two threshold values. Output parameters `foreColor` and `backColor` are set by the function. The colour returned for each band is: `value < lowVal` → low colours; `value < midVal` → mid colours; `value >= midVal` → high colours.

```cpp
// Integer overload
void thresholdColor(uint16_t value,
                    uint16_t lowVal, uint16_t lowColor, uint16_t lowBack,
                    uint16_t midVal, uint16_t midColor, uint16_t midBack,
                                     uint16_t highColor, uint16_t highBack,
                    uint16_t &foreColor, uint16_t &backColor);

// Float overload — clamps value to [0, 65535] before delegating to uint16_t overload
void thresholdColor(float value,
                    float lowVal, uint16_t lowColor, uint16_t lowBack,
                    float midVal, uint16_t midColor, uint16_t midBack,
                                  uint16_t highColor, uint16_t highBack,
                    uint16_t &foreColor, uint16_t &backColor);
```

### Formatters

**Basic:**
`formatInt(value)`, `formatFloat(value, decimals)`, `formatPerc(value)` (appends `%`), `formatUnits(value, units)`, `formatFloatUnits(value, decimals, units)`.

**KSP telemetry:**

`formatSep(value)` — comma-separated float. For values ≥ 1000 the decimal part is dropped and a thousands separator is inserted (e.g. `1234.5` → `"1,234"`). Required dependency of `formatAlt()`.

`formatTime(timeVal)` — Kerbin time string using `int64_t` intermediates (no overflow beyond ~24.8 Kerbin days). Output examples: `"1 d: 02 h: 30 m: 15 s"`, `"02 h: 30 m: 15 s"`, `"30 m: 15 s"`, `"15 s"`. Uses Kerbin day = 6 hours. Sign prefix (`-`) is applied for negative values.

`formatAlt(value)` — auto-scaling altitude string. Scales through m / km / Mm / Gm.

`twString(twIndex, physTW)` — human-readable time warp rate string (e.g. `"100x"`, `"PHYS-2x"`). Normal indices 0–7; physics indices 1–3.

### Font Measurement

`getFontCharWidth(font, c)` — pixel width of a single character in a proportional tFont.

`getFontStringWidth(font, str)` — total pixel width of a string in a proportional tFont.

### Graphical Widgets

`drawVertBarGraph(tft, x0, y0, w, h, prevVal, newVal, barColor, drawBorder, scale=1000)` — vertical bar graph, bottom-fill, values 0–scale. Erases only the changed segment rather than redrawing the full bar. Call from `updateScreen*()` only when the value changes; update `prevVal` in the caller after each call.

`drawArcDisplay(tft, cx, cy, radius, needleW, minVal, maxVal, prevVal, curVal, color)` — semicircular arc indicator spanning 180 degrees. Erases the previous needle position before drawing the new one. The arc track is redrawn on every call.

`drawLabelledAxis(tft, x0, axisW, barTop, barBottom, font, axisColor, backColor)` — draws a vertical percentage axis with major ticks every 10% and minor ticks every 5%. Labels (0%, 50%, 100%) are right-justified within `axisW` pixels. `0%` is at `barBottom`, `100%` is at `barTop`. The axis line is drawn at `x0 + axisW - 1`.

`drawVerticalText(tft, x0, y0, w, h, font, text, color, backColor)` — draws a string one character per line within a rectangle, creating a vertical label strip. Characters are centred horizontally within `w` and the full strip is centred vertically within `h`. The strip is filled with `backColor` before drawing. Used where text rotation is needed since the RA8875 has no native rotation support.

### Boot Screen Helpers

Shared terminal-aesthetic rendering primitives used by all KCMk1 panel boot sequences. All functions operate in RA8875 graphics mode (never text mode). The sketch defines its own local font and column constants and passes them through.

`bsPrint(tft, font, x, y, text, col)` — print text at explicit coordinates with no y advance.

`bsLine(tft, font, col_x, y, rowH, text, col)` — print one line at column `col_x` and advance `y` by `rowH`. Returns new `y`.

`bsBig(tft, font, col_x, y, text, col)` — print with a double-height font and advance `y` by 38 px. Returns new `y`.

`bsBlank(y, rowH)` — advance `y` by `rowH` without drawing (blank line). Returns new `y`.

`bsWrap(tft, font, col_x, y, rowH, text, col, maxW)` — word-wrap `text` across multiple lines within `maxW` pixels, advancing `y` by `rowH` per line. Returns new `y`.

`bsShuffle(arr, n)` — Fisher-Yates in-place shuffle of a `uint8_t` index array of length `n`. The caller must have called `randomSeed()` before use.

### Celestial Bodies

`getBodyParams(SOI)` — looks up a Simpit SOI string in the built-in body table and returns a `BodyParams` struct. Returns a zeroed/empty struct if the SOI is not recognised — check `soiName[0] != '\0'` to detect a valid result. Call whenever Simpit reports a new SOI.

**`BodyParams` fields:**

| Field | Type | Description |
|-------|------|-------------|
| `soiName` | `const char *` | Simpit SOI string key |
| `dispName` | `const char *` | Display name |
| `image` | `const char *` | SD card BMP path |
| `cond` | `const char *` | Surface condition: `"Vacuum"`, `"Atmosphere"`, `"Breathable"`, `"Plasma"` |
| `minSafe` | `float` | Minimum safe orbit altitude (m) |
| `flyHigh` | `float` | Low/high atmosphere science biome boundary (m); 0 if no atmosphere |
| `lowSpace` | `float` | Atmosphere top / low-space threshold (m); 0 if no atmosphere |
| `highSpace` | `float` | High-space threshold (m) |
| `reentryAlt` | `float` | Committed reentry altitude (m); 0 if no atmosphere |
| `soiAlt` | `double` | SOI radius (m); `DBL_MAX` for Kerbol |
| `radius` | `float` | Body radius (m) |
| `gravity` | `float` | Surface gravity (m/s²) |
| `escapeVelocity` | `float` | Escape velocity (m/s) |
| `synchronousOrbit` | `float` | Synchronous orbit altitude (m); 0 if not applicable |
| `synodicPeriod` | `float` | Synodic period (s); 0 if not applicable |
| `orbitInclination` | `float` | Orbital inclination (degrees) |
| `hasAtmo` | `bool` | True if body has atmosphere |
| `hasO2` | `bool` | True if atmosphere is breathable |
| `hasSurface` | `bool` | True if body has a landable surface |
| `highQThreshold` | `float` | Dynamic pressure threshold for HIGH_Q warning (Pa); 0 = suppressed |

Bodies in the table: Kerbol, Moho, Eve, Gilly, Kerbin, Mun, Minmus, Duna, Ike, Dres, Jool, Laythe, Vall, Tylo, Bop, Pol, Eeloo (all 17 stock KSP1 bodies with wiki-canonical values).

The pointer fields `soiName`, `dispName`, `image`, and `cond` point into static string literals and remain valid for the lifetime of the program. Treat as read-only.

### System Utilities

`executeReboot()` — soft reboot via ARM AIRCR register. Does not return. Teensy 4.0 specific; do not call from within an ISR.

`disconnectUSB()` — shuts down and resets the USB1 controller. Call immediately before `executeReboot()` for a clean USB re-enumeration on the host.

---

## Fonts

All Roboto Black sizes (12, 16, 20, 24, 36, 40, 48, 72 px) and IBM CP437 terminal fonts (TerminalFont_16 at 8×16 px, TerminalFont_32 at 16×32 px) are included automatically from `src/fonts/` via `#include` in the library header. No additional installation step is needed.

Font objects are named `Roboto_Black_24`, `TerminalFont_16`, etc. Pass a pointer when calling library functions: `&Roboto_Black_24`.

---

## Colour Palette

All colours are RGB565 format with `TFT_` prefix, defined in `KerbalDisplayCommon.h`.

| Name | Value | Description |
|------|-------|-------------|
| `TFT_BLACK` | `0x0000` | Black |
| `TFT_OFF_BLACK` | `0x2104` | Near black |
| `TFT_DARK_GREY` | `0x39E7` | Dark grey |
| `TFT_GREY` | `0x8410` | Mid grey |
| `TFT_LIGHT_GREY` | `0xBDF7` | Light grey |
| `TFT_WHITE` | `0xFFFF` | White |
| `TFT_GREEN` | `0x07E0` | Green |
| `TFT_DARK_GREEN` | `0x03E0` | Dark green |
| `TFT_JUNGLE` | `0x01E0` | Deep jungle green |
| `TFT_NEON_GREEN` | `0x3FE2` | Neon green |
| `TFT_SAP_GREEN` | `0x53E5` | Sap green |
| `TFT_RED` | `0xF800` | Red |
| `TFT_MAROON` | `0x7800` | Maroon |
| `TFT_DARK_RED` | `0x6000` | Dark red |
| `TFT_CORNELL` | `0xB0E3` | Cornell red |
| `TFT_BLUE` | `0x001F` | Blue |
| `TFT_NAVY` | `0x000F` | Navy |
| `TFT_ROYAL` | `0x010C` | Royal blue (deep) |
| `TFT_SKY` | `0x761F` | Sky blue |
| `TFT_FRENCH_BLUE` | `0x347C` | French blue |
| `TFT_AIR_SUP_BLUE` | `0x7517` | Air superiority blue |
| `TFT_AQUA` | `0x5D1C` | Aqua |
| `TFT_CYAN` | `0x07FF` | Cyan |
| `TFT_MAGENTA` | `0xF81F` | Magenta |
| `TFT_PURPLE` | `0x8010` | Purple |
| `TFT_VIOLET` | `0x901A` | Violet |
| `TFT_YELLOW` | `0xFDC2` | Yellow |
| `TFT_DULL_YELLOW` | `0xEEEB` | Dull yellow |
| `TFT_DARK_YELLOW` | `0xA500` | Dark yellow |
| `TFT_OLIVE` | `0x8400` | Olive |
| `TFT_ORANGE` | `0xFBE0` | Orange |
| `TFT_INT_ORANGE` | `0xFA80` | International orange |
| `TFT_GOLD` | `0xD566` | Gold |
| `TFT_MED_GREEN` | `0x0507` | Medium green |
| `TFT_MINT` | `0xA6F6` | Mint |
| `TFT_TAN` | `0xB46A` | Tan |
| `TFT_ROSE` | `0xF3CF` | Rose |
| `TFT_SILVER` | `0xC618` | Silver — used for zone border fills in KCMk1 panels |
| `TFT_BROWN` | `0x8200` | Brown |
| `TFT_UPS_BROWN` | `0x6203` | UPS brown |

---

## Version History

| Version | Notes |
|---------|-------|
| **2.1.0** | Added `drawStandbySplash()` shared standby BMP wrapper. Added `thresholdColor()` float overload. Added boot screen helpers (`bsPrint`, `bsLine`, `bsBig`, `bsBlank`, `bsWrap`, `bsShuffle`). Fixed `formatTime()` to use `int64_t` (no overflow beyond ~24.8 Kerbin days); pure-seconds format now outputs `"N s"`. Fixed `drawBMP()` to restore active window on mid-read errors. Fixed `drawButton()` to restore active window after border draw. Changed `param`/`value` string parameters from `String` to `const String &`. Added compile-time version defines: `KDC_VERSION_MAJOR`, `KDC_VERSION_MINOR`, `KDC_VERSION_PATCH`. |
| **2.0.1** | Added `PrintState` struct; updated `printValue` and `printDisp` to require `PrintState &`. **Breaking change from v1.x.** Added `drawVerticalText()` for vertical section label strips. |
| **1.0.0** | Initial release. `DispCache`-based flicker-free rendering, `drawVertBarGraph()`, `drawArcDisplay()`, `drawLabelledAxis()`, full body table, GSL1680F touch driver, BMP loader. |

---

## Notes

- **RA8875 text mode** — never call `setFontScale()` or the RA8875's built-in `print()` text mode API. This puts the chip into internal text mode and PaulStoffregen v0.7.11 does not expose a public graphics-mode restore call. Use `tft.setFont()` / `tft.setCursor()` / `tft.print()` exclusively (GFX graphics mode), which is what all library functions do internally.
- **RA8875 active window** — `fillRect()` and `drawRect()` modify the active window and leave it set to the rect bounds. `drawButton()` and `drawBMP()` both restore the window to full-screen on exit. If calling `fillRect()` or `drawRect()` directly, restore with `tft.setActiveWindow(0, KCM_SCREEN_W-1, 0, KCM_SCREEN_H-1)` before drawing text or calling other library functions.
- **`RA8875_DISPLAY_SIZE`** — defaults to `RA8875_800x480`. Override before the `#include` if using a different panel.

Licensed under the GNU General Public License v3.0.
Final code written by J. Rostoker for Jeb's Controller Works.
