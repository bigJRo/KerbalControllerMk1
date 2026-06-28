# Kerbal Controller Mk1 — I2C Protocol Specification

**Version:** 2.5  
**Status:** Released  
**Project:** Kerbal Controller Mk1  
**Organization:** Jeb's Controller Works  
**Author:** J. Rostoker  
**Date:** 2026-06-28

---

## 1. Overview

This document defines the I2C communication protocol between the Kerbal Controller Mk1 system controller and all target modules on the bus. It covers bus configuration, interrupt signalling, the shared command set, lifecycle management, and device-specific data packet formats.

Terminology follows the 2021 NXP I2C specification:

- **Controller** — the main microcontroller that initiates all I2C transactions
- **Target** — a module that responds to controller-initiated transactions
- **Standard module** — a target using the KerbalButtonCore library on KC-01-1802 hardware, with up to 16 button inputs and NeoPixel/discrete LED outputs
- **Display module** — a target using the Kerbal7SegmentCore library on KC-01-1842 hardware, with encoder, 7-segment display, and 3 NeoPixel buttons
- **Device-specific module** — a target with its own standalone firmware (EVA module, joystick modules, throttle module)

The controller owns all game state and situational logic. Target modules own only their local hardware — LED colours, button state, display values. All application logic belongs in the module sketch; libraries are hardware interface layers.

> **Conformance note:** As of v2.5, the **KerbalButtonCore (v2.0)**, **KerbalJoystickCore (v2.0)**, and **Throttle (v2.0)** firmware bodies are fully conformant with this document, alongside the original **Kerbal7SegmentCore** reference implementation — closing issues KBC-001–006, KJC-001–005, and THR-001–004. Every conformant module emits the universal 3-byte header, increments the transaction counter on each INT assertion, and implements the full BOOT_READY → DISABLED → ACTIVE ↔ SLEEPING lifecycle with `CMD_ENABLE`/`CMD_DISABLE`.
>
> The EVA Module (0x07), Dual Encoder (0x0E), and Switch Panel (0x0F) ship **standalone** firmware that does not use these libraries and has **not yet** been updated to v2.x; they remain non-conformant (see Section 8).

---

## 2. Bus Configuration

| Parameter | Value |
|-----------|-------|
| Speed | 100 kHz (standard) or 400 kHz (fast) |
| Voltage | 3.3V |
| Pull-ups | 4.7kΩ to 3.3V on SDA and SCL |
| Address range | 0x20 – 0x2E |
| Max modules | 15 |

Addresses are assigned sequentially from 0x20. Each module sketch hardcodes its assigned address. Type ID and I2C address are independent — the address defines where a module sits on the bus; the Type ID defines what it is.

---

## 3. Interrupt Signalling

Each module has a dedicated active-low INT output connected to a dedicated controller GPIO. The output is **push-pull** — it actively drives both levels: HIGH when idle (released), LOW to assert. No pull-up resistor is fitted at the controller or the module; the push-pull output defines both levels. Each module's INT line is independent — there is no wired-OR bus interrupt.

Level handling depends on the module's logic-voltage domain. **5V modules (ATtiny816)** drive a 5V push-pull output through a passive divider on the module board (10kΩ series + 20kΩ to GND) to produce a 3.3V-compatible INT_BUS signal. **3.3V carriers (Teensy 4.1 display carriers)** drive INT_BUS directly at 3.3V push-pull, with no divider. Signaling polarity (active-low, driven low to assert) is identical across both classes; only the level-conversion hardware differs. See Hardware Reference §13.1.

| Condition | INT State |
|-----------|-----------|
| No unread state changes | High (released) |
| State has changed | Low (asserted) |
| Controller completes read transaction | Cleared by module |
| State changed again during read | Re-asserted immediately |

**BOOT_READY:** At power-on, the module asserts INT with `lifecycle = BOOT_READY` in the status byte. The controller reads the packet and responds with `CMD_DISABLE` to complete initialisation. The module holds INT asserted until the master reads — there is no timeout.

**Last-write-wins:** If the sketch calls `k7scQueuePacket()` multiple times before the master reads, the most recent packet overwrites the previous one. The controller always receives the current state.

