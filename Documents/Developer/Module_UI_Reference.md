# Kerbal Controller Mk1 — Expansion Module Specification

**Organization:** Jeb's Controller Works  
**Author:** J. Rostoker  
**Version:** 0.3 (Draft)  
**Date:** 2026-05-23  
**Document type:** Developer — Hardware Design Specification

---

## 1. Purpose and Scope

This document specifies the architecture, connectivity, power, and board topology for the KCMk1 Expansion Module — a self-contained autopilot input and display panel. It connects to the main KCMk1 controller via a GX16-10 signal connector and a USB-C panel mount connector, drawing 12V power, I2C, interrupt, and reset signals through the GX16-10, and USB 2.0 data through the USB-C connection.

This document covers the expansion unit only. The main controller hardware is unchanged; expansion connectivity is already allocated in the existing design (SDA_EXT/SCL_EXT, EXT_INT_1/EXT_INT_2, RST, V_12 on the 50-pin IDC connector, and the USBC2 expansion USB-C port). Signal protection and conditioning architecture for the external interface is defined in Hardware Reference §8.4 and §9.4.

---

## 2. Functional Overview

The expansion module provides:

- **DSKY-inspired 25-key keyboard** (Kailh Choc V1 switches, SK6812MINI-EA per-key RGB backlight) for autopilot verb/noun command entry
- **Rotary encoder** for value entry and menu navigation
- **2× 5" 800×480 TFT displays** — one data/program display, one graphical display
- **Capacitive touch** on each display for secondary/configuration input
- **Internal USB hub** (CH334F) distributing the single expansion USB-C connection to internal USB devices
- **Internal I2C bus** on which the expansion Teensy 4.1 acts as master, commanding two RP2040 display processors

The expansion Teensy 4.1 is the sole USB device visible to the host PC from this unit. It handles all KerbalSimpit communication, orbital math, keyboard matrix scanning, NeoPixel control, and autopilot program logic. The two RP2040s are pure display processors — they receive telemetry and state data from the Teensy over the local I2C bus and handle all display rendering independently.

The expansion Teensy 4.1 acts as **I2C master on SDA_EXT**, initiating transactions to the main controller when autopilot commands need to be sent. The main controller Teensy 4.1 acts as I2C target on SDA_EXT. EXT_INT_1 is asserted by the expansion unit to signal the main controller that a transaction is pending.

---

## 3. External Connections

The expansion unit presents two connectors on its enclosure panel:

### 3.1 GX16-10 Signal Connector

A GX16-10 (10-pin, 16mm thread) aviation connector carries 12V power, I2C, interrupt, and reset signals. Both enclosure panels mount a **female socket with male pins** (standard panel-mount convention). The interconnecting cable has **female sockets at both ends**.

| Pin | Signal | Direction | Notes |
|-----|--------|-----------|-------|
| 1 | V_12 | Main → Expansion | 12V power |
| 2 | V_12 | Main → Expansion | Parallel — combined capacity for full expansion load |
| 3 | GND | — | Power return |
| 4 | GND | — | Power return parallel |
| 5 | SDA_EXT | Bidirectional | I2C data — LTC4311 + pull-ups on Ctrl Module only; passive at expansion end |
| 6 | SCL_EXT | Bidirectional | I2C clock — LTC4311 + pull-ups on Ctrl Module only; passive at expansion end |
| 7 | EXT_INT_1 | Expansion → Main | Active-low — 47Ω series at expansion end; 4.7kΩ pull-up at Ctrl Module |
| 8 | EXT_INT_2 | Main → Expansion | Active-low — 47Ω series at Ctrl Module; 4.7kΩ pull-up at expansion end |
| 9 | RST | Main → Expansion | Active-low — 47Ω series at Ctrl Module; 10kΩ pull-up + 100Ω + 100nF RC filter at expansion end |
| 10 | SPARE | — | Reserved for future use |

All signal protection components are on the respective PCBs, not in the cable. See Hardware Reference §8.4 for full signal architecture.

### 3.2 USB-C Panel Mount Connector

A USB-C panel mount bulkhead feedthrough connector carries USB 2.0 High Speed data from Hub 2 Port 4 on the main controller to the expansion unit's internal CH334F hub. A standard USB-C cable (3ft / 1m maximum) connects the two panel mounts.

VBUS is not connected at either panel mount — the expansion unit is powered entirely via V_12 on the GX16-10. A TUSB212RWBR USB 2.0 High Speed signal conditioner is fitted on the expansion routing board between the panel USB-C and the internal hub to compensate for the total cable run of approximately 6–7ft across both enclosures. See Hardware Reference §8.4.

---

## 4. Internal Architecture

### 4.1 Block Diagram

```
GX16-10 Connector
  ├── V_12 (×2) ──────────── Expansion Routing Board
  │                              ├── MPM3610 → 5V (hub logic, Teensy VIN)
  │                              └── AP2112K → 3.3V (TUSB212, logic)
  ├── SDA_EXT / SCL_EXT ─── (passive — LTC4311 + pull-ups on Ctrl Module) ── Teensy 4.1 I2C target port
  ├── EXT_INT_1 ──────────── 47Ω series ── Teensy 4.1 GPIO output
  ├── EXT_INT_2 ──────────── 4.7kΩ pull-up ── Teensy 4.1 GPIO input
  └── RST ─────────────────── 10kΩ pull-up → 100Ω → Teensy 4.1 RST pin (100nF to GND at RST)

USB-C Panel Mount
  └── USB 2.0 ── TUSB212 ── CH334F Hub
                                 └── Port 1 → Teensy 4.1 (sole USB/Simpit device)

Teensy 4.1
  ├── I2C master (local bus, 3.3V)
  │     ├── RP2040 A (0x10) → SSD1963 + 5" TFT + cap touch → Left display (graphical)
  │     └── RP2040 B (0x11) → SSD1963 + 5" TFT + cap touch → Right display (data/program)
  ├── I2C target (SDA_EXT) ← main controller
  ├── EXT_INT_1 output → GX16 pin 7 (asserted when expansion has data for main controller)
  ├── EXT_INT_2 input ← GX16 pin 8 (asserted by main controller to push state updates)
  ├── GPIO matrix (4 rows × 7 cols = 28 positions, 25 populated)
  ├── Encoder A/B/SW (3 GPIO)
  └── NeoPixel data out → SK6812MINI-EA chain (25 LEDs)

Keyboard Matrix PCB (KC-01-EX21)
  └── Connected to Teensy carrier via 2×10P headers (top and bottom)

Encoder PCB (KC-01-EX22)
  └── Connected to Teensy carrier via dedicated header
```

### 4.2 I2C Bus Topology

The expansion unit uses two separate I2C buses:

| Bus | Master | Targets | Purpose |
|-----|--------|---------|---------|
| SDA_EXT / SCL_EXT | Expansion Teensy 4.1 | Main controller Teensy 4.1 | Autopilot commands to main controller |
| Local I2C (internal) | Expansion Teensy 4.1 | RP2040 A (0x10), RP2040 B (0x11) | Telemetry and state data to display processors |

The expansion Teensy 4.1 is master on both buses simultaneously, using two separate I2C peripheral ports.

---

## 5. Board Topology

### 5.1 Boards Required

| Board | Designator | Description |
|-------|------------|-------------|
| Expansion Routing Board | KC-01-EX10 | GX16-10 and USB-C panel connectors; TUSB212 USB conditioner; 47Ω series on EXT_INT_1 output; 4.7kΩ pull-up on EXT_INT_2 input; 10kΩ pull-up + 100Ω + 100nF RC filter on RST input; CH334F internal hub; MPM3610 + AP2112K power regulation; terminal blocks for GX16 and USB-C internal wiring. No LTC4311 — I2C bus management on Ctrl Module side only |
| Expansion Teensy Carrier | KC-01-EX23 | Teensy 4.1 via headers; local I2C master to RP2040s; SDA_EXT I2C target port; EXT_INT_1 output (47Ω series); EXT_INT_2 input; RST input (100Ω + 100nF RC filter); 2×10P headers mating to keyboard matrix PCB; encoder header; NeoPixel data output |
| Expansion Keyboard Matrix | KC-01-EX21 | 25× Kailh Choc V1 switches (hand solder); 25× SK6812MINI-EA RGB LEDs; 25× 1N4148W matrix diodes; 25× 100nF decoupling caps; 21mm key grid; 147×100mm board; 2×10P HCTL PM254-2-10-Z-8.5 headers top and bottom |
| Expansion Encoder PCB | KC-01-EX22 | PEC11R-4220F-S0024 rotary encoder (C143797); independently height-adjustable behind panel |
| Expansion Display Carrier A | KC-01-EX30 | RP2040; SSD1963 parallel LCD driver; 5" 800×480 TFT panel (TBD); capacitive touch controller; I2C target on local bus (address 0x10) — Left display, graphical role |
| Expansion Display Carrier B | KC-01-EX31 | RP2040; SSD1963 parallel LCD driver; 5" 800×480 TFT panel (TBD); capacitive touch controller; I2C target on local bus (address 0x11) — Right display, data/program role |

