# KCMk1_Action_Control

**Module:** Action Control  
**Version:** 2.0  
**Date:** 2026-06-28  
**Author:** J. Rostoker — Jeb's Controller Works  
**License:** GNU General Public License v3.0 (GPL-3.0)  
**Hardware:** KC-01-1802 Button Module Base v1.1  
**Library:** KerbalButtonCore v2.0.0  

---

## Overview

The Action Control module provides twelve custom action group toggles (AG1–AG12) for Kerbal Space Program. All twelve positions are uniform GREEN action-group buttons.

This module uses all 12 NeoPixel RGB button positions (KBC indices 0–11). KBC indices 12–15 are no-connects on the PCB. (As of v2.0 the former CP PRI / CP ALT control-point buttons at B10/B11 are action groups — the control-point functions moved to the AUX CTRL module — and the CTRL_MODE discrete inputs moved to the direct-wired Ctrl Mode Toggle on the joystick.)

---

## Module Identity

| Parameter | Value |
|---|---|
| I2C Address | `0x22` |
| Module Type ID | `0x03` (KBC_TYPE_ACTION_CONTROL) |
| Capability Flags | `0x00` (core states only) |
| Extended States | No |
| NeoPixel Buttons | 12 (KBC indices 0–11) |
| Not Installed | KBC indices 12–15 (no-connect) |

---

## Panel Layout

Physical panel orientation: 2 rows × 6 columns. Column 6 is leftmost, Column 1 is rightmost. Button numbering starts top-right (B0) and proceeds left across each row.

![Action Control Panel Layout](panel_layout.svg)

Active state colors shown. All NeoPixel buttons illuminate dim white in the ENABLED state.

---

## Button Reference

### NeoPixel Buttons (KBC indices 0–11)

The AG number is resolved controller-side; the module reports button events only.

| KBC Index | PCB Label | Function | Active Color | Notes |
|---|---|---|---|---|
| B0 | BUTTON01 | AG 6 | GREEN | Action group — user defined |
| B1 | BUTTON02 | AG 12 | GREEN | Action group — user defined |
| B2 | BUTTON03 | AG 5 | GREEN | Action group — user defined |
| B3 | BUTTON04 | AG 11 | GREEN | Action group — user defined |
| B4 | BUTTON05 | AG 4 | GREEN | Action group — user defined |
| B5 | BUTTON06 | AG 10 | GREEN | Action group — user defined |
| B6 | BUTTON07 | AG 3 | GREEN | Action group — user defined |
| B7 | BUTTON08 | AG 9 | GREEN | Action group — user defined |
| B8 | BUTTON09 | AG 2 | GREEN | Action group — user defined |
| B9 | BUTTON10 | AG 8 | GREEN | Action group — user defined |
| B10 | BUTTON11 | AG 1 | GREEN | Action group — user defined (was CP PRI) |
| B11 | BUTTON12 | AG 7 | GREEN | Action group — user defined (was CP ALT) |

KBC indices 12–15 (BUTTON13–16) are no-connects on the PCB.

### Color Design Notes

- **Action Groups (GREEN)** — all twelve action groups share uniform green. They are user-configurable per vessel in KSP so no functional color distinction is meaningful between them. A single illuminated green button instantly identifies the active group.

---

## LED States

This module uses core LED states only for NeoPixel buttons. Discrete inputs have no LED state.

| State | NeoPixel Behavior | Discrete Behavior |
|---|---|---|
| OFF | Unlit | N/A |
| ENABLED | Dim white backlight | N/A |
| ACTIVE | Full brightness, button color | N/A |

---

## Wiring

### NeoPixel Button Inputs

| PCB Connector | PCB Label | KBC Index | Function |
|---|---|---|---|
| P2 | BUTTON01 | 0 | AG 6 |
| P2 | BUTTON02 | 1 | AG 12 |
| P2 | BUTTON03 | 2 | AG 5 |
| P2 | BUTTON04 | 3 | AG 11 |
| P3 | BUTTON05 | 4 | AG 4 |
| P3 | BUTTON06 | 5 | AG 10 |
| P3 | BUTTON07 | 6 | AG 3 |
| P3 | BUTTON08 | 7 | AG 9 |
| P4 | BUTTON09 | 8 | AG 2 |
| P4 | BUTTON10 | 9 | AG 8 |
| P4 | BUTTON11 | 10 | AG 1 |
| P4 | BUTTON12 | 11 | AG 7 |

KBC indices 12–15 (BUTTON13–16) are no-connects on the PCB.

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

1. Open `KCMk1_Action_Control.ino` in Arduino IDE
2. Confirm IDE settings above
3. Connect UPDI programmer to the module's UPDI header
4. Click Upload

### Verify Operation

After flashing, the module powers on dark (BOOT_READY → DISABLED). Once the controller sends `CMD_ENABLE`, all 12 NeoPixel buttons illuminate in a dim white ENABLED state. Use the `DiagnosticDump` example sketch from the KerbalButtonCore library for full verification.

---

## I2C Bus Position

This module occupies address `0x22`. The system controller expects `KBC_TYPE_ACTION_CONTROL` (0x03) at this address during startup enumeration.

| Address | Module |
|---|---|
| `0x20` | UI Control |
| `0x21` | Function Control |
| `0x22` | **Action Control** ← this module |
| `0x23` | Stability Control |
| `0x24` | Vehicle Control |
| `0x25` | Time Control |
| `0x26`–`0x2E` | Reserved / future modules |

---

## Protocol Reference

Full I2C protocol specification: `I2C_Protocol_Specification.md` v2.6

Button state packet (module → controller, INT-triggered, 7 bytes):
```
Byte 0:   Status byte   (lifecycle bits 1:0, fault bit 2, data-changed bit 3)
Byte 1:   Module Type ID
Byte 2:   Transaction counter
Byte 3:   Button events HI  (bit N = button N, current state)
Byte 4:   Button events LO  (bit N = button 8+N)
Byte 5:   Change mask  HI   (bit N = button N changed since last read)
Byte 6:   Change mask  LO
```

LED state command (controller → module):
```
CMD_SET_LED_STATE (0x02) + 8 bytes nibble-packed
Values for the B12–B15 nibbles are accepted but ignored (no LED hardware)
```

---

## Revision History

| Version | Date | Notes |
|---|---|---|
| 1.0 | 2026-04-07 | Initial release |
| 2.0 | 2026-06-28 | Updated to KerbalButtonCore v2.0 (I2C protocol v2.6): B10/B11 are now AG1/AG7 action groups (GREEN), former CP PRI/ALT moved to AUX CTRL; CTRL_MODE discrete inputs removed (moved to direct-wired Ctrl Mode Toggle); B12–B15 now no-connect. Packet gains the universal 3-byte header; module powers on dark until CMD_ENABLE. PCB designator corrected to KC-01-1802. |