The specific conditions that trigger INT assertion vary by module type. Standard modules assert INT on any debounced button state change. Joystick modules apply deadzone, change threshold, and minimum quiet period filtering for axis data; button changes on those modules always assert INT immediately.

### 3.1 Hardware Reset (RST)

In addition to the software `CMD_RESET` command (§6), the system defines an optional hardware reset signal, **RST**, on a dedicated per-module IDC conductor (16-pin module IDC pin 14; 34-pin hub IDC pin 19; 50-pin backbone pin 25). RST is active-low.

RST and `CMD_RESET` are distinct and serve different purposes:

| Mechanism | Scope | Effect |
|-----------|-------|--------|
| `CMD_RESET` (0x07, I2C) | Application state | Module clears local state to defaults, stays ACTIVE. MCU keeps running. |
| RST (hardware line, active-low) | Whole module | Master asserts the line; the module executes a full microcontroller reboot. |

RST is **optional** and module-class-specific:

- **Teensy display carriers (KC-01-1912):** RST is connected. The master asserts RST low to command a full carrier reboot. The carrier firmware treats the RST input as a reset-command signal and performs a full software-initiated system reset on assertion. Receiving-end protection (10kΩ pull-up + 100Ω series + 100nF to GND) is applied per Hardware Reference §8.4 for carriers fed RST over the external interface.
- **ATtiny816 modules:** RST is no-connect (not used). Reset for these modules is via `CMD_RESET` over I2C only.

A module is conformant whether or not it implements RST; the line is available on the IDC for modules that need a master-commanded hardware reboot.

---

## 4. Universal Packet Header

Every data packet from any module starts with a 3-byte universal header. The controller always reads at least these 3 bytes from any module. Module-specific payload follows immediately after.

```
Byte 0:  Status byte
           bits 1:0  Lifecycle state
                       0x00 = ACTIVE      normal operation
                       0x01 = SLEEPING    paused, state frozen
                       0x02 = DISABLED    no game context
                       0x03 = BOOT_READY  awaiting master init
           bit  2    Fault flag (1 = hardware fault present)
           bit  3    Data changed (1 = state changed; always set when packet queued)
           bits 7:4  Reserved (0)
Byte 1:  Module Type ID (see Section 8)
Byte 2:  Transaction counter — uint8, increments on every INT assertion,
         wraps 255→0. Allows controller to detect missed packets.
```

Constants from `KerbalModuleCommon.h`:

| Constant | Value | Description |
|----------|-------|-------------|
| `KMC_STATUS_ACTIVE` | 0x00 | Normal operation |
| `KMC_STATUS_SLEEPING` | 0x01 | Suspended via CMD_SLEEP |
| `KMC_STATUS_DISABLED` | 0x02 | No valid game context |
| `KMC_STATUS_BOOT_READY` | 0x03 | Just powered on, awaiting master init |
| `KMC_STATUS_LIFECYCLE_MASK` | 0x03 | Mask for lifecycle bits |
| `KMC_STATUS_FAULT` | 0x04 | Hardware fault (bit 2) |
| `KMC_STATUS_DATA_CHANGED` | 0x08 | State changed since last read (bit 3) |

---

## 5. Command Set

All commands are initiated by the controller as I2C write transactions. The command byte is the first byte of every write transaction. All module types implement the full command set.

### 5.1 Command Table

