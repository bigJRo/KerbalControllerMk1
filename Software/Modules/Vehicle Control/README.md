# Vehicle Control Module for Kerbal Controller Mk1

This is the finalized firmware for the **Vehicle Control Module** in the **Kerbal Controller Mk1** system.  
It handles button input and RGB LED feedback for UI-related actions, communicating with a host device over I2C.

---


## 📦 Overview

- **MCU**: ATtiny816 (via megaTinyCore)
- **Communication**: I2C (slave address `0x23`)
- **Inputs**: 16 buttons via 2× 74HC165 shift registers
- **Outputs**:
  - 12 RGB NeoPixel LEDs for button state/status
  - 4 discrete output pins (Lock LEDs)
- **Host Compatibility:** Works with the Kerbal Simpit host-side Arduino firmware

---

## 🚀 Features

- Reads 16 input buttons using shift registers
- Controls 12 RGB LEDs via NeoPixels
- Updates 4 discrete LEDs for "lock" or persistent states
- Communicates with host microcontroller over I2C
- Uses color-coded feedback based on a shared color table
- Low memory footprint using `PROGMEM` for color data

---

## 🧠 Button Layout

| Index | Command          | Default LED Color |
|-------|------------------|-------------------|
| 0     | Brake       	   | Red               |
| 1     | Lights           | Golden Yellow     |
| 2     | Solar Array      | Sky Blue          |
| 3     | Gear             | Green             |
| 4     | Cargo Door       | Mint              |
| 5     | Antenna          | Magenta           |
| 6     | Ladder           | Cyan              |
| 7     | Radiator         | Lime              |
| 8     | Drouge Deploy    | Orange            |
| 9     | Main Deploy      | Amber             |
| 10    | Drouge Cut       | Red               |
| 11    | Main Cut         | Red               |
| 12    | Brake Lock       | N/A               |
| 13    | Parachute Enable | N/A               |
| 14    | Lights Lock      | N/A               |
| 15    | Gear Lock        | N/A               |

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

### Lock Indicator LEDs (Discrete)
| Label        | Pin | ATtiny816 Pin |
|--------------|-----|---------------|
| `led_13`     | 11  | PB3           |
| `led_14`     | 15  | PC2           |
| `led_15`     | 10  | PB2           |
| `led_16`     | 14  | PC1           |

---

## I2C Protocol

## 🧾 I2C Protocol

This module acts as an I2C **slave** at address `0x23`.  
Communication format:

### Master → Module (Write 2 bytes)
- `[0]`: LED control bits 0–7
- `[1]`: LED control bits 8–15

### Module → Master (Read 4 bytes)
- `[0]`: Button bits 0–7
- `[1]`: Button bits 8–15
- `[2]`: LED bits 0–7 (echo)
- `[3]`: LED bits 8–15 (echo)

- Each bit controls a corresponding LED or function (e.g., lock states).
- NeoPixels reflect state changes with defined colors, dim gray when inactive, or black when unavailable

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
