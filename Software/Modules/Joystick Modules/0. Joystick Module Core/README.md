# JoystickModuleCore – Kerbal Controller Mk1 Core Library

This is the core firmware library for **Joystick Modules** used in the **Kerbal Controller Mk1** system.  
It consolidates shared behavior across all joystick-based modules, such as I2C communication, analog input processing, RGB LED control, and more.

---

## 📦 Overview

- **Library Name**: JoystickModuleCore
- **MCU Target**: ATtiny816 (via megaTinyCore)
- **Communication**: I2C (slave device)
- **Inputs**:
  - 3 analog joystick axes
  - 3 buttons (including joystick press)
- **Outputs**:
  - 2 RGB NeoPixel LEDs
- **Special Functions**:
  - Automatic interrupt triggering on button press or joystick deflection
  - Bulb test on startup for visual diagnostics

---

## 🚀 Features

- 🟢 **Modular Initialization**: Call `beginModule(panel_addr)` to configure the core features of a new joystick module.
- 🎮 **Joystick Handling**: Reads analog joystick axes with deadzone processing and 16-bit scaling.
- 🎨 **LED Feedback**: Controls 2 addressable RGB LEDs with color mapping from a shared `ColorIndex` enum.
- 💡 **Bulb Test Routine**: On startup, all LEDs cycle through red, green, blue, and white.
- ⚡ **Interrupt-Driven**: Sets a digital interrupt line low when input changes are detected.
- 🧠 **Color Tables in PROGMEM**: Saves SRAM by storing LED color definitions in flash memory.
- 🛠️ **I2C Slave Interface**: Compatible with master polling or direct control using a 7-byte protocol.

---

## 🧰 Hardware Pin Mapping

| Signal        | ATtiny816 Pin | Arduino Macro | Description                         |
|---------------|---------------|---------------|-------------------------------------|
| SDA           | PB1           | `SDA`         | I2C data line                       |
| SCL           | PB0           | `SCL`         | I2C clock line                      |
| Interrupt     | PA4           | `INT_OUT`     | Output to host to signal activity  |
| Axis 1        | PB5           | `A1`          | Joystick analog input (X)          |
| Axis 2        | PA7           | `A2`          | Joystick analog input (Y)          |
| Axis 3        | PA6           | `A3`          | Joystick analog input (Z or twist) |
| Joystick Btn  | PA5           | `BUTTON_JOY`  | Joystick center press button       |
| RGB Btn 1     | PC0           | `BUTTON01`    | Discrete button 1 (with RGB)       |
| RGB Btn 2     | PB3           | `BUTTON02`    | Discrete button 2 (with RGB)       |
| NeoPixels     | PC1           | `neopixCmd`   | NeoPixel LED data out              |

---

## 📡 I²C Protocol Description

The JoystickModuleCore library communicates over I²C as a **slave** device. It uses a 7-byte protocol to communicate input states and receive LED updates.

### 📤 Master Read (from module)

When the host performs a read operation, the module responds with 7 bytes:

| Byte | Description               |
|------|---------------------------|
| [0]  | Button bits (3 lowest bits used) |
| [1-2]| Axis 1 (int16_t, big-endian)     |
| [3-4]| Axis 2 (int16_t, big-endian)     |
| [5-6]| Axis 3 (int16_t, big-endian)     |

### 📥 Master Write (to module)

When the host sends data, the module expects 1 byte:

| Byte | Description               |
|------|---------------------------|
| [0]  | LED state bits (0–7)      |

- Each bit in the LED state corresponds to a visual mode or LED override.
- When a change is detected, the flag `updateLED` is set for processing in the main loop.

---

## 🔧 API Summary

### `beginModule(panel_addr)`
Initializes pins, I2C, NeoPixels, and performs a bulb test.

### `readJoystickInputs()`
Reads analog axes, debounces button inputs, and detects joystick movement.

### `setInterrupt()` / `clearInterrupt()`
Controls the INT line for host communication.

### `handleRequestEvent()` / `handleReceiveEvent()`
Handles I2C communication with the host controller.

### `getColorFromTable(ColorIndex index)`
Retrieves RGB values from the PROGMEM-based color table.

### `overlayColor(...)`
Applies conditional overlay logic for LED indicators.

---

## 📚 Example Usage

```cpp
#include <JoystickModuleCore.h>

constexpr uint8_t panel_addr = 0x23;

void setup() {
  beginModule(panel_addr);
}

void loop() {
  if (updateLED) {
    // handle_ledUpdate();  <-- Defined in your module sketch
    updateLED = false;
  }

  uint8_t pins[NUM_BUTTONS] = { BUTTON01, BUTTON02, BUTTON_JOY };
  readJoystickInputs(pins);
}
```

---

## 🧩 Dependencies

- [`Wire`](https://www.arduino.cc/reference/en/language/functions/communication/wire/)
- [`tinyNeoPixel`](https://github.com/adafruit/Adafruit_NeoPixel)
- [`avr/pgmspace.h`](https://www.nongnu.org/avr-libc/user-manual/pgmspace.html)

---

## 🔧 Customization Notes

The following elements must be defined in the module sketch:

- `panel_addr` – I2C address
- `pixel_Array[]` – LED color map for the module
- `commandNames[][]` – Names of commands per button index
- `handle_ledUpdate()` – Logic to respond to I2C LED state changes

---

## 📂 File Structure

- `JoystickModuleCore.h` – Header file with declarations and macros
- `JoystickModuleCore.cpp` – Source file with implementation
- `library.properties` – Arduino library metadata
- `examples/ExampleUsage/ExampleUsage.ino` – Simple usage demo

---

## 📄 License

This project is licensed under the GNU General Public License v3.0  
Final code written by J. Rostoker for **Jeb's Controller Works**