### 5.2 Power Architecture

All power enters via V_12 on the GX16-10 connector and is regulated on the Expansion Routing Board:

| Regulator | Type | Input | Output | Consumers |
|-----------|------|-------|--------|-----------|
| MPM3610 | DC-DC | 12V | 5V | CH334F V5, Teensy 4.1 VIN, RP2040 boards VIN |
| AP2112K | LDO | 5V | 3.3V | TUSB212, LTC4311, logic level signals |

Each display carrier board (KC-01-EX30/31) carries its own local MPM3610 for 5V and AP2112K for 3.3V, consistent with the module power architecture pattern. The RP2040 and SSD1963 draw from these local regulators.

**Estimated 12V current draw:**

| Consumer | 12V typical |
|----------|------------|
| Teensy 4.1 + keyboard NeoPixels (25× SK6812) | ~0.5A |
| Display Carrier A (RP2040 + SSD1963 + TFT) | ~0.4A |
| Display Carrier B (RP2040 + SSD1963 + TFT) | ~0.4A |
| CH334F hub + routing board logic | ~0.1A |
| **Total typical** | **~1.4A @ 12V** |

GX16-10 pins 1 and 2 in parallel provide up to 10A combined — substantial headroom.

> **Open item E-02:** Confirm 5V current budget and regulator topology once SSD1963 + TFT panel power draw is measured or datasheet-verified.

---

## 6. USB Architecture

### 6.1 Hub Configuration

The expansion unit's internal CH334F hub distributes the single USB-C panel connection:

| Parameter | Value |
|-----------|-------|
| Hub IC | CH334F (WCH) |
| LCSC # | C5187527 |
| Upstream | USB-C panel mount (via TUSB212 signal conditioner) |
| Downstream ports used | 1 (Teensy 4.1 only) |
| Downstream ports spare | 3 |
| Power mode | Self-powered (PSELF = HIGH) |
| Crystal | 12MHz SMD3225-4P (C9002) |

Only the Teensy 4.1 is a USB device in this unit. The RP2040 display processors communicate with the Teensy over local I2C — they are not USB devices and do not connect to the hub.

> **Open item E-09:** With only one downstream USB device, the CH334F hub adds latency and a hub tier. Evaluate whether connecting the Teensy 4.1 directly to the USB-C panel mount (bypassing the hub) is preferable, using the TUSB212 inline on the direct connection. This simplifies the board but removes the 3 spare downstream ports.

### 6.2 VBUS Isolation

Consistent with the main controller design, VBUS on the upstream USB-C panel mount is left unconnected. The CH334F V5 pin is powered from the internal 5V rail. The Teensy 4.1 downstream Micro-USB VCC pin is left unconnected — the Teensy is powered from the 5V rail via its VIN pin.

### 6.3 USB Signal Conditioner

A TUSB212RWBR (LCSC C702372) is fitted on the expansion routing board between the USB-C panel mount and the CH334F hub. It compensates for the ~6–7ft total conductor path (internal main controller wiring + 3ft external cable + internal expansion wiring) that would otherwise degrade USB 2.0 High Speed signal integrity. Boost level set per TUSB212 datasheet Table 1 for the actual cable length.

---

## 7. Keyboard Subsystem

### 7.1 Physical Layout

The keyboard replicates the Apollo DSKY keyboard layout with an enhanced function row added at the top. All keys use **Kailh Choc V1** switches (tactile brown or equivalent) with **MBK shine-through keycaps** at a **21mm × 21mm** grid spacing.

```
Col:   1       2      3      4      5      6      7
     [PROG]  [HOLD] [MENU] [INFO] [MARK] [PAGE] [ENC↕]  ← Enhanced row (row 0)
     [VERB]  [ + ]  [ 7 ]  [ 8 ]  [ 9 ]  [CLR] [ENTR]  ← DSKY rows
     [NOUN]  [ - ]  [ 4 ]  [ 5 ]  [ 6 ]  [PRO] [RSET]
             [ 0 ]  [ 1 ]  [ 2 ]  [ 3 ]  [KYRL]
```

**Key count:** 25 switches + 1 rotary encoder. The encoder occupies col 7 row 0 — no switch at that position. Row 3 col 1 and row 3 col 7 are absent (consistent with original DSKY layout asymmetry).

**Key function summary:**

| Key | Function |
|-----|----------|
| VERB | Enter verb (operation) code — two digit numeric entry |
| NOUN | Enter noun (data object) code — two digit numeric entry |
| PROG | Direct program select — enter program number without verb ceremony |
| 0–9, +, − | Numeric and sign entry |
| CLR | Clear current entry |
| PRO | Proceed — confirm and execute current verb/noun |
| ENTR | Enter — confirm numeric value entry |
| KEY REL | Key release — dismiss current prompt |
| RSET | Reset — clear fault state |
| HOLD | Pause running program, preserve state |
| MENU | Open touch interface menu on right display |
| INFO | Display extended parameters behind current program view |
| MARK | Capture current craft state (orbital snapshot, maneuver reference) |
| PAGE | Cycle graphical display view for current program |
| ENC (encoder) | Turn: scroll/adjust highlighted value. Click: confirm selection |

### 7.2 Matrix Wiring

The 25 switches are wired in a 4-row × 7-column matrix with one diode per key (1N4148W, SOD-123). Diode orientation: anode to switch, cathode to row line. The Teensy 4.1 drives columns as outputs (active low) and reads rows as inputs (internal pull-up, reads low on keypress).

| Parameter | Value |
|-----------|-------|
| Row lines | 4 (ROW_0–ROW_3) |
| Column lines | 7 (COL_0–COL_6) |
| Teensy GPIO for matrix | 11 (4 + 7) |
| Teensy GPIO for encoder | 3 (A, B, SW) |
| Teensy GPIO for NeoPixel | 1 |

### 7.3 Per-Key RGB Backlight

One SK6812MINI-EA (LCSC C5378731) RGB LED per key position, reverse-mount on the keyboard matrix PCB back, shining through a PCB cutout into the Choc V1 switch housing. LEDs are daisy-chained DOUT→DIN from LED_0 (PROG) left-to-right top-to-bottom. LED_DIN enters from the Teensy carrier via J1; LED_DOUT exits to J2 for potential future chaining.

One 100nF decoupling capacitor (C1525) per LED, placed as close as possible to VDD.

### 7.4 Keyboard PCB (KC-01-EX21) Specification

| Parameter | Value |
|-----------|-------|
| Board dimensions | 147mm × 100mm |
| Key grid | 21mm × 21mm |
| Switch footprint | Kailh Choc V1, direct solder (hand solder post-JLCPCB) |
| LED placement | SK6812MINI-EA reverse mount on PCB back, PCB cutout per key |
| Connectors | J1 (top): HCTL PM254-2-10-Z-8.5 (C2897411); J2 (bottom): same |
| Matrix | 4 rows × 7 columns, 25 populated positions |

**J1 (top header) pin allocation:**

| Pins | Signal |
|------|--------|
| 1–2 | COL_0, COL_1 |
| 3–4 | COL_2, COL_3 |
| 5–6 | COL_4, COL_5 |
| 7–8 | COL_6, LED_DIN |
| 9–10 | ROW_0, ROW_1 |
| 11–12 | 5V, GND |
| 13–20 | NC, GND (distributed) |

**J2 (bottom header) pin allocation:**

| Pins | Signal |
|------|--------|
| 1–2 | ROW_2, ROW_3 |
| 3–4 | LED_DOUT, 5V |
| 5–8 | GND (×4) |
| 9–16 | NC (×8) |
| 17–20 | GND (×4) |

**Keyboard PCB BOM:**

| Ref | Component | LCSC # | Qty | Assembly |
|-----|-----------|--------|-----|----------|
| SW1–SW25 | Kailh Choc V1 tactile | Self-source | 25 | Hand solder |
| D1–D25 | 1N4148W SOD-123 | C81598 | 25 | SMT (JLCPCB) |
| LED1–LED25 | SK6812MINI-EA | C5378731 | 25 | SMT (JLCPCB) |
| C1–C25 | 100nF 16V X7R 0402 | C1525 | 25 | SMT (JLCPCB) |
| J1, J2 | HCTL PM254-2-10-Z-8.5 | C2897411 | 2 | Hand solder |

### 7.5 Encoder PCB (KC-01-EX22) Specification

| Parameter | Value |
|-----------|-------|
| Encoder | PEC11R-4220F-S0024 (Bourns) |
| LCSC # | C143797 |
| Shaft | 20mm flatted, 2mm bushing |
| Detents | 24 per revolution |
| Push switch | Yes |
| Panel cover | 3mm — shaft protrudes appropriately above panel |
| Mounting | Independently height-adjustable from keyboard matrix PCB |

---

## 8. Display Subsystem

### 8.1 Display Roles

| Display | Position | Role |
|---------|----------|------|
| Left display | KC-01-EX30 | Graphical — orbit diagram, flight director, trajectory, maneuver node visualization. View cycles via PAGE key |
| Right display | KC-01-EX31 | Data/Program — PROG/VERB/NOUN indicators, live program parameters, KER-style numeric readouts. Content driven by running program |

