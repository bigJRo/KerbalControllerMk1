# KCMk1 Master Firmware — Design

| | |
|---|---|
| **Document type** | Developer — design for approval |
| **Location** | `Documents/Developer/Master_Firmware_Design.md` |
| **Status** | Draft 5, for review. Nothing here is implemented yet |
| **Draft 5** | Rebuilt from the board-by-board and panel-by-panel review recorded in `Hardware_Inventory.md`. Adds the start-up / boot reporting design, demo and debug modes, the new autopilot authority rule, the Sys Info Display, and the watchdog design (§12.3) |
| **Supersedes** | Drafts 1–4, and `Controller_Main_IO_Design.md` |

---

## At a glance

We are replacing the master firmware (`Software/Controller_Main`) with a **new sketch written from the ground up**, `Software/KCMk1_Master/`.

- **Why:** the old sketch and the Hardware Reference are out of date and contradict the rev-2 hardware. For example, none of the old sketch's interrupt pins matches the rev-2 Ctrl Module.
- **Built from:** the hardware inventory, made from the schematics and confirmed panel by panel. It covers every board, connector, slot, switch and button label.
- **Kept:** the autopilots' control laws and consoles. They are ported, with one behaviour change agreed in the review: sticks no longer disconnect them (§11).
- **How:** eight stages, one PR each, every one checked on the host before it merges (§16). Display and module firmware changes are a separate list (§17).

### Before work starts

1. **Approval of this plan.** The watchdog design (§12.3) has been reviewed and settled.

---

## Contents

