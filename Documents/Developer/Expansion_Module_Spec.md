# Kerbal Controller Mk1 — Expansion Module Specification

**Organization:** Jeb's Controller Works  
**Author:** J. Rostoker  
**Version:** 0.4 (Draft)  
**Date:** 2026-11-21  
**Document type:** Developer — Hardware Design Specification

---

## 1. Purpose and Scope

This document specifies the KCMk1 Expansion Module: a separate enclosure with a DSKY-style keypad and two 7" touch displays. It becomes the controller's autopilot interface and also runs programs of its own. It connects to the main controller through the GX16-10 signal connector and the USB-C panel mount that are already fitted, but reserved, on the main enclosure (Hardware Reference §10).

Version 0.4 brings the May 2026 draft in line with the controller as built. The master firmware (`Software/KCMk1_Master`) now runs every autopilot, and the 7" display driver (KC-01-1911) is a proven board. §2.1 lists the decisions this revision rests on.

Signal protection on the external interface is defined in Hardware Reference §8.4 and §10, and is unchanged.

---

## 2. Functional Overview

The expansion module provides:

- **A DSKY-style keypad**: 25 keys (Kailh Choc V1, SK6812MINI-EA per-key RGB backlight) and one rotary encoder (§7).
- **Two 7" 1024×600 capacitive touch displays, in portrait (600 × 1024)**, each on a standard 7" TFT Display Driver (KC-01-1911), the same board as the main controller's four displays (§8).
- **The autopilot interface.** The keypad and the right display operate the master's autopilots: ascent, aircraft and rover hold, orbital (burn) and landing (§4.3). Once the expansion passes its bench checks, the autopilot consoles move off Information Display 2 (§4.4).
- **Its own programs**: planning, calculators and data pages that do not fly the vessel (§4.5).
- **An internal USB hub** (CH334F). It shares the single expansion USB-C connection between the expansion Teensy and the two display drivers.

The **expansion Teensy 4.1** scans the keypad and encoder, drives the key backlights, and runs the program logic. It also passes autopilot commands and state between the master and the right display. It has its own Kerbal Simpit link for the flight data its programs need. Each display driver also has its own Kerbal Simpit link for the telemetry it draws.

### 2.1 Decisions in this revision

| # | Decision | Replaces (v0.3) |
|---|---|---|
| D1 | **Role: both.** The expansion drives the master's autopilots (the master keeps all autopilot logic) and runs its own non-flying programs | The expansion ran the autopilot programs and orbital math and sent commands to the master |
| D2 | **The master polls the expansion on the EXT bus.** The main controller is I2C master on `Wire1` (SDA_EXT / SCL_EXT); the expansion is a target. The expansion pulls EXT_INT_1 to ask for a poll. This is the same pattern as every other board in the system | The expansion was I2C master on SDA_EXT and the main controller a target |
| D3 | **Displays reuse the 7" TFT Display Driver** (KC-01-1911, Teensy 4.1 + LT7683, 1024×600 panel, capacitive touch) and the display firmware library (`KerbalDisplayCommon`) | New RP2040 + SSD1963 5" boards (KC-01-EX30 / EX31) |
| D4 | **The expansion Teensy has its own Kerbal Simpit link** for its programs' flight data. About 210–240 bytes per update (≈ 2–2.5 kB/s at the 125 ms refresh rate) is not forwarded over EXT | — |
| D5 | **The autopilot interface moves entirely to the expansion.** The Information Display 2 consoles stay until the expansion's autopilot pages pass their bench checks, and are then removed from Information Display 2 and the manual | Not covered |
| D6 | **Autopilots are operated with keys plus console pages.** The right display shows the selected autopilot's page (modes, setpoints, status), like today's consoles. The keys do the work; touch is secondary | Verb / noun entry |
| D7 | **Panel layout A, portrait.** Both 7" displays stand in portrait (600 × 1024) side by side, with the keypad centred below them. The panel is about 260 × 305 mm | Not covered |
| D8 | **No A/P OFF on the expansion.** Autopilots are switched off from the main controller: the A/P switch, or the RHC stick button. The expansion engages, sets and disengages individual modes only | The consoles' A/P OFF button |

