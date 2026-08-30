# KCMk1_InfoDisp

**Kerbal Controller Mk1 — Information Display Panel Sketch** · v1.7.1
Teensy 4.1 firmware for the KSP flight information display module.
Part of the KCMk1 controller system. Operates as an I2C slave under a Teensy 4.1 master.

---

## Overview

The Information Display is a 1024×600 touchscreen panel that presents real-time KSP flight telemetry sourced from KerbalSimpit. It runs on a Teensy 4.1 and receives telemetry over USB serial from a running KSP instance.

The panel provides fourteen screens — Launch, Ascent Autopilot, Spacecraft/Aircraft/Rover/Vehicle (PFD), Orbit (+ Advanced Elements + Maneuver), Target/Docking/Navigation, Landing (Powered Descent + Re-entry) — ordered to follow mission phase progression from pre-launch through landing. Navigation is via a sidebar of six buttons on the panel's outboard edge. A first press of a button jumps to its context/primary screen; pressing the button that already owns the active screen cycles that button's modes, and the caption shows the active mode. Title-bar taps no longer switch screens.

**Two units, two roles.** The controller carries two Info Displays, and they are not clones. Both run this firmware and every screen is reachable from either panel, but each has a job and a different set of defaults:

| | Info Display 1 (0x12, panel A1) | Info Display 2 (0x13, panel B1) |
|---|---|---|
| Role | Vehicle-type — *what am I flying?* | Mission-phase — *what phase am I in?* |
| Holds | The PFD family: SPACECRAFT / AIRCRAFT / ROVER / VEHICLE | LAUNCH / LANDING / DOCKING / TARGET / NAVIGATION / MANEUVER / ORBIT |
| Resting screen | SPACECRAFT (PFD) | ORBIT |
| Home screen (boot / standby / demo) | SPACECRAFT (PFD) | LAUNCH |
| Sidebar edge | Left (outboard on A1) | Right (outboard on B1) |
| Sixth button | VEH — Vehicle Info | ASC — Ascent Autopilot console |
| Ascent Autopilot | Not carried; cannot send commands | Sole owner of the command channel |

Both panels sit inboard on their own half of the console, so their content areas meet at the centreline and read as one field, with the two button columns falling outboard under the pilot's respective hands.

