# KCMk1_Annunciator

**Kerbal Controller Mk1 — Annunciator Panel Sketch** · v3.0.0
Teensy 4.1 firmware for the KSP annunciator display module.
Part of the KCMk1 controller system. Operates as an I2C slave under a Teensy 4.1 master.

---

## Overview

The Annunciator is a 1024×600 touchscreen display panel that presents real-time KSP telemetry sourced from KerbalSimpit. It runs on a Teensy 4.1 and receives telemetry over USB serial from a running KSP instance. A Teensy 4.1 master controller coordinates the Annunciator via I2C, configuring its operating mode at boot and receiving status updates as flight conditions change.

The panel provides three screens — Main, SOI, and Standby. Main and SOI are navigated by touch; Standby is entered and left automatically on flight-scene changes (or an I2C idle request from the master). The Main screen is the primary operational view, presenting the Caution & Warning panel, vessel situation indicators, SOI thumbnail, and key telemetry readouts. The SOI screen provides detailed celestial body data. The Standby screen displays a full-screen splash image when the system is idle.

---

## Hardware

The display controller is the **LT7683** (the physical part on the BuyDisplay ER-TFT070A2-6-5633 module); it is register-compatible with the RA8876, so the firmware drives it through the `wwatson4506/TeensyRA8876-8080` FlexIO3 driver (class `RA8876_t41_p`). "RA8876" therefore appears in driver/library/class names throughout, while the hardware part is the LT7683.

| Component | Part | Interface |
|-----------|------|-----------|
| Microcontroller | Teensy 4.1 | — |
| Display | LT7683 (RA8876-compatible) 1024×600 IPS TFT (BuyDisplay ER-TFT070A2-6-5633) | 16-bit 8080 parallel (FlexIO3) |
| Touch controller | FT5316 5-point capacitive | Software I2C (pins 4/5) |
| SD card | Teensy 4.1 on-board microSD | SDIO (`BUILTIN_SDCARD`) |
| Audio | Buzzer (`tone()`) + DFPlayer Mini | Pin 2 / Serial2 |
| KSP telemetry | KerbalSimpit plugin | SerialUSB1 (second USB COM port) |
| I2C slave bus | Master Teensy 4.1 | Wire2 (I2C) |

### Pin Assignments

| Pin | Function | Direction | Assigned by |
|-----|----------|-----------|-------------|
| 34 | Display /CS chip select | OUT | KCMk1_SystemConfig (`KCM_TFT_CS`) |
| 33 | Display RS register/data select | OUT | KCMk1_SystemConfig (`KCM_TFT_RS`) |
| 35 | Display /RESET | OUT | KCMk1_SystemConfig (`KCM_TFT_RESET`) |
| 36 | Display /WR write strobe | OUT | KCMk1_SystemConfig (`KCM_TFT_WR`) — FlexIO3 driver-owned |
| 37 | Display /RD read strobe | OUT | KCMk1_SystemConfig (`KCM_TFT_RD`) — FlexIO3 driver-owned |
| 32 | Display WAIT (busy flow control) | IN | KCMk1_SystemConfig (`KCM_TFT_WAIT`) |
| 31 | Display INT (unused for now) | IN | KCMk1_SystemConfig (`KCM_TFT_INT`) |
| 9 | Backlight enable / PWM | OUT | KCMk1_SystemConfig (`KCM_TFT_BL`) |
| 19, 18, 14, 15, 40, 41, 17, 16, 22, 23, 20, 21, 38, 39, 26, 27 | Display 16-bit data bus DB0..DB15 | — | KCMk1_SystemConfig (`KCM_TFT_DB0`..`DB15`) — FlexIO3 driver-owned |
| 4 | FT5316 SCL (software I2C) | — | KCMk1_SystemConfig (`KCM_CTP_SCL`) |
| 5 | FT5316 SDA (software I2C) | — | KCMk1_SystemConfig (`KCM_CTP_SDA`) |
| 3 | FT5316 /RESET | OUT | KCMk1_SystemConfig (`KCM_CTP_RST`) |
| 6 | FT5316 INT (data ready) | IN | KCMk1_SystemConfig (`KCM_CTP_INT`) |
| 2 | Buzzer tone output (master alarm) | OUT | KCMk1_SystemConfig (`KCM_AUDIO_TONE_PIN`) |
| 7 | DFPlayer Mini RX2 (Serial2) | IN | Teensy hardware Serial2 (fixed) |
| 8 | DFPlayer Mini TX2 (Serial2) | OUT | Teensy hardware Serial2 (fixed) |
| 24 | I2C SCL2 (Wire2 — master bus) | — | KCMk1_SystemConfig (`KCM_I2C_BUS`) |
| 25 | I2C SDA2 (Wire2 — master bus) | — | KCMk1_SystemConfig (`KCM_I2C_BUS`) |
| 0 | I2C interrupt output to master (active-LOW) | OUT | KCMk1_SystemConfig (`KCM_I2C_INT_PIN`) |
| 1 | Shared reset line from master | IN | KCMk1_SystemConfig (`KCM_I2C_RST_PIN`) |