### 8.2 DSKY-Faithful Mode

The right display can render a faithful DSKY-style layout: PROG, VERB, NOUN indicator boxes at top; three 5-digit data registers (R1, R2, R3) below; caution/status lamp area. This mode is activated when the running program specifies it. The left display shows a complementary graphical view in all modes.

### 8.3 Touch Interface

Capacitive touch on each display serves as a secondary input layer for:
- Configuration and setup functions (brightness, display layout)
- Context-sensitive menus opened by the MENU key
- Soft buttons for enhanced functions that don't warrant a dedicated hard key

Touch is not used for primary flight operations — all time-critical inputs use the physical keyboard.

### 8.4 Display Hardware

| Parameter | Value |
|-----------|-------|
| Display driver IC | SSD1963 |
| Interface | 16-bit parallel |
| Display MCU | RP2040 |
| Resolution | 800×480 |
| Panel size | 5" (TBD part number) |
| Touch | Capacitive (controller TBD) |
| Communication to Teensy | I2C (local bus) — high-level data/state packets, not pixel data |

The RP2040's PIO state machines drive the SSD1963 parallel bus in hardware, freeing the CPU cores for graphics rendering and I2C communication. Dual-core operation: core 0 handles I2C communication with the Teensy; core 1 handles display rendering.

> **Open item E-05:** Confirm 5" TFT panel part number and SSD1963 compatibility. Confirm capacitive touch controller IC.

> **Open item E-06:** Define I2C packet format between Teensy 4.1 and RP2040s — what data is sent, at what rate, and how display content is specified.

---

## 9. I2C Address Allocation

All expansion unit I2C devices operate on SDA_EXT (external bus to main controller) or the local internal I2C bus.

| Address | Device | Bus | Notes |
|---------|--------|-----|-------|
| 0x10 | Display Carrier A (RP2040) | Local (internal) | Left graphical display |
| 0x11 | Display Carrier B (RP2040) | Local (internal) | Right data/program display |
| TBD | Main controller Teensy 4.1 (target) | SDA_EXT | Address assigned when main controller firmware is defined |

The expansion keypad is not an I2C device — it is a direct GPIO matrix on the Teensy 4.1. No keypad I2C addresses are needed.

> **Open item E-07:** Confirm I2C address for main controller target on SDA_EXT and update Hardware Reference §8.3 accordingly.

---

## 10. Open Items

| # | Item | Type |
|---|------|------|
| E-02 | Confirm 5V current budget — verify SSD1963 + TFT panel power draw | Power |
| E-05 | Confirm 5" TFT panel part number; confirm SSD1963 compatibility; confirm capacitive touch controller IC | Hardware |
| E-06 | Define I2C packet format between Teensy 4.1 and RP2040 display processors | Firmware |
| E-07 | Confirm main controller I2C target address on SDA_EXT | Address allocation |
| E-09 | Evaluate bypassing internal CH334F hub — connect Teensy 4.1 directly to USB-C panel mount via TUSB212 | Architecture |

---

## 11. Not in Scope

- Changes to the main controller hardware or firmware
- Changes to the 50-pin IDC connector or expansion USB-C port on the main controller (already defined in Hardware Reference §9 and §7.7)
- Panel C (the planned button/display growth panel — separate from this expansion unit)
- KerbalSimpit plugin changes — additional Teensy clients require no plugin modifications
- Autopilot program logic and DSKY verb/noun vocabulary (firmware design document TBD)

---

## 12. Revision History

| Version | Date | Description |
|---------|------|-------------|
| 0.1 | 2026-05-23 | Initial draft — architecture, connectivity, board topology, open items |
| 0.2 | 2026-05-23 | Major revision. Architecture changed: display processors changed from Teensy 4.0 to RP2040; display driver changed from RA8875 to SSD1963; keyboard changed from ATtiny816 shift-register modules to direct Teensy 4.1 GPIO matrix; keypad reduced from 28 to 25 keys + 1 encoder; external interface changed from 50-pin IDC to GX16-10 + USB-C panel mount; expansion Teensy 4.1 established as I2C master on SDA_EXT; TUSB212 USB signal conditioner added; LTC4311 on SDA_EXT/SCL_EXT both ends; series resistors and RST RC filter documented. Board designators assigned (KC-01-EX10 through EX31). Keyboard PCB BOM finalized. Display roles confirmed (left = graphical, right = data/program). DSKY keyboard layout finalized with enhanced function row. Open items E-01, E-03, E-04, E-08 closed. |
| 0.3 | 2026-05-23 | Signal architecture corrections. LTC4311 relocated from both ends to Ctrl Module only — single LTC4311 on SDA_EXT/SCL_EXT is sufficient; placed on main controller side per architecture principle. I2C passive at expansion end — no LTC4311 and no pull-ups on Expansion Routing Board. Physical pull-up resistors added throughout: 4.7kΩ on EXT_INT_1 at Ctrl Module; 4.7kΩ on EXT_INT_2 at Expansion Routing Board; 10kΩ on RST at Expansion Routing Board. Full signal path documented per signal including 47Ω source resistors and pull-up placement. Block diagram, GX16 pin table, and board topology table updated to reflect corrections. |
# Kerbal Controller Mk1 — Module UI Reference

**Jeb's Controller Works** | v5.1 | 2026-05-19

This document is the definitive developer reference for all user-facing controls and indicators on each KCMk1 module. Arranged by I2C address. For panel location context see the address summary table at the end of this document.

---

## Required Mods

| Mod | Purpose |
|-----|---------|
| **AGX (Action Groups Extended)** | Enables custom action groups beyond AG1–AG10. Required for all CAG assignments. The controller uses AGX groups 1–238 across 6 control groups. |
| **Hullcam VDS Continued** | Adds mountable external cameras. Required for Dual Encoder ENC2 camera cycling. Default keybindings: `-` (next), `=` (previous), `Backspace` (reset). |

---

## LED State Color Key

All NeoPixel buttons share a common set of LED states. Not all states are used on every module.

| State | Appearance | Meaning |
|-------|-----------|---------|
| OFF | Dark | Inactive / not installed |
| ENABLED | Dim white (`KMC_BACKLIT`) | Module active, function available |
| ACTIVE | Full color (per button) | Function engaged or selected |
| WARNING | Flashing amber 500ms on/off | Attention required |
| ALERT | Flashing red 150ms on/off | Immediate action required |
| ARMED | Static cyan | Primed and ready |
| PARTIAL_DEPLOY | Static amber | Partially deployed state |

Discrete LED buttons are ON/OFF only — ENABLED and ACTIVE both produce full brightness. Flash states have no effect on discrete LEDs.

---

## Custom AG Assignment Table

All custom action groups use AGX numbers. ENC1 on the Dual Encoder module selects the active control group.

**Offset formula:** AGX number = Base CAG + ((Group − 1) × 40)

### Control Groups

| Group | Name | Usage |
|-------|------|-------|
| 1 | Default Group | Default for main craft |
| 2 | Lander Group #1 | Primary or single lander |
| 3 | Lander Group #2 | Secondary lander |
| 4 | Custom Group | User-defined |
| 5 | Re-Entry Vehicle Group | Re-entry craft |
| 6 | Space Station Group | Space station control set |

### Assignment Table