| Command | Value | Payload | Description |
|---------|-------|---------|-------------|
| `CMD_GET_IDENTITY` | 0x01 | — | Query module identity. Module replies with 4-byte identity packet (see §6) |
| `CMD_SET_LED_STATE` | 0x02 | 8 bytes | Nibble-packed LED state for 16 positions. Module-specific variants noted in §9 |
| `CMD_SET_BRIGHTNESS` | 0x03 | 1 byte | ENABLED state backlight brightness (0–255) |
| `CMD_BULB_TEST` | 0x04 | 1 byte | 0x01 = start, 0x00 = stop. All LEDs full white. Fires regardless of lifecycle state |
| `CMD_SLEEP` | 0x05 | — | Freeze state exactly as-is. No visual change. INT suppressed |
| `CMD_WAKE` | 0x06 | — | Resume from SLEEPING. Module sends current state packet |
| `CMD_RESET` | 0x07 | — | Reset to defaults, stay ACTIVE. Clears LEDs and pending state |
| `CMD_ACK_FAULT` | 0x08 | — | Acknowledge and clear fault flag |
| `CMD_ENABLE` | 0x09 | — | Enter ACTIVE lifecycle state |
| `CMD_DISABLE` | 0x0A | — | Enter DISABLED lifecycle state |
| `CMD_SET_VALUE` | 0x0D | 2 bytes | Signed int16 big-endian — set display value (display modules only) |

### 5.2 Command Semantics

**CMD_ENABLE / CMD_DISABLE:** Sets lifecycle to ACTIVE or DISABLED respectively. The sketch detects the transition and acts — lighting buttons on enable, resetting state and extinguishing LEDs on disable.

**CMD_SLEEP:** Lifecycle transitions to SLEEPING. State is frozen exactly as-is. No visual change. INT suppressed while sleeping. On CMD_WAKE, the module resumes and sends a state packet.

**CMD_SLEEP vs CMD_DISABLE:** These are distinct states with different semantics. SLEEPING preserves current visual state and resumes it on wake — used for game pause. DISABLED resets to defaults and extinguishes all outputs — used when no flight scene is active or on serial loss.

**CMD_RESET:** Clears all module-specific state to defaults. Module stays ACTIVE. Buttons return to backlit. Display blanks. INT deasserted.

**CMD_BULB_TEST:** Persistent — 0x01 starts, 0x00 stops. No timeout. The master controls duration explicitly. Commandable regardless of lifecycle state.

**CMD_SET_VALUE:** Display modules only. Sets the displayed value (0–9999) and encoder tracking state.

---

## 6. Identity Packet

Sent in response to `CMD_GET_IDENTITY`. Always 4 bytes, regardless of module type.

```
Byte 0:  Module Type ID (see §8)
Byte 1:  Firmware version major
Byte 2:  Firmware version minor
Byte 3:  Capability flags (see §7)
```

---

## 7. Capability Flags

Bitmask in identity packet byte 3. Defined in `KerbalModuleCommon.h` as `KMC_CAP_*`.

| Bit | Constant | Value | Description |
|-----|----------|-------|-------------|
| 0 | `KMC_CAP_EXTENDED_STATES` | 0x01 | Supports WARNING/ALERT/ARMED/PARTIAL_DEPLOY LED states |
| 1 | `KMC_CAP_FAULT` | 0x02 | Active fault condition — clear with CMD_ACK_FAULT |
| 2 | `KMC_CAP_ENCODERS` | 0x04 | Encoder delta present in response packet |
| 3 | `KMC_CAP_JOYSTICK` | 0x08 | Analog joystick axes present in response packet |
| 4 | `KMC_CAP_DISPLAY` | 0x10 | 7-segment display and encoder present |
| 5 | `KMC_CAP_MOTORIZED` | 0x20 | Motorized position control |

---

## 8. Module Registry

Total packet size = 3-byte header + payload size. The controller must read the full total packet size on each INT-triggered read.

