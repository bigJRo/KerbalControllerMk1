# KCMk1 Hardware Inventory (working notes)

| | |
|---|---|
| **Document type** | Developer — working notes |
| **Status** | In progress: we are going through every board and panel together. This becomes the source for the Hardware Reference rewrite |
| **Source** | The schematics only. Where an older document disagrees, it is noted under *Differences* |

---

## 1. Boards confirmed

| Board | Schematic | PCB | Revision | Role |
|---|---|---|---|---|
| Ctrl Module | KC-01-1701 | KC-01-1702 | V2.0 | Master Teensy 4.1, power latch, INA228, EMC2101, I2C accelerators, USB hub |
| Ctrl Module Extension | KC-01-1711 | KC-01-1712 | V2.0 | Passive: splits the 50-pin interboard cable to the B side, fan, expansion terminal |
| Panel Hub | KC-01-1731 | KC-01-1732 | V2.0 | Input-panel hub: 8 module slots + safe switch |
| 7" TFT Display Driver | KC-01-1911 | KC-01-1912 | V2.1 | One 7" display. On each display panel one driver also acts as the hub: 3 module slots + safe switch |
| Button Module | KC-01-1801 | KC-01-1802 | V2.0 | 12 RGB buttons + 4 panel inputs |
| Wide Button Module | KC-01-1811 | KC-01-1812 | V2.0 | 12 RGB buttons + 8 panel inputs |
| Throttle Module | KC-01-1871 | KC-01-1872 | V2.0 | Motorised fader, touch sensor, 5 illuminated panel inputs |
| Joystick Module | KC-01-1831 | KC-01-1832 | V2.0 | 3-axis stick + stick button (terminal), 2 RGB buttons |
| 7-Segment Display Module | KC-01-1841 | KC-01-1842 | V2.0 | 4-digit display, encoder with push, 3 RGB buttons |
| 2" TFT Display Driver | KC-01-1901 | KC-01-1902 | V2.0 | XIAO RP2350, 2" 240×320 ST7789 IPS, microSD |
| Dual Encoder | KC-01-1861 | KC-01-1862 | V2.0 | Two encoder inputs + a safe-switch terminal |
| Encoder | KC-01-1851 | KC-01-1852 | V2.0 | Passive carrier for one PEC11R encoder with push |

**Retired:** Display Hub (KC-01-1721 / 1722). Its job is done by the 7" TFT Display Driver.

## 2. How the boards connect

```
Ctrl Module (KC-01-1701)
 ├─ CN3  34-pin ── A2 ── Panel Hub ............ INT_1–INT_8,  INT_SAFE
 ├─ CN4  34-pin ── A1 ── 7" Display Driver .... INT_9–INT_12, INT_SAFE2
 ├─ CN1  fan (PWM + tach)
 └─ CN2  50-pin ── Ctrl Module Extension (KC-01-1711)
                    ├─ CN4  34-pin ── B2 ── Panel Hub ........... B_INT_1–B_INT_8,  B_INT_SAFE
                    ├─ CN5  34-pin ── B1 ── 7" Display Driver ... B_INT_9–B_INT_12, B_INT_SAFE2
                    ├─ CN3  fan (PWM only)
                    └─ CN2  10-pin terminal ── expansion unit
```

The A and B sides are wired the same way: an 8-line connector for the input panel's Panel Hub, and a 4-line connector for the display panel's hub driver.

## 3. 34-pin panel connector

Both kinds of panel connector share one pinout. A display-driver connector carries only four of the eight interrupt lines.

| Pins | Signal |
|---|---|
| 1–8 | V_12 |
| 9–16, 20, 30–34 | GND |
| 17 / 18 | SDA_BUS / SCL_BUS |
| 19 | RST |
| 21–28 | INT_1 … INT_8 (display connector: 21–24 only; 25–28 not connected) |
| 29 | INT_SAFE |

**Line mapping:**

| Panel connector | Pins 21–24 | Pins 25–28 | Pin 29 |
|---|---|---|---|
| A2 (Ctrl CN3) | INT_1–4 | INT_5–8 | INT_SAFE |
| A1 (Ctrl CN4) | INT_9–12 | — | INT_SAFE2 |
| B2 (Ext CN4) | B_INT_1–4 | B_INT_5–8 | B_INT_SAFE |
| B1 (Ext CN5) | B_INT_9–12 | — | B_INT_SAFE2 |

**On a display driver** the four lines are used like this:
- **Pins 21–23:** its module slots P2, P3 and P4.
- **Pin 24:** the driver's own interrupt.

So on A1, INT_12 is the hub display, and INT_9–INT_11 are its slots.

## 4. Ctrl Module Extension (KC-01-1711 V2.0)

| Connector | What | Notes |
|---|---|---|
| CN1 | 50-pin interboard, to the Ctrl Module | Same pinout as Ctrl Module CN2 |
| CN4 | B2 panel connection (34-pin, 8 lines) | Silkscreen "B2 PANEL CONNECTION" |
| CN5 | B1 panel connection (34-pin, 4 lines) | Silkscreen "B1 PANEL CONNECTION" |
| CN3 | Fan, 4-pin | 1 FAN_PWM, 2 not connected, 3 V_12, 4 GND. No tach: only the Ctrl Module's fan reports RPM |
| CN2 | Expansion terminal, 10-pin (DB125-3.81) | 1–2 V_12, 3–4 GND, 5 EXT_INT_2, 6 EXT_INT_1, 7 RST, 8 SCL_EXT, 9 SDA_EXT, 10 not connected |

The board has no active components or protection; every signal passes straight through.

## 5. Module boards

### 5.1 Common to every ATtiny816 module

- **Slot connector P1 (16-pin):** pin 13 is INT, driven from the ATtiny through a 10 kΩ / 20 kΩ divider; pin 14 (RST) and pin 16 are not connected.
- **Power and I2C:** each board makes its own 5 V from the 12 V slot supply. An I2C level shifter sits between the 3.3 V bus and the 5 V ATtiny.
- **No address pins.** The I2C address and role are set by the firmware flashed (UPDI header H1). For example, UI Control and Action Control are the same board (1801) with different firmware.
- **Panel inputs** (screw terminals) are active-high. Each terminal has a +5 V pin, and each input a 10 kΩ pull-down: a switch from +5 V to the input reads 1 when closed.

### 5.2 Button Module (KC-01-1801 V2.0)

| Inputs | Where |
|---|---|
| B0–B11 | RGB buttons on the board (one NeoPixel chain) |
| B12, B13 | Terminal CN2 (4-pin): 1–2 +5 V, 3 B12, 4 B13 |
| B14, B15 | Terminal CN1 (4-pin): 1 B15, 2 B14, 3–4 +5 V |