| Base | Function | Module | Button | Grp1 | Grp2 | Grp3 | Grp4 | Grp5 | Grp6 |
|------|----------|--------|--------|------|------|------|------|------|------|
| 1 | AG1 | Action Control | B10 | 1 | 41 | 81 | 121 | 161 | 201 |
| 2 | AG2 | Action Control | B8 | 2 | 42 | 82 | 122 | 162 | 202 |
| 3 | AG3 | Action Control | B6 | 3 | 43 | 83 | 123 | 163 | 203 |
| 4 | AG4 | Action Control | B4 | 4 | 44 | 84 | 124 | 164 | 204 |
| 5 | AG5 | Action Control | B2 | 5 | 45 | 85 | 125 | 165 | 205 |
| 6 | AG6 | Action Control | B0 | 6 | 46 | 86 | 126 | 166 | 206 |
| 7 | AG7 | Action Control | B11 | 7 | 47 | 87 | 127 | 167 | 207 |
| 8 | AG8 | Action Control | B9 | 8 | 48 | 88 | 128 | 168 | 208 |
| 9 | AG9 | Action Control | B7 | 9 | 49 | 89 | 129 | 169 | 209 |
| 10 | AG10 | Action Control | B5 | 10 | 50 | 90 | 130 | 170 | 210 |
| 11 | AG11 | Action Control | B3 | 11 | 51 | 91 | 131 | 171 | 211 |
| 12 | AG12 | Action Control | B1 | 12 | 52 | 92 | 132 | 172 | 212 |
| 13 | Antenna | Vehicle Control | B2 | 13 | 53 | 93 | 133 | 173 | 213 |
| 14 | Fuel Cell | Vehicle Control | B4 | 14 | 54 | 94 | 134 | 174 | 214 |
| 15 | Solar Array | Vehicle Control | B5 | 15 | 55 | 95 | 135 | 175 | 215 |
| 16 | Cargo Door | Vehicle Control | B6 | 16 | 56 | 96 | 136 | 176 | 216 |
| 17 | Radiator | Vehicle Control | B7 | 17 | 57 | 97 | 137 | 177 | 217 |
| 18 | Ladder | Vehicle Control | B9 | 18 | 58 | 98 | 138 | 178 | 218 |
| 19 | Heat Shield Deploy | Vehicle Control | B8 | 19 | 59 | 99 | 139 | 179 | 219 |
| 20 | Heat Shield Release | Vehicle Control | B8 | 20 | 60 | 100 | 140 | 180 | 220 |
| 21 | Main Chute Deploy | Vehicle Control | B10 | 21 | 61 | 101 | 141 | 181 | 221 |
| 22 | Main Chute Cut | Vehicle Control | B10 | 22 | 62 | 102 | 142 | 182 | 222 |
| 23 | Drogue Chute Deploy | Vehicle Control | B11 | 23 | 63 | 103 | 143 | 183 | 223 |
| 24 | Drogue Chute Cut | Vehicle Control | B11 | 24 | 64 | 104 | 144 | 184 | 224 |
| 25 | LES | Function Control | B0 | 25 | 65 | 105 | 145 | 185 | 225 |
| 26 | Fairing Jettison | Function Control | B1 | 26 | 66 | 106 | 146 | 186 | 226 |
| 27 | Engine Alt Mode | Function Control | B6 | 27 | 67 | 107 | 147 | 187 | 227 |
| 28 | Science Collect | Function Control | B7 | 28 | 68 | 108 | 148 | 188 | 228 |
| 29 | Engine Group 1 | Function Control | B8 | 29 | 69 | 109 | 149 | 189 | 229 |
| 30 | Science Group 1 | Function Control | B9 | 30 | 70 | 110 | 150 | 190 | 230 |
| 31 | Engine Group 2 | Function Control | B10 | 31 | 71 | 111 | 151 | 191 | 231 |
| 32 | Science Group 2 | Function Control | B11 | 32 | 72 | 112 | 152 | 192 | 232 |
| 33 | Air Intake | Function Control | B2 | 33 | 73 | 113 | 153 | 193 | 233 |
| 34 | Lock Surfaces | Function Control | B3 | 34 | 74 | 114 | 154 | 194 | 234 |
| 35 | CP Primary | AUX CTRL | B10 | 35 | 75 | 115 | 155 | 195 | 235 |
| 36 | CP Alternate | AUX CTRL | B10 | 36 | 76 | 116 | 156 | 196 | 236 |
| 37 | CP Docking Port | AUX CTRL | B11 | 37 | 77 | 117 | 157 | 197 | 237 |
| 38 | Airbrake | Function Control | B4 | 38 | 78 | 118 | 158 | 198 | 238 |
| 38 | Airbrake (duplicate) | Rotation Module | BTN_JOY | 38 | 78 | 118 | 158 | 198 | 238 |
| 39 | Reaction Wheel Disable | Function Control | B5 | 39 | 79 | 119 | 159 | 199 | 239 |

**Notes:**
- State machine pairs (Heat Shield, Main Chute, Drogue Chute) use sequential base CAGs — deploy is always the lower number, cut/release always the higher.
- Airbrake at base 38 is intentionally assigned to both Function Control B4 (primary) and Rotation Module BTN_JOY (ergonomic duplicate during atmospheric flight).
- Maximum AGX number used: 239 (within AGX 250 limit).
- The 1-slot gap between groups provides headroom for future additions without renumbering.

---

## 0x20 — UI Control Module

**PCB:** KC-01-1822 | **Type:** 0x01 | **Panel:** A2 | **Library:** KerbalButtonCore | **Extended States:** No

12 NeoPixel buttons in a 2×6 grid.

```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│ Map      │ Map      │ Map      │ Map      │ UI       │Screenshot│
│ Enable   │ Back     │ Forward  │ Reset    │ Toggle   │          │
│ SKY      │ SKY      │ SKY      │ SKY      │ GREEN    │ GREEN    │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────┤
│ IVA      │ Ship     │ Ship     │ Navball  │ Nav Ball │ Debug    │
│          │ Back     │ Forward  │ Ref Cycle│ Toggle   │          │
│ CORAL    │ TEAL     │ TEAL     │ GREEN    │ AMBER    │ RED      │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
  Col 6(L)   Col 5      Col 4      Col 3      Col 2      Col 1(R)
```

| KBC Index | Function | ACTIVE Color | Implementation |
|-----------|----------|-------------|----------------|
| B0 | Screenshot | GREEN | Macro: F2 → wait → F1 → wait → F2 |
| B1 | Debug | RED | `KEYBOARD_EMULATOR` Alt+F12 |
| B2 | UI Toggle | GREEN | `KEYBOARD_EMULATOR` F2 |
| B3 | Nav Ball Toggle | AMBER | `KEYBOARD_EMULATOR` Numpad `.` |
| B4 | Map Reset | SKY | `KEYBOARD_EMULATOR` `'` |
| B5 | Navball Reference Cycle | GREEN | Simpit `NAVBALLMODE_MESSAGE` — cycles Surface → Orbit → Target |
| B6 | Map Forward | SKY | `KEYBOARD_EMULATOR` Tab |
| B7 | Ship Forward | TEAL | `KEYBOARD_EMULATOR` `]` |
| B8 | Map Back | SKY | `KEYBOARD_EMULATOR` Shift+Tab |
| B9 | Ship Back | TEAL | `KEYBOARD_EMULATOR` `[` |
| B10 | Map Enable | SKY | `KEYBOARD_EMULATOR` M |
| B11 | IVA | CORAL | `KEYBOARD_EMULATOR` C |
| B12–B15 | Not installed | — | — |

**Notes:** Nav Ball Toggle (B3) uses Numpad `.` (numpad delete), not the main keyboard delete key. Map Reset (B4) uses apostrophe `'`. Ship Forward/Back (B7/B9) cycle the active controlled vessel, not just camera focus.

---

## 0x21 — Function Control Module

**PCB:** KC-01-1842 | **Type:** 0x02 | **Panel:** A2 | **Library:** KerbalButtonCore | **Extended States:** No

12 NeoPixel buttons (B0–B11) in a 6×2 grid. B12–B15 are no-connects on the PCB. B16–B23 are Switch Group 1 panel-mounted switch inputs via DB127S-5.08-6P screw terminals (two × 4 switches), active-high with 10kΩ pull-down networks RN1/RN2. Firmware reads 3-byte shift register packets (24 inputs total across U14/U15/U16). Layout follows mission timeline — launch actions top, flight systems middle, aerodynamic systems and engine families lower.

```
┌──────────────────┬──────────────────┐
│ LES              │ Fairing Jettison │
│ RED              │ AMBER            │
├──────────────────┼──────────────────┤
│ Air Intake       │ Lock Surfaces    │
│ TEAL             │ SKY              │
├──────────────────┼──────────────────┤
│ Airbrake         │ RW Disable       │
│ CYAN             │ AMBER            │
├──────────────────┼──────────────────┤
│ Engine Alt Mode  │ Science Collect  │
│ ORANGE           │ PURPLE           │
├──────────────────┼──────────────────┤
│ Engine Group 1   │ Science Group 1  │
│ YELLOW           │ INDIGO           │
├──────────────────┼──────────────────┤
│ Engine Group 2   │ Science Group 2  │
│ CHARTREUSE       │ BLUE             │
└──────────────────┴──────────────────┘
  Col 1 (left)       Col 2 (right)
```

| KBC Index | Function | ACTIVE Color | Base CAG | Implementation |
|-----------|----------|-------------|---------|----------------|
| B0 | LES (Launch Escape System) | RED | 25 | Custom AG (AGX) |
| B1 | Fairing Jettison | AMBER | 26 | Custom AG (AGX) |
| B2 | Air Intake | TEAL | 33 | Custom AG (AGX) |
| B3 | Lock Surfaces | SKY | 34 | Custom AG (AGX) |
| B4 | Airbrake | CYAN | 38 | Custom AG (AGX) — intentional duplicate of Rotation BTN_JOY |
| B5 | Reaction Wheel Disable | AMBER | 39 | Custom AG (AGX) |
| B6 | Engine Alt Mode | ORANGE | 27 | Custom AG (AGX) |
| B7 | Science Collect | PURPLE | 28 | Custom AG (AGX) |
| B8 | Engine Group 1 | YELLOW | 29 | Custom AG (AGX) |
| B9 | Science Group 1 | INDIGO | 30 | Custom AG (AGX) |
| B10 | Engine Group 2 | CHARTREUSE | 31 | Custom AG (AGX) |
| B11 | Science Group 2 | BLUE | 32 | Custom AG (AGX) |

### Switch Group 1 (B16–B23 discrete inputs)

