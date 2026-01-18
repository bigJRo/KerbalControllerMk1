# Assembly Test Module – Kerbal Controller Mk1  
**Version 1.0 (Reference)**

This is the finalized firmware for the **Assembly Test Module** in the **Kerbal Controller Mk1** system.  
It is intended as an **assembly verification module** for new builds, providing a post-startup rainbow LED animation plus standard ButtonModuleCore behavior for I²C-driven RGB LED state and local discrete output toggling.

---

## 📦 Overview

- **MCU**: ATtiny816 (megaTinyCore)
- **Communication**: I²C (slave address `0x2B`)
- **Inputs**:
  - 16 momentary buttons via shift registers (handled by ButtonModuleCore)
- **Outputs**:
  - 12 RGB NeoPixel LEDs (assembly test + standard state indication)
  - 4 discrete digital outputs (locally toggled on button press)
- **Host Compatibility**: Designed for Kerbal Simpit–based host firmware
- **Core Library**: `ButtonModuleCore v1.1` (baseline)

---

## 🚀 Features

- Runs standard **ButtonModuleCore bulb test** at startup
- Runs a **10-second rainbow cascade + keyboard rainbow** LED animation for assembly verification
- Standard **v1.1 priority-based LED rendering** for RGB LEDs during normal operation
- Discrete outputs (12–15) **toggle locally** when their corresponding buttons are pressed
- Optimized LED updates (only changed LEDs are refreshed)

---

## 🌈 Startup LED Assembly Test Sequence

After the standard bulb test, the module runs a 10-second animation:

1. **Cascade phase** (~4 seconds)  
   - Rainbow hues start at LED index 0
   - The illumination cascades through each LED until all 12 are lit

2. **Keyboard rainbow phase** (~6 seconds)  
   - All LEDs remain lit
   - A smooth rainbow scrolls across indices, like an RGB keyboard

3. **End state**  
   - All LEDs return to **OFF (BLACK)**

---

## 🎛 Button Mapping

Button labels are defined directly by the firmware via the `commandNames[]` table.

| Index | Label  | Function |
|------:|--------|----------|
| 0     | LED0   | Test / host-controlled RGB LED 0 |
| 1     | LED1   | Test / host-controlled RGB LED 1 |
| 2     | LED2   | Test / host-controlled RGB LED 2 |
| 3     | LED3   | Test / host-controlled RGB LED 3 |
| 4     | LED4   | Test / host-controlled RGB LED 4 |
| 5     | LED5   | Test / host-controlled RGB LED 5 |
| 6     | LED6   | Test / host-controlled RGB LED 6 |
| 7     | LED7   | Test / host-controlled RGB LED 7 |
| 8     | LED8   | Test / host-controlled RGB LED 8 |
| 9     | LED9   | Test / host-controlled RGB LED 9 |
| 10    | LED10  | Test / host-controlled RGB LED 10 |
| 11    | LED11  | Test / host-controlled RGB LED 11 |
| 12    | DOUT0  | Toggle discrete output 0 |
| 13    | DOUT1  | Toggle discrete output 1 |
| 14    | DOUT2  | Toggle discrete output 2 |
| 15    | DOUT3  | Toggle discrete output 3 |

---

## 💡 Output Behavior (ButtonModuleCore v1.1)

### RGB LEDs (Indices 0–11)

Each RGB LED follows the standard **priority model**:

1. **`led_bits` = 1**  
   → LED displays its **assigned color**

2. **Else if `button_active_bits` = 1**  
   → LED displays **DIM_GRAY**

3. **Else**  
   → LED is **OFF (BLACK)**

### RGB LED Color Mapping

LED colors are defined directly by the firmware via the `ColorIndex pixel_Array[]` table.

| LED Index | Assigned Color |
|----------:|----------------|
| 0         | RED            |
| 1         | AMBER          |
| 2         | ORANGE         |
| 3         | GOLDEN_YELLOW  |
| 4         | SKY_BLUE       |
| 5         | GREEN          |
| 6         | MINT           |
| 7         | MAGENTA        |
| 8         | CYAN           |
| 9         | LIME           |
| 10        | YELLOW         |
| 11        | FUCHSIA        |

---

### Discrete Outputs (Indices 12–15)

Discrete outputs are **locally toggled** on button press:

- Pressing button **12–15** toggles the corresponding discrete output **HIGH/LOW**
- These outputs are independent of `button_active_bits`
- They are intended for assembly verification of discrete wiring

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

Then the module runs the 10-second rainbow assembly test and returns LEDs to OFF.

---

## 🔖 Versioning & Freeze Policy

- **Assembly Test Module**: **v1.0 (FROZEN)**
- **ButtonModuleCore**: v1.1 (baseline)

No functional changes should be made without a module version bump.

---

© 2025 Jeb’s Controller Works  
Licensed under the GNU General Public License v3.0
