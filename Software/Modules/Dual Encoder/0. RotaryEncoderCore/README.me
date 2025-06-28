# RotaryEncoderCore – Kerbal Controller Mk1 Core Library

This is the core firmware library for **Rotary Encoder Modules** used in the **Kerbal Controller Mk1** system.  
It abstracts encoder movement tracking and button handling with debounce and press classification.

---

## 📦 Overview

- **Library Name**: RotaryEncoderCore
- **MCU Target**: ATtiny816 (via megaTinyCore)
- **Communication**: Not directly, but I2C callbacks are supported
- **Inputs**:
  - Rotary encoder A/B signals
  - Encoder push button (active-high)
- **Outputs**:
  - Position counter
  - Button press event flag
- **Special Functions**:
  - Callback hooks for short/long press and movement
  - Debounced button press tracking
  - Interrupt-safe implementation

---

## 🚀 Features

- 🌀 **Quadrature Decoding**: Tracks encoder rotation via A/B signal transitions
- 🕹️ **Button Press Detection**: Detects short and long press with debounce
- 🛎️ **Callback Support**: Define custom lambdas for `onChange`, `onShortPress`, and `onLongPress`
- 🧠 **Minimal RAM Use**: Single encoder struct tracks all necessary state

---

## 🔧 API Summary

### `attachEncoder(RotaryEncoder&, pinA, pinB, buttonPin)`
Initializes the encoder and button pins.

### `updateEncoder(RotaryEncoder&)`
Call from `loop()` to detect encoder movement and update `position`.

### `updateEncoderButton(RotaryEncoder&)`
Call from `loop()` to detect and classify button presses.

### `initEncoder(RotaryEncoder&)`
Sets initial state to ensure clean transition tracking.

---

## 🧪 Button Events

| Event Code | Meaning     |
|------------|-------------|
| `0x00`     | No event    |
| `0x01`     | Short press |
| `0x02`     | Long press  |

---

## 📚 Example Usage

```cpp
#include "RotaryEncoderCore.h"

RotaryEncoder encoder;

void setup() {
  attachEncoder(encoder, PIN_PA1, PIN_PB3, PIN_PA5);
  initEncoder(encoder);
  encoder.onChange = [] (int32_t pos) { Serial.println(pos); };
  encoder.onShortPress = [] () { Serial.println("Short Press"); };
  encoder.onLongPress = [] () { Serial.println("Long Press"); };
}

void loop() {
  updateEncoder(encoder);
  updateEncoderButton(encoder);
}
```

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
