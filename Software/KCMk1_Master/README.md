# KCMk1_Master

The new master firmware for the Kerbal Controller Mk1: Teensy 4.1 on the Ctrl Module (KC-01-1701 V2.0). It replaced `Software/Controller_Main`, deleted at the cutover (stage 8); its autopilots were ported here (stage 7).

| | |
|---|---|
| **Version** | 0.10.0 (`KCM_MASTER_VERSION`, `config.h`) |
| **Design** | `Documents/Developer/Master_Firmware_Design.md` |
| **Hardware** | `Documents/Developer/Hardware_Inventory.md` |
| **Protocols** | `Documents/Developer/I2C_Protocol_Specification.md` (display carriers, BACKLIGHT and input test frames: §15.3; Sys Info Display: §15.4) |
| **Status** | All eight stages built (design §16); `Controller_Main` deleted. Not yet run on hardware: the bench checks below come next |

## Building

- **Board:** Teensy 4.1, **USB Type: Dual Serial**. `Serial` is the debug console; `SerialUSB1` is the KerbalSimpit link. Point KSP's Simpit `Settings.cfg` at the second COM port.
- **Libraries:** KerbalSimpit 2.4.0; WDT_T4 and EEPROM (ship with Teensyduino); KerbalModuleCommon (`Software/Modules/libraries`, the modules' shared header: type ids, commands, LED states, packet sizes).
- **Layout:** `KCMk1_Master.ino` holds `setup()` and `loop()` only. Every other part is a `.cpp` / `.h` pair, compiled on its own, so nothing depends on the IDE's prototype generation.

## What it does so far

| Part | Files | Does |
|---|---|---|
| Boot | `boot.*` | Runs the start-up steps. Each step reports a line to the Sys Info Display (a short form that fits two rows, `bootLineS()`) and to Serial (the long form); a step that times out shows in red and start-up carries on. The Sys Info Display stays on its boot screen until KSP connects or demo is on |
| Sys Info Display | `sysinfo.*` | Boot lines (short forms; kept and replayed if the display answers late, the first marked REPLAY), heartbeat with the flight-scene bit, the 27-byte status frame (supply, fan, links, module present / FAULT masks, I2C errors, uptime, version), the switch-error list, the backlight frame. Display firmware: `Software/Displays/KCMk1_SysInfo` |
| Displays | `carriers.*` | The four 7" displays' commands every 100 ms with PROCEED, byte for byte as the old master; status reads every 500 ms (each display's own KSP link, and Info Display 1's screen; the last good packet of each is kept for the input test screen). The BACKLIGHT frame `[0xB8, flight %, standby %, 0]` to all four at once on a change and every 2 s; levels from `BL_FLIGHT_PCT` 70 / `BL_STANDBY_PCT` 20, changed with `backlight` (GitHub #47) |
| Input test | `input_test.*` | While Info Display 1 reports screen 18 (its input test screen, design §13.1), page 0 (every input, 40 bytes, sync 0xAB: presence, FAULT, the button modules' state words as sent) every 100 ms, and page 1 (INT lines, GPWS / Pre-Warp values, lifecycles, 24 bytes) and page 2 (the four 7" displays' last status, 24 bytes) every 500 ms; byte layout in `input_test.h`. Adds up the ENC2 count since boot |
| KSP link | `simpit_link.*` | Non-blocking handshake every 2 s while KSP is down; inbound dispatch by table; SIMPIT LOST after 5 s of silence in flight, re-handshake after 10 s |
| KSP send gate | `ksp_out.*` | The only way to send to KSP; sends nothing in demo or while the link is down |
| I2C | `i2c_bus.*` | Every transaction counted and logged; a bus held low is clocked free |
| Board | `board.*` | Reset cause, watchdog (5 s, warning 0.5 s before), breadcrumb, reboot-loop guard, device probes, display reset. From stage 8: the 12 V power monitor (INA228, 4 Hz, low-supply warning), the fan from the EMC2101's internal temperature (1 Hz, 30–100 % with 2 °C hysteresis, rpm), the Sys Info Display's status screen |
| Debug | `debug.*` | Categorised, rate-limited log; serial console |
| State | `state.*`, `config.h` | Shared state; pins, addresses and timings |
| Module bus (stage 2) | `module_bus.*` | The 13 input modules: identity, INT-line learning at power-on (and the displays' after release), stored map in EEPROM, LEDs off and DISABLE at boot; INT-driven reads (polling for a module with no known line); packet checks (type, lifecycle, fault, missed packets); events pressed / released / tapped / held (2 s) for every packet kind; LED frames (changes only, ≤ one per 50 ms, resent on ENABLE); lifecycle (ACTIVE in a flight scene or demo, DISABLED otherwise; a rebooted module is brought back); re-probe of absent modules every 5 s. From stage 3: the switch modules' inputs in any lifecycle, master reset, and the bulb test (start / stop, at least 0.5 s) |
| Switches (stage 3) | `switches.*` | Switch Group 1 (Function Control B16–B23), Switch Group 2 (Vehicle Control B16–B23), the mode selector (Time Control B12 / B13). The switch modules are polled every 100 ms while not ACTIVE, so demo, debug and the interlocks follow their switches in any scene. The first read sets the master's modes and sends nothing; a flip acts. GEAR, BRAKE, EXT LT, SAS and RCS act on KSP only in a flight scene. SWITCH ERR when one disagrees with the vessel for 1 s (not in demo, not on EVA), with the switches named on the Sys Info Display. MSTR RESET, DISP CLR, LTG TEST |
| Interlocks (stage 3) | `interlocks.*` | ENGINE SAFE / ARMING / ARMED: 0 % to KSP, the lever driven to 0 % (not while touched), FINE only while ARMED; A/P INHIB; drop notices for HOLD and the autopilots |
| ABORT, STAGE (stage 3) | `panel_safe.*` | Safe lines debounced 10 ms; ABORT action group; STAGE in a flight scene with a 1 s lockout |

| Panels (stage 4) | `panels.*`, `cag.*`, `keys.*` | Shared rules: LEDs set every pass from the current state (dark = unusable, lit = KSP reports it on, demo = ACTIVE while held); presses act only in a flight; CAG numbers with the control-group offset; keyboard taps and held keys (released when a flight ends) |
| Vehicle Control | `panel_vehicle.cpp` | BRAKES held (with the BRAKE switch rule), LIGHTS and GEAR toggles, CAG 13–18, HEAT SHIELD / MAIN / DROGUE state machines from KSP's group states (act on release, 2 s hold resets; chutes need CHUTE ARM) |
| Stability Control | `panel_stability.cpp`, `hold.*` | SAS modes (dark with SAS OFF or when unavailable), HOLD (SPC: SAS stability assist; PLN / RVR from stage 7), INVERT (F held) |
| Action Control | `panel_action.cpp` | AG 1–12 = CAG 1–12, no offset |
| AUX CTRL | `panel_aux.cpp` | EVA keys (dark outside EVA), UNDOCK, PRI / ALT control points |
| Dual Encoder | `panel_encoders.cpp` | Control group 1–6, camera next / previous / reset |

| Control channels (stage 5) | `ksp_frame.*`, `lever.h` | Rotation, translation, wheel, camera and throttle merged every pass and sent on a change; neutral first in each flight; sticks ignored under an autopilot or HOLD; throttle 0 unless ENGINE ARMED, else the autopilot owner (lever driven to match) or the lever; the autopilot interface (`thrAutoThrottle`, `rotSetAutoAxes`, `wheelSetAuto`, …) |
| Sticks | `panel_sticks.cpp` | Mapping by control mode (SPC / PLN / RVR), THC / RHC PREC, trim (BTN02 / BTN01), stick button (A/P disconnect, RVR brakes, EVA run), camera mode and buttons, EVA keys |
| Throttle | `panel_throttle.cpp` | The lever's state and command; 100 % / UP / DOWN / 0 % move the lever |

| UI Control (stage 6) | `panel_ui.cpp` | Keys (F2, Alt+F12, numpad ., ', Tab, Shift+Tab, [ ], M, C), the screenshot macro (F2 F1 F2, 150 ms apart), navball cycle |
| Function Control | `panel_function.cpp` | CAG 25–39 buttons with the control-group offset |
| Time Control | `panel_time.cpp` | WARP TO ApA / PeA / MNVR (burn start) / SOI / MORN less the Pre-Warp lead, dark when not available; STOP, > / <, PHYS + > / < (physics 1×–4×), LOAD (F9 held), PAUSE, SAVE |
| GPWS | `panel_gpws.cpp` | Its state and threshold relayed to the Annunciator |
| Autopilots (stage 7) | `autopilots.*`, `{ascent,hold,burn,landing}_autopilot.*`, `attitude_controller.*`, `ap_arbiter.cpp`, `stage_helpers.cpp`, `control_links.h` | Ported from Controller_Main, control laws unchanged; fed by every inbound Simpit message; their axes on the frame channels; no stick / lever disconnect; A/P INHIB, ENGINE SAFE, A/P disconnect rules; HOLD in PLN / RVR through the hold autopilot |
| Consoles | `infodisp_link.*` | Info Display 2 consoles: one command at a time with ACK, the frame for the console on screen, the summary to both Info Displays | board services and cutover (8).

## Serial console

Open the first serial port at any baud rate and type a command:

| Command | Shows |
|---|---|
| `help` | The command list |
| `status` | Boot phase, reset cause, watchdog, links, annunciator flags, I2C and send counters |
| `simpit` | KSP link, each channel's message count and age |
| `modules` | Each module: present, firmware, INT line, lifecycle, reads / missed / faults |
| `intmap [clear]` | Each INT line: level now, owner, source (default / stored / learned); `clear` erases the stored map |
| `switches` | Every switch: position, the vessel's state, SWITCH ERR; the mode selector |
| `interlocks` | ENGINE state, the lever (position, touch, precision), A/P, drop listeners |
| `frame` | Each control channel's last values sent; the throttle owner and autopilot axes |
| `board` | 12 V supply (voltage, current, power), board temperature, fan duty and rpm |
| `ap` / `hp` / `bp` / `lp` | The ascent / hold / burn / landing autopilot's bench line: `STATUS`, `ARM`, `ENG ALT`, `SET IAS 180`, `OFF`, … |
| `log` | Debug categories; `log <cat\|all> on\|off\|auto` |
| `debug on\|off` | Debug output, until the next UPTLM flip |
| `backlight [flight %] [standby %]` | The 7" displays' backlight levels; with values (0–100) sends new levels to all four at once, until a restart. The displays clamp to their 5 % floor |
| `wdtest yes` | Hangs the loop, so the watchdog resets the master in 5 s (bench test) |

## Host tools

```
python3 tools/host_build.py --all                         # compile, config check, selftest
python3 tools/host_build.py --all --simpit PATH/src       # also compile against the real KerbalSimpit
```

- **Compile:** every file, with `-Wall -Wextra -Werror`, against the stand-ins in `tools/host/`.
- **`--simpit`:** compiles against a clone of `Simpit-team/KerbalSimpitRevamped-Arduino` instead, so every message id and payload is checked against the real library.
- **Config check:** compares the display addresses and sync bytes in `config.h` with `KCMk1_SystemConfig`, and the BACKLIGHT sync, length and default levels (`BL_FLIGHT_PCT` / `BL_STANDBY_PCT` against `KCM_BL_BRIGHTNESS_PCT` / `KCM_BL_STANDBY_PCT`).
- **Console table:** 24 entries (`CON_MAX_CMDS`); a command that does not fit is reported on Serial at start-up rather than dropped silently.
- **Selftest** (`tools/tests/`): runs `setup()` and `loop()` against simulated devices: Sys Info Display, four displays, INA228, EMC2101, KSP. It checks:
  - boot lines, order and colours;
  - PROCEED before KSP is tried;
  - the waiting line and the 60 s hint;
  - release on connect or demo;
  - display command bytes;
  - SIMPIT LOST and the re-handshake;
  - display link bits;
  - missing parts;
  - a late Sys Info Display (the replay marked REPLAY, no REPLACE in it);
  - bus recovery;
  - the send gate in demo;
  - the console and the debug rate limit;
  - watchdog reports and the reboot-loop guard;
  - modules (stage 2):
    - discovery and INT-line learning, including re-cabling and a restart with the stored map;
    - lifecycle with the flight scene and demo;
    - every event kind (tap, re-press, hold) and packet kind;
    - LED frames and their order;
    - missed packets, faults, a module reboot;
    - a wrong module type;
    - a late module, polling, a module that stops answering;
    - a watchdog restart in flight;
    - the console.
  - switches and interlocks (stage 3):
    - the first read sets the modes and sends nothing; no switch is enforced on entering a flight;
    - each Switch Group 2 flip in flight, and none outside a flight;
    - SWITCH ERR: settle time, the Sys Info frame, the exceptions, EVA, demo;
    - demo and debug from the switches, in the menus;
    - the mode selector, including both inputs on;
    - ENGINE SAFE / ARM with the lever (touched, held up while arming), FINE, A/P INHIB;
    - Switch Group 1 missing;
    - ABORT and STAGE: debounce, lockout, flight only, a line high at start-up;
    - MSTR RESET, DISP CLR, LTG TEST (start / stop, with a 0.5 s minimum);
    - a watchdog restart in flight with switches on.
  - flight panels (stage 4):
    - Vehicle: CAG buttons and the control-group offset, toggles, LEDs from KSP, BRAKES with REL and LOCK, the state machines (deploy, cut, nothing, 2 s reset, CHUTE SAFE);
    - Stability: SAS modes with the switch and availability, INVERT and SWITCH ERR, HOLD (engage, restore SAS, A/P INHIB, mode change, PLN, MSTR RESET);
    - Action: no offset; AUX: EVA keys, flash, UNDOCK, PRI / ALT; Dual Encoder: group wrap, camera keys;
    - held keys released when the flight ends; demo LEDs.
  - sticks and throttle (stage 5):
    - neutral channels on entering a flight, nothing sent while nothing moves;
    - SPC / PLN / RVR mapping, precision, trim (set, hold, clear, cleared on a mode change);
    - wheel throttle 0 under ENGINE SAFE; the RVR stick button's brakes;
    - sticks ignored under HOLD, camera mode still working, the A/P disconnect;
    - translation and the camera buttons; EVA keys with hysteresis, released when the EVA ends;
    - the lever and its buttons (FINE, ENGINE SAFE); an autopilot owning the throttle (lever driven, pilot ignored, FINE dropped and restored, ENGINE SAFE wins); autopilot rotation axes;
    - demo; the `frame` console command.
  - board (stage 8): the INA228 and EMC2101 set up at boot, readings, the Sys Info STATUS frame, the fan curve and its hysteresis, the low-supply warning, the `board` command, devices missing; the BACKLIGHT frame to all four displays and the Sys Info Display, its 2 s repeat, the `backlight` command and an out-of-range level refused; the Sys Info STATUS frame's module, I2C, uptime and version bytes, and the CONTROL flight-scene bit.
  - autopilots (stage 7): a console command engages the hold autopilot, ACKed once, its frame and the summary pushed; sticks ignored and nothing disconnects on stick input; ENGINE SAFE refuses and drops IAS (ALT stays); A/P INHIB disconnects and refuses; the A/P disconnect, A/P OFF, telemetry loss, leaving the flight; HOLD in PLN and RVR through the hold autopilot (INHIB leaves it, a console engage releases it, ENGINE SAFE leaves heading only); the ascent's axes and its ENGINE SAFE drop; the bench consoles.
  - remaining panels (stage 6): UI keys, modifiers, navball, the spaced screenshot macro; Function CAGs and offset; Time warp-to with the lead and each availability rule, STOP, warp up / down, PHYS steps, LOAD held, PAUSE, SAVE; the GPWS relay; the input test frames (none while closed, all three pages and their sizes while Info Display 1 reports screen 18, the raw state words of a button and a switch, a module's FAULT bit, axes, lever, touch, ENC2 count, safe lines, INT lines, each module's lifecycle, the displays answering, their flags and Info Display 2's screen, and the frames stopping on close).

Run it after every change.

## Bench checks

These can only be confirmed on the hardware (design §18):

1. **Breadcrumb retention:** the SNVS general-purpose registers keep the breadcrumb (written by the watchdog warning) through a watchdog reset, and `SRC_SRSR` reports it. Type `wdtest yes` at the console; after the reset the boot screen should say `RESET: WATCHDOG` and `HUNG IN dbgService`. Three in a minute trip the reboot-loop guard (HALTED).
2. **SIMPIT LOST:** a parked vessel still sends inbound Simpit traffic, so SIMPIT LOST and the re-handshake do not trip falsely.
3. **Boot order:** the boot screen shows each step, and a missing display or sensor shows in red without stopping start-up.
4. **INT learning (stage 2):** after a power cycle every module's line shows LEARNED in `intmap`, matching the slot it is in; after moving one module to another slot and power-cycling, its new line is learned.
5. **Empty slots:** an empty slot's INT line is not driven. `intmap` should show its level, and the master ignores it. Check that no floating line confuses the learning (it would show as "lines rose at once" in the log).
6. **LED order:** a known LED frame lights the expected buttons (low nibble = even button).
7. **Switches in the menus (stage 3):** with KSP at the main menu, flip SCE to AUX: demo comes on (Annunciator DEMO, modules lit). This relies on a DISABLED module answering a read with its current inputs.
8. **Switch polarity (stage 3):** in flight, each Switch Group 2 switch's "on" position (GEAR DOWN, BRAKE LOCK, EXT LT ILLUM, SAS ENAB, RCS ENAB) turns its function on, and `switches` shows the vessel agreeing.
9. **ENGINE SAFE (stage 3):** move the lever while SAFE: it goes back to 0 % when released; flip to ARM with the lever held up: inputs are taken only once it is back at 0 %.
10. **LTG TEST (stage 3):** every module lights full white for as long as LTG TEST is held (at least 0.5 s for a flick), and the panel keeps working meanwhile. Needs the start / stop module firmware (KerbalButtonCore 2.3, KerbalJoystickCore 2.1, Throttle 2.2).
10a. **Lever hold:** with an autopilot owning the throttle, hold the lever away from its position: the motor pushes back for about 0.5 s, then lets go; release, and it drives back to the autopilot's value. With no autopilot, a touch stops the motor at once.
11. **Keyboard (stage 4):** on EVA the AUX keys work; INVERT holds F (SAS inverts while held); ENC2 changes the Hullcam camera. KSP must have focus.
12. **Action groups (stage 4):** each CAG button lights with its group in KSP, in control groups 1 and 2; a parachute deploys and cuts from MAIN, and a 2 s hold resets it after a repack.
13. **Stick directions (stage 5):** in SPC, PLN and RVR each axis moves the vessel the expected way (pitch, yaw, roll, translation, steering, wheel throttle), camera mode turns the camera, and on EVA the keys walk the expected way. Fix any wrong sign with `STICK_SIGN` in `panel_sticks.cpp`.
14. **Lever (stage 5):** the 100 % / UP / DOWN / 0 % buttons move the lever; with ENGINE SAFE the throttle stays at 0 % in KSP.
15. **Time Control (stage 6):** each WARP TO stops the Pre-Warp lead short of its event; the buttons go dark as their events disappear (landed, no node, in the atmosphere, escape); the screenshot macro hides the UI in the picture.
16. **Autopilots (stage 7):** each Info Display 2 console engages and disengages its modes and shows them; A/P INHIB and ENGINE SAFE refuse with a message on KSP's screen; the rotation stick button disconnects; HOLD holds altitude and heading in PLN, heading and speed in RVR.
17. **Board (stage 8):** the Sys Info status screen shows the 12 V supply as a meter reads it (this checks the 15 mΩ shunt); the fan speeds up as the board warms, and its rpm is plausible.
18. **Backlight:** `backlight 70 20` dims each 7" display to 20 % on its standby screen and back to 70 % in flight, with a smooth ramp; step down with `backlight 10 5` and `backlight 5 5` and check for flicker at low duty and whether the 5 % floor (`KCM_BL_MIN_PCT`) is still readable. Change the floor in KCMk1_SystemConfig if not.
19. **Input test screen:** on Information Display 1, a two-finger pull down from the top-right corner opens it (LIVE in the title) and the same pull closes it. Press buttons, flip switches, move both sticks and the lever, turn ENC2, close ABORT / STAGE and turn the mode selector: each shows on the screen (the button boxes are the state words as the modules send them). Unplug a module: it reads MISSING. Outside a flight scene the modules read DISABLED. The DISPLAYS rows show each 7" display's LINK and FLIGHT; unplug one and it reads NO ANSWER; Info 2's screen name follows its console.
