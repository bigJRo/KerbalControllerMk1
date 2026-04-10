# Kerbal Controller Mk1 — Module UI Reference

**Jeb's Controller Works** | v2.0 | 2026-04-09

This document covers every user-facing control and indicator on each module. Arranged by module in I2C address order. Panel layouts use the physical orientation as seen from the front of the panel.

---

## LED State Color Key

All NeoPixel buttons share a common set of LED states. Not all states are used on every module.

| State | Appearance | Meaning |
|---|---|---|
| OFF | Dark | Inactive / not installed |
| ENABLED | Dim white | Module active, function available |
| ACTIVE | Full color (per button) | Function engaged or selected |
| WARNING | Flashing amber (500ms) | Attention required |
| ALERT | Flashing red (150ms) | Immediate action required |
| ARMED | Static cyan | Primed and ready |
| PARTIAL_DEPLOY | Static amber | Partially deployed state |

Discrete LED buttons (toggle-type) are ON/OFF only — ENABLED and ACTIVE both produce full brightness. Flash states have no effect on discrete LEDs.

---

## 0x20 — UI Control Module

**PCB:** KC-01-1822 | **Type:** 0x01 | **Extended States:** No

12 NeoPixel buttons in a 2×6 grid. No discrete LED buttons installed.

### Panel Layout

```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│ Map      │ Map      │ Map      │ Map      │ UI       │Screenshot│
│ Enable   │ Back     │ Forward  │ Reset    │ Toggle   │          │
│ SKY      │ SKY      │ SKY      │ SKY      │ GREEN    │ GREEN    │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────┤
│ IVA      │ Ship     │ Ship     │ Cycle    │ Nav Ball │ Debug    │
│          │ Back     │ Forward  │ Nav      │ Toggle   │          │
│ CORAL    │ TEAL     │ TEAL     │ GREEN    │ AMBER    │ RED      │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
  Col 6(L)   Col 5      Col 4      Col 3      Col 2      Col 1(R)
```

### Button Reference

| KBC Index | PCB Label | Function | ACTIVE Color |
|---|---|---|---|
| 0 | BUTTON01 | Screenshot | GREEN |
| 1 | BUTTON02 | Debug | RED |
| 2 | BUTTON03 | UI Toggle | GREEN |
| 3 | BUTTON04 | Nav Ball Toggle | AMBER |
| 4 | BUTTON05 | Map Reset | SKY |
| 5 | BUTTON06 | Cycle Nav | GREEN |
| 6 | BUTTON07 | Map Forward | SKY |
| 7 | BUTTON08 | Ship Forward | TEAL |
| 8 | BUTTON09 | Map Back | SKY |
| 9 | BUTTON10 | Ship Back | TEAL |
| 10 | BUTTON11 | Map Enable | SKY |
| 11 | BUTTON12 | IVA | CORAL |
| 12–15 | BUTTON13–16 | Not installed | — |

---

## 0x21 — Function Control Module

**PCB:** KC-01-1822 | **Type:** 0x02 | **Extended States:** No

12 NeoPixel buttons in a 6×2 grid. No discrete LED buttons installed.

### Panel Layout

```
┌──────────────────┬──────────────────┐
│ Engine Alt Mode  │ Science Collect  │
│ ORANGE           │ PURPLE           │
├──────────────────┼──────────────────┤
│ Engine Group 1   │ Science Group 1  │
│ YELLOW           │ INDIGO           │
├──────────────────┼──────────────────┤
│ Engine Group 2   │ Science Group 2  │
│ CHARTREUSE       │ BLUE             │
├──────────────────┼──────────────────┤
│ LES              │ Air Intake       │
│ RED              │ TEAL             │
├──────────────────┼──────────────────┤
│ Lock Surfaces    │ Air Brake        │
│ AMBER            │ CYAN             │
├──────────────────┼──────────────────┤
│ Heat Shield      │ Heat Shield      │
│ Deploy           │ Release          │
│ GREEN            │ RED              │
└──────────────────┴──────────────────┘
  Col 1 (left)       Col 2 (right)
```

