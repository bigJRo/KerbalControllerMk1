# Kerbal Controller Mk1 — I2C Protocol Specification

**Version:** 2.0  
**Status:** Released  
**Project:** Kerbal Controller Mk1  
**Date:** 2026-04-28  

---

## 1. Overview

This document defines the I2C communication protocol between the Kerbal Controller Mk1 system controller and all target modules on the bus. It covers bus configuration, the interrupt signalling mechanism, the shared command set, lifecycle management, and device-specific data packet formats.

Terminology follows the 2021 NXP I2C specification:

- **Controller** — the main microcontroller that initiates all I2C transactions
- **Target** — a module that responds to controller-initiated transactions
- **Standard module** — a target using the KerbalButtonCore library on KC-01-1822 hardware, with up to 16 button inputs and NeoPixel/discrete LED outputs
- **Display module** — a target using the Kerbal7SegmentCore library on KC-01-1881/1882 hardware, with encoder, 7-segment display, and 3 NeoPixel buttons
- **Device-specific module** — a target with its own standalone firmware

The controller owns all game state and situational logic. Target modules own only their local hardware — LED colours, button state, display values. The library is a hardware interface layer; all application logic belongs in the sketch.

---

## 2. Bus Configuration

| Parameter | Value |
|---|---|
| Speed | 100 kHz (standard) or 400 kHz (fast) |
| Voltage | 3.3V |
| Pull-ups | 4.7kΩ to 3.3V on SDA and SCL |
| Address range | 0x20 – 0x2F |
| Max modules | 16 |

---

## 3. Interrupt Signalling

Each module has a dedicated active-low open-drain INT output connected to a controller GPIO with pull-up.

**Assertion:** Module pulls INT low when it has a data packet ready for the controller to read.  
**Deassertion:** INT returns high automatically when the controller completes the read transaction.  
**Last-write-wins:** If the sketch calls `k7scQueuePacket()` multiple times before the master reads, the most recent packet overwrites the previous one. The controller always receives the current state.

The controller should poll INT in its main loop or use a hardware interrupt to detect assertions. It should not read from a module unless INT is asserted.

**BOOT_READY:** At power-on, the module asserts INT with lifecycle=BOOT_READY in the status byte. The controller reads the packet and responds with CMD_DISABLE to complete initialisation. The module holds INT asserted until the master reads it — there is no timeout.

---

## 4. Universal Packet Header

Every data packet from any module starts with a 3-byte universal header. The controller always reads at least these 3 bytes from any module. Module-specific payload follows immediately after.

```
Byte 0:  Status byte
           bits 1:0  Lifecycle state
                       00 = ACTIVE      normal operation
                       01 = SLEEPING    paused, state frozen
                       10 = DISABLED    no game context
                       11 = BOOT_READY  awaiting master init
           bit  2    Fault flag (1 = hardware fault present)
           bit  3    Data changed (1 = state changed, always set when packet queued)
           bits 7:4  Reserved (0)
Byte 1:  Module Type ID (see Section 8)
Byte 2:  Transaction counter — uint8, increments on every INT assertion,
         wraps 255→0. Allows controller to detect missed packets.
```

---

## 5. Command Set

All commands are initiated by the controller as I2C write transactions. The command byte is the first byte of every write transaction.

### 5.1 Base Commands (all module types)

| Command | Value | Payload | Description |
|---|---|---|---|
| `CMD_GET_IDENTITY` | 0x01 | — | Query module identity. Module replies with 4-byte identity packet (see §6) |
| `CMD_SET_LED_STATE` | 0x02 | 1 byte | Module-specific LED state byte — sketch interprets |
| `CMD_SET_BRIGHTNESS` | 0x03 | 1 byte | Top nibble → MAX7219 intensity (0–15) for display modules |
| `CMD_BULB_TEST` | 0x04 | 1 byte | 0x01 = start, 0x00 = stop. All LEDs full white, all display segments on. **Commandable regardless of lifecycle state** — fires even while SLEEPING or DISABLED. On stop: ACTIVE restores normal display and LED state; SLEEPING restores frozen pre-sleep state; DISABLED restores to dark |
| `CMD_SLEEP` | 0x05 | — | Freeze state exactly. No visual change. INT suppressed |
| `CMD_WAKE` | 0x06 | — | Resume from sleep. Module sends current state packet |
| `CMD_RESET` | 0x07 | — | Reset to defaults, stay ACTIVE. Module sends confirmation packet |
| `CMD_ACK_FAULT` | 0x08 | — | Acknowledge and clear fault flag |
| `CMD_ENABLE` | 0x09 | — | Enter ACTIVE lifecycle state |
| `CMD_DISABLE` | 0x0A | — | Enter DISABLED lifecycle state |

