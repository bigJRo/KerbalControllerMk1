# KCMk1_InfoDisp

**Kerbal Controller Mk1 — Information Display Panel Sketch**
**Version 0.13.3**

Teensy 4.0 firmware for the KSP flight information display module. Part of the KCMk1 controller system. Operates as an I2C slave under a Teensy 4.1 master.

---

## Overview

The Information Display is an 800×480 touchscreen panel that presents real-time KSP flight telemetry sourced from KerbalSimpit. It runs on a Teensy 4.0 and receives telemetry over USB serial from a running KSP instance.

The panel provides ten screens — Launch, Orbit, Attitude, Maneuver, Target, Docking, Landing, Vehicle Info, Aircraft, and Rover — ordered to follow mission phase progression from pre-launch through landing. Navigation is via a right-hand sidebar with one button per screen.

**Context-switching:** The display automatically selects the most appropriate screen when the scene or vessel changes. Planes route to AIRCRAFT, rovers to ROVER, vessels on the pad or landed to LAUNCH (with the pre-launch board), landers in flight to LANDING (powered descent), vessels near a docking target to DOCKING, and all others to ORBIT.

**Colour conventions** are consistent across all screens: dark green = nominal, yellow = caution, white-on-red = alarm, dark grey = inactive/not applicable. Alarm thresholds are aligned with the KCMk1 Annunciator C&W panel where applicable.

---

## Hardware

| Component | Part | Interface |
|-----------|------|-----------|
| Microcontroller | Teensy 4.0 | — |
| Display | RA8875 800×480 TFT | SPI |
| Touch controller | GSL1680F capacitive | Wire1 (pins 16/17) |
| SD card | SD (on RA8875 board) | SPI |
| KSP telemetry | KerbalSimpit plugin | SerialUSB1 (USB COM port 2) |
| I2C slave bus | Master Teensy 4.1 at 0x12 | Wire (pins 18/19) |

### Pin Assignments

| Pin | Function | Direction | Assigned by |
|-----|----------|-----------|-------------|
| 10 | RA8875 SPI chip select | OUT | KerbalDisplayCommon (`RA8875_CS`) |
| 11 | SPI MOSI | OUT | Teensy hardware SPI (fixed) |
| 12 | SPI MISO | IN | Teensy hardware SPI (fixed) |
| 13 | SPI SCK | OUT | Teensy hardware SPI (fixed) |
| 15 | RA8875 RESET | OUT | KerbalDisplayCommon (`RA8875_RESET`) |
| 4 | SD card detect (active-LOW) | IN | KerbalDisplayCommon (`SD_DETECT_PIN`) |
| 5 | SD card SPI chip select | OUT | KerbalDisplayCommon (`SD_CS_PIN`) |
| 16 | GSL1680F SCL (Wire1) | — | KerbalDisplayCommon (`CTP_SCL_PIN`) |
| 17 | GSL1680F SDA (Wire1) | — | KerbalDisplayCommon (`CTP_SDA_PIN`) |
| 3 | GSL1680F WAKE | OUT | KerbalDisplayCommon (`CTP_WAKE_PIN`) |
| 22 | GSL1680F INT (HIGH when touched) | IN | KerbalDisplayCommon (`CTP_INT_PIN`) |
| 2 | I2C INT (active-LOW, output to master) | OUT | I2CSlave.ino (`I2C_INT_PIN`) |
| 9 | Audio PWM (claimed by library, unused) | OUT | KerbalDisplayAudio (`AUDIO_PIN`) |
| 18 | I2C SDA (Wire — master bus) | — | Wire library |
| 19 | I2C SCL (Wire — master bus) | — | Wire library |

**Serial ports:**
- `Serial` (USB COM port 1) — debug output when `debugMode = true`
- `SerialUSB1` (USB COM port 2) — KerbalSimpit telemetry

**I2C note:** Wire1 (pins 16/17) is used exclusively for the GSL1680F touch controller. Wire (pins 18/19) is used for the I2C slave interface to the Teensy 4.1 master.

---

## Dependencies

| Library | Version | Notes |
|---------|---------|-------|
| KerbalDisplayCommon | 2.0.1 | Display primitives, fonts, touch driver, system utils |
| KerbalDisplayAudio | 1.0.1 | Audio library (dependency only — audio not used on this panel) |
| RA8875 (PaulStoffregen) | 0.7.11 | Display driver — do not upgrade without testing; text mode API changed in later versions |
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

