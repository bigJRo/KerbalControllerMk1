# KCMk1_InfoDisp

**Kerbal Controller Mk1 — Information Display Panel Sketch** · v1.0.0
Teensy 4.1 firmware for the KSP flight information display module.
Part of the KCMk1 controller system. Operates as an I2C slave under a Teensy 4.1 master.

---

## Overview

The Information Display is a 1024×600 touchscreen panel that presents real-time KSP flight telemetry sourced from KerbalSimpit. It runs on a Teensy 4.1 and receives telemetry over USB serial from a running KSP instance.

The panel provides thirteen screens — Launch, Ascent Autopilot, Spacecraft/Aircraft/Rover/Vehicle (PFD), Orbit (+ Advanced Elements + Maneuver), Target/Docking, Landing (Powered Descent + Re-entry) — ordered to follow mission phase progression from pre-launch through landing. Navigation is via a right-hand sidebar of six buttons. A first press of a button jumps to its context/primary screen; pressing the button that already owns the active screen cycles that button's modes, and the caption shows the active mode. The PFD button covers the four vehicle-type screens (Spacecraft/Aircraft/Rover/Vehicle) and the ORB button covers Orbit, Advanced Elements, and Maneuver. Title-bar taps no longer switch screens.

**Context-switching:** The display automatically selects the most appropriate screen when the scene or vessel changes. Planes in the atmosphere route to AIRCRAFT, rovers to ROVER, pre-launch vessels to LAUNCH (with the pre-launch board), sub-orbital landers to LANDING (powered descent), vessels near a docking target to DOCKING, recoverable vessels to VEHICLE INFO, and everything else to SPACECRAFT (PFD). Orbit is never auto-selected — reach it from the sidebar.

**Colour conventions** are consistent across all screens: dark green = nominal, yellow = caution, white-on-red = alarm, dark grey = inactive/not applicable. Alarm thresholds are aligned with the KCMk1 Annunciator C&W panel.

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
| 2 | Master-alarm buzzer (`tone()`) | OUT | `KCM_AUDIO_TONE_PIN` |
| 7,8 | DFPlayer Mini serial (Serial2 RX2/TX2) | — | `KCM_DFPLAYER_SERIAL` |
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
| KerbalDisplayAudio | — | Master-alarm buzzer (`tone()`, pin 2) + DFPlayer audio support |
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
| `debugMode` | `false` | Serial debug output. Set `false` for production. |
| `demoMode` | `false` | `true` = sine-wave demo, no KSP. `false` = live Simpit. |
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

For the full threshold listing (aircraft, landing, docking, orbit, spacecraft thresholds), see `AAA_Config.ino`.

---

## I2C Protocol

The InfoDisp operates as an I2C slave on the Wire2 bus (`KCM_I2C_BUS`, pins 24/25). Two identical Info Display boards run this same firmware; the slave address is chosen at compile time by `INFO_DISP_UNIT` in `AAA_Config.ino`:

- `INFO_DISP_UNIT 1` → **0x12** (`KCM_I2C_ADDR_INFODISP`) — Info Display 1
- `INFO_DISP_UNIT 2` → **0x13** (`KCM_I2C_ADDR_INFODISP_2`) — Info Display 2

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

The panel displays thirteen screens navigated by six right-hand sidebar buttons. Screen order follows mission phase progression. A first press of a button jumps to its context/primary screen; pressing the button that already owns the active screen cycles its modes, and the button caption shows the active mode. Several buttons are multi-mode: PFD covers SPACECRAFT / AIRCRAFT / ROVER / VEHICLE, ORB covers ORBIT / Advanced Elements / MANEUVER, TGT covers TARGET / DOCKING, and LNDG covers POWERED DESCENT / RE-ENTRY. Title-bar taps no longer switch screens.

