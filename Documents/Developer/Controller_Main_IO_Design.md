# Controller_Main — I/O Layer Overhaul (Design)

**Document type:** Developer / Design for approval
**Location:** `Documents/Developer/Controller_Main_IO_Design.md`
**Status:** Draft 1, for review. Nothing in this document is implemented yet.
**Scope:** the master's I/O layer: `Controller_Main.ino`, the pre-v5 legacy behind it (`function_definitions.ino`, `module_variables.h`), every input module, and the master's own hardware. The autopilots and the newer link tabs keep their code and interfaces; they gain inputs, not rewrites.

---

## 1. Where the master is today

| Area | State |
|---|---|
| Simpit link | Working (#86): non-blocking start, `update()` every loop, handler feeds the autopilots |
| Display links | Working: Info Displays (`infodisp_link`), Annunciator and Resource Display (`annun_link`) |
| Throttle module (0x2C) | Working (`throttle_link`) |
| Rotation joystick (0x28) | Forwarded to KSP as spacecraft axes only (`rotation_link`): no airplane / rover mapping, no EVA, no buttons |
| Translation joystick (0x29) | Read only for the pilot-override test; nothing reaches KSP |
| GPWS panel (0x2A) | Read and relayed to the Annunciator (`annun_link`) |
| UI, Function, Action, Stability, Vehicle, Time, AUX, Pre-Warp, Dual Encoder | **Not read** (#89). Three pre-v5 handlers exist in `function_definitions.ino` and are never called; their bit layouts predate the v5.x panels |
| Panel LEDs | Never written. The master keeps KSP's action / CAG state but sends no `CMD_SET_LED_STATE` |
| Module lifecycle | Not managed: no BOOT_READY handling, no ENABLE on flight / DISABLE on exit (except the throttle and GPWS panel) |
| Staging / abort | Two direct-wired pins with `Bounce` (pins 30 / 37 from the rev-1 map). In v5 staging is Stability Control B14 / B15 |
| Fan (EMC2101) | `setFanControl()` exists, never called, `fan.begin()` never called |
| Power monitor | Code drives an **INA219 at 0x44**; the Ctrl Module carries an **INA228 at 0x40** (Hardware Reference §6.4) |
| Watchdog | `WDT_T4` declared, never started |
| Sys Info Display (0x14) | Nothing; its protocol is not defined yet (I2C spec §15.1 "future work") |
| Pin map | `Controller_Main.ino` carries the **rev-1** pin list (`PanelCtrl_INT`, `EVA_INT`, `Switch_INT`, analog joystick pins …). No document gives the rev-2 Ctrl Module's GPIO assignment for the INT lines (see Q1) |

## 2. Goals

1. **Every input module integrated** to Module UI Reference v5.9: each button, switch, encoder and axis does what the reference says, with LED feedback from KSP state.
2. **One architecture** for all modules: a table-driven module bus driver, one small tab per module, the same pattern as the link tabs that already work.
3. **Hardware services:** fan, power monitor, watchdog, status outputs.
4. **Checkable on the host:** every stage passes `tools/host_compile.py` (stand-in and real Simpit) and an extended `tools/master_selftest.py` that simulates modules on the bus.
5. **No behaviour change for what already works** (autopilots, display links, throttle) unless a stage says so.

## 3. File layout

```
Controller_Main/
  Controller_Main.ino      setup() and loop() only: includes, init order, the scheduler
  master_config.h          pins, I2C addresses, timing constants, feature switches (one place)
  master_state.h/.ino      the panel state everyone reads: ctrlMode, ctrlGrp, switch
                           positions, KSP-reported state for LEDs, lifecycle flags
  module_bus.h/.ino        generic module driver (§4)
  cag_map.ino              base CAG -> AGX for the active control group (replaces setActionGroups)
  keys.ino                 KEYBOARD_EMULATOR helpers: tap, hold, release, modifiers, macros
  panel_ui.ino             0x20 UI Control
  panel_function.ino       0x21 Function Control + Switch Group 1
  panel_action.ino         0x22 Action Control
  panel_stability.ino      0x23 Stability Control (SAS, RCS, invert, staging)
  panel_vehicle.ino        0x24 Vehicle Control + Switch Group 2 + chute / heat-shield state machines
  panel_time.ino           0x25 Time Control (+ reads 0x2B Pre-Warp)
  panel_aux.ino            0x26 AUX CTRL (EVA, cruise, control point)
  panel_encoders.ino       0x2D Dual Encoder (control group, cameras)
  panel_prewarp.ino        0x2B Pre-Warp Time
  rotation_link.ino        0x28 — extended: control-mode axis mapping, EVA keys, buttons
  translation_link.ino     0x29 — new: TRANSLATION / camera / EVA jetpack (override test moves here)
  throttle_link.ino        0x2C — unchanged
  annun_link.ino           0x10 / 0x11 — unchanged except its GPWS read moves onto module_bus
  infodisp_link.ino        0x12 / 0x13 — unchanged
  sysinfo_link.ino         0x14 — when its protocol exists (Q6)
  hw_services.ino          fan, power monitor, watchdog, status / ALIVE outputs
  simpit_message_handler.ino   unchanged role; also feeds master_state for LED feedback
  (autopilot tabs unchanged)
```

**Removed:** `function_definitions.ino` and `module_variables.h` (their useful parts — CAG offsets, reboot / USB helpers, fan curve — move into the files above), the rev-1 pin list, the `Bounce` staging / abort objects if staging moves to Stability Control (Q2).

## 4. Module bus driver (`module_bus`)

One table describes every ATtiny816 module:

```c
struct ModuleDef {
  uint8_t  addr;        // 0x20..0x2D
  uint8_t  typeId;      // KMC_TYPE_* expected in the header
  uint8_t  intPin;      // dedicated INT GPIO (master_config.h), or NO_PIN to poll
  uint8_t  flags;       // enabled-in-flight, enabled-on-EVA, has-LEDs, ...
  void   (*onPacket)(const uint8_t *pkt, uint8_t n);   // the panel tab's handler
  void   (*onLifecycle)(uint8_t state);                // optional
};
```

**Start-up** (I2C spec §12.1), for each module: `CMD_RESET`, `CMD_GET_IDENTITY` (type and capability flags checked against the table; a mismatch or no answer marks the module absent and is reported once on Serial), all LEDs OFF, `CMD_DISABLE`. Absent modules are skipped thereafter and re-probed every few seconds, so a panel plugged in late comes up.

**Reads:** INT-driven. Each pass the driver reads every module whose INT line is low (a read without INT returns a zeroed header — I2C spec §9.3 note — so there is no blind polling of ATtiny modules). Checks per packet: type id, lifecycle bits (BOOT_READY → `CMD_DISABLE` then re-apply the wanted state), fault bit (logged, `CMD_ACK_FAULT`), transaction counter (a gap is logged as a missed packet). Then the panel tab's `onPacket`.

**Button events:** the events plane is the *current* state and the change plane accumulates every transition since the last read (KerbalButtonCore `buildPayload`). The driver turns that into `pressed` / `released` / `tapped` masks for the tabs: a bit that changed and is now 1 is a press; changed and now 0 is a release — **or a tap, if the master never saw it down** (press and release both landed between two reads). Tabs act on press-or-tap, so a quick tap is never lost.

**LEDs:** each tab writes the desired state per button into a 16-nibble buffer; the driver sends `CMD_SET_LED_STATE` only when the buffer changed, at most every 50 ms per module. Bulb test (Switch Group 1 LTG) overrides all buffers while held.

**Lifecycle policy:**

| Event | Modules |
|---|---|
| Boot / no Simpit | DISABLED (dark) |
| Flight scene entered | ENABLE all present modules |
| Flight scene left | DISABLE all |
| Simpit lost | DISABLE all |
| EVA | Stay ACTIVE; tabs gate their actions (AUX CTRL's EVA functions need the module live). The spec's "DISABLE on EVA" is not followed — see Q4 |
| Game paused | No Simpit signal for pause; SLEEP not used (Q5) |

## 5. Scheduler (`loop()`)

Fixed order, every pass, nothing blocking:

1. `simpitService()` — inbound messages (also reconnect attempts)
2. `moduleBusService()` — INT-driven module reads → panel tabs → KSP commands
3. Joystick / throttle links (`rotService`, `trnService`, `thrService`)
4. Autopilots (`apUpdate`, `hpUpdate`, `bpUpdate`, `lpUpdate`)
5. LED flush (`moduleBusFlushLeds`)
6. Display links (`idlService`, `anlService`, `sysService`)
7. Slow services on their own timers (`hwService`: fan 1 Hz, power 4 Hz, watchdog feed every pass)

A pass budget is measured in the selftest (target: well under 5 ms with all modules idle).

## 6. Module behaviour

Everything below is Module UI Reference v5.9 unless marked **(design)** — a decision this document makes because the reference is silent or ambiguous, open to change.

### 6.1 Shared rules

- **CAG numbers:** `AGX = base + (ctrlGrp − 1) × 40`, from `custom_action_grp_def.h`. The active group comes from Dual Encoder ENC1.
- **CAG buttons** send `toggleCAG(AGX)`; their LED is ACTIVE while KSP reports that AGX active (CAGSTATUS), ENABLED otherwise.
- **Keyboard actions** use `KEYBOARD_EMULATOR` with the key codes in `keyboard_def.h` (missing codes added there).
- **Button LEDs at rest:** ENABLED (backlit) when the function is available in the current context, OFF when not (e.g. EVA-only buttons outside EVA) **(design)**.

### 6.2 Per module

| Module | Inputs → action | LED feedback |
|---|---|---|
| **UI Control 0x20** | B0 screenshot macro (F2, F1, F2); B1 Alt+F12; B2 F2; B3 Numpad `.`; B4 `'`; B5 `NAVBALLMODE_MESSAGE` cycle; B6 Tab; B7 `]`; B8 Shift+Tab; B9 `[`; B10 M; B11 C | Momentary flash of ACTIVE on press **(design)**; ENABLED otherwise |
| **Function Control 0x21** | B0–B11 CAG toggles (bases 25–34, 38, 39). **Switch Group 1:** B16 MSTR RESET → `CMD_RESET` all modules + display `DISPLAY_RESET`; B17 DISPL CLR → `CMD_SET_VALUE 0` to 0x2A / 0x2B; B18 ENGINE SAFE/ARM; B19 THROTTLE INOP/ACT; B20 SCE AUX → Simpit re-init and channel refresh; B21 UPTLM XMIT → `debug`; B22 LTG TEST → bulb test while held; B23 THRTL STD/FINE (see Q8 for B18 / B19 / B23) | CAG state |
| **Action Control 0x22** | B0–B11 AG1–AG12 (`toggleCAG`, grouped) | CAG state |
| **Stability Control 0x23** | B0–B9 SAS modes (`setSASMode`); B10 RCS toggle; B11 Invert = F held while held; B14 Stage (rising edge, only while B15 Stage Enable is on); B15 Stage Enable | SAS mode ACTIVE on the current KSP mode (SAS_MODE_INFO), RCS from ACTIONSTATUS, Stage Enable LED |
| **Vehicle Control 0x24** | B0 brakes, B1 lights, B3 gear toggles; B2 / B4–B7 / B9 CAG toggles; B8 heat shield and B10 / B11 chutes as state machines (chutes only while CHUTE is ARM; SAFE resets them). **Switch Group 2:** CHUTE interlock; GEAR / BRAKE / EXT LT / SAS / RCS → activate / deactivate on each edge; THC/RHC → joystick precision; AUDIO → Annunciator audio | ACTIONSTATUS for lights / gear / brakes; CAG state; state machines: ACTIVE (deployed), CUT (cut / released) |
| **Time Control 0x25** | B0 / B2 / B4 / B6 / B8 `TIMEWARP_TO` Ap / Pe / maneuver / SOI / morning with a lead of minus the Pre-Warp minutes; B1 warp stop; B5 / B7 warp up / down (physics warp while B3 held); B9 F9; B10 Esc; B11 F5 | ENABLED; warp-to buttons ACTIVE briefly on press **(design)** |
| **AUX CTRL 0x26** | B0–B7, B9 EVA keys (only while EVA); B8 cruise (Q9); B10 / B11 control point (CAG 35 / 36 / 37, mutually exclusive, deactivate the old first) | EVA buttons ENABLED on EVA, OFF otherwise; CP: ACTIVE (primary), ACTIVE_ALT (alternate), ACTIVE on B11 (docking port) |
| **Pre-Warp 0x2B** | Display value = lead time in minutes, kept for Time Control; DISPL CLR sets it to 0 | Module-managed |
| **Dual Encoder 0x2D** | ENC1 → control group 1–6 (wraps, no Simpit message); ENC1 button (Q10); ENC2 → `-` / `=`; ENC2 button → Backspace | — (no LEDs). The control group is shown on the Annunciator's CTRLGRP |
| **Rotation 0x28** | Axes per control mode: SPC yaw/pitch/roll, PLN roll/pitch/yaw, RVR `WHEEL` steer/throttle; EVA: W/A/S/D/Q/E held while deflected, BTN_JOY = Left Shift. BTN01 Alt+X (reset trim), BTN02 set trim, BTN_JOY airbrake CAG 38 | BTN01 / BTN02 momentary |
| **Translation 0x29** | `TRANSLATION` X/Y/Z; BTN_JOY held → camera mode (`CAMERA_ROTATION`); EVA: W/A/S/D + Left Shift / Left Ctrl held; BTN01 camera mode cycle (V); BTN02 camera reset | BTN01 / BTN02 momentary |

**Control mode** (SPC / PLN / RVR) comes from the Ctrl Mode Toggle (Q3). It selects the rotation mapping and goes to the Annunciator (`annun_link` byte 1) unchanged.

**Precision:** THC/RHC PREC (SG2 B22) scales both sticks (`precisionEn`, already honoured by `rotation_link`); the throttle module's own precision follows the switch Q8 settles.

### 6.3 What reaches the Annunciator

With this design every field of its command has a source: control mode (toggle), control group (ENC1), audio (SG2 AUDIO), THRTL ENA / ENG ARM / THRTL PREC (Q8), TRIM (rotation BTN02 trim set **(design)**), INPUT PREC (SG2 B22), DEMO / DEBUG (SG1 UPTLM for debug), WARP (Simpit), AUTOPILOT (autopilots). SWITCH ERR could flag a switch position the master rejects (e.g. CHUTE moved to ARM with a chute already cut) **(design)**; SIMPIT LOST = connected but no inbound message for 5 s in a flight scene **(design)**.

## 7. Hardware services (`hw_services`)

| Service | Plan |
|---|---|
| Fan (EMC2101, 0x4C) | `begin()` at start-up; the existing temperature → duty curve (30 / 40 / 50 / 70 / 100 % at 25 / 30 / 40 / 50 °C), evaluated at 1 Hz with 2 °C hysteresis **(design)**; RPM and temperature kept for the Sys Info Display |
| Power (INA228, 0x40) | Replace the INA219 code; bus voltage, current, power at 4 Hz; kept for the Sys Info Display; Serial warning if the 12 V rail sags below 11 V **(design)** |
| Watchdog (WDT_T4) | Started at the end of `setup()`, fed every loop pass, 1 s timeout **(design)**. The Simpit handshake attempt (≈1 s) is the longest blocking call; the timeout must clear it (or the attempt moves before the watchdog starts) |
| Status outputs | Whatever the rev-2 Ctrl Module actually carries (Q1); the rev-1 RESET / IDLE / DEMO / DEBUG state pins are not carried over unless they still exist |
| Sys Info Display (0x14) | `sysinfo_link` pushes power / fan / temperature / link status once its protocol is defined (Q6) |

## 8. Verification

- **Host build** both ways at every stage (`host_compile.py`, stand-in and `--simpit`).
- **Selftest** grows a module simulator: each module address answers identity and serves scripted packets with INT; the tests drive presses, taps, switches and encoder turns and assert the Simpit calls, keyboard events, CAG numbers and LED frames. Shared stand-ins gain recording for `activateAction` / `toggleCAG` / `setSASMode` / `send` so the assertions see what reached "KSP".
- **Display panels:** unaffected; their host build, lint and renders are re-run whenever a shared stand-in changes.
- **On hardware:** the test plan gains a CONSOLE session per panel once the pin map (Q1) is in.

## 9. Stages (one PR each)

1. **Foundation.** `master_config.h` / `master_state`, `module_bus` (start-up, INT reads, events, LEDs, lifecycle), the new `loop()`; legacy files removed; selftest module simulator. No panel does anything yet except come up and go dark / backlit with the flight scene.
2. **Flight controls.** Vehicle Control (+ SG2, state machines), Stability Control (+ staging), Action Control, CAG map, Dual Encoder control group. Annunciator audio / control group / precision fields live.
3. **Everything else on the panels.** UI Control, Function Control (+ SG1), Time Control + Pre-Warp, AUX CTRL, Dual Encoder cameras.
4. **Sticks.** Rotation control-mode mapping, EVA keys and buttons; translation forwarding, camera mode, EVA jetpack.
5. **Hardware services.** Fan, INA228, watchdog, status outputs; Sys Info Display link if its protocol is ready.

Each stage updates the Module UI Reference / test plan where behaviour is decided, and closes the corresponding part of #89.

## 10. Open questions

| # | Question | Needed by | Proposed default |
|---|---|---|---|
| Q1 | **Rev-2 Ctrl Module pin map.** Which master GPIO is each module's INT line (Module Hub INT_1–8 per hub, B_INT_1–12, INT_SAFE / B_INT_SAFE / B_INT_SAFE2), and which status / ALIVE outputs exist? No document carries it; `Controller_Main.ino` has the rev-1 list | Stage 1 | One table in `master_config.h`; until filled, modules are polled on a 20 ms timer instead of INT (works, costs bus time) |
| Q2 | **Staging and abort.** The reference puts Stage / Stage Enable on Stability Control B14 / B15 and has no abort input. Are the two direct-wired buttons (pins 30 / 37) gone? Where is ABORT? Are the safe-switch INT lines (INT_SAFE) the stage / abort covers? | Stage 2 | Staging from Stability B14 / B15; abort kept on a direct pin until told otherwise |
| Q3 | **Ctrl Mode Toggle wiring** (Module UI Reference open item 14). Two master GPIOs for the 3-position switch? | Stage 2 | Two inputs with pull-ups: SPC = both high, PLN = A low, RVR = B low |
| Q4 | **EVA lifecycle.** I2C spec §11 says DISABLE modules on EVA, but AUX CTRL's EVA buttons need the module live | Stage 1 | Keep all ACTIVE on EVA; tabs gate their actions |
| Q5 | **Game pause / SLEEP.** Simpit sends no pause state | Stage 1 | SLEEP unused |
| Q6 | **Sys Info Display protocol** (0x14). Define it now (fields, cadence) or later? | Stage 5 | Later; stage 5 does fan / power / watchdog without it |
| Q7 | **Power monitor:** INA228 at 0x40 per the Hardware Reference (the code has an INA219 at 0x44) | Stage 5 | INA228 |
| Q8 | **Switch Group 1 throttle switches.** B18 ENGINE SAFE/ARM ("throttle input disabled"), B19 THROTTLE INOP/ACT (reference says it toggles throttle-module precision), B23 THRTL STD/FINE (reference says joystick precision). B19 and B23 read as swapped, and SG2 B22 THC/RHC is already joystick precision | Stage 3 | B18 ENGINE ARM → engine arm (throttle module enabled / ENG ARM tile); B19 THROTTLE ACT → throttle input enabled (THRTL ENA tile); B23 THRTL FINE → throttle-module precision (THRTL PREC tile); B22 THC/RHC PREC → joystick precision (INPUT PREC tile) |
| Q9 | **AUX B8 Cruise Control** ("controller-side hold"). The Rover Autopilot already has CRUISE | Stage 3 | B8 engages the Rover Autopilot's CRUISE at the current speed (same as the console), and disengages it |
| Q10 | **Dual Encoder ENC1 button** ("docked flag toggle for TFT display sync") — the master no longer has a TFT | Stage 2 | Reset the control group to 1 |
| Q11 | **UI Control LEDs.** All twelve are momentary keyboard actions with no KSP state to reflect | Stage 3 | ENABLED at rest, ACTIVE for 150 ms on press |
| Q12 | **SCE AUX** ("Simpit reset and data fetch") and **MSTR RESET** scope: should MSTR RESET also soft-reset the display carriers (`DISPLAY_RESET`) and the master's own autopilots? | Stage 3 | MSTR RESET: `CMD_RESET` all modules + `DISPLAY_RESET` to the four carriers + A/P OFF. SCE AUX: Simpit re-handshake and a full channel refresh |