---

## 3. External Connections

The expansion unit has two connectors on its enclosure panel, matching the main enclosure (Hardware Reference §10).

### 3.1 GX16-10 Signal Connector

Both enclosure panels mount a **female socket with male pins**. The cable has **female sockets at both ends**.

| Pin | Signal | Direction | Notes |
|-----|--------|-----------|-------|
| 1 | V_12 | Main → Expansion | 12 V power |
| 2 | V_12 | Main → Expansion | Parallel, for current capacity |
| 3 | GND | — | Power return |
| 4 | GND | — | Power return, parallel |
| 5 | SDA_EXT | Bidirectional | I2C data. The master is bus master (D2). LTC4311 and pull-ups on the Ctrl Module only; passive at the expansion end |
| 6 | SCL_EXT | Master → Expansion | I2C clock, driven by the master. As pin 5 |
| 7 | EXT_INT_1 | Expansion → Main | Active-low, "poll me" (§9.2). 47 Ω series at the expansion end; 47 Ω series and 4.7 kΩ pull-up at the Ctrl Module. Master pin 33 |
| 8 | EXT_INT_2 | Main → Expansion | Active-low, "state changed" (§9.2). 4.7 kΩ pull-up at the expansion end; no source resistor on the V2.0 Ctrl Module (Hardware Reference §8.4, §15). Master pin 32 |
| 9 | RST | Main → Expansion | Active-low. 47 Ω series at the Ctrl Module; 10 kΩ pull-up and 100 Ω + 100 nF RC filter at the expansion end |
| 10 | SPARE | — | Reserved |

All signal protection components are on the boards, not in the cable. The Ctrl Module Extension's CN2 terminal order differs from the GX16-10 order from pin 5 on, so the main-side harness cross-maps them (Hardware Reference §10.4).

**RST** is the master's display reset line (master pin 2): asserting it reboots the four main displays and the expansion Teensy together. Inside the expansion, the expansion Teensy resets its own two display drivers (§4.1).

### 3.2 USB-C Panel Mount Connector

A USB-C bulkhead feedthrough carries USB 2.0 High Speed from the main controller's Hub 2 Port 4 (USBC2, Hardware Reference §7.7) to the expansion's internal CH334F hub. The two panel mounts are joined by a standard USB-C cable, 1 m maximum.

VBUS is not connected at either panel mount; the expansion is powered from V_12 on the GX16-10. A TUSB212RWBR USB 2.0 signal conditioner sits on the Expansion Routing Board between the panel USB-C and the hub, to cover the ≈ 2 m total path across both enclosures (Hardware Reference §8.4).

---

## 4. Internal Architecture

### 4.1 Block Diagram

```
GX16-10 ──► Expansion Routing Board (KC-01-EX10)
  ├─ V_12 (×2) ─┬─ MPM3610 → 5 V (expansion Teensy, hub, keypad LEDs) → AP2112K → 3.3 V
  │             └─ V_12 to each display slot (the display drivers regulate their own)
  ├─ SDA_EXT / SCL_EXT ── passive ──► Expansion Teensy: I2C target (EXT)
  ├─ EXT_INT_1 ◄── 47 Ω ── Expansion Teensy output ("poll me")
  ├─ EXT_INT_2 ── 4.7 kΩ pull-up ──► Expansion Teensy input ("state changed")
  └─ RST ── 10 kΩ pull-up, 100 Ω + 100 nF ──► Expansion Teensy reset

USB-C panel mount ── TUSB212 ── CH334F hub
                                  ├─ Port 1 → Expansion Teensy 4.1   (Simpit link)
                                  ├─ Port 2 → Left display driver   (Simpit link)
                                  ├─ Port 3 → Right display driver  (Simpit link)
                                  └─ Port 4 → spare

Expansion Teensy 4.1 (Expansion Teensy Carrier, KC-01-EX23)
  ├─ I2C target on EXT (from the master)
  ├─ I2C master on the local bus ──┬─ slot S1 (16-pin) → Left display driver  (KC-01-1911)
  │                                └─ slot S2 (16-pin) → Right display driver (KC-01-1911)
  ├─ INT in from each display driver, RST out to both
  ├─ Keypad matrix, 4 rows × 7 columns (25 keys) ── Keyboard Matrix PCB (KC-01-EX21)
  ├─ Encoder A / B / push ── Encoder PCB (KC-01-EX22)
  └─ NeoPixel data → 25 × SK6812MINI-EA key backlights
```

