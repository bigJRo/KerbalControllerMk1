# Time Control Module – Kerbal Controller Mk1

This is the finalized firmware for the **Time Control Module** in the **Kerbal Controller Mk1** system.  
It handles time-warp and save/load-related button input with RGB LED feedback, communicating with a host device over I2C.

---

## 📦 Overview

- **MCU**: ATtiny816 (via megaTinyCore)
- **Communication**: I2C (slave address `0x24`)
- **Inputs**: 16 buttons via 2× 74HC165 shift registers
- **Outputs**:
  - 12 RGB NeoPixel LEDs for button state/status
  - Discrete LED pins are defined but unused in this module
- **Host Compatibility:** Works with the Kerbal Simpit host-side Arduino firmware

---

## 🚀 Features

- Reads 16 input buttons using shift registers
- Controls 12 RGB LEDs via NeoPixels
- Communicates with host microcontroller over I2C
- Uses color-coded feedback based on a shared color table
- Special behavior: Cancel Warp LED turns BLACK when off
- Low memory footprint using `PROGMEM` for color data

---

## 🧠 Button Layout

| Index | Command          | Default LED Color |
|-------|------------------|-------------------|
| 0     | Pause            | Amber             |
| 1     | Warp to Morn     | Golden Yellow     |
| 2     | Warp to SOI      | Golden Yellow     |
| 3     | Warp to MNVR     | Golden Yellow     |
| 4     | Warp to PeA      | Golden Yellow     |
| 5     | Warp to ApA      | Golden Yellow     |
| 6     | Save             | Sky Blue          |
| 7     | Load             | Sky Blue          |
| 8     | Warp             | Golden Yellow     |
| 9     | Warp +           | Golden Yellow     |
| 10    | Physics Warp     | Golden Yellow     |
| 11    | Cancel Warp      | Red (Black when off) |
| 12–15 | Unused / Reserved| N/A               |

---

## 🧰 Hardware Connections

### Shift Register Pins
| Signal        | Pin     | ATtiny816 Pin | Description                 |
|---------------|---------|---------------|-----------------------------|
| `load`        | 7       | PA7           | Shift register latch        |
| `clockEnable` | 6       | PA6           | Enable (active LOW)         |
| `clockIn`     | 13      | PB5           | Clock input for shift reg   |
| `dataInPin`   | 5       | PA5           | Serial data from shift reg  |

### NeoPixel
| Signal      | Pin  | ATtiny816 Pin | Description         |
|-------------|------|---------------|---------------------|
| `neopixCmd` | 12   | PB4           | Data to NeoPixel chain |

### I2C Pins
| Signal | Pin | ATtiny816 Pin | Description     |
|--------|-----|---------------|-----------------|
| SDA    | 9   | PB1           | I2C Data        |
| SCL    | 8   | PB0           | I2C Clock       |

### Discrete LED Pins (Unused)
| Label        | Pin | ATtiny816 Pin | Purpose     |
|--------------|-----|---------------|-------------|
| `led_13`     | 11  | PB3           | Defined but unused |
| `led_14`     | 15  | PC2           | Defined but unused |
| `led_15`     | 10  | PB2           | Defined but unused |
| `led_16`     | 14  | PC1           | Defined but unused |

---

## 🧾 I2C Protocol

This module acts as an I2C **slave** at address `0x24`.  
Communication format:

### Master → Module (Write 2 bytes)
- `[0]`: LED control bits 0–7
- `[1]`: LED control bits 8–15

### Module → Master (Read 4 bytes)
- `[0]`: Button bits 0–7
- `[1]`: Button bits 8–15
- `[2]`: LED bits 0–7 (echo)
- `[3]`: LED bits 8–15 (echo)

- Each bit controls a corresponding LED or function.
- NeoPixels reflect state changes with defined colors or dim gray when inactive (except Cancel Warp).

---

## 🧑‍💻 Development Notes

- Built using the Arduino IDE with [megaTinyCore](https://github.com/SpenceKonde/megaTinyCore).
- Uses:
  - `Wire.h` for I2C
  - `ShiftIn.h` for button input
  - `tinyNeoPixel.h` for RGB LED control
- Shared color table used across all modules for consistent feedback

---

## 📜 License

Licensed under [GNU GPL v3](https://www.gnu.org/licenses/gpl-3.0.en.html).  
Original reference from [CodapopKSP/UntitledSpaceCraft](https://github.com/CodapopKSP/UntitledSpaceCraft).

---

## 🛠 Author

Final version authored by **J. Rostoker** for **Jeb's Controller Works**.  
Based on original work from [UntitledSpaceCraft](https://github.com/CodapopKSP/UntitledSpaceCraft) by CodapopKSP.  
Adapted and finalized by J. Rostoker for **Jeb's Controller Works**.
