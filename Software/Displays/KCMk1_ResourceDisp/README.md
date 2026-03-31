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
| 9 | Audio PWM output (claimed by library, unused) | OUT | KerbalDisplayAudio (`AUDIO_PIN`) |
| 18 | I2C SDA (Wire — master bus) | — | Sketch / Wire library |
| 19 | I2C SCL (Wire — master bus) | — | Sketch / Wire library |
| 2 | I2C interrupt output to master (active-LOW) | OUT | Sketch (`I2C_INT_PIN`) |

**Serial ports:**
- `Serial` (USB COM port 4) — debug output only
- `SerialUSB1` (USB COM port 5) — KerbalSimpit telemetry traffic

**I2C note:** Wire (pins 18/19) is the master bus shared with the Teensy 4.1 at address 0x11. Wire1 (pins 16/17) is used exclusively for the GSL1680F touch controller. Pull-ups on the master bus (4.7 kΩ to 3.3 V) should be placed on the master side.

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
| KerbalDisplayCommon | 2.0.1 | Display primitives, fonts, BMP loader, touch driver, system utils |
| KerbalDisplayAudio | 1.0.0 | Audio library (included as dependency; audio not used on this panel) |
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

**TAC Life Support resources** (Food, Water, Oxygen, Waste, Liquid Waste, CO2) additionally require the **TAC Life Support** mod. Without it those channels are never sent.

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
| `demoMode` | `false` | `true` = sine-wave demo values, no KSP connection. `false` = live Simpit mode. Can also be toggled at runtime by the I2C master — reinitialises demo state or connects Simpit as appropriate. |
| `debugMode` | `true` | Enables Serial debug output (touch coordinates, screen transitions, Simpit messages) |
| `DISPLAY_ROTATION` | `0` | `0` = normal (connector at bottom), `2` = 180° (inverted mounting) |
| `LOW_RES_THRESHOLD` | `20` | Resource % below which the bar fill color shifts to red |
| `MIN_SLOTS` | `4` | Minimum number of active resource slots (enforced by removeResource) |
| `MAX_SLOTS` | `16` | Maximum number of active resource slots |
| `DEFAULT_SLOT_COUNT` | `8` | Number of slots loaded by `initDefaultSlots()` (STD preset) |
| `VESSEL_CACHE_SIZE` | `20` | Maximum number of per-vessel slot configurations held in session RAM |
| `BAR_LEVEL_HYSTERESIS` | `0.002` | Minimum fractional resource level change required to trigger a bar redraw (0.2%). Prevents constant SPI traffic from small Simpit fluctuations. |

---

## Resource Slots

The display tracks between `MIN_SLOTS` (4) and `MAX_SLOTS` (16) resource slots simultaneously. Each slot holds a `ResourceType` and four float values: `current`, `maxVal`, `stageCurrent`, `stageMax`. Values are populated by Simpit messages and zeroed on vessel switch or scene exit.

### ResourceType Enum

All supported resource types are listed in `KCMk1_ResourceDisp.h`. Short labels (used on bar graphs) and full names (used on the detail screen) are defined in `Resources.ino`.

| Group | Resources | Short labels |
|-------|-----------|-------------|
| Power | Electric Charge, Stored Charge | EC, StC |
| Propellants (native KSP1) | Liquid Fuel, Oxidizer, Solid Fuel, Monopropellant, Xenon | LF, LOx, SF, MP, XE |
| Propellants (CRP mod) | Liquid Hydrogen, Liquid Methane, Lithium, Intake Air | LH2, LMe, Li, AIR |
| Nuclear (CRP mod) | Enriched Uranium, Depleted Fuel | EUr, DFu |
| Other | Ore, Ablator | ORE, ABL |
| Life Support (TAC-LS mod) | Oxygen, Carbon Dioxide, Food, Waste, Water, Liquid Waste | O2, CO2, FD, WST, H2O, LWS |
| Agriculture (CRP mod) | Fertilizer | Fer |

### Stage Data