## Screens

The panel displays ten screens navigated by the right-hand sidebar. Screen order follows mission phase progression.

| # | Sidebar ID | Title | Toggle |
|---|-----------|-------|--------|
| 0 | LNCH | LAUNCH | Title bar tap: ASCENT / CIRCULARIZATION (auto or manual). On pad: pre-launch board shown automatically. |
| 1 | ORB | ORBIT | Title bar tap: APSIDES / ADVANCED ELEMENTS |
| 2 | ATT | ATTITUDE | — |
| 3 | MNVR | MANEUVER | — |
| 4 | TGT | TARGET | NO TARGET SET fullscreen when no target |
| 5 | DOCK | DOCKING | NO TARGET SET / DOCKED fullscreen when applicable |
| 6 | LNDG | LANDING | Title bar tap: POWERED DESCENT / RE-ENTRY |
| 7 | VEH | VEHICLE INFO | — |
| 8 | ACFT | AIRCRAFT | — |
| 9 | ROVR | ROVER | — |

### Screen Descriptions

**LNCH — Launch / Circularization**

*Pre-launch board* (automatic when `sit_PreLaunch`, bypassed for planes and rovers): Full checklist view covering vessel name, vessel type, SAS, RCS, throttle, EC%, crew count, CommNet signal, ΔV.Tot, and parachute CAG states (drogue deploy/cut, main deploy/cut). All rows colour-coded. Tap content area or launch to advance to ascent.

*Ascent phase:* STATE (Alt.SL, V.Srf, V.Vrt), TRAJ (ApA, T+Ap), PROP (Throttle, T.Burn, ΔV.Stg).

*Circularization phase:* STATE (Alt.SL, V.Orb), TRAJ (ApA, PeA, T+Ap), PROP (Throttle, T.Burn, ΔV.Stg).

Auto-switches at ~6% body radius with hysteresis. Title bar tap sets manual override (red indicator dot).

**ORB — Orbit**

*Apsides view* (default): STATE (Alt.SL, V.Orb), APSI (ApA, PeA), TIME (T+Ap or T+Pe — switches by orbital position; suppressed on near-circular orbit), BURN (T+Ign, ΔV.Tot), PROP (ΔV.Stg | ΔV.Tot), CT (RCS | SAS). Title shows `ORBIT [ SOI ]`.

*Advanced Elements view:* SHAPE (Ecc, SMA), APSE (ApA, PeA), PLANE (Inc | LAN), AN (True | Mean anomaly), PR (Period). Navigating away always resets to Apsides view.

**ATT — Attitude**

CRAFT (Hdg, Pitch, Roll, SAS), VEL (V.Hdg, V.Pit — velocity vector), ERR (Hdg.Err, Pit.Err — nose-to-velocity angle). Sign convention: positive Hdg.Err = nose right of velocity; positive Pit.Err = nose above velocity (matches ACFT AoA convention). Error rows coloured only in atmosphere.

**MNVR — Maneuver**

STATE (Alt.SL, V.Orb), TIME (T+Ign, T+Mnvr), BURN (ΔV.Mnvr, T.Burn, ΔV.Tot), HD (M.Hdg | M.Pitch). All fields show `---` dark grey when no node planned.

**TGT — Target**

STATE (Alt.SL, V.Orb), TGT (Dist, V.Tgt), BRG (Brg, Elv), APCH (Brg.Err, Elv.Err — velocity-to-target errors). `NO TARGET SET` fullscreen when no target. Dist turns white-on-green below 200m.

**DOCK — Docking**

APCH (Alt.SL, Dist, V.Tgt, V.Drft), DR (Drft.H | Drft.V), ERR (Brg.Err | Elv.Err — velocity-to-target; Nos.Brg | Nos.Elv — nose-to-target pointing errors), CT (RCS | SAS). `NO TARGET SET` or `DOCKED` fullscreen when applicable. All angle errors and drift components snap to +0.0 near zero. SAS: TARGET = green, OFF = white-on-red, all others = red.

**LNDG — Landing**

