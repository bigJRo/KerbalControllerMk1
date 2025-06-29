# Time Display Module – Kerbal Controller Mk1 Device

This is the firmware for the **Time Display Module** used in the **Kerbal Controller Mk1** system.  
It displays preset time values on a 4-digit display, provides RGB LED feedback, and communicates with the host via I²C.

---

## 📦 Overview

- **Module Name**: Time Display Module
- **MCU Target**: ATtiny816 (via megaTinyCore)
- **Communication**: I²C (slave device, address `0x21`)
- **Inputs**:
  - 3 discrete pushbuttons (active HIGH)
  - Rotary encoder with pushbutton
- **Outputs**:
  - 3 RGB NeoPixel LEDs (one per button)
  - 4-digit 7-segment LED display via MAX7219
  - Interrupt pin to host (active LOW)
- **Special Functions**:
  - Bulb test on startup
  - Fixed time selection via buttons (1, 60, 1440)
  - RGB LED status indicators

---

## 🚀 Features

- 🔄 **I²C Communication**: 4-byte protocol for communication with host.
- 🔢 **Time Display**: Displays values 1, 60, or 1440 based on button press.
- 🎨 **RGB Button LEDs**: 2-bit modes per LED (OFF, DIM, GREEN, AMBER).
- 🔘 **Simple Press Detection**: Short-press only (no long press).
- 💡 **Bulb Test Routine**: RGB and display diagnostics on boot.
- ⚡ **Interrupt Pin**: Signals host when a button is pressed.

---

## 🎮 Button Mapping

| Button Index | Pin     | Functionality           |
|--------------|---------|-------------------------|
| 0            | `PC1`   | Set to 1 min     |
| 1            | `PC2`   | Set to 1 hr    |
| 2            | `PC3`   | Set 1 day  |
| -            | `PA2`   | Rotary Encoder Pushbutton    |

- All buttons are **active HIGH** and **debounced in software**.
- Only short presses are recognized. Each triggers a display update and interrupt.

---

## 🌈 LED Color Configuration

| 2-bit Mode | Color Name | RGB Value      | Purpose             |
|------------|------------|----------------|---------------------|
| `00`       | OFF        | (0, 0, 0)      | Inactive            |
| `01`       | DIM        | (32, 32, 32)   | Idle / background   |
| `10`       | GREEN      | (0, 128, 0)    | Active / OK         |
| `11`       | AMBER      | (128, 96, 0)   | Warning / Attention |

LED colors are controlled by the host over I²C.

---

## 🔧 Hardware Pin Mapping

| Signal           | ATtiny816 Pin | Arduino Macro | Description                         |
|------------------|---------------|----------------|-------------------------------------|
| SDA              | PB1           | `SDA`          | I²C data line                       |
| SCL              | PB0           | `SCL`          | I²C clock line                      |
| Interrupt Output | PA4           | `INT_OUT`      | Active-low signal to host          |
| MAX7219 Data     | PA5           | `dataIn`       | Serial data to 7-segment display   |
| MAX7219 Load     | PA6           | `loadData`     | Latch line for display             |
| MAX7219 Clock    | PB7           | `clockIn`      | Clock line for display             |
| NeoPixel Output  | PC0           | `neopixCmd`    | RGB LED signal line                |
| Button 1         | PC1           | `BTN1`         | Time preset 1                      |
| Button 2         | PC2           | `BTN2`         | Time preset 60                     |
| Button 3         | PC3           | `BTN3`         | Time preset 1440                   |
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
| [2]  | Button event bits (1 bit per button) |
| [3]  | Reserved (always `0x00`)             |

### 📥 Master Write (to module)
Accepts **1 byte**:

| Bit(s) | Description                  |
|--------|------------------------------|
| [7]    | Display enable flag          |
| [5:0]  | LED mode bits (2 per LED)    |

---

## 🧰 Setup Instructions

1. Install Arduino IDE and the [megaTinyCore](https://github.com/SpenceKonde/megaTinyCore) via Boards Manager.
2. Select **ATtiny816** under `Tools > Board > ATtiny1616/1606/...`.
3. Set `Clock` to 20 MHz (internal).
4. Use **pyupdi** or other UPDI programmer.
5. Install the following libraries:
   - `LedControl`
   - `tinyNeoPixel`
   - `RotaryEncoderCore`
6. Open `time_display.ino` and upload.

---

## 📚 Example Host Usage

```cpp
Wire.requestFrom(0x21, 4);
uint8_t buffer[4];
Wire.readBytes(buffer, 4);
uint16_t value = (buffer[0] << 8) | buffer[1];
uint8_t buttons = buffer[2];

Wire.beginTransmission(0x21);
Wire.write(0b10100110);  // Enable display, set LED states
Wire.endTransmission();
```

---

## 🧩 Library Dependencies

- `Wire`
- `tinyNeoPixel`
- `LedControl`
- `RotaryEncoderCore`

---

## 📂 File Structure

- `time_display.ino` – Firmware sketch
- `RotaryEncoderCore.*` – Rotary encoder logic
- `README.md` – Module documentation

---

## 📄 License

This project is licensed under the GNU General Public License v3.0  
Final code written by **J. Rostoker** for **Jeb's Controller Works**