The display slots S1 and S2 use the standard 16-pin module connector (Hardware Reference §12): 12 V, GND, 3.3 V, local SDA / SCL, INT and RST. Each display driver plugs in through its own P1, as the Annunciator and Information Display 2 do in the main controller.

### 4.2 I2C Buses

| Bus | Master | Targets | Carries |
|-----|--------|---------|---------|
| EXT (SDA_EXT / SCL_EXT) | Main controller Teensy 4.1 (`Wire1`, pins 17 / 16) | Expansion Teensy 4.1 | Autopilot commands up; autopilot status and summary down (§9) |
| Local (inside the expansion) | Expansion Teensy 4.1 | Left and right display drivers | Display carrier protocol (I2C spec §15), page contents and touch events |

The expansion Teensy uses two of its I2C ports, one as a target and one as a master. Neither bus is shared with the main controller's module bus.

### 4.3 Autopilot Integration

The master keeps every autopilot: its control laws, the arbiter, the interlocks (A/P INHIB, ENGINE SAFE, the A/P disconnect) and the disconnect reasons. The expansion is a remote console:

- **Commands up.** A key action becomes the same command the Information Display 2 consoles send today: engage or disengage a mode, set a setpoint, A/P OFF, ARM / DISARM. It reaches the master as a short command frame on EXT. The master applies the same rules as for the consoles: refusals are printed on the KSP screen, and a refused command is never echoed.
- **State down.** The master sends the expansion the status frames it sends Information Display 2 today: ascent 0xA5, aircraft 0xA6, rover 0xA7, orbital 0xA8, landing 0xA9 and the 8-byte summary 0xAA. The expansion Teensy relays them to the right display, which draws the page.
- **Interlocks and HOLD are unchanged.** The panel switches and the HOLD button work as they do now, with or without the expansion.
- **No A/P OFF on the expansion (D8).** Switching every autopilot off stays on the main controller: the A/P switch to INHIB, or the RHC stick button. The expansion disengages one mode at a time (PRO on an engaged mode).

### 4.4 Migration from Information Display 2

1. **Until the expansion passes its bench checks,** the consoles on Information Display 2 stay as they are. The master serves both: it takes commands from either and sends state to both.
2. **Afterwards,** the consoles (ASC, ACAP, RVAP, ORAP, LDAP), the A/P sidebar key and the autopilot rules of the mission screen are removed from Information Display 2. The master stops sending it autopilot frames. The Technical Information Manual and the display references are updated to match.
3. **Without the expansion,** the controller then has no autopilot console; HOLD still works.

### 4.5 Expansion Programs

Programs that compute or display but do not fly the vessel run on the expansion Teensy, from its own Simpit data. Examples: transfer and phase-angle planning, Δv and burn-time calculators, and orbit and target data pages. The program list is to be defined (E-15). Any future program that needs to fly the vessel is added to the master as an autopilot, and the expansion gets a console page for it.

---

## 5. Board Topology

### 5.1 Boards

| Board | Designator | Description |
|-------|------------|-------------|
| Expansion Routing Board | KC-01-EX10 | GX16-10 and USB-C panel wiring terminals; TUSB212 USB conditioner; CH334F hub (3 ports used, 1 spare); MPM3610 12 V → 5 V and AP2112K → 3.3 V; 47 Ω series on EXT_INT_1; 4.7 kΩ pull-up on EXT_INT_2; 10 kΩ pull-up and 100 Ω + 100 nF RC filter on RST. No LTC4311 and no I2C pull-ups (the Ctrl Module has them) |
| Expansion Teensy Carrier | KC-01-EX23 | Teensy 4.1 on headers; EXT target port; local I2C master with pull-ups to 3.3 V; display slots S1 and S2 (16-pin, Hardware Reference §12), each with its own INT line and a shared RST output; 2 × 10-pin headers to the keyboard PCB; encoder header; NeoPixel output; USB from hub port 1 |
| Keyboard Matrix PCB | KC-01-EX21 | Unchanged from v0.3 (§7.4) |
| Encoder PCB | KC-01-EX22 | Unchanged from v0.3 (§7.5) |
| Left and right displays | KC-01-1911 (×2) + 7" panels | Standard 7" TFT Display Driver with ER-TFT070A2-6-5633 1024×600 panel and capacitive touch. No new board. **KC-01-EX30 and EX31 are retired** |