### Button Reference

| KBC Index | PCB Label | Function | ACTIVE Color |
|---|---|---|---|
| 0 | BUTTON01 | Science Collect | PURPLE |
| 1 | BUTTON02 | Engine Alt Mode | ORANGE |
| 2 | BUTTON03 | Science Group 1 | INDIGO |
| 3 | BUTTON04 | Engine Group 1 | YELLOW |
| 4 | BUTTON05 | Science Group 2 | BLUE |
| 5 | BUTTON06 | Engine Group 2 | CHARTREUSE |
| 6 | BUTTON07 | Air Intake | TEAL |
| 7 | BUTTON08 | LES | RED |
| 8 | BUTTON09 | Air Brake | CYAN |
| 9 | BUTTON10 | Lock Surfaces | AMBER |
| 10 | BUTTON11 | Heat Shield Release | RED |
| 11 | BUTTON12 | Heat Shield Deploy | GREEN |
| 12–15 | BUTTON13–16 | Not installed | — |

---

## 0x22 — Action Control Module

**PCB:** KC-01-1822 | **Type:** 0x03 | **Extended States:** No

12 NeoPixel buttons (action groups + control points) in a 2×6 grid. Two discrete input positions carry spacecraft and rover mode detection signals — no LED outputs.

### Panel Layout

```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│ CP PRI   │ AG 1     │ AG 2     │ AG 3     │ AG 4     │ AG 5     │
│ ROSE     │ GREEN    │ GREEN    │ GREEN    │ GREEN    │ GREEN    │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────┤
│ CP ALT   │ AG 6     │ AG 7     │ AG 8     │ AG 9     │ AG 10    │
│ ROSE     │ GREEN    │ GREEN    │ GREEN    │ GREEN    │ GREEN    │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
  Col 6(L)   Col 5      Col 4      Col 3      Col 2      Col 1(R)
```

### Button Reference

| KBC Index | PCB Label | Function | ACTIVE Color |
|---|---|---|---|
| 0 | BUTTON01 | Action Group 5 | GREEN |
| 1 | BUTTON02 | Action Group 10 | GREEN |
| 2 | BUTTON03 | Action Group 4 | GREEN |
| 3 | BUTTON04 | Action Group 9 | GREEN |
| 4 | BUTTON05 | Action Group 3 | GREEN |
| 5 | BUTTON06 | Action Group 8 | GREEN |
| 6 | BUTTON07 | Action Group 2 | GREEN |
| 7 | BUTTON08 | Action Group 7 | GREEN |
| 8 | BUTTON09 | Action Group 1 | GREEN |
| 9 | BUTTON10 | Action Group 6 | GREEN |
| 10 | BUTTON11 | Control Point Primary | ROSE |
| 11 | BUTTON12 | Control Point Alternate | ROSE |
| 12 | BUTTON13 | Spacecraft Mode detect (input only) | — |
| 13 | BUTTON14 | Not installed | — |
| 14 | BUTTON15 | Rover Mode detect (input only) | — |
| 15 | BUTTON16 | Not installed | — |

---

## 0x23 — Stability Control Module

**PCB:** KC-01-1822 | **Type:** 0x04 | **Extended States:** No

10 NeoPixel buttons (SAS modes + invert) in a 2×6 grid with one empty position. Two discrete inputs for SAS and RCS enable signals — no LED outputs.

### Panel Layout

```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│ (empty)  │ Stab     │ Prograde │ Normal   │ Radial   │ Target   │
│          │ Assist   │          │          │ In       │          │
│          │ GREEN    │ GREEN    │ GREEN    │ GREEN    │ GREEN    │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────┤
│ Invert   │ Maneuver │Retrograde│ Anti-    │ Radial   │ Anti-    │
│          │          │          │ Normal   │ Out      │ Target   │
│ AMBER    │ GREEN    │ GREEN    │ GREEN    │ GREEN    │ GREEN    │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
  Col 6(L)   Col 5      Col 4      Col 3      Col 2      Col 1(R)
```

