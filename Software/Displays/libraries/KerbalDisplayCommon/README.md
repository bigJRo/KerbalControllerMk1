# KerbalDisplayCommon

**Kerbal Controller Mk1 — Shared Display Library**
UI toolkit for RA8875-based touchscreen display panels used in KSP controller builds.
Part of the KCMk1 controller system.

---

## Overview

KerbalDisplayCommon provides the shared display firmware core for KCMk1 display panels. It abstracts the RA8875 display driver into higher-level UI primitives — buttons, text blocks, value formatters, threshold coloring, BMP drawing, and capacitive touch — so that sketch code can focus on layout and telemetry logic rather than display housekeeping.

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

`drawBMP(tft, filename, x, y)` — draws a 24-bit uncompressed BMP from the SD card at screen position `(x, y)`. Only 24-bit BI_RGB (uncompressed) files are supported. Both bottom-up (standard) and top-down BMPs are handled. On `malloc` success all rows are read sequentially for maximum throughput; on `malloc` failure rows are read individually. Image dimensions are clamped to 800x480 before stack allocation. Returns a `BMPResult` error code on failure, also printed to Serial with the filename.

**BMPResult codes:** `BMP_OK`, `BMP_ERR_NO_CARD`, `BMP_ERR_SD_INIT`, `BMP_ERR_FILE`, `BMP_ERR_SIGNATURE`, `BMP_ERR_DIB`, `BMP_ERR_COMPRESSED`, `BMP_ERR_DIMENSIONS`, `BMP_ERR_READ`, `BMP_ERR_NOT_24BIT`.

### Capacitive Touch

`setupTouch()` — initialises Wire1 and the GSL1680F touch controller. Uploads the panel firmware over I2C (required on every power cycle) and starts the chip. Call once from `setup()`.

`isTouched()` — polls the `CTP_INT_PIN` GPIO directly and returns `true` if the pin is HIGH. The GSL1680F holds INT HIGH for the full duration of a touch, so polling is reliable without an ISR. No I2C read is performed.

`readTouch()` — reads all active touch points from the GSL1680F. Returns a `TouchResult` struct with `count` and `points[]` (each point has `x`, `y`, `id`). Call when `isTouched()` returns `true`.

`clearTouchISR()` — no-op retained for API compatibility. The library previously used a falling-edge ISR; it now uses GPIO polling. Call sites can be left unchanged — the call is safe and costs nothing.

`touchISRCount()` — returns a running count of `isTouched()` calls that returned `true`. Retained for diagnostics.

### Buttons

`drawButton(tft, x, y, w, h, label, font, isOn)` — draws a filled rectangle with centered, word-wrapped text in on or off state. Colors and text are taken from a `ButtonLabel` struct. Single words wider than the button width will overflow without clipping.

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

Three core rendering functions — all other display block functions are built on these.

`textLeft(tft, font, x0, y0, w, h, value, foreColor, backColor)` — left-justified, vertically centred.

`textRight(tft, font, x0, y0, w, h, value, foreColor, backColor)` — right-justified, vertically centred.

`textCenter(tft, font, x0, y0, w, h, value, foreColor, backColor)` — horizontally and vertically centred.

### Display Blocks

Higher-level functions that combine text rendering, background fill, and optional border into a single call.

`printDisp(tft, font, x0, y0, w, h, param, value, paramColor, valColor, valBack, backColor, borderColor)` — draws a parameter label left-justified and a value right-justified in a rectangle. A cached overload is available: pass a `DispCache &cache` as the final argument to skip redraw entirely when content and colors are unchanged.

`printDispChrome(tft, font, x0, y0, w, h, label, labelColor, backColor, borderColor)` — draws only the static label and border, leaving the value region untouched. Use once at screen entry; follow with `printValue()` on each update.

`printValue(tft, font, x0, y0, w, h, param, value, valColor, valBack, backColor)` — redraws only the right-hand value region, leaving the param label and border completely untouched. Use on per-frame updates.

`printName(tft, font, x0, y0, w, h, value, color, backColor, borderColor, maxLength=30)` — left-justified label, truncated to `maxLength` characters.

`printTitle(tft, font, x0, y0, w, h, value, color, backColor, borderColor)` — horizontally centred title string.

### Threshold Color

`thresholdColor(value, lowVal, lowColor, lowBack, midVal, midColor, midBack, highColor, highBack, foreColor, backColor)` — selects foreground and background colors based on two thresholds. `foreColor` and `backColor` are output parameters set by the function.

### Formatters

**Basic:**
`formatInt(value)`, `formatFloat(value, decimals)`, `formatPerc(value)` (appends `%`), `formatUnits(value, units)`, `formatFloatUnits(value, decimals, units)`.

