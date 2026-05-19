# Kerbal Controller Mk1 — Panel Interface Guide

**Jeb's Controller Works** | v5.0 | 2026-05-10

This is the definitive source of truth for all physical controls, modules, and their functions across the KCMk1 controller. Arranged by panel, then by module within each panel.

---

## Document Status

| Item | Status |
|---|---|
| Panel A1 | Complete |
| Panel A2 | Complete — Switch Group 1 hardware TBD |
| Panel B1 | Complete |
| Panel B2 | Complete — Switch Group 2 hardware TBD; 12-button EVA/TBD module 4 slots unassigned; module rename pending |
| Ctrl Mode Toggle wiring | TBD |
| Switch Group hardware (A2, B2) | TBD |
| Operator feedback review | Cleanup item — see Section 7 |

---

## Open Items / To-Do List

| # | Item | Type | Notes |
|---|---|---|---|
| 1 | Switch Group hardware | Design | Both A2 and B2 switch groups need hardware decided — PCB or off-shelf solution |
| 2 | Ctrl Mode Toggle wiring | Design | 3-position Spacecraft/Airplane/Rover toggle on A2 — destination (Ctrl Board or module) TBD |
| 3 | 12-button module 4 open slots | ✅ Resolved | B8=Plant Flag, B9=CP Toggle, B10=CP Docking Port, B11=Cruise Control |
| 4 | Module rename (EVA/TBD) | ✅ Resolved | Renamed to AUX CTRL |
| 5 | Ctrl Module / Ctrl Ext Module | Design | Both deferred — placement and function TBD |
| 6 | Operator feedback review | Design | With Indicator module removed, verify mode state / abort / switch error / comm status feedback exists on TFT screens or elsewhere. Currently no dedicated panel indicator for these. |
| 7 | Firmware: Function Control sketch | Code | New layout: LES B0, Fairing Jett B1, Air Intake B2, Lock Surfaces B3, Airbrake B4 (CAG 38), RW Disable B5 (CAG 39), Engine Alt B6, Science Collect B7, Engine Grp1 B8, Science Grp1 B9, Engine Grp2 B10, Science Grp2 B11. CP buttons removed. TODO #15 resolved — unblocked. |
| 8 | Firmware: Action Control sketch | Code | Old sketch has CP PRI/CP ALT at B10/B11. Current design has AG11/AG12 there. Sketch needs update. |
| 9 | Firmware: Vehicle Control sketch | Code | Old sketch has B12–B15 discrete inputs. Current design removes all discrete inputs. Sketch needs update. |
| 10 | Firmware: Stability Control sketch | Code | Old sketch has B12 SAS Enable / B13 RCS Enable discrete inputs. Current design removes these and adds RCS button at B10 (GREEN). Sketch needs update. |
| 11 | Documentation: Switch Panel (0x2F) | Documentation | v4.0 Switch Panel section describes old 10-switch layout. Replaced by Switch Groups 1 and 2 in this document. Hardware address TBD. |
| 12 | Round Display | Design | Removed from B2. Confirm no other placement intended. |
| 13 | ENC2 Dual Encoder function | Design | Hullcam VDS camera cycling confirmed. Verify mod keybindings match in-game settings. |
| 14 | B17 AUTO PILOT on old Indicator | Design | Indicator module removed. Auto Pilot indicator no longer exists. Confirm acceptable or find new home. |
| 15 | Function Control button order | ✅ Resolved | New order: LES/Fairing Jett, Air Intake/Lock Surfaces, CP Toggle/CP Dock, Engine Alt/Science Collect, Engine Grp1/Science Grp1, Engine Grp2/Science Grp2. TODO #7 now unblocked. |

---

## Required Mods

| Mod | Purpose |
|---|---|
| **AGX (Action Groups Extended)** | Enables custom action groups beyond AG1–AG10. Required for all CAG assignments. Controller uses AGX groups 1–238 across 6 control groups. |
| **Hullcam VDS Continued** | Adds mountable external cameras. Required for Dual Encoder ENC2 camera cycling. Default keybindings: `-` (next), `=` (previous), `Backspace` (reset). |

---

## LED State Color Key

All NeoPixel buttons share a common set of LED states.

| State | Appearance | Meaning |
|---|---|---|
| OFF | Dark | Inactive / not installed |
| ENABLED | Dim white | Module active, function available |
| ACTIVE | Full color (per button) | Function engaged or selected |
| WARNING | Flashing amber 500ms | Attention required |
| ALERT | Flashing red 150ms | Immediate action required |
| ARMED | Static cyan | Primed and ready |
| PARTIAL_DEPLOY | Static amber | Partially deployed state |

Discrete LED buttons are ON/OFF only — ENABLED and ACTIVE both produce full brightness.

---

## Custom AG Assignment Table

All custom action groups use AGX numbers. ENC1 on the Dual Encoder selects the active control group.

**Offset formula:** AGX number = Base CAG + ((Group − 1) × 40)

### Control Groups

| Group | Name |
|---|---|
| 1 | Default Group |
| 2 | Lander Group #1 |
| 3 | Lander Group #2 |
| 4 | Custom Group |
| 5 | Re-Entry Vehicle Group |
| 6 | Space Station Group |

### Assignment Table

