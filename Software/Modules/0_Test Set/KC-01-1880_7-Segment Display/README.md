# KC-01-1880 Tester Scripts

**Target board:** Xiao RA4M1  
**Purpose:** I2C test controllers for KC-01-1880 module validation  

---

## Overview

Two separate testers serve different purposes:

| Tester | Module firmware | Purpose |
|---|---|---|
| `KC01_1880_Tester` | `KC01_1880_TestFirmware` | Hardware and library validation |
| `KCMk1_GPWS_Tester` | `KCMk1_GPWS_Input` | GPWS application validation |

Run `KC01_1880_Tester` when validating a new board or a library change. Run `KCMk1_GPWS_Tester` when validating GPWS firmware changes.

---

## Wiring (both testers)

| Xiao Pin | KC-01-1882 P1 | Signal |
|---|---|---|
| D4 | Pin 13 | SDA |
| D5 | Pin 14 | SCL |
| D6 | Pin 15 | INT (active LOW, 10kΩ pull-up to 3.3V) |
| 3.3V | Pin 4 | VCC |
| GND | Pin 3 | GND |

---

## KC01_1880_Tester

**Pair with:** `KC01_1880_TestFirmware` on the module  
**Purpose:** Validates Kerbal7SegmentCore library functions and KC-01-1880 hardware in isolation.

The test firmware configures all three button modes so every library function can be exercised:
- BTN01: 3-state cycle (BACKLIT → GREEN → AMBER)
- BTN02: toggle (BACKLIT ↔ BLUE)
- BTN03: momentary flash (RED briefly, returns to BACKLIT)
- BTN_EN: resets display to 0

### Automated tests (run with `A`)

| Test | Description |
|---|---|
| T00 | I2C bus scan — confirms module at 0x2A |
| T01 | GET_IDENTITY — type ID, firmware version, capability flags |
| T03 | CMD_SET_VALUE — writes and reads back 4 values |
| T04 | Brightness sweep — 5 intensity levels (visual) |
| T06 | CMD_SLEEP / CMD_WAKE — state frozen, INT suppressed (visual) |
| T07 | CMD_RESET — value returns to 0, button states cleared |
| T12 | CMD_DISABLE / CMD_ENABLE — dark then backlit (visual) |
| T13 | CMD_ACK_FAULT — module alive after command |
| T14 | INT pin — idle HIGH, asserts on change, clears after read |

### Manual tests (require physical interaction)

| Test | Action | Expected |
|---|---|---|
| T02 | Turn encoder | Data packet received with updated value |
| T05 | Visual confirm | All segments on, all NeoPixels white during test |
| T08 | Press BTN01 3× | LED: BACKLIT→GREEN→AMBER→BACKLIT |
| T09 | Press BTN02 2× | LED: BACKLIT→BLUE→BACKLIT |
| T10 | Press BTN03 1× | LED: flashes RED→BACKLIT |
| T11 | Press BTN_EN | Display resets to 0 |

---

## KCMk1_GPWS_Tester

**Pair with:** `KCMk1_GPWS_Input` on the module  
**Purpose:** Validates GPWS application firmware behaviour and lifecycle scenarios.

Interactive serial menu — open at 115200 baud and follow prompts.

### Hardware tests

| Test | Description |
|---|---|
| T00 | I2C bus scan |
| T01 | GET_IDENTITY |
| T02 | Data packet read |
| T03 | CMD_SET_VALUE |
| T04 | Display intensity |
| T05 | CMD_BULB_TEST |
| T06 | CMD_SLEEP / CMD_WAKE |
| T07 | CMD_RESET |
| T08 | BTN01 GPWS Enable cycle |
| T09 | BTN02 Proximity Alarm |
| T11 | BTN_EN threshold reset |
| T12 | CMD_DISABLE / CMD_ENABLE |
| T13 | CMD_ACK_FAULT |
| T14 | INT pin behaviour |

### Lifecycle scenario tests

| Test | Scenario |
|---|---|
| S1 | Boot handshake |
| S2 | No flight scene (DISABLED) |
| S3 | Flight scene load (ENABLE) |
| S4 | Flight scene exit (DISABLE) |
| S5 | Vessel switch (no action) |
| S6 | Game pause / alt-tab (SLEEP / WAKE) |
| S7 | Simpit serial loss (DISABLE) |
| S8 | Master controller reboot |
| S9 | Full lifecycle sequence |

---

## KC01_1880_ColorTester

**Pair with:** `KC01_1880_ColorValidation` on the module  
**Purpose:** NeoPixel colour validation — walks through the full palette and logs PASS/FAIL per colour.

### Procedure
1. Flash `KC01_1880_ColorValidation` to the module
2. Flash `KC01_1880_ColorTester` to Xiao RA4M1
3. Open Serial Monitor at 115200 baud
4. Press Enter to begin — module shows colour 0
5. Press BTN_EN on the module to advance to each colour
6. Enter Y/N for each — results logged to serial

25 colours total. Final summary printed at end.

---

## IDE Settings

| Setting | Value |
|---|---|
| Board | Xiao RA4M1 (Arduino framework) |
| Port | COM port for the Xiao |

## Required Libraries

- KerbalModuleCommon v1.1.0
- Wire (bundled with Arduino)