Whether EX10 and EX23 stay two boards or merge into one is left to the board layout (E-12).

### 5.2 Power

All power comes from V_12 on the GX16-10, which is the main controller's 12 V supply.

| Consumer | 12 V typical | 12 V peak | Notes |
|----------|-------------|-----------|-------|
| Left display driver | 0.23 A | 0.25 A | Power Budget 1.3, 7" carrier figure (480 / 520 mA at 5 V) |
| Right display driver | 0.23 A | 0.25 A | As above |
| Expansion Teensy, hub, TUSB212, keypad backlight | ≈ 0.14 A | ≈ 0.50 A | ≈ 300 mA typical and ≈ 1.1 A peak at 5 V; the peak is all 25 keys at full white |
| **Total** | **≈ 0.6 A (7 W)** | **≈ 1.0 A (12 W)** | |

The 12 V supply (10 A) has 4.18 A peak headroom with panels A and B, and 2.87 A with the planned Panel C growth (Power Budget §3). The expansion fits within either.

The keypad's peak load (≈ 1.1 A at 5 V) is close to one MPM3610's 1.2 A rating. Either the firmware caps the key backlight brightness, or the routing board uses a larger 5 V converter (E-02).

---

## 6. USB and Simpit

| Parameter | Value |
|-----------|-------|
| Hub IC | CH334F (WCH), LCSC C5187527, self-powered, 12 MHz crystal (C9002) |
| Upstream | USB-C panel mount, through the TUSB212 (LCSC C702372) |
| Port 1 | Expansion Teensy 4.1 |
| Port 2 | Left display driver |
| Port 3 | Right display driver |
| Port 4 | Spare |

Three USB devices make the hub necessary, so open item E-09 (bypass the hub) is closed.

**VBUS:** not connected at either panel mount. The hub's V5 comes from the internal 5 V rail. Each Teensy is powered through VIN, with its Micro-USB VCC not connected, as in the main controller.

**Kerbal Simpit:** each of the three devices has its own Simpit link, so `Settings.cfg` grows from five ports to eight. The Technical Information Manual's setup section is updated when the expansion is built.

---

## 7. Keyboard Subsystem

### 7.1 Physical Layout

The keyboard follows the Apollo DSKY layout, with a function row added at the top. All keys are **Kailh Choc V1** switches (tactile) with **MBK keycaps** with custom legends (FKcaps) on a **21 mm** grid (E-17).

```
Col:   1       2      3      4      5      6      7
     [PROG]  [HOLD] [MENU] [INFO] [MARK] [PAGE] [ENC↕]  ← Function row (row 0)
     [VERB]  [ + ]  [ 7 ]  [ 8 ]  [ 9 ]  [CLR] [ENTR]  ← DSKY rows
     [NOUN]  [ - ]  [ 4 ]  [ 5 ]  [ 6 ]  [PRO] [RSET]
             [ 0 ]  [ 1 ]  [ 2 ]  [ 3 ]  [KYRL]
```

**Key count:** 25 switches and 1 rotary encoder. The encoder takes column 7 of row 0. Row 3 has no key in columns 1 and 7.

**Key functions.** The functions below follow D6. The detailed mapping, including which keys select a mode and how a setpoint is chosen, is open item E-11.