**Serial ports:**
- `Serial` (USB COM port 4) — debug output only
- `SerialUSB1` (USB COM port 5) — KerbalSimpit telemetry traffic

**I2C note:** Wire2 (pins 24/25 = SCL2/SDA2) is the master bus shared with the Teensy 4.1. The FT5316 touch controller runs on a separate software (bit-banged) I2C bus on pins 4/5 — pins 18/19/16/17 are consumed by the display data bus. Pull-ups on the master bus (4.7 kΩ to 3.3 V) should be placed on the master side.

**Pin configuration:** All hardware pins are defined centrally in `KCMk1_SystemConfig.h`, the shared header used by the Annunciator, ResourceDisp, and InfoDisp panels. To remap a pin, edit the `KCM_*` define there rather than overriding per-sketch. The display data bus, /WR, and /RD lines are owned by the FlexIO3 driver (`wwatson4506/TeensyRA8876-8080`); only /CS, RS, and /RESET are passed to it as plain GPIO.

---

## Dependencies

| Library | Version | Notes |
|---------|---------|-------|
| KerbalDisplayCommon | 3.0.0 | Display primitives (`KCM_TFT`/`KCM_Display`), fonts, BMP loader, touch driver, system utils. Rev-2 (RA8876 / Teensy 4.1) requires ≥ 3.0.0 |
| KerbalDisplayAudio | 1.1.0 | Non-blocking audio state machine |
| TeensyRA8876-8080 (RA8876_t41_p) | latest | RA8876 16-bit 8080 parallel display driver (wwatson4506) — replaces the rev-1 PaulStoffregen RA8875 library |
| TeensyRA8876-GFX-Common | latest | GFX common layer for the RA8876 driver |
| ILI9341_fonts (ILI9341_t3) | latest | Anti-aliased fonts (PaulStoffregen) |
| KerbalSimpit | latest | KSP telemetry plugin interface |

### KerbalSimpit Plugin Settings

Location: `KSP/GameData/KerbalSimpit/PluginData/Settings.cfg`

```
PortName    = COM5       # SerialUSB1 — the second USB COM port (Teensy dual serial)
BaudRate    = 115200
RefreshRate = 125
Verbose     = True
```

**Note:** `CW_BUS_VOLTAGE` (EC low warning) requires the **Alternate Resource Panel (ARP)** mod in KSP1. Without ARP, `ELECTRIC_MESSAGE` is never sent and the bus voltage alarm will not fire. It will not false-trigger — the `EC_total > 0` guard prevents that.

---

## Configuration

All tunables are in `AAA_Config.ino`. The three operating mode flags can also be set at runtime via the inbound I2C packet from the master.

| Constant | Default | Description |
|----------|---------|-------------|
| `demoMode` | `false` | `false` = live Simpit (production). `true` = bench testing without KSP (Simpit disabled; state driven internally). Can also be toggled at runtime by the I2C master. |
| `audioEnabled` | `false` | Enables all audio feedback (alarms, chirps, tones). Can also be set via I2C. |
| `debugMode` | `false` | Enables Serial debug output (touch coords, screen transitions, C&W changes, I2C traffic). |
| `standaloneMode` | `false` | Bypasses the I2C master handshake on boot. Use for bench testing without the master controller connected. |
| `standaloneTest` | `false` | Enters serial-driven test mode after boot. Implies `standaloneMode`. Set `demoMode = false` when using this. |
| `DISPLAY_ROTATION` | `0` | `0` = normal (connector at bottom), `2` = 180° (inverted mounting). |
| `tempAlarm` | `90` | Temperature % of limit at which C&W shows red and MASTER ALARM triggers. |
| `CW_ALT_THRESHOLD_M` | `200.0` | Surface altitude (m) below which `CW_ALT` fires. |
| `CW_GEAR_UP_THRESHOLD_M` | `200.0` | Surface altitude (m) below which `CW_GEAR_UP` fires when gear is up. |
| `CW_RCS_LOW_FRAC` | `0.20` | MonoProp fraction below which `CW_RCS_LOW` fires (20%). |
| `CW_PROP_LOW_WARN_FRAC` | `0.20` | Stage propellant fraction below which PROP_LOW shows yellow (20%). |
| `CW_PROP_LOW_ALARM_FRAC` | `0.05` | Stage propellant fraction below which PROP_LOW shows red and MASTER ALARM triggers (5%). |
| `CW_EC_LOW_FRAC` | `0.05` | EC fraction below which `CW_BUS_VOLTAGE` fires (5%). |

