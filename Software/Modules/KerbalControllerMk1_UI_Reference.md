# Kerbal Controller Mk1 — Module UI Reference

**Jeb's Controller Works** | v4.0 | 2026-04-09

This document covers every user-facing control and indicator on each module. Arranged by module in I2C address order. Panel layouts use the physical orientation as seen from the front of the panel.

---

## Required Mods

The following mods are required for full controller functionality:

| Mod | Purpose |
|---|---|
| **AGX (Action Groups Extended)** | Enables custom action groups beyond the standard AG1–AG10. Required for all CAG assignments. The controller uses AGX groups 1–238 across 6 control groups. See the Custom AG Assignment Table in this document. |
| **Hullcam VDS Continued** | Adds mountable external cameras to vessels. Required for Dual Encoder ENC2 camera cycling functionality. Default keybindings: `-` (next), `=` (previous), `Backspace` (reset). |

Users must install both mods and configure the appropriate in-game action group assignments per the custom AG assignment table in this document.

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

## Custom AG Assignment Table

All custom action groups use AGX numbers. The controller organizes assignments into 6 control groups, each offset by 40. ENC1 on the Dual Encoder module selects the active control group.

### Control Groups

| Group | Name | Usage |
|---|---|---|
| 1 | Default Group | Default for main craft — used with a single craft |
| 2 | Lander Group #1 | Primary or single lander |
| 3 | Lander Group #2 | Secondary lander |
| 4 | Custom Group | Additional group available for user-defined use |
| 5 | Re-Entry Vehicle Group | Re-entry craft attached to a space station or exploration craft |
| 6 | Space Station Group | Space station control set |

**Offset formula:** AGX number = Base CAG + ((Group − 1) × 40)

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
| 27 | Engine Alt Mode | Function Control | B2 | 27 | 67 | 107 | 147 | 187 | 227 |
| 28 | Science Collect | Function Control | B3 | 28 | 68 | 108 | 148 | 188 | 228 |
| 29 | Engine Group 1 | Function Control | B4 | 29 | 69 | 109 | 149 | 189 | 229 |
| 30 | Science Group 1 | Function Control | B5 | 30 | 70 | 110 | 150 | 190 | 230 |
| 31 | Engine Group 2 | Function Control | B6 | 31 | 71 | 111 | 151 | 191 | 231 |
| 32 | Science Group 2 | Function Control | B7 | 32 | 72 | 112 | 152 | 192 | 232 |
| 33 | Air Intake | Function Control | B8 | 33 | 73 | 113 | 153 | 193 | 233 |
| 34 | Lock Surfaces | Function Control | B9 | 34 | 74 | 114 | 154 | 194 | 234 |
| 35 | CP Primary | Function Control | B10 | 35 | 75 | 115 | 155 | 195 | 235 |
| 36 | CP Alternate | Function Control | B10 | 36 | 76 | 116 | 156 | 196 | 236 |
| 37 | CP Docking Port | Function Control | B11 | 37 | 77 | 117 | 157 | 197 | 237 |
| 38 | Airbrake Toggle | Rotation Module | BTN_JOY | 38 | 78 | 118 | 158 | 198 | 238 |

### Notes

- State machine pairs (Heat Shield, Main Chute, Drogue Chute) use sequential base CAGs — deploy is always the lower number, cut/release always the higher. Within a block the sequence relationship is preserved.
- CP Primary and CP Alternate (base 35/36) share button B10 on Function Control — they are mutually exclusive and the controller activates only one at a time.
- Maximum AGX number used: 238 (well within AGX 250 limit).
- The 2-slot gap between groups (39–40, 79–80, etc.) provides headroom for future additions without renumbering.

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
│ IVA      │ Ship     │ Ship     │ Navball  │ Nav Ball │ Debug    │
│          │ Back     │ Forward  │ Ref Cycle│ Toggle   │          │
│ CORAL    │ TEAL     │ TEAL     │ GREEN    │ AMBER    │ RED      │
└──────────┴──────────┴──────────┴──────────┴──────────┴──────────┘
  Col 6(L)   Col 5      Col 4      Col 3      Col 2      Col 1(R)
```

### Button Reference

| KBC Index | PCB Label | Function | ACTIVE Color | Implementation |
|---|---|---|---|---|
| 0 | BUTTON01 | Screenshot | GREEN | Macro: F2 (hide UI) → F1 (screenshot) → F2 (restore UI) |
| 1 | BUTTON02 | Debug | RED | `KEYBOARD_EMULATOR` Alt+F12 |
| 2 | BUTTON03 | UI Toggle | GREEN | `KEYBOARD_EMULATOR` F2 |
| 3 | BUTTON04 | Nav Ball Toggle | AMBER | `KEYBOARD_EMULATOR` Numpad . (numpad delete) |
| 4 | BUTTON05 | Map Reset | SKY | `KEYBOARD_EMULATOR` ' (apostrophe) |
| 5 | BUTTON06 | Navball Reference Cycle | GREEN | Simpit `NAVBALLMODE_MESSAGE` (channel 28) — cycles Surface → Orbit → Target |
| 6 | BUTTON07 | Map Forward | SKY | `KEYBOARD_EMULATOR` Tab |
| 7 | BUTTON08 | Ship Forward | TEAL | `KEYBOARD_EMULATOR` ] |
| 8 | BUTTON09 | Map Back | SKY | `KEYBOARD_EMULATOR` Shift+Tab |
| 9 | BUTTON10 | Ship Back | TEAL | `KEYBOARD_EMULATOR` [ |
| 10 | BUTTON11 | Map Enable | SKY | `KEYBOARD_EMULATOR` M |
| 11 | BUTTON12 | IVA | CORAL | `KEYBOARD_EMULATOR` C |
| 12–15 | BUTTON13–16 | Not installed | — | — |

### Notes

**Screenshot (B0):** Three-step macro — the controller must send F2 (hide UI), wait a short interval, send F1 (capture), wait again, then send F2 (restore UI). A clean frameless screenshot results without requiring the player to hold any keys manually.

**Debug (B1):** Alt+F12 opens the KSP debug console. The `KEYBOARD_EMULATOR` modifier byte carries the Alt key flag alongside F12.

**Nav Ball Toggle (B3):** Uses Numpad `.` (the numpad delete/decimal key), not the main keyboard delete key. These are distinct keycodes.

**Map Reset (B4):** Apostrophe `'` resets the map camera to re-center on the focused object.

