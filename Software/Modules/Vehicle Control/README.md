# KCMk1_Vehicle_Control

**Module:** Vehicle Control  
**Version:** 2.2  
**Date:** 2026-06-28  
**Author:** J. Rostoker — Jeb's Controller Works  
**License:** GNU General Public License v3.0 (GPL-3.0)  
**Hardware:** KC-01-1812 Wide Button Module Base v1.1  
**Library:** KerbalButtonCore v2.0.0 (24-input)  

---

## Overview

The Vehicle Control module provides vehicle systems management including brakes, lights, landing gear, antenna, fuel cell, solar array, cargo door, radiator, ladder, heat shield, and parachutes for Kerbal Space Program. The heat shield (B8) and parachute (B10/B11) buttons are single-button state machines (deploy → cut/release) and support extended LED states; the controller sequences them.

This module reads **24 inputs** via three daisy-chained 74HC165 shift registers: the 12 NeoPixel buttons at KBC indices 0–11, and the eight **Switch Group 2** panel switches at KBC indices 16–23. KBC indices 12–15 are no-connects (the former parking-brake / parachutes-armed / lights-lock / gear-lock discrete inputs were removed — those switches are now Switch Group 2 as of v2.0). The 24-input variant is selected in the sketch via `#define KBC_INPUT_COUNT 24` / `KBC_SHIFTREG_COUNT 3`.

This module declares `KBC_CAP_EXTENDED_STATES` — the system controller uses WARNING, ALERT, ARMED, and PARTIAL_DEPLOY states on the state-machine buttons (B8/B10/B11) to communicate deploy / cut-release status. Layout follows Module UI Reference v5.4 (canonical).

---

## Module Identity

| Parameter | Value |
|---|---|
| I2C Address | `0x24` |
| Module Type ID | `0x05` (KBC_TYPE_VEHICLE_CONTROL) |
| Capability Flags | `0x01` (KBC_CAP_EXTENDED_STATES) |
| Extended States | Yes — state-machine buttons B8 / B10 / B11 |
| NeoPixel Buttons | 12 (KBC indices 0-11) |
| Switch Group 2 inputs | 8 (KBC indices 16–23, discrete, no LED) |
| Not Installed | KBC indices 12–15 (no-connect) |
| Data packet | 9 bytes (3-byte header + 6-byte payload) |

---

## Panel Layout

Physical panel orientation: 6 rows x 2 columns. Column 1 is leftmost, Column 2 is rightmost. Button numbering starts top-right (B0) and proceeds left within each row, then steps down.

![Vehicle Control Panel Layout](panel_layout.svg)

Active state colors shown. All NeoPixel buttons illuminate dim white in the ENABLED state. The state-machine buttons (B8 Heat Shield, B10 Main Chute, B11 Drogue Chute) also support WARNING, ALERT, ARMED, and PARTIAL_DEPLOY states. Switch Group 2 inputs (B16–B23) are outside the NeoPixel grid and not shown.

---

## Button Reference

### NeoPixel Buttons (KBC indices 0-11)

| KBC Index | PCB Label | Function | Active Color | Extended States | Notes |
|---|---|---|---|---|---|
| B0 | BUTTON01 | Brakes | RED | No | High-consequence — red for attention |
| B1 | BUTTON02 | Lights | YELLOW | No | Natural light association |
| B2 | BUTTON03 | Antenna | PINK | No | CAG 13 — comms |
| B3 | BUTTON04 | Gear | GREEN | No | Terrain family |
| B4 | BUTTON05 | Fuel Cell | CYAN | No | CAG 14 — power generation |
| B5 | BUTTON06 | Solar Array | GOLD | No | CAG 15 — power / sunlight |
| B6 | BUTTON07 | Cargo Door | TEAL | No | CAG 16 — terrain family |
| B7 | BUTTON08 | Radiator | ORANGE | No | CAG 17 — thermal management |
| B8 | BUTTON09 | Heat Shield | GREEN (deploy) / RED (release) | Yes | CAG 19/20 — state machine |
| B9 | BUTTON10 | Ladder | LIME | No | CAG 18 — terrain family |
| B10 | BUTTON11 | Main Chute | GREEN (deploy) / RED (cut) | Yes | CAG 21/22 — state machine, requires CHUTE ARM |
| B11 | BUTTON12 | Drogue Chute | GREEN (deploy) / RED (cut) | Yes | CAG 23/24 — state machine, requires CHUTE ARM |

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
- **State-machine buttons — deploy (GREEN) / cut-release (RED)** — Heat Shield (B8), Main Chute (B10), and Drogue Chute (B11) are the most consequential actions on the panel. GREEN for deploy (positive, controlled), RED for cut/release (irreversible). The controller sequences deploy → cut via extended states.
- **Power family (CYAN, GOLD)** — Fuel Cell (CYAN) and Solar Array (GOLD) are the two power-generation controls. Distinct from YELLOW (lights) and ORANGE (thermal).

