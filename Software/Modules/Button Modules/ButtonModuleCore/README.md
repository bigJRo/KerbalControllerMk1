# ButtonModuleCore

`ButtonModuleCore` is a shared utility library for all **Button Modules** used in the **Kerbal Controller Mk1** system.  
It provides core firmware support for I2C communication, button scanning, interrupt control, NeoPixel LED feedback, and status display logic.

---

## 📦 Overview

- **MCU**: ATtiny816 (via megaTinyCore)
- **Inputs**: 16 buttons via 2× 74HC165 shift registers
- **Outputs**:
  - 12 RGB NeoPixel LEDs for button state/status
  - 4 discrete output pins for "lock" LEDs
- **Communication**: I2C (slave device)
- **Host Compatibility**: Designed to interface with the Kerbal Simpit Arduino firmware

---

## 🚀 Features

- Shared `beginModule()` initializes pins, I2C, shift registers, and LEDs
- Color table stored in `PROGMEM` for efficient memory use
- Interrupt handling for notifying host of button changes
- LED color mapping and overlay support
- Constants and mappings reused across all modules

---

## 📂 Directory Structure

```
ButtonModuleCore/
├── src/
│   ├── ButtonModuleCore.h
│   └── ButtonModuleCore.cpp
├── examples/
│   └── ExampleUsage/
│       └── ExampleUsage.ino
├── library.properties
└── README.md
```

---

## 🧪 Example Usage

```cpp
#include <ButtonModuleCore.h>

constexpr uint8_t panel_addr = 0x23;

const char commandNames[16][16] PROGMEM = {
  "Cmd1", "Cmd2", "Cmd3", "Cmd4",
  "Cmd5", "Cmd6", "Cmd7", "Cmd8",
  "Cmd9", "Cmd10", "Cmd11", "Cmd12",
  "Lock1", "Lock2", "Lock3", "Lock4"
};

void setup() {
  beginModule(panel_addr);
}

void loop() {
  if (updateLED) {
    // handle_ledUpdate();  <-- user-defined
    updateLED = false;
  }

  readButtonStates();
}
```

---

## 🛠 Notes

This library is not standalone and is meant to be included in firmware projects for modules in the Kerbal Controller system.

Licensed under **GPL v3.0**  
(C) 2025 Jeb's Controller Works
