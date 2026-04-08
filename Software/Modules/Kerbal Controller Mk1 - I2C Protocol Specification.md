# Kerbal Controller Mk1 — I2C Protocol Specification

**Version:** 1.4  
**Status:** Released  
**Project:** Kerbal Controller Mk1  

---

## 1. Overview

This document defines the I2C communication protocol between the Kerbal Controller Mk1 system controller and all target modules on the bus. It covers bus configuration, the interrupt signaling mechanism, the shared command set, the standard button module data format, and device-specific data formats for modules with different input types.

Terminology follows the 2021 NXP I2C specification:
- **Controller** — the main microcontroller that initiates all I2C transactions
- **Target** — a module that responds to controller-initiated transactions
- **Standard module** — a target using the KerbalButtonCore library on KC-01-1822 hardware, with up to 16 button inputs and NeoPixel/discrete LED outputs
- **Device-specific module** — a target with different hardware and its own standalone firmware (e.g. EVA module, joystick modules)

All modules on the bus — regardless of type — share the same command set and identity response format. Data response formats and lengths are module-specific and determined by the controller at startup from the identity response.

The controller owns all game state and situational logic. Target modules own only LED display behavior for their assigned states.

---

## 2. Bus Configuration

| Parameter | Value |
|---|---|
| Protocol | I2C |
| Recommended Speed | 400 kHz (Fast Mode) |
| Maximum Devices | 15 |
| Address Range | `0x20` – `0x2E` |
| Address Assignment | Hardcoded per module sketch |

Addresses are assigned sequentially from `0x20`. Each module sketch hardcodes its assigned address. Reserved addresses are documented in the Module Type ID Registry (Section 8).

---

## 3. Interrupt Line

Each module has a dedicated active-low interrupt output (INT) connected to the controller. The INT line signals that the module has new data ready to be read.

| Condition | INT State |
|---|---|
| No unread state changes | High (released) |
| State has changed | Low (asserted) |
| Controller completes read transaction | Cleared |
| State changed again during read | Re-asserted immediately |

The specific conditions that trigger INT assertion vary by module type. Standard modules assert INT on any debounced button state change. Device-specific modules may apply additional filtering — for example, joystick modules use a hybrid strategy that applies deadzone, change threshold, and minimum quiet period filtering before asserting INT for axis data, while button changes on those modules are always immediate.

### 3.1 Dual-Buffer Latching

To guarantee that every state change edge is reported, modules implement a dual-buffer strategy:

- **Live buffer** — updated continuously as inputs change
- **Latched buffer** — snapshot taken at the moment INT is asserted

When the controller reads, it receives the latched buffer. On completion of the read transaction:
1. INT clears
2. The live buffer is snapshotted into the latch
3. If the new latch differs from what was just sent, INT re-asserts immediately

This guarantees at least one INT + read cycle per state change edge. Rapid changes between reads may coalesce into a single report, but no edge is silently dropped.

---

## 4. Standard Module Data Format

### 4.1 Button State Response (Target → Controller)

Transmitted in response to a controller read request following INT assertion. This format applies to all standard modules (Type IDs 0x01–0x06). Device-specific modules use their own formats — see Section 9.

**Length:** 4 bytes

```
Byte 0:  Current state HIGH   — bits 15–8  (1 = pressed, 0 = released)
Byte 1:  Current state LOW    — bits 7–0
Byte 2:  Change mask HIGH     — bits 15–8  (1 = changed since last read)
Byte 3:  Change mask LOW      — bits 7–0
```

Bit N corresponds to button index N. Bit 0 is the lowest-indexed button; bit 15 is the highest.

**Controller logic:** AND the current state with the change mask to identify which buttons changed and what state they changed to.

---

### 4.2 LED State Command (Controller → Target)

Transmitted when the controller updates LED states on a module.

**Command byte:** `0x02`  
**Payload length:** 8 bytes  
**Total on wire:** 9 bytes (address + command + 8 payload bytes)

