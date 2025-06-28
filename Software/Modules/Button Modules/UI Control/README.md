# UI Control Module – Kerbal Controller Mk1

This is the finalized firmware for the **UI Control Module** in the **Kerbal Controller Mk1** system.  
It handles button input and RGB LED feedback for UI-related actions, communicating with a host device over I2C.

---

## 📦 Overview

- **MCU**: ATtiny816 (via megaTinyCore)
- **Communication**: I2C (slave address `0x20`)
- **Inputs**: 16 buttons via 2× 74HC165 shift registers
- **Outputs**:
  - 12 RGB NeoPixel LEDs for button state/status
  - Discrete LED pins are defined but unused in this module
- **Host Compatibility:** Works with the Kerbal Simpit host-side Arduino firmware

---

## 🚀 Features

- Reads 16 input buttons using shift registers
- Controls 12 RGB LEDs via NeoPixels
- Updates 4 discrete LEDs for "lock" or persistent states
- Communicates with host microcontroller over I2C
- Uses color-coded feedback based on a shared color table
- Low memory footprint using `PROGMEM` for color data
- Built-in Bulb Test on startup for verifying LED and output function

---

## 🚀 Features

- Reads 16 input buttons using shift registers
- Controls 12 RGB LEDs via NeoPixels using `tinyNeoPixel`
- Controls 4 discrete output pins based on last 4 button states
- Communicates with host microcontroller over I2C
- Uses a shared color table in PROGMEM for low SRAM usage
- Built-in **Bulb Test** on startup to verify RGB and digital output operation

---

## 🎛 Button Mapping

| Index | Label          | Function        |
|-------|----------------|-----------------|
| 0     | Map Enable     | Toggle map      |
| 1     | Cycle Map -    | Previous map view |
| 2     | Cycle Map +    | Next map view   |
| 3     | Navball Mode   | Toggle navball mode |
| 4     | IVA            | Internal view   |
| 5     | Cycle Cam      | Cycle camera    |
| 6     | Cycle Ship -   | Previous ship   |
| 7     | Cycle Ship +   | Next ship       |
| 8     | Reset Focus    | Reset camera focus |
| 9     | Screen Shot    | Take screenshot |
| 10    | UI             | Toggle UI       |
| 11    | DEBUG          | Debug mode      |
| 12–15 | Discrete Outputs | See below     |

### 🔌 Discrete Outputs

| Index | Label        | Pin Function |
|-------|--------------|--------------|
| 12    | Unused/Reserved  | Discrete LED output (LED13)       |
| 13    | Unused/Reserved  | Discrete LED output (LED14)       |
| 14    | Unused/Reserved  | Discrete LED output (LED15)       |
| 15    | Unused/Reserved  | Discrete LED output (LED16)       |

---

## 💡 LED Color Mapping

| LED Index | Color Index  | Function         |
|-----------|--------------|------------------|
| 0         | AMBER        | Map Enable       |
| 1         | GREEN        | Cycle Map -      |
| 2         | GREEN        | Cycle Map +      |
| 3         | BLUE         | Navball Mode     |
| 4         | AMBER        | IVA              |
| 5         | SKY_BLUE     | Cycle Cam        |
| 6         | GREEN        | Cycle Ship -     |
| 7         | GREEN        | Cycle Ship +     |
| 8         | GREEN        | Reset Focus      |
| 9         | MAGENTA      | Screen Shot      |
| 10        | AMBER        | UI               |
| 11        | RED          | DEBUG            |

Inactive LEDs display `DIM_GRAY`.

---

## 🔧 Setup

The firmware calls `beginModule(panel_addr)` in `setup()`, which:

- Initializes I2C, NeoPixels, shift registers, and discrete outputs
- Runs a **bulb test**:
  - RGB LEDs cycle through red, green, blue, white  
  - Final LED pattern held for 2 seconds  
  - Odd discrete pins set HIGH, even set LOW

---

## 📂 Files

- `FunctionControlModule.ino` – Main firmware file
- `ButtonModuleCore` – Shared library (in separate folder)

---

## 🛠 Build Environment

- Arduino IDE or PlatformIO
- Board: ATtiny816 (megaTinyCore)
- Libraries:
  - `tinyNeoPixel`
  - `ShiftIn`
  - `Wire`

---

© 2025 Jeb's Controller Works. Licensed under GPLv3.
