# Dual Rotary Module – Kerbal Controller Mk1

This is the finalized firmware for the **Dual Rotary Module** in the **Kerbal Controller Mk1** system.  
It handles two rotary encoders with integrated push buttons and communicates with a host microcontroller over I2C.

---

## 📦 Overview

- **MCU**: ATtiny816 (via megaTinyCore)
- **Communication**: I2C (slave address `0x22`)
- **Inputs**:
  - 2 rotary encoders with push buttons
- **Outputs**:
  - I2C event messages containing position and button press states
- **Host Compatibility**: Works with the Kerbal Simpit host-side Arduino firmware
- **Core Library**: Uses [RotaryEncoderCore](../RotaryEncoderCore)

---

## 🚀 Features

- Reads two rotary encoders using pin-change interrupts
- Detects button presses with debounce and tracks short vs long presses
- Sends 8-byte report over I2C on change:
  - Encoder 1 position and button event
  - Encoder 2 position and button event
- Triggers hardware interrupt to notify the host when a change occurs
- Lightweight, interrupt-safe, and modular

---

## 🎛 Encoder Mapping

| Encoder | Signal  | ATtiny816 Pin | Description          |
|---------|---------|---------------|----------------------|
| 1       | A       | PA1           | Rotary encoder A     |
| 1       | B       | PB3           | Rotary encoder B     |
| 1       | Button  | PA5           | Active-high button   |
| 2       | A       | PC3           | Rotary encoder A     |
| 2       | B       | PC2           | Rotary encoder B     |
| 2       | Button  | PC1           | Active-high button   |

---

## 🧪 Button Press Events

| Event Code | Meaning     |
|------------|-------------|
| `0x00`     | No event    |
| `0x01`     | Short press |
| `0x02`     | Long press  |

---

## 📡 I²C Protocol Description

The Dual Rotary Module sends an 8-byte response when polled by the host:

### 📤 Master Read (from module)

When the host performs a read operation, the module responds with:

| Byte | Description                        |
|------|------------------------------------|
| [0]  | Encoder 1 position (MSB)           |
| [1]  | Encoder 1 position (LSB)           |
| [2]  | Encoder 1 button event             |
| [3]  | Encoder 2 position (MSB)           |
| [4]  | Encoder 2 position (LSB)           |
| [5]  | Encoder 2 button event             |
| [6]  | Reserved (0)                       |
| [7]  | Reserved (0)                       |

Interrupt line is pulled LOW when any encoder movement or button press is detected.

---

## 🔧 Setup

The firmware:
- Calls `attachEncoder()` for each encoder in `setup()`
- Registers lambda callbacks for:
  - Encoder movement (`onChange`)
  - Button short press (`onShortPress`)
  - Button long press (`onLongPress`)
- Handles I²C communication using `Wire.onRequest()` and `Wire.onReceive()`
- Debounces button state in `loop()`

---

## 📂 Files

- `dual_encoder.ino` – Main firmware file
- `RotaryEncoderCore` – Shared library (in separate folder)

---

## 🛠 Build Environment

- Arduino IDE or PlatformIO
- Board: ATtiny816 (megaTinyCore)
- Libraries:
  - `Wire`

---

© 2025 Jeb's Controller Works. Licensed under GPLv3.