| Base | Function | Module | Button | Grp1 | Grp2 | Grp3 | Grp4 | Grp5 | Grp6 |
|---|---|---|---|---|---|---|---|---|---|
| 1 | AG1 | Action Control | B10 | 1 | 41 | 81 | 121 | 161 | 201 |
| 2 | AG2 | Action Control | B8 | 2 | 42 | 82 | 122 | 162 | 202 |
| 3 | AG3 | Action Control | B6 | 3 | 43 | 83 | 123 | 163 | 203 |
| 4 | AG4 | Action Control | B4 | 4 | 44 | 84 | 124 | 164 | 204 |
| 5 | AG5 | Action Control | B2 | 5 | 45 | 85 | 125 | 165 | 205 |
| 6 | AG6 | Action Control | B0 | 6 | 46 | 86 | 126 | 166 | 206 |
| 7 | AG7 | Action Control | B11 | 7 | 47 | 87 | 127 | 167 | 207 |
| 8 | AG8 | Action Control | B9 | 8 | 48 | 88 | 128 | 168 | 208 |
| 9 | AG9 | Action Control | B7 | 9 | 49 | 89 | 129 | 169 | 209 |
| 10 | AG10 | Action Control | B5 | 10 | 50 | 90 | 130 | 170 | 210 |
| 11 | AG11 | Action Control | B3 | 11 | 51 | 91 | 131 | 171 | 211 |
| 12 | AG12 | Action Control | B1 | 12 | 52 | 92 | 132 | 172 | 212 |
| 13 | Antenna | Vehicle Control | B2 | 13 | 53 | 93 | 133 | 173 | 213 |
| 14 | Fuel Cell | Vehicle Control | B4 | 14 | 54 | 94 | 134 | 174 | 214 |
| 15 | Solar Array | Vehicle Control | B5 | 15 | 55 | 95 | 135 | 175 | 215 |
| 16 | Cargo Door | Vehicle Control | B6 | 16 | 56 | 96 | 136 | 176 | 216 |
| 17 | Radiator | Vehicle Control | B7 | 17 | 57 | 97 | 137 | 177 | 217 |
| 18 | Ladder | Vehicle Control | B9 | 18 | 58 | 98 | 138 | 178 | 218 |
| 19 | Heat Shield Deploy | Vehicle Control | B8 | 19 | 59 | 99 | 139 | 179 | 219 |
| 20 | Heat Shield Release | Vehicle Control | B8 | 20 | 60 | 100 | 140 | 180 | 220 |
| 21 | Main Chute Deploy | Vehicle Control | B10 | 21 | 61 | 101 | 141 | 181 | 221 |
| 22 | Main Chute Cut | Vehicle Control | B10 | 22 | 62 | 102 | 142 | 182 | 222 |
| 23 | Drogue Chute Deploy | Vehicle Control | B11 | 23 | 63 | 103 | 143 | 183 | 223 |
| 24 | Drogue Chute Cut | Vehicle Control | B11 | 24 | 64 | 104 | 144 | 184 | 224 |
| 25 | LES | Function Control | B0 | 25 | 65 | 105 | 145 | 185 | 225 |
| 26 | Fairing Jettison | Function Control | B1 | 26 | 66 | 106 | 146 | 186 | 226 |
| 27 | Engine Alt Mode | Function Control | B6 | 27 | 67 | 107 | 147 | 187 | 227 |
| 28 | Science Collect | Function Control | B7 | 28 | 68 | 108 | 148 | 188 | 228 |
| 29 | Engine Group 1 | Function Control | B8 | 29 | 69 | 109 | 149 | 189 | 229 |
| 30 | Science Group 1 | Function Control | B9 | 30 | 70 | 110 | 150 | 190 | 230 |
| 31 | Engine Group 2 | Function Control | B10 | 31 | 71 | 111 | 151 | 191 | 231 |
| 32 | Science Group 2 | Function Control | B11 | 32 | 72 | 112 | 152 | 192 | 232 |
| 33 | Air Intake | Function Control | B2 | 33 | 73 | 113 | 153 | 193 | 233 |
| 34 | Lock Surfaces | Function Control | B3 | 34 | 74 | 114 | 154 | 194 | 234 |
| 35 | CP Primary | AUX CTRL | B10 | 35 | 75 | 115 | 155 | 195 | 235 |
| 36 | CP Alternate | AUX CTRL | B10 | 36 | 76 | 116 | 156 | 196 | 236 |
| 37 | CP Docking Port | AUX CTRL | B11 | 37 | 77 | 117 | 157 | 197 | 237 |
| 38 | Airbrake | Function Control | B4 | 38 | 78 | 118 | 158 | 198 | 238 |
| 38 | Airbrake (duplicate) | Rotation Module | BTN_JOY | 38 | 78 | 118 | 158 | 198 | 238 |
| 39 | Reaction Wheel Disable | Function Control | B5 | 39 | 79 | 119 | 159 | 199 | 239 |

---

## Section 1 — Panel A1

**Location:** Left panel

| Module | Type | Address | Notes |
|---|---|---|---|
| Panel Power Button | Direct wire to Ctrl Board | — | Hard power, not software controlled |
| Annunciator | TFT display with touch | — | Caution & Warning display |
| Info 1 Display | TFT display with touch | — | Orbital / Launch / Landing screens |
| GPWS Input Panel | 7-segment + encoder + 3 buttons | 0x2A | Altitude threshold input |

### 1.1 — GPWS Input Panel (0x2A)

**PCB:** KC-01-1881/1882 | **Type:** 0x0B

| Control | Function | Behavior |
|---|---|---|
| 4-digit display | Altitude threshold | Shows current value 0–9999m, no leading zeros |
| Encoder | Adjust threshold | ±1 slow / ±10 medium / ±100 fast |
| Encoder button | Reset to default | Returns to 200m |
| BTN01 | GPWS Enable | 3-state cycle: OFF → ACTIVE (green, full GPWS) → AMBER (proximity tone only) |
| BTN02 | Proximity Alarm | Toggle: OFF ↔ GREEN |
| BTN03 | Rendezvous Radar | Toggle: OFF ↔ GREEN |

---

## Section 2 — Panel A2

**Location:** Left-center panel

| Module | Type | Address | Notes |
|---|---|---|---|
| Switch Group 1 | 8 latching/momentary switches | TBD | Hardware TBD — see Section 2.1 |
| Ctrl Mode Toggle | 3-position direct wire | — | Spacecraft / Airplane / Rover — wiring destination TBD |
| UI Control | 12 NeoPixel buttons | 0x20 | |
| Function Control | 12 NeoPixel buttons | 0x21 | |
| Action Control | 12 NeoPixel buttons | 0x22 | |
| Throttle Module | Motorized fader + 4 buttons | 0x2C | |
| Translation Joystick | 3-axis + 2 buttons | 0x29 | |

