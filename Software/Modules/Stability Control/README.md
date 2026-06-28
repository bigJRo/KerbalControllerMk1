# KCMk1_Stability_Control

**Module:** Stability Control  
**Version:** 2.0  
**Date:** 2026-06-28  
**Author:** J. Rostoker — Jeb's Controller Works  
**License:** GNU General Public License v3.0 (GPL-3.0)  
**Hardware:** KC-01-1802 Button Module Base v1.1  
**Library:** KerbalButtonCore v2.0.0  

---

## Overview

The Stability Control module provides ten SAS mode buttons, an RCS toggle, and a control-inversion modifier for Kerbal Space Program, plus two discrete staging positions. The staging enable position (B15) includes both a discrete input and a discrete LED output acting as a safety interlock indicator.

This module uses all 12 NeoPixel RGB button positions (KBC indices 0–11). KBC indices 12–13 are no-connects (the former SAS_ENA / RCS_ENA inputs moved to Switch Group 2 on the Vehicle Control module as of v2.0); KBC indices 14–15 are the discrete staging input and staging-enable input + interlock LED.

---

## Module Identity

| Parameter | Value |
|---|---|
| I2C Address | `0x23` |
| Module Type ID | `0x04` (KBC_TYPE_STABILITY_CONTROL) |
| Capability Flags | `0x00` (core states only) |
| Extended States | No |
| NeoPixel Buttons | 12 (KBC indices 0–11) |
| Not Installed | KBC indices 12–13 (no-connect) |
| Discrete Staging | KBC index 14 (Stage, input), 15 (Stage Enable, input + LED) |

---

## Panel Layout

Physical panel orientation: 2 rows x 6 columns. Column 6 is leftmost, Column 1 is rightmost. Button numbering starts top-right (B0) and proceeds left across each row.

![Stability Control Panel Layout](panel_layout.svg)

Active state colors shown. All NeoPixel buttons (B0–B11) illuminate dim white in the ENABLED state. Discrete positions B14–B15 (staging) are outside the NeoPixel grid and not shown.

---

## Button Reference

### NeoPixel Buttons (KBC indices 0-11)

| KBC Index | PCB Label | Function | Active Color | Notes |
|---|---|---|---|---|
| B0 | BUTTON01 | Target | GREEN | SAS mode — one active at a time |
| B1 | BUTTON02 | Anti-Target | GREEN | SAS mode |
| B2 | BUTTON03 | Radial In | GREEN | SAS mode |
| B3 | BUTTON04 | Radial Out | GREEN | SAS mode |
| B4 | BUTTON05 | Normal | GREEN | SAS mode |
| B5 | BUTTON06 | Anti-Normal | GREEN | SAS mode |
| B6 | BUTTON07 | Prograde | GREEN | SAS mode |
| B7 | BUTTON08 | Retrograde | GREEN | SAS mode |
| B8 | BUTTON09 | Stab Assist | GREEN | SAS mode |
| B9 | BUTTON10 | Maneuver | GREEN | SAS mode |
| B10 | BUTTON11 | RCS | GREEN | RCS toggle |
| B11 | BUTTON12 | Invert | AMBER | Control modifier |

### Discrete Positions (KBC indices 12-15)

| KBC Index | PCB Label | Signal | LED | Notes |
|---|---|---|---|---|
| B12 | BUTTON13 | Not installed | N/A | (was SAS_ENA — moved to Switch Group 2) |
| B13 | BUTTON14 | Not installed | N/A | (was RCS_ENA — moved to Switch Group 2) |
| B14 | BUTTON15 | Stage | N/A — input only | Latching staging trigger |
| B15 | BUTTON16 | Stage Enable | Discrete LED | Safety interlock indicator (driven by controller) |

### Color Design Notes

- **SAS modes (GREEN)** — all 10 SAS modes share uniform green. Only one mode can be active at a time in KSP, so the single illuminated green button identifies the current mode without any color distinction between modes.
- **RCS (GREEN)** — RCS toggle, nominal/active green consistent with the SAS bank.
- **Invert (AMBER)** — amber signals awareness; control inversion modifies whatever SAS mode is active and warrants a distinct visual.

