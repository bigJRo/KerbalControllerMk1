# KerbalButtonCore

**Version:** 1.0.0  
**Author:** J. Rostoker — Jeb's Controller Works  
**License:** GNU General Public License v3.0 (GPL-3.0)  
**Hardware:** KC-01-1822 Button Module Base v1.1  
**Target MCU:** ATtiny816 (megaTinyCore)  

---

## Overview

KerbalButtonCore is the Arduino library for Kerbal Controller Mk1 button input modules. Each module runs on an ATtiny816 and acts as an I2C target device, reporting button press/release events to a system controller and receiving LED state commands in return.

The library manages:

- 16 button inputs via two daisy-chained SN74HC165PWR shift registers
- 12 RGB NeoPixel buttons (WS2811, tinyNeoPixel_Static)
- 4 discrete LED outputs (2N3904 NPN switched, on/off only)
- I2C target communication (full KBC protocol v1.1)
- Dual-buffer button state latching with guaranteed edge detection
- Per-button LED state machine with core and extended states
- Non-blocking flash timing for WARNING and ALERT states
- Sleep/wake power management

Full protocol specification: [`KBC_Protocol_Spec.md`](../KBC_Protocol_Spec.md)

---

## Hardware

All modules share the common PCB design KC-01-1822 v1.1 based on the ATtiny816-MNR.

### Pin Assignment

| ATtiny816 Pin | Net Name     | Function                          |
|---------------|--------------|-----------------------------------|
| PA1 (pin 20)  | INT          | Interrupt output (active low)     |
| PA4 (pin 5)   | NEOPIX_CMD   | NeoPixel data output              |
| PA5 (pin 6)   | DATA_IN      | Shift register serial data        |
| PA6 (pin 7)   | CLK_EN       | Shift register clock enable       |
| PA7 (pin 8)   | SHIFT_LD     | Shift register parallel load      |
| PB0 (pin 14)  | SCL          | I2C clock                         |
| PB1 (pin 13)  | SDA          | I2C data                          |
| PB3 (pin 11)  | LED13        | Discrete LED — KBC index 12       |
| PB5 (pin 9)   | CLK_IN       | Shift register clock              |
| PC0 (pin 15)  | LED14        | Discrete LED — KBC index 13       |
| PC1 (pin 16)  | LED16        | Discrete LED — KBC index 15       |
| PC2 (pin 17)  | LED15        | Discrete LED — KBC index 14       |

No-connect pins: PA2 (1), PA3 (2), PB4 (10), PB2 (12), PC3 (18).

### Button Index Convention

| KBC Index | PCB Label   | Hardware Type  |
|-----------|-------------|----------------|
| 0 – 11    | BUTTON01–12 | NeoPixel RGB   |
| 12 – 15   | BUTTON13–16 | Discrete LED   |

---

## Dependencies

Install via Arduino Library Manager or manually:

- **ShiftIn** — InfectedBytes/ArduinoShiftIn  
  `https://github.com/InfectedBytes/ArduinoShiftIn`
- **tinyNeoPixel_Static** — included with megaTinyCore  
  `https://github.com/SpenceKonde/megaTinyCore`

---

## Installation

1. Download or clone this repository
2. Place the `KerbalButtonCore` folder in your Arduino `libraries` directory
3. Install dependencies listed above
4. In Arduino IDE: Tools → Board → megaTinyCore → ATtiny816
5. Tools → tinyNeoPixel Port → **Port A** (required for PIN_PA4)
6. Tools → Clock → 10 MHz or higher (required for NeoPixel timing)

---

## Quick Start

