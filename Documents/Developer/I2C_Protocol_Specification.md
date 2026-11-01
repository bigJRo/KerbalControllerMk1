# Kerbal Controller Mk1 — I2C Protocol Specification

**Version:** 2.21  
**Status:** Released  
**Project:** Kerbal Controller Mk1  
**Organization:** Jeb's Controller Works  
**Author:** J. Rostoker  
**Date:** 2026-11-01

---

## 1. Overview

This document defines the I2C communication protocol between the Kerbal Controller Mk1 system controller and all target modules on the bus. It covers bus configuration, interrupt signalling, the shared command set, lifecycle management, and device-specific data packet formats.

Terminology follows the 2021 NXP I2C specification:

- **Controller** — the main microcontroller that initiates all I2C transactions
- **Target** — a module that responds to controller-initiated transactions
- **Standard module** — a target using the KerbalButtonCore library on KC-01-1802 hardware, with up to 16 button inputs and NeoPixel/discrete LED outputs
- **Display module** — a target using the Kerbal7SegmentCore library on KC-01-1842 hardware, with encoder, 7-segment display, and 3 NeoPixel buttons
- **Device-specific module** — a target with its own standalone firmware (joystick modules, throttle module, dual encoder)

The controller owns all game state and situational logic. Target modules own only their local hardware — LED colours, button state, display values. All application logic belongs in the module sketch; libraries are hardware interface layers.

> **Conformance note:** As of v2.6, **all module firmware is conformant** with this document. The **KerbalButtonCore (v2.0)**, **KerbalJoystickCore (v2.0)** libraries and the standalone **Throttle (v2.0)** and **Dual Encoder (v2.0)** firmware join the original **Kerbal7SegmentCore** reference implementation — closing issues KBC-001–006, KJC-001–005, and THR-001–004 and the standalone-firmware items. Every module emits the universal 3-byte header, increments the transaction counter on each INT assertion, and implements the full BOOT_READY → DISABLED → ACTIVE ↔ SLEEPING lifecycle with `CMD_ENABLE`/`CMD_DISABLE`.

> **Display carriers (0x10–0x14):** The Teensy 4.1 display carriers (Annunciator 0x10, Resource Display 0x11, Info Display 1 0x12, Info Display 2 0x13, and the future System Info Display 0x14) sit outside the 0x20–0x2E module range and use **panel-specific** point-to-point protocols rather than the universal packet format defined for modules above. Their shared framing (bus, address block, sync bytes, and the requestType command scheme) is defined in **§15**; per-display packet layouts live in each display's `README.md`, and the Info Display's Ascent Autopilot command/status frames are specified in **`Ascent_Autopilot_Interface.md`**.

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
| `CMD_SET_LEVER_HOLD` | 0x0E | 1 byte | Throttle Module only: 0x01 the master holds the lever, 0x00 releases it (§9.4). `CMD_SET_THROTTLE` 0x0B and `CMD_SET_PRECISION` 0x0C are also Throttle-only (§9.4) |

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
| 0x07 | `KMC_TYPE_AUX_CTRL` | Auxiliary Control | 0x26 | 4 bytes | 7 bytes | 0x00 | ✓ KerbalButtonCore v2.1 |
| 0x08 | — | Reserved | 0x27 | — | — | — | — |
| 0x09 | `KMC_TYPE_JOYSTICK_ROTATION` | Joystick Rotation | 0x28 | 9 bytes | 12 bytes | 0x08 | ✓ KerbalJoystickCore v2.0 |
| 0x0A | `KMC_TYPE_JOYSTICK_TRANS` | Joystick Translation | 0x29 | 9 bytes | 12 bytes | 0x08 | ✓ KerbalJoystickCore v2.0 |
| 0x0B | `KMC_TYPE_GPWS_INPUT` | GPWS Input Panel | 0x2A | 5 bytes | 8 bytes | 0x10 | ✓ K7SC reference |
| 0x0C | `KMC_TYPE_PRE_WARP_TIME` | Pre-Warp Time | 0x2B | 5 bytes | 8 bytes | 0x10 | ✓ K7SC reference |
| 0x0D | `KMC_TYPE_THROTTLE` | Throttle Module | 0x2C | 4 bytes | 7 bytes | 0x20 | ✓ Throttle fw v2.0 |
| 0x0E | `KMC_TYPE_DUAL_ENCODER` | Dual Encoder | 0x2D | 4 bytes | 7 bytes | 0x04 | ✓ Dual Encoder fw v2.0 |

> **Note:** Type ID 0x0F / address 0x2E (formerly the Switch Panel) is retired — the single-module design was superseded by Switch Groups 1/2 on Function and Vehicle Control. Both the type ID and the address are now unused.
>
> **Note:** The Dual Encoder ships self-contained firmware (its own `I2C.cpp`) rather than the KerbalButtonCore library, but is equally conformant; it uses the device-specific payload defined in §9.5. (Auxiliary Control at 0x26 — formerly the standalone EVA module — is now a standard KerbalButtonCore button module.)
>
> **Note:** Function Control (0x21) and Vehicle Control (0x24) read 24 inputs (12 NeoPixel buttons + 8 Switch Group inputs at indices 16–23) and therefore use the 6-byte switch-group payload defined in §9.1 (9-byte total packet). All other standard button modules read 16 inputs and use the 4-byte payload (7-byte total).
>
> **Note:** The Indicator Module (previously Type ID 0x10) has been removed from the protocol specification. It is a pure output device requiring no response packet and uses a non-standard 9-byte LED payload. Its behavior is documented in its own module README.

