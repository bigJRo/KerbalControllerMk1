# Vehicle Control Module – Kerbal Controller Mk1

This is the finalized firmware for the **Vehicle Control Module** in the **Kerbal Controller Mk1** system.  
It handles vehicle-related button input and LED feedback, and communicates with a host device over I2C.

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
| 0     | Brake         | Vehicle brake       |
| 1     | Lights        | Toggle lights       |
| 2     | Solar         | Deploy solar panels |
| 3     | Gear          | Landing gear        |
| 4     | Cargo         | Open cargo bay      |
| 5     | Antenna       | Deploy antenna      |
| 6     | Ladder        | Extend ladder       |
| 7     | Radiator      | Activate radiator   |
| 8     | DrogueDep     | Drogue deploy       |
| 9     | MainDep       | Main deploy         |
| 10    | DrogueCut     | Drogue cut          |
| 11    | MainCut       | Main cut            |
| 12–15 | Lock States   | LED13–LED16 outputs |

---

## 💡 LED Color Mapping

| LED Index | Color Index     | Function            |
|-----------|------------------|---------------------|
| 0         | RED              | Brake               |
| 1         | GOLDEN_YELLOW    | Lights              |
| 2         | SKY_BLUE         | Solar               |
| 3         | GREEN            | Gear                |
| 4         | MINT             | Cargo               |
| 5         | MAGENTA          | Antenna             |
| 6         | CYAN             | Ladder              |
| 7         | LIME             | Radiator            |
| 8         | ORANGE           | Drogue Deploy       |
| 9         | AMBER            | Main Deploy         |
| 10        | RED              | Drogue Cut          |
| 11        | RED              | Main Cut            |

---

## 🔧 Setup

The firmware calls `beginModule(panel_addr)` in `setup()`, which:
- Initializes I2C, NeoPixels, shift registers, and discrete outputs
- Runs a **bulb test** on startup (cycles NeoPixels through R, G, B, W and flashes outputs)

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
