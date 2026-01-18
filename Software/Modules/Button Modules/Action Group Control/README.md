# UI Control Module – Kerbal Controller Mk1  
**Version 1.0 (Reference)**

This is the finalized firmware for the **UI Control Module** in the **Kerbal Controller Mk1** system.  
It handles user-interface control buttons, RGB LED feedback, and I²C communication with the host using the shared **ButtonModuleCore v1.1** library.

---

## 📦 Overview

- **MCU**: ATtiny816 (megaTinyCore)
- **Communication**: I²C (slave address `0x20`)
- **Inputs**:
  - 16 momentary buttons via shift registers (handled by ButtonModuleCore)
- **Outputs**:
  - 12 RGB NeoPixel LEDs for UI state and mode indication
- **Host Compatibility**: Designed for Kerbal Simpit–based host firmware
- **Core Library**: `ButtonModuleCore v1.1` (baseline)

---

## 🚀 Features

- Reads up to **16 UI control buttons** via shared core logic
- Controls RGB NeoPixel LEDs for UI state feedback
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

| Index | Label |
|------:|-------|
| 0  | Screen Shot |
| 1  | DEBUG |
| 2  | UI |
| 3  | Navball Enable |
| 4  | Reset Map |
| 5  | Navball Mode |
| 6  | Cycle Map + |
| 7  | Cycle Ship + |
| 8  | Cycle Map - |
| 9  | Cycle Ship - |
| 10 | Map Enable |
| 11 | IVA |
| 12 | — (unused) |
| 13 | — (unused) |
| 14 | — (unused) |
| 15 | — (unused) |

---

## 💡 LED Behavior (ButtonModuleCore v1.1)

Each RGB LED follows a strict priority model:

1. **`led_bits` = 1** → Assigned color  
2. **Else if `button_active_bits` = 1** → DIM_GRAY  
3. **Else** → OFF (BLACK)

---

## 🎨 LED Color Mapping

LED colors are defined directly by the firmware via the `ColorIndex pixel_Array[]` table.

| LED Index | Function | ColorIndex |
|----------:|----------|------------|
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
2. `button_pressed` (auto-clears after send)
3. `button_released` (auto-clears after send)

---

## 🔧 Setup & Initialization

```cpp
beginModule(panel_addr);
```

Initializes hardware, I²C, button inputs, LEDs, and runs a stepped bulb test (250 ms per step).

---

## 🔖 Versioning & Freeze Policy

- **UI Control Module**: **v1.0 (FROZEN)**
- **ButtonModuleCore**: v1.1 (baseline)

No functional changes without version bump.

---

© 2025 Jeb’s Controller Works  
Licensed under the GNU GPL v3.0
