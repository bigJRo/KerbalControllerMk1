# Action Group Control Module – Kerbal Controller Mk1

This is the finalized firmware for the **Action Group Control Module** in the **Kerbal Controller Mk1** system.  
It handles button input and RGB LED feedback for Action Group toggles, communicating with a host device over I2C.

---

## 📦 Overview

- **MCU**: ATtiny816 (via megaTinyCore)
- **Communication**: I2C (slave address `0x29`)
- **Inputs**: 16 buttons via 2× 74HC165 shift registers
- **Outputs**:
  - 12 RGB NeoPixel LEDs for AG state indication
  - 4 discrete LED pins (unused in this module)
- **Host Compatibility:** Works with the Kerbal Simpit host-side Arduino firmware

---

## 🚀 Features

- Reads 16 input buttons using shift registers
- Controls 12 RGB LEDs via NeoPixels
- Responds to host commands to set LED states
- Communicates with host microcontroller over I2C
- Uses color-coded feedback based on a shared color table
- Low memory footprint using `PROGMEM` for color data

---

## 🧠 Button Layout

| Index | Command    | Default LED Color |
|-------|------------|-------------------|
| 0     | AG1        | Green             |
| 1     | AG2        | Green             |
| 2     | AG3        | Green             |
| 3     | AG4        | Green             |
| 4     | AG5        | Green             |
| 5     | AG6        | Green             |
| 6     | AG7        | Green             |
| 7     | AG8        | Green             |
| 8     | AG9        | Green             |
| 9     | AG10       | Green             |
| 10    | AG11       | Green             |
| 11    | AG12       | Green             |
| 12–15 | Unused     | N/A               |

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

### Discrete LEDs (Unused in this module)
| Label        | Pin | ATtiny816 Pin |
|--------------|-----|---------------|
| `led_13`     | 11  | PB3           |
| `led_14`     | 15  | PC2           |
| `led_15`     | 10  | PB2           |
| `led_16`     | 14  | PC1           |

---

## 🧾 I2C Protocol

This module acts as an I2C **slave** at address `0x29`.  
Communication format:

### Master → Module (Write 2 bytes)
- `[0]`: LED control bits 0–7
- `[1]`: LED control bits 8–15

### Module → Master (Read 4 bytes)
- `[0]`: Button bits 0–7
- `[1]`: Button bits 8–15
- `[2]`: LED bits 0–7 (echo)
- `[3]`: LED bits 8–15 (echo)

- Each bit corresponds to a specific Action Group LED.
- NeoPixels reflect state changes with defined colors or dim gray when inactive.

---

## 🧑‍💻 Development Notes

- Built using the Arduino IDE with [megaTinyCore](https://github.com/SpenceKonde/megaTinyCore).
- Uses:
  - `Wire.h` for I2C
  - `ShiftIn.h` for input
  - `tinyNeoPixel.h` for RGB LED control
- Color index logic and names shared across all modules via `colorTable[]`.

---

## 📜 License

Licensed under [GNU GPL v3](https://www.gnu.org/licenses/gpl-3.0.en.html).  
Original reference from [CodapopKSP/UntitledSpaceCraft](https://github.com/CodapopKSP/UntitledSpaceCraft).

---

## 🛠 Author

Final version authored by **J. Rostoker** for **Jeb's Controller Works**.  
Based on original work from [UntitledSpaceCraft](https://github.com/CodapopKSP/UntitledSpaceCraft) by CodapopKSP.  
Adapted and finalized by J. Rostoker for **Jeb's Controller Works**.