Rows 0-4 shared between modes: T.Grnd (row 0), Alt.Rdr (row 1), V.Srf (row 2), V.Vrt (row 3). T.Grnd shows `---` dark grey when in stable orbit or escape trajectory.

*Powered descent:* Row 4 = Fwd | Lat (horizontal drift in craft heading frame — positive = thrust needed to correct; roll-corrected), Row 5 = ΔV.Stg, Row 6 = Throttle | RCS, Row 7 = Gear | SAS. Fwd/Lat thresholds tighten as T.Grnd decreases (four bands).

*Re-entry:* Row 3 switches between PeA (above 20km radar alt) and V.Hrz (below). Row 4 = V.Srf, Row 5 = Mach | G, Row 6 = Drogue | Main parachute states, Row 7 = Gear | SAS. 6-state phase logic drives Row 0 (T+Atm above atmosphere / T.Grnd in atmosphere), Row 1 (Alt.SL / Alt.Rdr), Row 3 PeA colouring. Parachute state machine: STOWED (speed-coloured) → ARMED → OPEN yellow (semi-deploy) → OPEN green (full-deploy). Atmosphere ceiling uses `currentBody.lowSpace` (correct 70km for Kerbin, not the 18km science altitude boundary). SAS white-on-red above Mach 3.0 if OFF.

**VEH — Vehicle Info**

INFO (vessel name, type, situation), CREW (control level, CommNet signal, crew | capacity), PROP (ΔV.Stg, ΔV.Tot, EC%).

**ACFT — Aircraft**

STATE (Alt.Rdr, V.Srf, IAS, V.Vrt), AT (Hdg | Pitch | Roll), AERO (AoA | Slip, Mach | G), CT (Gear | Brakes | EC%). AoA and Slip sign convention matches ATT Pit.Err/Hdg.Err.

**ROVR — Rover**

STATE (V.Srf, Hdg), NAV (Brg | Err — bearing and heading error to target), TERR (Alt.Rdr, Pitch | Roll), CTRL (Throttle FWD/NEUT/REV, Gear, Brakes, EC%, SAS). Alt.Rdr colouring inverted (close = green). Always routed to by `contextScreen()` for `type_Rover`.

---

## Context Switching

`contextScreen()` selects the screen automatically on vessel or scene change, in priority order:

1. **Plane** (`type_Plane`) → AIRCRAFT
2. **Rover** (`type_Rover`) → ROVER
3. **On ground** (`sit_PreLaunch` or `sit_Landed`) → LAUNCH
4. **Lander in flight** (`type_Lander`) → LANDING (powered descent)
5. **Target within 200m** → DOCKING
6. **Recoverable vessel** → VEHICLE INFO
7. **Everything else** → ORBIT

A deferred dock-check fires on the next `TARGETINFO` message after a vessel switch to catch the case where target distance is not yet known at switch time.

---

## I2C Slave Interface

Slave address **0x12** on Wire (pins 18/19). Interrupt pin 2 (active-LOW) signals the master when a fresh status packet is ready.

### Outbound Packet (InfoDisp → Master), 4 bytes

| Byte | Content |
|------|---------|
| 0 | `0xAD` sync byte |
| 1 | Flags: bit 0 = simpitConnected, bit 1 = flightScene, bit 2 = demoMode |
| 2 | `activeScreen` (ScreenType enum value) |
| 3 | Reserved (0x00) |

### Inbound Command (Master → InfoDisp), 2 bytes

| Bits | Field | Values |
|------|-------|--------|
| 7:4 | requestType | 0x0=NOP, 0x1=STATUS, 0x2=PROCEED, 0x3=MCU_RESET, 0x4=DISPLAY_RESET |
| 3 | idle_state | 1 = switch to standby when not in flight |
| 1 | demoMode | 1 = enable demo mode |
| 0 | debugMode | 1 = enable Serial debug |

### Boot Handshake

After initialisation, the InfoDisp asserts INT and spins on `PROCEED` (0x2) before entering `loop()`. This synchronises the boot sequence across all KCMk1 panels.

---

## Boot Screen

Three KSP-themed terminal sequences selected at random each boot, with pooled item variation for additional run-to-run difference:

- **Sequence B — Jeb's Mission Log:** 15-entry pool, 6 shown. Day-by-day log from a typical Jeb mission. Ends with `TELEMETRY RESTORED. Welcome back, Jeb.`
- **Sequence C — Loading Screen Tips:** 15-tip pool, 6 shown. Parody KSP loading tips with random closing quip.
- **Sequence E — Gene's Pre-Flight Checklist:** 15-item pool, 7 shown. Crew manifest always last. Ends with `BOARD IS GREEN. Godspeed, Jebediah.`

Seeded from ARM cycle counter for genuine boot-to-boot variation. To change text: edit the pool arrays in `_boot_B()`, `_boot_C()`, or `_boot_E()` in `BootScreen.ino`.

---

## Configuration

All tunables in `AAA_Config.ino`.

### Operating Mode

| Constant | Default | Description |
|----------|---------|-------------|
| `debugMode` | `true` | Serial debug output. Set `false` for production. |
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

### Aircraft Thresholds

| Constant | Default | Description |
|----------|---------|-------------|
| `STALL_SPEED_MS` | `0.0` | IAS stall speed (m/s). `0` = disabled. |
| `GEAR_MAX_SPEED_MS` | `160.0` | Max safe gear-down speed (m/s) |
| `ROLL_WARN_DEG` / `ROLL_ALARM_DEG` | `60.0` / `90.0` | Roll warning / alarm (°) |
| `AOA_WARN_DEG` / `AOA_ALARM_DEG` | `10.0` / `20.0` | Angle of attack warning / alarm (°) |
| `SLIP_WARN_DEG` / `SLIP_ALARM_DEG` | `5.0` / `15.0` | Sideslip warning / alarm (°) |
| `G_WARN_POS` / `G_ALARM_POS` | `4.0` / `9.0` | Positive G warning / alarm |
| `G_WARN_NEG` / `G_ALARM_NEG` | `-2.0` / `-5.0` | Negative G warning / alarm |

### Landing Thresholds

| Constant | Default | Description |
|----------|---------|-------------|
| `LNDG_TGRND_ALARM_S` / `LNDG_TGRND_WARN_S` | `10.0` / `30.0` | T.Grnd alarm / warning — gear UP (s) |
| `LNDG_VVRT_ALARM_MS` / `LNDG_VVRT_WARN_MS` | `-8.0` / `-5.0` | V.Vrt alarm / warning (m/s) |
| `ALT_RDR_ALARM_M` / `ALT_RDR_WARN_M` | `50.0` / `500.0` | Radar altitude alarm / warning (m) |
| `LNDG_DROGUE_SAFE_MS` / `LNDG_DROGUE_RISKY_MS` | `750.0` / `850.0` | Drogue STOWED green / red boundary (m/s) |
| `LNDG_MAIN_SAFE_MS` / `LNDG_MAIN_RISKY_MS` | `475.0` / `550.0` | Main STOWED green / red boundary (m/s) |
| `LNDG_DROGUE_FULL_ALT` / `LNDG_MAIN_FULL_ALT` | `2500.0` / `1000.0` | Full-deploy altitude AGL (m) |
| `REENTRY_SAS_AERO_STABLE_MACH` | `3.0` | SAS OFF alarm threshold (Mach) |

Fwd/Lat thresholds (four bands by T.Grnd): see `LNDG_HVEL_*` constants.

### Docking Thresholds

| Constant | Default | Description |
|----------|---------|-------------|
| `DOCK_DIST_ALARM_M` / `DOCK_DIST_WARN_M` | `50.0` / `200.0` | Distance alarm / warning (m) |
| `DOCK_DRIFT_WARN_MS` / `DOCK_DRIFT_ALARM_MS` | `0.1` / `0.5` | Lateral drift warning / alarm (m/s) |
| `DOCK_BRG_WARN_DEG` / `DOCK_BRG_ALARM_DEG` | `10.0` / `20.0` | Bearing/elevation error warning / alarm (°) |

### Other Thresholds

