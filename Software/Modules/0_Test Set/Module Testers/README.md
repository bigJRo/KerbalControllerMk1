# KCMk1 Per-Module Testers

**Version:** 1.0.0
**Date:** 2026-11-08
**Board:** KC-01-9001 v2.0 / KC-01-9002 Module Tester (Seeed XIAO RA4M1, 2.8" ST7789V touchscreen, INA228)

One bench-test sketch for each ATtiny816 module. Each sketch knows its module's inputs, LEDs, colours and behaviour, and walks the operator through a full test. Results appear on the touchscreen and are printed to the USB serial port as a report.

To check a freshly built board before its real firmware is loaded, flash the stand-alone firmware in `../Construction Tests/` instead: it needs only power.

The universal `KCMk1_Module_Tester` sketch is still the quick way to look at any module (scan, live view, construction test). Use these per-module sketches to sign off a built module.

---

## Sketches

| Sketch | Module | I2C | Before you start |
|---|---|---|---|
| `KCMk1_Test_UI_Control` | UI Control | 0x20 | — |
| `KCMk1_Test_Function_Control` | Function Control | 0x21 | All Switch Group 1 switches OFF |
| `KCMk1_Test_Action_Control` | Action Control | 0x22 | — |
| `KCMk1_Test_Stability_Control` | Stability Control | 0x23 | — |
| `KCMk1_Test_Vehicle_Control` | Vehicle Control | 0x24 | All Switch Group 2 switches OFF |
| `KCMk1_Test_Time_Control` | Time Control | 0x25 | Mode selector at PLN (centre) |
| `KCMk1_Test_Auxiliary_Control` | Auxiliary Control | 0x26 | — |
| `KCMk1_Test_Joystick_Rotation` | Joystick Rotation | 0x28 | Hands off the stick at power-on |
| `KCMk1_Test_Joystick_Translation` | Joystick Translation | 0x29 | Hands off the stick at power-on |
| `KCMk1_Test_GPWS` | GPWS | 0x2A | — |
| `KCMk1_Test_PreWarp_Time` | Pre-Warp Time | 0x2B | — |
| `KCMk1_Test_Throttle` | Throttle | 0x2C | Motor supply connected; hands off the lever |
| `KCMk1_Test_Dual_Encoder` | Dual Encoder | 0x2D | — |

`KCMk1_Test_GPWS` and `KCMk1_Test_PreWarp_Time` replace the serial-menu testers in `KC-01-1880_7-Segment Display/`.

### Steps

| Module family | Steps |
|---|---|
| Button modules (UI, Function, Action, Stability, Vehicle, Time, Aux) | Identity, boot handshake, enable and INT, button walk, switch walk (Function, Vehicle) or mode selector (Time), LED walk, LED states (Vehicle adds WARNING, ALERT, ARMED, PARTIAL_DEPLOY, CUT), brightness, bulb test, lifecycle, supply current |
| Joysticks | Identity, boot handshake, enable and INT, buttons, centre and deadzone, full travel on each axis (the sign of each direction is recorded), LED walk, LED states / brightness / bulb test, lifecycle, supply current |
| GPWS | Identity, boot handshake, enable and INT, ENA mode cycle, PROX and RNDZ, encoder and knob push, encoder speed and limits, display digits and brightness, bulb test, lifecycle, supply current |
| Pre-Warp Time | Identity, boot handshake, enable and INT, presets and knob push, encoder, encoder speed and limits, display digits and brightness (incl. the 9999 clamp), bulb test, lifecycle, supply current |
| Throttle | Identity, boot handshake, enable and INT, buttons, lever travel and touch sensor, motor seek (0 / 100 / 50 %), touch blocks the motor, precision mode, lever hold, backlights and bulb test, lifecycle, supply current |
| Dual Encoder | Identity, boot handshake, enable and INT, push switches, ENC1 and ENC2 counted both ways (exactly 10 clicks each way), lifecycle, supply current |

What the shared steps check:

- **Identity:** type ID, firmware version (same major, minor at least the one the tester was written for), capability flags, and no fault flag.
- **Boot handshake:** the tester pulses RST, then expects INT within the start-up time, a boot packet (BOOT_READY, transaction counter 1), INT released by the read, and DISABLED after CMD_DISABLE. If RST does not restart the module, the tester asks you to unplug and reconnect it, and notes that RST had no effect.
- **Enable and INT:** after CMD_ENABLE the module is ACTIVE and INT stays high with no input. One press and release then gives exactly the expected number of packets, and the counter goes up by one per INT.
- **Button / switch walk:** each input is named in turn and must be the only one that changes. An input that is ON at the start, or a bit that is not connected and goes high, fails the step. Tap **FAIL** if an input does nothing.
- **LED walk:** one LED at a time in its ACTIVE colour, with the expected colour shown on screen.
- **Lifecycle:** while DISABLED or SLEEPING the module must not raise INT but still reads its inputs; WAKE counts one INT.
- **Supply current:** module current from the INA228, idle (DISABLED, dark) and under a load (bulb test). The limits are provisional; tighten them once real boards have been measured.

---

## Using a tester

1. Load the sketch for the module (board: Seeed XIAO RA4M1). Connect the module to P1 and switch the tester on.
2. The tester waits for the module at its address and checks the identity. A different module, or a bad identity, is reported on screen.
3. The menu lists the steps and the last result of each. Tap **RUN ALL** for the full test, tap a step to run it alone, or tap **LIVE** for the raw packet view.
4. Follow the instruction on each step. Automatic checks advance by themselves; visual checks wait for **PASS** or **FAIL**. Every step has **SKIP** and **ABORT**.
5. A failed step shows the reason and offers **NEXT**, **RETRY** or **ABORT**. The summary lists every step, with the failure reason in place of the name.

Open the serial monitor at 115200 baud for a report:

```
==================================================
KCMk1 tester: Action Control  (tester core 1.0.0)
Address 0x22  type 0x3  fw 2.2  caps 0x0
--------------------------------------------------
[ 1] Identity               PASS type 0x03 fw 2.2 caps 0x00
[ 2] Boot handshake         PASS boot INT after 4 ms, tx 1
[ 3] Enable and INT         PASS 2 packets, tx continuous
[ 4] Buttons                PASS 12 inputs ok
...
Result: PASS   pass 10  fail 0  skip 0
```

---

## Installing

1. Copy `Software/Modules/libraries/KCMk1TesterCore` and `KerbalModuleCommon` into your Arduino `libraries` folder.
2. Install from the Library Manager: **Adafruit GFX Library**, **Adafruit ST7735 and ST7789 Library**, **Adafruit FT6206 Library**.
3. Board: Seeed XIAO RA4M1 (Renesas core). If the ST7789 library fails on `wiring_private.h`, add the empty-file shim described in `KCMk1_Module_Tester/README.md`.

---

## Firmware these testers expect

The testers were written against the module firmware in this repository and check its identity version. A module running older firmware fails **Identity** ("fw 2.2, tester expects 2.4") — reflash it first.

| Module | Identity fw | Fixed in that version (found by these testers) |
|---|---|---|
| Button modules (KerbalButtonCore) | 2.4 | Library compiled inside the sketch. Before, the Arduino IDE built the library without the sketch's `KBC_INPUT_COUNT 24`, so Function and Vehicle Control ran a 16-input library (Buttons and Switches failed). |
| Joysticks (KerbalJoystickCore) | 2.2 | Each button edge reported once (Enable and INT counted 4 packets). Library compiled inside the sketch, so the Translation stick's `KJC_AXIS2_INVERT -1` takes effect (AXIS2 forward now gives the opposite sign to the Rotation stick). |
| Throttle | 2.3 | A button press raises INT with the lever still (Enable and INT reported "no INT after the input"). |
| GPWS, Pre-Warp Time (Kerbal7SegmentCore) | 1.0 | — |
| Dual Encoder | 2.0 | — |

Still open, reported as a note only: on the joysticks, full negative deflection (-32768) equals the `KJC_AXIS_SUPPRESS` sentinel, so that sample raises no INT; the axis step notes "-32768 sent without INT".

---

## Not yet checked on hardware

The testers have been compiled and run on the host against the real module firmware (below), but not yet on a tester board. On first use, check:

- Touch mapping and screen orientation (same set-up as the universal tester, which was confirmed on hardware).
- Whether RST on P1 restarts the module; if not, the boot step uses the reconnect fallback.
- The supply-current limits, against real measurements.
- That the operator prompts are practical, for example "20 clicks in one go" for the encoder acceleration check.

---

## Simulation

`../tools/tester_sim/` builds each tester together with the real module firmware on the host and runs it with a scripted operator. See its README.

```
cd "Software/Modules/0_Test Set/tools/tester_sim"
python3 run_sims.py              # all modules, normal and fault runs
python3 run_sims.py gpws --shots # one module, with screenshots
```
