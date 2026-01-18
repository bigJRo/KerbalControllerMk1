# Action Group Control Module – Kerbal Controller Mk1  
**Version 1.0 (Reference)**

This is the finalized firmware for the **Action Group Control Module** in the **Kerbal Controller Mk1** system.  
It handles action group button input, RGB LED feedback, and I²C communication with the host using the shared **ButtonModuleCore v1.1** library.

---

## 📦 Overview

- **MCU**: ATtiny816 (megaTinyCore)
- **Communication**: I²C (slave address `0x29`)
- **Inputs**: 16 momentary buttons
- **Outputs**: 12 RGB NeoPixel LEDs
- **Core Library**: ButtonModuleCore v1.1

---

## 🎛 Button Mapping

| Index | Label | Function |
|------:|-------|----------|
| 0 | AG1 | Action Group 1 |
| 1 | AG2 | Action Group 2 |
| 2 | AG3 | Action Group 3 |
| 3 | AG4 | Action Group 4 |
| 4 | AG5 | Action Group 5 |
| 5 | AG6 | Action Group 6 |
| 6 | AG7 | Action Group 7 |
| 7 | AG8 | Action Group 8 |
| 8 | AG9 | Action Group 9 |
| 9 | AG10 | Action Group 10 |
| 10 | AG11 | Action Group 11 |
| 11 | AG12 | Action Group 12 |
| 12 | — | Unused |
| 13 | — | Unused |
| 14 | SPC_MODE | Spacecraft Mode |
| 15 | RVR_MODE | Rover Mode |

---

© 2025 Jeb’s Controller Works  
Licensed under GPLv3