### Button Reference

| KBC Index | PCB Label | Function | ACTIVE Color |
|---|---|---|---|
| 0 | BUTTON01 | SAS — Target | GREEN |
| 1 | BUTTON02 | SAS — Anti-Target | GREEN |
| 2 | BUTTON03 | SAS — Radial In | GREEN |
| 3 | BUTTON04 | SAS — Radial Out | GREEN |
| 4 | BUTTON05 | SAS — Normal | GREEN |
| 5 | BUTTON06 | SAS — Anti-Normal | GREEN |
| 6 | BUTTON07 | SAS — Prograde | GREEN |
| 7 | BUTTON08 | SAS — Retrograde | GREEN |
| 8 | BUTTON09 | SAS — Stability Assist | GREEN |
| 9 | BUTTON10 | SAS — Maneuver | GREEN |
| 10 | BUTTON11 | Not installed | — |
| 11 | BUTTON12 | Invert | AMBER |
| 12 | BUTTON13 | SAS Enable (input only) | — |
| 13 | BUTTON14 | RCS Enable (input only) | — |
| 14–15 | BUTTON15–16 | Not installed | — |

---

## 0x24 — Vehicle Control Module

**PCB:** KC-01-1822 | **Type:** 0x05 | **Extended States:** Yes

12 NeoPixel buttons in a 6×2 grid. Parachute buttons (indexes 8–11) support extended LED states for pre-deployment sequencing. Four discrete inputs carry auxiliary vehicle state signals — no LED outputs.

### Panel Layout

```
┌──────────────────┬──────────────────┐
│ Lights           │ Brakes           │
│ YELLOW           │ RED              │
├──────────────────┼──────────────────┤
│ Gear             │ Solar Array      │
│ GREEN            │ GOLD             │
├──────────────────┼──────────────────┤
│ Antenna          │ Cargo Door       │
│ PINK             │ TEAL             │
├──────────────────┼──────────────────┤
│ Radiator         │ Ladder           │
│ ORANGE           │ LIME             │
├──────────────────┼──────────────────┤
│ Drogue Deploy ★  │ Main Deploy ★    │
│ GREEN            │ GREEN            │
├──────────────────┼──────────────────┤
│ Drogue Cut ★     │ Main Cut ★       │
│ RED              │ RED              │
└──────────────────┴──────────────────┘
  Col 1 (left)       Col 2 (right)
```

★ Parachute buttons support extended LED states (see below).

### Button Reference

| KBC Index | PCB Label | Function | ACTIVE Color |
|---|---|---|---|
| 0 | BUTTON01 | Brakes | RED |
| 1 | BUTTON02 | Lights | YELLOW |
| 2 | BUTTON03 | Solar Array | GOLD |
| 3 | BUTTON04 | Gear | GREEN |
| 4 | BUTTON05 | Cargo Door | TEAL |
| 5 | BUTTON06 | Antenna | PINK |
| 6 | BUTTON07 | Ladder | LIME |
| 7 | BUTTON08 | Radiator | ORANGE |
| 8 | BUTTON09 | Main Deploy ★ | GREEN |
| 9 | BUTTON10 | Drogue Deploy ★ | GREEN |
| 10 | BUTTON11 | Main Cut ★ | RED |
| 11 | BUTTON12 | Drogue Cut ★ | RED |
| 12 | BUTTON13 | Parking Brake (input only) | — |
| 13 | BUTTON14 | Parachutes Armed (input only) | — |
| 14 | BUTTON15 | Lights Lock (input only) | — |
| 15 | BUTTON16 | Gear Lock (input only) | — |

### Parachute Button Extended LED States

The controller uses these states to sequence deployment:

| State | Appearance | Meaning |
|---|---|---|
| ENABLED | Dim white | Chute available, no action |
| WARNING | Flashing amber | Deployment window approaching |
| ALERT | Flashing red | Deploy immediately |
| ARMED | Static cyan | Chute armed and ready |
| PARTIAL_DEPLOY | Static amber | Drogue deployed, main pending |
| ACTIVE | Full green/red | Deployed / Cut |

