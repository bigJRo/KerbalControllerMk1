# KCMk1_InfoDisp

**Kerbal Controller Mk1 — Information Display Panel Sketch** · v0.13.3
Teensy 4.0 firmware for the KSP flight information display module.
Part of the KCMk1 controller system. Operates as an I2C slave under a Teensy 4.1 master.

---

## Overview

The Information Display is an 800×480 touchscreen panel that presents real-time KSP flight telemetry sourced from KerbalSimpit. It runs on a Teensy 4.0 and receives telemetry over USB serial from a running KSP instance.

The panel provides ten screens — Launch, Orbit, Spacecraft (PFD), Maneuver, Target, Docking, Landing, Vehicle Info, Aircraft, and Rover — ordered to follow mission phase progression from pre-launch through landing. Navigation is via a right-hand sidebar with one button per screen.

**Context-switching:** The display automatically selects the most appropriate screen when the scene or vessel changes. Planes route to AIRCRAFT, rovers to ROVER, vessels on the pad or landed to LAUNCH (with the pre-launch board), landers in flight to LANDING (powered descent), vessels near a docking target to DOCKING, and all others to ORBIT.

**Colour conventions** are consistent across all screens: dark green = nominal, yellow = caution, white-on-red = alarm, dark grey = inactive/not applicable. Alarm thresholds are aligned with the KCMk1 Annunciator C&W panel.

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
| 9 | Audio PWM (claimed by library, not used) | OUT | KerbalDisplayAudio (`AUDIO_PIN`) |
| 18 | I2C SDA (Wire — master bus) | — | Wire library |
| 19 | I2C SCL (Wire — master bus) | — | Wire library |
| 2 | I2C INT (active-LOW, output to master) | OUT | Sketch (`KCM_I2C_INT_PIN`) |

**Serial ports:**
- `Serial` (USB COM port 1) — debug output when `debugMode = true`
- `SerialUSB1` (USB COM port 2) — KerbalSimpit telemetry

**I2C note:** Wire1 (pins 16/17) is used exclusively for the GSL1680F touch controller. Wire (pins 18/19) is used for the I2C slave interface to the Teensy 4.1 master.

---

## Dependencies

| Library | Version | Notes |
|---------|---------|-------|
| KerbalDisplayCommon | 2.1.0 | Display primitives, fonts, touch driver, system utils |
| KerbalDisplayAudio | 1.0.1 | Direct sketch dependency — audio output not used on this panel |
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
| `DV_STG_ALARM_MS` | `150.0` m/s | `CW_LOW_DV_MS` |
| `LNCH_BURNTIME_ALARM_S` | `60.0` s | `CW_LOW_BURN_S` |

For the full threshold listing (aircraft, landing, docking, orbit, spacecraft thresholds), see `AAA_Config.ino`.

---

## I2C Protocol

The InfoDisp operates as an I2C slave at address **0x12** (`KCM_I2C_ADDR_INFODISP`) on the Wire bus.

### Outbound Packet — InfoDisp → Master

Size: **4 bytes**. Sent in response to `Wire.requestFrom(0x12, 4)` after INT asserts.

| Byte | Field | Description |
|------|-------|-------------|
| 0 | Sync | `0xAE` (`KCM_I2C_SYNC_INFODISP`) — framing validation |
| 1 | Flags | Bit 0: `simpitConnected`  Bit 1: `flightScene`  Bit 2: `demoMode`  Bits 3–7: reserved (0) |
| 2 | `activeScreen` | Current `ScreenType` enum value (0–9) |
| 3 | Reserved | `0x00` |

**Note:** The sync byte is `0xAE`, not `0xAD`. The value `0xAD` is used by the ResourceDisp — using it here would cause framing collisions if the master dispatches based on sync byte. Master sketches written before v0.13.3 should update their InfoDisp sync byte expectation accordingly.

### Inbound Packet — Master → InfoDisp

