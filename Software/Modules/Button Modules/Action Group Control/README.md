# Action Group Control Module – Kerbal Controller Mk1  
**Version 1.0 (Reference)**

This is the finalized firmware for the **Action Group Control Module** in the **Kerbal Controller Mk1** system.  
It handles action group selection, control-point switching, RGB LED feedback, and I²C communication with the host using the shared **ButtonModuleCore v1.1** library.

---

## 📦 Overview

- **MCU**: ATtiny816 (megaTinyCore)
- **Communication**: I²C (slave address `0x29`)
- **Inputs**:
  - 16 momentary buttons via shift registers (handled by ButtonModuleCore)
- **Outputs**:
  - 12 RGB NeoPixel LEDs (action group and control-point indicators)
- **Host Compatibility**: Designed for Kerbal Simpit–based host firmware
- **Core Library**: `ButtonModuleCore v1.1` (baseline)

---

## 🚀 Features

- Reads up to **16 button inputs** via shared core logic
- Controls **12 RGB NeoPixel LEDs** for action group and control-point feedback
- Clean separation of:
  - Button state tracking
  - I²C protocol handling
  - Module-specific LED behavior
- **Priority-based LED rendering** using ButtonModuleCore v1.1 semantics
- **Automatic bulb test** executed during startup
- Optimized LED updates (only LEDs with changed visible state are refreshed)

---

## 🎛 Button Mapping

Button labels are defined directly by the firmware via the `commandNames[]` table.

| Index | Label        | Function                     |
|------:|--------------|------------------------------|
| 0     | AG5          | Action Group 5               |
| 1     | AG11         | Action Group 11              |
| 2     | AG4          | Action Group 4               |
| 3     | AG9          | Action Group 9               |
| 4     | AG3          | Action Group 3               |
| 5     | AG8          | Action Group 8               |
| 6     | AG2          | Action Group 2               |
| 7     | AG7          | Action Group 7               |
| 8     | AG1          | Action Group 1               |
| 9     | AG6          | Action Group 6               |
| 10    | CtrlPt_PRI   | Control Point – Primary      |
| 11    | CtrlPt_RED   | Control Point – Redundant    |
| 12    | —            | Unused / reserved            |
| 13    | —            | Unused / reserved            |
| 14    | SPC_MODE     | Spacecraft Control Mode      |
| 15    | RVR_MODE     | Rover Control Mode           |

---

## 💡 LED Behavior (ButtonModuleCore v1.1)

Each RGB LED follows a strict **priority model**:

1. **`led_bits` = 1**  
   → LED displays its **assigned color**

2. **Else if `button_active_bits` = 1**  
   → LED displays **DIM_GRAY**

3. **Else**  
   → LED is **OFF (BLACK)**

This allows soft backlighting for available functions and full-color indication for active states.

---

## 🎨 LED Color Mapping

LED colors are defined directly by the firmware via the `ColorIndex pixel_Array[]` table.

| LED Index | Label        | Assigned Color |
|----------:|--------------|----------------|
| 0         | AG5          | GREEN          |
| 1         | AG11         | GREEN          |
| 2         | AG4          | GREEN          |
| 3         | AG9          | GREEN          |
| 4         | AG3          | GREEN          |
| 5         | AG8          | GREEN          |
| 6         | AG2          | GREEN          |
| 7         | AG7          | GREEN          |
| 8         | AG1          | GREEN          |
| 9         | AG6          | GREEN          |
| 10        | CtrlPt_PRI  | MINT           |
| 11        | CtrlPt_RED  | LIME           |

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

This initializes I²C, button inputs, LEDs, and runs a stepped bulb test (250 ms per step).

---

## 🔖 Versioning & Freeze Policy

- **Action Group Control Module**: **v1.0 (Reference – updated firmware mapping)**
- **ButtonModuleCore**: v1.1 (baseline)

No functional changes should be made without a module version bump.

---

© 2025 Jeb’s Controller Works  
Licensed under the GNU General Public License v3.0
