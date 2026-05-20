# Kerbal Controller Mk1 — Hardware Reference

**Organization:** Jeb's Controller Works  
**Author:** J. Rostoker  
**Version:** 1.1  
**Date:** May 2026  
**Document type:** Developer — Hardware

---

## Revision History

| Version | Date | Description |
|---------|------|-------------|
| 1.0 | 2026-05-19 | Initial release — system architecture, board topology, power distribution, USB hub, interboard connector pinout, PCB design list |
| 1.1 | 2026-05-19 | Corrected interrupt architecture (push-pull output + passive voltage divider, not open-drain + pull-up). Corrected I2C device map — removed LTC4311 (not a device), corrected EMC2101 address to 0x4C, removed MCP9808 (not in design). Split device map into fixed silicon and programmable modules. Added complete verified I2C address map for all display carriers and ATtiny816 modules. Updated related documents table. |

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
10. [Module Hub 34-Pin IDC Connector](#10-module-hub-34-pin-idc-connector)
11. [Module 16-Pin IDC Connector](#11-module-16-pin-idc-connector)
12. [PCB Design List](#12-pcb-design-list)
13. [Related Documents](#13-related-documents)

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
| Ctrl Module | KC-01-1800 | Teensy 4.1 | Master controller, soft latch power, INA228, EMC2101, Main Bus LTC4311, ST7789 status display, USB Hub [2× CH334F cascaded, single upstream USB-C, 6 downstream Micro-USB + 1 expansion USB-C] |
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
| 0x16–0x1F | Reserved | — | — |
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
| EXT_INT_1 / EXT_INT_2 | Interrupt | External interrupt lines — for expansion module or external panel connections |
| FAN_PWM | PWM | Fan speed control PWM signal — driven by EMC2101 on Ctrl Module |

> **Note:** RST on pin 25 has a dedicated GND return on pin 26 — keep these routed together to minimize noise on the reset line.

> **Note:** GND on pin 2 adjacent to FAN_PWM on pin 1 provides a clean local return for the PWM signal.

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

## 12. PCB Design List

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
| Ctrl Module | KC-01-1800 | Master controller | Planned | JLCPCB | Teensy 4.1, soft latch, INA228, EMC2101, LTC4311, USB hub |
| Ctrl Ext Module | KC-01-1820 | Distribution board | Planned | JLCPCB | IDC distribution, extension support |
| A1 Panel Routing Board | TBD | Routing board | Planned | JLCPCB | IDC distribution |
| A2 Panel Routing Board | TBD | Routing board | Planned | JLCPCB | IDC distribution |
| B1 Panel Routing Board | TBD | Routing board | Planned | JLCPCB | IDC distribution |
| B2 Panel Routing Board | TBD | Routing board | Planned | JLCPCB | IDC distribution |

---

## 13. Related Documents

| Document | Location | Contents |
|----------|----------|---------|
| I2C Protocol Specification | `docs/developer/I2C_Protocol_Specification.md` | v2.2 — packet formats for all module types, addresses, command/response structure, lifecycle state machine |
| Module UI Reference | `docs/developer/Module_UI_Reference.md` | v5.1 — per-module button assignments, switch wiring, CAG table, axis mappings, firmware implementation detail |
| Power Budget | `docs/developer/Power_Budget.md` | Per-module and per-panel power consumption, supply headroom analysis |
| Library READMEs | Per library in source tree | KerbalButtonCore, KerbalJoystickCore, Kerbal7SegmentCore, KerbalModuleCommon, KerbalDisplayCommon, KerbalDisplayAudio |