B16–B23 are discrete (no LED) inputs wired to panel-mounted switches on Panel A2 via two DB127S-5.08-6P screw terminal connectors (4 switches each). Active-high, 10kΩ pull-down networks. Physical layout: 2 rows × 4 columns.

```
┌──────────┬──────────┬──────────┬──────────┐
│  MSTR    │  DISPL   │  ENGINE  │ THROTTLE │
│  OPER ↑  │  OPER ↑  │  SAFE ↑  │  INOP ↑  │
│  B16     │  B17     │  B18     │  B19     │
│  RESET ↓ │  CLR ↓   │  ARM ↓   │  ACT ↓   │
├──────────┼──────────┼──────────┼──────────┤
│  SCE     │  UPTLM   │  LTG     │ THRTL    │
│  NORM ↑  │ INHIB ↑  │  NORM ↑  │  STD ↑   │
│  B20     │  B21     │  B22     │  B23     │
│  AUX ↓   │  XMIT ↓  │  TEST ↓  │  FINE ↓  │
└──────────┴──────────┴──────────┴──────────┘
  Col 1      Col 2      Col 3      Col 4
```

| KBC Index | Switch Title | OFF | ON | Type | Controller Action |
|-----------|-------------|-----|-----|------|-----------------|
| B16 | MSTR | OPER | RESET | Momentary (hold) | Rising edge: CMD_RESET all modules |
| B17 | DISPL | OPER | CLR | Momentary (hold) | Rising edge: CMD_SET_VALUE 0 to 0x2A, 0x2B — reset all screens to default |
| B18 | ENGINE | SAFE | ARM | Latching | SAFE: throttle input disabled. ARM: throttle module active |
| B19 | THROTTLE | INOP | ACT | Latching | Rising: CMD_SET_PRECISION 0x01 → Throttle module. Falling: CMD_SET_PRECISION 0x00 |
| B20 | SCE | NORM | AUX | Latching | AUX: Simpit reset and data fetch |
| B21 | UPTLM | INHIB | XMIT | Latching | XMIT: enable panel debug telemetry output |
| B22 | LTG | NORM | TEST | Momentary (hold) | TEST rising: bulb test start (CMD_BULB_TEST 0x01 all modules). Falling: bulb test stop (0x00) |
| B23 | THRTL | STD | FINE | Latching | FINE: system-wide input scaling for joystick precision |

**⚠️ TODO #7:** Firmware sketch needs update to match new layout, and must be updated to handle 3-byte shift register reads for B16–B23 switch inputs.

---

## 0x22 — Action Control Module

**PCB:** KC-01-1822 | **Type:** 0x03 | **Panel:** A2 | **Library:** KerbalButtonCore | **Extended States:** No

12 NeoPixel buttons (AG1–AG12). Panel reads right-to-left: AG1 top-right, AG12 bottom-left.

```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│ AG 1     │ AG 2     │ AG 3     │ AG 4     │ AG 5     │ AG 6     │
│ GREEN    │ GREEN    │ GREEN    │ GREEN    │ GREEN    │ GREEN    │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────┤
│ AG 7     │ AG 8     │ AG 9     │ AG 10    │ AG 11    │ AG 12    │
│ GREEN    │ GREEN    │ GREEN    │ GREEN    │ GREEN    │ GREEN    │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
  Col 6(R)   Col 5      Col 4      Col 3      Col 2      Col 1(L)
```

| KBC Index | Panel Position | Function | Base CAG | Implementation |
|-----------|---------------|----------|---------|----------------|
| B0 | Col 1 top | AG6 | 6 | Simpit `CAGTOGGLE_MESSAGE` |
| B1 | Col 1 bottom | AG12 | 12 | Simpit `CAGTOGGLE_MESSAGE` |
| B2 | Col 2 top | AG5 | 5 | Simpit `CAGTOGGLE_MESSAGE` |
| B3 | Col 2 bottom | AG11 | 11 | Simpit `CAGTOGGLE_MESSAGE` |
| B4 | Col 3 top | AG4 | 4 | Simpit `CAGTOGGLE_MESSAGE` |
| B5 | Col 3 bottom | AG10 | 10 | Simpit `CAGTOGGLE_MESSAGE` |
| B6 | Col 4 top | AG3 | 3 | Simpit `CAGTOGGLE_MESSAGE` |
| B7 | Col 4 bottom | AG9 | 9 | Simpit `CAGTOGGLE_MESSAGE` |
| B8 | Col 5 top | AG2 | 2 | Simpit `CAGTOGGLE_MESSAGE` |
| B9 | Col 5 bottom | AG8 | 8 | Simpit `CAGTOGGLE_MESSAGE` |
| B10 | Col 6 top | AG1 | 1 | Simpit `CAGTOGGLE_MESSAGE` |
| B11 | Col 6 bottom | AG7 | 7 | Simpit `CAGTOGGLE_MESSAGE` |
| B12–B15 | Not installed | — | — | — |

AGX number sent = Base CAG + ((Group − 1) × 40).

**⚠️ TODO #8:** Firmware sketch still has CP PRI/CP ALT at B10/B11. Needs update to AG11/AG12.

---

## 0x23 — Stability Control Module

**PCB:** KC-01-1822 | **Type:** 0x04 | **Panel:** B2 | **Library:** KerbalButtonCore | **Extended States:** No

10 NeoPixel SAS mode buttons + RCS toggle + Invert modifier. B14/B15 are discrete inputs for staging. B12/B13 not installed.

```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│ RCS      │ Stab     │ Prograde │ Normal   │ Radial   │ Target   │
│          │ Assist   │          │          │ In       │          │
│ GREEN    │ GREEN    │ GREEN    │ GREEN    │ GREEN    │ GREEN    │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────┤
│ Invert   │ Maneuver │Retrograde│ Anti-    │ Radial   │ Anti-    │
│          │          │          │ Normal   │ Out      │ Target   │
│ AMBER    │ GREEN    │ GREEN    │ GREEN    │ GREEN    │ GREEN    │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
  Col 6(L)   Col 5      Col 4      Col 3      Col 2      Col 1(R)
```

| KBC Index | Function | ACTIVE Color | Implementation |
|-----------|----------|-------------|----------------|
| B0 | SAS — Target | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.Target |
| B1 | SAS — Anti-Target | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.AntiTarget |
| B2 | SAS — Radial In | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.RadialIn |
| B3 | SAS — Radial Out | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.RadialOut |
| B4 | SAS — Normal | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.Normal |
| B5 | SAS — Anti-Normal | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.AntiNormal |
| B6 | SAS — Prograde | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.Prograde |
| B7 | SAS — Retrograde | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.Retrograde |
| B8 | SAS — Stability Assist | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.StabilityAssist |
| B9 | SAS — Maneuver | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.Maneuver |
| B10 | RCS | GREEN | Simpit `AGACTIVATE_MESSAGE` / `AGDEACTIVATE_MESSAGE` RCS_ACTION |
| B11 | Invert | AMBER | `KEYBOARD_EMULATOR` F — held while button held, released when released |
| B12–B13 | Not installed | — | — |
| B14 | Stage (latching toggle, discrete) | — | Rising edge → Simpit `AGACTIVATE_MESSAGE` STAGE_ACTION |
| B15 | Stage Enable (latching toggle + discrete LED) | — | ACTIVE: staging enabled. OFF: staging locked. |

**⚠️ TODO #10:** Firmware sketch still has B12 SAS Enable / B13 RCS Enable. Needs update — remove B12/B13, add RCS at B10.

---

## 0x24 — Vehicle Control Module

**PCB:** KC-01-1842 | **Type:** 0x05 | **Panel:** B2 | **Library:** KerbalButtonCore | **Extended States:** Yes

12 NeoPixel buttons (B0–B11). B12–B15 are no-connects on the PCB. B16–B23 are Switch Group 2 panel-mounted switch inputs via DB127S-5.08-6P screw terminals (two × 4 switches), active-high with 10kΩ pull-down networks. Firmware reads 3-byte shift register packets. Parachute and heat shield buttons use extended LED states via state machines.

```
┌──────────────────┬──────────────────┐
│ Lights           │ Brakes           │
│ YELLOW           │ RED              │
├──────────────────┼──────────────────┤
│ Gear             │ Antenna          │
│ GREEN            │ PINK             │
├──────────────────┼──────────────────┤
│ Solar Array      │ Fuel Cell        │
│ GOLD             │ CYAN             │
├──────────────────┼──────────────────┤
│ Radiator         │ Cargo Door       │
│ ORANGE           │ TEAL             │
├──────────────────┼──────────────────┤
│ Ladder           │ Heat Shield ★    │
│ LIME             │ GREEN/RED        │
├──────────────────┼──────────────────┤
│ Drogue Chute ★   │ Main Chute ★     │
│ GREEN/RED        │ GREEN/RED        │
└──────────────────┴──────────────────┘
  Col 1 (left)       Col 2 (right)
```

★ Multi-state buttons.

