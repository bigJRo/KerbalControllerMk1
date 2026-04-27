# KC-01-1880 7-Segment Display Module — Test Suite

**Module:** KC-01-1880 7-Segment Display Module  
**MCU:** ATtiny816 (megaTinyCore, 20 MHz internal)  
**Test board:** Xiao RA4M1  

---

## Overview

This repository contains all firmware and test scripts for the KC-01-1880 7-Segment Display Module. Sketches are divided into two folders based on target hardware.

```
KC01_1880_ModuleFirmware/    — flash to ATtiny816 on the KC-01-1880 module
KC01_1880_TesterScripts/     — flash to Xiao RA4M1 test board
```

---

## Drawing Tree

| Number | Document |
|---|---|
| KC-01-1880 | Module assembly |
| KC-01-1881 | Schematic |
| KC-01-1882 | PCB board drawing |
| KC-01-1883 | Parts list (BOM) |
| KC-01-1884 | Placement file |

---

## Required Libraries

Install both in Arduino IDE before compiling any sketch (Sketch → Include Library → Add .ZIP):

| Library | Version | Target |
|---|---|---|
| KerbalModuleCommon | v1.1.0 | Both ATtiny816 and Xiao |
| Kerbal7SegmentCore | v1.1.0 | ATtiny816 only |

---

## Module Firmware (`KC01_1880_ModuleFirmware`)

All sketches in this folder target the **ATtiny816** on the KC-01-1880 module.

| Sketch | Purpose | Standalone? |
|---|---|---|
| `kc01_1880_demo` | Quick demo — encoder, NeoPixels, BTN_EN | Yes |
| `KC01_1880_TestFirmware` | Full I2C test firmware | No — requires Tester |
| `KC01_1880_ColorValidation` | NeoPixel colour cycle via BTN_EN | No — requires ColorTester |
| `KC01_1880_ConstructionTest` | Post-assembly hardware verification | Yes |
| `KC01_1880_MaxPowerTest` | Peak current measurement | Yes |

See `KC01_1880_ModuleFirmware/README.md` for full details.

---

## Tester Scripts (`KC01_1880_TesterScripts`)

All sketches in this folder target the **Xiao RA4M1** test board.

| Sketch | Purpose | Paired with |
|---|---|---|
| `KC01_1880_Tester` | Full 15-test I2C validation suite | `KC01_1880_TestFirmware` |
| `KC01_1880_ColorTester` | 25-colour NeoPixel validation logger | `KC01_1880_ColorValidation` |

See `KC01_1880_TesterScripts/README.md` for full details including wiring.

---

## Test Workflows

### Full I2C Validation

1. Flash `KC01_1880_TestFirmware` to the module (ATtiny816)
2. Flash `KC01_1880_Tester` to the Xiao RA4M1
3. Wire Xiao to KC-01-1882 P1 connector (see TesterScripts README)
4. Open Serial Monitor at 115200 baud
5. Press **A** to run all automated tests
6. Run manual tests T02, T08–T11 individually

### Colour Validation

1. Flash `KC01_1880_ColorValidation` to the module (ATtiny816)
2. Flash `KC01_1880_ColorTester` to the Xiao RA4M1
3. Wire Xiao to KC-01-1882 P1 connector (same wiring as above)
4. Open Serial Monitor at 115200 baud
5. Press **Enter** to begin — press **BTN_EN** on module to advance colours

### Post-Assembly Construction Test

1. Flash `KC01_1880_ConstructionTest` to the module (ATtiny816)
2. No Xiao required — module runs standalone
3. Observe boot sequence: display sweep → digit patterns → NeoPixel R/G/B per pixel
4. Use interactive mode to verify encoder, all buttons, and NeoPixel mapping

### Power Measurement

1. Flash `KC01_1880_MaxPowerTest` to the module (ATtiny816)
2. No Xiao required
3. Connect ammeter in series with VCC
4. Record current reading — this is worst-case draw for power budgeting

---

## IDE Settings

### ATtiny816 (module firmware)

| Setting | Value |
|---|---|
| Board | ATtiny816 (megaTinyCore) |
| Clock | 20 MHz internal |
| tinyNeoPixel Port | Port C |
| Programmer | serialUPDI |

### Xiao RA4M1 (tester scripts)

| Setting | Value |
|---|---|
| Board | Xiao RA4M1 |
| Serial Monitor | 115200 baud |
