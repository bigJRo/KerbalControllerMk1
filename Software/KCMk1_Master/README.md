# KCMk1_Master

The new master firmware for the Kerbal Controller Mk1: Teensy 4.1 on the Ctrl Module (KC-01-1701 V2.0). It replaces `Software/Controller_Main`, which stays in the repository as a reference until the cutover stage.

| | |
|---|---|
| **Design** | `Documents/Developer/Master_Firmware_Design.md` |
| **Hardware** | `Documents/Developer/Hardware_Inventory.md` |
| **Protocols** | `Documents/Developer/I2C_Protocol_Specification.md` (Sys Info Display: §15.4) |
| **Status** | Stage 1 of 8: skeleton and boot |

## Building

- **Board:** Teensy 4.1, **USB Type: Dual Serial**. `Serial` is the debug console; `SerialUSB1` is the KerbalSimpit link. Point KSP's Simpit `Settings.cfg` at the second COM port.
- **Libraries:** KerbalSimpit 2.4.0; WDT_T4 (ships with Teensyduino).
- **Layout:** `KCMk1_Master.ino` holds `setup()` and `loop()` only. Every other part is a `.cpp` / `.h` pair, compiled on its own, so nothing depends on the IDE's prototype generation.

## What stage 1 does

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

Still to come: modules and INT learning (stage 2), switches and interlocks (3), flight panels (4), sticks and throttle (5), the other panels (6), autopilots (7), board services and cutover (8).

## Serial console

Open the first serial port at any baud rate and type a command:

| Command | Shows |
|---|---|
| `help` | The command list |
| `status` | Boot phase, reset cause, watchdog, links, annunciator flags, I2C and send counters |
| `simpit` | KSP link, each channel's message count and age |
| `log` | Debug categories; `log <cat\|all> on\|off\|auto` |
| `debug on\|off` | Debug output (until the UPTLM switch is read, from stage 3) |
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
  - watchdog reports and the reboot-loop guard.

Run it after every change.

## Bench checks for stage 1

These can only be confirmed on the hardware (design §18):

1. **Breadcrumb retention:** the SNVS general-purpose registers keep the breadcrumb (written by the watchdog warning) through a watchdog reset, and `SRC_SRSR` reports it. Type `wdtest yes` at the console; after the reset the boot screen should say `RESET: WATCHDOG` and `HUNG IN dbgService`. Three in a minute trip the reboot-loop guard (HALTED).
2. **SIMPIT LOST:** a parked vessel still sends inbound Simpit traffic, so SIMPIT LOST and the re-handshake do not trip falsely.
3. **Boot order:** the boot screen shows each step, and a missing display or sensor shows in red without stopping start-up.
