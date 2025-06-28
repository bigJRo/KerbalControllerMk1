
# ButtonModuleCore – Kerbal Controller Mk1 Core Library

This is the core firmware library for **Button Modules** used in the **Kerbal Controller Mk1** system.  
It consolidates shared behavior across all button-based modules, such as I2C communication, button input processing, RGB LED control, and more.

---

## 📦 Overview

- **Library Name**: ButtonModuleCore
- **MCU Target**: ATtiny816 (via megaTinyCore)
- **Communication**: I2C (slave device)
- **Inputs**: 16 buttons via 74HC165 shift registers
- **Outputs**:
  - 12 RGB NeoPixel LEDs
  - 4 discrete digital outputs for "lock" or persistent indicators
- **Special Functions**:
  - Automatic interrupt triggering for button changes
  - Bulb test on startup for visual diagnostics

---

## 🚀 Features

- 🟢 **Modular Initialization**: Call `beginModule(panel_addr)` to configure the core features of a new button module.
- 🔄 **Shift Register Integration**: Reads 16 buttons via chained 74HC165s using the `ShiftIn` library.
- 🎨 **LED Feedback**: Controls 12 addressable RGB LEDs with color mapping from a shared `ColorIndex` enum.
- 💡 **Bulb Test Routine**: On startup, all LEDs cycle through red, green, blue, and white. Discrete outputs flash in sequence.
- ⚡ **Interrupt-Driven**: Sets a digital interrupt line low when button state changes are detected.
- 🧠 **Color Tables in PROGMEM**: Saves SRAM by storing LED color definitions in flash memory.
- 🛠️ **I2C Slave Interface**: Compatible with master polling or direct control using a 4-byte protocol.

---

## 🧰 Hardware Pin Mapping

| Signal        | ATtiny816 Pin | Arduino Macro | Description                         |
|---------------|---------------|---------------|-------------------------------------|
| SDA           | PB1           | `SDA`         | I2C data line                       |
| SCL           | PB0           | `SCL`         | I2C clock line                      |
| Interrupt     | PA4           | `INT_OUT`     | Output to host to signal activity  |
| Shift Load    | PA7           | `load`        | Shift register load                 |
| Clock Enable  | PA6           | `clockEnable` | Shift register clock enable (low)  |
| Clock In      | PB5           | `clockIn`     | Shift register clock input         |
| Data In       | PA5           | `dataInPin`   | Shift register data input          |
| NeoPixels     | PB4           | `neopixCmd`   | NeoPixel LED data out              |
| Discrete LED1 | PB3           | `led_13`      | Discrete output 1 (Lock LED)       |
| Discrete LED2 | PC2           | `led_14`      | Discrete output 2 (Lock LED)       |
| Discrete LED3 | PB2           | `led_15`      | Discrete output 3 (Lock LED)       |
| Discrete LED4 | PC1           | `led_16`      | Discrete output 4 (Lock LED)       |

---

## 📡 I²C Protocol Description

The ButtonModuleCore library communicates over I²C as a **slave** device. It uses a simple 4-byte protocol to communicate button states and receive LED updates.

### 📤 Master Read (from module)

When the host performs a read operation, the module responds with 4 bytes:

| Byte | Description               |
|------|---------------------------|
| [0]  | Button bits 0–7           |
| [1]  | Button bits 8–15          |
| [2]  | LED state bits 0–7        |
| [3]  | LED state bits 8–15       |

### 📥 Master Write (to module)

When the host sends data, the module expects 2 bytes:

| Byte | Description               |
|------|---------------------------|
| [0]  | New LED state bits 0–7    |
| [1]  | New LED state bits 8–15   |

- Each bit in the LED state corresponds to an LED or discrete output.
- The module updates colors or output states based on these bits.
- When a state change is detected, the flag `updateLED` is set for processing in the main loop.

---

## 🔧 API Summary

### `beginModule(panel_addr)`
Initializes pins, I2C, LEDs, and shift registers. Starts the bulb test.

### `readButtonStates()`
Checks for any changes in the button inputs via shift register.

### `setInterrupt()` / `clearInterrupt()`
Controls the INT line for host communication.

### `handleRequestEvent()` / `handleReceiveEvent()`
Handles I2C communication with the host controller.

### `getColorFromTable(ColorIndex index)`
Retrieves RGB values from the PROGMEM-based color table.

### `overlayColor(...)`
Applies conditional overlay logic for special LED states.

---

## 📚 Example Usage

```cpp
#include <ButtonModuleCore.h>

constexpr uint8_t panel_addr = 0x23;

void setup() {
  beginModule(panel_addr);
}

void loop() {
  if (updateLED) {
    // handle_ledUpdate();  <-- Defined in your module sketch
    updateLED = false;
  }

  readButtonStates();  // Poll shift register for new input
}
```

---

## 🧩 Dependencies

- [`Wire`](https://www.arduino.cc/reference/en/language/functions/communication/wire/)
- [`ShiftIn`](https://github.com/GreyGnome/ShiftIn)
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

- `ButtonModuleCore.h` – Header file with declarations and macros
- `ButtonModuleCore.cpp` – Source file with implementation
- `library.properties` – Arduino library metadata
- `examples/ExampleUsage/ExampleUsage.ino` – Simple usage demo

---

## 📄 License

This project is licensed under the GNU General Public License v3.0  
Final code written by J. Rostoker for **Jeb's Controller Works**
