# Rotation Control Module – Kerbal Controller Mk1

This is the finalized firmware for the **Rotation Control Module** in the **Kerbal Controller Mk1** system.  
It handles joystick input (3 axes), RGB LED feedback, and I2C communication with the host.

---

## 📦 Overview

- **MCU**: ATtiny816 (via megaTinyCore)
- **Communication**: I2C (slave address `0x2C`)
- **Inputs**:
  - 3 discrete buttons (2 RGB, 1 joystick button)
  - 3-axis joystick (analog)
- **Outputs**:
  - 2 RGB NeoPixel LEDs for button status
- **Host Compatibility**: Works with the Kerbal Simpit host-side Arduino firmware
- **Core Library**: Uses [JoystickModuleCore](../JoystickModuleCore)

---

## 🚀 Features

- Reads 3 buttons with debounce logic (including joystick press)
- Captures and scales 3 analog joystick axes to signed 16-bit values
- Controls 2 RGB LEDs via NeoPixels using `tinyNeoPixel`
- Communicates with host microcontroller over I2C
- Uses a shared color table in `PROGMEM` for low SRAM usage
- Triggers interrupts on input change or joystick movement
- **Built-in Bulb Test** on startup for verifying LED functionality

---

## 🎮 Control Mapping

| Index | Label         | Function           |
|-------|---------------|--------------------|
| 0     | Reset Trim    | RGB Button 1       |
| 1     | Trim          | RGB Button 2       |
| 2     | Airbrake      | Joystick Button    |

---

## 🕹 Joystick Axis Mapping

| Axis   | Function        | Range        |
|--------|------------------|--------------|
| Axis 1 | X-axis           | -32768 to 32767 |
| Axis 2 | Y-axis           | -32768 to 32767 |
| Axis 3 | Z-axis (twist)   | -32768 to 32767 |

Deadzone is applied to prevent micro jitter near the center position.

---

## 💡 LED Color Configuration

| LED Index | Label       | Color  |
|-----------|-------------|--------|
| 0         | Reset Trim  | GREEN  |
| 1         | Trim        | AMBER  |

Inactive LEDs display DIM_GRAY.

---

## 🔧 Setup

The firmware calls `beginModule(panel_addr)` in `setup()`, which:
- Initializes I2C, NeoPixels, and button/analog inputs
- Runs a **bulb test**:
  - RGB LEDs cycle through red, green, blue, white  
  - Final LED pattern held for 2 seconds

---

## 📂 Files

- `rotation.ino` – Main firmware file
- `JoystickModuleCore` – Shared library (in separate folder)

---

## 🛠 Build Environment

- Arduino IDE or PlatformIO
- Board: ATtiny816 (megaTinyCore)
- Libraries:
  - `tinyNeoPixel`
  - `Wire`

---

© 2025 Jeb's Controller Works. Licensed under GPLv3.
