# KCMk1_Annunciator

**Kerbal Controller Mk1 — Annunciator Panel Sketch** · v3.2.0
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
| Audio | PAM8302A amp + speaker (`tone()`) + DFPlayer Mini | Pin 29 (+ TONE_EN enable pin 30) / Serial2 |
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
| 29 | Master-alarm tone → PAM8302A amp input | OUT | KCMk1_SystemConfig (`KCM_AUDIO_TONE_PIN`) |
| 30 | PAM8302A amp enable (`/SD`, net TONE_EN) — mutes idle hiss between cues | OUT | KCMk1_SystemConfig (`KCM_AUDIO_EN_PIN`) |
| 7 | DFPlayer Mini RX2 (Serial2) | IN | Teensy hardware Serial2 (fixed) |
| 8 | DFPlayer Mini TX2 (Serial2) | OUT | Teensy hardware Serial2 (fixed) |
| 11 | DFPlayer BUSY (net AUDIO_BUSY) — LOW = clip playing | IN | KCMk1_SystemConfig (`KCM_AUDIO_BUSY_PIN`) |
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
| KerbalDisplayCommon | ≥ 3.0.0 | Display primitives (`KCM_TFT`/`KCM_Display`), fonts, BMP loader, touch driver, system utils. Rev-2 (LT7683 / Teensy 4.1) requires ≥ 3.0.0 |
| KerbalDisplayAudio | ≥ 1.3.0 | Non-blocking `tone()` audio state machine + bundled `KCM_DFPlayer` driver (DFPlayer Mini BUSY polling for the GPWS function) |
| TeensyRA8876-8080 (RA8876_t41_p) | latest | RA8876 16-bit 8080 parallel display driver (wwatson4506) — replaces the rev-1 PaulStoffregen RA8875 library |
| TeensyRA8876-GFX-Common | latest | GFX common layer for the RA8876 driver |
| ILI9341_fonts (ILI9341_t3) | latest | Anti-aliased fonts (PaulStoffregen) |
| KerbalSimpit | 2.4.0 | KSP telemetry plugin interface |

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

**GPWS function tunables** live in the `TUNABLES` block at the top of `GPWS.ino` (envelope floors/slopes/ceilings, callout ladders, bank-angle ramp, and the ~1.5 s repeat cadences), kept together there so the whole aviation-faithful envelope definition is reviewable in one place. The GPWS *configuration* (mode / proximity-alarm / rendezvous-radar / threshold) is not a tunable — it originates on the GPWS Input Panel module and is relayed by the master over I2C. See the **GPWS Function** feature section.

**Master alarm mask** — the set of C&W bits that illuminate MASTER ALARM and drive audio. Defined in `AAA_Config.ino` using the `CW_*` constants from `KCMk1_Annunciator.h`. Current mask (9 bits): `GROUND_PROX`, `HIGH_G`, `BUS_VOLTAGE`, `HIGH_TEMP`, `LOW_DV`, `ABORT`, `PE_LOW`, `PROP_LOW`, `LIFE_SUPPORT`.

---

## I2C Protocol

The Annunciator operates as an I2C slave at address **0x10** (`KCM_I2C_ADDR_ANNUNCIATOR`) on the Wire2 bus. The master (Teensy 4.1) drives the bus. Communication is interrupt-driven: the Annunciator asserts pin 0 LOW when new data is ready; the master reads and then sends a command packet in response.

### Outbound Packet — Annunciator → Master

Size: **6 bytes**. Sent in response to `KCM_I2C_BUS.requestFrom(0x10, 6)` (Wire2, pins 24/25) after INT asserts. All 25 C&W bits (`CW_COUNT`) are transmitted across four bytes.

| Byte | Field | Description |
|------|-------|-------------|
| 0 | Sync | `0xAC` (`KCM_I2C_SYNC_ANNUNCIATOR`) — framing validation |
| 1 | Flags | Bit 0: `simpitConnected`  Bit 1: `flightScene`  Bit 2: `masterAlarmOn`  Bits 3–7: reserved (0) |
| 2 | CW b0-7   | `cautionWarningState` bits 7:0 |
| 3 | CW b8-15  | `cautionWarningState` bits 15:8 |
| 4 | CW b16-23 | `cautionWarningState` bits 23:16 |
| 5 | CW b24-31 | `cautionWarningState` bits 31:24 (only bit 24 = `CW_EVA_ACTIVE` used) |

### Inbound Packet — Master → Annunciator