```cpp
#include <Wire.h>
#include <KerbalButtonCore.h>

// Define the ACTIVE color for each of the 16 buttons
const RGBColor activeColors[KBC_BUTTON_COUNT] = {
    KBC_SKY,    // B0  — Map Enable
    KBC_RED,    // B1  — Debug
    KBC_SKY,    // B2  — Map Forward
    KBC_AMBER,  // B3  — Nav Ball
    KBC_SKY,    // B4  — Map Reset
    KBC_GREEN,  // B5  — Cycle Nav
    KBC_SKY,    // B6  — Map Back
    KBC_TEAL,   // B7  — Ship Forward
    KBC_SKY,    // B8  — Map Back
    KBC_TEAL,   // B9  — Ship Back
    KBC_SKY,    // B10 — Map Enable
    KBC_CORAL,  // B11 — IVA
    KBC_GREEN,  // B12 — discrete (not used on UI module)
    KBC_GREEN,  // B13 — discrete (not used on UI module)
    KBC_GREEN,  // B14 — discrete (not used on UI module)
    KBC_GREEN,  // B15 — discrete (not used on UI module)
};

// Instantiate — Type ID and capability flags defined in KBC_Protocol.h
KerbalButtonCore kbc(KBC_TYPE_UI_CONTROL, 0, activeColors);

void setup() {
    Wire.begin(0x20);   // Set this module's I2C address — must be first
    kbc.begin();        // Initialise all subsystems
}

void loop() {
    kbc.update();       // Poll buttons, service LEDs, sync INT — all here
}
```

That's the complete sketch. All I2C communication, button debouncing, LED updates, and INT management happen inside `update()`.

---

## I2C Protocol Summary

**Address range:** 0x20 – 0x2E (set per sketch)  
**Speed:** 400 kHz recommended  

### Commands (controller → module)

| Command              | Byte   | Payload         |
|----------------------|--------|-----------------|
| CMD_GET_IDENTITY     | `0x01` | none            |
| CMD_SET_LED_STATE    | `0x02` | 8 bytes         |
| CMD_SET_BRIGHTNESS   | `0x03` | 1 byte (0–255)  |
| CMD_BULB_TEST        | `0x04` | none            |
| CMD_SLEEP            | `0x05` | none            |
| CMD_WAKE             | `0x06` | none            |
| CMD_RESET            | `0x07` | none            |
| CMD_ACK_FAULT        | `0x08` | none            |

### Button State Response (module → controller, 4 bytes)

```
Byte 0-1: Current state bitmask  (bit N = button N, 1=pressed)
Byte 2-3: Change mask            (bit N = button N changed since last read)
```

### LED State Payload (8 bytes, nibble-packed)

Two buttons per byte, high nibble first. Nibble values:

| Value | State          | Behavior                     |
|-------|----------------|------------------------------|
| `0x0` | OFF            | Unlit                        |
| `0x1` | ENABLED        | Dim white backlight          |
| `0x2` | ACTIVE         | Full brightness, module color |
| `0x3` | WARNING        | Flashing amber, 500ms        |
| `0x4` | ALERT          | Flashing red, 150ms          |
| `0x5` | ARMED          | Static cyan                  |
| `0x6` | PARTIAL_DEPLOY | Static amber                 |

### Identity Response (4 bytes)

```
Byte 0: Module Type ID
Byte 1: Firmware major
Byte 2: Firmware minor
Byte 3: Capability flags
```

---

## LED States

### Core States (all modules)

| State   | NeoPixel                        | Discrete LED  |
|---------|---------------------------------|---------------|
| OFF     | Off                             | Off           |
| ENABLED | Dim white (scaled by brightness)| On (full)     |
| ACTIVE  | Per-button color, full bright   | On (full)     |

### Extended States (capability-flagged modules)

| State          | Color | Behavior              |
|----------------|-------|-----------------------|
| WARNING        | Amber | 500ms on / 500ms off  |
| ALERT          | Red   | 150ms on / 150ms off  |
| ARMED          | Cyan  | Static full bright    |
| PARTIAL_DEPLOY | Amber | Static full bright    |

To enable extended states, pass `KBC_CAP_EXTENDED_STATES` as the capability flags argument to the constructor.

---

## Color Palette

All named colors are defined in `KBC_Colors.h`.