**Navball Reference Cycle (B5):** Sends Simpit's `NAVBALLMODE_MESSAGE` (channel 28) to cycle the navball reference frame between Surface, Orbit, and Target. This is distinct from Nav Ball Toggle (B3) which shows/hides the navball widget. Reference frame cycling is critical during rendezvous (switch to Target) and landing approach (switch to Surface).

**Map Forward / Map Back (B6/B8):** Tab and Shift+Tab cycle map focus between objects in the orbital map view. Important during rendezvous for switching map center between your vessel and the target.

**Ship Forward / Ship Back (B7/B9):** `]` and `[` cycle the active controlled vessel — this changes which vessel you are piloting, not just which vessel the camera follows.

---

## 0x21 — Function Control Module

**PCB:** KC-01-1822 | **Type:** 0x02 | **Extended States:** No

12 NeoPixel buttons in a 6×2 grid. No discrete LED buttons installed. All buttons send Custom Action Group (AGX) commands. Layout follows mission timeline — launch actions top, functional systems bottom.

### Panel Layout

```
┌──────────────────┬──────────────────┐
│ LES              │ Fairing Jettison │
│ RED              │ AMBER            │
├──────────────────┼──────────────────┤
│ Engine Alt Mode  │ Science Collect  │
│ ORANGE           │ PURPLE           │
├──────────────────┼──────────────────┤
│ Engine Group 1   │ Science Group 1  │
│ YELLOW           │ INDIGO           │
├──────────────────┼──────────────────┤
│ Engine Group 2   │ Science Group 2  │
│ CHARTREUSE       │ BLUE             │
├──────────────────┼──────────────────┤
│ Air Intake       │ Lock Surfaces    │
│ TEAL             │ SKY              │
├──────────────────┼──────────────────┤
│ CP Toggle        │ CP Docking Port  │
│ ROSE/CORAL       │ PINK             │
└──────────────────┴──────────────────┘
  Col 1 (left)       Col 2 (right)
```

### Button Reference

| KBC Index | PCB Label | Function | ACTIVE Color | Base CAG | Implementation |
|---|---|---|---|---|---|
| 0 | BUTTON01 | LES (Launch Escape System) | RED | 25 | Custom AG (AGX) |
| 1 | BUTTON02 | Fairing Jettison | AMBER | 26 | Custom AG (AGX) |
| 2 | BUTTON03 | Engine Alt Mode | ORANGE | 27 | Custom AG (AGX) |
| 3 | BUTTON04 | Science Collect | PURPLE | 28 | Custom AG (AGX) |
| 4 | BUTTON05 | Engine Group 1 | YELLOW | 29 | Custom AG (AGX) |
| 5 | BUTTON06 | Science Group 1 | INDIGO | 30 | Custom AG (AGX) |
| 6 | BUTTON07 | Engine Group 2 | CHARTREUSE | 31 | Custom AG (AGX) |
| 7 | BUTTON08 | Science Group 2 | BLUE | 32 | Custom AG (AGX) |
| 8 | BUTTON09 | Air Intake | TEAL | 33 | Custom AG (AGX) |
| 9 | BUTTON10 | Lock Surfaces | SKY | 34 | Custom AG (AGX) |
| 10 | BUTTON11 | Control Point Toggle (PRI/ALT) | ROSE (primary) / CORAL (alternate) | 35/36 | Cycles between base CAG 35 (PRI) and 36 (ALT) — mutually exclusive |
| 11 | BUTTON12 | Control Point Docking Port | PINK | 37 | Deactivates PRI/ALT CAG, activates base CAG 37 |
| 12–15 | BUTTON13–16 | Not installed | — | — | — |

### Control Point Logic

B10 and B11 manage which part of the vessel is the control reference point. Three states tracked by the controller:

| State | B10/B11 Color | Base CAG | Meaning |
|---|---|---|---|
| Primary | ROSE (B10) | 35 | Primary control point active |
| Alternate | CORAL (B10) | 36 | Alternate control point active |
| Docking Port | PINK (B11) | 37 | Docking port control point active |

Transitions always deactivate the previous CAG before activating the new one. All three are mutually exclusive. All CAG numbers adjust by active control group offset.

