# KCMk1_Master

The new master firmware for the Kerbal Controller Mk1: Teensy 4.1 on the Ctrl Module (KC-01-1701 V2.0). It replaces `Software/Controller_Main`, which stays in the repository as a reference until the cutover stage.

| | |
|---|---|
| **Design** | `Documents/Developer/Master_Firmware_Design.md` |
| **Hardware** | `Documents/Developer/Hardware_Inventory.md` |
| **Protocols** | `Documents/Developer/I2C_Protocol_Specification.md` (Sys Info Display: §15.4) |
| **Status** | Stages 1–4 of 8: skeleton and boot; module bus; switches and interlocks; flight panels |

## Building

- **Board:** Teensy 4.1, **USB Type: Dual Serial**. `Serial` is the debug console; `SerialUSB1` is the KerbalSimpit link. Point KSP's Simpit `Settings.cfg` at the second COM port.
- **Libraries:** KerbalSimpit 2.4.0; WDT_T4 and EEPROM (ship with Teensyduino); KerbalModuleCommon (`Software/Modules/libraries`, the modules' shared header: type ids, commands, LED states, packet sizes).
- **Layout:** `KCMk1_Master.ino` holds `setup()` and `loop()` only. Every other part is a `.cpp` / `.h` pair, compiled on its own, so nothing depends on the IDE's prototype generation.

## What it does so far

| Part | Files | Does |
|---|---|---|
| Boot | `boot.*` | Runs the start-up steps. Each step reports a line to the Sys Info Display and to Serial; a step that times out shows in red and start-up carries on. The Sys Info Display stays on its boot screen until KSP connects or demo is on |
| Sys Info Display | `sysinfo.*` | Boot lines (kept and replayed if the display answers late), heartbeat, status frame, switch-error window |
| Displays | `carriers.*` | The four 7" displays' commands every 100 ms with PROCEED, byte for byte as the old master; status reads every 500 ms (each display's own KSP link) |
| KSP link | `simpit_link.*` | Non-blocking handshake every 2 s while KSP is down; inbound dispatch by table; SIMPIT LOST after 5 s of silence in flight, re-handshake after 10 s |
| KSP send gate | `ksp_out.*` | The only way to send to KSP; sends nothing in demo or while the link is down |
| I2C | `i2c_bus.*` | Every transaction counted and logged; a bus held low is clocked free |
| Board | `board.*` | Reset cause, watchdog (5 s, warning 0.5 s before), breadcrumb, reboot-loop guard, device probes (INA228, EMC2101), display reset |
| Debug | `debug.*` | Categorised, rate-limited log; serial console |
| State | `state.*`, `config.h` | Shared state; pins, addresses and timings |
| Module bus (stage 2) | `module_bus.*` | The 13 input modules: identity, INT-line learning at power-on (and the displays' after release), stored map in EEPROM, LEDs off and DISABLE at boot; INT-driven reads (polling for a module with no known line); packet checks (type, lifecycle, fault, missed packets); events pressed / released / tapped / held (2 s) for every packet kind; LED frames (changes only, ≤ one per 50 ms, resent on ENABLE); lifecycle (ACTIVE in a flight scene or demo, DISABLED otherwise; a rebooted module is brought back); re-probe of absent modules every 5 s. From stage 3: the switch modules' inputs in any lifecycle, master reset, and a quiet bus during the bulb test |
| Switches (stage 3) | `switches.*` | Switch Group 1 (Function Control B16–B23), Switch Group 2 (Vehicle Control B16–B23), the mode selector (Time Control B12 / B13). The switch modules are polled every 100 ms while not ACTIVE, so demo, debug and the interlocks follow their switches in any scene. The first read sets the master's modes and sends nothing; a flip acts. GEAR, BRAKE, EXT LT, SAS and RCS act on KSP only in a flight scene. SWITCH ERR when one disagrees with the vessel for 1 s (not in demo, not on EVA), with the switches named on the Sys Info Display. MSTR RESET, DISP CLR, LTG TEST |
| Interlocks (stage 3) | `interlocks.*` | ENGINE SAFE / ARMING / ARMED: 0 % to KSP, the lever driven to 0 % (not while touched), FINE only while ARMED; A/P INHIB; drop notices for HOLD and the autopilots |
| ABORT, STAGE (stage 3) | `panel_safe.*` | Safe lines debounced 10 ms; ABORT action group; STAGE in a flight scene with a 1 s lockout |

| Panels (stage 4) | `panels.*`, `cag.*`, `keys.*` | Shared rules: LEDs set every pass from the current state (dark = unusable, lit = KSP reports it on, demo = ACTIVE while held); presses act only in a flight; CAG numbers with the control-group offset; keyboard taps and held keys (released when a flight ends) |
| Vehicle Control | `panel_vehicle.cpp` | BRAKES held (with the BRAKE switch rule), LIGHTS and GEAR toggles, CAG 13–18, HEAT SHIELD / MAIN / DROGUE state machines from KSP's group states (act on release, 2 s hold resets; chutes need CHUTE ARM) |
| Stability Control | `panel_stability.cpp`, `hold.*` | SAS modes (dark with SAS OFF or when unavailable), HOLD (SPC: SAS stability assist; PLN / RVR from stage 7), INVERT (F held) |
| Action Control | `panel_action.cpp` | AG 1–12 = CAG 1–12, no offset |
| AUX CTRL | `panel_aux.cpp` | EVA keys (dark outside EVA), UNDOCK, PRI / ALT control points |
| Dual Encoder | `panel_encoders.cpp` | Control group 1–6, camera next / previous / reset |

Still to come: sticks and throttle (5), the other panels (6), autopilots (7), board services and cutover (8).

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
| `log` | Debug categories; `log <cat\|all> on\|off\|auto` |
| `debug on\|off` | Debug output, until the next UPTLM flip |
| `wdtest yes` | Hangs the loop, so the watchdog resets the master in 5 s (bench test) |

## Host tools

```
python3 tools/host_build.py --all                         # compile, config check, selftest
python3 tools/host_build.py --all --simpit PATH/src       # also compile against the real KerbalSimpit
```

- **Compile:** every file, with `-Wall -Wextra -Werror`, against the stand-ins in `tools/host/`.
- **`--simpit`:** compiles against a clone of `Simpit-team/KerbalSimpitRevamped-Arduino` instead, so every message id and payload is checked against the real library.
- **Config check:** compares the display addresses and sync bytes in `config.h` with `KCMk1_SystemConfig`.
- **Selftest** (`tools/tests/`): runs `setup()` and `loop()` against simulated devices: Sys Info Display, four displays, INA228, EMC2101, KSP. It checks:
  - boot lines, order and colours;
  - PROCEED before KSP is tried;
  - the waiting line and the 60 s hint;
  - release on connect or demo;
  - display command bytes;
  - SIMPIT LOST and the re-handshake;
  - display link bits;
  - missing parts;
  - a late Sys Info Display;
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
    - MSTR RESET, DISP CLR, LTG TEST (including the bulb-test quiet window);
    - a watchdog restart in flight with switches on.
  - flight panels (stage 4):
    - Vehicle: CAG buttons and the control-group offset, toggles, LEDs from KSP, BRAKES with REL and LOCK, the state machines (deploy, cut, nothing, 2 s reset, CHUTE SAFE);
    - Stability: SAS modes with the switch and availability, INVERT and SWITCH ERR, HOLD (engage, restore SAS, A/P INHIB, mode change, PLN, MSTR RESET);
    - Action: no offset; AUX: EVA keys, flash, UNDOCK, PRI / ALT; Dual Encoder: group wrap, camera keys;
    - held keys released when the flight ends; demo LEDs.

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
10. **LTG TEST (stage 3):** today's modules run a fixed 2 s test (design §17); check nothing else on the bus misbehaves during it.
11. **Keyboard (stage 4):** on EVA the AUX keys work; INVERT holds F (SAS inverts while held); ENC2 changes the Hullcam camera. KSP must have focus.
12. **Action groups (stage 4):** each CAG button lights with its group in KSP, in control groups 1 and 2; a parachute deploys and cuts from MAIN, and a 2 s hold resets it after a repack.