The five cross-panel aligned thresholds below are sourced from `KCMk1_SystemConfig.h` and must stay in sync with their InfoDisp equivalents. Edit in `KCMk1_SystemConfig.h` only — the local constants here are aliases.

| Constant | Value | InfoDisp equivalent |
|----------|-------|---------------------|
| `CW_GROUND_PROX_S` | `10.0` s | `LNDG_TGRND_ALARM_S` |
| `CW_HIGH_G_ALARM` | `9.0` g | `G_ALARM_POS` |
| `CW_HIGH_G_WARN` | `−5.0` g | `G_ALARM_NEG` |
| `CW_LOW_DV_MS` | `150.0` m/s | `DV_STG_ALARM_MS` |
| `CW_LOW_BURN_S` | `60.0` s | `LNCH_BURNTIME_ALARM_S` |

`CW_EC_LOW_FRAC` (`0.05`) is panel-specific and is not shared.

**Master alarm mask** — the set of C&W bits that illuminate MASTER ALARM and drive audio. Defined in `AAA_Config.ino` using the `CW_*` constants from `KCMk1_Annunciator.h`. Current mask (9 bits): `GROUND_PROX`, `HIGH_G`, `BUS_VOLTAGE`, `HIGH_TEMP`, `LOW_DV`, `ABORT`, `PE_LOW`, `PROP_LOW`, `LIFE_SUPPORT`.

---

## I2C Protocol

The Annunciator operates as an I2C slave at address **0x10** (`KCM_I2C_ADDR_ANNUNCIATOR`) on the Wire2 bus. The master (Teensy 4.1) drives the bus. Communication is interrupt-driven: the Annunciator asserts pin 0 LOW when new data is ready; the master reads and then sends a command packet in response.

### Outbound Packet — Annunciator → Master

Size: **4 bytes**. Sent in response to `Wire.requestFrom(0x10, 4)` after INT asserts.

| Byte | Field | Description |
|------|-------|-------------|
| 0 | Sync | `0xAC` (`KCM_I2C_SYNC_ANNUNCIATOR`) — framing validation |
| 1 | Flags | Bit 0: `simpitConnected`  Bit 1: `flightScene`  Bit 2: `masterAlarmOn`  Bits 3–7: reserved (0) |
| 2 | CW low | `cautionWarningState` bits 7:0 |
| 3 | CW high | `cautionWarningState` bits 15:8 |

### Inbound Packet — Master → Annunciator

Size: **3 bytes (legacy)** or **6 bytes (rev-2 extended)**. Sent by master at any time via `Wire.beginTransmission(0x10)` / `Wire.write()` / `Wire.endTransmission()`. `onI2CReceive()` accepts either length (`I2C_CMD_SIZE = 3` or `I2C_CMD_SIZE_EXT = 6`) and drains any other length, so the master can be upgraded independently. The extended bytes 3–5 are not yet in the formal protocol spec (see `I2CSlave.ino` TODO).

| Byte | Field | Description |
|------|-------|-------------|
| 0 | `controlByte` | See bit map below |
| 1 | `ctrlModeByte` | `CtrlMode` enum: `0`=Rover, `1`=Plane, `2`=Spacecraft |
| 2 | `ctrlGrpByte` | Active control group, 1-based (1–10) |
| 3 | `modeFlags` low | `state.modeFlags` bits 7:0 (`MF_*` — drives the whole mode/status grid) — *extended only* |
| 4 | `modeFlags` high | `state.modeFlags` bits 11:8 — *extended only* |
| 5 | `capValue` | "Cap" readout (`state.capValue`) — *extended only* |