---

## 0x25 — Time Control Module

**PCB:** KC-01-1822 | **Type:** 0x06 | **Extended States:** No

12 NeoPixel buttons in a 2×6 grid. No discrete LED buttons installed. Yellow family color theme throughout.

### Panel Layout

```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│ Pause    │ Morning  │ SOI      │ Maneuver │ PeA      │ ApA      │
│          │          │          │          │          │          │
│ AMBER    │ YELLOW   │ YELLOW   │ YELLOW   │ YELLOW   │ YELLOW   │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────┤
│ Save     │ Load     │ Warp     │ Warp     │ Physics  │ Stop     │
│          │          │ Back     │ Forward  │ Warp     │          │
│ LIME     │ MINT     │ WARM WHT │ WARM WHT │ CHARTRS  │ AMBER    │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
  Col 6(L)   Col 5      Col 4      Col 3      Col 2      Col 1(R)
```

### Button Reference

| KBC Index | PCB Label | Function | ACTIVE Color |
|---|---|---|---|
| 0 | BUTTON01 | Warp to Apoapsis | YELLOW |
| 1 | BUTTON02 | Warp Stop | AMBER |
| 2 | BUTTON03 | Warp to Periapsis | YELLOW |
| 3 | BUTTON04 | Physics Warp | CHARTREUSE |
| 4 | BUTTON05 | Warp to Maneuver | YELLOW |
| 5 | BUTTON06 | Warp Forward | WARM WHITE |
| 6 | BUTTON07 | Warp to SOI | YELLOW |
| 7 | BUTTON08 | Warp Back | WARM WHITE |
| 8 | BUTTON09 | Warp to Morning | YELLOW |
| 9 | BUTTON10 | Quick Load | MINT |
| 10 | BUTTON11 | Pause | AMBER |
| 11 | BUTTON12 | Quick Save | LIME |
| 12–15 | BUTTON13–16 | Not installed | — |

---

## 0x26 — EVA Module

**PCB:** KC-01-1852 | **Type:** 0x07 | **Extended States:** No

6 NeoPixel buttons (WS2811 RGB) via direct GPIO — no shift registers. 2 rotary encoder headers (ENC1, ENC2) present on PCB but not yet implemented.

### Panel Layout

```
┌──────────────┬──────────────┬──────────────┐
│ EVA Lights   │ Board Craft  │ Jump/Let Go  │
│ MINT         │ GREEN        │ CHARTREUSE   │
├──────────────┼──────────────┼──────────────┤
│ Jetpack      │ EVA          │ Grab         │
│ Enable       │ Construction │              │
│ LIME         │ TEAL         │ SEAFOAM      │
└──────────────┴──────────────┴──────────────┘
  Col 1 (left)   Col 2 (center)  Col 3 (right)
```

### Button Reference

| Index | Function | ACTIVE Color |
|---|---|---|
| 0 | EVA Lights | MINT |
| 1 | Jetpack Enable | LIME |
| 2 | Board Craft | GREEN |
| 3 | EVA Construction | TEAL |
| 4 | Jump / Let Go | CHARTREUSE |
| 5 | Grab | SEAFOAM |

### Notes
- No shift registers — buttons wired direct to GPIO
- NeoPixel on Port C (PC0), not Port A as on KBC standard modules
- Encoder headers H1 and H2 present but not populated

---

## 0x28 — Joystick Rotation Module

**PCB:** KC-01-1831/1832 | **Type:** 0x09 | **Capability:** JOYSTICK

Three-axis hall-effect joystick. Two NeoPixel buttons. Reports signed INT16 axis values.

### Controls

| Control | Function | Notes |
|---|---|---|
| AXIS1 (X) | Pitch | Calibrated to center at startup |
| AXIS2 (Y) | Roll | Calibrated to center at startup |
| AXIS3 (Z) | Yaw | Calibrated to center at startup |
| BTN01 (GREEN) | Reset Trim | NeoPixel, active high |
| BTN02 (AMBER) | Set Trim | NeoPixel, active high |
| BTN_JOY | TBD | No LED, joystick pushbutton |