| Constant | Default | Description |
|----------|---------|-------------|
| `DV_STG_ALARM_MS` / `DV_STG_WARN_MS` | `150.0` / `300.0` | Stage ΔV alarm / warning (m/s) |
| `DV_TOT_WARN_MS` | `500.0` | Total ΔV warning (m/s) |
| `ORB_CIRCULAR_PCT` | `1.0` | ApA/PeA within this % → suppress T+ row |
| `ATT_ERR_WARN_DEG` / `ATT_ERR_ALARM_DEG` | `5.0` / `15.0` | Attitude error warning / alarm (°, atmosphere only) |
| `VEH_SIGNAL_WARN_PCT` | `50.0` | CommNet signal warning (%) |

---

## Annunciator Alignment

| Annunciator | InfoDisp equivalent | Match |
|-------------|-------------------|-------|
| `CW_GROUND_PROX` — T.Grnd < 10s, gear UP | T.Grnd / V.Vrt white-on-red at same condition | ✓ Exact |
| `CW_HIGH_G` — g > 9 or < −5 | G: white-on-red at same values | ✓ Exact |
| `CW_LOW_DV` — stageDV < 150 m/s | ΔV.Stg white-on-red at same value | ✓ Exact |
| `CW_ALT` — alt < 500m | Alt.Rdr yellow at 500m, white-on-red at 50m | ✓ Aligned |

---

## Tab Structure

| File | Description |
|------|-------------|
| `KCMk1_InfoDisp.ino` | `setup()` and `loop()` only |
| `AAA_Config.ino` | All tunable constants |
| `AAA_Globals.ino` | Global state, `AppState`, `switchToScreen()`, `contextScreen()` |
| `AAA_Screens.ino` | Shared screen infrastructure, layout constants, dispatch switches |
| `Screen_LNCH.ino` | Launch / Circularization + Pre-launch board |
| `Screen_ORB.ino` | Orbit (Apsides default + Advanced Elements tap-through) |
| `Screen_ATT.ino` | Attitude |
| `Screen_MNVR.ino` | Maneuver |
| `Screen_RNDZ.ino` | Target / Rendezvous |
| `Screen_DOCK.ino` | Docking |
| `Screen_LNDG.ino` | Landing (Powered Descent + Re-entry) |
| `Screen_VEH.ino` | Vehicle Info |
| `Screen_ACFT.ino` | Aircraft |
| `Screen_MISC.ino` | Rover |
| `TouchEvents.ino` | Touch debounce, sidebar and title bar dispatch |
| `SimpitHandler.ino` | KerbalSimpit message handler and channel registration |
| `I2CSlave.ino` | I2C slave at 0x12 — packet build, command processing, boot handshake |
| `BootScreen.ino` | Randomised KSP-themed boot sequence |
| `Demo.ino` | Demo mode — sine-wave AppState animation |

**Tab ordering note:** The `AAA_` prefix ensures `AAA_Screens.ino` compiles before all `Screen_*.ino` tabs. Do not rename these files.

---

## Development Phases

- **Phase 1 (complete):** Display framework — 10 screens, sidebar navigation, demo mode
- **Phase 2 (complete):** KerbalSimpit integration — all screens verified in flight testing
- **Phase 3 (in progress):** I2C slave interface + boot screen. Core implementation complete; pending integrated hardware test.

### Phase 3 Remaining
- Integrated test with full KCMk1 hardware assembly (Teensy 4.1 master + all panels)
- Set `debugMode = false` before final production release

### Phase 4 (planned)
- Maneuver screen reassessment (mostly empty without an active node)
- Any UX improvements identified from integrated testing

---

## Notes

- **`debugMode`** defaults to `true`. Set `false` before production deployment.
- **`demoMode`** defaults to `false` (live Simpit). Set `true` for bench testing without KSP.
- **Display rotation** — `DISPLAY_ROTATION = 2` for inverted mounting, `0` for production.
- **`KerbalDisplayAudio`** is a required dependency but audio output is not used on this panel (claims pin 9).
- **`KerbalDisplayCommon` v2.0.1** is required. The `PrintState` struct in v2.0.0 is a breaking change from v1.x — do not downgrade.
- **INTERSECTS_MESSAGE** — intercept data not available in KSP1; TARGET screen INT rows not implemented.
- **V.Tgt** — always positive magnitude; KSP1 does not provide signed closure velocity.
- **EC%** on pre-launch board and rover screen may require the Alternate Resource Panel (ARP) mod in KSP1.

Licensed under the GNU General Public License v3.0.
Final code written by J. Rostoker for Jeb's Controller Works.