---

## 9. Data Packet Formats

All formats below show the complete packet including the 3-byte universal header.

### 9.1 Standard Button Module (4-byte payload, 7 bytes total)

Modules: UI Control, Function Control, Action Control, Stability Control, Vehicle Control, Time Control, Auxiliary Control. (The Dual Encoder uses a device-specific payload — see §9.5.)

```
Byte 0:  Status byte       (see §4)
Byte 1:  Module Type ID
Byte 2:  Transaction counter
Byte 3:  Button state 0    — current pressed state (bit0=button0 … bit7=button7)
Byte 4:  Button state 1    — current pressed state (bit0=button8 … bit7=button15)
Byte 5:  Change mask 0     — changed since the last read (bit0=button0 … bit7=button7)
Byte 6:  Change mask 1     — changed since the last read (bit0=button8 … bit7=button15)
```

The state plane is the inputs as latched at the read (1 = pressed / switch on), lowest inputs in the first byte. AND it with the change mask to see which inputs changed and what they changed to. (Earlier revisions called bytes 3–4 "events HI / LO, rising edge"; the firmware has always sent the current state, KerbalButtonCore `buildPayload()`.)

#### 9.1.1 Switch-group variant (24 inputs, 6-byte payload)

Function Control (0x21) and Vehicle Control (0x24) add a third shift register (U16) to read eight panel switches as discrete inputs at indices 16–23 (Switch Group 1 and 2 respectively), in addition to their 12 NeoPixel buttons (indices 0–11; indices 12–15 are no-connects). These modules emit a **6-byte payload (9-byte total packet)** — the events and change planes each extend to three bytes:

```
Byte 0:  Status byte       (see §4)
Byte 1:  Module Type ID
Byte 2:  Transaction counter
Byte 3:  Button state  0   — current state (bit0=input0 … bit7=input7)
Byte 4:  Button state  1   — (bit0=input8 … bit7=input15)
Byte 5:  Button state  2   — (bit0=input16 … bit7=input23)   ← switch group
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

The module starts DISABLED. The controller must send `CMD_ENABLE` to activate motor control and LED illumination. When disabled, the motor drives to 0% and holds, resisting pilot touch for at most 500 ms (see below).

Module-specific commands:

| Command | Value | Payload | Description |
|---------|-------|---------|-------------|
| `CMD_SET_THROTTLE` | 0x0B | 2 bytes uint16 BE | Command a specific throttle position (0 to INT16_MAX) |
| `CMD_SET_PRECISION` | 0x0C | 1 byte | 0x01 = enter precision mode, 0x00 = exit |
| `CMD_SET_LEVER_HOLD` | 0x0E | 1 byte | 0x01 = the master owns the lever (an autopilot owns the throttle), 0x00 = the pilot does. Cleared by `CMD_DISABLE` and `CMD_RESET` |

**Touch and the lever hold** (Throttle v2.2):
- **Hold off:** a touch stops the motor at once, and `CMD_SET_THROTTLE` is refused while touched.
- **Hold on:** `CMD_SET_THROTTLE` is accepted while touched and kept as the target. Against a hand the motor pushes back toward the target for at most 500 ms per touch (stall protection), then lets go; when the hand lets go, it drives back to the latest target.
- **DISABLED:** the motor resists a touch toward 0 % for at most 500 ms, then lets go, and drives back to 0 % on release.

INT asserts on any button press or throttle value change exceeding the minimum change threshold (4 ADC counts).

### 9.5 Dual Encoder Module (4-byte payload, 7 bytes total)

Module: Dual Encoder (0x0E). Two quadrature encoders with pushbuttons, no LEDs. Capability flag `KMC_CAP_ENCODERS` (0x04) — encoder deltas are present in the response packet.

```
Byte 0:  Status byte        (see §4)
Byte 1:  Module Type ID     (0x0E)
Byte 2:  Transaction counter
Byte 3:  Button events      — rising edges: bit0=ENC1_SW, bit1=ENC2_SW; bits 7–2 unused
Byte 4:  Change mask        — same bit layout
Byte 5:  ENC1 delta         — signed int8, +CW / −CCW, accumulated since last read
Byte 6:  ENC2 delta         — signed int8, +CW / −CCW, accumulated since last read
```

Encoder deltas accumulate between reads so no detents are lost; they are cleared on read. Button events are rising-edge only. INT asserts on any encoder movement or button press.

---

## 10. LED State Command

### 10.1 Standard LED Payload (all modules except display modules)

`CMD_SET_LED_STATE` payload is 8 bytes, nibble-packed, one nibble per button position. Two buttons per byte, **low nibble = even (lower) button index**, as `kmcLedPackSet()` / `kmcLedPackGet()` pack it and every module library unpacks it:

```
Payload Byte 0:  Button 1  [7:4]  |  Button 0  [3:0]
Payload Byte 1:  Button 3  [7:4]  |  Button 2  [3:0]
...
Payload Byte 7:  Button 15 [7:4]  |  Button 14 [3:0]
```

(Before 2.16 this section showed the nibbles the other way round; the libraries were always as above.) On the joystick modules the LED positions are BTN01 = 0 and BTN02 = 1, not the button-event bit order (§9.2).

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
| 0x7 | `KMC_LED_CUT` | Full brightness static | `KMC_RED` (state-machine terminal — cut / release) |
| 0x8 | `KMC_LED_ACTIVE_ALT` | Full brightness static | Per-button alternate color (e.g. CP Toggle Alternate = `KMC_CORAL`) |
| 0x9–0xF | — | Reserved | — |

States 0x3–0x7 (WARNING, ALERT, ARMED, PARTIAL_DEPLOY, CUT) are **extended states** — they require `KMC_CAP_EXTENDED_STATES` (bit 0). Modules that do not support extended states treat these values as OFF.

State 0x8 (`KMC_LED_ACTIVE_ALT`) is a **core rendering state**, not an extended state: it selects a button's *second* active colour from an optional per-button alternate-colour array (falling back to the primary active colour if none is supplied). It is available to any module regardless of the extended-states capability — used by Auxiliary Control's CP Toggle (ROSE Primary via `ACTIVE`, CORAL Alternate via `ACTIVE_ALT`).

**`CUT` vs `ACTIVE_ALT` (two ways to give a button a second solid colour):** use `CUT` (0x7) when the second state is an **irreversible terminal** (cut / release / jettison) — the colour is a fixed `KMC_RED` that enforces the palette's "RED = irreversible" rule and needs no per-button configuration. Use `ACTIVE_ALT` (0x8) when the second state is an **arbitrary alternate of equal status** (e.g. mode A / mode B) — its colour comes from the sketch's alternate-colour array and may be any colour. Do not imitate `CUT` with `ACTIVE_ALT`+red, and do not use `CUT` for a non-terminal toggle.

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
| DISABLED | No game context. All outputs dark. Input changes do not assert INT. State reset to defaults. A data read still returns the current inputs (button modules keep debouncing while DISABLED); the KCMk1 master polls its switch modules this way |
| ACTIVE | Normal operation. Sketch running. Pilot can interact |
| SLEEPING | State frozen exactly as-is. No visual change. INT suppressed |

Transitions:

- **BOOT_READY → DISABLED:** Master sends CMD_DISABLE after reading BOOT_READY packet
- **DISABLED → ACTIVE:** Master sends CMD_ENABLE when flight scene loads
- **ACTIVE → SLEEPING:** Master sends CMD_SLEEP on game pause
- **SLEEPING → ACTIVE:** Master sends CMD_WAKE on game resume
- **ACTIVE → DISABLED:** Master sends CMD_DISABLE on scene exit or serial loss. Modules stay ACTIVE during EVA (the AUX CTRL EVA buttons need them live; each master panel handler decides what works on EVA)
- **Any → ACTIVE (same):** CMD_RESET clears state but lifecycle stays ACTIVE

---

## 12. Transaction Examples

### 12.1 Startup Sequence

The sequence below is the generic one. The KCMk1 master (`Software/KCMk1_Master`, `module_bus.cpp`) leaves out step 1: a `CMD_RESET` deasserts the boot INT, and the master uses that INT to learn which line each module is on (it reads the boot packet and watches which line goes high). Its sequence is identity → boot-packet read (INT learning) → all LEDs OFF → `CMD_DISABLE`.

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

## 15. Display-Carrier I2C Protocol

The Teensy 4.1 display carriers (Annunciator, Resource Display, Info Display 1/2, and the future System Info Display) are I2C **targets** on the same bus as the modules but occupy a distinct address block (0x10–0x14) and use **panel-specific** point-to-point packets rather than the universal 3-byte-header module packet (§4). They do not implement the module lifecycle state machine, capability flags, or the module command set (§5); instead they share the lightweight framing and command scheme below. INT signalling (§3, active-low push-pull, 3.3V direct) and the optional hardware RST line (§3.1) apply to the carriers exactly as to modules.

### 15.1 Bus and Address Block

| Parameter | Value |
|-----------|-------|
| Bus (carrier side) | `KCM_I2C_BUS` = Wire2 (Teensy 4.1 SCL2=24 / SDA2=25) |
| INT line | `KCM_I2C_INT_PIN` = pin 0, active-low push-pull, one per carrier |
| Hardware RST | `KCM_I2C_RST_PIN` = pin 1 (shared reset from master; see §3.1) |

| I2C Address | Constant | Carrier | Outbound sync byte | Status |
|-------------|----------|---------|--------------------|--------|
| 0x10 | `KCM_I2C_ADDR_ANNUNCIATOR` | Annunciator (C&W) | 0xAC (`KCM_I2C_SYNC_ANNUNCIATOR`) | Implemented |
| 0x11 | `KCM_I2C_ADDR_RESDISP` | Resource Display | 0xAD (`KCM_I2C_SYNC_RESDISP`) | Implemented |
| 0x12 | `KCM_I2C_ADDR_INFODISP` | Info Display 1 | 0xAE (`KCM_I2C_SYNC_INFODISP`) | Implemented |
| 0x13 | `KCM_I2C_ADDR_INFODISP_2` | Info Display 2 | 0xAE (shared with Info 1) | Implemented (same firmware, `INFO_DISP_UNIT`; sole Ascent-AP command source) |
| 0x14 | `KCM_I2C_ADDR_SYSINFODISP` | System Info Display (2" XIAO RP2350 carrier, KC-01-1901) | 0xAF | Protocol defined (§15.4); firmware to come |

Info Display 1 and 2 run the **same** firmware image; the target address is selected at compile time by `INFO_DISP_UNIT` in `KCMk1_InfoDisp.h` (1 → 0x12, 2 → 0x13). Both use sync byte 0xAE. The unit also selects the panel's role — Info Display 1 is the vehicle-type display, Info Display 2 the mission-phase display and the sole owner of the Ascent Autopilot command channel, so an Ascent-AP command frame is only ever non-zero from 0x13. See the InfoDisp README.

### 15.2 Outbound Status Packet (Carrier → Master)

Every carrier's outbound packet begins with **byte 0 = its sync byte** (per the table above) for framing validation; the remaining bytes are panel-specific status. The master reads the packet with `KCM_I2C_BUS.requestFrom(addr, N)` after the carrier asserts INT.

| Carrier | Packet size | Layout reference |
|---------|-------------|------------------|
| Annunciator (0x10) | 6 bytes (flags + all 25 C&W bits across 4 bytes) | `KCMk1_Annunciator/README.md` |
| Resource Display (0x11) | 4 bytes | `KCMk1_ResourceDisp/README.md` |
| Info Display 1/2 (0x12/0x13) | 10 bytes (3 status + 7-byte Ascent-AP command frame) | `KCMk1_InfoDisp/README.md`, `Ascent_Autopilot_Interface.md` |

### 15.3 Inbound Control Byte (Master → Carrier)

Every carrier's inbound command starts with a **controlByte** whose layout is shared across all carriers:

```
controlByte:
  bits 7:4  requestType
              0x0 = NOP            no operation
              0x1 = STATUS         send an immediate status packet (asserts INT)
              0x2 = PROCEED        release the boot hold and enter loop()
              0x3 = MCU_RESET      soft-reboot the carrier MCU
              0x4 = DISPLAY_RESET  reset display state / redraw
  bit  3    idle_state   (1 = switch to standby when not in a flight scene)
  bit  2    audio / trim (Annunciator: audio enable; InfoDisp: trim hold)
  bit  1    demoMode     (1 = enable demo values)
  bit  0    debugMode    (1 = enable Serial debug output)