| Btn | Sidebar | Screen(s) | First press / cycle | Caption |
|-----|---------|-----------|---------------------|---------|
| 0 | LNCH | LAUNCH | Pre-launch board shown automatically on pad (a press dismisses it into ascent); otherwise press cycles ASCENT ↔ CIRCULARIZATION | PRE / ASC / CIRC |
| 1 | PFD | SPACECRAFT / AIRCRAFT / ROVER / VEHICLE | First press = context screen (SPC/ACFT/ROVR by vessel; VEH auto-selected for recoverable vessels); press cycles SPC → ACFT → ROVR → VEH | SPC / ACFT / ROVR / VEH |
| 2 | ORB | ORBIT / ORBIT ADVANCED / MANEUVER | First press = ORBIT (Apsides); press cycles ORB → ORB+ (Advanced Elements) → MNVR (Maneuver) | ORB / ORB+ / MNVR |
| 3 | TGT | TARGET / DOCKING | First press = DOCKING when a target is within docking range, else TARGET; press cycles TGT ↔ DOCK. NO TARGET SET / DOCKED fullscreen when applicable | TGT / DOCK |
| 4 | LNDG | POWERED DESCENT / RE-ENTRY | First press = POWERED DESCENT; press cycles DESC ↔ ENTR | DESC / ENTR |
| 5 | ASC | ASCENT AUTOPILOT | Standalone — parked at the bottom, physically separated and drawn in a distinct purple. On-screen keypad for parameter entry; touch ARM/DISARM | ASC |

**LNCH** — *Pre-launch board* (automatic when `sit_PreLaunch`, bypassed for planes and rovers): vessel name, type, SAS, RCS, throttle, EC%, crew count, CommNet signal, ΔV.Tot, and parachute CAG states. Tap content area or launch to advance to ascent. *Ascent:* Alt.SL, V.Srf, V.Vrt, ApA, T+Ap, Throttle, T.Burn, ΔV.Stg. *Circularization:* Alt.SL, V.Orb, ApA, PeA, T+Ap, Throttle, T.Burn, ΔV.Stg. Auto-switches at ~6% body radius with hysteresis.

**ASC** — Ascent Autopilot touch console for the Simpit ascent autopilot (which runs on Controller_Main). Three columns: MISSION inputs (target apoapsis, inclination, launch N/S), VEH PROFILE inputs (loft, roll hold, max-G) + the ARM/DISARM button, and GUIDANCE outputs (commanded pitch/heading/throttle, G, dynamic pressure, ApA, PeA). Boxed input fields open an on-screen numeric keypad (or toggle) and can be edited at any time; a pilot edit shows in cyan until the autopilot echoes the accepted value back. The phase banner and ARM button colour reflect the autopilot phase (IDLE / VERTICAL / GRAVITY TURN / COAST / CIRCULARIZE / COMPLETE / ABORT). Edits and ARM/DISARM are sent over I2C — see `Documents/Developer/Ascent_Autopilot_Interface.md`.

**ORB** — *Apsides (default):* graphical orbit + inclination diagram with numeric readouts Alt.SL, PeA, ApA (left panel) and Inc, Period, Arg.Pe, T+Pe/T+Ap (right panel). *Advanced Elements:* text-only readout — SMA, Ecc, PeA, ApA, Alt.SL, V.Orb, Period (left column) and Inc, LAN, Arg.Pe, True Anom, Mean Anom, T+Pe, T+Ap (right column). Reached by cycling the ORB sidebar button (ORB → ORB+ → MNVR); a first press of ORB from another screen returns to Apsides.

**PFD** — Primary Flight Display (SPACECRAFT): full EADI ball with pitch ladder, roll pointer, fixed spacecraft (boresight) symbol, navball velocity/target/maneuver markers, and Hdg/Pitch/Roll readouts. Right panel: Alt.SL, V.Orb/V.Srf (label swaps with orbital mode), ApA, PeA, T+Ap, T+Ign, ΔV.Stg, and RCS/SAS buttons.

**MNVR** — Burn alignment reticle (blue maneuver marker vs a fixed nose crosshair; neon-green alignment box when within 5°) plus a numeric panel: ΔV.Mnvr, ΔV.Plan (total across all planned nodes), ΔV.Stg, T+Ign, T+Mnvr, Burn Dur, Brg/Elv (nose-to-node error split), and RCS/SAS buttons. A ΔV-burn bar sits under the reticle. **NO MANEUVER** fullscreen when no node is planned.

**TGT** — RPOD scope reticle + numeric panel: Alt.SL, V.Orb, Dist, V.Close (signed closure rate — negative = closing), raw bearing/elevation to target (Brg/Elv), approach alignment errors (velocity-to-target, Err/Err), and T+Int (intercept time = Dist ÷ |closure|, shown only while closing). Dist turns white-on-green below 200 m. NO TARGET SET fullscreen when no target.

**DOCK** — Approach reticle + numeric panel: Dist, T+Dock (Dist ÷ |closure|, closing only), V.Close (signed closure rate), V.Lat (total lateral drift magnitude), Vel.Brg/Vel.Elv (velocity-vector bearing/elevation error to the port), Nos.Off (total nose angular offset from the port), and RCS/SAS buttons. SAS: TARGET = green, STAB = cyan, OFF = white-on-red, all other modes = red. DOCKED / NO TARGET SET fullscreen when applicable.

