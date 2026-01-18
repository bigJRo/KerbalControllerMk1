# Function Control Module – Kerbal Controller Mk1  
**Version 1.0 (Reference)**

This is the finalized firmware for the **Function Control Module** in the **Kerbal Controller Mk1** system.  
It handles spacecraft function buttons, RGB LED feedback, discrete indicator outputs, and I²C communication with the host using the shared **ButtonModuleCore v1.1** library.

---

## 📦 Overview

- **MCU**: ATtiny816 (megaTinyCore)
- **Communication**: I²C (slave address `0x28`)
- **Inputs**:  
  - 16 momentary buttons via shift registers (handled by ButtonModuleCore)
- **Outputs**:
  - 12 RGB NeoPixel LEDs (function indicators)
  - 4 discrete digital outputs (non-dimmable indicators)
- **Host Compatibility**: Designed for Kerbal Simpit–based host firmware
- **Core Library**: `ButtonModuleCore v1.1` (baseline)

---

## 🚀 Features

- Reads up to **16 button inputs** via shared core logic
- Controls **12 RGB NeoPixel LEDs** for function state feedback
- Controls **4 discrete digital outputs** for non-RGB indicators
- Clean separation of:
  - Button state tracking
  - I²C protocol handling
  - Module-specific LED/output behavior
- **Priority-based LED rendering** using ButtonModuleCore v1.1 semantics
- **Automatic bulb test** executed during startup
- Optimized output updates (only changed outputs are refreshed)

---

## 🎛 Button Mapping

| Index | Label | Function |
|------:|-------|----------|
| 0 | Air Intake | Toggle air intake |
| 1 | Launch Escape | Launch escape system |
| 2 | Sci Collect | Science collection |
| 3 | Engine Alt | Alternate engine mode |
| 4 | Science Grp 1 | Science group 1 |
| 5 | Engine Grp 1 | Engine group 1 |
| 6 | Science Grp 2 | Science group 2 |
| 7 | Engine Grp 2 | Engine group 2 |
| 8 | Ctrl Pt Alt | Control point alternate |
| 9 | Ctrl Pt Pri | Control point primary |
| 10 | Lock Ctrl | Control lock |
| 11 | Heat Shield | Heat shield |
| 12 | Throttle Lock | Discrete output |
| 13 | Precision | Discrete output |
| 14 | SCE | Discrete output |
| 15 | Audio | Discrete output |

---

## 💡 Output Behavior (ButtonModuleCore v1.1)

### RGB LEDs (Indices 0–11)

Each RGB LED follows a strict **priority model**:

1. **`led_bits` = 1** → Assigned color  
2. **Else if `button_active_bits` = 1** → DIM_GRAY  
3. **Else** → OFF (BLACK)

### RGB LED Color Mapping

| LED Index | Label | Assigned Color |
|----------:|-------|----------------|
| 0 | Air Intake | SKY_BLUE |
| 1 | Launch Escape | RED |
| 2 | Sci Collect | BLUE |
| 3 | Engine Alt | AMBER |
| 4 | Science Grp 1 | CYAN |
| 5 | Engine Grp 1 | GREEN |
| 6 | Science Grp 2 | CYAN |
| 7 | Engine Grp 2 | GREEN |
| 8 | Ctrl Pt Alt | LIME |
| 9 | Ctrl Pt Pri | LIME |
| 10 | Lock Ctrl | AMBER |
| 11 | Heat Shield | MINT |

---

### Discrete Outputs (Indices 12–15)

- **`led_bits` = 1** → Output HIGH  
- **Else** → Output LOW  

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

Initializes hardware, I²C, and runs a stepped bulb test (250 ms per step).

---

## 🔖 Versioning & Freeze Policy

- **Function Control Module**: **v1.0 (FROZEN)**
- **ButtonModuleCore**: v1.1 (baseline)

---

© 2025 Jeb’s Controller Works  
Licensed under the GNU GPL v3.0
