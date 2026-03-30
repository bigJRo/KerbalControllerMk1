# KCMk1_InfoDisp

**Kerbal Controller Mk1 — Information Display Panel Sketch**
Teensy 4.0 firmware for the KSP flight information display module.
Part of the KCMk1 controller system. Operates as an I2C slave under a Teensy 4.1 master (Phase 3).

---

## Overview

The Information Display is an 800×480 touchscreen panel that presents real-time KSP flight telemetry sourced from KerbalSimpit. It runs on a Teensy 4.0 and receives telemetry over USB serial from a running KSP instance.

The panel provides ten screens — Launch, Apsides, Orbit, Attitude, Maneuver, Rendezvous, Docking, Vehicle, Landing, and Aircraft — ordered to follow mission phase progression from ascent through landing. Navigation is via a right-hand sidebar with one button per screen. Two screens (Launch and Landing) have a secondary mode toggled by tapping the title bar. All ten screens use section labels and dividers for at-a-glance grouping.

Colour conventions are consistent across all screens: dark green = nominal, yellow = caution, white-on-red = alarm, dark grey = inactive/not-applicable. Alarm thresholds are aligned with the KCMk1 Annunciator C&W panel where applicable (in particular, `CW_GROUND_PROX` and `CW_HIGH_G` are numerically identical).

---

## Hardware

| Component | Part | Interface |
|-----------|------|-----------|
| Microcontroller | Teensy 4.0 | — |
| Display | RA8875 800×480 TFT | SPI |
| Touch controller | GSL1680F capacitive | Wire1 (I2C) |
| SD card | SD (on RA8875 board) | SPI |
| KSP telemetry | KerbalSimpit plugin | SerialUSB1 (second USB COM port) |
| I2C slave bus | Master Teensy 4.1 | Wire (I2C) — Phase 3 |

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
| 9 | Audio PWM output (claimed by library, unused) | OUT | KerbalDisplayAudio (`AUDIO_PIN`) |
| 18 | I2C SDA (Wire — master bus) | — | Phase 3 / Wire library |
| 19 | I2C SCL (Wire — master bus) | — | Phase 3 / Wire library |

**Serial ports:**
- `Serial` (USB COM port 1) — debug output only
- `SerialUSB1` (USB COM port 2) — KerbalSimpit telemetry traffic (Phase 2)

**I2C note:** Wire (pins 18/19) is reserved for the Teensy 4.1 master bus (Phase 3). Wire1 (pins 16/17) is used exclusively for the GSL1680F touch controller.

**Override note:** KerbalDisplayCommon and KerbalDisplayAudio pin assignments can be overridden by defining the constant before the `#include`:

```cpp
#define RA8875_CS 9
#define AUDIO_PIN 8
#include <KerbalDisplayCommon.h>
#include <KerbalDisplayAudio.h>
```

---

## Dependencies

| Library | Version | Notes |
|---------|---------|-------|
| KerbalDisplayCommon | 2.0.1 | Display primitives, fonts, touch driver, system utils |
| KerbalDisplayAudio | 1.0.0 | Audio library (included as dependency; audio not used on this panel) |
| RA8875 (PaulStoffregen) | 0.7.11 | Display driver — do not upgrade without testing; text mode API changed in later versions |
| KerbalSimpit | 2.4.0 | KSP telemetry plugin interface (Phase 2) |

### KerbalSimpit Plugin Settings (Phase 2)

Location: `KSP/GameData/KerbalSimpit/PluginData/Settings.cfg`

```
PortName    = COM5       # SerialUSB1 — the second USB COM port (Teensy dual serial)
BaudRate    = 115200
RefreshRate = 125
Verbose     = True
```

---

## Screens

The panel displays ten screens navigated by the right-hand sidebar. The sidebar highlights the active screen button. Screen order follows mission phase progression.

