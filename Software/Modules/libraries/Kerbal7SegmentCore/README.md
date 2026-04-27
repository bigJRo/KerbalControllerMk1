# Kerbal7SegmentCore

**Version:** 1.1.0  
**Author:** J. Rostoker — Jeb's Controller Works  
**License:** GNU General Public License v3.0 (GPL-3.0)  
**Hardware:** KC-01-1881/1882 7-Segment Display Module v2.0  
**Target MCU:** ATtiny816 (megaTinyCore, 20 MHz internal)  

---

## Overview

Kerbal7SegmentCore is the Arduino library for Kerbal Controller Mk1 7-segment display modules (KC-01-1882). Each module runs on an ATtiny816, acts as an I2C target device, and provides:

- 4-digit 7-segment display via MAX7219 (software SPI, bit-banged)
- Rotary encoder input with click-count-based acceleration
- Three SK6812MINI-EA NeoPixel buttons with configurable behaviour modes
- Encoder pushbutton (BTN_EN)
- I2C target communication (Kerbal Controller Mk1 protocol)
- INT pin assertion for event-driven controller polling

**Dependency:** KerbalModuleCommon v1.1.0 (colour palette, command constants, type IDs)

---

## Hardware

**PCB:** KC-01-1881 (schematic) / KC-01-1882 (board)  
**MCU:** ATtiny816-MNR  
**Display IC:** MAX7219CWG+T  
**Display:** FJ4401AG 4-digit 7-segment (common cathode)  
**LEDs:** SK6812MINI-EA (NEO_GRB 3-byte mode — white channel not connected on this board)  
**Encoder:** PEC11R-4220F-S0024 with hardware RC debounce (C1/C2 10nF)

### Pin Assignment (verified against KC-01-1881 schematic v2.0)

| ATtiny816 | Net | Signal | Notes |
|---|---|---|---|
| PA1 (pin 20) | BUTTON01 | BTN01 input | Active HIGH, pull-down R9 |
| PA5 (pin 6) | LOAD_DATA | MAX7219 CS#/LOAD | |
| PA6 (pin 7) | DATA_IN | MAX7219 MOSI | |
| PA7 (pin 8) | CLK | MAX7219 SCK | |
| PB0 (pin 14) | SCL | I2C clock | |
| PB1 (pin 13) | SDA | I2C data | |
| PB3 (pin 11) | BUTTON_EN | Encoder pushbutton | Active HIGH, pull-down R6 |
| PB4 (pin 10) | ENC_A | Encoder channel A | Pull-up R4, debounce C1 |
| PB5 (pin 9) | ENC_B | Encoder channel B | Pull-up R2, debounce C2 |
| PC0 (pin 15) | INT | Interrupt output | Active LOW |
| PC1 (pin 16) | BUTTON03 | BTN03 input | Active HIGH, pull-down R11 |
| PC2 (pin 17) | BUTTON02 | BTN02 input | Active HIGH, pull-down R10 |
| PC3 (pin 18) | NEOPIX_CMD | SK6812 data chain | LED2→LED3→LED4 |

### NeoPixel Note

The SK6812MINI-EA on this board responds to **NEO_GRB 3-byte** framing only. The GRBW 4-byte mode produces incorrect colours and pixel misalignment. All colour values are passed as `(r, g, b)` — the library handles wire-order reordering internally via tinyNeoPixel.

---

## Dependencies

- **KerbalModuleCommon v1.1.0** — colour palette, KMC_CMD_* constants, KMC_TYPE_* IDs
- **tinyNeoPixel_Static** — bundled with megaTinyCore, no separate install needed
- **Wire** — bundled with megaTinyCore

`Wire.begin()` is called internally by `k7scBegin()` — do **not** call it in the sketch.

---

## Installation

1. Install **KerbalModuleCommon v1.1.0** from zip (Sketch → Include Library → Add .ZIP)
2. Install **Kerbal7SegmentCore v1.1.0** from zip
3. Arduino IDE settings:
   - Board: `ATtiny816` (megaTinyCore)
   - Clock: `20 MHz internal`
   - tinyNeoPixel Port: `Port C`
   - Programmer: `serialUPDI` or `jtag2updi`