| KBC Index | R×C | Function | ACTIVE Color | Base CAG | Implementation |
|-----------|-----|----------|-------------|---------|----------------|
| B0 | R1C2 | Brakes | RED | — | Simpit `BRAKES_ACTION` toggle |
| B1 | R1C1 | Lights | YELLOW | — | Simpit `LIGHTS_ACTION` toggle |
| B2 | R2C2 | Antenna | PINK | 13 | Custom AG (AGX) |
| B3 | R2C1 | Gear | GREEN | — | Simpit `GEAR_ACTION` toggle |
| B4 | R3C2 | Fuel Cell | CYAN | 14 | Custom AG (AGX) |
| B5 | R3C1 | Solar Array | GOLD | 15 | Custom AG (AGX) |
| B6 | R4C2 | Cargo Door | TEAL | 16 | Custom AG (AGX) |
| B7 | R4C1 | Radiator | ORANGE | 17 | Custom AG (AGX) |
| B8 | R5C2 | Heat Shield ★ | GREEN (deploy) / RED (release) | 19/20 | State machine |
| B9 | R5C1 | Ladder | LIME | 18 | Custom AG (AGX) |
| B10 | R6C2 | Main Chute ★ | GREEN (deploy) / RED (cut) | 21/22 | State machine — requires CHUTE switch ARM |
| B11 | R6C1 | Drogue Chute ★ | GREEN (deploy) / RED (cut) | 23/24 | State machine — requires CHUTE switch ARM |
| B12–B15 | — | No-connect (PCB) | — | — | — |

### Parachute State Machine (B10/B11)

Requires Switch Group 2 CHUTE switch in ARM position (continuous interlock). CHUTE returning to SAFE resets both buttons to OFF immediately.

| State | LED | Action on press |
|-------|-----|-----------------|
| OFF | Dark | Fire base CAG deploy → DEPLOYED |
| DEPLOYED | GREEN (ACTIVE) | Fire base CAG cut → CUT |
| CUT | RED (ACTIVE) | Terminal state |

Main Chute: base CAG 21 (deploy) / 22 (cut). Drogue Chute: base CAG 23 (deploy) / 24 (cut).

### Heat Shield State Machine (B8)

Independent — no switch interlock.

| State | LED | Action on press |
|-------|-----|-----------------|
| OFF | Dark | Fire base CAG 19 (deploy) → DEPLOYED |
| DEPLOYED | GREEN (ACTIVE) | Fire base CAG 20 (release) → RELEASED |
| RELEASED | RED (ACTIVE) | Terminal state |

### Switch Group 2 (B16–B23 discrete inputs)

B16–B23 are discrete (no LED) inputs wired to panel-mounted switches on Panel B2 via two DB127S-5.08-6P screw terminal connectors (4 switches each). Active-high, 10kΩ pull-down networks. Physical layout: 2 rows × 4 columns.

```
┌──────────┬──────────┬──────────┬──────────┐
│  CHUTE   │  GEAR    │  BRAKE   │  EXT LT  │
│  SAFE ↑  │  UP ↑    │  REL ↑   │  OFF ↑   │
│  B16     │  B17     │  B18     │  B19     │
│  ARM ↓   │  DOWN ↓  │  LOCK ↓  │  ILLUM ↓ │
├──────────┼──────────┼──────────┼──────────┤
│  SAS     │  RCS     │ THC/RHC  │  AUDIO   │
│  OFF ↑   │  OFF ↑   │  NORM ↑  │  MUTE ↑  │
│  B20     │  B21     │  B22     │  B23     │
│  ENAB ↓  │  ENAB ↓  │  PREC ↓  │  LIVE ↓  │
└──────────┴──────────┴──────────┴──────────┘
  Col 1      Col 2      Col 3      Col 4
```

| KBC Index | Switch Title | OFF | ON | Type | Controller Action |
|-----------|-------------|-----|-----|------|-----------------|
| B16 | CHUTE | SAFE | ARM | Latching | Safety interlock — enables B10/B11 parachute state machines when ARM |
| B17 | GEAR | UP | DOWN | Latching | DOWN: Simpit `AGACTIVATE_MESSAGE` GEAR_ACTION. UP: `AGDEACTIVATE_MESSAGE` GEAR_ACTION |
| B18 | BRAKE | REL | LOCK | Latching | LOCK: Simpit `AGACTIVATE_MESSAGE` BRAKES_ACTION. REL: `AGDEACTIVATE_MESSAGE` BRAKES_ACTION |
| B19 | EXT LT | OFF | ILLUM | Latching | ILLUM: Simpit `AGACTIVATE_MESSAGE` LIGHTS_ACTION. OFF: `AGDEACTIVATE_MESSAGE` LIGHTS_ACTION |
| B20 | SAS | OFF | ENAB | Latching | ENAB: Simpit `AGACTIVATE_MESSAGE` SAS_ACTION. OFF: `AGDEACTIVATE_MESSAGE` SAS_ACTION |
| B21 | RCS | OFF | ENAB | Latching | ENAB: Simpit `AGACTIVATE_MESSAGE` RCS_ACTION. OFF: `AGDEACTIVATE_MESSAGE` RCS_ACTION |
| B22 | THC/RHC | NORM | PREC | Latching | PREC: system-wide translation/rotation input precision scaling |
| B23 | AUDIO | MUTE | LIVE | Latching | LIVE: enables controller audio output |

**⚠️ TODO #9:** Firmware sketch still has old B12–B15 discrete input handling. Needs update — remove B12–B15 handling, add Switch Group 2 reading at B16–B23 with 3-byte shift register reads.

---

## 0x25 — Time Control Module

**PCB:** KC-01-1822 | **Type:** 0x06 | **Panel:** B2 | **Library:** KerbalButtonCore | **Extended States:** No

12 NeoPixel buttons. Yellow family color theme throughout.

```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│ Pause    │ Morning  │ SOI      │ Maneuver │ PeA      │ ApA      │
│ AMBER    │ YELLOW   │ YELLOW   │ YELLOW   │ YELLOW   │ YELLOW   │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────┤
│ Save     │ Load     │ Warp     │ Warp     │ Physics  │ Stop     │
│          │          │ Back     │ Forward  │ Warp     │          │
│ LIME     │ MINT     │ WARM WHT │ WARM WHT │ CHARTRS  │ AMBER    │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
  Col 6(L)   Col 5      Col 4      Col 3      Col 2      Col 1(R)
```

| KBC Index | Function | ACTIVE Color | Implementation |
|-----------|----------|-------------|----------------|
| B0 | Warp to Apoapsis | YELLOW | Simpit `TIMEWARP_TO_MESSAGE` ApA − Pre-Warp offset |
| B1 | Warp Stop | AMBER | Simpit `TIMEWARP_MESSAGE` stop (rate = 1×) |
| B2 | Warp to Periapsis | YELLOW | Simpit `TIMEWARP_TO_MESSAGE` PeA − Pre-Warp offset |
| B3 | Physics Warp (modifier) | CHARTREUSE | Hold + B5/B7 → physics warp increment/decrement |
| B4 | Warp to Maneuver | YELLOW | Simpit `TIMEWARP_TO_MESSAGE` Maneuver − Pre-Warp offset |
| B5 | Warp Forward | WARM WHITE | Simpit `TIMEWARP_MESSAGE` rate increment |
| B6 | Warp to SOI | YELLOW | Simpit `TIMEWARP_TO_MESSAGE` SOI − Pre-Warp offset |
| B7 | Warp Back | WARM WHITE | Simpit `TIMEWARP_MESSAGE` rate decrement |
| B8 | Warp to Morning | YELLOW | Simpit `TIMEWARP_TO_MESSAGE` Morning − Pre-Warp offset |
| B9 | Quick Load | MINT | `KEYBOARD_EMULATOR` F9 |
| B10 | Pause | AMBER | `KEYBOARD_EMULATOR` Escape |
| B11 | Quick Save | LIME | `KEYBOARD_EMULATOR` F5 |
| B12–B15 | Not installed | — | — |

**B3 (Physics Warp)** is a modifier — hold B3 and press B5 (Warp Forward) or B7 (Warp Back) to change physics warp rate. B3 alone does nothing. Warp-to-event buttons (B0/B2/B4/B6/B8) read the current value from the Pre-Warp Time Module (0x2B) as the lead-time offset in minutes.

---

## 0x26 — AUX CTRL Module

**PCB:** KC-01-1822 | **Type:** 0x07 | **Panel:** B2 | **Library:** KerbalButtonCore | **Extended States:** No

12 NeoPixel buttons. EVA functions (B0–B7) active only when `isEVA = true`. General flight functions (B8–B11) active in all modes.

```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│ CP       │ Cruise   │ EVA      │ Jump /   │ Board    │ EVA      │
│ Toggle   │ Control  │ Chute    │ Let Go   │ Craft    │ Lights   │
│ ROSE/CRL │ GOLD     │ AMBER    │ CHARTRSE │ GREEN    │ MINT     │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────┤
│ CP       │ Plant    │ Helmet   │ Grab     │ EVA      │ Jetpack  │
│ Dock Port│ Flag     │ Toggle   │          │ Constr.  │ Enable   │
│ PINK     │ SEAFOAM★ │ SKY      │ SEAFOAM  │ TEAL     │ LIME     │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
  Col 6(L)   Col 5      Col 4      Col 3      Col 2      Col 1(R)
```