### 5.3 Wide Button Module (KC-01-1811 V2.0)

| Inputs | Where |
|---|---|
| B0–B11 | RGB buttons on the board |
| B12–B15 | Not connected |
| B16–B19 | Terminal U16 (6-pin): 1–2 +5 V, 3 B19, 4 B18, 5 B17, 6 B16 |
| B20–B23 | Terminal U17 (6-pin): 1 B23, 2 B22, 3 B21, 4 B20, 5–6 +5 V |

### 5.4 Throttle Module (KC-01-1871 V2.0)

- **Fader:** motorised 10 kΩ slide pot (wiper to the ATtiny), L293D H-bridge on its own 9 V supply, AT42QT1010 touch sensor on the fader track, direction LEDs.
- **Panel inputs:** five 4-pin terminals, each pin 1 +5 V, pin 2 input, pin 3 +5 V (lamp), pin 4 lamp return:
  - U5: THRTL_ENA;
  - U7: THRTL_100;
  - U8: THRTL_UP;
  - U10: THRTL_DOWN;
  - U11: THRTL_00.
- **Lamps:** all five share one enable (LED_ENA).
- **THRTL_ENA** is not used by the module firmware ("not wired in the final build").

### 5.5 Joystick Module (KC-01-1831 V2.0)

- **On the board:** BTN01, BTN02 (RGB buttons).
- **Stick, on terminal U6 (6-pin):**

  | Pin | Signal |
  |---|---|
  | 6 | +5 V |
  | 5 | AXIS1 |
  | 4 | AXIS2 |
  | 3 | AXIS3 |
  | 2 | BUTTON_JOY |
  | 1 | GND |

### 5.6 7-Segment Display Module (KC-01-1841 V2.0)

- **Display:** 4-digit 7-segment (FJ4401AG), driven by a MAX7219.
- **Controls:** rotary encoder with push button; three buttons, each with an SK6812 LED.
- **Panel inputs:** none.

### 5.7 2" TFT Display Driver (KC-01-1901 V2.0)

- **Controller:** Seeed XIAO RP2350. The I2C bus and INT go to the XIAO directly at 3.3 V, with no divider or level shifter.
- **Display:** 2" 240×320 IPS (ST7789) on SPI, with backlight control.
- **Storage:** microSD card on the same SPI bus.
- **Slot connector P1:** pin 13 INT; pins 9 (3.3 V), 14 (RST) and 16 not connected. So the master's RST line cannot reboot it.
- **Power:** its own 12 V → 5 V → 3.3 V regulators.
- **No safe-switch terminal.**

### 5.8 Dual Encoder (KC-01-1861 V2.0) and Encoder carrier (KC-01-1851 V2.0)

**Dual Encoder:**
- **Encoder headers** H1 (ENC1) and H2 (ENC2), 5-pin, to an Encoder carrier each:

  | Pin | Signal |
  |---|---|
  | 1 | +5 V |
  | 2 | A |
  | 3 | B |
  | 4 | Button |
  | 5 | GND |

- **Safe-switch terminal U4:** the same 6-pin circuit as the hubs (active-high, 10 kΩ / 20 kΩ divider), but its output goes to slot pin 16 (SAFE_INT), **not** to the ATtiny. Every hub leaves slot pin 16 unconnected, so this switch cannot reach the master or the module.
- **No LEDs.**

**Encoder carrier:** PEC11R-4220F-S0024 with RC-filtered A / B (10 kΩ pull-ups, 10 nF), and the push button active-high with a 10 kΩ pull-down.

## 6. Differences from older documents

| # | Topic | Older document says | Schematic shows |
|---|---|---|---|
| 1 | ST7789 status display | Hardware Reference §5.1: on the Ctrl Module | None; SPI pins unconnected |
| 2 | EXT_INT protection | 47 Ω on the EXT_INT_2 output; pull-up on EXT_INT_1 | 47 Ω and pull-up both on EXT_INT_1; nothing on EXT_INT_2, on either board |
| 3 | Display Hub | KC-01-1722 for A1 / B1 | Retired; the 7" display driver is the hub (confirmed) |
| 4 | Safe switch on the 16-pin module slot | Hardware Reference §12: INT_SAFE on the slot | Slot pin 16 not connected; the safe switch has its own screw terminal. The "*Safe Switch Connection" notes beside slot P1 (Panel Hub) and P2 (display driver) are left over |
| 5 | Safe switch terminal V_EN | — | Pins 2, 3, 5 tied to a V_EN net that nothing drives |
| 6 | Fan headers | — | Pin order 1 PWM, 2 tach / NC, 3 12 V, 4 GND: the reverse of the usual PC fan numbering. Worth checking against the fans' cable |
| 7 | Throttle Module PCB number | Module UI Reference and the Hardware Reference change log: KC-01-1822 | KC-01-1872 (schematic 1871), as the Hardware Reference table says |
| 8 | Throttle panel inputs | Module UI Reference: four buttons | Five terminals; the fifth (THRTL_ENA) is not wired and unused by the firmware |
| 9 | Switch Group 1 order | Module UI Reference: B16 MSTR, B17 DISPL, B18 ENGINE, B19 THROTTLE, B20 SCE, B21 UPTLM, B22 LTG, B23 THRTL | B16 MSTR, B17 DISP, B18 SCE, B19 UPTLM, B20 LTG, B21 ENGINE, B22 THROTTLE, B23 THRTL (confirmed by the owner; §7.2) |
| 10 | 2" display safe switch | Hardware Reference §5.3: the 2.0" carrier "can carry a safe switch via screw terminal" | No terminal on KC-01-1901 V2.0 |
| 12 | Dual Encoder safe switch | Hardware Reference §12: INT_SAFE on the module connector for the Dual Encoder | Its terminal drives slot pin 16, which no hub connects |
| 13 | Staging | Module UI Reference: Stability B14 Stage, B15 Stage Enable | Stage button on the B2 Panel Hub safe line; Stability terminals unwired |
| 14 | Ctrl Mode Toggle | Module UI Reference open item 14: destination undecided; Time Control B12–B15 "not installed" | Time Control B12 / B13 |
| 11 | Ctrl Module ST7789 | — | The only ST7789 in the system is the 2" Sys Info Display on A2; the Hardware Reference's Ctrl Module entry probably refers to an earlier plan |

## 7. Panels

### 7.1 A1 — left display panel