### Notes
- **Do not touch joystick during first ~80ms after power-on** — startup calibration reads center position at boot
- Deadzone applied in firmware — small center deflections report zero
- Change threshold prevents noise from generating unnecessary I2C interrupts

---

## 0x29 — Joystick Translation Module

**PCB:** KC-01-1831/1832 | **Type:** 0x0A | **Capability:** JOYSTICK

Three-axis hall-effect joystick. Two NeoPixel buttons. Reports signed INT16 axis values.

### Controls

| Control | Function | Notes |
|---|---|---|
| AXIS1 (X) | Surge (forward/back) | Calibrated to center at startup |
| AXIS2 (Y) | Sway (left/right) | Calibrated to center at startup |
| AXIS3 (Z) | Heave (up/down) | Calibrated to center at startup |
| BTN01 (MAGENTA) | Cycle Camera Mode | NeoPixel, active high |
| BTN02 (GREEN) | Camera Reset | NeoPixel, active high |
| BTN_JOY | Enable Camera Mode | No LED, joystick pushbutton |

### Notes
- **Do not touch joystick during first ~80ms after power-on** — startup calibration reads center position at boot
- Same physical hardware as Rotation module, different firmware assignment

---

## 0x2A — GPWS Input Panel

**PCB:** KC-01-1881/1882 | **Type:** 0x0B | **Capability:** DISPLAY

4-digit 7-segment display driven by MAX7219. Rotary encoder for value input. Three SK6812MINI-EA GRBW NeoPixel buttons. Encoder pushbutton.

### Physical Layout (top to bottom)

```
┌─────────────────────────┐
│    [ 4-digit display ]  │  Altitude/distance threshold (0–9999m)
│       [ Encoder ]       │  Turn to adjust value
│  [BTN01][BTN02][BTN03]  │
└─────────────────────────┘
```

### Controls

| Control | Function | Behavior |
|---|---|---|
| Display | Altitude threshold | Shows current value, no leading zeros |
| Encoder | Adjust threshold | ±1 slow / ±10 medium / ±100 fast |
| Encoder button | Reset to default | Returns to 200m |
| BTN01 | GPWS Enable | 3-state cycle: OFF → ACTIVE (green, full GPWS) → AMBER (proximity tone only) |
| BTN02 | Proximity Alarm | Toggle: OFF ↔ GREEN |
| BTN03 | Rendezvous Radar | Toggle: OFF ↔ GREEN |

### Encoder Acceleration

| Spin speed | Step size |
|---|---|
| Slow (>150ms between detents) | ±1 |
| Medium (50–150ms) | ±10 |
| Fast (<50ms) | ±100 |

### State Byte (data packet byte 2)

| Bits | Meaning |
|---|---|
| 1:0 | BTN01 state (0=off, 1=full GPWS, 2=proximity only) |
| 2 | BTN02 active (proximity alarm enabled) |
| 3 | BTN03 active (rendezvous radar enabled) |

---

## 0x2B — Pre-Warp Time Module

**PCB:** KC-01-1881/1882 | **Type:** 0x0C | **Capability:** DISPLAY

4-digit 7-segment display driven by MAX7219. Rotary encoder for value input. Three SK6812MINI-EA GRBW NeoPixel buttons (flash mode) for presets. Encoder pushbutton.

### Physical Layout (top to bottom)

```
┌─────────────────────────┐
│    [ 4-digit display ]  │  Pre-warp duration in minutes (0–9999)
│       [ Encoder ]       │  Turn to adjust value
│  [BTN01][BTN02][BTN03]  │
└─────────────────────────┘
```

### Controls