---

## Quick Start

```cpp
#include <KerbalModuleCommon.h>
#include <Kerbal7SegmentCore.h>

#define I2C_ADDRESS    0x2A
#define DEFAULT_VALUE  200

const ButtonConfig btnConfigs[K7SC_NEO_COUNT] = {
    { BTN_MODE_CYCLE,
      { K7SC_ENABLED_COLOR, toGRBW(KMC_GREEN), toGRBW(KMC_AMBER) },
      3, 0 },
    { BTN_MODE_TOGGLE,
      { K7SC_ENABLED_COLOR, toGRBW(KMC_BLUE), K7SC_OFF },
      2, 0 },
    { BTN_MODE_FLASH,
      { toGRBW(KMC_RED), K7SC_OFF, K7SC_OFF },
      0, 300 },
};

void setup() {
    k7scBegin(I2C_ADDRESS, KMC_TYPE_GPWS_INPUT,
              KMC_CAP_DISPLAY, btnConfigs, DEFAULT_VALUE);
}

void loop() {
    k7scUpdate();
    if (buttonsGetEncoderPress()) {
        encoderSetValue(DEFAULT_VALUE);
    }
}
```

---

## API Reference

### Top-level
```cpp
void    k7scBegin(uint8_t i2cAddress, uint8_t typeId, uint8_t capFlags,
                  const ButtonConfig* btnConfigs, uint16_t initialValue = 0);
void    k7scUpdate();
uint8_t k7scGetPendingEvents();
```

### Display
```cpp
void     displaySetValue(uint16_t value);        // 0–9999
uint16_t displayGetValue();
void     displaySetIntensity(uint8_t intensity); // 0–15
void     displayTest();                          // all segments on (non-blocking)
void     displayTestEnd();
void     displayBlank();
void     displayRestore();
void     displayShutdown();
void     displayWake();
```

### Encoder
```cpp
void    encoderSetValue(uint16_t value);
bool    encoderIsValueChanged();
void    encoderClearChanged();
```

### Buttons
```cpp
bool    buttonsGetEncoderPress();                // true once per press, auto-clears
uint8_t buttonsGetStateByte();
void    buttonsSetBrightness(uint8_t brightness);
void    buttonsSetPixelColor(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
void    buttonsShow();
void    buttonsBulbTest();                       // non-blocking, call buttonsShow() after 2s
void    buttonsBulbTestEnd();
void    buttonsClearAll();
```

---

## Button Behaviour Modes

| Mode | Constant | Description |
|---|---|---|
| Cycle | `BTN_MODE_CYCLE` | Each press advances through defined colour states |
| Toggle | `BTN_MODE_TOGGLE` | Press alternates between two states |
| Flash | `BTN_MODE_FLASH` | Brief colour flash on press, returns to ENABLED |
| Momentary | `BTN_MODE_MOMENTARY` | No LED — action only |

```cpp
struct ButtonConfig {
    ButtonMode  mode;
    GRBWColor   colors[3];
    uint8_t     numStates;   // CYCLE only: total states including base
    uint16_t    flashMs;     // FLASH only: duration override (0 = default)
};
```

Use `toGRBW(KMC_*)` to convert KerbalModuleCommon palette colours to `GRBWColor`.

---

## Encoder Acceleration

Step size based on consecutive clicks in the same direction. Reversal resets count to 1.

| Clicks | Step |
|---|---|
| 1–14 | 1 |
| 15–29 | 10 |
| 30–49 | 100 |
| 50+ | 1000 |

Thresholds are `#ifndef`-guarded — override before `#include` in sketch.

Encoder uses PORTB rising-edge interrupt on ENC_A (PB4) — no edges missed at any spin speed.

---

## I2C Protocol

**Speed:** 100–400 kHz

### Commands (controller → module)