Size: **3 bytes (legacy)**, **6 bytes (rev-2 extended)**, or **9 bytes (rev-3, adds relayed GPWS config)**. Sent by master at any time via `KCM_I2C_BUS.beginTransmission(0x10)` / `KCM_I2C_BUS.write()` / `KCM_I2C_BUS.endTransmission()` (Wire2). `onI2CReceive()` accepts any of the three lengths (`I2C_CMD_SIZE = 3`, `I2C_CMD_SIZE_EXT = 6`, `I2C_CMD_SIZE_GPWS = 9`) and drains any other length, so the master can be upgraded independently. All forms are documented in `Documents/Developer/I2C_Protocol_Specification.md` §15.3.

| Byte | Field | Description |
|------|-------|-------------|
| 0 | `controlByte` | See bit map below |
| 1 | `ctrlModeByte` | `CtrlMode` enum: `0`=Rover, `1`=Plane, `2`=Spacecraft |
| 2 | `ctrlGrpByte` | Active control group, 1-based (1–10) |
| 3 | `modeFlags` low | `state.modeFlags` bits 7:0 (`MF_*` — drives the whole mode/status grid) — *rev-2+* |
| 4 | `modeFlags` high | `state.modeFlags` bits 11:8 — *rev-2+* |
| 5 | `capValue` | "Cap" readout (`state.capValue`) — *rev-2+* |
| 6 | `gpwsConfig` | GPWS config — bits 1:0 = mode (`0`=OFF, `1`=ACTIVE, `2`=PROX), bit 2 = proxAlarm, bit 3 = rdvRadar — *rev-3* |
| 7 | `gpwsThreshold` high | GPWS threshold bug, int16 metres, big-endian — altitude decision height, or target range in rendezvous mode — *rev-3* |
| 8 | `gpwsThreshold` low | — *rev-3* |

Bytes 3–5 are applied only when a 6-byte (or longer) command is received; bytes 6–8 only when a 9-byte command is received. A shorter command leaves the corresponding fields unchanged. The GPWS config byte uses the **same bit layout the GPWS Input Panel module reports** in its own packet (state byte), so the master relays it unchanged; see the GPWS Function feature section.

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
- **Inbound:** add a new accepted length (as `I2C_CMD_SIZE_GPWS = 9` did), grow `i2cCmdBuf`/`I2C_CMD_SIZE_MAX`, accept it in `onI2CReceive()`, and add fields to `processI2CCommand()` in `I2CSlave.ino`
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

### GPWS Function (voice callouts)

`GPWS.ino` implements an aviation-**faithful** **Ground Proximity Warning System** that runs entirely on the Annunciator's Teensy 4.1. It watches the Simpit telemetry in `state` (surface altitude ≈ radio altitude, vertical speed, surface speed, gear, situation, roll, throttle, and target range) and speaks callouts through the **DFPlayer Mini** on the 7" board (Serial2). It is **independent of the `tone()` master-alarm** state machine — all GPWS audio is on the DFPlayer path and can sound alongside the C&W tone. All tunables (envelope boundaries, ladders, cadences) live in the `TUNABLES` block at the top of `GPWS.ino`.

**Two profiles, selected by vessel type.** `type_Plane` runs the full **aircraft EGPWS suite** (Modes 1–6 + extras) documented in the rest of this section; **any other vessel type** (rocket, lander, probe, relay, …) runs the **lander / rocket profile** (`gpwsUpdateLander()`) — a vertical-landing aid described under *Lander / rocket profile* below. The panel mode and threshold bug apply the same way to both.

**Configuration source.** Four parameters **originate on the GPWS Input Panel module** (KC-01-1880, I2C `0x2A`) and are relayed by the master inside the rev-3 inbound I2C command (bytes 6–8). The config byte reuses the module's own reported state-byte layout (pass-through). Real GPWS has no per-mode arming — system on = all modes active — so the panel's controls map to that as three audio profiles:

| Panel state | Audio profile |
|---|---|
| **OFF** | Silent |
| **GREEN** (ACTIVE) | **Everything** — all warning modes (1/2/3/4/6-bank) **+** altitude callouts + RETARD + the threshold-bug outcome (see below) |
| **AMBER + proxAlarm** | Altitude callouts + RETARD + the threshold-bug outcome **only** (no warnings) |
| **AMBER + rdvRadar** | Target-**distance** callouts + bug tone **only**, on `tgtDistance` |

proxAlarm (BTN02) and rdvRadar (BTN03) are **mutually exclusive** amber submodes (module firmware ≥ 2.1.0). The **threshold** (encoder, 0–9999 m) is a settable **bug**. What fires when you descend through it depends on gear, vessel type, and how high the bug is set:

| At the crossing (altitude profiles) | Result |
|---|---|
| **Gear up** (aircraft *or* lander) | **GROUND PROXIMITY** (clip 34) — you're at your set altitude with no gear |
| **Gear down**, aircraft, bug **≥ 300 m** (`DH_SPLIT_M`) | Generic altitude **TONE** — a high bug is just a level marker |
| **Gear down**, aircraft, bug **< 300 m** | Real **decision height**: **APPROACHING MINIMUMS** (100 ft above) then **MINIMUMS** (at the bug) — no tone |
| **Gear down**, lander (any bug) | **TONE** — a lander's bug is always a landing cue (no minimums, no 300 m split) |

In **rdvRadar** the bug is a *range*, so closing through it just plays the **TONE** (gear-agnostic). The number callout at the bug altitude is always suppressed so it never doubles with whatever the bug itself plays.

**Priority ladder** — one clip owns the DFPlayer per frame. Warnings are GREEN-only; callouts/tone/minimums also sound in the amber profiles. The order follows the **EGPWS aural-priority table** (Honeywell MK VI/VIII), highest first:

| # | Callout | Mode | Condition |
|---|---------|------|-----------|
| 1 | STALL | — (extra) | a separate stall-warning system that outranks all of GPWS: airborne in atmosphere, AoA `= pitch − srfVelPitch > STALL_AOA_DEG` (20° = `KCM_AOA_STALL_DEG`, shared with the InfoDisp AoA arc), speed `> STALL_MIN_SPEED_MS` |
| 2 | WHOOP WHOOP PULL UP / TERRAIN, TERRAIN | 1 (inner) / 2 | descent rate `> PULLUP_FLOOR + PULLUP_SLOPE·alt` (< 747 m), **or** terrain closure over the Mode-2 boundary (**2A** gear-up / **2B** gear-down envelope — see below). Mode 2 says TERRAIN once on entry, then PULL UP repeats |
| 3 | TERRAIN, TERRAIN (post-warning) | 2 | after PULL UP exits, TERRAIN keeps repeating (`POSTTERR_GAP_MS`) while terrain clearance is still decreasing (closure `> POST_TERR_FLOOR_MS`) — manual Mode-2 tail behaviour |
| 4 | bug TONE | — | threshold-bug crossing (leads MINIMUMS); see below |
| 5 | MINIMUMS | 6 | decision-height callout at the bug — **gear down only** (armed at the crossing), fired from its own high-priority slot |
| 6 | TOO LOW, TERRAIN | 4A / 4C | **4A**: gear up, `< TERR4_ALT_M` (1000 ft), speed `> TOOLOW_SPEED_MS` (~190 kt). **4C**: gear up and sinking below `MTC_FRAC` (75%) of the post-takeoff peak radio altitude (minimum-terrain-clearance floor) |
| 7 | RETARD | — (extra) | repeating flare callout, above the low-altitude numbers so it isn't buried: descending, gear down, in atmosphere, below `RETARD_ALT_M` (20 ft), **and thrust still commanded** (`throttleCmd > 0`) so it stops at idle |
| 8 | altitude / distance callouts | 6 | the number ladder + APPROACHING MINIMUMS (see below) |
| 9 | TOO LOW, GEAR | 4A | gear up, airborne, `< GEAR_ALT_M` (500 ft) — not descent-gated, but **inhibited for `TOOLOW_ARM_MS` (~15 s) after liftoff** so the initial climb-out (gear legitimately up) is silent |
| 10 | SINK RATE | 1 (outer) | descent rate `> SINK_FLOOR + SINK_SLOPE·alt` (< 747 m) — floored, so a normal flare stays silent |
| 11 | DON'T SINK | 3 | armed on liftoff, disarmed above `M3_CEIL_M`; net altitude loss from the post-takeoff peak `> max(M3_LOSS_MIN, M3_LOSS_FRAC·height)`. Spoken **twice**, then only as the loss deepens by `M3_WORSEN_FRAC` (20%) |
| 12 | BANK ANGLE | 6 | in atmosphere, `\|roll\|` beyond the altitude-ramped limit (see envelope below) |
| 13 | V1 | — (extra) | takeoff roll (on ground), speed `≥ V1_SPEED_MS`; spoken once, re-arms when stationary |
| 14 | ROTATE | — (extra) | takeoff roll (on ground), speed `≥ VR_SPEED_MS`; spoken once |
| 15 | GEAR UP | — (advisory) | retract-gear reminder: airborne, gear still down, climbing (`vel_vert > 0`), `GEARUP_DELAY_MS` (~6 s) after liftoff; spoken once per takeoff |

