# KCMk1_Vehicle_Control

**Module:** Vehicle Control  
**Version:** 2.0  
**Date:** 2026-06-28  
**Author:** J. Rostoker — Jeb's Controller Works  
**License:** GNU General Public License v3.0 (GPL-3.0)  
**Hardware:** KC-01-1812 Wide Button Module Base v1.1  
**Library:** KerbalButtonCore v2.0.0 (24-input)  

---

## Overview

The Vehicle Control module provides vehicle systems management including brakes, lights, landing gear, solar arrays, antenna, cargo door, ladder, radiators, and parachute deployment for Kerbal Space Program. Parachute buttons (B8-B11) support extended LED states for pre-deployment sequencing.

This module reads **24 inputs** via three daisy-chained 74HC165 shift registers: the 12 NeoPixel buttons at KBC indices 0–11, and the eight **Switch Group 2** panel switches at KBC indices 16–23. KBC indices 12–15 are no-connects (the former parking-brake / parachutes-armed / lights-lock / gear-lock discrete inputs were removed — those switches are now Switch Group 2 as of v2.0). The 24-input variant is selected in the sketch via `#define KBC_INPUT_COUNT 24` / `KBC_SHIFTREG_COUNT 3`.

This module declares `KBC_CAP_EXTENDED_STATES` — the system controller uses WARNING, ALERT, ARMED, and PARTIAL_DEPLOY states on the parachute buttons to communicate deployment status.

---

## Module Identity

| Parameter | Value |
|---|---|
| I2C Address | `0x24` |
| Module Type ID | `0x05` (KBC_TYPE_VEHICLE_CONTROL) |
| Capability Flags | `0x01` (KBC_CAP_EXTENDED_STATES) |
| Extended States | Yes — parachute buttons B8-B11 |
| NeoPixel Buttons | 12 (KBC indices 0-11) |
| Switch Group 2 inputs | 8 (KBC indices 16–23, discrete, no LED) |
| Not Installed | KBC indices 12–15 (no-connect) |
| Data packet | 9 bytes (3-byte header + 6-byte payload) |

---

## Panel Layout

Physical panel orientation: 6 rows x 2 columns. Column 1 is leftmost, Column 2 is rightmost. Button numbering starts top-right (B0) and proceeds left within each row, then steps down.

![Vehicle Control Panel Layout](panel_layout.svg)

Active state colors shown. All NeoPixel buttons illuminate dim white in the ENABLED state. Buttons marked with extended states (B8-B11) also support WARNING, ALERT, ARMED, and PARTIAL_DEPLOY states. Switch Group 2 inputs (B16–B23) are outside the NeoPixel grid and not shown.

---

## Button Reference

### NeoPixel Buttons (KBC indices 0-11)

| KBC Index | PCB Label | Function | Active Color | Extended States | Notes |
|---|---|---|---|---|---|
| B0 | BUTTON01 | Brakes | RED | No | Significant — red for attention |
| B1 | BUTTON02 | Lights | YELLOW | No | Natural light association |
| B2 | BUTTON03 | Solar Array | GOLD | No | Power / sunlight |
| B3 | BUTTON04 | Gear | GREEN | No | Terrain family |
| B4 | BUTTON05 | Cargo Door | TEAL | No | Terrain family |
| B5 | BUTTON06 | Antenna | PINK | No | Comms family |
| B6 | BUTTON07 | Ladder | LIME | No | Terrain family |
| B7 | BUTTON08 | Radiator | ORANGE | No | Thermal management |
| B8 | BUTTON09 | Main Deploy | GREEN | Yes | Parachute deploy |
| B9 | BUTTON10 | Drogue Deploy | GREEN | Yes | Parachute deploy |
| B10 | BUTTON11 | Main Cut | RED | Yes | Irreversible |
| B11 | BUTTON12 | Drogue Cut | RED | Yes | Irreversible |

KBC indices 12–15 (BUTTON13–16) are no-connects on the PCB.

### Switch Group 2 (KBC indices 16–23, discrete inputs, no LED)

Reported in the button-event payload (inputs 16–23). Switch semantics and controller actions are resolved on the main controller; the module reports raw input state only. The CHUTE switch (B16) is the safety interlock for the parachute state machines.

| KBC Index | Switch | OFF / ON |
|---|---|---|
| B16 | CHUTE | SAFE / ARM |
| B17 | GEAR | UP / DOWN |
| B18 | BRAKE | REL / LOCK |
| B19 | EXT LT | OFF / ILLUM |
| B20 | SAS | OFF / ENAB |
| B21 | RCS | OFF / ENAB |
| B22 | THC/RHC | NORM / PREC |
| B23 | AUDIO | MUTE / LIVE |

### Color Design Notes