---

## Wiring

### NeoPixel Button Inputs

| PCB Connector | PCB Label | KBC Index | Function |
|---|---|---|---|
| P2 | BUTTON01 | 0 | Target |
| P2 | BUTTON02 | 1 | Anti-Target |
| P2 | BUTTON03 | 2 | Radial In |
| P2 | BUTTON04 | 3 | Radial Out |
| P3 | BUTTON05 | 4 | Normal |
| P3 | BUTTON06 | 5 | Anti-Normal |
| P3 | BUTTON07 | 6 | Prograde |
| P3 | BUTTON08 | 7 | Retrograde |
| P4 | BUTTON09 | 8 | Stab Assist |
| P4 | BUTTON10 | 9 | Maneuver |
| P4 | BUTTON11 | 10 | RCS |
| P4 | BUTTON12 | 11 | Invert |

### Discrete Positions

| PCB Connector | PCB Label | KBC Index | Signal | LED Pin |
|---|---|---|---|---|
| P5 | BUTTON13 | 12 | Not installed | Not connected |
| P5 | BUTTON14 | 13 | Not installed | Not connected |
| P5 | BUTTON15 | 14 | Stage | Not connected |
| P5 | BUTTON16 | 15 | Stage Enable | Discrete LED (interlock) |

---

## Installation

### Prerequisites

1. Arduino IDE with megaTinyCore installed
2. KerbalButtonCore library installed (`Sketch → Include Library → Add .ZIP Library`)
3. ShiftIn library installed (InfectedBytes/ArduinoShiftIn)
4. tinyNeoPixel_Static included with megaTinyCore — no separate install needed

### Arduino IDE Settings

| Setting | Value |
|---|---|
| Board | ATtiny816 (megaTinyCore) |
| Clock | 10 MHz internal or higher |
| tinyNeoPixel Port | **Port A** — critical for NeoPixel timing |
| Programmer | jtag2updi or SerialUPDI |

### Flash Procedure

1. Open `KCMk1_Stability_Control.ino` in Arduino IDE
2. Confirm IDE settings above
3. Connect UPDI programmer to the module's UPDI header
4. Click Upload

### Verify Operation

After flashing, the module powers on dark (BOOT_READY → DISABLED). Once the controller sends `CMD_ENABLE`, NeoPixel buttons B0–B11 illuminate in dim white ENABLED state. Toggle the B14/B15 staging inputs and confirm their state changes are reported via I2C. Use the `DiagnosticDump` example sketch for full verification.

---

## I2C Bus Position

| Address | Module |
|---|---|
| `0x20` | UI Control |
| `0x21` | Function Control |
| `0x22` | Action Control |
| `0x23` | **Stability Control** — this module |
| `0x24` | Vehicle Control |
| `0x25` | Time Control |
| `0x26`–`0x2E` | Reserved / future modules |

---

## Protocol Reference

Full I2C protocol specification: `I2C_Protocol_Specification.md` v2.6

Button state packet (7 bytes): 3-byte universal header (status, type ID, transaction counter) followed by events HI/LO and change HI/LO. Bits of note in the events/change planes:
- Bit 10 — RCS button
- Bit 14 — Stage input
- Bit 15 — Stage Enable input

LED nibble notes for `CMD_SET_LED_STATE`:
- B12-B13: any value accepted but ignored (no-connect)
- B14: any value accepted but ignored (no LED hardware)
- B15: drives the discrete staging-interlock LED

---

## Revision History

| Version | Date | Notes |
|---|---|---|
| 1.0 | 2026-04-07 | Initial release |
| 2.0 | 2026-06-28 | Updated to KerbalButtonCore v2.0 (I2C protocol v2.6): RCS added at B10 (GREEN); SAS_ENA/RCS_ENA inputs at B12/B13 removed (moved to Switch Group 2); B14/B15 documented as the discrete staging input and staging-enable input + interlock LED. Packet gains the universal 3-byte header; module powers on dark until CMD_ENABLE. PCB designator corrected to KC-01-1802. |
