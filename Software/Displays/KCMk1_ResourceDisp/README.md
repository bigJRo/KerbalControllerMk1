# KCMk1_ResourceDisp

**Kerbal Controller Mk1 — Resource Display Panel Sketch** · v1.3.0
Teensy 4.0 firmware for the KSP resource monitoring display module.
Part of the KCMk1 controller system. Operates as an I2C slave under a Teensy 4.1 master.

---

## Overview

The Resource Display is an 800×480 touchscreen panel that presents real-time KSP resource telemetry sourced from KerbalSimpit. It runs on a Teensy 4.0 and receives telemetry over USB serial from a running KSP instance. The panel tracks up to 16 resource slots simultaneously and supports per-vessel configuration memory for up to 20 vessels per session.

The panel provides four screens — Standby, Main, Select, and Detail — navigated by touch. The Main screen is the primary operational view, displaying resource levels as a bar graph. The Select screen allows the user to configure which resources are monitored and load preset configurations. The Detail screen provides a numerical breakdown of a single resource by craft and stage values.

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
| 9 | Audio PWM (claimed by library, not used) | OUT | KerbalDisplayAudio (`AUDIO_PIN`) |
| 18 | I2C SDA (Wire — master bus) | — | Sketch / Wire library |
| 19 | I2C SCL (Wire — master bus) | — | Sketch / Wire library |
| 2 | I2C interrupt output to master (active-LOW) | OUT | Sketch (`KCM_I2C_INT_PIN`) |

**Serial ports:**
- `Serial` (USB COM port 4) — debug output only
- `SerialUSB1` (USB COM port 5) — KerbalSimpit telemetry traffic

**I2C note:** Wire (pins 18/19) is the master bus at address **0x11**. Wire1 (pins 16/17) is used exclusively for the GSL1680F touch controller. Pull-ups on the master bus (4.7 kΩ to 3.3 V) should be placed on the master side.

---

## Dependencies

| Library | Version | Notes |
|---------|---------|-------|
| KerbalDisplayCommon | 2.1.0 | Display primitives, fonts, BMP loader, touch driver, system utils |
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

**ARP requirement:** Most resource channels require the **Alternate Resource Panel (ARP)** mod in KSP1. Without ARP, the corresponding Simpit channels are never sent and affected resource slots will remain at zero.

**TAC Life Support resources** (Food, Water, Oxygen, Waste, Liquid Waste, CO2) additionally require the **TAC Life Support** mod.

**CRP custom resources** (Stored Charge, Enriched Uranium, Depleted Fuel, Fertilizer, Intake Air, Liquid Hydrogen, Liquid Methane, Lithium) require **Community Resource Pack (CRP)** and the following two blocks added to `Settings.cfg`:

```
CustomResourceMessages
{
    resourceName1 = StoredCharge
    resourceName2 = EnrichedUranium
    resourceName3 = DepletedFuel
    resourceName4 = Fertilizer
}
CustomResourceMessages
{
    resourceName1 = IntakeAir
    resourceName2 = LqdHydrogen
    resourceName3 = LqdMethane
    resourceName4 = Lithium
}
```

---

## Configuration

All tunables are in `AAA_Config.ino`.

| Constant | Default | Description |
|----------|---------|-------------|
| `debugMode` | `false` | Enables Serial debug output (touch coordinates, screen transitions, Simpit messages). |
| `demoMode` | `false` | `true` = sine-wave demo values, no KSP connection. Can also be toggled at runtime by the I2C master. |
| `DISPLAY_ROTATION` | `0` | `0` = normal (connector at bottom), `2` = 180° (inverted mounting). |
| `LOW_RES_THRESHOLD` | `20` | Resource % below which the bar fill colour shifts to red. |
| `MIN_SLOTS` | `4` | Minimum number of active resource slots (enforced by `removeResource`). |
| `MAX_SLOTS` | `16` | Maximum number of active resource slots. |
| `DEFAULT_SLOT_COUNT` | `8` | Number of slots loaded by `initDefaultSlots()` (STD preset). |
| `VESSEL_CACHE_SIZE` | `20` | Maximum number of per-vessel slot configurations held in session RAM. |
| `BAR_LEVEL_HYSTERESIS` | `0.002` | Minimum fractional resource level change required to trigger a bar redraw (0.2%). Prevents constant SPI traffic from small Simpit fluctuations. |