Bytes 3–5 are applied only when a 6-byte command is received; a 3-byte command leaves `modeFlags` and `capValue` unchanged.

**`controlByte` bit map:**

| Bits | Field | Description |
|------|-------|-------------|
| 7:4 | `requestType` | Command code — see table below |
| 3 | `idle_state` | `1` = switch to Standby when not in a flight scene |
| 2 | `audioOn` | `1` = enable audio feedback |
| 1 | `demoMode` | `1` = enable demo mode (disables Simpit) |
| 0 | `debugMode` | `1` = enable Serial debug output |

**Request type codes (`controlByte` bits 7:4):**

| Code | Name | Action |
|------|------|--------|
| `0x0` | NOP | No operation |
| `0x1` | STATUS | Force immediate status packet — assert INT now |
| `0x2` | PROCEED | Release boot hold — Annunciator clears screen and enters main loop |
| `0x3` | MCU_RESET | Soft reboot the Annunciator (USB disconnect then ARM AIRCR reset) |
| `0x4` | DISPLAY_RESET | Reset display state and force full redraw of current screen |

### Expanding the Protocol

- **Outbound:** increment `I2C_PACKET_SIZE` and add fields to `fillI2CPacketBuffer()` in `I2CSlave.ino`
- **Inbound:** increment `I2C_CMD_SIZE` and add fields to `processI2CCommand()` in `I2CSlave.ino`
- Update the master sketch to match in both cases

---

## Features

### Screens

Three screens are available. Transitions are managed by `switchToScreen()` in `AAA_Globals.ino`; all screen state, dirty tracking, and chrome invalidation flow through this single function.

**Standby** — full-screen splash BMP (`/StandbySplash_1024x600.bmp` from SD). No dynamic content. Displayed when `flightScene` is false and `idle_state` is asserted by the master, or on initial boot before a flight scene is active. Standby is not touch-navigable; the panel leaves it automatically when a flight scene begins (`SCENE_CHANGE`).

**Main** — primary operational view. Contains:
- MASTER ALARM button (top-left, 274×176 px) — illuminates red when any master-alarm-mask C&W bit is set. Touch to silence audio.
- Caution & Warning panel (25 annunciator buttons, 5 rows × 5 columns, 120×80 px each) at x=274
- SOI label (274×48) and body thumbnail/globe (274×176) in the left column below MASTER ALARM — touch to open the SOI screen
- Inner regime column (x=874, 75 px wide): DOCK vertical-text indicator (75×100) at the top, then four regime tiles (75×75) — FLYING LOW, FLYING HIGH, LOW SPACE, HIGH SPACE — with only the single active regime lit
- Outer vessel-situation column (x=949, eight 75×50 tiles, top→bottom): CONTACT, PRE-LAUNCH, FLIGHT, SUB-ORBIT, ORBIT, ESCAPE, LANDED, SPLASH. CONTACT lights whenever LANDED or SPLASH is set
- Mode/status grid (6 columns × 2 rows, twelve 100×40 tiles) driven by `state.modeFlags` (`MF_*` bits reported by the master): DEMO, WARP, AUDIO, THRTL ENA, TRIM, AUTOPILOT / DEBUG, SWITCH ERR, SIMPIT LOST, THRTL PREC, INPUT PREC, ENG ARM
- SPCFT/PLN/RVR control-mode tile (row 3, 212×80) with vessel-type icon — text green when the control mode matches the vessel type, red on mismatch
- Bottom telemetry readouts: vessel name and TimeWarp (left column, 424 wide); STG / Tmax / CREW and COMM / Tskin / CAP (right triple, 200 each); CtrlGrp (212×80)

**SOI** — celestial body detail screen. Left panel: KASA meatball BMP. Centre: body name. Right: body BMP. Lower rows (40pt, 52px each): Min Safe Alt, SOI Radius, Reentry Alt (atmo bodies), High Atmo Alt (atmo bodies), Low Space Alt (atmo bodies), High Space Alt, Condition, Surface Gravity. Touch anywhere to return to Main.

**Standby** — full-screen splash BMP when system is idle.

### Screen Transitions

