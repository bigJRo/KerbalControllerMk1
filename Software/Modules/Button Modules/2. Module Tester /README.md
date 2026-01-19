# XIAO SAMD21 Assembly Test Host – Kerbal Controller Mk1  
**Version 1.0 (Reference)**

This firmware runs on a **Seeeduino XIAO SAMD21** and acts as a **host controller** for the **Assembly Test Module** in the Kerbal Controller Mk1 system.

It interfaces with a ButtonModuleCore‑based module over **I²C**, responds to the module’s interrupt line, and exercises button, LED, and discrete output paths for **assembly verification and debugging**.

---

## 📦 Overview

- **MCU**: Seeeduino XIAO SAMD21
- **Role**: I²C master / host controller
- **Communication**:
  - I²C master → ButtonModuleCore module
  - Interrupt input from module (`INT_OUT`, active LOW)
- **Target Module**: Assembly Test Module (ButtonModuleCore v1.1)
- **Debug Interface**: USB Serial

---

## 🔌 Hardware Connections

| XIAO Pin | Connected To                | Notes |
|---------:|-----------------------------|-------|
| SDA      | Module SDA                  | I²C data |
| SCL      | Module SCL                  | I²C clock |
| D6       | Module `INT_OUT`             | Active LOW interrupt |
| GND      | Module GND                  | Common ground |
| 5V / 3V3| Module VCC (as appropriate) | Match module requirements |

> **Note:** `INT_OUT` must be pulled HIGH when idle. The module drives it LOW to signal a state change.

---

## 🚀 Functional Behavior

### Interrupt‑Driven Operation

1. The module asserts **INT_OUT (LOW)** when button state changes.
2. The XIAO detects the interrupt and:
   - Requests **6 bytes** of status data over I²C
   - Clears the module’s interrupt condition by reading
3. The XIAO processes button events and writes updated control words back to the module.

### Button Event Handling

| Event | Host Action |
|------|-------------|
| **Button Pressed** | Set corresponding bit in `button_active_bits` |
| **Button Released** | Clear all `button_active_bits` and `led_bits`, then set `led_bits` for the released button |

This produces a clean **press → active**, **release → latched result** test cycle.

---

## 💡 LED & Output Semantics

- **RGB LEDs (0–11)**  
  Controlled entirely by the module using ButtonModuleCore v1.1 priority logic:
  1. `led_bits` → assigned color  
  2. else `button_active_bits` → DIM_GRAY  
  3. else OFF  

- **Discrete Outputs (12–15)**  
  Toggled **locally by the module** when the corresponding button is pressed.  
  The host does not manage discrete output state directly.

---

## 🔄 I²C Protocol (ButtonModuleCore v1.1)

### Module → Host (Read, 6 bytes)

| Bytes | Description |
|------:|-------------|
| 0–1   | `button_state` (LSB, MSB) |
| 2–3   | `button_pressed` (LSB, MSB) |
| 4–5   | `button_released` (LSB, MSB) |

### Host → Module (Write, 4 bytes)

| Bytes | Description |
|------:|-------------|
| 0–1   | `led_bits` (LSB, MSB) |
| 2–3   | `button_active_bits` (LSB, MSB) |

---

## 🖥 Serial Debug Output

The firmware provides detailed debug output over USB Serial:

- Interrupt asserted and released notifications
- Bitfield dumps (HEX + binary) for:
  - `button_state`
  - `button_pressed`
  - `button_released`
  - `led_bits`
  - `button_active_bits`

**Baud rate:** `115200`

Example output:
```
[INT] Asserted (line LOW)
[I2C<-] Status from module:
  state   : 0x0020 (0000000000100000)
  pressed : 0x0020 (0000000000100000)
  released: 0x0000 (0000000000000000)
[I2C->] Control words to module:
  led_bits        : 0x0000 (0000000000000000)
  button_active   : 0x0020 (0000000000100000)
```

---

## 🔧 Configuration

Key settings at the top of the sketch:

```cpp
constexpr uint8_t MODULE_ADDR    = 0x2B;
constexpr uint8_t MODULE_INT_PIN = 6;
constexpr uint32_t SERIAL_BAUD   = 115200;
```

Adjust these to match your wiring and module address.

---

## 🔖 Versioning & Use

- **Host Firmware**: **v1.0 (Reference)**
- **Target Module**: Assembly Test Module v1.0
- **Core Protocol**: ButtonModuleCore v1.1

This firmware is intended for **bench testing, bring‑up, and verification**, not flight use.

---

© 2026 Jeb’s Controller Works  
Licensed under the GNU General Public License v3.0