1. [Decisions](#1-decisions)
2. [Sources](#2-sources)
3. [What the master does](#3-what-the-master-does)
4. [Architecture](#4-architecture)
5. [Start-up and boot reporting](#5-start-up-and-boot-reporting)
6. [Module bus](#6-module-bus)
7. [Outputs to KSP](#7-outputs-to-ksp)
8. [Panel behaviour](#8-panel-behaviour)
9. [Annunciator fields](#9-annunciator-fields)
10. [Demo and debug modes](#10-demo-and-debug-modes)
11. [Autopilots](#11-autopilots)
12. [Board services and the watchdog](#12-board-services-and-the-watchdog)
13. [Display carriers](#13-display-carriers)
14. [Sys Info Display](#14-sys-info-display)
15. [Verification](#15-verification)
16. [Stages](#16-stages)
17. [Work outside the master](#17-work-outside-the-master)
18. [Open items](#18-open-items)
- [Appendix A — Pin map](#appendix-a--pin-map)
- [Appendix B — Glossary](#appendix-b--glossary)

---

## 1. Decisions

- **New sketch, from the ground up**, in `Software/KCMk1_Master/`. `Controller_Main` gets no further changes apart from a fix that blocks bench testing. It is deleted at cutover.
- **The autopilots are ported, not rewritten.** Their control laws, state machines and console protocol carry over. The interfaces they use to reach the rest of the master are re-pointed, and the pilot-override rule is replaced (§11).
- **Hardware truth is `Hardware_Inventory.md`.** It was made from the schematics and confirmed against the panels. The Hardware Reference and the Module UI Reference are rewritten from it at cutover.
- **Plan first.** This document is approved before stage 1 starts.

## 2. Sources

Where two sources disagree, the higher one wins.

1. **`Hardware_Inventory.md`:** boards, connectors, slot assignments, switch groups, every button map and label, and the behaviour decisions from the review (its §8, §10).
2. **The schematics** behind it:
   - Ctrl Module 1701 and Ctrl Module Extension 1711;
   - Panel Hub 1731;
   - 7" and 2" display drivers 1911 / 1901;
   - Button 1801, Wide Button 1811, Throttle 1871, Joystick 1831, 7-Segment 1841, Dual Encoder 1861 and Encoder 1851.
3. **I2C Protocol Specification v2.14:** module packets, lifecycle, commands, display-carrier framing.
4. **Display firmware and READMEs:** the commands the displays expect today.
5. **Autopilot design docs:** `Hold_Mode_Autopilot.md`, `Mission_Autopilot.md`, `Ascent_Autopilot.md`, `Ascent_Autopilot_Interface.md`, read with the authority change in §11.
6. **KerbalSimpit 2.4.0** library source: message ids, payloads, warp targets, SAS availability.

The Module UI Reference v5.9 is **superseded** for button maps and switch groups. Its button layouts, Switch Group 1 order and several functions no longer match the panels.

## 3. What the master does

| Job | In short |
|---|---|
| **Talk to KSP** | Runs its own Simpit link on `SerialUSB1` (Teensy USB type *Dual Serial*). Keeps the vessel state everything else reads. It is the only sender of control messages to KSP |
| **Run the module bus** | Brings up every input module, reads it when it signals, turns its packets into events, drives its LEDs |
| **Turn inputs into actions** | Simpit commands, custom action groups, keyboard shortcuts, and the master's own modes |
| **Arbitrate control** | One rotation, translation, wheel and throttle message per frame. An engaged autopilot has full authority until it is disconnected deliberately (§7, §11) |
| **Enforce interlocks** | ENGINE SAFE, A/P INHIB, CHUTE SAFE, SAS OFF, the switch rules and SWITCH ERR |
| **Run the autopilots** | And their console on Info Display 2; HOLD |
| **Feed the displays** | Annunciator, Resource Display, Info Displays 1 / 2, Sys Info Display |
| **Report** | Boot progress to the Sys Info Display; debug and a command console on Serial |
| **Look after the board** | Fan, power monitor, watchdog, display reset |

**Out of scope:** the expansion unit. Its pins (EXT bus, EXT_INT_1 / 2) are reserved and left idle.

## 4. Architecture

### 4.1 Layers

```
  INPUTS              STATE             DECISIONS                OUTPUTS
  ─────────────       ───────────       ───────────────────      ─────────────────────
  simpit_link   ──▶   vessel      ──▶   panel handlers     ──▶   ksp_out     ──▶ KSP
  module_bus    ──▶   panel       ──▶   interlocks         ──▶   module LEDs ──▶ modules
  board         ──▶   master      ──▶   autopilots, HOLD   ──▶   carriers    ──▶ displays
  console (Serial)                      annunciation             debug log   ──▶ Serial
```

1. **Inputs only write state; decisions only read state and ask outputs to act.** Only outputs touch Simpit, the module bus or the displays.
2. **Each piece of state has one writer:**
   - `vessel` is written by `simpit_link`;
   - `panel` (switch positions, control mode, control group, trim, precision) by the panel handlers;
   - `master` (lifecycle, demo / debug, link health, faults) by the master core.
3. **Every tab has a header**, so the host build and the IDE see the same code without the IDE's prototype generation.
4. **Demo is one gate in the outputs.** In demo, `ksp_out` sends nothing, and every "KSP connected / in flight" check reads as true (§10.1). Nothing above the outputs needs a demo special case except the LEDs, which show local state.

### 4.2 Main loop

The same order every pass, with nothing blocking:

| # | Call | Does |
|---|---|---|
| 1 | `wdtFeed()` | The single watchdog feed point (§12.3) |
| 2 | `simpitService()` | Inbound KSP messages; retries the connection while it is down |
| 3 | `boardServiceFast()` | Safe-line debounce, mode selector, console input |
| 4 | `moduleBusService()` | Reads every module that is signalling; its panel handler acts at once |
| 5 | `interlockService()` | ENGINE SAFE / ARM, A/P INHIB, SWITCH ERR evaluation |
| 6 | `autopilotService()` | Autopilots, HOLD and the arbiter |
| 7 | `kspOutService()` | Merged stick / throttle message (every 20 ms or on an owner change) and queued commands; nothing in demo |
| 8 | `moduleBusFlushLeds()` | LED changes, at most every 50 ms per module |
| 9 | `carrierService()` | Display commands every 100 ms and on change; consoles; input test frame |
| 10 | `sysInfoService()` | Sys Info Display: boot lines, status frame, switch-error list |
| 11 | `boardServiceSlow()` | Fan (1 Hz), power monitor (4 Hz), loop timing |

A breadcrumb in no-init RAM records which call is running, for the watchdog report (§12.3).

**Timing target:** under 2 ms per pass when idle, under 5 ms worst case; logged in debug.

### 4.3 Files

```
Software/KCMk1_Master/
│
├─ KCMk1_Master.ino      setup() and loop() only
├─ config.h              pin map, addresses, timings — all in one place
├─ state.h/.ino          vessel, panel and master state
├─ boot.h/.ino           start-up sequence and boot reporting (§5)
│
├─ simpit_link.h/.ino    KSP connection, inbound messages
├─ module_bus.h/.ino     module driver (§6)
├─ ksp_out.h/.ino        the only sender to KSP (§7)
├─ interlocks.h/.ino     ENGINE, A/P, SWITCH ERR (§8.2, §8.3)
├─ cag.h/.ino            action-group numbering
├─ keys.h/.ino           keyboard shortcuts and macros
│
├─ panel_ui.ino          UI Control            0x20
├─ panel_function.ino    Function Control      0x21  + Switch Group 1
├─ panel_action.ino      Action Control        0x22
├─ panel_stability.ino   Stability Control     0x23  (SAS, HOLD, INVERT)
├─ panel_vehicle.ino     Vehicle Control       0x24  + Switch Group 2, state machines
├─ panel_time.ino        Time Control          0x25  + mode selector, Pre-Warp 0x2B
├─ panel_aux.ino         AUX CTRL              0x26
├─ panel_sticks.ino      Rotation / Translation 0x28 / 0x29, trim
├─ panel_gpws.ino        GPWS Input Panel      0x2A
├─ panel_throttle.ino    Throttle Module       0x2C
├─ panel_encoders.ino    Dual Encoder          0x2D
├─ panel_safe.ino        safe lines: ABORT, STAGE
│
├─ carriers.h/.ino       Annunciator, Resource Display, Info Displays
├─ sysinfo.h/.ino        Sys Info Display (§14)
├─ debug.h/.ino          debug log and serial command console (§10.2)
├─ board.h/.ino          fan, power, watchdog, display reset
├─ ap_*.ino / .h         the ported autopilots (§11)
│
└─ tools/                host build, selftest, module simulator
```

## 5. Start-up and boot reporting

`setup()` runs these steps in order. Each step sends a line to the Sys Info Display (§14) **and** to Serial, and each has its own timeout. A step that times out is reported in red with what it was waiting on, and start-up carries on.

| # | Step | Line on the boot screen | Timeout |
|---|---|---|---|
| 1 | Serial, Wire (BUS) at 400 kHz, pins | `CONFIG  …` | — |
| 2 | Wait for the Sys Info Display to answer | (first line it shows) | 3 s, then carry on without it |
| 3 | Boot banner: firmware version, reset cause (§12.3) | `KCMk1 MASTER vX.Y  RESET: …` | — |
| 4 | Board devices: INA228, EMC2101 | `POWER MON OK` / `FAN CTRL OK` or MISSING | 100 ms each |
| 5 | Load the stored INT map (EEPROM) | `INT MAP: STORED / DEFAULT` | — |
| 6 | Module discovery and INT learning (§6.2) | one line per module: `0x24 VEHICLE  B_INT_4  OK` / MISSING / WRONG TYPE | 50 ms per module |
| 7 | Display carriers: PROCEED to each, wait for its status | one line per display: `ANNUNCIATOR  OK` / NO ANSWER | 500 ms each |
| 8 | First Simpit handshake attempt | `KSP: CONNECTING …` | about 1 s (library) |
| 9 | Start the watchdog (§12.3) | `WATCHDOG ON` | — |

Then `loop()` starts. The panels, displays and switches all work, and **the master never waits for KSP.**

- **The boot screen stays up** until the master's Simpit link connects, or SCE is switched to AUX (demo). Until then it shows a `WAITING FOR KSP` line with elapsed time and retry count.
- **After 60 s,** a hint line is added: KSP running? Simpit COM port? USB cable?
- **Then** the Sys Info Display changes to its status screen.

**Stalls:**
- **Every I2C transaction is bounded,** so a stuck bus is reported, not hung on. A bus held low gets a recovery attempt (SCL clocked to free it) and a red line.
- **A master that hangs cannot report itself.** The Sys Info Display shows `MASTER NOT RESPONDING` and the last line received if it hears nothing for 5 s during boot (§14).
- **The watchdog does not run during `setup()`,** so a stalled boot stays on screen rather than rebooting out of view.

## 6. Module bus

### 6.1 Module table

```c
struct ModuleDef {
  uint8_t addr;          // I2C address, 0x20..0x2D
  uint8_t typeId;        // module type expected in its packet header
  uint8_t defaultInt;    // default INT pin (config.h), or NO_PIN
  uint8_t flags;         // has LEDs, 24 inputs, active on EVA, ...
  void  (*onPacket)(const uint8_t *pkt, uint8_t n);   // its panel handler
};
```

Expected slots (Inventory §7), which become the `config.h` default map:

| Line | Module | | Line | Module |
|---|---|---|---|---|
| INT_1 | Action Control 0x22 | | B_INT_1 | — |
| INT_2 | Sys Info Display 0x14 | | B_INT_2 | Pre-Warp 0x2B |
| INT_3 | Throttle 0x2C | | B_INT_3 | Time Control 0x25 |
| INT_4 | UI Control 0x20 | | B_INT_4 | Vehicle Control 0x24 |
| INT_5 | — | | B_INT_5 | — |
| INT_6 | GPWS 0x2A | | B_INT_6 | Stability Control 0x23 |
| INT_7 | Translation 0x29 | | B_INT_7 | AUX CTRL 0x26 |
| INT_8 | Function Control 0x21 | | B_INT_8 | Rotation 0x28 |
| INT_9 | Annunciator 0x10 | | B_INT_9 | Info Display 2 0x13 |
| INT_10 | — | | B_INT_10 | — |
| INT_11 | — | | B_INT_11 | Dual Encoder 0x2D |
| INT_12 | Info Display 1 0x12 | | B_INT_12 | Resource Display 0x11 |

Safe lines: INT_SAFE = ABORT, B_INT_SAFE = STAGE; INT_SAFE2 and B_INT_SAFE2 unused (§8.6).

### 6.2 Finding each module's interrupt line

1. At power-on, every module holds INT low until first read (I2C spec §3).
2. The master reads the modules one at a time.
3. The line that goes high belongs to that module. The display carriers are found the same way, with a STATUS request.

- **Storage:** the learned map is saved in EEPROM and used when the master restarts on its own (modules already up, not signalling).
- **First boot:** the default map above covers a first boot.
- **Unknown line:** a module with no known line is polled every 20 ms. Button modules return their full current state on every read, so polling works, at the cost of bus time.
- **Mismatches** between the learned, stored and default maps go to the boot screen and the debug log.

### 6.3 Start-up per module

1. **Identity:** check the type against the table.
2. **LEDs off.**
3. **DISABLED.**

A module that does not answer, or reports the wrong type, is marked absent and re-checked every 5 s, so a panel plugged in late comes up. After a watchdog restart the modules are **not** reset; their state is read back instead (§12.3).

### 6.4 Reading a module

- **Per-packet checks:** type; lifecycle (a module that rebooted is disabled and returned to the wanted state); fault bit (logged and acknowledged); transaction counter (a gap is logged).
- **Button events.** Each packet carries the current state and the buttons changed since the last read. The bus makes four kinds of event:

  | Event | Meaning |
  |---|---|
  | **pressed** | Changed, and now down |
  | **released** | Changed, now up, and the master saw it down |
  | **tapped** | Changed, now up, but the master never saw it down |
  | **held** | Down continuously for a set time (used for the 2 s state-machine reset) |

- **Act on press or on release.** Most handlers act on *pressed or tapped*. The heat-shield and parachute buttons act on *release*, so that a 2 s hold can be a reset without first firing the button (§8.5).
- **LEDs:** handlers set the wanted state per button; the bus sends only changes, at most every 50 ms per module. LTG TEST overrides everything while held.

### 6.5 Module lifecycle

| Situation | Modules |
|---|---|
| Boot, KSP not connected, not in demo | DISABLED (dark) |
| Flight scene entered | All ENABLEd |
| Flight scene left, or KSP lost | All DISABLEd |
| Kerbal on EVA | Stay ENABLEd; handlers decide what works |
| Game paused | No change (Simpit has no pause signal) |
| **Demo on** | All ENABLEd for the duration, whatever the scene |

The I2C spec's "disable on EVA" is updated to match (§17).

## 7. Outputs to KSP

KSP keeps only the latest message on each control channel, so `ksp_out` is the only code that sends them. **In demo it sends nothing at all** (§10.1).

| Channel | Who controls it |
|---|---|
| **Rotation** | The pilot's stick, mapped by control mode (§8.7), plus the master's trim. While any autopilot or HOLD is engaged: the autopilot only, and the stick is ignored |
| **Translation** | The pilot's stick; ignored while an autopilot or HOLD is engaged. Camera mode (stick button held) moves the camera instead, and keeps working under an autopilot |
| **Wheel** | Steering and wheel throttle from the stick in RVR, or the rover autopilot / HOLD. The wheel throttle is held at 0 while ENGINE is SAFE |
| **Throttle** | One owner: the pilot's lever, or the autopilot the arbiter gave it to. ENGINE SAFE overrides both (§8.2). Under an autopilot the lever follows the autopilot (§11.3) |
| **Commands** | Action groups, SAS, staging, warp, camera, keys: queued, sent in order, rate-limited |

## 8. Panel behaviour

The button maps, labels and layouts are in **Inventory §10** and are not repeated here. This section gives the rules.

### 8.1 Shared rules

- **Custom action groups:** `CAG = base + (control group − 1) × 40`, control group 1–6 from Dual Encoder ENC1.
  - **Exception:** Action Control's AG 1–12 always send CAG 1–12.
- **Activate vs toggle:** action-group buttons toggle, except the state machines (§8.5), which only ever activate.
- **LEDs:**
  - lit while KSP reports the function active;
  - backlit when the button is usable now;
  - **dark when it is not usable**, and its press is then ignored. That covers EVA buttons outside EVA, SAS modes with SAS off or unavailable, chutes with CHUTE SAFE, and WARP TO buttons whose event does not exist.
- **Momentary-only buttons** (UI Control, etc.) flash ACTIVE for 150 ms; KSP reports none of their states.

### 8.2 Switch Group 1 (Function Control B16–B23)

| Input | Switch | Type | Does |
|---|---|---|---|
| B16 | MSTR OPER / RESET | Momentary | Master reset: `CMD_RESET` all modules, `DISPLAY_RESET` to the displays, all autopilots and HOLD off. RST is kept for a display that stops answering |
| B17 | DISP OPER / CLR | Momentary | Zero the GPWS and Pre-Warp displays; reset the screens to default |
| B18 | SCE NORM / AUX | Latching | Demo mode off / on (§10.1) |
| B19 | UPTLM INHIB / XMIT | Latching | Debug mode off / on (§10.2) |
| B20 | LTG NORM / TEST | Momentary | Bulb test while held |
| B21 | A/P INHIB / ARM | Latching | Autopilot master arm |
| B22 | ENGINE SAFE / ARM | Latching | Throttle interlock |
| B23 | THRTL STD / FINE | Latching | Throttle-module precision |

**ENGINE SAFE / ARM (B22):**

| Position | What the master does |
|---|---|
| **SAFE** | Sends 0 % throttle to KSP and drives the lever to 0 %. Every throttle input is ignored: lever, lever buttons, autopilots. The rover wheel throttle is held at 0; steering still works. A throttle-using autopilot cannot engage, and one engaged drops (reason ENG SAFE). If the lever is moved, it is driven back to 0 % on release. The module stays enabled, and the master re-sends 0 % until the lever reports it |
| **ARM** | Drives the lever to 0 % and sends 0 % again; once the lever is there, inputs are taken |

**A/P INHIB / ARM (B21):**
- **INHIB:** no console autopilot can engage, and switching to INHIB disconnects every engaged one (reason A/P INHIB).
- **ARM:** consoles work as normal.
- **HOLD is not affected** by this switch (§8.4).

### 8.3 Switch Group 2 (Vehicle Control B16–B23) and SWITCH ERR

| Input | Switch | Rule |
|---|---|---|
| B16 | CHUTE SAFE / ARM | Enables the parachute buttons; with SAFE they are dark and ignored |
| B17 | GEAR UP / DOWN | Sets the gear on the flip. LANDING GEAR button still toggles |
| B18 | BRAKE REL / LOCK | Sets the brakes on the flip. BRAKES button = brakes while held; on release they come off only if the switch is REL |
| B19 | EXT LT OFF / ILLUM | Sets the lights on the flip. LIGHTS button still toggles |
| B20 | SAS OFF / ENAB | Sets SAS on the flip. With OFF the SAS mode buttons are dark and ignored |
| B21 | RCS OFF / ENAB | Sets RCS on the flip |
| B22 | THC/RHC NORM / PREC | Stick precision |
| B23 | AUDIO MUTE / LIVE | Annunciator audio |

**General rule for every switch:**
- A switch acts **only when flipped.**
- Entering a flight scene or changing vessel **never enforces** a switch.
- Whenever a switch disagrees with the vessel, **SWITCH ERR** lights until they agree. The Sys Info Display names the switches (§14).

**Exceptions:** no SWITCH ERR for SAS while INVERT is held, or for SAS / RCS while an autopilot or HOLD that changes them is engaged.

### 8.4 Stability Control: SAS, HOLD, INVERT

**SAS mode buttons (B0–B9):**

| SAS switch | LEDs | Presses |
|---|---|---|
| OFF | All dark | Ignored |
| ENAB | KSP's current mode lit, available modes backlit, unavailable modes dark | Ignored for unavailable modes |

Current mode and availability come from Simpit `SAS_MODE_INFO` (mode 255 = SAS off; `SASModeAvailability` bitmask).

**HOLD (B10):** a temporary hold of the current condition. It is not gated by A/P ARM, and A/P INHIB does not drop it.

| Control mode | Holds | How |
|---|---|---|
| SPC | Attitude | KSP SAS Stability Assist |
| PLN | Altitude + heading | Hold autopilot ALT + HDG at the current values |
| RVR | Heading + speed | Hold autopilot RHDG + CRUISE at the current values; heading only while ENGINE is SAFE |

- **Released by:** a second press, A/P disconnect, or a console autopilot engaging.
- **While holding:** the LED is ACTIVE and the Annunciator's AUTOPILOT lights.

**INVERT (B11):** holds F while held.

### 8.5 Vehicle Control state machines (HEAT SHIELD, MAIN, DROGUE)

The state comes from KSP's report of the two action groups, not from memory in the master.

| Deploy CAG | Cut / release CAG | State | LED | Release (under 2 s) does |
|---|---|---|---|---|
| off | off | READY | Backlit | Activate the deploy CAG |
| on | off | DEPLOYED | Green | Activate the cut / release CAG |
| either | on | CUT / RELEASED | Red | Nothing |

- **2 s hold:** deactivates both groups, returning the button to READY (after a repack). The chutes need CHUTE ARM for this.
- **Vessel switches need no reset;** each vessel's groups are simply shown.

### 8.6 Safe lines and the mode selector

| Input | Function |
|---|---|
| **INT_SAFE (A2): ABORT** | Rising edge → Simpit ABORT action group. Arming switch in series on the panel, so a press only arrives while armed |
| **B_INT_SAFE (B2): STAGE** | Rising edge → STAGE, flight scene only, 1 s lockout. Arming switch in series |
| **Mode selector (Time Control B12 / B13)** | B12 = SPC, neither = PLN (centre), B13 = RVR. Both on is reported as a debug message (not SWITCH ERR), and the last good mode is kept |

Safe lines read active-high, as plain inputs (no pull-up), debounced 10 ms.

### 8.7 Sticks

**Rotation stick mapping:**

| Mode | Rotation stick |
|---|---|
| SPC | Yaw / pitch / roll |
| PLN | Roll / pitch / yaw |
| RVR | Steering / wheel throttle |
| EVA | W / A / S / D / Q / E held while deflected |

**Rotation buttons:**
- **BTN02** sets the master's trim from the current deflection.
- **BTN01** clears it.
- **The trim clears** on a control-mode change.
- **The Annunciator's TRIM** lights while trim is set.

**Rotation stick button:**

| Mode | Function |
|---|---|
| SPC, PLN | A/P disconnect: every autopilot and HOLD |
| RVR | Brakes while held (same rule as BRAKES) |
| EVA | Left Shift (run) while held |

**Translation:**
- **Stick:** translation X / Y / Z.
- **Button held:** camera mode.
- **BTN01:** camera mode cycle.
- **BTN02:** camera reset.
- **EVA:** W / A / S / D + Left Shift / Left Ctrl.

**THC / RHC PREC** scales both sticks. **While any autopilot or HOLD is engaged, both sticks are ignored**, except camera mode and the stick buttons.

### 8.8 Time Control

**WARP TO buttons** warp to the event, less the Pre-Warp minutes:

| Button | Warp target | Lit when |
|---|---|---|
| ApA | Apoapsis | Orbit closed, not landed |
| PeA | Periapsis | Not landed |
| MNVR | **Burn start** (`TIMEWARP_TO_NEXT_BURN`) | A maneuver node exists |
| SOI | Next SOI change | In flight, outside the atmosphere |
| MORN | Next morning | Landed or splashed |

**Other buttons:**
- **STOP:** cancels any warp, including a warp-to, back to 1×.
- **PHYS + > / <:** steps physics warp 1×–4×.
- **> / <:** normal warp.
- **LOAD:** holds F9 for as long as it is held.
- **PAUSE:** Esc.
- **SAVE:** F5.

The availability rules are checked against Simpit on the bench.

### 8.9 Other panels

| Panel | Behaviour |
|---|---|
| **AUX CTRL** | **[EVA] group** (B0–B7, B9): EVA keys, dark outside EVA. **B8 UNDOCK:** CAG 37. **B10 PRI / B11 ALT:** control points (CAG 35 / 36), mutually exclusive; the LED shows the active one, and pressing the active one does nothing |
| **Function Control** | CAG buttons per Inventory §10.5. **B4 = FLAPS** (CAG 39) |
| **UI Control** | Keyboard and Simpit navball cycle per Inventory §10.7 |
| **Throttle** | Lever, 100 % / UP / DOWN / 0 % buttons, all under the ENGINE and autopilot rules. **FINE** is dropped while an autopilot owns the throttle and restored after |
| **GPWS, Pre-Warp** | Module-managed. The master relays GPWS state to the Annunciator and reads the Pre-Warp minutes |
| **Dual Encoder** | **ENC1:** control group 1–6 (wraps). **ENC1 button:** resets it to 1. **ENC2:** camera next / previous. **ENC2 button:** camera reset |

## 9. Annunciator fields

| Field | Source |
|---|---|
| Control mode | Mode selector |
| Control group | ENC1 |
| DEMO | SCE |
| WARP | Simpit |
| AUDIO | AUDIO switch |
| **A/P ARM** (bit 3, was THRTL ENA) | A/P INHIB / ARM switch. The tile label changes in the Annunciator firmware (§17) |
| TRIM | Master trim set |
| AUTOPILOT | Any autopilot or HOLD engaged |
| DEBUG | UPTLM |
| THRTL PREC | THRTL FINE |
| INPUT PREC | THC / RHC PREC |
| ENG ARM | ENGINE SAFE / ARM |
| SIMPIT LOST | Connected and in flight, but nothing from KSP for 5 s |
| SWITCH ERR | Any switch disagreeing with the vessel (§8.3) |
| GPWS bytes | GPWS module state and threshold |

## 10. Demo and debug modes

### 10.1 Demo (SCE AUX)

**Purpose:** showing the controller off, and bench testing. Allowed any time.

- **Nothing goes to KSP.** `ksp_out` is silent, every "KSP connected / in flight" check passes, and autopilots and HOLD drop on entry.
- **Panels respond locally.** Buttons sit backlit and show ACTIVE while pressed; switches act on their own panel (e.g. CHUTE ARM enables the chute buttons); joysticks and 7-segments respond. There are no light patterns.
- **Displays:** all get the demo flag. Every display follows the master's flag and nothing else (display changes, §17).
- **Leaving demo:** modules return to the current scene's lifecycle, and LEDs to KSP's state. Switches flipped during demo are not acted on, so a mismatch lights SWITCH ERR.

### 10.2 Debug (UPTLM XMIT) and the serial console

- **The master** logs to `Serial` (USB). The debug flag also turns on every display's own debug output.
- **Format:** one line per event — timestamp, category, message.
- **Errors** always print. Everything else prints only while XMIT is on.
- **Rate limit:** output is limited, with "*n* suppressed" lines.

| # | Category | Logged |
|---|---|---|
| 1 | Modules | Found / missing, wrong type, re-probe |
| 2 | INT map | Learned lines; differences from stored / default |
| 3 | Module health | Faults, missed packets |
| 4 | Switches | SWITCH ERR detail; switch faults (mode selector both on) |
| 5 | KSP link | Connect / disconnect, scene and vessel changes |
| 6 | Autopilots | Engage / disengage with reason; HOLD |
| 7 | INT traffic | Each INT and the decoded packet |
| 8 | Master state | Every change of a state / config variable |
| 9 | Timing | Loop time, average and worst per second |
| 10 | To KSP | Every Simpit message sent, decoded (axes on change) |
| 11 | From KSP | Messages per channel, time since last |
| 12 | I2C | NACKs, short reads, timeouts, bus recovery |
| 13 | Displays | Commands sent (on change), console traffic |
| 14 | Ownership | Throttle / attitude owner, lever hold, ENGINE SAFE / ARM, lever sync |
| 15 | Board | Boot banner, reset cause, power, temperature, fan |
| 16 | Lifecycle | Every ENABLE / DISABLE / RESET sent |
| 17 | Keys | Keyboard events sent |
| 18 | Sticks | Stick positions, while in demo or debug |

**Console commands** (typed on Serial), at least:

| Command | Shows |
|---|---|
| `help` | The command list |
| `status` | Modes, interlocks, owners, link state |
| `modules` | Each module: present, type, line, faults |
| `intmap` | Learned / stored / default map |
| `simpit` | Channels, rates, last message ages |
| `switches` | Every switch position vs the vessel |
| `log <cat> on/off` | One category on or off |

## 11. Autopilots

### 11.1 What carries over

- **Unchanged:** control laws, state machines, constants, the Info Display 2 console protocol, the arbiter, the staging helpers, the ingest / setter / status functions.
- **Moved with `git mv`** before editing, so each diff shows only the changes below.

### 11.2 What changes

| Today | New master |
|---|---|
| The Simpit handler feeds each autopilot | `simpit_link` feeds them the same way, from one table |
| Throttle and rotation commands go to `throttle_link` / `rotation_link` | Same functions, now in `ksp_out` |
| The ascent autopilot sends its own rotation | It hands its axes to `ksp_out` like the others |
| SAS / warp / stage commands are sent directly | Through the `ksp_out` queue |
| Shared globals (`brakes_on`, …) | Fields of `vessel` |
| **Any pilot input on a stick or the lever disconnects every autopilot** | **Retired.** See §11.3 |

### 11.3 Authority

An engaged autopilot (or HOLD) keeps full authority until it is disconnected deliberately, by:
- the A/P disconnect (rotation stick button, SPC / PLN);
- A/P INHIB;
- a console's A/P OFF;
- HOLD's own button;
- ENGINE SAFE (throttle-using modes);
- its own disengage conditions.

While engaged:
- **Sticks:** ignored completely; camera mode and stick buttons still work.
- **Throttle:**
  - the autopilot sets it, and the lever is driven to match;
  - the lever, its buttons and touch are ignored;
  - the throttle module pushes back against a hand for at most about 0.5 s (stall protection), then returns the lever when released (Throttle firmware change, §17);
  - FINE is dropped while the autopilot owns the throttle and restored after.

The STICK / LEVER disconnect reasons go unused. The autopilot design docs are updated to match (§17).

## 12. Board services and the watchdog

### 12.1 Services

| Service | Plan |
|---|---|
| **Fan** (EMC2101, 0x4C) | Temperature from the EMC2101's **internal** sensor (the board temperature). No external sensor is fitted on H4, so the remote channel is not used and its open-diode fault is ignored. Duty 30 / 40 / 50 / 70 / 100 % at 25 / 30 / 40 / 50 °C, 1 Hz, 2 °C hysteresis. RPM from the Ctrl Module fan (the Extension fan has no tach) |
| **Power** (INA228, 0x40) | Voltage, current, power at 4 Hz; warning below 11 V |
| **Display reset** (pin 2) | Normally high; a 10 ms low pulse reboots the four 7" displays (not the Sys Info Display, whose RST is not connected) |

### 12.2 Status outputs

There are none: the board's LEDs are power indicators only.

### 12.3 Watchdog

The old sketch declares a watchdog but never starts it. This is the design (settled).

**Why it matters in flight.** KSP keeps the last control message it received. A master that hangs with a stick deflected, or with an autopilot's throttle applied, leaves that input on until something changes it. The watchdog limits how long that can last: at most the 5 s timeout, plus the restart and Simpit reconnect.

| Item | Design |
|---|---|
| **Hardware** | Teensy 4.1 WDOG1, through the `WDT_T4` library (already used) |
| **During `setup()`** | Off, so a stalled boot stays visible (§5). The Sys Info Display's own 5 s timeout reports a hung master |
| **Start** | The last step of `setup()` |
| **Timeout** | **5 s.** The longest blocking call in the loop is the Simpit handshake attempt (about 1 s), well inside it |
| **Feeding** | Once per loop pass, at one place only (§4.2 step 1); never from an interrupt or inside a wait |
| **Early warning** | The library's trigger callback, 0.5 s before the reset (at 4.5 s), stores the breadcrumb (§4.2) and prints a line |
| **After a reset** | The boot banner reports `RESET: WATCHDOG during <call> (<module>)` (reset cause from `SRC_SRSR`, breadcrumb from no-init RAM), on the Sys Info boot screen in red and on Serial |
| **Restart in flight** | See below |
| **Reboot-loop guard** | Three watchdog resets within 60 s: the master stops on the boot screen with the error and does not start the watchdog, so the fault stays visible. The count and times are kept in no-init RAM |
| **Where it is reported** | The Sys Info boot screen and Serial only; nothing on the Annunciator |

**Restart in flight:**
- Modules are **not** reset; their state is read back.
- The stored INT map is used.
- After the Simpit link reconnects, the first messages sent are neutral rotation, translation and wheel. The throttle follows the lever, under the ENGINE switch.
- Autopilots and HOLD come back disengaged, and this is logged.

## 13. Display carriers

| Display | Master sends | New in this design |
|---|---|---|
| **Annunciator 0x10** | The rev-3 9-byte command, every 100 ms and on change, with PROCEED; the GPWS bytes | Fields per §9; demo / debug flags |
| **Resource Display 0x11** | Its 2-byte command | Demo / debug flags |
| **Info Display 1 0x12** | Control bytes; console summary | **Input test screen frame** while that screen is open (§13.1) |
| **Info Display 2 0x13** | Control bytes; autopilot console frames and summary (unchanged protocol) | — |
| **Sys Info Display 0x14** | §14 | New |

**Every display reports its own Simpit link state** in its status packet, so the Sys Info Display can show it. Display firmware change (§17).

### 13.1 Input test screen (Info Display 1)

- **Opened and closed by:** a two-finger diagonal pull down from the top-right corner.
- **Available:** any time.
- **Shows:** sticks, the lever, every module's buttons and switches, the mode selector, safe lines, module presence and INT lines.
- **Data:** the display tells the master the screen is open, and the master then pushes a frame about 10 times a second.
- **Detailed design** (layout, frame format) is an open item (§18).

## 14. Sys Info Display

| Screen | When | Content |
|---|---|---|
| **Boot** | Power-on until KSP connects or demo is on | The boot lines of §5, a `WAITING FOR KSP` line, and the 60 s hint. `MASTER NOT RESPONDING` plus the last line if nothing arrives for 5 s |
| **Controller status** | Normal running | 12 V voltage, current, power; board temperature; fan speed; KSP link state of the master and each display |
| **SWITCH ERR window** | While SWITCH ERR is lit | In front of the status screen, naming each switch in error (switch, expected, actual) until all clear |

- **Graphics:** small icons and logos from its microSD card (set to be chosen, §18).
- **Protocol (0x14), outline.** The detailed layouts are part of stage 1:

  | Frame | Content |
  |---|---|
  | Boot line | Severity + text (≤ 40 characters) |
  | Boot done | Switch to the status screen |
  | Status | Voltage, current, power, temperature, fan RPM, link bitmap, every 500 ms |
  | Switch-error list | Switch ids with expected / actual, on change |

  Plus the usual PROCEED / control byte and a status packet back.

## 15. Verification

- **Host build** at every stage: `host_compile.py`, against both the Simpit stand-in and the real library.
- **Selftest** with a module simulator. Each address answers identity and serves scripted packets with INT. Tests drive presses, taps, holds, switch flips, encoders and sticks, and check:
  - what reached "KSP";
  - the LED frames;
  - the lifecycle commands;
  - the display and Sys Info frames;
  - interlocks (ENGINE SAFE, A/P INHIB, CHUTE, SAS), SWITCH ERR, demo silence, and the boot sequence with missing modules.
- **Autopilots:** checked through the same harness with scripted telemetry.
- **On the bench:** a master section in the test plan, written as each stage lands. It includes using the input test screen once it exists.

## 16. Stages

One PR per stage.

| # | Stage | Builds | Done when |
|---|---|---|---|
| 1 | **Skeleton and boot** | `config.h`, state, `simpit_link`, display commands, boot sequence (§5), Sys Info protocol (master side), debug log and console, watchdog, host tools | Boots against the simulator with modules missing; boot lines correct; displays released before KSP; 2 s reconnect; watchdog report after a forced hang |
| 2 | **Module bus** | Table, INT learning + EEPROM, events (incl. hold / release), LEDs, lifecycle, demo enable | Every module comes up, follows the scene, is found after re-cabling |
| 3 | **Switches and interlocks** | Switch Groups 1 and 2, SWITCH ERR, ENGINE SAFE / ARM, A/P INHIB, mode selector, safe lines (ABORT, STAGE), demo / debug | All §8.2, §8.3, §8.6 rules pass in the selftest |
| 4 | **Flight panels** | Vehicle (state machines), Stability (SAS, HOLD), Action, AUX, Dual Encoder, CAG map | §8.4, §8.5, §8.9 pass |
| 5 | **Sticks and throttle** | `ksp_out` arbitration, stick mapping, trim, camera, EVA keys, lever, throttle buttons, FINE handling | §7, §8.7 pass |
| 6 | **Remaining panels** | UI, Function, Time + Pre-Warp (availability), GPWS | All of §8 covered |
| 7 | **Autopilots** | Port (§11), authority rule, Info Display 2 consoles | Autopilot checks pass; consoles engage / disengage |
| 8 | **Board and cutover** | Fan, INA228, Sys Info status frame; delete `Controller_Main`; rewrite the Hardware Reference and Module UI Reference; update the test plan; close #89 | Nothing refers to `Controller_Main` |

The work outside the master (§17) runs alongside and is needed for full bench tests:
- the Sys Info Display firmware, by stage 1's bench test;
- the display link-status reporting, by stage 8;
- the throttle hold, by stage 7.

## 17. Work outside the master

From Inventory §11:

| # | Where | Change |
|---|---|---|
| 1 | Annunciator | Tile THRTL ENA → A/P ARM; touch DOCK to toggle (a real dock / undock event overrides it); demo follows the flag only; report its Simpit link state |
| 2 | Info Displays | Demo follows the flag only; report link state; Info Display 1: input test screen, two-finger gesture, its frame |
| 3 | Resource Display | Demo follows the flag only; report link state |
| 4 | Sys Info Display | New firmware: boot, status and SWITCH ERR screens; microSD icons; its protocol |
| 5 | Throttle module | Hold the lever against the pilot with a stall limit; return it on release |
| 6 | Pre-Warp module | BTN03 preset 1440 → 360 min (1 Kerbin day) |
| 7 | Function Control module | Colour table to the confirmed layout; B4 FLAPS |
| 8 | AUX CTRL module | B8 UNDOCK; B10 PRI, B11 ALT |
| 9 | Stability Control module | B10 HOLD |
| 10 | Panel labels | Function B4 RW DISABLE → FLAPS; AUX B8 UNDOCK |
| 11 | I2C Protocol Specification | Sys Info protocol; input test frame; display link-status bit; EVA lifecycle |
| 12 | Autopilot design docs | Authority rule (§11.3) |
| 13 | Module UI Reference, Hardware Reference | Rewritten from the inventory |
| 14 | Annunciator reference, user manual | A/P ARM, DOCK touch, switch rules |

## 18. Open items

| # | Item | Needed by |
|---|---|---|
| 1 | Sys Info Display frame layouts (§14) | Stage 1 |
| 2 | Sys Info Display icons / logos | Its firmware |
| 3 | Input test screen: layout and frame (§13.1) | Its firmware |
| 4 | WARP TO availability, checked against Simpit on the bench (§8.8) | Stage 6 |
| 5 | SAS availability bitmask, checked on the bench (§8.4) | Stage 4 |

---

## Appendix A — Pin map

The full pin map and wiring are in Inventory §1–§4. Summary for `config.h`:

| Teensy pins | Signal |
|---|---|
| 9, 10, 25–30 | INT_1–INT_8 (A2 Panel Hub slots P1–P8) |
| 31 | INT_SAFE (A2) — ABORT |
| 4, 5, 6, 7 | INT_9–INT_12 (A1: Info Display 1 slots P2–P4; Info Display 1 itself) |
| 8 | INT_SAFE2 (A1) — unused |
| 23, 22, 21, 20, 15, 14, 41, 40 | B_INT_1–B_INT_8 (B2 Panel Hub slots P1–P8) |
| 39, 38, 37, 36 | B_INT_9–B_INT_12 (B1: Resource Display slots P2–P4; Resource Display itself) |
| 35 | B_INT_SAFE (B2) — STAGE |
| 34 | B_INT_SAFE2 (B1) — unused |
| 2 | RST out (7" displays) |
| 18 / 19 | `Wire` SDA / SCL (BUS) |
| 17 / 16 | `Wire1` SDA / SCL (EXT) — reserved |
| 33 / 32 | EXT_INT_1 in / EXT_INT_2 out — reserved |
| 0, 1, 3, 11, 12, 13, 24 | Not connected |

**Board devices:**
- INA228 at 0x40;
- EMC2101 at 0x4C;
- LTC4311 on both buses.

## Appendix B — Glossary

| Term | Meaning |
|---|---|
| **Master** | The Teensy 4.1 on the Ctrl Module; this firmware |
| **Module** | An ATtiny816 panel board on the I2C bus |
| **Carrier / display** | A display board: the 7" drivers (Annunciator, Info Displays 1 / 2, Resource Display) and the 2" Sys Info Display |
| **Hub** | The Panel Hub on an input panel, or the 7" display driver that hosts its panel's slots |
| **INT line** | A module's own wire to the master, low when it has news |
| **Safe line** | A switch wire straight to the master from a hub's safe-switch terminal (ABORT, STAGE) |
| **CAG / AG** | Custom action group (AGX), by number |
| **Control group** | Which of six sets of custom action groups the buttons fire (ENC1) |
| **Control mode** | SPC / PLN / RVR, from the mode selector |
| **HOLD** | Stability B10: a temporary hold of the current condition |
| **SWITCH ERR** | A switch that disagrees with the vessel |
| **Demo** | SCE AUX: everything responds locally, nothing reaches KSP |
| **Inventory** | `Hardware_Inventory.md` |