| Key | Function |
|-----|----------|
| PROG | Select a program: an autopilot (ascent, aircraft, rover, orbital, landing) or an expansion program |
| VERB / NOUN | Verb and noun entry for the expansion programs, and shortcuts to autopilot actions |
| 0–9, +, − | Numeric and sign entry |
| CLR | Clear the current entry |
| ENTR | Enter: confirm a value |
| PRO | Proceed: engage or disengage the highlighted mode, ARM / DISARM, EXEC a planned burn, or execute the current verb |
| KEY REL | Key release: dismiss the current prompt |
| RSET | Reset: clear a fault or alarm shown on the expansion |
| HOLD | Pause an expansion program and keep its state. Not the panel HOLD button |
| MENU | Open the touch menu on the right display |
| INFO | Show the extended parameters behind the current page |
| MARK | Capture the current state (an orbit snapshot or a reference for a maneuver) |
| PAGE | Cycle the left display's view for the current program |
| ENC | Turn: select or adjust the highlighted item. Press: confirm |

### 7.2 Matrix Wiring

The 25 switches are wired as a 4-row × 7-column matrix with one diode per key (1N4148W, SOD-123): anode to the switch, cathode to the row line. The Teensy drives the columns as outputs (active low) and reads the rows as inputs (internal pull-up; a pressed key reads low).

| Parameter | Value |
|-----------|-------|
| Row lines | 4 (ROW_0–ROW_3) |
| Column lines | 7 (COL_0–COL_6) |
| Teensy GPIO for the matrix | 11 |
| Teensy GPIO for the encoder | 3 (A, B, push) |
| Teensy GPIO for the NeoPixels | 1 |

### 7.3 Per-Key RGB Backlight

One SK6812MINI-EA (LCSC C5378731) per key sits reverse-mounted on the back of the keyboard PCB and shines through a cutout into the Choc V1 housing. The LEDs are daisy-chained DOUT → DIN from LED_0 (PROG), left to right and top to bottom. LED_DIN comes in from the Teensy carrier on J1; LED_DOUT leaves on J2 for future chaining. Each LED has a 100 nF decoupling capacitor (C1525) placed close to VDD.

### 7.4 Keyboard PCB (KC-01-EX21)

| Parameter | Value |
|-----------|-------|
| Board dimensions | 147 mm × 100 mm |
| Key grid | 21 mm × 21 mm |
| Switch footprint | Kailh Choc V1, direct solder (hand solder after JLCPCB assembly) |
| LED placement | SK6812MINI-EA reverse mount on the back, cutout per key |
| Connectors | J1 (top) and J2 (bottom): HCTL PM254-2-10-Z-8.5 (C2897411) |
| Matrix | 4 rows × 7 columns, 25 positions populated |

**J1 (top header):**

| Pins | Signal |
|------|--------|
| 1–2 | COL_0, COL_1 |
| 3–4 | COL_2, COL_3 |
| 5–6 | COL_4, COL_5 |
| 7–8 | COL_6, LED_DIN |
| 9–10 | ROW_0, ROW_1 |
| 11–12 | 5 V, GND |
| 13–20 | NC, GND (distributed) |

**J2 (bottom header):**

| Pins | Signal |
|------|--------|
| 1–2 | ROW_2, ROW_3 |
| 3–4 | LED_DOUT, 5 V |
| 5–8 | GND (×4) |
| 9–16 | NC (×8) |
| 17–20 | GND (×4) |

**BOM:**

| Ref | Component | LCSC # | Qty | Assembly |
|-----|-----------|--------|-----|----------|
| SW1–SW25 | Kailh Choc V1 tactile | Self-source | 25 | Hand solder |
| D1–D25 | 1N4148W SOD-123 | C81598 | 25 | SMT (JLCPCB) |
| LED1–LED25 | SK6812MINI-EA | C5378731 | 25 | SMT (JLCPCB) |
| C1–C25 | 100 nF 16 V X7R 0402 | C1525 | 25 | SMT (JLCPCB) |
| J1, J2 | HCTL PM254-2-10-Z-8.5 | C2897411 | 2 | Hand solder |

### 7.5 Encoder PCB (KC-01-EX22)

| Parameter | Value |
|-----------|-------|
| Encoder | PEC11R-4220F-S0024 (Bourns), LCSC C143797 |
| Shaft | 20 mm flatted, 2 mm bushing |
| Detents | 24 per revolution, with push switch |
| Panel | 3 mm cover |
| Mounting | Height adjustable independently of the keyboard PCB |