---

## I2C Protocol

The Resource Display operates as an I2C slave at address **0x11** (`KCM_I2C_ADDR_RESDISP`) on the Wire bus.

### Outbound Packet — ResourceDisp → Master

Size: **4 bytes**. Sent in response to `Wire.requestFrom(0x11, 4)` after INT asserts.

| Byte | Field | Description |
|------|-------|-------------|
| 0 | Sync | `0xAD` (`KCM_I2C_SYNC_RESDISP`) — framing validation |
| 1 | Flags | Bit 0: `simpitConnected`  Bit 1: `flightScene`  Bit 2: `demoMode`  Bits 3–7: reserved (0) |
| 2 | `slotCount` | Number of currently active resource slots (0–16) |
| 3 | Reserved | `0x00` |

### Inbound Packet — Master → ResourceDisp

Size: **2 bytes**. Sent by master at any time via `Wire.beginTransmission(0x11)` / `Wire.write()` / `Wire.endTransmission()`.

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
| `0x2` | PROCEED | Release boot hold — ResourceDisp enters main loop |
| `0x3` | MCU_RESET | Soft reboot the ResourceDisp (USB disconnect then ARM AIRCR reset) |
| `0x4` | DISPLAY_RESET | Reset display state and force full redraw of current screen |

### Expanding the Protocol

- **Outbound:** increment `I2C_PACKET_SIZE` and add fields to `fillI2CPacketBuffer()` in `I2CSlave.ino`
- **Inbound:** increment `I2C_CMD_SIZE` and add fields to `processI2CCommand()` in `I2CSlave.ino`
- Update the master sketch to match in both cases

---

## Features

### Screens

**Standby** — full-screen BMP splash (`/StandbySplash_800x480.bmp` from SD card). Displayed on boot and whenever the panel is not in an active KSP flight scene. In live mode, `SCENE_CHANGE_MESSAGE` entering flight transitions to Main automatically. In demo mode, any touch advances to Main.

**Main** — primary operational view. Displays resource bars for all active slots in a left-to-right bar graph with a right-hand sidebar. Bar height represents the fraction of maximum capacity. Bar fill colour shifts to red when the resource falls below `LOW_RES_THRESHOLD`. A percentage label appears above each bar. A vertical percentage axis (0–100%) is drawn on the left edge.

Sidebar buttons:
- **TOTL / STG** — toggles between vessel-total and active-stage values
- **DFLT** — resets to the default 8-slot STD configuration
- **SEL** — opens the Select screen
- **DATA** — opens the Detail screen

**Select** — resource configuration screen. Left panel: 5-column grid of all available resources. Right panel: ordered slot list with a CLEAR button. Top row: preset buttons (STD, XPD, VEH, LSP, AIR, ADV) and a BACK button. Tapping a resource toggles it. Presets replace the current configuration entirely. In live mode, a Simpit channel refresh is requested after any configuration change.

**Detail** — numerical readout for a single resource. Left panel: selector column with one button per active slot. Right panel: resource name header, followed by data rows. Resources with stage data (LF, LOx, SF, Xenon, Ablator) show six rows in CRAFT and STAGE sections. Resources without stage data show three rows in CRAFT only.

### Resource Slots

The display tracks between `MIN_SLOTS` (4) and `MAX_SLOTS` (16) resource slots. Each slot holds a `ResourceType` and four float values: `current`, `maxVal`, `stageCurrent`, `stageMax`. Values are zeroed on vessel switch or scene exit.

| Group | Resources | Short labels |
|-------|-----------|-------------|
| Power | Electric Charge, Stored Charge | EC, StC |
| Propellants (native) | Liquid Fuel, Oxidizer, Solid Fuel, Monopropellant, Xenon | LF, LOx, SF, MP, XE |
| Propellants (CRP) | Liquid Hydrogen, Liquid Methane, Lithium, Intake Air | LH2, LMe, Li, AIR |
| Nuclear (CRP) | Enriched Uranium, Depleted Fuel | EUr, DFu |
| Other | Ore, Ablator | ORE, ABL |
| Life Support (TAC-LS) | Oxygen, Carbon Dioxide, Food, Waste, Water, Liquid Waste | O2, CO2, FD, WST, H2O, LWS |
| Agriculture (CRP) | Fertilizer | Fer |

