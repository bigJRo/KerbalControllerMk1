
# Vehicle Control Module – Kerbal Controller Mk1

This is the firmware for the **Vehicle Control Module** in the *Kerbal Controller Mk1* project, designed by J. Rostoker for Jeb's Controller Works.  
Inspired by [UntitledSpaceCraft](https://github.com/CodapopKSP/UntitledSpaceCraft) and adapted to run on an ATtiny816 using the Arduino framework.

## Features

- 16-button interface using shift registers
- 12 NeoPixel status LEDs with per-function coloring
- 4 discrete output lock-state LEDs
- I2C communication with a host (Kerbal Simpit-compatible)
- Highly memory-efficient color control using `PROGMEM`
- Supports overlay LED logic (e.g., parachute deployment stages)

## Hardware Overview

### Microcontroller: ATtiny816 (megaTinyCore)
- All GPIO mapped using megaTinyCore pin notation

### Shift Register Input (16 buttons):
- Uses `ShiftIn` library to read 2 chained 8-bit registers

### NeoPixel LEDs:
- 12 individually addressable WS2812 LEDs

### Discrete Output LEDs:
- 4 lock-state LEDs connected to digital output pins

## Pin Assignments

| Function        | ATtiny Pin | Physical | Description                     |
|----------------|------------|----------|---------------------------------|
| `SDA`          | PB1        | 9        | I2C Data                        |
| `SCL`          | PB0        | 8        | I2C Clock                       |
| `INT_OUT`      | PA4        | 4        | Interrupt to host (active low) |
| `load`         | PA7        | 7        | Shift register latch            |
| `clockEnable`  | PA6        | 6        | Shift register clock enable     |
| `clockIn`      | PB5        | 13       | Shift register clock            |
| `dataInPin`    | PA5        | 5        | Shift register data             |
| `neopixCmd`    | PB4        | 12       | NeoPixel data                   |
| `led_13`       | PB3        | 11       | Discrete LED 1 (Brake Lock)     |
| `led_14`       | PC2        | 15       | Discrete LED 2 (Parachute Lock)|
| `led_15`       | PB2        | 10       | Discrete LED 3 (Lights Lock)    |
| `led_16`       | PC1        | 14       | Discrete LED 4 (Gear Lock)      |

## Communication Protocol

I2C Slave Address: `0x23`

- **Master reads (4 bytes):**
  - Byte 0: Button bits 0–7
  - Byte 1: Button bits 8–15
  - Byte 2: LED bits 0–7
  - Byte 3: LED bits 8–15

- **Master writes (2 bytes):**
  - Byte 0: LED bits 0–7
  - Byte 1: LED bits 8–15

## LED Behavior

- **LEDs 0–7**: Follow button colors from `pixel_Array`
- **LEDs 8–11**: Overlay logic using `overlayColor()` depending on bit 13 (enabled), and bits 8/9 (deployed)
- **LEDs 12–15**: Discrete lock indicator LEDs (digital HIGH/LOW)

## Host Requirements

This module should be connected to a host microcontroller (e.g., Arduino) that:
- Sends LED control signals over I2C
- Receives button states from the panel
- Uses the [Kerbal Simpit](https://github.com/digitalcircuit/kerbal-simpit) protocol for game integration

## License

This firmware is licensed under the GNU GPLv3.
Original inspiration and framework courtesy of CodapopKSP.