★ Plant Flag uses SEAFOAM flash — brief illuminate on press, returns to ENABLED.

| KBC Index | Function | ACTIVE Color | Base CAG | Active When | Implementation |
|-----------|----------|-------------|---------|-------------|----------------|
| B0 | EVA Lights | MINT | — | `isEVA = true` | `KEYBOARD_EMULATOR` U |
| B1 | Jetpack Enable | LIME | — | `isEVA = true` | `KEYBOARD_EMULATOR` R |
| B2 | Board Craft | GREEN | — | `isEVA = true` | `KEYBOARD_EMULATOR` B |
| B3 | EVA Construction | TEAL | — | `isEVA = true` | `KEYBOARD_EMULATOR` I |
| B4 | Jump / Let Go | CHARTREUSE | — | `isEVA = true` | `KEYBOARD_EMULATOR` Space |
| B5 | Grab | SEAFOAM | — | `isEVA = true` | `KEYBOARD_EMULATOR` F |
| B6 | EVA Chute | AMBER | — | `isEVA = true` | `KEYBOARD_EMULATOR` P |
| B7 | Helmet Toggle | SKY | — | `isEVA = true` | `KEYBOARD_EMULATOR` O |
| B8 | Cruise Control | GOLD | — | Rover mode | Controller-side hold — press to set current wheel throttle, press again to release |
| B9 | Plant Flag | SEAFOAM (flash) | — | `isEVA = true` | `KEYBOARD_EMULATOR` G |
| B10 | CP Toggle (PRI/ALT) | ROSE (PRI) / CORAL (ALT) | 35/36 | All modes | Cycles between base CAG 35 and 36 — mutually exclusive |
| B11 | CP Docking Port | PINK | 37 | All modes | Deactivates PRI/ALT, activates base CAG 37 |

### Control Point Logic (B10/B11)

Three mutually exclusive states. Transitions always deactivate the previous CAG before activating the new one. All CAG numbers adjust by active control group offset.

| State | Button | Color | Base CAG |
|-------|--------|-------|---------|
| Primary | B10 | ROSE | 35 |
| Alternate | B10 | CORAL | 36 |
| Docking Port | B11 | PINK | 37 |

---

## 0x28 — Joystick Rotation Module

**PCB:** KC-01-1831/1832 | **Type:** 0x09 | **Panel:** B2 | **Library:** KerbalJoystickCore | **Capability:** JOYSTICK

Three-axis hall-effect joystick. Two NeoPixel buttons. Four operating modes — three selected by the Ctrl Mode Toggle (direct-wired 3-position switch), EVA auto-activated by `isEVA` from Simpit.

### Mode Overview

| Mode | Trigger | Simpit Channel |
|------|---------|----------------|
| Spacecraft | SPC switch position | `ROTATION_MESSAGE` |
| Airplane | AIR switch position | `ROTATION_MESSAGE` |
| Rover | RVR switch position | `WHEEL_MESSAGE` |
| EVA | `isEVA = true` from Simpit (auto) | `KEYBOARD_EMULATOR` |

### Ctrl Mode Toggle — Axis Mapping

| Mode | AXIS1 | AXIS2 | AXIS3 |
|------|-------|-------|-------|
| Spacecraft | `ROTATION_MESSAGE` yaw | `ROTATION_MESSAGE` pitch | `ROTATION_MESSAGE` roll |
| Airplane | `ROTATION_MESSAGE` roll | `ROTATION_MESSAGE` pitch | `ROTATION_MESSAGE` yaw |
| Rover | `WHEEL_MESSAGE` steer | `WHEEL_MESSAGE` throttle | Unused |

Airplane and Spacecraft modes differ only in the AXIS1/AXIS3 swap — roll and yaw are exchanged.

### EVA Mode (auto — `isEVA = true`)

Digital key emulation. Any deflection outside deadzone = key held; return to center = key released.

| Axis / Control | Physical | KSP Action | Key |
|----------------|----------|-----------|-----|
| AXIS1 left/right | Stick left/right | Walk Left / Right | A / D (held) |
| AXIS2 forward/back | Stick fwd/back | Walk Forward / Back | W / S (held) |
| AXIS3 CCW/CW | Twist | Jetpack Rotate Left / Right | Q / E (held) |
| BTN_JOY held | Button | Run | Left Shift (held) |

### Controls

| Control | Function | ACTIVE Color | Base CAG |
|---------|----------|-------------|---------|
| BTN01 | Reset Trim — clears all trim to neutral (Alt+X) | GREEN | — |
| BTN02 | Set Trim — captures current deflection as trim offset | AMBER | — |
| BTN_JOY | Airbrake toggle (spacecraft/airplane/rover) / Run hold (EVA) | — | 38 (intentional duplicate of Function Control B4) |

**Note:** Do not touch joystick during first ~80ms after power-on — startup calibration reads center position at boot.

---

## 0x29 — Joystick Translation Module

**PCB:** KC-01-1831/1832 | **Type:** 0x0A | **Panel:** A2 | **Library:** KerbalJoystickCore | **Capability:** JOYSTICK

Three-axis hall-effect joystick. Two NeoPixel buttons. Three operating modes.

### Mode Overview

| Mode | Trigger | Simpit Channel |
|------|---------|----------------|
| Translation | Normal operation | `TRANSLATION_MESSAGE` |
| Camera | BTN_JOY held (any mode) | `CAMERA_MESSAGE` |
| EVA Jetpack | `isEVA = true` (auto) | `KEYBOARD_EMULATOR` |

### Translation Mode

AXIS2 inversion is applied in module firmware — controller receives correct polarity directly.

| Axis | Physical | KSP Function | Simpit Field |
|------|----------|-------------|-------------|
| AXIS1 | Left / Right | Translate Left / Right | `TRANSLATION_MESSAGE` X |
| AXIS2 | Forward push / Back pull | Translate Up / Down | `TRANSLATION_MESSAGE` Y |
| AXIS3 | CW / CCW twist | Translate Forward / Reverse | `TRANSLATION_MESSAGE` Z |

### Camera Mode (BTN_JOY held — any mode)

Mode switch is entirely in controller firmware — module sends identical axis data regardless.

| Axis | Physical | Camera Function | Simpit Field |
|------|----------|----------------|-------------|
| AXIS1 | Left / Right | Camera Yaw Left / Right | `CAMERA_MESSAGE` yaw |
| AXIS2 | Forward push / Back pull | Camera Pitch Up / Down | `CAMERA_MESSAGE` pitch |
| AXIS3 | CW / CCW twist | Zoom In / Zoom Out | `CAMERA_MESSAGE` zoom |

### EVA Jetpack Mode (auto — `isEVA = true`)

Digital key emulation. W/A/S/D serve double duty — walking on ground, jetpack translation in space.

| Axis / Control | Physical | KSP Action | Key |
|----------------|----------|-----------|-----|
| AXIS1 left/right | Stick left/right | Jetpack left/right | A / D (held) |
| AXIS2 forward/back | Stick fwd/back | Jetpack forward/back | W / S (held) |
| AXIS3 CW/CCW | Twist | Jetpack up/down | Left Shift / Left Ctrl (held) |
| BTN_JOY held | Button | Camera control | (camera mode applies) |

### Controls

| Control | Function | ACTIVE Color |
|---------|----------|-------------|
| BTN01 | Cycle Camera Mode (auto/free/orbital/chase) | MAGENTA |
| BTN02 | Camera Reset | GREEN |
| BTN_JOY held | Camera Control mode (all modes) | — |

**Note:** Do not touch joystick during first ~80ms after power-on. AXIS2 inversion is in firmware — controller uses values directly.

---

## 0x2A — GPWS Input Panel

**PCB:** KC-01-1881/1882 | **Type:** 0x0B | **Panel:** A1 | **Library:** Kerbal7SegmentCore | **Capability:** DISPLAY

4-digit 7-segment display (MAX7219), rotary encoder, three SK6812MINI-EA NeoPixel buttons.

| Control | Function | Behavior |
|---------|----------|----------|
| 4-digit display | Altitude threshold | Shows current value 0–9999m, no leading zeros |
| Encoder | Adjust threshold | ±1 slow / ±10 medium / ±100 fast |
| Encoder button | Reset to default | Returns to 200m |
| BTN01 | GPWS Enable | 3-state cycle: OFF → ACTIVE (green, full GPWS) → AMBER (proximity tone only) |
| BTN02 | Proximity Alarm | Toggle: OFF ↔ GREEN |
| BTN03 | Rendezvous Radar | Toggle: OFF ↔ GREEN |

**Encoder acceleration:** Slow (>150ms between detents) = ±1 · Medium (50–150ms) = ±10 · Fast (<50ms) = ±100

**INT suppression:** When BTN01 is in state 0 (GPWS off), BTN02, BTN03, and encoder events are suppressed — BTN01 always reports.

