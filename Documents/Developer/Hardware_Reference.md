# Kerbal Controller Mk1 — Hardware Reference

**Organization:** Jeb's Controller Works  
**Author:** J. Rostoker  
**Version:** 1.0  
**Date:** May 2026  
**Document type:** Developer — Hardware

---

## Revision History

| Version | Date | Description |
|---------|------|-------------|
| 1.0 | 2026-05-19 | Initial release — system architecture, board topology, power distribution, USB hub, interboard connector pinout, PCB design list |

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
10. [Module 16-Pin IDC Connector](#10-module-16-pin-idc-connector)
11. [PCB Design List](#11-pcb-design-list)
12. [Related Documents](#12-related-documents)

---

## 1. Overview

The Kerbal Controller Mk1 (KCMk1) is a custom multi-panel hardware controller for Kerbal Space Program (KSP), communicating with the game via the KerbalSimpit v2.4.0 plugin. The controller consists of four physical panels (A1, A2, B1, B2) plus a planned C extension, each containing multiple input and display modules interconnected via I2C and distributed power.

This document is the primary hardware reference for developers designing, building, or modifying the controller. It covers system architecture, board topology, power distribution, USB architecture, interboard connectors, and PCB inventory.

---

## 2. Required KSP Mods

| Mod | Version | Purpose |
|-----|---------|---------|
| KerbalSimpit | v2.4.0 | Serial communication between KSP and the master controller |
| AGX (Action Groups Extended) | Current | Expands custom action groups to 250 (required for full button mapping) |
| Hullcam VDS Continued | Current | Camera cycling support for ENC2 encoder |

---

## 3. System Architecture

### 3.1 Top-Level Signal Flow

A single USB cable connects the controller to the host PC. Inside the controller, a two-chip USB hub (2× CH334F cascaded) distributes USB to all Teensy-based microcontrollers. A single I2C bus connects the master Teensy 4.1 to all ATtiny816 input/output modules. A 12V main bus provides power throughout, with local DC-DC conversion on each board.

### 3.2 Master Controller

The master controller is a Teensy 4.1 mounted on the Ctrl Module PCB. It is responsible for:

- Polling all I2C module slaves on the main SDA/SCL bus
- Interpreting button/axis data and translating to KSP commands via KerbalSimpit over USB
- Driving fan speed control (PWM via EMC2302)
- Monitoring system power via INA228 current/power sensor
- Monitoring system temperature via MCP9808
- Providing I2C bus acceleration via LTC4311
- Driving a local ST7789 status display

### 3.3 Display Subsystem

Four Teensy 4.0 microcontrollers each drive an RA8875 800×480 TFT display via SPI. Each Teensy 4.0 connects to the host PC via the USB hub as an independent USB device running KerbalSimpit. The four display roles are: Annunciator (Caution & Warning panel), Resource Display, Info Display 1, and Info Display 2.

### 3.4 Module Architecture

Up to 15 ATtiny816-based I2C slave modules provide button input, display output, joystick, and encoder functions. Each module communicates over the shared I2C bus using the KBC protocol. Modules assert an active-low open-drain interrupt line when state changes, allowing the master to poll only when needed rather than continuously scanning.

> **Note:** Level shifting from 3.3V (master) to 5V (modules) is handled on the master board only. Module boards have no level shifting components.

---

## 4. Panel Layout

| Panel | Location | Primary Function | Hub Board |
|-------|----------|-----------------|-----------|
| A1 | Left panel | Caution & Warning display, GPWS, primary flight instruments | A1 Panel Routing Board |
| A2 | Center-left panel | Action controls, UI controls, function controls, throttle, translation | A2 Panel Routing Board |
| B1 | Center-right panel | Stability, staging, time controls, rotation, vehicle controls | B1 Panel Routing Board |
| B2 | Right panel | Secondary displays, dual encoder, auxiliary controls | B2 Panel Routing Board |
| C (planned) | Extension unit | Additional displays and button panels — future expansion | TBD |

---

## 5. Board Topology

### 5.1 Central Boards

| Board | PCB Designator | Microcontroller | Function |
|-------|---------------|----------------|---------|
| Ctrl Module | TBD | Teensy 4.1 | Master controller, soft latch power, INA228, EMC2302, MCP9808, LTC4311, ST7789 status display |
| Ctrl Ext Module | TBD | XIAO RP2040 | B1/B2 IDC distribution, audio module, MCP9808 temperature sensor |
| USB Hub Board | TBD | None (passive) | 2× CH334F cascaded, single upstream USB-C, 6 downstream Micro-USB + 1 expansion USB-C |

### 5.2 Display Carrier Boards

| Board | Microcontroller | Display | Role |
|-------|----------------|---------|------|
| Teensy 4.0 Display Carrier (×4) | Teensy 4.0 | RA8875 800×480 TFT | One shared PCB design; four builds for Annunciator, Resource, Info 1, Info 2 |
| XIAO RA4M1 Round Display Carrier | XIAO RA4M1 | Round display (TBD) | B2 round display module |

### 5.3 Panel Routing Boards

| Board | Function | Special Features |
|-------|---------|-----------------|
| A1 Panel Routing Board | IDC distribution for Panel A1 modules | Abort button circuit |
| A2 Panel Routing Board | IDC distribution for Panel A2 modules | — |
| B1 Panel Routing Board | IDC distribution for Panel B1 modules | Stage button circuit |
| B2 Panel Routing Board | IDC distribution for Panel B2 modules | — |

### 5.4 Module Boards

| Board | Designator | Type | Buttons | Notes |
|-------|-----------|------|---------|-------|
| Button Module Base | KC-01-1821 v1.2 | ATtiny816 | 16 | 12× NeoPixel RGB + 4× discrete LED; schematic reviewed |
| Wide Button Module | KC-01-1841 | ATtiny816 | 24 | 16× NeoPixel RGB + 8× panel switch via screw terminals; schematic reviewed |
| 7-Segment Display Module | KC-01-1882 | ATtiny816 | — | MAX7219 + FJ4401AG 4-digit display + rotary encoder |
| Dev/Test Board | KC-01-9101 | ATtiny816 | — | Firmware validation; blink and NeoPixel confirmed working |

### 5.5 Test & Programming Fixtures

| Board | Function |
|-------|---------|
| Universal Test Fixture | 16-pin IDC + I2C breakout + 12V power for module bring-up |
| UPDI Programming Jig | Multi-channel ATtiny816 programmer |

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

A soft-latching power circuit on the Ctrl Module controls the 12V main bus. A single momentary pushbutton turns power on with a short press and turns power off with a 3-second hold. The circuit uses a P-channel MOSFET (IRF9540N, D2PAK) as the load switch, with a TLC555 timer providing the hold-to-off timing. No microcontroller is required for power on/off — the circuit is fully analog.

### 6.3 Power Rails

| Rail | Voltage | Source | Consumers |
|------|---------|--------|-----------|
| V_12 | 12V | Main input bus | MPM3610 converters on each board, 12V direct loads (fans, motors) |
| V_5 | 5V | MPM3610 DC-DC (per board) | ATtiny816 modules, Teensy VIN, logic supply |
| V_3P3 | 3.3V | AP2112K LDO (per board) | Teensy 4.0/4.1 logic, displays, XIAO modules |
| VDD33_HUBn | 3.3V | CH334F internal LDO (per hub IC) | CH334F internal logic only — isolated net, not shared |

### 6.4 Power Monitoring

An INA228 high-side current/power monitor on the Ctrl Module measures the 12V rail. It sits in series with a 20mΩ 2512 shunt resistor between the soft latch output and the 12V distribution bus. Results are reported to the master Teensy 4.1 via I2C (address 0x40) and displayed on the ST7789 status display.

### 6.5 Status LEDs

| LED | Color | LCSC Part | Rail Monitored | Series Resistor |
|-----|-------|-----------|---------------|----------------|
| Power indicator | Red | C2286 (KT-0603R) | 3.3V rail | 270Ω (C22966) |
| 12V indicator | Green | C2297 (KT-0805G) | 12V rail | 2.0kΩ (C22975) |
| 12V indicator 2 | Yellow | C2296 (KT-0805Y) | 12V rail | 2.0kΩ (C22975) |

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
| Hub 1 (U11) | CH334F | Upstream USB-C connector | Port 1 → Teensy 4.1 (master), Port 2 → Teensy 4.0 display 1, Port 3 → Teensy 4.0 display 2, Port 4 → Hub 2 upstream |
| Hub 2 (U12) | CH334F | Hub 1 Port 4 (DP4/DM4) | Port 1 → Teensy 4.0 display 3, Port 2 → Teensy 4.0 display 4, Port 3 → Expansion USB-C, Port 4 → No connect |

### 7.6 Downstream Connectors

| Connector | Type | Device | VCC Pin |
|-----------|------|--------|---------|
| USB1 | Micro-USB-B SMD | Teensy 4.1 (master controller) | No connect |
| USB2 | Micro-USB-B SMD | Teensy 4.0 — Annunciator display | No connect |
| USB3 | Micro-USB-B SMD | Teensy 4.0 — Resource display | No connect |
| USB4 | Micro-USB-B SMD | Teensy 4.0 — Info Display 1 | No connect |
| USB5 | Micro-USB-B SMD | Teensy 4.0 — Info Display 2 | No connect |
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
| Level shifting | On master board only — BSS138 MOSFETs or equivalent |

### 8.2 Interrupt Architecture

Each module has a dedicated active-low open-drain INT line. Pull-up: 4.7kΩ to VCC on the module board. Indicator LED: 1kΩ from VCC to INT net (active low, ~3–4mA sink when asserted). Series 47Ω resistor at the connector for long wire run protection. The master asserts INT low using `OUTPUT/LOW` and releases using `INPUT` (high-Z).

### 8.3 Ctrl Module I2C Device Map

| Device | Address | Function |
|--------|---------|---------|
| LTC4311 | N/A (not a slave) | Bus accelerator — active termination, no I2C address |
| EMC2302 | 0x2F | Dual fan controller (FAN_PWM) |
| MCP9808 | 0x18 | Ambient temperature sensor (Ctrl Module) |
| MCP9808 | 0x19 | Ambient temperature sensor (Ctrl Ext Module) |
| INA228 | 0x40 | 12V rail current and power monitor |
| ATtiny816 modules | 0x10–0x32 | All 15 input/output modules (via shared bus) |

---

## 9. Interboard Connector Pinout

### 9.1 Overview

A 50-pin 2×25 IDC connector bridges between the Ctrl Module/Ctrl Ext Module and the panel routing boards. It carries 12V power for the B1, B2, and C panel groups; the internal I2C bus; a separate external I2C bus; 12 individual interrupt lines for B-panel modules; two special safe-switch interrupts; two external interrupts; a global reset signal; and the fan PWM signal.

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
| B_INT_1 to B_INT_12 | Interrupt | Individual active-low interrupt lines for each B-panel ATtiny816 module |
| B_INT_SAFE / B_INT_SAFE2 | Interrupt | Special safe-switch interrupt lines — separate from general B_INT group |
| EXT_INT_1 / EXT_INT_2 | Interrupt | External interrupt lines — for expansion module or external panel connections |
| FAN_PWM | PWM | Fan speed control PWM signal — driven by EMC2302 on Ctrl Module |

> **Note:** RST on pin 25 has a dedicated GND return on pin 26 — keep these routed together to minimize noise on the reset line.

> **Note:** GND on pin 2 adjacent to FAN_PWM on pin 1 provides a clean local return for the PWM signal.

---

## 10. Module 16-Pin IDC Connector

Each ATtiny816 module connects to its panel routing board via a 16-pin 2×8 IDC connector with the following standard pinout:

| Pin | Signal | Notes |
|-----|--------|-------|
| 1–4 | V_IN (12V) | 12V power from panel bus |
| 5–8 | GND | Ground |
| 9 | V_3P3 | 3.3V from AP2112K LDO on hub board — module logic supply |
| 10 | GND | Ground |
| 11 | SDA_BUS | I2C data |
| 12 | SCL_BUS | I2C clock |
| 13 | INT_n | Module-specific interrupt line (routes to individual pin on 50-pin IDC) |
| 14 | RST | Reset signal (ATtiny816 modules: no-connect — RST used for display Teensys only) |
| 15 | GND | Ground |
| 16 | NC | No connect |

---

## 11. PCB Design List

| Board | Type | Status | Fabricator | Notes |
|-------|------|--------|-----------|-------|
| KC-01-9101 Dev/Test Board | ATtiny816 dev | Complete — validated | JLCPCB | Blink and NeoPixel firmware confirmed |
| KC-01-1821 Button Module Base v1.2 | ATtiny816 module | Schematic complete | JLCPCB | 16-button, 12× NeoPixel + 4× discrete LED; ready for layout |
| KC-01-1841 Wide Button Module | ATtiny816 module | Schematic complete | JLCPCB | 24-button, 16× NeoPixel + 8× screw terminal switches; ready for layout |
| KC-01-1882 7-Segment Display Module | ATtiny816 module | Complete | JLCPCB | MAX7219 + 4-digit 7-seg + rotary encoder |
| USB Hub Board | Custom PCB | Schematic complete, layout complete | JLCPCB | 2× CH334F, 6-layer stackup |
| Ctrl Module | Custom PCB | In design | JLCPCB | Teensy 4.1, soft latch, INA228, EMC2302, MCP9808, LTC4311, ST7789 |
| Ctrl Ext Module | Custom PCB | In design | JLCPCB | XIAO RP2040, audio, MCP9808, B1/B2 IDC |
| Teensy 4.0 Display Carrier (×4) | Custom PCB | In design | JLCPCB | One design, four builds |
| XIAO RA4M1 Round Display Carrier | Custom PCB | Planned | JLCPCB | B2 round display |
| A1 Panel Routing Board | Custom PCB | Planned | JLCPCB | IDC distribution + Abort circuit |
| A2 Panel Routing Board | Custom PCB | Planned | JLCPCB | IDC distribution |
| B1 Panel Routing Board | Custom PCB | Planned | JLCPCB | IDC distribution + Stage circuit |
| B2 Panel Routing Board | Custom PCB | Planned | JLCPCB | IDC distribution |
| Universal Test Fixture | Custom PCB | Planned | JLCPCB | 16-pin IDC + I2C breakout + 12V power |
| UPDI Programming Jig | Custom PCB | Planned | JLCPCB | Multi-channel ATtiny816 programmer |

---

## 12. Related Documents

| Document | Version | Audience | Contents |
|----------|---------|----------|---------|
| I2C Protocol Specification | v2.1 | Firmware developers | Packet formats for all 15 module types; addresses; command/response structure |
| Module Interface Reference | v1.9 | Firmware developers | Per-module firmware details, pin assignments, I2C addresses, module READMEs |
| Panel Interface Guide | Current | Firmware/Software developers | CAG assignment table, axis mappings, switch label definitions |
| Library READMEs | Per library | Firmware developers | KerbalButtonCore, KerbalJoystickCore, Kerbal7SegmentCore, KerbalModuleCommon, KerbalDisplayCommon, KerbalDisplayAudio |
| KCMk1 Power Budget | Current | Hardware developers | Per-module and per-panel power consumption; supply headroom analysis |