| # | ID | Screen | Title bar toggle |
|---|----|--------|-----------------|
| 0 | LNCH | Launch / Circularization | Tap to toggle phase (auto or manual override) |
| 1 | APSI | Apsides | — |
| 2 | ORB | Orbit | SOI name shown dynamically |
| 3 | ATT | Attitude | — |
| 4 | MNVR | Maneuver | — |
| 5 | RNDZ | Rendezvous | NO TARGET fullscreen when no target |
| 6 | DOCK | Docking | NO TARGET fullscreen when no target |
| 7 | VEH | Vehicle Info | — |
| 8 | LNDG | Landing | Tap to toggle Powered Descent / Re-entry |
| 9 | ACFT | Aircraft | — |

### Screen Descriptions

**LNCH — Launch / Circularization**
Two phases sharing the same screen. Ascent phase shows Alt.SL, V.Srf, V.Vrt (STATE), ApA and T+Ap (TRAJ), and Throttle, T.Burn, ΔV.Stg (PROP). Circularization phase shows Alt.SL and V.Orb (STATE), ApA, PeA and T+Ap (TRAJ), and Throttle, T.Burn, ΔV.Stg (PROP). The sketch auto-switches phases based on altitude relative to body radius with hysteresis; tapping the title bar sets a manual override (shown as a red indicator dot) and toggles the phase directly.

**APSI — Apsides**
Three sections: STATE (Alt.SL, V.Orb, SOI), AP (ApA, T+Ap), PE (PeA, T+Pe), and PR (Period). Apoapsis and periapsis are coloured by body-relative safe altitude (`warnAlt`).

**ORB — Orbit**
Five sections: SHAPE (Ecc, SMA), APSE (ApA, PeA), PLANE (Inc|LAN, Arg.Pe), AN (T.Anom|M.Anom), PR (Period). Title bar shows current SOI name. Apoapsis and periapsis coloured by `warnAlt`.

**ATT — Attitude**
Three sections: CRAFT (Hdg, Pitch, Roll, SAS), ORB V or SRF V (velocity vector Hdg and Pitch), ERR (heading error, pitch error). Velocity reference switches between orbital and surface based on altitude. SAS mode coloured by navball convention. Error rows coloured by `ATT_ERR_WARN/ALARM_DEG`.

**MNVR — Maneuver**
Four sections: STATE (Alt.SL, V.Orb), TIME (T+Ign, T+Mnvr), BURN (ΔV.Mnvr, T.Burn, ΔV.Tot), HD (M.Hdg|M.Pitch). T+Ign is the engine-start cue (T+Mnvr minus half burn duration). ΔV.Tot coloured red if less than ΔV.Mnvr (burn cannot complete), yellow if within `MNVR_DV_MARGIN` factor.

**RNDZ — Rendezvous**
Four sections: STATE (Alt.SL, V.Orb), TGT (Dist, V.Tgt), INT1 (Time, Dist at intercept 1), INT2 (Time, Dist at intercept 2). Shows "NO TARGET" fullscreen when no target is set. Intercept distance coloured green if good (<1 km), yellow if workable, red if poor (correction needed).

**DOCK — Docking**
Four sections: APCH (Alt.SL, Dist, V.Tgt, V.Drft), DR (Drft.H|Drft.V), BRG (Brg|Elev, Vel.Brg|Vel.Elv), CT (RCS|SAS). Shows "NO TARGET" fullscreen when no target is set. SAS mode uses docking-specific colour conventions; RCS OFF is white-on-red.

**VEH — Vehicle Info**
Three sections: INFO (Name, Type, Status), CREW (Control, Signal, Crew|Cap), PROP (ΔV.Stg, ΔV.Tot). Vessel type colour-coded by category. Control level coloured red for no control, orange/yellow for limited. Signal coloured by `VEH_SIGNAL_WARN_PCT`.

**LNDG — Landing (Powered Descent / Re-entry)**
Toggled by tapping the title bar. Both modes share STATE (T.Grnd, Alt.Rdr, V.Vrt, Fwd|Lat or V.Hrz, V.Srf).

