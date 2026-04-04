# KCMk1_Annunciator

**Kerbal Controller Mk1 — Annunciator Panel Sketch** · v1.1.1
Teensy 4.0 firmware for the KSP annunciator display module.
Part of the KCMk1 controller system. Operates as an I2C slave under a Teensy 4.1 master.

---

## Overview

The Annunciator is an 800×480 touchscreen display panel that presents real-time KSP telemetry sourced from KerbalSimpit. It runs on a Teensy 4.0 and receives telemetry over USB serial from a running KSP instance. A Teensy 4.1 master controller coordinates the Annunciator via I2C, configuring its operating mode at boot and receiving status updates as flight conditions change.

The panel provides three screens — Main, SOI, and Standby — navigated by touch. The Main screen is the primary operational view, presenting the Caution & Warning panel, vessel situation indicators, SOI thumbnail, and key telemetry readouts. The SOI screen provides detailed celestial body data. The Standby screen displays a full-screen splash image when the system is idle.

---

## Hardware

| Component | Part | Interface |
|-----------|------|-----------|
| Microcontroller | Teensy 4.0 | — |
| Display | RA8875 800×480 TFT | SPI |
| Touch controller | GSL1680F capacitive | Wire1 (I2C) |
| SD card | SD (on RA8875 board) | SPI |
| KSP telemetry | KerbalSimpit plugin | SerialUSB1 (second USB COM port) |
| I2C slave bus | Master Teensy 4.1 | Wire (I2C) |

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
| 9 | Audio PWM output (buzzer/speaker) | OUT | KerbalDisplayAudio (`AUDIO_PIN`) |
| 18 | I2C SDA (Wire — master bus) | — | Sketch / Wire library |
| 19 | I2C SCL (Wire — master bus) | — | Sketch / Wire library |
| 2 | I2C interrupt output to master (active-LOW) | OUT | Sketch (`KCM_I2C_INT_PIN`) |

**Serial ports:**
- `Serial` (USB COM port 4) — debug output only
- `SerialUSB1` (USB COM port 5) — KerbalSimpit telemetry traffic

**I2C note:** Wire (pins 18/19) is the master bus shared with the Teensy 4.1. Wire1 (pins 16/17) is used exclusively for the GSL1680F touch controller. Pull-ups on the master bus (4.7 kΩ to 3.3 V) should be placed on the master side.

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
| KerbalDisplayCommon | 2.1.0 | Display primitives, fonts, BMP loader, touch driver, system utils |
| KerbalDisplayAudio | 1.0.1 | Non-blocking audio state machine |
| RA8875 (PaulStoffregen) | 0.7.11 | Display driver — do not upgrade without testing; text mode API changed in later versions |
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
| `demoMode` | `false` | Bench testing without KSP. Simpit disabled; all state driven internally. Can also be toggled at runtime by the I2C master — switching to demo reinitialises display state; switching to live connects Simpit (or requests a channel refresh if already connected). |
| `audioEnabled` | `false` | Enables all audio feedback (alarms, chirps, tones). Can also be set via I2C. |
| `debugMode` | `false` | Enables Serial debug output (touch coords, screen transitions, C&W changes, I2C traffic). |
| `DISPLAY_ROTATION` | `0` | `0` = normal (connector at bottom), `2` = 180° (inverted mounting). |
| `tempCaution` | `70` | Temperature % of limit at which the C&W panel shows yellow. |
| `tempAlarm` | `90` | Temperature % of limit at which C&W shows red and MASTER ALARM triggers. |
| `commCaution` | `75` | CommNet signal % below which comms indicator shows yellow. |
| `commAlarm` | `25` | CommNet signal % below which comms indicator shows red. |
| `LOW_DV_MECO_HOLDOFF_MS` | `1500` | Ms after MECO clears before LOW_DV warning can fire (prevents false flash on throttle-up). |
| `ALERT_ALT_THRESHOLD` | `3500.0` | Altitude (m ASL) upward-crossing that triggers an alert chirp. |
| `ALERT_VEL_THRESHOLD` | `100.0` | Surface velocity (m/s) upward-crossing that triggers an alert chirp. |
| `CW_ALT_THRESHOLD_M` | `500.0` | Surface altitude (m) below which `CW_ALT` fires. |

