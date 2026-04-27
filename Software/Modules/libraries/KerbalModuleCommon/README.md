# KerbalModuleCommon

**Version:** 1.1.0  
**Author:** J. Rostoker — Jeb's Controller Works  
**License:** GNU General Public License v3.0 (GPL-3.0)  

---

## Overview

KerbalModuleCommon is the shared foundation library for all Kerbal Controller Mk1 modules. It is the single source of truth for:

- Colour types (`RGBColor`, `GRBWColor`) and conversion helpers
- System colour palette (`KMC_*` constants), hardware-validated on SK6812MINI-EA
- I2C command byte definitions (`KMC_CMD_*`)
- Module Type ID registry (`KMC_TYPE_*`)
- Capability flags (`KMC_CAP_*`)
- LED state nibble values (`KMC_LED_*`)
- LED payload nibble pack/unpack helpers

All module libraries (Kerbal7SegmentCore, KerbalButtonCore, KerbalJoystickCore) and standalone module sketches depend on this library. Define colours, commands, and type IDs here — never locally in a module.

---

## Installation

Sketch → Include Library → Add .ZIP → select `KerbalModuleCommon_v1.1.0.zip`

Supported architectures: `megaavr` (ATtiny816 etc.), `renesas_uno` (Xiao RA4M1 etc.), and all others (`*`). The library is pure header-only C++ with no platform-specific code.

---

## Colour Types

```cpp
struct RGBColor  { uint8_t r, g, b; };
struct GRBWColor { uint8_t g, r, b, w; };  // wire order: G first
```

**Important:** `GRBWColor` field order matches SK6812MINI-EA wire protocol (G, R, B, W), not human RGB order. Always use `toGRBW()` to construct a `GRBWColor` from RGB values — never initialise the struct with `{r, g, b, w}` literals directly.

### Conversion helpers

```cpp
// Convert RGBColor to GRBWColor wire format (W=0)
GRBWColor toGRBW(RGBColor c);

// Produce a GRBWColor using only the white channel
GRBWColor KMC_WHITE_ONLY(uint8_t w);

// Scale an RGBColor by brightness (0-255)
RGBColor scaleColor(RGBColor color, uint8_t brightness);

// Scale a GRBWColor by brightness (0-255)
GRBWColor scaleColorGRBW(GRBWColor color, uint8_t brightness);
```

### NeoPixel format note

For modules using **NEO_GRB 3-byte** pixels (SK6812MINI-EA on KC-01-1880), pass colour values directly to `setPixelColor(i, r, g, b)` — the NeoPixel library handles the GRB wire reordering internally. `toGRBW()` is used by modules that need a `GRBWColor` struct for the `ButtonConfig` colour arrays.

---

## System Colour Palette

All colours hardware-validated on SK6812MINI-EA (KC-01-1880 v2.0, NEO_GRB mode).

### Semantic colours (fixed meaning across all modules)

| Constant | RGB | Meaning |
|---|---|---|
| `KMC_OFF` | 0, 0, 0 | Fully unlit |
| `KMC_GREEN` | 0, 255, 0 | Active / go / nominal |
| `KMC_RED` | 255, 0, 0 | Irreversible action — cut, release, jettison |
| `KMC_AMBER` | 255, 80, 0 | Caution / WARNING flash state |

### Whites

| Constant | RGB | Character |
|---|---|---|
| `KMC_WHITE_COOL` | 255, 180, 255 | Bright cool white — general use |
| `KMC_WHITE_NEUTRAL` | 255, 160, 180 | Neutral daylight |
| `KMC_WHITE_WARM` | 255, 140, 100 | Warm incandescent feel |
| `KMC_WHITE_SOFT` | 120, 70, 20 | Dim warm amber — **night vision safe** |

`KMC_WHITE_SOFT` is specifically tuned for dark cockpit use. Warm, dim light causes minimal rod cell bleaching and allows eyes to readapt quickly when the light is turned off.

### Extended palette

| Constant | RGB | Use |
|---|---|---|
| `KMC_ORANGE` | 255, 60, 0 | Engine Alt Mode, Radiator |
| `KMC_YELLOW` | 234, 179, 8 | Warp targets, Lights |
| `KMC_GOLD` | 255, 160, 0 | Solar Array, power generation |
| `KMC_CHARTREUSE` | 163, 230, 53 | Physics warp |
| `KMC_LIME` | 132, 204, 22 | Terrain, Quicksave |
| `KMC_MINT` | 100, 255, 160 | RCS, Quickload |
| `KMC_BLUE` | 0, 0, 255 | Science data |
| `KMC_SKY` | 14, 165, 233 | Navigation, map |
| `KMC_TEAL` | 20, 184, 166 | Ship navigation, atmosphere |
| `KMC_CYAN` | 6, 182, 212 | Atmosphere, ARMED state |
| `KMC_PURPLE` | 120, 0, 220 | Science discovery |
| `KMC_INDIGO` | 60, 0, 200 | Science group |
| `KMC_VIOLET` | 160, 0, 255 | Mechanical access, Cargo Door |
| `KMC_CORAL` | 255, 100, 54 | Mode shift, IVA |
| `KMC_ROSE` | 255, 80, 120 | Vessel configuration, Control Points |
| `KMC_PINK` | 236, 72, 153 | Communications, Antenna |
| `KMC_MAGENTA` | 255, 0, 255 | Debug / development |

---

## I2C Commands

All modules implement the full base command set:

| Constant | Value | Payload | Description |
|---|---|---|---|
| `KMC_CMD_GET_IDENTITY` | 0x01 | — | Query module identity (4-byte response) |
| `KMC_CMD_SET_LED_STATE` | 0x02 | 8 bytes nibble-packed | Set full LED state |
| `KMC_CMD_SET_BRIGHTNESS` | 0x03 | 1 byte | Set ENABLED state brightness |
| `KMC_CMD_BULB_TEST` | 0x04 | — | Trigger bulb test |
| `KMC_CMD_SLEEP` | 0x05 | — | Enter low-power sleep |
| `KMC_CMD_WAKE` | 0x06 | — | Resume from sleep |
| `KMC_CMD_RESET` | 0x07 | — | Reset to default state |
| `KMC_CMD_ACK_FAULT` | 0x08 | — | Acknowledge and clear fault flag |
| `KMC_CMD_ENABLE` | 0x09 | — | Enable module |
| `KMC_CMD_DISABLE` | 0x0A | — | Disable module |
| `KMC_CMD_SET_VALUE` | 0x0D | 2 bytes BE uint16 | Display modules — set value |

---

## Module Type ID Registry

| Constant | Value | Module | I2C Address |
|---|---|---|---|
| `KMC_TYPE_UI_CONTROL` | 0x01 | UI Control | 0x20 |
| `KMC_TYPE_FUNCTION_CONTROL` | 0x02 | Function Control | 0x21 |
| `KMC_TYPE_ACTION_CONTROL` | 0x03 | Action Control | 0x22 |
| `KMC_TYPE_STABILITY_CONTROL` | 0x04 | Stability Control | 0x23 |
| `KMC_TYPE_VEHICLE_CONTROL` | 0x05 | Vehicle Control | 0x24 |
| `KMC_TYPE_TIME_CONTROL` | 0x06 | Time Control | 0x25 |
| `KMC_TYPE_EVA_MODULE` | 0x07 | EVA Module | 0x26 |
| `KMC_TYPE_JOYSTICK_ROTATION` | 0x09 | Joystick Rotation | 0x28 |
| `KMC_TYPE_JOYSTICK_TRANS` | 0x0A | Joystick Translation | 0x29 |
| `KMC_TYPE_GPWS_INPUT` | 0x0B | GPWS Input Panel | 0x2A |
| `KMC_TYPE_PRE_WARP_TIME` | 0x0C | Pre-Warp Time | 0x2B |
| `KMC_TYPE_THROTTLE` | 0x0D | Throttle Module | 0x2C |
| `KMC_TYPE_DUAL_ENCODER` | 0x0E | Dual Encoder | 0x2D |
| `KMC_TYPE_SWITCH_PANEL` | 0x0F | Switch Panel | 0x2E |
| `KMC_TYPE_INDICATOR` | 0x10 | Indicator Module | 0x2F |

---

## Capability Flags

Reported in identity response byte 3:

| Constant | Bit | Description |
|---|---|---|
| `KMC_CAP_EXTENDED_STATES` | 0 | Supports extended LED states (WARNING, ALERT, ARMED, PARTIAL_DEPLOY) |
| `KMC_CAP_FAULT` | 1 | Active fault condition — clear with CMD_ACK_FAULT |
| `KMC_CAP_ENCODERS` | 2 | Encoder delta in response packet |
| `KMC_CAP_JOYSTICK` | 3 | Analog joystick axes in response packet |
| `KMC_CAP_DISPLAY` | 4 | 7-segment display and encoder present |
| `KMC_CAP_MOTORIZED` | 5 | Motorized position control |

---

## LED State Nibble Values

Used with `KMC_CMD_SET_LED_STATE` nibble-packed payload:

| Constant | Value | Description |
|---|---|---|
| `KMC_LED_OFF` | 0x0 | Unlit |
| `KMC_LED_ENABLED` | 0x1 | Dim white backlight |
| `KMC_LED_ACTIVE` | 0x2 | Full brightness, per-button colour |
| `KMC_LED_WARNING` | 0x3 | Flashing amber, 500ms on/off |
| `KMC_LED_ALERT` | 0x4 | Flashing red, 150ms on/off |
| `KMC_LED_ARMED` | 0x5 | Static cyan |
| `KMC_LED_PARTIAL_DEPLOY` | 0x6 | Static amber |

### Nibble pack/unpack helpers

```cpp
// Get LED state nibble for button N from packed 8-byte payload
uint8_t kmcLedPackGet(const uint8_t* payload, uint8_t button);

// Set LED state nibble for button N in packed 8-byte payload
void kmcLedPackSet(uint8_t* payload, uint8_t button, uint8_t state);
```

---

## Changelog

### v1.1.0 (2026-04-27)

- Architecture support expanded to `megaavr, renesas_uno, *`
- Entire colour palette hardware-validated on SK6812MINI-EA (KC-01-1880 v2.0, NEO_GRB 3-byte mode)
- Semantic colours corrected to pure primaries: GREEN `{0,255,0}`, RED `{255,0,0}`
- AMBER adjusted to `{255,80,0}` for unambiguous orange-amber appearance
- White family rebalanced to compensate for SK6812 green channel sensitivity
- WHITE_SOFT changed to warm dim amber `{120,70,20}` for night vision preservation
- Purple/Indigo/Violet zeroed green channel to prevent "cool white" appearance
- BLUE changed to pure `{0,0,255}`, ORANGE reduced to `{255,60,0}`
- README added

---

## License

GNU General Public License v3.0 — https://www.gnu.org/licenses/gpl-3.0.html  
Code by J. Rostoker, Jeb's Controller Works.