| Event | Result |
|-------|--------|
| `SCENE_CHANGE` → flight | Switch to Main + request Simpit channel refresh |
| `SCENE_CHANGE` → non-flight | Switch to Standby |
| I2C `idle_state` asserted + not in flight | Switch to Standby immediately |
| Touch on SOI label/globe area (Main, left column below MASTER ALARM) | Switch to SOI |
| Touch anywhere on SOI | Return to Main |
| Vessel switch | Full redraw of current screen + `updateCautionWarningState()` + Simpit channel refresh |
| EVA state change | Full redraw of current screen |

### Caution & Warning

The C&W panel is a 32-bit bitmask (`state.cautionWarningState`) recomputed on every relevant Simpit message and every demo/test frame by `updateCautionWarningState()` in `CautionWarning.ino`. Bits that are set in `masterAlarmMask` also illuminate MASTER ALARM and drive audio.

| Bit | Label | Severity | Condition |
|-----|-------|----------|-----------|
| 0 | LOW ΔV ⚠ | Warning | Stage ΔV < `CW_LOW_DV_MS` or burn time < `CW_LOW_BURN_S` — suppressed during throttle holdoff |
| 1 | HIGH G ⚠ | Warning | G-forces > `CW_HIGH_G_ALARM` or < `CW_HIGH_G_WARN` |
| 2 | HIGH TEMP ⚠ | Warning | `maxTemp` or `skinTemp` > `tempAlarm` threshold |
| 3 | BUS VOLTAGE ⚠ | Warning | EC < `CW_EC_LOW_FRAC` (5%) of total capacity |
| 4 | ABORT ⚠ | Warning | Abort action group active |
| 5 | GROUND PROX ⚠ | Warning | Descending, T.Grnd < `CW_GROUND_PROX_S` |
| 6 | Pe LOW ⚠ | Warning/Yellow | Red: Pe below reentry alt (atmo) or minSafe (airless). Yellow companion: Pe in aerobrake zone |
| 7 | PROP LOW ⚠ | Warning/Yellow | Red: stage prop < 5%. Yellow companion: stage prop < 20% |
| 8 | LIFE SUPP ⚠ | Warning/Yellow | Red: TAC-LS resource critical. Yellow companion: resource caution |
| 9 | O2 PRESENT | Info | Breathable atmosphere present |
| 10 | IMPACT IMM | Caution | T.Impact < `CW_IMPACT_IMM_S` (60 s) |
| 11 | ALT | Caution | Surface altitude < `CW_ALT_THRESHOLD_M` (200 m) |
| 12 | DESCENT | Caution | Descending (vertical velocity < 0) |
| 13 | GEAR UP | Caution | Gear up, surface altitude < `CW_GEAR_UP_THRESHOLD_M` (200 m), descending |
| 14 | ATMO | Caution | Inside atmosphere |
| 15 | RCS LOW | Caution | MonoProp < `CW_RCS_LOW_FRAC` (20%) |
| 16 | PROP RATIO | Caution | LF/OX ratio deviates from nominal 9:11 |
| 17 | COMM LOST | Caution | CommNet signal lost |
| 18 | Ap LOW | Caution | Apoapsis below atmosphere top (or minSafe for airless bodies) |
| 19 | HIGH Q | Caution | Dynamic pressure above `currentBody.highQThreshold` (0 = suppressed) |
| 20 | ORBIT STABLE | Positive | Pe and Ap both above atmosphere, Ap below SOI, situation=ORBIT |
| 21 | ELEC GEN | Positive | EC increasing (charging) |
| 22 | CHUTE ENV | State | Dynamic: green=safe for mains, yellow=drogue only, red=too fast for any chute |
| 23 | SRB ACTIVE | State | Solid fuel decreasing (SRB burning) |
| 24 | EVA ACTIVE | State | Kerbal on EVA |

⚠ = included in `masterAlarmMask` (illuminates MASTER ALARM, drives audio)

**Audio triggers** (when `audioEnabled` is true):
- WARNING bits newly set → master alarm starts
- All WARNING bits cleared → master alarm stops, silence latch reset
- `CW_ALT` or `CW_IMPACT_IMM` newly set → caution tone
- `CW_DESCENT`, `CW_ATMO`, or `CW_GEAR_UP` newly set → caution chirp
- Altitude crossing `ALERT_ALT_THRESHOLD` upward → alert chirp
- Surface velocity crossing `ALERT_VEL_THRESHOLD` upward → alert chirp
- `ORBIT` bit set (entering orbit) → alert chirp
- Apoapsis crossing body's minimum safe altitude upward → alert chirp

