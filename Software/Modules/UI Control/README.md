# UI Control Module – Kerbal Controller Mk1

This is the finalized firmware for the **UI Control Module** in the **Kerbal Controller Mk1** system.  
It handles button input and RGB LED feedback for UI-related actions, communicating with a host device over I2C.

---

## 📦 Overview

- **MCU**: ATtiny816 (via megaTinyCore)
- **Communication**: I2C (slave address `0x20`)
- **Inputs**: 16 buttons via 2× 74HC165 shift registers
- **Outputs**:
  - 12 RGB NeoPixel LEDs for button state/status
  - 4 discrete output pins (Lock LEDs)

This module is part of a modular control surface for Kerbal Space Program.

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
| 0     | Map Enable       | Amber             |
| 1     | Cycle Map -      | Green             |
| 2     | Cycle Map +      | Green             |
| 3     | Navball Mode     | Blue              |
| 4     | IVA              | Amber             |
| 5     | Cycle Cam        | Sky Blue          |
| 6     | Cycle Ship -     | Green             |
| 7     | Cycle Ship +     | Green             |
| 8     | Reset Focus      | Green             |
| 9     | Screen Shot      | Magenta           |
| 10    | UI               | Amber             |
| 11    | DEBUG            | Red               |
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

### Lock Indicator LEDs (Discrete)
| Label        | Pin | ATtiny816 Pin |
|--------------|-----|---------------|
| `led_13`     | 11  | PB3           |
| `led_14`     | 15  | PC2           |
| `led_15`     | 10  | PB2           |
| `led_16`     | 14  | PC1           |

---

## 🧾 I2C Protocol

This module acts as an I2C **slave** at address `0x20`.  
Communication format:

### Host Reads (4 bytes):
```
[0] → button state bits 0–7  
[1] → button state bits 8–15  
[2] → LED bits LSB (0–7)  
[3] → LED bits MSB (8–15)
```

### Host Writes (2 bytes):
```
[0] → LED bits LSB  
[1] → LED bits MSB
```

- Each bit controls a corresponding LED or function (e.g., lock states).
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
