# Function Control Module – Kerbal Controller Mk1

This is the finalized firmware for the **Function Control Module** in the **Kerbal Controller Mk1** system.  
It handles function group button input and RGB LED feedback, communicating with a host device over I2C.

---

## 📦 Overview

- **MCU**: ATtiny816 (via megaTinyCore)
- **Communication**: I2C (slave address `0x28`)
- **Inputs**: 16 buttons via 2× 74HC165 shift registers
- **Outputs**:
  - 12 RGB NeoPixel LEDs for button state/status
  - 4 discrete output pins (Throttle Lock, Precision, SCE, Audio)
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
| 0     | Air Intake       | Sky Blue          |
| 1     | Launch Escape    | Red               |
| 2     | Sci Collect      | Blue              |
| 3     | Engine Alt       | Amber             |
| 4     | Science Grp 1    | Cyan              |
| 5     | Engine Grp 1     | Green             |
| 6     | Science Grp 2    | Cyan              |
| 7     | Engine Grp 2     | Green             |
| 8     | Ctrl Pt Alt      | Lime              |
| 9     | Ctrl Pt Pri      | Lime              |
| 10    | Lock Ctrl        | Amber             |
| 11    | Heat Shield      | Mint              |
| 12    | Throttle Lock    | (Discrete LED)    |
| 13    | Precision        | (Discrete LED)    |
| 14    | SCE              | (Discrete LED)    |
| 15    | Audio            | (Discrete LED)    |

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

### Discrete Output LEDs
| Label           | Pin | ATtiny816 Pin | Function         |
|------------------|-----|---------------|------------------|
| `led_13`         | 11  | PB3           | Throttle Lock    |
| `led_14`         | 15  | PC2           | Precision Mode   |
| `led_15`         | 10  | PB2           | SCE              |
| `led_16`         | 14  | PC1           | Audio            |

---

## 🧾 I2C Protocol

This module acts as an I2C **slave** at address `0x28`.  
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
- NeoPixels reflect state changes with defined colors or dim gray when inactive.
- Discrete LEDs are driven HIGH/LOW directly via GPIO pins.

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
