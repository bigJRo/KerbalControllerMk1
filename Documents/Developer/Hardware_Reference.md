# Kerbal Controller Mk1 — Hardware Reference

**Organization:** Jeb's Controller Works  
**Author:** J. Rostoker  
**Version:** 1.3  
**Date:** May 2026  
**Document type:** Developer — Hardware

---

## Revision History

| Version | Date | Description |
|---------|------|-------------|
| 1.0 | 2026-05-19 | Initial release — system architecture, board topology, power distribution, USB hub, interboard connector pinout, PCB design list |
| 1.1 | 2026-05-19 | Corrected interrupt architecture (push-pull output + passive voltage divider, not open-drain + pull-up). Corrected I2C device map — removed LTC4311 (not a device), corrected EMC2101 address to 0x4C, removed MCP9808 (not in design). Split device map into fixed silicon and programmable modules. Added complete verified I2C address map for all display carriers and ATtiny816 modules. Updated related documents table. |
| 1.2 | 2026-05-19 | Added Section 12 — Module Conformance Requirements. Defines hardware (connector, power, interrupt line, I2C) and firmware (identity response, universal header, lifecycle state machine, base command set, INT assertion) requirements for any conformant KCMk1 module. PCB Design List renumbered to Section 13, Related Documents to Section 14. |
| 1.3 | 2026-05-23 | Added Section 9.4 — External Expansion Interface (GX16-10 signal connector + USB-C panel mount). Added §8.4 — External Interface Signal Architecture (series resistors on EXT_INT and RST lines, RST RC filter, TUSB212 USB signal conditioner, LTC4311 on SDA_EXT/SCL_EXT). Updated §8.1 — SDA_EXT bus notes. Updated §8.3 — I2C device map with expansion unit addresses (0x16, 0x17). Updated §9.3 — EXT_INT and RST signal descriptions reference new §9.4. Updated §5.1 — Ctrl Module notes TUSB212 on expansion USB-C. Updated §13 PCB design list with expansion unit boards. |

---

## Table of Contents

