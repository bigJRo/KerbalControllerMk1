# UI Control Module – Kerbal Controller Mk1  
**Version 1.0 (Reference)**

This is the finalized firmware for the **UI Control Module** in the **Kerbal Controller Mk1** system.  
It handles UI-related control buttons, RGB LED feedback, and I²C communication with the host using the shared **ButtonModuleCore v1.1** library.

---

## 📦 Overview

- **MCU**: ATtiny816 (megaTinyCore)
- **Communication**: I²C (slave address `0x20`)
- **Inputs**:  
  - 16 momentary buttons via shift registers (handled by ButtonModuleCore)
- **Outputs**:
  - 12 RGB NeoPixel LEDs (UI indicators)
- **Host Compatibility**: Designed for Kerbal Simpit–based host firmware
- **Core Library**: `ButtonModuleCore v1.1` (baseline)

---

## 🚀 Features

- Reads up to **16 UI control buttons**
- Controls RGB NeoPixel LEDs for UI state feedback
- Priority-based LED rendering
- Automatic bulb test on startup
- Optimized LED updates

---

## 🎛 Button Mapping

| Index | Label | Function |
|------:|-------|----------|
| 0 | Screen Shot | Capture screenshot |
| 1 | DEBUG | Debug function |
| 2 | UI | UI mode toggle |
| 3 | Navball Enable | Enable Navball |
| 4 | Reset Map | Reset map |
| 5 | Navball Mode | Change Navball mode |
| 6 | Cycle Map + | Cycle map forward |
| 7 | Cycle Ship + | Cycle ship forward |
| 8 | Cycle Map - | Cycle map backward |
| 9 | Cycle Ship - | Cycle ship backward |
| 10 | Map Enable | Enable map |
| 11 | IVA | Internal view |
| 12 | — | Unused |
| 13 | — | Unused |
| 14 | — | Unused |
| 15 | — | Unused |

---

## 💡 LED Behavior (ButtonModuleCore v1.1)

1. **`led_bits` = 1** → Assigned color  
2. **Else if `button_active_bits` = 1** → DIM_GRAY  
3. **Else** → OFF (BLACK)

### RGB LED Color Mapping

| LED Index | Label | Assigned Color |
|----------:|-------|----------------|
| 0 | Screen Shot | MAGENTA |
| 1 | DEBUG | RED |
| 2 | UI | ORANGE |
| 3 | Navball Enable | AMBER |
| 4 | Reset Map | GREEN |
| 5 | Navball Mode | BLUE |
| 6 | Cycle Map + | GREEN |
| 7 | Cycle Ship + | GREEN |
| 8 | Cycle Map - | GREEN |
| 9 | Cycle Ship - | GREEN |
| 10 | Map Enable | AMBER |
| 11 | IVA | SKY_BLUE |

---

## 🔌 I²C Protocol Summary (v1.1)

### Host → Module
- Sends 16-bit control words
- Sets or clears:
  - `led_bits`
  - `button_active_bits`
- May invoke bulb test

### Module → Host
Returns on request:
1. `button_state`
2. `button_pressed`
3. `button_released`

---

## 🔧 Setup & Initialization

```cpp
beginModule(panel_addr);
```

---

## 🔖 Versioning & Freeze Policy

- **UI Control Module**: **v1.0 (FROZEN)**
- **ButtonModuleCore**: v1.1 (baseline)

---

© 2025 Jeb’s Controller Works  
Licensed under the GNU GPL v3.0