| Control | Function | Behavior |
|---|---|---|
| Display | Pre-warp duration | Shows minutes, no leading zeros |
| Encoder | Adjust duration | ±1 slow / ±10 medium / ±100 fast |
| Encoder button | Reset to 0 | Clears to default |
| BTN01 (GOLD flash) | 5 min preset | Jumps display to 5, flashes GOLD briefly |
| BTN02 (GOLD flash) | 1 hour preset | Jumps display to 60, flashes GOLD briefly |
| BTN03 (GOLD flash) | 1 day preset | Jumps display to 1440, flashes GOLD briefly |

### Encoder Acceleration

| Spin speed | Step size |
|---|---|
| Slow (>150ms between detents) | ±1 |
| Medium (50–150ms) | ±10 |
| Fast (<50ms) | ±100 |

### Notes
- Preset buttons use FLASH mode — illuminate gold for 150ms on press then return to dim white
- Value transmitted to controller continuously as encoder is turned
- Controller initiates warp sequence based on current display value

---

## 0x2C — Throttle Module

**PCB:** KC-01-1861/1862 | **Type:** 0x0D

Motorized 10k slide potentiometer with L293D H-bridge motor driver. Capacitive touch sensor. Four discrete pushbuttons with discrete LED outputs.

### Controls

| Control | Function | Notes |
|---|---|---|
| Slide potentiometer | Throttle position | Motorized — moves under controller command |
| Touch sensor | Pilot contact detect | Touching slider disables motor, allows manual input |
| THRTL_100 + LED | Set throttle to 100% | Discrete button + LED |
| THRTL_UP + LED | Step throttle up | Discrete button + LED |
| THRTL_DOWN + LED | Step throttle down | Discrete button + LED |
| THRTL_00 + LED | Set throttle to 0% | Discrete button + LED |

### Operating Modes

**Normal mode:** Controller commands position; motor drives slider to target; touching slider overrides motor and allows manual input; releasing restores motor control.

**Precision mode** (controller-activated via CMD_SET_PRECISION): Slider physically centered for equal authority; virtual range mapped to full 0–100% output. Allows fine control independent of physical position.

### Status Byte (data packet byte 0)

| Bit | Meaning |
|---|---|
| 0 | Module enabled |
| 1 | Precision mode active |
| 2 | Pilot touching slider |
| 3 | Motor moving |

### Notes
- Module starts **disabled** at power-on — controller must send CMD_ENABLE
- Requires +12V motor bus in addition to +5V logic supply
- Motor holds slider at 0% while disabled, resisting manual movement

---

## 0x2D — Dual Encoder Module

**PCB:** KC-01-1871/1872 | **Type:** 0x0E | **Capability:** ENCODERS

Two independent quadrature rotary encoders (PEC11R-4220F-S0024) with pushbutton switches. Hardware RC debounce (10nF) on A/B channels. No LEDs.

### Controls

| Control | Function |
|---|---|
| ENC1 (turn) | Reports signed delta to controller (function TBD by controller) |
| ENC1 button | Reports press event to controller (function TBD by controller) |
| ENC2 (turn) | Reports signed delta to controller (function TBD by controller) |
| ENC2 button | Reports press event to controller (function TBD by controller) |

### Data Packet

| Byte | Content |
|---|---|
| 0 | Button events (bit0=ENC1_SW, bit1=ENC2_SW) |
| 1 | Change mask (same layout) |
| 2 | ENC1 delta (signed int8, +CW / -CCW) |
| 3 | ENC2 delta (signed int8, +CW / -CCW) |

### Notes
- Encoder functions are defined entirely by the main controller — this module reports raw deltas without semantic interpretation
- Deltas accumulate between reads — multiple detents between I2C reads are summed, not dropped

---

## 0x2E — Switch Panel Module

**PCB:** KC-01 Switch Panel | **Type:** 0x0F

10 toggle switch inputs. No LEDs. All switches are active-high with 10k pull-downs.

### Switch Reference