1. [Overview](#1-overview)
2. [Required KSP Mods](#2-required-ksp-mods)
3. [System Architecture](#3-system-architecture)
4. [Panel Layout](#4-panel-layout)
5. [Board Topology](#5-board-topology)
6. [Power Distribution](#6-power-distribution)
7. [USB Architecture](#7-usb-architecture)
8. [I2C Bus Architecture](#8-i2c-bus-architecture)
9. [Interboard Connector Pinout](#9-interboard-connector-pinout)
   - [9.4 External Expansion Interface](#94-external-expansion-interface)
10. [Module Hub 34-Pin IDC Connector](#10-module-hub-34-pin-idc-connector)
11. [Module 16-Pin IDC Connector](#11-module-16-pin-idc-connector)
12. [Module Conformance Requirements](#12-module-conformance-requirements)
13. [PCB Design List](#13-pcb-design-list)
14. [Related Documents](#14-related-documents)

---

## 1. Overview

The Kerbal Controller Mk1 (KCMk1) is a custom multi-panel hardware controller for Kerbal Space Program (KSP), communicating with the game via the KerbalSimpit v2.4.0 plugin. The controller consists of four physical panels (A1, A2, B1, B2) plus a planned C extension, each containing multiple input and display modules interconnected via I2C and distributed power.

This document is the primary hardware reference for developers designing, building, or modifying the controller. It covers system architecture, board topology, power distribution, USB architecture, interboard connectors, and PCB inventory.

---

## 2. Required KSP Mods

| Mod | Version | Purpose |
|-----|---------|---------|
| KerbalSimpit | v2.4.0 | Serial communication between KSP and the master controller |
| AGX (Action Groups Extended) | Current | Expands custom action groups to 256 (required for full button mapping) |
| Hullcam VDS Continued | Current | Camera cycling support for ENC2 encoder |

---

## 3. System Architecture

### 3.1 Top-Level Signal Flow

A single USB cable connects the controller to the host PC. Inside the controller, a two-chip USB hub (2× CH334F cascaded) distributes USB to all Teensy-based microcontrollers. A single I2C bus connects the master Teensy 4.1 to all ATtiny816 input/output modules, Teensy 4.0 based display modules, and XIAO RA4M1 based display modules. A 12V main bus provides power throughout, with local DC-DC conversion on each board.

### 3.2 Master Controller

The master controller is a Teensy 4.1 mounted on the Ctrl Module PCB. It is responsible for:

- Polling all I2C module targets on the main SDA/SCL bus based on individual module interrupts
- Interpreting button/axis data and translating to KSP commands via KerbalSimpit over USB
- Driving fan speed control and system temperature via EMC2101
- Monitoring system power via INA228 current/power sensor
- Providing both main and extension I2C bus acceleration via LTC4311
- Communicating system parameters to the system display modules

### 3.3 Display Subsystem

Four Teensy 4.0 microcontrollers each drive an RA8875 800×480 TFT display via SPI. Each Teensy 4.0 connects to the host PC via the USB hub as an independent USB device running KerbalSimpit. The four display roles are: Annunciator (Caution & Warning panel), Resource Display, Info Display 1, and Info Display 2.

### 3.4 System Displays

Two XIAO RA4M1 microcontrollers each drive an ST7789 320×170 IPS display via SPI. Each connects only to the master controller and does not have a direct USB connection to Simpit. The two display roles are: System Temperature/Power data and System Status Indicators.

### 3.5 IO Module Architecture

ATtiny816-based I2C target modules provide button input, display output, joystick, and encoder functions. Each module communicates over the shared I2C bus using the KCMk1 I2C Protocol Specification. Modules assert an active-low interrupt line when state changes, allowing the master to poll only when needed rather than continuously scanning.

---

## 4. Panel Layout

| Panel | Location | Primary Function | Hub Board |
|-------|----------|------------------|-----------|
| A1 | Left Display Panel | Caution & Warning display, GPWS, primary flight instruments | A1 Panel Hub |
| A2 | Left Input Panel | Action controls, function controls, throttle, UI controls, translation input | A2 Panel Hub |
| B1 | Right Display Panel | Secondary flight display, resource display, dual encoder inputs, and Abort button | B1 Panel Hub |
| B2 | Right Input Panel | Vehicle controls, stability controls, staging button, auxiliary controls, time controls, rotation input | B2 Panel Hub |
| C (planned) | Extension unit | Additional displays and button panels — future expansion | TBD |

For the full per-module breakdown of I2C addresses, button assignments, switch wiring, and firmware implementation detail, see `docs/developer/Module_UI_Reference.md`.

---

## 5. Board Topology

### 5.1 Central Boards

| Board | Designator | Microcontroller | Function |
|-------|------------|-----------------|----------|
| Ctrl Module | KC-01-1800 | Teensy 4.1 | Master controller, soft latch power, INA228, EMC2101, Main Bus LTC4311, ST7789 status display, USB Hub [2× CH334F cascaded, single upstream USB-C, 6 downstream Micro-USB + 1 expansion USB-C]. Carries 47Ω series resistors on EXT_INT_2 and RST outputs to 50-pin IDC |
| Ctrl Ext Module | KC-01-1820 | N/A | B1/B2 IDC distribution, extension support |

### 5.2 Panel Routing Boards

| Board | Designator | Function |
|-------|------------|----------|
| A1 Panel Routing Board | TBD | IDC distribution for Panel A1 modules |
| A2 Panel Routing Board | TBD | IDC distribution for Panel A2 modules |
| B1 Panel Routing Board | TBD | IDC distribution for Panel B1 modules |
| B2 Panel Routing Board | TBD | IDC distribution for Panel B2 modules |

### 5.3 Display Carrier Boards

| Board | Designator | Microcontroller | Display | Role |
|-------|------------|-----------------|---------|------|
| 5" TFT Display Carrier | KC-01-1900 | Teensy 4.0 | RA8875 800×480 TFT | Large Format Graphic Touch Display |
| 1.9" IPS Display Carrier | KC-01-1910 | XIAO RA4M1 | ST7789 320×170 IPS TFT | Small Format Graphic Display — can carry a safe switch via screw terminal |

### 5.4 IO Module Boards

| Board | Designator | Microcontroller | Button Inputs | Switch Inputs | Notes |
|-------|------------|-----------------|---------------|---------------|-------|
| Button Module | KC-01-1820 | ATtiny816 | 12 | 4 | 12× NeoPixel RGB + 4× panel switch via screw terminals |
| Joystick Module | KC-01-1830 | ATtiny816 | 2 | N/A | 2× NeoPixel RGB + JH-D202X-R4 inputs via screw terminals |
| Wide Button Module | KC-01-1840 | ATtiny816 | 12 | 8 | 12× NeoPixel RGB + 8× panel switch via DB127S-5.08-6P screw terminals |
| Dual Encoder Module | KC-01-1850 | ATtiny816 | N/A | 1* | Inputs for 2× rotary encoder carrier boards + 1× safe switch via screw terminals |
| Throttle Module | KC-01-1860 | ATtiny816 | N/A | 5 | RSA0N11M9A0J 10k motorized potentiometer + 5× panel switch via screw terminals w/ status LED output |
| 7-Segment Display Module | KC-01-1880 | ATtiny816 | 3 | N/A | MAX7219 + FJ4401AG 4-digit display; rotary encoder |

### 5.5 Test & Programming Fixtures

| Board | Designator | Function |
|-------|------------|---------|
| Universal Test Fixture | TBD | 16-pin IDC + 12V power for module bring-up & bench testing |

---

## 6. Power Distribution

### 6.1 Main Power Supply

| Parameter | Value |
|-----------|-------|
| Input voltage | 12V DC |
| Rated current | 10A |
| Typical system load | 3.125A / 37.5W |
| Peak system load | 4.71A / 56.5W |

### 6.2 Power On/Off

A soft-latching power circuit on the Ctrl Module controls the 12V main bus. A single momentary pushbutton turns power on with a short press and turns power off with a 3-second hold. The circuit uses a P-channel MOSFET (IRF9540N, D2PAK) as the load switch, with an RC network for hold-to-off timing. No microcontroller is required for power on/off — the circuit is fully analog.

### 6.3 Power Rails

| Rail | Voltage | Source | Consumers |
|------|---------|--------|-----------|
| V_12 | 12V | Main input bus | MPM3610 converters on each board, 12V direct loads (fans, motors) |
| V_5 | 5V | MPM3610 DC-DC (per board) | ATtiny816 modules, Teensy VIN, logic supply |
| V_3P3 | 3.3V | AP2112K LDO (per board, as required) | Teensy 4.0/4.1 logic, displays, XIAO modules |
| VDD33_HUBn | 3.3V | CH334F internal LDO (per hub IC) | CH334F internal logic only — isolated net, not shared |

### 6.4 Power Monitoring

An INA228 high-side current/power monitor on the Ctrl Module measures the 12V rail. It sits in series with a 15mΩ 2512 shunt resistor between the soft latch output and the 12V distribution bus. Results are reported to the master Teensy 4.1 via I2C (address 0x40) and displayed on the system status display.

### 6.5 Status LEDs

| LED | Color | LCSC Part | Rail Monitored | Series Resistor |
|-----|-------|-----------|----------------|----------------|
| Power indicator | Red | KT-0603R | 3.3V rail | 270Ω (C22966) |
| 12V indicator | Green | KT-0805G | 12V rail | 2.0kΩ (C22975) |

---

## 7. USB Architecture

### 7.1 Overview

A single USB-C cable connects the controller to the host PC. All USB traffic is carried over this connection. Inside the controller, a custom USB hub board distributes USB to six downstream devices. VBUS from the PC is completely isolated — no power flows from the host through the USB connection in either direction. All devices are powered from the controller's internal 12V bus.

### 7.2 Hub IC

| Parameter | Value |
|-----------|-------|
| Hub IC | CH334F (WCH) × 2 cascaded |
| Package | QFN24 4×4mm |
| LCSC Part Number | C5187527 |
| USB standard | USB 2.0 High Speed (480Mbps) |
| TT mode | MTT (Multi-Transaction Translator) |
| Total downstream ports | 7 (3 on Hub 1 + 4 on Hub 2) |
| Downstream ports used | 6 (Teensy 4.1 × 1, Teensy 4.0 × 4, expansion × 1) |
| Power mode | Self-powered (PSELF pin floating, internal pull-up = HIGH) |
| Crystal | 12MHz, SMD3225-4P, LCSC C9002 (basic part) |

### 7.3 VBUS Isolation

VBUS on the upstream USB-C connector is left completely unconnected at the PCB level. The CH334F V5 pin (pin 15) is powered from the internal +5V rail, not from USB VBUS. This guarantees no current path exists between the host PC power rail and the controller power supply under any condition.

> **Note:** The downstream Micro-USB connector VCC pins are also left unconnected. Downstream devices (Teensys) are powered from the controller's 5V rail via their own board regulators.

### 7.4 Upstream Connector (USB-C)

| Pin / Group | Connection | Notes |
|-------------|-----------|-------|
| D+ (A6, B6) | DMU — Hub 1 pin 13 | USB 2.0 data |
| D- (A7, B7) | DPU — Hub 1 pin 14 | USB 2.0 data |
| CC1 (A5) | 5.1kΩ → GND | Device identification resistor |
| CC2 (B5) | 5.1kΩ → GND | Device identification resistor |
| VBUS (A4B9, B1A12) | No connect | Completely isolated |
| SBU1, SBU2 | No connect | Not used |
| GND | GND | Signal return |

### 7.5 Hub Cascade

| Hub | IC | Upstream Source | Downstream Ports |
|-----|----|----------------|-----------------|
| Hub 1 | CH334F | Upstream USB-C connector | Port 1 → Teensy 4.1 (master), Port 2 → Teensy 4.0 display 1, Port 3 → Teensy 4.0 display 2, Port 4 → Hub 2 upstream |
| Hub 2 | CH334F | Hub 1 Port 4 (DP4/DM4) | Port 1 → Teensy 4.0 display 3, Port 2 → Teensy 4.0 display 4, Port 3 → Spare micro USB, Port 4 → Expansion USB-C |

### 7.6 Downstream Connectors

| Connector | Type | Device | VCC Pin |
|-----------|------|--------|---------|
| USB1 | Micro-USB-B SMD | Teensy 4.1 (master controller) | No connect |
| USB2 | Micro-USB-B SMD | Teensy 4.0 — Annunciator display | No connect |
| USB3 | Micro-USB-B SMD | Teensy 4.0 — Info Display 1 | No connect |
| USB4 | Micro-USB-B SMD | Teensy 4.0 — Info Display 2 | No connect |
| USB5 | Micro-USB-B SMD | Teensy 4.0 — Resource display | No connect |
| USB6 | Micro-USB-B SMD | Spare | No connect |
| USBC2 | USB-C (TYPE-C-31-M-12) | Expansion port — panel USB-C or future extension module | No connect |

### 7.7 Expansion USB-C (USBC2)

| Pin / Group | Connection | Notes |
|-------------|-----------|-------|
| D+ (A6, B6) | DP4 — Hub 2 pin 6 | USB 2.0 data |
| D- (A7, B7) | DM4 — Hub 2 pin 5 | USB 2.0 data |
| CC1, CC2 | No connect | Downstream-facing port — no CC resistors needed |
| VBUS | No connect | Power isolated |
| SBU1, SBU2 | No connect | Not used |
| GND | GND | Signal return |

### 7.8 PCB Stackup and Routing

| Layer | Function |
|-------|---------|
| Layer 1 (Top) | USB signal routing |
| Layer 2 | Ground plane |
| Layer 3 | 3.3V power |
| Layer 4 | Ground plane |
| Layer 5 | 12V routing |
| Layer 6 (Bottom) | Signal |

Differential pair routing: 6.5mil traces, 8mil separation, 90Ω differential impedance (verified with JLCPCB impedance calculator). All D+/D− pairs routed on the same layer as matched-length differential pairs. Vias used at the Hub 1 to Hub 2 cascade connection to reverse pin order — both D+ and D− use the same number of vias at the same location.

---

## 8. I2C Bus Architecture

### 8.1 Bus Configuration

| Parameter | Value |
|-----------|-------|
| Bus speed | 400kHz (Fast Mode) |
| Bus accelerator | LTC4311 on Ctrl Module |
| Pull-up voltage | 3.3V (master side) |
| Module voltage | 5V (ATtiny816 modules) |
| Level shifting | Passive resistor divider on each module — see §8.2 |

The main controller provides two I2C buses:

- **SDA_BUS / SCL_BUS** — primary internal bus. Master is Teensy 4.1. All ATtiny816 modules and display carriers are targets on this bus. LTC4311 bus accelerator fitted on Ctrl Module.
- **SDA_EXT / SCL_EXT** — external bus routed to the 50-pin IDC connector (§9) and thence to the expansion interface (§9.4). The expansion unit Teensy 4.1 operates as I2C master on this bus. The main controller Teensy 4.1 operates as I2C target. LTC4311 bus accelerators are fitted at both ends of the SDA_EXT/SCL_EXT cable run — see §8.4.

### 8.2 Interrupt Architecture

Each ATtiny816 module drives its interrupt line from PA1 as a push-pull digital output — there is no pull-up resistor on the module. A passive voltage divider on each module board performs 5V→3.3V level conversion: R5 (10kΩ) in series from PA1 to the `INT_BUS` node, R6 (20kΩ) from `INT_BUS` to GND. This produces a 3.3V-compatible signal on `INT_BUS` that the master controller reads directly without additional level shifting.

| PA1 State | INT_MOD | INT_BUS | Meaning |
|-----------|---------|---------|---------|
| HIGH (5V) | 5V | 3.33V | Idle — no interrupt pending |
| LOW (0V) | 0V | 0V | Asserted — module has data ready |

The master detects INT_BUS going low on the dedicated interrupt line for each module and initiates an I2C read transaction. The module deasserts (returns PA1 HIGH) when the master completes the read.

### 8.3 I2C Device Map

#### Fixed Silicon Devices

| Address | Device | Location | Function |
|---------|--------|----------|---------|
| 0x40 | INA228 | Ctrl Module | 12V rail current and power monitor |
| 0x4C | EMC2101 | Ctrl Module | Fan speed controller |

#### Programmable Modules

| Address | Device | Type | Panel |
|---------|--------|------|-------|
| 0x10 | Annunciator | Teensy 4.0 | A1 |
| 0x11 | Resource Display | Teensy 4.0 | B1 |
| 0x12 | Info Display 1 | Teensy 4.0 | A1 |
| 0x13 | Info Display 2 | Teensy 4.0 | B1 |
| 0x14 | Sys Info Display | XIAO RA4M1 | A2 |
| 0x15 | Status Indicator Display | XIAO RA4M1 | B2 |
| 0x16–0x1F | Reserved — expansion unit | — | — |
| 0x16 | Expansion Keypad Module A | Teensy 4.1 (expansion) | Expansion Unit |
| 0x17 | Expansion Keypad Module B | Teensy 4.1 (expansion) | Expansion Unit |
| 0x18–0x1F | Reserved (expansion growth) | — | — |
| 0x20 | UI Control | ATtiny816 | A2 |
| 0x21 | Function Control | ATtiny816 | A2 |
| 0x22 | Action Control | ATtiny816 | A2 |
| 0x23 | Stability Control | ATtiny816 | B2 |
| 0x24 | Vehicle Control | ATtiny816 | B2 |
| 0x25 | Time Control | ATtiny816 | B2 |
| 0x26 | Auxiliary Control | ATtiny816 | B2 |
| 0x27 | Reserved | — | — |
| 0x28 | Joystick Rotation | ATtiny816 | B2 |
| 0x29 | Joystick Translation | ATtiny816 | A2 |
| 0x2A | GPWS Input Panel | ATtiny816 | A1 |
| 0x2B | Pre-Warp Time | ATtiny816 | B2 |
| 0x2C | Throttle | ATtiny816 | A2 |
| 0x2D | Dual Encoder | ATtiny816 | B1 |
| 0x2E | Reserved | — | — |
| 0x2F | Reserved | — | — |

### 8.4 External Interface Signal Architecture

Signals routed over the external expansion cable (§9.4) require additional protection and conditioning not needed for internal IDC runs. The following requirements apply to all signals crossing the GX16 external connector interface.

**I2C — SDA_EXT / SCL_EXT**

The SDA_EXT/SCL_EXT lines run over an external cable of up to 1m (3ft). At 400kHz Fast Mode, cable capacitance and connector junctions degrade signal rise times beyond the I2C specification limit. An LTC4311 I2C bus accelerator must be fitted at each end of the cable run:

- One LTC4311 on the main controller side — on the Ctrl Module or Ctrl Ext Module, on the SDA_EXT/SCL_EXT lines before they reach the 50-pin IDC connector
- One LTC4311 on the expansion unit side — on the Expansion Routing Board, on the SDA_EXT/SCL_EXT lines after they arrive from the GX16 connector

The LTC4311 actively drives rising edges, compensating for cable capacitance and restoring Fast Mode timing compliance.

**EXT_INT_1 (Expansion → Main Controller)**

EXT_INT_1 is driven by the expansion unit Teensy 4.1 as a push-pull active-low output. A **47Ω series resistor** must be fitted on the expansion routing board between the Teensy GPIO output and the GX16 connector pin. This resistor damps cable reflections and limits fault current on accidental shorts.

The main controller receives EXT_INT_1 on a Teensy 4.1 GPIO input with internal pull-up enabled. No additional components required on the main controller side for this signal.

**EXT_INT_2 (Main Controller → Expansion)**

EXT_INT_2 is driven by the main controller Teensy 4.1 as a push-pull active-low output. A **47Ω series resistor** must be fitted on the main controller board (Ctrl Module or Ctrl Ext Module) between the Teensy GPIO output and the 50-pin IDC connector pin. This resistor damps cable reflections and limits fault current.

The expansion unit receives EXT_INT_2 on a Teensy 4.1 GPIO input with internal pull-up enabled. No additional components required on the expansion side for this signal.

**RST (Main Controller → Expansion)**

RST is an active-low reset signal driven by the main controller. A **47Ω series resistor** must be fitted on the main controller board between the RST source and the 50-pin IDC connector pin.

On the expansion unit side, the RST line connects to the Teensy 4.1 RST pin through an RC filter to prevent spurious resets from cable glitches or electrostatic discharge:

- 100Ω series resistor between the GX16 pin and the Teensy RST pin
- 100nF capacitor from the Teensy RST pin to GND

The RC filter (τ = 10µs) rejects transients shorter than ~50µs while passing legitimate reset pulses.

**Signal Protection Summary**

| Signal | Direction | Protection at Source | Protection at Receiver |
|--------|-----------|---------------------|----------------------|
| SDA_EXT / SCL_EXT | Bidirectional | LTC4311 at each end | LTC4311 at each end |
| EXT_INT_1 | Expansion → Main | 47Ω series (expansion routing board) | GPIO pull-up only |
| EXT_INT_2 | Main → Expansion | 47Ω series (main controller board) | GPIO pull-up only |
| RST | Main → Expansion | 47Ω series (main controller board) | 100Ω + 100nF RC filter (expansion routing board) |

**USB Signal Conditioning — TUSB212**

The expansion USB-C port (§7.7) feeds a 3ft external cable plus internal wiring in both enclosures, totalling approximately 6–7ft of conductor path through multiple connector junctions. A TUSB212 USB 2.0 High Speed signal conditioner must be fitted on the expansion routing board between the panel USB-C connector and the internal expansion hub (CH334F). The TUSB212 regenerates the High Speed differential signal, resetting the effective cable length budget.

| Parameter | Value |
|-----------|-------|
| IC | TUSB212RWBR (TI) |
| LCSC # | C702372 |
| Supply voltage | 3.3V |
| Placement | Expansion Routing Board, between panel USB-C and internal CH334F hub |
| Boost setting | Set per cable length — refer to TUSB212 datasheet Table 1 for resistor value |

The TUSB212 is transparent to all USB speeds — Low Speed and Full Speed signals pass through unmodified; High Speed signals are actively conditioned.

---

## 9. Interboard Connector Pinout

### 9.1 Overview

A 50-pin 2×25 IDC connector bridges between the Ctrl Module and Ctrl Ext Modules. It carries 12V power for the B1, B2, and C panel groups; the internal I2C bus; a separate external I2C bus; 12 individual interrupt lines for B-panel modules; two special safe-switch interrupts; two external interrupts; a global reset signal; and the fan PWM signal.

### 9.2 Pinout Table

| Pin (Left) | Signal (Left) | Signal (Right) | Pin (Right) |
|:----------:|:-------------:|:--------------:|:-----------:|
| 1  | FAN_PWM      | GND          | 2  |
| 3  | GND          | GND          | 4  |
| 5  | B_INT_SAFE   | B_INT_SAFE2  | 6  |
| 7  | B_INT_11     | B_INT_12     | 8  |
| 9  | B_INT_9      | B_INT_10     | 10 |
| 11 | B_INT_7      | B_INT_8      | 12 |
| 13 | B_INT_5      | B_INT_6      | 14 |
| 15 | B_INT_3      | B_INT_4      | 16 |
| 17 | B_INT_1      | B_INT_2      | 18 |
| 19 | GND          | GND          | 20 |
| 21 | SDA_EXT      | SCL_EXT      | 22 |
| 23 | EXT_INT_1    | EXT_INT_2    | 24 |
| 25 | RST          | GND          | 26 |
| 27 | SDA_BUS      | SCL_BUS      | 28 |
| 29 | GND          | GND          | 30 |
| 31 | GND          | GND          | 32 |
| 33 | GND          | GND          | 34 |
| 35 | GND          | GND          | 36 |
| 37 | GND          | GND          | 38 |
| 39 | V_12         | V_12         | 40 |
| 41 | V_12         | V_12         | 42 |
| 43 | V_12         | V_12         | 44 |
| 45 | V_12         | V_12         | 46 |
| 47 | V_12         | V_12         | 48 |
| 49 | V_12         | V_12         | 50 |

### 9.3 Signal Descriptions

| Signal | Type | Description |
|--------|------|-------------|
| V_12 | Power | 12V main bus — 12 pins (39–50), supports full system load |
| GND | Power | Ground return — 16 pins total distributed throughout connector |
| SDA_BUS / SCL_BUS | I2C | Main internal I2C bus — connects to LTC4311 accelerated bus on Ctrl Module |
| SDA_EXT / SCL_EXT | I2C | Separate external I2C bus — for panel external connector or expansion unit |
| RST | Signal | Global reset — active low, resets all Teensy-driven display modules |
| B_INT_1 to B_INT_12 | Interrupt | Individual active-low interrupt lines for each B-panel module |
| B_INT_SAFE / B_INT_SAFE2 | Interrupt | Special safe-switch interrupt lines — separate from general B_INT group |
| EXT_INT_1 / EXT_INT_2 | Interrupt | External interrupt lines — used by the expansion unit interface (§9.4). Signal protection architecture defined in §8.4 |
| FAN_PWM | PWM | Fan speed control PWM signal — driven by EMC2101 on Ctrl Module |

> **Note:** RST on pin 25 has a dedicated GND return on pin 26 — keep these routed together to minimize noise on the reset line. RST signal protection architecture defined in §8.4.

> **Note:** GND on pin 2 adjacent to FAN_PWM on pin 1 provides a clean local return for the PWM signal.

### 9.4 External Expansion Interface

The expansion unit connects to the main controller via two panel-mounted connectors on each enclosure: a GX16-10 circular aviation connector for power and signals, and a USB-C panel mount connector for USB 2.0 data.

#### GX16-10 Signal Connector

A GX16-10 (10-pin, 16mm thread) aviation connector carries 12V power, I2C, interrupt, and reset signals between the main controller and expansion unit enclosures. Both enclosure panels mount a **female socket with male pins** (standard panel-mount convention). The interconnecting cable has **female sockets at both ends**.

| Parameter | Value |
|-----------|-------|
| Connector | GX16-10 |
| Panel mount | Female socket, male pins |
| Cable ends | Female socket at both ends |
| Rated current | 5A per pin |
| Rated voltage | 125V |
| Wire gauge | 20–24 AWG |

**Pin Assignment:**

| Pin | Signal | Direction | Notes |
|-----|--------|-----------|-------|
| 1 | V_12 | Main → Expansion | 12V power |
| 2 | V_12 | Main → Expansion | Parallel — combined capacity for full expansion load |
| 3 | GND | — | Power return |
| 4 | GND | — | Power return parallel |
| 5 | SDA_EXT | Bidirectional | I2C data — LTC4311 at each end (§8.4) |
| 6 | SCL_EXT | Bidirectional | I2C clock — LTC4311 at each end (§8.4) |
| 7 | EXT_INT_1 | Expansion → Main | Active-low — 47Ω series at expansion end (§8.4) |
| 8 | EXT_INT_2 | Main → Expansion | Active-low — 47Ω series at main controller end (§8.4) |
| 9 | RST | Main → Expansion | Active-low — 47Ω series at source, RC filter at expansion end (§8.4) |
| 10 | SPARE | — | Reserved for future use |

> **Note:** V_12 pins 1 and 2 in parallel provide up to 10A combined capacity — well in excess of the expansion unit's ~1.5A @ 12V typical load.

> **Note:** All signal protection components (series resistors, LTC4311, RC filter) are on the respective board PCBs, not in the cable. The cable carries bare signals between connectors.

#### USB-C Panel Mount Connector

A USB-C panel mount bulkhead connector (female-to-female feedthrough) on each enclosure panel carries USB 2.0 High Speed data from Hub 2 Port 4 on the main controller to the expansion unit's internal CH334F hub. A standard USB-C cable connects the two panel mounts.

| Parameter | Value |
|-----------|-------|
| Connector | USB-C panel mount, female bulkhead feedthrough |
| Cable | Standard USB-C to USB-C, 3ft (1m) maximum |
| Signal conditioning | TUSB212RWBR on expansion routing board (§8.4) |
| VBUS | Not connected at either panel mount — expansion powered via GX16 V_12 |

> **Note:** The USB-C panel mount on the main controller side connects directly to USBC2 (§7.7) with no signal conditioning — the TUSB212 is placed on the expansion unit side only, after the full cable run.

---

## 10. Module Hub 34-Pin IDC Connector

### 10.1 Overview

A 34-pin 2×17 IDC connector bridges between the Ctrl Module/Ctrl Ext Module and the panel routing boards. It carries 12V power for the individual modules; the internal I2C bus; individual interrupt lines; one special safe-switch interrupt; and a global reset signal.

### 10.2 Pinout Table

| Pin (Left) | Signal (Left) | Signal (Right) | Pin (Right) |
|:----------:|:-------------:|:--------------:|:-----------:|
| 1  | V_12         | V_12         | 2  |
| 3  | V_12         | V_12         | 4  |
| 5  | V_12         | V_12         | 6  |
| 7  | V_12         | V_12         | 8  |
| 9  | GND          | GND          | 10 |
| 11 | GND          | GND          | 12 |
| 13 | GND          | GND          | 14 |
| 15 | GND          | GND          | 16 |
| 17 | SDA_BUS      | SCL_BUS      | 18 |
| 19 | RST          | GND          | 20 |
| 21 | INT_1        | INT_2        | 22 |
| 23 | INT_3        | INT_4        | 24 |
| 25 | INT_5        | INT_6        | 26 |
| 27 | INT_7        | INT_8        | 28 |
| 29 | INT_SAFE     | GND          | 30 |
| 31 | GND          | GND          | 32 |
| 33 | GND          | GND          | 34 |

### 10.3 Signal Descriptions

| Signal | Type | Description |
|--------|------|-------------|
| V_12 | Power | 12V main bus — 8 pins (1–8), supports full panel load |
| GND | Power | Ground return — 14 pins total distributed throughout connector |
| SDA_BUS / SCL_BUS | I2C | Main internal I2C bus — connects to Ctrl Module |
| RST | Signal | Global reset — active low (ATtiny816 modules: no-connect — RST used for displays only) |
| INT_1 to INT_8 | Interrupt | Individual active-low interrupt lines for each panel module |
| INT_SAFE | Interrupt | Special safe-switch interrupt line — separate from general INT group |

---

## 11. Module 16-Pin IDC Connector

### 11.1 Overview

Each IO or Display module connects to its panel routing board via a 16-pin 2×8 IDC connector. It carries 12V power for the modules; the internal I2C bus; module interrupt; a global reset signal; and a 3.3V input from the hub board to support level shifting (to allow modules to not have a 3.3V line if not otherwise required).

### 11.2 Pinout Table

| Pin (Left) | Signal (Left) | Signal (Right) | Pin (Right) |
|:----------:|:-------------:|:--------------:|:-----------:|
| 1  | V_IN (12V)   | V_IN (12V)   | 2  |
| 3  | V_IN (12V)   | V_IN (12V)   | 4  |
| 5  | GND          | GND          | 6  |
| 7  | GND          | GND          | 8  |
| 9  | V_3P3        | GND          | 10 |
| 11 | SDA_BUS      | SCL_BUS      | 12 |
| 13 | INT          | RST          | 14 |
| 15 | GND          | INT_SAFE     | 16 |

### 11.3 Signal Descriptions

| Signal | Type | Description |
|--------|------|-------------|
| V_IN | Power | 12V main bus |
| GND | Power | Ground return |
| V_3P3 | Power | 3.3V input from hub board — supports level shifting on modules that require it |
| SDA_BUS / SCL_BUS | I2C | Main internal I2C bus — connects to Ctrl Module |
| RST | Signal | Global reset — active low (ATtiny816 modules: no-connect — RST used for displays only) |
| INT | Interrupt | Individual active-low interrupt line for this module |
| INT_SAFE | Interrupt | Safe-switch interrupt line — only populated on modules that support safe switch connections (Dual Encoder or 1.9" Display) |

---

## 12. Module Conformance Requirements

This section defines the minimum hardware and firmware requirements for any module board to be a conformant member of the KCMk1 system. A conformant module can be connected to any panel routing board, addressed by the master controller, and commanded via the KCMk1 I2C Protocol Specification without special-casing.

### 12.1 Hardware Requirements

**Connector**
The module must implement the 16-pin 2×8 IDC connector with the pinout defined in Section 11. All power, ground, I2C, interrupt, and reset signals must be connected as specified. INT_SAFE is only required on modules that support a safe switch input.

**Power**
The module must accept 12V on V_IN and provide its own local regulation. The standard power architecture is:
- MPM3610 DC-DC converter: 12V → 5V (module logic and NeoPixel supply)
- AP2112K LDO: 5V → 3.3V (if 3.3V logic is required locally)

Modules must not draw power from the V_3P3 pin on the IDC connector for primary logic supply — that pin is provided for level shifting support only.

**Interrupt Line**
The INT signal must be driven by a push-pull digital output on the module microcontroller (ATtiny816 PA1). A passive voltage divider must be fitted on the module board to level-shift the 5V push-pull output to a 3.3V-compatible signal on INT_BUS:

- R5: 10kΩ in series from the microcontroller output to the INT_BUS node
- R6: 20kΩ from INT_BUS to GND
- INT_BUS connects to the IDC INT pin

No pull-up resistor is used. The divider produces 3.3V on INT_BUS when the output is HIGH (idle) and 0V when LOW (asserted).

**I2C**
The module must operate as an I2C target at 400kHz (Fast Mode). The I2C address must be assigned from the range 0x20–0x2E and must not conflict with any address in the device map (Section 8.3). The module must not include its own I2C pull-up resistors — pull-ups are provided by the master side.

### 12.2 Firmware Requirements

All conformant modules must implement the following regardless of module type.

**Identity Response**
The module must respond to `CMD_GET_IDENTITY` (0x01) with a 4-byte identity packet:

```
Byte 0:  Module Type ID  — unique value from the KCMk1 module registry
Byte 1:  Firmware version major
Byte 2:  Firmware version minor
Byte 3:  Capability flags  — KMC_CAP_* bitmask (0x00 if no optional capabilities)
```

**Universal Packet Header**
Every response packet sent by the module must begin with the 3-byte universal header:

```
Byte 0:  Status byte  — lifecycle state (bits 1:0), fault flag (bit 2), data changed (bit 3)
Byte 1:  Module Type ID
Byte 2:  Transaction counter — uint8, increments on every INT assertion, wraps 255→0
```

Module-specific payload follows immediately after the header. The controller always reads header + payload as a single transaction.

**Lifecycle State Machine**
The module must implement the full four-state lifecycle:

| State | Value | Entry Condition | Behavior |
|-------|-------|----------------|---------|
| BOOT_READY | 0x03 | Power-on | Assert INT immediately. Hold until master reads and sends CMD_DISABLE |
| DISABLED | 0x02 | CMD_DISABLE received | All outputs off. Inputs suppressed. State reset to defaults |
| ACTIVE | 0x00 | CMD_ENABLE received | Normal operation. Inputs processed. Outputs driven |
| SLEEPING | 0x01 | CMD_SLEEP received | State frozen exactly as-is. No visual change. INT suppressed |

**Base Command Set**
The module must handle all base commands:

| Command | Value | Required Behavior |
|---------|-------|------------------|
| `CMD_GET_IDENTITY` | 0x01 | Return 4-byte identity packet |
| `CMD_SET_LED_STATE` | 0x02 | Apply nibble-packed LED state to outputs |
| `CMD_SET_BRIGHTNESS` | 0x03 | Set ENABLED state backlight brightness |
| `CMD_BULB_TEST` | 0x04 | 0x01: all outputs full white/on. 0x00: stop. Fires in any lifecycle state |
| `CMD_SLEEP` | 0x05 | Transition to SLEEPING — freeze state, suppress INT |
| `CMD_WAKE` | 0x06 | Transition to ACTIVE — resume, send current state packet |
| `CMD_RESET` | 0x07 | Reset outputs and state to defaults, remain ACTIVE |
| `CMD_ACK_FAULT` | 0x08 | Clear fault flag in status byte |
| `CMD_ENABLE` | 0x09 | Transition to ACTIVE |
| `CMD_DISABLE` | 0x0A | Transition to DISABLED |

Unrecognised command bytes must be silently ignored — no error response, no state change.

**INT Assertion**
The module must assert INT (drive PA1 LOW) when it has a state change ready for the master to read. INT must be deasserted (PA1 HIGH) when the master completes the read transaction. The transaction counter must increment on every INT assertion.

INT must not be asserted while the module is in SLEEPING or DISABLED state, except for the initial BOOT_READY assertion at power-on.

### 12.3 Optional Capabilities

Optional capabilities are declared in the capability flags byte of the identity response. The master uses these flags to determine what additional data is present in the response payload.

| Flag | Value | Capability | Additional Payload |
|------|-------|------------|-------------------|
| `KMC_CAP_EXTENDED_STATES` | 0x01 | Supports WARNING/ALERT/ARMED/PARTIAL_DEPLOY LED states | None — affects CMD_SET_LED_STATE interpretation |
| `KMC_CAP_FAULT` | 0x02 | Can report hardware faults | Fault flag set in status byte; cleared by CMD_ACK_FAULT |
| `KMC_CAP_ENCODERS` | 0x04 | Encoder delta present in response | Encoder delta bytes in payload |
| `KMC_CAP_JOYSTICK` | 0x08 | Analog joystick axes present | Axis value bytes in payload |
| `KMC_CAP_DISPLAY` | 0x10 | 7-segment display and encoder present | Value bytes in payload; accepts CMD_SET_VALUE (0x0D) |
| `KMC_CAP_MOTORIZED` | 0x20 | Motorized position control | Accepts CMD_SET_THROTTLE (0x0B), CMD_SET_PRECISION (0x0C) |

Capability flags not listed above are reserved and must be set to 0.

### 12.4 Conformance Summary

| Requirement | Mandatory | Reference |
|-------------|-----------|---------|
| 16-pin IDC connector per Section 11 pinout | Yes | §11 |
| 12V input, local MPM3610 + AP2112K regulation | Yes | §6.3 |
| Push-pull INT with R5/R6 voltage divider | Yes | §8.2 |
| I2C target at 400kHz, address 0x20–0x2E | Yes | §8.1, §8.3 |
| CMD_GET_IDENTITY response | Yes | §12.2 |
| 3-byte universal header on all response packets | Yes | §12.2 |
| Full lifecycle state machine (BOOT_READY/DISABLED/ACTIVE/SLEEPING) | Yes | §12.2 |
| All base commands (CMD_ENABLE through CMD_DISABLE) | Yes | §12.2 |
| Transaction counter increment on INT assertion | Yes | §12.2 |
| Capability flags declared accurately in identity response | Yes | §12.3 |
| Optional capability payload per declared flags | If flag set | §12.3 |

---

## 13. PCB Design List

| Board | Designator | Type | Status | Fab | Notes |
|-------|------------|------|--------|-----|-------|
| Button Module Base | KC-01-1821 | ATtiny816 module | Schematic complete | JLCPCB | 12-button, 12× NeoPixel + 4× discrete LED; ready for layout |
| Wide Button Module | KC-01-1842 | ATtiny816 module | Schematic complete | JLCPCB | 12× NeoPixel + 8× panel switch via DB127S-5.08-6P screw terminals (B16–B23); ready for layout |
| Joystick Module | KC-01-1831 | ATtiny816 module | Planned | JLCPCB | Hall-effect joystick, 2× NeoPixel |
| Dual Encoder Module | KC-01-1851 | ATtiny816 module | Planned | JLCPCB | 2× rotary encoder inputs, safe switch |
| Throttle Module | KC-01-1861 | ATtiny816 module | Planned | JLCPCB | Motorized pot, L293D H-bridge |
| 7-Segment Display Module | KC-01-1881 | ATtiny816 module | Planned | JLCPCB | MAX7219, 4-digit display, rotary encoder |
| 5" TFT Display Carrier | KC-01-1900 | Teensy 4.0 carrier | Planned | JLCPCB | RA8875, GSL1680F touch |
| 1.9" IPS Display Carrier | KC-01-1910 | XIAO RA4M1 carrier | Planned | JLCPCB | ST7789, safe switch screw terminal |
| USB Hub Board | TBD | Custom PCB | Schematic complete, layout complete | JLCPCB | 2× CH334F, 6-layer stackup |
| Ctrl Module | KC-01-1800 | Master controller | Planned | JLCPCB | Teensy 4.1, soft latch, INA228, EMC2101, LTC4311, USB hub. Carries 47Ω series resistors on EXT_INT_2 and RST before 50-pin IDC |
| Ctrl Ext Module | KC-01-1820 | Distribution board | Planned | JLCPCB | IDC distribution, extension support |
| A1 Panel Routing Board | TBD | Routing board | Planned | JLCPCB | IDC distribution |
| A2 Panel Routing Board | TBD | Routing board | Planned | JLCPCB | IDC distribution |
| B1 Panel Routing Board | TBD | Routing board | Planned | JLCPCB | IDC distribution |
| B2 Panel Routing Board | TBD | Routing board | Planned | JLCPCB | IDC distribution |
| Expansion Routing Board | KC-01-EX10 | Expansion interface | Planned | JLCPCB | GX16-10 panel mount, USB-C panel mount, TUSB212 USB conditioner, LTC4311 on SDA_EXT/SCL_EXT, 47Ω series on EXT_INT_1, RST RC filter (100Ω + 100nF), CH334F internal hub, MPM3610 + AP2112K power regulation |
| Expansion Keyboard Matrix | KC-01-EX21 | Keyboard PCB | Schematic complete | JLCPCB | 25× Kailh Choc V1 (hand solder), 25× SK6812MINI-EA, 25× 1N4148W, 25× 100nF; 21mm grid, 147×100mm, 2×10P headers top and bottom |
| Expansion Encoder PCB | KC-01-EX22 | Encoder PCB | Planned | JLCPCB | PEC11R-4220F-S0024 rotary encoder; independently height-adjustable |
| Expansion Teensy Carrier | KC-01-EX23 | Teensy 4.1 carrier | Planned | JLCPCB | Teensy 4.1, I2C master (local bus to RP2040s), I2C target (SDA_EXT), EXT_INT_1 output, EXT_INT_2 input, matrix scan, NeoPixel data |
| Expansion Display Carrier A | KC-01-EX30 | RP2040 display carrier | Planned | JLCPCB | RP2040, SSD1963, 5" TFT, capacitive touch controller; I2C target on local bus |
| Expansion Display Carrier B | KC-01-EX31 | RP2040 display carrier | Planned | JLCPCB | RP2040, SSD1963, 5" TFT, capacitive touch controller; I2C target on local bus |

---

## 14. Related Documents

| Document | Location | Contents |
|----------|----------|---------|
| I2C Protocol Specification | `docs/developer/I2C_Protocol_Specification.md` | v2.2 — packet formats for all module types, addresses, command/response structure, lifecycle state machine |
| Module UI Reference | `docs/developer/Module_UI_Reference.md` | v5.1 — per-module button assignments, switch wiring, CAG table, axis mappings, firmware implementation detail |
| Power Budget | `docs/developer/Power_Budget.md` | Per-module and per-panel power consumption, supply headroom analysis |
| Expansion Module Specification | `docs/developer/Expansion_Module_Spec.md` | v0.1 — expansion unit architecture, board topology, GX16 interface, keyboard subsystem, display subsystem |
| Library READMEs | Per library in source tree | KerbalButtonCore, KerbalJoystickCore, Kerbal7SegmentCore, KerbalModuleCommon, KerbalDisplayCommon, KerbalDisplayAudio |