| Item | Board | Connects to | Master sees it on |
|---|---|---|---|
| Information Display 1 (0x12) — **panel hub** | 7" TFT Display Driver (1911) | Its CN1 (34-pin) → Ctrl Module CN4 | INT_12 (its own line) |
| Annunciator (0x10) | 7" TFT Display Driver (1911) | Its P1 (16-pin) → Information Display 1's slot P2 | INT_9 |
| Power button | — | Ctrl Module H1 (power switch header, soft-latch circuit) | Nothing: the latch is analog and has no Teensy pin |

- **Free:** Information Display 1's slots P3 (INT_10) and P4 (INT_11).
- **Safe switch:** Information Display 1's terminal is empty, so INT_SAFE2 is unused.
- **The Annunciator's own terminal** goes nowhere. A driver plugged in through its 16-pin P1 has no safe-switch path, because P1 pin 16 is not connected.
- **No power-off from the master.** The master cannot switch the controller off or see the power button.

### 7.2 A2 — left input panel (Panel Hub on Ctrl Module CN3)

| Slot | Line | Module | Board | Address |
|---|---|---|---|---|
| P1 | INT_1 | Action Control | Button (1801) | 0x22 |
| P2 | INT_2 | Sys Info Display | 2.0" IPS carrier (1901) | 0x14 |
| P3 | INT_3 | Throttle | Throttle (1871) | 0x2C |
| P4 | INT_4 | UI Control | Button (1801) | 0x20 |
| P5 | INT_5 | — empty — | | |
| P6 | INT_6 | GPWS | 7-Segment (1841) | 0x2A |
| P7 | INT_7 | Translation joystick | Joystick (1831) | 0x29 |
| P8 | INT_8 | Function Control | Wide Button (1811) | 0x21 |

| Panel item | Wired to | Master sees it on |
|---|---|---|
| Abort button | Panel Hub safe-switch terminal | INT_SAFE (Teensy 31), active-high |
| Switch Group 1 (8 switches) | Function Control terminals U16 / U17 → B16–B23 (below) | Function Control's packet |
| Throttle buttons 100 %, UP, DOWN, 0 % | Throttle terminals U7, U8, U10, U11 | Throttle's packet |
| Translation stick | Joystick terminal U6 | Translation's packet |

**Unused:** slot P5; Action Control and UI Control terminals (B12–B15); Throttle THRTL_ENA (terminal U5).

**Switch Group 1** (Function Control, confirmed layout):

| Input | Terminal pin | Switch | Positions (off / on) | Type | Meaning |
|---|---|---|---|---|---|
| B16 | U16-6 | MSTR | OPER / RESET | Momentary | Master reset |
| B17 | U16-5 | DISP | OPER / CLR | Momentary | Display clear |
| B18 | U16-4 | SCE | NORM / AUX | Latching | Demo mode off / on |
| B19 | U16-3 | UPTLM | INHIB / XMIT | Latching | Debug telemetry |
| B20 | U17-4 | LTG | NORM / TEST | Momentary | Bulb test |
| B21 | U17-3 | A/P | INHIB / ARM | Latching | Autopilot master arm (§8) |
| B22 | U17-2 | ENGINE | SAFE / ARM | Latching | Throttle interlock (§8). Renamed from THROTTLE INOP / ACT |
| B23 | U17-1 | THRTL | STD / FINE | Latching | Throttle precision |

Each switch closes from a +5 V pin (U16-1/2, U17-5/6) to its input.

### 7.3 B1 — right display panel

| Item | Board | Connects to | Master sees it on |
|---|---|---|---|
| Resource Display (0x11) — **panel hub** | 7" TFT Display Driver (1911) | Its CN1 (34-pin) → Ctrl Module Extension CN5 | B_INT_12 (its own line) |
| Information Display 2 (0x13) | 7" TFT Display Driver (1911) | Its P1 → Resource Display slot P2 | B_INT_9 |
| Dual Encoder (0x2D) | Dual Encoder (1861) + 2 × Encoder (1851) | Its P1 → Resource Display slot P4 | B_INT_11 |

- **Free:** Resource Display slot P3 (B_INT_10).
- **Safe switches:** none wired, so B_INT_SAFE2 is unused. The Dual Encoder's own terminal could not reach the master anyway (§5.8).

### 7.4 B2 — right input panel (Panel Hub on Ctrl Module Extension CN4)

| Slot | Line | Module | Board | Address |
|---|---|---|---|---|
| P1 | B_INT_1 | — empty — | | |
| P2 | B_INT_2 | Pre-Warp Time | 7-Segment (1841) | 0x2B |
| P3 | B_INT_3 | Time Control | Button (1801) | 0x25 |
| P4 | B_INT_4 | Vehicle Control | Wide Button (1811) | 0x24 |
| P5 | B_INT_5 | — empty — | | |
| P6 | B_INT_6 | Stability Control | Button (1801) | 0x23 |
| P7 | B_INT_7 | AUX CTRL | Button (1801) | 0x26 |
| P8 | B_INT_8 | Rotation joystick | Joystick (1831) | 0x28 |

| Panel item | Wired to | Master sees it on |
|---|---|---|
| Stage button (momentary) + arming switch | Panel Hub safe-switch terminal, the arming switch in series (as Abort) | B_INT_SAFE (Teensy 35), active-high; a press only reaches the master while armed |
| Ctrl Mode selector SPC / PLN / RVR (3-position, PLN in the centre) | Time Control terminal CN2: B12 (pin 3), B13 (pin 4), +5 V (pins 1–2) | Time Control's packet: B12 = SPC, B13 = RVR, neither = PLN |
| Switch Group 2 (8 switches) | Vehicle Control terminals U16 / U17 → B16–B23 (below) | Vehicle Control's packet |
| Rotation stick | Joystick terminal U6 | Rotation's packet |

**Unused:**
- slots P1 and P5;
- Stability Control terminals (B12–B15), so the reference's Stage / Stage Enable on B14 / B15 are not wired;
- Time Control B14 / B15;
- AUX CTRL terminals.

**Switch Group 2** (Vehicle Control) — order confirmed by the owner (same as Module UI Reference v5.9):

| Input | Terminal pin | Switch | Off / on | Type | Reference action |
|---|---|---|---|---|---|
| B16 | U16-6 | CHUTE | SAFE / ARM | Latching | Arms the parachute buttons; SAFE resets them |
| B17 | U16-5 | GEAR | UP / DOWN | Latching | Gear up / down |
| B18 | U16-4 | BRAKE | REL / LOCK | Latching | Brakes off / on |
| B19 | U16-3 | EXT LT | OFF / ILLUM | Latching | Lights off / on |
| B20 | U17-4 | SAS | OFF / ENAB | Latching | SAS off / on |
| B21 | U17-3 | RCS | OFF / ENAB | Latching | RCS off / on |
| B22 | U17-2 | THC/RHC | NORM / PREC | Latching | Stick precision |
| B23 | U17-1 | AUDIO | MUTE / LIVE | Latching | Annunciator audio |