*Powered Descent:* adds ST (ΔV:) and VEH (Throttle, Gear|Brakes). Fwd and Lat show heading-relative horizontal velocity components for correction guidance.

*Re-entry:* adds AT (Mach|G) and VEH (Drogue|Main, Gear|Brakes). Parachute slots coloured by dynamic pressure safety limits. Gear UP in re-entry shows dark grey (not an alarm).

T.Grnd and V.Vrt use gear-aware colouring: gear UP uses time-based alarm thresholds matching `CW_GROUND_PROX`; gear DOWN uses speed-based thresholds capped at yellow (never red unless V.Vrt exceeds `LNDG_VVRT_ALARM_MS`, indicating structural damage speed).

**ACFT — Aircraft**
Four sections: STATE (Rdr Alt, V.Srf, IAS, V.Vrt), AT (Hdg|Pitch|Roll), AERO (AoA|Slip, Mach|G), CT (Gear|Brakes). IAS coloured by `STALL_SPEED_MS` (disabled by default). Gear coloured context-dependently: UP is dark grey in flight, alarm on ground; DOWN shows yellow above `GEAR_MAX_SPEED_MS`. Brakes coloured by ground state.

---

## Configuration

All tunables are in `AAA_Config.ino`.

### Operating Mode

| Constant | Default | Description |
|----------|---------|-------------|
| `debugMode` | `true` | Enables Serial debug output (touch coordinates, screen transitions) |
| `demoMode` | `true` | `true` = sine-wave demo values, no KSP connection. Set `false` when Simpit is integrated (Phase 2). |
| `DISPLAY_ROTATION` | `0` | `0` = normal (connector at bottom), `2` = 180° (inverted mounting) |

### Parachute Action Group Assignments (LNDG Re-entry)

| Constant | Default | Description |
|----------|---------|-------------|
| `DROGUE_DEPLOY_CAG` | `1` | CAG number that deploys the drogue chute |
| `DROGUE_CUT_CAG` | `2` | CAG number that cuts the drogue chute |
| `MAIN_DEPLOY_CAG` | `3` | CAG number that deploys the main chute |
| `MAIN_CUT_CAG` | `4` | CAG number that cuts the main chute |

Set to `0` to disable that indicator (will always show STOWED).

### Alarm Thresholds — Aircraft (ACFT)

| Constant | Default | Description |
|----------|---------|-------------|
| `STALL_SPEED_MS` | `0.0` | IAS stall speed (m/s). Set `0` to disable. Yellow below this, white-on-red below half. |
| `GEAR_MAX_SPEED_MS` | `160.0` | Max safe gear-down speed (m/s). Gear DOWN above this → yellow. |
| `ROLL_WARN_DEG` | `60.0` | Roll warning threshold (degrees) |
| `ROLL_ALARM_DEG` | `90.0` | Roll alarm threshold — inverted / structural risk |
| `AOA_WARN_DEG` | `10.0` | AoA warning threshold |
| `AOA_ALARM_DEG` | `20.0` | AoA alarm threshold — beyond stall |
| `SLIP_WARN_DEG` | `5.0` | Sideslip warning threshold |
| `SLIP_ALARM_DEG` | `15.0` | Sideslip alarm threshold |
| `G_WARN_POS` | `4.0` | Positive G warning |
| `G_ALARM_POS` | `9.0` | Positive G alarm (structural / physiological limit) |
| `G_WARN_NEG` | `-2.0` | Negative G warning |
| `G_ALARM_NEG` | `-5.0` | Negative G alarm |

### Alarm Thresholds — Landing (LNDG)

