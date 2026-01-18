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
  - 4 discrete digital outputs (binary indicators)
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

Button labels are defined directly by the firmware via the `commandNames[]` table.

| Index | Label                | Function |
|------:|----------------------|----------|
| 0     | Sci Collect          | Science collection |
| 1     | Engine Alt           | Alternate engine mode |
| 2     | Science Grp 1        | Science group 1 |
| 3     | Engine Grp 1         | Engine group 1 |
| 4     | Science Grp 2        | Science group 2 |
| 5     | Engine Grp 2         | Engine group 2 |
| 6     | Air Intake           | Toggle air intake |
| 7     | Launch Escape        | Launch escape system |
| 8     | Air Brake            | Toggle air brake |
| 9     | Lock Ctrl            | Control lock |
| 10    | Heat Shield Release  | Heat shield release |
| 11    | Heat Shield Deploy   | Heat shield deploy |
| 12    | Reserved/Unused      | Discrete output (reserved) |
| 13    | Reserved/Unused      | Discrete output (reserved) |
| 14    | Reserved/Unused      | Discrete output (reserved) |
| 15    | Reserved/Unused      | Discrete output (reserved) |

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

LED colors are defined directly by the firmware via the `ColorIndex pixel_Array[]` table.

| LED Index | Label                | Assigned Color |
|----------:|----------------------|----------------|
| 0         | Sci Collect          | BLUE           |
| 1         | Engine Alt           | AMBER          |
| 2         | Science Grp 1        | CYAN           |
| 3         | Engine Grp 1         | GREEN          |
| 4         | Science Grp 2        | CYAN           |
| 5         | Engine Grp 2         | GREEN          |
| 6         | Air Intake           | SKY_BLUE       |
| 7         | Launch Escape        | RED            |
| 8         | Air Brake            | ORANGE         |
| 9         | Lock Ctrl            | AMBER          |
| 10        | Heat Shield Release  | MINT           |
| 11        | Heat Shield Deploy   | RED            |

---

### Discrete Outputs (Indices 12–15)

Discrete outputs are **binary only** and do not support dimming:

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

Initializes hardware, I²C, and runs a stepped bulb test (250 ms per step).

---

## 🔖 Versioning & Freeze Policy

- **Function Control Module**: **v1.0 (Reference – updated mapping)**
- **ButtonModuleCore**: v1.1 (baseline)

No functional changes should be made without version bump.

---

© 2025 Jeb’s Controller Works  
Licensed under the GNU GPL v3.0