The five cross-panel aligned thresholds below are sourced from `KCMk1_SystemConfig.h` and must stay in sync with their InfoDisp equivalents. Edit in `KCMk1_SystemConfig.h` only — the local constants here are aliases.

| Constant | Value | InfoDisp equivalent |
|----------|-------|---------------------|
| `CW_GROUND_PROX_S` | `10.0` s | `LNDG_TGRND_ALARM_S` |
| `CW_HIGH_G_ALARM` | `9.0` g | `G_ALARM_POS` |
| `CW_HIGH_G_WARN` | `−5.0` g | `G_ALARM_NEG` |
| `CW_LOW_DV_MS` | `150.0` m/s | `DV_STG_ALARM_MS` |
| `CW_LOW_BURN_S` | `60.0` s | `LNCH_BURNTIME_ALARM_S` |

`CW_EC_LOW_FRAC` (`0.10`) is panel-specific and is not shared.

**Master alarm mask** — the set of C&W bits that illuminate MASTER ALARM and drive audio. Defined in `AAA_Config.ino` using the `CW_*` constants from `KCMk1_Annunciator.h`. Current mask: `GROUND_PROX`, `HIGH_G`, `BUS_VOLTAGE`, `HIGH_TEMP`, `LOW_DV`.

---

## I2C Protocol

The Annunciator operates as an I2C slave at address **0x10** (`KCM_I2C_ADDR_ANNUNCIATOR`) on the Wire bus. The master (Teensy 4.1) drives the bus. Communication is interrupt-driven: the Annunciator asserts pin 2 LOW when new data is ready; the master reads and then sends a command packet in response.

### Outbound Packet — Annunciator → Master

Size: **4 bytes**. Sent in response to `Wire.requestFrom(0x10, 4)` after INT asserts.

| Byte | Field | Description |
|------|-------|-------------|
| 0 | Sync | `0xAC` (`KCM_I2C_SYNC_ANNUNCIATOR`) — framing validation |
| 1 | Flags | Bit 0: `simpitConnected`  Bit 1: `flightScene`  Bit 2: `masterAlarmOn`  Bits 3–7: reserved (0) |
| 2 | CW low | `cautionWarningState` bits 7:0 |
| 3 | CW high | `cautionWarningState` bits 15:8 |

### Inbound Packet — Master → Annunciator

Size: **3 bytes**. Sent by master at any time via `Wire.beginTransmission(0x10)` / `Wire.write()` / `Wire.endTransmission()`.

| Byte | Field | Description |
|------|-------|-------------|
| 0 | `controlByte` | See bit map below |
| 1 | `ctrlModeByte` | `CtrlMode` enum: `0`=Rover, `1`=Plane, `2`=Spacecraft |
| 2 | `ctrlGrpByte` | Active control group, 1-based (1–10) |

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

**Standby** — full-screen splash BMP (`/StandbySplash_800x480.bmp` from SD). No dynamic content. Displayed when `flightScene` is false and `idle_state` is asserted by the master, or on initial boot before a flight scene is active. A 3-finger touch advances to Main when `flightScene` is true.

**Main** — primary operational view. Contains:
- MASTER ALARM button (top-left, 240×168 px) — illuminates red when any WARNING-level C&W bit is set. Touch to silence audio.
- Caution & Warning panel (16 annunciator buttons, 4 rows × 4 columns)
- Vessel situation column (7 indicators)
- Control mode indicator (`ROVER` / `PLANE` / `SPCFT`) — turns red if mode mismatches vessel type
- SOI label and body thumbnail (bottom-left, links to SOI screen on touch)
- Data readouts: vessel name, Tmax%, Crew, TW index, CommNet%, Stage, Tskin%, CtrlGrp

**SOI** — celestial body detail screen. Left panel: KASA meatball BMP. Centre: body name. Right: body BMP. Lower rows: Min. Safe Alt, High/Low Atmosphere Alt (if applicable), High Space Alt, surface condition, surface gravity. Touch anywhere to return to Main.

### Screen Transitions

