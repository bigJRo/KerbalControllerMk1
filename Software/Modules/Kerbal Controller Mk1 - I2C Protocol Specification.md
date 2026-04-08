# Kerbal Controller Mk1 — I2C Protocol Specification

**Version:** 1.3  
**Status:** Released  
**Project:** Kerbal Controller Mk1  

---

## 1. Overview

This document defines the I2C communication protocol between the Kerbal Controller Mk1 system controller and all target modules on the bus. Modules may be KerbalButtonCore (KBC) standard modules or non-KBC device-specific modules. All share the same bus, command set, and identity response format. Data response formats are device-specific and resolved by the controller at startup via CMD_GET_IDENTITY.

Terminology follows the 2021 NXP I2C specification:
- **Controller** — the main microcontroller that initiates transactions (formerly Master)
- **Target** — a module that responds to transactions (formerly Slave)
- **KBC module** — a target running the KerbalButtonCore library on KC-01-1822 hardware
- **Non-KBC module** — a target with device-specific hardware and standalone firmware

The controller owns all game state and situational logic. Target modules own only LED display behavior for their assigned states.

---

## 2. Bus Configuration

| Parameter | Value |
|---|---|
| Protocol | I2C |
| Recommended Speed | 400 kHz (Fast Mode) |
| Maximum Devices | 15 |
| Address Range | `0x20` – `0x2E` |
| Address Assignment | Set per sketch via Wire library |

Addresses are assigned sequentially starting at `0x20`. Each module sketch hardcodes its assigned address.

---

## 3. Interrupt Line

Each module has a dedicated active-low interrupt line (INT) connected to the controller.

| Condition | INT State |
|---|---|
| No unread button changes | High (released) |
| Button state has changed | Low (asserted) |
| Controller completes read transaction | Cleared |
| Live state differs from last sent | Re-asserted immediately |

### 3.1 Dual-Buffer Latching

To guarantee that every press and release edge is reported, modules implement a dual-buffer strategy:

- **Live buffer** — updated continuously as buttons change
- **Latched buffer** — snapshot of live buffer taken at the moment INT is asserted

When the controller reads, it receives the latched buffer. On completion of the read transaction:
1. INT clears
2. The live buffer is snapshotted into the latch
3. If the new latch differs from what was just sent, INT re-asserts immediately

This guarantees at least one INT + read cycle per state change edge. Rapid toggling between reads may coalesce, but no edge is silently dropped.

---

## 4. Packet Formats

### 4.1 Button State Packet (Target → Controller)

Transmitted in response to a controller read request following INT assertion.

The **KBC standard packet** is 4 bytes and applies to all KBC modules (Type IDs 0x01–0x06). Non-KBC modules use device-specific packet formats of varying length. The controller determines the correct packet format and read length for each address from the Module Type ID received during CMD_GET_IDENTITY at startup. See Section 9 for device-specific packet formats.

**KBC Standard Packet — 4 bytes:**

```
Byte 0:  Current state HIGH   — bits 15–8  (1 = pressed, 0 = released)
Byte 1:  Current state LOW    — bits 7–0
Byte 2:  Change mask HIGH     — bits 15–8  (1 = changed since last read)
Byte 3:  Change mask LOW      — bits 7–0
```

Bit 0 of each pair corresponds to Button 0; bit 15 corresponds to Button 15.

**Controller logic:** AND the current state mask and change mask to determine which buttons changed and what state they changed to.

---

### 4.2 LED State Packet (Controller → Target)

Transmitted when the controller updates LED states on a module.

**Command byte:** `0x02`  
**Payload length:** 8 bytes  
**Total on wire:** 9 bytes (address + command + 8 payload bytes)

Each button is assigned one nibble (4 bits). Two buttons are packed per byte, high nibble first.

```
Payload Byte 0:  Button 0  [7:4]  |  Button 1  [3:0]
Payload Byte 1:  Button 2  [7:4]  |  Button 3  [3:0]
Payload Byte 2:  Button 4  [7:4]  |  Button 5  [3:0]
Payload Byte 3:  Button 6  [7:4]  |  Button 7  [3:0]
Payload Byte 4:  Button 8  [7:4]  |  Button 9  [3:0]
Payload Byte 5:  Button 10 [7:4]  |  Button 11 [3:0]
Payload Byte 6:  Button 12 [7:4]  |  Button 13 [3:0]
Payload Byte 7:  Button 14 [7:4]  |  Button 15 [3:0]
```

#### LED State Nibble Values

| Value | State | Behavior | Color |
|---|---|---|---|
| `0x0` | OFF | Unlit | — |
| `0x1` | ENABLED | Dim static | White |
| `0x2` | ACTIVE | Full brightness static | Defined per module sketch |
| `0x3` | WARNING | Flashing (500 ms on / 500 ms off) | Amber |
| `0x4` | ALERT | Flashing (150 ms on / 150 ms off) | Red |
| `0x5` | ARMED | Full brightness static | Cyan |
| `0x6` | PARTIAL_DEPLOY | Full brightness static | Amber |
| `0x7`–`0xF` | Reserved | — | — |