| Switch | Bit | Function | Controller Action |
|---|---|---|---|
| SW1 | 0 | Live Control Mode | Toggle: enable/suspend live telemetry and control link |
| SW2 | 1 | Demo Mode | Toggle: enter/exit simulated data mode → INDICATOR B8 (DEMO) ACTIVE |
| SW3 | 2 | Master Reset | Rising edge only: CMD_RESET to all modules, then re-enumerate |
| SW4 | 3 | Display Reset | Rising edge only: CMD_SET_VALUE 0 to GPWS (0x2A) and Pre-Warp (0x2B) |
| SW5 | 4 | SCE to Auxiliary | Toggle: activate/deactivate SCE auxiliary power → INDICATOR B12 (SCE AUX) ACTIVE |
| SW6 | 5 | Bulb Test | Rising edge: CMD_BULB_TEST start. Falling edge: CMD_BULB_TEST 0x00 stop |
| SW7 | 6 | Audio On | Toggle: enable/disable audio system → INDICATOR B9 (AUDIO) ACTIVE |
| SW8 | 7 | Precision Input | Toggle: precision input mode system-wide → INDICATOR B6 (PREC INPUT) ACTIVE |
| SW9 | 8 | Engine Arm | Toggle: arm/disarm engine ignition system |
| SW10 | 9 | Throttle Fine Control | Rising edge: CMD_SET_PRECISION 0x01 → Throttle. Falling edge: CMD_SET_PRECISION 0x00 → INDICATOR B3 (THRTL PREC) ACTIVE |

### Data Packet

| Byte | Content |
|---|---|
| 0 | State HIGH byte (bits 15–8 — bit 9=SW10, bits 10–15 always 0) |
| 1 | State LOW byte (bits 7–0 = SW8–SW1) |
| 2 | Change mask HIGH |
| 3 | Change mask LOW |

### Controller Behavior Notes

**SW3 (Master Reset) and SW4 (Display Reset)** act only on the rising edge regardless of switch type. The controller ignores the held-on state and falling edge for these functions — a reset is a momentary action.

**SW6 (Bulb Test)** uses both edges: rising edge sends the start command to all modules, falling edge sends the stop command. This allows the operator to hold the switch for a sustained test or flip it quickly for a brief flash check.

**SW10 (Throttle Fine Control)** directly drives `CMD_SET_PRECISION` on the Throttle module (0x2C). When toggled on, the throttle motor drives the slider to physical center and the full travel range maps to fine control authority. When toggled off, the motor repositions to match current output in normal mapping.

**SW8 (Precision Input) vs SW10 (Throttle Fine Control)** are distinct: SW8 controls system-wide precision input scaling (joystick axes and other inputs), while SW10 controls the Throttle module's motorized precision mode specifically. Both can be active simultaneously.

### Notes
- Supports both latching and momentary toggle switches
- Every state change edge generates an INT — dual-buffer re-assertion ensures no edge is lost even if a momentary switch flips and returns between reads
- INT pin on PB2 (not PA1 as on standard modules)

---

## 0x2F — Indicator Module

**PCB:** KC-01 Indicator v2.0 | **Type:** 0x10 | **Extended States:** Yes (ABORT pixel)

Pure output module. 18 SK6812mini-012 RGB NeoPixels in a 6×3 grid driven entirely by the main controller via CMD_SET_LED_STATE. No inputs. Two encoder headers present on PCB but not installed.

**Note for controller:** This module uses a 9-byte CMD_SET_LED_STATE payload (18 nibbles) rather than the standard 8-byte payload. The controller must send 10 bytes on wire (address + command + 9 payload bytes) to this address.

### Panel Layout

```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│ THRTL    │ THRTL    │ PREC     │          │ SCE      │ ABORT    │
│ ENA      │ PREC     │ INPUT    │ AUDIO    │ AUX      │          │
│ GREEN    │ MINT     │ CYAN     │ PURPLE   │ ORANGE   │ RED ★    │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────┤
│ LIGHT    │ BRAKE    │ LNDG     │ CHUTE    │          │          │
│ ENA      │ LOCK     │ GEAR     │ ARM      │ RCS      │ SAS      │
│ YELLOW   │ RED      │ GREEN    │ AMBER    │ MINT     │ GREEN    │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────┤
│          │          │          │ COMM     │ SWITCH   │ AUTO     │
│ CTRL     │ DEBUG    │ DEMO     │ ACTIVE   │ ERROR    │ PILOT    │
│ LIME     │ ROSE     │ SKY      │ TEAL     │ RED      │ BLUE     │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
  Col 1      Col 2      Col 3      Col 4      Col 5      Col 6
```

