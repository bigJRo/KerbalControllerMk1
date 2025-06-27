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

## 📄 License

This project is licensed under the GNU General Public License v3.0  
Final code written by J. Rostoker for **Jeb's Controller Works**
