# Action Group Control Module – Kerbal Controller Mk1  
**Version 1.0 (Reference)**

This is the finalized firmware for the **Action Group Control Module** in the **Kerbal Controller Mk1** system.  
It handles action group button input, RGB LED feedback, and I²C communication with the host using the shared **ButtonModuleCore v1.1** library.

---

## 📦 Overview

- **MCU**: ATtiny816 (megaTinyCore)
- **Communication**: I²C (slave address `0x29`)
- **Inputs**:  
  - 16 momentary buttons via shift registers (handled by ButtonModuleCore)
- **Outputs**:
  - 12 RGB NeoPixel LEDs (Action Group indicators)
- **Host Compatibility**: Designed for Kerbal Simpit–based host firmware
- **Core Library**: `ButtonModuleCore v1.1` (baseline)

---

## 🚀 Features

- Reads up to **16 button inputs** via shared core logic
- Controls **12 RGB NeoPixel LEDs** for Action Group feedback
- Clean separation of:
  - Button state tracking
  - I²C protocol handling
  - Module-specific LED behavior
- **Priority-based LED rendering** using ButtonModuleCore v1.1 semantics
- **Automatic bulb test** executed during startup
- Optimized LED updates (only changed LEDs are refreshed)

---

## 🎛 Button Mapping

| Index | Label     | Function          |
|------:|-----------|-------------------|
| 0     | AG1       | Action Group 1    |
| 1     | AG2       | Action Group 2    |
| 2     | AG3       | Action Group 3    |
| 3     | AG4       | Action Group 4    |
| 4     | AG5       | Action Group 5    |
| 5     | AG6       | Action Group 6    |
| 6     | AG7       | Action Group 7    |
| 7     | AG8       | Action Group 8    |
| 8     | AG9       | Action Group 9    |
| 9     | AG10      | Action Group 10   |
| 10    | AG11      | Action Group 11   |
| 11    | AG12      | Action Group 12   |
| 12–13 | —         | Unused            |
| 14    | SPC_MODE  | Spacecraft Mode   |
| 15    | RVR_MODE  | Rover Mode        |

---

## 💡 LED Behavior (ButtonModuleCore v1.1)

Each LED follows a strict **priority model**:

1. **`led_bits` = 1**  
   → LED displays its **assigned color** (GREEN for AG1–AG12)

2. **Else if `button_active_bits` = 1**  
   → LED displays **DIM_GRAY**

3. **Else**  
   → LED is **OFF (BLACK)**

### LED Color Mapping

| LED Index | Label    | Assigned Color |
|----------:|----------|----------------|
| 0–11      | AG1–AG12 | GREEN          |

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
2. `button_pressed` (auto-clears after send)
3. `button_released` (auto-clears after send)

---

## 🔧 Setup & Initialization

Called from `setup()`:

```cpp
beginModule(panel_addr);
```

Initializes hardware, I²C, and runs a stepped bulb test (250 ms per step).

---

## 🔖 Versioning & Freeze Policy

- **Action Group Control Module**: **v1.0 (FROZEN)**
- **ButtonModuleCore**: v1.1 (baseline)

No functional changes without version bump.

---

© 2025 Jeb’s Controller Works  
Licensed under the GNU GPL v3.0
