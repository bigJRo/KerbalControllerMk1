# Stability Control Module – Kerbal Controller Mk1

This is the finalized firmware for the **Stability Control Module** in the **Kerbal Controller Mk1** system.  
It handles SAS-related input and RGB LED feedback, and communicates with a host device over I2C.

---

## 📦 Overview

- **MCU**: ATtiny816 (via megaTinyCore)  
- **Communication**: I2C (slave address `0x2A`)  
- **Inputs**: 16 buttons via 2× 74HC165 shift registers  
- **Outputs**:  
  - 12 RGB NeoPixel LEDs for SAS status indicators  
  - 4 discrete output pins for SAS/RCS toggles  
- **Host Compatibility:** Works with the Kerbal Simpit host-side Arduino firmware  
- **Core Library:** Uses `ButtonModuleCore`

---

## 🚀 Features

- Reads 16 input buttons using shift registers  
- Controls 12 RGB LEDs via NeoPixels using `tinyNeoPixel`  
- Controls 4 discrete digital outputs  
- Communicates with a host microcontroller over I2C  
- Shared PROGMEM color table minimizes SRAM usage  
- Built-in **Bulb Test** on startup to verify RGB and digital output operation

---

## 🎛 Button Mapping

| Index | Label           | Function                    |
|-------|------------------|-----------------------------|
| 0     | SAS Maneuver     | SAS Mode: Maneuver Node     |
| 1     | SAS Hold         | SAS Mode: Stability Assist  |
| 2     | SAS Retrograde   | SAS Mode: Retrograde        |
| 3     | SAS Prograde     | SAS Mode: Prograde          |
| 4     | SAS Antinormal   | SAS Mode: Anti-Normal       |
| 5     | SAS Normal       | SAS Mode: Normal            |
| 6     | SAS Rad Out      | SAS Mode: Radial Out        |
| 7     | SAS Rad In       | SAS Mode: Radial In         |
| 8     | Anti-Target      | SAS Mode: Anti-Target       |
| 9     | Target           | SAS Mode: Target            |
| 10    | SAS Invert       | SAS Inversion               |
| 11    | Unused           | No function assigned        |
| 12–15 | Discrete Outputs | See below                   |

### 🔌 Discrete Outputs

| Index | Label        | Pin Function |
|-------|--------------|--------------|
| 12    | SAS Enable   | LED13        |
| 13    | RCS Enable   | LED14        |
| 14    | Reserved     | LED15        |
| 15    | Reserved     | LED16        |

---

## 💡 LED Color Mapping

All active SAS functions use `GREEN`. Unused slots default to `BLACK`.  
If a SAS bit is unset, the corresponding LED shows `DIM_GRAY`.

| LED Index | Color Index | Function             |
|-----------|-------------|----------------------|
| 0–10      | GREEN       | SAS Mode indicators  |
| 11        | BLACK       | Unused position      |

---

## 🔧 Setup

The firmware calls `beginModule(panel_addr)` in `setup()`, which:

- Initializes I2C, shift registers, NeoPixels, and output pins  
- Runs a **bulb test**:
  - RGB LEDs cycle through red, green, blue, white  
  - Final LED pattern held for 2 seconds  
  - Odd discrete pins set HIGH, even set LOW

---

## 📂 Files

- `StabilityControlModule.ino` – Main firmware file  
- `ButtonModuleCore` – Shared core library (in `/ButtonModuleCore`)

---

## 🛠 Build Environment

- **IDE**: Arduino IDE or PlatformIO  
- **Board**: ATtiny816 (megaTinyCore)  
- **Libraries**:
  - `tinyNeoPixel`
  - `ShiftIn`
  - `Wire`

---

© 2025 Jeb's Controller Works. Licensed under **GPLv3**.
