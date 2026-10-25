# KCMk1 Master Firmware — Design

**Document type:** Developer / Design for approval
**Location:** `Documents/Developer/Master_Firmware_Design.md`
**Status:** Draft 3, for review. Nothing in this document is implemented yet.
**Supersedes:** `Controller_Main_IO_Design.md` (drafts 1–2, an overhaul of the existing sketch). Draft 3 replaces that plan with a new master written from the ground up.

---

## 1. Decisions

| Decision | Choice |
|---|---|
| Approach | A new master firmware, designed and written from the ground up. `Software/Controller_Main` is not edited further except for a fix that blocks the bench |
| Location | New sketch `Software/KCMk1_Master/`. `Controller_Main` stays as a reference until the new master does everything it did, then one cutover PR deletes it (§11) |
| Autopilots | **Ported, not rewritten.** Ascent, hold, burn, landing, the attitude controller, the arbiter and the staging helpers keep their control laws, their console protocol and their design docs. They already talk to the rest of the master through narrow interfaces (`*Ingest*()` in, `thrAutoThrottle()` / `rotSetAutoAxes()` / Simpit sends out); the port re-points those interfaces at the new layers and changes nothing else (§8) |
| Hardware truth | The schematics, the I2C Protocol Specification (v2.14) and the Module UI Reference (v5.9), which match the module and display firmware. The Hardware Reference and the old sketch are **not** sources: where they disagree with the schematics they are out of date. The Hardware Reference is rewritten from the same sources at cutover (§11) |
| Process | Plan first: this document is approved before stage 1 starts. Each stage is one PR, checked on the host before it merges |

## 2. Sources, in order of authority

1. **Schematics.** Ctrl Module KC-01-1701 V2.0 (in hand; Appendix A). Still wanted: Ctrl Ext Module (KC-01-1711), Panel Hub (KC-01-1731), Display Hub (KC-01-1721), and the module boards where a safe line or the Ctrl Mode Toggle is wired (Q2, Q3).
2. **I2C Protocol Specification v2.14:** module packets, lifecycle, commands, the display-carrier protocol (§15).
3. **Module UI Reference v5.9:** what every button, switch, encoder and axis does.
4. **Display firmware and READMEs:** what the Annunciator, Info Displays and Resource Display expect from the master (their commands are fixed; the new master sends exactly what the current one does).
5. **Autopilot design docs:** `Hold_Mode_Autopilot.md`, `Mission_Autopilot.md`, `Ascent_Autopilot.md`, `Ascent_Autopilot_Interface.md`.
6. **KerbalSimpit 2.4.0** (the library, not its docs): message ids and payloads.

## 3. What the master does

- Runs the **Simpit link** to KSP on its own USB serial (`SerialUSB1`; Teensy USB type *Dual Serial*), keeps the vessel state the rest of the firmware reads, and is the **only** sender of control messages to KSP.
- Runs the **module bus**: brings up every ATtiny816 module, reads it when it asserts INT, turns its packets into input events, drives its LEDs, manages its lifecycle.
- Turns **panel inputs into actions**: Simpit commands, CAG toggles, keyboard emulation, and changes to the master's own modes (control mode, control group, precision, audio, …).
- Merges **pilot and autopilot outputs**: one rotation, one translation, one wheel and one throttle message per frame, with the autopilot owning only the axes it holds and the pilot override rule on top.
- Runs the **autopilots** and their console link to Info Display 2.
- Feeds the **display carriers** (Annunciator, Resource Display, Info Displays 1 / 2, later the Sys Info Display) their command bytes and releases them from boot.
- Runs the **board**: fan (EMC2101), power monitor (INA228), watchdog, the global display reset line.

Not in scope: the expansion unit (EXT bus target, EXT_INT_1 / 2). The pins are reserved and left idle.

## 4. Architecture

### 4.1 Layers

```
   inputs                 state                    decisions                  outputs
 ┌──────────────┐     ┌──────────────┐       ┌────────────────────┐     ┌──────────────────┐
 │ simpit_link  │────▶│ vessel       │──────▶│ panel_* handlers   │────▶│ ksp_out          │──▶ Simpit
 │ module_bus   │────▶│ panel        │──────▶│ autopilots + arb.  │────▶│  (merge, owners) │
 │ board (INA,  │────▶│ master       │──────▶│ annunciation rules │────▶│ module_bus LEDs  │──▶ modules
 │  EMC, pins)  │     └──────────────┘       └────────────────────┘     │ carriers         │──▶ displays
 └──────────────┘                                                        └──────────────────┘
```

