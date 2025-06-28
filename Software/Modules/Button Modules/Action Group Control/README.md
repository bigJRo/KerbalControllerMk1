# Action Group Control Module – Kerbal Controller Mk1

This is the finalized firmware for the **Action Group Control Module** in the **Kerbal Controller Mk1** system.  
It handles action group toggling, RGB LED feedback, and I2C communication with the host.

---

## 📦 Overview

- **MCU**: ATtiny816 (via megaTinyCore)
- **Communication**: I2C (slave address `0x23`)
- **Inputs**: 16 buttons via 2× 74HC165 shift registers
- **Outputs**:
  - 12 RGB NeoPixel LEDs for button state/status
  - 4 discrete output pins (Lock LEDs)
- **Host Compatibility**: Works with the Kerbal Simpit host-side Arduino firmware
- **Core Library**: Uses [ButtonModuleCore](../ButtonModuleCore)

---

## 🚀 Features

- Reads 16 input buttons using shift registers
- Controls 12 RGB LEDs via NeoPixels using `tinyNeoPixel`
- Updates 4 discrete outputs (lock states)
- Communicates with host microcontroller over I2C
- Uses a shared color table in `PROGMEM` for low SRAM usage
- Module-specific LED logic for parachute overlay handling
- **Built-in Bulb Test** on startup for verifying LED and output function

---

## 🎛 Button Mapping

| Index | Label         | Function            |
|-------|---------------|---------------------|
| 0     | AG1           | Action Group #1     |
| 1     | AG2           | Action Group #2     |
| 2     | AG3           | Action Group #3     |
| 3     | AG4           | Action Group #4     |
| 4     | AG5           | Action Group #5     |
| 5     | AG6           | Action Group #6     |
| 6     | AG7           | Action Group #7     |
| 7     | AG8           | Action Group #8     |
| 8     | AG9           | Action Group #9     |
| 9     | AG10          | Action Group #10    |
| 10    | AG11          | Action Group #11    |
| 11    | AG12          | Action Group #12    |
| 12–15 | Discrete Outputs | See below        |

### 🔌 Discrete Outputs

| Index | Label        | Pin Function |
|-------|--------------|--------------|
| 12    | Unused       | Discrete LED output (LED13) |
| 13    | Unused       | Discrete LED output (LED14) |
| 14    | Unused       | Discrete LED output (LED15) |
| 15    | Unused       | Discrete LED output (LED16) |

---

## 💡 LED Color Configuration

| LED Index | Label | Color  |
|-----------|-------|--------|
| 0–11      | AG1–12| GREEN  |

Inactive LEDs display DIM_GRAY.

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

- `VehicleControlModule.ino` – Main firmware file
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


---