### 2.1 — Switch Group 1

**Hardware:** TBD | **Panel:** A2

Physical layout: 2 rows × 4 columns. Row 1 left to right: MSTR, DISPL, SCE, UPTLM. Row 2 left to right: LTG, ENGINE, THROTTLE (spans cols 3–4).

| # | Title | OFF | ON | Type | Function |
|---|---|---|---|---|---|
| SW1 | MSTR | OPER | RST | Momentary (hold) | Panel/controller reset — hold to actuate |
| SW2 | DISPL | OPER | CLR | Momentary (hold) | Display reset — commands all screens to default |
| SW3 | SCE | NORM | AUX | Latching | Simpit reset and data fetch |
| SW4 | UPTLM | INHIB | XMIT | Latching | Enable/disable panel debug telemetry output |
| SW5 | LTG | NORM | TEST | Momentary (hold) | Lamp test — hold to illuminate all indicators |
| SW6 | ENGINE | SAFE | ARM | Latching | Engine ignition arm/safe interlock |
| SW7 | THROTTLE | INOP | ACT | Latching | Throttle input enable/disable |
| SW8 | THROTTLE | STD | FINE | Latching | Throttle precision mode |

SW7 and SW8 share the group label **THROTTLE** spanning columns 3–4 of row 2. Individual switch titles are not displayed — the ON/OFF labels identify each switch function.

### 2.2 — Ctrl Mode Toggle

**Type:** 3-position latching switch, direct wire | **Wiring:** TBD

Selects the vehicle type, which determines how rotation and translation joystick axes are mapped and which Simpit message channel is used.

| Position | Mode | Rotation Mapping | Translation Mapping |
|---|---|---|---|
| Up | Spacecraft | Yaw=AXIS1, Pitch=AXIS2, Roll=AXIS3 | `TRANSLATION_MESSAGE` |
| Center | Airplane | Roll=AXIS1, Pitch=AXIS2, Yaw=AXIS3 | `TRANSLATION_MESSAGE` |
| Down | Rover | Steer=AXIS1, Throttle=AXIS2 | `WHEEL_MESSAGE` |

EVA mode overrides all switch positions when `isEVA = true` from Simpit `FLIGHT_STATUS_MESSAGE`.

### 2.3 — UI Control Module (0x20)

**PCB:** KC-01-1822 | **Type:** 0x01 | **Extended States:** No

```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│ Map      │ Map      │ Map      │ Map      │ UI       │Screenshot│
│ Enable   │ Back     │ Forward  │ Reset    │ Toggle   │          │
│ SKY      │ SKY      │ SKY      │ SKY      │ GREEN    │ GREEN    │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────┤
│ IVA      │ Ship     │ Ship     │ Navball  │ Nav Ball │ Debug    │
│          │ Back     │ Forward  │ Ref Cycle│ Toggle   │          │
│ CORAL    │ TEAL     │ TEAL     │ GREEN    │ AMBER    │ RED      │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
  Col 6(L)   Col 5      Col 4      Col 3      Col 2      Col 1(R)
```