- **Inputs** only write state. **Decisions** only read state and call outputs. **Outputs** are the only code that writes to Simpit, the module bus or the carriers.
- State is plain structs with one writer each: `vessel` (written by `simpit_link`), `panel` (written by the `panel_*` handlers: switch positions, control mode, control group, precision, audio), `master` (lifecycle, link health, fault counters).
- No tab reaches into another tab's statics; everything crosses through a header. The Arduino prototype hoisting is not relied on: every tab has a header, so the host build and the IDE see the same contract.

### 4.2 Loop

Fixed order every pass, nothing blocking, pass time measured:

1. `simpitService()`: inbound messages into `vessel`; reconnect attempts while down.
2. `boardServiceFast()`: watchdog feed, safe-line debouncing.
3. `moduleBusService()`: read every module whose INT is low; the panel handlers run from here and act at once.
4. `autopilotService()`: the four autopilots and the arbiter.
5. `kspOutService()`: one merged rotation / translation / wheel / throttle message per frame (20 ms, or at once on a change of owner), plus any queued one-shot commands.
6. `moduleBusFlushLeds()`: LED frames that changed, at most every 50 ms per module.
7. `carrierService()`: display carrier commands every 100 ms and on change; Info Display 2 console poll.
8. `boardServiceSlow()`: fan 1 Hz, power 4 Hz, serial diagnostics.

Target: under 2 ms per pass with every module idle, under 5 ms in the worst pass (all modules asserting). The selftest measures it on the host as a relative check; the bench test measures it on the Teensy.

### 4.3 File layout

```
Software/KCMk1_Master/
  KCMk1_Master.ino        setup() and loop() only
  config.h                rev-2 pin map, I2C addresses, timing, feature switches (one place)
  state.h / state.ino     vessel, panel and master state structs
  simpit_link.h / .ino    Simpit start, registration, message dispatch into vessel + autopilot ingest
  module_bus.h / .ino     module table, INT learning, start-up, reads, events, LEDs, lifecycle (§5)
  ksp_out.h / .ino        the only Simpit sender: axis merge, throttle owner, one-shot queue, keys
  cag.h / .ino            AGX numbering for the active control group
  keys.h / .ino           keyboard emulation: tap, hold, release, modifiers, macros
  panel_ui.ino            0x20 UI Control
  panel_function.ino      0x21 Function Control + Switch Group 1
  panel_action.ino        0x22 Action Control
  panel_stability.ino     0x23 Stability Control
  panel_vehicle.ino       0x24 Vehicle Control + Switch Group 2 + chute / heat-shield state machines
  panel_time.ino          0x25 Time Control + 0x2B Pre-Warp
  panel_aux.ino           0x26 AUX CTRL
  panel_sticks.ino        0x28 Rotation, 0x29 Translation (axes, buttons, EVA, camera)
  panel_throttle.ino      0x2C Throttle Module (lever follow, touch override)
  panel_gpws.ino          0x2A GPWS Input Panel
  panel_encoders.ino      0x2D Dual Encoder
  panel_safe.ino          the safe lines (abort / stage, Q2 / Q14)
  carriers.h / .ino       Annunciator, Resource Display, Info Displays 1 / 2, Sys Info Display
  board.h / .ino          fan, power, watchdog, RST, safe-line pins
  autopilot/              the ported autopilot tabs, arbiter, staging helpers, attitude controller (§8)
  tools/                  host_compile.py, selftest.py, the module simulator, stand-ins
```

The Arduino IDE compiles only the top folder's tabs, so the autopilot files sit in the top folder in practice with an `ap_` prefix; `autopilot/` above is the grouping, not a path.

## 5. Module bus

### 5.1 Module table

```c
struct ModuleDef {
  uint8_t addr;          // 0x20..0x2D
  uint8_t typeId;        // KMC_TYPE_* expected in the header
  uint8_t defaultInt;    // INT pin from config.h, or NO_PIN
  uint8_t flags;         // has LEDs, active on EVA, extended LED states, ...
  void  (*onPacket)(const uint8_t *pkt, uint8_t n);   // the panel handler
};
```

