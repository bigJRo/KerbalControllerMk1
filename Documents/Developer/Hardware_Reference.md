# Kerbal Controller Mk1 — Hardware Reference

**Organization:** Jeb's Controller Works  
**Author:** J. Rostoker  
**Version:** 2.0  
**Date:** 2026-10-26  
**Document type:** Developer — Hardware

---

## Revision History

| Version | Date | Description |
|---------|------|-------------|
| 1.0 | 2026-05-19 | Initial release — system architecture, board topology, power distribution, USB hub, interboard connector pinout, PCB design list |
| 1.1 | 2026-05-19 | Corrected interrupt architecture (push-pull output + passive voltage divider, not open-drain + pull-up). Corrected I2C device map — removed LTC4311 (not a device), corrected EMC2101 address to 0x4C, removed MCP9808 (not in design). Split device map into fixed silicon and programmable modules. Added complete verified I2C address map for all display carriers and ATtiny816 modules. Updated related documents table. |
| 1.2 | 2026-05-19 | Added Section 12 — Module Conformance Requirements. Defines hardware (connector, power, interrupt line, I2C) and firmware (identity response, universal header, lifecycle state machine, base command set, INT assertion) requirements for any conformant KCMk1 module. PCB Design List renumbered to Section 13, Related Documents to Section 14. |
| 1.3 | 2026-05-23 | Added Section 9.4 — External Expansion Interface (GX16-10 signal connector + USB-C panel mount). Added §8.4 — External Interface Signal Architecture. Updated §8.1, §8.3, §9.3, §5.1, §13. |
| 1.4 | 2026-05-23 | Corrected §8.4 signal architecture throughout. LTC4311 relocated to Ctrl Module only. Physical pull-up resistors added. EXT_INT_2 receiving-end clarified. Full signal path documented. Ctrl Ext Module role clarified. §8.3 corrected — expansion keypad modules removed (direct GPIO matrix). §9.4 promoted to standalone Section 10 (External Expansion Interface) with new §10.4 Internal Wiring. Sections 10–14 renumbered to 11–15. §13.4 conformance table cross-references updated. |
| 1.5 | 2026-05-24 | Corrected all board designators throughout to match schematic/board numbering convention (odd = schematic, even = board PCB). §5.1: Ctrl Module KC-01-1702, Ctrl Ext Module KC-01-1712. §5.2: Panel Routing Boards consolidated to Display Hub KC-01-1722 and Panel Hub KC-01-1732 (replacing four TBD entries). §5.3: 5" TFT Display Carrier KC-01-1912, 1.9" IPS Display Carrier KC-01-1902. §5.4: Button Module KC-01-1802, Wide Button Module KC-01-1812, Joystick Module KC-01-1832, Dual Encoder KC-01-1862, Throttle Module KC-01-1822, 7-Segment Display Module KC-01-1842. §5.5 renamed to Encoder Carrier Board (KC-01-1852). §5.6 added: Module Tester KC-01-9002 (replacing Universal Test Fixture TBD). §14 PCB Design List updated with all correct designators; Encoder KC-01-1852 and Module Tester KC-01-9002 added; USB Hub Board TBD entry removed (hub is integrated on Ctrl Module KC-01-1702); Expansion board entries removed (covered in Expansion Module Spec). Throttle Module notes updated to reflect MPM3610 9V buck replacing CJ7809 LDO. |
| 1.6 | 2026-05-25 | Cleaned up references and reorganized. |
| 1.7 | 2026-06-06 | Main display carriers (KC-01-1912) changed from Teensy 4.0 + RA8875 800×480 SPI to Teensy 4.1 + LT7683 (RA8876-register-compatible) 1024×600 7" IPS, 8080 16-bit parallel (ER-TFT070A2-6-5633 module). All four carriers (Annunciator, Info 1, Info 2, Resource) standardized on this combo. Display VDD on 5V rail, VDDIO 3.3V (bench-confirmed) — Teensy 4.1 interfaces directly, no level shifters; the former SPI RA8875 MISO buffer (SN74LVC1G125) is not required on the parallel bus. On-module 128 Mbit display RAM (W9812G6JH) enables 1024×600 double-buffering. Per-carrier local MPM3610 confirmed adequate (~760 mA peak vs 1.2 A). §13.1 interrupt conformance generalized to cover 5V (ATtiny816, push-pull + 10k/20k divider) and 3.3V (Teensy carrier, push-pull direct, no divider) module classes; INT remains active-low push-pull, driven low to assert, no pull-up. Updated §3.1, §3.3, §5.3, §6.3, §7.2, §7.5, §7.6, §8.3, §13.1, §13.4, §14. |
| 1.8 | 2026-06-27 | Enclosure panel redesign. A1 reduced to power button + two 7" displays (Annunciator + configurable Info Display); GPWS Input Panel (0x2A) relocated A1→A2; Abort button relocated B1→A2; staging button and control mode select switch confirmed on B2. Small display carrier (KC-01-1902) migrated from XIAO RA4M1 + 1.9" 320×170 to XIAO RP2350 + 2.0" 240×320 (ER-TFTM020-2) with shared-SPI microSD socket; Sys Info Display (0x14) now on this carrier, Panel A2. Status Indicator Display (0x15, XIAO RA4M1, B2) removed from design — 0x15 folded into the expansion reserved block (0x15–0x1F), and the previously overlapping 0x16–0x1F / 0x18–0x1F rows consolidated into a single range. Updated §3.1, §3.4, §4, §5.3, §8.3. |
| 1.9 | 2026-08-03 | Documentation audit reconciliation. §6.1 system-load figures pointed to Power Budget as authoritative (interim placeholders retained) since the RP2350 Sys Info Display draw is not yet measured. §8.3 device map: clarified 0x2E (Switch Panel — hardware TBD) and 0x2F (was Indicator Module — removed) to match the Module UI Reference. §14 Related Documents: corrected cross-referenced versions (I2C Protocol v2.4→v2.6, Module UI v5.3→v5.4, Power Budget v1.1→v1.2), added the Ascent Autopilot Interface row, and corrected the `docs/developer/` paths to `Documents/Developer/`. Chip naming standardized to LT7683 (RA8876-compatible) in §14. |
| 1.10 | 2026-08-08 | §5.4: corrected the Throttle Module board designator from KC-01-1822 to KC-01-1872, matching the released drawing set (KC-01-1870 assembly / KC-01-1871 schematic / KC-01-1872 board / KC-01-1873 BOM / KC-01-1874 placement). Throttle row note updated: the five illuminated panel switches share a single backlight-enable output (not per-switch LEDs). §14 Related Documents: bumped the I2C Protocol Specification cross-reference to v2.10 (the two parallel v2.6/v2.9 spec lineages were reconciled into a single v2.10 document). |
| 2.0 | 2026-10-26 | Rebuilt from `Hardware_Inventory.md` (the V2.0 schematics, confirmed panel by panel) and `Master_Firmware_Design.md`; every difference in Inventory §6 resolved. **Boards:** Display Hub (KC-01-1721/1722) retired — on A1 and B1 one 7" TFT Display Driver (KC-01-1911/1912) is the panel hub (Info Display 1 → Ctrl Module CN4; Resource Display → Extension CN5); Panel Hub (KC-01-1731/1732) on A2 (Ctrl CN3) and B2 (Extension CN4). Schematic and PCB numbers both listed. "Carrier" boards renamed to "Display Driver"; Ctrl Ext Module renamed Ctrl Module Extension. Removed the Ctrl Module ST7789 status display (not fitted; SPI pins unconnected). 2" display driver has no safe-switch terminal. Dual Encoder safe-switch terminal cannot reach the master. Throttle THRTL_ENA terminal unwired. **§3:** new §3.6 board interconnection. **§4:** rewritten per panel (slots, INT lines, safe switches, mode selector, Switch Groups 1 / 2 wiring). **§6:** power button has no Teensy pin; INA228 data shown on the Sys Info Display; new §6.6 fan and temperature (EMC2101 internal sensor only, nothing fitted on H4; fan header pin order). **§8:** I2C level shifter (not a divider) on ATtiny modules; safe lines documented as active-high; device map gains board / slot / INT columns, 0x2E marked retired; EXT_INT protection corrected (47 Ω + 4.7 kΩ both on EXT_INT_1 at the Ctrl Module, nothing on EXT_INT_2); new §8.5 master pin map from `KCMk1_Master/config.h`. **§9:** 50-pin pinout corrected to the V2.0 schematics. **§10:** Extension CN2 expansion terminal pinout added. **§11:** renamed Panel 34-Pin IDC Connector; line mapping and display-driver use of the four lines added. **§12 / §13:** slot pin 16 not connected on any hub; safe switches use the hub's own terminal; RST not connected on the 2" driver. **§14:** added Hardware Inventory and Master Firmware Design; I2C Protocol v2.17; Module UI Reference v6.0 (rewritten alongside this revision); master firmware is now `Software/KCMk1_Master` (the old `Software/Controller_Main` is deleted). New §15 Open Items. |

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
10. [External Expansion Interface](#10-external-expansion-interface)
11. [Panel 34-Pin IDC Connector](#11-panel-34-pin-idc-connector)
12. [Module 16-Pin IDC Connector](#12-module-16-pin-idc-connector)
13. [Module Conformance Requirements](#13-module-conformance-requirements)
14. [Related Documents](#14-related-documents)
15. [Open Items](#15-open-items)

---

## 1. Overview

The Kerbal Controller Mk1 (KCMk1) is a custom multi-panel hardware controller for Kerbal Space Program (KSP), communicating with the game via the KerbalSimpit v2.4.0 plugin. The controller consists of four physical panels (A1, A2, B1, B2) plus a planned C extension, each containing multiple input and display modules interconnected via I2C and distributed power.

This document is the primary hardware reference for developers designing, building, or modifying the controller. It covers system architecture, board topology, power distribution, USB architecture, interboard connectors, and PCB inventory.

Version 2.0 was rebuilt from `Documents/Developer/Hardware_Inventory.md`, the working source made from the V2.0 schematics and confirmed panel by panel by the owner. Where this document and the inventory disagree, the inventory wins. The master firmware's view of the same hardware is in `Documents/Developer/Master_Firmware_Design.md` and `Software/KCMk1_Master/config.h`.

**Drawing numbers.** Each board has a drawing set KC-01-xxx0 (assembly), xxx1 (schematic), xxx2 (PCB), xxx3 (BOM), xxx4 (placement): odd = schematic, even = PCB. The drawings are in `Documents/Drawings/`. Tables in this document give both the schematic and the PCB number.

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

A single USB cable connects the controller to the host PC. Inside the controller, a two-chip USB hub (2× CH334F cascaded) distributes USB to all Teensy-based microcontrollers. A single I2C bus connects the master Teensy 4.1 to all ATtiny816 input/output modules, Teensy 4.1 based display modules, and the XIAO RP2350 based system display. A 12V main bus provides power throughout, with local DC-DC conversion on each board.

Each panel has one hub board that gives its modules power, the I2C bus and one interrupt line per slot. The input panels (A2, B2) use the Panel Hub; on each display panel (A1, B1) one of the 7" TFT Display Drivers is the hub (§3.6, §5.2).

### 3.2 Master Controller

The master controller is a Teensy 4.1 mounted on the Ctrl Module PCB. Its firmware is `Software/KCMk1_Master` (pins, addresses and timings in `config.h`; design in `Master_Firmware_Design.md`). It is responsible for:

- Polling all I2C module targets on the main SDA/SCL bus based on individual module interrupts. Each module's interrupt line is learned at boot; the default map is in §8.5
- Interpreting button/axis data and translating to KSP commands via KerbalSimpit over USB (Teensy USB type Dual Serial: Simpit on `SerialUSB1`, the debug console on `Serial`)
- Driving fan speed control via the EMC2101, using its internal temperature sensor (§6.6)
- Monitoring system power via INA228 current/power sensor
- Providing I2C bus acceleration via two independent LTC4311 ICs — one for SDA_BUS (internal module bus) and one for SDA_EXT (external expansion bus)
- Reading the two used safe-switch lines (ABORT on A2, STAGE on B2; §8.2)
- Feeding the displays, including boot progress and controller status (12 V, current, power, board temperature, fan) to the Sys Info Display (0x14)
- Pulsing the global RST line (Teensy pin 2) to reboot the four 7" display drivers

The master cannot see the power button and cannot switch the controller off: the soft latch is analog and has no Teensy pin (§6.2).

### 3.3 Display Subsystem

Four Teensy 4.1 microcontrollers each drive a 7" 1024×600 IPS TFT display via an 8080 16-bit parallel interface to an LT7683 graphics controller (register-compatible with the RA8876; module ER-TFT070A2-6-5633). Each Teensy 4.1 connects to the host PC via the USB hub as an independent USB device running KerbalSimpit. The four display roles are: Annunciator (Caution & Warning panel), Resource Display, Info Display 1, and Info Display 2. All four use the same board, the 7" TFT Display Driver (KC-01-1911 / 1912). On each display panel one of them is also the panel hub: Info Display 1 on A1 and the Resource Display on B1. The other display on that panel plugs into a slot on the hub display.

The LT7683 carries 128 Mbit on-module display RAM (Winbond W9812G6JH) and a hardware geometry/BTE engine, supporting hardware-accelerated fills and full 1024×600 double-buffering — removing the SPI-bound full-frame redraw limit of the previous RA8875/SPI carriers. The display module VDD is supplied from the 5V rail; the LT7683 host-interface logic (VDDIO) operates at 3.3V regardless of VDD (bench-confirmed on this module), so the Teensy 4.1's 3.3V GPIO interfaces directly with no level shifting. The panel must be powered from 5V VDD (≤520 mA) through the driver's local MPM3610 — never jumpered to 3.3V VDD (730 mA on 3.3V exceeds the driver's AP2112K). Because the interface is 8080 parallel (no MISO line), the SN74LVC1G125 MISO buffer required by the former SPI RA8875 design is not needed.

### 3.4 System Display

One XIAO RP2350 microcontroller drives an ST7789 240×320 IPS display (2.0", ER-TFTM020-2) via SPI, on the 2" TFT Display Driver (KC-01-1901 / 1902). It connects only to the master controller and does not have a direct USB connection to Simpit. Its role is the Sys Info Display (0x14): boot progress, then controller status (12 V voltage, current and power, board temperature, fan speed, each board's KSP link state) and a window naming any switch in SWITCH ERR. It sits in Panel A2 slot P2 (INT_2). Its RST pin is not connected, so the master's RST line cannot reboot it. It is the only ST7789 in the system. The former Status Indicator Display (XIAO RA4M1, Panel B2) has been removed from the design.

### 3.5 IO Module Architecture

ATtiny816-based I2C target modules provide button input, display output, joystick, and encoder functions. Each module communicates over the shared I2C bus using the KCMk1 I2C Protocol Specification. Modules assert an active-low interrupt line when state changes, allowing the master to poll only when needed rather than continuously scanning.

The modules have no address pins. The I2C address and role are set by the firmware flashed through the UPDI header (H1), so one board type can serve several roles: UI Control and Action Control, for example, are both the Button Module (KC-01-1801 / 1802) with different firmware.

### 3.6 Board Interconnection

```
Ctrl Module (KC-01-1701 / 1702)
 ├─ CN3  34-pin ── A2 ── Panel Hub ............ INT_1–INT_8,  INT_SAFE
 ├─ CN4  34-pin ── A1 ── 7" Display Driver .... INT_9–INT_12, INT_SAFE2   (Info Display 1)
 ├─ CN1  fan (PWM + tach)
 ├─ H1   power switch header (soft latch)
 └─ CN2  50-pin ── Ctrl Module Extension (KC-01-1711 / 1712)
                    ├─ CN4  34-pin ── B2 ── Panel Hub ........... B_INT_1–B_INT_8,  B_INT_SAFE
                    ├─ CN5  34-pin ── B1 ── 7" Display Driver ... B_INT_9–B_INT_12, B_INT_SAFE2   (Resource Display)
                    ├─ CN3  fan (PWM only)
                    └─ CN2  10-pin terminal ── expansion unit (§10)
```

The A and B sides are wired the same way: an 8-line connector for the input panel's Panel Hub, and a 4-line connector for the display panel's hub driver. Modules plug into the hubs' 16-pin slots (§12).

---

## 4. Panel Layout

### 4.1 Summary

| Panel | Location | Primary Function | Hub Board | Connects to | Lines |
|-------|----------|------------------|-----------|-------------|-------|
| A1 | Left Display Panel | Caution & Warning display, configurable info display, power button | Info Display 1 (7" TFT Display Driver) | Ctrl Module CN4 | INT_9–INT_12, INT_SAFE2 |
| A2 | Left Input Panel | Action controls, function controls (Switch Group 1), throttle, UI controls, translation input, GPWS, system info display, Abort button | Panel Hub | Ctrl Module CN3 | INT_1–INT_8, INT_SAFE |
| B1 | Right Display Panel | Resource display, Info Display 2, dual encoder inputs | Resource Display (7" TFT Display Driver) | Ctrl Module Extension CN5 | B_INT_9–B_INT_12, B_INT_SAFE2 |
| B2 | Right Input Panel | Vehicle controls (Switch Group 2), stability controls, staging button, control mode selector, auxiliary controls, time controls, pre-warp time, rotation input | Panel Hub | Ctrl Module Extension CN4 | B_INT_1–B_INT_8, B_INT_SAFE |
| C (planned) | Extension unit | Additional displays and button panels — future expansion | TBD | Expansion interface (§10) | EXT_INT_1 / EXT_INT_2 |

Button maps, labels and functions are in `Documents/Developer/Module_UI_Reference.md` (v6.0) and `Hardware_Inventory.md` §10. This section covers what is plugged in where and how it is wired.

### 4.2 Panel A1 — Left Display Panel

| Item | Board | Connects to | Master sees it on |
|------|-------|-------------|-------------------|
| Info Display 1 (0x12) — **panel hub** | 7" TFT Display Driver | Its CN1 (34-pin) → Ctrl Module CN4 | INT_12 (its own line) |
| Annunciator (0x10) | 7" TFT Display Driver | Its P1 (16-pin) → Info Display 1 slot P2 | INT_9 |
| Power button | — | Ctrl Module H1 (power switch header, soft latch) | Nothing — the latch is analog and has no Teensy pin |

- **Free:** Info Display 1 slots P3 (INT_10) and P4 (INT_11).
- **Safe switch:** Info Display 1's safe-switch terminal is empty, so INT_SAFE2 is unused.
- **The Annunciator's own safe-switch terminal** goes nowhere: a driver plugged in through its 16-pin P1 has no safe-switch path, because slot pin 16 is not connected (§12).

### 4.3 Panel A2 — Left Input Panel (Panel Hub on Ctrl Module CN3)

| Slot | Line | Module | Board | Address |
|------|------|--------|-------|---------|
| P1 | INT_1 | Action Control | Button Module | 0x22 |
| P2 | INT_2 | Sys Info Display | 2" TFT Display Driver | 0x14 |
| P3 | INT_3 | Throttle | Throttle Module | 0x2C |
| P4 | INT_4 | UI Control | Button Module | 0x20 |
| P5 | INT_5 | — empty — | | |
| P6 | INT_6 | GPWS Input Panel | 7-Segment Display Module | 0x2A |
| P7 | INT_7 | Joystick Translation | Joystick Module | 0x29 |
| P8 | INT_8 | Function Control | Wide Button Module | 0x21 |

| Panel item | Wired to | Master sees it on |
|------------|----------|-------------------|
| Abort button + arming switch | Panel Hub safe-switch terminal, arming switch in series | INT_SAFE (Teensy 31), active-high; a press only reaches the master while armed |
| Switch Group 1 (8 switches) | Function Control terminals U16 / U17 → B16–B23 (§4.6) | Function Control's packet |
| Throttle buttons 100 %, UP, DOWN, 0 % | Throttle terminals U7, U8, U10, U11 | Throttle's packet |
| Translation stick | Joystick terminal U6 | Translation's packet |

**Unused:** slot P5; Action Control and UI Control terminals (B12–B15); Throttle THRTL_ENA (terminal U5).

### 4.4 Panel B1 — Right Display Panel

| Item | Board | Connects to | Master sees it on |
|------|-------|-------------|-------------------|
| Resource Display (0x11) — **panel hub** | 7" TFT Display Driver | Its CN1 (34-pin) → Ctrl Module Extension CN5 | B_INT_12 (its own line) |
| Info Display 2 (0x13) | 7" TFT Display Driver | Its P1 → Resource Display slot P2 | B_INT_9 |
| Dual Encoder (0x2D) | Dual Encoder + 2 × Encoder | Its P1 → Resource Display slot P4 | B_INT_11 |

- **Free:** Resource Display slot P3 (B_INT_10).
- **Safe switches:** none wired, so B_INT_SAFE2 is unused. The Dual Encoder's own safe-switch terminal could not reach the master anyway (§5.4).

### 4.5 Panel B2 — Right Input Panel (Panel Hub on Ctrl Module Extension CN4)

| Slot | Line | Module | Board | Address |
|------|------|--------|-------|---------|
| P1 | B_INT_1 | — empty — | | |
| P2 | B_INT_2 | Pre-Warp Time | 7-Segment Display Module | 0x2B |
| P3 | B_INT_3 | Time Control | Button Module | 0x25 |
| P4 | B_INT_4 | Vehicle Control | Wide Button Module | 0x24 |
| P5 | B_INT_5 | — empty — | | |
| P6 | B_INT_6 | Stability Control | Button Module | 0x23 |
| P7 | B_INT_7 | Auxiliary Control (AUX CTRL) | Button Module | 0x26 |
| P8 | B_INT_8 | Joystick Rotation | Joystick Module | 0x28 |

| Panel item | Wired to | Master sees it on |
|------------|----------|-------------------|
| Stage button (momentary) + arming switch | Panel Hub safe-switch terminal, arming switch in series (as Abort) | B_INT_SAFE (Teensy 35), active-high; a press only reaches the master while armed |
| Control mode selector SPC / PLN / RVR (3-position, PLN in the centre) | Time Control terminal CN2: B12 (pin 3), B13 (pin 4), +5 V (pins 1–2) | Time Control's packet: B12 = SPC, B13 = RVR, neither = PLN |
| Switch Group 2 (8 switches) | Vehicle Control terminals U16 / U17 → B16–B23 (§4.6) | Vehicle Control's packet |
| Rotation stick | Joystick terminal U6 | Rotation's packet |

**Unused:** slots P1 and P5; Stability Control terminals (B12–B15) — staging is the stage button above, not Stability B14 / B15; Time Control B14 / B15; AUX CTRL terminals.

### 4.6 Switch Group Wiring

Both switch groups are on Wide Button Modules. Each switch closes from a +5 V terminal pin (U16-1/2, U17-5/6) to its input. Behaviour is in `Master_Firmware_Design.md` §8.2–§8.3.

**Switch Group 1** (Function Control 0x21, Panel A2):

| Input | Terminal pin | Switch | Positions (off / on) | Type |
|-------|--------------|--------|----------------------|------|
| B16 | U16-6 | MSTR | OPER / RESET | Momentary |
| B17 | U16-5 | DISP | OPER / CLR | Momentary |
| B18 | U16-4 | SCE | NORM / AUX | Latching |
| B19 | U16-3 | UPTLM | INHIB / XMIT | Latching |
| B20 | U17-4 | LTG | NORM / TEST | Momentary |
| B21 | U17-3 | A/P | INHIB / ARM | Latching |
| B22 | U17-2 | ENGINE | SAFE / ARM | Latching |
| B23 | U17-1 | THRTL | STD / FINE | Latching |

**Switch Group 2** (Vehicle Control 0x24, Panel B2):

| Input | Terminal pin | Switch | Positions (off / on) | Type |
|-------|--------------|--------|----------------------|------|
| B16 | U16-6 | CHUTE | SAFE / ARM | Latching |
| B17 | U16-5 | GEAR | UP / DOWN | Latching |
| B18 | U16-4 | BRAKE | REL / LOCK | Latching |
| B19 | U16-3 | EXT LT | OFF / ILLUM | Latching |
| B20 | U17-4 | SAS | OFF / ENAB | Latching |
| B21 | U17-3 | RCS | OFF / ENAB | Latching |
| B22 | U17-2 | THC/RHC | NORM / PREC | Latching |
| B23 | U17-1 | AUDIO | MUTE / LIVE | Latching |

---

## 5. Board Topology

### 5.1 Central Boards

| Board | Schematic | PCB | Rev | Microcontroller | Function |
|-------|-----------|-----|-----|-----------------|----------|
| Ctrl Module | KC-01-1701 | KC-01-1702 | V2.0 | Teensy 4.1 | Master controller, soft latch power, INA228 power monitor, EMC2101 fan controller, 2× LTC4311 (one for SDA_BUS, one for SDA_EXT, each with 4.7kΩ pull-ups to 3.3V), USB Hub [2× CH334F cascaded, single upstream USB-C, 6 downstream Micro-USB + 1 expansion USB-C]. 47Ω series (R13) on the RST output; 47Ω series (R15) and 4.7kΩ pull-up to 3.3V (R14) on EXT_INT_1; EXT_INT_2 has no series resistor or pull-up. No status display is fitted (the Teensy SPI pins are not connected) |
| Ctrl Module Extension | KC-01-1711 | KC-01-1712 | V2.0 | N/A | Passive: splits the 50-pin interboard cable to the B side (B1, B2 panel connectors), a second fan header, and the expansion terminal. No active components or protection; every signal passes straight through |

**Ctrl Module connectors:**

| Connector | What | Notes |
|-----------|------|-------|
| CN1 | Fan, 4-pin | PWM + tach; the only fan that reports RPM (§6.6) |
| CN2 | 50-pin interboard, to the Extension | §9 |
| CN3 | A2 panel connection (34-pin, 8 lines) | INT_1–INT_8, INT_SAFE (§11) |
| CN4 | A1 panel connection (34-pin, 4 lines) | INT_9–INT_12, INT_SAFE2 (§11) |
| H1 | Power switch header | Soft-latch power button (§6.2) |
| H4 | EMC2101 external temperature sensor | Nothing fitted (§6.6) |
| USBC1 | Upstream USB-C | §7.4 |
| USB1–USB6, USBC2 | Downstream USB | §7.6, §7.7 |

**Ctrl Module Extension connectors:**

| Connector | What | Notes |
|-----------|------|-------|
| CN1 | 50-pin interboard, to the Ctrl Module | Same pinout as Ctrl Module CN2 (§9) |
| CN4 | B2 panel connection (34-pin, 8 lines) | Silkscreen "B2 PANEL CONNECTION" |
| CN5 | B1 panel connection (34-pin, 4 lines) | Silkscreen "B1 PANEL CONNECTION" |
| CN3 | Fan, 4-pin | PWM only, no tach (§6.6) |
| CN2 | Expansion terminal, 10-pin (DB125-3.81) | §10.4 |

### 5.2 Panel Hubs

| Board | Schematic | PCB | Rev | Used on | Function |
|-------|-----------|-----|-----|---------|----------|
| Panel Hub | KC-01-1731 | KC-01-1732 | V2.0 | A2, B2 | Input-panel hub: CN1 34-pin panel connector (8 lines), 8 module slots P1–P8 (16-pin), one safe-switch terminal (U4). Local MPM3610 (5V) + AP2112K (3.3V) for the slots' V_3P3 pin and the safe-switch circuit |
| 7" TFT Display Driver (as hub) | KC-01-1911 | KC-01-1912 | V2.1 | A1 (Info Display 1), B1 (Resource Display) | A 7" display (§5.3) that also acts as its panel's hub: CN1 34-pin panel connector (4 lines), 3 module slots P2–P4 (16-pin), one safe-switch terminal |

**Retired:** Display Hub (KC-01-1721 / 1722). Its job is done by the 7" TFT Display Driver.

On a display driver acting as hub, the four lines of its panel connector are used like this: pins 21–23 go to its slots P2, P3 and P4; pin 24 is the driver's own interrupt (§11.4). Its own P1 is a 16-pin module connector, used when the driver sits in another driver's slot (the Annunciator and Info Display 2).

The safe-switch terminal on each hub feeds that panel connector's safe line (INT_SAFE, INT_SAFE2, B_INT_SAFE or B_INT_SAFE2) through a 10kΩ / 20kΩ divider (§8.2). Pins 2, 3 and 5 of the terminal are tied to a V_EN net that nothing drives (§15).

### 5.3 Display Driver Boards

| Board | Schematic | PCB | Rev | Microcontroller | Display | Role |
|-------|-----------|-----|-----|-----------------|---------|------|
| 7" TFT Display Driver | KC-01-1911 | KC-01-1912 | V2.1 | Teensy 4.1 | LT7683 (RA8876-compatible) 1024×600 7" IPS TFT, 8080 16-bit parallel (ER-TFT070A2-6-5633) | Large Format Graphic Touch Display — Annunciator, Info Display 1, Info Display 2, Resource Display. VDD 5V / VDDIO 3.3V (no level shifters). On-module 128 Mbit display RAM. Local MPM3610 (5V) + AP2112K (3.3V logic). FT5316 capacitive touch via software I2C. On A1 and B1 one driver is also the panel hub (§5.2) |
| 2" TFT Display Driver | KC-01-1901 | KC-01-1902 | V2.0 | XIAO RP2350 | ST7789 240×320 IPS TFT (2.0", ER-TFTM020-2), SPI, backlight control | Small Format Graphic Display — Sys Info Display (0x14), A2 slot P2. microSD on the same SPI bus. Own 12V → 5V → 3.3V regulators. I2C and INT go to the XIAO directly at 3.3V (no divider or level shifter). Slot pins 9 (V_3P3), 14 (RST) and 16 not connected. No safe-switch terminal |

The 7" driver's Teensy uses Wire2 (SCL2 = 24, SDA2 = 25) for the module bus, pin 0 for INT and pin 1 for RST (I2C Protocol Specification §15.1; pin map in `KCMk1_SystemConfig.h`, §14).

| Display | Address | Panel | Position |
|---------|---------|-------|----------|
| Annunciator | 0x10 | A1 | Info Display 1 slot P2 |
| Resource Display | 0x11 | B1 | Panel hub |
| Info Display 1 | 0x12 | A1 | Panel hub |
| Info Display 2 | 0x13 | B1 | Resource Display slot P2 |
| Sys Info Display | 0x14 | A2 | Panel Hub slot P2 |

### 5.4 IO Module Boards

| Board | Schematic | PCB | Rev | Microcontroller | Button Inputs | Panel Inputs | Notes |
|-------|-----------|-----|-----|-----------------|---------------|--------------|-------|
| Button Module | KC-01-1801 | KC-01-1802 | V2.0 | ATtiny816 | 12 | 4 | B0–B11 on-board RGB buttons (one NeoPixel chain). B12, B13 on terminal CN2 (1–2 +5 V, 3 B12, 4 B13); B14, B15 on terminal CN1 (1 B15, 2 B14, 3–4 +5 V). Used for UI, Action, Stability, Time and AUX Control |
| Wide Button Module | KC-01-1811 | KC-01-1812 | V2.0 | ATtiny816 | 12 | 8 | B0–B11 on-board RGB buttons; B12–B15 not connected. B16–B19 on terminal U16 (1–2 +5 V, 3 B19, 4 B18, 5 B17, 6 B16); B20–B23 on terminal U17 (1 B23, 2 B22, 3 B21, 4 B20, 5–6 +5 V); DB127S-5.08-6P terminals. Used for Function and Vehicle Control (Switch Groups 1 / 2) |
| Throttle Module | KC-01-1871 | KC-01-1872 | V2.0 | ATtiny816 | N/A | 5 (4 used) | RSA0N11M9A0J 10k motorized slide potentiometer (wiper to the ATtiny), L293D H-bridge on its own 9V supply (MPM3610), AT42QT1010 touch sensor on the fader track, direction LEDs. Five 4-pin illuminated-switch terminals (pin 1 +5 V, 2 input, 3 +5 V lamp, 4 lamp return): U5 THRTL_ENA, U7 THRTL_100, U8 THRTL_UP, U10 THRTL_DOWN, U11 THRTL_00. All five lamps share one enable (LED_ENA). THRTL_ENA is not wired in the final build and is unused by the firmware |
| Joystick Module | KC-01-1831 | KC-01-1832 | V2.0 | ATtiny816 | 2 + stick button | N/A | 2× NeoPixel RGB buttons (BTN01, BTN02). JH-D202X-R4 3-axis stick on 6-pin terminal U6: 6 +5 V, 5 AXIS1, 4 AXIS2, 3 AXIS3, 2 BUTTON_JOY, 1 GND |
| 7-Segment Display Module | KC-01-1841 | KC-01-1842 | V2.0 | ATtiny816 | 3 + encoder push | N/A | MAX7219 + FJ4401AG 4-digit display; rotary encoder with push button; three buttons, each with an SK6812 LED. Used for GPWS and Pre-Warp Time |
| Dual Encoder | KC-01-1861 | KC-01-1862 | V2.0 | ATtiny816 | 2 encoder pushes | None reachable | Encoder headers H1 (ENC1) and H2 (ENC2), 5-pin, to an Encoder carrier each (§5.5). No LEDs. Safe-switch terminal U4 (same circuit as the hubs) drives slot pin 16, not the ATtiny; no hub connects slot pin 16, so this switch cannot reach the master or the module |

**Common to every ATtiny816 module:**

- **Slot connector P1 (16-pin, §12):** pin 13 is INT, driven from the ATtiny through a 10 kΩ / 20 kΩ divider; pin 14 (RST) and pin 16 are not connected.
- **Power and I2C:** each board makes its own 5 V from the 12 V slot supply. An I2C level shifter sits between the 3.3 V bus and the 5 V ATtiny.
- **No address pins:** the address and role are set by the firmware flashed through the UPDI header H1 (§3.5).
- **Panel inputs** (screw terminals) are active-high. Each terminal has a +5 V pin, and each input a 10 kΩ pull-down: a switch from +5 V to the input reads 1 when closed.

### 5.5 Encoder Carrier Board

| Board | Schematic | PCB | Rev | Function |
|-------|-----------|-----|-----|----------|
| Encoder | KC-01-1851 | KC-01-1852 | V2.0 | Passive carrier for one PEC11R-4220F-S0024 rotary encoder with push. A / B RC-filtered (10 kΩ pull-ups, 10 nF); push button active-high with a 10 kΩ pull-down. 5-pin header to the Dual Encoder: 1 +5 V, 2 A, 3 B, 4 Button, 5 GND |

### 5.6 Test & Programming Fixtures

| Board | Designator | Function |
|-------|------------|---------|
| Module Tester | KC-01-9002 | XIAO RA4M1 master, soft latch power, INA228, 1.9" TFT display, 16-pin module connector for module bring-up & bench testing |

> The Module Tester is not covered by the hardware inventory and has no drawing set in `Documents/Drawings/`; this entry is carried over from v1.10 unverified (§15).

---

## 6. Power Distribution

### 6.1 Main Power Supply

| Parameter | Value |
|-----------|-------|
| Input voltage | 12V DC |
| Rated current | 10A |
| Typical system load | see Power Budget (interim ~3.1A / 37W @ 12V) |
| Peak system load | see Power Budget (interim ~4.7A / 57W @ 12V) |

> System-load figures are maintained authoritatively in `Documents/Developer/Power_Budget.md` (v1.2). As of that revision the A2 / System Info Display (XIAO RP2350) draw is **not yet measured**, so the system totals and headroom are flagged TBD with interim placeholders; the values above are the last full-system estimate and will be reconciled once the RP2350 row is measured. The 12V / 10A supply rating is fixed hardware.

### 6.2 Power On/Off

A soft-latching power circuit on the Ctrl Module controls the 12V main bus. A single momentary pushbutton on Panel A1, wired to the Ctrl Module power switch header H1, turns power on with a short press and turns power off with a 3-second hold. The circuit uses a P-channel MOSFET (IRF9540N, D2PAK) as the load switch, with an RC network for hold-to-off timing. No microcontroller is required for power on/off — the circuit is fully analog. It has no Teensy pin: the master can neither see the button nor switch the controller off.

### 6.3 Power Rails

| Rail | Voltage | Source | Consumers |
|------|---------|--------|-----------|
| V_12 | 12V | Main input bus | MPM3610 converters on each board, 12V direct loads (fans, motors) |
| V_5 | 5V | MPM3610 DC-DC (per board) | ATtiny816 modules, Teensy VIN, display panel VDD, logic supply |
| V_3P3 | 3.3V | AP2112K LDO (per board, as required) | Teensy 4.1 logic, display VDDIO / touch / glue logic, XIAO modules. Display panel VDD is NOT on this rail — it is supplied from V_5 |
| VDD33_HUBn | 3.3V | CH334F internal LDO (per hub IC) | CH334F internal logic only — isolated net, not shared |

### 6.4 Power Monitoring

An INA228 high-side current/power monitor on the Ctrl Module measures the 12V rail. It sits in series with a 15mΩ 2512 shunt resistor between the soft latch output and the 12V distribution bus. Results are reported to the master Teensy 4.1 via I2C (address 0x40), read at 4 Hz, and shown on the Sys Info Display's controller status screen (voltage, current, power). The master warns below 11 V (`Master_Firmware_Design.md` §12.1).

### 6.5 Status LEDs

| LED | Color | LCSC Part | Rail Monitored | Series Resistor |
|-----|-------|-----------|----------------|----------------|
| Power indicator | Red | KT-0603R | 3.3V rail | 270Ω (C22966) |
| 12V indicator | Green | KT-0805G | 12V rail | 2.0kΩ (C22975) |

These are power indicators only; the master has no status LED outputs.

### 6.6 Fan and Temperature

| Item | Detail |
|------|--------|
| Controller | EMC2101 on the Ctrl Module, I2C 0x4C. Drives FAN_PWM to both fan headers |
| Temperature | The EMC2101's **internal** sensor, read as the board temperature. Nothing is fitted on H4 (external sensor header), so the remote channel is not used and its open-diode fault is ignored |
| Ctrl Module fan, CN1 | 4-pin: 1 FAN_PWM, 2 tach, 3 V_12, 4 GND. The only fan that reports RPM |
| Extension fan, CN3 | 4-pin: 1 FAN_PWM (via 50-pin pin 1), 2 not connected, 3 V_12, 4 GND. No tach |
| Control (firmware) | 1 Hz, duty 30 / 40 / 50 / 70 / 100 % at 25 / 30 / 40 / 50 °C, 2 °C hysteresis (`Master_Firmware_Design.md` §12.1) |

> **Note:** The fan header pin order (1 PWM, 2 tach, 3 12 V, 4 GND) is the reverse of the usual PC fan numbering (1 GND, 2 12 V, 3 tach, 4 PWM). Check it against the fans' cable before connecting (§15).

---

## 7. USB Architecture

### 7.1 Overview

A single USB-C cable connects the controller to the host PC. All USB traffic is carried over this connection. Inside the controller, the USB hub on the Ctrl Module distributes USB to six downstream devices. VBUS from the PC is completely isolated — no power flows from the host through the USB connection in either direction. All devices are powered from the controller's internal 12V bus.

### 7.2 Hub IC

| Parameter | Value |
|-----------|-------|
| Hub IC | CH334F (WCH) × 2 cascaded |
| Package | QFN24 4×4mm |
| LCSC Part Number | C5187527 |
| USB standard | USB 2.0 High Speed (480Mbps) |
| TT mode | MTT (Multi-Transaction Translator) |
| Total downstream ports | 7 (3 on Hub 1 + 4 on Hub 2) |
| Downstream ports used | 6 (Teensy 4.1 × 5 [master + 4 displays], expansion × 1) |
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
| Hub 1 | CH334F | Upstream USB-C connector | Port 1 → Teensy 4.1 (master), Port 2 → Teensy 4.1 display 1, Port 3 → Teensy 4.1 display 2, Port 4 → Hub 2 upstream |
| Hub 2 | CH334F | Hub 1 Port 4 (DP4/DM4) | Port 1 → Teensy 4.1 display 3, Port 2 → Teensy 4.1 display 4, Port 3 → Spare micro USB, Port 4 → Expansion USB-C |

### 7.6 Downstream Connectors

| Connector | Type | Device | VCC Pin |
|-----------|------|--------|---------|
| USB1 | Micro-USB-B SMD | Teensy 4.1 (master controller) | No connect |
| USB2 | Micro-USB-B SMD | Teensy 4.1 — Annunciator display | No connect |
| USB3 | Micro-USB-B SMD | Teensy 4.1 — Info Display 1 | No connect |
| USB4 | Micro-USB-B SMD | Teensy 4.1 — Info Display 2 | No connect |
| USB5 | Micro-USB-B SMD | Teensy 4.1 — Resource display | No connect |
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
| Module voltage | 5V (ATtiny816 modules); 3.3V (Teensy 4.1 and XIAO RP2350 display drivers) |
| Level shifting | I2C: a level shifter on each ATtiny816 module between the 3.3V bus and the 5V ATtiny. INT: passive resistor divider on each ATtiny816 module — see §8.2. Display drivers connect directly at 3.3V |
| Master pins | `Wire` SDA 18 / SCL 19 (BUS); `Wire1` SDA 17 / SCL 16 (EXT) |

The main controller provides two I2C buses:

- **SDA_BUS / SCL_BUS** — primary internal bus. Master is Teensy 4.1. All ATtiny816 modules and display drivers are targets on this bus. One LTC4311 bus accelerator fitted on Ctrl Module with pull-up resistors to 3.3V.
- **SDA_EXT / SCL_EXT** — external bus routed to the 50-pin IDC connector (§9) and thence to the expansion interface (§10). The expansion unit Teensy 4.1 operates as I2C master on this bus. The main controller Teensy 4.1 operates as I2C target. One LTC4311 bus accelerator fitted on Ctrl Module with pull-up resistors to 3.3V. No pull-ups or LTC4311 on the expansion unit side — see §8.4. The current master firmware reserves this bus and leaves it idle.

### 8.2 Interrupt Architecture

Each module drives its interrupt line as an active-low push-pull digital output — there is no pull-up resistor at the module or the controller. INT idles HIGH and is driven LOW to assert. Each module's INT line is dedicated to its own controller GPIO; there is no wired-OR bus interrupt. Level handling depends on the module's logic-voltage domain (see §13.1):

- **5V modules (ATtiny816):** drive PA1 push-pull through a passive divider — R5 (10kΩ) in series from PA1 to the `INT_BUS` node, R6 (20kΩ) from `INT_BUS` to GND — producing a 3.3V-compatible signal the master reads directly.
- **3.3V display drivers (Teensy 4.1 7" drivers, XIAO RP2350 2" driver):** drive `INT_BUS` directly at 3.3V push-pull, no divider.

| Module class | Output state | INT_BUS | Meaning |
|--------------|--------------|---------|---------|
| ATtiny816 (5V) | PA1 HIGH (5V) | 3.33V (divider) | Idle — no interrupt pending |
| ATtiny816 (5V) | PA1 LOW (0V) | 0V | Asserted — module has data ready |
| Display driver (3.3V) | GPIO HIGH (3.3V) | 3.3V | Idle — no interrupt pending |
| Display driver (3.3V) | GPIO LOW (0V) | 0V | Asserted — module has data ready |

The master detects INT_BUS going low on the dedicated interrupt line for each module and initiates an I2C read transaction. The module deasserts (returns the output HIGH) when the master completes the read. At power-on every module holds INT low until first read, which the master uses to learn which line belongs to which module; the default map is in §8.5.

**Safe-switch lines** (INT_SAFE, INT_SAFE2, B_INT_SAFE, B_INT_SAFE2) work the other way: they are **active-high**. The switch on a hub's safe-switch terminal closes +5V into a 10kΩ / 20kΩ divider, which gives about 3.3V at the master when closed and 0V when open. The master reads them as plain inputs with no pull-up. Only two are used: INT_SAFE (A2, ABORT) and B_INT_SAFE (B2, STAGE). Each has an arming switch in series on the panel, so a press only reaches the master while armed, and the master cannot see the arming switch itself.

### 8.3 I2C Device Map

#### Fixed Silicon Devices

| Address | Device | Location | Function |
|---------|--------|----------|---------|
| 0x40 | INA228 | Ctrl Module | 12V rail current and power monitor |
| 0x4C | EMC2101 | Ctrl Module | Fan speed controller; internal temperature sensor |

#### Programmable Modules

| Address | Device | Type | Board (PCB) | Panel | Slot | INT line |
|---------|--------|------|-------------|-------|------|----------|
| 0x10 | Annunciator | Teensy 4.1 | 7" TFT Display Driver (1912) | A1 | Info Display 1 P2 | INT_9 |
| 0x11 | Resource Display | Teensy 4.1 | 7" TFT Display Driver (1912) | B1 | Panel hub | B_INT_12 |
| 0x12 | Info Display 1 | Teensy 4.1 | 7" TFT Display Driver (1912) | A1 | Panel hub | INT_12 |
| 0x13 | Info Display 2 | Teensy 4.1 | 7" TFT Display Driver (1912) | B1 | Resource Display P2 | B_INT_9 |
| 0x14 | Sys Info Display | XIAO RP2350 | 2" TFT Display Driver (1902) | A2 | Panel Hub P2 | INT_2 |
| 0x15–0x1F | Reserved — expansion unit / growth | — | — | — | — | — |
| 0x20 | UI Control | ATtiny816 | Button Module (1802) | A2 | P4 | INT_4 |
| 0x21 | Function Control | ATtiny816 | Wide Button Module (1812) | A2 | P8 | INT_8 |
| 0x22 | Action Control | ATtiny816 | Button Module (1802) | A2 | P1 | INT_1 |
| 0x23 | Stability Control | ATtiny816 | Button Module (1802) | B2 | P6 | B_INT_6 |
| 0x24 | Vehicle Control | ATtiny816 | Wide Button Module (1812) | B2 | P4 | B_INT_4 |
| 0x25 | Time Control | ATtiny816 | Button Module (1802) | B2 | P3 | B_INT_3 |
| 0x26 | Auxiliary Control | ATtiny816 | Button Module (1802) | B2 | P7 | B_INT_7 |
| 0x27 | Reserved | — | — | — | — | — |
| 0x28 | Joystick Rotation | ATtiny816 | Joystick Module (1832) | B2 | P8 | B_INT_8 |
| 0x29 | Joystick Translation | ATtiny816 | Joystick Module (1832) | A2 | P7 | INT_7 |
| 0x2A | GPWS Input Panel | ATtiny816 | 7-Segment Display Module (1842) | A2 | P6 | INT_6 |
| 0x2B | Pre-Warp Time | ATtiny816 | 7-Segment Display Module (1842) | B2 | P2 | B_INT_2 |
| 0x2C | Throttle | ATtiny816 | Throttle Module (1872) | A2 | P3 | INT_3 |
| 0x2D | Dual Encoder | ATtiny816 | Dual Encoder (1862) | B1 | Resource Display P4 | B_INT_11 |
| 0x2E | Unused (was Switch Panel — retired; superseded by Switch Groups 1 / 2) | — | — | — | — | — |
| 0x2F | Reserved (was Indicator Module — removed from design) | — | — | — | — | — |

### 8.4 External Interface Signal Architecture

Signals routed over the external expansion cable (§10) require additional protection and conditioning not needed for internal IDC runs. The following requirements apply to all signals crossing the GX16 external connector interface. The main-controller side below is as built (Ctrl Module V2.0, Extension V2.0). The expansion-unit side is the planned design in `Expansion_Module_Spec.md` (v0.3, draft); no expansion hardware has been built.

**Full Signal Path**

All external interface signals travel the following path in each direction. Protection components are placed at the source and receiver ends as specified per signal below:

```
Ctrl Module PCB
  → 50-pin IDC connector on Ctrl Module (CN2)
  → IDC cable
  → 50-pin IDC connector on Ctrl Module Extension (CN1)
  → PCB routing on Ctrl Module Extension (passive)
  → terminal block on Ctrl Module Extension (CN2)
  → internal wiring (main enclosure)
  → GX16-10 panel socket (main enclosure)
  → GX16 cable (3ft / 1m)
  → GX16-10 panel socket (expansion enclosure)
  → internal wiring (expansion enclosure)
  → terminal block on Expansion Routing Board
  → PCB routing on Expansion Routing Board
  → Expansion Teensy 4.1
```

The Ctrl Module Extension performs passive signal routing only for all expansion interface signals — no active or protection components on this board for the expansion interface.

**I2C — SDA_EXT / SCL_EXT**

One LTC4311 I2C bus accelerator is fitted on the **Ctrl Module PCB** on the SDA_EXT/SCL_EXT lines, between the Teensy 4.1 I2C target port and the 50-pin IDC connector. Pull-up resistors to 3.3V are co-located with the LTC4311 on the Ctrl Module. No pull-ups or LTC4311 on the Expansion Routing Board — the expansion unit side is passive for I2C.

The LTC4311 actively drives rising edges, compensating for the total capacitive load of the IDC cable, internal wiring, GX16 connectors, and external cable run.

**EXT_INT_1 (Expansion → Main Controller)**

| Segment | Component | Location |
|---------|-----------|----------|
| Source protection (planned) | 47Ω series resistor | Expansion Routing Board, between Teensy 4.1 GPIO output and terminal block |
| Receiver series (as built) | 47Ω series resistor (R15) | Ctrl Module PCB, between 50-pin IDC pin and Teensy 4.1 GPIO input (pin 33) |
| Receiver pull-up (as built) | 4.7kΩ to 3.3V (R14) | Ctrl Module PCB, on the same line |

Signal path: Expansion Teensy 4.1 → 47Ω → terminal block → GX16 cable → terminal block → 50-pin IDC → 4.7kΩ pull-up + 47Ω series → Ctrl Module Teensy 4.1 GPIO input.

**EXT_INT_2 (Main Controller → Expansion)**

| Segment | Component | Location |
|---------|-----------|----------|
| Source protection (as built) | None — Teensy 4.1 pin 32 connects straight to the 50-pin IDC | Ctrl Module PCB |
| Receiver pull-up (planned) | 4.7kΩ to 3.3V | Expansion Routing Board, between terminal block and Teensy 4.1 GPIO input |

Signal path: Ctrl Module Teensy 4.1 → 50-pin IDC → GX16 cable → terminal block → 4.7kΩ pull-up → Expansion Teensy 4.1 GPIO input.

The v1.x design called for a 47Ω source resistor on EXT_INT_2 at the Ctrl Module; the V2.0 board does not have one (§15). No RC filter is required at the expansion receiver — a spurious interrupt is recoverable in firmware. The 4.7kΩ physical pull-up is sufficient receiving-end protection.

**RST (Main Controller → Expansion)**

| Segment | Component | Location |
|---------|-----------|----------|
| Source protection (as built) | 47Ω series resistor (R13) | Ctrl Module PCB, between Teensy 4.1 pin 2 and the IDC connectors |
| Receiver pull-up (planned) | 10kΩ to 3.3V | Expansion Routing Board, between terminal block and RC filter input |
| Receiver RC filter (planned) | 100Ω series + 100nF to GND | Expansion Routing Board, between pull-up node and Teensy 4.1 RST pin |

Signal path: Ctrl Module RST source → 47Ω → 50-pin IDC → GX16 cable → terminal block → 10kΩ pull-up → 100Ω → Teensy 4.1 RST pin (100nF to GND at RST pin).

The RC filter (τ = 10µs) rejects transients shorter than ~50µs while passing legitimate reset pulses. A 10kΩ pull-up (rather than 4.7kΩ) is used on RST to keep the RST pin firmly high during normal operation while still allowing the source to drive it low cleanly through the 47Ω source resistor. RST is the same global line that reboots the 7" display drivers (§9.3), so the master's 10 ms reset pulse also reaches the expansion unit.

**Signal Protection Summary**

| Signal | Direction | Ctrl Module (as built) | Expansion Routing Board (planned) |
|--------|-----------|------------------------|-----------------------------------|
| SDA_EXT / SCL_EXT | Bidirectional | LTC4311 + pull-ups to 3.3V | Passive (no components) |
| EXT_INT_1 | Expansion → Main | 47Ω series + 4.7kΩ pull-up to 3.3V (receiver) | 47Ω series (source) |
| EXT_INT_2 | Main → Expansion | None | 4.7kΩ pull-up to 3.3V (receiver) |
| RST | Main → Expansion | 47Ω series (source) | 10kΩ pull-up + 100Ω + 100nF RC filter (receiver) |

**USB Signal Conditioning — TUSB212**

The expansion USB-C port (§7.7) feeds a 3ft external cable plus internal wiring in both enclosures, totalling approximately 6–7ft of conductor path through multiple connector junctions. A TUSB212RWBR USB 2.0 High Speed signal conditioner must be fitted on the expansion routing board between the panel USB-C connector and the internal expansion hub (CH334F). The TUSB212 regenerates the High Speed differential signal, resetting the effective cable length budget.

| Parameter | Value |
|-----------|-------|
| IC | TUSB212RWBR (TI) |
| LCSC # | C702372 |
| Supply voltage | 3.3V |
| Placement | Expansion Routing Board, between panel USB-C and internal CH334F hub |
| Boost setting | Set per cable length — refer to TUSB212 datasheet Table 1 for resistor value |

The TUSB212 is transparent to all USB speeds — Low Speed and Full Speed signals pass through unmodified; High Speed signals are actively conditioned.

### 8.5 Master Pin Map

The master's Teensy 4.1 pins, from `Software/KCMk1_Master/config.h` and `Master_Firmware_Design.md` Appendix A. The module column is the default INT map the firmware uses on a first boot; the master then learns the actual line of each module (§8.2).

| Line | Teensy pin | Panel | Slot / source | Default module |
|------|-----------|-------|---------------|----------------|
| INT_1 | 9 | A2 | Panel Hub P1 | Action Control 0x22 |
| INT_2 | 10 | A2 | Panel Hub P2 | Sys Info Display 0x14 |
| INT_3 | 25 | A2 | Panel Hub P3 | Throttle 0x2C |
| INT_4 | 26 | A2 | Panel Hub P4 | UI Control 0x20 |
| INT_5 | 27 | A2 | Panel Hub P5 | — |
| INT_6 | 28 | A2 | Panel Hub P6 | GPWS 0x2A |
| INT_7 | 29 | A2 | Panel Hub P7 | Translation 0x29 |
| INT_8 | 30 | A2 | Panel Hub P8 | Function Control 0x21 |
| INT_SAFE | 31 | A2 | Panel Hub safe-switch terminal | ABORT |
| INT_9 | 4 | A1 | Info Display 1 P2 | Annunciator 0x10 |
| INT_10 | 5 | A1 | Info Display 1 P3 | — |
| INT_11 | 6 | A1 | Info Display 1 P4 | — |
| INT_12 | 7 | A1 | Info Display 1 itself | Info Display 1 0x12 |
| INT_SAFE2 | 8 | A1 | Info Display 1 safe-switch terminal | Unused |
| B_INT_1 | 23 | B2 | Panel Hub P1 | — |
| B_INT_2 | 22 | B2 | Panel Hub P2 | Pre-Warp 0x2B |
| B_INT_3 | 21 | B2 | Panel Hub P3 | Time Control 0x25 |
| B_INT_4 | 20 | B2 | Panel Hub P4 | Vehicle Control 0x24 |
| B_INT_5 | 15 | B2 | Panel Hub P5 | — |
| B_INT_6 | 14 | B2 | Panel Hub P6 | Stability Control 0x23 |
| B_INT_7 | 41 | B2 | Panel Hub P7 | AUX CTRL 0x26 |
| B_INT_8 | 40 | B2 | Panel Hub P8 | Rotation 0x28 |
| B_INT_SAFE | 35 | B2 | Panel Hub safe-switch terminal | STAGE |
| B_INT_9 | 39 | B1 | Resource Display P2 | Info Display 2 0x13 |
| B_INT_10 | 38 | B1 | Resource Display P3 | — |
| B_INT_11 | 37 | B1 | Resource Display P4 | Dual Encoder 0x2D |
| B_INT_12 | 36 | B1 | Resource Display itself | Resource Display 0x11 |
| B_INT_SAFE2 | 34 | B1 | Resource Display safe-switch terminal | Unused |

| Teensy pin | Signal |
|-----------|--------|
| 2 | RST out to the 7" display drivers (idle high; 10 ms low pulse to reboot) |
| 18 / 19 | `Wire` SDA / SCL (BUS) |
| 17 / 16 | `Wire1` SDA / SCL (EXT) — reserved |
| 33 / 32 | EXT_INT_1 in / EXT_INT_2 out — reserved (the firmware leaves both as inputs) |
| 0, 1, 3, 11, 12, 13, 24 | Not connected |

---

## 9. Interboard Connector Pinout

### 9.1 Overview

A 50-pin 2×25 IDC connector bridges between the Ctrl Module (CN2) and the Ctrl Module Extension (CN1); both ends have the same pinout. It carries 12V power for the B1, B2, and C panel groups; the internal I2C bus; a separate external I2C bus; 12 individual interrupt lines for B-panel modules; two special safe-switch interrupts; two external interrupts; a global reset signal; and the fan PWM signal.

### 9.2 Pinout Table

Pinout from the V2.0 schematics (KC-01-1701 CN2 and KC-01-1711 CN1, which agree). It replaces the v1.x table, which had pins 5–24 in a different order.

| Pin (Left) | Signal (Left) | Signal (Right) | Pin (Right) |
|:----------:|:-------------:|:--------------:|:-----------:|
| 1  | FAN_PWM      | GND          | 2  |
| 3  | GND          | GND          | 4  |
| 5  | EXT_INT_1    | EXT_INT_2    | 6  |
| 7  | B_INT_SAFE   | B_INT_SAFE2  | 8  |
| 9  | B_INT_11     | B_INT_12     | 10 |
| 11 | B_INT_9      | B_INT_10     | 12 |
| 13 | B_INT_7      | B_INT_8      | 14 |
| 15 | B_INT_5      | B_INT_6      | 16 |
| 17 | B_INT_3      | B_INT_4      | 18 |
| 19 | B_INT_1      | B_INT_2      | 20 |
| 21 | GND          | GND          | 22 |
| 23 | SDA_EXT      | SCL_EXT      | 24 |
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
| SDA_EXT / SCL_EXT | I2C | Separate external I2C bus — routed by the Extension to the expansion terminal (§10) |
| RST | Signal | Global reset — active low. Reboots the 7" display drivers on B1 (through Extension CN5) and goes to the expansion terminal. Not used by ATtiny816 modules or the 2" display driver |
| B_INT_1 to B_INT_12 | Interrupt | Individual active-low interrupt lines: B_INT_1–8 to the B2 Panel Hub (Extension CN4), B_INT_9–12 to the B1 hub display (Extension CN5) |
| B_INT_SAFE / B_INT_SAFE2 | Interrupt | Active-high safe-switch lines (§8.2) — B_INT_SAFE to B2 (STAGE), B_INT_SAFE2 to B1 (unused) |
| EXT_INT_1 / EXT_INT_2 | Interrupt | External interrupt lines — used by the expansion unit interface (§10). Signal protection architecture defined in §8.4 |
| FAN_PWM | PWM | Fan speed control PWM signal — driven by EMC2101 on Ctrl Module, to the Extension fan header CN3 |

> **Note:** RST on pin 25 has a dedicated GND return on pin 26 — keep these routed together to minimize noise on the reset line. RST signal protection architecture defined in §8.4.

> **Note:** GND on pin 2 adjacent to FAN_PWM on pin 1 provides a clean local return for the PWM signal.

---

## 10. External Expansion Interface

The expansion unit connects to the main controller via two panel-mounted connectors on each enclosure: a GX16-10 circular aviation connector for power and signals, and a USB-C panel mount connector for USB 2.0 data. Signal protection and conditioning architecture for all signals crossing this interface is defined in §8.4. The expansion unit is a planned design (`Expansion_Module_Spec.md`); on the main controller only the Ctrl Module Extension's CN2 terminal is built.

### 10.1 Overview

| Connector | Purpose | Cable |
|-----------|---------|-------|
| GX16-10 panel socket | 12V power, I2C, interrupts, reset | GX16-10 female-to-female, 3ft (1m) |
| USB-C panel mount | USB 2.0 High Speed data | USB-C to USB-C, 3ft (1m) |

Both connectors are fitted on each enclosure panel. Internal wiring runs from each connector to a terminal block on the respective routing board inside each enclosure — no signals are soldered directly to panel connectors.

### 10.2 GX16-10 Signal Connector

A GX16-10 (10-pin, 16mm thread) aviation connector carries 12V power, I2C, interrupt, and reset signals between the main controller and expansion unit enclosures.

| Parameter | Value |
|-----------|-------|
| Connector | GX16-10 |
| Panel mount type | Female socket with male pins (standard panel-mount convention) |
| Cable ends | Female socket at both ends |
| Rated current | 5A per pin |
| Rated voltage | 125V |
| Wire gauge | 24AWG signal, 20AWG power (V_12, GND) |
| Internal termination | Terminal block CN2 on Ctrl Module Extension (main side); terminal block on Expansion Routing Board (expansion side) |

**Pin Assignment:**

| Pin | Signal | Direction | Notes |
|-----|--------|-----------|-------|
| 1 | V_12 | Main → Expansion | 12V power |
| 2 | V_12 | Main → Expansion | Parallel — combined capacity for full expansion load |
| 3 | GND | — | Power return |
| 4 | GND | — | Power return parallel |
| 5 | SDA_EXT | Bidirectional | I2C data — LTC4311 + pull-ups on Ctrl Module only (§8.4) |
| 6 | SCL_EXT | Bidirectional | I2C clock — LTC4311 + pull-ups on Ctrl Module only (§8.4) |
| 7 | EXT_INT_1 | Expansion → Main | Active-low — 47Ω series at expansion end; 47Ω series + 4.7kΩ pull-up at Ctrl Module (§8.4) |
| 8 | EXT_INT_2 | Main → Expansion | Active-low — no series resistor at Ctrl Module; 4.7kΩ pull-up at expansion end (§8.4) |
| 9 | RST | Main → Expansion | Active-low — 47Ω series at Ctrl Module; 10kΩ pull-up + RC filter at expansion end (§8.4) |
| 10 | SPARE | — | Reserved for future use |

> **Note:** V_12 pins 1 and 2 in parallel provide up to 10A combined capacity — well in excess of the expansion unit's ~1.5A @ 12V typical load.

> **Note:** All signal protection components (series resistors, pull-ups, RC filter) are on the respective board PCBs, not in the cable. The cable carries bare signals between connectors.

### 10.3 USB-C Panel Mount Connector

A USB-C panel mount bulkhead connector (female-to-female feedthrough) on each enclosure panel carries USB 2.0 High Speed data from Hub 2 Port 4 on the main controller to the expansion unit's internal CH334F hub. A standard USB-C cable connects the two panel mounts.

| Parameter | Value |
|-----------|-------|
| Connector | USB-C panel mount, female bulkhead feedthrough |
| Cable | Standard USB-C to USB-C, 3ft (1m) maximum |
| Signal conditioning | TUSB212RWBR on Expansion Routing Board (§8.4) |
| VBUS | Not connected at either panel mount — expansion powered via GX16 V_12 |

> **Note:** The USB-C panel mount on the main controller side connects directly to USBC2 (§7.7) with no signal conditioning — the TUSB212 is placed on the expansion unit side only, after the full cable run.

### 10.4 Internal Wiring

Between each panel connector and the respective routing board, point-to-point internal wiring carries all signals. This wiring segment is not on any PCB — it connects the GX16 solder cups or pigtail leads to a terminal block on the board.

| Segment | From | To | Wire spec |
|---------|------|----|-----------|
| Main controller side | GX16-10 panel socket (main enclosure) | Terminal block CN2 on Ctrl Module Extension | 24AWG signal, 20AWG power |
| Expansion side | GX16-10 panel socket (expansion enclosure) | Terminal block on Expansion Routing Board | 24AWG signal, 20AWG power |
| Main USB-C | USB-C panel mount (main enclosure) | USBC2 on Ctrl Module (§7.7) | USB 2.0 rated cable assembly |
| Expansion USB-C | USB-C panel mount (expansion enclosure) | TUSB212 input on Expansion Routing Board | USB 2.0 rated cable assembly |

**Ctrl Module Extension CN2 (10-pin DB125-3.81 terminal):**

| Terminal pin | Signal | GX16-10 pin |
|:------------:|--------|:-----------:|
| 1 | V_12 | 1 |
| 2 | V_12 | 2 |
| 3 | GND | 3 |
| 4 | GND | 4 |
| 5 | EXT_INT_2 | 8 |
| 6 | EXT_INT_1 | 7 |
| 7 | RST | 9 |
| 8 | SCL_EXT | 6 |
| 9 | SDA_EXT | 5 |
| 10 | Not connected | 10 (SPARE) |

> **Note:** From pin 5 on, the terminal order differs from the GX16-10 pin order, so the main-side harness must cross-map pins 5–9 as shown. Wire by signal name, not by pin number.

---

## 11. Panel 34-Pin IDC Connector

### 11.1 Overview

A 34-pin 2×17 IDC connector bridges between the Ctrl Module / Ctrl Module Extension and each panel's hub: the Panel Hub on A2 and B2, and the hub 7" TFT Display Driver on A1 and B1. It carries 12V power for the individual modules; the internal I2C bus; individual interrupt lines; one special safe-switch line; and a global reset signal. All four panel connectors share one pinout; a display-panel connector carries only four of the eight interrupt lines.

| Connector | Panel | Hub |
|-----------|-------|-----|
| Ctrl Module CN3 | A2 | Panel Hub CN1 |
| Ctrl Module CN4 | A1 | Info Display 1 (7" driver) CN1 |
| Extension CN4 | B2 | Panel Hub CN1 |
| Extension CN5 | B1 | Resource Display (7" driver) CN1 |

### 11.2 Pinout Table

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

### 11.3 Signal Descriptions

| Signal | Type | Description |
|--------|------|-------------|
| V_12 | Power | 12V main bus — 8 pins (1–8), supports full panel load |
| GND | Power | Ground return — 14 pins total distributed throughout connector |
| SDA_BUS / SCL_BUS | I2C | Main internal I2C bus — connects to Ctrl Module |
| RST | Signal | Global reset — active low (ATtiny816 modules: no-connect — RST used for the 7" display drivers only) |
| INT_1 to INT_8 | Interrupt | Individual active-low interrupt lines for each panel module (display connector: pins 21–24 only; 25–28 not connected) |
| INT_SAFE | Interrupt | Active-high safe-switch line from the hub's safe-switch terminal (§8.2) — separate from general INT group |

### 11.4 Line Mapping

| Panel connector | Pins 21–24 | Pins 25–28 | Pin 29 |
|-----------------|------------|------------|--------|
| A2 (Ctrl CN3) | INT_1–4 | INT_5–8 | INT_SAFE |
| A1 (Ctrl CN4) | INT_9–12 | — | INT_SAFE2 |
| B2 (Ext CN4) | B_INT_1–4 | B_INT_5–8 | B_INT_SAFE |
| B1 (Ext CN5) | B_INT_9–12 | — | B_INT_SAFE2 |

On a Panel Hub, pins 21–28 go to slots P1–P8 in order. On a hub display driver, pins 21–23 go to its slots P2, P3 and P4, and pin 24 is the driver's own interrupt. So on A1, INT_12 is Info Display 1 itself and INT_9–INT_11 are its slots; on B1, B_INT_12 is the Resource Display and B_INT_9–B_INT_11 its slots.

---

## 12. Module 16-Pin IDC Connector

### 12.1 Overview

Each IO or Display module connects to a slot on its panel's hub via a 16-pin 2×8 IDC connector (P1 on the module). It carries 12V power for the modules; the internal I2C bus; module interrupt; a global reset signal; and a 3.3V input from the hub board to support level shifting (to allow modules to not have a 3.3V line if not otherwise required). Pin 16 is not connected on any hub.

### 12.2 Pinout Table

| Pin (Left) | Signal (Left) | Signal (Right) | Pin (Right) |
|:----------:|:-------------:|:--------------:|:-----------:|
| 1  | V_IN (12V)   | V_IN (12V)   | 2  |
| 3  | V_IN (12V)   | V_IN (12V)   | 4  |
| 5  | GND          | GND          | 6  |
| 7  | GND          | GND          | 8  |
| 9  | V_3P3        | GND          | 10 |
| 11 | SDA_BUS      | SCL_BUS      | 12 |
| 13 | INT          | RST          | 14 |
| 15 | GND          | NC           | 16 |

### 12.3 Signal Descriptions

| Signal | Type | Description |
|--------|------|-------------|
| V_IN | Power | 12V main bus |
| GND | Power | Ground return |
| V_3P3 | Power | 3.3V input from hub board — supports level shifting on modules that require it. Not connected on the 2" display driver |
| SDA_BUS / SCL_BUS | I2C | Main internal I2C bus — connects to Ctrl Module |
| RST | Signal | Global reset — active low. Used by the 7" display drivers (driver firmware performs a full reboot on assertion); no-connect on ATtiny816 modules (reset via CMD_RESET over I2C) and on the 2" display driver. Optional per module class — see §13.1 |
| INT | Interrupt | Individual active-low interrupt line for this module |
| NC (pin 16) | — | Not connected on any hub (Panel Hub or hub display driver). The v1.x design put INT_SAFE here; safe switches now wire to the hub's own safe-switch terminal (§5.2, §8.2). The Dual Encoder still drives this pin from its safe-switch terminal, so that terminal has no path to the master |

> **Note:** The "*Safe Switch Connection" notes beside slot P1 on the Panel Hub schematic and slot P2 on the 7" display driver schematic are left over from the v1.x design. No slot carries a safe-switch signal.

---

## 13. Module Conformance Requirements

This section defines the minimum hardware and firmware requirements for any module board to be a conformant member of the KCMk1 system. A conformant module can be connected to any panel hub slot, addressed by the master controller, and commanded via the KCMk1 I2C Protocol Specification without special-casing.

### 13.1 Hardware Requirements

**Connector**
The module must implement the 16-pin 2×8 IDC connector with the pinout defined in Section 12. All power, ground, I2C, interrupt, and reset signals must be connected as specified. Pin 16 is not connected on any hub, so a module must not rely on it: a module that needs a safe switch cannot get one through its slot, and a panel safe switch is wired to the hub's safe-switch terminal instead. RST is required on the 7" display drivers and no-connect on ATtiny816 modules and the 2" display driver (see "Hardware Reset" below).

**Power**
The module must accept 12V on V_IN and provide its own local regulation. The standard power architecture is:
- MPM3610 DC-DC converter: 12V → 5V (module logic and NeoPixel supply; on display drivers, also the display panel VDD)
- AP2112K LDO: 5V → 3.3V (if 3.3V logic is required locally)

Modules must not draw power from the V_3P3 pin on the IDC connector for primary logic supply — that pin is provided for level shifting support only.

On the 7" display drivers (KC-01-1911 / 1912), each driver sources its full 5V load (display panel ~480/520 mA + Teensy 4.1 ~100/120 mA + local 3.3V logic) from one MPM3610 (1.2 A rated), leaving ~37% peak headroom. The display panel VDD is on the 5V rail; VDDIO operates at 3.3V. Do not power the panel from 3.3V.

**Interrupt Line**
The INT signal is an active-low, push-pull digital output driven by the module microcontroller. INT idles HIGH and is driven LOW to assert. Each module's INT line is dedicated to its own controller GPIO — there is no wired-OR bus interrupt. No pull-up resistor is fitted at the controller or the module; the push-pull output defines both levels.

Level handling depends on the module's logic-voltage domain:

- **5V modules (ATtiny816, e.g. KC-01-1802 / KC-01-1842):** the 5V push-pull output (PA1) is level-shifted to a 3.3V-compatible signal on INT_BUS by a passive divider on the module board:
  - R5: 10kΩ in series from the microcontroller output to the INT_BUS node
  - R6: 20kΩ from INT_BUS to GND
  - The divider produces ~3.3V on INT_BUS when the output is HIGH (idle) and 0V when LOW (asserted).
- **3.3V display drivers (Teensy 4.1 7" driver KC-01-1912; XIAO RP2350 2" driver KC-01-1902):** the microcontroller drives INT_BUS directly at 3.3V push-pull. No divider is fitted — the output is already in the 3.3V domain.

In both cases INT_BUS connects to the IDC INT pin (§12, pin 13). The signaling polarity (active-low, driven low to assert) is identical across module classes; only the level-conversion hardware differs.

**Hardware Reset (RST)**
RST (§12, pin 14; §11, pin 19; §9, pin 25) is an active-low global reset line, optional and module-class-specific:

- **7" display drivers (KC-01-1912):** RST is connected. The master asserts RST low (a 10 ms pulse on Teensy pin 2) to command a full driver reboot; the driver firmware treats the RST input as a reset-command signal and performs a full software-initiated system reset on assertion. This is distinct from the software CMD_RESET (0x07) command, which resets application state only. For boards fed RST over the external interface, receiving-end protection (10kΩ pull-up + 100Ω series + 100nF to GND) applies per §8.4.
- **2" display driver (KC-01-1902):** RST is not connected; the master cannot hard-reset the Sys Info Display.
- **ATtiny816 modules:** RST is no-connect. Reset for these modules is via CMD_RESET over I2C only.

A module is conformant whether or not it implements RST.

**I2C**
The module must operate as an I2C target at 400kHz (Fast Mode). The I2C address must be assigned from the range 0x20–0x2E and must not conflict with any address in the device map (Section 8.3). The module must not include its own I2C pull-up resistors — pull-ups are provided by the master side. (Note: display drivers may carry local pull-ups on a separate software-I2C touch bus to their on-board touch controller; that bus is independent of the KBC SDA_BUS/SCL_BUS and is not subject to this rule.)

### 13.2 Firmware Requirements

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
The module must assert INT (drive the output LOW) when it has a state change ready for the master to read. INT must be deasserted (output HIGH) when the master completes the read transaction. The transaction counter must increment on every INT assertion.

INT must not be asserted while the module is in SLEEPING or DISABLED state, except for the initial BOOT_READY assertion at power-on.

### 13.3 Optional Capabilities

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

### 13.4 Conformance Summary

| Requirement | Mandatory | Reference |
|-------------|-----------|---------|
| 16-pin IDC connector per Section 12 pinout; nothing on pin 16 | Yes | §12 |
| 12V input, local MPM3610 + AP2112K regulation | Yes | §6.3 |
| Active-low push-pull INT; 5V modules add R5/R6 divider, 3.3V display drivers drive direct | Yes | §8.2, §13.1 |
| Hardware RST line (7" display drivers) / no-connect (ATtiny816, 2" display driver) | If 7" driver | §13.1 |
| I2C target at 400kHz, address 0x20–0x2E | Yes | §8.1, §8.3 |
| CMD_GET_IDENTITY response | Yes | §13.2 |
| 3-byte universal header on all response packets | Yes | §13.2 |
| Full lifecycle state machine (BOOT_READY/DISABLED/ACTIVE/SLEEPING) | Yes | §13.2 |
| All base commands (CMD_ENABLE through CMD_DISABLE) | Yes | §13.2 |
| Transaction counter increment on INT assertion | Yes | §13.2 |
| Capability flags declared accurately in identity response | Yes | §13.3 |
| Optional capability payload per declared flags | If flag set | §13.3 |

---

## 14. Related Documents

| Document | Location | Contents |
|----------|----------|---------|
| Hardware Inventory | `Documents/Developer/Hardware_Inventory.md` | Working source for this document — every board, connector, slot, switch and button label, from the V2.0 schematics, confirmed panel by panel. Wins over this document where they disagree |
| Master Firmware Design | `Documents/Developer/Master_Firmware_Design.md` | Design of the master firmware `Software/KCMk1_Master`: module bus, INT learning, board services (INA228, EMC2101, display reset), Sys Info Display, pin map (Appendix A) |
| Master firmware configuration | `Software/KCMk1_Master/config.h` | The pins, I2C addresses, sync bytes and timings the master firmware uses |
| I2C Protocol Specification | `Documents/Developer/I2C_Protocol_Specification.md` | v2.17 — packet formats for all module types, addresses, command/response structure, lifecycle state machine, interrupt signalling, hardware reset, display-carrier protocol (including the Sys Info Display, §15.4) |
| Module UI Reference | `Documents/Developer/Module_UI_Reference.md` | v6.0 (rewritten from the inventory alongside this revision) — per-module button assignments, switch groups, CAG table, axis mappings, firmware implementation detail |
| Power Budget | `Documents/Developer/Power_Budget.md` | v1.2 — per-module and per-panel power consumption, supply headroom analysis |
| Ascent Autopilot Interface | `Documents/Developer/Ascent_Autopilot_Interface.md` | Info Display ↔ master byte-level contract for the Simpit ascent autopilot command/status frames |
| Expansion Module Specification | `Documents/Developer/Expansion_Module_Spec.md` | v0.3 (draft) — expansion unit architecture, board topology, GX16 interface, keyboard subsystem, display subsystem |
| Drawings | `Documents/Drawings/` | Assembly, schematic, PCB, BOM and placement drawings for each board (KC-01-xxx0 to xxx4) |
| KC-01-1912 Pin Assignment | `Software/Displays/libraries/KCMk1_SystemConfig/src/KCMk1_SystemConfig.h` | Authoritative Teensy 4.1 ↔ LT7683 (RA8876-compatible) 7" display-driver pin map (`KCM_TFT_*`, touch, audio, I2C bus defines) |
| Library READMEs | Per library in source tree | KerbalButtonCore, KerbalJoystickCore, Kerbal7SegmentCore, KerbalModuleCommon, KerbalDisplayCommon, KerbalDisplayAudio |

---

## 15. Open Items

Items the sources do not settle. Each needs the owner's decision or a bench check.

| # | Item | Source |
|---|------|--------|
| 1 | **Fan header pin order.** Both fan headers are numbered 1 PWM, 2 tach / NC, 3 12 V, 4 GND, the reverse of the usual PC fan numbering. Check against the fans' cable before connecting (§6.6) | Inventory §6 #6 |
| 2 | **Safe-switch terminal V_EN.** Pins 2, 3 and 5 of the hubs' safe-switch terminal are tied to a V_EN net that nothing drives. Its intended use is not recorded (§5.2) | Inventory §6 #5 |
| 3 | **50-pin pinout.** §9.2 is taken from the V2.0 schematics (Ctrl Module CN2, Extension CN1), which agree with each other but not with the v1.x table. The inventory says only "same pinout as Ctrl Module CN2"; the owner should confirm the pinout and add it to the inventory | Schematics KC-01-1701, KC-01-1711 |
| 4 | **EXT_INT protection.** On the Ctrl Module, EXT_INT_1 has 47 Ω + 4.7 kΩ and EXT_INT_2 has nothing, while `Expansion_Module_Spec.md` v0.3 expects a 47 Ω source resistor on EXT_INT_2 at the Ctrl Module. Decide whether the expansion side adds protection on EXT_INT_2 or a later Ctrl Module revision does, and update the expansion spec (§8.4) | Inventory §6 #2 |
| 5 | **Expansion harness.** The Extension CN2 terminal order differs from the planned GX16-10 pin assignment from pin 5 on; the harness cross-maps them (§10.4). The GX16-10 assignment itself is still the planned v1.x design, with no drawing | Inventory §4 |
| 6 | **Dual Encoder safe-switch terminal.** It drives slot pin 16, which no hub connects, so it cannot be used. Either retire the terminal or connect pin 16 on a future hub revision | Inventory §5.8, §6 #12 |
| 7 | **Sys Info Display power draw** not yet measured, so the system load in §6.1 is still interim | Power Budget v1.2 |
| 8 | **Module Tester (KC-01-9002)** is not in the inventory and has no drawings in the repository; §5.6 is carried over from v1.10 unverified | — |
| 9 | **7" TFT Display Driver revision.** The schematic drawing is V2.1 and the PCB, BOM and placement drawings are V2.0. Confirm whether the board itself changed in V2.1 | `Documents/Drawings/` |