### Vessel Situation

The Annunciator assembles its own vessel situation display bitmask from the raw `vesselSituation` field in `FLIGHT_STATUS_MESSAGE`. The DOCKED state is tracked separately via `VESSEL_CHANGE_MESSAGE`.

| Bit | Constant | Source |
|-----|----------|--------|
| 0 | `VSIT_DOCKED` | `VESSEL_CHANGE_MESSAGE` msg[0]==2/3 |
| 1 | `VSIT_PRELAUNCH` | `sit_PreLaunch` |
| 2 | `VSIT_FLIGHT` | `sit_Flying` |
| 3 | `VSIT_SUBORBIT` | `sit_SubOrb` |
| 4 | `VSIT_ORBIT` | `sit_Orbit` |
| 5 | `VSIT_ESCAPE` | `sit_Escaping` |
| 6 | `VSIT_SPLASH` | `sit_Splashed` |
| 7 | `VSIT_LANDED` | `sit_Landed` |

Bit 0 (DOCKED) is set/cleared by `VESSEL_CHANGE_MESSAGE`; all other bits are assembled from the raw `FLIGHT_STATUS` situation in `SimpitHandler.ino`. On the outer situation column, LANDED is shown above SPLASH (display order `sitRowToArr`), and both drive the CONTACT tile. Named constants are defined in `KCMk1_Annunciator.h`.

---

## Tab Structure

| File | Description |
|------|-------------|
| `KCMk1_Annunciator.ino` | `setup()` and `loop()` only |
| `AAA_Config.ino` | All tunable constants and operating mode flags (including `standaloneMode`, `standaloneTest`) |
| `AAA_Globals.ino` | Global state, `AppState`, `switchToScreen()`, `invalidateAllState()`, `resetDisplays()` |
| `ScreenMain.ino` | Main screen layout constants, chrome, C&W panel, situation column, regime column, mode/status grid, SPCFT tile, update pass |
| `ScreenSOI.ino` | SOI screen chrome and update pass |
| `ScreenStandby.ino` | Standby screen — delegates to `drawStandbySplash()` |
| `CautionWarning.ino` | C&W state machine: `updateCautionWarningState()` |
| `Audio.ino` | Master alarm condition tracking and audio trigger logic |
| `TouchEvents.ino` | Touch debounce and gesture dispatch |
| `SimpitHandler.ino` | KerbalSimpit message handler and channel registration |
| `I2CSlave.ino` | I2C slave at 0x10 — packet build/fill, command processing, boot handshake |
| `BootScreen.ino` | Terminal-aesthetic BIOS POST boot sequence |
| `Demo.ino` | Demo mode — independent field sweep, calls `updateCautionWarningState()` |
| `TestMode.ino` | Serial-driven test framework: logic tests (automated pass/fail) and display walk-through (58 steps). Activated by `standaloneTest = true`. |

---

## Boot Sequence

The Annunciator follows a deterministic startup handshake with the master before entering the main loop.

1. Hardware init (display, SD, touch, audio, I2C slave)
2. Boot screen renders (terminal-aesthetic BIOS POST; header shows live version string via `snprintf`)
3. Simpit handshake runs (or demo mode initialises) — `simpitConnected` set accordingly
4. Annunciator builds a status packet and **asserts pin 0 LOW** (INT)
5. Master detects INT, calls `Wire.requestFrom(0x10, 4)`, reads the 4-byte status packet
6. Master inspects state, sends a command packet (3-byte legacy or 6-byte extended) with `requestType = 0x2` (PROCEED) — configuration flags (`demoMode`, `debugMode`, `audioOn`, `idle_state`, `ctrlMode`, `ctrlGrp`, and in the extended form `modeFlags`/`capValue`) can be included in the same packet
7. Annunciator receives PROCEED, clears the boot screen, enters `loop()`

**Important:** The master should read the status packet (step 5) before sending PROCEED (step 6). If PROCEED is sent before reading, INT will still be asserted since the `onRequest` handler hasn't fired yet.

---

## Version History