## 8. Behaviour decisions made during the review

These go into the firmware design once the panel review is finished.

### 8.1 ENGINE SAFE / ARM (Switch Group 1, B22)

- **SAFE:**
  - the master sends 0 % throttle to KSP and drives the lever to 0 %;
  - every throttle input is ignored: lever, throttle buttons, and autopilots;
  - the rover wheel throttle is held at 0 too (stick and autopilot); steering still works;
  - an autopilot that needs the throttle cannot engage, and one already engaged drops with the reason ENG SAFE;
  - if the pilot moves the lever while SAFE, it is driven back to 0 % on release.
- **ARM:**
  - the master drives the lever to 0 % and sends 0 % again;
  - once the lever is there, inputs are taken.
- **Annunciator:** ENG ARM shows the switch.
- **Hardware note:** the Throttle Module refuses lever commands while disabled or touched, so the module stays enabled in SAFE and the master re-sends the 0 % target until the lever reports it.

### 8.2 A/P INHIB / ARM (Switch Group 1, B21)

- **INHIB:** no autopilot can engage; moving the switch to INHIB disconnects every engaged autopilot (reason A/P INHIB).
- **ARM:** autopilots can be engaged from the consoles as before.
- **Annunciator:** two lights, both in mode row 0.
  - **A/P ARM** shows the switch: lit on ARM, dark on INHIB. It replaces THRTL ENA, keeping the same mode-flag bit (bit 3), so only the tile label changes in the Annunciator firmware, along with its references and the user manual.
  - **AUTOPILOT** (unchanged) is lit while any autopilot is engaged.

### 8.3 Autopilots and the physical lever

Whenever an autopilot owns the throttle, the master sends its value to KSP **and** drives the lever to the same position, so the lever always shows the throttle KSP is using. Today's `throttle_link` already does this (1 % deadband, at most one lever command per 200 ms); it carries over.

One gap, found in the module firmware: **in precision (FINE) mode the lever position does not equal the throttle.** The module maps the lever to a narrow band around the point where FINE was selected, but its lever command (`CMD_SET_THROTTLE`) positions the lever as if FINE were off. **Decided:** while an autopilot owns the throttle, the master takes the module out of precision mode, and restores it when the autopilot lets go.

### 8.4 Switch Group 2 and the matching buttons

**General rule for every switch:**
- A switch acts **when it is flipped**, never by itself.
- Entering a flight scene or changing vessel **does not enforce** any switch. If a switch disagrees with the vessel then, or at any time in flight, the Annunciator's **SWITCH ERR** lights until they agree again.

| Switch | Rule |
|---|---|
| **BRAKE** | Acts on its flip and on Vehicle Control's Brakes button (B0). LOCK: brakes on. REL: brakes off. The button applies the brakes while held (no longer a toggle). On release, the brakes come off only if the switch is REL; with the switch on LOCK they stay on |
| **GEAR**, **EXT LT** | Act on the flip. The buttons (B3, B1) still toggle |
| **SAS**, **RCS** | Act on the flip. SWITCH ERR is not raised for SAS while Invert (Stability B11, the F key) is held, nor for SAS or RCS while an autopilot that changes them is engaged |
| **CHUTE**, **THC/RHC**, **AUDIO** | As the reference |

### 8.5 Ctrl Mode selector

- **Decoding:** B12 = SPC, neither = PLN (centre), B13 = RVR.
- **B12 and B13 both on** cannot happen on a healthy switch. It is reported as a **debug message**, not SWITCH ERR (the panel would show no visible discrepancy, and the light alone would not say what is wrong). While it lasts, the last good mode is kept.

### 8.6 Stage button

- A rising edge sends STAGE, only in a flight scene, with a 1 s lockout so a bounce or double press cannot fire two stages.
- The arming switch is wired in series, the same way as the Abort button, so a press only reaches the master while armed. The master cannot see the arming switch itself.

### 8.7 HOLD (Stability B10)

HOLD is a temporary hold of the current condition. It is *not* gated by A/P ARM, and A/P INHIB does not drop it.

| Control mode | Holds | How |
|---|---|---|
| SPC | Attitude | KSP SAS, Stability Assist |
| PLN | Altitude + heading | Hold autopilot ALT + HDG at the current values; throttle untouched |
| RVR | Heading + speed | Hold autopilot RHDG + CRUISE at the current values. With ENGINE SAFE, heading only (wheel throttle held at 0) |

- **Released by:** a second press, the A/P disconnect (rotation stick button), or a console autopilot engaging. Stick input does not release it (§8.10).
- **LED:** ACTIVE while holding.
- **Annunciator:** AUTOPILOT lights while HOLD is on.

The candidates considered for B10 were: HOLD, controller pointing layer, kill rotation, navball reference, custom action group, SAS mode guard, KSP fine control, RCS hold, A/P DISC, Info Display shortcut.

**Kill rotation was dropped:** with Simpit reporting no angular velocity, it could only engage SAS Stability Assist, which is not useful enough. The AUX CTRL review (§10.1) then removed the docking-port control point altogether.

### 8.8 Demo mode (Switch Group 1 SCE: NORM = off, AUX = on)

**Purpose:** showing the controller off, and bench testing.

**Allowed:** any time the switch is set.

**Nothing reaches KSP.** While in demo, the master sends no Simpit message of any kind, and nothing depends on a Simpit connection: every "is KSP connected / in a flight scene" check is bypassed. Engaged autopilots and HOLD are dropped on entry.

**Panels respond locally:**
- **Buttons:** every one sits in its default backlit state, and shows its ACTIVE colour while pressed. There is no self-running light pattern in any state.
- **Switches** show their positions; for example, CHUTE ARM enables the parachute buttons' local response.
- **7-segment modules and joysticks** respond locally (the 7-segments are module-managed anyway).
- **Modules** are ENABLEd for the duration, whatever the flight scene.

**Displays:** all four carriers get the demo flag and animate demo data. *Every display follows the master's flag and nothing else:* it enters and leaves demo only when the flag changes. Today the Info Displays clear demo on a scene change, and the Resource Display stops servicing Simpit; both are brought in line (§11).

**Annunciator:** DEMO lit.

**Leaving demo:**
- Modules return to the lifecycle for the current scene.
- LEDs return to KSP's state.
- Switches flipped during demo are not acted on; the usual rule applies, so a switch that now disagrees with the vessel lights SWITCH ERR.