- **Terrain family (GREEN, TEAL, LIME)** — Gear, Cargo Door, and Ladder share a green-adjacent family. Physical proximity on the vehicle to the ground unifies them.
- **Brakes (RED)** — not strictly irreversible but high consequence — red draws attention.
- **Parachute deploy (GREEN) / cut (RED)** — the most consequential paired actions on the panel. GREEN for deploy (positive, controlled), RED for cut (irreversible). Extended states carry the sequencing story.
- **Solar Array (GOLD)** — distinct warm color for power generation. Not confused with YELLOW (lights) or ORANGE (thermal).

---

## Extended LED States — Parachute Buttons

Buttons B8-B11 support the full extended state set. The system controller uses these to communicate parachute deployment status. All timing and visual behavior is handled by the module.

| State | Color | Behavior | Meaning |
|---|---|---|---|
| ENABLED | White (dim) | Static | System nominal, parachutes available |
| ACTIVE | GREEN / RED | Static | Normal active state |
| WARNING | Amber | 500ms flash | Deployment window approaching |
| ALERT | Red | 150ms flash | Deploy immediately |
| ARMED | Cyan | Static | Parachute armed and ready |
| PARTIAL_DEPLOY | Amber | Static | Drogue deployed, main pending |

---

## Wiring

### NeoPixel Button Inputs

| PCB Connector | PCB Label | KBC Index | Function |
|---|---|---|---|
| P2 | BUTTON01 | 0 | Brakes |
| P2 | BUTTON02 | 1 | Lights |
| P2 | BUTTON03 | 2 | Solar Array |
| P2 | BUTTON04 | 3 | Gear |
| P3 | BUTTON05 | 4 | Cargo Door |
| P3 | BUTTON06 | 5 | Antenna |
| P3 | BUTTON07 | 6 | Ladder |
| P3 | BUTTON08 | 7 | Radiator |
| P4 | BUTTON09 | 8 | Main Deploy |
| P4 | BUTTON10 | 9 | Drogue Deploy |
| P4 | BUTTON11 | 10 | Main Cut |
| P4 | BUTTON12 | 11 | Drogue Cut |
| — | (no-connect) | 12–15 | Not connected |
| DB127S #1 | SW1–4 | 16–19 | Switch Group 2: CHUTE, GEAR, BRAKE, EXT LT |
| DB127S #2 | SW5–8 | 20–23 | Switch Group 2: SAS, RCS, THC/RHC, AUDIO |

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

1. Open `KCMk1_Vehicle_Control.ino` in Arduino IDE
2. Confirm IDE settings above
3. Connect UPDI programmer to the module's UPDI header
4. Click Upload

### Verify Operation

After flashing, the module powers on dark (BOOT_READY → DISABLED). Once the controller sends `CMD_ENABLE`, all 12 NeoPixel buttons illuminate in dim white ENABLED state. Send `KBC_LED_WARNING` to B8 via `CMD_SET_LED_STATE` and confirm the amber flash. Verify the Switch Group 2 inputs (KBC indices 16–23) are reported in the data packet. Use the `DiagnosticDump` example sketch for full input and extended state verification.

---

## I2C Bus Position

| Address | Module |
|---|---|
| `0x20` | UI Control |
| `0x21` | Function Control |
| `0x22` | Action Control |
| `0x23` | Stability Control |
| `0x24` | **Vehicle Control** — this module |
| `0x25` | Time Control |
| `0x26`–`0x2E` | Reserved / future modules |

---

## Protocol Reference

Full I2C protocol specification: `I2C_Protocol_Specification.md` v2.6 (§9.1.1 switch-group variant)

Identity response capability flags: `0x01` (KBC_CAP_EXTENDED_STATES) — the system controller uses this to know extended LED states are valid for this module.

Button state packet (9 bytes): 3-byte universal header (status, type ID, transaction counter) followed by three event bytes and three change bytes. Switch Group 2 occupies inputs 16–23 (event/change byte index 2):
- Bit 16 — CHUTE, 17 — GEAR, 18 — BRAKE, 19 — EXT LT
- Bit 20 — SAS, 21 — RCS, 22 — THC/RHC, 23 — AUDIO

LED state nibble values for parachute buttons (B8-B11):
```
0x0 = OFF
0x1 = ENABLED  (dim white)
0x2 = ACTIVE   (full GREEN or RED)
0x3 = WARNING  (amber flash 500ms)
0x4 = ALERT    (red flash 150ms)
0x5 = ARMED    (static cyan)
0x6 = PARTIAL_DEPLOY (static amber)
```

---

## Revision History

| Version | Date | Notes |
|---|---|---|
| 1.0 | 2026-04-07 | Initial release |
| 2.0 | 2026-06-28 | Updated to KerbalButtonCore v2.0 (I2C protocol v2.6): old B12–B15 discrete inputs removed; Switch Group 2 added at KBC indices 16–23 via the 24-input / 3-byte shift-register variant. Packet is now 9 bytes (3-byte header + 6-byte payload); module powers on dark until CMD_ENABLE. PCB designator corrected to KC-01-1812. B0–B11 button layout unchanged. |