| Constant | Default | Description |
|----------|---------|-------------|
| `LNDG_TGRND_ALARM_S` | `10.0` | T.Grnd alarm (s) — gear UP only; matches `CW_GROUND_PROX` |
| `LNDG_TGRND_WARN_S` | `30.0` | T.Grnd warning (s) — gear UP only |
| `LNDG_VVRT_ALARM_MS` | `-8.0` | V.Vrt alarm (m/s) — crash landing speed regardless of gear |
| `LNDG_VVRT_WARN_MS` | `-5.0` | V.Vrt warning (m/s) |
| `ALT_RDR_ALARM_M` | `50.0` | Radar altitude alarm (m) — LNDG and ACFT |
| `ALT_RDR_WARN_M` | `500.0` | Radar altitude warning (m) — LNDG and ACFT |
| `LNDG_DROGUE_SAFE_Q` | `1500.0` | Drogue chute: max safe deploy dynamic pressure (Pa) |
| `LNDG_DROGUE_UNSAFE_Q` | `5000.0` | Drogue chute: unsafe deploy dynamic pressure (Pa) |
| `LNDG_MAIN_SAFE_Q` | `500.0` | Main chute: max safe deploy dynamic pressure (Pa) |
| `LNDG_MAIN_UNSAFE_Q` | `3000.0` | Main chute: unsafe deploy dynamic pressure (Pa) |

Fwd/Lat horizontal velocity thresholds tighten contextually as T.Grnd decreases (four bands: loose >60 s, mid >30 s, tight >10 s, final <10 s). See `LNDG_HVEL_*` constants.

### Alarm Thresholds — Rendezvous (RNDZ)

| Constant | Default | Description |
|----------|---------|-------------|
| `RNDZ_DIST_ALARM_M` | `500.0` | Distance alarm (m) |
| `RNDZ_DIST_WARN_M` | `5000.0` | Distance warning (m) |
| `RNDZ_VCLOSURE_WARN_MS` | `5.0` | Closure rate warning (m/s) |
| `RNDZ_VCLOSURE_ALARM_MS` | `10.0` | Closure rate alarm (m/s) — within `RNDZ_VCLOSURE_ALARM_DIST_M` |
| `RNDZ_VCLOSURE_ALARM_DIST_M` | `2000.0` | Distance within which fast closure alarms |
| `RNDZ_INT_WARN_S` | `120.0` | Intercept time warning (s) |
| `RNDZ_INTDIST_GOOD_M` | `1000.0` | Intercept distance: green if below (good) |
| `RNDZ_INTDIST_WARN_M` | `5000.0` | Intercept distance: yellow if below (workable), red if above (poor) |

### Alarm Thresholds — Docking (DOCK)

| Constant | Default | Description |
|----------|---------|-------------|
| `DOCK_DIST_ALARM_M` | `50.0` | Distance alarm (m) |
| `DOCK_DIST_WARN_M` | `200.0` | Distance warning (m) |
| `DOCK_VCLOSURE_ALARM_MS` | `2.0` | Closure rate alarm (m/s) — within `DOCK_VCLOSURE_ALARM_DIST_M` |
| `DOCK_VCLOSURE_ALARM_DIST_M` | `100.0` | Distance within which fast closure alarms |
| `DOCK_DRIFT_WARN_MS` | `0.1` | Lateral drift warning (m/s) |
| `DOCK_DRIFT_ALARM_MS` | `0.5` | Lateral drift alarm (m/s) |
| `DOCK_BRG_WARN_DEG` | `10.0` | Bearing/elevation warning (degrees) |
| `DOCK_BRG_ALARM_DEG` | `20.0` | Bearing/elevation alarm — target off bore |

### Alarm Thresholds — Other