| Command | Value | Effect |
|---|---|---|
| `KMC_CMD_GET_IDENTITY` | 0x01 | Returns 4-byte identity packet |
| `KMC_CMD_SET_BRIGHTNESS` | 0x03 | Byte: top nibble→display intensity, full byte→NeoPixel brightness |
| `KMC_CMD_BULB_TEST` | 0x04 | All pixels white + all segments on for 2s |
| `KMC_CMD_SLEEP` | 0x05 | Display off, LEDs off, INT suppressed |
| `KMC_CMD_WAKE` | 0x06 | Restore display and LEDs |
| `KMC_CMD_RESET` | 0x07 | Reset all state, display to 0 |
| `KMC_CMD_SET_VALUE` | 0x0D | 2-byte big-endian uint16 — set display value |

### Data Packet (6 bytes, module → controller)

```
Byte 0: Events     bit0=BTN01, bit1=BTN02, bit2=BTN03, bit3=BTN_EN
Byte 1: ChangeMask same bit layout
Byte 2: State      bits 0-1=BTN01 cycle state, bit2=BTN02 active, bit3=BTN03 active
Byte 3: Reserved   0x00
Byte 4: Value HIGH display value (big-endian uint16)
Byte 5: Value LOW
```

INT asserts LOW when events pending. Deasserts after packet read.

---

## Tunable Constants

All `#ifndef`-guarded in `K7SC_Config.h`:

| Constant | Default | Description |
|---|---|---|
| `K7SC_ENC_MEDIUM_COUNT` | 15 | Clicks before step→10 |
| `K7SC_ENC_FAST_COUNT` | 30 | Clicks before step→100 |
| `K7SC_ENC_TURBO_COUNT` | 50 | Clicks before step→1000 |
| `K7SC_BTN_DEBOUNCE_MS` | 30 | Button stable-time before edge accepted (ms) |
| `K7SC_FLASH_DURATION_MS` | 150 | Default FLASH mode duration (ms) |
| `K7SC_ENABLED_BRIGHTNESS` | 32 | NeoPixel brightness for ENABLED state |
| `K7SC_MAX_INTENSITY` | 8 | MAX7219 startup intensity (0–15) |
| `K7SC_POLL_INTERVAL_MS` | 5 | Button poll rate (ms) |

---

## Module Registry

| Module | Type ID | I2C Address | Default Value |
|---|---|---|---|
| GPWS Input Panel | `KMC_TYPE_GPWS_INPUT` (0x0B) | 0x2A | 200 m |
| Pre-Warp Time | `KMC_TYPE_PRE_WARP_TIME` (0x0C) | 0x2B | 0 min |

---

## Changelog

### v1.1.0 (2026-04-27)

**Breaking changes:**
- `k7scBegin()` now takes `i2cAddress` as first parameter; `Wire.begin()` is called internally
- `buttonsBulbTest(ms)` → non-blocking `buttonsBulbTest()` + `buttonsBulbTestEnd()`
- `displayTest(ms)` → non-blocking `displayTest()` + `displayTestEnd()`

**Hardware corrections (validated on KC-01-1882 v2.0):**
- NeoPixel mode: GRBW → NEO_GRB 3-byte (white channel not wired on this board)
- Display digit mapping corrected: DIG0=thousands, DIG3=units
- BTN_EN polarity: active LOW → active HIGH with pull-down
- Clock: 3.33 MHz → 20 MHz

**Encoder:**
- Replaced polling with PORTB rising-edge interrupt on ENC_A — no missed edges
- Replaced time-based acceleration with click-count-based (4 tiers: 1/10/100/1000)

**Buttons:**
- Replaced count-based debounce with 30ms time-based debounce
- Release edges no longer assert INT — press-only events
- Added `buttonsSetBrightness()`, `buttonsSetPixelColor()`, `buttonsShow()`

**Display:**
- `displayWake()` writes value before exiting shutdown — no stale value flash
- `KMC_CMD_SET_BRIGHTNESS` sets both display intensity and NeoPixel brightness

**I2C:**
- INT assertion simplified to direct live-state reads at packet send time
- GET_IDENTITY timing fixed

---

## License

GNU General Public License v3.0 — https://www.gnu.org/licenses/gpl-3.0.html  
Code by J. Rostoker, Jeb's Controller Works.