★ ABORT supports extended LED states — see below.

### Pixel Reference

Pixels are indexed column-major: down column 1 (B0–B2), then column 2 (B3–B5), and so on.

| Pixel | Label | ACTIVE Color | Notes |
|---|---|---|---|
| B0 | THRTL ENA | GREEN | |
| B1 | LIGHT ENA | YELLOW | Matches Vehicle Control Lights |
| B2 | CTRL | LIME | Green family |
| B3 | THRTL PREC | MINT | |
| B4 | BRAKE LOCK | RED | Matches Vehicle Control Brakes |
| B5 | DEBUG | ROSE | Red family, distinct from fault RED |
| B6 | PREC INPUT | CYAN | |
| B7 | LNDG GEAR LOCK | GREEN | Matches Vehicle Control Gear |
| B8 | DEMO | SKY | Blue family |
| B9 | AUDIO | PURPLE | |
| B10 | CHUTE ARM | AMBER | |
| B11 | COMM ACTIVE | TEAL | |
| B12 | SCE AUX | ORANGE | |
| B13 | RCS | MINT | Matches Stability Control convention |
| B14 | SWITCH ERROR | RED | |
| B15 | ABORT | RED ★ | Primary consumer of ALERT state |
| B16 | SAS | GREEN | Matches Stability Control |
| B17 | AUTO PILOT | BLUE | |

### ABORT Extended State Sequence (B15)

| State | Appearance | Meaning |
|---|---|---|
| ENABLED | Dim white | Nominal — abort available |
| ARMED | Static cyan | Abort sequence primed |
| ALERT | Flashing red (150ms) | Abort in progress |
| ACTIVE | Solid red | Post-abort confirmation |
| OFF | Dark | Abort system not available |

### Notes
- All pixel states set exclusively by controller via CMD_SET_LED_STATE
- Flash timing (WARNING, ALERT) runs on-module — controller sends state nibble only
- **CMD_SET_LED_STATE payload: 9 bytes** (not 8) — controller must account for this
- NeoPixel on PA5, Port A — IDE tinyNeoPixel port setting must be Port A
- INT pin on PC3 (not PA1 as on standard modules)

---

## I2C Address & Type ID Summary

| Address | Module | Type ID | Library | Extended States |
|---|---|---|---|---|
| 0x20 | UI Control | 0x01 | KerbalButtonCore | No |
| 0x21 | Function Control | 0x02 | KerbalButtonCore | No |
| 0x22 | Action Control | 0x03 | KerbalButtonCore | No |
| 0x23 | Stability Control | 0x04 | KerbalButtonCore | No |
| 0x24 | Vehicle Control | 0x05 | KerbalButtonCore | Yes |
| 0x25 | Time Control | 0x06 | KerbalButtonCore | No |
| 0x26 | EVA Module | 0x07 | Standalone | No |
| 0x27 | (reserved) | 0x08 | — | — |
| 0x28 | Joystick Rotation | 0x09 | KerbalJoystickCore | No |
| 0x29 | Joystick Translation | 0x0A | KerbalJoystickCore | No |
| 0x2A | GPWS Input Panel | 0x0B | Kerbal7SegmentCore | No |
| 0x2B | Pre-Warp Time | 0x0C | Kerbal7SegmentCore | No |
| 0x2C | Throttle Module | 0x0D | Standalone | No |
| 0x2D | Dual Encoder | 0x0E | Standalone | No |
| 0x2E | Switch Panel | 0x0F | Standalone | No |
| 0x2F | Indicator | 0x10 | Standalone | Yes (ABORT pixel) |