---

## 8. Display Subsystem

### 8.1 Hardware

Both displays use the main controller's standard 7" display: a KC-01-1911 driver (Teensy 4.1, LT7683, local MPM3610 / AP2112K), an ER-TFT070A2-6-5633 1024×600 panel with capacitive touch, and a microSD card for assets. Each runs its own firmware, built on `KerbalDisplayCommon` like the other displays, and keeps its own Simpit link for the telemetry it draws.

On the local bus, each driver is a display carrier (I2C spec §15). It answers status reads, raises its INT line for a touch event, and takes control bytes and content frames from the expansion Teensy. The expansion Teensy can reset both drivers through RST on the display slots.

Both displays run in portrait (D7). The display library already sets the panel rotation at start-up (`kcmDisplayBegin`), so the expansion firmware selects a portrait rotation and maps the touch coordinates to match. The panel's best viewing direction changes when it is turned on its side, which should be checked on the bench (E-18).

### 8.2 Roles

| Display | Role |
|---------|------|
| **Left** | Graphical: orbit diagram, trajectory, maneuver and target views, and the flight director for the selected program. PAGE cycles the views |
| **Right** | Program and autopilot pages: the PROG / VERB / NOUN indicators, the selected autopilot's page (modes, setpoints, status, the disconnect reason, like today's consoles), or the expansion program's data registers |

A DSKY-style layout of the right display (PROG, VERB and NOUN boxes, registers R1–R3, status lamps) is available to the expansion programs that ask for it.

### 8.3 Touch

Touch is secondary: settings (brightness, layout), the MENU key's menus, and soft buttons for functions that do not warrant a hard key. Flight-critical actions always have a key.

---

## 9. I2C Addresses and the EXT Protocol

### 9.1 Addresses

| Address | Device | Bus | Notes |
|---------|--------|-----|-------|
| 0x15 | Expansion Teensy 4.1 | EXT | Proposed. It follows the display carriers (0x10–0x14) so that logs and tools never show two devices at one address |
| 0x16 | Left display driver | Local | Proposed |
| 0x17 | Right display driver | Local | Proposed |

The keypad and encoder are wired directly to the expansion Teensy's pins and have no I2C address.

### 9.2 EXT Protocol (outline)

The protocol follows the display-carrier pattern (I2C spec §15). The frames are defined in a new I2C spec section when the firmware is designed (E-10).

| Direction | Frame | When |
|---|---|---|
| Master → expansion | Control (debug, demo, flight scene), as for the carriers | Every 500 ms, as a heartbeat |
| Master → expansion | Autopilot status 0xA5–0xA9 and summary 0xAA, as sent to Information Display 2 today | On change, at the console rates |
| Master reads expansion | Status: ready, Simpit link, and the number of commands queued | Every poll |
| Master reads expansion | Command frame: the console command format, one per read | While EXT_INT_1 is low |

- **EXT_INT_1** (expansion → master): the expansion pulls it low while it has commands queued, and the master reads them at once instead of waiting for the next poll.
- **EXT_INT_2** (master → expansion): the master pulses it when it has written new autopilot state. It is optional, because the frames arrive by write anyway, so it is kept for a later use.
- **If the expansion stops answering,** the master treats it as absent. It stops sending frames and re-probes every 5 s, as it does for the Sys Info Display (`SYSINFO_REPROBE_MS`). The expansion is optional, so its absence is not an error at boot.

---

## 10. Open Items