| Event | Result |
|-------|--------|
| `SCENE_CHANGE` → flight | Switch to Main + request Simpit channel refresh |
| `SCENE_CHANGE` → non-flight | Switch to Standby |
| I2C `idle_state` asserted + not in flight | Switch to Standby immediately |
| 3-finger touch on Standby + `flightScene` | Switch to Main |
| Touch on SOI thumbnail area (Main) | Switch to SOI |
| Touch anywhere on SOI | Return to Main |
| Vessel switch | Full redraw of current screen + `updateCautionWarningState()` + Simpit channel refresh |
| EVA state change | Full redraw of current screen |

### Caution & Warning

The C&W panel is a 16-bit bitmask (`state.cautionWarningState`) recomputed on every relevant Simpit message and every demo frame by `updateCautionWarningState()` in `CautionWarning.ino`. Bits that are set in `masterAlarmMask` also illuminate MASTER ALARM and drive audio.

| Bit | Label | Severity | Condition |
|-----|-------|----------|-----------|
| 0 | HIGH SPACE | Info | Aloft, above atmosphere, above body's high-space threshold |
| 1 | LOW SPACE | Info | Aloft, above atmosphere, below high-space threshold |
| 2 | FLYING HIGH | Info | Aloft, in atmosphere, above `flyHigh` altitude |
| 3 | FLYING LOW | Info | Aloft, in atmosphere, below `flyHigh` altitude |
| 4 | ALT | Caution | Aloft, surface altitude < `CW_ALT_THRESHOLD_M` (500 m) |
| 5 | DESCENT | Caution | Aloft and descending (vertical velocity < 0) |
| 6 | GROUND PROX ⚠ | Warning | Aloft, descending, gear up, T.Grnd < `CW_GROUND_PROX_S` (10 s) |
| 7 | MECO | Info | Throttle at 0% while in flight (not pre-launch) |
| 8 | HIGH G ⚠ | Warning | G-forces > `CW_HIGH_G_ALARM` (9 g) or < `CW_HIGH_G_WARN` (−5 g) |
| 9 | BUS VOLTAGE ⚠ | Warning | EC < `CW_EC_LOW_FRAC` (10%) of total capacity (requires ARP mod) |
| 10 | HIGH TEMP ⚠ | Warning | `maxTemp` or `skinTemp` > `tempAlarm` threshold |
| 11 | LOW ΔV ⚠ | Warning | Stage ΔV < `CW_LOW_DV_MS` (150 m/s) or burn time < `CW_LOW_BURN_S` (60 s) — suppressed during MECO + `LOW_DV_MECO_HOLDOFF_MS` hold-off |
| 12 | WARP | Caution | Time warp index > 0 |
| 13 | ATMO | Caution | Vessel inside atmosphere |
| 14 | O2 PRESENT | Info | Atmosphere is breathable (gated on `inAtmo` to prevent stale reads) |
| 15 | CONTACT | Info | Landed or splashed (raw `sit_Landed` or `sit_Splashed` from Simpit) |

⚠ = included in `masterAlarmMask` by default

**Audio triggers** (when `audioEnabled` is true):
- WARNING bits newly set → master alarm starts
- All WARNING bits cleared → master alarm stops, silence latch reset
- `CW_ALT` newly set → caution tone
- `CW_DESCENT` or `CW_ATMO` newly set → caution chirp
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

Named constants are defined in `KCMk1_Annunciator.h`.

---

## Tab Structure

| File | Description |
|------|-------------|
| `KCMk1_Annunciator.ino` | `setup()` and `loop()` only |
| `AAA_Config.ino` | All tunable constants and operating mode flags |
| `AAA_Globals.ino` | Global state, `AppState`, `switchToScreen()`, `invalidateAllState()`, `resetDisplays()` |
| `ScreenMain.ino` | Main screen chrome and update pass |
| `ScreenSOI.ino` | SOI screen chrome and update pass |
| `ScreenStandby.ino` | Standby screen — delegates to `drawStandbySplash()` |
| `CautionWarning.ino` | C&W state machine: `updateCautionWarningState()` |
| `Audio.ino` | Master alarm condition tracking and audio trigger logic |
| `TouchEvents.ino` | Touch debounce and gesture dispatch |
| `SimpitHandler.ino` | KerbalSimpit message handler and channel registration |
| `I2CSlave.ino` | I2C slave at 0x10 — packet build/fill, command processing, boot handshake |
| `BootScreen.ino` | Terminal-aesthetic BIOS POST boot sequence |
| `Demo.ino` | Demo mode — independent field sweep, calls `updateCautionWarningState()` |

