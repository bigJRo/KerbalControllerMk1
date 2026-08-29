# KerbalDisplayCommon

**Kerbal Controller Mk1 — Shared Display Library** · v3.5.0
UI toolkit for LT7683 (RA8876-compatible) touchscreen display panels used in KSP controller builds.
Part of the KCMk1 controller system.

---

## Overview

KerbalDisplayCommon provides the shared display firmware core for KCMk1 display panels. It abstracts the RA8876 display driver (the `KCM_TFT` type, `RA8876_t41_p` via KCM_Display) into higher-level UI primitives — buttons, text blocks, value formatters, threshold colouring, and BMP drawing — so that sketch code can focus on layout and telemetry logic rather than display housekeeping. As of hardware rev 2, capacitive touch has moved out into its own KCM_Touch library.

The physical display controller is the **LT7683** (on the ER-TFT070A2-6-5633 module); it is register-compatible with the RA8876, so the driver library, its class (`RA8876_t41_p`), and its GFX API all carry the "RA8876" name even though the hardware part is the LT7683.

The library is designed to be used alongside KerbalDisplayAudio and KerbalSimpit in multi-tab Arduino sketches targeting the Teensy 4.1.

---

## Hardware

| Component | Part | Interface |
|-----------|------|-----------|
| Microcontroller | Teensy 4.1 | — |
| Display | LT7683 (RA8876-compatible) 1024×600 IPS TFT — BuyDisplay ER-TFT070A2-6-5633 | 16-bit 8080 parallel (FlexIO3) |
| Touch controller | FT5316 5-point capacitive (via KCM_Touch) | Software I2C (bit-banged) |
| SD card | Teensy 4.1 on-board microSD | SDIO (`BUILTIN_SDCARD`) |

### Pin Assignments

All display, touch, and SD pin assignments now live in **`KCMk1_SystemConfig.h`** (pulled in via `KCM_Display.h`) rather than being defined per-sketch — the carrier board is fixed, so there is nothing to override. The rev-1 `RA8875_*` and `SD_CS_PIN` defines from the SPI stack have been removed. The 16-bit 8080 data bus (`DB0..DB15`) is owned by the TeensyRA8876-8080 FlexIO3 driver; only the control lines below are plain GPIO passed to the driver.

**Display control lines** (`KCM_TFT_*` in `KCMk1_SystemConfig.h`):

| Pin | Define | Function |
|-----|--------|----------|
| 34 | `KCM_TFT_CS` | /CS chip select (active-LOW) |
| 33 | `KCM_TFT_RS` | RS register/data select |
| 35 | `KCM_TFT_RESET` | /RST hardware reset (active-LOW) |
| 36 | `KCM_TFT_WR` | /WR write strobe (active-LOW) |
| 37 | `KCM_TFT_RD` | /RD read strobe (active-LOW) |
| 32 | `KCM_TFT_WAIT` | WAIT busy flow control |
| 31 | `KCM_TFT_INT` | INT interrupt from LT7683 |
| 9 | `KCM_TFT_BL` | Backlight enable / PWM |

**FT5316 touch** (handled by the KCM_Touch library, `KCM_CTP_*`): SCL pin 4, SDA pin 5, /RST pin 3, INT pin 6, I2C address `0x38`, on a bit-banged software I2C bus.

**SD card:** `KCM_SD_CS` = `BUILTIN_SDCARD` (Teensy 4.1 on-board microSD, SDIO).

---

## Dependencies

**Library Manager / build-machine installs:**

| Library | Notes |
|---------|-------|
| wwatson4506/TeensyRA8876-8080 (`RA8876_t41_p`) | Display driver for the 16-bit 8080 parallel bus — replaces the rev-1 PaulStoffregen RA8875 driver |
| TeensyRA8876-GFX-Common | GFX common layer used by the RA8876 driver |
| PaulStoffregen/ILI9341_fonts | ILI9341_t3 font format — replaces the rev-1 sumotoy tFont |
| SD | Teensy bundled version (used via `BUILTIN_SDCARD` / SDIO) |
| Wire | Teensy bundled version |

**In-repo dependencies (this repo, not Library Manager):**

