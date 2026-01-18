# Stability Control Module – Kerbal Controller Mk1  
**Version 1.0 (Reference)**

This is the finalized firmware for the **Stability Control Module** in the **Kerbal Controller Mk1** system.  
It handles SAS and attitude-related control commands, RGB LED feedback, discrete indicator outputs, and I²C communication with the host using the shared **ButtonModuleCore v1.1** library.

---

## 📦 Overview

- **MCU**: ATtiny816 (megaTinyCore)
- **Communication**: I²C (slave address `0x2A`)
- **Inputs**:
  - 16 momentary buttons via shift registers (handled by ButtonModuleCore)
- **Outputs**:
  - 12 RGB NeoPixel LEDs (SAS mode indicators)
  - 4 discrete digital outputs (binary indicators)
- **Host Compatibility**: Designed for Kerbal Simpit–based host firmware
- **Core Library**: `ButtonModuleCore v1.1` (baseline)

---

## 🚀 Features

- Reads up to **16 button inputs** via shared core logic
- Controls **12 RGB NeoPixel LEDs** for SAS and attitude mode feedback
- Controls **4 discrete digital outputs** for binary state indicators
- Clean separation of:
  - Button state tracking
  - I²C protocol handling
  - Module-specific LED/output behavior
- **Priority-based LED rendering** using ButtonModuleCore v1.1 semantics
- **Automatic bulb test** executed during startup
- Optimized output updates (only changed outputs are refreshed)

---

## 🎛 Button Mapping

Button labels are defined directly by the firmware via the `commandNames[]` table.

| Index | Label            | Function                               |
|------:|------------------|----------------------------------------|
| 0     | Target           | SAS target mode                        |
| 1     | Anti-Target      | SAS anti-target mode                   |
| 2     | SAS Rad Out      | SAS radial-out mode                    |
| 3     | SAS Rad In       | SAS radial-in mode                     |
| 4     | SAS Normal       | SAS normal mode                        |
| 5     | SAS Antinormal   | SAS anti-normal mode                   |
| 6     | SAS Prograde     | SAS prograde mode                      |
| 7     | SAS Retrograde   | SAS retrograde mode                    |
| 8     | SAS Hold         | SAS stability assist                   |
| 9     | SAS Maneuver     | SAS maneuver node hold                 |
| 10    | Unused           | Unused / reserved                      |
| 11    | SAS Invert       | SAS inverted orientation               |
| 12    | SAS Enable       | Enable / disable SAS                   |
| 13    | Unused/Reserved  | Unused / reserved                      |
| 14    | RCS Enable       | Enable / disable RCS                   |
| 15    | Unused/Reserved  | Unused / reserved                      |

---

## 💡 Output Behavior (ButtonModuleCore v1.1)

### RGB LEDs (Indices 0–11)

Each RGB LED follows a strict **priority model**:

1. **`led_bits` = 1**  
   → LED displays its **assigned color**

2. **Else if `button_active_bits` = 1**  
   → LED displays **DIM_GRAY**

3. **Else**  
   → LED is **OFF (BLACK)**

### RGB LED Color Mapping

| LED Index | Label          | Assigned Color |
|----------:|----------------|----------------|
| 0         | SAS Target     | GREEN          |
| 1         | SAS Anti-Target| GREEN          |
| 2         | SAS Rad Out    | GREEN          |
| 3         | SAS Rad In     | GREEN          |
| 4         | SAS Normal     | GREEN          |
| 5         | SAS Antinormal | GREEN          |
| 6         | SAS Prograde   | GREEN          |
| 7         | SAS Retrograde | GREEN          |
| 8         | SAS Hold       | GREEN          |
| 9         | SAS Maneuver   | GREEN          |
| 10        | Unused         | BLACK          |
| 11        | SAS Invert     | GREEN          |

---

### Discrete Outputs (Indices 12–15)

Discrete outputs are **binary only**:

- **`led_bits` = 1** → Output **HIGH**
- **Else** → Output **LOW**

`button_active_bits` does **not** affect discrete outputs.

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
1. **`button_state`** – current debounced button states  
2. **`button_pressed`** – rising-edge events (auto-cleared after send)  
3. **`button_released`** – falling-edge events (auto-cleared after send)

---

## 🔧 Setup & Initialization

Called from `setup()`:

```cpp
beginModule(panel_addr);
```

Initializes I²C, button inputs, LEDs, discrete outputs, and runs a stepped bulb test (250 ms per step).

---

## 🔖 Versioning & Freeze Policy

- **Stability Control Module**: **v1.0 (FROZEN)**
- **ButtonModuleCore**: v1.1 (baseline)

No functional changes should be made without a module version bump.

---

© 2025 Jeb’s Controller Works  
Licensed under the GNU General Public License v3.0