Resources with dedicated Simpit stage channels (LF, LOx, SF, Xenon, Ablator) populate all four slot fields independently. All other resources have no stage channel — their `stageCurrent`/`stageMax` mirror their vessel values. The Detail screen detects this automatically and suppresses the STAGE section for vessel-only resources, showing only the CRAFT section.

---

## Simpit Channel Mapping

| Resource(s) | Simpit Channel | Stage channel | Mod required |
|-------------|----------------|---------------|--------------|
| Electric Charge | `ELECTRIC_MESSAGE` | mirrors vessel | ARP |
| Liquid Fuel | `LF_MESSAGE` / `LF_STAGE_MESSAGE` | ✓ | ARP |
| Oxidizer | `OX_MESSAGE` / `OX_STAGE_MESSAGE` | ✓ | ARP |
| Solid Fuel | `SF_MESSAGE` / `SF_STAGE_MESSAGE` | ✓ | ARP |
| Monopropellant | `MONO_MESSAGE` | mirrors vessel | ARP |
| Xenon | `XENON_GAS_MESSAGE` / `XENON_GAS_STAGE_MESSAGE` | ✓ | ARP |
| Ore | `ORE_MESSAGE` | mirrors vessel | ARP |
| Ablator | `AB_MESSAGE` / `AB_STAGE_MESSAGE` | ✓ | ARP |
| Food, Water, Oxygen | `TACLS_RESOURCE_MESSAGE` | mirrors vessel | ARP + TAC-LS |
| Waste, Liquid Waste, CO2 | `TACLS_WASTE_MESSAGE` | mirrors vessel | ARP + TAC-LS |
| Stored Charge, Enriched Uranium, Depleted Fuel, Fertilizer | `CUSTOM_RESOURCE_1_MESSAGE` | mirrors vessel | ARP + CRP |
| Intake Air, Liquid Hydrogen, Liquid Methane, Lithium | `CUSTOM_RESOURCE_2_MESSAGE` | mirrors vessel | ARP + CRP |

On flight scene entry and vessel switch, `simpit.requestMessageOnChannel(0)` is called to force KSP to immediately resend values on all subscribed channels. This ensures bars populate without waiting for the first change event on each channel.

---

## Screens

Four screens are available. Transitions are managed by `switchToScreen()` in `AAA_Globals.ino`.

**Standby**
Full-screen BMP splash (`/StandbySplash_800x480.bmp` from SD card — same file as the Annunciator). Displayed on boot and whenever the display is not in an active KSP flight scene. In live mode, a `SCENE_CHANGE_MESSAGE` entering flight transitions to Main automatically. In demo mode, any touch on the standby screen advances to Main.

**Main**
Primary operational view. Displays resource bars for all active slots in a left-to-right bar graph with a right-hand sidebar. Bar height represents the fraction of maximum capacity. Bar fill color shifts to red when the resource falls below `LOW_RES_THRESHOLD`. A percentage label appears above each bar.

Sidebar buttons (right column):
- **TOTL / STG** — toggles between vessel-total and active-stage values for resources with stage data
- **DFLT** — resets to the default 8-slot STD configuration (EC, LF, LOx, MP, SF, O2, Food, Water)
- **SEL** — opens the Select screen
- **DATA** — opens the Detail screen

A vertical percentage axis (0–100%) is drawn on the left edge of the bar area.

**Select**
Resource configuration screen. Left panel: 5-column grid of all available resources. Right panel: ordered slot list showing current configuration, with a CLEAR button. Top row: preset buttons (STD, XPD, VEH, LSP, AIR, ADV) and a BACK button. Tapping a resource in the grid toggles it in or out of the active slot list. The CLEAR button removes all slots (bypasses `MIN_SLOTS` — use DFLT to restore defaults). Tapping a preset replaces the current configuration entirely. In live mode, a Simpit channel refresh is requested after any configuration change so new slots populate immediately.

**Detail**
Numerical readout for a single resource. Left panel: selector column with one button per active slot. Right panel: header with resource name and color accent, followed by data rows.