### Notes

**Layout rationale:** Top row (LES, Fairing Jettison) contains launch-sequence actions. Engine and Science families occupy the middle rows in left/right columns. Air Intake and Lock Surfaces are aerodynamic/flight systems. Control Point buttons at bottom manage vessel control reference.

---

## 0x22 — Action Control Module

**PCB:** KC-01-1822 | **Type:** 0x03 | **Extended States:** No

12 NeoPixel buttons (AG1–AG12) in a 2×6 grid. Two discrete input positions carry spacecraft and rover mode detection signals — no LED outputs. Control Point buttons moved to Function Control module.

### Panel Layout

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

Panel reads right-to-left: AG1 top-right, AG12 bottom-left.

### Button Reference

| KBC Index | PCB Label | Panel Position | Function | ACTIVE Color | Base CAG | Implementation |
|---|---|---|---|---|---|---|
| 0 | BUTTON01 | Col 1 top | AG6 | GREEN | 6 | Simpit `CAGTOGGLE_MESSAGE` |
| 1 | BUTTON02 | Col 1 bottom | AG12 | GREEN | 12 | Simpit `CAGTOGGLE_MESSAGE` |
| 2 | BUTTON03 | Col 2 top | AG5 | GREEN | 5 | Simpit `CAGTOGGLE_MESSAGE` |
| 3 | BUTTON04 | Col 2 bottom | AG11 | GREEN | 11 | Simpit `CAGTOGGLE_MESSAGE` |
| 4 | BUTTON05 | Col 3 top | AG4 | GREEN | 4 | Simpit `CAGTOGGLE_MESSAGE` |
| 5 | BUTTON06 | Col 3 bottom | AG10 | GREEN | 10 | Simpit `CAGTOGGLE_MESSAGE` |
| 6 | BUTTON07 | Col 4 top | AG3 | GREEN | 3 | Simpit `CAGTOGGLE_MESSAGE` |
| 7 | BUTTON08 | Col 4 bottom | AG9 | GREEN | 9 | Simpit `CAGTOGGLE_MESSAGE` |
| 8 | BUTTON09 | Col 5 top | AG2 | GREEN | 2 | Simpit `CAGTOGGLE_MESSAGE` |
| 9 | BUTTON10 | Col 5 bottom | AG8 | GREEN | 8 | Simpit `CAGTOGGLE_MESSAGE` |
| 10 | BUTTON11 | Col 6 top | AG1 | GREEN | 1 | Simpit `CAGTOGGLE_MESSAGE` |
| 11 | BUTTON12 | Col 6 bottom | AG7 | GREEN | 7 | Simpit `CAGTOGGLE_MESSAGE` |
| 12 | BUTTON13 | — | Spacecraft Mode (input only) | — | — | Controller-side mode switch — no Simpit message |
| 13 | BUTTON14 | — | Not installed | — | — | — |
| 14 | BUTTON15 | — | Rover Mode (input only) | — | — | Controller-side mode switch — no Simpit message |
| 15 | BUTTON16 | — | Not installed | — | — | — |

### Mode Switch (B12/B14)

B12 and B14 are the two contacts of a single 3-position latching physical switch. They set the controller's internal panel control mode — no Simpit messages are sent on switch change.

| B12 (Spacecraft) | B14 (Rover) | Mode | Effect |
|---|---|---|---|
| 0 | 0 | Aircraft | Rotation stick: roll=AXIS1, pitch=AXIS2, yaw=AXIS3 |
| 1 | 0 | Spacecraft | Rotation stick: yaw=AXIS1, pitch=AXIS2, roll=AXIS3 |
| 0 | 1 | Rover | Rotation stick routes to `WHEEL_MESSAGE` |

EVA mode overrides all switch positions when `isEVA = true`.

### Notes

All 12 action group buttons rotate with the active control group selected by ENC1 on the Dual Encoder module. The AGX number sent = Base CAG + ((Group − 1) × 40). AG1 in Group 1 sends AGX 1, AG1 in Group 2 sends AGX 41, etc.

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

| KBC Index | PCB Label | Function | ACTIVE Color | Implementation |
|---|---|---|---|---|
| 0 | BUTTON01 | SAS — Target | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.Target |
| 1 | BUTTON02 | SAS — Anti-Target | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.AntiTarget |
| 2 | BUTTON03 | SAS — Radial In | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.RadialIn |
| 3 | BUTTON04 | SAS — Radial Out | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.RadialOut |
| 4 | BUTTON05 | SAS — Normal | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.Normal |
| 5 | BUTTON06 | SAS — Anti-Normal | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.AntiNormal |
| 6 | BUTTON07 | SAS — Prograde | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.Prograde |
| 7 | BUTTON08 | SAS — Retrograde | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.Retrograde |
| 8 | BUTTON09 | SAS — Stability Assist | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.StabilityAssist |
| 9 | BUTTON10 | SAS — Maneuver | GREEN | Simpit `SAS_MODE_MESSAGE` AutopilotMode.Maneuver |
| 10 | BUTTON11 | Not installed | — | — |
| 11 | BUTTON12 | Invert | AMBER | `KEYBOARD_EMULATOR` F — held while button held, released when button released |
| 12 | BUTTON13 | SAS Enable (latching toggle) | — | HIGH → Simpit `AGACTIVATE_MESSAGE` SAS_ACTION / LOW → `AGDEACTIVATE_MESSAGE` SAS_ACTION |
| 13 | BUTTON14 | RCS Enable (latching toggle) | — | HIGH → Simpit `AGACTIVATE_MESSAGE` RCS_ACTION / LOW → `AGDEACTIVATE_MESSAGE` RCS_ACTION |
| 14–15 | BUTTON15–16 | Not installed | — | — |