| Type ID | Constant | Module | I2C Address | Payload | Total | Cap Flags | Conformant |
|---------|----------|--------|-------------|---------|-------|-----------|------------|
| 0x01 | `KMC_TYPE_UI_CONTROL` | UI Control | 0x20 | 4 bytes | 7 bytes | 0x00 | ✓ KerbalButtonCore v2.0 |
| 0x02 | `KMC_TYPE_FUNCTION_CONTROL` | Function Control | 0x21 | 6 bytes | 9 bytes | 0x00 | ✓ KerbalButtonCore v2.0 (24-input) |
| 0x03 | `KMC_TYPE_ACTION_CONTROL` | Action Control | 0x22 | 4 bytes | 7 bytes | 0x00 | ✓ KerbalButtonCore v2.0 |
| 0x04 | `KMC_TYPE_STABILITY_CONTROL` | Stability Control | 0x23 | 4 bytes | 7 bytes | 0x00 | ✓ KerbalButtonCore v2.0 |
| 0x05 | `KMC_TYPE_VEHICLE_CONTROL` | Vehicle Control | 0x24 | 6 bytes | 9 bytes | 0x01 | ✓ KerbalButtonCore v2.0 (24-input) |
| 0x06 | `KMC_TYPE_TIME_CONTROL` | Time Control | 0x25 | 4 bytes | 7 bytes | 0x00 | ✓ KerbalButtonCore v2.0 |
| 0x07 | `KMC_TYPE_EVA_MODULE` | EVA Module | 0x26 | 4 bytes | 7 bytes | 0x04 | Pending — standalone fw† |
| 0x08 | — | Reserved | 0x27 | — | — | — | — |
| 0x09 | `KMC_TYPE_JOYSTICK_ROTATION` | Joystick Rotation | 0x28 | 9 bytes | 12 bytes | 0x08 | ✓ KerbalJoystickCore v2.0 |
| 0x0A | `KMC_TYPE_JOYSTICK_TRANS` | Joystick Translation | 0x29 | 9 bytes | 12 bytes | 0x08 | ✓ KerbalJoystickCore v2.0 |
| 0x0B | `KMC_TYPE_GPWS_INPUT` | GPWS Input Panel | 0x2A | 5 bytes | 8 bytes | 0x10 | ✓ K7SC reference |
| 0x0C | `KMC_TYPE_PRE_WARP_TIME` | Pre-Warp Time | 0x2B | 5 bytes | 8 bytes | 0x10 | ✓ K7SC reference |
| 0x0D | `KMC_TYPE_THROTTLE` | Throttle Module | 0x2C | 4 bytes | 7 bytes | 0x20 | ✓ Throttle fw v2.0 |
| 0x0E | `KMC_TYPE_DUAL_ENCODER` | Dual Encoder | 0x2D | 4 bytes | 7 bytes | 0x04 | Pending — standalone fw† |
| 0x0F | `KMC_TYPE_SWITCH_PANEL` | Switch Panel | 0x2E | 4 bytes | 7 bytes | 0x00 | Pending — standalone fw† |

> **† Standalone firmware pending conformance:** The EVA Module, Dual Encoder, and Switch Panel ship self-contained firmware (their own `I2C.cpp`) rather than KerbalButtonCore, so the KBC-001–006 library work does not cover them. They still implement the v1.x packet (no universal header, no transaction counter, no lifecycle state machine) and are tracked for a follow-up update.
>
> **Note:** Function Control (0x21) and Vehicle Control (0x24) read 24 inputs (12 NeoPixel buttons + 8 Switch Group inputs at indices 16–23) and therefore use the 6-byte switch-group payload defined in §9.1 (9-byte total packet). All other standard button modules read 16 inputs and use the 4-byte payload (7-byte total).
>
> **Note:** The Indicator Module (previously Type ID 0x10) has been removed from the protocol specification. It is a pure output device requiring no response packet and uses a non-standard 9-byte LED payload. Its behavior is documented in its own module README.

---

## 9. Data Packet Formats

All formats below show the complete packet including the 3-byte universal header.

### 9.1 Standard Button Module (4-byte payload, 7 bytes total)

Modules: UI Control, Function Control, Action Control, Stability Control, Vehicle Control, Time Control, EVA Module, Dual Encoder, Switch Panel.

```
Byte 0:  Status byte       (see §4)
Byte 1:  Module Type ID
Byte 2:  Transaction counter
Byte 3:  Button events HI  — rising edge bitmask (bit0=button0 … bit7=button7)
Byte 4:  Button events LO  — rising edge bitmask (bit0=button8 … bit7=button15)
Byte 5:  Change mask HI    — change bitmask      (bit0=button0 … bit7=button7)
Byte 6:  Change mask LO    — change bitmask      (bit0=button8 … bit7=button15)
```

AND the events bytes with the change mask to identify which buttons changed and what state they changed to.

**Switch Panel note:** Bits 10–15 of the change mask and state are always 0x00 — only 10 switch positions are used.