| Library | Notes |
|---------|-------|
| KCM_Display | Provides the `KCM_TFT` type (`RA8876_t41_p`) plus pins/resolution from `KCMk1_SystemConfig.h` |
| KCMk1_SystemConfig | Master hardware/pin configuration header |
| KCM_Touch | FT5316 capacitive touch driver (touch moved out of KDC in rev 2) |

---

## Configuration

| Constant | Default | Description |
|----------|---------|-------------|
| `KCM_SCREEN_W` | `1024` | Panel width (px) — from `KCMk1_SystemConfig.h` |
| `KCM_SCREEN_H` | `600` | Panel height (px) — from `KCMk1_SystemConfig.h` |
| `KCM_SD_CS` | `BUILTIN_SDCARD` | Teensy 4.1 on-board microSD (SDIO) — from `KCMk1_SystemConfig.h` |
| `NO_BORDER` | `0x0001` | Pass as `borderColor` to skip border drawing |
| `TEXT_BORDER` | `8` | Horizontal pixel padding used by all text functions |

---

## Features

### Display Setup

`setupDisplay(tft, backColor)` — initialises the display, fills the screen with `backColor`, and prints a ready message to Serial if debug mode is enabled. Call once from `setup()` before any drawing functions.

`setKDCDebugMode(bool)` — enables or disables verbose Serial output from the library (I2C scan, BMP success messages). Error messages (SD failures, BMP errors) are always printed regardless of this setting. Call after `Serial.begin()`.

### SD Card and BMP Drawing

`setupSD()` — initialises the Teensy 4.1 on-board microSD socket. Must be called once from `setup()` before any `drawBMP()` calls. Calls `SD.begin(KCM_SD_CS)` with `KCM_SD_CS == BUILTIN_SDCARD` (SDIO). Returns `true` on success. The rev-1 separate `SD_DETECT_PIN` check from the SPI wiring is gone.

`drawBMP(tft, filename, x, y)` — draws a 24-bit uncompressed BMP from the SD card at screen position `(x, y)`. Only 24-bit BI_RGB (uncompressed) files are supported. Both bottom-up (standard) and top-down BMPs are handled. Each row is blitted with `writeRect()`, a fast windowed transfer that addresses an explicit rectangle. On `malloc` success all rows are read sequentially for maximum throughput; on `malloc` failure rows are read individually. Image dimensions are clamped to 1024×600 (`KCM_SCREEN_W`/`KCM_SCREEN_H`) before drawing. Returns a `BMPResult` error code on failure, also printed to Serial with the filename. The `writeRect()`-based blit needs no active-window or cursor restore on exit.

**BMPResult codes:** `BMP_OK`, `BMP_ERR_NO_CARD`, `BMP_ERR_SD_INIT`, `BMP_ERR_FILE`, `BMP_ERR_SIGNATURE`, `BMP_ERR_DIB`, `BMP_ERR_COMPRESSED`, `BMP_ERR_DIMENSIONS`, `BMP_ERR_READ`, `BMP_ERR_NOT_24BIT`.

`drawStandbySplash(tft)` — convenience wrapper: calls `fillScreen(BLACK)`, then `drawBMP("/StandbySplash_1024x600.bmp", 0, 0)`. Used by all KCMk1 panels for their shared standby screen. `setupSD()` must have been called first.

### Capacitive Touch

**Touch has moved out of KerbalDisplayCommon as of hardware rev 2.** The rev-1 GSL1680F driver (and its 800×480 firmware blob on Wire1) is replaced by the separate **KCM_Touch** library, which drives an FT5316 5-point capacitive controller on a bit-banged software I2C bus (SCL pin 4, SDA pin 5, /RST pin 3, INT pin 6, address `0x38`). Include `<KCM_Touch.h>` from the sketch; it provides the same API surface as before: `TouchPoint` / `TouchResult` / `setupTouch()` / `isTouched()` / `readTouch()` / `clearTouchISR()` / `touchISRCount()`.

### Buttons

`drawButton(tft, x, y, w, h, label, font, isOn)` — draws a filled rectangle with centred, word-wrapped text in on or off state. Colours and text are taken from a `ButtonLabel` struct. Under the rev-2 RA8876 GFX driver, `fillRect()`/`drawRect()` do not leave a clipping window, so no active-window restore is needed after drawing.

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

`getFontCharWidth(font, c)` — pixel width of a single character in a proportional ILI9341_t3 font.

