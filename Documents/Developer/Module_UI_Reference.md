# Kerbal Controller Mk1 — Module UI Reference

**Jeb's Controller Works** | v6.1 | 2026-10-27

| | |
|---|---|
| **Document type** | Developer reference |
| **Location** | `Documents/Developer/Module_UI_Reference.md` |
| **Covers** | Every control on every panel: what each button, switch, stick, encoder and display does, its LED, and where it sits |
| **Master firmware** | `Software/KCMk1_Master` (v0.8.1) |
| **Module firmware** | `Software/Modules/` |

---

## At a glance

This is the reference for what the pilot touches and sees. It is rebuilt for v6.0 from the confirmed panel review.

- **Sources, highest first:**
  1. `Hardware_Inventory.md` §7, §8, §10 and §11, confirmed by the owner panel by panel;
  2. `Master_Firmware_Design.md` §8–§11, including the rules marked *(stage n)*;
  3. the master firmware in `Software/KCMk1_Master/`;
  4. the module firmware in `Software/Modules/`, for what each module does on its own.
- **Where they disagree,** this document gives the confirmed behaviour and lists the gap in §15 or §16.
- **Most behaviour lives in the master.** A button module only reports its buttons and shows the LED state the master sends. The exceptions are the GPWS and Pre-Warp modules, which manage their own displays and buttons, and the Throttle Module's lever motor.

---

## Contents