### 5.2 INT line learning

Which hub slot a module plugs into decides its INT line. That is cabling, so the master learns it rather than depending on a slot table. At power-on every module holds INT low in BOOT_READY until read (I2C spec §3). Reading one module at a time and watching which of the 26 INT inputs (Appendix A) goes high gives that module's line. Display carriers are learned the same way: a STATUS request (§15.3) asserts their INT.

The learned map is kept in EEPROM, so a master that restarts on its own (modules already initialised, INT high) still has it. The `config.h` default covers a first boot. A module with no known line is polled every 20 ms. Differences between learned, stored and default are reported once on Serial. Pins are read with `digitalReadFast` each pass; no pin interrupts are used.

### 5.3 Start-up

Per module (I2C spec §12.1):
1. `CMD_GET_IDENTITY`: type and capability are checked against the table.
2. All LEDs OFF.
3. `CMD_DISABLE`.

A module that does not answer, or answers with the wrong type, is marked absent and reported once. Absent modules are re-probed every 5 s, so a panel plugged in late comes up.

### 5.4 Reads and events

- **Per-packet checks:** type id; lifecycle (BOOT_READY → `CMD_DISABLE`, then the wanted state again); fault bit (logged, `CMD_ACK_FAULT`); transaction counter (a gap is logged).
- **Button events.** The events plane is the current state, and the change plane accumulates every transition since the last read. The bus turns them into `pressed` / `released` / `tapped` masks:
  - changed and now 1 is a press;
  - changed and now 0 is a release, or a tap if the master never saw the button down (press and release both landed between two reads).

  Handlers act on press-or-tap, so a quick tap is never lost.
- **LEDs:** each handler writes the wanted state per button into a 16-nibble buffer. The bus sends `CMD_SET_LED_STATE` only when the buffer changed, at most every 50 ms per module. The bulb test (SG1 LTG TEST) overrides every buffer while held.

### 5.5 Lifecycle

| Event | Modules |
|---|---|
| Boot, no Simpit | DISABLED (dark) |
| Flight scene entered | ENABLE all present |
| Flight scene left / Simpit lost | DISABLE all |
| EVA | Stay ACTIVE; handlers gate their own actions, because AUX CTRL's EVA buttons need the module live (Q4) |
| Game paused | No Simpit pause signal; SLEEP unused (Q5) |

## 6. Outputs to KSP (`ksp_out`)

The Simpit plugin keeps only the latest message per control channel, so two senders on one channel clobber each other. `ksp_out` is therefore the only code that sends them.