Size: **2 bytes**. Sent by master at any time.

| Byte | Field | Description |
|------|-------|-------------|
| 0 | `controlByte` | See bit map below |
| 1 | Reserved | `0x00` — available for future use |

**`controlByte` bit map:**

| Bits | Field | Description |
|------|-------|-------------|
| 7:4 | `requestType` | Command code — see table below |
| 3 | `idle_state` | `1` = switch to Standby when not in a flight scene |
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

The panel displays ten screens navigated by the right-hand sidebar. Screen order follows mission phase progression.

| # | Sidebar | Title | Tap-through |
|---|---------|-------|-------------|
| 0 | LNCH | LAUNCH | Title bar: ASCENT / CIRCULARIZATION. Pre-launch board shown automatically on pad. |
| 1 | ORB | ORBIT | Title bar: APSIDES / ADVANCED ELEMENTS |
| 2 | PFD | SPACECRAFT | — |
| 3 | MNVR | MANEUVER | — |
| 4 | TGT | TARGET | NO TARGET SET fullscreen when no target |
| 5 | DOCK | DOCKING | NO TARGET SET / DOCKED fullscreen when applicable |
| 6 | LNDG | LANDING | Title bar: POWERED DESCENT / RE-ENTRY |
| 7 | VEH | VEHICLE INFO | — |
| 8 | ACFT | AIRCRAFT | — |
| 9 | ROVR | ROVER | — |

**LNCH** — *Pre-launch board* (automatic when `sit_PreLaunch`, bypassed for planes and rovers): vessel name, type, SAS, RCS, throttle, EC%, crew count, CommNet signal, ΔV.Tot, and parachute CAG states. Tap content area or launch to advance to ascent. *Ascent:* Alt.SL, V.Srf, V.Vrt, ApA, T+Ap, Throttle, T.Burn, ΔV.Stg. *Circularization:* Alt.SL, V.Orb, ApA, PeA, T+Ap, Throttle, T.Burn, ΔV.Stg. Auto-switches at ~6% body radius with hysteresis.

**ORB** — *Apsides (default):* Alt.SL, V.Orb, ApA, PeA, T+Ap or T+Pe, T+Ign, ΔV.Tot, ΔV.Stg, RCS, SAS. *Advanced Elements:* Ecc, SMA, ApA, PeA, Inc, LAN, True/Mean anomaly, Period. Navigating away resets to Apsides.

**PFD** — Primary Flight Display: full EADI ball with pitch ladder, roll indicator, and fixed aircraft symbol. Right panel: Hdg, Pitch, Roll, SAS, velocity vector heading/pitch, heading/pitch error (nose-to-velocity). Errors coloured only in atmosphere.

**MNVR** — Alt.SL, V.Orb, T+Ign, T+Mnvr, ΔV.Mnvr, T.Burn, ΔV.Tot, burn heading/pitch. All fields show `---` when no node planned.

**TGT** — Alt.SL, V.Orb, Dist, V.Tgt, raw bearing/elevation to target, approach angle errors (velocity-to-target). Dist turns white-on-green below 200 m. NO TARGET SET fullscreen when no target.

**DOCK** — Alt.SL, Dist, V.Tgt, total lateral drift magnitude, horizontal/vertical drift components, velocity-to-target bearing/elevation errors, nose-to-target pointing errors, RCS, SAS. SAS: TARGET = green, OFF = white-on-red, all others = red. DOCKED / NO TARGET SET fullscreen when applicable.

**LNDG** — *Powered descent:* T.Grnd, Alt.Rdr, V.Srf, V.Vrt, Fwd/Lat horizontal drift (roll-corrected, craft heading frame), ΔV.Stg, Throttle/RCS, Gear/SAS. Fwd/Lat thresholds tighten as T.Grnd decreases. *Re-entry:* T.Grnd/T+Atm, Alt.SL/Alt.Rdr, V.Vrt (switches to V.Hrz below 20 km radar), PeA, V.Srf, Mach/G, drogue/main parachute states, Gear/SAS. 6-state phase logic drives row labels. SAS white-on-red above Mach 3 if OFF.

