# KCMk1 Module Tester

**Version:** 2.0.0
**Date:** 2026-06-28
**Author:** J. Rostoker — Jeb's Controller Works
**License:** GNU General Public License v3.0 (GPL-3.0)
**Board:** KC-01-9001 v2.0 / KC-01-9002

---

## Overview

A standalone, touchscreen field-validation tester for every Kerbal Controller Mk1
I2C target module. It powers a single module under test, scans the bus, identifies
the module, and presents a live test dashboard — button/axis/encoder state, LED-state
test controls, lifecycle commands, and real-time supply voltage/current/power.

This is a **ground-up rewrite** of the original serial-menu tester. It targets the new
hardware (Seeed XIAO RA4M1 + capacitive TFT + INA228) and tracks the current I2C
protocol by including the shared **KerbalModuleCommon** library as its single source of
truth for type IDs, commands, packet sizes, and LED states — so it cannot silently drift
from the modules the way the old hard-coded tester did.

---

## Hardware (KC-01-9001 v2.0)

| Block | Part |
|---|---|
| MCU | Seeed **XIAO RA4M1** (Renesas RA4M1, Arduino UNO R4 core) |
| Display | **ER-TFT028A3-4** — 2.8" 240×320 ILI9341, 4-wire SPI |
| Touch | **FT6236** capacitive, I2C `0x38` + `CTP_INT` |
| Power monitor | **INA228** high-side V/I sensor, 15 mΩ shunt, I2C `0x40` |
| Power | 12 V barrel → MPM3610 (5 V) → AP2112K (3V3), soft-latching switch |
| Module port (P1) | 12 V, 3V3, GND, I2C (SDA/SCL), INT, RST — **3.3 V logic, direct** |

### XIAO RA4M1 pin map

| Pin | Net | Function |
|---|---|---|
| D0 | CTP_INT | Capacitive-touch interrupt |
| D1 | INT_BUS | Module INT (active low) |
| D2 | RST | Module reset (active low) |
| D3 | TFT_DC | Display data/command |
| D4 / D5 | SDA / SCL | I2C — module + INA228 + touch (shared) |
| D6 | BACKLITE | Display backlight enable |
| D7 | TFT_CS | Display chip select |
| D8 / D9 / D10 | SCK / MISO / MOSI | SPI to display |

The touch controller (`0x38`) and INA228 (`0x40`) share the bus with the module under
test (`0x20`–`0x2E`); the scan logic skips the on-board addresses.

---

## Firmware structure

| File | Role |
|---|---|
| `KCMk1_Module_Tester.ino` | App state machine: SPLASH → SCAN → DASHBOARD |
| `TesterConfig.h` | Pin map, I2C addresses, INA228 calibration, timing |
| `ModuleCatalog.h/.cpp` | Per-module metadata (name, kind, input labels) |
| `TesterHW.h/.cpp` | I2C controller ops, packet parsing, INA228 driver |
| `TesterUI.h/.cpp` | LovyanGFX + FT6236 touchscreen UI (splash / scan / dashboard) |

All protocol constants (`KMC_TYPE_*`, `KMC_CMD_*`, `KMC_*_PACKET_SIZE`, `KMC_LED_*`,
`KMC_STATUS_*`, nibble helpers) come from **KerbalModuleCommon**.

---

## Dependencies

| Library | Notes |
|---|---|
| **LovyanGFX** | ILI9341 display + FT6236 touch driver (Library Manager) |
| **KerbalModuleCommon** | Shared KCMk1 protocol/palette header (this repo) |
| Wire | Arduino core I2C |

---

## Build & flash

| Setting | Value |
|---|---|
| Board | Seeed XIAO RA4M1 (Arduino UNO R4 / Renesas RA4M1 core) |
| Libraries | LovyanGFX, KerbalModuleCommon |
| Programmer | USB (UF2 / bootloader) |

1. Install the Renesas Arduino core (Arduino UNO R4 / XIAO RA4M1) and LovyanGFX.
2. Ensure `KerbalModuleCommon` is on the library path.
3. Open this folder in the Arduino IDE and upload.

---

## Usage

1. **Power on** (soft-latch switch). The splash screen appears, then the scan screen.
2. **Connect a module** to P1. The tester scans `0x20`–`0x2E`, identifies each module,
   and lists it. The top bar shows live module supply **V / A / W** from the INA228.
3. **Touch a module** to open its dashboard:
   - Header shows lifecycle (ACTIVE / SLEEPING / DISABLED / BOOT), fault flag, and the
     transaction counter.
   - The input area shows live button / switch-group / axis / encoder / value state for
     that module type.
   - Control buttons: **Enbl / Dsbl / Slp / Wake / Rst / Bulb / LED / Back**
     (`CMD_ENABLE`, `CMD_DISABLE`, `CMD_SLEEP`, `CMD_WAKE`, `CMD_RESET`, `CMD_BULB_TEST`,
     LED-state cycle, return to scan).
   - **LED** steps every position through the full LED-state set
     (ENABLED → ACTIVE → WARNING → ALERT → ARMED → PARTIAL_DEPLOY → CUT → ACTIVE_ALT).

---

## Verification status

This firmware is written against the schematic and the current protocol but **has not
been compiled or run on hardware here**. On first bring-up, verify:

- The LovyanGFX `LGFX` config for the XIAO RA4M1 (SPI host, panel reset = software,
  rotation, and FT6236 touch coordinate mapping).
- INA228 address (`0x40`, A0/A1 = GND) and the current/power calibration
  (`INA228_MAX_CURRENT_A`, `INA228_RSHUNT_OHMS` in `TesterConfig.h`) against bench readings.
- The module reset polarity/pulse on P1.

---

## Revision History

| Version | Date | Notes |
|---|---|---|
| 1.0 | 2026-04-18 | Serial-menu tester for the KC-01-9001 v1.0 board (XIAO SAMD21). |
| 2.0 | 2026-06-28 | Ground-up rewrite for KC-01-9001 v2.0 (XIAO RA4M1): capacitive touchscreen UI (LovyanGFX), INA228 power telemetry, current protocol (v2.9) via KerbalModuleCommon, all current module types. |
