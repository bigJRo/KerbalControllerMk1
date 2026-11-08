# KCMk1 Construction Test Firmware

**Version:** 1.0.0
**Date:** 2026-11-08

Stand-alone test firmware for checking a freshly built module board. Flash it to the board's ATtiny816, apply power, and the board tests itself: LEDs, display and motor automatically, then each input as you operate it. Nothing else is needed: no master, no I2C, no tester sketch. Power can come from 12 V on the slot connector or from the Module Tester.

Flash the module's real firmware afterwards. To test a board that is already fitted with its real firmware, use the touchscreen tester sketches in `../Module Testers/` instead.

| Sketch | Board | Modules on this board |
|---|---|---|
| `KC01_1801_ConstructionTest` | Button Module KC-01-1801/1802 | UI, Action, Stability, Time and Auxiliary Control |
| `KC01_1811_ConstructionTest` | Wide Button Module KC-01-1811/1812 | Function and Vehicle Control |
| `KC01_1831_ConstructionTest` | Joystick Module KC-01-1831/1832 | Joystick Rotation and Translation |
| `KC01_1841_ConstructionTest` | 7-Segment Display Module KC-01-1841/1842 | GPWS and Pre-Warp Time |
| `KC01_1871_ConstructionTest` | Throttle Module KC-01-1871/1872 | Throttle |

There is no construction test for the Dual Encoder (KC-01-1861/1862): the board has no LEDs, so it cannot show a result on its own. Check it with `Module Testers/KCMk1_Test_Dual_Encoder`.

---

## Button boards (1801, 1811)

1. **LED test** (about 10 s, automatic): each of the 12 button LEDs lights red, green and blue in turn, then all light white.
   - A step that stays dark is a dead LED. If every LED after it also stays dark, the data chain is broken there.
   - Swapped colours mean the colour order is wrong.
2. **Input test** (until power-off). Each button LED shows:

| LED | Meaning |
|---|---|
| Dim red | Button not pressed yet |
| White | Button held now |
| Dim green | Button seen |
| Blue | A terminal input is closed (see below) |
| Flashing red / white | Input already closed at power-on: stuck button, solder bridge or switch left on. Turns normal once it opens |
| Flashing magenta on LEDs 9–12 (1811 only) | One of the unconnected inputs B12–B15 reads closed: a board fault |

When every button and terminal input has been seen, all LEDs flash green three times.

The terminal inputs have no LEDs of their own, so they show blue on button LEDs 1–4 (1801: B12–B15) or 1–8 (1811: B16–B23). LED *n* is the *n*-th LED in the chain, which is button input B(*n*−1). Test each terminal input by joining it to the terminal's +5 V pin with a wire, or use the panel switches if they are already wired. The pin-by-pin table is in each sketch's header.

The input mapping and debounce come from KerbalButtonCore, the same code the module firmware uses. So a button that lights the wrong LED here is wired to the wrong input on the board.

## Joystick board (1831)

Keep your hands off the stick at power-on: the firmware samples each axis's centre then.

1. **LED test:** LED 1 shows red, green, blue, then LED 2 does the same, then both show white. If an axis rests far from mid-scale (pot not centred, or a wiring fault), both LEDs then flash amber three times.
2. **Input test:**

| Input | LED |
|---|---|
| AXIS1 (left / right) | LED 1 green one way, red the other, brighter with more deflection, white at the end of travel |
| AXIS2 (forward / back) | LED 2, same as AXIS1 |
| AXIS3 (twist) | Blue added to LED 1 one way and to LED 2 the other, cyan at the end of travel |
| BTN01 / BTN02 held | Its LED white |
| Stick button held | Both LEDs magenta |

With the stick released, both LEDs should be dark, which shows the centre and deadzone are fine. When both ends of all three axes and all three buttons have been seen, both LEDs flash green three times.

## 7-segment board (1841)

1. **Automatic part:**
   - all segments and LEDs on;
   - a digit sweep;
   - each LED red, green, blue and white in turn;
   - an INT pulse;
   - then 1111 with the LEDs amber, for 2 s.
2. **Prompts:** the display shows the number of the input to operate, and you have 15 s for each.

| Prompt | Input |
|---|---|
| 1, 2, 3 | Press the button whose LED is green |
| 4 | Press the encoder knob (all LEDs blue) |
| 5 | Turn the knob clockwise (LEDs amber) |
| 6 | Turn the knob anticlockwise (LEDs amber) |

3. **Result:**
   - **PASS:** the display flashes 8888 five times, then shows 0 with all LEDs green.
   - **FAIL:** the display shows the number of the first input that failed, and the LEDs of failed buttons are red.

   The result stays on until power-off.

This was `KC-01-1880_7-Segment Display/KC01_1880_ConstructionTest`. It has been renamed for the current board number, and its result now stays on screen.

## Throttle board (1871)

The motor runs from the board's own 9 V supply. The panel lamps (one shared output) and the lever are the only indicators. Keep your hands off the lever until step 3.

1. **Lamp test:** the lamps blink 3 times.
2. **Motor test:** the lever drives to 0 %, then 100 %, then 50 %, and the lamps blink once at each arrival. A fault stops the motor and repeats a blink code until you press any throttle button:

| Blinks | Fault |
|---|---|
| 1 / 2 / 3 | Did not reach 0 % / 100 % / 50 % within 3 s |
| 4 | Lever moved the wrong way (motor or wiper reversed) |
| 5 | Lever did not move (no 9 V, or a motor, driver or wiper fault) |

3. **Input test:** the lamps stay on.

| Input | What happens |
|---|---|
| Touch the fader knob | Lamps go off while it is touched |
| 100% / 0% | Lever drives to the top / bottom |
| UP / DOWN | Lever steps 10 % up / down |

Touching the lever stops the motor. When all four buttons and the touch sensor have been seen, the lamps blink quickly 5 times.

All the construction tests except the 7-segment one pull the INT pin low while an input is active, so you can probe it on slot pin 13. The 7-segment test pulses INT once during its automatic part.

---

## IDE settings

| Setting | Value |
|---|---|
| Board | ATtiny816 (megaTinyCore) |
| Clock | 10 MHz internal (20 MHz for the 1841 test, as before) |
| tinyNeoPixel Port | 1801 / 1811: **Port A**; 1831: **Port C**; 1841: as for GPWS / Pre-Warp |
| Programmer | jtag2updi or SerialUPDI |
| Libraries | 1801 / 1811: KerbalButtonCore 2.4+, ShiftIn; 1831: KerbalJoystickCore (config header only); 1841: Kerbal7SegmentCore; all: KerbalModuleCommon |

## Simulation

Each test runs on the host in `../tools/tester_sim/`. A simulated technician follows the firmware's timeline, works the inputs, and checks the LEDs, lamps, display and lever against the behaviour described above. The runs include fault cases: a dead button, a stuck or unconnected input, an off-centre stick, a dead 7-segment button, a reversed motor and no motor supply.

```
python3 run_sims.py ct1801 ct1811 ct1831 ct1841 ct1871
```