**LNDG** — *Powered descent:* T.Grnd, Alt.Rdr, V.Srf, V.Vrt, Fwd/Lat horizontal drift (roll-corrected, craft heading frame), ΔV.Stg, Throttle/RCS, Gear/SAS. Fwd/Lat thresholds tighten as T.Grnd decreases.

**ENTR** — *Re-entry* (separate sidebar screen): graphical instrument screen — corridor tape, atmosphere-density bar, G meter, and a heat-shield / retrograde alignment ball — with a text panel of T+Grnd/T+Atm (row 0 toggles by descent phase), Alt.SL/Alt.Rdr (row 1 toggles by atmosphere state), V.Srf, V.Vrt, PeA, Mach, drogue/main parachute states, and Gear/SAS buttons. G load is shown by the graphical G meter, not a row. 6-state phase logic drives the corridor bands and row toggles. SAS white-on-red above Mach 3 if OFF.

**VEH** — Vessel name, type, situation, control level, CommNet signal, crew/capacity, ΔV.Stg, ΔV.Tot.

**ACFT** — Full EADI ball (pitch ladder, roll pointer, aircraft symbol, Hdg/Pitch/Roll readouts) plus a right panel: Alt.Rdr, V.Srf, IAS, V.Vrt, Ma/G split, AoA/Sl split, and GEAR / AIRBRK / BRAKES buttons. The AIRBRK button reads IN (stowed) or OUT (deployed, cyan) from the airbrake CAG (`AIRBRAKE_CAG`, base 38).

**ROVR** — Rotating compass (heading readout, cardinal ring, rover icon, target-bearing triangle when a target is set) with a Dist strip along the bottom. Left column: V.Srf, EC%, and BRAKES / GEAR / SAS buttons. Top corners: FWD / REV drive-state blocks (both muted = NEUTRAL) driven by wheel throttle. Right column: Elev (surface elevation, Alt.SL − Alt.Rdr), Pitch and Roll tilt indicators.

### Context Switching

`contextScreen()` selects the screen automatically on vessel or scene change, in priority order:

1. Plane in the atmosphere (`type_Plane` && `inAtmo`) → AIRCRAFT
2. Rover (`type_Rover`) → ROVER
3. Pre-launch (`sit_PreLaunch`) → LAUNCH (pre-launch board) — landed vessels are *not* auto-routed
4. Sub-orbital lander (`type_Lander` && `sit_SubOrb`) → LANDING (powered descent)
5. Target within 200 m (`DOCK_DIST_WARN_M`) → DOCKING
6. Recoverable vessel → VEHICLE INFO
7. Everything else → SPACECRAFT (PFD) — Orbit is manual-select from the sidebar

A deferred dock-check fires on the next `TARGETINFO` message after a vessel switch to catch the case where target distance is not yet known at switch time.

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
| `AAA_Globals.ino` | Global state, `AppState`, `switchToScreen()`, `contextScreen()`, `drawStandbyScreen()` |
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
| `I2CSlave.ino` | I2C slave at 0x12/0x13 (`INFO_DISP_UNIT`) — packet build/fill, command processing, boot handshake |
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
- **Backlight** is on pin 9 (`KCM_TFT_BL`, PWM at `KCM_BL_BRIGHTNESS_PCT`); the master-alarm buzzer (`tone()`) is on pin 2 and the DFPlayer on Serial2 — all defined in `KCMk1_SystemConfig.h`.
- **KerbalDisplayCommon ≥ 3.0.0** is required (hardware rev 2 / `KCM_TFT`, `RA8876_t41_p`). Do not downgrade.
- **`INTERSECTS_MESSAGE`** — orbit-intercept data (`intercept1/2Dist`, `intercept1/2Time`) is not available in KSP1; those `AppState` fields remain unused stubs. This is unrelated to the TARGET screen's **T+Int** row, which *is* implemented — it is derived on-panel as Dist ÷ |closure rate|.
- **Closure velocity** — the TARGET and DOCKING screens both show closure as a signed **V.Close** value (negative = closing, positive = opening); the intercept-time rows (T+Int / T+Dock) are shown only while closing.
- **EC%** on the pre-launch board and Rover screen may require the Alternate Resource Panel (ARP) mod in KSP1.

Licensed under the GNU General Public License v3.0.
Final code written by J. Rostoker for Jeb's Controller Works.