**EVA Module note:** Only bits 0–5 of the events and change bytes are used; bits 6–15 are always 0x00. Encoder delta bytes (bytes 5–6 in v2.1) are removed — encoder hardware is unpopulated and will be addressed in a future module revision.

#### 9.1.1 Switch-group variant (24 inputs, 6-byte payload)

Function Control (0x21) and Vehicle Control (0x24) add a third shift register (U16) to read eight panel switches as discrete inputs at indices 16–23 (Switch Group 1 and 2 respectively), in addition to their 12 NeoPixel buttons (indices 0–11; indices 12–15 are no-connects). These modules emit a **6-byte payload (9-byte total packet)** — the events and change planes each extend to three bytes:

```
Byte 0:  Status byte       (see §4)
Byte 1:  Module Type ID
Byte 2:  Transaction counter
Byte 3:  Button events  0  — rising-state bitmask (bit0=input0 … bit7=input7)
Byte 4:  Button events  1  — (bit0=input8 … bit7=input15)
Byte 5:  Button events  2  — (bit0=input16 … bit7=input23)   ← switch group
Byte 6:  Change mask    0  — (bit0=input0 … bit7=input7)
Byte 7:  Change mask    1  — (bit0=input8 … bit7=input15)
Byte 8:  Change mask    2  — (bit0=input16 … bit7=input23)    ← switch group
```

The events/change semantics are identical to the 4-byte payload — AND the events plane with the change plane to identify which inputs changed and what they changed to. Switch positions at indices 16–23 are reported as raw input state; their logical meaning and controller actions are defined in the Module UI Reference (Switch Group 1/2 tables). The library selects this variant at compile time via `KBC_INPUT_COUNT = 24` / `KBC_SHIFTREG_COUNT = 3`.

### 9.2 Joystick Modules (9-byte payload, 12 bytes total)

Modules: Joystick Rotation (0x09), Joystick Translation (0x0A).

```
Byte 0:   Status byte       (see §4)
Byte 1:   Module Type ID
Byte 2:   Transaction counter
Byte 3:   Button events     — bit0=BTN_JOY, bit1=BTN01, bit2=BTN02; bits 7–3 unused
Byte 4:   Change mask       — same bit layout as byte 3
Byte 5:   Button state      — persistent state, same bit layout
Byte 6:   AXIS1 HI          — signed int16, big-endian (-32768 to +32767)
Byte 7:   AXIS1 LO
Byte 8:   AXIS2 HI          — signed int16, big-endian
Byte 9:   AXIS2 LO
Byte 10:  AXIS3 HI          — signed int16, big-endian
Byte 11:  AXIS3 LO
```

Axis values are centered at zero. Modules perform startup calibration to establish the physical center reference. Do not touch either joystick during the first ~80ms after power-on.

Axis semantic mapping (which physical direction produces positive or negative values and how each axis maps to a KSP function) is the responsibility of the main controller firmware. See the Module UI Reference.

INT assertion follows a hybrid strategy: button changes assert INT immediately; axis changes assert INT only when outside the deadzone, the change exceeds the threshold, and the minimum quiet period has elapsed since the last axis-driven INT.

Default thresholds:

| Parameter | Default | Description |
|-----------|---------|-------------|
| Deadzone | ±32 counts | Suppresses output within ~3% of center |
| Change threshold | ±8 counts | Suppresses noise on a held position |
| Quiet period | 10ms | Caps axis-driven INT at 100 reads/second max |

### 9.3 Display Modules (5-byte payload, 8 bytes total)

Modules: GPWS Input Panel (0x0B), Pre-Warp Time (0x0C). Both share identical hardware (KC-01-1842) and packet format. **These modules are the reference implementation for this specification.**