| Constant          | RGB            | Primary Use                    |
|-------------------|----------------|--------------------------------|
| `KBC_GREEN`       | 34, 197, 94    | Default active / nominal       |
| `KBC_RED`         | 239, 68, 68    | Irreversible actions           |
| `KBC_AMBER`       | 245, 158, 11   | Awareness / caution            |
| `KBC_YELLOW`      | 234, 179, 8    | Lights / warp targets          |
| `KBC_GOLD`        | 255, 200, 0    | Solar array / power            |
| `KBC_ORANGE`      | 249, 115, 22   | Engine / thermal               |
| `KBC_CHARTREUSE`  | 163, 230, 53   | Engine Grp 2 / physics warp    |
| `KBC_LIME`        | 132, 204, 22   | Ladder / save                  |
| `KBC_MINT`        | 150, 255, 200  | Load / restore                 |
| `KBC_BLUE`        | 59, 130, 246   | Science Grp 2                  |
| `KBC_SKY`         | 14, 165, 233   | Map family                     |
| `KBC_TEAL`        | 20, 184, 166   | Ship / atmosphere              |
| `KBC_CYAN`        | 6, 182, 212    | Air brake / ARMED state        |
| `KBC_PURPLE`      | 139, 92, 246   | Science collect                |
| `KBC_INDIGO`      | 99, 102, 241   | Science Grp 1                  |
| `KBC_VIOLET`      | 180, 106, 255  | Cargo door                     |
| `KBC_CORAL`       | 255, 100, 54   | IVA                            |
| `KBC_ROSE`        | 255, 80, 120   | Control points                 |
| `KBC_PINK`        | 236, 72, 153   | Antenna / comms                |
| `KBC_WHITE_COOL`  | 255, 255, 255  | ENABLED backlight              |
| `KBC_WHITE_WARM`  | 255, 200, 150  | Warp rate                      |
| `KBC_WHITE_SOFT`  | 180, 184, 204  | Low-light environments         |
| `KBC_OFF`         | 0, 0, 0        | Unlit                          |

Custom colors: `KBC_COLOR(r, g, b)` macro or direct `RGBColor` struct.

---

## Module Type ID Registry

| ID     | Module              |
|--------|---------------------|
| `0x01` | UI Control          |
| `0x02` | Function Control    |
| `0x03` | Action Control      |
| `0x04` | Stability Control   |
| `0x05` | Vehicle Control     |
| `0x06` | Time Control        |
| `0xFF` | Unknown             |

---

## Overriding Defaults

Any `KBC_Config.h` constant guarded by `#ifndef` can be overridden per sketch by defining it before the include:

```cpp
#define KBC_ENABLED_BRIGHTNESS  64    // brighter ENABLED backlight
#define KBC_WARNING_ON_MS       300   // faster warning flash
#define KBC_WARNING_OFF_MS      300
#include <KerbalButtonCore.h>
```

---

## Advanced Usage

For module sketches that need extended state behavior, the subsystems are accessible directly:

```cpp
// Set a single button to ARMED state and render immediately
kbc.ledControl().setButtonState(3, KBC_LED_ARMED);
kbc.ledControl().render();

// Read current debounced state of button 5
bool pressed = kbc.shiftReg().getButtonState(5);

// Report a hardware fault to the controller
kbc.reportFault();
```

---

## File Structure

```
KerbalButtonCore/
├── README.md
├── library.properties
├── src/
│   ├── KerbalButtonCore.h      — top-level include (sketches use this only)
│   ├── KerbalButtonCore.cpp
│   ├── KBC_Config.h            — pin definitions and timing constants
│   ├── KBC_Protocol.h          — I2C protocol constants and packet structs
│   ├── KBC_Colors.h            — RGB color palette and helpers
│   ├── KBC_ShiftReg.h/.cpp     — 74HC165 shift register abstraction
│   ├── KBC_LEDControl.h/.cpp   — LED state machine
│   └── KBC_I2C.h/.cpp          — I2C target command handler
└── examples/
    ├── BasicModule/            — minimal core-states-only module sketch
    ├── ExtendedModule/         — module with WARNING/ALERT/ARMED states
    └── DiagnosticDump/         — serial debug output of button and LED state
```

---

## License

Licensed under the GNU General Public License v3.0 (GPL-3.0).  
See https://www.gnu.org/licenses/gpl-3.0.html

Code written by J. Rostoker for Jeb's Controller Works.
