# Kerbal7SegmentCore

**Version:** 1.0.0  
**Author:** J. Rostoker — Jeb's Controller Works  
**License:** GNU General Public License v3.0 (GPL-3.0)  
**Hardware:** KC-01-1881/1882 7-Segment Display Module v2.0  
**Target MCU:** ATtiny816 (megaTinyCore)  

---

## Overview

Kerbal7SegmentCore is the Arduino library for Kerbal Controller Mk1 7-segment display modules. Each module runs on an ATtiny816 and acts as an I2C target device, displaying a configurable value on a 4-digit 7-segment display and reporting button state and the current display value to the system controller.

The library manages:

- 4-digit 7-segment display via MAX7219CWG+T (software SPI)
- No-leading-zero digit formatting, range 0–9999
- Rotary encoder input with three-tier acceleration
- Hardware-debounced encoder (10nF RC caps on ENC_A/ENC_B)
- Three SK6812MINI-EA NeoPixel GRBW buttons with four configurable behavior modes
- Encoder pushbutton (direct GPIO, no LED)
- I2C target communication (Kerbal Controller Mk1 I2C protocol)
- GRBW white channel for ENABLED dim backlight state

This is **not** a KerbalButtonCore (KBC) module. It shares the same I2C wire protocol but uses a device-specific 6-byte data packet and has an additional command for setting the display value.

---

## Hardware

**PCB:** KC-01-1881 (schematic) / KC-01-1882 (board)  
**MCU:** ATtiny816-MNR  
**Display IC:** MAX7219CWG+T  
**Display:** FJ4401AG 4-digit 7-segment (common cathode)  
**LEDs:** SK6812MINI-EA (GRBW, 4 bytes per pixel)  
**Encoder:** PEC11R-4220F-S0024 with hardware RC debounce  

### Pin Assignment

| ATtiny816 Pin | Net | Function |
|---|---|---|
| PA1 (pin 20) | INT | Interrupt output (active low) |
| PA4 (pin 5) | BUTTON03 | NeoPixel button 3 (active high) |
| PA5 (pin 6) | DATA_IN | MAX7219 SPI data |
| PA6 (pin 7) | LOAD_DATA | MAX7219 SPI latch / CS |
| PA7 (pin 8) | CLK | MAX7219 SPI clock |
| PB0 (pin 14) | SCL | I2C clock |
| PB1 (pin 13) | SDA | I2C data |
| PB4 (pin 10) | ENC_A | Encoder channel A (hardware debounced) |
| PB5 (pin 9) | ENC_B | Encoder channel B (hardware debounced) |
| PC0 (pin 15) | BUTTON02 | NeoPixel button 2 (active high) |
| PC1 (pin 16) | BUTTON01 | NeoPixel button 1 (active high) |
| PC2 (pin 17) | NEOPIX_CMD | SK6812MINI-EA data output |
| PC3 (pin 18) | BUTTON_EN | Encoder pushbutton (active high, no LED) |

---

## Dependencies

- **tinyNeoPixel_Static** — included with megaTinyCore, no separate install needed

No other dependencies. MAX7219 communication is bit-banged SPI implemented in the library.

---

## Installation

1. Download or clone this repository
2. Place the `Kerbal7SegmentCore` folder in your Arduino `libraries` directory
3. In Arduino IDE: Tools → Board → megaTinyCore → ATtiny816
4. Tools → tinyNeoPixel Port → **Port C** (required for PC2)
5. Tools → Clock → 10 MHz or higher

---

## SK6812MINI-EA vs WS2811

This library uses SK6812MINI-EA LEDs, which are **GRBW** format — four channels per pixel (Green, Red, Blue, White) rather than three. The dedicated white channel is used for the ENABLED dim backlight state, providing a cleaner neutral white than scaling RGB down.

All colors in `K7SC_Colors.h` use the `GRBWColor` struct with `w=0` for color states and `w=brightness` for the ENABLED white state. Do not substitute WS2811 library calls directly.

---

## Button Behavior Modes

Each of the three NeoPixel buttons is independently configured via a `ButtonConfig` struct:

| Mode | Constant | Description |
|---|---|---|
| Cycle | `BTN_MODE_CYCLE` | Multi-state toggle — each press advances through defined states |
| Toggle | `BTN_MODE_TOGGLE` | Binary on/off — press alternates between two states |
| Flash | `BTN_MODE_FLASH` | Momentary color flash — illuminates briefly on press, returns to ENABLED |
| Momentary | `BTN_MODE_MOMENTARY` | No LED — triggers action only |

The `ButtonConfig` struct:

```cpp
struct ButtonConfig {
    ButtonMode  mode;
    GRBWColor   colors[3];   // state colors (usage varies by mode — see below)
    uint8_t     numStates;   // CYCLE: total states including off
    uint16_t    flashMs;     // FLASH: override duration (0 = K7SC_FLASH_DURATION_MS)
};
```

**CYCLE** — `colors[0]` = off state, `colors[1]` = state 1, `colors[2]` = state 2. `numStates` = total states (2 or 3).  
**TOGGLE** — `colors[0]` unused (library uses ENABLED), `colors[1]` = active color.  
**FLASH** — `colors[0]` = flash color. Returns to ENABLED dim white after `flashMs`.  
**MOMENTARY** — colors not used.

---

## Encoder Acceleration

Step size is determined by the time between encoder clicks:

| Speed | Click interval | Step size |
|---|---|---|
| Slow | > 150 ms | ±1 |
| Medium | 50–150 ms | ±10 |
| Fast | < 50 ms | ±100 |

All thresholds are `#ifndef`-guarded constants in `K7SC_Config.h` and can be overridden per sketch. Values clamp at 0 and 9999.

---

## Quick Start

```cpp
#include <Wire.h>
#include <Kerbal7SegmentCore.h>

// Define button behavior for each NeoPixel button
const ButtonConfig btnConfigs[K7SC_NEO_COUNT] = {
    // BTN01 — 3-state cycle: off → GREEN → AMBER → off
    { BTN_MODE_CYCLE, {K7SC_OFF, K7SC_GREEN, K7SC_AMBER}, 3, 0 },
    // BTN02 — toggle: off ↔ GREEN
    { BTN_MODE_TOGGLE, {K7SC_OFF, K7SC_GREEN, K7SC_OFF}, 2, 0 },
    // BTN03 — toggle: off ↔ GREEN
    { BTN_MODE_TOGGLE, {K7SC_OFF, K7SC_GREEN, K7SC_OFF}, 2, 0 },
};

void setup() {
    Wire.begin(0x2A);
    k7scBegin(0x0B, K7SC_CAP_DISPLAY, btnConfigs, 200);  // start at 200
}

void loop() {
    k7scUpdate();

    // Encoder button resets display value
    if (buttonsGetEncoderPress()) {
        encoderSetValue(200);
    }
}
```

---

## I2C Protocol

**Address:** Set per sketch (0x2A = GPWS Input, 0x2B = Pre-Warp Time)  
**Speed:** 400 kHz recommended

### Data Packet (module → controller, 6 bytes)

```
Byte 0:   Button events  (bit0=BTN01, bit1=BTN02, bit2=BTN03, bit3=BTN_EN)
Byte 1:   Change mask    (same bit layout)
Byte 2:   Module state   (bits 0-1 = BTN01 cycle state,
                          bit 2 = BTN02 active, bit 3 = BTN03 active)
Byte 3:   Reserved       (always 0x00)
Byte 4:   Value HIGH     (display value, big-endian uint16)
Byte 5:   Value LOW      (display value, big-endian uint16)
```

### Additional Command — Set Display Value

`K7SC_CMD_SET_VALUE (0x09)` — controller sets the display value directly:
```
[ADDR+W] [0x09] [VALUE_HIGH] [VALUE_LOW]
```
Value is big-endian uint16, clamped to 0–9999. The encoder tracking state is updated to match so subsequent encoder turns continue from the new value.

### Standard Commands

All standard Kerbal Controller Mk1 commands are supported (0x01–0x08). `CMD_SET_LED_STATE` is accepted but ignored — button LED states are managed entirely by the module. `CMD_SLEEP` shuts down the display and clears LEDs. `CMD_WAKE` restores both.

---

## Default Constants

All `#ifndef`-guarded — override by defining before the include in your sketch:

| Constant | Default | Description |
|---|---|---|
| `K7SC_ENC_SLOW_MS` | 150 | Slow/medium threshold in ms |
| `K7SC_ENC_FAST_MS` | 50 | Medium/fast threshold in ms |
| `K7SC_STEP_SLOW` | 1 | Step size at slow speed |
| `K7SC_STEP_MEDIUM` | 10 | Step size at medium speed |
| `K7SC_STEP_FAST` | 100 | Step size at fast speed |
| `K7SC_ENC_DEBOUNCE_MS` | 2 | Software debounce guard (ms) |
| `K7SC_BTN_DEBOUNCE_COUNT` | 4 | Reads to confirm button change |
| `K7SC_FLASH_DURATION_MS` | 150 | Default flash button duration |
| `K7SC_ENABLED_BRIGHTNESS` | 32 | W channel brightness for ENABLED state |
| `K7SC_MAX_INTENSITY` | 8 | MAX7219 display intensity (0-15) |
| `K7SC_POLL_INTERVAL_MS` | 5 | Button and encoder poll rate |

---

## Module Registry

| Module | Type ID | I2C Address | Initial Value |
|---|---|---|---|
| GPWS Input Panel | `0x0B` | `0x2A` | 200 (meters) |
| Pre-Warp Time | `0x0C` | `0x2B` | 0 (minutes) |

---

## File Structure

```
Kerbal7SegmentCore/
├── README.md
├── library.properties
├── src/
│   ├── Kerbal7SegmentCore.h/.cpp  — top-level include and update loop
│   ├── K7SC_Config.h              — pins, constants, thresholds
│   ├── K7SC_Colors.h              — GRBW palette and helpers
│   ├── K7SC_Display.h/.cpp        — MAX7219 driver (software SPI)
│   ├── K7SC_Encoder.h/.cpp        — encoder input with acceleration
│   ├── K7SC_Buttons.h/.cpp        — button input and SK6812 LED control
│   └── K7SC_I2C.h/.cpp            — I2C target and INT management
└── examples/
    ├── BasicDisplayModule/        — minimal sketch template
    └── DiagnosticDump/            — serial debug and hardware verification
```

---

## License

Licensed under the GNU General Public License v3.0 (GPL-3.0).  
See https://www.gnu.org/licenses/gpl-3.0.html

Code written by J. Rostoker for Jeb's Controller Works.