**KSP telemetry:**
`formatSep(value)` — comma-separated float, e.g. `"1,234.56"`. Required dependency of `formatAlt()`.
`formatTime(timeVal)` — Kerbin time string, e.g. `"1 d: 02 h: 30 m: 15 s"`. Uses Kerbin day = 6 hours.
`formatAlt(value)` — auto-scaling altitude string. Scales through m / km / Mm / Gm.
`twString(twIndex, physTW)` — human-readable time warp rate, e.g. `"100x"`, `"PHYS-2x"`. Normal indices 0-7; physics indices 1-3.

### Font Measurement

`getFontCharWidth(font, c)` — pixel width of a single character in a proportional tFont.
`getFontStringWidth(font, str)` — total pixel width of a string in a proportional tFont.

### Graphical Widgets

`drawVertBarGraph(tft, x0, y0, w, h, prevVal, newVal, barColor, drawBorder, scale=1000)` — vertical bar graph, bottom-fill, values 0-scale. Erases only the changed segment rather than redrawing the full bar. Call from `updateScreen*()` only when the value changes; update `prevVal` in the caller after each call.

`drawArcDisplay(tft, cx, cy, radius, needleW, minVal, maxVal, prevVal, curVal, color)` — semicircular arc indicator spanning 180 degrees. Erases the previous needle position before drawing the new one. The arc track is redrawn on every call.

`drawLabelledAxis(tft, x0, axisW, barTop, barBottom, font, axisColor, backColor)` — draws a vertical percentage axis with major ticks every 10% and minor ticks every 5%. Labels (0%, 50%, 100%) are right-justified within `axisW` pixels. `0%` is at `barBottom`, `100%` is at `barTop`. The axis line is drawn at `x0 + axisW - 1`. Used by `ScreenMain` bar graph panels.

`drawVerticalText(tft, x0, y0, w, h, font, text, color, backColor)` — draws a string one character per line within a rectangle, creating a vertical label strip. Characters are centred horizontally within `w` and the full strip is centred vertically within `h`. The strip is filled with `backColor` before drawing. Used where text rotation is needed since the RA8875 has no native rotation support.

### Celestial Bodies

`getBodyParams(SOI)` — looks up a Simpit SOI string in the built-in body table and returns a `BodyParams` struct. Returns a zeroed/empty struct if the SOI is not recognised — check `soiName[0] != '\0'` to detect a valid result. Call whenever Simpit reports a new SOI.

**`BodyParams` fields:** `soiName`, `dispName`, `minSafe`, `flyHigh`, `lowSpace`, `highSpace`, `surfGrav`, `radius`, `image` (SD card BMP path), `cond` (`"Vacuum"`, `"Atmosphere"`, `"Breathable"`, `"Plasma"`). All altitudes in metres; `surfGrav` in g; `flyHigh`/`lowSpace` = 0 means no atmosphere; `surfGrav` = 0 means not landable.

Bodies in the table: Kerbol, Moho, Eve, Gilly, Kerbin, Mun, Minmus, Duna, Ike, Dres, Jool, Laythe, Vall, Tylo, Bop, Pol, Eeloo.

### System Utilities

`executeReboot()` — soft reboot via ARM AIRCR register. Does not return. Teensy 4.0 specific; do not call from within an ISR.

`disconnectUSB()` — shuts down and resets the USB1 controller. Call immediately before `executeReboot()` for a clean USB re-enumeration on the host.

---

## Fonts

All Roboto Black sizes (12, 16, 20, 24, 36, 40, 48, 72px) and IBM CP437 terminal fonts (TerminalFont_16 at 8x16px, TerminalFont_32 at 16x32px) are included automatically from `src/fonts/` via `#include` in the library header. No additional installation step is needed.

Font objects are named `Roboto_Black_24`, `TerminalFont_16`, etc. Pass a pointer when calling library functions: `&Roboto_Black_24`.

---

## Color Palette

All colors are RGB565 format with `TFT_` prefix, defined in `KerbalDisplayCommon.h`.

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
| `TFT_SILVER` | `0xC618` | Silver |
| `TFT_BROWN` | `0x8200` | Brown |
| `TFT_UPS_BROWN` | `0x6203` | UPS brown |

---

## Notes

- **RA8875 text mode** — never call `setFontScale()` or the RA8875's built-in `print()` text mode API. This puts the chip into internal text mode and PaulStoffregen v0.7.11 does not expose a public graphics-mode restore call. Use `tft.setFont()` / `tft.setCursor()` / `tft.print()` exclusively (GFX graphics mode), which is what all library functions do internally.
- **`RA8875_DISPLAY_SIZE`** — defaults to `RA8875_800x480`. Override before the `#include` if using a different panel.
- **String heap usage** — several functions accept `String` arguments. Low risk on Teensy 4.0 (512 KB RAM) but worth noting if porting to a memory-constrained target.
- **`BodyParams` string fields** — `soiName`, `dispName`, `image`, and `cond` are pointers into static string literals. They remain valid for the lifetime of the program. Do not assign them to stack-allocated char arrays — treat as read-only. Use `strcpy()` into a local buffer if a mutable copy is needed.

Licensed under the GNU General Public License v3.0.
Final code written by J. Rostoker for Jeb's Controller Works.