**State byte (packet byte 5):** Bits 1:0 = BTN01 cycle state (0=off, 1=full GPWS, 2=proximity only) · Bit 2 = BTN02 active · Bit 3 = BTN03 active

---

## 0x2B — Pre-Warp Time Module

**PCB:** KC-01-1881/1882 | **Type:** 0x0C | **Panel:** B2 | **Library:** Kerbal7SegmentCore | **Capability:** DISPLAY

4-digit 7-segment display (MAX7219), rotary encoder, three SK6812MINI-EA NeoPixel buttons (flash mode for presets).

| Control | Function | Behavior |
|---------|----------|----------|
| 4-digit display | Pre-warp duration | Shows minutes 0–9999, no leading zeros |
| Encoder | Adjust duration | ±1 slow / ±10 medium / ±100 fast |
| Encoder button | Reset to 0 | Clears to default |
| BTN01 | 5 min preset | GOLD flash on press, display jumps to 5 |
| BTN02 | 1 hour preset | GOLD flash on press, display jumps to 60 |
| BTN03 | 1 day preset | GOLD flash on press, display jumps to 1440 |

**Encoder acceleration:** Slow (>150ms) = ±1 · Medium (50–150ms) = ±10 · Fast (<50ms) = ±100

Value transmitted to controller continuously as encoder is turned. Time Control buttons (0x25 B0/B2/B4/B6/B8) read this value as the lead-time offset before the warp target event.

---

## 0x2C — Throttle Module

**PCB:** KC-01-1861/1862 | **Type:** 0x0D | **Panel:** A2 | **Library:** Standalone

Motorized 10k slide potentiometer with L293D H-bridge motor driver. Capacitive touch sensor. Four discrete pushbuttons with discrete LED outputs.

| Control | Function | Notes |
|---------|----------|-------|
| Slide potentiometer | Throttle position | Motorized — moves under controller command |
| Touch sensor | Pilot contact detect | Touching slider overrides motor, allows manual input |
| THRTL_100 + LED | Set throttle to 100% | Discrete button + LED |
| THRTL_UP + LED | Step throttle up | Discrete button + LED |
| THRTL_DOWN + LED | Step throttle down | Discrete button + LED |
| THRTL_00 + LED | Set throttle to 0% | Discrete button + LED |

**Normal mode:** Controller commands position; motor drives slider to target; touching slider overrides; releasing restores motor control.

**Precision mode** (CMD_SET_PRECISION 0x01): Slider physically centered for equal authority; virtual range maps to full 0–100% output.

**Status byte (packet byte 3):** Bit 0 = enabled · Bit 1 = precision mode · Bit 2 = pilot touching · Bit 3 = motor moving

Module starts **disabled** — controller must send CMD_ENABLE. Requires +12V motor bus in addition to +5V logic.

---

## 0x2D — Dual Encoder Module

**PCB:** KC-01-1871/1872 | **Type:** 0x0E | **Panel:** B1 | **Library:** Standalone | **Capability:** ENCODERS

Two independent quadrature encoders with pushbuttons. No LEDs.

| Control | Function | Implementation |
|---------|----------|----------------|
| ENC1 CW | AG Block next | Controller-side — increments active custom AG block |
| ENC1 CCW | AG Block previous | Controller-side — decrements active custom AG block |
| ENC1 button | Docked flag toggle | Controller-side — toggles `isDocked` for TFT display sync. No Simpit message. |
| ENC2 CW | Next external camera | `KEYBOARD_EMULATOR` `-` (Hullcam VDS Continued) |
| ENC2 CCW | Previous external camera | `KEYBOARD_EMULATOR` `=` (Hullcam VDS Continued) |
| ENC2 button | Reset camera | `KEYBOARD_EMULATOR` Backspace (Hullcam VDS Continued) |

ENC1 shifts the entire custom AG mapping table — all AGX numbers sent by Function Control and AUX CTRL adjust to the new block offset. The mapping change takes effect on the next button press with no Simpit message sent on ENC1 turn.

ENC2 requires **Hullcam VDS Continued** mod. Verify default keybindings match in-game settings.

**⚠️ TODO #13:** Verify Hullcam VDS keybindings match in-game settings before use.

---

## 0x2E — Switch Panel (Reserved)

**Type:** 0x0F | **Status:** Hardware TBD

Address 0x2E is reserved for a future consolidated switch panel module. The switch functions previously assigned here have been distributed to discrete inputs on Function Control (0x21) and Vehicle Control (0x24) as Switch Groups 1 and 2.

---

## Panel Location Summary

| Address | Module | Type ID | Panel | Library | Status |
|---------|--------|---------|-------|---------|--------|
| 0x20 | UI Control | 0x01 | A2 | KerbalButtonCore | Active |
| 0x21 | Function Control | 0x02 | A2 | KerbalButtonCore | Active — TODO #7 |
| 0x22 | Action Control | 0x03 | A2 | KerbalButtonCore | Active — TODO #8 |
| 0x23 | Stability Control | 0x04 | B2 | KerbalButtonCore | Active — TODO #10 |
| 0x24 | Vehicle Control | 0x05 | B2 | KerbalButtonCore | Active — TODO #9 |
| 0x25 | Time Control | 0x06 | B2 | KerbalButtonCore | Active |
| 0x26 | AUX CTRL | 0x07 | B2 | KerbalButtonCore | Active — fully allocated |
| 0x27 | Reserved | — | — | — | — |
| 0x28 | Joystick Rotation | 0x09 | B2 | KerbalJoystickCore | Active |
| 0x29 | Joystick Translation | 0x0A | A2 | KerbalJoystickCore | Active |
| 0x2A | GPWS Input Panel | 0x0B | A1 | Kerbal7SegmentCore | Active |
| 0x2B | Pre-Warp Time | 0x0C | B2 | Kerbal7SegmentCore | Active |
| 0x2C | Throttle Module | 0x0D | A2 | Standalone | Active |
| 0x2D | Dual Encoder | 0x0E | B1 | Standalone | Active |
| 0x2E | Switch Panel | 0x0F | — | — | Hardware TBD |
| 0x2F | (was Indicator) | — | — | — | **Removed from design** |

---

## Removed from Design (v4.0 → v5.1)

| Item | Disposition |
|------|------------|
| Indicator Module (0x2F) — all 18 pixels | Module removed. See TODO #6 for feedback gap analysis. |
| Switch Panel (0x2E) single-module design | Replaced by Switch Groups 1 (on 0x21) and 2 (on 0x24) |
| Stability Control B12 SAS Enable | Moved to Switch Group 2 row 2 (hardware TBD) |
| Stability Control B13 RCS Enable | Moved to Switch Group 2 row 2 + Stability Control B10 button |
| Vehicle Control B12 Parking Brake | Moved to Switch Group 2 B14 (BRAKE) |
| Vehicle Control B13 Parachutes Armed | Moved to Switch Group 2 B12 (CHUTE) |
| Vehicle Control B14 Lights Lock | Moved to Switch Group 2 B15 (EXT LT) |
| Vehicle Control B15 Gear Lock | Moved to Switch Group 2 B13 (GEAR) |
| Action Control B12 Spacecraft Mode discrete | Moved to Ctrl Mode Toggle direct wire |
| Action Control B14 Rover Mode discrete | Moved to Ctrl Mode Toggle direct wire |
| Function Control B10 CP Toggle | Moved to AUX CTRL B10 |
| Function Control B11 CP Docking Port | Moved to AUX CTRL B11 |

---

## Open Items

| # | Item | Type |
|---|------|------|
| 6 | Operator feedback review — ABORT, mode state, COMM ACTIVE, SWITCH ERROR lost with Indicator module | Design |
| 7 | Function Control sketch — update to new layout + 3-byte shift register reads for B16–B23 | Code |
| 8 | Action Control sketch — B10/B11 to AG11/AG12 | Code |
| 9 | Vehicle Control sketch — update to Switch Group 2 at B16–B23 with 3-byte reads | Code |
| 10 | Stability Control sketch — remove B12/B13, add RCS at B10 | Code |
| 13 | Verify Hullcam VDS Continued keybindings | Validation |
| 14 | Ctrl Mode Toggle wiring destination (Ctrl Board or module) | Design |

---

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 4.0 | 2026-04-09 | Initial release — 15 modules documented including Indicator Module |
| 5.0 | 2026-05-10 | Panel Interface Guide — panel-centric reorganization; Function Control redesigned; AUX CTRL (fmr EVA) expanded to 12 buttons; CP buttons moved to AUX CTRL; Switch Panel replaced by Switch Groups 1+2; Indicator Module removed; all discrete inputs removed from Vehicle Control, four from Stability Control |
| 5.1 | 2026-05-19 | Reconciliation of v4.0 UI Reference and v5.0 Panel Interface Guide into single module-centric developer document. Switch Group 1 (on Function Control B12–B15) and Switch Group 2 (on Vehicle Control B12–B15) fully documented — panel-mounted switches wired to discrete inputs. Address summary table added. Removed-from-design table added. Open items carried forward. |