| Constant | Default | Description |
|----------|---------|-------------|
| `DV_STG_ALARM_MS` | `150.0` | Stage ΔV alarm (m/s) — LNCH, LNDG, VEH |
| `DV_STG_WARN_MS` | `300.0` | Stage ΔV warning (m/s) |
| `DV_TOT_WARN_MS` | `500.0` | Total ΔV warning (m/s) — VEH |
| `ORB_ECC_WARN` | `0.9` | Eccentricity warning — highly elliptical |
| `ORB_ECC_ALARM` | `1.0` | Eccentricity alarm — escape trajectory |
| `APSI_TIME_WARN_S` | `60.0` | Time to apsis warning (s) |
| `MNVR_TIGN_WARN_S` | `60.0` | Time to ignition warning (s) |
| `MNVR_DV_MARGIN` | `1.1` | ΔV.Tot margin factor — yellow if total is within this multiple of maneuver ΔV |
| `LNCH_TOAPO_WARN_S` | `30.0` | Time to apoapsis warning during ascent (s) |
| `LNCH_BURNTIME_ALARM_S` | `60.0` | Stage burn time alarm (s) |
| `LNCH_BURNTIME_WARN_S` | `120.0` | Stage burn time warning (s) |
| `VEH_SIGNAL_WARN_PCT` | `50.0` | CommNet signal warning (%) |
| `ATT_ERR_WARN_DEG` | `5.0` | Attitude error warning (degrees) |
| `ATT_ERR_ALARM_DEG` | `15.0` | Attitude error alarm (degrees) |

---

## AppState

`AppState` is the central telemetry struct defined in `KCMk1_InfoDisp.h`. In Phase 1 (demo mode) it is populated by `Demo.ino`. In Phase 2 (Simpit) it will be populated by `SimpitHandler.ino`. All float fields default to `0.0f`; String fields to `"---"`.

Key field groups:

| Group | Fields | Simpit source (Phase 2) |
|-------|--------|------------------------|
| Altitude & velocity | `altitude`, `radarAlt`, `orbitalVel`, `surfaceVel`, `verticalVel` | `ALTITUDE_MESSAGE`, `VELOCITY_MESSAGE` |
| Apsides & time | `apoapsis`, `periapsis`, `timeToAp`, `timeToPe` | `APSIDES_MESSAGE`, `APSIDESTIME_MESSAGE` |
| Orbital elements | `inclination`, `eccentricity`, `semiMajorAxis`, `orbitalPeriod`, `LAN`, `argOfPe`, `trueAnomaly`, `meanAnomaly` | `ORBIT_MESSAGE` |
| Delta-V & burn | `stageDeltaV`, `totalDeltaV`, `stageBurnTime`, `throttle` | `DELTAV_MESSAGE`, `THROTTLE_CMD_MESSAGE` |
| Maneuver node | `mnvrTime`, `mnvrDeltaV`, `mnvrDuration`, `mnvrHeading`, `mnvrPitch` | `MANEUVER_MESSAGE` |
| Attitude | `heading`, `pitch`, `roll`, `orbVelHeading`, `orbVelPitch`, `srfVelHeading`, `srfVelPitch` | `ROTATION_DATA_MESSAGE` |
| Aircraft | `machNumber`, `IAS`, `gForce`, `airDensity` | `ATMO_CONDITIONS_MESSAGE` |
| Action groups | `gear_on`, `brakes_on`, `rcs_on`, `drogueDeploy`, `drogueCut`, `mainDeploy`, `mainCut` | `ACTIONSTATUS_MESSAGE`, `CAGSTATUS_MESSAGE` |
| Target | `targetAvailable`, `tgtDistance`, `tgtVelocity`, `tgtHeading`, `tgtPitch`, `tgtVelHeading`, `tgtVelPitch` | `TARGETINFO_MESSAGE` |
| Intercepts | `intercept1Dist`, `intercept1Time`, `intercept2Dist`, `intercept2Time` | `INTERSECTS_MESSAGE` |
| Vessel info | `vesselName`, `vesselType`, `ctrlLevel`, `situation`, `isRecoverable`, `gameSOI`, `crewCount`, `crewCapacity`, `commNetSignal`, `inAtmo`, `sasMode` | `FLIGHT_STATUS_MESSAGE`, `VESSEL_NAME_MESSAGE`, `SAS_MODE_INFO_MESSAGE` |

---

## Colour Conventions

| Colour | Meaning |
|--------|---------|
| Dark green | Nominal / safe |
| Yellow | Caution — monitor, may require action |
| White-on-red | Alarm — immediate action required |
| Red-on-black | Informational negative (e.g. negative altitude, past-node time) |
| Dark grey | Inactive / not applicable in current context |