### 5.2 Display Module Commands

| Command | Value | Payload | Description |
|---|---|---|---|
| `CMD_SET_VALUE` | 0x0D | 2 bytes | Signed int16 big-endian — set threshold/display value |

### 5.3 Command Semantics

**CMD_ENABLE / CMD_DISABLE:** The library sets `cmdState.lifecycle` accordingly. The sketch detects the transition and acts — lighting buttons, resetting state, etc.

**CMD_SLEEP:** State is frozen exactly as-is. No visual change. The sketch should suppress all INT assertions while sleeping. On CMD_WAKE the module resumes and sends a state packet.

**CMD_RESET:** Clears all module-specific state to defaults. Module stays ACTIVE (not DISABLED). Buttons return to backlit. Display blanks. Module sends a confirmation packet.

**CMD_BULB_TEST:** Persistent — 0x01 starts, 0x00 stops. No timeout in the library. The master controls duration explicitly.

**CMD_SET_LED_STATE:** Payload is a raw byte passed through to the sketch. Interpretation is module-specific. Some modules (e.g. GPWS) ignore it because they manage their own LEDs.

---

## 6. Identity Packet

Sent in response to CMD_GET_IDENTITY. Always 4 bytes, regardless of module type.

```
Byte 0:  Module Type ID (see §8)
Byte 1:  Firmware version major
Byte 2:  Firmware version minor
Byte 3:  Capability flags (see §7)
```

The controller reads the identity packet at startup to determine the module type and the data packet size/format for that address.

---

## 7. Capability Flags

Bitmask in identity packet byte 3.

| Bit | Constant | Description |
|---|---|---|
| 0 | `KMC_CAP_EXTENDED_STATES` | Supports WARNING/ALERT/ARMED/PARTIAL_DEPLOY LED states |
| 1 | `KMC_CAP_FAULT` | Active fault condition — clear with CMD_ACK_FAULT |
| 2 | `KMC_CAP_ENCODERS` | Encoder delta present in response packet |
| 3 | `KMC_CAP_JOYSTICK` | Analog joystick axes present in response packet |
| 4 | `KMC_CAP_DISPLAY` | 7-segment display and encoder present |
| 5 | `KMC_CAP_MOTORIZED` | Motorized position control |

---

## 8. Module Registry

| Type ID | Constant | Module | I2C Address | Payload Size |
|---|---|---|---|---|
| 0x01 | `KMC_TYPE_UI_CONTROL` | UI Control | 0x20 | 4 bytes |
| 0x02 | `KMC_TYPE_FUNCTION_CONTROL` | Function Control | 0x21 | 4 bytes |
| 0x03 | `KMC_TYPE_ACTION_CONTROL` | Action Control | 0x22 | 4 bytes |
| 0x04 | `KMC_TYPE_STABILITY_CONTROL` | Stability Control | 0x23 | 4 bytes |
| 0x05 | `KMC_TYPE_VEHICLE_CONTROL` | Vehicle Control | 0x24 | 4 bytes |
| 0x06 | `KMC_TYPE_TIME_CONTROL` | Time Control | 0x25 | 4 bytes |
| 0x07 | `KMC_TYPE_EVA_MODULE` | EVA Module | 0x26 | 4 bytes |
| 0x08 | — | Reserved | 0x27 | — |
| 0x09 | `KMC_TYPE_JOYSTICK_ROTATION` | Joystick Rotation | 0x28 | 8 bytes |
| 0x0A | `KMC_TYPE_JOYSTICK_TRANS` | Joystick Translation | 0x29 | 8 bytes |
| 0x0B | `KMC_TYPE_GPWS_INPUT` | GPWS Input Panel | 0x2A | 5 bytes |
| 0x0C | `KMC_TYPE_PRE_WARP_TIME` | Pre-Warp Time | 0x2B | 5 bytes |
| 0x0D | `KMC_TYPE_THROTTLE` | Throttle Module | 0x2C | 4 bytes |
| 0x0E | `KMC_TYPE_DUAL_ENCODER` | Dual Encoder | 0x2D | 4 bytes |
| 0x0F | `KMC_TYPE_SWITCH_PANEL` | Switch Panel | 0x2E | 4 bytes |
| 0x10 | `KMC_TYPE_INDICATOR` | Indicator Module | 0x2F | 0 bytes |