Each button is assigned one nibble (4 bits). Two buttons are packed per byte, high nibble first:

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

The 8-byte payload is used for all modules that accept LED commands. Device-specific modules with fewer LED positions accept the full payload and ignore nibbles for positions that are not installed.

#### LED State Values

| Value | State | Behavior | Color |
|---|---|---|---|
| `0x0` | OFF | Unlit | — |
| `0x1` | ENABLED | Dim static | White |
| `0x2` | ACTIVE | Full brightness static | Defined per module |
| `0x3` | WARNING | Flashing (500 ms on / 500 ms off) | Amber |
| `0x4` | ALERT | Flashing (150 ms on / 150 ms off) | Red |
| `0x5` | ARMED | Full brightness static | Cyan |
| `0x6` | PARTIAL_DEPLOY | Full brightness static | Amber |
| `0x7`–`0xF` | Reserved | — | — |

States `0x3`–`0x6` are extended states. All LED animation and timing behavior is handled entirely by the target module. The controller transmits only the state value. Modules that do not support extended states treat values above `0x2` as OFF. Extended state support is indicated by bit 0 of the capability flags in the identity response.

#### Flash Timing

| State | On Time | Off Time | Character |
|---|---|---|---|
| WARNING | 500 ms | 500 ms | Slow, even flash — draws attention |
| ALERT | 150 ms | 150 ms | Fast, urgent flash — demands immediate action |

#### Nibble Extraction Reference

```c
// Extract state for button N from payload array
uint8_t state = (N % 2 == 0)
    ? (payload[N / 2] >> 4) & 0x0F   // high nibble
    : (payload[N / 2]) & 0x0F;        // low nibble

// Pack state for button N into payload array
if (N % 2 == 0)
    payload[N / 2] = (payload[N / 2] & 0x0F) | (state << 4);
else
    payload[N / 2] = (payload[N / 2] & 0xF0) | (state & 0x0F);
```

---

### 4.3 Identity Response (Target → Controller)

Transmitted in response to `CMD_GET_IDENTITY`. Identical format for all module types.

**Length:** 4 bytes

```
Byte 0:  Module Type ID    — unique identifier per module type (see Section 8)
Byte 1:  Firmware Major    — firmware version major number
Byte 2:  Firmware Minor    — firmware version minor number
Byte 3:  Capability Flags  — bitmask (see below)
```

#### Capability Flags

| Bit | Meaning | Notes |
|---|---|---|
| 0 | Extended LED states supported (`0x3`–`0x6`) | Standard modules only |
| 1 | Module fault condition active | Standard modules only |
| 2 | Encoder data present in response packet | Device-specific modules |
| 3 | Analog joystick axes present in response packet | Device-specific modules |
| 4–7 | Reserved | — |

#### Type ID vs. I2C Address

The Module Type ID and the I2C address are intentionally independent. The address defines where a module sits on the bus; the Type ID defines what it is. This allows the controller to verify that the correct module type is present at each address during startup, and means that reassigning an address does not change a module's identity.

---

## 5. Command Set

All commands are initiated by the controller. The command byte is the first byte of every write transaction. All module types implement the full command set.

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
Controller requests module identification. The target responds with the 4-byte identity packet. Used during startup to enumerate connected modules, confirm addresses, verify module types, and read capability flags.

**CMD_SET_LED_STATE (`0x02`)**  
Controller transmits the LED state as an 8-byte nibble-packed payload. The target updates all LED outputs. Modules with fewer than 16 LED positions accept the full payload and ignore unused nibbles. This is the primary runtime LED control command.

**CMD_SET_BRIGHTNESS (`0x03`)**  
Sets the brightness level of the ENABLED state dim white backlight. Single byte payload, range 0–255. Allows the controller to match brightness across modules or adapt to ambient conditions.

**CMD_BULB_TEST (`0x04`)**  
Triggers a bulb test sequence. All LEDs illuminate at full white for 2000ms, then the module restores its previous LED state. Used to verify LED hardware at startup or during maintenance.

