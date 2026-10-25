# KCMk1 Master Firmware — Design

| | |
|---|---|
| **Document type** | Developer — design for approval |
| **Location** | `Documents/Developer/Master_Firmware_Design.md` |
| **Status** | Draft 3, for review. Nothing here is implemented yet |
| **Supersedes** | `Controller_Main_IO_Design.md` (drafts 1–2, which planned an overhaul of the existing sketch) |

---

## At a glance

We are replacing the master firmware (`Software/Controller_Main`) with a **new sketch written from the ground up**, `Software/KCMk1_Master/`.

- **Why:** the old sketch and the Hardware Reference are out of date and contradict the rev-2 hardware. For example, none of today's interrupt pins matches the rev-2 Ctrl Module (Appendix A).
- **What is kept:** the autopilots. They are ported with their control laws, consoles and design docs unchanged (§8).
- **What it is built from:** the schematics, the I2C Protocol Specification and the Module UI Reference.
- **How:** eight stages, one PR each, every one checked on the host before it merges (§11). The old sketch is deleted in the last stage.

### What is needed before work starts

1. **Approval of this plan.**
2. **Three hardware answers**, needed by stage 3. Each has a default that gets built if there is no answer (§12):
   - **Q2 — Staging:** from Stability Control B14 / B15, or from the B2 staging button?
   - **Q3 — Ctrl Mode Toggle:** which module inputs will it be wired to?
   - **Q14 — Safe lines:** what is connected to INT_SAFE, INT_SAFE2, B_INT_SAFE and B_INT_SAFE2?
3. **The remaining schematics**, if available: Ctrl Ext Module (KC-01-1711), Panel Hub (KC-01-1731), Display Hub (KC-01-1721).

---

## Contents