Total packet size = 3-byte header + payload size.

---

## 9. Data Packet Formats

### 9.1 Standard Button Module (4-byte payload, 7 bytes total)

Modules: UI Control, Function Control, Action Control, Stability Control, Vehicle Control, Time Control, EVA Module, Throttle, Dual Encoder, Switch Panel.

```
Header (3 bytes):  see §4
Byte 3:  Events     — rising edge bitmask  (bit0=button0 … bit7=button7)
Byte 4:  Events     — rising edge bitmask  (bit0=button8 … bit7=button15)
Byte 5:  Change     — change mask          (bit0=button0 … bit7=button7)
Byte 6:  Change     — change mask          (bit0=button8 … bit7=button15)
```

### 9.2 Joystick Module (8-byte payload, 11 bytes total)

Modules: Joystick Rotation (0x09), Joystick Translation (0x0A).

```
Header (3 bytes):  see §4
Byte 3:  Events    — button rising edge bitmask (bit0=BTN01 … bit3=BTN_EN)
Byte 4:  Change    — button change mask
Byte 5:  State     — button persistent state
Byte 6:  Axis X HI — signed int16, big-endian (-32768 to 32767)
Byte 7:  Axis X LO
Byte 8:  Axis Y HI — signed int16, big-endian
Byte 9:  Axis Y LO
Byte 10: Axis Z HI — signed int16, big-endian (rotation module only, 0 otherwise)
Byte 11: Axis Z LO
```

### 9.3 GPWS Input Panel (5-byte payload, 8 bytes total)

Module: GPWS Input Panel (0x0B).

```
Header (3 bytes):  see §4
Byte 3:  Events    — button rising edge bitmask
                      bit 0 = BTN01 (GPWS Enable, 3-state cycle)
                      bit 1 = BTN02 (Proximity Alarm, toggle)
                      bit 2 = BTN03 (Rendezvous Radar, toggle)
                      bit 3 = BTN_EN (encoder pushbutton)
Byte 4:  Change    — button change mask (same bit layout)
Byte 5:  State     — module state
                      bits 1:0 = BTN01 cycle state (0=OFF, 1=ACTIVE, 2=PROX)
                      bit  2   = BTN02 active (proximity alarm on)
                      bit  3   = BTN03 active (rendezvous radar on)
Byte 6:  Value HI  — altitude threshold, signed int16, big-endian
Byte 7:  Value LO
```

**INT suppression:** When BTN01 is in state 0 (GPWS off), BTN02, BTN03, and encoder events are suppressed. BTN01 always reports since it is the mode switch.

### 9.4 Pre-Warp Time Module (5-byte payload, 8 bytes total)

Module: Pre-Warp Time (0x0C). Format TBD — same header structure, payload to be defined when module is implemented.

---

## 10. Lifecycle State Machine

```
                    Power on
                       │
                  BOOT_READY ──── master reads ──── CMD_DISABLE
                                                        │
           ┌─────────────────────────────────────── DISABLED
           │                                            │
        CMD_ENABLE ◄──────────────────────────── CMD_DISABLE
           │
        ACTIVE ◄──── CMD_WAKE ────── SLEEPING ◄── CMD_SLEEP
           │                                           │
           └─────────────────── stays ACTIVE ──────────┘
                             (on CMD_RESET)
```