---

## 0x24 — Vehicle Control Module

**PCB:** KC-01-1822 | **Type:** 0x05 | **Extended States:** Yes

12 NeoPixel buttons in a 6×2 grid. Ordered top-to-bottom through phases of flight. Parachute and heat shield buttons support extended LED states via state machines. Four discrete inputs carry vehicle state signals — no LED outputs.

### Panel Layout

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

### Button Reference

| KBC Index | PCB Label | R×C | Function | ACTIVE Color | Base CAG | Implementation |
|---|---|---|---|---|---|---|
| 0 | BUTTON01 | R1C2 | Brakes | RED | — | Simpit `BRAKES_ACTION` toggle |
| 1 | BUTTON02 | R1C1 | Lights | YELLOW | — | Simpit `LIGHTS_ACTION` toggle |
| 2 | BUTTON03 | R2C2 | Antenna | PINK | 13 | Custom AG (AGX) |
| 3 | BUTTON04 | R2C1 | Gear | GREEN | — | Simpit `GEAR_ACTION` toggle |
| 4 | BUTTON05 | R3C2 | Fuel Cell | CYAN | 14 | Custom AG (AGX) |
| 5 | BUTTON06 | R3C1 | Solar Array | GOLD | 15 | Custom AG (AGX) |
| 6 | BUTTON07 | R4C2 | Cargo Door | TEAL | 16 | Custom AG (AGX) |
| 7 | BUTTON08 | R4C1 | Radiator | ORANGE | 17 | Custom AG (AGX) |
| 8 | BUTTON09 | R5C2 | Heat Shield ★ | GREEN (deploy) / RED (release) | 19/20 | State machine — see below |
| 9 | BUTTON10 | R5C1 | Ladder | LIME | 18 | Custom AG (AGX) |
| 10 | BUTTON11 | R6C2 | Main Chute ★ | GREEN (deploy) / RED (cut) | 21/22 | State machine — requires B13 armed |
| 11 | BUTTON12 | R6C1 | Drogue Chute ★ | GREEN (deploy) / RED (cut) | 23/24 | State machine — requires B13 armed |
| 12 | BUTTON13 | — | Parking Brake (latching toggle) | — | — | HIGH → Simpit `AGACTIVATE_MESSAGE` BRAKES_ACTION / LOW → `AGDEACTIVATE_MESSAGE` BRAKES_ACTION |
| 13 | BUTTON14 | — | Parachutes Armed (latching toggle) | — | — | Safety interlock — enables B10/B11 state machines when HIGH |
| 14 | BUTTON15 | — | Lights Lock (latching toggle) | — | — | HIGH → Simpit `AGACTIVATE_MESSAGE` LIGHTS_ACTION / LOW → `AGDEACTIVATE_MESSAGE` LIGHTS_ACTION |
| 15 | BUTTON16 | — | Gear Lock (latching toggle) | — | — | HIGH → Simpit `AGACTIVATE_MESSAGE` GEAR_ACTION / LOW → `AGDEACTIVATE_MESSAGE` GEAR_ACTION |

### Parachute State Machine (B10/B11 — requires B13 armed)

B13 (Parachutes Armed) is a continuous safety interlock. All parachute actions require B13 HIGH. Flipping B13 LOW at any point resets the entire sequence and returns both buttons to OFF.

**Main Chute (B10):**

| State | LED | Condition | Action on press |
|---|---|---|---|
| OFF | Dark | B13 LOW | None |
| ARMED | Cyan | B13 HIGH | Fire base CAG 21 (deploy) → transition to DEPLOYED |
| DEPLOYED | Green (ACTIVE) | After deploy press | Fire base CAG 22 (cut) → transition to CUT |
| CUT | Red (ACTIVE) | After cut press | Terminal state |

**Drogue Chute (B11):** Identical state machine — base CAG 23 (deploy) and base CAG 24 (cut).

### Heat Shield State Machine (B8)

Independent of B13 interlock. Single button cycles through three states sequentially:

| State | LED | Action on press |
|---|---|---|
| OFF | Dark | Fire base CAG 19 (deploy) → transition to DEPLOYED |
| DEPLOYED | Green (ACTIVE) | Fire base CAG 20 (release) → transition to RELEASED |
| RELEASED | Red (ACTIVE) | Terminal state |

### Switch Error Detection

The controller continuously compares physical latching toggle states (B12–B15) against game-reported state from Simpit `ACTIONSTATUS_MESSAGE`. Any mismatch — for example after switching vessels — illuminates SWITCH ERROR (Indicator B14). Monitored toggles:

| Toggle | Game state checked |
|---|---|
| B12 Parking Brake | `BRAKES_ACTION` |
| B14 Lights Lock | `LIGHTS_ACTION` |
| B15 Gear Lock | `GEAR_ACTION` |
| Stability B12 SAS Enable | `SAS_ACTION` |
| Stability B13 RCS Enable | `RCS_ACTION` |

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