For resources with stage data (LF, LOx, SF, Xenon, Ablator): six rows in two sections — CRAFT (Available / Total / Remaining%) and STAGE (Available / Total / Remaining%).

For all other resources (vessel-only): three rows in the CRAFT section only, occupying the same vertical position as the CRAFT section in the full layout. The STAGE section is suppressed to avoid showing duplicate mirrored values.

### Screen Transitions

| Event | Result |
|-------|--------|
| Boot (live mode) | Standby |
| Boot (demo mode) | Standby (touch to advance to Main) |
| `SCENE_CHANGE` → flight | Main + request Simpit channel refresh |
| `SCENE_CHANGE` → non-flight | Standby |
| Touch on Standby (demo mode) | Main |
| DFLT button (Main) | Main (full redraw with default slots) |
| SEL button (Main) | Select |
| DATA button (Main) | Detail |
| BACK button (Select) | Main |
| BACK button (Detail) | Main |
| Vessel switch | Full redraw of current screen + request Simpit channel refresh |

---

## Presets

Six preset slot configurations are available from the Select screen.

| Preset | Name | Slots | Resources |
|--------|------|-------|-----------|
| STD | Standard | 8 | EC, LF, LOx, MP, SF, O2, Food, Water |
| XPD | Expedition | 9 | EC, LF, LOx, MP, LH2, EUr, O2, Food, Water |
| VEH | Vehicle | 15 | EC, StC, LF, LOx, MP, SF, XE, ORE, ABL, LH2, LMe, Li, EUr, DFu, Fer |
| LSP | Life Support | 8 | EC, O2, CO2, Food, Waste, H2O, LWS, Fer |
| AIR | Aircraft | 8 | EC, LF, LOx, AIR, MP, O2, Food, Water |
| ADV | Advanced | 16 | EC, StC, XE, ORE, LH2, LMe, Li, EUr, DFu, O2, Food, H2O, CO2, Waste, LWS, Fer |

---

## Per-Vessel Slot Memory

The display maintains an in-RAM cache of up to `VESSEL_CACHE_SIZE` (20) per-vessel slot configurations for the duration of the session (until power cycle or microcontroller reset). The cache is keyed by vessel name received from Simpit's `VESSEL_NAME_MESSAGE`.

**Save:** Current slot types are saved to the cache under the current vessel name on vessel switch and on flight scene exit.

**Recall:** When a new vessel name arrives, the cache is searched. If a matching entry is found, the slot types are restored and a Simpit channel refresh is requested so values populate immediately. If not found, the current layout is preserved.

**Eviction:** When the cache is full and a new vessel is encountered, the oldest entry (last slot) is overwritten. Only slot *types* are cached — values are always repopulated from Simpit on recall.

---

## I2C Protocol

The ResourceDisp operates as an I2C slave at address **0x11** on the Wire bus. The master (Teensy 4.1) drives the bus. Communication is interrupt-driven: the ResourceDisp asserts pin 2 LOW when new data is ready; the master reads and then sends a command packet in response.

### Outbound Packet — ResourceDisp → Master

Size: **4 bytes**. Sent in response to `Wire.requestFrom(0x11, 4)` after INT asserts.

| Byte | Field | Description |
|------|-------|-------------|
| 0 | Sync | `0xAD` — framing validation magic byte |
| 1 | Flags | Bit 0: `simpitConnected`  Bit 1: `flightScene`  Bit 2: `demoMode`  Bits 3–7: reserved (0) |
| 2 | `slotCount` | Number of currently active resource slots (0–16) |
| 3 | Reserved | `0x00` — available for future use |

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
| 2 | Reserved | Always `0` — reserved for future common use |
| 1 | `demoMode` | `1` = enable demo mode (disables Simpit) |
| 0 | `debugMode` | `1` = enable Serial debug output |

**Request type codes (`controlByte` bits 7:4):**

