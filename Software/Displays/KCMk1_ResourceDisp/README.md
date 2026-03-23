# KCMk1_ResourceDisp

**Kerbal Controller Mk1 — Resource Display Panel Sketch**
Teensy 4.0 firmware for the KSP resource monitoring display module.
Part of the KCMk1 controller system. Operates as an I2C slave under a Teensy 4.1 master (Phase 3).

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
| 9 | Audio PWM output (reserved) | OUT | KerbalDisplayAudio (`AUDIO_PIN`) |
| 18 | I2C SDA (Wire — master bus) | — | Sketch / Wire library (Phase 3) |
| 19 | I2C SCL (Wire — master bus) | — | Sketch / Wire library (Phase 3) |
| 2 | I2C interrupt output to master (active-LOW) | OUT | Sketch (`I2C_INT_PIN`) — Phase 3 |

**Serial ports:**
- `Serial` (USB COM port 4) — debug output only
- `SerialUSB1` (USB COM port 5) — KerbalSimpit telemetry traffic

**I2C note (Phase 3):** Wire (pins 18/19) will be the master bus shared with the Teensy 4.1 at address 0x11. Wire1 (pins 16/17) is used exclusively for the GSL1680F touch controller. Pull-ups on the master bus (4.7 kΩ to 3.3 V) should be placed on the master side.

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
| KerbalDisplayCommon | 1.0.0 | Display primitives, fonts, BMP loader, touch driver, system utils |
| KerbalDisplayAudio | 1.0.0 | Non-blocking audio state machine (reserved — not yet wired) |
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
| `demoMode` | `true` | `true` = sine-wave demo values, no KSP connection. `false` = live Simpit mode. Set `false` before deploying with KSP. |
| `debugMode` | `true` | Enables Serial debug output (touch coordinates, screen transitions, Simpit messages) |
| `audioEnabled` | `false` | Reserved for future audio feedback — not yet wired |
| `DISPLAY_ROTATION` | `0` | `0` = normal (connector at bottom), `2` = 180° (inverted mounting) |
| `LOW_RES_THRESHOLD` | `20` | Resource % below which the bar fill color shifts to red |
| `MIN_SLOTS` | `4` | Minimum number of active resource slots (enforced by removeResource) |
| `MAX_SLOTS` | `16` | Maximum number of active resource slots |
| `DEFAULT_SLOT_COUNT` | `8` | Number of slots loaded by `initDefaultSlots()` (STD preset) |
| `VESSEL_CACHE_SIZE` | `20` | Maximum number of per-vessel slot configurations held in session RAM |

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

## Boot Sequence

The boot sequence is simpler than the Annunciator since the ResourceDisp currently has no I2C master handshake (Phase 3 will add this).

1. Hardware init (display, SD, touch)
2. Slot configuration initialised (default types, zeroed values in live mode)
3. Simpit handshake runs (live mode) or demo state initialised (demo mode)
4. Standby screen displayed
5. `SCENE_CHANGE_MESSAGE` entering flight → Main screen, Simpit channel refresh

**Phase 3 note:** A boot handshake with the Teensy 4.1 master (matching the Annunciator pattern) will be added when the I2C slave interface at 0x11 is implemented.

---

## Notes

- **`demoMode`** defaults to `true` for safe bench operation. Set `false` in `AAA_Config.ino` before deploying with KSP. `SCENE_CHANGE_MESSAGE` will also clear `demoMode` at runtime when a flight scene is entered.
- **`audioEnabled`** is reserved for future use. `KerbalDisplayAudio` is included but no audio calls are currently wired. Pin 9 is claimed by the library but unused until Phase 3 audio is implemented.
- **Display rotation** — set `DISPLAY_ROTATION = 2` for inverted bench mounting, `0` for production. Touch coordinate remapping is not required; the GSL1680F reports screen-native coordinates.
- **Touch implementation** — `isTouched()` polls the GSL1680F INT pin directly via `digitalRead()`. The INT pin stays HIGH for the full duration of a touch, making polling reliable without an ISR.
- **String heap usage** — `currentVesselName` and `VesselSlotRecord::vesselName` use Arduino `String`. Low risk on Teensy 4.0 (512 KB RAM) but worth noting if porting to a memory-constrained target.
- **Phase 3 (I2C slave at 0x11):** The ResourceDisp will operate as an I2C slave to the Teensy 4.1 master, following the same protocol structure as the Annunciator (address 0x10). The master will be able to set operating mode flags and trigger display resets. A boot handshake matching the Annunciator pattern will gate entry to the main loop.

Licensed under the GNU General Public License v3.0.
Final code written by J. Rostoker for Jeb's Controller Works.