---

## Boot Sequence

The Annunciator follows a deterministic startup handshake with the master before entering the main loop.

1. Hardware init (display, SD, touch, audio, I2C slave)
2. Boot screen renders (terminal-aesthetic BIOS POST; header shows live version string via `snprintf`)
3. Simpit handshake runs (or demo mode initialises) — `simpitConnected` set accordingly
4. Annunciator builds a status packet and **asserts pin 2 LOW** (INT)
5. Master detects INT, calls `Wire.requestFrom(0x10, 4)`, reads the 4-byte status packet
6. Master inspects state, sends a 3-byte command packet with `requestType = 0x2` (PROCEED) — configuration flags (`demoMode`, `debugMode`, `audioOn`, `idle_state`, `ctrlMode`, `ctrlGrp`) can be included in the same packet
7. Annunciator receives PROCEED, clears the boot screen, enters `loop()`

**Important:** The master should read the status packet (step 5) before sending PROCEED (step 6). If PROCEED is sent before reading, INT will still be asserted since the `onRequest` handler hasn't fired yet.

---

## Version History

| Version | Notes |
|---------|-------|
| **1.1.1** | Touch count filter now rejects anything other than a single-finger touch (`!= 1`). I2C constants consolidated to `KCMk1_SystemConfig.h` (`KCM_I2C_ADDR_ANNUNCIATOR`, `KCM_I2C_INT_PIN`, `KCM_I2C_SYNC_ANNUNCIATOR`). Cross-panel aligned thresholds now alias `KCM_*` constants — edit in `KCMk1_SystemConfig.h` only. Touch filter constants (`TOUCH_DEBOUNCE_MS`, `TOUCH_DEAD_ZONE`, `TOUCH_JITTER_MAX`) now alias `KCM_TOUCH_*`. Boot screen header now shows live version string (sketch + KDC + KDA) via `snprintf`. `initDemoMode()` now calls `switchToScreen()` at completion to record the screen switch timestamp. `DISPLAY_RESET` I2C command now documented to call `resetDisplays()` before `switchToScreen()` (per-flight flag clearing). Updated to KerbalDisplayCommon 2.1.0 and KerbalDisplayAudio 1.0.1. |
| **1.1.0** | Updated to KerbalDisplayCommon v2.0.1 (`PrintState` required for `printDisp`/`printValue`). Ported 6-layer touch phantom defence from InfoDisp (Y dead zone, double-read stability, jitter check, require-release). Extracted all C&W numeric thresholds to `AAA_Config.ino`. Added sketch version constants to header. |
| **1.0.0** | Initial release. 3-screen display (Main / SOI / Standby), full C&W panel, KerbalSimpit integration, KerbalDisplayAudio, I2C slave boot handshake with master Teensy 4.1. |

---

## Notes

- **ARP mod required** for `CW_BUS_VOLTAGE`. Without ARP, KSP1 never sends `ELECTRIC_MESSAGE`.
- **`audioEnabled`** defaults to `false` — must be enabled in `AAA_Config.ino` or via I2C from the master.
- **`DISPLAY_ROTATION`** — set `2` for inverted bench mounting, `0` for production. Touch coordinate remapping is not needed; the GSL1680F reports in screen-native coordinates at rotation 0.
- **Demo mode** drives all `AppState` fields at configurable rates. `ctrlMode` and `ctrlGrp` are not cycled — they are owned by the master and preserved as last set via I2C. Switching demo off at runtime connects Simpit if not already connected, or requests a full channel refresh if it is.
- **String heap usage** — `state.vesselName` and `state.gameSOI` use Arduino `String`. Low risk on Teensy 4.0 (512 KB RAM) but worth noting if porting to a memory-constrained target.

Licensed under the GNU General Public License v3.0.
Final code written by J. Rostoker for Jeb's Controller Works.