1. [Required KSP mods](#1-required-ksp-mods)
2. [Conventions](#2-conventions)
3. [Shared rules](#3-shared-rules)
4. [Custom action groups](#4-custom-action-groups)
5. [Panels](#5-panels)
6. [Switches](#6-switches)
7. [Safe lines: ABORT and STAGE](#7-safe-lines-abort-and-stage)
8. [Control modes](#8-control-modes)
9. [Button modules](#9-button-modules)
10. [Joysticks](#10-joysticks)
11. [Display-managed modules: GPWS and Pre-Warp](#11-display-managed-modules-gpws-and-pre-warp)
12. [Throttle Module](#12-throttle-module)
13. [Dual Encoder](#13-dual-encoder)
14. [Demo mode](#14-demo-mode)
15. [Firmware and label gaps](#15-firmware-and-label-gaps)
16. [Open items](#16-open-items)
17. [Removed in v6.0](#17-removed-in-v60)
18. [Revision history](#18-revision-history)

---

## 1. Required KSP mods

| Mod | Purpose |
|---|---|
| **AGX (Action Groups Extended)** | Custom action groups beyond KSP's ten. Every CAG button needs it. The highest group the controller uses is 239 (§4) |
| **Hullcam VDS Continued** | Mountable external cameras, for Dual Encoder ENC2. Default keys: `-` next camera, `=` previous camera, `Backspace` reset. Check that the in-game bindings still match (§16) |

KerbalSimpit is the link to KSP. Simpit's keyboard emulator only reaches KSP while KSP has focus.

---

## 2. Conventions

### 2.1 Input numbering

**Bn** is the module firmware's input index. The schematic nets count from 1: Bn is net `BUTTON`(n + 1), so B0 = `BUTTON01`. It matches the silkscreen (SW12–SW15 on the Button Module, B16–B23 on the Wide Button Module).

| Board | B0–B11 | B12–B15 | B16–B23 |
|---|---|---|---|
| Button Module (KC-01-1801 / 1802) | 12 RGB buttons | Screw terminals CN1 / CN2 | — |
| Wide Button Module (KC-01-1811 / 1812) | 12 RGB buttons | Not connected | Screw terminals U16 / U17 (Switch Groups) |

Board numbers are given as *schematic / PCB*.

### 2.2 Button layout

Both grids are viewed from the front of the panel. They were confirmed against the built panels.

**Button Module, 2 × 6:**

| | C6 | C5 | C4 | C3 | C2 | C1 |
|---|---|---|---|---|---|---|
| **R1** | B10 | B8 | B6 | B4 | B2 | B0 |
| **R2** | B11 | B9 | B7 | B5 | B3 | B1 |

**Wide Button Module, 6 × 2:**

| | C1 (left) | C2 (right) |
|---|---|---|
| **R1** | B1 | B0 |
| **R2** | B3 | B2 |
| **R3** | B5 | B4 |
| **R4** | B7 | B6 |
| **R5** | B9 | B8 |
| **R6** | B11 | B10 |

So B0 is always the top-right button.

### 2.3 Labels

- **[GROUP]** is a group label printed on the panel, not on the button. For example, [WARP TO] above a row.
- **CAPITALS** are the exact button label.
- **(…)** is a description of the function, not a label.

### 2.4 LED states

The states are defined in KerbalModuleCommon. The master sends one per button.

| State | Appearance | Meaning | Used by the master |
|---|---|---|---|
| OFF | Dark | Not usable now; a press is ignored | Yes |
| ENABLED | Dim warm white (`KMC_BACKLIT`) | Usable now | Yes |
| ACTIVE | Full per-button colour | Function on, or a press being acknowledged | Yes |
| CUT | Static red | State-machine end state: chute cut, heat shield released | Yes (Vehicle Control) |
| WARNING | Amber, flashing 500 ms | Attention | No |
| ALERT | Red, flashing 150 ms | Immediate action | No |
| ARMED | Static cyan | Primed | No |
| PARTIAL_DEPLOY | Static amber | Partly deployed | No |
| ACTIVE_ALT | Second per-button colour | Two-colour buttons | No (its only user, the AUX CP toggle, is gone) |

- **Discrete LEDs** (the Throttle Module lamps) are on or off only.
- **LTG TEST** (Switch Group 1) overrides every LED while it runs.

### 2.5 Colour palette

From `KerbalModuleCommon.h`. Module sketches use the same names with a `KBC_` or `KJC_` prefix.

| Name | RGB | | Name | RGB |
|---|---|---|---|---|
| BACKLIT | 15, 8, 2 | | SEAFOAM | 80, 200, 160 |
| WHITE | 255, 255, 255 | | BLUE | 0, 0, 255 |
| WHITE_WARM | 255, 140, 100 | | SKY | 14, 165, 233 |
| GREEN | 0, 255, 0 | | TEAL | 20, 184, 166 |
| RED | 255, 0, 0 | | CYAN | 6, 182, 212 |
| AMBER | 255, 80, 0 | | PURPLE | 120, 0, 220 |
| ORANGE | 255, 60, 0 | | INDIGO | 60, 0, 200 |
| YELLOW | 234, 179, 8 | | VIOLET | 160, 0, 255 |
| GOLD | 255, 160, 0 | | CORAL | 255, 100, 54 |
| CHARTREUSE | 163, 230, 53 | | ROSE | 255, 80, 120 |
| LIME | 132, 204, 22 | | PINK | 236, 72, 153 |
| MINT | 100, 255, 160 | | MAGENTA | 255, 0, 255 |

WHITE_COOL, WHITE_NEUTRAL and WHITE_SOFT also exist; no module uses them.

---

## 3. Shared rules

These apply to every panel unless its section says otherwise (design §8.1).

### 3.1 When a press acts

- **A press reaches KSP only when the panels are live:** a flight scene, the Simpit link up, demo off.
- **Most buttons act on the press.** A quick press that came and went between two reads (a *tap*) counts.
- **The Vehicle Control state machines act on release,** so a 2 s hold can be a reset instead (§9.5).
- **Held keys** (INVERT's F, LOAD's F9, the EVA keys) are released when the flight ends, the link drops or demo starts.

### 3.2 LEDs

| LED | When |
|---|---|
| **Dark** | The button cannot be used now, and its press is ignored. For example: EVA buttons outside EVA, SAS modes with SAS OFF, chutes with CHUTE SAFE, a WARP TO with no event |
| **Backlit** | Usable now |
| **Lit (ACTIVE)** | KSP reports the function on |
| **Flash (ACTIVE 150 ms)** | Buttons whose state KSP does not report flash on a press |

- **Before KSP reports the action groups** in a new flight, every CAG button shows backlit. The master asks for CAGSTATUS, ACTIONSTATUS and SAS_MODE_INFO on entering a flight.
- **Each panel sets all its LEDs every pass** from the current state, so a LED never shows a stale state. The bus sends only changes, at most every 50 ms per module.

### 3.3 Module lifecycle

| Situation | Modules |
|---|---|
| Boot, KSP not connected, not in demo | DISABLED (dark) |
| Flight scene entered | All ENABLEd |
| Flight scene left, or KSP lost | All DISABLEd |
| Kerbal on EVA | Stay ENABLEd; each panel decides what works |
| Demo on | All ENABLEd, whatever the scene |

**Switch modules keep working while DISABLED.** Function, Vehicle and Time Control carry the switches and the mode selector, so the master polls them every 100 ms while they are not ACTIVE. Demo, debug, the interlocks and the control mode follow their switches in any scene, menus included.

### 3.4 Action groups: toggle or activate

- **Plain CAG buttons send a toggle** (`CAGTOGGLE`), and light while KSP reports the group active.
- **The state machines send activate only** (`CAGACTIVATE`), and deactivate for the 2 s reset.
- **The control points** (AUX PRI / ALT) deactivate the other point, then activate their own.

---

## 4. Custom action groups

### 4.1 Numbering rule

**`CAG = base + (control group − 1) × 40`**, with the control group 1–6 from Dual Encoder ENC1 (§13).

- **Exception: Action Control's AG 1–12 are never offset.** They always send CAG 1–12, whatever the control group.
- **Everything else is offset:** Vehicle Control (including the heat-shield and parachute state machines), Function Control, and AUX CTRL's UNDOCK, PRI and ALT.
- **In code:** `cagNumber(base)` applies the offset and `cagFixed(n)` does not (`cag.h`).

### 4.2 Control groups

The names are a suggested use; nothing in the firmware depends on them.

| Group | Offset | Suggested use |
|---|---|---|
| 1 | 0 | Default: main craft |
| 2 | 40 | Lander 1 |
| 3 | 80 | Lander 2 |
| 4 | 120 | Custom |
| 5 | 160 | Re-entry vehicle |
| 6 | 200 | Space station |

The Annunciator shows the current group.

### 4.3 Assignment table

**Action Control (no offset):** AG 1–12 send CAG 1–12 in every control group (§9.3).

**Offset groups:**

| Base | Function | Module | Input | Grp 1 | Grp 2 | Grp 3 | Grp 4 | Grp 5 | Grp 6 |
|---|---|---|---|---|---|---|---|---|---|
| 13 | ANTENNA | Vehicle Control | B2 | 13 | 53 | 93 | 133 | 173 | 213 |
| 14 | FUEL CELL | Vehicle Control | B4 | 14 | 54 | 94 | 134 | 174 | 214 |
| 15 | SOLAR ARRAY | Vehicle Control | B5 | 15 | 55 | 95 | 135 | 175 | 215 |
| 16 | CARGO DOOR | Vehicle Control | B6 | 16 | 56 | 96 | 136 | 176 | 216 |
| 17 | RADIATOR | Vehicle Control | B7 | 17 | 57 | 97 | 137 | 177 | 217 |
| 18 | LADDER | Vehicle Control | B9 | 18 | 58 | 98 | 138 | 178 | 218 |
| 19 | HEAT SHIELD deploy | Vehicle Control | B8 | 19 | 59 | 99 | 139 | 179 | 219 |
| 20 | HEAT SHIELD release | Vehicle Control | B8 | 20 | 60 | 100 | 140 | 180 | 220 |
| 21 | [PARACHUTE] MAIN deploy | Vehicle Control | B10 | 21 | 61 | 101 | 141 | 181 | 221 |
| 22 | [PARACHUTE] MAIN cut | Vehicle Control | B10 | 22 | 62 | 102 | 142 | 182 | 222 |
| 23 | [PARACHUTE] DROGUE deploy | Vehicle Control | B11 | 23 | 63 | 103 | 143 | 183 | 223 |
| 24 | [PARACHUTE] DROGUE cut | Vehicle Control | B11 | 24 | 64 | 104 | 144 | 184 | 224 |
| 25 | LES | Function Control | B1 | 25 | 65 | 105 | 145 | 185 | 225 |
| 26 | FAIRING JETT | Function Control | B0 | 26 | 66 | 106 | 146 | 186 | 226 |
| 27 | [ENGINE] ALT | Function Control | B7 | 27 | 67 | 107 | 147 | 187 | 227 |
| 28 | [SCIENCE] COLLECT | Function Control | B6 | 28 | 68 | 108 | 148 | 188 | 228 |
| 29 | [ENGINE] GROUP 1 | Function Control | B9 | 29 | 69 | 109 | 149 | 189 | 229 |
| 30 | [SCIENCE] GROUP 1 | Function Control | B8 | 30 | 70 | 110 | 150 | 190 | 230 |
| 31 | [ENGINE] GROUP 2 | Function Control | B11 | 31 | 71 | 111 | 151 | 191 | 231 |
| 32 | [SCIENCE] GROUP 2 | Function Control | B10 | 32 | 72 | 112 | 152 | 192 | 232 |
| 33 | AIR INTAKE | Function Control | B3 | 33 | 73 | 113 | 153 | 193 | 233 |
| 34 | LOCK SURFACES | Function Control | B2 | 34 | 74 | 114 | 154 | 194 | 234 |
| 35 | [CTRL PT] PRI | AUX CTRL | B10 | 35 | 75 | 115 | 155 | 195 | 235 |
| 36 | [CTRL PT] ALT | AUX CTRL | B11 | 36 | 76 | 116 | 156 | 196 | 236 |
| 37 | UNDOCK | AUX CTRL | B8 | 37 | 77 | 117 | 157 | 197 | 237 |
| 38 | AIR BRAKE | Function Control | B5 | 38 | 78 | 118 | 158 | 198 | 238 |
| 39 | FLAPS | Function Control | B4 | 39 | 79 | 119 | 159 | 199 | 239 |

**Notes:**
- **CAG numbers stay with the function.** The v6.0 Function Control remap changes which button sends a group, not which group a function uses, so vessels' action-group setups are unchanged.
- **State-machine pairs** use consecutive bases: deploy is the lower number, cut / release the higher.
- **Base 1–12 in groups 2–6** (41–52, 81–92, …) are not used, because Action Control is not offset.
- **Highest number used:** 239.
- **Info Display AIRBRK indicator:** the AIRCRAFT screen shows the air brake from CAG `AIRBRAKE_CAG` (default 38, in the InfoDisp `AAA_Config.ino`; 0 turns it off), with the same control-group offset as the AIR BRAKE button. The master sends the control group in byte 8 of the Info Display summary frame (InfoDisp v1.17).

---

## 5. Panels

The controller has four panels. A1 and B1 are display panels; A2 and B2 are input panels. The *line* is the module's interrupt line to the master (Inventory §2–§3).

### 5.1 A1 — left display panel

| Item | Board | Where | Line |
|---|---|---|---|
| Information Display 1 (0x12) — panel hub | 7" TFT Display Driver (1911 / 1912) | CN1 → Ctrl Module CN4 | INT_12 |
| Annunciator (0x10) | 7" TFT Display Driver (1911 / 1912) | Information Display 1, slot P2 | INT_9 |
| Power button | — | Ctrl Module H1 (soft latch) | None: the master cannot see it or switch the controller off |

- **Free:** Information Display 1 slots P3 (INT_10) and P4 (INT_11).
- **Safe switch:** none; INT_SAFE2 is unused.

### 5.2 A2 — left input panel

Panel Hub on Ctrl Module CN3.

| Slot | Line | Module | Board | Address |
|---|---|---|---|---|
| P1 | INT_1 | Action Control | Button (1801) | 0x22 |
| P2 | INT_2 | Sys Info Display | 2" TFT Display Driver (1901) | 0x14 |
| P3 | INT_3 | Throttle | Throttle (1871) | 0x2C |
| P4 | INT_4 | UI Control | Button (1801) | 0x20 |
| P5 | INT_5 | — empty — | | |
| P6 | INT_6 | GPWS | 7-Segment (1841) | 0x2A |
| P7 | INT_7 | Translation joystick | Joystick (1831) | 0x29 |
| P8 | INT_8 | Function Control | Wide Button (1811) | 0x21 |

| Panel item | Wired to | Section |
|---|---|---|
| ABORT button (arming switch in series) | Panel Hub safe-switch terminal → INT_SAFE | §7 |
| Switch Group 1 (8 switches) | Function Control U16 / U17 → B16–B23 | §6.2 |
| Throttle buttons 100 %, UP, DOWN, 0 % | Throttle terminals U7, U8, U10, U11 | §12 |
| Translation stick | Translation joystick terminal U6 | §10.2 |

**Unused:** slot P5; the Action Control and UI Control terminals (B12–B15); Throttle THRTL_ENA (terminal U5).

### 5.3 B1 — right display panel

| Item | Board | Where | Line |
|---|---|---|---|
| Resource Display (0x11) — panel hub | 7" TFT Display Driver (1911 / 1912) | CN1 → Ctrl Module Extension CN5 | B_INT_12 |
| Information Display 2 (0x13) | 7" TFT Display Driver (1911 / 1912) | Resource Display, slot P2 | B_INT_9 |
| Dual Encoder (0x2D) | Dual Encoder (1861) + 2 × Encoder (1851) | Resource Display, slot P4 | B_INT_11 |

- **Free:** Resource Display slot P3 (B_INT_10).
- **Safe switch:** none; B_INT_SAFE2 is unused. The Dual Encoder's own safe-switch terminal cannot reach the master (Inventory §5.8).

### 5.4 B2 — right input panel

Panel Hub on Ctrl Module Extension CN4.

| Slot | Line | Module | Board | Address |
|---|---|---|---|---|
| P1 | B_INT_1 | — empty — | | |
| P2 | B_INT_2 | Pre-Warp Time | 7-Segment (1841) | 0x2B |
| P3 | B_INT_3 | Time Control | Button (1801) | 0x25 |
| P4 | B_INT_4 | Vehicle Control | Wide Button (1811) | 0x24 |
| P5 | B_INT_5 | — empty — | | |
| P6 | B_INT_6 | Stability Control | Button (1801) | 0x23 |
| P7 | B_INT_7 | AUX CTRL | Button (1801) | 0x26 |
| P8 | B_INT_8 | Rotation joystick | Joystick (1831) | 0x28 |

| Panel item | Wired to | Section |
|---|---|---|
| STAGE button (momentary, arming switch in series) | Panel Hub safe-switch terminal → B_INT_SAFE | §7 |
| Control mode selector SPC / PLN / RVR | Time Control terminal CN2 → B12, B13 | §6.6 |
| Switch Group 2 (8 switches) | Vehicle Control U16 / U17 → B16–B23 | §6.5 |
| Rotation stick | Rotation joystick terminal U6 | §10.1 |

**Unused:** slots P1 and P5; the Stability Control terminals (B12–B15); Time Control B14 / B15; the AUX CTRL terminals.

### 5.5 Address summary

| Address | Module | Type ID | Panel / slot | Board | Library |
|---|---|---|---|---|---|
| 0x20 | UI Control | 0x01 | A2 P4 | Button 1801 / 1802 | KerbalButtonCore |
| 0x21 | Function Control | 0x02 | A2 P8 | Wide Button 1811 / 1812 | KerbalButtonCore (24 inputs) |
| 0x22 | Action Control | 0x03 | A2 P1 | Button 1801 / 1802 | KerbalButtonCore |
| 0x23 | Stability Control | 0x04 | B2 P6 | Button 1801 / 1802 | KerbalButtonCore |
| 0x24 | Vehicle Control | 0x05 | B2 P4 | Wide Button 1811 / 1812 | KerbalButtonCore (24 inputs, extended states) |
| 0x25 | Time Control | 0x06 | B2 P3 | Button 1801 / 1802 | KerbalButtonCore |
| 0x26 | AUX CTRL | 0x07 | B2 P7 | Button 1801 / 1802 | KerbalButtonCore |
| 0x28 | Rotation joystick | 0x09 | B2 P8 | Joystick 1831 / 1832 | KerbalJoystickCore |
| 0x29 | Translation joystick | 0x0A | A2 P7 | Joystick 1831 / 1832 | KerbalJoystickCore |
| 0x2A | GPWS Input Panel | 0x0B | A2 P6 | 7-Segment 1841 / 1842 | Kerbal7SegmentCore |
| 0x2B | Pre-Warp Time | 0x0C | B2 P2 | 7-Segment 1841 / 1842 | Kerbal7SegmentCore |
| 0x2C | Throttle | 0x0D | A2 P3 | Throttle 1871 / 1872 | Standalone |
| 0x2D | Dual Encoder | 0x0E | B1 (Resource Display P4) | Dual Encoder 1861 / 1862 | Standalone |

0x27, 0x2E and 0x2F are not used. The displays (0x10–0x14) are covered by their own references.

---

## 6. Switches

### 6.1 The switch rule

These rules apply to both switch groups (Inventory §8.4, design §8.3).

- **A switch acts only when it is flipped.** It never acts by itself.
- **Nothing is enforced.** Entering a flight scene, changing vessel or reading a switch for the first time (power-on, a module coming back) sends nothing to KSP. The first read only sets the master's own state, for example demo, ENGINE or the precision flags.
- **A flip that cannot act** (not in a flight scene, link down, demo) is logged, and nothing is sent.
- **Momentary switches** (MSTR, DISP, LTG) act on their spring-loaded position. A flick between two reads still counts.

**SWITCH ERR.** Five switches have a vessel state to compare: GEAR, BRAKE, EXT LT, SAS and RCS.

| Rule | Detail |
|---|---|
| **Lights when** | A switch disagrees with KSP's report of its action group |
| **Settle time** | It must disagree for 1 s first, so KSP's reply to a flip does not flash it |
| **Clears when** | The switch and the vessel agree again |
| **Checked only** | In a flight scene, with the link up, not in demo, not on EVA, once KSP has sent the action-group status for this scene and Switch Group 2 has been read. On EVA the vessel is the kerbal, whose gear, brakes and SAS are all off |
| **Not checked** | SAS while INVERT is held; SAS while any autopilot or HOLD is engaged; BRAKE while the BRAKES button or the RVR stick button holds the brakes on |
| **Shown on** | The Annunciator (SWITCH ERR). The Sys Info Display names each switch in error, its position and the vessel's state; the `switches` console command lists them all |

CHUTE, THC/RHC, AUDIO and Switch Group 1 have no vessel state, so they never raise SWITCH ERR.

### 6.2 Switch Group 1 (Function Control B16–B23, panel A2)

| Input | Terminal | Switch | Off / on | Type | Does |
|---|---|---|---|---|---|
| B16 | U16-6 | MSTR | OPER / RESET | Momentary | **Master reset:** `CMD_RESET` to every module (each is then returned to its wanted lifecycle and LEDs), `DISPLAY_RESET` to the four 7" displays, every autopilot and HOLD off |
| B17 | U16-5 | DISP | OPER / CLR | Momentary | **Display clear:** GPWS and Pre-Warp values to 0 (`CMD_SET_VALUE` 0); the screens back to their defaults (`DISPLAY_RESET`) |
| B18 | U16-4 | SCE | NORM / AUX | Latching | **Demo mode** off / on (§14) |
| B19 | U16-3 | UPTLM | INHIB / XMIT | Latching | **Debug** off / on: the master's log on USB serial, and every display's own debug output |
| B20 | U17-4 | LTG | NORM / TEST | Momentary | **Bulb test** while held (today a fixed 2 s, §15) |
| B21 | U17-3 | A/P | INHIB / ARM | Latching | **Autopilot master arm** (§6.4) |
| B22 | U17-2 | ENGINE | SAFE / ARM | Latching | **Throttle interlock** (§6.3) |
| B23 | U17-1 | THRTL | STD / FINE | Latching | **Throttle precision** (§12.3) |

Each switch closes from a +5 V pin (U16-1/2, U17-5/6) to its input.

### 6.3 ENGINE SAFE / ARM (B22)

The master tracks three states: SAFE, ARMING and ARMED. Throttle inputs are taken only when ARMED.

| Position | What happens |
|---|---|
| **SAFE** | 0 % throttle to KSP, and the lever is driven to 0 %. Every throttle input is ignored: lever, lever buttons, autopilots. The rover wheel throttle is held at 0; steering still works. An autopilot that needs the throttle cannot engage, and one engaged drops (reason ENG SAFE). If the pilot moves the lever, it is driven back to 0 % on release |
| **Flip to ARM** (ARMING) | 0 % sent again, and the lever driven to 0 %. Still no throttle inputs |
| **ARMED** | Once the lever reports 0 % (within 1 %) after the flip, inputs are taken. With no Throttle Module present, ARMED at once |

- **Before Switch Group 1 is read,** the engine is SAFE.
- **Already on ARM when first read** (power-on, a restart in flight): ARMED at once, with the lever where the pilot left it.
- **The Throttle Module stays enabled in SAFE.** It refuses lever commands while touched or disabled, so the master re-sends the 0 % target every 200 ms until the lever reports it, and waits while the lever is touched.
- **Annunciator:** ENG ARM shows the switch.

### 6.4 A/P INHIB / ARM (B21)

| Position | What happens |
|---|---|
| **INHIB** | No console autopilot can engage or arm; a refused command is shown on KSP's screen ("A/P INHIB: autopilot not engaged"). Flipping to INHIB disconnects every engaged console autopilot (reason A/P INHIB) |
| **ARM** | The Info Display 2 consoles work as normal |

- **HOLD is not affected:** it is not gated by A/P ARM, and A/P INHIB does not drop it (§9.4).
- **Before Switch Group 1 is read,** the A/P is INHIB.
- **Annunciator:** A/P ARM shows the switch (the tile was THRTL ENA; label change pending, §15). AUTOPILOT is lit while any autopilot or HOLD is engaged.

### 6.5 Switch Group 2 (Vehicle Control B16–B23, panel B2)

| Input | Terminal | Switch | Off / on | Type | Does on the flip |
|---|---|---|---|---|---|
| B16 | U16-6 | CHUTE | SAFE / ARM | Latching | ARM enables the parachute buttons; SAFE makes them dark and ignored. Nothing is sent to KSP |
| B17 | U16-5 | GEAR | UP / DOWN | Latching | Gear up / down. The LANDING GEAR button still toggles |
| B18 | U16-4 | BRAKE | REL / LOCK | Latching | Brakes off / on. The BRAKES button holds the brakes on while held; on release they come off only if BRAKE is on REL |
| B19 | U16-3 | EXT LT | OFF / ILLUM | Latching | Lights off / on. The LIGHTS button still toggles |
| B20 | U17-4 | SAS | OFF / ENAB | Latching | SAS off / on. With OFF the SAS mode buttons are dark and ignored |
| B21 | U17-3 | RCS | OFF / ENAB | Latching | RCS off / on |
| B22 | U17-2 | THC/RHC | NORM / PREC | Latching | Stick precision: both sticks scaled to 0.3 (§10.3). Annunciator INPUT PREC |
| B23 | U17-1 | AUDIO | MUTE / LIVE | Latching | Annunciator audio. Annunciator AUDIO |

GEAR, BRAKE, EXT LT, SAS and RCS set KSP's action group to match the new position (activate for on, deactivate for off).

### 6.6 Control mode selector (Time Control B12 / B13, panel B2)

A 3-position switch, PLN in the centre, wired to Time Control terminal CN2 (B12 pin 3, B13 pin 4, +5 V pins 1–2).

| B12 | B13 | Mode |
|---|---|---|
| On | Off | **SPC** (spacecraft) |
| Off | Off | **PLN** (plane), centre |
| Off | On | **RVR** (rover) |
| On | On | Fault: cannot happen on a healthy switch |

- **The fault** is reported as a debug message, not SWITCH ERR (the panel would show no visible discrepancy). While it lasts, the last good mode is kept.
- **Before the selector is read,** the mode is SPC.
- **A mode change** clears the trim, releases HOLD, ends the rotation stick button's current action, and re-sends the control channels.
- **What each mode does** is in §8. The Annunciator shows the mode.

---

## 7. Safe lines: ABORT and STAGE

Each button is wired through its hub's safe-switch terminal, with an arming switch in series. A press only reaches the master while armed; the master cannot see the arming switch itself.

| Button | Panel | Line | Does |
|---|---|---|---|
| **ABORT** | A2 | INT_SAFE (Teensy pin 31) | Rising edge: KSP's ABORT action group. Sent whatever the scene |
| **STAGE** | B2 | B_INT_SAFE (Teensy pin 35) | Rising edge: STAGE, in a flight scene only, with a 1 s lockout so a bounce or double press cannot fire two stages |

- **Read as:** active-high plain inputs, debounced 10 ms.
- **At start-up,** a line already high is not a press.
- **Like everything else,** nothing is sent in demo or with the link down.

Staging is no longer on Stability Control B14 / B15 (§17).

---

## 8. Control modes

The control mode comes from the selector (§6.6). EVA is automatic: it applies whenever KSP reports the active vessel is a kerbal, whatever the selector says.

| | SPC | PLN | RVR | EVA |
|---|---|---|---|---|
| **Rotation stick AXIS1 / 2 / 3** | Yaw / pitch / roll | Roll / pitch / yaw | Steering / wheel throttle / — | D–A / W–S / E–Q keys |
| **Rotation stick button** | A/P disconnect | A/P disconnect | Brakes while held | Left Shift (run) while held |
| **Translation stick** | Translation X / Y / Z | Translation X / Y / Z | Translation X / Y / Z | D–A / W–S / Left Shift–Left Ctrl keys |
| **HOLD (Stability B10)** | Attitude: SAS Stability Assist | Altitude + heading | Heading + speed | Not available |
| **Rotation channel to KSP** | Stick + trim | Stick + trim | 0 | 0 |
| **Wheel channel to KSP** | 0 | 0 | Stick + trim | 0 |

### 8.1 HOLD in each mode

HOLD (Stability B10) is a temporary hold of the current condition (Inventory §8.7, design §8.4, §11.4).

| Mode | Holds | How |
|---|---|---|
| **SPC** | Attitude | Turns SAS on if it was off, and sets Stability Assist. On release SAS goes back to what it was: off, or its previous mode |
| **PLN** | Altitude + heading | Hold autopilot ALT + HDG at the current values. Throttle untouched |
| **RVR** | Heading + speed | Hold autopilot RHDG, plus CRUISE at the current speed when the ENGINE is ARMED. With ENGINE SAFE, heading only (wheel throttle held at 0). ENGINE SAFE later drops CRUISE, and HOLD stays on heading |

- **Engaging HOLD** first drops any console autopilot. If the hold autopilot refuses (PLN / RVR: no atmosphere, not on the ground, stale telemetry), HOLD does not engage.
- **Not gated by A/P ARM.** A/P INHIB and ENGINE SAFE do not release it.
- **Released by:**
  - a second press of HOLD;
  - the A/P disconnect (rotation stick button, SPC / PLN);
  - a console autopilot engaging;
  - the hold autopilot letting go by itself (PLN / RVR);
  - a control-mode change, a vessel change, EVA, leaving the flight or the link dropping;
  - demo, and MSTR RESET.
- **Stick input does not release it** (§8.2).
- **While engaged:** the HOLD LED is ACTIVE, the Annunciator's AUTOPILOT is lit, and SWITCH ERR does not check SAS.

### 8.2 Autopilot authority

An engaged autopilot or HOLD keeps full authority until it is disconnected deliberately (Inventory §8.10, design §11.3). **Moving a stick or the lever never disconnects anything.**

| Disconnected by | Drops |
|---|---|
| A/P disconnect (rotation stick button, SPC / PLN) | Every autopilot and HOLD (reason A/P DISC) |
| A/P INHIB | Every console autopilot; not HOLD |
| A console's A/P OFF | Every console autopilot and HOLD, in every mode |
| HOLD's own button | HOLD |
| ENGINE SAFE | Throttle-using modes: the ascent autopilot, a planned or running burn, IAS / MACH, CRUISE / FOLLOW, landing DESC / HOVR / BRAKE. Attitude modes stay |
| The autopilot's own conditions | No node, SOI change, and so on |

**While anything is engaged:**
- **Both sticks are ignored**, every axis. Camera mode and the stick buttons still work.
- **Throttle:** the autopilot sets it, and the master drives the lever to match. The lever, its buttons and touch are ignored. FINE is dropped while the autopilot owns the throttle and restored after.

---

## 9. Button modules

Each module section gives the panel view, then every input. **LED** gives what the master shows: *dark*, *backlit*, *lit* (ACTIVE, KSP reports it on) or *flash* (ACTIVE for 150 ms on a press). **Colour** is the ACTIVE colour set in the module firmware.

### 9.1 UI Control (0x20)

**Board:** Button Module 1801 / 1802 | **Type:** 0x01 | **Panel:** A2 P4 | **Master:** `panel_ui.cpp`

| | C6 | C5 | C4 | C3 | C2 | C1 |
|---|---|---|---|---|---|---|
| **R1** | [MAP] ENA | [MAP] < | [MAP] > | [MAP] RESET | UI | SCREEN SHOT |
| **R2** | IVA | [SHIP] < | [SHIP] > | CYCLE NAV | NAV BALL | DEBUG |

| Input | Label | Function | Action | Colour |
|---|---|---|---|---|
| B0 | SCREEN SHOT | Screenshot without the UI | Macro: F2, F1, F2, 150 ms apart | GREEN |
| B1 | DEBUG | KSP debug console | Alt+F12 | RED |
| B2 | UI | Toggle the UI | F2 | GREEN |
| B3 | NAV BALL | Toggle the navball | Numpad `.` (not the main Delete key) | AMBER |
| B4 | [MAP] RESET | Map view reset | `'` (apostrophe) | SKY |
| B5 | CYCLE NAV | Navball mode: Surface → Orbit → Target | Simpit navball mode message | GREEN |
| B6 | [MAP] > | Map focus forward | Tab | SKY |
| B7 | [SHIP] > | Next vessel (changes the controlled vessel) | `]` | TEAL |
| B8 | [MAP] < | Map focus back | Shift+Tab | SKY |
| B9 | [SHIP] < | Previous vessel | `[` | TEAL |
| B10 | [MAP] ENA | Map view on / off | M | SKY |
| B11 | IVA | IVA view | C | CORAL |
| B12–B15 | — | Not connected | | |

**LEDs:** all backlit; flash on a press. KSP reports none of these states (map view, UI hidden, navball mode).

### 9.2 Function Control (0x21)

**Board:** Wide Button Module 1811 / 1812 | **Type:** 0x02 | **Panel:** A2 P8 | **Master:** `panel_function.cpp` (buttons), `switches.cpp` (Switch Group 1)

| | C1 (left) | C2 (right) |
|---|---|---|
| **R1** | LES | FAIRING JETT |
| **R2** | AIR INTAKE | LOCK SURFACES |
| **R3** | AIR BRAKE | FLAPS (panel label still RW DISABLE, §15) |
| **R4** | [ENGINE] ALT | [SCIENCE] COLLECT |
| **R5** | [ENGINE] GROUP 1 | [SCIENCE] GROUP 1 |
| **R6** | [ENGINE] GROUP 2 | [SCIENCE] GROUP 2 |

| Input | Label | Function | Base CAG | Colour (by function) |
|---|---|---|---|---|
| B0 | FAIRING JETT | Jettison fairing | 26 | AMBER |
| B1 | LES | Launch escape system | 25 | RED |
| B2 | LOCK SURFACES | Lock control surfaces | 34 | SKY |
| B3 | AIR INTAKE | Air intakes | 33 | TEAL |
| B4 | FLAPS | Flaps | 39 | SEAFOAM |
| B5 | AIR BRAKE | Air brakes | 38 | CYAN |
| B6 | [SCIENCE] COLLECT | Science collect | 28 | PURPLE |
| B7 | [ENGINE] ALT | Alternate engine mode | 27 | ORANGE |
| B8 | [SCIENCE] GROUP 1 | Science group 1 | 30 | INDIGO |
| B9 | [ENGINE] GROUP 1 | Engine group 1 | 29 | YELLOW |
| B10 | [SCIENCE] GROUP 2 | Science group 2 | 32 | BLUE |
| B11 | [ENGINE] GROUP 2 | Engine group 2 | 31 | CHARTREUSE |
| B12–B15 | — | Not connected | | |
| B16–B23 | Switch Group 1 | §6.2 | | |

- **Action:** each button toggles its CAG, with the control-group offset.
- **LEDs:** lit while KSP reports the group active, backlit otherwise.
- **Colours:** each colour stays with its function (Function Control firmware v2.1). FLAPS is SEAFOAM, next to the other aero controls (SKY, TEAL, CYAN).

### 9.3 Action Control (0x22)

**Board:** Button Module 1801 / 1802 | **Type:** 0x03 | **Panel:** A2 P1 | **Master:** `panel_action.cpp`

| | C6 | C5 | C4 | C3 | C2 | C1 |
|---|---|---|---|---|---|---|
| **R1** | AG 1 | AG 2 | AG 3 | AG 4 | AG 5 | AG 6 |
| **R2** | AG 7 | AG 8 | AG 9 | AG 10 | AG 11 | AG 12 |

| Input | Label | CAG | | Input | Label | CAG |
|---|---|---|---|---|---|---|
| B0 | AG 6 | 6 | | B1 | AG 12 | 12 |
| B2 | AG 5 | 5 | | B3 | AG 11 | 11 |
| B4 | AG 4 | 4 | | B5 | AG 10 | 10 |
| B6 | AG 3 | 3 | | B7 | AG 9 | 9 |
| B8 | AG 2 | 2 | | B9 | AG 8 | 8 |
| B10 | AG 1 | 1 | | B11 | AG 7 | 7 |

- **Action:** each button toggles its CAG. **No control-group offset:** these always send CAG 1–12.
- **LEDs:** lit while KSP reports the group active, backlit otherwise. Colour: GREEN for all twelve.
- **B12–B15:** not connected.

### 9.4 Stability Control (0x23)

**Board:** Button Module 1801 / 1802 | **Type:** 0x04 | **Panel:** B2 P6 | **Master:** `panel_stability.cpp`, `hold.cpp`

| | C6 | C5 | C4 | C3 | C2 | C1 |
|---|---|---|---|---|---|---|
| **R1** | HOLD | [SAS] STABILITY ASSIST | [SAS] PROGRADE | [SAS] NORMAL | [SAS] RADIAL IN | [SAS] TARGET |
| **R2** | INVERT | [SAS] MANEUVER | [SAS] RETROGRADE | [SAS] ANTI-NORMAL | [SAS] RADIAL OUT | [SAS] ANTI-TARGET |

| Input | Label | Action | KSP AutopilotMode | Colour |
|---|---|---|---|---|
| B0 | [SAS] TARGET | SAS mode | Target = 7 | GREEN |
| B1 | [SAS] ANTI-TARGET | SAS mode | Anti-Target = 8 | GREEN |
| B2 | [SAS] RADIAL IN | SAS mode | Radial In = 5 | GREEN |
| B3 | [SAS] RADIAL OUT | SAS mode | Radial Out = 6 | GREEN |
| B4 | [SAS] NORMAL | SAS mode | Normal = 3 | GREEN |
| B5 | [SAS] ANTI-NORMAL | SAS mode | Anti-Normal = 4 | GREEN |
| B6 | [SAS] PROGRADE | SAS mode | Prograde = 1 | GREEN |
| B7 | [SAS] RETROGRADE | SAS mode | Retrograde = 2 | GREEN |
| B8 | [SAS] STABILITY ASSIST | SAS mode | Stability Assist = 0 | GREEN |
| B9 | [SAS] MANEUVER | SAS mode | Maneuver = 9 | GREEN |
| B10 | HOLD | HOLD on / off (§8.1) | — | MAGENTA (the usual autopilot colour; Stability Control firmware v2.1) |
| B11 | INVERT | Holds F while held (KSP's SAS invert) | — | AMBER |
| B12–B15 | — | Not connected | | |

**SAS mode buttons (B0–B9):**

| SAS switch | LEDs | Presses |
|---|---|---|
| OFF | All dark | Ignored |
| ENAB | KSP's current mode lit; available modes backlit; unavailable modes dark | Ignored for an unavailable mode |

- **Mode and availability** come from Simpit `SAS_MODE_INFO`: the current mode (255 = SAS off) and the `SASModeAvailability` bitmask (bit = `1 << AutopilotMode`).
- **Until `SAS_MODE_INFO` arrives,** every mode counts as available; KSP ignores a mode it does not allow.
- **The SAS buttons stay dark** while the SAS switch has not been read.

**HOLD (B10):**
- **LED:** ACTIVE while holding; backlit when it can be engaged (a live flight, not EVA); dark otherwise.
- **Behaviour:** §8.1.

**INVERT (B11):**
- Holds F for as long as it is held. SWITCH ERR does not check SAS meanwhile.
- **Dark and ignored on EVA,** where F is GRAB.
- **LED:** ACTIVE while F is held, backlit otherwise.

### 9.5 Vehicle Control (0x24)

**Board:** Wide Button Module 1811 / 1812 | **Type:** 0x05 (extended LED states) | **Panel:** B2 P4 | **Master:** `panel_vehicle.cpp` (buttons), `switches.cpp` (Switch Group 2)

| | C1 (left) | C2 (right) |
|---|---|---|
| **R1** | LIGHTS | BRAKES |
| **R2** | LANDING GEAR | ANTENNA |
| **R3** | SOLAR ARRAY | FUEL CELL |
| **R4** | RADIATOR | CARGO DOOR |
| **R5** | LADDER | HEAT SHIELD |
| **R6** | [PARACHUTE] DROGUE | [PARACHUTE] MAIN |

| Input | Label | Action | LED | Colour |
|---|---|---|---|---|
| B0 | BRAKES | Brakes on while held. On release they come off only if BRAKE is on REL (§6.5) | Lit while KSP reports brakes on | RED |
| B1 | LIGHTS | Lights toggle | Lit while lights on | YELLOW |
| B2 | ANTENNA | CAG 13 toggle | Lit while the group is active | PINK |
| B3 | LANDING GEAR | Gear toggle | Lit while gear down | GREEN |
| B4 | FUEL CELL | CAG 14 toggle | Lit while the group is active | CYAN |
| B5 | SOLAR ARRAY | CAG 15 toggle | Lit while the group is active | GOLD |
| B6 | CARGO DOOR | CAG 16 toggle | Lit while the group is active | TEAL |
| B7 | RADIATOR | CAG 17 toggle | Lit while the group is active | ORANGE |
| B8 | HEAT SHIELD | State machine: deploy CAG 19, then release CAG 20 | Below | GREEN / CUT red |
| B9 | LADDER | CAG 18 toggle | Lit while the group is active | LIME |
| B10 | [PARACHUTE] MAIN | State machine: deploy CAG 21, then cut CAG 22. Needs CHUTE ARM | Below; dark with CHUTE SAFE | GREEN / CUT red |
| B11 | [PARACHUTE] DROGUE | State machine: deploy CAG 23, then cut CAG 24. Needs CHUTE ARM | Below; dark with CHUTE SAFE | GREEN / CUT red |
| B12–B15 | — | Not connected | | |
| B16–B23 | Switch Group 2 | §6.5 | | |

All CAG numbers carry the control-group offset.

**State machines (B8, B10, B11).** The state comes from KSP's report of the two action groups, never from memory in the master.

| Deploy CAG | Cut / release CAG | State | LED | Release (under 2 s) does |
|---|---|---|---|---|
| Inactive | Inactive | READY | Backlit | Activate the deploy CAG |
| Active | Inactive | DEPLOYED | Green (ACTIVE) | Activate the cut / release CAG |
| Either | Active | CUT / RELEASED | Red (CUT) | Nothing |

- **Acts on release.** A release before 2 s is a press, as in the table.
- **2 s hold = reset:** deactivates both groups, returning the button to READY (for example after a repack). It fires nothing else. The chutes need CHUTE ARM for this too.
- **Activate, never toggle.** A press can never undo a deploy.
- **No reset on vessel change.** Each vessel's groups are shown as KSP reports them. CHUTE SAFE → ARM resets nothing; a cut chute stays red.
- **Limit (accepted):** an active group means it was fired, not that the part is deployed. A chute waiting for safe conditions, or one destroyed, still shows DEPLOYED.

**BRAKES and the RVR stick button** share one rule: while either holds the brakes, they are on; when the last one lets go, they come off unless BRAKE is on LOCK. SWITCH ERR does not check BRAKE meanwhile.

### 9.6 Time Control (0x25)

**Board:** Button Module 1801 / 1802 | **Type:** 0x06 | **Panel:** B2 P3 | **Master:** `panel_time.cpp` (buttons, Pre-Warp lead), `switches.cpp` (mode selector)

| | C6 | C5 | C4 | C3 | C2 | C1 |
|---|---|---|---|---|---|---|
| **R1** | PAUSE | [WARP TO] MORN | [WARP TO] SOI | [WARP TO] MNVR | [WARP TO] PeA | [WARP TO] ApA |
| **R2** | SAVE | LOAD | [TIME WARP] < | [TIME WARP] > | [TIME WARP] PHYS | [TIME WARP] STOP |

| Input | Label | Action | LED | Colour |
|---|---|---|---|---|
| B0 | [WARP TO] ApA | Warp to apoapsis, less the Pre-Warp lead | Backlit when available, dark otherwise; flash on a press | YELLOW |
| B1 | [TIME WARP] STOP | Cancel any warp, including a warp-to, then 1× | Backlit; flash | AMBER |
| B2 | [WARP TO] PeA | Warp to periapsis, less the lead | As ApA | YELLOW |
| B3 | [TIME WARP] PHYS | Modifier, held: PHYS + > / < steps physics warp 1×–4× | Lit while held | CHARTREUSE |
| B4 | [WARP TO] MNVR | Warp to the **burn start** of the next node (`TIMEWARP_TO_NEXT_BURN`, KSP's burn-time estimate), less the lead | As ApA | YELLOW |
| B5 | [TIME WARP] > | Warp faster (with PHYS: next physics rate) | Backlit; flash | WARM WHITE |
| B6 | [WARP TO] SOI | Warp to the next SOI change, less the lead | As ApA | YELLOW |
| B7 | [TIME WARP] < | Warp slower (with PHYS: previous physics rate) | Backlit; flash | WARM WHITE |
| B8 | [WARP TO] MORN | Warp to the next morning, less the lead | As ApA | YELLOW |
| B9 | LOAD | Quick load: F9 held for as long as LOAD is held (KSP needs it held) | Lit while F9 is held | MINT |
| B10 | PAUSE | Pause menu: Esc | Backlit; flash | AMBER |
| B11 | SAVE | Quick save: F5 | Backlit; flash | LIME |
| B12 | Mode selector SPC | §6.6 | — | |
| B13 | Mode selector RVR | §6.6 | — | |
| B14–B15 | — | Not connected | | |

**WARP TO availability.** A button is dark, and its press ignored, when its event does not exist.

| Button | Available when | From |
|---|---|---|
| ApA | Orbit closed (eccentricity < 1), not landed | `ORBIT`, `APSIDESTIME`, `FLIGHT_STATUS` |
| PeA | Not landed | `APSIDESTIME`, `FLIGHT_STATUS` |
| MNVR | A node exists (Δv of the next node > 0) | `MANEUVER` |
| SOI | In flight, outside the atmosphere | `FLIGHT_STATUS`, `ATMO_CONDITIONS` |
| MORN | Landed or splashed | `FLIGHT_STATUS` |

- **"Landed"** includes splashed and pre-launch.
- **Before KSP has reported** what a rule needs, the button counts as available, since KSP ignores a warp it cannot do. MNVR is the exception: it needs a node reported. The master asks for MANEUVER on entering a flight.
- **SOI** cannot know whether an encounter is ahead: Simpit gives no time to the next SOI change. KSP ignores the press when there is none.
- **The lead** is the Pre-Warp display's value in minutes (§11.2), sent as a negative delay (minutes × 60 s).
- **PHYS + > / <** works out the next physics rate from the rate index KSP reports.
- The availability rules are still to be checked against Simpit on the bench (§16).

### 9.7 AUX CTRL (0x26)

**Board:** Button Module 1801 / 1802 | **Type:** 0x07 | **Panel:** B2 P7 | **Master:** `panel_aux.cpp`

| | C6 | C5 | C4 | C3 | C2 | C1 |
|---|---|---|---|---|---|---|
| **R1** | [CTRL PT] PRI | UNDOCK | [EVA] EVA CHUTE | [EVA] JUMP / LET GO | [EVA] BOARD | [EVA] EVA LIGHTS |
| **R2** | [CTRL PT] ALT | [EVA] FLAG | [EVA] HELM | [EVA] GRAB | [EVA] CONST | [EVA] JET PACK |

| Input | Label | Function | Active when | Action | LED | Colour today |
|---|---|---|---|---|---|---|
| B0 | [EVA] EVA LIGHTS | EVA lights | EVA | Key U | EVA: backlit, flash; else dark | MINT |
| B1 | [EVA] JET PACK | Jetpack | EVA | Key R | As B0 | LIME |
| B2 | [EVA] BOARD | Board craft | EVA | Key B | As B0 | GREEN |
| B3 | [EVA] CONST | EVA construction | EVA | Key I | As B0 | TEAL |
| B4 | [EVA] JUMP / LET GO | Jump / let go | EVA | Key Space | As B0 | CHARTREUSE |
| B5 | [EVA] GRAB | Grab | EVA | Key F | As B0 | SEAFOAM |
| B6 | [EVA] EVA CHUTE | EVA parachute | EVA | Key P | As B0 | AMBER |
| B7 | [EVA] HELM | Helmet toggle | EVA | Key O | As B0 | SKY |
| B8 | UNDOCK (new label) | Undock / decouple docking ports | Always | CAG 37 toggle | Lit while the group is active | GOLD |
| B9 | [EVA] FLAG | Plant flag | EVA | Key G | As B0 | SEAFOAM |
| B10 | [CTRL PT] PRI | Primary control point | Always | CAG 35 (below) | Lit while active | ROSE |
| B11 | [CTRL PT] ALT | Alternate control point | Always | CAG 36 (below) | Lit while active | PINK |
| B12–B15 | — | Not connected | | | | |

- **EVA group (B0–B7, B9):** dark outside EVA, and presses ignored. On EVA each press taps its key; KSP reports none of these states, so each flashes.
- **PRI / ALT:** pressing one switches to that control point. It deactivates the other's CAG first, then activates its own. Its LED stays lit to show which is active. Pressing the one already active does nothing.
- **UNDOCK, PRI and ALT** carry the control-group offset.

---

## 10. Joysticks

Both joysticks are the Joystick Module (1831 / 1832, KerbalJoystickCore): a 3-axis stick with its own button on terminal U6, plus two RGB buttons BTN01 and BTN02 on the board. Master: `panel_sticks.cpp`.

**Do not touch a stick for about 80 ms after power-on.** The module reads its centre position then.

### 10.1 Rotation joystick (0x28)

**Type:** 0x09 | **Panel:** B2 P8

**Stick, by control mode** (AXIS1 = left / right, AXIS2 = forward / back, AXIS3 = twist):

| Mode | AXIS1 | AXIS2 | AXIS3 | Sent as |
|---|---|---|---|---|
| SPC | Yaw | Pitch | Roll | Rotation |
| PLN | Roll | Pitch | Yaw | Rotation |
| RVR | Steering | Wheel throttle | — | Wheel (held at 0 throttle while ENGINE is not ARMED) |
| EVA | D / A held | W / S held | E / Q held | Keys |

**Buttons:**

| Control | Function | LED | Colour |
|---|---|---|---|
| BTN01 | **Clear trim:** clears the master's trim set by BTN02. Not KSP's Alt+X | Backlit | GREEN |
| BTN02 | **Set trim:** adds the current (precision-scaled) stick deflection to the trim, so the trim holds what is being commanded now. A quick press between two reads counts | Lit while a trim is set | AMBER |
| Stick button | By mode, chosen when it is pressed: **SPC, PLN** — A/P disconnect (every autopilot and HOLD); **RVR** — brakes while held (the BRAKES rule, §9.5); **EVA** — Left Shift (run) while held | — | — |

- **Trim** is the master's own offset, added to the stick from then on. It is not scaled by THC/RHC PREC. In RVR it offsets steering and wheel throttle.
- **The trim clears** on a control-mode change.
- **The Annunciator's TRIM** lights while a trim is set.
- **The stick button's brake or Shift** ends on a mode change or when the flight ends.
- **Air brake** is only on Function Control B5; the stick button no longer fires it.

### 10.2 Translation joystick (0x29)

**Type:** 0x0A | **Panel:** A2 P7

| Mode | AXIS1 | AXIS2 | AXIS3 | Sent as |
|---|---|---|---|---|
| Normal (any control mode) | Translation X | Translation Y | Translation Z | Translation |
| Camera (stick button held) | Camera yaw | Camera pitch | Zoom | Camera rotation |
| EVA | D / A held | W / S held | Left Shift / Left Ctrl held | Keys |

The module inverts AXIS2 itself (`KJC_AXIS2_INVERT`), so the master uses its values as they come.

**Buttons:**

| Control | Function | LED | Colour |
|---|---|---|---|
| BTN01 | Cycle camera mode (auto / free / orbital / chase): `CAMERA_NEXT_MODE` | Backlit; flash | MAGENTA |
| BTN02 | Camera reset: sends the auto camera mode | Backlit; flash | GREEN |
| Stick button | Held: the stick moves the camera instead of translating (on EVA, instead of the EVA keys) | — | — |

- **Camera mode keeps working under an autopilot or HOLD.** Translation does not.
- **Camera mode works on EVA too.** While the button is held the stick moves the camera and its EVA keys (W A S D, Left Shift / Left Ctrl) are released; the rotation stick's EVA keys keep working.

### 10.3 Shared stick rules

- **Precision (THC/RHC PREC):** both sticks are scaled to 0.3. The trim and the camera axes are not scaled. The EVA keys use the raw deflection.
- **EVA keys:** a key goes down past 50 % deflection and up below 30 %. All are released when the EVA ends.
- **Under an autopilot or HOLD:** both sticks are ignored, except camera mode and the buttons (§8.2).
- **Axis signs** are as the modules report them (`STICK_SIGN` in `panel_sticks.cpp`), to be checked on the bench (§16).
- **When sent:** each channel goes out when it changes by more than about 0.5 %, at most every 20 ms. Every channel is sent again on entering a flight, the link coming up, leaving demo, and an autopilot / HOLD, control-mode or EVA change. Nothing is sent outside a flight scene, in demo, or with the link down.
- **Debug:** stick positions are logged while in demo or debug.

---

## 11. Display-managed modules: GPWS and Pre-Warp

Both are the 7-Segment Display Module (1841 / 1842, Kerbal7SegmentCore): a 4-digit display, an encoder with push, and three RGB buttons. **They manage their own display, buttons and LEDs.** The master only reads their value and state.

**Encoder steps (both modules):** consecutive same-direction clicks set the step. 1–14 clicks: ±1; 15–29: ±10; 30–49: ±100; 50 and more: ±1000. A direction reversal or a 500 ms pause resets it to ±1. Range 0–9999, no leading zeros.

**On DISABLE** (leaving a flight scene) both modules go dark and reset to their defaults.

### 11.1 GPWS Input Panel (0x2A)

**Type:** 0x0B | **Panel:** A2 P6 | **Master:** `panel_gpws.cpp` (relay only)

| Control | Function | LED |
|---|---|---|
| Display | Altitude threshold, 0–9999 m. On only while GPWS is on (full or proximity) | — |
| Encoder | Adjust the threshold (only while GPWS is on) | — |
| Encoder button | Threshold back to 200 m | — |
| BTN01 | GPWS mode: OFF → full GPWS → proximity tone only → OFF. Turning it OFF also clears BTN02 and BTN03 | OFF backlit; full GPWS GREEN; proximity AMBER |
| BTN02 | Proximity alarm on / off | GREEN while on, backlit otherwise |
| BTN03 | Rendezvous radar on / off | GREEN while on, backlit otherwise |

- **BTN02 and BTN03** are the two proximity sub-modes and exclude each other. Pressing either while GPWS is full switches it to proximity (amber). Both do nothing while GPWS is OFF.
- **While GPWS is OFF,** only BTN01 reports to the master.
- **Relay:** the master passes the state byte (bits 1:0 mode, bit 2 proximity alarm, bit 3 rendezvous radar) and the threshold to the Annunciator's GPWS bytes on every change.
- **DISP CLR** sets the threshold to 0.

### 11.2 Pre-Warp Time (0x2B)

**Type:** 0x0C | **Panel:** B2 P2 | **Master:** `panel_time.cpp` (reads the minutes)

| Control | Function | LED |
|---|---|---|
| Display | Warp lead time, 0–9999 minutes. Always on while ACTIVE | — |
| Encoder | Adjust the lead | — |
| Encoder button | Reset to 0 | — |
| BTN01 | Preset 5 min | GOLD flash (150 ms), then backlit |
| BTN02 | Preset 1 hour (60 min) | GOLD flash, then backlit |
| BTN03 | Preset 1 Kerbin day: **6 h = 360 min** (firmware today: 1440, §15) | GOLD flash, then backlit |

- **Every Time Control WARP TO** button warps to its event less this lead (§9.6).
- **The value persists** across vessel switches. DISP CLR sets it to 0.

---

## 12. Throttle Module

**Board:** Throttle Module 1871 / 1872 | **Address:** 0x2C | **Type:** 0x0D | **Panel:** A2 P3 | **Master:** `panel_throttle.cpp`, `interlocks.cpp`, `ksp_frame.cpp` | **Library:** standalone sketch

A motorised 10 kΩ slide pot with an L293D H-bridge on its own 9 V supply, a capacitive touch sensor on the fader track, and five illuminated panel inputs.

### 12.1 Controls

| Control | Terminal | Function |
|---|---|---|
| Lever | — | Throttle. KSP's throttle follows the lever while the ENGINE is ARMED and no autopilot owns it |
| Touch sensor | — | Detects the pilot's hand. While touched, the motor stops and the module refuses lever commands |
| THRTL_100 (100 %) | U7 | Lever to 100 % |
| THRTL_UP (UP) | U8 | Lever up 10 % |
| THRTL_DOWN (DOWN) | U10 | Lever down 10 % |
| THRTL_00 (0 %) | U11 | Lever to 0 % |
| THRTL_ENA | U5 | Not wired; unused |

### 12.2 Rules

- **The buttons move the lever, and the throttle follows the lever** like any pilot input. The module only reports its buttons (Throttle firmware v2.1); the master sends the lever command (design §8.9, *stage 5*).
- **The buttons are ignored** while the ENGINE is not ARMED, while an autopilot owns the throttle, and in FINE (a lever target is not the throttle there). In demo they move the lever (still subject to ENGINE and FINE).
- **ENGINE SAFE:** 0 % to KSP, lever driven to 0 % (§6.3).
- **Under an autopilot:** the autopilot sets the throttle and the master drives the lever to the same position (1 % band, at most one command per 200 ms), so the lever always shows the throttle KSP is using. The lever, its buttons and touch are ignored. While the pilot holds the lever, it is not driven; it goes back to the autopilot's position when released. (The confirmed design also has the motor push back against a hand for up to about 0.5 s: a Throttle firmware change, §15.)
- **Lamps:** all five share one enable. They are on while the module is ENABLED and off while DISABLED; they do not show button state.

### 12.3 Precision (THRTL STD / FINE)

- **FINE:** the module maps the full lever travel to ±10 % around the throttle at the moment FINE was selected, and drives the lever to its centre for equal authority.
- **FINE reaches the module only while the ENGINE is ARMED** and no autopilot owns the throttle. While an autopilot owns it, the master takes the module out of FINE and restores FINE when the autopilot lets go.
- **The Annunciator's THRTL PREC** shows the switch, whatever the module is doing.

---

## 13. Dual Encoder

**Board:** Dual Encoder 1861 / 1862 + 2 × Encoder 1851 / 1852 | **Address:** 0x2D | **Type:** 0x0E | **Panel:** B1 (Resource Display slot P4) | **Master:** `panel_encoders.cpp` | **Library:** standalone sketch

Two encoders with push buttons, one detent = one step, no LEDs.

| Control | Function | Action |
|---|---|---|
| ENC1 | Control group 1–6, wrapping | Master state only; nothing is sent to KSP. Works in demo too. The Annunciator shows the group |
| ENC1 button | Control group back to 1 | As above |
| ENC2 clockwise | Next external camera | Key `-` (Hullcam VDS default) |
| ENC2 anticlockwise | Previous external camera | Key `=` (Hullcam VDS default) |
| ENC2 button | Camera reset | Key Backspace (Hullcam VDS default) |

- **ENC2** sends one key per detent, at most 5 per read, and only when the panels are live.
- **A new control group** takes effect on the next CAG button press, and the CAG LEDs then show the new group's state (§4).
- **The Dual Encoder's safe-switch terminal** goes to slot pin 16, which no hub connects; it does nothing.

---

## 14. Demo mode

Switch Group 1 SCE: NORM = off, AUX = on (Inventory §8.8, design §10.1). For showing the controller off and for bench testing. Allowed any time, menus included.

| Area | In demo |
|---|---|
| **KSP** | Nothing is sent at all. Every "KSP connected / in a flight scene" check passes, so nothing depends on a Simpit connection |
| **Autopilots and HOLD** | Dropped on entry; HOLD cannot be engaged |
| **Modules** | All ENABLEd for the duration, whatever the scene |
| **Buttons** | Every usable button sits backlit and shows ACTIVE while pressed. No light patterns run by themselves |
| **Switches** | Still gate their buttons on the panel: CHUTE SAFE darkens the chutes, SAS OFF the SAS modes |
| **Always backlit in demo** | The EVA group, every WARP TO button, HOLD, INVERT, the state machines (when not gated by CHUTE) |
| **Sticks and 7-segments** | Respond locally. GPWS and Pre-Warp work as normal. Stick positions are logged |
| **Dual Encoder** | ENC1 changes the control group. ENC2 sends nothing |
| **Throttle** | The lever buttons move the lever (ENGINE and FINE rules still apply) |
| **ABORT, STAGE** | Nothing sent |
| **Displays** | All four carriers get the demo flag and animate demo data. The Sys Info Display leaves its boot screen |
| **Annunciator** | DEMO lit |

**Leaving demo:**
- Modules return to the lifecycle for the current scene.
- LEDs return to KSP's state.
- Switches flipped during demo are not acted on. The usual rule applies, so a switch that now disagrees with the vessel lights SWITCH ERR.

**Displays follow the master's flag only.** Today the Info Displays clear demo on a scene change and the Resource Display stops servicing Simpit; both are being brought in line (§15).

---

## 15. Firmware and label gaps

The confirmed behaviour is what this document describes. These items do not match it yet (Inventory §11, design §17, plus gaps found for this revision).

| # | Where | Today | Confirmed | Source |
|---|---|---|---|---|
| 1 | Function Control module firmware | **Fixed in v2.1 (2026-10-27).** Was: colour table in the old button order | Each colour with its function (§9.2); FLAPS SEAFOAM | Inventory §11 #4 |
| 2 | Function Control panel label | B4 reads RW DISABLE | FLAPS | Inventory §11 #13 |
| 3 | AUX CTRL module firmware | **Fixed in v2.1 (2026-10-27).** Was: B8 Cruise Control, B10 PRI / ALT toggle with a CORAL alternate, B11 Docking Port | B8 UNDOCK GOLD; B10 PRI ROSE; B11 ALT PINK; no alternate colour | Inventory §11 #5 |
| 4 | AUX CTRL panel label | B8 has no label | UNDOCK | Inventory §11 #13 |
| 5 | Stability Control module firmware | **Fixed in v2.1 (2026-10-27).** Was: B10 GREEN (RCS); B14 / B15 described as Stage / Stage Enable | B10 HOLD MAGENTA. B14 / B15 are unwired and the master ignores them | Inventory §11 #6 |
| 6 | Pre-Warp module firmware | BTN03 preset 1440 min (an Earth day) | 360 min (1 Kerbin day) | Inventory §11 #3 |
| 7 | Button, joystick and throttle firmware | `CMD_BULB_TEST` runs a fixed 2 s test with `delay()` inside the I2C handler; the Throttle Module ignores the stop. The master keeps off the bus for 2.1 s, then sends the stop | Start / stop, no timeout, not blocking: LTG TEST lights for as long as it is held | Inventory §11 #19, design §6.4 |
| 8 | Throttle module firmware | Refuses lever commands while touched, so the lever cannot be held against the pilot | Under an autopilot, push back against a hand for at most about 0.5 s (stall limit), then return the lever on release | Inventory §11 #15, design §11.3 |
| 9 | Throttle module firmware | **Fixed in v2.1 (2026-10-27).** Was: the module moved the lever on its own buttons (100 % / 0 %, ±5 %), even in ENGINE SAFE, in FINE and under an autopilot | The module only reports its buttons; the master moves the lever (§12.2) | Found for v6.0; Inventory §11 #20 |
| 10 | Annunciator firmware | Tile THRTL ENA | A/P ARM (same bit 3) | Inventory §11 #1 |
| 11 | Info Display and Resource Display firmware | Info Displays clear demo on a scene change; the Resource Display stops servicing Simpit in demo | Every display enters and leaves demo only on the master's flag | Inventory §11 #10–#12 |
| 12 | Module READMEs and sketch comments | Some still describe the old layouts (for example the Rotation BTN01 "Alt+X" note). Throttle, Function Control, Stability Control and AUX CTRL were brought up to date in v6.1 | As this document | — |

---

## 16. Open items

| # | Item | Status |
|---|---|---|
| 1 | Throttle Module acts on its own buttons (§15 #9). **Decided:** the master owns the lever; the module only reports its buttons (Throttle v2.1) | Closed (v6.1) |
| 2 | Colours for Function Control FLAPS, Stability HOLD, AUX UNDOCK and AUX ALT (§15 #1, #3, #5). **Decided:** FLAPS SEAFOAM, HOLD MAGENTA, UNDOCK GOLD, ALT PINK | Closed (v6.1) |
| 3 | HOLD and the console's A/P OFF. **Decided:** A/P OFF releases HOLD in every mode, SPC included, like the stick's A/P disconnect (Inventory §8.10). Master v0.8.1 | Closed (v6.1) |
| 4 | SWITCH ERR autopilot exception. Inventory §8.4 and design §8.3: "SAS or RCS while an autopilot that changes them is engaged". The code skips SAS while any autopilot or HOLD is engaged, and always checks RCS (no autopilot changes RCS). Same effect today; noted in case an autopilot ever uses RCS | Noted |
| 5 | Camera mode on EVA. **Decided:** camera mode works on EVA too; the translation stick's EVA keys pause while its button is held (§10.2). Master v0.8.1 | Closed (v6.1) |
| 6 | Info Display AIRBRK indicator read CAG 38 as a fixed number. **Decided:** it follows the control group; the master sends the group in the summary frame (byte 8) and InfoDisp v1.17 offsets the CAG (§4.3) | Closed (v6.1) |
| 7 | Hullcam VDS keys: check that `-` / `=` / Backspace match the in-game bindings, and that Simpit's keyboard emulator reaches KSP for the EVA keys, INVERT's F and the camera keys | Bench (design §18 #10) |
| 8 | SAS availability bitmask tracks pilot / probe level as expected | Bench (design §18 #4) |
| 9 | WARP TO availability rules against Simpit | Bench (design §18 #3) |
| 10 | CAGSTATUS bit *n* is custom action group *n*; CAGSTATUS / SAS_MODE_INFO arrive when requested on entering a flight | Bench (design §18 #11) |
| 11 | Each stick axis's sign, and the EVA keys' directions | Bench (design §18 #12) |
| 12 | KSP's GEAR group reads active with the gear down, and each Switch Group 2 "on" position matches its group | Bench (design §18 #8) |
| 13 | The lever follows an autopilot's throttle and goes back after a touch; the buttons step it; the touch flag and the drive back to 0 % while SAFE | Bench (design §18 #9, #13) |
| 14 | The screenshot macro's 150 ms spacing is enough for KSP to hide the UI before F1 | Bench (design §18 #14) |
| 15 | A DISABLED button module answers a read with its current inputs, so the switches work in the menus | Bench (design §18 #7) |

**Closed by v6.0:**
- **Ctrl Mode Toggle wiring** (v5.9 #14): Time Control B12 / B13 (§6.6).
- **Operator feedback lost with the Indicator module** (v5.9 #6): ABORT and STAGE are hardware-armed buttons, and the Annunciator carries the control mode, control group, A/P ARM, ENG ARM, AUTOPILOT, TRIM, SWITCH ERR and the precision lights (design §9).

---

## 17. Removed in v6.0

Superseded by the confirmed panel review (Inventory §10) and the master design.

| Removed | Now |
|---|---|
| AUX CTRL B8 Cruise Control | B8 UNDOCK (CAG 37). Rover speed hold is HOLD in RVR (§8.1) |
| AUX CTRL B10 PRI / ALT toggle; B11 Docking Port control point | B10 PRI (CAG 35), B11 ALT (CAG 36); the docking-port control point is gone |
| Stability Control B10 RCS | B10 HOLD. RCS is the Switch Group 2 RCS switch |
| Stability Control B14 Stage, B15 Stage Enable | STAGE on the B2 safe line, with a hardware arming switch (§7) |
| Function Control B5 RW DISABLE (CAG 39) | B4 FLAPS (CAG 39) |
| Function Control order with LES on B0 | Every left / right pair swapped (B0 FAIRING JETT, B1 LES, …); CAG numbers kept with the function |
| Rotation stick button: air brake (CAG 38 duplicate) | By control mode (§10.1) |
| Rotation BTN01: KSP trim reset (Alt+X) | Clears the master's own trim |
| Dual Encoder ENC1 button: docked-flag toggle | Resets the control group to 1. Docking state is the Annunciator DOCK tile |
| Action Control AG 1–12 offset by the control group | Always CAG 1–12 |
| Vehicle BRAKES toggle | Brakes while held; the BRAKE switch dominates |
| Parachute state reset by CHUTE SAFE | State from KSP's report; reset by a 2 s hold |
| Switch Group 1 order B18 ENGINE, B19 THROTTLE INOP / ACT, B20 SCE, B21 UPTLM, B22 LTG | B18 SCE, B19 UPTLM, B20 LTG, B21 A/P INHIB / ARM, B22 ENGINE SAFE / ARM (THROTTLE INOP / ACT renamed), B23 THRTL |
| SCE AUX as "Simpit reset and data fetch" | Demo mode |
| Any pilot stick or lever input disconnects the autopilots | Autopilot authority (§8.2) |
| The single-module Switch Panel and the Indicator Module | Switch Groups 1 and 2; the Annunciator |
| "AIR" control mode | PLN |
| Throttle Module PCB KC-01-1822, four inputs | KC-01-1871 / 1872, five terminals (THRTL_ENA unused) |

---

## 18. Revision history

| Version | Date | Changes |
|---|---|---|
| 4.0 | 2026-04-09 | Initial release — 15 modules documented including Indicator Module |
| 5.0 | 2026-05-10 | Panel Interface Guide — panel-centric reorganisation; Function Control redesigned; AUX CTRL (formerly EVA) expanded to 12 buttons; CP buttons moved to AUX CTRL; switch functions moved to Switch Groups 1 + 2; Indicator Module removed |
| 5.1 | 2026-05-19 | v4.0 reference and v5.0 guide merged into one module-centric document. Switch Groups 1 and 2, address summary, removed-from-design table |
| 5.2 | 2026-05-25 | PCB designators corrected |
| 5.3 | 2026-06-27 | GPWS Input Panel moved from A1 to A2 |
| 5.4 | 2026-06-28 | Module firmware items #7–#10 resolved against KerbalButtonCore v2.0 (24-input switch modules; Action Control B10 / B11 as AG buttons; Stability RCS at B10) |
| 5.5 | 2026-06-28 | GPWS / Pre-Warp encoder acceleration corrected to the click-count model |
| 5.6 | 2026-06-28 | `CUT` LED state added for the Vehicle Control state machines |
| 5.7 | 2026-06-28 | Retired 0x2E module removed from the codebase |
| 5.8 | 2026-06-28 | AUX CTRL 12-button firmware; `KMC_SEAFOAM` added |
| 5.9 | 2026-06-28 | `ACTIVE_ALT` LED state added for the AUX CTRL CP toggle |
| 6.0 | 2026-10-26 | **Rewritten from the confirmed panel review (`Hardware_Inventory.md` §7, §8, §10, §11), `Master_Firmware_Design.md` §8–§11 and the master firmware in `Software/KCMk1_Master`.** New: per-panel slot maps (A1, A2, B1, B2); confirmed button layouts and labels for every module; Switch Group 1 in its confirmed order with A/P INHIB / ARM and ENGINE SAFE / ARM; the switch rule and SWITCH ERR; the mode selector; ABORT and STAGE on the safe lines; control modes, HOLD per mode and autopilot authority; sticks (trim, stick button by mode, camera, EVA keys, precision); the throttle rules; the Dual Encoder control group; demo mode; CAG numbering with Action Control fixed; 
| **6.1** | **2026-10-27** | **Open items 1, 2, 3, 5 and 6 decided.** The Throttle Module only reports its buttons; the master moves the lever (§12.2, Throttle v2.1). Colours: FLAPS SEAFOAM, HOLD MAGENTA, UNDOCK GOLD, ALT PINK; the Function Control colours follow the confirmed layout and AUX's CORAL alternate is gone (Function / Stability / AUX v2.1). A console's A/P OFF releases HOLD in every mode (§8.2). Camera mode works on EVA (§10.2). The AIRBRK indicator follows the control group (§4.3, InfoDisp v1.17). §15 #1, #3, #5, #9 fixed. Master v0.8.1 |firmware and label gaps; open items. Removed: Cruise Control, Stage / Stage Enable on Stability, RW DISABLE, the RCS button, the pilot-input autopilot override, and the other superseded content listed in §17. Board numbers corrected (Throttle 1871 / 1872) |