#### Extended State Notes

States `0x3`–`0x6` are extended states. All LED animation and display behavior is handled entirely by the target module. The controller only transmits the state value — it has no knowledge of flash timing, brightness curves, or color specifics. Modules that do not implement extended states treat values above `0x2` as OFF.

Extended state support is indicated by bit 0 of the capability flags byte in the identity response (see Section 4.3).

#### Flash Timing

Flash timing for WARNING and ALERT states is defined in the module firmware and is not controllable via the I2C protocol.

| State | On Time | Off Time | Character |
|---|---|---|---|
| WARNING | 500 ms | 500 ms | Slow, even flash — draws attention |
| ALERT | 150 ms | 150 ms | Fast, urgent flash — demands immediate action |

#### Nibble Extraction (Controller / Target Reference)

```c
// Extract state for button N from payload array
uint8_t state = (N % 2 == 0)
    ? (payload[N / 2] >> 4) & 0x0F   // high nibble
    : (payload[N / 2]) & 0x0F;        // low nibble

// Pack state for button N into payload array
if (N % 2 == 0)
    payload[N / 2] = (payload[N / 2] & 0x0F) | (state << 4);  // high nibble
else
    payload[N / 2] = (payload[N / 2] & 0xF0) | (state & 0x0F); // low nibble
```

---

### 4.3 Identity Response Packet (Target → Controller)

Transmitted in response to `CMD_GET_IDENTITY`.

**Length:** 4 bytes

```
Byte 0:  Module Type ID       — unique identifier per module type (see Section 8)
Byte 1:  Firmware Version     — major
Byte 2:  Firmware Version     — minor
Byte 3:  Capability Flags     — bitmask (see below)
```

#### Capability Flags

| Bit | Meaning | Applies To |
|---|---|---|
| 0 | Supports extended LED states (`0x3`–`0x6`) | KBC modules |
| 1 | Module fault condition active | KBC modules |
| 2 | Encoder data present in response packet | Non-KBC modules |
| 3–7 | Reserved | — |

#### Type ID vs. I2C Address

The Module Type ID and the module's I2C address are intentionally independent. The address defines **where** a module is on the bus; the Type ID defines **what** it is. This allows the controller to verify at startup that the correct module type is installed at each expected address, and ensures that reassigning a module to a different address does not change its identity.

---

## 5. Command Set

All commands are initiated by the controller. The command byte is the first byte of every write transaction.

| Command | Byte | Payload | Direction | Response |
|---|---|---|---|---|
| `CMD_GET_IDENTITY` | `0x01` | None | C → T | 4-byte identity packet |
| `CMD_SET_LED_STATE` | `0x02` | 8 bytes | C → T | None |
| `CMD_SET_BRIGHTNESS` | `0x03` | 1 byte (0–255) | C → T | None |
| `CMD_BULB_TEST` | `0x04` | None | C → T | None |
| `CMD_SLEEP` | `0x05` | None | C → T | None |
| `CMD_WAKE` | `0x06` | None | C → T | None |
| `CMD_RESET` | `0x07` | None | C → T | None |
| `CMD_ACK_FAULT` | `0x08` | None | C → T | None |

### Command Descriptions

**CMD_GET_IDENTITY (`0x01`)**  
Controller requests module identification. Target responds with the 4-byte identity packet. Used during controller startup to enumerate connected modules, confirm addresses, verify module types, and read capability flags.

**CMD_SET_LED_STATE (`0x02`)**  
Controller transmits the full 16-button LED state as an 8-byte nibble-packed payload. Target updates all LED outputs immediately. This is the primary runtime LED control command.

**CMD_SET_BRIGHTNESS (`0x03`)**  
Sets the brightness level of the ENABLED state dim white backlight. Single byte payload, range 0–255. Allows the controller to match brightness across different physical button types or ambient conditions.

**CMD_BULB_TEST (`0x04`)**  
Triggers a bulb test sequence on the module. All LEDs (NeoPixel and discrete) illuminate at full white for 2000ms, then the module restores its previous LED state and renders. Intended to verify all LEDs are functional at startup or during maintenance.

**CMD_SLEEP (`0x05`)**  
Puts the module into low power mode. LEDs turn off and polling may be reduced. Module remains responsive on I2C.

**CMD_WAKE (`0x06`)**  
Resumes normal operation from sleep. Module restores previous LED state and full polling rate.

**CMD_RESET (`0x07`)**  
Returns all LEDs to OFF state and clears any latched button state. INT line is deasserted. Used for controller startup and error recovery.

**CMD_ACK_FAULT (`0x08`)**  
Clears the module fault flag after the controller has handled the fault condition. The fault flag is readable via the capability flags byte if the controller re-queries identity.

---

## 6. Transaction Examples

### 6.1 Controller Startup Sequence