| KBC Index | PCB Label | Function | ACTIVE Color | Implementation |
|---|---|---|---|---|
| 0 | BUTTON01 | Warp to Apoapsis | YELLOW | Simpit `TIMEWARP_TO_MESSAGE` ApA − Pre-Warp offset |
| 1 | BUTTON02 | Warp Stop | AMBER | Simpit `TIMEWARP_MESSAGE` stop (rate = 1×) |
| 2 | BUTTON03 | Warp to Periapsis | YELLOW | Simpit `TIMEWARP_TO_MESSAGE` PeA − Pre-Warp offset |
| 3 | BUTTON04 | Physics Warp (modifier) | CHARTREUSE | Held with B5/B7 → Simpit `TIMEWARP_MESSAGE` physics warp increment/decrement |
| 4 | BUTTON05 | Warp to Maneuver | YELLOW | Simpit `TIMEWARP_TO_MESSAGE` Maneuver − Pre-Warp offset |
| 5 | BUTTON06 | Warp Forward | WARM WHITE | Simpit `TIMEWARP_MESSAGE` rate increment |
| 6 | BUTTON07 | Warp to SOI | YELLOW | Simpit `TIMEWARP_TO_MESSAGE` SOI − Pre-Warp offset |
| 7 | BUTTON08 | Warp Back | WARM WHITE | Simpit `TIMEWARP_MESSAGE` rate decrement |
| 8 | BUTTON09 | Warp to Morning | YELLOW | Simpit `TIMEWARP_TO_MESSAGE` Morning − Pre-Warp offset |
| 9 | BUTTON10 | Quick Load | MINT | `KEYBOARD_EMULATOR` F9 |
| 10 | BUTTON11 | Pause | AMBER | `KEYBOARD_EMULATOR` Escape |
| 11 | BUTTON12 | Quick Save | LIME | `KEYBOARD_EMULATOR` F5 |
| 12–15 | BUTTON13–16 | Not installed | — | — |

### Notes

**Physics Warp (B3):** B3 is a modifier, not a standalone toggle. Hold B3 and press B5 (Warp Forward) or B7 (Warp Back) to increment or decrement the physics warp rate. B3 alone does nothing.

**Warp to events (B0/B2/B4/B6/B8):** The controller reads the current value from the Pre-Warp Time Module (0x2B) and uses it as the lead-time offset in minutes before the target event. A Pre-Warp value of 0 warps directly to the event.

---

## 0x26 — EVA Module

**PCB:** KC-01-1852 | **Type:** 0x07 | **Extended States:** No

8 NeoPixel buttons (WS2811 RGB) via direct GPIO — no shift registers. 2 rotary encoder headers (ENC1, ENC2) present on PCB but not yet implemented. Expanded to 2×4 grid with EVA Chute and Helmet Toggle additions.

### Panel Layout

```
┌──────────────┬──────────────┬──────────────┬──────────────┐
│ EVA Lights   │ Board Craft  │ Jump/Let Go  │ EVA Chute    │
│ MINT         │ GREEN        │ CHARTREUSE   │ AMBER        │
├──────────────┼──────────────┼──────────────┼──────────────┤
│ Jetpack      │ EVA          │ Grab         │ Helmet       │
│ Enable       │ Construction │              │ Toggle       │
│ LIME         │ TEAL         │ SEAFOAM      │ SKY          │
└──────────────┴──────────────┴──────────────┴──────────────┘
  Col 1          Col 2          Col 3          Col 4
```

### Button Reference

| Index | Function | ACTIVE Color | Implementation |
|---|---|---|---|
| 0 | EVA Lights | MINT | `KEYBOARD_EMULATOR` U |
| 1 | Jetpack Enable | LIME | `KEYBOARD_EMULATOR` R |
| 2 | Board Craft | GREEN | `KEYBOARD_EMULATOR` B |
| 3 | EVA Construction | TEAL | `KEYBOARD_EMULATOR` I |
| 4 | Jump / Let Go | CHARTREUSE | `KEYBOARD_EMULATOR` Space |
| 5 | Grab | SEAFOAM | `KEYBOARD_EMULATOR` F |
| 6 | EVA Chute | AMBER | `KEYBOARD_EMULATOR` P |
| 7 | Helmet Toggle | SKY | `KEYBOARD_EMULATOR` O |

### Notes
- No shift registers — buttons wired direct to GPIO
- NeoPixel on Port C (PC0), not Port A as on KBC standard modules
- Encoder headers H1 and H2 present but not populated
- All buttons active only when `isEVA = true` — controller should suppress EVA module inputs during normal flight
- EVA Chute (AMBER) signals safety-critical action — deploy parachute for atmospheric EVA or landing
- Helmet Toggle allows sealing/unsealing suit in breathable atmospheres (Kerbin, Laythe)

---

## 0x28 — Joystick Rotation Module

**PCB:** KC-01-1831/1832 | **Type:** 0x09 | **Capability:** JOYSTICK

Three-axis hall-effect joystick. Two NeoPixel buttons. Reports signed INT16 axis values calibrated to center at startup.

The rotation stick has four operating modes. Three are selected by the spacecraft/aircraft/rover mode switch on the Action Control module (bits 0+1 of address 0x22). The fourth — EVA — is triggered automatically by the controller when Simpit reports `isEVA = true` in `FLIGHT_STATUS_MESSAGE`, overriding the physical switch regardless of its position.