| Channel | Rule |
|---|---|
| Rotation | Per axis: the autopilot's value where it holds that axis (`rotSetAutoAxes` mask), else the pilot's stick. One message per 20 ms frame, and at once on a change |
| Translation | Pilot only (no autopilot uses it today); camera mode diverts the stick to `CAMERA_ROTATION` |
| Wheel | Pilot stick in RVR mode, or the hold autopilot's rover channel while engaged |
| Throttle | One owner at a time (pilot, or the autopilot holding the throttle per the arbiter). The lever follows the owner; a pilot touch latches an override (today's `throttle_link` rule, unchanged) |
| One-shots | Action groups, CAGs, SAS mode, staging, warp, camera, keyboard: queued and sent in order, rate-limited so a macro cannot flood the serial link |

**Pilot override:** any pilot input on either stick (beyond threshold for a debounce time) or a touch on the lever disconnects every autopilot. This is today's rule and its constants are unchanged.

## 7. Panel behaviour

This is Module UI Reference v5.9, unless marked **(design)**: a decision this document makes because the reference is silent or ambiguous.

### 7.1 Shared rules

- **CAG numbers:** `AGX = base + (ctrlGrp − 1) × 40`. The active group comes from Dual Encoder ENC1.
- **CAG buttons** send `toggleCAG(AGX)`. Their LED is ACTIVE while KSP reports that AGX active, ENABLED otherwise.
- **Keyboard actions** use `KEYBOARD_EMULATOR` with the codes in `keyboard_def.h`; missing codes are added there.
- **LEDs at rest:** ENABLED when the function is available in the current context, OFF when not, e.g. EVA-only buttons outside EVA **(design)**.

### 7.2 Per module

| Module | Inputs → action | LED feedback |
|---|---|---|
| **UI Control 0x20** | B0 screenshot macro (F2, F1, F2); B1 Alt+F12; B2 F2; B3 Numpad `.`; B4 `'`; B5 navball mode cycle; B6 Tab; B7 `]`; B8 Shift+Tab; B9 `[`; B10 M; B11 C | ENABLED; ACTIVE for 150 ms on press **(design)** |
| **Function Control 0x21** | B0–B11 CAG toggles (bases 25–34, 38, 39). **Switch Group 1:** B16 MSTR RESET; B17 DISPL CLR (zero 0x2A / 0x2B); B18 ENGINE SAFE/ARM; B19 THROTTLE INOP/ACT; B20 SCE AUX (Simpit re-handshake + channel refresh); B21 UPTLM XMIT (debug); B22 LTG TEST (bulb test while held); B23 THRTL STD/FINE. See Q8 and Q12 | CAG state |
| **Action Control 0x22** | B0–B11 AG1–AG12 (`toggleCAG`, grouped) | CAG state |
| **Stability Control 0x23** | B0–B9 SAS modes; B10 RCS toggle; B11 Invert (F held while held); B14 Stage (rising edge, only while B15 Stage Enable is on); B15 Stage Enable. See Q2 | SAS mode from SAS_MODE_INFO, RCS from ACTIONSTATUS, Stage Enable |
| **Vehicle Control 0x24** | B0 brakes, B1 lights, B3 gear; B2 / B4–B7 / B9 CAG toggles; B8 heat shield and B10 / B11 chutes as state machines (chutes only while CHUTE is ARM; SAFE resets them). **Switch Group 2:** CHUTE interlock; GEAR / BRAKE / EXT LT / SAS / RCS act on each edge; THC/RHC PREC → stick precision; AUDIO → Annunciator audio | ACTIONSTATUS; CAG state; state machines: ACTIVE (deployed), CUT (cut / released) |
| **Time Control 0x25** | B0 / B2 / B4 / B6 / B8 warp to Ap / Pe / maneuver / SOI / morning, led by the Pre-Warp minutes; B1 warp stop; B5 / B7 warp up / down (physics warp while B3 held); B9 F9; B10 Esc; B11 F5 | ENABLED; warp-to ACTIVE briefly on press **(design)** |
| **AUX CTRL 0x26** | B0–B7, B9 EVA keys (EVA only); B8 cruise (Q9); B10 / B11 control point (CAG 35 / 36 / 37, mutually exclusive) | EVA buttons ENABLED on EVA, OFF otherwise; control point: ACTIVE / ACTIVE_ALT |
| **Pre-Warp 0x2B** | Value = lead time in minutes, used by Time Control; DISPL CLR zeroes it | Module-managed |
| **Dual Encoder 0x2D** | ENC1 → control group 1–6 (wraps); ENC1 button (Q10); ENC2 → `-` / `=`; ENC2 button → Backspace | None. The group shows on the Annunciator's CTRLGRP |
| **Rotation 0x28** | Axes per control mode: SPC yaw / pitch / roll; PLN roll / pitch / yaw; RVR wheel steer / throttle. EVA: W/A/S/D/Q/E held while deflected. BTN01 Alt+X (reset trim); BTN02 set trim; BTN_JOY airbrake CAG 38 (Left Shift on EVA) | BTN01 / BTN02 momentary |
| **Translation 0x29** | Translation X/Y/Z; BTN_JOY held → camera mode; EVA: W/A/S/D + Left Shift / Left Ctrl held; BTN01 camera mode cycle (V); BTN02 camera reset | BTN01 / BTN02 momentary |
| **Throttle 0x2C** | Lever → KSP throttle; touch / buttons latch the pilot override; precision from SG1 (Q8) | Module-managed |
| **GPWS 0x2A** | State byte and threshold relayed to the Annunciator | Module-managed |

**Control mode** (SPC / PLN / RVR) comes from the Ctrl Mode Toggle (Q3). It selects the stick mapping and goes to the Annunciator unchanged.

### 7.3 Annunciator fields

Every field of the Annunciator's command has a source:

| Field | Source |
|---|---|
| Control mode | Ctrl Mode Toggle (Q3) |
| Control group | ENC1 |
| AUDIO | SG2 AUDIO |
| THRTL ENA, ENG ARM, THRTL PREC | Throttle switches (Q8) |
| INPUT PREC | SG2 THC/RHC PREC |
| TRIM | BTN02 trim set **(design)** |
| DEBUG | SG1 UPTLM XMIT |
| WARP | Simpit |
| AUTOPILOT | Any autopilot engaged |
| SIMPIT LOST | Connected, in a flight scene, but no inbound message for 5 s **(design)** |
| SWITCH ERR | A switch position the master rejects, e.g. CHUTE moved to ARM with a chute already cut **(design)** |

## 8. Porting the autopilots

What moves unchanged: control laws, state machines, constants, the console protocol to Info Display 2 (`infodisp_link`), the arbiter, the staging helpers, and every `*Ingest*()` / setter / status function.

What changes:

| Today | New |
|---|---|
| `simpit_message_handler.ino` calls each autopilot's ingest functions | `simpit_link` calls the same functions, from one dispatch table |
| `thrAutoThrottle()` / `thrAutoRelease()` in `throttle_link` | Same names, now in `ksp_out` (throttle owner) |
| `rotSetAutoAxes()` / `rotClearAutoAxes()` in `rotation_link` | Same names, now in `ksp_out` (rotation merge) |
| The ascent autopilot sends its own rotation message while armed, and `rotation_link` stays silent | Ascent hands its axes to `rotSetAutoAxes()` with all three held, so `ksp_out` stays the single sender |
| `pilotOverrideDetected()` reads the joystick modules directly | Reads `panel` state that `panel_sticks` / `panel_throttle` keep; same thresholds |
| Autopilots send one-shot Simpit messages (SAS, warp, stage) through `mySimpit` | Through the `ksp_out` one-shot queue |
| Globals from `module_variables.h` (`brakes_on`, …) | `vessel` fields |

Each moved tab keeps its file history (`git mv`, then edit), so the diff shows only the interface changes. The autopilot design docs need no change beyond their file references.

## 9. Board services

| Service | Plan |
|---|---|
| Fan (EMC2101, 0x4C) | The temperature comes from the remote diode on header H4, falling back to the internal sensor if the diode reads open. Duty curve 30 / 40 / 50 / 70 / 100 % at 25 / 30 / 40 / 50 °C, evaluated at 1 Hz with 2 °C hysteresis **(design)**. RPM from FAN_TACH |
| Power (INA228, 0x40) | Bus voltage, current and power at 4 Hz. A Serial warning if the 12 V rail sags below 11 V **(design)** |
| Watchdog | Started at the end of `setup()`, fed every pass, 1 s timeout. The Simpit handshake attempt (about 1 s, the longest blocking call) runs before the watchdog starts and, once it has started, only with the watchdog fed on either side **(design)** |
| Display reset (RST, pin 2) | Held high. Pulsed low for 10 ms to reboot every Teensy display carrier at once (Q12) |
| Status outputs | None: the rev-2 Ctrl Module has no master-driven LEDs or state pins |
| Sys Info Display (0x14) | Gets power, fan, temperature and link status once its protocol is defined (Q6) |

## 10. Verification

- **Host build** at every stage: `tools/host_compile.py`, against both the Simpit stand-in and the real library (`--simpit`). This is the same method as today's tool, moved to the new sketch.
- **Selftest with a module simulator.** Each module address answers identity and serves scripted packets while holding its INT pin low. Tests drive power-on learning, presses, taps, switch edges, encoder turns and stick moves, and check what reached "KSP": Simpit messages, CAG numbers, keyboard events. They also check the LED frames, lifecycle commands and carrier bytes. The autopilots' behaviour is checked through the same harness with scripted telemetry.
- **Display panels:** unaffected. Their host builds, lint and renders re-run when a shared stand-in changes.
- **On hardware:** a master section in the test plan, one CONSOLE session per panel, written alongside each stage.

## 11. Stages (one PR each)

| # | Stage | Done when |
|---|---|---|
| 1 | **Skeleton.** `config.h` (rev-2 pins), state, `simpit_link` (non-blocking start, registration, dispatch), `carriers` (the four display commands, byte for byte as today), `board` (watchdog, RST), host tools moved over | The new sketch builds and its selftest reproduces today's start-up checks: carriers released before KSP, 2 s retry, WARP flag |
| 2 | **Module bus.** Table, INT learning + EEPROM, start-up, reads, events, LED flush, lifecycle; the simulator | Every module comes up, goes dark / backlit with the flight scene, and a re-cabled module is found on its new line |
| 3 | **Flight panels.** Vehicle (+ SG2, state machines), Stability (+ staging), Action, Dual Encoder control group, CAG map, safe lines; Annunciator fields live | Every button, switch and LED on those panels behaves to §7 in the selftest |
| 4 | **Sticks and throttle.** `ksp_out` merge and owners, rotation per control mode, translation, camera, EVA keys, throttle lever | Stick and lever messages correct per mode; override latches |
| 5 | **The rest of the panels.** UI, Function (+ SG1), Time + Pre-Warp, AUX, Dual Encoder cameras, GPWS | All of §7 covered |
| 6 | **Autopilots.** Port per §8, plus the Info Display 2 console link | The autopilot checks pass through the new harness; consoles engage / disengage |
| 7 | **Board services.** Fan, INA228, Sys Info link if its protocol exists | Readings on Serial; fan follows the curve |
| 8 | **Cutover.** Delete `Software/Controller_Main`; rewrite the Hardware Reference from the schematics; update the test plan, the I2C spec's master notes and the README links; close #89 | Nothing in the repo refers to `Controller_Main` |

## 12. Open questions

Needed before the stage named. The proposed default is what gets built if there is no other answer.

| # | Question | Stage | Proposed default |
|---|---|---|---|
| Q2 | **Staging and abort.** The reference has Stage / Stage Enable on Stability B14 / B15 and no abort input; the panel list has a staging button on B2 and an Abort button on A2 | 3 | Staging from Stability B14 with B15 as the enable; ABORT from INT_SAFE |
| Q3 | **Ctrl Mode Toggle wiring.** No spare Teensy pin reaches a connector (0, 1, 3, 11–13 and 24 are unconnected), so the 3-position switch can only reach the master through a module input | 3 | Two free discrete inputs on a 24-input module. Until it is wired, control mode follows `vesselType` in FLIGHT_STATUS (Plane → PLN, Rover → RVR, else SPC) |
| Q4 | **EVA lifecycle.** I2C spec §11 says DISABLE modules on EVA, but AUX CTRL's EVA buttons need the module live | 2 | All ACTIVE on EVA; handlers gate their actions. The spec is updated to match |
| Q5 | **Game pause / SLEEP.** Simpit sends no pause state | 2 | SLEEP unused |
| Q6 | **Sys Info Display protocol (0x14).** Define now or later? | 7 | Later |
| Q8 | **SG1 throttle switches.** The reference wording has B19 and B23 swapped against their labels, and SG2 THC/RHC is already stick precision | 4 | B18 ENGINE ARM → throttle module enabled (ENG ARM); B19 THROTTLE ACT → throttle input forwarded (THRTL ENA); B23 THRTL FINE → throttle-module precision (THRTL PREC) |
| Q9 | **AUX B8 cruise.** The rover autopilot already has CRUISE | 5 | B8 engages / disengages rover CRUISE at the current speed |
| Q10 | **Dual Encoder ENC1 button** ("docked flag toggle for TFT sync"): there is no master TFT | 3 | Reset the control group to 1 |
| Q12 | **MSTR RESET / SCE AUX scope** | 5 | MSTR RESET: `CMD_RESET` all modules, `DISPLAY_RESET` to the carriers, A/P OFF; RST kept for a carrier that stops answering. SCE AUX: Simpit re-handshake and channel refresh |
| Q13 | **Hub slot per module** | 2 | INT learning (§5.2), so no slot table is needed. If you have the slot list, it becomes the `config.h` default |
| Q14 | **What the four safe lines carry** | 3 | INT_SAFE = ABORT (Abort button on A2); B_INT_SAFE = STAGE button (B2), if staging is not taken from Stability (Q2); INT_SAFE2 and B_INT_SAFE2 unused. All active-low, debounced 10 ms |
| Q15 | **Sketch name.** `KCMk1_Master` | 1 | `Software/KCMk1_Master/` |

Resolved: Q1 (rev-2 pin map, Appendix A); Q7 (INA228 at 0x40).

---

## Appendix A. Ctrl Module pin map (KC-01-1701 V2.0)

This is from the schematic, sheet 1 (Teensy 4.1 U4 on headers H2 / H3), and is the pin map `config.h` carries.

### A.1 Teensy pins

| Teensy | Net | Dir | Goes to |
|---|---|---|---|
| 2 | RST | out | Global display reset, through R13 47 Ω; every hub and the 50-pin interboard |
| 9, 10, 25–30 | INT_1 … INT_8 (9 = INT_1, 10 = INT_2, 25 = INT_3 … 30 = INT_8) | in | The 8-line hub connector, so **A2** (the left input panel, 7 modules) |
| 31 | INT_SAFE | in | A2 hub, INT_SAFE |
| 4, 5, 6, 7 | INT_9 … INT_12 | in | The 4-line hub connector, so **A1** (the left display panel) |
| 8 | INT_SAFE2 | in | A1 hub, INT_SAFE |
| 23, 22, 21, 20 | B_INT_1 … B_INT_4 | in | 50-pin interboard → Ctrl Ext Module → B1 / B2 hubs |
| 15, 14 | B_INT_5, B_INT_6 | in | ″ |
| 41, 40, 39, 38, 37, 36 | B_INT_7 … B_INT_12 | in | ″ |
| 35 | B_INT_SAFE | in | ″ |
| 34 | B_INT_SAFE2 | in | ″ |
| 33 | EXT_INT_1 | in | Expansion → master, through R15 47 Ω; R14 4.7 kΩ pull-up to 3.3 V |
| 32 | EXT_INT_2 | out | Master → expansion |
| 18 / 19 | SDA_BUS / SCL_BUS | I2C | `Wire`: every module and display carrier; LTC4311 U6; INA228 U3; EMC2101 U5 |
| 17 / 16 | SDA_EXT / SCL_EXT | I2C | `Wire1`: expansion bus; LTC4311 U7 |
| 0, 1, 3, 11, 12, 13, 24 | — | — | Not connected (24 is wired only between the two header footprints) |

Which connector is A1 and which is A2 follows from the line counts:
- **A2** (the 8-line connector) carries 7 modules: UI, Function, Action, Translation, GPWS, Throttle and the Sys Info Display.
- **A1** (the 4-line connector) has two 7" displays and the power button.
- **The B side** has 12 lines for 9 modules: Stability, Vehicle, Time, AUX, Rotation and Pre-Warp on B2; Info Display 2, Resource Display and Dual Encoder on B1.

The old sketch's rev-1 numbers land on other lines on this board. The throttle reads pin 32 (EXT_INT_2), rotation 33 (EXT_INT_1), translation 31 (INT_SAFE), GPWS 38 (B_INT_10), staging 30 (INT_8) and abort 37 (B_INT_11). So none of today's links sees its module's INT on rev-2 hardware.

### A.2 On-board devices

| Device | Bus / pins | Notes |
|---|---|---|
| INA228 (U3) | `Wire`, 0x40 | A0 / A1 to GND |
| EMC2101 (U5) | `Wire`, 0x4C | Drives FAN_PWM (4-pin fan CN1 and interboard pin 1), reads FAN_TACH; remote diode on header H4 (DP / DN) |
| LTC4311 ×2 (U6, U7) | — | Bus accelerators on BUS and EXT |
| USB hub | — | Two CH334F cascaded: one USB-C upstream; six Micro-USB and one expansion USB-C downstream. No firmware involvement |
| Status LEDs | — | Rail indicators, hardware only |

### A.3 Where the schematic and the Hardware Reference disagree

These are for the hardware owner; the firmware follows the schematic.

1. **ST7789 status display.** Hardware Reference §5.1 lists one on the Ctrl Module, but the schematic has none: the SPI pins 11–13 are unconnected.
2. **EXT_INT series resistor.**
   - The Hardware Reference (§5.1, §8.4) puts the 47 Ω series resistor on the EXT_INT_2 *output* and only a pull-up on EXT_INT_1.
   - The schematic puts R15 47 Ω on EXT_INT_1, between the pull-up and Teensy pin 33, and nothing on EXT_INT_2.

   The 47 Ω on an input is harmless, but EXT_INT_2 leaves the box unprotected. Check this before the expansion unit is built.
