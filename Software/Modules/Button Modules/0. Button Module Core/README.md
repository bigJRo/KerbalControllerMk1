# ButtonModuleCore – Kerbal Controller Mk1 Core Library

This library provides the shared firmware core for **Kerbal Controller Mk1** button modules built around an **ATtiny816** (megaTinyCore).

## What it provides

- 16-button input scanning via two chained **74HC165** shift registers (`ShiftIn`)
- 12 **NeoPixel RGB** outputs (`tinyNeoPixel`)
- 4 **discrete** LED outputs
- I2C slave protocol:
  - Master reads: **button state** + **pressed edges** + **released edges** (6 bytes)
  - Master writes: **LED control** bits (16-bit) and optional **button_active_bits** (16-bit)
- Startup **bulb test** pattern

- ## 📦 Overview

- **Library Name**: ButtonModuleCore
- **MCU Target**: ATtiny816 (via megaTinyCore)
- **Communication**: I2C (slave device)
- **Inputs**: 16 buttons via 74HC165 shift registers
- **Outputs**:
  - 12 RGB NeoPixel LEDs
  - 4 discrete digital outputs for "lock" or persistent indicators
- **I2C slave protocol**:
  - Master reads: **button state** + **pressed edges** + **released edges** (6 bytes)
  - Master writes: **LED control** bits (16-bit) and optional **button_active_bits** (16-bit)
- **Special Functions**:
  - Bulb test on startup for visual diagnostics

---

## 🚀 Features

- 🟢 **Modular Initialization**: Call `beginModule(panel_addr)` to configure the core features of a new button module.
- 🔄 **Shift Register Integration**: Reads 16 buttons via chained 74HC165s using the `ShiftIn` library.
- 🎨 **LED Feedback**: Controls 12 addressable RGB LEDs with color mapping from a shared `ColorIndex` enum.
- 💡 **Bulb Test Routine**: On startup, all LEDs cycle through red, green, blue, and white. Discrete outputs flash in sequence.
- ⚡ **Interrupt-Driven**: Sets a digital interrupt line low when button state changes are detected.
- 🧠 **Color Tables in PROGMEM**: Saves SRAM by storing LED color definitions in flash memory.
- 🛠️ **I2C Slave Interface**: Compatible with master polling or direct control

## Pin mapping (ATtiny816 / megaTinyCore)

These are the canonical defines used by the library (see `src/ButtonModuleCore.h`).

| Signal | Arduino macro | Pin |
|---|---:|---|
| I2C SDA | `SDA` | `PIN_PB1` |
| I2C SCL | `SCL` | `PIN_PB0` |
| Interrupt out (to host) | `INT_OUT` | `PIN_PA1` |
| Shift register load | `load` | `PIN_PA7` |
| Shift register clock enable (active low) | `clockEnable` | `PIN_PA6` |
| Shift register clock in | `clockIn` | `PIN_PB5` |
| Shift register data in | `dataInPin` | `PIN_PA5` |
| NeoPixel data out | `neopixCmd` | `PIN_PA4` |
| Discrete LED 1 | `led_13` | `PIN_PB3` |
| Discrete LED 2 | `led_14` | `PIN_PC0` |
| Discrete LED 3 | `led_15` | `PIN_PC2` |
| Discrete LED 4 | `led_16` | `PIN_PC1` |

## Usage

In your module firmware:

1. Call `beginModule(panel_addr)` from `setup()`.
2. Call `readButtonStates()` frequently in `loop()`.
3. When `updateLED` becomes true, use the received control words:
   - `led_bits` (16-bit)
   - `button_active_bits` (16-bit)

How `led_bits` and `button_active_bits` map onto the 12 NeoPixels and 4 discrete LEDs is **module-specific**. This core library stores the values and sets `updateLED` when they change.

## I2C Protocol Description

The ButtonModuleCore library communicates over I²C as a **slave** device.

### Master Read (module -> master)

The module responds with **6 bytes**:

| Bytes | Field | Description |
|---:|---|---|
| 0..1 | `button_state_bits` | Current sampled state of all 16 buttons (LSB first) |
| 2..3 | `button_pressed_bits` | Edge bits latched for transitions **0 -> 1** since last master read |
| 4..5 | `button_released_bits` | Edge bits latched for transitions **1 -> 0** since last master read |

**Pressed/released are one-shot:** they are cleared after being sent to the master.

### Master Write (master -> module)

Accepted payloads:

- **2 bytes (legacy):**

| Bytes | Field | Description |
|---:|---|---|
| 0..1 | `led_bits` | LED state bits for full usage (LSB first) |

- **4 bytes:**

| Bytes | Field | Description |
|---:|---|---|
| 0..1 | `led_bits` | LED state bits for full usage (LSB first) |
| 2..3 | `button_active_bits` | LED state bits to indicated function active to trigger backlight (LSB first) |

- Each bit in the LED state corresponds to an LED or discrete output.
- The module updates colors or output states based on these bits.
- When a state change is detected, the flag `updateLED` is set for processing in the main loop.

## Bulb test

`beginModule()` runs `bulbTest()` during startup. The current bulb test implementation cycles all NeoPixels through a basic color sequence, then displays a fixed final pattern, and finally clears the outputs.

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