---

## Extended LED States — State-Machine Buttons (B8 / B10 / B11)

The Heat Shield (B8), Main Chute (B10), and Drogue Chute (B11) buttons support the full extended state set. The system controller uses these to communicate deploy / cut-release status. All flash timing and visual behavior is handled by the module.

| State | Color | Behavior | Meaning |
|---|---|---|---|
| ENABLED | White (dim) | Static | System nominal, function available |
| ACTIVE | GREEN | Static | Deployed (the button's active color) |
| CUT | RED | Static | Cut / released — terminal state |
| WARNING | Amber | 500ms flash | Deployment window approaching |
| ALERT | Red | 150ms flash | Deploy / cut immediately |
| ARMED | Cyan | Static | Parachute armed and ready |
| PARTIAL_DEPLOY | Amber | Static | Drogue deployed, main pending |

The state machines render deploy as static GREEN (`KBC_LED_ACTIVE`) and the terminal cut/release as static RED (`KBC_LED_CUT`, nibble `0x7`, added in KerbalButtonCore v2.1). The controller drives the OFF → DEPLOYED → CUT/RELEASED sequence by sending the corresponding LED state; the module renders each statically per the canonical Module UI Reference.

---

## Wiring

### NeoPixel Button Inputs

| PCB Connector | PCB Label | KBC Index | Function |
|---|---|---|---|
| P2 | BUTTON01 | 0 | Brakes |
| P2 | BUTTON02 | 1 | Lights |
| P2 | BUTTON03 | 2 | Antenna |
| P2 | BUTTON04 | 3 | Gear |
| P3 | BUTTON05 | 4 | Fuel Cell |
| P3 | BUTTON06 | 5 | Solar Array |
| P3 | BUTTON07 | 6 | Cargo Door |
| P3 | BUTTON08 | 7 | Radiator |
| P4 | BUTTON09 | 8 | Heat Shield |
| P4 | BUTTON10 | 9 | Ladder |
| P4 | BUTTON11 | 10 | Main Chute |
| P4 | BUTTON12 | 11 | Drogue Chute |
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

LED state nibble values for the state-machine buttons (B8 / B10 / B11):
```
0x0 = OFF
0x1 = ENABLED  (dim white)
0x2 = ACTIVE   (deployed — static GREEN)
0x3 = WARNING  (amber flash 500ms)
0x4 = ALERT    (red flash 150ms)
0x5 = ARMED    (static cyan)
0x6 = PARTIAL_DEPLOY (static amber)
0x7 = CUT      (cut / released — static RED, terminal)
```

---

## Revision History

| Version | Date | Notes |
|---|---|---|
| 1.0 | 2026-04-07 | Initial release |
| 2.0 | 2026-06-28 | Updated to KerbalButtonCore v2.0 (I2C protocol v2.6): old B12–B15 discrete inputs removed; Switch Group 2 added at KBC indices 16–23 via the 24-input / 3-byte shift-register variant. Packet is now 9 bytes (3-byte header + 6-byte payload); module powers on dark until CMD_ENABLE. PCB designator corrected to KC-01-1812. |
| 2.1 | 2026-06-28 | B0–B11 layout reconciled to Module UI Reference v5.4 (canonical): B2 Antenna, B4 Fuel Cell, B5 Solar Array, B6 Cargo Door; B8 is now Heat Shield and B10/B11 are single-button Main/Drogue Chute state machines (deploy → cut/release) replacing the former separate deploy/cut buttons; B9 is Ladder. Heat shield and parachutes use extended LED states sequenced controller-side. |
| 2.2 | 2026-06-28 | State machines now render the canonical static-red cut/release terminal via the new `KBC_LED_CUT` state (nibble 0x7, KerbalButtonCore v2.1 / KerbalModuleCommon v1.4); deploy = static GREEN, cut/release = static RED. Removed the prior controller-side-only rendering limitation. |