### Mode Overview

| Mode | Trigger | Simpit Channel | BTN_JOY |
|---|---|---|---|
| Spacecraft | SPC switch position | `ROTATION_MESSAGE` | Toggle Airbrake |
| Airplane | AIR switch position | `ROTATION_MESSAGE` | Toggle Airbrake |
| Rover | RVR switch position | `WHEEL_MESSAGE` | Toggle Airbrake |
| EVA | `isEVA` flag from Simpit (auto) | `KEYBOARD_EMULATOR` | Run (hold) |

### Spacecraft Mode (Figure 3-14)

| Axis | Packet Field | Physical | KSP Function | Simpit Field |
|---|---|---|---|---|
| AXIS1 | Bytes 2–3 | Left / Right | Yaw Left / Yaw Right | `ROTATION_MESSAGE` yaw |
| AXIS2 | Bytes 4–5 | Forward / Back | Pitch Down / Pitch Up | `ROTATION_MESSAGE` pitch |
| AXIS3 | Bytes 6–7 | CCW / CW | Roll Left / Roll Right | `ROTATION_MESSAGE` roll |

### Airplane Mode (Figure 3-13)

| Axis | Packet Field | Physical | KSP Function | Simpit Field |
|---|---|---|---|---|
| AXIS1 | Bytes 2–3 | Left / Right | Roll Left / Roll Right | `ROTATION_MESSAGE` roll |
| AXIS2 | Bytes 4–5 | Forward / Back | Pitch Down / Pitch Up | `ROTATION_MESSAGE` pitch |
| AXIS3 | Bytes 6–7 | CCW / CW | Yaw Left / Yaw Right | `ROTATION_MESSAGE` yaw |

Airplane and Spacecraft modes differ only in the assignment of AXIS1 and AXIS3 — roll and yaw are swapped. Pitch (AXIS2) is identical in both modes.

### Rover Mode (Figure 3-12)

Routes to `WHEEL_MESSAGE` instead of `ROTATION_MESSAGE`. AXIS3 (twist) is unused and should be ignored by the controller in this mode.

| Axis | Packet Field | Physical | KSP Function | Simpit Field |
|---|---|---|---|---|
| AXIS1 | Bytes 2–3 | Left / Right | Steer Left / Steer Right | `WHEEL_MESSAGE` steer |
| AXIS2 | Bytes 4–5 | Forward / Back | Wheel Throttle + / Wheel Throttle − | `WHEEL_MESSAGE` throttle |
| AXIS3 | Bytes 6–7 | (twist) | N/A — unused | — |

### EVA Mode (Figure 3-15) — Auto-activated by `isEVA`

When `FLIGHT_STATUS_MESSAGE` reports `isEVA = true`, the controller automatically overrides to EVA mode regardless of the mode switch position. EVA inputs are emulated as keyboard events via `KEYBOARD_EMULATOR` — analog deflection is treated as digital (any deflection outside deadzone = key held, return to center = key released).

| Axis / Control | Physical | KSP Action | Key Emulated |
|---|---|---|---|
| AXIS1 left | Stick left | Walk Left | A (held) |
| AXIS1 right | Stick right | Walk Right | D (held) |
| AXIS2 forward | Stick forward | Walk Forward | W (held) |
| AXIS2 back | Stick back | Walk Back | S (held) |
| AXIS3 CCW | Twist CCW | Jetpack Rotate Left | Q (held) |
| AXIS3 CW | Twist CW | Jetpack Rotate Right | E (held) |
| BTN_JOY held | Button held | Run | Left Shift (held) |

When `isEVA` returns false (Kerbal boards a vessel), the controller restores the mode determined by the physical switch position.

### Controls

| Control | Function | Base CAG |
|---|---|---|
| BTN01 (GREEN) | Reset Trim — clears all trim to neutral (Alt+X equivalent) | — |
| BTN02 (AMBER) | Set Trim — captures current joystick deflection as trim offset | — |
| BTN_JOY | Toggle Airbrake (spacecraft/airplane/rover) or Run/hold (EVA) | 38 |

### Notes
- **Do not touch joystick during first ~80ms after power-on** — startup calibration reads center position at boot
- AXIS3 (twist) is unused in Rover mode — controller should pass zero for `WHEEL_MESSAGE` steer or simply not read AXIS3 in that mode
- EVA mode uses digital key emulation, not analog axis values — deadzone threshold determines key on/off
- Same physical hardware as Translation module (KC-01-1831/1832)

---

## 0x29 — Joystick Translation Module

**PCB:** KC-01-1831/1832 | **Type:** 0x0A | **Capability:** JOYSTICK

Three-axis hall-effect joystick. Two NeoPixel buttons. Reports signed INT16 axis values calibrated to center at startup.

The translation stick has three operating modes. Normal and Camera are always available; EVA is triggered automatically when Simpit reports `isEVA = true`, overriding normal mode (camera mode via BTN_JOY remains available during EVA).

### Mode Overview

| Mode | Trigger | Simpit Channel |
|---|---|---|
| Translation | Normal operation | `TRANSLATION_MESSAGE` |
| Camera | BTN_JOY held (any mode) | `CAMERA_MESSAGE` |
| EVA Jetpack | `isEVA` flag from Simpit (auto) | `KEYBOARD_EMULATOR` |

