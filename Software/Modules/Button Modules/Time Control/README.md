# Time Control Module – Kerbal Controller Mk1

This is the finalized firmware for the **Time Control Module** in the **Kerbal Controller Mk1** system.  
It handles time-warp and save/load-related button input with RGB LED feedback, communicating with a host device over I2C.

---

## 📦 Overview

- **MCU**: ATtiny816 (via megaTinyCore)  
- **Communication**: I2C (slave address `0x24`)  
- **Inputs**: 16 buttons via 2× 74HC165 shift registers  
- **Outputs**:  
  - 12 RGB NeoPixel LEDs for time control functions  
  - No discrete outputs used  
- **Host Compatibility:** Works with the Kerbal Simpit host-side Arduino firmware  
- **Core Library:** Uses `ButtonModuleCore`

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

| Index | Label           | Function                   |
|-------|------------------|----------------------------|
| 0     | Pause            | Pause game time            |
| 1     | Warp to Morn     | Warp to Morning            |
| 2     | Warp to SOI      | Warp to Sphere of Influence|
| 3     | Warp to MNVR     | Warp to Maneuver Node      |
| 4     | Warp to PeA      | Warp to Periapsis          |
| 5     | Warp to ApA      | Warp to Apoapsis           |
| 6     | Save             | Quick Save                 |
| 7     | Load             | Quick Load                 |
| 8     | Warp             | Standard Warp              |
| 9     | Warp +           | Increase Warp              |
| 10    | Physics Warp     | Physics Warp Mode          |
| 11    | Cancel Warp      | Cancel Warp (BLACK when off) |
| 12–15 | Discrete Outputs | See below                         |

### 🔌 Discrete Outputs

| Index | Label        | Pin Function |
|-------|--------------|--------------|
| 12    | Reserved / Unused  | Discrete LED output (LED13)       |
| 13    | Reserved / Unused  | Discrete LED output (LED14)       |
| 14    | Reserved / Unused  | Discrete LED output (LED15)       |
| 15    | Reserved / Unused  | Discrete LED output (LED16)       |

---

## 💡 LED Color Mapping

| LED Index | Color Index     | Function            |
|-----------|------------------|---------------------|
| 0         | AMBER            | Pause               |
| 1–5       | GOLDEN_YELLOW    | Warp targets        |
| 6–7       | SKY_BLUE         | Save / Load         |
| 8–10      | GOLDEN_YELLOW    | Warp/Physics warp   |
| 11        | RED / BLACK      | Cancel Warp         |

- All inactive LEDs show **DIM_GRAY** except LED 11, which shows **BLACK** when off

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
