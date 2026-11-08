# Tester simulation

Runs each per-module tester sketch on the host against the **real module firmware**, with a scripted operator, and checks the result.

```
python3 run_sims.py                  # every module, normal and fault runs
python3 run_sims.py action vehicle   # selected modules
python3 run_sims.py gpws --shots     # also save PNG screenshots to shots/gpws/
python3 run_sims.py throttle --keep  # keep the build directory (run ./sim by hand)
```

Needs g++ and Python 3 with Pillow (font generation and screenshots).

## How it works

- **Two programs in one process.** The tester sketch, KCMk1TesterCore and the simulation core form the executable. The module sketch and its library are built into a shared library, the way the Arduino IDE builds them: the module library's `.cpp` files do not see the sketch's `#define`s, and every sketch function is declared after the `#include`s. A RST pulse from the tester unloads and reloads that library, so the module restarts with fresh globals.
- **Stand-ins** (`stubs/`) for the Arduino core, Wire (one object; controller side for the tester, target side for the module), the ST7789 framebuffer and FT6236 touch, tinyNeoPixel, ShiftIn and the ATtiny port registers. The MAX7219 display is decoded from the bit-banged SPI, the encoder ISRs run on simulated quadrature edges, and the INA228 reports a current worked out from the LEDs, display and motor.
- **The operator** (`sim_core.cpp`, `scenarios.cpp`) reads the text on the tester's screen and works the simulated hardware: presses buttons, moves switches, sticks and the throttle lever (simple lever physics driven by the module's motor outputs), turns encoders, unplugs and reconnects the module. Visual checks are answered from the simulated LEDs and display, so a wrong command in a test step fails the run.
- **Fault runs** (`--opt`): `kbc_dead`, `kbc_stuck` (input masks), `dead_pin`, `rst_dead`, `enc_reversed`, `enc_extra`. Each fault run lists the steps expected to fail.

The table in `run_sims.py` records the expected result of every run. A run can also require text in the serial report (the joystick runs check that AXIS2 forward has opposite signs on the Rotation and Translation sticks).

The simulation found four module-firmware problems when the testers were first run (24-input button modules, joystick double edges and AXIS2 inversion, throttle button INT); they are fixed in the firmware versions the testers now expect.