### Translation Mode (BTN_JOY released, not EVA)

AXIS2 inversion is applied in module firmware — the packet already reflects the corrected polarity shown below.

| Axis | Packet Field | Physical | KSP Function | Simpit Field |
|---|---|---|---|---|
| AXIS1 | Bytes 2–3 | Left / Right | Translate Left / Translate Right | `TRANSLATION_MESSAGE` X |
| AXIS2 | Bytes 4–5 | Forward push / Back pull | Translate Up / Translate Down | `TRANSLATION_MESSAGE` Y |
| AXIS3 | Bytes 6–7 | CW / CCW twist | Translate Forward / Reverse | `TRANSLATION_MESSAGE` Z |

AXIS2 inversion is handled in module firmware — controller receives correct polarity and should use values directly without further negation.

### Camera Control Mode (BTN_JOY held — any mode)

When BTN_JOY is held, the controller routes axis values to `CAMERA_MESSAGE` instead of the current mode's message. The module sends identical data — the mode switch is entirely in controller firmware. Releasing BTN_JOY restores the previous mode immediately.

| Axis | Physical | Camera Function | Simpit Field |
|---|---|---|---|
| AXIS1 | Left / Right | Camera Yaw Left / Right | `CAMERA_MESSAGE` yaw |
| AXIS2 | Forward push / Back pull | Camera Pitch Up / Down | `CAMERA_MESSAGE` pitch |
| AXIS3 | CW / CCW twist | Zoom In / Zoom Out | `CAMERA_MESSAGE` zoom |

### EVA Jetpack Mode (auto — `isEVA` = true)

When `FLIGHT_STATUS_MESSAGE` reports `isEVA = true`, the controller automatically switches to EVA jetpack mode. Inputs are emulated as keyboard events via `KEYBOARD_EMULATOR` — analog deflection is treated as digital (any deflection outside deadzone = key held, return to center = key released).

W/A/S/D serve double duty in KSP EVA — they control both walking (on ground) and jetpack translation (in space). KSP determines which applies based on whether the Kerbal is grounded. The Rotation stick also emits W/A/S/D in EVA mode for walking — this is intentional and not a conflict since the keys are idempotent.

| Axis / Control | Physical | KSP Jetpack Action | Key Emulated |
|---|---|---|---|
| AXIS1 left | Stick left | Jetpack left | A (held) |
| AXIS1 right | Stick right | Jetpack right | D (held) |
| AXIS2 forward | Stick forward push | Jetpack forward | W (held) |
| AXIS2 back | Stick back pull | Jetpack back | S (held) |
| AXIS3 CW | Twist CW | Jetpack up | Left Shift (held) |
| AXIS3 CCW | Twist CCW | Jetpack down | Left Control (held) |
| BTN_JOY held | Button held | Camera control | (normal camera mode applies) |

When `isEVA` returns false, the controller releases any held EVA keys and restores normal translation mode.

### Controls

| Control | Function |
|---|---|
| BTN01 (MAGENTA) | Cycle Camera Mode — cycles auto/free/orbital/chase |
| BTN02 (GREEN) | Camera Reset — resets camera to default position |
| BTN_JOY held | Camera Control mode (all modes including EVA) |

### Notes
- **Do not touch joystick during first ~80ms after power-on** — startup calibration reads center position at boot
- AXIS2 inversion is in module firmware — controller receives correct polarity directly
- AXIS3 (twist) may benefit from a slightly wider deadzone than AXIS1/AXIS2 due to mechanical play on twist axes
- EVA mode uses digital key emulation — deadzone threshold determines key on/off; consider a wider EVA deadzone to prevent false triggers from stick resting position
- Ensure all held EVA keys are explicitly released when `isEVA` transitions false
- Same physical hardware as Rotation module (KC-01-1831/1832)

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

| Control | Function | Implementation |
|---|---|---|
| ENC1 (CW) | AG Block next | Controller-side — increments active custom AG block mapping |
| ENC1 (CCW) | AG Block previous | Controller-side — decrements active custom AG block mapping |
| ENC1 button | Docked flag toggle | Controller-side — toggles `isDocked` flag for TFT display sync. Use when docking state indicator gets out of sync after vessel switching. No Simpit message sent. |
| ENC2 (CW) | Next external camera | `KEYBOARD_EMULATOR` `-` (Hullcam VDS Continued) |
| ENC2 (CCW) | Previous external camera | `KEYBOARD_EMULATOR` `=` (Hullcam VDS Continued) |
| ENC2 button | Reset camera | `KEYBOARD_EMULATOR` Backspace (Hullcam VDS Continued) |

### ENC1 — AG Block Selector

ENC1 selects which block of custom action groups the Function Control and Action Control CP buttons map to. Turning ENC1 shifts the entire custom AG mapping table — all Function Control buttons and Action Control CP PRI/CP ALT send different AGX numbers depending on the active block. This allows independent control of docked vessels (e.g. lander + spacecraft) each with their own AG assignments.

The block remapping is handled entirely in controller firmware — no Simpit message is sent on ENC1 turn. The new AG numbers take effect on the next button press. The number of available blocks is determined by the total custom AG count and AGX capacity.