| Version | Notes |
|---------|-------|
| **3.0.0** | Hardware rev 2 port. Migrated from Teensy 4.0 / RA8875 SPI 800×480 to Teensy 4.1 / LT7683 (RA8876-compatible) 16-bit 8080 parallel (FlexIO3) 1024×600 IPS TFT (BuyDisplay ER-TFT070A2-6-5633), driven via `KCM_TFT` (`KCM_Display`) over the `wwatson4506/TeensyRA8876-8080` driver. Touch changed from GSL1680F (Wire1) to FT5316 5-point capacitive on a software I2C bus (pins 4/5). SD moved to the Teensy 4.1 on-board microSD over SDIO (`BUILTIN_SDCARD`). Audio: `tone()` buzzer moved to pin 2, DFPlayer Mini added on Serial2 (RX2=7 / TX2=8). Slave I2C bus moved from Wire (18/19) to Wire2 (24/25); INT-to-master on pin 0, shared RST on pin 1. All hardware pins centralised in `KCMk1_SystemConfig.h`. Requires KerbalDisplayCommon ≥ 3.0.0. Screens relaid out to 1024×600. Main bottom zone reworked: the rev-1 panel condition strip and 2×2 flight-condition block were replaced by a 6×2 mode/status grid of 12 `MF_*` tiles driven by `state.modeFlags` (`updateModeGrid`), a single vertical 4-tile regime column under DOCK, and a separate SPCFT control-mode tile (`updateSpcftTile`). Inbound I2C command gained a 6-byte extended form carrying `modeFlags` + `capValue`. |
| **2.1.0** | Complete C&W panel redesign: 25 indicators (5×5), body-aware Pe LOW / Ap LOW / ORBIT STABLE logic using full BodyParams (reentryAlt, lowSpace, soiAlt). Two-tier yellow/red indicators for PE_LOW, PROP_LOW, LIFE_SUPP. Dynamic CHUTE_ENV (off/green/yellow/red). Positive indicators: ORBIT_STABLE, ELEC_GEN. State indicators: SRB_ACTIVE, EVA_ACTIVE. CNTCT situation button driven by LANDED/SPLASH (not VSIT_DOCKED). DOCK vertical text indicator above situation column. Panel condition strip (10 buttons): DEMO/CTRL/DEBUG and SPCFT/PLN/RVR use black background with coloured text. Zone separation via TFT_SILVER gutters. Layout updated: 98×73 C&W buttons, repositioned DOCK/situation/panel/flight-condition zones. SOI screen adds Reentry Alt and SOI Radius rows, removes Escape Velocity, reduces font to 28pt at 36px row height. `standaloneMode` and `standaloneTest` operating modes added. Serial-driven test framework (`TestMode.ino`): 66 logic tests + 57-step display walk-through. KerbalDisplayCommon body table expanded with full BodyParams (gravity, escapeVelocity, synchronousOrbit, reentryAlt, soiAlt, hasSurface, highQThreshold). |
| **2.0.0** | Major rewrite. RA8875 KDC v2 flicker-free rendering (PrintState, printDisp, printValue). Full AppState struct. Body-aware SOI screen with KASA meatball and per-body BMP. I2C slave boot handshake with master Teensy 4.1. |
| **1.1.1** | Touch count filter, I2C constants to KCMk1_SystemConfig.h, cross-panel threshold aliases, boot screen live version string, KerbalDisplayCommon 2.1.0. |
| **1.0.0** | Initial release. 3-screen display (Main / SOI / Standby), basic C&W panel, KerbalSimpit integration, KerbalDisplayAudio, I2C slave. |

---

## Notes

- **ARP mod required** for `CW_BUS_VOLTAGE`. Without ARP, KSP1 never sends `ELECTRIC_MESSAGE`.
- **`audioEnabled`** defaults to `false` — must be enabled in `AAA_Config.ino` or via I2C from the master.
- **`DISPLAY_ROTATION`** — set `2` for inverted bench mounting, `0` for production. Touch coordinate remapping is not needed; the FT5316 reports in screen-native coordinates at rotation 0.
- **Demo mode** drives all `AppState` fields at configurable rates. `ctrlMode` and `ctrlGrp` are not cycled — they are owned by the master and preserved as last set via I2C. Switching demo off at runtime connects Simpit if not already connected, or requests a full channel refresh if it is.
- **String heap usage** — `state.vesselName` and `state.gameSOI` use Arduino `String`. Low risk on Teensy 4.1 (1 MB RAM) but worth noting if porting to a memory-constrained target.

Licensed under the GNU General Public License v3.0.
Final code written by J. Rostoker for Jeb's Controller Works.