### Per-Vessel Configuration Memory

Slot configurations are saved per vessel name in `vesselCache[]` (up to `VESSEL_CACHE_SIZE` entries). On vessel switch, the panel attempts to recall the previous configuration for the new vessel. If not found, the current configuration is retained. On scene exit, the current configuration is saved for the active vessel. The cache is held in RAM only — it does not persist across power cycles.

---

## Tab Structure

| File | Description |
|------|-------------|
| `KCMk1_ResourceDisp.ino` | `setup()` and `loop()` only |
| `AAA_Config.ino` | All tunable constants (thresholds, modes, slot config) |
| `AAA_Globals.ino` | `ResourceSlot` struct, display objects, Simpit object, screen state, vessel cache helpers |
| `Resources.ino` | Resource type definitions, colour map, slot initialisation |
| `ScreenMain.ino` | Main bar graph screen with 4-button sidebar |
| `ScreenSelect.ino` | Resource selection screen (grid + presets + order panel) |
| `ScreenDetail.ino` | Numerical resource detail screen (craft/stage values per resource) |
| `ScreenStandby.ino` | Standby screen — delegates to `drawStandbySplash()` |
| `TouchEvents.ino` | Touch debounce and gesture dispatch |
| `SimpitHandler.ino` | KerbalSimpit message handler and channel registration |
| `I2CSlave.ino` | I2C slave at 0x11 — packet build/fill, command processing, boot handshake |
| `BootScreen.ino` | Jurassic Park-themed terminal boot sequence |
| `Demo.ino` | Demo mode — sine-wave resource values, no KSP connection |

---

## Boot Sequence

The ResourceDisp follows the same deterministic startup handshake as the other KCMk1 panels.

1. Hardware init (display, SD, touch, I2C slave)
2. Boot screen renders (Jurassic Park-themed terminal sequence; header shows live version string)
3. Simpit connects (or demo mode initialises)
4. ResourceDisp builds a status packet and **asserts pin 2 LOW** (INT)
5. Master reads the 4-byte status packet
6. Master sends a 2-byte command packet with `requestType = 0x2` (PROCEED) — configuration flags can be included in the same packet
7. ResourceDisp receives PROCEED, transitions to Standby screen, enters `loop()`

---

## Version History

| Version | Notes |
|---------|-------|
| **1.3.0** | I2C slave interface and boot handshake with master (Phase 3). I2C constants consolidated to `KCMk1_SystemConfig.h`. Touch count filter changed to `!= 1`. Touch filter constants alias `KCM_TOUCH_*`. Boot screen header shows live version string (sketch + KDC + KDA) via `snprintf`. `switchToScreen()` now records `lastScreenSwitch` timestamp. KDA dependency clarified as direct (not a KDC sub-dependency). Updated to KerbalDisplayCommon 2.1.0 and KerbalDisplayAudio 1.0.1. |
| **1.2.0** | KerbalSimpit integration for live resource telemetry. Per-vessel configuration memory (`vesselCache[]`). Simpit channel refresh on vessel switch and scene entry. |
| **1.1.0** | Select screen presets (STD, XPD, VEH, LSP, AIR, ADV). Detail screen with CRAFT/STAGE sections. Stage data support for LF, LOx, SF, Xenon, Ablator. |
| **1.0.0** | Initial release. Main bar graph with 4-button sidebar, demo mode, 16-slot configuration. |

---

## Notes

- **`debugMode`** defaults to `false`. Set `true` during development for touch coordinates and Simpit message logging.
- **`demoMode`** defaults to `false` (live Simpit). Set `true` for bench testing without KSP.
- **ARP mod** is required for most resource channels. Without it, bars remain at zero.
- **Stage data** is only available for resources with dedicated Simpit stage channels (LF, LOx, SF, Xenon, Ablator). All others mirror vessel totals in the STAGE section and suppress it on the Detail screen.
- **Vessel cache eviction** — when `VESSEL_CACHE_SIZE` is exceeded, the oldest entry is overwritten (simple last-in eviction). No LRU tracking.

Licensed under the GNU General Public License v3.0.
Final code written by J. Rostoker for Jeb's Controller Works.