**CMD_SLEEP (`0x05`)**  
Puts the module into low power mode. LEDs turn off and input polling rate may be reduced. The module remains responsive on I2C.

**CMD_WAKE (`0x06`)**  
Resumes normal operation from sleep. The module restores LEDs to their previous state and returns to full polling rate.

**CMD_RESET (`0x07`)**  
Clears all LED and input state. INT is deasserted. Used for controller startup and error recovery.

**CMD_ACK_FAULT (`0x08`)**  
Acknowledges a module fault condition. Modules that implement fault tracking clear their fault flag on receipt. Modules that do not track faults accept and ignore this command.

---

## 6. Transaction Examples

### 6.1 Controller Startup Sequence

```
For each address 0x20 – 0x2E:
  1. Controller sends CMD_RESET        [ADDR+W] [0x07]
  2. Controller sends CMD_GET_IDENTITY [ADDR+W] [0x01]
  3. Controller reads identity         [ADDR+R] → [TYPE] [MAJ] [MIN] [FLAGS]
  4. Controller stores TYPE and FLAGS for this address
  5. Controller resolves data packet format and read length from TYPE (see Section 9)
  6. Controller sends CMD_SET_LED_STATE with all positions = OFF
  7. Controller sends CMD_WAKE         [ADDR+W] [0x06]
```

### 6.2 Data Read (INT-Triggered)

```
1. Target asserts INT low
2. Controller detects INT on the module's INT line
3. Controller reads data using the packet length for this address:
     Standard module:  [ADDR+R] → [STATE_HI] [STATE_LO] [CHG_HI] [CHG_LO]
     Device-specific:  length and format determined from Type ID (see Section 9)
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
| Standard module data read | 4 bytes |
| Joystick module data read | 8 bytes |
| EVA module data read | 4 bytes |
| LED state update | 9 bytes (cmd + 8 payload) |
| Identity query + response | 6 bytes total |
| Single command (no payload) | 2 bytes |

### Worst-Case Runtime Load

Bus load depends on which modules are active and how frequently they assert INT. Standard modules are completely quiet when no buttons are pressed. Joystick modules apply deadzone and change threshold filtering to minimize traffic when at rest or held steady.

```
Worst case — all 9 current modules simultaneously:
  6 × standard module reads    = 24 bytes
  1 × EVA module read          =  4 bytes
  2 × joystick module reads    = 16 bytes
  9 × LED state updates        = 81 bytes
                                ─────────
  Total per full sweep         = 125 bytes