```
Byte 0:  Status byte        (see §4)
Byte 1:  Module Type ID
Byte 2:  Transaction counter
Byte 3:  Button events      — bit0=BTN01, bit1=BTN02, bit2=BTN03, bit3=BTN_EN;
                               bits 7–4 unused
Byte 4:  Change mask        — same bit layout as byte 3
Byte 5:  Module state       — bits 1:0 = BTN01 cycle state (0=OFF, 1=ACTIVE, 2=PROX),
                               bit 2 = BTN02 active,
                               bit 3 = BTN03 active;
                               bits 7–4 unused
Byte 6:  Value HI           — display value, big-endian uint16, range 0–9999
Byte 7:  Value LO
```

Button events (byte 3) report rising edges only. Byte 5 carries persistent module state — the controller can read this byte alone to determine current logical state without tracking history.

Button LED states are managed entirely by the module. `CMD_SET_LED_STATE` is accepted and ignored. `CMD_SET_VALUE (0x0D)` sets the display value and encoder tracking state.

INT asserts on any button state change or display value change from the encoder.

**GPWS INT suppression:** When BTN01 is in state 0 (GPWS off), BTN02, BTN03, and encoder events are suppressed. BTN01 always reports since it is the mode switch.

### 9.4 Throttle Module (4-byte payload, 7 bytes total)

```
Byte 0:  Status byte        (see §4)
Byte 1:  Module Type ID     (0x0D)
Byte 2:  Transaction counter
Byte 3:  Status flags       — bit0=enabled, bit1=precision mode,
                               bit2=pilot touching slider, bit3=motor moving;
                               bits 7–4 unused
Byte 4:  Button events      — bit0=THRTL_100, bit1=THRTL_UP,
                               bit2=THRTL_DOWN, bit3=THRTL_00;
                               bits 7–4 unused
Byte 5:  Value HI           — throttle position, big-endian uint16, 0 to INT16_MAX
Byte 6:  Value LO
```

Button events (byte 4) are rising-edge only — cleared after each read. The throttle value represents current slider position regardless of how it was set.

The module starts DISABLED. The controller must send `CMD_ENABLE` to activate motor control and LED illumination. When disabled, the motor drives to 0% and holds, resisting pilot touch.

Module-specific commands:

| Command | Value | Payload | Description |
|---------|-------|---------|-------------|
| `CMD_SET_THROTTLE` | 0x0B | 2 bytes uint16 BE | Command a specific throttle position (0 to INT16_MAX) |
| `CMD_SET_PRECISION` | 0x0C | 1 byte | 0x01 = enter precision mode, 0x00 = exit |

INT asserts on any button press or throttle value change exceeding the minimum change threshold (4 ADC counts).

---

## 10. LED State Command

### 10.1 Standard LED Payload (all modules except display modules)

`CMD_SET_LED_STATE` payload is 8 bytes, nibble-packed, one nibble per button position. Two buttons per byte, high nibble = lower button index:

```
Payload Byte 0:  Button 0  [7:4]  |  Button 1  [3:0]
Payload Byte 1:  Button 2  [7:4]  |  Button 3  [3:0]
...
Payload Byte 7:  Button 14 [7:4]  |  Button 15 [3:0]
```

Total on wire: 9 bytes (command byte + 8 payload bytes).

Display modules (0x0B, 0x0C) accept and ignore `CMD_SET_LED_STATE` — they manage their own LED states internally.

### 10.2 LED State Values

Defined in `KerbalModuleCommon.h` as `KMC_LED_*`.

| Value | Constant | Behavior | Color |
|-------|----------|----------|-------|
| 0x0 | `KMC_LED_OFF` | Unlit | — |
| 0x1 | `KMC_LED_ENABLED` | Dim static | `KMC_BACKLIT` warm white |
| 0x2 | `KMC_LED_ACTIVE` | Full brightness static | Per-button color |
| 0x3 | `KMC_LED_WARNING` | Flashing 500ms on / 500ms off | `KMC_AMBER` |
| 0x4 | `KMC_LED_ALERT` | Flashing 150ms on / 150ms off | `KMC_RED` |
| 0x5 | `KMC_LED_ARMED` | Full brightness static | `KMC_CYAN` |
| 0x6 | `KMC_LED_PARTIAL_DEPLOY` | Full brightness static | `KMC_AMBER` |
| 0x7–0xF | — | Reserved | — |