### ENC2 — External Camera Cycle

Requires **Hullcam VDS Continued** mod. Verify default keybindings (`-`, `=`, Backspace) match in-game settings or update to match.

### Data Packet

| Byte | Content |
|---|---|
| 0 | Button events (bit0=ENC1_SW, bit1=ENC2_SW) |
| 1 | Change mask (same layout) |
| 2 | ENC1 delta (signed int8, +CW / -CCW) |
| 3 | ENC2 delta (signed int8, +CW / -CCW) |

### Future Upgrade — Trim Adjustment

A future hardware expansion may add trim adjustment capability. Planned implementation: additional encoder inputs on the main controller or a new dedicated trim module. ENC1 button (currently reserved) may be incorporated into this upgrade path. See trim adjustment future upgrade note.

---

## 0x2E — Switch Panel Module

**PCB:** KC-01 Switch Panel | **Type:** 0x0F

10 toggle switch inputs in a 5×2 grid. No LEDs. SW1 and SW2 are mechanically coupled to a single 3-position latching MODE switch. All other switches are independent 2-position toggles.

### Panel Layout

```
┌──────────┬──────────┬──────────┬──────────┬──────────┐
│  MODE    │  MSTR    │  SCE     │  AUDIO   │  ENGINE  │
│  CTRL ↑  │  OPER ↑  │  NORM ↑  │  OFF ↑   │  SAFE ↑  │
│ SW1+SW2  │  SW3     │  SW5     │  SW7     │  SW9     │
│  DEMO ↓  │ RESET ↓  │  AUX ↓   │  ON ↓    │  ARM ↓   │
├──────────┼──────────┼──────────┼──────────┼──────────┤
│ (MODE    │ DISPLAY  │  LTG     │  INPUT   │  THRTL   │
│  lower)  │  OPER ↑  │  OPER ↑  │  NORM ↑  │  STD ↑   │
│          │  SW4     │  SW6     │  SW8     │  SW10    │
│          │ RESET ↓  │  TEST ↓  │  FINE ↓  │  FINE ↓  │
└──────────┴──────────┴──────────┴──────────┴──────────┘
```

### MODE Switch (SW1 + SW2) — 3-Position Latching

A single 3-position latching switch drives both SW1 and SW2. The two bits encode three mutually exclusive operating modes. Center position (DBG) is the power-on default since all switches open = all bits zero.

| Position | SW1 (Bit 0) | SW2 (Bit 1) | Mode | Indicator |
|---|---|---|---|---|
| Up | 1 | 0 | CTRL — Live Control | B2 CTRL ACTIVE |
| Center | 0 | 0 | DBG — Debug | B5 DEBUG ACTIVE |
| Down | 0 | 1 | DEMO — Demo/Simulated | B8 DEMO ACTIVE |

Controller decodes mode with: `mode = stateWord & 0x03` (0=DBG, 1=CTRL, 2=DEMO)

### Switch Reference

| Switch | Bits | Panel Label | Up State | Down State | Controller Action |
|---|---|---|---|---|---|
| SW1+SW2 | 0+1 | MODE | CTRL | DEMO (DBG=center) | See MODE table above |
| SW3 | 2 | MSTR | OPER | RESET | Rising edge only: CMD_RESET all modules |
| SW4 | 3 | DISPLAY | OPER | RESET | Rising edge only: CMD_SET_VALUE 0 to 0x2A, 0x2B |
| SW5 | 4 | SCE | NORM | AUX | Toggle → INDICATOR B12 (SCE AUX) ACTIVE |
| SW6 | 5 | LTG | OPER | TEST | Rising: bulb test start. Falling: bulb test stop |
| SW7 | 6 | AUDIO | OFF | ON | Toggle → INDICATOR B9 (AUDIO) ACTIVE |
| SW8 | 7 | INPUT | NORM | FINE | Toggle → INDICATOR B6 (PREC INPUT) ACTIVE |
| SW9 | 8 | ENGINE | SAFE | ARM | Toggle: arm/disarm engine ignition |
| SW10 | 9 | THRTL | STD | FINE | Rising: CMD_SET_PRECISION 0x01 → Throttle. Falling: 0x00 → INDICATOR B3 (THRTL PREC) ACTIVE |

### Data Packet

| Byte | Content |
|---|---|
| 0 | State HIGH byte (bit 9=SW10, bits 10–15 always 0) |
| 1 | State LOW byte (bits 7–0 = SW8–SW1) |
| 2 | Change mask HIGH |
| 3 | Change mask LOW |

### Notes
- SW3 (MSTR) and SW4 (DISPLAY) act on rising edge only — reset is a one-shot action regardless of switch type
- SW6 (LTG) uses both edges: down starts bulb test, returning up stops it
- SW8 (INPUT, precision input) and SW10 (THRTL, throttle fine control) are distinct — SW8 is system-wide input scaling, SW10 drives the Throttle module's motorized precision mode specifically
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

### ABORT State (B15)

| State | Appearance | Trigger |
|---|---|---|
| OFF | Dark | `ACTIONSTATUS_MESSAGE` ABORT_ACTION = inactive |
| ALERT | Flashing red (150ms) | `ACTIONSTATUS_MESSAGE` ABORT_ACTION = active |

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
| 0x2F | Indicator | 0x10 | Standalone | Yes (ABORT) |
