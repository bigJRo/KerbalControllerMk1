# Kerbal Controller Mk1 — Expansion Module Specification

**Organization:** Jeb's Controller Works  
**Author:** J. Rostoker  
**Version:** 0.2 (Draft)  
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
| 5 | SDA_EXT | Bidirectional | I2C data — LTC4311 at each end |
| 6 | SCL_EXT | Bidirectional | I2C clock — LTC4311 at each end |
| 7 | EXT_INT_1 | Expansion → Main | Active-low — 47Ω series resistor at expansion end |
| 8 | EXT_INT_2 | Main → Expansion | Active-low — 47Ω series resistor at main controller end |
| 9 | RST | Main → Expansion | Active-low — 47Ω series at source; 100Ω + 100nF RC filter at expansion end |
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
  │                              └── AP2112K → 3.3V (TUSB212, LTC4311, logic)
  ├── SDA_EXT / SCL_EXT ─── LTC4311 ── Teensy 4.1 I2C target port
  ├── EXT_INT_1 ──────────── 47Ω ────── Teensy 4.1 GPIO output
  ├── EXT_INT_2 ──────────── Teensy 4.1 GPIO input
  └── RST ─────────────────── 100Ω + 100nF ── Teensy 4.1 RST pin

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
| Expansion Routing Board | KC-01-EX10 | GX16-10 and USB-C panel connectors; TUSB212 USB conditioner; LTC4311 on SDA_EXT/SCL_EXT; CH334F internal hub; MPM3610 + AP2112K power regulation; distributes power, I2C, INT, RST to internal boards |
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