States 0x3–0x6 are extended states. Modules that do not support extended states treat values above 0x2 as OFF. Extended state support is indicated by `KMC_CAP_EXTENDED_STATES` (bit 0) in the capability flags.

### 10.3 Nibble Pack / Unpack

From `KerbalModuleCommon.h`:

```c
// Get LED state for button N
uint8_t state = kmcLedPackGet(payload, N);

// Set LED state for button N
kmcLedPackSet(payload, N, state);
```

---

## 11. Lifecycle State Machine

```
                    Power on
                       │
                  BOOT_READY ──── master reads ──── CMD_DISABLE ────┐
                                                                     │
           ┌──────────────────────────────────────────────────── DISABLED
           │                                                         │
        CMD_ENABLE ◄─────────────────────────────────────────── CMD_DISABLE
           │
        ACTIVE ◄──── CMD_WAKE ────── SLEEPING ◄──── CMD_SLEEP
           │                                              │
           └──────────── CMD_RESET (stays ACTIVE) ────────┘
```

| State | Description |
|-------|-------------|
| BOOT_READY | Module just powered on. INT asserted. Waits for master to read, then receive CMD_DISABLE |
| DISABLED | No game context. All outputs dark. Inputs suppressed. State reset to defaults |
| ACTIVE | Normal operation. Sketch running. Pilot can interact |
| SLEEPING | State frozen exactly as-is. No visual change. INT suppressed |

Transitions:

- **BOOT_READY → DISABLED:** Master sends CMD_DISABLE after reading BOOT_READY packet
- **DISABLED → ACTIVE:** Master sends CMD_ENABLE when flight scene loads
- **ACTIVE → SLEEPING:** Master sends CMD_SLEEP on game pause
- **SLEEPING → ACTIVE:** Master sends CMD_WAKE on game resume
- **ACTIVE → DISABLED:** Master sends CMD_DISABLE on scene exit, serial loss, or EVA
- **Any → ACTIVE (same):** CMD_RESET clears state but lifecycle stays ACTIVE

---

## 12. Transaction Examples

### 12.1 Startup Sequence

```
For each address 0x20 – 0x2E:
  1. Controller sends CMD_RESET        [ADDR+W] [0x07]
  2. Controller sends CMD_GET_IDENTITY [ADDR+W] [0x01]
  3. Controller reads identity         [ADDR+R] → [TYPE] [MAJ] [MIN] [FLAGS]
  4. Controller stores TYPE and FLAGS for this address
  5. Controller resolves total packet size from TYPE (see §8)
  6. Controller sends CMD_SET_LED_STATE with all positions = OFF
  7. Controller sends CMD_DISABLE      [ADDR+W] [0x0A]
```

### 12.2 Data Read (INT-Triggered)

```
1. Target asserts INT low
2. Controller detects INT on the module's dedicated INT line
3. Controller reads total packet size bytes for this address:
     [ADDR+R] → [STATUS] [TYPE_ID] [TX_COUNTER] [payload bytes...]
4. Target clears INT on read completion
5. Controller checks status byte lifecycle bits and fault flag
6. Controller checks transaction counter for missed packets
7. Controller interprets payload according to module type
```

### 12.3 LED State Update

```
Controller sends:
  [ADDR+W] [0x02] [B0] [B1] [B2] [B3] [B4] [B5] [B6] [B7]

Where B0–B7 are the 8 nibble-packed payload bytes.
Total on wire: 9 bytes.
```

---

## 13. Bus Timing Estimates

At 400 kHz fast mode, approximate per-transaction times:

| Transaction | Bytes | Time |
|-------------|-------|------|
| Read one 7-byte module (standard, 16-input) | 7 bytes | ~0.18ms |
| Read one 9-byte module (switch-group, 24-input) | 9 bytes | ~0.23ms |
| Read one 12-byte module (joystick) | 12 bytes | ~0.30ms |
| Read one 8-byte module (display) | 8 bytes | ~0.20ms |
| Write CMD_SET_LED_STATE | 9 bytes | ~0.23ms |
| Write single command (no payload) | 2 bytes | ~0.05ms |
| Sweep all 15 addresses (worst case) | ~170 bytes | ~4.3ms |