`getFontStringWidth(font, str)` — total pixel width of a string in a proportional ILI9341_t3 font.

### Graphical Widgets

`drawVertBarGraph(tft, x0, y0, w, h, prevVal, newVal, barColor, drawBorder, scale=1000)` — vertical bar graph, bottom-fill, values 0–scale. Erases only the changed segment rather than redrawing the full bar. Call from `updateScreen*()` only when the value changes; update `prevVal` in the caller after each call.

`drawArcDisplay(tft, cx, cy, radius, needleW, minVal, maxVal, prevVal, curVal, color)` — semicircular arc indicator spanning 180 degrees. Erases the previous needle position before drawing the new one. The arc track is redrawn on every call.

`drawLabelledAxis(tft, x0, axisW, barTop, barBottom, font, axisColor, backColor)` — draws a vertical percentage axis with major ticks every 10% and minor ticks every 5%. Labels (0%, 50%, 100%) are right-justified within `axisW` pixels. `0%` is at `barBottom`, `100%` is at `barTop`. The axis line is drawn at `x0 + axisW - 1`.

`drawVerticalText(tft, x0, y0, w, h, font, text, color, backColor)` — draws a string one character per line within a rectangle, creating a vertical label strip. Characters are centred horizontally within `w` and the full strip is centred vertically within `h`. The strip is filled with `backColor` before drawing. Used where text rotation is needed since the display controller has no native rotation support.

### Navball Markers

KSP-style attitude-reticle markers used by the InfoDisp EADI and the MNVR / DOCK / TGT reticle screens. Each takes `(tft, cx, cy, r, color)` — `cx,cy` is the marker centre, `r` the ring radius; all sub-elements (stroke width, dot, spokes) scale from `r` so the marker stays proportional at any size. Colour is the caller's choice (green for velocity/prograde, blue for maneuver, etc.).

| Function | Marker |
|----------|--------|
| `drawProgradeMarker` | Prograde — ring + centre dot + three spokes (up/left/right) |
| `drawRetrogradeMarker` | Retrograde — ring + X |
| `drawManeuverMarker` | Maneuver node — prograde-style with a gap between dot and prongs |
| `drawTargetMarker` | Target — gapped-arc ring + centre dot |
| `drawAntiTargetMarker` | Anti-target — ✕ over a gapped-arc ring |
| `drawNormalMarker` / `drawAntiNormalMarker` | Normal / anti-normal (up / down triangle spokes) |
| `drawRadialInMarker` / `drawRadialOutMarker` | Radial-in / radial-out |
| `drawLevelIndicator` | Level / horizon reference marker |

`enum KspMarkerKind { KSP_MK_PROGRADE, KSP_MK_TARGET, KSP_MK_MANEUVER, KSP_MK_RETROGRADE, KSP_MK_NORMAL, KSP_MK_ANTINORMAL, KSP_MK_RADIAL_IN, KSP_MK_RADIAL_OUT, KSP_MK_ANTITARGET, KSP_MK_LEVEL }` — stable ordinals for callers that select a marker by kind (existing ordinals never change; new kinds append).

