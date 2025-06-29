# Throttle Module – Kerbal Controller Mk1 Device

This is the firmware for the **Throttle Module** used in the **Kerbal Controller Mk1** system.  
It provides analog and discrete throttle control using motorized movement, pushbutton steps, and I²C-based communication with the host.

---

## 📦 Overview

- **Module Name**: Throttle Module
- **MCU Target**: ATtiny816 (via megaTinyCore)
- **Communication**: I²C (slave device, address `0x29`)
- **Inputs**:
  - 4 discrete pushbuttons (active HIGH)
  - Throttle position wiper (analog input)
  - Touch sensor (active LOW)
  - Fine mode switch (active HIGH)
- **Outputs**:
  - 4 discrete LEDs
  - Motor direction and speed control
  - Interrupt pin to host (active LOW)
- **Special Functions**:
  - Bulb test on startup
  - Full-range and fine-range throttle positioning
  - Touch-enabled motor engagement
  - Debounced step-up/down throttle controls

---

## 🚀 Features

- 🔄 **I²C Communication**: 2-byte protocol to send 16-bit throttle position.
- 🎮 **Motorized Throttle Control**: Moves to full, zero, or incrementally changes position.
- ✋ **Touch Sensor Gating**: Only moves when touched.
- 🎚️ **Fine Mode**: Reduces output range to 10% for precise adjustments.
- 🔘 **Step Buttons**: Support full, zero, up, and down throttle.
- 💡 **Bulb Test Routine**: LEDs and motor sweep on startup.
- ⚡ **Interrupt Pin**: Notifies host of position change.

---

## 🎮 Button Mapping

| Button Index | Pin     | Functionality             |
|--------------|---------|---------------------------|
| 0            | `PB4`   | Throttle to 100%          |
| 1            | `PB3`   | Throttle step up          |
| 2            | `PC1`   | Throttle step down        |
| 3            | `PC2`   | Throttle to 0%            |

- All buttons are **active HIGH** and **debounced in software**.
- Each press updates the throttle position accordingly.

---

## 💡 LED Configuration

| LED Pin | Pin     | Indicates                  |
|---------|---------|----------------------------|
| LED_100 | `PB5`   | Full throttle selected     |
| LED_UP  | `PB2`   | Step up activity           |
| LED_DOWN| `PC0`   | Step down activity         |
| LED_00  | `PC3`   | Zero throttle selected     |

All LEDs are digital and controlled by firmware during throttle activity.

---

## 🔧 Hardware Pin Mapping

| Signal           | ATtiny816 Pin | Arduino Macro | Description                         |
|------------------|---------------|----------------|-------------------------------------|
| SDA              | PB1           | `SDA`          | I²C data                            |
| SCL              | PB0           | `SCL`          | I²C clock                           |
| Interrupt Output | PA3           | `INT_OUT`      | Active-low interrupt to host        |
| Throttle Wiper   | PA1           | `WIPER`        | Analog position input               |
| Touch Sensor     | PA2           | `TOUCH`        | Touch-to-engage input               |
| Fine Mode Switch | PA4           | `FINE_MODE`    | Toggle for precision control        |
| Motor Reverse    | PA5           | `MOTOR_REV`    | Motor reverse direction             |
| Motor Forward    | PA6           | `MOTOR_FWD`    | Motor forward direction             |
| Motor Speed      | PA7           | `SPEED`        | PWM speed control                   |
| Button 100%      | PB4           | `THRTL_100`    | Throttle to full                    |
| LED 100%         | PB5           | `LED_100`      | LED indicator                       |
| Button Up        | PB3           | `THRTL_UP`     | Step up                            |
| LED Up           | PB2           | `LED_UP`       | LED indicator                       |
| Button Down      | PC1           | `THRTL_DOWN`   | Step down                          |
| LED Down         | PC0           | `LED_DOWN`     | LED indicator                       |
| Button 0%        | PC2           | `THRTL_00`     | Throttle to zero                    |
| LED 0%           | PC3           | `LED_00`       | LED indicator                       |

---

## 📡 I²C Protocol Description

### 📤 Master Read (from module)

Returns **2 bytes** representing a 16-bit throttle value:

| Byte | Description                      |
|------|----------------------------------|
| [0]  | Low byte of `host_throttle_position` |
| [1]  | High byte of `host_throttle_position` |

Value is scaled:
- 0 → INT16_MAX (full throttle)
- Or, 0 → INT16_MAX / 10 in **Fine Mode**

### 📥 Master Write (to module)

Sends **1 byte** command:

| Bit(s) | Description                |
|--------|----------------------------|
| [0]    | Enable throttle processing |

---

## 🧰 Setup Instructions

1. Install Arduino IDE and the [megaTinyCore](https://github.com/SpenceKonde/megaTinyCore).
2. Select **ATtiny816** via `Tools > Board > ATtiny1616/1606/...`.
3. Set `Clock` to **20 MHz (internal)**
4. Use a **pyupdi programmer** via UPDI.
5. Open `throttle.ino` and upload the sketch.

---

## 📚 Example Host Usage

```cpp
// Read throttle position
Wire.requestFrom(0x29, 2);
uint8_t low = Wire.read();
uint8_t high = Wire.read();
uint16_t throttleValue = (high << 8) | low;

// Enable throttle control
Wire.beginTransmission(0x29);
Wire.write(0b00000001);  // Bit 0: enable throttle
Wire.endTransmission();
```

---

## 🧩 Library Dependencies

- `Wire` (I²C communication)

---

## 📂 File Structure

- `throttle.ino` – Firmware sketch
- `README.md` – Module documentation

---

## 📄 License

This project is licensed under the GNU General Public License v3.0  
Final code written by J. Rostoker for **Jeb's Controller Works**