**Context-switching:** Each panel automatically selects the most appropriate screen for its own role when the scene or vessel changes; see [Context Switching](#context-switching) below. A deliberate sidebar selection latches and survives context events until the vessel or the scene changes.

**Colour conventions** are consistent across all screens: dark green = nominal, yellow = caution, white-on-red = alarm, dark grey = inactive/not applicable, cyan = pilot-entered or pending. Alarm thresholds are aligned with the KCMk1 Annunciator C&W panel.

The sidebar is deliberately outside that vocabulary: its keys are chrome, not data, so they are achromatic — white-on-black unselected, black-on-grey selected — and colour appears on a key only when it carries live state. This is the convention bezel-key flight decks use (a Garmin G1000 softkey row is exactly this), and it keeps the alerting colours available to the telemetry the sidebar frames.

---

## Hardware

| Component | Part | Interface |
|-----------|------|-----------|
| Microcontroller | Teensy 4.1 | — |
| Display | LT7683 (RA8876-compatible) 1024×600 7" IPS TFT | 8080 16-bit parallel |
| Touch controller | FT5316 capacitive | software I2C |
| SD card | Teensy 4.1 on-board microSD | SDIO (`BUILTIN_SDCARD`) |
| KSP telemetry | KerbalSimpit plugin | SerialUSB1 (second USB COM port, e.g. COM5) |
| I2C slave bus | Master Teensy 4.1 at 0x12 (Info Display 1) or 0x13 (Info Display 2) | Wire2 (pins 24/25) |

The display controller is the **LT7683** (the physical part on the ER-TFT070A2-6-5633 module); it is register-compatible with the RA8876, so the firmware drives it through the `wwatson4506/TeensyRA8876-8080` FlexIO3 driver (class `RA8876_t41_p`). "RA8876" therefore appears in driver/library/class names throughout, while the hardware part is the LT7683.

See `Documents/Developer/Hardware_Reference.md` (§8.3, KC-01-1912 carrier) for the display-carrier hardware detail.

### Pin Assignments

All pins are defined in `KCMk1_SystemConfig.h` (hardware rev 2: Teensy 4.1 + LT7683 (RA8876-compatible) 8080-parallel carrier, KC-01-1912). The 16 display data lines are driven by the Teensy 4.1 FlexIO3 hardware via the `wwatson4506/TeensyRA8876-8080` driver — the defines below are documentation; the driver owns the data/WR/RD lines.

| Pin(s) | Function | Direction | Define |
|--------|----------|-----------|--------|
| 19,18,14,15,40,41,17,16,22,23,20,21,38,39,26,27 | Display data bus DB0..DB15 (FlexIO3) | — | `KCM_TFT_DB0..DB15` |
| 34 | Display /CS chip select | OUT | `KCM_TFT_CS` |
| 33 | Display RS register/data select | OUT | `KCM_TFT_RS` |
| 35 | Display /RESET | OUT | `KCM_TFT_RESET` |
| 36 | Display /WR write strobe | OUT | `KCM_TFT_WR` |
| 37 | Display /RD read strobe | OUT | `KCM_TFT_RD` |
| 32 | Display WAIT (busy from LT7683) | IN | `KCM_TFT_WAIT` |
| 31 | Display INT (from LT7683, unused) | IN | `KCM_TFT_INT` |
| 9 | Backlight enable / PWM | OUT | `KCM_TFT_BL` |
| 4 | Touch software-I2C SCL | — | `KCM_CTP_SCL` |
| 5 | Touch software-I2C SDA | — | `KCM_CTP_SDA` |
| 3 | Touch /RESET (active-LOW) | OUT | `KCM_CTP_RST` |
| 6 | Touch INT (data-ready) | IN | `KCM_CTP_INT` |
| 29 | Master-alarm tone → PAM8302A amp | OUT | `KCM_AUDIO_TONE_PIN` |
| 7,8 | DFPlayer Mini serial (Serial2 RX2/TX2) | — | `KCM_DFPLAYER_SERIAL` |
| 11 | DFPlayer BUSY (net AUDIO_BUSY, LOW = playing) | IN | `KCM_AUDIO_BUSY_PIN` |
| 24,25 | I2C slave bus to master (Wire2 SCL2/SDA2) | — | `KCM_I2C_BUS` |
| 0 | I2C INT (active-LOW, output to master) | OUT | `KCM_I2C_INT_PIN` |
| 1 | I2C RST (shared reset line from master) | IN | `KCM_I2C_RST_PIN` |

**Serial ports:**
- `Serial` (USB COM port 1) — debug output when `debugMode = true`
- `SerialUSB1` (second USB COM port, e.g. COM5) — KerbalSimpit telemetry

**I2C note:** The slave interface to the Teensy 4.1 master is on **Wire2 (pins 24/25)**. Pins 18/19 (Wire) and 16/17 (Wire1) are consumed by the display data bus, so FT5316 touch runs on a bit-banged software-I2C bus (pins 4/5).

---

## Dependencies

| Library | Version | Notes |
|---------|---------|-------|
| KerbalDisplayCommon | ≥ 3.0.0 | Display primitives, fonts, value formatting, threshold colouring (pulls in KCM_Display) |
| KCM_Display | — | `KCM_TFT` display type = `RA8876_t41_p` + shared resolution/pins (this repo) |
| KCM_Touch | — | FT5316 5-point capacitive touch driver (this repo) |
| KCMk1_SystemConfig | — | Shared hardware pin map + cross-panel threshold constants (this repo) |
| TeensyRA8876-8080 (`RA8876_t41_p`) + TeensyRA8876-GFX-Common | — | RA8876 8080-parallel display driver (wwatson4506) |
| ILI9341_fonts (PaulStoffregen) | — | ILI9341_t3 font format used by KerbalDisplayCommon |
| KerbalDisplayAudio | — | Master-alarm audio (`tone()`, pin 29 → PAM8302A amp) + DFPlayer support |
| KerbalSimpit | 2.4.0 | KSP telemetry plugin interface |

### KerbalSimpit Plugin Settings

Location: `KSP/GameData/KerbalSimpit/PluginData/Settings.cfg`

```
PortName    = COM5       # SerialUSB1 — the second USB COM port (Teensy dual serial)
BaudRate    = 115200
RefreshRate = 125
Verbose     = True
```

---

## Configuration

All tunables are in `AAA_Config.ino`. Cross-panel aligned thresholds are sourced from `KCMk1_SystemConfig.h` — edit there only.

### Operating Mode

| Constant | Default | Description |
|----------|---------|-------------|
| `INFO_DISP_UNIT` | `1` | **In `KCMk1_InfoDisp.h`, not this file.** `1` = Info Display 1 (0x12, vehicle-type role, left sidebar), `2` = Info Display 2 (0x13, mission-phase role, right sidebar, owns the Ascent Autopilot). Set before flashing each board. It lives in the header because the layout constants and sidebar tables are compile-time conditional on it. |
| `debugMode` | `false` | Serial debug output. Set `false` for production. |
| `demoMode` | `false` | `true` = sine-wave demo, no KSP. `false` = live Simpit. In demo the panel also stands in for Controller_Main on the Ascent Autopilot console: ARM/DISARM and parameter edits are applied to a demo-side autopilot model and echoed back, so the console's round trip closes on the bench. The demo boots armed; DISARM parks the ascent at IDLE and ARM resumes it. A full demo ascent runs about 53 s. |
| `DISPLAY_ROTATION` | `0` | `0` = normal, `2` = 180° inverted. |

### Parachute CAG Assignments

| Constant | Default | Description |
|----------|---------|-------------|
| `DROGUE_DEPLOY_CAG` | `1` | CAG that deploys the drogue |
| `DROGUE_CUT_CAG` | `2` | CAG that cuts the drogue |
| `MAIN_DEPLOY_CAG` | `3` | CAG that deploys the main |
| `MAIN_CUT_CAG` | `4` | CAG that cuts the main |

Set to `0` to disable (always shows STOWED).

### Key Thresholds

Cross-panel aligned thresholds (edit in `KCMk1_SystemConfig.h`):

| Constant | Value | Annunciator equivalent |
|----------|-------|------------------------|
| `LNDG_TGRND_ALARM_S` | `10.0` s | `CW_GROUND_PROX_S` |
| `G_ALARM_POS` | `9.0` g | `CW_HIGH_G_ALARM` |
| `G_ALARM_NEG` | `−5.0` g | `CW_HIGH_G_WARN` |
| `G_WARN_POS` | `4.0` g | `KCM_HIGH_G_WARN_POS` (caution tier) |
| `G_WARN_NEG` | `−2.0` g | `KCM_HIGH_G_WARN_NEG` (caution tier) |
| `DV_STG_ALARM_MS` | `150.0` m/s | `CW_LOW_DV_MS` |
| `LNCH_BURNTIME_ALARM_S` | `60.0` s | `CW_LOW_BURN_S` |

### Context Routing Bounds (Info Display 2)

| Constant | Default | Description |
|----------|---------|-------------|
| `TGT_CONTEXT_MAX_M` | `2000.0` m | Outer bound of the TARGET auto-select window. Inside `DOCK_DIST_WARN_M` (200 m) the DOCKING screen takes priority; past this the RPOD scope has nothing useful to show and ORBIT is the better view. |
| `MNVR_CONTEXT_LEAD_S` | `600.0` s | How far ahead of ignition a planned node auto-selects MANEUVER. Gated on the burn being imminent rather than on a node merely existing, since nodes persist long after they matter. |

For the full threshold listing (aircraft, landing, docking, orbit, spacecraft thresholds), see `AAA_Config.ino`.

---

## I2C Protocol

The InfoDisp operates as an I2C slave on the Wire2 bus (`KCM_I2C_BUS`, pins 24/25). Both Info Display boards run this same firmware image; the unit is chosen at compile time by `INFO_DISP_UNIT` in `KCMk1_InfoDisp.h`:

- `INFO_DISP_UNIT 1` → **0x12** (`KCM_I2C_ADDR_INFODISP`) — Info Display 1
- `INFO_DISP_UNIT 2` → **0x13** (`KCM_I2C_ADDR_INFODISP_2`) — Info Display 2

Beyond the address, the unit selects the panel's role: which context ladder runs, which side the sidebar sits on, what button 5 is, and whether the Ascent Autopilot command channel is live. See the role table in the Overview.

The sync/framing byte (0xAE) is shared by both units. The System Info Display (0x14) is separate hardware and is future work — see `Documents/Developer/Hardware_Reference.md`. In the byte layouts below, `I2C_SLAVE_ADDR` resolves to whichever address the build targets.

### Outbound Packet — InfoDisp → Master

Size: **10 bytes** (`I2C_PACKET_SIZE`). Sent in response to `KCM_I2C_BUS.requestFrom(I2C_SLAVE_ADDR, 10)` (Wire2) after INT asserts. Bytes 0–2 are the status header; bytes 3–9 are the Ascent Autopilot command frame (see `Documents/Developer/Ascent_Autopilot_Interface.md`).

| Byte | Field | Description |
|------|-------|-------------|
| 0 | Sync | `0xAE` (`KCM_I2C_SYNC_INFODISP`) — framing validation |
| 1 | Flags | Bit 0: `simpitConnected`  Bit 1: `flightScene`  Bit 2: `demoMode`  Bits 3–7: reserved (0) |
| 2 | `activeScreen` | Current `ScreenType` enum value (0–12) |
| 3 | `cmdSeq` | Ascent-AP command sequence (0 = none; else 1–255, unique per queued command) |
| 4 | `cmdOp` | Ascent-AP command opcode (`AP_CMD_*`) |
| 5–8 | `cmdPayload` | IEEE-754 float32, little-endian (command argument) |
| 9 | `xsum` | XOR of bytes 3–8 (command-frame integrity) |

The master executes the command in bytes 4–8 once, then echoes `cmdSeq` back in the inbound `ackSeq` byte to pop it. Bytes 0–2 are unchanged from earlier revisions, so a master that reads only the first 4 bytes still gets valid status.

**Note:** The sync byte is `0xAE`, not `0xAD`. The value `0xAD` is used by the ResourceDisp — using it here would cause framing collisions if the master dispatches based on sync byte. Master sketches written before v0.13.3 should update their InfoDisp sync byte expectation accordingly.

### Inbound Command Packet — Master → InfoDisp

Size: **2 bytes**. Sent by master at any time.

| Byte | Field | Description |
|------|-------|-------------|
| 0 | `controlByte` | See bit map below |
| 1 | `ackSeq` | Ascent-AP command acknowledgement — the `cmdSeq` just executed (0 = none) |

### Inbound Ascent-AP Status Push — Master → InfoDisp

Size: **40 bytes** (`I2C_AP_STATUS_SIZE`), dispatched by write length, sync `0xA5`. The master pushes the autopilot's `AscentStatus` so the Ascent Autopilot screen can render live guidance and confirm accepted parameters. Byte 1 = flags (bit0 armed, bit1 southerly, bit2 rollEnable), byte 2 = phase, bytes 4–39 = nine float32 (targetAlt, inclination, loft, rollDeg, maxG, cmdPitch, cmdHeading, cmdThrottle, dynPressure). Full layout in `Documents/Developer/Ascent_Autopilot_Interface.md` §5.

**`controlByte` bit map:**

| Bits | Field | Description |
|------|-------|-------------|
| 7:4 | `requestType` | Command code — see table below |
| 3 | `idle_state` | `1` = switch to Standby when not in a flight scene |
| 2 | `trimEnabled` | `1` = trim hold engaged — shows the cyan `TRIM` annunciation on the SPACECRAFT / AIRCRAFT screens |
| 1 | `demoMode` | `1` = enable demo mode |
| 0 | `debugMode` | `1` = enable Serial debug output |

**Request type codes (`controlByte` bits 7:4):**

| Code | Name | Action |
|------|------|--------|
| `0x0` | NOP | No operation |
| `0x1` | STATUS | Force immediate status packet — assert INT now |
| `0x2` | PROCEED | Release boot hold — InfoDisp enters main loop |
| `0x3` | MCU_RESET | Soft reboot the InfoDisp (USB disconnect then ARM AIRCR reset) |
| `0x4` | DISPLAY_RESET | Reset display state and force full redraw of current screen |

### Boot Handshake

After initialisation, the InfoDisp asserts INT and spins on `PROCEED` (0x2) before entering `loop()`. This synchronises the boot sequence across all KCMk1 panels. The master can include configuration flags (`idle_state`, `demoMode`, `debugMode`) in the same packet as PROCEED.

---

## Screens

The panel displays fourteen screens navigated by six outboard-edge sidebar buttons. Screen order follows mission phase progression. A first press of a button jumps to its context/primary screen; pressing the button that already owns the active screen cycles its modes, and the button caption shows the active mode. Several buttons are multi-mode: PFD covers SPACECRAFT / AIRCRAFT / ROVER (plus VEHICLE on unit 2), ORB covers ORBIT / Advanced Elements / MANEUVER, TGT covers TARGET / DOCKING / NAVIGATION, and LNDG covers POWERED DESCENT / RE-ENTRY. Title-bar taps no longer switch screens.

Buttons 0–4 are identical on both units, so every screen stays reachable from either panel and losing one display never costs a screen class. Only button 5 differs.

PFD is pinned to the top key on both units: it is the screen a pilot returns to from anywhere, it is unit 1's entire role, and the top key is the easiest to find without looking away from the content. The remaining five keep the original mission-phase progression — LNCH, ORB, TGT, LNDG — with the single-mode key at the bottom.

| Btn | Sidebar | Screen(s) | First press / cycle | Caption |
|-----|---------|-----------|---------------------|---------|
| 0 | PFD | SPACECRAFT / AIRCRAFT / ROVER (/ VEHICLE) | First press = context screen (SPC/ACFT/ROVR by vessel). Press cycles SPC → ACFT → ROVR on unit 1, which carries VEH on its own button; unit 2 keeps the four-deep ring SPC → ACFT → ROVR → VEH | SPC / ACFT / ROVR / VEH |
| 1 | LNCH | LAUNCH | Pre-launch board shown automatically on pad (a press dismisses it into ascent); otherwise press cycles ASCENT ↔ CIRCULARIZATION | PRE / ASC / CIRC |
| 2 | ORB | ORBIT / ORBIT ADVANCED / MANEUVER | First press = ORBIT (Apsides); press cycles ORB → ORB+ (Advanced Elements) → MNVR (Maneuver) | ORB / ORB+ / MNVR |
| 3 | TGT | TARGET / DOCKING / NAVIGATION | First press = DOCKING when a target is within docking range, else TARGET; press cycles TGT → DOCK → NAV. NO TARGET SET / DOCKED fullscreen when applicable. NAV needs no target and works without one | TGT / DOCK / NAV |
| 4 | LNDG | POWERED DESCENT / RE-ENTRY | First press = POWERED DESCENT; press cycles DESC ↔ ENTR | DESC / ENTR |
| 5 | ASC *(unit 2)* | ASCENT AUTOPILOT | Standalone — parked at the bottom, below the display-nav cluster. Turns green while the autopilot is armed. On-screen keypad for parameter entry; touch ARM/DISARM. Unit 2 only: it is the sole owner of the autopilot command channel | ASC |
| 5 | VEH *(unit 1)* | VEHICLE INFO | Standalone, single-mode. Promoted out of the PFD ring on the vehicle-type panel — it is where the ladder routes a recoverable vessel, and moving it here shortens that panel's PFD ring from four modes to three | VEH |

**LNCH** — *Pre-launch board* (automatic when `sit_PreLaunch`, bypassed for planes and rovers): vessel name, type, SAS, RCS, throttle, EC%, crew count, CommNet signal, ΔV.Tot, and parachute CAG states. Tap content area or launch to advance to ascent. *Ascent:* Alt.SL, V.Srf, V.Vrt, ApA, T+Ap, Throttle, T.Burn, ΔV.Stg. *Circularization:* Alt.SL, V.Orb, ApA, PeA, T+Ap, Throttle, T.Burn, ΔV.Stg. Auto-switches at ~6% body radius with hysteresis.

**ASC** — Ascent Autopilot touch console for the Simpit ascent autopilot (which runs on Controller_Main). The ARM button, the ARMED/DISARMED banner and the sidebar ASC key all annunciate the autopilot's own reported state, never an unacknowledged tap: a tap raises a pending cue (cyan border, `ARMING…`/`DISARMING…`, `...` on the banner) and the state changes only when Controller_Main echoes it back. Three columns: MISSION inputs (target apoapsis, inclination, launch N/S), VEH PROFILE inputs (loft, roll hold, max-G) + the ARM/DISARM button, and GUIDANCE outputs (commanded pitch/heading/throttle, G, dynamic pressure, ApA, PeA). Boxed input fields open an on-screen numeric keypad (or toggle) and can be edited at any time; a pilot edit shows in cyan until the autopilot echoes the accepted value back. The phase banner and ARM button colour reflect the autopilot phase (IDLE / VERTICAL / GRAVITY TURN / COAST / CIRCULARIZE / COMPLETE / ABORT). Edits and ARM/DISARM are sent over I2C — see `Documents/Developer/Ascent_Autopilot_Interface.md`.

**NAV** — Navigation display: a plan view for atmospheric flight, and the other half of a glass-cockpit pair with the PFD on the opposite panel. Same compass card as ROVER (shared renderer, `Compass.ino`), with the nose fixed at 12 o'clock and two bearing markers: **green** for ground track — where the vessel is actually moving — and **violet** for target bearing, matching the target colour used elsewhere. Heading box above the card; left column TRK and DRIFT; right column DIST and V.CLS (dashed when no target is set); bottom strip V.Srf and Alt.Rdr. **DRIFT** — ground track minus heading, the crab angle — is the one number on this screen that appears nowhere else on either panel, and it turns yellow past `NAV_DRIFT_WARN_DEG` (10°): it is the difference between a heading that is holding and one that is quietly sliding off. Track and drift are dashed below `NAV_TRK_MIN_MS` (5 m/s), where KSP's reported velocity heading wanders. Everything is derived from telemetry the panel already receives — no new Simpit channels.

**ORB** — *Apsides (default):* graphical orbit + inclination diagram with numeric readouts Alt.SL, PeA, ApA (left panel) and Inc, Period, Arg.Pe, T+Pe/T+Ap (right panel). *Advanced Elements:* text-only readout — SMA, Ecc, PeA, ApA, Alt.SL, V.Orb, Period (left column) and Inc, LAN, Arg.Pe, True Anom, Mean Anom, T+Pe, T+Ap (right column). Reached by cycling the ORB sidebar button (ORB → ORB+ → MNVR); a first press of ORB from another screen returns to Apsides.

**PFD** — Primary Flight Display (SPACECRAFT): full EADI ball with pitch ladder, roll pointer, fixed spacecraft (boresight) symbol, navball velocity/target/maneuver markers, and Hdg/Pitch/Roll readouts. Right panel: Alt.SL, V.Orb/V.Srf (label swaps with orbital mode), ApA, PeA, T+Ap, T+Ign, ΔV.Stg, and RCS/SAS buttons.

*On EVA* the ball is unchanged — you do orient on EVA — but the panel swaps rows 2–6, which otherwise show a Kerbal a stage ΔV, an apoapsis and a time-to-ignition that do not exist: **Alt.Rdr, V.Srf, Dist, V.Close, EC**. Row 1 pins to V.Orb rather than following orbital mode, so V.Orb and V.Srf are both always present — the pair is how you tell station-keeping from drifting. Dist and V.Close dash out when no target is selected; EC (suit charge) uses ROVER's thresholds, 20% caution and 5% alarm, and is the number that ends an EVA. RCS and SAS stay: both are exactly as meaningful for a Kerbal on a jetpack. The swap is triggered by `vesselType == type_EVA` and re-enters the screen so the row labels redraw, the same mechanism the V.Orb/V.Srf swap has always used.

**MNVR** — Burn alignment reticle (blue maneuver marker vs a fixed nose crosshair; neon-green alignment box when within 5°) plus a numeric panel: ΔV.Mnvr, ΔV.Plan (total across all planned nodes), ΔV.Stg, T+Ign, T+Mnvr, Burn Dur, Brg/Elv (nose-to-node error split), and RCS/SAS buttons. A ΔV-burn bar sits under the reticle. **NO MANEUVER** fullscreen when no node is planned.

**TGT** — RPOD scope reticle + numeric panel: Alt.SL, V.Orb, Dist, V.Close (signed closure rate — negative = closing), bearing/elevation of the target from the nose (Brg/Elv), approach-path error about the target axis (V.Brg/V.Elv, same labels and quantity as DOCK), and T+Int (intercept time = Dist ÷ |closure|, shown only while closing). All scope markers are nose-referenced: the VEL marker sits at centre when the relative velocity runs along the boresight, and on the TGT marker when it is aimed at the target. Dist turns white-on-green below 200 m. NO TARGET SET fullscreen when no target.

**DOCK** — Approach reticle + numeric panel: Dist, T+Dock (Dist ÷ |closure|, closing only), V.Close (signed closure rate), V.Lat (total lateral drift magnitude), Brg/Elv (the port relative to the nose) and V.Brg/V.Elv (approach-path error about the target axis; `---` past 90°) — the same four angles TGT shows, in the same split-row layout — Nos.Off (total nose angular offset from the port, colour-banded), and RCS/SAS buttons. The green VEL marker shows the craft-to-port **relative velocity** referenced to the nose, and the whole marker layer is rotated into the craft's body axes (by −roll), so it flies like a prograde marker at any roll attitude: marker up and right of the crosshair → thrust left and down to centre it. SAS: TARGET = green, STAB = cyan, OFF = white-on-red, all other modes = red. DOCKED / NO TARGET SET fullscreen when applicable.

**LNDG** — *Powered descent:* T.Grnd, Alt.Rdr, V.Srf, V.Vrt, Fwd/Lat horizontal drift (roll-corrected, craft heading frame), ΔV.Stg, Throttle/RCS, Gear/SAS. Fwd/Lat thresholds tighten as T.Grnd decreases.

**ENTR** — *Re-entry* (separate sidebar screen): graphical instrument screen — corridor tape, atmosphere-density bar, G meter, and a heat-shield / retrograde alignment ball — with a text panel of T+Grnd/T+Atm (row 0 toggles by descent phase), Alt.SL/Alt.Rdr (row 1 toggles by atmosphere state), V.Srf, V.Vrt, PeA, Mach, drogue/main parachute states, and Gear/SAS buttons. G load is shown by the graphical G meter, not a row. 6-state phase logic drives the corridor bands and row toggles. SAS white-on-red above Mach 3 if OFF.

**VEH** — Vessel name, type, situation, control level, CommNet signal, crew/capacity, ΔV.Stg, ΔV.Tot.

**ACFT** — Full EADI ball (pitch ladder, roll pointer, aircraft symbol, Hdg/Pitch/Roll readouts) plus a right panel: Alt.Rdr, V.Srf, IAS, V.Vrt, Ma/G split, AoA/Sl split, and GEAR / AIRBRK / BRAKES buttons. The AIRBRK button reads IN (stowed) or OUT (deployed, cyan) from the airbrake CAG (`AIRBRAKE_CAG`, base 38).

**ROVR** — Rotating compass (shared renderer, `Compass.ino`; heading readout, cardinal ring, rover icon, target-bearing triangle when a target is set) with a Dist strip along the bottom. Left column: V.Srf, EC%, and BRAKES / GEAR / SAS buttons. Top corners: FWD / REV drive-state blocks (both muted = NEUTRAL) driven by wheel throttle. Right column: Elev (surface elevation, Alt.SL − Alt.Rdr), Pitch and Roll tilt indicators.

### Context Switching

Each panel runs its own ladder. Splitting them is what makes two displays worth more than one: the single-display ladder had to interleave vessel-type and mission-phase rules and rank one above the other, and vessel type won — so a spaceplane on the pad never saw the pre-launch board, and neither a spaceplane nor a rover closing on a target ever auto-routed to DOCKING. With a panel per question, neither ladder masks the other and both are complete.

**Info Display 1 — `vehicleContextScreen()`**

| Priority | Condition | Screen |
|---|---|---|
| 1 | Rover | ROVER |
| 2 | Plane in the atmosphere | AIRCRAFT |
| 3 | Everything else | SPACECRAFT (PFD) |

This panel answers one question and admits no exceptions: the answer is always an instrument for the vehicle. VEHICLE INFO is a status summary rather than an instrument, so it belongs to the mission panel, which routes to it from the surface rule; it stays one press away here on the VEH key. With that, no reachable pairing shows the same screen on both panels.

**Info Display 2 — `missionContextScreen()`**

| Priority | Condition | Screen |
|---|---|---|
| 1 | Pre-launch (`sit_PreLaunch`) — every vessel type, spaceplanes included | LAUNCH (pre-launch board) |
| 2 | Descending, periapsis inside the atmosphere, and either above it or past `REENTRY_CTX_MACH` | RE-ENTRY |
| 3 | Descending faster than `LNDG_CTX_VVERT_MS` below `LNDG_CTX_ALT_M` radar altitude (planes in atmosphere excluded) | POWERED DESCENT |
| 4 | Target within `DOCK_DIST_WARN_M` (200 m) | DOCKING |
| 5 | Landed or splashed | TARGET if one is set, else VEHICLE INFO |
| 6 | Node planned and T+Ign < `MNVR_CONTEXT_LEAD_S` (10 min), including during the burn | MANEUVER |
| 7 | Target between 200 m and `TGT_CONTEXT_MAX_M` (2000 m) | TARGET |
| 8 | On EVA | TARGET |
| 9 | An aircraft in an atmosphere, apoapsis below the top of it — flying, not climbing out | NAVIGATION |
| 10 | Everything else | ORBIT |

Rule 8 puts a Kerbal outside the craft on the screen for the one thing they are doing: getting to something. Rules 4 and 7 already cover a target that is set and close; rule 8 catches the rest, including the case that matters most — nothing selected, where TARGET's `NO TARGET SET` fullscreen is honest advice rather than a dead end, and where ORBIT was previously offering a Kerbal an apoapsis. It sits below the surface rule deliberately: a Kerbal standing on the Mun is not on an approach. Rule 9 is what NAVIGATION exists for; see the screen entry above. Its condition is exactly the one Info 1 uses to put AIRCRAFT up, so NAV appears if and only if its partner PFD is the aircraft PFD — the two panels are the standard airliner pair, or they are not paired at all. Stated as bare "in an atmosphere" it also caught a booster a kilometre off the pad, handing a rocket a compass rose during ascent and then swapping to ORBIT mid-burn as apoapsis crossed the top of the atmosphere; the apoapsis test does the remaining work, keeping a spaceplane on ORBIT once it is climbing out. Rule 2 reuses the RE-ENTRY screen's own corridor classifier (`_reCorridor()` / `_rePeRegime()`), so the rule and the screen cannot disagree about what a re-entry is, and it fires before entry interface. Rule 3 replaced `type_Lander && sit_SubOrb`, which missed every Ship landing on the Mun and fired during ascent, since sub-orbital is also what a rocket climbing out looks like. Rule 5 replaced ORBIT-for-everything, which gave a rover parked on Duna an apoapsis, periapsis, inclination and period.

Three of these are new, and all three are things a single display could not afford:

- **ORBIT as the resting state.** Previously "Orbit is never auto-selected — reach it from the sidebar", because auto-selecting it would have cost the pilot their attitude reference. Info Display 1 now holds that permanently.
- **MANEUVER on an imminent burn** (rule 4), gated on time-to-ignition rather than on a node merely existing — nodes persist long after they stop being the pilot's concern. Negative time-to-ignition passes too, so the screen stays up through the burn.
- **TARGET in the approach window** (rule 5), bounded at both ends: inside 200 m rule 3 has already taken it, and past 2000 m the RPOD scope has nothing useful to show.

A deferred dock-check fires on the next `TARGETINFO` message after a vessel switch to catch the case where target distance is not yet known at switch time; it re-runs the ladder rather than routing to DOCKING directly.

**Both ladders run every frame.** They previously ran only at vessel and scene boundaries, which meant the panels did not follow the mission: liftoff, reaching orbit, a node coming due, a target closing and re-entry all passed without either panel reconsidering. Two guards keep continuous evaluation from flapping — a release band on every numeric rule (once a rule owns the screen its threshold widens, so station-keeping at 200 m cannot oscillate the panel) and a minimum dwell between automatic switches (`CONTEXT_DWELL_MS`).

**Manual selection latch.** A sidebar press that lands somewhere other than the ladder's current choice latches that choice. The override means *not this, now* rather than *never again*, so it releases three ways: when the ladder's answer changes (the situation it was set against has passed), when the pilot presses the button owning the screen the ladder currently wants (an explicit return to automatic), or on vessel change / scene entry. `contextSwitchAllowed()` is the gate every auto-route passes.

**AUTO / MAN chip.** A rounded-outline badge at the right of the title bar states, in words, whether the screen was chosen by the ladder or is being held by hand. MAN is dark green — engaged-mode green, the same assignment the ASC key uses while the autopilot is armed, since a held selection is a mode the pilot has engaged; AUTO is grey. Both are outlined rather than filled, matching the sidebar keys, so the badge reads as status and not as something to press. It covers the LAUNCH screen's own ASC/CIRC phase override too, which replaced that screen's red override dot.

### Annunciator Alignment

| Annunciator | InfoDisp equivalent | Match |
|-------------|---------------------|-------|
| `CW_GROUND_PROX` — T.Grnd < 10 s, gear up | T.Grnd / V.Vrt white-on-red at same condition | ✓ Exact |
| `CW_HIGH_G` — g > 9 or < −5 | G: white-on-red at same values | ✓ Exact |
| `CW_LOW_DV` — stage ΔV < 150 m/s | ΔV.Stg white-on-red at same value | ✓ Exact |
| `CW_ALT` — alt < 200 m | Alt.Rdr yellow at 500 m, white-on-red at 50 m | Related (different thresholds) |

---

## Tab Structure

| File | Description |
|------|-------------|
| `KCMk1_InfoDisp.ino` | `setup()` and `loop()` only |
| `AAA_Config.ino` | All tunable constants |
| `AAA_Globals.ino` | Global state, `AppState`, `switchToScreen()`, the two context ladders + `contextScreen()`, the manual-selection latch, `drawStandbyScreen()` |
| `AAA_Screens.ino` | Shared screen infrastructure, layout constants, `drawValue()` helper, dispatch switches |
| `Screen_LNCH.ino` | Launch dispatcher (selects pre-launch / ascent / circularization) |
| `Screen_LNCH_PreLaunch.ino` | Launch pre-launch checklist board |
| `Screen_LNCH_Ascent.ino` | Launch ascent (graphical: ladder, V.Vrt/V.Orb bars, FPA dial, atmosphere gauge) |
| `Screen_LNCH_Circ.ino` | Launch circularization (graphical: orbit diagram, ATT/IGN/Burn-Dur cluster, ΔV bar) |
| `Screen_LNCH_AscentAP.ino` | Ascent Autopilot touch console (keypad, editable params, ARM/DISARM, I2C command channel) |
| `Screen_ORB.ino` | Orbit (Apsides default — graphical orbit + inclination diagram) |
| `Screen_OrbAdv.ino` | Orbit Advanced Elements (text-only, tap-through) |
| `Screen_SCFT.ino` | Spacecraft / PFD — full EADI ball (sidebar PFD, screen index 2) |
| `EADIBall.ino` | Shared EADI attitude-ball renderer (used by the SPACECRAFT and AIRCRAFT PFDs) |
| `Screen_MNVR.ino` | Maneuver — alignment reticle + numeric panel |
| `Screen_TGT.ino` | Target / Rendezvous — RPOD scope + numeric panel |
| `Screen_DOCK.ino` | Docking — approach reticle + numeric panel |
| `Screen_LNDG.ino` | Landing dispatcher (selects powered descent / re-entry) |
| `Screen_LNDG_Powered.ino` | Landing powered descent (graphical: tape, X-Pointer, ATT, V.Vrt) |
| `Screen_LNDG_Reentry.ino` | Landing re-entry (text-only readout board) |
| `Screen_VEH.ino` | Vehicle Info |
| `Screen_ACFT.ino` | Aircraft — full EADI ball |
| `Screen_ROVR.ino` | Rover — compass, FWD/REV drive-state blocks, tilt indicators (screen index 9) |
| `TouchEvents.ino` | Touch debounce and sidebar dispatch (mode cycling / screen switch) |
| `SimpitHandler.ino` | KerbalSimpit message handler and channel registration |
| `I2CSlave.ino` | I2C slave at 0x12/0x13 (`INFO_DISP_UNIT`, set in `KCMk1_InfoDisp.h`) — packet build/fill, command processing, boot handshake |
| `BootScreen.ino` | Randomised KSP-themed boot sequences (B: Mission Log, C: Loading Tips, E: Pre-Flight Checklist) |
| `Demo.ino` | Demo mode — sine-wave `AppState` animation |

**Tab naming note:** The `AAA_` prefix ensures `AAA_Screens.ino` compiles before all `Screen_*.ino` tabs. The `Screen_LNCH_*.ino` and `Screen_LNDG_*.ino` files are mode-specific sub-files dispatched by `Screen_LNCH.ino` and `Screen_LNDG.ino` respectively. Filename `Screen_SCFT.ino` corresponds to the SPACECRAFT screen (sidebar label PFD, screen index 2).

---

## Boot Sequence

1. Hardware init (display, SD, touch, I2C slave)
2. Boot screen renders (one of three KSP-themed sequences chosen at random; header shows live version string via `snprintf`)
3. Simpit connects (or demo mode initialises)
4. InfoDisp builds a status packet and **asserts pin 0 LOW** (INT)
5. Master reads the 10-byte status packet
6. Master sends a 2-byte command packet with `requestType = 0x2` (PROCEED)
7. InfoDisp receives PROCEED, enters `loop()`

The boot screen sequences are seeded from the ARM cycle counter for genuine boot-to-boot variation. Three themes are available, each drawing from a pool of 15 items: **B** — Jeb's Mission Log, **C** — KSP Loading Screen Tips, **E** — Gene's Pre-Flight Checklist.

---

## Version History

| Version | Notes |
|---------|-------|
| **1.7.1** | **NAVIGATION was capturing rocket ascent.** Walking the ten mission scenarios against the ladders caught it immediately: a booster a kilometre off the pad is also in an atmosphere with a trivial apoapsis, so 1.6.0's rule handed a vertical rocket a compass rose from liftoff — and then swapped to ORBIT mid-burn, the moment apoapsis crossed the top of the atmosphere. Two wrong screens and an oscillation, on the busiest three minutes of a flight. The rule now requires the aircraft condition, which is the one Info 1 already uses to put AIRCRAFT up: NAV appears if and only if its partner PFD is the aircraft PFD, so the two panels are the standard airliner pair or they are not paired at all. The apoapsis test stays and does the remaining work, keeping a spaceplane on ORBIT once it is climbing out. |
| **1.7.0** | **EVA gets instruments.** The InfoDisp had no EVA awareness at all — `type_EVA` appeared in exactly two label lookups and in neither ladder — while the other two panels already handled it (the Annunciator tracks `inEVA`, the ResourceDisp swaps to a fixed EVA bar set). What a Kerbal outside the craft actually saw: **SPACECRAFT** on the vehicle panel, whose attitude ball is right but whose numeric panel offered ΔV.Stg, ApA, PeA, T+Ap and T+Ign — five rows, none of which a person holding a jetpack has; and **ORBIT** on the mission panel whenever no target was set, which is an apoapsis for a Kerbal. Both are fixed. SPACECRAFT swaps rows 2–6 to Alt.Rdr, V.Srf, Dist, V.Close and EC, pins row 1 to V.Orb so the V.Orb/V.Srf pair is always readable as one (station-keeping vs drifting), and keeps RCS and SAS, which mean exactly as much on a jetpack. Suit charge takes row 6 because ΔV.Stg would read a flat green 0 m/s, and it is the number that ends an EVA; it uses ROVER's own thresholds and reads the same field. The mission ladder gains rule 8, EVA → TARGET, below the surface rule (a Kerbal standing on the Mun is not on an approach) and above NAVIGATION. With no target that is TARGET's `NO TARGET SET` fullscreen, which at that moment is honest advice rather than a dead end. |
| **1.6.0** | **A navigation display, and one compass instead of two.** The mission panel had nothing to offer during atmospheric flight: every rule above it wanted a spacecraft, so a plane in cruise fell through to ORBIT, whose apsides and period mean nothing to an aircraft. **NAVIGATION** fills that gap as the plan view beside the PFD — the standard airliner pairing — showing where the vessel is pointed, where it is actually going, and where the target is. The discriminator that makes the rule safe is apoapsis against the top of the atmosphere: a jet in cruise and a spaceplane climbing out are both in atmosphere at the same altitude and speed, but only one has an apoapsis above `currentBody.lowSpace`, and the spaceplane keeps ORBIT, which is what it wants. **DRIFT** — ground track minus heading — is the only number on the screen that appears nowhere else on either panel; in an atmosphere it is the crab angle, and it is the difference between a heading that is holding and one that is quietly sliding off. NAV joins the TGT key's ring (TGT → DOCK → NAV) and needs no target to be useful. **The compass rose moved to `Compass.ino`**, shared by ROVER and NAV, following the consolidation the reticle layer (MNVR/TGT/DOCK) and the EADI tape (SCFT/ACFT) already went through — a second hand-maintained rose would have been the third place to fix any bug found in the first. The split is the same one those two use: the shared layer is pure geometry and pixels, owning no telemetry and no state, while each screen supplies its own radii (`CompassGeom`) and its own prev-drawn caches (`CompassCache`, `CompassMarkerCache`) so redraw gating stays per-screen, and decides what a marker means — ROVER draws one, NAV two. ROVER's rendering is unchanged and was proven so rather than assumed: the old and new geometry were extracted and run side by side on the host, comparing every emitted draw call — coordinates, colours and call order — at every heading and every marker bearing in quarter-degree steps, 23,041 comparisons with zero divergence. NAV adopts ROVER's card geometry outright, since the vertical budget between the title rule and the bottom edge is identical on both screens and ROVER's numbers already spend it correctly; sizing the card independently had put the heading box four pixels into the title rule. |
| **1.5.0** | **VEHICLE INFO belongs to the mission panel alone.** Info Display 1 routed to it for a recoverable vessel, which made `VEH \| VEH` reachable — the only pairing where both panels showed the same screen — for a capsule down and awaiting recovery. VEHICLE INFO is a status summary, not an instrument, so it is mission-panel material; the surface rule added in 1.4.0 already routes there. `vehicleContextScreen()` is now exactly the vessel-type question with no exceptions, and every one of the fourteen reachable pairings shows two different screens. VEH remains one press away on Info 1's own key. **AUTO/MAN chip restyled:** a rounded-rectangle outline (`drawRoundRectOutline()`, built from `drawLine` plus a Bresenham quarter-arc since the driver has no round-rect primitive), with MAN in dark green rather than cyan — engaged-mode green, matching the ASC key's armed colour, since a held selection is a mode the pilot has engaged rather than an exception. Outlined rather than filled, like the sidebar keys, so it reads as status and not as a control. |
| **1.4.1** | **The recoverable rule outranked the vehicle it was flying.** `vehicleContextScreen()` tested `isRecoverable` first, so a rover being driven on Kerbin — recoverable, since KSP reports anything on the home body that way — showed VEHICLE INFO instead of ROVER, and a plane rolling out after landing showed it instead of AIRCRAFT. Worse, the mission panel's new surface fallback also routes to VEHICLE INFO, so both panels showed the same static summary while the pilot was actively driving. The single-screen ladder this was split from had the recoverable rule at position 6, below plane and rover; flattening it to the top in 1.1.0 was the regression, and it only became visible once 1.4.0 made the ladder run every frame instead of at boundaries. Vessel type now comes first and recoverable is the fallback before SPACECRAFT, restoring the original precedence. `ROVR \| VEH` and `ACFT \| VEH` are good pairings — the instrument for what you are operating beside the summary of it. |
| **1.4.0** | **The panels follow the mission.** Both ladders were evaluated only at vessel and scene boundaries — `contextScreen()` had three call sites, none of them per-frame — so liftoff, reaching orbit, a node coming due, a target closing and re-entry all passed without either panel reconsidering. Whatever was true at the last boundary stayed on screen until the pilot pressed something, which made the MANEUVER and TARGET rules added in 1.1.0 effectively unreachable in ordinary flight, and left a spaceplane on AIRCRAFT in vacuum showing IAS, Mach and AoA. New `updateContextScreen()` runs both ladders every frame. **Flap guards:** a release band on each numeric rule (the threshold widens once that rule owns the screen, so holding station at 200 m or a burn sitting at ten minutes out cannot oscillate the panel) plus `CONTEXT_DWELL_MS`, a minimum gap between automatic switches, as a blanket guard for what the bands do not cover. **The latch became releasable**, which continuous evaluation requires — otherwise one exploratory press would disable routing for the whole flight. An override now records what the ladder was recommending when it was set (`_latchedAgainst`) and releases when that answer changes, so parking on ORBIT during a rendezvous holds until the target actually reaches docking range; pressing the button that owns the ladder's current choice is an explicit return to automatic. `isManualLockScreen()` is gone: RE-ENTRY pinned itself because it was manual-only and a `VESSEL_CHANGE` would yank it away, and both halves of that are now false. **Three new rules.** RE-ENTRY, reusing the RE-ENTRY screen's own corridor classifier so rule and screen agree by construction, gated on speed so an aircraft in level flight — whose periapsis is far underground and therefore "in the corridor" — does not trip it. POWERED DESCENT re-gated on proximity and descent rate instead of `type_Lander && sit_SubOrb`, which missed every Ship landing on the Mun and fired on the way up. And a surface fallback: landed or splashed routes to TARGET when one is set, otherwise VEHICLE INFO, replacing ORBIT-for-everything. **AUTO / MAN chip** in the title bar states which mode the panel is in, in words rather than a coloured dot; it replaces the LAUNCH screen's red override dot and covers that screen's phase override too. |
| **1.3.1** | **Demo mode now acknowledges the Ascent Autopilot console.** With 1.3.0 making annunciations wait for Controller_Main, and `apEnqueueCmd()` a no-op in demo, the ARM button on the bench did nothing at all — correct, but useless for exercising the console. The demo now plays the master's part: `apDemoApplyCommand()` applies ARM/DISARM and all six parameter setters to a demo-side autopilot model, and `stepDemoState()` publishes that model into `state.ap*` instead of the literals it used to hardcode, so the command-out/value-back round trip closes and the pending cue clears exactly as it does in flight. Previously the demo overwrote every AP field every frame, so even if a command had been applied the demo would have stamped over it on the next tick — which is why a parameter edit's cyan pending value could never clear on the bench either. The ascent animation is now gated on the armed state rather than deriving it: the demo boots armed so the guidance outputs animate as they always have, DISARM parks it in IDLE and holds the timer, ARM resumes where it left off. The `AP_CMD_*` opcodes and `AP_ROLL_OFF` moved from the console tab to `KCMk1_InfoDisp.h`, which is their proper home as the command contract shared by the console, the I2C layer and now the demo — and is required, since `Demo.ino` compiles before `Screen_LNCH_AscentAP.ino` in the concatenated sketch. |
| **1.3.0** | **Armed annunciations now show the autopilot's state, not the pilot's request.** Tapping DISARM flipped the ARM button and the ARMED/DISARMED banner to DISARMED immediately, before Controller_Main had acknowledged anything — so a command that was delayed, dropped, or never sent left the panel reading DISARMED while the autopilot was still flying the vehicle. That is the one direction of this control that must not be optimistic. The two questions are now separate functions and named for which is which: `apArmCommanded()` (what the pilot has asked for) decides only what the *next* tap does, so a second tap reverses a pending command rather than repeating it and a lost acknowledgement cannot lock the button out; `apArmedAnnunciated()` (what the autopilot reports) is what everything that *tells* the pilot anything reads — the button, the banner, and the sidebar ASC key — so none of them can disagree and none can say DISARMED while the vehicle is armed. Applied to ARM as well as DISARM: claiming ARMED before the autopilot has it is the same class of lie. **A tap still gets immediate feedback**, since a touchscreen has no detent — but as a pending cue rather than a state change: the ARM button's border goes to the same cyan the editable fields use for an unreconciled value, its hint reads `ARMING…` / `DISARMING…`, and the banner appends `...`. The cue clears when the echo lands. `apEnqueueCmd()` now returns whether it actually queued anything and the cue is gated on that, so in demo mode — and on unit 1, where the command channel is compiled out — a tap that sends nothing does not leave a cue that can never clear. |
| **1.2.1** | **A panel booting into a running flight no longer sits on standby.** `flightScene` was only ever set by `SCENE_CHANGE_MESSAGE`, which Simpit sends as an *event* — there is no way to ask for the current scene. A panel that boots (or whose USB re-enumerates) while a flight is already running therefore never hears it and sits on the standby screen until the pilot happens to change scene or vessel. Since Simpit sends `FLIGHT_STATUS` only from a flight scene, receiving it while the panel believes it is not in one is proof that the transition was missed, so the panel now adopts the scene there. Both routes go through one new `enterFlightScene()` so they cannot drift apart. The hook sits at the end of the `FLIGHT_STATUS` handler rather than earlier, so the message's own vessel data is already applied and the context ladder routes on the real vessel rather than on default state. The same defect and the same fix apply to the Annunciator and the Resource Display, which share the pattern and boot alongside this panel. |
| **1.2.0** | **PFD is now the top sidebar key on both units.** It was second, below LNCH, because the sidebar was ordered purely by mission phase. That ordering made sense when one panel had to serve every phase; with the roles split it buries the screen a pilot returns to from anywhere — and on unit 1 it buried that panel's entire reason for existing. The remaining five keys keep the mission-phase progression (LNCH, ORB, TGT, LNDG) and the single-mode key (VEH on unit 1, ASC on unit 2) stays at the bottom, so the change is a promotion rather than a reshuffle. Order is identical on both panels, so one reach serves either. `SB_PFD_BTN` and `SB_LNCH_BTN` swap values and the two tables reorder; every consumer already went through the named constants, so nothing else moved. |
| **1.1.3** | **The ASC key stayed green after a DISARM.** The sidebar read `state.apArmed` — the armed flag Controller_Main echoes back in its AscentStatus push — while everything on the Ascent Autopilot screen reads `apGArmed()`, which prefers the pilot's commanded intent while a tap is still in flight. So the ARM button flipped to DISARMED on touch and the key stayed green until the echo landed, and stayed green *indefinitely* whenever it did not: in demo mode `stepDemoState()` drives `state.apArmed` from the demo phase and never acknowledges a DISARM tap at all, so the override never cleared and the two annunciations disagreed permanently. Two things annunciating one fact were reading different sources of truth. New public `apArmedEffective()` exposes what the ARM button itself renders, and both `drawSidebar()` and `updateSidebar()` now read it, so the key and the button cannot disagree — including during the command round-trip, where the key now turns green on the tap rather than on the echo, matching the button. |
| **1.1.2** | **The sidebar stops spending the alerting colours.** Its keys were filled `TFT_NAVY` (#00007B) when unselected and `TFT_CORNELL` when selected — and `TFT_CORNELL` is a red, #B51C19. That put the vocabulary's highest-urgency colour on screen permanently, on every screen, for the whole flight, marking the least urgent fact the panel knows (the page you are already looking at) while this same panel uses white-on-red for the ΔV, G-load and ground-proximity alarms. The ASC key was purple/violet, a hue flight-deck convention does not assign at all. Meanwhile the content-area buttons (RCS, SAS, GEAR, BRAKES) already followed the project's documented language correctly, so the sidebar was the one place using a different vocabulary. The keys are now achromatic and selection is reverse video — white-on-black unselected, black-on-grey selected — which is what bezel-key flight decks do: the Garmin G1000 softkey row, the closest analogue to this sidebar, is exactly this, and FAA display guidance assigns light grey to inactive soft-button labels for the same reason. No new colour constants; both `ButtonLabel` literals come from the existing palette, and legend contrast improves at the ends (21:1 unselected, 5.5:1 selected). **Colour returns to the sidebar with a job:** the ASC key is `TFT_DARK_GREEN` while the autopilot is armed — engaged-mode green, the standard assignment — so the one sidebar key with state worth annunciating is the only key that carries a hue, and only while that state holds. It previously looked identical whether the autopilot was idle or flying the vehicle. That state colour needs a repaint path, since `drawSidebar()` runs from `drawStaticScreen()` and therefore only on a screen change: new `updateSidebar()` runs once per steady-state frame and repaints the 84 px strip only when the armed state actually flips, so the key turns green during an ascent rather than at the pilot's next screen switch. It compiles out on unit 1, which has no state-coloured key. Geometry is unchanged — the flat fills, square corners and 1 px rules were already right. |
| **1.1.1** | **Info Display 1 now actually comes up on the PFD.** Two defaults were still pointing at LAUNCH after the 1.1.0 role split. First, `SCREEN_HOME`: the panel's boot value, its demo-mode entry screen and the screen it parks on behind the standby splash were all hard-coded to `screen_LNCH` on both units, so the vehicle-type panel opened on LAUNCH and only reached the PFD once a scene or vessel change fired a context switch — and never at all if the panel booted into an already-running flight scene. It is now per unit: SPACECRAFT on unit 1, LAUNCH on unit 2. This is the role's resting screen, not its ladder fallback — unit 2's ladder rests on ORBIT, but a panel that has never seen telemetry is far likelier to be about to launch. Second, and the reason the PFD did not appear even on the pad: **`vehicleContextScreen()` returned VEHICLE INFO for a vessel sitting on the launchpad.** KSP reports a pre-launch vessel as recoverable, and the split had left the recoverable rule as the ladder's top entry. In the old combined ladder the pre-launch rule sat above it and shielded it; moving pre-launch to the mission panel removed that shield without replacing it. The rule is now gated on `!(situation & sit_PreLaunch)` explicitly, so a rocket, spaceplane or rover on the pad routes to its attitude screen while a landed-and-recoverable vessel after a flight still routes to VEHICLE INFO. Five ladder cases added for the pad. |
| **1.1.0** | **The two Info Displays stop being clones.** They ran byte-identical firmware differing only in I2C address, so both auto-routed to the same screen at the same moment: two panels showing one screenful of information, twice, and re-converging at exactly the busiest moments — crossing 200 m to a target snapped both to DOCKING and destroyed whatever second view the pilot had set up. Each panel now has a role. **Info Display 1 is the vehicle-type panel** (*what am I flying?*) and holds the PFD family; **Info Display 2 is the mission-phase panel** (*what phase am I in?*) and holds the plan and target views. This is not just a default: the old `contextScreen()` was two unrelated ladders interleaved, forced to rank one above the other, and vessel type won — so its rules 1–2 masked its rules 3–5. A spaceplane on the pad never saw the pre-launch board; neither a spaceplane nor a rover closing on a target ever auto-routed to DOCKING. `contextScreen()` now splits into `vehicleContextScreen()` and `missionContextScreen()`, one per panel, and neither masks the other. The plane/rover exclusion on the pre-launch board — which existed only to paper over that masking — is gone. **Three routes the second panel pays for:** ORBIT becomes the mission panel's resting state (previously never auto-selected, because on one display it would have stolen the attitude reference); MANEUVER auto-selects on an imminent burn, gated on time-to-ignition (`MNVR_CONTEXT_LEAD_S`, 10 min) rather than on a node merely existing, and stays up through the burn; TARGET auto-selects in a bounded approach window (200 m to `TGT_CONTEXT_MAX_M`, 2000 m). **Manual selection now latches** — any sidebar press survives context events until the vessel or scene changes (`contextSwitchAllowed()`), generalising the per-button `_pfdManualOverride` pattern; with each panel doing a job, a screen the pilot parked is a screen they are using. **Sidebars moved outboard.** Both panels sit inboard on their own half of the console, so their inner edges meet at the centreline; unit 1's sidebar moves to its left edge, mirroring the pair about that line. The content areas become adjacent and read as one field, every content-area touch target on unit 1 moves 84 px inboard rather than away, and each button column falls under its own hand. Implemented as a canvas-origin offset (`canvasContentRegion()`) rather than by editing ~700 drawing calls: the RA8876 canvas stride stays `SCREEN_W`, so advancing the base address by `CONTENT_X` pixels shifts every row identically. Unit 2's offset is zero and its register writes are unchanged. **Button 5 is per-unit:** unit 2 keeps the Ascent Autopilot console and is now its sole owner — `apEnqueueCmd()` compiles to a no-op on unit 1, so the single-editor assumption behind the pending-edit reconcile is a property of the command channel rather than of the navigation table. Unit 1 carries VEH there instead, which also shortens its PFD ring from four modes to three. **Cycling is quicker:** a repeat press on the same sidebar button now debounces at 150 ms instead of 500 ms (a first press, a different button, and content-area taps keep the full window), so a three-deep cycle no longer costs 1.5 s of enforced waiting. Buttons 0–4 remain identical on both units and all thirteen screens stay reachable from either panel — the units differ in defaults, not in capability. `INFO_DISP_UNIT` moved from `AAA_Config.ino` to `KCMk1_InfoDisp.h`, since the layout constants and sidebar tables are now compile-time conditional on it. |
| **1.0.8** | **DOCK now carries the same four angle rows as TGT.** It showed only `V.Brg`/`V.Elv`, as two full-width rows, with the nose-to-port angle available solely as the unsigned `Nos.Off` magnitude — so the signed pair TGT shows was missing on the screen that needs it most. Rows 4 and 5 are now split pairs mirroring TGT exactly: `Brg`\|`Elv` (the port relative to the nose, informational) then `V.Brg`\|`V.Elv` (approach-path error about the target axis, colour-banded, `---` past 90°). `Nos.Off` keeps the freed full-width row 6 and stays colour-banded, so alignment status is still signalled and DOCK is a superset of TGT rather than a rearrangement. One trade-off: the half-width cells cannot hold a decimal — measured against the real font, `V.Brg:` plus `+12.3°` needs 204 px against 178 available — so both pairs are whole degrees, matching TGT. `V.Lat` (m/s, two decimals) remains the precision instrument at docking range, and the marker itself gives sub-degree visual resolution. |
| **1.0.7** | **Off-scale markers are now dimmed instead of silently lying.** All three reticles clamp a marker that falls beyond full scale, which keeps its direction honest but discards its distance — and drew it identically to a real reading. The ambiguous bands were wide: DOCK honest to 17.3° then pinned all the way to 162.9° where the anti-target takes over (146° wide), TGT honest to 51.9° then pinned to 128.5° (77°), MNVR honest to 17.3° then pinned to 180° with no antipodal marker at all (163°). A pinned marker is now drawn at half brightness — `TFT_DIM_VIOLET` for the port/target, `TFT_DIM_NEON_GRN` for velocity, `TFT_NAVY` for MNVR's maneuver diamond — so it reads as pegged rather than as a live value, while `Nos.Off` / `Brg` / `Elv` carry the true angle. The colour change is forced through the redraw gate by a cached pinned flag: the transition can move the marker as little as 1 px, which the `>1 px` movement test would otherwise swallow. |
| **1.0.6** | **Angle readouts now come from the same projection the markers do, and each quantity has one name.** The `Brg`/`Elv`/`Err` rows still computed horizon-frame heading/pitch differences after 1.0.3 moved the markers to a true boresight projection, so the numbers contradicted the picture *and* the ring-derived colour bands: a node 10° off the nose printed 13.8° at 45° pitch and 62.9° at 80°, and a 6° approach-path error printed 9.9° at 60° pitch — yellow, while the marker sat inside the green ring. MNVR's `Brg`/`Elv` now come from the same boresight angles that place its marker and gate its alignment box. **TGT's `Brg`/`Elv` row was showing the vessel's compass bearing:** it printed `state.tgtHeading` wrapped to ±180 under a comment claiming "positive = target to the right", so a target dead ahead on heading 120 read +120 instead of 0. It is now nose-relative, the same quantity MNVR shows. **Naming:** DOCK's `Vel.Brg`/`Vel.Elv` and TGT's `Err`/`Err` were the identical value under two names — both are now **`V.Brg`/`V.Elv`**, matching the project's existing `V.` prefix for velocity quantities. That pair is now measured about the target axis, making it the exact 3D approach-path error rather than a horizon-frame difference, and it shows `---` past 90° (the idiom `T+Int`/`T+Dock` already use) where the signed pair stops being flyable — which also keeps it inside TGT's 178 px half-cells, verified against the real font metrics. |
| **1.0.5** | **The reticle rings are now the colour bands.** Every reticle draws its good zone at full-scale/4 and its middle ring at full-scale/2, but the numeric thresholds agreed with neither on DOCK (ring 5°, numbers green to 10°) nor TGT (ring 15°, numbers yellow at 5°). Warn and alarm are now exactly those two radii — DOCK/MNVR 5°/10°, TGT 15°/30° — so green inside the inner ring, yellow to the middle ring, red beyond, and the picture cannot disagree with the numbers. This also corrects an inversion: TGT was *tighter* than DOCK (5/15 vs 10/20), the opposite of its own comment and of what the two phases need. MNVR's `angCol` lambda ignored its argument and returned a fixed colour, so its angle readouts never changed colour at all; it now uses `ATT_ERR_WARN_DEG`/`ATT_ERR_ALARM_DEG` (new) like the other two. A distinct target-prograde glyph for the TGT/DOCK velocity marker was prototyped in this cycle and **not adopted** — the plain prograde symbol is kept on both the reticles (target-relative velocity) and the PFD ball (orbital/surface), with context and the per-screen legend distinguishing them. |
| **1.0.4** | **Every boresight display is now body-referenced.** MNVR and TGT joined DOCK, the EADI ball markers and the re-entry retro ball in building their axes with the vessel roll, so screen up is always the craft's roof and one pilot instinct serves every display. Previously MNVR and TGT were horizon-referenced: with the craft rolled 90°, pitching up slid the MNVR node marker *sideways* rather than toward the crosshair. TGT mattered for a second reason — TGT and DOCK are the same task at different ranges (shared sidebar button; the Dist row goes white-on-green below 200 m to say "switch to DOCK"), so a frame difference re-anchored every marker at exactly the moment the pilot crossed over. With no screen left wanting the horizon frame, `reticleComputeAngles()` drops its `bodyReferenced` argument and the rule is documented rather than configured. Note the two marker classes respond oppositely, on every screen and by design: steer *toward* a position marker (node/port/target), thrust *away* from a velocity marker (prograde). |
| **1.0.3** | **True boresight projection on every attitude display.** MNVR / TGT / DOCK, the EADI ball markers and the re-entry retro ball all plotted world directions by subtracting headings and pitches and scaling the differences — valid only near zero pitch. Because heading lines converge toward the poles, the bearing axis stretched by roughly 1/cos(pitch): a 10° error read 14° at 45° pitch and 20° at 60°, so the degree-labelled rings were misreporting, anisotropically. Past ~80° it inverted outright — which is exactly where **a radial-in/out maneuver node puts the vessel**, making MNVR's alignment marker actively wrong for that class of burn. All five now call `kspBodyAxes` + `kspBoresightAngles` (KerbalDisplayCommon 3.3.0): the vessel attitude resolves into 3D body axes and the direction projects azimuthal-equidistant about the boresight, so a marker's distance from the crosshair is its true angular offset at any attitude. `Nos.Off` (DOCK) and the MNVR alignment box are now exact rather than Pythagorean approximations, and re-entry `AoA` is the true nose-to-airflow angle. `ReticleGeom::rollRef` is gone — body vs horizon reference is now chosen by whether the caller builds its axes with roll or with 0 — and `reticleComputeAngles()` takes that flag. MNVR stays horizon-referenced pending a separate decision (`MNVR_BODY_REF`). Verified over 17,664 attitude/offset combinations: worst radius error 0.02°. |
| **1.0.2** | **Reticle consolidation + one roll handedness.** The marker layer moved out to KerbalDisplayCommon 3.2.0 (`ReticleGeom` / `ReticleDotCache` / `ReticleAngles`, `reticleProject` / `reticleClampDot` / `reticleEraseDot` / `reticleRepairDotChrome` / `reticleUpdateDots`), beside the `reticleDrawBase` chrome it already drew on; `reticleComputeAngles()` stays in `AAA_Screens.ino` as the only reticle code that reads `state`. **MNVR now runs the shared layer**, deleting its own `_mnvrClampMrk` and `_mnvrRepairChrome` copies (a byte-for-byte duplicate of the DOCK/TGT chrome repair) — three reticle screens, one implementation. `reticleUpdateDots` takes a `ReticleAngles` instead of eight loose floats and no longer reads the global `state` for roll (it rides in `ReticleAngles::roll`). **Fixed: the re-entry retro-ball marker rotated by `+state.roll`** while the EADI ball and the re-entry screen's own bank pointer both use `-state.roll` — it spun the wrong way as bank built up. All three body-referenced displays now call the single `kspCockpitOffset`, so this class of disagreement can no longer exist. `eadiHdgDelta` moved to the library; the last three single-use wrappers (`_mnvrWrapErr`, `_reWrap180`, `_dockWrapErr`/`_tgtWrapErr`) are gone. |
| **1.0.1** | **DOCK/TGT velocity marker frame fix.** The green VEL marker was plotted target-referenced (`tgtHeading − tgtVelHeading`) on a nose-referenced reticle, so it mixed two frames and did not respond to RCS translation the way a prograde marker should. All four reticle markers are now nose-referenced in `reticleComputeAngles()`; the target-referenced approach-path error moved to new `appBrg`/`appElv` fields, which still drive DOCK Vel.Brg/Vel.Elv and TGT B.Err/E.Err unchanged (and are now exactly the on-screen VEL↔PORT gap). DOCK additionally sets the new `ReticleGeom::rollRef`, rotating its marker layer by −roll into the craft's body axes — the same transform `eadiDrawAdiMarker()` applies to navball markers — so screen up/right always matches the RCS translation axes: velocity marker up and right → thrust left and down. TGT/MNVR stay horizon-referenced. |
| **1.0.0** | Production release. **Navigation redesign:** mode switching moved from title-bar taps to the left sidebar, which cycles its buttons; sidebar reduced from ten buttons to **six**. A first press of a button jumps to its context/primary screen; pressing the button that already owns the active screen cycles its modes, and the caption shows the active mode. Multi-mode buttons: **PFD** covers SPACECRAFT / AIRCRAFT / ROVER / VEHICLE, **ORB** covers ORBIT / Advanced Elements / MANEUVER, **TGT** covers TARGET / DOCKING, **LNDG** covers POWERED DESCENT / RE-ENTRY; **ASC** (Ascent Autopilot) stays standalone. Title-bar taps no longer switch screens. VEH and MNVR are reachable by cycling their host button (VEH still auto-selected for recoverable vessels via context). **Refactors:** shared `sasNavballLabel()` for SCFT/ACFT/ROVR; heading-wrap routed through `eadiHdgDelta`; dead EADI-ball constants removed; shared Ascent/Circ 8-row readout panel; shared SCFT/ACFT EADI tape + roll-readout scaffolding; shared TGT/DOCK reticle dot layer. **Fixes (audit batches A/E/F):** core/infra cleanup, reticle-bar stale statics, corrected AoA-arc comment. Compile-time `INFO_DISP_UNIT` selector picks the target board (Info Display 1 = 0x12, Info Display 2 = 0x13) from one firmware. Chip naming standardized to LT7683 (RA8876-compatible). README navigation section rewritten for the six-button model. Requires KerbalDisplayCommon ≥ 3.1.2. |
| **0.14.0** | Ascent Autopilot screen added (sidebar button **ASC**, screen index 12) — touch console for the Simpit ascent autopilot with an on-screen numeric keypad, editable mission/vehicle parameters, and touch ARM/DISARM. Outbound I2C packet extended 4→10 bytes with an Ascent-AP command frame (`cmdSeq`/`cmdOp`/float payload/XOR); inbound control byte 1 now carries the command `ackSeq`; new 40-byte Master→InfoDisp `AscentStatus` push (sync `0xA5`, dispatched by length). New `Documents/Developer/Ascent_Autopilot_Interface.md` byte-level contract. ORB+ (Advanced Elements) moved from its own button to an ORBIT title tap; Re-entry is now its own **ENTR** sidebar screen. KSP navball markers (prograde/retrograde/target/maneuver) doubled in size and thickened. Reticle markers now sourced from the shared `KerbalDisplayCommon` library (`drawThickLine` added). Hardware baseline updated to Teensy 4.1 + LT7683 1024×600 parallel carrier (KC-01-1912). Dead code removed (`SCREEN_IDS`, unused row-geometry helpers, `_prevShowAp`); stale comments corrected (INT pin 0, 10-byte packet). Bug fixes: keypad modal reset on screen leave/return, relative reconcile tolerance for high target apoapsis, demo-mode command-queue guard. |
| **0.13.3** | Phase 3 complete: I2C slave interface and boot handshake. I2C sync byte corrected from `0xAD` to `0xAE` (collision with ResourceDisp). I2C constants consolidated to `KCMk1_SystemConfig.h`. `idleState` change now immediately calls `drawStandbyScreen()`. Demo→live I2C transition now calls `initSimpit()`. Loop order corrected: touch processed before Simpit (matches Annunciator/ResourceDisp). `setKDCDebugMode()` moved to immediately after `SerialUSB1.begin()`. `simpit` object moved to `AAA_Globals.ino`. `switchToScreen()` now records `lastScreenSwitch` timestamp. `_lndgReentryMode`, `_orbAdvancedMode`, `_prevShowAp`, `_attPrevOrbMode` now reset on vessel switch. `stepDemoState()` now returns immediately if `!demoMode`. `stgWarn` float cast added in `Screen_VEH`. Touch count filter changed to `!= 1`. Boot screen header shows live version string. Phase markers in comments updated. Updated to KerbalDisplayCommon 2.1.0 (thresholdColor float overload, formatTime int64_t, drawValue split-column overload, drawStandbySplash, fmtTime removed — call sites now call formatTime() directly). |
| **0.13.2** | Phase 2 complete: KerbalSimpit integration verified for all 10 screens. Hover screen name corrected (TARGET/TGT). Parachute state machine (STOWED→ARMED→OPEN) implemented. Re-entry 6-state phase logic. DOCK drift decomposition. ATT heading/pitch error colouring gated on atmosphere. Maneuver screen `---` suppression when no node planned. |
| **0.13.0** | Phase 1 complete: display framework with all 10 screens, sidebar navigation, demo mode. |

---

## Notes

- **`debugMode`** defaults to `false`. Set `true` only during development.
- **`demoMode`** defaults to `false` (live Simpit telemetry). Set `true` for the sine-wave demo without KSP.
- **Display rotation** — `DISPLAY_ROTATION = 2` for inverted mounting, `0` for production.
- **Backlight** is on pin 9 (`KCM_TFT_BL`, PWM at `KCM_BL_BRIGHTNESS_PCT`); the master-alarm tone (`tone()` → PAM8302A amp) is on pin 29 (amp enable TONE_EN on pin 30) and the DFPlayer on Serial2 — all defined in `KCMk1_SystemConfig.h`.
- **KerbalDisplayCommon ≥ 3.5.0** is required (hardware rev 2 / `KCM_TFT`, `RA8876_t41_p`; 3.5.0 carries the shared reticle marker layer and the `kspBodyAxes` / `kspBoresightAngles` boresight projection). Do not downgrade.
- **`INTERSECTS_MESSAGE`** — orbit-intercept data (`intercept1/2Dist`, `intercept1/2Time`) is not available in KSP1; those `AppState` fields remain unused stubs. This is unrelated to the TARGET screen's **T+Int** row, which *is* implemented — it is derived on-panel as Dist ÷ |closure rate|.
- **Closure velocity** — the TARGET and DOCKING screens both show closure as a signed **V.Close** value (negative = closing, positive = opening); the intercept-time rows (T+Int / T+Dock) are shown only while closing.
- **EC%** on the pre-launch board and Rover screen may require the Alternate Resource Panel (ARP) mod in KSP1.

Licensed under the GNU General Public License v3.0.
Final code written by J. Rostoker for Jeb's Controller Works.
