# Function Control Module – Kerbal Controller Mk1

This is the finalized firmware for the **Function Control Module** in the **Kerbal Controller Mk1** system.  
It handles function-related inputs (like engine groups, science, control modes), RGB LED feedback, and I2C communication with the host.

---

## 📦 Overview

- **MCU**: ATtiny816 (via megaTinyCore)
- **Communication**: I2C (slave address `0x28`)
- **Inputs**: 16 buttons via 2× 74HC165 shift registers
- **Outputs**:
  - 12 RGB NeoPixel LEDs for button state/status
  - 4 discrete output pins (Throttle Lock, Precision, SCE, Audio)
- **Host Compatibility**: Works with the Kerbal Simpit host-side Arduino firmware
- **Core Library**: Uses `ButtonModuleCore`

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

| Index | Label           | Function                          |
|-------|------------------|-----------------------------------|
| 0     | Air Intake       | Activate air intake               |
| 1     | Launch Escape    | Trigger escape tower              |
| 2     | Sci Collect      | Trigger science collection        |
| 3     | Engine Alt       | Alternate engine configuration    |
| 4     | Science Grp 1    | Activate science group 1          |
| 5     | Engine Grp 1     | Activate engine group 1           |
| 6     | Science Grp 2    | Activate science group 2          |
| 7     | Engine Grp 2     | Activate engine group 2           |
| 8     | Ctrl Pt Alt      | Alternate control point           |
| 9     | Ctrl Pt Pri      | Primary control point             |
| 10    | Lock Ctrl        | Lock control mode                 |
| 11    | Heat Shield      | Activate heat shield              |
| 12–15 | Discrete Outputs | See below                         |

### 🔌 Discrete Outputs

| Index | Label        | Pin Function |
|-------|--------------|--------------|
| 12    | Throttle Lock    | Discrete LED output (LED13)       |
| 13    | Precision        | Discrete LED output (LED14)       |
| 14    | SCE              | Discrete LED output (LED15)       |
| 15    | Audio            | Discrete LED output (LED16)       |


---

## 💡 LED Color Mapping

| LED Index | Color Index   | Function         |
|-----------|----------------|------------------|
| 0         | SKY_BLUE       | Air Intake       |
| 1         | RED            | Launch Escape    |
| 2         | BLUE           | Sci Collect      |
| 3         | AMBER          | Engine Alt       |
| 4         | CYAN           | Science Grp 1    |
| 5         | GREEN          | Engine Grp 1     |
| 6         | CYAN           | Science Grp 2    |
| 7         | GREEN          | Engine Grp 2     |
| 8         | LIME           | Ctrl Pt Alt      |
| 9         | LIME           | Ctrl Pt Pri      |
| 10        | AMBER          | Lock Ctrl        |
| 11        | MINT           | Heat Shield      |

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