**VEH** — Vessel name, type, situation, control level, CommNet signal, crew/capacity, ΔV.Stg, ΔV.Tot.

**ACFT** — Alt.Rdr, V.Srf, IAS, V.Vrt, Hdg/Pitch/Roll, AoA/Slip, Mach/G, Gear/Brakes/EC%.

**ROVR** — V.Srf, Hdg, bearing and heading error to target, Alt.Rdr, Pitch/Roll, wheel throttle (FWD/NEUT/REV), Gear, Brakes, EC%, SAS. Alt.Rdr colouring inverted (close to ground = green).

### Context Switching

`contextScreen()` selects the screen automatically on vessel or scene change, in priority order:

1. Plane (`type_Plane`) → AIRCRAFT
2. Rover (`type_Rover`) → ROVER
3. On ground (`sit_PreLaunch` or `sit_Landed`) → LAUNCH
4. Lander in flight (`type_Lander`) → LANDING (powered descent)
5. Target within 200 m → DOCKING
6. Recoverable vessel → VEHICLE INFO
7. Everything else → ORBIT

A deferred dock-check fires on the next `TARGETINFO` message after a vessel switch to catch the case where target distance is not yet known at switch time.

### Annunciator Alignment

| Annunciator | InfoDisp equivalent | Match |
|-------------|---------------------|-------|
| `CW_GROUND_PROX` — T.Grnd < 10 s, gear up | T.Grnd / V.Vrt white-on-red at same condition | ✓ Exact |
| `CW_HIGH_G` — g > 9 or < −5 | G: white-on-red at same values | ✓ Exact |
| `CW_LOW_DV` — stage ΔV < 150 m/s | ΔV.Stg white-on-red at same value | ✓ Exact |
| `CW_ALT` — alt < 500 m | Alt.Rdr yellow at 500 m, white-on-red at 50 m | ✓ Aligned |

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
| `Screen_ORB.ino` | Orbit (Apsides default — graphical orbit + inclination diagram) |
| `Screen_OrbAdv.ino` | Orbit Advanced Elements (text-only, tap-through) |
| `Screen_SCFT.ino` | Spacecraft / PFD — full EADI ball (sidebar PFD, screen index 2) |
| `Screen_MNVR.ino` | Maneuver — alignment reticle + numeric panel |
| `Screen_TGT.ino` | Target / Rendezvous — RPOD scope + numeric panel |
| `Screen_DOCK.ino` | Docking — approach reticle + numeric panel |
| `Screen_LNDG.ino` | Landing dispatcher (selects powered descent / re-entry) |
| `Screen_LNDG_Powered.ino` | Landing powered descent (graphical: tape, X-Pointer, ATT, V.Vrt) |
| `Screen_LNDG_Reentry.ino` | Landing re-entry (text-only readout board) |
| `Screen_VEH.ino` | Vehicle Info |
| `Screen_ACFT.ino` | Aircraft — full EADI ball |
| `Screen_ROVR.ino` | Rover — compass, throttle bar, tilt indicators (screen index 9) |
| `TouchEvents.ino` | Touch debounce, sidebar and title bar dispatch |
| `SimpitHandler.ino` | KerbalSimpit message handler and channel registration |
| `I2CSlave.ino` | I2C slave at 0x12 — packet build/fill, command processing, boot handshake |
| `BootScreen.ino` | Randomised KSP-themed boot sequences (B: Mission Log, C: Loading Tips, E: Pre-Flight Checklist) |
| `Demo.ino` | Demo mode — sine-wave `AppState` animation |