**The bug TONE, MINIMUMS, RETARD** and the number ladders sit *inside* the priority ladder above (slots 4/5/7/8) — the altitude ladder adds APPROACHING MINIMUMS / MINIMUMS (altitude profiles), the distance ladder replaces it (distance profile).

- **Altitude ladder** — real radio-altitude **feet** values (Airbus-dense near the ground), spoken once per crossing, compared against `alt_surf` in metres: **2500, 1000, 500, 400, 300, 200, 100, 90, 80, 70, 60, 50, 40, 30, 20, 10, 5 ft**.
- **APPROACHING MINIMUMS** at `threshold + APPR_MIN_MARGIN_M` (~100 ft above the bug), **MINIMUMS** at the bug itself (with the bug tone) — **gear down only**. With gear up neither the number at the decision height is masked nor MINIMUMS spoken.
- **Distance ladder** — metres of target range, spoken once per crossing (rdvRadar profile, target present): **500, 200, 100, 50, 40, 30, 20, 10, 5 m**. Reuses the number clips.

**Extras (`STALL`, `V1`, `ROTATE`, `RETARD`, `GEAR UP`)** aren't real GPWS modes. STALL's AoA threshold is **cross-panel aligned** with the InfoDisp aircraft AoA arc (`KCM_AOA_STALL_DEG` = 20°, the "beyond stall AoA" red tier) in `KCMk1_SystemConfig.h`. V1/ROTATE have no per-craft Simpit source, so they fire off fixed speeds in the `TUNABLES` block — defaults `V1_SPEED_MS = 55`, `VR_SPEED_MS = 65` m/s (KSP planes typically rotate ~50 m/s, ranging ~40–110 m/s by design; **set these to the plane you fly**). **GEAR UP** is a retract-gear reminder (borrowed from the KSP_GPWS mod) that fires once after a positive post-takeoff climb is established.

**Envelopes** are floored piecewise-linear descent-rate / closure-rate boundaries approximating the Honeywell MK VI/VIII envelopes (representative values — exact boundaries vary by model). The **floor** on Mode 1 is what keeps PULL UP/SINK RATE silent on a normal landing. Mode 2 closure is derived from the **smoothed rate of change of `alt_surf`** (which captures terrain rising under the vessel — KSP has no forward-looking terrain), and uses one of two envelopes: **2A** (gear up — ceiling `M2A_CEIL_M` ≈ 2200 ft, more sensitive) or the desensitised landing-config **2B** (gear down — ceiling `M2B_CEIL_M` ≈ 789 ft, higher closure floor), so a normal gear-down approach over rising terrain doesn't nuisance-trip. Gear state is the 2B proxy (real EGPWS also keys on flaps, which KSP doesn't expose). The **BANK ANGLE** limit is a three-segment ramp matching the turbofan envelope: ±`BANK_LO_DEG` (10°) at/below `BANK_RAMP_LO_M` (30 ft), rising to ±`BANK_MID_DEG` (40°) by `BANK_RAMP_MID_M` (150 ft), then to ±`BANK_HI_DEG` (55°) by `BANK_RAMP_HI_M` (2450 ft) and held above.

**Non-blocking & deferral.** `gpwsUpdate()` never calls `delay()`; recurring warnings re-arm from `millis()` cadences and the DFPlayer **BUSY** line. Warnings preempt; callouts start only when idle and **defer rather than drop** — the ladder tracker (`_prevAlt`/`_prevDist`) is held while suppressed, so a deferred callout announces the vessel's *current* altitude/range, and is bumped up while climbing (or the range opening) so a non-descending suppressor can't strand it. State resets on scene exit / vessel switch (`gpwsReset()`).

**Coverage vs. a real GPWS** (mirrored in the `GPWS.ino` header):

| Real GPWS mode | Status | Notes |
|----------------|--------|-------|
| **Mode 1** — Excessive descent rate | ✅ Implemented | Floored SINK RATE (outer) + PULL UP (inner) descent-rate-vs-altitude envelopes, < 2450 ft |
| **Mode 2** — Terrain closure | ✅ Implemented (approx) | Closure from smoothed d(`alt_surf`)/dt; TERRAIN → PULL UP, then post-warning TERRAIN while still closing. **2A/2B split** — gear-down selects the desensitised landing-config (2B) envelope (gear as the flaps proxy) |
| **Mode 3** — Altitude loss after takeoff | ✅ Implemented | DON'T SINK; proportional loss (~10% of height) over the ~1500 ft climbout window. Spoken twice, then only as the loss deepens |
| **Mode 4** — Unsafe terrain clearance | ◐ Partial | 4A TOO LOW GEAR (< 500 ft, inhibited ~15 s after liftoff) + speed-expanded TOO LOW TERRAIN (< 1000 ft); 4C minimum-terrain-clearance floor (75% of the post-takeoff peak, gear up). 4B TOO LOW FLAPS omitted (no flap data) |
| **Mode 5** — Below glideslope | ✗ Not implemented | No ILS in KSP |
| **Mode 6** — Advisory callouts | ✅ Implemented | Feet altitude callouts, MINIMUMS at the decision height, altitude-ramped BANK ANGLE |
| **Mode 7** — Windshear | ✗ Not implemented | No wind data |
| **EGPWS/TAWS** — forward-looking terrain | ✗ Not implemented | No terrain database / look-ahead in KSP |