```
For each address 0x20 – 0x2E:
  1. Controller sends CMD_RESET        [ADDR+W] [0x07]
  2. Controller sends CMD_GET_IDENTITY [ADDR+W] [0x01]
  3. Controller reads identity         [ADDR+R] → [TYPE] [MAJ] [MIN] [FLAGS]
  4. Controller stores TYPE and FLAGS for this address
  5. Controller resolves packet format and read length from TYPE (see Section 9)
  6. Controller sends CMD_SET_LED_STATE with all buttons = OFF
     (payload format may vary by module type — see Section 9)
  7. Controller sends CMD_WAKE         [ADDR+W] [0x06]
```

### 6.2 Button Event (INT-Triggered)

```
1. Target asserts INT low
2. Controller detects INT
3. Controller reads button state using the packet length for this address
   KBC standard:  [ADDR+R] → [STATE_HI] [STATE_LO] [CHG_HI] [CHG_LO]
   Device-specific: length and format determined from Type ID (see Section 9)
4. Target clears INT on read completion
5. Controller interprets packet according to module type
```

### 6.3 LED State Update

```
Controller sends:
  [ADDR+W] [0x02] [B0] [B1] [B2] [B3] [B4] [B5] [B6] [B7]

Where B0–B7 are the 8 nibble-packed payload bytes.
```

---

## 7. Bus Load Analysis

### Per-Transaction Sizes

| Transaction | Bytes on Wire |
|---|---|
| Button state read | 4 bytes |
| LED state update | 9 bytes (cmd + 8 payload) |
| Identity query + response | 6 bytes total |
| Single command (no payload) | 2 bytes |

### Worst-Case Runtime Load (All 15 Modules)

```
15 × button state read   =  60 bytes
15 × LED state update    = 135 bytes
                           ─────────
Total per full sweep     = 195 bytes

@ 400 kHz fast mode      ≈ 4.9 ms
@ 100 kHz standard mode  ≈ 19.5 ms
```

INT-driven button reads mean the average case is significantly lower than worst case — only modules with actual state changes generate read traffic.

---

## 8. Module Type ID Registry

Type IDs are independent of I2C address. The controller maps each bus address to an expected Type ID at compile time and validates this during startup enumeration. The Type ID also determines the packet format and read length used for that address — see Section 9.

KBC module Type ID constants are defined in `KBC_Protocol.h` as `KBC_TYPE_*`. Non-KBC modules define their own Type ID constants independently.

| ID | Module | Type | I2C Address | Cap Flags | Read Bytes |
|---|---|---|---|---|---|
| `0x00` | Reserved | — | — | — | — |
| `0x01` | UI Control | KBC | `0x20` | `0x00` | 4 |
| `0x02` | Function Control | KBC | `0x21` | `0x00` | 4 |
| `0x03` | Action Control | KBC | `0x22` | `0x00` | 4 |
| `0x04` | Stability Control | KBC | `0x23` | `0x00` | 4 |
| `0x05` | Vehicle Control | KBC | `0x24` | `0x01` | 4 |
| `0x06` | Time Control | KBC | `0x25` | `0x00` | 4 |
| `0x07` | EVA Module | Non-KBC | `0x26` | `0x04` | 4 |
| `0xFF` | Unknown / Uninitialized | — | — | — | — |

---

## 9. Device-Specific Packet Formats

Non-KBC modules use packet formats defined here rather than the KBC standard 4-byte format. The controller resolves which format to use from the Module Type ID received during startup enumeration.

### 9.1 EVA Module (Type ID 0x07)

**Read packet — 4 bytes:**

```
Byte 0:  Button state     — bits 5–0 = buttons 0–5 (1=pressed), bits 7–6 unused
Byte 1:  Change mask      — bits 5–0 = buttons 0–5 (1=changed), bits 7–6 unused
Byte 2:  ENC1 delta       — signed int8, relative clicks since last read (+CW, −CCW)
Byte 3:  ENC2 delta       — signed int8, relative clicks since last read (+CW, −CCW)
```

Controller logic: check bits 0–5 of byte 0 against bits 0–5 of byte 1 to identify changed buttons. Read bytes 2 and 3 as signed int8 for encoder deltas — accumulate into position if needed. Encoder deltas are zero until encoders are physically connected and firmware is updated.

**LED state command** — same CMD_SET_LED_STATE (0x02) with 8-byte nibble-packed payload. Only nibbles for buttons 0–5 are meaningful; nibbles for buttons 6–15 are accepted and ignored.

**Reference:** `KCMk1_EVA_Module/Config.h`

---

## 10. Revision History

| Version | Date | Notes |
|---|---|---|
| 1.0 | 2026-04-07 | Initial draft |
| 1.1 | 2026-04-07 | Extended state behaviors defined; updated to 2021 NXP I2C terminology (Controller/Target); Type ID rationale clarified; flash timing defaults added |
| 1.2 | 2026-04-07 | Status released; CMD_BULB_TEST behavior defined; Module Type ID registry fully populated with all six KBC modules and assigned addresses |
| 1.3 | 2026-04-08 | Broadened scope from KBC-only to Kerbal Controller Mk1 system protocol; device-specific packet format architecture introduced; Section 9 added for non-KBC module packets; EVA Module (0x07) added to registry; capability flags table updated; startup sequence updated to reflect device-specific handling |
