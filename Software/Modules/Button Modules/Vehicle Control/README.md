# Vehicle Control Module – Kerbal Controller Mk1  
**Version 1.0 (Reference)**

This is the finalized firmware for the **Vehicle Control Module** in the **Kerbal Controller Mk1** system.  
It handles core vehicle functions, parachute and deployment controls, RGB LED feedback, discrete indicator outputs, and I²C communication with the host using the shared **ButtonModuleCore v1.1** library.

---

## 📦 Overview

- **MCU**: ATtiny816 (megaTinyCore)
- **Communication**: I²C (slave address `0x23`)
- **Inputs**:
  - 16 momentary buttons via shift registers (handled by ButtonModuleCore)
- **Outputs**:
  - 12 RGB NeoPixel LEDs (vehicle function indicators)
  - 4 discrete digital outputs (lock indicators)
- **Host Compatibility**: Designed for Kerbal Simpit–based host firmware
- **Core Library**: `ButtonModuleCore v1.1` (baseline)

---

## 🚀 Features

- Reads up to **16 button inputs** via shared core logic
- Controls **12 RGB NeoPixel LEDs** for vehicle function feedback
- Controls **4 discrete digital outputs** for lock indicators
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

| Index | Label            | Function |
|------:|------------------|----------|
| 0     | Brake            | Toggle brakes |
| 1     | Lights           | Toggle lights |
| 2     | Solar            | Toggle solar panels |
| 3     | Gear             | Toggle landing gear |
| 4     | Cargo            | Toggle cargo bay |
| 5     | Antenna          | Toggle antenna |
| 6     | Ladder           | Toggle ladder |
| 7     | Radiator         | Toggle radiator |
| 8     | MainDep          | Deploy main parachute |
| 9     | DrogueDep        | Deploy drogue parachute |
| 10    | MainCut          | Cut main parachute |
| 11    | DrogueCut        | Cut drogue parachute |
| 12    | Brake Lock       | Brake lock indicator |
| 13    | Parachute Lock  | Parachute lock indicator |
| 14    | Lights Lock     | Lights lock indicator |
| 15    | Gear Lock       | Gear lock indicator |

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

| LED Index | Label        | Assigned Color |
|----------:|--------------|----------------|
| 0         | Brake        | RED            |
| 1         | Lights       | GOLDEN_YELLOW  |
| 2         | Solar        | SKY_BLUE       |
| 3         | Gear         | GREEN          |
| 4         | Cargo        | MINT           |
| 5         | Antenna     | MAGENTA        |
| 6         | Ladder      | CYAN           |
| 7         | Radiator    | LIME           |
| 8         | MainDep     | AMBER          |
| 9         | DrogueDep   | ORANGE         |
| 10        | MainCut     | RED            |
| 11        | DrogueCut   | RED            |

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

Initializes I²C, button inputs, LEDs, discrete outputs, and runs a stepped bulb test (250 ms per step).

---

## 🔖 Versioning & Freeze Policy

- **Vehicle Control Module**: **v1.0 (FROZEN)**
- **ButtonModuleCore**: v1.1 (baseline)

No functional changes should be made without a module version bump.

---

© 2025 Jeb’s Controller Works  
Licensed under the GNU General Public License v3.0
