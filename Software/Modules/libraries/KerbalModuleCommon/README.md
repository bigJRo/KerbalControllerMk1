# KerbalModuleCommon

**Version:** 1.1.0  
**Author:** J. Rostoker — Jeb's Controller Works  
**License:** GNU General Public License v3.0 (GPL-3.0)  

---

## Overview

KerbalModuleCommon is the shared foundation library for all Kerbal Controller Mk1 modules. It is the single source of truth for:

- `RGBColor` struct and the `KMC_*` colour palette
- I2C command byte definitions (`KMC_CMD_*`)
- Module Type ID registry (`KMC_TYPE_*`)
- Capability flags (`KMC_CAP_*`)
- Packet header status constants (`KMC_STATUS_*`)
- LED state nibble values (`KMC_LED_*`)
- LED payload nibble pack/unpack helpers

All module libraries (Kerbal7SegmentCore, KerbalButtonCore, KerbalJoystickCore) and standalone module sketches depend on this library.

---

## Installation

Sketch → Include Library → Add .ZIP → select `KerbalModuleCommon_v1.1.0.zip`

Supported architectures: `megaavr`, `renesas_uno`, and `*`. Header-only — no platform-specific code.

---

## Colour Type

```cpp
struct RGBColor { uint8_t r, g, b; };
```

All hardware on this system uses NEO_GRB 3-byte pixels. Colours are stored as `RGBColor` and passed directly to `setPixelColor(i, r, g, b)` — the NeoPixel library handles wire-order reordering internally.

---

## System Colour Palette

All colours hardware-validated on SK6812MINI-EA (KC-01-1880 v2.0, NEO_GRB mode).

### Utility

| Constant | RGB | Meaning |
|---|---|---|
| `KMC_OFF` | 0, 0, 0 | Fully unlit |
| `KMC_DISCRETE_ON` | 1, 1, 1 | Discrete indicator on — not for NeoPixel positions |
| `KMC_WHITE` | 255, 255, 255 | Full white — bulb test / diagnostics only. Reads green-tinged on SK6812MINI-EA |

### Semantic — fixed meaning across all modules

| Constant | RGB | Meaning |
|---|---|---|
| `KMC_BACKLIT` | 15, 8, 2 | ENABLED backlight — button powered and available |
| `KMC_GREEN` | 0, 255, 0 | Active / go / nominal |
| `KMC_RED` | 255, 0, 0 | Irreversible action — cut, release, jettison |
| `KMC_AMBER` | 255, 80, 0 | Caution / WARNING flash state |

### Whites

| Constant | RGB | Character |
|---|---|---|
| `KMC_WHITE_COOL` | 255, 180, 255 | Bright cool white |
| `KMC_WHITE_NEUTRAL` | 255, 160, 180 | Neutral daylight |
| `KMC_WHITE_WARM` | 255, 140, 100 | Warm incandescent |
| `KMC_WHITE_SOFT` | 120, 70, 20 | Dim warm amber — night vision safe |

### Extended palette

| Constant | RGB | Suggested use |
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

| Constant | Value | Payload | Description |
|---|---|---|---|
| `KMC_CMD_GET_IDENTITY` | 0x01 | — | Query module identity (4-byte response) |
| `KMC_CMD_SET_LED_STATE` | 0x02 | 1 byte | Module-specific LED state byte |
| `KMC_CMD_SET_BRIGHTNESS` | 0x03 | 1 byte | Display intensity — top nibble → MAX7219 (0–15) |
| `KMC_CMD_BULB_TEST` | 0x04 | 1 byte | 0x01 = start, 0x00 = stop |
| `KMC_CMD_SLEEP` | 0x05 | — | Freeze state, suppress INT |
| `KMC_CMD_WAKE` | 0x06 | — | Resume from sleep |
| `KMC_CMD_RESET` | 0x07 | — | Reset to module defaults, stay ACTIVE |
| `KMC_CMD_ACK_FAULT` | 0x08 | — | Acknowledge and clear fault flag |
| `KMC_CMD_ENABLE` | 0x09 | — | Enter ACTIVE lifecycle state |
| `KMC_CMD_DISABLE` | 0x0A | — | Enter DISABLED lifecycle state |
| `KMC_CMD_SET_VALUE` | 0x0D | 2 bytes BE int16 | Set display/encoder value |

---

## Packet Header — Status Byte (byte 0)

| Bits | Field | Values |
|---|---|---|
| 1:0 | Lifecycle | 0x00=ACTIVE, 0x01=SLEEPING, 0x02=DISABLED, 0x03=BOOT_READY |
| 2 | Fault | 1 = hardware fault present |
| 3 | Data changed | 1 = state changed since last read |

Constants: `KMC_STATUS_ACTIVE`, `KMC_STATUS_SLEEPING`, `KMC_STATUS_DISABLED`, `KMC_STATUS_BOOT_READY`, `KMC_STATUS_FAULT`, `KMC_STATUS_DATA_CHANGED`, `KMC_STATUS_LIFECYCLE_MASK`

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

| Constant | Bit | Description |
|---|---|---|
| `KMC_CAP_EXTENDED_STATES` | 0 | Supports WARNING/ALERT/ARMED/PARTIAL_DEPLOY states |
| `KMC_CAP_FAULT` | 1 | Active fault — clear with CMD_ACK_FAULT |
| `KMC_CAP_ENCODERS` | 2 | Encoder delta in response packet |
| `KMC_CAP_JOYSTICK` | 3 | Analog joystick axes in response packet |
| `KMC_CAP_DISPLAY` | 4 | 7-segment display and encoder present |
| `KMC_CAP_MOTORIZED` | 5 | Motorized position control |

---

## LED State Nibble Values

Used with `KMC_CMD_SET_LED_STATE` and `kmcLedPackGet()` / `kmcLedPackSet()` helpers.

| Constant | Value | Description |
|---|---|---|
| `KMC_LED_OFF` | 0x0 | Unlit |
| `KMC_LED_ENABLED` | 0x1 | ENABLED backlight (KMC_BACKLIT) |
| `KMC_LED_ACTIVE` | 0x2 | Full brightness, per-button colour |
| `KMC_LED_WARNING` | 0x3 | Flashing amber |
| `KMC_LED_ALERT` | 0x4 | Flashing red |
| `KMC_LED_ARMED` | 0x5 | Static cyan |
| `KMC_LED_PARTIAL_DEPLOY` | 0x6 | Static amber |

---

## Changelog

### v1.1.0 (2026-04-27)
- `GRBWColor`, `toGRBW()`, `KMC_WHITE_ONLY()`, `scaleColor()` removed — all hardware uses NEO_GRB 3-byte
- `KMC_BACKLIT {15, 8, 2}` added — system-wide ENABLED backlight colour
- `KMC_WHITE {255, 255, 255}` added — bulb test / diagnostics only
- `KMC_STATUS_*` packet header constants added
- Entire colour palette hardware-validated on SK6812MINI-EA (KC-01-1880 v2.0)

---

## License

GNU General Public License v3.0 — https://www.gnu.org/licenses/gpl-3.0.html  
Code by J. Rostoker, Jeb's Controller Works.
