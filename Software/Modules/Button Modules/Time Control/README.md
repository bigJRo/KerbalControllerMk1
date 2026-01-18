# Time Control Module – Kerbal Controller Mk1  
**Version 1.0 (Reference)**

This is the finalized firmware for the **Time Control Module** in the **Kerbal Controller Mk1** system.  
It handles time-warp control, save/load commands, pause control, RGB LED feedback, and I²C communication with the host using the shared **ButtonModuleCore v1.1** library.

---

## 📦 Overview

- **MCU**: ATtiny816 (megaTinyCore)
- **Communication**: I²C (slave address `0x24`)
- **Inputs**:
  - 16 momentary buttons via shift registers (handled by ButtonModuleCore)
- **Outputs**:
  - 12 RGB NeoPixel LEDs (time and state indicators)
- **Host Compatibility**: Designed for Kerbal Simpit–based host firmware
- **Core Library**: `ButtonModuleCore v1.1` (baseline)

---

## 🚀 Features

- Reads up to **16 button inputs** via shared core logic
- Controls **12 RGB NeoPixel LEDs** for time and state feedback
- Clean separation of:
  - Button state tracking
  - I²C protocol handling
  - Module-specific LED behavior
- **Priority-based LED rendering** using ButtonModuleCore v1.1 semantics
- **Automatic bulb test** executed during startup
- Optimized LED updates (only changed LEDs are refreshed)

---

## 🎛 Button Mapping

Button labels are defined directly by the firmware via the `commandNames[]` table.

| Index | Label            | Function                          |
|------:|------------------|-----------------------------------|
| 0     | Warp to ApA      | Time-warp to Apoapsis             |
| 1     | Cancel Warp     | Cancel active time-warp           |
| 2     | Warp to PeA      | Time-warp to Periapsis            |
| 3     | Physics Warp    | Toggle physics warp               |
| 4     | Warp to MNVR    | Time-warp to maneuver node        |
| 5     | Warp +          | Increase time-warp rate           |
| 6     | Warp to SOI     | Time-warp to sphere-of-influence  |
| 7     | Warp -          | Decrease time-warp rate           |
| 8     | Warp to Morn    | Time-warp to morning              |
| 9     | Load            | Load game                         |
| 10    | Pause           | Pause / unpause simulation        |
| 11    | Save            | Save game                         |
| 12    | —               | Unused / reserved                 |
| 13    | —               | Unused / reserved                 |
| 14    | —               | Unused / reserved                 |
| 15    | —               | Unused / reserved                 |

---

## 💡 LED Behavior (ButtonModuleCore v1.1)

Each RGB LED follows a strict **priority model**:

1. **`led_bits` = 1**  
   → LED displays its **assigned color**

2. **Else if `button_active_bits` = 1**  
   → LED displays **DIM_GRAY**

3. **Else**  
   → LED is **OFF (BLACK)**

This allows soft backlighting for available controls and full-color indication for active states.

---

## 🎨 LED Color Mapping

LED colors are defined directly by the firmware via the `ColorIndex pixel_Array[]` table.

| LED Index | Label           | Assigned Color |
|----------:|-----------------|----------------|
| 0         | Warp to ApA     | GOLDEN_YELLOW  |
| 1         | Cancel Warp    | RED            |
| 2         | Warp to PeA     | GOLDEN_YELLOW  |
| 3         | Physics Warp   | GOLDEN_YELLOW  |
| 4         | Warp to MNVR   | GOLDEN_YELLOW  |
| 5         | Warp +         | GOLDEN_YELLOW  |
| 6         | Warp to SOI    | GOLDEN_YELLOW  |
| 7         | Warp -         | GOLDEN_YELLOW  |
| 8         | Warp to Morn   | GOLDEN_YELLOW  |
| 9         | Load           | SKY_BLUE       |
| 10        | Pause          | AMBER          |
| 11        | Save           | SKY_BLUE       |

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

- **Time Control Module**: **v1.0 (FROZEN)**
- **ButtonModuleCore**: v1.1 (baseline)

No functional changes should be made without a module version bump.

---

© 2025 Jeb’s Controller Works  
Licensed under the GNU General Public License v3.0