| # | Item | Type |
|---|------|------|
| E-02 | 5 V budget on the routing board: keypad backlight peak against one MPM3610 (1.2 A); a brightness cap or a larger converter | Power |
| E-05 | Closed by D3: 7" ER-TFT070A2-6-5633 panel on the KC-01-1911 driver | — |
| E-06 | Content frames from the expansion Teensy to the two displays (page selection, values, DSKY registers), on top of the I2C spec §15 carrier protocol | Firmware |
| E-07 | Closed: the master is bus master on EXT (D2). The expansion address is proposed as 0x15 (§9.1) | — |
| E-09 | Closed: three USB devices keep the hub (§6) | — |
| E-10 | EXT frame definitions and the master-side EXT service (`Wire1`, EXT_INT_1 / 2); a new I2C spec section | Firmware |
| E-11 | Key mapping for the autopilots: PROG numbers, mode selection, setpoint entry and engage (D6) | Firmware / UI |
| E-12 | Enclosure for layout A (D7): two portrait 7" displays over the keypad, about 260 × 305 mm; depth; whether EX10 and EX23 merge into one board | Mechanical |
| E-13 | EXT bus speed over the ≈ 1 m GX16-10 cable: confirm 400 kHz with the LTC4311, or fall back to 100 kHz | Bench check |
| E-14 | Kerbal Simpit with eight ports: KSP-side load, and the `Settings.cfg` layout | Bench check |
| E-15 | The expansion program list | Design |
| E-16 | Retire the Information Display 2 consoles after the expansion's bench checks (§4.4): InfoDisp firmware, display references, TIM | Firmware / docs |
| E-17 | MBK keycaps with custom legends (FKcaps): legend method and whether a legend can shine through for the per-key RGB; key spacing (MBK caps are sized for 18 × 17 mm Choc spacing; the keyboard PCB uses a 21 mm grid) | Hardware |
| E-18 | Portrait viewing angle of the 7" panel; touch coordinate rotation | Bench check |
| E-19 | Program numbers for the autopilots and the expansion programs | Design |

---

## 11. Not in Scope

- The main controller's hardware. The GX16-10, USBC2, EXT bus and EXT_INT lines are already allocated.
- Panel C, the planned button and display growth panel, which is separate from this unit.
- The Kerbal Simpit plugin: extra serial ports need no plugin changes.

In scope later, when the firmware is designed: the master's EXT service (E-10) and the Information Display 2 console retirement (E-16).

---

## 12. Revision History

| Version | Date | Description |
|---------|------|-------------|
| 0.1 | 2026-05-23 | Initial draft: architecture, connectivity, board topology, open items |
| 0.2 | 2026-05-23 | Major revision. Display processors changed from Teensy 4.0 to RP2040; display driver from RA8875 to SSD1963; keyboard from ATtiny816 shift-register modules to a direct Teensy 4.1 GPIO matrix; keypad reduced from 28 to 25 keys plus 1 encoder; external interface from 50-pin IDC to GX16-10 plus USB-C panel mount; expansion Teensy 4.1 made I2C master on SDA_EXT; TUSB212 USB signal conditioner added; LTC4311 on SDA_EXT / SCL_EXT at both ends; series resistors and RST RC filter documented. Board designators assigned (KC-01-EX10 to EX31); keyboard PCB BOM finalized; display roles confirmed (left graphical, right data / program); DSKY layout finalized with the function row. Open items E-01, E-03, E-04 and E-08 closed |
| 0.3 | 2026-05-23 | Signal architecture corrections. A single LTC4311 on SDA_EXT / SCL_EXT, on the Ctrl Module only; the I2C lines are passive at the expansion end. Pull-ups placed: 4.7 kΩ on EXT_INT_1 at the Ctrl Module, 4.7 kΩ on EXT_INT_2 and 10 kΩ on RST at the Expansion Routing Board. The full signal path is documented per signal |
| 0.4 | 2026-11-21 | Brought in line with the controller as built (decisions D1–D6, §2.1). The master keeps all autopilot logic, and the expansion is its autopilot interface and runs its own non-flying programs. The master is I2C master on EXT, the expansion a target at 0x15 (proposed), with EXT_INT_1 as "poll me". The displays are two standard 7" TFT Display Drivers (KC-01-1911); EX30 and EX31 retired. The expansion Teensy and both display drivers each have a Simpit link (eight Simpit ports in all). Autopilot consoles move from Information Display 2 to the expansion after its bench checks; keys plus console pages. Power re-estimated (≈ 0.6 A typical, ≈ 1.0 A peak at 12 V). Open items E-05, E-07 and E-09 closed; E-10 to E-16 added. Then, from the mockup review: layout A with portrait displays (D7), no A/P OFF on the expansion (D8), MBK keycaps from FKcaps; E-17 to E-19 added |