In normal operation average case is substantially lower — only modules with actual state changes generate read traffic.

---

## 14. Vessel Switch Behaviour

Vessel switch behaviour is module-specific and determined by each module's controller sketch. The I2C protocol has no vessel-switch command. The controller sends whatever combination of standard commands is appropriate:

- **GPWS Input Panel** — no action. State persists across vessel switches. Pilot configures as needed.
- **Other modules** — TBD when master controller firmware is implemented.

---

## 15. Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-04-07 | Initial draft — button modules only |
| 1.1 | 2026-04-07 | Extended LED state behaviors defined; NXP I2C 2021 terminology; flash timing defaults |
| 1.2 | 2026-04-07 | Status Released; CMD_BULB_TEST behavior; module registry populated |
| 1.3 | 2026-04-08 | EVA module added; device-specific packet architecture |
| 1.4 | 2026-04-08 | Joystick modules added (0x09, 0x0A); bus load updated |
| 1.5 | 2026-04-08 | CMD_SET_VALUE added; display modules (0x0B, 0x0C) added |
| 1.6 | 2026-04-08 | CMD_ENABLE/DISABLE added; CMD_SET_THROTTLE/PRECISION/VALUE renumbered; throttle module added |
| 1.7 | 2026-04-08 | Dual Encoder module added |
| 1.8 | 2026-04-08 | Switch Panel added |
| 1.9 | 2026-04-08 | Indicator Module added |
| 2.0 | 2026-04-09 | Indicator Module expanded to 18 pixels; payload exception documented; bus load updated |
| 2.1 | 2026-04-09 | Axis semantic mapping removed (belongs in UI Reference); startup calibration note added |
| 2.2 | 2026-05-19 | Universal 3-byte header on all response packets (status byte with lifecycle/fault/data-changed, type ID, transaction counter). Full lifecycle state machine (BOOT_READY/DISABLED/ACTIVE/SLEEPING) defined and documented. SLEEPING vs DISABLED semantics explicitly distinguished. All packet sizes updated (+3 bytes for header). Module registry updated with total packet sizes and conformance status. Indicator Module removed from specification (pure output, non-standard payload — see module README). EVA Module encoder bytes removed (hardware unpopulated). Bus timing updated for new packet sizes. Pending firmware issues identified in conformance column (KBC-001–006, KJC-001–005, THR-001–004). |
| 2.3 | 2026-05-25 | Corrected PCB designators in §1 terminology definitions and §9.3. Standard module hardware reference 1822→1802; display module hardware reference 1881/1882→1842. |
| 2.4 | 2026-06-06 | §3 Interrupt Signalling corrected from "active-low open-drain + controller pull-up" to the actual implementation: active-low push-pull, driven low to assert, no pull-up — aligning the spec with Hardware Reference §13.1 (which adopted push-pull at its revision 1.1). Added 5V (ATtiny816, divider) vs 3.3V (Teensy carrier, direct) level-handling distinction. Added §3.1 Hardware Reset (RST) documenting the optional active-low per-module reset line used by Teensy display carriers (full reboot) and distinct from CMD_RESET (application-state reset). Removed a duplicate stale v2.2 copy of the document that had been appended to the file. |
| 2.5 | 2026-06-28 | Conformance reached for the three pending firmware bodies — KerbalButtonCore v2.0 (KBC-001–006), KerbalJoystickCore v2.0 (KJC-001–005), and Throttle v2.0 (THR-001–004); §1 conformance note and §8 registry updated. Added §9.1.1 switch-group variant: Function Control (0x21) and Vehicle Control (0x24) now read 24 inputs (third shift register U16, Switch Group 1/2 at indices 16–23) and emit a 6-byte payload / 9-byte packet; §8 totals and §13 bus timing updated accordingly. Corrected the joystick registry entry (§8) to 9-byte payload / 12-byte total to match the §9.2 byte layout (button events + change + state bytes were added to the payload but the registry total had not been updated). Clarified that the EVA Module, Dual Encoder, and Switch Panel ship standalone firmware not covered by the library updates and remain pending. |