**Cadence.** Ladder callouts, MINIMUMS, APPROACHING MINIMUMS, V1, ROTATE, GEAR UP and the bug tone fire **once** per crossing/event. PULL UP repeats near-gaplessly (`HARD_GAP_MS`, BUSY-gated); post-warning TERRAIN repeats on `POSTTERR_GAP_MS`. SINK RATE / TOO LOW TERRAIN / TOO LOW GEAR / BANK ANGLE / STALL / RETARD re-annunciate on their `*_GAP_MS` cadence (~1–1.5 s) while in the envelope. **DON'T SINK** is the exception — it is spoken twice, then falls silent until the altitude loss deepens by `M3_WORSEN_FRAC` (per the manual), rather than repeating on a fixed cadence. Doublet/siren phrasing (WHOOP WHOOP PULL UP, TERRAIN×2, DON'T SINK×2, BANK ANGLE×2) is baked into the clip; the firmware provides the between-annunciation repeat.

**Lander / rocket profile.** For any vessel whose type is **not** `type_Plane`, `gpwsUpdate()` hands off to `gpwsUpdateLander()` — a vertical-landing aid rather than the aircraft suite. The panel mode/threshold work the same (GREEN = warnings + callouts + tone; proxAlarm = callouts + tone; rdvRadar = target-distance callouts). Every warning requires **airborne + descending**, so a rover or parked craft stays silent. Callouts, priority high → low:

| Callout | Condition |
|---------|-----------|
| **SINK RATE** | time-to-impact `alt_surf / \|vel_vert\| < LANDER_TIMP_S` (the **same metric as the C&W GROUND PROX lamp**, `KCM_GROUND_PROX_S` = 10 s, so voice and lamp agree) **and** descending faster than `LANDER_SINK_FLOOR_MS` (6 m/s). A gentle, leg-survivable touchdown (stock legs fail ~12 m/s) stays silent. Reuses the SINK RATE clip |
| **HORIZONTAL SPEED** | lateral speed above an altitude-ramped limit `HSPEED_FRAC · (alt + HSPEED_BASE_M)` below `HSPEED_CEIL_M` (400 m) — tip-over / skid risk. **New clip 33** |
| **bug TONE / GROUND PROXIMITY** | threshold-bug crossing: **gear down** → tone; **gear up** → GROUND PROXIMITY (clip 34). No 300 m split or minimums in this profile |
| **RETARD** | thrust still commanded (`throttleCmd > 0`) in the final descent below `LANDER_RETARD_ALT_M` (15 m). Reuses the RETARD clip |
| **Altitude callouts (metres)** | descending through a **metre** ladder — 2500, 1000, 500, 100, 50, 40, 30, 20, 10 m — reusing the number clips. (rdvRadar swaps in the target-distance ladder) |

No terrain-closure, don't-sink, too-low-gear, bank-angle, stall or minimums in this profile — they don't apply to a vertical rocket landing.