**BOOT_READY** — Module has just powered on. INT asserted. Controller reads packet, sends CMD_DISABLE.  
**DISABLED** — No game context. All dark. Input suppressed. Defaults reset.  
**ACTIVE** — Normal operation. Sketch running. Pilot can interact.  
**SLEEPING** — State frozen exactly as-is. No visual change. Input suppressed.

Transitions:
- BOOT_READY → DISABLED: controller sends CMD_DISABLE after reading BOOT_READY packet
- DISABLED → ACTIVE: controller sends CMD_ENABLE when flight scene loads
- ACTIVE → SLEEPING: controller sends CMD_SLEEP on game pause
- SLEEPING → ACTIVE: controller sends CMD_WAKE on game resume
- ACTIVE → DISABLED: controller sends CMD_DISABLE on flight scene exit, serial loss, or EVA
- Any → ACTIVE (same): CMD_RESET clears values but lifecycle stays ACTIVE

---

## 11. Vessel Switch Behaviour

Vessel switch behaviour is module-specific and determined by the module's controller sketch. The I2C protocol has no vessel-switch command. The controller sends whatever combination of standard commands is appropriate for the module type:

- **GPWS Input Panel** — no action. State persists across vessel switches. Pilot configures as needed.
- Other modules — TBD when implemented.

---

## 12. LED State Nibble Values

Used with CMD_SET_LED_STATE for modules that support nibble-packed LED state.

| Value | Constant | Description |
|---|---|---|
| 0x0 | `KMC_LED_OFF` | Unlit |
| 0x1 | `KMC_LED_ENABLED` | ENABLED backlight (`KMC_BACKLIT`) |
| 0x2 | `KMC_LED_ACTIVE` | Full brightness, per-button colour |
| 0x3 | `KMC_LED_WARNING` | Flashing amber, 500ms on/off |
| 0x4 | `KMC_LED_ALERT` | Flashing red, 150ms on/off |
| 0x5 | `KMC_LED_ARMED` | Static cyan |
| 0x6 | `KMC_LED_PARTIAL_DEPLOY` | Static amber |
| 0x7–0xF | — | Reserved |

---

## 13. Bus Timing Estimates

At 100 kHz I2C, approximate worst-case sweep times:

| Scenario | Bytes | Time |
|---|---|---|
| Read one 8-byte module | 11 bytes | ~1.1ms |
| Read all 16 addresses (8-byte max) | 176 bytes | ~17.6ms |
| Write CMD_ENABLE | 3 bytes | ~0.3ms |

At 400 kHz, divide by approximately 4.

---

## 14. Revision History

| Version | Date | Changes |
|---|---|---|
| 2.0 | 2026-04-28 | Universal 3-byte header on all packets. Lifecycle enum (ACTIVE/SLEEPING/DISABLED/BOOT_READY) in status byte. Transaction counter. Signed int16 for SET_VALUE. GPWS packet updated to 5-byte payload (8 bytes total). Vessel switch is protocol-silent — module-specific behaviour. CMD_BULB_TEST payload defined (0x01/0x00 start/stop). CMD_SLEEP semantics clarified (freeze, no visual change). CMD_RESET semantics clarified (stay ACTIVE). Lifecycle state machine documented. |
| 1.4 | 2026-04-26 | Joystick module packet formats added. Module registry expanded. KBC-centric language removed. |
| 1.3 | 2026-04-25 | EVA module added. Non-KBC module concept introduced. |
| 1.2 | 2026-04-14 | CMD_ENABLE/DISABLE added. Capability flags expanded. |
| 1.1 | 2026-04-11 | CMD_SET_VALUE added for display modules. |
| 1.0 | 2026-04-09 | Initial release. Button modules only. |

---

## 15. Implementation Notes

**Counter wrapping:** The transaction counter wraps 255→0. This is expected and correct. The controller should handle the wrap gracefully — it is not an error condition.

**Unexpected reads:** If the controller reads from a module without a prior INT assertion, the module returns a zeroed 3-byte header. The controller should not do this in normal operation.

**Identity before reads:** The controller must perform CMD_GET_IDENTITY at startup for each address before reading data packets, to determine the correct packet size and format.

**Wire.begin():** Called internally by the module library. Do not call it in the sketch.