```

At power-on each carrier holds in `setup()` until the master sends `PROCEED` (`STANDALONE_TEST` in the sketch bypasses this for bench use). Inbound command lengths are panel-specific:

| Carrier | Inbound command | Notes |
|---------|-----------------|-------|
| Annunciator (0x10) | 3 bytes (legacy), **6 bytes (rev-2 extended)**, or **9 bytes (rev-3)** | Rev-2 appends `modeFlags` (2 bytes) + `capValue` (1 byte). Rev-3 further appends **GPWS config** (1 byte) + **GPWS threshold bug** (int16, big-endian, 2 bytes — an altitude decision height, or a target range in rendezvous mode). `onI2CReceive()` accepts any of the three lengths so the master can be upgraded independently. The GPWS config byte reuses the **GPWS Input Panel (0x0B/0x2A) reported state-byte layout** — bits 1:0 = mode (0=OFF, 1=ACTIVE, 2=PROX), bit 2 = proxAlarm, bit 3 = rdvRadar — so the master relays it straight through to the Annunciator's GPWS voice-callout function. See `KCMk1_Annunciator/README.md`. |
| Resource Display (0x11) | 2 bytes | controlByte + reserved |
| Info Display 1/2 (0x12/0x13) | 2 bytes control/ack + status pushes dispatched by **sync byte** (any write ≥ 3 bytes): 40-byte `AscentStatus` (sync **0xA5**), 48-byte aircraft (sync **0xA6**), 36-byte rover (sync **0xA7**), 52-byte orbital (sync **0xA8**), 44-byte landing (sync **0xA9**), 9-byte summary (sync **0xAA**; byte 8 is the master's control group 1–6, for the CAG indicators — a display also accepts the older 8-byte summary as group 1), and on Info Display 1 the input test frames (sync **0xAB**, 40 and 24 bytes; below) | Byte 1 of the 2-byte command carries the console `ackSeq`. See `Ascent_Autopilot_Interface.md`, `Hold_Mode_Autopilot.md` §8 and `Mission_Autopilot.md` §8. The master pushes the full frame only for the console on screen, to Info Display 2; the summary goes to both displays every 100 ms. |

The `requestType` nibble values and the boot `PROCEED` handshake are identical on all three implemented carriers; only the payload width and the panel-specific status/command fields differ.

**BACKLIGHT frame (all four 7" carriers).** A second write, separate from the command above, sets the backlight levels (GitHub #47):

| Byte | Field | Notes |
|------|-------|-------|
| 0 | Sync **0xB8** | `KCM_I2C_SYNC_BACKLIGHT` |
| 1 | Flight level, % | 0–100 |
| 2 | Standby level, % | 0–100; used while the panel shows its standby screen |
| 3 | Reserved | 0 |

The frame is 4 bytes (`KCM_I2C_BACKLIGHT_LEN`), a length none of the carriers uses for its command, so each one can tell it apart; the Info Display also checks the sync byte before its status-push dispatch. A carrier latches the two levels in its receive handler and applies them on the main thread. It clamps each level to `KCM_BL_MIN_PCT` (5 %) … 100 % and ramps the PWM toward the new level (`KCM_BL_RAMP_MS`, 300 ms for the full range) instead of stepping. Until the first frame arrives it uses the `KCMk1_SystemConfig` defaults (`KCM_BL_BRIGHTNESS_PCT` 70 %, `KCM_BL_STANDBY_PCT` 20 %). The standby level applies on the Annunciator's and Resource Display's standby screen, and on an Info Display while it is neither in a flight scene nor in demo. The master sends the frame to all four 7" carriers at once on a change and every 2 s (`CARRIER_BACKLIGHT_MS`), so a carrier that restarts gets it again; its levels start at `BL_FLIGHT_PCT` / `BL_STANDBY_PCT` (checked against the SystemConfig defaults by the master's host build) and the console command `backlight [flight %] [standby %]` changes them until a restart.

**Input test frames (Info Display 1, 0x12).** Info Display 1 has a hidden input test screen (Hardware Inventory §8.11). While it is open the display reports **screen 18** in byte 2 of its status packet (§15.2) instead of the screen underneath. The master reads that byte every 500 ms with the status read, and while it reads 18 it writes three frames to 0x12, all with sync **0xAB**. Multi-byte fields are little-endian. The display accepts them in demo too.

Page 0, the inputs: 40 bytes, every 100 ms.

| Byte | Field |
|------|-------|
| 0 | Sync 0xAB |
| 1 | Page 0 |
| 2–3 | Present: a bit per module, in the master's `ModId` order (UI, FUNCTION, ACTION, STABILITY, VEHICLE, TIME, AUX, ROTATION, TRANSLATION, GPWS, PRE-WARP, THROTTLE, DUAL ENC) |
| 4–5 | FAULT: a bit per module, set when its last packet had the FAULT bit (status byte bit 2, §4) |
| 6–7 | A packet seen: a bit per module |
| 8–23 | Button state words exactly as each module sent them (packet bytes 3 onward, §9.1): UI 2, FUNCTION 3, ACTION 2, STABILITY 2, VEHICLE 3, TIME 2, AUX 2 bytes (FUNCTION / VEHICLE bits 16–23 are the switches); 0 until a packet is seen |
| 24 | ROTATION buttons: bit 0 stick, bit 1 BTN01, bit 2 BTN02 |
| 25–27 | ROTATION AXIS1..3, int8 % |
| 28 | TRANSLATION buttons |
| 29–31 | TRANSLATION AXIS1..3, int8 % |
| 32 | GPWS: low nibble recent presses (BTN01..03, EN), high nibble module state |
| 33 | PRE-WARP: the same |
| 34 | Lever, 0–100 % (0xFF unknown) |
| 35 | Throttle: bit 0 touched, bit 1 precision (FINE), bit 2 ACTIVE; high nibble recent presses (100 %, UP, DOWN, 0 %) |
| 36 | Dual Encoder: low nibble recent presses (ENC1, ENC2), high nibble control group |
| 37 | ENC2 count, int8 (added up by the master since boot) |
| 38 | Control mode (`CTRL_MODE_*`: 0 RVR, 1 PLN, 2 SPC) |
| 39 | Safe lines: bit 0 ABORT, bit 1 STAGE |

Page 1, the modules: 24 bytes, every 500 ms.

| Byte | Field |
|------|-------|
| 0 | Sync 0xAB |
| 1 | Page 1 |
| 2–14 | INT line of each module, `ModId` order: 0–11 INT_1..12, 12–23 B_INT_1..12, 0xFF none |
| 15–16 | GPWS display value, uint16 little-endian |
| 17–18 | Pre-Warp display value, uint16 little-endian |
| 19–22 | Lifecycle, 2 bits per module as last reported (module *m* at bits 2*m*+1:2*m*, `ModId` order): 0 ACTIVE, 1 SLEEPING, 2 DISABLED, 3 BOOT_READY (`KMC_STATUS_*`) |
| 23 | 0 |

Page 2, the four 7" displays: 24 bytes, every 500 ms, sent after page 1. The master takes these from the status packets it reads every 500 ms (§15.2).

| Byte | Field |
|------|-------|
| 0 | Sync 0xAB |
| 1 | Page 2 |
| 2 | Answering: bit 0 Annunciator, bit 1 Info Display 1, bit 2 Info Display 2, bit 3 Resource Display |
| 3 | Annunciator flags: bit 0 KSP link, bit 1 flight scene, bit 2 master alarm |
| 4–7 | Annunciator C&W bits, little-endian (25 used) |
| 8 | Info Display 1 flags: bit 0 KSP link, bit 1 flight scene, bit 2 demo |
| 9 | Info Display 1 screen (status byte 2) |
| 10 | Info Display 2 flags (as byte 8) |
| 11 | Info Display 2 screen |
| 12 | Resource Display flags: bit 0 KSP link, bit 1 flight scene, bit 2 demo, bit 3 caution, bit 4 alarm, bit 5 time tier |
| 13 | Resource Display bar count |
| 14 | Resource Display worst resource (`ResourceType`) |
| 15–23 | 0 |

These bytes are copied from each display's status packet (bytes 1 onward). A display that stops answering keeps its last good status here; byte 2 says which ones are current.

"Recent" means a rising edge in a module packet read within the last 300 ms (`INPUT_TEST_RECENT_MS`). A frame of the wrong length for its page is dropped. If no page 0 arrives for 1.5 s the screen shows NO DATA. When the screen closes the status byte reports the real screen again and the frames stop. The master's layout is in `KCMk1_Master/input_test.h`.

**Master side.** The new master (`Software/KCMk1_Master`, `carriers.cpp`) sends all four carriers their command every 100 ms and at once on a change, with `requestType = PROCEED` on every write, and reads each carrier's status packet every 500 ms (bit 0 of every carrier's flags byte is its own Simpit link, shown on the System Info Display). A carrier treats PROCEED as "leave the boot hold" and nothing else, so repeating it is harmless, and it releases a carrier that powers up after the master or reboots on its own. The Annunciator's GPWS bytes are the GPWS Input Panel's state byte and display value, read by the master when the panel asserts INT; the master answers the panel's BOOT_READY / DISABLED report with `CMD_DISABLE` / `CMD_ENABLE` (§5).

### 15.4 System Info Display (0x14)

The System Info Display is the 2" carrier on Panel A2 (KC-01-1901: XIAO RP2350, ER-TFTM020-2 240×320 ST7789 in portrait, microSD); firmware `Software/Displays/KCMk1_SysInfo` (v1.0.0), design `Sys_Info_Display_Design.md`. It shows the master's boot progress, then the controller status, and SWITCH ERR (`Master_Firmware_Design.md` §14). Unlike the 7" carriers it has **no boot hold** (no PROCEED handshake) and no RST connection, and it does not use its INT line in this protocol (it holds it high). It is `Wire1` on the RP2350 (I2C1, GPIO 6 / 7). Every master write is one frame; byte 0 is the frame id. Multi-byte fields are **little-endian**.

**Master → System Info Display:**

| Id | Frame | Bytes | Layout |
|---|---|---|---|
| 0xB0 | CONTROL | 2 | `[B0, ctrl]`. ctrl bit 0 debug, bit 1 demo, bit 2 **flight scene** (the master's SCENE_CHANGE state; from v2.21); bits 7:3 reserved (0). Sent every 500 ms: the heartbeat |
| 0xB1 | BOOT_LINE | 4 + len | `[B1, seq, attr, len, text…]`. `seq` increments per line; `len` ≤ 40, ASCII. `attr` bits 1:0 severity (0 info, 1 OK, 2 warning, 3 error); bit 6 **REPLAY** — the first line of a replay: clear the boot log first (from v2.21); bit 7 REPLACE — overwrite the previous line instead of adding one (the live `KSP WAIT n S` line). From master v0.10.0 the text is each line's **short form**, written to fit two rows of the display's Roboto Black 32; Serial keeps the long form (`Sys_Info_Display_Design.md` §8.2) |
| 0xB2 | BOOT_DONE | 1 | `[B2]`: leave the boot screen for the status screen |
| 0xB3 | STATUS | 27 (13) | See below. Sent every 500 ms after BOOT_DONE. A display accepts the 13-byte form of v2.15–v2.20 and shows the module and service items as unknown |
| 0xB4 | SWITCH_ERR | 2 + 3n | `[B4, n, {id, expected, actual} × n]`, n ≤ 10; n = 0 closes the window. Switch ids: 1–8 = Switch Group 1 B16–B23, 9–16 = Switch Group 2 B16–B23. `expected` = the switch's position, `actual` = the vessel's state (1 = the switch's second position, e.g. GEAR DOWN) |
| 0xB8 | BACKLIGHT | 4 | `[B8, flight %, standby %, 0]`, the frame the 7" carriers take (§15.3), sent with them every 2 s and at once on a change (from v2.21). Levels clamped to `KCM_BL_MIN_PCT`, 300 ms ramp. The display uses the flight level in a flight scene and on any alert, the standby level on its status screen otherwise |

**STATUS (0xB3), 27 bytes:**

| Byte | Field | Notes |
|---|---|---|
| 0 | 0xB3 | |
| 1 | valid | bit 0 power, bit 1 temperature, bit 2 fan |
| 2–3 | bus voltage, u16 | 0.01 V |
| 4–5 | current, i16 | mA |
| 6–7 | power, u16 | 0.1 W |
| 8 | board temperature, i8 | °C |
| 9–10 | fan, u16 | rpm |
| 11 | links | a bit per board with its own Simpit link: bit 0 master (up and not SIMPIT LOST), 1 Annunciator, 2 Info Display 1, 3 Info Display 2, 4 Resource Display |
| 12 | known | the same bits: answered its last status read (bit 0 always set) |
| 13 | fan duty, % | from v2.21: bytes 13–26 |
| 14 | module count | 13 (`MOD_COUNT`) |
| 15–16 | present, u16 | bit n = module n answering, in the master's order: UI, FUNCTION, ACTION, STABILITY, VEHICLE, TIME, AUX, ROTATION, TRANSLATION, GPWS, PRE-WARP, THROTTLE, DUAL ENC |
| 17–18 | FAULT, u16 | status byte bit 2 in the module's last packet |
| 19–20 | I2C errors, u16 | NACKs, short reads and other bus errors since the master started, saturating |
| 21–24 | master uptime, u32 | seconds |
| 25–26 | master version | major, minor |

**System Info Display → master** (4-byte read): `[0xAF, flags, fwMajor, fwMinor]`; flags bit 0 ready, bit 1 microSD card mounted (a card is optional: clear is not an error), bit 2 boot screen showing.

**Behaviour:**
- **Splash** at power-on for at least 2 s and until the first BOOT_LINE, never more than 5 s (built in, or `/KCMK1/SPLASH.BMP` from the card). Frames received meanwhile are kept.
- **Boot screen** from then until BOOT_DONE: the newest line, the counts of OK / warning / error lines on show, and the newest problem. A STATUS frame also ends the boot screen (a display that missed BOOT_DONE).
- **No frame for 5 s,** on the boot or the status screen: `MASTER / SILENT n S`, with the last values kept in grey (the status screen part from v2.21).
- **Late start:** the master keeps the boot lines it sent before the display answered and replays them when it does, the first with REPLAY set and none with REPLACE, so a display that starts late still shows the boot from the top and never counts a line twice.

---

## 16. Revision History

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
| 2.6 | 2026-06-28 | Remaining standalone firmware brought to conformance: EVA Module (v2.0), Dual Encoder (v2.0), and Switch Panel (v2.0) now emit the universal 3-byte header, transaction counter, and full lifecycle state machine. §1 conformance note updated to "all module firmware conformant". §8 registry: EVA/Dual Encoder/Switch Panel marked ✓; EVA capability flags corrected 0x04→0x00 (encoder headers unpopulated, no delta bytes sent). Added §9.5 defining the Dual Encoder device-specific packet (button events + change + two int8 encoder deltas) and removed Dual Encoder from the §9.1 standard-module list. |
| 2.5 | 2026-06-28 | Conformance reached for the three pending firmware bodies — KerbalButtonCore v2.0 (KBC-001–006), KerbalJoystickCore v2.0 (KJC-001–005), and Throttle v2.0 (THR-001–004); §1 conformance note and §8 registry updated. Added §9.1.1 switch-group variant: Function Control (0x21) and Vehicle Control (0x24) now read 24 inputs (third shift register U16, Switch Group 1/2 at indices 16–23) and emit a 6-byte payload / 9-byte packet; §8 totals and §13 bus timing updated accordingly. Corrected the joystick registry entry (§8) to 9-byte payload / 12-byte total to match the §9.2 byte layout (button events + change + state bytes were added to the payload but the registry total had not been updated). Clarified that the EVA Module, Dual Encoder, and Switch Panel ship standalone firmware not covered by the library updates and remain pending. |
| 2.7 | 2026-06-28 | Switch Panel module (type 0x0F / address 0x2E) removed from the codebase — design superseded by Switch Groups 1/2 on Function and Vehicle Control. §8 registry row and the §9.1 standard-module references dropped; 0x0F/0x2E retired. |
| 2.8 | 2026-06-28 | EVA module (type 0x07 / address 0x26) replaced by the **Auxiliary Control** module: the standalone 6-button EVA firmware was retired in favour of a standard 12-button KerbalButtonCore module (KC-01-1802). Type ID 0x07 renamed `KMC_TYPE_EVA_MODULE` → `KMC_TYPE_AUX_CTRL` (value unchanged); §8 registry row updated (now KerbalButtonCore v2.1, standard 4-byte payload / 7-byte packet); Auxiliary Control added to the §9.1 standard-module list and removed from the device-specific list; the obsolete EVA-only packet note (bits 0–5, encoder bytes) dropped. |
| 2.9 | 2026-06-28 | §9 LED state table updated: added `KMC_LED_CUT` (0x7, static red state-machine terminal — previously defined in firmware but missing from this table) and the new `KMC_LED_ACTIVE_ALT` (0x8, second per-button active colour). Clarified gating: 0x3–0x7 are extended states (require `KMC_CAP_EXTENDED_STATES`); 0x8 `ACTIVE_ALT` is a core rendering state available regardless of capability — used by AUX CTRL's CP Toggle (ROSE Primary / CORAL Alternate). Backed by KerbalModuleCommon v1.6 / KerbalButtonCore v2.2. |
| 2.21 | 2026-11-01 | §15.4: the **Sys Info Display** firmware is built (`KCMk1_SysInfo` v1.0.0, master v0.10.0). CONTROL gains bit 2 (flight scene); BOOT_LINE gains attr bit 6 REPLAY and carries each line's short form; STATUS grows 13 → 27 bytes (fan duty, module count, present and FAULT masks, I2C errors, uptime, master version; the 13-byte form still accepted); the BACKLIGHT frame (0xB8) now goes to 0x14 too; the 5 s silence rule covers the status screen; read flag bit 1 = card mounted. Full STATUS byte table added. No module-side packet formats changed. |
| 2.20 | 2026-10-31 | §15.3: **BACKLIGHT frame** `[0xB8, flight %, standby %, 0]` to all four 7" carriers (GitHub #47; KCMk1_SystemConfig 1.3.0, KCM_Display 1.1.0, master v0.9.0), and the Info Display 1 **input test frames** (sync 0xAB; page 0, 40 bytes every 100 ms: presence, FAULT, packet-seen masks and the button modules' raw state words; page 1, 24 bytes every 500 ms: INT lines, display-module values and 2-bit lifecycles; page 2, 24 bytes every 500 ms: the four 7" displays' status) sent while the display reports screen 18 in status byte 2 (InfoDisp v1.18.0). No module-side packet formats changed. |
| 2.19 | 2026-10-31 | §9.4: `CMD_SET_LEVER_HOLD` (0x0E) for the Throttle Module, and the touch rules with and without it (500 ms stall limit). §5.2's start / stop bulb test is now what every module does (KerbalButtonCore 2.3, KerbalJoystickCore 2.1, Throttle 2.2). |
| 2.18 | 2026-10-27 | §15.3: the Info Display summary (0xAA) grows to 9 bytes; byte 8 carries the master's control group 1–6 so the AIRBRK indicator follows it (InfoDisp v1.17, master v0.8.1). An 8-byte summary still reads as group 1. |
| 2.17 | 2026-10-26 | §11: DISABLED reads return the current inputs (only the INT is suppressed), which the KCMk1 master relies on to read its switch modules in any scene. §15.4: SWITCH_ERR `expected` / `actual` defined. No packet formats changed. |
| 2.16 | 2026-10-26 | §10.1: LED nibble order corrected to match `kmcLedPackSet()` and every module library (low nibble = even button index); joystick LED positions noted. §11: modules stay ACTIVE on EVA. §12.1: the KCMk1 master's start-up sequence (no CMD_RESET, so the boot INT can be used to learn the module's INT line). No packet formats changed. |
| 2.15 | 2026-10-25 | §15.4: **System Info Display (0x14) protocol** defined (sync 0xAF): CONTROL heartbeat, BOOT_LINE (with REPLACE), BOOT_DONE, STATUS, SWITCH_ERR frames, and its 4-byte status read. §15.3: master-side note for the new `KCMk1_Master`. No module-side packet formats changed. |
| 2.14 | 2026-10-25 | §15.3: master side documented. Controller_Main now sends the Annunciator (0x10) the rev-3 command and the Resource Display (0x11) its control byte, both repeating `PROCEED` (until now neither carrier was ever written to, so both held in `setup()`), and relays the GPWS Input Panel (0x2A) to the Annunciator. No packet formats changed. |
| 2.13 | 2026-10-24 | §15.3: Info Display (0x12/0x13) gains an 8-byte autopilot **summary** push (sync **0xAA**) written to both displays every 100 ms: each console's flag byte plus the landing mode and ENTRY, so a display can see an engaged autopilot off its console (#83). Layout in `Mission_Autopilot.md` §8.1. No module-side packet formats changed. |
| 2.12 | 2026-09-05 | §15.3: Info Display (0x12/0x13) inbound status pushes are now dispatched by **sync byte** rather than write length. The aircraft frame (0xA6) grows 44 → 48 bytes and the rover frame (0xA7) 28 → 36; two new frames, **0xA8** orbital (52 bytes) and **0xA9** landing (44 bytes), carry the mission autopilot consoles. Opcodes `0x14`–`0x16`, `0x38`–`0x3A`, `0x40`–`0x5E` added to the console command frame. Layouts in `Mission_Autopilot.md` §8. No module-side packet formats changed. |
| 2.11 | 2026-08-15 | §15.3: Annunciator (0x10) inbound command gains a **rev-3 9-byte form** appending a **GPWS config byte** (mode/proxAlarm/rdvRadar, reusing the GPWS Input Panel 0x0B/0x2A reported state-byte layout) + an **int16 altitude threshold** (big-endian metres). The master relays the GPWS Input Panel's configuration to the Annunciator, whose new on-board GPWS function (`GPWS.ino`) speaks ground-proximity voice callouts on the DFPlayer. Backward compatible: `onI2CReceive()` accepts the 3/6/9-byte forms. No module-side packet formats changed. |
| 2.10 | 2026-08-08 | Branch reconciliation. This module-firmware line (2.6–2.9: EVA/Dual Encoder/Switch Panel conformance, Switch Panel retirement, EVA → Auxiliary Control, LED-state additions) and a parallel documentation line — which had independently reused version numbers 2.6–2.9 — are merged into a single specification. Grafted in from the parallel line: the new **§15 Display-Carrier I2C Protocol** (Teensy 4.1 carriers 0x10–0x14; per-carrier sync bytes 0xAC/0xAD/0xAE + 0xA5 AscentStatus; the shared `requestType` control-byte scheme NOP/STATUS/PROCEED/MCU_RESET/DISPLAY_RESET; the boot PROCEED handshake; the Annunciator 3/6-byte extended command) and the §1 **Display carriers (0x10–0x14)** note (correcting the carrier block to 0x10–0x14 and pointing per-display layouts to each display's README and `Ascent_Autopilot_Interface.md`). The former §15 (Revision History) is renumbered §16. No module-side packet formats changed. |