| KBC Index | Function | Color | Implementation |
|---|---|---|---|
| B0 | Screenshot | GREEN | Macro: F2 → F1 → F2 |
| B1 | Debug | RED | `KEYBOARD_EMULATOR` Alt+F12 |
| B2 | UI Toggle | GREEN | `KEYBOARD_EMULATOR` F2 |
| B3 | Nav Ball Toggle | AMBER | `KEYBOARD_EMULATOR` Numpad . |
| B4 | Map Reset | SKY | `KEYBOARD_EMULATOR` ' |
| B5 | Navball Reference Cycle | GREEN | Simpit `NAVBALLMODE_MESSAGE` — cycles Surface → Orbit → Target |
| B6 | Map Forward | SKY | `KEYBOARD_EMULATOR` Tab |
| B7 | Ship Forward | TEAL | `KEYBOARD_EMULATOR` ] |
| B8 | Map Back | SKY | `KEYBOARD_EMULATOR` Shift+Tab |
| B9 | Ship Back | TEAL | `KEYBOARD_EMULATOR` [ |
| B10 | Map Enable | SKY | `KEYBOARD_EMULATOR` M |
| B11 | IVA | CORAL | `KEYBOARD_EMULATOR` C |
| B12–B15 | Not installed | — | — |

### 2.4 — Function Control Module (0x21)

**PCB:** KC-01-1822 | **Type:** 0x02 | **Extended States:** No

All buttons send Custom Action Group (AGX) commands. Layout follows mission timeline — launch actions top, functional systems bottom.

```
┌──────────────────┬──────────────────┐
│ LES              │ Fairing Jettison │
│ RED              │ AMBER            │
├──────────────────┼──────────────────┤
│ Air Intake       │ Lock Surfaces    │
│ TEAL             │ SKY              │
├──────────────────┼──────────────────┤
│ Airbrake         │ RW Disable       │
│ CYAN             │ AMBER            │
├──────────────────┼──────────────────┤
│ Engine Alt Mode  │ Science Collect  │
│ ORANGE           │ PURPLE           │
├──────────────────┼──────────────────┤
│ Engine Group 1   │ Science Group 1  │
│ YELLOW           │ INDIGO           │
├──────────────────┼──────────────────┤
│ Engine Group 2   │ Science Group 2  │
│ CHARTREUSE       │ BLUE             │
└──────────────────┴──────────────────┘
  Col 1 (left)       Col 2 (right)
```

| KBC Index | Function | Color | Base CAG | Implementation |
|---|---|---|---|---|
| B0 | LES (Launch Escape System) | RED | 25 | Custom AG (AGX) |
| B1 | Fairing Jettison | AMBER | 26 | Custom AG (AGX) |
| B2 | Air Intake | TEAL | 33 | Custom AG (AGX) |
| B3 | Lock Surfaces | SKY | 34 | Custom AG (AGX) |
| B4 | Airbrake | CYAN | 38 | Custom AG (AGX) |
| B5 | Reaction Wheel Disable | AMBER | 39 | Custom AG (AGX) |
| B6 | Engine Alt Mode | ORANGE | 27 | Custom AG (AGX) |
| B7 | Science Collect | PURPLE | 28 | Custom AG (AGX) |
| B8 | Engine Group 1 | YELLOW | 29 | Custom AG (AGX) |
| B9 | Science Group 1 | INDIGO | 30 | Custom AG (AGX) |
| B10 | Engine Group 2 | CHARTREUSE | 31 | Custom AG (AGX) |
| B11 | Science Group 2 | BLUE | 32 | Custom AG (AGX) |
| B12–B15 | Not installed | — | — | — |

**⚠️ TODO #7:** Firmware sketch needs update to match new layout — Airbrake at B4, Reaction Wheel Disable at B5, CP buttons removed.

### 2.5 — Action Control Module (0x22)

**PCB:** KC-01-1822 | **Type:** 0x03 | **Extended States:** No

12 NeoPixel buttons AG1–AG12, all GREEN. Panel reads right-to-left: AG1 top-right, AG12 bottom-left.

```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│ AG 1     │ AG 2     │ AG 3     │ AG 4     │ AG 5     │ AG 6     │
│ GREEN    │ GREEN    │ GREEN    │ GREEN    │ GREEN    │ GREEN    │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────┤
│ AG 7     │ AG 8     │ AG 9     │ AG 10    │ AG 11    │ AG 12    │
│ GREEN    │ GREEN    │ GREEN    │ GREEN    │ GREEN    │ GREEN    │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
  Col 6(R)   Col 5      Col 4      Col 3      Col 2      Col 1(L)
```

| KBC Index | Panel Position | Function | Base CAG |
|---|---|---|---|
| B0 | Col 1 top | AG6 | 6 |
| B1 | Col 1 bottom | AG12 | 12 |
| B2 | Col 2 top | AG5 | 5 |
| B3 | Col 2 bottom | AG11 | 11 |
| B4 | Col 3 top | AG4 | 4 |
| B5 | Col 3 bottom | AG10 | 10 |
| B6 | Col 4 top | AG3 | 3 |
| B7 | Col 4 bottom | AG9 | 9 |
| B8 | Col 5 top | AG2 | 2 |
| B9 | Col 5 bottom | AG8 | 8 |
| B10 | Col 6 top | AG1 | 1 |
| B11 | Col 6 bottom | AG7 | 7 |
| B12–B15 | Not installed | — | — |

All AG buttons use Simpit `CAGTOGGLE_MESSAGE`. AGX number sent = Base CAG + ((Group − 1) × 40).

**⚠️ TODO #8:** Firmware sketch still has CP PRI/CP ALT at B10/B11. Needs update to AG11/AG12.

### 2.6 — Throttle Module (0x2C)

**PCB:** KC-01-1861/1862 | **Type:** 0x0D

| Control | Function | Notes |
|---|---|---|
| Slide potentiometer | Throttle position | Motorized — moves under controller command |
| Touch sensor | Pilot contact detect | Touching slider disables motor, allows manual input |
| THRTL_100 + LED | Set throttle to 100% | Discrete button + LED |
| THRTL_UP + LED | Step throttle up | Discrete button + LED |
| THRTL_DOWN + LED | Step throttle down | Discrete button + LED |
| THRTL_00 + LED | Set throttle to 0% | Discrete button + LED |

Normal mode: controller commands position; motor drives slider; touching slider allows manual override. Precision mode (CMD_SET_PRECISION): slider centered, virtual range maps to full 0–100%.

Module starts disabled at power-on — controller must send CMD_ENABLE. Requires +12V motor bus.

### 2.7 — Translation Joystick (0x29)

**PCB:** KC-01-1831/1832 | **Type:** 0x0A

Three modes: Translation (normal), Camera (BTN_JOY held), EVA Jetpack (auto when `isEVA = true`).

**Translation Mode:**

| Axis | Physical | KSP Function | Simpit Field |
|---|---|---|---|
| AXIS1 | Left / Right | Translate Left / Right | `TRANSLATION_MESSAGE` X |
| AXIS2 | Forward / Back | Translate Up / Down | `TRANSLATION_MESSAGE` Y |
| AXIS3 | CW / CCW twist | Translate Forward / Reverse | `TRANSLATION_MESSAGE` Z |

AXIS2 inversion applied in module firmware — controller receives correct polarity.

**Camera Mode (BTN_JOY held):**

| Axis | Camera Function |
|---|---|
| AXIS1 | Camera Yaw Left / Right |
| AXIS2 | Camera Pitch Up / Down |
| AXIS3 | Zoom In / Zoom Out |

**EVA Jetpack Mode (auto — `isEVA` = true):** Digital key emulation via `KEYBOARD_EMULATOR`.

| Axis | KSP Action | Key |
|---|---|---|
| AXIS1 left/right | Jetpack left/right | A / D (held) |
| AXIS2 forward/back | Jetpack forward/back | W / S (held) |
| AXIS3 CW/CCW | Jetpack up/down | Left Shift / Left Ctrl (held) |
| BTN_JOY held | Camera control | (camera mode applies) |

| Button | Function | Color |
|---|---|---|
| BTN01 | Cycle Camera Mode (auto/free/orbital/chase) | MAGENTA |
| BTN02 | Camera Reset | GREEN |
| BTN_JOY held | Camera Control mode | — |

---

## Section 3 — Panel B1

**Location:** Right-center panel

| Module | Type | Address | Notes |
|---|---|---|---|
| Info 2 Display | TFT display with touch | — | Resource / Vehicle / additional screens |
| Resource Display | TFT display with touch | — | Resource monitoring |
| Dual Encoder | 2 encoders + 2 buttons | 0x2D | |
| Abort Button | Direct wire | — | Momentary, ABORT_ACTION |

### 3.1 — Dual Encoder Module (0x2D)

**PCB:** KC-01-1871/1872 | **Type:** 0x0E

| Control | Function | Implementation |
|---|---|---|
| ENC1 CW | AG Block next | Controller-side — increments active custom AG block |
| ENC1 CCW | AG Block previous | Controller-side — decrements active custom AG block |
| ENC1 button | Docked flag toggle | Controller-side — toggles `isDocked` flag for TFT display sync. No Simpit message. |
| ENC2 CW | Next external camera | `KEYBOARD_EMULATOR` `-` (Hullcam VDS Continued) |
| ENC2 CCW | Previous external camera | `KEYBOARD_EMULATOR` `=` (Hullcam VDS Continued) |
| ENC2 button | Reset camera | `KEYBOARD_EMULATOR` Backspace (Hullcam VDS Continued) |

**⚠️ TODO #13:** Verify Hullcam VDS keybindings match in-game settings before use.

---

## Section 4 — Panel B2

**Location:** Right panel

| Module | Type | Address | Notes |
|---|---|---|---|
| Switch Group 2 | 8 latching switches | TBD | Hardware TBD — see Section 4.1 |
| Stability Control | 12 NeoPixel + 2 discrete | 0x23 | |
| Vehicle Control | 12 NeoPixel buttons | 0x24 | No discrete inputs |
| Time Control | 12 NeoPixel buttons | 0x25 | |
| Pre-Warp Time Dial | 7-segment + encoder + 3 buttons | 0x2B | |
| AUX CTRL Module | 12 NeoPixel buttons | 0x26 | EVA B0–B8, CP Toggle B9, CP Dock B10, Cruise Ctrl B11 |
| Rotation Joystick | 3-axis + 2 buttons | 0x28 | |
| Stage Button | Direct wire | — | Momentary, STAGE_ACTION |

### 4.1 — Switch Group 2

**Hardware:** TBD | **Panel:** B2

Physical layout: 2 rows × 4 columns.

```
┌──────────┬──────────┬──────────┬──────────┐
│ CHUTE    │ GEAR     │ BRAKE    │ EXT LT   │
│ SAFE ↑   │ UP ↑     │ REL ↑    │ OFF ↑    │
│ ARM ↓    │ DOWN ↓   │ LOCK ↓   │ ILLUM ↓  │
├──────────┼──────────┼──────────┼──────────┤
│ SAS      │ RCS      │ THC/RHC  │ AUDIO    │
│ OFF ↑    │ OFF ↑    │ NORM ↑   │ MUTE ↑   │
│ ENAB ↓   │ ENAB ↓   │ PREC ↓   │ LIVE ↓   │
└──────────┴──────────┴──────────┴──────────┘
  Col 1      Col 2      Col 3      Col 4
```

Col 1 is nearest panel center.

| # | Title | OFF | ON | Type | Function | Implementation |
|---|---|---|---|---|---|---|
| SW1 | CHUTE | SAFE | ARM | Latching | Parachute deploy interlock | Safety interlock — enables chute deploy actions when ARM |
| SW2 | GEAR | UP | DOWN | Latching | Landing gear | Simpit `GEAR_ACTION` activate/deactivate |
| SW3 | BRAKE | REL | LOCK | Latching | Parking brake | Simpit `BRAKES_ACTION` activate/deactivate |
| SW4 | EXT LT | OFF | ILLUM | Latching | Exterior lights | Simpit `LIGHTS_ACTION` activate/deactivate |
| SW5 | SAS | OFF | ENAB | Latching | Stability Assist System | Simpit `AGACTIVATE_MESSAGE` / `AGDEACTIVATE_MESSAGE` SAS_ACTION |
| SW6 | RCS | OFF | ENAB | Latching | Reaction Control System | Simpit `AGACTIVATE_MESSAGE` / `AGDEACTIVATE_MESSAGE` RCS_ACTION |
| SW7 | THC/RHC | NORM | PREC | Latching | Translation/Rotation input precision | System-wide input scaling |
| SW8 | AUDIO | MUTE | LIVE | Latching | Panel audio output | Enables/disables controller audio |

### 4.2 — Stability Control Module (0x23)

**PCB:** KC-01-1822 | **Type:** 0x04 | **Extended States:** No

```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│ RCS      │ Stab     │ Prograde │ Normal   │ Radial   │ Target   │
│          │ Assist   │          │          │ In       │          │
│ GREEN    │ GREEN    │ GREEN    │ GREEN    │ GREEN    │ GREEN    │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────┤
│ Invert   │ Maneuver │Retrograde│ Anti-    │ Radial   │ Anti-    │
│          │          │          │ Normal   │ Out      │ Target   │
│ AMBER    │ GREEN    │ GREEN    │ GREEN    │ GREEN    │ GREEN    │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
  Col 6(L)   Col 5      Col 4      Col 3      Col 2      Col 1(R)
```

| KBC Index | Function | Color | Implementation |
|---|---|---|---|
| B0 | SAS — Target | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.Target |
| B1 | SAS — Anti-Target | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.AntiTarget |
| B2 | SAS — Radial In | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.RadialIn |
| B3 | SAS — Radial Out | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.RadialOut |
| B4 | SAS — Normal | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.Normal |
| B5 | SAS — Anti-Normal | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.AntiNormal |
| B6 | SAS — Prograde | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.Prograde |
| B7 | SAS — Retrograde | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.Retrograde |
| B8 | SAS — Stability Assist | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.StabilityAssist |
| B9 | SAS — Maneuver | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.Maneuver |
| B10 | RCS | GREEN | Simpit `AGACTIVATE_MESSAGE` / `AGDEACTIVATE_MESSAGE` RCS_ACTION |
| B11 | Invert | AMBER | `KEYBOARD_EMULATOR` F — held while button held |
| B12–B13 | Not installed | — | — |
| B14 | Stage (latching toggle) | — | Rising edge → Simpit `AGACTIVATE_MESSAGE` STAGE_ACTION |
| B15 | Stage Enable (latching toggle + discrete LED) | — | ACTIVE: staging enabled. OFF: staging locked. LED mirrors controller-set state. |

**⚠️ TODO #10:** Firmware sketch still has B12 SAS Enable / B13 RCS Enable discrete inputs. Needs update — remove B12/B13, add RCS at B10 (GREEN).

### 4.3 — Vehicle Control Module (0x24)

**PCB:** KC-01-1822 | **Type:** 0x05 | **Extended States:** Yes

12 NeoPixel buttons only. No discrete inputs.

```
┌──────────────────┬──────────────────┐
│ Lights           │ Brakes           │
│ YELLOW           │ RED              │
├──────────────────┼──────────────────┤
│ Gear             │ Antenna          │
│ GREEN            │ PINK             │
├──────────────────┼──────────────────┤
│ Solar Array      │ Fuel Cell        │
│ GOLD             │ CYAN             │
├──────────────────┼──────────────────┤
│ Radiator         │ Cargo Door       │
│ ORANGE           │ TEAL             │
├──────────────────┼──────────────────┤
│ Ladder           │ Heat Shield ★    │
│ LIME             │ GREEN/RED        │
├──────────────────┼──────────────────┤
│ Drogue Chute ★   │ Main Chute ★     │
│ GREEN/RED        │ GREEN/RED        │
└──────────────────┴──────────────────┘
  Col 1 (left)       Col 2 (right)
```

★ Multi-state buttons — single button cycles through sequential states via state machine.

| KBC Index | R×C | Function | Color | Base CAG | Implementation |
|---|---|---|---|---|---|
| B0 | R1C2 | Brakes | RED | — | Simpit `BRAKES_ACTION` toggle |
| B1 | R1C1 | Lights | YELLOW | — | Simpit `LIGHTS_ACTION` toggle |
| B2 | R2C2 | Antenna | PINK | 13 | Custom AG (AGX) |
| B3 | R2C1 | Gear | GREEN | — | Simpit `GEAR_ACTION` toggle |
| B4 | R3C2 | Fuel Cell | CYAN | 14 | Custom AG (AGX) |
| B5 | R3C1 | Solar Array | GOLD | 15 | Custom AG (AGX) |
| B6 | R4C2 | Cargo Door | TEAL | 16 | Custom AG (AGX) |
| B7 | R4C1 | Radiator | ORANGE | 17 | Custom AG (AGX) |
| B8 | R5C2 | Heat Shield ★ | GREEN (deploy) / RED (release) | 19/20 | State machine — see below |
| B9 | R5C1 | Ladder | LIME | 18 | Custom AG (AGX) |
| B10 | R6C2 | Main Chute ★ | GREEN (deploy) / RED (cut) | 21/22 | State machine — requires CHUTE ARM switch |
| B11 | R6C1 | Drogue Chute ★ | GREEN (deploy) / RED (cut) | 23/24 | State machine — requires CHUTE ARM switch |
| B12–B15 | — | Not installed | — | — | — |

**⚠️ TODO #9:** Firmware sketch still has B12–B15 discrete inputs. Needs update — remove all discrete input handling.

#### Parachute State Machine (B10/B11)

Requires Switch Group 2 CHUTE switch in ARM position. CHUTE flipping to SAFE at any point resets both buttons to OFF.

| State | LED | Action on press |
|---|---|---|
| OFF | Dark | Fire base CAG deploy → DEPLOYED |
| DEPLOYED | GREEN (ACTIVE) | Fire base CAG cut → CUT |
| CUT | RED (ACTIVE) | Terminal state |

Main Chute: base CAG 21 (deploy) / 22 (cut). Drogue Chute: base CAG 23 (deploy) / 24 (cut).

#### Heat Shield State Machine (B8)

Independent — no switch interlock required.

| State | LED | Action on press |
|---|---|---|
| OFF | Dark | Fire base CAG 19 (deploy) → DEPLOYED |
| DEPLOYED | GREEN (ACTIVE) | Fire base CAG 20 (release) → RELEASED |
| RELEASED | RED (ACTIVE) | Terminal state |

### 4.4 — Time Control Module (0x25)

**PCB:** KC-01-1822 | **Type:** 0x06 | **Extended States:** No

```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│ Pause    │ Morning  │ SOI      │ Maneuver │ PeA      │ ApA      │
│ AMBER    │ YELLOW   │ YELLOW   │ YELLOW   │ YELLOW   │ YELLOW   │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────┤
│ Save     │ Load     │ Warp     │ Warp     │ Physics  │ Stop     │
│          │          │ Back     │ Forward  │ Warp     │          │
│ LIME     │ MINT     │ WARM WHT │ WARM WHT │ CHARTRSE │ AMBER    │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
  Col 6(L)   Col 5      Col 4      Col 3      Col 2      Col 1(R)
```

| KBC Index | Function | Color | Implementation |
|---|---|---|---|
| B0 | Warp to Apoapsis | YELLOW | Simpit `TIMEWARP_TO_MESSAGE` ApA − Pre-Warp offset |
| B1 | Warp Stop | AMBER | Simpit `TIMEWARP_MESSAGE` stop |
| B2 | Warp to Periapsis | YELLOW | Simpit `TIMEWARP_TO_MESSAGE` PeA − Pre-Warp offset |
| B3 | Physics Warp (modifier) | CHARTREUSE | Hold + B5/B7 for physics warp increment/decrement |
| B4 | Warp to Maneuver | YELLOW | Simpit `TIMEWARP_TO_MESSAGE` Maneuver − Pre-Warp offset |
| B5 | Warp Forward | WARM WHITE | Simpit `TIMEWARP_MESSAGE` rate increment |
| B6 | Warp to SOI | YELLOW | Simpit `TIMEWARP_TO_MESSAGE` SOI − Pre-Warp offset |
| B7 | Warp Back | WARM WHITE | Simpit `TIMEWARP_MESSAGE` rate decrement |
| B8 | Warp to Morning | YELLOW | Simpit `TIMEWARP_TO_MESSAGE` Morning − Pre-Warp offset |
| B9 | Quick Load | MINT | `KEYBOARD_EMULATOR` F9 |
| B10 | Pause | AMBER | `KEYBOARD_EMULATOR` Escape |
| B11 | Quick Save | LIME | `KEYBOARD_EMULATOR` F5 |
| B12–B15 | Not installed | — | — |

B3 (Physics Warp) is a modifier — hold B3 and press B5 or B7 to change physics warp rate. B3 alone does nothing.

### 4.5 — Pre-Warp Time Dial (0x2B)

**PCB:** KC-01-1881/1882 | **Type:** 0x0C

| Control | Function | Behavior |
|---|---|---|
| 4-digit display | Pre-warp duration | Shows minutes 0–9999, no leading zeros |
| Encoder | Adjust duration | ±1 slow / ±10 medium / ±100 fast |
| Encoder button | Reset to 0 | Clears to default |
| BTN01 (GOLD flash) | 5 min preset | Jumps to 5 |
| BTN02 (GOLD flash) | 1 hour preset | Jumps to 60 |
| BTN03 (GOLD flash) | 1 day preset | Jumps to 1440 |

### 4.6 — AUX CTRL Module (0x26)

**PCB:** KC-01-1852 | **Type:** 0x07 | **Hardware:** 12-button NeoPixel module

EVA functions (B0–B7) active only when `isEVA = true`. General flight functions (B8–B11) active in all modes.

```
┌──────────┬──────────┬──────────┬──────────┬──────────┬──────────┐
│ CP       │ Cruise   │ EVA      │ Jump /   │ Board    │ EVA      │
│ Toggle   │ Control  │ Chute    │ Let Go   │ Craft    │ Lights   │
│ ROSE/CRL │ GOLD     │ AMBER    │ CHARTRSE │ GREEN    │ MINT     │
├──────────┼──────────┼──────────┼──────────┼──────────┼──────────┤
│ CP       │ Plant    │ Helmet   │ Grab     │ EVA      │ Jetpack  │
│ Dock Port│ Flag     │ Toggle   │          │ Constr.  │ Enable   │
│ PINK     │ SEAFOAM★ │ SKY      │ SEAFOAM  │ TEAL     │ LIME     │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
  Col 6(L)   Col 5      Col 4      Col 3      Col 2      Col 1(R)
```

★ Plant Flag uses SEAFOAM flash — brief illuminate on press, returns to dim white.

| KBC Index | Col | Function | Color | Base CAG | Active when | Implementation |
|---|---|---|---|---|---|---|
| B0 | 1 top | EVA Lights | MINT | — | `isEVA = true` | `KEYBOARD_EMULATOR` U |
| B1 | 1 bottom | Jetpack Enable | LIME | — | `isEVA = true` | `KEYBOARD_EMULATOR` R |
| B2 | 2 top | Board Craft | GREEN | — | `isEVA = true` | `KEYBOARD_EMULATOR` B |
| B3 | 2 bottom | EVA Construction | TEAL | — | `isEVA = true` | `KEYBOARD_EMULATOR` I |
| B4 | 3 top | Jump / Let Go | CHARTREUSE | — | `isEVA = true` | `KEYBOARD_EMULATOR` Space |
| B5 | 3 bottom | Grab | SEAFOAM | — | `isEVA = true` | `KEYBOARD_EMULATOR` F |
| B6 | 4 top | EVA Chute | AMBER | — | `isEVA = true` | `KEYBOARD_EMULATOR` P |
| B7 | 4 bottom | Helmet Toggle | SKY | — | `isEVA = true` | `KEYBOARD_EMULATOR` O |
| B8 | 5 top | Cruise Control | GOLD | — | Rover mode | Controller-side — holds current wheel throttle. Press to set, press again to release. |
| B9 | 5 bottom | Plant Flag | SEAFOAM (flash) | — | `isEVA = true` | `KEYBOARD_EMULATOR` G |
| B10 | 6 top | CP Toggle (PRI/ALT) | ROSE (primary) / CORAL (alternate) | 35/36 | All modes | Cycles between base CAG 35 and 36 — mutually exclusive |
| B11 | 6 bottom | CP Docking Port | PINK | 37 | All modes | Deactivates PRI/ALT, activates base CAG 37 |

#### Control Point Logic

Three mutually exclusive states. Transitions always deactivate previous CAG before activating new one. All CAG numbers adjust by active control group offset.

| State | Button | Color | Base CAG |
|---|---|---|---|
| Primary | B10 | ROSE | 35 |
| Alternate | B10 | CORAL | 36 |
| Docking Port | B11 | PINK | 37 |

#### Plant Flag (B8)

Flash behavior — SEAFOAM illuminates briefly on press then returns to dim white ENABLED state. Only active when `isEVA = true`. Only functional when Kerbal is standing on a surface.

### 4.7 — Rotation Joystick (0x28)

**PCB:** KC-01-1831/1832 | **Type:** 0x09

Four operating modes. Three selected by Ctrl Mode Toggle (Section 2.2). EVA auto-activated when `isEVA = true`.

**Spacecraft Mode:**

| Axis | Physical | Simpit Field |
|---|---|---|
| AXIS1 | Left / Right | `ROTATION_MESSAGE` yaw |
| AXIS2 | Forward / Back | `ROTATION_MESSAGE` pitch |
| AXIS3 | CCW / CW | `ROTATION_MESSAGE` roll |

**Airplane Mode:**

| Axis | Physical | Simpit Field |
|---|---|---|
| AXIS1 | Left / Right | `ROTATION_MESSAGE` roll |
| AXIS2 | Forward / Back | `ROTATION_MESSAGE` pitch |
| AXIS3 | CCW / CW | `ROTATION_MESSAGE` yaw |

**Rover Mode:** Routes to `WHEEL_MESSAGE`. AXIS3 unused.

**EVA Mode (auto — `isEVA` = true):** Digital key emulation.

| Axis | KSP Action | Key |
|---|---|---|
| AXIS1 left/right | Walk Left / Right | A / D (held) |
| AXIS2 forward/back | Walk Forward / Back | W / S (held) |
| AXIS3 CCW/CW | Jetpack Rotate Left / Right | Q / E (held) |
| BTN_JOY held | Run | Left Shift (held) |

| Button | Function | Color | Base CAG |
|---|---|---|---|
| BTN01 | Reset Trim | GREEN | — |
| BTN02 | Set Trim | AMBER | — |
| BTN_JOY | Airbrake toggle (non-EVA) / Run hold (EVA) | 38 | Intentional duplicate of Function Control B4 — ergonomic redundancy for atmospheric flight |

---

## Section 5 — I2C Address Summary

| Address | Module | Type ID | Panel | Status |
|---|---|---|---|---|
| 0x20 | UI Control | 0x01 | A2 | Active |
| 0x21 | Function Control | 0x02 | A2 | Active — firmware TODO #7 |
| 0x22 | Action Control | 0x03 | A2 | Active — firmware TODO #8 |
| 0x23 | Stability Control | 0x04 | B2 | Active — firmware TODO #10 |
| 0x24 | Vehicle Control | 0x05 | B2 | Active — firmware TODO #9 |
| 0x25 | Time Control | 0x06 | B2 | Active |
| 0x26 | AUX CTRL | 0x07 | B2 | Active — fully allocated |
| 0x27 | Reserved | — | — | — |
| 0x28 | Rotation Joystick | 0x09 | B2 | Active |
| 0x29 | Translation Joystick | 0x0A | A2 | Active |
| 0x2A | GPWS Input Panel | 0x0B | A1 | Active |
| 0x2B | Pre-Warp Time Dial | 0x0C | B2 | Active |
| 0x2C | Throttle Module | 0x0D | A2 | Active |
| 0x2D | Dual Encoder | 0x0E | B1 | Active |
| 0x2E | Switch Groups 1+2 | 0x0F | A2 / B2 | Hardware TBD |
| 0x2F | (was Indicator) | — | — | **Removed from design** |
| TBD | Ctrl Module | TBD | TBD | Deferred |
| TBD | Ctrl Ext Module | TBD | TBD | Deferred |

---

## Section 6 — Removed from Design

The following items existed in v4.0 but have been removed:

| Item | Reason |
|---|---|
| Indicator Module (0x2F) | Entire module removed — all annunciator pixels dropped |
| ABORT indicator | Removed with Indicator module — see TODO #6 |
| CTRL / DEBUG / DEMO indicators | Removed with Indicator module — see TODO #6 |
| SWITCH ERROR indicator | Removed with Indicator module — see TODO #6 |
| COMM ACTIVE indicator | Removed with Indicator module — see TODO #6 |
| THRTL ENA / THRTL PREC / PREC INPUT / AUDIO / SCE AUX / RCS / SAS / CHUTE ARM / LNDG GEAR LOCK / LIGHT ENA / BRAKE LOCK indicators | Removed with Indicator module |
| AUTO PILOT indicator (B17) | Was deferred TBD — removed with Indicator module |
| MODE switch (SW1+SW2 CTRL/DBG/DEMO) | Removed — SCE and UPTLM switches serve related functions |
| Stability Control B12 SAS Enable | Moved to Switch Group 2 SW5 |
| Stability Control B13 RCS Enable | Moved to Switch Group 2 SW6 / Stability Control B10 button |
| Vehicle Control B12 Parking Brake | Moved to Switch Group 2 SW3 (BRAKE) |
| Vehicle Control B13 Parachutes Armed | Moved to Switch Group 2 SW1 (CHUTE) |
| Vehicle Control B14 Lights Lock | Moved to Switch Group 2 SW4 (EXT LT) |
| Vehicle Control B15 Gear Lock | Moved to Switch Group 2 SW2 (GEAR) |
| Round Display | Removed from B2 |
| Action Control B12 Spacecraft Mode discrete input | Moved to Ctrl Mode Toggle direct wire |
| Action Control B14 Rover Mode discrete input | Moved to Ctrl Mode Toggle direct wire |

---

## Section 7 — Operator Feedback Review (TODO #6)

**Status: Open design item.**

With the Indicator module removed, the following operator feedback mechanisms no longer exist as dedicated panel indicators. Each needs a deliberate decision: screen-based feedback is sufficient, a new hardware indicator is needed, or the feedback is not required.

| Lost indicator | Feedback needed? | Notes |
|---|---|---|
| ABORT active | High — critical safety feedback | Was flashing red on Indicator B15 |
| CTRL / DEBUG / DEMO mode | Medium — operator needs to know current mode | Was B2/B5/B8 on Indicator |
| SWITCH ERROR | Medium — mismatch between toggle state and game state | Was B14 on Indicator |
| COMM ACTIVE (Simpit connected) | Medium — operator needs to know controller is talking to game | Was B11 on Indicator |
| THRTL ENA | Low — throttle arm state visible on ENGINE switch | — |
| SAS active | Low — SAS mode buttons on Stability Control provide context | — |
| RCS active | Low — RCS button on Stability Control provides context | — |
| CHUTE ARM | Low — CHUTE switch position is self-evident | — |
| GEAR LOCK / BRAKE LOCK / LIGHT ENA | Low — switch positions are self-evident | — |

Recommendation: Review TFT screen designs to confirm ABORT, mode state, and COMM ACTIVE are surfaced on at least one display before closing this item.

> **PLACEHOLDER** — Replace this file with `KCMk1_Panel_Interface_Guide.md`
> from `/mnt/user-data/outputs/` in the ATtiny816 module review session