Brakes OFF and Gear UP use context-dependent colouring: dark grey when in flight (informational), white-on-red when on the ground (actionable alarm).

---

## Annunciator Alignment

The alarm thresholds are intentionally consistent with the KCMk1 Annunciator C&W panel:

| Annunciator C&W | InfoDisp equivalent | Match |
|-----------------|--------------------|----|
| `CW_GROUND_PROX` — T.Grnd < 10 s, gear UP | T.Grnd / V.Vrt white-on-red at same condition | ✓ Exact |
| `CW_HIGH_G` — g > 9 or < −5 | G: white-on-red at same values | ✓ Exact |
| `CW_LOW_DV` — stageDV < 150 m/s or burnTime < 60 s | ΔV.Stg and T.Burn white-on-red at same values | ✓ Exact |
| `CW_ALT` — alt < 500 m | Alt.Rdr yellow at 500 m, white-on-red at 50 m (more granular) | ✓ Aligned |
| `CW_BUS_VOLTAGE` / `CW_HIGH_TEMP` | Not shown — covered by Annunciator master alarm and Resource Display | — |

---

## Tab Structure

| File | Description |
|------|-------------|
| `KCMk1_InfoDisp.ino` | `setup()` and `loop()` only |
| `AAA_Config.ino` | All tunable constants — edit this for flight testing |
| `AAA_Globals.ino` | Global state, display object, `AppState`, `switchToScreen()` |
| `AAA_Screens.ino` | Shared screen infrastructure, layout constants, helpers, dispatch switches |
| `Screen_LNCH.ino` | Launch / Circularization screen |
| `Screen_APSI.ino` | Apsides screen |
| `Screen_ORB.ino` | Orbit screen |
| `Screen_ATT.ino` | Attitude screen |
| `Screen_MNVR.ino` | Maneuver screen |
| `Screen_RNDZ.ino` | Rendezvous screen |
| `Screen_DOCK.ino` | Docking screen |
| `Screen_VEH.ino` | Vehicle Info screen |
| `Screen_LNDG.ino` | Landing screen (Powered Descent + Re-entry) |
| `Screen_ACFT.ino` | Aircraft screen |
| `TouchEvents.ino` | Touch debounce, phantom rejection, sidebar and title bar dispatch |
| `Demo.ino` | Demo mode — sine-wave animation of all AppState fields |

**Tab ordering note:** The `AAA_` prefix ensures `AAA_Screens.ino` compiles before all `Screen_*.ino` tabs (Arduino IDE concatenates tabs alphabetically). Do not rename these files.

---

## Development Phases

- **Phase 1 (current):** Display framework — 10 screens, sidebar navigation, demo mode
- **Phase 2 (planned):** KerbalSimpit integration — `SimpitHandler.ino` populates `AppState` from live KSP telemetry. All Simpit channel mappings are documented in `AAA_Config.ino`.
- **Phase 3 (planned):** I2C slave interface — operates under Teensy 4.1 master at a fixed address

---

## Notes

- **`demoMode`** defaults to `true` in Phase 1. Set `false` in `AAA_Config.ino` once Simpit integration is complete (Phase 2).
- **Display rotation** — set `DISPLAY_ROTATION = 2` for inverted bench mounting, `0` for production. Touch coordinate remapping is not required; the GSL1680F reports screen-native coordinates.
- **Parachute state tracking** — the static bools `_drogueDeployed`, `_drogueCut`, `_mainDeployed`, `_mainCut` in `Screen_LNDG.ino` persist across `loop()` calls but will require reset on vessel switch. This is deferred to Phase 2 Simpit integration.
- **`KerbalDisplayAudio`** is included as a library dependency and claims pin 9, but audio output is not implemented on this panel.
- **`KerbalDisplayCommon` v2.0.1** is required. The `PrintState` struct introduced in v2.0.0 is a breaking change from v1.x — do not downgrade.

Licensed under the GNU General Public License v3.0.
Final code written by J. Rostoker for Jeb's Controller Works.