@ 400 kHz fast mode            ≈ 3.1 ms
```

In normal operation the average case is substantially lower — only modules with actual state changes generate read traffic.

---

## 8. Module Type ID Registry

Type IDs are independent of I2C address. The controller uses the Type ID from the identity response to determine the data packet format and read length for each address.

| ID | Module | I2C Address | Cap Flags | Read Bytes |
|---|---|---|---|---|
| `0x00` | Reserved | — | — | — |
| `0x01` | UI Control | `0x20` | `0x00` | 4 |
| `0x02` | Function Control | `0x21` | `0x00` | 4 |
| `0x03` | Action Control | `0x22` | `0x00` | 4 |
| `0x04` | Stability Control | `0x23` | `0x00` | 4 |
| `0x05` | Vehicle Control | `0x24` | `0x01` | 4 |
| `0x06` | Time Control | `0x25` | `0x00` | 4 |
| `0x07` | EVA Module | `0x26` | `0x04` | 4 |
| `0x08` | Reserved | `0x27` | — | — |
| `0x09` | Joystick Rotation | `0x28` | `0x08` | 8 |
| `0x0A` | Joystick Translation | `0x29` | `0x08` | 8 |
| `0xFF` | Unknown / Uninitialized | — | — | — |

**Capability flag values:**  
`0x01` = Extended LED states (bit 0)  
`0x04` = Encoder data in packet (bit 2)  
`0x08` = Analog joystick axes in packet (bit 3)

---

## 9. Device-Specific Packet Formats

Modules with Type IDs `0x07` and above use packet formats defined in this section. The controller resolves the correct format from the Type ID received during startup enumeration.

### 9.1 EVA Module (Type ID 0x07) — 4 bytes

```
Byte 0:  Button state   — bits 5–0 = buttons 0–5 (1=pressed), bits 7–6 unused
Byte 1:  Change mask    — bits 5–0 = buttons 0–5 (1=changed), bits 7–6 unused
Byte 2:  ENC1 delta     — signed int8, relative clicks since last read (+CW, −CCW)
Byte 3:  ENC2 delta     — signed int8, relative clicks since last read (+CW, −CCW)
```

AND byte 0 with byte 1 to identify changed buttons. Encoder deltas are currently always zero — encoders are present on the PCB but not yet connected or implemented.

**LED command:** CMD_SET_LED_STATE with standard 8-byte payload. Nibbles for buttons 0–5 are active; nibbles for positions 6–15 are accepted and ignored.

**Reference:** `KCMk1_EVA_Module/Config.h`

---

### 9.2 Joystick Modules (Type IDs 0x09, 0x0A) — 8 bytes

Applies to both the Joystick Rotation module (0x09, address 0x28) and the Joystick Translation module (0x0A, address 0x29). Both share identical hardware (KC-01-1831/1832) and packet format.

```
Byte 0:   Button state  — bit0=BTN_JOY, bit1=BTN01, bit2=BTN02; bits 7–3 unused
Byte 1:   Change mask   — same bit layout as byte 0
Byte 2-3: AXIS1         — signed int16, big-endian, -32768 to +32767
Byte 4-5: AXIS2         — signed int16, big-endian, -32768 to +32767
Byte 6-7: AXIS3         — signed int16, big-endian, -32768 to +32767
```

Axis values are centered at zero. Each module performs startup calibration to establish the physical center reference for each axis. A split map is applied to ensure a symmetric INT16 output range regardless of the mechanical center position of the joystick.

INT assertion follows a hybrid filtering strategy:
- Button changes assert INT immediately with no throttling
- Axis changes assert INT only when outside the deadzone, the change exceeds the threshold, and the minimum quiet period has elapsed since the last axis-driven INT

Default thresholds (all in raw ADC counts):

| Parameter | Default | Description |
|---|---|---|
| Deadzone | ±32 counts | Suppresses output within ~3% of center |
| Change threshold | ±8 counts | Suppresses noise on a held position |
| Quiet period | 10 ms | Caps axis-driven INT at 100 reads/second maximum |

**LED command:** CMD_SET_LED_STATE with standard 8-byte payload. Only nibbles for BTN01 (index 0) and BTN02 (index 1) are active. BTN_JOY has no LED hardware. Remaining nibbles are accepted and ignored.

**Reference:** `KerbalJoystickCore/src/KJC_Config.h`

---

## 10. Revision History

| Version | Date | Notes |
|---|---|---|
| 1.0 | 2026-04-07 | Initial draft |
| 1.1 | 2026-04-07 | Extended LED state behaviors defined; updated to 2021 NXP I2C terminology; Type ID rationale clarified; flash timing defaults added |
| 1.2 | 2026-04-07 | Status set to Released; CMD_BULB_TEST behavior defined; Module Type ID registry fully populated with six standard modules |
| 1.3 | 2026-04-08 | Scope broadened to full Kerbal Controller Mk1 system protocol; device-specific packet architecture introduced; EVA Module added to registry; Section 9 added |
| 1.4 | 2026-04-08 | Full language revision to remove standard-module-centric framing throughout; Section 4 restructured as Standard Module Data Format; INT section updated to cover device-specific strategies; Section 7 bus load updated for current module set; joystick modules (0x09, 0x0A) and reserved entries (0x08) added to registry; Section 9.2 joystick packet format added; capability flags table expanded |