**DFPlayer clip index** (folder `/01` on the DFPlayer's own microSD card — *not* the Teensy BMP card). Supply `001.mp3`…`034.mp3`. The number clips (15–31) are shared between the feet altitude ladder and the metre distance ladder:

| Track | Spoken | | Track | Spoken |
|-------|--------|---|-------|--------|
| 1 | "WHOOP WHOOP PULL UP" | | 17 | "FIVE HUNDRED" (500) |
| 2 | "TERRAIN, TERRAIN" | | 18 | "FOUR HUNDRED" (400) |
| 3 | "SINK RATE" | | 19 | "THREE HUNDRED" (300) |
| 4 | "TOO LOW, GEAR" | | 20 | "TWO HUNDRED" (200) |
| 5 | "TOO LOW, TERRAIN" | | 21 | "ONE HUNDRED" (100) |
| 6 | "DON'T SINK, DON'T SINK" | | 22 | "NINETY" (90) |
| 7 | "BANK ANGLE, BANK ANGLE" | | 23 | "EIGHTY" (80) |
| 8 | "STALL" | | 24 | "SEVENTY" (70) |
| 9 | "MINIMUMS" | | 25 | "SIXTY" (60) |
| 10 | "APPROACHING MINIMUMS" | | 26 | "FIFTY" (50) |
| 11 | "V ONE" | | 27 | "FORTY" (40) |
| 12 | "ROTATE" | | 28 | "THIRTY" (30) |
| 13 | "RETARD" | | 29 | "TWENTY" (20) |
| 14 | bug tone / beep | | 30 | "TEN" (10) |
| 15 | "TWO THOUSAND FIVE HUNDRED" | | 31 | "FIVE" (5) |
| 16 | "ONE THOUSAND" | | 32 | "GEAR UP" / "LANDING GEAR" |
| | | | 33 | "HORIZONTAL SPEED" |
| | | | 34 | "GROUND PROXIMITY" |

Audio is gated by `audioEnabled` and the flight scene.

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
| `GPWS.ino` | Ground Proximity Warning System voice callouts on the DFPlayer (`gpwsSetup`/`gpwsUpdate`/`gpwsSetConfig`/`gpwsReset`) |
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
5. Master detects INT, calls `KCM_I2C_BUS.requestFrom(0x10, 6)` (Wire2), reads the 6-byte status packet
6. Master inspects state, sends a command packet (3-byte legacy or 6-byte extended) with `requestType = 0x2` (PROCEED) — configuration flags (`demoMode`, `debugMode`, `audioOn`, `idle_state`, `ctrlMode`, `ctrlGrp`, and in the extended form `modeFlags`/`capValue`) can be included in the same packet
7. Annunciator receives PROCEED, clears the boot screen, enters `loop()`

**Important:** The master should read the status packet (step 5) before sending PROCEED (step 6). If PROCEED is sent before reading, INT will still be asserted since the `onRequest` handler hasn't fired yet.

---

## Version History

| Version | Notes |
|---------|-------|
| **3.5.0** | Threshold-bug crossing reworked into a gear/type/altitude decision tree. **Gear up** through the bug → new **GROUND PROXIMITY** call (clip **34**), both profiles. **Gear down**: aircraft with the bug **≥ 300 m** (`DH_SPLIT_M`) get a generic **tone**; **< 300 m** it's a real decision height (**APPROACHING MINIMUMS** + **MINIMUMS**, no tone); landers always tone. rdvRadar range bug still tone-only. The number callout at the bug altitude is now always masked so it can't double with the bug's own call. Requires the new `034` clip. |
| **3.4.1** | Lander **SINK RATE** gains a **descent-rate floor** (`LANDER_SINK_FLOOR_MS` = 6 m/s). Below it a touchdown is comfortably survivable (stock LT-05/LT-1/LT-2 legs fail at ~12 m/s), so a gentle powered landing no longer trips SINK RATE inside the time-to-impact window. |
| **3.4.0** | GPWS gains a **vessel-type-selected lander / rocket profile** (`gpwsUpdateLander()`). `type_Plane` keeps the aircraft EGPWS suite; **any other vessel type** (rocket, lander, probe, …) gets a vertical-landing aid: **SINK RATE** (time-to-impact, sharing the C&W GROUND PROX metric `KCM_GROUND_PROX_S` so the voice and lamp agree), **HORIZONTAL SPEED** (tip-over risk, new clip **33**), **RETARD** (thrust-still-commanded final descent), and **metre** altitude callouts — plus the threshold bug tone. Every lander warning requires airborne + descending, so grounded vessels stay silent. The panel mode / threshold apply to both profiles. Requires the new `033` clip on the DFPlayer. |
| **3.3.0** | GPWS refinements informed by a comparison with the KSP_GPWS mod. **Mode 2** gains a **2A/2B envelope split** — gear-down selects a lower-ceiling, desensitised landing-config envelope (gear as the flaps proxy) so normal gear-down approaches over rising terrain don't nuisance-trip. **TOO LOW GEAR** is now **inhibited for ~15 s after liftoff** (`TOOLOW_ARM_MS`), silencing the initial gear-up climb-out. **RETARD** is gated on **commanded throttle** (`throttleCmd > 0`), so it stops when the throttle is pulled to idle in the flare. New **GEAR UP** advisory (clip **32**) — a retract-gear reminder that fires once after a positive post-takeoff climb is established. Requires the new `032` clip on the DFPlayer. |
| **3.2.1** | GPWS aural priority follows the **Honeywell MK VI/VIII aural-priority table**: STALL > PULL UP/TERRAIN > post-warning TERRAIN > bug TONE > MINIMUMS > TOO LOW TERRAIN > RETARD > callouts > TOO LOW GEAR > SINK RATE > DON'T SINK > BANK ANGLE > V1 > ROTATE. Mode 2 adds a **post-warning TERRAIN** tail (repeats after PULL UP exits while terrain clearance keeps decreasing); Mode 3 **DON'T SINK** speaks twice, then again only as the altitude loss deepens; Mode 4 adds the **4C** minimum-terrain-clearance floor (TOO LOW TERRAIN when sinking below 75% of the post-takeoff peak radio altitude, gear up); **MINIMUMS** is gear-gated with its own priority slot (the decision-height number is masked only with gear down); **BANK ANGLE** uses a three-segment altitude ramp (±10° → ±40° → ±55°). |
| **3.2.0** | **GPWS function** added (`GPWS.ino`): an aviation-**faithful** Ground Proximity Warning System on the Annunciator's Teensy 4.1, driving the **DFPlayer Mini**. Mode coverage: Mode 1 floored SINK RATE (outer) + PULL UP (inner) descent-rate envelopes; Mode 2 TERRAIN/PULL UP from smoothed terrain-closure (d`alt_surf`/dt); Mode 3 DON'T SINK (proportional altitude loss after takeoff); Mode 4A TOO LOW GEAR + speed-expanded TOO LOW TERRAIN; Mode 6 **feet** radio-altitude callouts + MINIMUMS + altitude-ramped BANK ANGLE. Modes 5/7, 4B and forward-looking terrain omitted for lack of KSP data. **Control model:** GREEN = all modes + callouts; amber **proxAlarm** = altitude callouts + MINIMUMS only; amber **rdvRadar** = target-**distance** callouts (on `tgtDistance`) — the two amber submodes are mutually exclusive (GPWS Input module firmware ≥ 2.1.0). The encoder **threshold** is a settable bug that plays a tone clip on crossing (altitude or range). Config relayed by the master over I2C (rev-3 9-byte command, `I2C_CMD_SIZE_GPWS`, reusing the module's state-byte layout). Subscribes to `ROTATION_DATA` (roll/pitch) and `TARGETINFO` (range). Non-blocking; warnings preempt, callouts defer; recurring warnings re-annunciate on ~1.5 s cadences. Adds non-GPWS extras (fixed-threshold approximations): **STALL** (AoA), takeoff **V1**/**ROTATE**, flare **RETARD**, and **APPROACHING MINIMUMS**; the altitude ladder is Airbus-dense (2500…5 ft incl. 90/80/70/60). 31 DFPlayer clips (number words shared between the feet and metre ladders). Independent of the `tone()` master-alarm path. Requires **KerbalDisplayAudio ≥ 1.3.0**. |
| **3.1.1** | KC-01-1911 **V2.1 audio hardware**: the `tone()` master-alarm output moved to **pin 29**, and the S8050 buzzer stage was replaced by a **PAM8302A Class-D amplifier + external 8 Ω speaker** with an input volume trim. The amp's active-low shutdown (net **TONE_EN**, **pin 30**) is driven by the audio library to power the amp down between cues, muting Class-D idle hiss. The DFPlayer Mini **BUSY** line is now wired to the Teensy (net **AUDIO_BUSY**, **pin 11**) for optional `isPlaying()` polling. No sketch-logic changes: `KerbalDisplayAudio` self-configures these pins from `KCMk1_SystemConfig` (`KCM_AUDIO_TONE_PIN` / `KCM_AUDIO_EN_PIN` / `KCM_AUDIO_BUSY_PIN`). Requires **KerbalDisplayAudio ≥ 1.3.0**. |
| **3.1.0** | Silenced-alarm re-blare fix: a C&W condition that clears and re-triggers while the master alarm is silenced now re-arms the alarm instead of staying latched silent. Outbound status I2C widened to carry all **25** C&W bits (4 C&W bytes; request packet 4→6 bytes) so the master receives the full panel state, not a truncated subset. Audit batch B cleanup (bug fixes, dead-code removal). Built against KerbalDisplayCommon 3.1.2. |
| **3.0.0** | Hardware rev 2 port. Migrated from Teensy 4.0 / RA8875 SPI 800×480 to Teensy 4.1 / LT7683 (RA8876-compatible) 16-bit 8080 parallel (FlexIO3) 1024×600 IPS TFT (BuyDisplay ER-TFT070A2-6-5633), driven via `KCM_TFT` (`KCM_Display`) over the `wwatson4506/TeensyRA8876-8080` driver. Touch changed from GSL1680F (Wire1) to FT5316 5-point capacitive on a software I2C bus (pins 4/5). SD moved to the Teensy 4.1 on-board microSD over SDIO (`BUILTIN_SDCARD`). Audio: `tone()` buzzer moved to pin 2, DFPlayer Mini added on Serial2 (RX2=7 / TX2=8). Slave I2C bus moved from Wire (18/19) to Wire2 (24/25); INT-to-master on pin 0, shared RST on pin 1. All hardware pins centralised in `KCMk1_SystemConfig.h`. Requires KerbalDisplayCommon ≥ 3.0.0. Screens relaid out to 1024×600. Main bottom zone reworked: the rev-1 panel condition strip and 2×2 flight-condition block were replaced by a 6×2 mode/status grid of 12 `MF_*` tiles driven by `state.modeFlags` (`updateModeGrid`), a single vertical 4-tile regime column under DOCK, and a separate SPCFT control-mode tile (`updateSpcftTile`). Inbound I2C command gained a 6-byte extended form carrying `modeFlags` + `capValue`. |
| **2.1.0** | Complete C&W panel redesign: 25 indicators (5×5), body-aware Pe LOW / Ap LOW / ORBIT STABLE logic using full BodyParams (reentryAlt, lowSpace, soiAlt). Two-tier yellow/red indicators for PE_LOW, PROP_LOW, LIFE_SUPP. Dynamic CHUTE_ENV (off/green/yellow/red). Positive indicators: ORBIT_STABLE, ELEC_GEN. State indicators: SRB_ACTIVE, EVA_ACTIVE. CNTCT situation button driven by LANDED/SPLASH (not VSIT_DOCKED). DOCK vertical text indicator above situation column. Panel condition strip (10 buttons): DEMO/CTRL/DEBUG and SPCFT/PLN/RVR use black background with coloured text. Zone separation via TFT_SILVER gutters. Layout updated: 98×73 C&W buttons, repositioned DOCK/situation/panel/flight-condition zones. SOI screen adds Reentry Alt and SOI Radius rows, removes Escape Velocity, reduces font to 28pt at 36px row height. `standaloneMode` and `standaloneTest` operating modes added. Serial-driven test framework (`TestMode.ino`): 66 logic tests + 57-step display walk-through. KerbalDisplayCommon body table expanded with full BodyParams (gravity, escapeVelocity, synchronousOrbit, reentryAlt, soiAlt, hasSurface, highQThreshold). |
| **2.0.0** | Major rewrite. RA8875 KDC v2 flicker-free rendering (PrintState, printDisp, printValue). Full AppState struct. Body-aware SOI screen with KASA meatball and per-body BMP. I2C slave boot handshake with master Teensy 4.1. |
| **1.1.1** | Touch count filter, I2C constants to KCMk1_SystemConfig.h, cross-panel threshold aliases, boot screen live version string, KerbalDisplayCommon 2.1.0. |
| **1.0.0** | Initial release. 3-screen display (Main / SOI / Standby), basic C&W panel, KerbalSimpit integration, KerbalDisplayAudio, I2C slave. |

---

## Notes

- **ARP mod required** for `CW_BUS_VOLTAGE`. Without ARP, KSP1 never sends `ELECTRIC_MESSAGE`.
- **`audioEnabled`** defaults to `false` — must be enabled in `AAA_Config.ino` or via I2C from the master. It gates both the `tone()` cues and the GPWS voice callouts.
- **GPWS voice** (DFPlayer) is separate from the `tone()` master-alarm path and requires numbered clips on the DFPlayer's own microSD card (see the GPWS Function feature section). Without the card / clips the commands are harmless no-ops. GPWS stays silent until the master relays a non-OFF mode (rev-3 I2C command) and a flight scene is active.
- **`DISPLAY_ROTATION`** — set `2` for inverted bench mounting, `0` for production. Touch coordinate remapping is not needed; the FT5316 reports in screen-native coordinates at rotation 0.
- **Demo mode** drives all `AppState` fields at configurable rates. `ctrlMode` and `ctrlGrp` are not cycled — they are owned by the master and preserved as last set via I2C. Switching demo off at runtime connects Simpit if not already connected, or requests a full channel refresh if it is.
- **String heap usage** — `state.vesselName` and `state.gameSOI` use Arduino `String`. Low risk on Teensy 4.1 (1 MB RAM) but worth noting if porting to a memory-constrained target.

Licensed under the GNU General Public License v3.0.
Final code written by J. Rostoker for Jeb's Controller Works.
