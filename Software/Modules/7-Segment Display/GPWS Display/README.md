# GPWS Display Module – Kerbal Controller Mk1 Device

This is the firmware for the **GPWS Display Module** used in the **Kerbal Controller Mk1** system.  
It handles numeric display output, RGB LED control for pushbuttons, rotary encoder input, and I²C communication with the host.

---

## 📦 Overview

- **Module Name**: GPWS Display Module
- **MCU Target**: ATtiny816 (via megaTinyCore)
- **Communication**: I²C (slave device, address `0x21`)
- **Inputs**:
  - Rotary encoder with pushbutton
  - 3 discrete pushbuttons (active HIGH)
- **Outputs**:
  - 3 RGB NeoPixel LEDs (one per button)
  - 4-digit 7-segment LED display via MAX7219
  - Interrupt pin to host (active LOW)
- **Special Functions**:
  - Bulb test on startup
  - Digit edit mode with flashing segment
  - Short and long press detection for each button

---

## 🚀 Features

- 🔄 **I²C Communication**: Simple 4-byte protocol for bidirectional communication with host.
- 🔢 **Digit Display**: Displays a 16-bit numeric value on 4-digit 7-segment display.
- ✏️ **Edit Mode**: Rotary encoder button enables digit selection and edit via rotation.
- 🎨 **RGB Button LEDs**: Independent 2-bit color modes for each button (OFF, DIM, GREEN, AMBER).
- 🔘 **Button Press Detection**: Reports short and long presses using bitfield encoding.
- 💡 **Bulb Test Routine**: Cycles NeoPixels and 7-segment test patterns on boot.
- ⚡ **Interrupt Pin**: Signals host of any user input changes.

---

## 🎮 Button Mapping

| Button Index | Pin     | Functionality                    |
|--------------|---------|----------------------------------|
| 0            | `PC1`   | GPWS Enable / Ranging Only       |
| 1            | `PC2`   | Landing RADAR                    |
| 2            | `PC3`   | Rendezvous RADAR                 |
| -            | `PA2`   | Rotary Encoder Pushbutton        |

- All buttons are **active HIGH** and are **debounced in software**.
- Events are encoded as 2-bit values (00 = no press, 01 = short, 10 = long).

---

## 🌈 LED Color Configuration

Each button has a corresponding RGB NeoPixel LED controlled via `tinyNeoPixel`.

| 2-bit Mode | Color Name | RGB Value      | Purpose             |
|------------|------------|----------------|---------------------|
| `00`       | OFF        | (0, 0, 0)      | Inactive            |
| `01`       | DIM        | (32, 32, 32)   | Idle / background   |
| `10`       | GREEN      | (0, 128, 0)    | Active / OK         |
| `11`       | AMBER      | (128, 96, 0)   | Warning / Attention |

LED color is updated by the host via the I²C write command.

---

## 🔧 Hardware Pin Mapping

| Signal           | ATtiny816 Pin | Arduino Macro | Description                         |
|------------------|---------------|----------------|-------------------------------------|
| SDA              | PB1           | `SDA`          | I²C data line                       |
| SCL              | PB0           | `SCL`          | I²C clock line                      |
| Interrupt Output | PA4           | `INT_OUT`      | Active-low change signal to host   |
| MAX7219 Data     | PA5           | `dataIn`       | Serial data to 7-segment display   |
| MAX7219 Load     | PA6           | `loadData`     | Latch line for display             |
| MAX7219 Clock    | PB7           | `clockIn`      | Clock line for display             |
| NeoPixel Output  | PC0           | `neopixCmd`    | RGB LED signal line                |
| Button 1         | PC1           | `BTN1`         | Pushbutton input                   |
| Button 2         | PC2           | `BTN2`         | Pushbutton input                   |
| Button 3         | PC3           | `BTN3`         | Pushbutton input                   |
| Encoder A        | PA3           | `enc_A`        | Rotary encoder input A             |
| Encoder B        | PA1           | `enc_B`        | Rotary encoder input B             |
| Encoder Button   | PA2           | `enc_BTN`      | Rotary encoder pushbutton          |

---

## 📡 I²C Protocol Description

### 📤 Master Read (from module)
Returns **4 bytes**:

| Byte | Description                          |
|------|--------------------------------------|
| [0]  | `response_value` high byte (MSB)     |
| [1]  | `response_value` low byte (LSB)      |
| [2]  | 2-bit button event values            |
| [3]  | Reserved (always `0x00`)             |

### 📥 Master Write (to module)
Accepts **1 byte**:

| Bit(s) | Description                  |
|--------|------------------------------|
| [7]    | Display enable flag          |
| [5:0]  | LED mode bits (2 per LED)    |

LED Mode Mapping:
- Bits 0–1 → LED 0
- Bits 2–3 → LED 1
- Bits 4–5 → LED 2

---

## 🧰 Setup Instructions

1. **Install Arduino IDE** and add the [megaTinyCore](https://github.com/SpenceKonde/megaTinyCore) via Boards Manager.
2. Select **ATtiny816** under `Tools > Board > ATtiny1616/1606/...` → choose **ATtiny816**.
3. Set `Clock` to 20 MHz (internal).
4. Set `Programmer` to **pyupdi (serialUPDI)** or your preferred UPDI programmer.
5. Install libraries:
   - `LedControl` (for MAX7219)
   - `tinyNeoPixel` (for ATtiny-compatible NeoPixels)
   - `RotaryEncoderCore` (custom library included in your repo)
6. Open `gpws_display.ino` and upload to device.

---

## 📚 Example Host Usage

```cpp
// Read 4-byte response
Wire.requestFrom(0x21, 4);
uint8_t buffer[4];
Wire.readBytes(buffer, 4);
uint16_t value = (buffer[0] << 8) | buffer[1];
uint8_t buttonEvents = buffer[2];

// Send LED modes and enable display
Wire.beginTransmission(0x21);
Wire.write(0b11100110); // LED: 10 01 10 (LED2=GREEN, LED1=DIM, LED0=GREEN)
Wire.endTransmission();
'''
---

## 🧩 Library Dependencies

- `Wire`
- `tinyNeoPixel`
- 'LedControl'
- `RotaryEncoderCore` (custom library for encoder input)

---

## 📂 File Structure

- `RotaryEncoderCore.h` – Header file
- `RotaryEncoderCore.cpp` – Implementation
- 'library.properties' – Arduino library metadata
- `examples/DualEncoderExample/DualEncoderExample.ino` – Example

---

## 📄 License

This project is licensed under the GNU General Public License v3.0  
Final code written by J. Rostoker for **Jeb's Controller Works**