`drawThickLine(tft, x0, y0, x1, y1, w, color, caps=true)` — straight line of stroke width `w` px (falls back to `drawLine` for `w≤1`), thickened symmetrically about the ideal line so it stays clean at any angle. `caps=false` suppresses the round end-caps (used for free-ended marker spokes/prongs so they don't read as blobs). This is the primitive the markers above build on.

### Boot Screen Helpers

Shared terminal-aesthetic rendering primitives used by all KCMk1 panel boot sequences. All functions operate in RA8876 graphics mode (never text mode). The sketch defines its own local font and column constants and passes them through.

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

`executeReboot()` — soft reboot via ARM AIRCR register. Does not return. Teensy 4.1 (IMXRT1062) specific; do not call from within an ISR.

`disconnectUSB()` — shuts down and resets the USB1 controller. Call immediately before `executeReboot()` for a clean USB re-enumeration on the host.

---

## Fonts

All fonts are in the ILI9341_t3 (`ILI9341_t3_font_t`) format. Roboto Black sizes (12, 16, 20, 24, 28, 32, 36, 40, 48, 72 px) and the `KcmTerm` monospace terminal font (16, 20, 24, 28, 32, 36, 40 px) are included automatically from `src/fonts_ili/` via `#include` in the library header. No additional installation step is needed. `KcmTerm` carries the hand-designed bitmaps of **Terminus Font 4.49** (SIL OFL 1.1) — pixel-exact at every native size — converted from the Terminus BDF sources by `bdf_to_ili9341.py`; it replaces the old IBM VGA `TerminalFont`. See `src/fonts_ili/OFL.txt`. (The rev-1 sumotoy tFont format has been replaced.)

Font objects are named `Roboto_Black_24`, `KcmTerm_24`, etc. Pass a pointer when calling library functions: `&Roboto_Black_24`.

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
| **3.5.0** | **Off-scale markers are visibly pinned.** A marker beyond full scale was clamped to the boundary and drawn identically to a real reading — on DOCK a port 45° off the nose was pixel-identical to one at 17.3°, across a 146°-wide ambiguous band. `reticleClampDot` now returns whether it clamped, `ReticleDotCache` carries `primaryPinned`/`velPinned`, and a pinned marker is drawn at half brightness in new `TFT_DIM_VIOLET` / `TFT_DIM_NEON_GRN` shades — direction stays honest, and the marker no longer claims a distance it doesn't have. `reticleEraseDot` gained a `restyled` argument (defaulted, so existing calls are unaffected): the pinned transition can move the marker as little as 1 px, which the `>1 px` movement gate swallows, so without it a bright marker could be left drawn while clamped. |
| **3.4.0** | `ReticleAngles::appBrg`/`appElv` become **`appRight`/`appUp`** — the relative-velocity direction decomposed about the **target** axis (carrying the vessel roll) instead of horizon-frame heading/pitch differences. The old pair inflated with pitch: a 6° approach-path error printed 9.9° at 60° pitch. The new pair is the exact 3D angle and agrees with the on-screen VEL-to-PORT marker gap to within 0.31° inside DOCK's ±20° scale, growing to ~2.9° at TGT's 60° rim. |
| **3.3.1** | Documented the project-wide rule that every boresight display builds its `KspBodyAxes` with the vessel roll, so screen up is always the craft's roof. The horizon-referenced path (`rollDeg = 0`) now has no caller; kept because it costs nothing and a world-referenced scope could want it. |
| **3.3.0** | **True boresight projection.** `kspCockpitOffset` is replaced by **`kspBodyAxes`** + **`kspBoresightAngles`**, which resolve the vessel attitude into 3D body axes (ENU) and project a world direction *azimuthal-equidistant* about the boresight — the plotted radius IS the true angular separation from the nose, at any attitude, out to 180°. The old scheme subtracted headings and pitches and scaled the differences, which is only valid near zero pitch: it stretched the bearing axis by roughly 1/cos(pitch) (a 10° error read 14° at 45°, 20° at 60°) and inverted past ~80°, exactly where a radial-in/out maneuver node sits. An atan2-per-axis alternative was rejected as gnomonic — it reads 71.8° for a true 60° at a 45° clock angle, which would make TGT's outer rings lie. Roll is now carried by the axes, so `ReticleGeom::rollRef` is gone: a caller picks its frame by passing roll (body) or 0 (horizon) when building the axes, and roll handedness has a single definition inside `kspBodyAxes`. `ReticleAngles` carries `right`/`up` pairs instead of bearing/elevation, and `reticleProject` drops its roll argument. Removing 3.2.0's `kspCockpitOffset` outright is safe — it existed only on this branch with no other consumers. |
| **3.2.0** | **Reticle marker layer moved in from KCMk1_InfoDisp.** `ReticleGeom` / `ReticleDotCache` / `ReticleAngles` plus `reticleProject`, `reticleClampDot`, `reticleEraseDot`, `reticleRepairDotChrome` and `reticleUpdateDots` now live beside the `reticleDrawBase` + `reticleRepair` chrome they already drew on — none of it reads telemetry, and MNVR / DOCK / TGT now run one implementation instead of three (MNVR previously carried its own copies of the clamp and the chrome repair). `ReticleGeom` gained `clampMargin` (explicit rim keep-out, replacing a hardcoded `dotRVel*3/2` that did not fit MNVR's marker), `lblFont`, and `rollRef`. New **`kspCockpitOffset`** — the single definition of roll handedness for every body-referenced display in the project (EADI ball markers, the DOCK reticle, the re-entry retro ball), returning a pixel offset so each caller keeps its own rounding. `eadiHdgDelta` (heading wrap to ±180°) also moved in; it had eighteen call sites across the InfoDisp sketch and no sketch-local dependencies. Additive only — no existing signature changed. |
| **3.1.2** | `drawButton()` word-wrap hardened (audit batch D): the word-split loop is now bounded by `MAX_WORDS` (a label with >32 words could write one row past `words[][]`) and the per-line copy by `MAX_LINECH` (a wide button + small font could overrun `lines[][64]`); over-long single words truncate to the line buffer. `reticleDrawBase` dropped a redundant concentric `drawCircle`. Comment/header drift corrected (`printDispChrome` doc, `bsBig` advance, buffer-size 800→1024, Teensy 4.0→4.1). |
| **3.1.1** | Marker polish: `drawThickLine` gained an optional `caps` argument (default true) so free-ended spokes/prongs can suppress the round end-caps that read as small blobs; the marker spokes, prongs, X and face-spokes now draw capless. Shrank the level-indicator nose dot. |
| **3.1.0** | Completed the KSP navball marker set. Added `drawRetrogradeMarker`, `drawNormalMarker`, `drawAntiNormalMarker`, `drawRadialInMarker`, `drawRadialOutMarker`, `drawAntiTargetMarker` and `drawLevelIndicator` (joining prograde/target/maneuver), and extended `KspMarkerKind` with the new kinds (existing ordinals unchanged). Prograde spokes lengthened, target arc gaps widened, and the maneuver marker now leaves a gap between the centre dot and its prongs. |
| **3.0.0** | Hardware rev 2 migration. Display type `RA8875` → `KCM_TFT` (`RA8876_t41_p` via KCM_Display); MCU Teensy 4.0 → 4.1; panel 800×480 SPI → 1024×600 16-bit 8080 parallel (FlexIO3). Fonts sumotoy tFont → ILI9341_t3 (`fonts_ili/`). BMP blit now via `writeRect()`. SD via Teensy 4.1 `BUILTIN_SDCARD` (SDIO). Touch moved out to the separate KCM_Touch library (FT5316 on software I2C); GSL1680F driver removed. Pin/resolution defines now sourced from `KCMk1_SystemConfig.h`. |
| **2.1.0** | Added `drawStandbySplash()` shared standby BMP wrapper. Added `thresholdColor()` float overload. Added boot screen helpers (`bsPrint`, `bsLine`, `bsBig`, `bsBlank`, `bsWrap`, `bsShuffle`). Fixed `formatTime()` to use `int64_t` (no overflow beyond ~24.8 Kerbin days); pure-seconds format now outputs `"N s"`. Fixed `drawBMP()` to restore active window on mid-read errors. Fixed `drawButton()` to restore active window after border draw. Changed `param`/`value` string parameters from `String` to `const String &`. Added compile-time version defines: `KDC_VERSION_MAJOR`, `KDC_VERSION_MINOR`, `KDC_VERSION_PATCH`. |
| **2.0.1** | Added `PrintState` struct; updated `printValue` and `printDisp` to require `PrintState &`. **Breaking change from v1.x.** Added `drawVerticalText()` for vertical section label strips. |
| **1.0.0** | Initial release. `DispCache`-based flicker-free rendering, `drawVertBarGraph()`, `drawArcDisplay()`, `drawLabelledAxis()`, full body table, GSL1680F touch driver, BMP loader. |

---

## Notes

- **Graphics mode** — all library functions render via the RA8876 GFX API (`tft.setFont()` / `tft.setCursor()` / `tft.print()` and the `writeRect()` blit). Stay in graphics mode; do not use any internal chip text mode.
- **Active window** — unlike the rev-1 RA8875 driver, the rev-2 RA8876 GFX driver does not leave a clipping window after `fillRect()`/`drawRect()`, so no active-window restore is needed after `drawButton()` or `drawBMP()`.
- **Resolution** — fixed at 1024×600 (`KCM_SCREEN_W` × `KCM_SCREEN_H`), sourced from `KCMk1_SystemConfig.h` (via `KCM_Display.h`). The carrier board is fixed hardware; there is nothing to override.

Licensed under the GNU General Public License v3.0.
Final code written by J. Rostoker for Jeb's Controller Works.