### 8.9 Debug mode (Switch Group 1 UPTLM: INHIB = off, XMIT = on)

**Where:** serial ports only.
- **The master** writes to its USB serial (`Serial`; KSP's link is `SerialUSB1`).
- **The debug flag** also switches on every display's own serial debug.
- **The Sys Info Display shows no debug;** what it shows is a separate discussion (§9).

**Annunciator:** DEBUG lit.

**What the master reports:**

| # | Category | Reported |
|---|---|---|
| 1 | Modules | Found / missing, type mismatch, re-probe results |
| 2 | INT learning | The line learned for each module; differences from the stored / default map |
| 3 | Module health | Fault bits, missed packets (transaction-counter gaps) |
| 4 | Switches | The detail behind each SWITCH ERR; switch faults such as both mode-selector inputs on |
| 5 | KSP link | Connect / disconnect, scene and vessel changes |
| 6 | Autopilots | Engage / disengage with the reason; HOLD |
| 7 | INT traffic | Each INT received and the packet that followed (decoded) |
| 8 | Master state | Every change of a state / config variable (control mode, control group, precision, audio, demo, A/P ARM, ENGINE ARM, trim, …) |
| 9 | Timing | Loop time (average / worst per second) |
| 10–17 | Additions | Below |

**Additions (confirmed):**
- **10 — Commands to KSP:** every Simpit message sent, decoded. Axis and throttle messages are logged on change rather than every frame.
- **11 — KSP inbound health:** messages per channel, and time since the last one on each.
- **12 — I2C errors:** NACKs, short reads, bus timeouts, bus recovery.
- **13 — Display carriers:** commands sent (on change); console command / ack traffic.
- **14 — Ownership:** throttle and attitude owner changes, lever override, ENGINE SAFE / ARM, lever sync after ARM.
- **15 — Board:** boot banner (firmware version, reset cause including watchdog), supply voltage / current, temperature, fan.
- **16 — Lifecycle:** every ENABLE / DISABLE / RESET sent to a module.
- **17 — Keyboard events** sent.

**Format and limits (proposed):**
- One line per event: timestamp, category tag, message, so a log can be filtered.
- Errors are always printed; the rest only while XMIT.
- Rate-limited, with a "*n* suppressed" line, so a flood cannot stall the loop.
- **Serial command set (confirmed)** for bench work: `status`, `modules`, `intmap`, `simpit` (exact list in the design).
- **Stick positions** are logged while in demo or debug.

**Input test screen (proposed, *to confirm*).** A hidden diagnostics screen on Information Display 1, opened by a gesture, showing what the master reads:
- both sticks' axes and buttons;
- the throttle lever;
- every module's buttons and switches;
- the mode selector and safe lines;
- module presence and INT lines.

The master pushes it a new frame while the screen is open (new display firmware and a new I2C frame, §11).

### 8.10 Autopilot authority (replaces the pilot-override rule)

**Stick input no longer disconnects autopilots.** An autopilot (and HOLD) stays engaged until it is disconnected deliberately, by:
- the A/P disconnect on the rotation stick button (SPC / PLN);
- A/P INHIB (Switch Group 1);
- the consoles' A/P OFF;
- HOLD's own button;
- ENGINE SAFE, for anything that needs the throttle;
- the autopilot's own disengage conditions (no node, SOI change, and so on).

**While engaged:**
- **Sticks:** ignored completely (every axis and the translation stick), whatever the autopilot holds. Only the A/P disconnect gives control back. Stick buttons keep working.
- **Throttle:** the autopilot always sets it. Lever movement, the lever buttons and touch are ignored, and KSP keeps the autopilot's value. The motor pushes back against a hand for at most about 0.5 s (stall protection), then returns the lever to the autopilot's position as soon as it is released (decided).

**What changes elsewhere:**
- The "any pilot input disconnects every autopilot" rule in `Hold_Mode_Autopilot.md`, `Mission_Autopilot.md` and today's `throttle_link` / `rotation_link` is retired. The STICK / LEVER disconnect reasons go unused.
- The Throttle Module firmware refuses lever commands while touched (`CMD_SET_THROTTLE` checks `wiperIsTouched()`), so holding the lever against the pilot needs a module firmware change.
- A motorised fader driven against a hand stalls its motor (L293D, 9 V). The hold needs a stall limit, for example: drive back to position whenever the hand lets go, and push against the hand for at most a short time.

### 8.11 Input test screen (Information Display 1)

- **Where:** a hidden screen on Information Display 1.
- **Opened and closed by:** a two-finger diagonal pull down from the top-right corner.
- **Available:** any time.
- **Shows:** what the master reads (§8.9): sticks, lever, every module's buttons and switches, the mode selector, the safe lines, module presence and INT lines.
- **Data:** the master pushes it a frame while the screen is open.
- **Note:** today every display ignores any touch with two or more contacts (single-finger filter). The FT5316 reports up to five points, so the gesture is possible, but it needs its own handling ahead of that filter.
- **Detailed design:** a later discussion.

### 8.12 Sys Info Display (0x14, A2 P2)

**Screens:**

| Screen | When | Content |
|---|---|---|
| **Boot** | From power-on **until KSP connects or demo mode is switched on** | One line per step as it happens: configuring items, each module found and enabled (or missing), each display released, connections established, waiting for KSP, any fault. If a step stalls, the screen stays on it, so the stalled step is visible |
| **Controller status** | Normal operation | 12 V voltage, current and power; board temperature; fan speed; the KSP link state of **each** microcontroller with its own link (master, Annunciator, Information Displays 1 and 2, Resource Display) |
| **SWITCH ERR window** | While SWITCH ERR is lit | Sits in front of the status screen and names each switch in error (switch, expected vs actual). It goes away when every error clears |

**Boot hold and stalls (proposed, *to confirm*):**
- **The master does not block.** `setup()` finishes, and the panels, displays and switches work as usual. Only the Sys Info Display stays on the boot screen, with a "Waiting for KSP" line (elapsed time, retry count), until the master's Simpit link connects or SCE is set to AUX (demo).
- **Every boot step has a timeout.** A step that times out is shown in red with what it was waiting on, and boot carries on (missing module, display not answering, sensor missing).
- **Waiting for KSP has no timeout,** but after 60 s a hint line is added (KSP running? Simpit COM port? USB cable?).
- **A master that hangs cannot report itself,** so the Sys Info Display has its own timeout. If it hears nothing from the master for 5 s during boot, it shows *MASTER NOT RESPONDING* with the last step received.

**Graphics:** small icons and logos (fan, power, thermometer, link, …) stored on its microSD card; set to be chosen.

**What this needs:**
- **The master** sends boot messages *from the start of `setup()`*. It waits briefly for the Sys Info Display to answer first, and mirrors every line to Serial. Every I2C call during setup has a timeout, so a stuck bus becomes a visible message rather than a hang.
- **The watchdog** starts only after `setup()`, so it does not reset a stalled boot out of view.
- **Each display** reports its own Simpit link state to the master (display firmware change, §11).
- **The I2C protocol for 0x14** is defined with the firmware design (Q6): boot line, status frame, switch-error list.
- **Note:** the Sys Info Display's RST pin is not connected (§5.7), so the master cannot hard-reset it.

## 9. Future discussion points

| # | Topic | Status |
|---|---|---|
| 1 | Demo and debug modes | Settled (§8.8, §8.9) |
| 2 | Stability Control B10 | Settled: HOLD (§8.7) |
| 3 | Function Control B4 | Settled: FLAPS (§10.5) |
| 4 | AUX CTRL B8 | Settled: UNDOCK (§10.1) |
| 5 | Rotation stick button | Settled: by control mode (§10.8) |
| 6 | Sys Info Display | Settled (§8.12); protocol defined with the firmware design |
| 7 | Input test screen: detailed design | Open (§8.11) |
| 8 | Sys Info Display icons / logos | Open (§8.12) |

## 10. Button module review

The Module UI Reference and the module firmware agree with each other, but not with the built panels: for example, both put Cruise Control on AUX CTRL B8, which the panel does not have. Each button module is reviewed against the physical panel labels, one at a time:

| # | Module | Status |
|---|---|---|
| 1 | AUX CTRL (0x26) | Done (§10.1) |
| 2 | Stability Control (0x23) | Done (§10.2) |
| 3 | Vehicle Control (0x24) | Done (§10.3) |
| 4 | Time Control (0x25) | Done (§10.4) |
| 5 | Function Control (0x21) | Done (§10.5) |
| 6 | Action Control (0x22) | Done (§10.6) |
| 7 | UI Control (0x20) | Done (§10.7) |
| 8 | Joystick, 7-segment and Dual Encoder buttons | Done (§10.8) |

**Numbering:** Bn is the module firmware's button index, the same as the schematic net BUTTONnn. It matches the silkscreen: SW12–SW15 on the Button Module, B16–B23 on the Wide Button Module.

**Layout:** confirmed against the panels, viewed from the front.

```
Button Module (1801), 2 × 6            Wide Button Module (1811), 6 × 2
      C6   C5   C4   C3   C2   C1             C1    C2
 R1   B10  B8   B6   B4   B2   B0        R1   B1    B0
 R2   B11  B9   B7   B5   B3   B1        R2   B3    B2
                                         R3   B5    B4
                                         R4   B7    B6
                                         R5   B9    B8
                                         R6   B11   B10
```

Labels: **[GROUP]** is a group label printed on the panel, not on the button; CAPITALS are the exact button label; (…) is the owner's description of the function.

### 10.1 AUX CTRL (0x26) — confirmed

| Input | Label | Function | Active when | Action |
|---|---|---|---|---|
| B0 | [EVA] EVA LIGHTS | EVA lights | EVA | Key U |
| B1 | [EVA] JET PACK | Jetpack | EVA | Key R |
| B2 | [EVA] BOARD | Board craft | EVA | Key B |
| B3 | [EVA] CONST | EVA construction | EVA | Key I |
| B4 | [EVA] JUMP / LET GO | Jump / let go | EVA | Key Space |
| B5 | [EVA] GRAB | Grab | EVA | Key F |
| B6 | [EVA] EVA CHUTE | EVA parachute | EVA | Key P |
| B7 | [EVA] HELM | Helmet toggle | EVA | Key O |
| B8 | **UNDOCK** (new label) | Undock / decouple docking ports | Always | CAG 37 (the freed docking-port group) |
| B9 | [EVA] FLAG | Plant flag | EVA | Key G |
| B10 | [CTRL PT] PRI | Primary control point | Always | CAG 35 |
| B11 | [CTRL PT] ALT | Alternate control point | Always | CAG 36 |

**Changes from the reference / firmware:**
- B8 Cruise Control is removed; B8 becomes UNDOCK.
- B10 is no longer a PRI / ALT toggle; it is PRI only.
- B11 is ALT (was Docking Port, CAG 37). CAG 37 moves to UNDOCK on B8.

**Behaviour:**
- **EVA group (B0–B7, B9):** dark outside EVA, and presses ignored.
- **PRI / ALT:** pressing one switches to that control point. It deactivates the other's CAG first, then activates its own, and its LED stays lit to show which is active. Pressing the one already active does nothing.

### 10.2 Stability Control (0x23) — confirmed

| Input | Label | Action |
|---|---|---|
| B0 | [SAS] TARGET | SAS mode Target |
| B1 | [SAS] ANTI-TARGET | SAS mode Anti-Target |
| B2 | [SAS] RADIAL IN | SAS mode Radial In |
| B3 | [SAS] RADIAL OUT | SAS mode Radial Out |
| B4 | [SAS] NORMAL | SAS mode Normal |
| B5 | [SAS] ANTI-NORMAL | SAS mode Anti-Normal |
| B6 | [SAS] PROGRADE | SAS mode Prograde |
| B7 | [SAS] RETROGRADE | SAS mode Retrograde |
| B8 | [SAS] STABILITY ASSIST | SAS mode Stability Assist |
| B9 | [SAS] MANEUVER | SAS mode Maneuver |
| B10 | HOLD | HOLD (§8.7) |
| B11 | INVERT | Holds F while held |
| B12–B15 | — | Not connected |

**Changes from the reference:** B10 is HOLD (was RCS). B14 / B15 Stage / Stage Enable are gone: staging is the B2 stage button (§8.6).

**SAS buttons (B0–B9):**
- **SAS switch OFF:** all ten are dark, and presses have no effect.
- **SAS switch ENAB:** the mode KSP reports is lit, and the other modes are backlit. A mode the vessel cannot use is dark and its press is ignored.
- **Mode availability comes from Simpit.** `SAS_MODE_INFO` carries the current mode (255 = SAS off) and a bitmask of the available modes (`SASModeAvailability`, bit = `1 << AutopilotMode`). The library notes that KSP ignores a mode it does not allow. *To verify on the bench* that the bitmask tracks pilot / probe level as expected.
- **Button order vs KSP's mode numbers:**

  | Button | AutopilotMode |
  |---|---|
  | B0 | Target = 7 |
  | B1 | Anti-Target = 8 |
  | B2 | Radial In = 5 |
  | B3 | Radial Out = 6 |
  | B4 | Normal = 3 |
  | B5 | Anti-Normal = 4 |
  | B6 | Prograde = 1 |
  | B7 | Retrograde = 2 |
  | B8 | Stability Assist = 0 |
  | B9 | Maneuver = 9 |

### 10.3 Vehicle Control (0x24) — confirmed

| Input | Label | Action |
|---|---|---|
| B0 | BRAKES | Brakes while held; the BRAKE switch dominates (§8.4) |
| B1 | LIGHTS | Lights toggle |
| B2 | ANTENNA | CAG 13 |
| B3 | LANDING GEAR | Gear toggle |
| B4 | FUEL CELL | CAG 14 |
| B5 | SOLAR ARRAY | CAG 15 |
| B6 | CARGO DOOR | CAG 16 |
| B7 | RADIATOR | CAG 17 |
| B8 | HEAT SHIELD | State machine: deploy CAG 19, then release CAG 20 |
| B9 | LADDER | CAG 18 |
| B10 | [PARACHUTE] MAIN | State machine: deploy CAG 21, then cut CAG 22; needs CHUTE ARM |
| B11 | [PARACHUTE] DROGUE | State machine: deploy CAG 23, then cut CAG 24; needs CHUTE ARM |
| B12–B15 | — | Not connected |
| B16–B23 | Switch Group 2 | §7.4, §8.4 |

Matches the reference; the only change is BRAKES (hold, §8.4).

**LEDs:**
- **CAG buttons, LIGHTS, LANDING GEAR:** lit while KSP reports the group (or lights / gear) active, backlit otherwise.
- **[PARACHUTE] MAIN / DROGUE:** dark, and presses ignored, while CHUTE is SAFE.

**State machines — decided:** the state comes from KSP's report of the two action groups, not from memory in the master.

| Deploy CAG | Cut / release CAG | State | LED | Press does |
|---|---|---|---|---|
| inactive | inactive | READY | Backlit | Activate the deploy CAG |
| active | inactive | DEPLOYED | Green (ACTIVE) | Activate the cut / release CAG |
| either | active | CUT / RELEASED | Red (CUT) | Nothing |

- **Activate, not toggle:** presses send *activate* (`CAGACTIVATE`), never a toggle. A press can never undo a deploy or switch a group off.
- **No reset needed:**
  - On a vessel switch or a new flight, the buttons show that vessel's groups as KSP reports them.
  - CHUTE SAFE → ARM no longer resets anything; a cut chute stays red.
- **Limit (accepted):** a group being active means it was fired, not that the part is deployed. For example, a chute waiting for safe conditions, or one destroyed, still shows DEPLOYED.
- **Reset:** holding the button for 2 s deactivates both of its groups, returning it to READY (e.g. after a repack). For the chutes this needs CHUTE ARM.
- **Acts on release:** these three buttons act when released, not when pressed. A release before 2 s is a press, as in the table; a 2 s hold is a reset and fires nothing else.

### 10.4 Time Control (0x25) — confirmed

| Input | Label | Action |
|---|---|---|
| B0 | [WARP TO] ApA | Warp to apoapsis, less the Pre-Warp lead |
| B1 | [TIME WARP] STOP | Cancel warp (including a warp-to), back to 1× |
| B2 | [WARP TO] PeA | Warp to periapsis, less the lead |
| B3 | [TIME WARP] PHYS | Held as a modifier: PHYS + > / < steps physics warp 1× – 4× |
| B4 | [WARP TO] MNVR | Warp to the **burn start** of the next node (Simpit `TIMEWARP_TO_NEXT_BURN`, KSP's burn-time estimate), less the lead |
| B5 | [TIME WARP] > | Warp faster |
| B6 | [WARP TO] SOI | Warp to the next SOI change, less the lead |
| B7 | [TIME WARP] < | Warp slower |
| B8 | [WARP TO] MORN | Warp to the next morning, less the lead |
| B9 | LOAD | Quick load: F9 held for as long as LOAD is held (KSP needs F9 held) |
| B10 | PAUSE | Pause menu (Esc) |
| B11 | SAVE | Quick save (F5) |
| B12 | Mode selector SPC | §8.5 |
| B13 | Mode selector RVR | §8.5 |
| B14–B15 | — | Not connected |

Matches the reference.

**WARP TO LEDs:** a button is dark, and its press ignored, when its event does not exist. What Simpit reports (to be checked on the bench):

| Button | Available when | From |
|---|---|---|
| ApA | Orbit closed (eccentricity < 1), not landed | `ORBIT`, `APSIDESTIME`, `FLIGHT_STATUS` |
| PeA | Not landed | `APSIDESTIME`, `FLIGHT_STATUS` |
| MNVR | A node exists (Δv of the next node > 0) | `MANEUVER` |
| SOI | In flight and outside the atmosphere. Simpit gives no time to the next SOI change, and an encounter can come from an elliptical orbit (e.g. a Mun transfer), so the button cannot know whether one is ahead; KSP ignores the press when there is none | `FLIGHT_STATUS`, `ATMO_CONDITIONS` |
| MORN | Landed or splashed | `FLIGHT_STATUS` |

### 10.5 Function Control (0x21) — confirmed

| Input | Label | Function | Base CAG |
|---|---|---|---|
| B0 | FAIRING JETT | Jettison fairing | 26 |
| B1 | LES | Launch escape system | 25 |
| B2 | LOCK SURFACES | Lock control surfaces | 34 |
| B3 | AIR INTAKE | Air intakes | 33 |
| B4 | RW DISABLE → **FLAPS** | Flaps (replaces reaction wheel disable; panel label to change) | 39 |
| B5 | AIR BRAKE | Air brakes | 38 |
| B6 | [SCIENCE] COLLECT | Science collect | 28 |
| B7 | [ENGINE] ALT | Alternate engine mode | 27 |
| B8 | [SCIENCE] GROUP 1 | Science group 1 | 30 |
| B9 | [ENGINE] GROUP 1 | Engine group 1 | 29 |
| B10 | [SCIENCE] GROUP 2 | Science group 2 | 32 |
| B11 | [ENGINE] GROUP 2 | Engine group 2 | 31 |
| B12–B15 | — | Not connected | |
| B16–B23 | Switch Group 1 | §7.2 | |

- **Changes from the reference:** every left / right pair is swapped (e.g. the reference had LES on B0 and Fairing on B1).
- **CAG numbers stay with the function**, so vessels' action-group setups are unchanged.
- **Module firmware:** its colour table follows the button index, so it needs the same swap.

### 10.6 Action Control (0x22) — confirmed

| Input | Label | Group | | Input | Label | Group |
|---|---|---|---|---|---|---|
| B0 | AG 6 | CAG 6 | | B1 | AG 12 | CAG 12 |
| B2 | AG 5 | CAG 5 | | B3 | AG 11 | CAG 11 |
| B4 | AG 4 | CAG 4 | | B5 | AG 10 | CAG 10 |
| B6 | AG 3 | CAG 3 | | B7 | AG 9 | CAG 9 |
| B8 | AG 2 | CAG 2 | | B9 | AG 8 | CAG 8 |
| B10 | AG 1 | CAG 1 | | B11 | AG 7 | CAG 7 |

- B12–B15: not connected.
- Matches the reference, except that **AG 1–12 are not offset by the control group**: these buttons always send CAG 1–12 (§10.8).

### 10.7 UI Control (0x20) — confirmed

| Input | Label | Function | Action |
|---|---|---|---|
| B0 | SCREEN SHOT | Screenshot without the UI | Macro: F2, F1, F2 |
| B1 | DEBUG | KSP debug console | Alt+F12 |
| B2 | UI | Toggle UI | F2 |
| B3 | NAV BALL | Toggle the navball | Numpad `.` |
| B4 | [MAP] RESET | Map view reset | `'` |
| B5 | CYCLE NAV | Cycle navball mode | Simpit navball mode message |
| B6 | [MAP] > | Map focus forward | Tab |
| B7 | [SHIP] > | Next vessel | `]` |
| B8 | [MAP] < | Map focus back | Shift+Tab |
| B9 | [SHIP] < | Previous vessel | `[` |
| B10 | [MAP] ENA | Map view on / off | M |
| B11 | IVA | IVA view | C |
| B12–B15 | — | Not connected | |

- **Matches the reference.**
- **LEDs:** backlit; ACTIVE for 150 ms on a press. KSP reports none of these states (map view, UI hidden, navball mode).

### 10.8 Joystick, 7-segment and Dual Encoder controls — confirmed

**Rotation joystick (0x28):**

| Control | Function |
|---|---|
| BTN01 | Clear trim: clears the master's trim set by BTN02 (*not* KSP's Alt+X) |
| BTN02 | Set trim: captures the current stick deflection as a trim offset, added to the stick from then on |
| Stick button | By control mode: **SPC and PLN** — A/P disconnect (every autopilot and HOLD); **RVR** — brakes while held (same rule as the BRAKES button: with BRAKE on LOCK the brakes stay on); **EVA** — Left Shift (run) while held. Airbrake stays on Function Control B5 only |

- **Annunciator TRIM** lights while a trim offset is set.
- **The trim clears on a control-mode change** (SPC / PLN / RVR).

**Translation joystick (0x29):**

| Control | Function |
|---|---|
| BTN01 | Cycle camera mode (auto / free / orbital / chase) |
| BTN02 | Camera reset |
| Stick button | Held: the stick moves the camera instead of translating |

**GPWS (0x2A)** — module-managed; the master relays its state and threshold to the Annunciator:

| Control | Function |
|---|---|
| Display + encoder | Altitude threshold 0–9999 m |
| Encoder button | Reset threshold to 200 m |
| BTN01 | OFF → full GPWS → proximity tone only |
| BTN02 | Proximity alarm on / off |
| BTN03 | Rendezvous radar on / off |

**Pre-Warp (0x2B)** — module-managed; the master reads the minutes:

| Control | Function |
|---|---|
| Display + encoder | Lead time 0–9999 minutes |
| Encoder button | Reset to 0 |
| BTN01 | Preset 5 min |
| BTN02 | Preset 1 hour |
| BTN03 | Preset **1 Kerbin day = 6 h = 360 min**. The module firmware has 1440, an Earth day |

**Dual Encoder (0x2D):**

| Control | Function |
|---|---|
| ENC1 | Control group 1–6, wraps |
| ENC1 button | Reset the control group to 1 |
| ENC2 | Next / previous external camera (`-` / `=`, Hullcam VDS) |
| ENC2 button | Camera reset (Backspace, Hullcam VDS) |

**Control group offset:** every CAG button is offset by the control group (`CAG = base + (group − 1) × 40`), **except Action Control's AG 1–12**, which always send CAG 1–12. The offset therefore applies to:
- Vehicle Control, including the heat-shield and parachute state machines;
- Function Control;
- AUX CTRL PRI / ALT;

## 11. Changes needed outside the master firmware

Collected from this review; each is a separate piece of work.

| # | Where | Change |
|---|---|---|
| 1 | Annunciator firmware | Tile label THRTL ENA → A/P ARM (§8.2) |
| 2 | Annunciator firmware | **Touch the DOCK tile to toggle it.** Docking is detected from Simpit's vessel-change events (dock / undock), which are missed, for example after switching vessel, so the pilot needs a way to correct the light. A later real dock / undock event overrides the touch setting |
| 3 | Pre-Warp module firmware | BTN03 preset 1440 → 360 min (1 Kerbin day) |
| 4 | Function Control module firmware | Colour table swapped to the confirmed layout (§10.5) |
| 5 | AUX CTRL module firmware | B8 cleared (no Cruise); B10 PRI, B11 ALT colours (§10.1) |
| 6 | Stability Control module firmware | B10 HOLD colour (was RCS) |
| 7 | Module UI Reference | Rewritten from this inventory: Switch Groups 1 / 2, every button map, mode selector, staging, safe switches, PCB numbers |
| 8 | Hardware Reference | Rewritten from this inventory (§6 lists the differences) |
| 9 | Annunciator reference and user manual | A/P ARM tile, DOCK touch |
| 13 | Panel labels | Function B4 RW DISABLE → FLAPS; AUX B8 new label UNDOCK |
| 14 | Info Display 1 firmware + I2C spec | Input test screen, its two-finger gesture and its frame (§8.11) |
| 15 | Throttle Module firmware | Hold the lever at the autopilot's position against the pilot, with a stall limit (§8.10) |
| 16 | Annunciator, Info Display, Resource Display firmware | Report their own Simpit link state to the master (§8.12) |
| 17 | Sys Info Display firmware | New: boot, status and SWITCH ERR screens; microSD icons (§8.12) |
| 18 | Autopilot design docs | Retire the pilot-input disconnect rule (§8.10) |
| 10 | Info Display firmware | Demo follows the master's flag only: stop clearing it on a scene change (§8.8) |
| 11 | Resource Display firmware | Demo follows the master's flag; same entry / exit rule as the other displays (§8.8) |
| 12 | Annunciator firmware | Check its demo handling against the same rule (§8.8) |