1. [Decisions](#1-decisions)
2. [Sources](#2-sources)
3. [What the master does](#3-what-the-master-does)
4. [Architecture](#4-architecture)
5. [Module bus](#5-module-bus)
6. [Outputs to KSP](#6-outputs-to-ksp)
7. [Panel behaviour](#7-panel-behaviour)
8. [Porting the autopilots](#8-porting-the-autopilots)
9. [Board services](#9-board-services)
10. [Verification](#10-verification)
11. [Stages](#11-stages)
12. [Open questions](#12-open-questions)
- [Appendix A — Ctrl Module pin map](#appendix-a--ctrl-module-pin-map-kc-01-1701-v20)
- [Appendix B — Glossary](#appendix-b--glossary)

---

## 1. Decisions

- **New sketch, from the ground up.** `Software/Controller_Main` gets no further changes, apart from a fix if something blocks bench testing.
- **Built alongside the old one.** The new sketch lives in `Software/KCMk1_Master/`. `Controller_Main` stays as a reference until the new master does everything it did, then a single cutover PR deletes it.
- **Autopilots are ported, not rewritten.** Ascent, hold, burn and landing are covered, along with the attitude controller, the arbiter and the staging helpers. They already talk to the rest of the master through narrow interfaces. The port re-points those interfaces and changes nothing else.
- **Hardware truth is the schematics.** The Hardware Reference and the old sketch are not used as sources. The Hardware Reference is rewritten from the schematics at cutover.
- **Plan first.** This document is approved before stage 1 starts.

## 2. Sources

Where two sources disagree, the higher one wins.

1. **Schematics.**
   - In hand: Ctrl Module KC-01-1701 V2.0 (Appendix A).
   - Still wanted: Ctrl Ext Module (KC-01-1711), Panel Hub (KC-01-1731), Display Hub (KC-01-1721), and any module board where a safe line or the Ctrl Mode Toggle is wired.
2. **I2C Protocol Specification v2.14:** module packets, lifecycle, commands, and the display protocol (§15).
3. **Module UI Reference v5.9:** what every button, switch, encoder and stick does.
4. **Display firmware and READMEs:** what the Annunciator, Info Displays and Resource Display expect. Their commands are fixed; the new master sends exactly what the current one does.
5. **Autopilot design docs:** `Hold_Mode_Autopilot.md`, `Mission_Autopilot.md`, `Ascent_Autopilot.md`, `Ascent_Autopilot_Interface.md`.
6. **KerbalSimpit 2.4.0:** the library source, for message ids and payloads.

## 3. What the master does

| Job | In short |
|---|---|
| **Talk to KSP** | Runs the Simpit link on its own USB serial (`SerialUSB1`, so the Teensy USB type is *Dual Serial*). Keeps the vessel state everything else reads. Is the only thing that sends control messages to KSP |
| **Run the module bus** | Brings up every input module, reads it when it signals, turns its packets into button / switch / axis events, drives its LEDs |
| **Turn inputs into actions** | Simpit commands, custom action groups, keyboard shortcuts, and the master's own modes (control mode, control group, precision, audio) |
| **Merge pilot and autopilot** | One rotation, translation, wheel and throttle message per frame. An autopilot only owns the axes it holds; the pilot can always override |
| **Run the autopilots** | And their console on Info Display 2 |
| **Feed the displays** | Sends the Annunciator, Resource Display and Info Displays their commands and releases them from their boot screens |
| **Look after the board** | Fan, power monitor, watchdog, display reset line |

**Out of scope:** the expansion unit. Its pins (EXT bus, EXT_INT_1 / 2) are reserved and left idle.

## 4. Architecture

### 4.1 Layers

Data flows one way, left to right:

```
  INPUTS              STATE             DECISIONS               OUTPUTS
  ─────────────       ───────────       ──────────────────      ─────────────────────
  simpit_link   ──▶   vessel      ──▶   panel handlers    ──▶   ksp_out     ──▶ KSP
  module_bus    ──▶   panel       ──▶   autopilots        ──▶   module LEDs ──▶ modules
  board         ──▶   master      ──▶   annunciation      ──▶   carriers    ──▶ displays
```

Three rules:

1. **Inputs only write state.** Decisions only read state and ask outputs to act. Only outputs touch Simpit, the module bus or the displays.
2. **Each piece of state has one writer:**
   - `vessel` is written by `simpit_link`;
   - `panel` (switch positions, control mode, control group, precision, audio) is written by the panel handlers;
   - `master` (lifecycle, link health, fault counters) is written by the master itself.
3. **Every tab has a header.** No tab reaches into another's variables, and the build does not rely on the Arduino IDE generating prototypes. The host build and the IDE see the same code.

### 4.2 Main loop

The same order every pass, with nothing blocking:

| # | Call | Does |
|---|---|---|
| 1 | `simpitService()` | Reads inbound KSP messages; retries the connection while it is down |
| 2 | `boardServiceFast()` | Feeds the watchdog; debounces the safe lines |
| 3 | `moduleBusService()` | Reads every module that is signalling; its panel handler acts at once |
| 4 | `autopilotService()` | Runs the autopilots and the arbiter |
| 5 | `kspOutService()` | Sends the merged stick / throttle message (every 20 ms, or at once when the owner changes) and any queued commands |
| 6 | `moduleBusFlushLeds()` | Sends LED changes, at most every 50 ms per module |
| 7 | `carrierService()` | Display commands every 100 ms and on change; Info Display 2 console |
| 8 | `boardServiceSlow()` | Fan (1 Hz), power monitor (4 Hz), diagnostics |

**Timing target:** under 2 ms per pass when idle, under 5 ms in the worst pass.

### 4.3 Files

```
Software/KCMk1_Master/
│
├─ KCMk1_Master.ino      setup() and loop() only
├─ config.h              pin map, I2C addresses, timings — all in one place
├─ state.h/.ino          vessel, panel and master state
│
├─ simpit_link.h/.ino    KSP connection and inbound messages
├─ module_bus.h/.ino     the module driver (§5)
├─ ksp_out.h/.ino        the only sender to KSP (§6)
├─ cag.h/.ino            action-group numbering for the active control group
├─ keys.h/.ino           keyboard shortcuts and macros
│
├─ panel_ui.ino          UI Control            0x20
├─ panel_function.ino    Function Control      0x21  + Switch Group 1
├─ panel_action.ino      Action Control        0x22
├─ panel_stability.ino   Stability Control     0x23
├─ panel_vehicle.ino     Vehicle Control       0x24  + Switch Group 2
├─ panel_time.ino        Time Control          0x25  + Pre-Warp 0x2B
├─ panel_aux.ino         AUX CTRL              0x26
├─ panel_sticks.ino      Rotation / Translation 0x28 / 0x29
├─ panel_gpws.ino        GPWS Input Panel      0x2A
├─ panel_throttle.ino    Throttle Module       0x2C
├─ panel_encoders.ino    Dual Encoder          0x2D
├─ panel_safe.ino        the safe lines (abort / stage)
│
├─ carriers.h/.ino       the display panels
├─ board.h/.ino          fan, power, watchdog, display reset
├─ ap_*.ino / .h         the ported autopilots (§8)
│
└─ tools/                host build, selftest, module simulator
```

The autopilot files sit in the top folder because the Arduino IDE only compiles tabs there. The `ap_` prefix keeps them together.

## 5. Module bus

### 5.1 Module table

Every module is one row in a table:

```c
struct ModuleDef {
  uint8_t addr;          // I2C address, 0x20..0x2D
  uint8_t typeId;        // module type expected in its packet header
  uint8_t defaultInt;    // INT pin from config.h, or NO_PIN
  uint8_t flags;         // has LEDs, active on EVA, ...
  void  (*onPacket)(const uint8_t *pkt, uint8_t n);   // its panel handler
};
```

### 5.2 Finding each module's interrupt line

Which interrupt line a module uses depends on which hub slot it is plugged into. That is cabling, so the master **works it out** instead of relying on a slot table:

1. At power-on, every module holds its INT line low until the master first reads it.
2. The master reads the modules one at a time.
3. The INT line that goes high after each read belongs to that module.

The display panels are found the same way: a STATUS request makes them signal.

- **Restarts:** the result is saved in EEPROM, so the map survives a master that restarts on its own, when the modules are already up and not signalling.
- **First boot:** a default map in `config.h` is used.
- **Unknown line:** a module whose line is unknown is polled every 20 ms.
- **Mismatches** between the learned, saved and default maps are reported once on Serial.

### 5.3 Start-up

For each module (I2C spec §12.1):

1. Ask its identity and check the type against the table.
2. Turn all its LEDs off.
3. Put it in DISABLED.

A module that does not answer, or reports the wrong type, is marked absent and reported once. Absent modules are re-checked every 5 s, so a panel plugged in late still comes up.

### 5.4 Reading a module

**Checks on every packet:**
- **Type:** must match the table.
- **Lifecycle:** if the module has rebooted, it is disabled and then put back in the wanted state.
- **Fault bit:** logged and acknowledged.
- **Transaction counter:** a gap means a missed packet, and is logged.

**Button events.** A packet carries each button's current state and which buttons changed since the last read. The bus turns that into three lists for the handlers:

| Change bit | Now | Event |
|---|---|---|
| changed | down | **pressed** |
| changed | up, and the master saw it down | **released** |
| changed | up, and the master never saw it down | **tapped** (pressed and released between two reads) |

Handlers act on *pressed or tapped*, so a quick tap is never lost.

**LEDs.** Handlers set the wanted state of each button's LED. The bus sends an update only when something changed, and at most every 50 ms per module. The bulb test (Switch Group 1, LTG TEST) lights everything while held.

### 5.5 When modules are on or off

| Situation | Modules |
|---|---|
| Boot, or KSP not connected | Off (dark) |
| Flight scene starts | All on |
| Flight scene ends, or KSP lost | All off |
| Kerbal on EVA | Stay on; each handler decides what still works (Q4) |
| Game paused | No change; Simpit does not report pause (Q5) |

## 6. Outputs to KSP

KSP keeps only the latest message on each control channel, so two senders would overwrite each other. `ksp_out` is therefore the **only** code that sends them.

| Channel | Who controls it |
|---|---|
| **Rotation** | Axis by axis: the autopilot on the axes it holds, the pilot's stick on the rest |
| **Translation** | The pilot. In camera mode the stick moves the camera instead |
| **Wheel** | The pilot's stick in rover mode, or the rover autopilot while engaged |
| **Throttle** | One owner at a time: the pilot, or the autopilot the arbiter gave it to. The motorised lever follows the owner |
| **Commands** | Action groups, SAS mode, staging, warp, camera, keys: queued, sent in order, and rate-limited so a macro cannot flood the link |

> **Pilot override:** moving either stick past its threshold, or touching the throttle lever, disconnects every autopilot. This is today's rule, with today's thresholds.

## 7. Panel behaviour

Everything here comes from Module UI Reference v5.9. Items marked **(design)** are decisions this document makes where the reference is silent or unclear.

### 7.1 Rules shared by every panel

- **Custom action groups:** the number sent is `base + (control group − 1) × 40`. The control group (1–6) is set with Dual Encoder ENC1.
- **Action-group buttons** toggle their group. The LED is bright while KSP reports the group active, and backlit otherwise.
- **Keyboard actions** are sent as key presses to KSP. Key codes live in `keyboard_def.h`.
- **LEDs at rest:** backlit when the button does something right now, dark when it doesn't (e.g. EVA buttons outside EVA) **(design)**.
- **Control mode** (spacecraft / plane / rover) comes from the Ctrl Mode Toggle (Q3). It decides how the sticks are mapped.

### 7.2 UI Control (0x20)

| Button | Action |
|---|---|
| B0 | Screenshot macro (F2, F1, F2) |
| B1 | Alt+F12 |
| B2 | F2 |
| B3 | Numpad `.` |
| B4 | `'` |
| B5 | Navball mode cycle |
| B6 / B8 | Tab / Shift+Tab |
| B7 / B9 | `]` / `[` |
| B10 | M |
| B11 | C |

**LEDs:** backlit; bright for 150 ms on press **(design)**.

### 7.3 Function Control (0x21)

- **B0–B11:** custom action groups (bases 25–34, 38, 39). LEDs show group state.
- **Switch Group 1:**

| Input | Switch | Action |
|---|---|---|
| B16 | MSTR RESET | Reset modules and displays (Q12) |
| B17 | DISPL CLR | Zero the GPWS and Pre-Warp displays |
| B18 | ENGINE SAFE / ARM | Throttle module on / off (Q8) |
| B19 | THROTTLE INOP / ACT | Throttle input to KSP on / off (Q8) |
| B20 | SCE AUX | Reconnect to KSP and refresh all data |
| B21 | UPTLM XMIT | Debug mode |
| B22 | LTG TEST | Bulb test while held |
| B23 | THRTL STD / FINE | Throttle precision (Q8) |

### 7.4 Action Control (0x22)

- **B0–B11:** action groups AG1–AG12, in the active control group. LEDs show group state.

### 7.5 Stability Control (0x23)

| Button | Action | LED |
|---|---|---|
| B0–B9 | SAS modes | Bright on the mode KSP reports |
| B10 | RCS toggle | RCS state |
| B11 | Invert: holds F while held | — |
| B14 | Stage, only while Stage Enable is on (Q2) | — |
| B15 | Stage Enable | On while enabled |

### 7.6 Vehicle Control (0x24)

| Button | Action |
|---|---|
| B0 / B1 / B3 | Brakes / lights / gear (LED from KSP) |
| B2, B4–B7, B9 | Custom action groups |
| B8 | Heat shield (deploy, then release) |
| B10 / B11 | Parachutes (deploy, then cut), only while CHUTE is ARM; SAFE resets them |

State-machine LEDs: green = deployed, red = cut / released.

**Switch Group 2:**

| Switch | Action |
|---|---|
| CHUTE | Arms the parachute buttons |
| GEAR / BRAKE / EXT LT / SAS / RCS | Sets the function on each flip |
| THC / RHC PREC | Stick precision |
| AUDIO | Annunciator sound on / off |

### 7.7 Time Control (0x25) and Pre-Warp (0x2B)

| Button | Action |
|---|---|
| B0 / B2 / B4 / B6 / B8 | Warp to Ap / Pe / maneuver / SOI / morning, stopping early by the Pre-Warp minutes |
| B1 | Stop warp |
| B5 / B7 | Warp faster / slower (physics warp while B3 is held) |
| B9 / B10 / B11 | F9 / Esc / F5 |

- **Pre-Warp display:** holds the lead time in minutes. DISPL CLR zeroes it.
- **LEDs:** backlit; warp-to buttons bright briefly on press **(design)**.

### 7.8 AUX CTRL (0x26)

| Button | Action |
|---|---|
| B0–B7, B9 | EVA keys (only on EVA; dark otherwise) |
| B8 | Cruise control (Q9) |
| B10 / B11 | Control point (action groups 35 / 36 / 37, only one at a time) |

### 7.9 Sticks

**Rotation (0x28):**

| Mode | Axes |
|---|---|
| Spacecraft | Yaw / pitch / roll |
| Plane | Roll / pitch / yaw |
| Rover | Steering / wheel throttle |
| EVA | W / A / S / D / Q / E held while deflected |

- **Buttons:** BTN01 reset trim (Alt+X); BTN02 set trim; stick button = airbrake (action group 38), or Left Shift on EVA.

**Translation (0x29):**
- **Axes:** translation X / Y / Z.
- **Stick button:** held = camera mode, so the stick moves the camera.
- **EVA:** W / A / S / D plus Left Shift / Left Ctrl.
- **Buttons:** BTN01 camera mode (V); BTN02 camera reset.

### 7.10 Other modules

| Module | Behaviour |
|---|---|
| **Throttle (0x2C)** | Lever to KSP throttle. Touching the lever or its buttons is a pilot override. Precision set by Switch Group 1 (Q8) |
| **GPWS (0x2A)** | Its setting and threshold are passed to the Annunciator |
| **Dual Encoder (0x2D)** | ENC1 = control group 1–6 (wraps); ENC1 button (Q10); ENC2 = `-` / `=`; ENC2 button = Backspace. The group shows on the Annunciator |

### 7.11 What the Annunciator shows, and where it comes from

| Annunciator field | Source |
|---|---|
| Control mode | Ctrl Mode Toggle (Q3) |
| Control group | Dual Encoder ENC1 |
| AUDIO | Switch Group 2 AUDIO |
| THRTL ENA, ENG ARM, THRTL PREC | Throttle switches in Switch Group 1 (Q8) |
| INPUT PREC | Switch Group 2 THC / RHC PREC |
| TRIM | Set by rotation BTN02 **(design)** |
| DEBUG | Switch Group 1 UPTLM XMIT |
| WARP | KSP |
| AUTOPILOT | Any autopilot engaged |
| SIMPIT LOST | In flight, connected, but nothing from KSP for 5 s **(design)** |
| SWITCH ERR | A switch moved to a position the master refuses, e.g. CHUTE to ARM with a chute already cut **(design)** |

## 8. Porting the autopilots

**Unchanged:** control laws, state machines, constants, the Info Display 2 console protocol, the arbiter, the staging helpers, and every function the autopilots expose.

**Changed — only where they connect to the rest of the master:**

| Today | In the new master |
|---|---|
| The Simpit handler feeds each autopilot | `simpit_link` feeds them the same way, from one table |
| Throttle commands go to `throttle_link` | Same functions, now in `ksp_out` |
| Rotation commands go to `rotation_link` | Same functions, now in `ksp_out` |
| The ascent autopilot sends its own rotation message | It hands its axes to `ksp_out` like the others, so there is one sender |
| The pilot-override check reads the joysticks itself | It reads the panel state; same thresholds |
| Autopilots send SAS / warp / stage commands directly | They go through the `ksp_out` queue |
| Shared globals (`brakes_on`, …) | Fields of `vessel` |

Files are moved with `git mv` before editing, so each diff shows only these changes.

## 9. Board services

| Service | Plan |
|---|---|
| **Fan** (EMC2101) | Reads the temperature sensor on header H4, falling back to the chip's own if H4 is not connected. Duty 30 / 40 / 50 / 70 / 100 % at 25 / 30 / 40 / 50 °C, checked once a second with 2 °C hysteresis **(design)** |
| **Power** (INA228) | Voltage, current and power four times a second. Warns on Serial if 12 V drops below 11 V **(design)** |
| **Watchdog** | Starts at the end of `setup()`, fed every pass, 1 s timeout. The KSP connection attempt (about 1 s) is kept clear of it **(design)** |
| **Display reset** (pin 2) | Normally high. A 10 ms low pulse reboots every display panel at once (Q12) |
| **Status LEDs** | None to drive: the board's LEDs are hardware power indicators |
| **Sys Info Display** (0x14) | Gets power, fan and link status once its protocol is defined (Q6) |

## 10. Verification

- **Host build, every stage.** Compiled on a PC against both a Simpit stand-in and the real library.
- **Selftest with simulated modules.** A simulator plays every module on the bus. Tests press buttons, flip switches, turn encoders and move sticks, then check:
  - what reached KSP;
  - the LEDs;
  - the display commands.

  The autopilots are checked through the same harness with scripted flight data.
- **Display panels:** unaffected; their own checks re-run if a shared stand-in changes.
- **On the bench:** a master section in the test plan, one session per panel, written as each stage lands.

## 11. Stages

One PR per stage.

| # | Stage | Builds | Done when |
|---|---|---|---|
| 1 | **Skeleton** | Pin map, state, KSP connection, display commands, watchdog, host tools | Starts up exactly like today: displays released before KSP, 2 s reconnect, WARP flag |
| 2 | **Module bus** | Module table, interrupt-line learning, start-up, events, LEDs, on / off | Every module comes up, follows the flight scene, and is found again after re-cabling |
| 3 | **Flight panels** | Vehicle, Stability, Action, control group, action-group map, safe lines, Annunciator fields | Every button, switch and LED on those panels matches §7 |
| 4 | **Sticks and throttle** | Output merging, stick mapping per mode, camera, EVA keys, throttle lever | Correct messages per mode; override works |
| 5 | **Remaining panels** | UI, Function + Switch Group 1, Time + Pre-Warp, AUX, cameras, GPWS | All of §7 covered |
| 6 | **Autopilots** | The port (§8) and the Info Display 2 console | Autopilot checks pass; consoles engage and disengage |
| 7 | **Board services** | Fan, power monitor, Sys Info link if ready | Readings on Serial; fan follows the curve |
| 8 | **Cutover** | Delete `Controller_Main`; rewrite the Hardware Reference; update the test plan, I2C spec notes and README links; close #89 | Nothing refers to `Controller_Main` |

## 12. Open questions

Each question has a **default** that gets built if there is no other answer. The numbers are kept from earlier drafts, so some are missing.

### Hardware — need your answer (stage 3)

**Q2 — Staging and abort**
- **Question:** where do STAGE and ABORT come from?
- **Why it is open:** the Module UI Reference puts Stage / Stage Enable on Stability Control B14 / B15 and has no abort input, but the panel layout has a staging button on B2 and an Abort button on A2.
- **Default:** stage from Stability B14, with B15 as the enable; ABORT from INT_SAFE.

**Q3 — Ctrl Mode Toggle**
- **Question:** which module inputs is the switch wired to?
- **Why it is open:** no spare Teensy pin reaches a connector, so the 3-position switch can only reach the master through a module input.
- **Default:** two spare inputs on a 24-input module. Until it is wired, control mode follows the vessel type KSP reports: plane → Plane, rover → Rover, anything else → Spacecraft.

**Q14 — The four safe lines**
- **Question:** what is wired to INT_SAFE, INT_SAFE2, B_INT_SAFE and B_INT_SAFE2?
- **Why it is open:** they are raw switch lines straight to the master. The Ctrl Module schematic shows where they go but not what drives them.
- **Default:**
  - INT_SAFE = ABORT (the Abort button is on A2);
  - B_INT_SAFE = STAGE button (B2), unless staging comes from Stability (Q2);
  - INT_SAFE2 and B_INT_SAFE2 unused.

  All are active-low and debounced 10 ms.

### Behaviour — defaults proposed

| # | Question | Default | Stage |
|---|---|---|---|
| Q4 | The I2C spec says turn modules off on EVA, but AUX CTRL's EVA buttons need them on | Keep modules on during EVA; update the spec | 2 |
| Q5 | Simpit does not report game pause | Ignore pause | 2 |
| Q6 | Define the Sys Info Display protocol now or later? | Later | 7 |
| Q8 | The reference text for the throttle switches B19 / B23 reads swapped against their labels | B18 ENGINE ARM = throttle module on; B19 THROTTLE ACT = lever reaches KSP; B23 THRTL FINE = lever precision | 4 |
| Q9 | AUX B8 cruise control: the rover autopilot already has cruise | B8 engages / disengages rover cruise at the current speed | 5 |
| Q10 | ENC1 button was "sync the master's TFT", but there is no TFT | Reset the control group to 1 | 3 |
| Q12 | What MSTR RESET and SCE AUX reset | MSTR RESET: reset all modules and displays, all autopilots off. SCE AUX: reconnect to KSP and refresh | 5 |
| Q13 | Which hub slot each module is in | Not needed: the master learns it (§5.2). A slot list, if you have one, becomes the default | 2 |
| Q15 | Sketch name | `Software/KCMk1_Master/` | 1 |

**Resolved:** Q1 (rev-2 pin map, Appendix A); Q7 (power monitor is an INA228 at 0x40); Q11 (UI Control LEDs, now in §7.2).

---

## Appendix A — Ctrl Module pin map (KC-01-1701 V2.0)

From the schematic, sheet 1. This is the pin map `config.h` will carry.

### A.1 Interrupt lines (all inputs)

| Lines | Teensy pins | Panel |
|---|---|---|
| INT_1 – INT_8 | 9, 10, 25, 26, 27, 28, 29, 30 | **A2** — left input panel |
| INT_SAFE | 31 | A2 |
| INT_9 – INT_12 | 4, 5, 6, 7 | **A1** — left display panel |
| INT_SAFE2 | 8 | A1 |
| B_INT_1 – B_INT_4 | 23, 22, 21, 20 | **B1 / B2** via the Ctrl Ext Module |
| B_INT_5 – B_INT_6 | 15, 14 | B1 / B2 |
| B_INT_7 – B_INT_12 | 41, 40, 39, 38, 37, 36 | B1 / B2 |
| B_INT_SAFE | 35 | B1 / B2 |
| B_INT_SAFE2 | 34 | B1 / B2 |

**Why A2 is the 8-line connector:** A2 has 7 modules (UI, Function, Action, Translation, GPWS, Throttle, Sys Info Display). A1 has only the two 7" displays and the power button, so it is on the 4-line connector. The B side has 12 lines for 9 modules:
- **B2:** Stability, Vehicle, Time, AUX, Rotation, Pre-Warp;
- **B1:** Info Display 2, Resource Display, Dual Encoder.

### A.2 Other pins

| Teensy pin | Signal | Use |
|---|---|---|
| 2 | RST (out) | Reboots every display panel; 47 Ω series (R13) |
| 18 / 19 | SDA / SCL BUS | `Wire` — every module and display, INA228, EMC2101 |
| 17 / 16 | SDA / SCL EXT | `Wire1` — expansion bus (unused for now) |
| 33 | EXT_INT_1 (in) | From the expansion unit; 4.7 kΩ pull-up, 47 Ω series (R15) |
| 32 | EXT_INT_2 (out) | To the expansion unit |
| 0, 1, 3, 11, 12, 13, 24 | — | Not connected |

> **The old sketch's pins are all wrong on this board.** Throttle reads pin 32 (now EXT_INT_2), rotation 33 (EXT_INT_1), translation 31 (INT_SAFE), GPWS 38 (B_INT_10), staging 30 (INT_8), abort 37 (B_INT_11).

### A.3 Devices on the board

| Device | Address | Notes |
|---|---|---|
| INA228 power monitor | 0x40 | On `Wire` |
| EMC2101 fan controller | 0x4C | On `Wire`. Drives the fan (connector CN1 and the interboard cable); temperature sensor input on header H4 |
| LTC4311 ×2 | — | Speed up the BUS and EXT I2C lines |
| USB hub (2× CH334F) | — | One USB-C in; six Micro-USB and one expansion USB-C out. No firmware |
| Status LEDs | — | Power-rail indicators, hardware only |

### A.4 Where the schematic and the Hardware Reference disagree

For the hardware owner; the firmware follows the schematic.

1. **ST7789 status display.** The Hardware Reference lists one on the Ctrl Module. The schematic has none (the SPI pins are not connected).
2. **EXT_INT series resistor.** The Hardware Reference puts the 47 Ω resistor on the EXT_INT_2 *output*. The schematic puts it on the EXT_INT_1 *input* and leaves EXT_INT_2 unprotected. **Check before the expansion unit is built.**

---

## Appendix B — Glossary

| Term | Meaning |
|---|---|
| **Master** | The Teensy 4.1 on the Ctrl Module; this firmware |
| **Module** | An ATtiny816 input panel (buttons, switches, sticks, encoders, 7-segment displays) on the I2C bus |
| **Carrier / display panel** | A Teensy 4.1 driving a screen: Annunciator, Info Displays 1 / 2, Resource Display, Sys Info Display |
| **INT line** | A module's own wire to the master, pulled low when it has something new to report |
| **Safe line** | A raw switch wire to the master (INT_SAFE etc.), not a module interrupt |
| **Simpit** | KerbalSimpit, the KSP plugin and library the master uses to talk to the game |
| **AG / CAG** | Action group / custom action group (from the AGX mod), toggled by number |
| **Control group** | Which of six sets of custom action groups the buttons fire (ENC1) |
| **Control mode** | Spacecraft, plane or rover; sets how the sticks are mapped |
| **Switch Group 1 / 2** | The panel toggle switches wired into Function Control and Vehicle Control |
| **Arbiter** | Decides which autopilot owns attitude and which owns throttle |
| **Lifecycle** | A module's on / off state as the master sets it (BOOT_READY, DISABLED, ACTIVE) |
| **Cutover** | The final stage, when the new master replaces `Controller_Main` |