| Code | Name | Action |
|------|------|--------|
| `0x0` | NOP | No operation |
| `0x1` | STATUS | Force immediate status packet — assert INT now |
| `0x2` | PROCEED | Release boot hold — ResourceDisp clears screen and enters main loop |
| `0x3` | MCU_RESET | Soft reboot the ResourceDisp (USB disconnect then ARM AIRCR reset) |
| `0x4` | DISPLAY_RESET | Reset display state and force full redraw of current screen |

**Runtime `demoMode` toggle:** when the master flips `demoMode` on, the ResourceDisp reinitialises sine-wave demo state. When flipped off, it connects Simpit if not already connected, or requests a full channel refresh if it is.

### Expanding the Protocol

- **Outbound:** increment `I2C_PACKET_SIZE` and add fields to `buildI2CPacket()` in `I2CSlave.ino`
- **Inbound:** increment `I2C_CMD_SIZE` and add fields to `processI2CCommand()` in `I2CSlave.ino`
- Update the master sketch to match in both cases

---

## Boot Sequence

The ResourceDisp follows the same deterministic startup handshake as the Annunciator.

1. Hardware init (display, SD, touch, I2C slave)
2. Boot simulation screen renders (Jurassic Park-themed terminal sequence)
3. Slot configuration initialised (default types, zeroed values in live mode)
4. Simpit handshake runs (live mode) or demo state initialised (demo mode)
5. ResourceDisp builds a status packet and **asserts pin 2 LOW** (INT)
6. Master detects INT, calls `Wire.requestFrom(0x11, 4)`, reads the status packet
7. Master sends a 3-byte command packet with `requestType = 0x2` (PROCEED)
8. ResourceDisp receives PROCEED, clears the boot screen, shows standby screen, enters `loop()`

In live mode, `SCENE_CHANGE_MESSAGE` entering flight transitions from standby to the main screen and requests a Simpit channel refresh.

---

## Version History

| Version | Notes |
|---------|-------|
| **1.3.0** | Updated to KerbalDisplayCommon v2.0.1 (`PrintState` required for `printValue`). Ported 6-layer touch phantom defence from InfoDisp (count filter, Y dead zone, double-read stability, jitter check, require-release, 500 ms debounce). Extracted bar update hysteresis to `BAR_LEVEL_HYSTERESIS` config constant. Added sketch version constants to header. |
| **1.2.0** | I2C slave boot handshake with master Teensy 4.1 (`I2CSlave.ino`), per-vessel slot memory cache (`VESSEL_CACHE_SIZE`). Phase 3 complete. |
| **1.1.0** | KerbalSimpit integration for live resource telemetry. Phase 2 complete. |
| **1.0.0** | Initial release. Demo mode bar graph display, touch-based resource selection and ordering, preset configurations, numerical detail screen. Phase 1 complete. |

---

## Notes

- **`demoMode`** defaults to `false` for production. Set `true` in `AAA_Config.ino` for bench testing without KSP. The I2C master can also toggle `demoMode` at runtime — switching to demo reinitialises sine-wave state; switching to live connects Simpit (or requests a channel refresh if already connected).
- **Display rotation** — set `DISPLAY_ROTATION = 2` for inverted bench mounting, `0` for production. Touch coordinate remapping is not required; the GSL1680F reports screen-native coordinates.
- **Touch implementation** — `isTouched()` polls the GSL1680F INT pin directly via `digitalRead()`. The INT pin stays HIGH for the full duration of a touch, making polling reliable without an ISR. Touch events pass through a 6-layer phantom defence (count filter, Y dead zone, bounds check, double-read stability, 500 ms debounce, require-release) before being dispatched.
- **String heap usage** — `currentVesselName` and `VesselSlotRecord::vesselName` use Arduino `String`. Low risk on Teensy 4.0 (512 KB RAM) but worth noting if porting to a memory-constrained target.
- **`KerbalDisplayAudio`** is included as a library dependency and claims pin 9, but audio output is not implemented on this panel.

Licensed under the GNU General Public License v3.0.
Final code written by J. Rostoker for Jeb's Controller Works.