**Tab naming note:** The `AAA_` prefix ensures `AAA_Screens.ino` compiles before all `Screen_*.ino` tabs. The `Screen_LNCH_*.ino` and `Screen_LNDG_*.ino` files are mode-specific sub-files dispatched by `Screen_LNCH.ino` and `Screen_LNDG.ino` respectively. Filename `Screen_SCFT.ino` corresponds to the SPACECRAFT screen (sidebar label PFD, screen index 2).

---

## Boot Sequence

1. Hardware init (display, SD, touch, I2C slave)
2. Boot screen renders (one of three KSP-themed sequences chosen at random; header shows live version string via `snprintf`)
3. Simpit connects (or demo mode initialises)
4. InfoDisp builds a status packet and **asserts pin 2 LOW** (INT)
5. Master reads the 4-byte status packet
6. Master sends a 2-byte command packet with `requestType = 0x2` (PROCEED)
7. InfoDisp receives PROCEED, enters `loop()`

The boot screen sequences are seeded from the ARM cycle counter for genuine boot-to-boot variation. Three themes are available, each drawing from a pool of 15 items: **B** — Jeb's Mission Log, **C** — KSP Loading Screen Tips, **E** — Gene's Pre-Flight Checklist.

---

## Version History

| Version | Notes |
|---------|-------|
| **0.13.3** | Phase 3 complete: I2C slave interface and boot handshake. I2C sync byte corrected from `0xAD` to `0xAE` (collision with ResourceDisp). I2C constants consolidated to `KCMk1_SystemConfig.h`. `idleState` change now immediately calls `drawStandbyScreen()`. Demo→live I2C transition now calls `initSimpit()`. Loop order corrected: touch processed before Simpit (matches Annunciator/ResourceDisp). `setKDCDebugMode()` moved to immediately after `SerialUSB1.begin()`. `simpit` object moved to `AAA_Globals.ino`. `switchToScreen()` now records `lastScreenSwitch` timestamp. `_lndgReentryMode`, `_orbAdvancedMode`, `_prevShowAp`, `_attPrevOrbMode` now reset on vessel switch. `stepDemoState()` now returns immediately if `!demoMode`. `stgWarn` float cast added in `Screen_VEH`. Touch count filter changed to `!= 1`. Boot screen header shows live version string. Phase markers in comments updated. Updated to KerbalDisplayCommon 2.1.0 (thresholdColor float overload, formatTime int64_t, drawValue split-column overload, drawStandbySplash, fmtTime removed — call sites now call formatTime() directly). |
| **0.13.2** | Phase 2 complete: KerbalSimpit integration verified for all 10 screens. Hover screen name corrected (TARGET/TGT). Parachute state machine (STOWED→ARMED→OPEN) implemented. Re-entry 6-state phase logic. DOCK drift decomposition. ATT heading/pitch error colouring gated on atmosphere. Maneuver screen `---` suppression when no node planned. |
| **0.13.0** | Phase 1 complete: display framework with all 10 screens, sidebar navigation, demo mode. |

---

## Notes

- **`debugMode`** defaults to `false`. Set `true` only during development.
- **`demoMode`** defaults to `false` (live Simpit). Set `true` for bench testing without KSP.
- **Display rotation** — `DISPLAY_ROTATION = 2` for inverted mounting, `0` for production.
- **KerbalDisplayAudio** is a required dependency but audio output is not used on this panel (pin 9 is claimed by the library via `setupAudio()`).
- **KerbalDisplayCommon v2.1.0** is required. Do not downgrade — `PrintState`, `thresholdColor` float overload, and `drawStandbySplash` are all used by this sketch.
- **`INTERSECTS_MESSAGE`** — intercept data not available in KSP1; TARGET screen INT rows not implemented.
- **V.Tgt** — always a positive magnitude in KSP1. Signed closure velocity is not available.
- **EC%** on the pre-launch board and Rover screen may require the Alternate Resource Panel (ARP) mod in KSP1.

Licensed under the GNU General Public License v3.0.
Final code written by J. Rostoker for Jeb's Controller Works.
