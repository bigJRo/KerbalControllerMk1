# KCMk1_ResourceDisp

**Kerbal Controller Mk1 — Resource Display Panel Sketch** · v3.12.1
Teensy 4.1 firmware for the KSP resource monitoring display module.
Part of the KCMk1 controller system. Operates as an I2C slave under a Teensy 4.1 master.

---

## Overview

The Resource Display is a 1024×600 touchscreen panel that presents real-time KSP resource telemetry sourced from KerbalSimpit. It runs on a Teensy 4.1 and receives telemetry over USB serial from a running KSP instance. The panel tracks up to 16 resource slots simultaneously and supports per-vessel configuration memory for up to 20 vessels per session.

The panel provides four screens — Standby, Main, Select, and Detail — navigated by touch.

### Gestures and keys

| Where | Gesture | Does |
|---|---|---|
| Main, any meter | Tap | Opens Detail on that resource |
| Main, a tape | Hold still 1 s | Sets a reserve bug at the level first touched, on a 5% grid |
| Main, on a bug | Hold still 1 s | Clears that bug |
| Main, on a bug | Drag | Moves the bug with the finger, in 1% steps |
| Main, alert strip | Tap a message | Opens Detail on that resource |
| Main sidebar | SEL / DATA / TTE / CLR BUG | Select screen / Detail screen / counter row percent or time-to-empty / remove every bug |
| Select | Tap a resource | Adds or removes it; presets replace the set; DFLT makes the set (with its bugs) the default for vessels not in memory, CLEAR then DFLT drops the stored default; BACK returns |
| Select | CLEAR, then leave the vessel | Forgets that vessel: an empty set is not a layout, so it starts from the default next time |
| Select | Hold CLEAR 3 s | Forgets every vessel (countdown in the counter area; lift early to cancel). The default layout is kept |
| Detail | Tap a selector key | Shows that resource; bug bar keys set its bug to an exact percent; BACK returns | The Main screen is the primary operational view, displaying resource levels as a row of Shuttle-style tape meters grouped by subsystem. The Select screen allows the user to configure which resources are monitored and load preset configurations. The Detail screen provides a numerical breakdown of a single resource by craft and stage values.

---

## Hardware

Hardware rev 2: the panel moved from the rev-1 RA8875 800×480 SPI stack to the shared 7" TFT carrier. All pins and the resolution come from `KCMk1_SystemConfig.h`.

The display controller is the **LT7683** (the physical part on the ER-TFT070A2-6-5633 module); it is register-compatible with the RA8876, so the firmware drives it through the `wwatson4506/TeensyRA8876-8080` FlexIO3 driver (class `RA8876_t41_p`). "RA8876" therefore appears in driver/library/class names throughout, while the hardware part is the LT7683.

| Component | Part | Interface |
|-----------|------|-----------|
| Microcontroller | Teensy 4.1 | — |
| Display | LT7683 (RA8876-compatible) 1024×600 TFT | 16-bit 8080 parallel (FlexIO3) |
| Touch controller | FT5316 5-point capacitive | software I2C (pins 4/5) |
| SD card | Teensy 4.1 on-board microSD | SDIO (`BUILTIN_SDCARD`) |
| KSP telemetry | KerbalSimpit plugin | SerialUSB1 (second USB COM port) |
| I2C slave bus | Master Teensy 4.1 | Wire2 (pins 24/25) |

### Pin Assignments

All display, touch, SD and I2C pins are defined centrally in `KCMk1_SystemConfig.h` (shared across the rev-2 panels). Key lines:

| Pin | Function | Direction | Define |
|-----|----------|-----------|--------|
| 34 | Display /CS chip select | OUT | `KCM_TFT_CS` |
| 33 | Display RS register/data select | OUT | `KCM_TFT_RS` |
| 35 | Display /RST reset | OUT | `KCM_TFT_RESET` |
| 36 | Display /WR write strobe | OUT | `KCM_TFT_WR` |
| 37 | Display /RD read strobe | OUT | `KCM_TFT_RD` |
| 32 | Display WAIT | IN | `KCM_TFT_WAIT` |
| 31 | Display INT | IN | `KCM_TFT_INT` |
| 9 | TFT backlight enable / PWM | OUT | `KCM_TFT_BL` |
| DB0..DB15 | 16-bit parallel data bus | — | FlexIO3 (driver-owned) |
| 4 | FT5316 SCL (software I2C) | — | `KCM_CTP_SCL` |
| 5 | FT5316 SDA (software I2C) | — | `KCM_CTP_SDA` |
| 3 | FT5316 /RST | OUT | `KCM_CTP_RST` |
| 6 | FT5316 INT | IN | `KCM_CTP_INT` |
| — | SD card (SDIO) | — | `BUILTIN_SDCARD` |
| 24 | I2C SCL2 (Wire2 — master bus) | — | `KCM_I2C_BUS` |
| 25 | I2C SDA2 (Wire2 — master bus) | — | `KCM_I2C_BUS` |
| 0 | I2C interrupt output to master (active-LOW) | OUT | `KCM_I2C_INT_PIN` |

**Serial ports:**
- `Serial` (USB COM port 4) — debug output only
- `SerialUSB1` (USB COM port 5) — KerbalSimpit telemetry traffic

**I2C note:** Wire2 (pins 24/25) is the master bus at address **0x11** (`KCM_I2C_ADDR_RESDISP`). The FT5316 touch controller runs on a separate bit-banged software I2C bus (pins 4/5, address 0x38). Pull-ups on the master bus (4.7 kΩ to 3.3 V) should be placed on the master side. Note pin 9 is now the TFT backlight; KerbalDisplayAudio is linked but never invoked on this panel (and its `AUDIO_PIN` default is pin 29, not 9), so it drives no pins and does not conflict with the backlight.

---

## Dependencies

| Library | Version | Notes |
|---------|---------|-------|
| KerbalDisplayCommon | ≥ 3.9.0 | Display primitives, fonts, BMP loader, system utils; pulls in KCM_Display (`KCM_TFT`) + SystemConfig |
| KCM_Touch | — | FT5316 capacitive touch driver (replaces the rev-1 GSL1680F driver) |
| TeensyRA8876-8080 (`RA8876_t41_p`) + TeensyRA8876-GFX-Common | — | RA8876 16-bit parallel driver + GFX layer — install on the build machine (not vendored) |
| KerbalDisplayAudio | 1.1.0 | Direct sketch dependency — audio output not used on this panel |
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

Operating-mode tunables are in `AAA_Config.ino`; the slot/cache sizing constants live in the sketch header `KCMk1_ResourceDisp.h`.

| Constant | Location | Default | Description |
|----------|----------|---------|-------------|
| `debugMode` | `AAA_Config.ino` | `false` | Enables Serial debug output (touch coordinates, screen transitions, Simpit messages). |
| `demoMode` | `AAA_Config.ino` | `false` | `true` = sine-wave demo values, no KSP connection. Can also be toggled at runtime by the I2C master. |
| `DEMO_EVA` | `AAA_Config.ino` | `false` | With `demoMode`, start the demo on EVA: the fixed EVA slot set and the ring-gauge layout. Bench switch for the EVA screen; no effect in live mode. |
| `PERSIST_SETTLE_MS` | `AAA_Config.ino` | `30000` | How long a layout, bug or TTE-toggle change must hold still before it is written to EEPROM. A vessel switch or leaving the flight scene writes at once. |
| `PERSIST_WIPE` | `AAA_Config.ino` | `false` | `true` erases the stored vessel memory and default layout at boot and logs it. Flash, boot once, set it back to `false`. |
| `MEM_CLEAR_HOLD_MS` | `AAA_Config.ino` | `3000` | How long CLEAR must be held on the Select screen to forget every vessel. |
| `DISPLAY_ROTATION` | `AAA_Config.ino` | `0` | `0` = normal (connector at bottom), `2` = 180° (inverted mounting). |
| `RES_WARN_FRAC` | `AAA_Config.ino` | `KCM_RES_LOW_WARN_FRAC` (0.20) | Caution (yellow) band: a consumable meter alerts below this fraction of capacity. Aliases the cross-panel constant so it matches the Annunciator PROP LOW / RCS LOW tier. |
| `RES_ALARM_FRAC` | `AAA_Config.ino` | `KCM_EC_LOW_ALARM_FRAC` (0.05) | Alarm (red) band: a consumable meter alerts below this fraction. Aliases the cross-panel constant so it matches the Annunciator red tier. |
| `WASTE_WARN_FRAC` / `WASTE_ALARM_FRAC` | `AAA_Config.ino` | `0.80` / `0.95` | Waste-type meters (CO2, Waste, Liquid Waste, Depleted Fuel) alert on filling up, at these fractions full. Match the Annunciator `TACLS_WASTE_*` fractions; keep in step by hand, there is no shared define yet. |
| `TREND_WINDOW_MS` | `AAA_Config.ino` | `2000` | Trend-arrow sample window. Each meter compares its value against the value at the start of the window. |
| `TREND_MIN_FRAC` | `AAA_Config.ino` | `0.0005` | Movement across one window, as a fraction of capacity, needed to show a rising/falling arrow, and the deadband below which the TTE rate reads as zero. Keeps both quiet on per-message jitter. |
| `TIME_WARN_S_*` / `TIME_ALARM_S_*` | `AAA_Config.ino` | EC 15 m / 5 m; O2 30 m / 10 m; H2O 12 h / 4 h; Food 72 h / 24 h | Time-remaining tiers per resource, game seconds. O2, water and food match the Annunciator's TAC-LS thresholds. |
| `TIME_HYST_FRAC` | `AAA_Config.ino` | `0.10` | A time tier is left only once the time exceeds its threshold by this fraction. |
| `TTE_WINDOW_MS` | `AAA_Config.ino` | `10000` | Rate sample window behind the TTE counter and the Detail Rate/Time rows. Longer than the trend window so slow drains register; with the deadband above, the slowest drain this window reports on its own is about 5.5 hours to empty. Rates are measured in real time and converted to game time with the warp index from `FLIGHT_STATUS_MESSAGE`; sampling restarts on a warp change. |
| `TTE_LONG_WINDOW_MS` | `AAA_Config.ino` | `300000` | A second, long rate window used when the short one sees nothing, reaching about a week to empty. Food and water on a small crew are invisible to ten seconds at 1x; without this their TTE read `---` and their time tiers could only fire under warp. |
| `ALARM_FLASH_MS` / `ALARM_FLASH_HALF_MS` | `AAA_Config.ino` | `3000` / `250` | How long a new alarm tile flashes in the alert strip, and the half period of the flash. |
| `ALERT_HYST_FRAC` | `AAA_Config.ino` | `0.01` | Hysteresis on every caution/alarm threshold, as a fraction of capacity. |
| `BUG_HOLD_MS` | `AAA_Config.ino` | `1000` | How long a touch must be held on a tape to set or clear a reserve bug. A shorter touch is a tap and opens Detail. |
| `BUG_SNAP_PCT` | `AAA_Config.ino` | `5` | A hold places a bug on a multiple of this percent. A drag then moves it in 1% steps, and the Detail screen's keys set it to an exact percent. |
| `BUG_GRAB_TOL` | `AAA_Config.ino` | `0.08` | A touch within this fraction of an existing reserve bug grabs it, for a hold-to-clear or a drag. |
| `BUG_DRAG_MIN_PX` | `AAA_Config.ino` | `12` | Travel before a grabbed bug starts to move. The hysteresis that keeps a twitch during a hold from reading as a drag. |
| `REFRESH_TIMEOUT_MS` | `AAA_Config.ino` | `3000` | How long after a channel refresh request a silent slot is drawn as `...` (awaiting) before it becomes `---` (not aboard). |
| `MAX_SLOTS` | `KCMk1_ResourceDisp.h` | `16` | Maximum number of active resource slots. |
| `DETAIL_HISTORY` | `KCMk1_ResourceDisp.h` | `true` | Draws the level history trace on the Detail screen. `false` stops the sampling and gives the Detail rows their full width back. |
| `HIST_LEN` / `HIST_PERIOD_MS` | `KCMk1_ResourceDisp.h` | `120` / `5000` | Samples kept and real milliseconds between them: ten minutes of history per resource, keyed by resource type so a layout change does not scramble it. |
| `DEFAULT_SLOT_COUNT` | `KCMk1_ResourceDisp.h` | `9` | Visible meters up to this count draw in the standard class; more go compact. Also the SPCT preset's size, the built-in default layout. |
| `VESSEL_CACHE_SIZE` | `KCMk1_ResourceDisp.h` | `20` | Maximum number of per-vessel slot configurations held in session RAM. |
| `BAR_LEVEL_HYSTERESIS` | `AAA_Config.ino` | `0.002` | Minimum fractional level change required to redraw a tape fill or its secondary marker (0.2%). Prevents constant bus traffic from small Simpit fluctuations. |

---

## I2C Protocol

The Resource Display operates as an I2C slave at address **0x11** (`KCM_I2C_ADDR_RESDISP`) on the Wire2 bus (`KCM_I2C_BUS`, pins 24/25).

### Outbound Packet — ResourceDisp → Master

Size: **4 bytes**. Sent in response to `KCM_I2C_BUS.requestFrom(0x11, 4)` (Wire2) after INT asserts.

| Byte | Field | Description |
|------|-------|-------------|
| 0 | Sync | `0xAD` (`KCM_I2C_SYNC_RESDISP`) — framing validation |
| 1 | Flags | Bit 0: `simpitConnected`  Bit 1: `flightScene`  Bit 2: `demoMode`  Bit 3: `cautionActive` (a selected resource in caution or across its bug)  Bit 4: `alarmActive`  Bit 5: `timeTierActive` (one of those is a time-remaining tier, not a level)  Bits 6–7: reserved (0) |
| 2 | `slotCount` | Number of currently active resource slots (0–16) |
| 3 | `worstResource` | `ResourceType` of the worst alert: the first slot in alarm, else the first in caution; `0` (`RES_NONE`) when all nominal. The summary is evaluated from the slots and the rate sampling directly, so it is the same whatever screen is up, and a change in it raises a packet like any other. |

### Inbound Packet — Master → ResourceDisp

Size: **2 bytes**. Sent by master at any time via `KCM_I2C_BUS.beginTransmission(0x11)` / `.write()` / `.endTransmission()` (Wire2).

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

**Standby** — full-screen BMP splash (`/StandbySplash_1024x600.bmp` from SD card). Displayed on boot and whenever the panel is not in an active KSP flight scene. In live mode, `SCENE_CHANGE_MESSAGE` entering flight transitions to Main automatically. In demo mode, any touch advances to Main.

**Main** — primary operational view: one vertical tape meter per active slot, laid out the way the Shuttle's F7/O3 panel meters were, with a left-hand sidebar and an alert strip across the top. Sidebar keys are achromatic — white-on-black with a grey border — so the panel's colour budget belongs entirely to the meters, their limit bands and their alert states; the TTE key reverse-videos while engaged to show it has moved off the default. See the InfoDisp README for the full rationale. A shared 0–100% axis sits just inboard of the sidebar.

**Alert strip** (top of the content area) — an EICAS-style message line listing every meter in caution or alarm, worst first. Its right end carries the **vessel name**, cyan when the layout on screen came from vessel memory and grey when it is the default (the Kerbal's name on EVA), and the propellant balance cell when LF and LOx are both aboard. A tap on a message opens Detail on that resource. Messages: `SF LOW` (or `CO2 HIGH` for a waste product) white-on-red for an alarm, `MP CAUT` in yellow for a caution, `LF BUG` in cyan when a reserve bug is crossed, `O2 TIME` in the tier's colour when a time-remaining tier raised the state, and `REFRESHING` in white while a channel refresh is pending. Messages that do not fit collapse to `+N`. A tile that has just turned alarm flashes for `ALARM_FLASH_MS`, the caution-and-warning convention for a new alarm, then holds steady. At its right end, when both Liquid Fuel and Oxidizer are on the panel, the **propellant balance** indicator: Apollo's OXID UNBAL meter for KSP's 9:11 LF:LOx burn ratio. A centre-zero bar deflects toward whichever propellant is in surplus, and a counter says by how many units (`LOx +123`). Stage quantities are used when both have them, else vessel totals.

Each meter, top to bottom:

- **Group label.** Meters are laid out in subsystem order — PWR, PROP, NUC, MISC, LS, AGR — with a bracketed label over each run and a 1 px divider between runs. Within a group the order is the Select grid's, so LF is always left of LOx on every vessel. The ordering is applied to the slot list itself (`sortSlotsByGroup()`) as slots are added and on every entry to the Main screen, so the Detail selector and the Select ORDER list agree with the meters.
- **The tape.** A narrow thermometer column, filled in the resource's fixed colour to the vessel total. On its left is a **limit-band column** with the resource's alarm and caution fractions painted red and yellow on the scale, visible whether or not the level is in them. Consumables band at the bottom (below `RES_WARN_FRAC` / `RES_ALARM_FRAC`); waste-type resources band at the top (above `WASTE_WARN_FRAC` / `WASTE_ALARM_FRAC`); Ore and Intake Air have no bands. Tick marks on the right edge: major every 10%, minor every 5%. The frame is grey when nominal and turns yellow or red on breach.
- **Split column for stage.** For the five resources with a separate stage channel (LF, LOx, SF, Xenon, Ablator), the tape is split the way the Shuttle PRPLT QTY meter carried its pointers side by side: the wide left column is the vessel total in the resource colour, the narrow right column is the *active stage* in a half-brightness shade of the same colour, and a white line across the whole tape marks the stage level so it reads against the ticks even where the narrow column is only a few pixels wide. Both values are always visible; there is no mode to toggle. The counters follow the total; stage figures are on the DATA screen. Resources without a stage channel draw one full-width column, which is itself the cue that there is no separate stage figure.
- **Reserve bug.** A touch **held** on the tape for `BUG_HOLD_MS` sets a cyan index at the level first touched, snapped to the nearest `BUG_SNAP_PCT` (cyan is this project's colour for pilot-entered values), its percent in small cyan figures beside it, and a cyan mark on the band. When the level crosses it, below for a consumable and above for a waste product, the meter's frame and counter take the bug's cyan and the alert strip shows `LF BUG` in cyan; a bug never raises an alarm, and the fixed limit bands outrank it. A touch that lands within `BUG_GRAB_TOL` of an existing bug grabs it: held still it clears the bug, moved more than `BUG_DRAG_MIN_PX` it drags the bug with the finger, in 1% steps, until release. Bugs are saved with the vessel's slot memory.
- **Label**, then the **counter row**: the percent of capacity, or with the TTE key engaged the **time to empty** at the current rate (`4:35` under ten minutes, `42m` under an hour, `5.5h` beyond; time to *full* for waste-type resources; `---` while steady, filling, or draining too slowly to measure). Coloured by state: white nominal, yellow caution, white-on-red alarm. A **trend arrow** two spaces to its right shows rising or falling (counter, gap and arrow slot are centred in the cell as one group, the arrow slot reserved so the number never shifts) while the value moves more than `TREND_MIN_FRAC` of capacity per `TREND_WINDOW_MS` window. The rate behind TTE is measured over the longer `TTE_WINDOW_MS` window and smoothed across windows, and survives the toggle.
- **Units counter**: the raw resource amount, compacted to fit (1.23 / 12.3 / 123 / 1234 / 12.3k).

**Presence.** The Simpit plugin sends a resource message with total 0 and available 0 when the vessel has no such resource, and a channel request forces that message even when nothing changed, so a zero total after a refresh is a definitive "not aboard". The panel records presence per resource type on every vessel message (`resPresence[]`), resets it to unknown on a vessel switch or scene entry, keeps the last known answer across a plain refresh so a Select change does not flash absent meters back, and treats a channel that never answers as absent at `REFRESH_TIMEOUT_MS` (the mod behind it is not installed). **An absent resource draws no meter**: the row is laid out from the visible slots only, so SPCT can carry Solid Fuel and Ablator for every launch and neither costs a column on a craft without them. The slot stays in the configuration and reappears the moment the vessel has some. On the Select screen an absent resource is dimmed and inert, presets skip it, and the ORDER list shows it dimmed. While a refresh is still pending an unresolved slot shows an empty tape with `...` in grey. Simpit only sends on change, so a stale-data flag is not possible on this link; presence and the refresh-pending distinction are what can be known.

**Time-remaining tiers.** Besides the percent bands, EC, Oxygen, Water and Food alert on the time they have left at the current rate, the same warp-corrected estimate the TTE counter shows: caution below `TIME_WARN_S_*`, alarm below `TIME_ALARM_S_*`, with `TIME_HYST_FRAC` to leave. The O2, water and food times match the Annunciator's TAC-LS thresholds. A state raised this way reads `TIME` in the alert strip. The rate estimate floors at about 5.5 hours to empty, so the water and food warn tiers cannot fire from this panel; the Annunciator covers those from consumption rates.

**Alert hysteresis.** Once a meter is in caution or alarm it must move `ALERT_HYST_FRAC` of capacity back across the threshold to leave it, so a level resting on a threshold cannot flip colour on every message.

**Touch.** A tap anywhere on a meter, or on its alert-strip message, opens the Detail screen on that resource. A touch held still on its tape for a second sets a reserve bug there, or clears the bug it landed on; a touch on a bug that moves drags it. The release after a matured hold or a drag does nothing.

**Spacing.** The meters always spread across the full meter area, so the pitch is the area divided by the slot count. The meter itself stays fixed: tape width, stage column and fonts come in two classes, standard for up to nine meters and compact for ten to sixteen, so a meter reads as the same instrument whether it has the screen to itself or shares it with fifteen others.

Sidebar buttons (top to bottom):
- **SEL** — opens the Select screen
- **DATA** — opens the Detail screen
- **TTE** — toggles the counter row between percent and time-to-empty; reverse-videos while engaged
- **CLR BUG** — removes every reserve bug on the vessel (two-line legend, orange guard treatment since it discards pilot-entered state)

**Select** — resource configuration screen. A tap the limit refuses (adding at 16 slots) flashes the slot counter yellow with `MAX` for a moment; there is no floor, since absent resources collapse and CLEAR could always empty the set. Slots are kept in subsystem order as they are added, so the ORDER list shows what the Main screen will draw. Presets, each a set that should all be aboard the craft type it names: **SPCT** (spacecraft: EC, LF, LOx, MP, SF, O2, Food, Water, Ablator), **XPD** (expedition: nuclear/hydrogen deep-space), **SRF** (surface: EC, Stored Charge, Ore, LF, LOx, MP, O2, Food, Water), **ACFT** (aircraft: EC, LF, Intake Air, MP, O2, Food, Water; no oxidizer, a spaceplane is SPCT plus Intake Air), **LSP** (all TAC-LS) and **ADV** (everything modded). Left panel: 5-column grid of all available resources. Right panel: ordered slot list with a **DFLT** key and a CLEAR button. Top row: the six preset keys and a BACK button. Tapping a resource toggles it. Presets replace the current configuration entirely. DFLT makes the current selection, reserve bugs included, the **default layout**: what the panel shows at boot and what a vessel not in memory starts with. It is stored with the vessel memory, the key lights cyan when the selection already is the default, and the slot counter flashes `DFLT SET`. An empty selection cannot be a default, so CLEAR then DFLT drops the stored one (`DFLT CLR`) and the SPCT preset is the default again. CLEAR is also how a vessel is forgotten: leave it with an empty set and its memory record goes, so it starts from the default next time. **Holding CLEAR** for `MEM_CLEAR_HOLD_MS` forgets every vessel, counting down `MEM CLR 3`, `2`, `1` in orange beside the slot count and ending on `MEMORY CLR`; lifting before the end cancels, and the default layout is untouched. The counter area shows `MEM n/20`, how many vessels are remembered. The preset table itself lives in `Resources.ino`, where the default logic reads it. In live mode, a Simpit channel refresh is requested after any configuration change.

**Detail** — numerical readout for a single resource. Left panel: selector column with one button per active slot, dimmed for a resource the vessel does not carry. Right panel: resource name header, followed by data rows: Available, Total, Remaining %, **Rate** (signed, per game second, or per minute or hour when the per-second figure would round to nothing) and **To empty** (labelled **To full** for a waste resource). Resources with stage data (LF, LOx, SF, Xenon, Ablator) show the five rows in both CRAFT and STAGE sections; the rest show CRAFT only. Rate and Time are the same estimate the Main screen's trend arrows and TTE counters use (`Sampling.ino`). A **history trace** in a column right of the rows (when `DETAIL_HISTORY`) shows the resource's level over the last ten minutes on a fixed 0 to 100 percent axis, newest at the right, in the resource colour, with the caution and alarm fractions as faint lines and the reserve bug in cyan; it is captioned with the game time it spans, which under warp is hours, and a time scale beneath is labelled in game time back from now at the left, middle and right ticks, exact across warp changes since each sample records the warp in force. The history is kept per resource type in `Sampling.ino` whatever screen is up, so opening Detail shows what was already accumulating; it resets on a vessel switch or scene entry. A **bug bar** along the bottom shows the resource's reserve bug in cyan with `-10` `-1` `+1` `+10` keys to set it to a precise percent and `CLR` to remove it; the first step on a resource without a bug starts one at the caution fraction.

**EVA Mode** — when KerbalSimpit's `FLIGHT_STATUS_MESSAGE` reports a Kerbal on EVA (the `FLIGHT_IS_EVA` flag), the Main screen hands its content area to a different layout (`ScreenEVA.ino`) for the fixed five — **Electric Charge, EVA Propellant, Oxygen, Food, Water** — and the previous vessel configuration is snapshotted and restored automatically when the Kerbal boards again. The EVA layout is a set of **270° ring gauges**, open at the bottom, filling clockwise from the lower left in the resource colour on a half-brightness track, a dial rather than a progress ring: one large gauge for EVA Propellant, the resource a jetpack is about, and four smaller ones for the rest. The label sits in the gap; inside each ring is the percent counter with its trend arrow and, always shown, the time to empty (the big gauge adds raw units). The limit bands are thin red and yellow arcs just outside the track at the low end of the sweep; a reserve bug is a cyan dot outside them. The sidebar, the alert strip and the gestures are unchanged: tap a gauge for Detail, hold on the ring to set a bug at the angle touched, hold on the bug to clear it, drag it around the arc. The rings are drawn as chains of filled circles on a fixed angular grid, spaced closely enough to read as a smooth band with rounded ends, so a level change repaints only the dots between the old and new end; the bands, too thin for that, are scan-converted pixel-exact by KerbalDisplayCommon's `fillArc`. The three columns are spaced so the white space between the big gauge and the left pair equals the space between the pairs, and the pair rows sit the same distance apart. While on EVA the Select grid, presets, CLEAR and DFLT are locked so no other resources can be added. **EVA Propellant** is only shown/selectable while on EVA; off-EVA its grid cell is blank and inert.

### Resource Slots

The display tracks up to `MAX_SLOTS` (16) resource slots. Each slot holds a `ResourceType` and four float values: `current`, `maxVal`, `stageCurrent`, `stageMax`. Values are zeroed on vessel switch or scene exit.

| Group | Resources | Short labels |
|-------|-----------|-------------|
| Power | Electric Charge, Stored Charge | EC, StC |
| Propellants (native) | Liquid Fuel, Oxidizer, Solid Fuel, Mono Propellant, Xenon Gas, EVA Propellant¹ | LF, LOx, SF, MP, XE, EVA |
| Propellants (CRP) | Liquid Hydrogen, Liquid Methane, Lithium, Intake Air | LH2, LMe, Li, AIR |
| Nuclear (CRP) | Enriched Uranium, Depleted Fuel | EUr, DFu |
| Other | Ore, Ablator | ORE, ABL |
| Life Support (TAC-LS) | Oxygen, Carbon Dioxide, Food, Waste, Water, Liquid Waste | O2, CO2, FD, WST, H2O, LWS |
| Agriculture (CRP) | Fertilizer | FER |

¹ EVA Propellant is only available while a Kerbal is on EVA (see **EVA Mode** above); it is hidden from the grid otherwise.

### Per-Vessel Configuration Memory

Slot configurations, with their reserve bugs, are saved per vessel name in `vesselCache[]` (up to `VESSEL_CACHE_SIZE` = 20 entries, keyed by the first 39 characters of the name `VESSEL_NAME_MESSAGE` reports). On vessel switch the panel recalls the configuration remembered for the new vessel; a vessel not in memory starts from the **default layout** (the pilot's, set with DFLT on the Select screen, else the SPCT preset), so a new craft never inherits whatever the previous vessel happened to show. Presence collapse then hides what the default over-selects. The cache is kept in recency order, so when it is full the vessel longest unused is the one forgotten. On EVA nothing is saved: the slots are the fixed EVA set, not a layout worth remembering under a Kerbal's name.

**Forgetting.** An empty set at a save point (leaving the scene, switching vessel, or the settle timer) removes the vessel's record rather than saving nothing, so CLEAR then leave is "forget this vessel". Holding CLEAR on the Select screen forgets them all at once. `PERSIST_WIPE` at compile time forgets everything, default included.

**Persistence.** The cache, the default layout and the TTE toggle survive a power cycle: they live in the Teensy 4.1's emulated EEPROM (`Persist.ino`), about 1.5 KB of a 4 KB wear-levelled block, with a magic, a schema number and a CRC-16 so a bad or outdated block is discarded rather than loaded. Writes happen on a vessel switch and on leaving the flight scene, where a few milliseconds' hitch cannot be seen, and otherwise once a change (a bug dragged, a layout built, the toggle pressed) has held still for `PERSIST_SETTLE_MS`, so a power cut mid-flight loses at most that long of edits. Only bytes that differ are programmed. Demo mode never writes. `PERSIST_WIPE` forgets everything at the next boot. Bugs are stored as whole percent, which is lossless since every gesture sets whole percent. The SD card was considered and passed over: a card can be absent or corrupt, FAT writes stall, and the bench would then behave differently from the installed unit.

Simpit identifies a vessel by name only, so two craft with the same name share one record and a renamed vessel starts afresh.

---

## Tab Structure

| File | Description |
|------|-------------|
| `KCMk1_ResourceDisp.ino` | `setup()` and `loop()` only |
| `AAA_Config.ino` | All tunable constants (thresholds, modes, slot config) |
| `AAA_Globals.ino` | `ResourceSlot` struct, display objects, Simpit object, screen state, vessel cache helpers (recency-ordered) |
| `Persist.ino` | EEPROM persistence of the vessel memory, the default layout and the TTE toggle: CRC'd image, settle timer, store-now at vessel switch, scene exit and DFLT |
| `Resources.ino` | Resource type definitions, colour map, subsystem groups, limit-band table, alert state with hysteresis, the mission preset table, default-layout loading and group sort |
| `Sampling.ino` | Per-slot rate and trend sampling (total and stage), warp-corrected time-to-empty, time tiers, the alert summary, the level history ring, refresh tracking — shared by Main, EVA and Detail |
| `ScreenMain.ino` | Main tape-meter screen with 4-button left-hand sidebar |
| `ScreenMainStrip.ino` | The Main screen's alert strip and propellant balance indicator, behind a small interface |
| `ScreenEVA.ino` | The Main screen's EVA layout: 270° ring gauges, one large for EVA Propellant and four small |
| `ScreenSelect.ino` | Resource selection screen (grid + presets + order panel + DFLT / CLEAR) |
| `ScreenDetail.ino` | Numerical resource detail screen (craft/stage values per resource) |
| `ScreenStandby.ino` | Standby screen — delegates to `drawStandbySplash()` |
| `TouchEvents.ino` | Touch debounce and gesture dispatch |
| `SimpitHandler.ino` | KerbalSimpit message handler and channel registration; single-resource channels are a table, not a case each |
| `I2CSlave.ino` | I2C slave at 0x11 — packet build/fill, command processing, boot handshake |
| `BootScreen.ino` | Jurassic Park-themed terminal boot sequence |
| `Demo.ino` | Demo mode — sine-wave resource values sweeping the full 0–100% range, plus a scripted presence scenario (every 20 s: nothing absent, then SF and Ablator absent, then those plus MP and Xenon) to exercise the collapse; no KSP connection |

---

## Boot Sequence

The ResourceDisp follows the same deterministic startup handshake as the other KCMk1 panels.

1. Hardware init (display, SD, touch, I2C slave)
2. Boot screen renders (Jurassic Park-themed terminal sequence; header shows live version string)
3. Simpit connects (or demo mode initialises)
4. ResourceDisp builds a status packet and **asserts the INT pin LOW** (`KCM_I2C_INT_PIN`, pin 0)
5. Master reads the 4-byte status packet
6. Master sends a 2-byte command packet with `requestType = 0x2` (PROCEED) — configuration flags can be included in the same packet
7. ResourceDisp receives PROCEED, transitions to Standby screen, enters `loop()`

**Standalone mode:** set `STANDALONE_TEST = true` in `AAA_Config.ino` (as on the other panels) to skip steps 4–7 — the panel proceeds straight past the boot screen into `loop()` with no master connected. The I2C slave is still initialised, so a master can be attached later; standalone only removes the blocking wait for PROCEED. Set `false` for production.

---

## Version History

| Version | Notes |
|---------|-------|
| **3.12.1** | **History trace stays in its box, and gets a time scale.** The newest-sample dot was drawn on the box's last interior column with a 3 px radius, so it overhung the border and the interior clear left its far side behind as a ghost. The plot area is now inset from the border by 4 px, the clear takes the whole box and the border is redrawn with it. Five ticks under the box carry a **time scale** labelled in game time back from now at the ends and the middle, computed from the warp recorded with each sample so it stays right across a warp change; a tick with no sample yet is blank until the buffer reaches it. |
| **3.12.0** | **Level history on the Detail screen.** A column right of the data rows carries a trace of the resource's level over the last ten minutes (`HIST_LEN` samples every `HIST_PERIOD_MS`), on a fixed 0 to 100 percent axis so a night-pass EC dip reads as a shape against the whole tank and the trace never rescales; the caution and alarm fractions are faint lines, the bug a cyan one, and the caption gives the game time spanned, since under warp ten real minutes is hours. The ring is kept per resource type in `Sampling.ino` on every screen, reset on vessel switch and scene entry. Behind `DETAIL_HISTORY`; off, the rows take the full width again. A trial: the rows give up 280 px, which the widest 48 px label and value still clear by 27 px. |
| **3.11.0** | **Slow drains get a time, the strip names the vessel, the master gets the alert picture.** A second, long rate window (`TTE_LONG_WINDOW_MS`, five minutes) takes over when the ten-second one sees nothing, so food and water on a small crew have a time to empty at 1x and their time tiers, which could only ever fire under warp, now work; the Detail Rate row scales to per minute or per hour rather than printing `0.00/s`, the time formatter has a days form (`4d 3h`, `27d`) instead of stopping at `>99h`, and the Detail time row is labelled **To empty**, or **To full** for a waste resource. The alert strip shows the **vessel name** at its right end, cyan when the layout on screen came from vessel memory and grey when it is the default, the Kerbal's name on EVA. The outbound I2C packet carries an **alert summary**: caution, alarm and time-tier flags and the worst resource's type, evaluated screen-independently, so the master or Annunciator can sound resource alarms. The time-tier logic moved to `Sampling.ino` and is shared by Main, EVA and the summary. Small: a `SEL to choose resources` hint under the empty-set message, a boot-sequence line reporting the vessel memory. The README's eviction note was stale since 3.8.0. |
| **3.10.0** | **Vessels can be forgotten, and the memory shows its fill.** CLEAR was documented as the explicit "forget" for a vessel, which held while the cache died with the power; with the cache persisted it left the vessel blank for the session and brought its old layout straight back next time. An empty set at a save point now removes the vessel's record, so CLEAR then leave starts it from the default next time. Holding CLEAR for `MEM_CLEAR_HOLD_MS` on the Select screen forgets every vessel, with a countdown beside the slot count and a release before the end cancelling; the default layout is kept. The counter area shows `MEM n/20`. The Select screen gained the small key-hold state machine this needed, alongside the Main screen's meter hold. |
| **3.9.0** | **A pilot-set default layout, and unknown vessels start from it.** The Select screen gains a **DFLT** key: the current selection, reserve bugs included, becomes the layout the panel shows at boot and the one a vessel not in memory starts with; CLEAR then DFLT drops it and the SPCT preset is the default again. The default is stored in the EEPROM block beside the vessel memory (schema 2, so the block written by 3.8.0 is discarded once). A vessel whose name is not in memory now loads the default instead of keeping whatever the previous vessel showed, which was the previous vessel's own remembered layout. The preset table moved from `ScreenSelect.ino` to `Resources.ino`, so the built-in default and the SPCT key read one table instead of two copies. |
| **3.8.0** | **Vessel memory and the TTE toggle persist across power cycles.** The per-vessel slot cache (types and reserve bugs) and the TTE toggle are stored in the Teensy 4.1's emulated EEPROM by the new `Persist.ino`: a CRC-16'd image with a magic and schema number, written on a vessel switch and on leaving the flight scene, and otherwise once a change has held still for `PERSIST_SETTLE_MS` (30 s). Demo mode never writes; `PERSIST_WIPE` forgets everything at boot. The cache itself is now recency-ordered with fixed-width names, so a full cache evicts the vessel longest unused instead of always the last entry, and nothing is saved while on EVA, where the slots are the fixed EVA set rather than a vessel's layout. |
| **3.7.1** | **EVA gauges re-spaced, bands pixel-exact, centre text cleared whole.** The small gauges grew (ring radius 56 → 70) and the big one gave up a little (190 → 168) so the three columns fit with equal white space, 63 px, between the big gauge and the left pair and between the pairs; the pair rows are the same distance apart and centred in the content height. The limit bands are now ring segments filled by the new `fillArc` in KerbalDisplayCommon 3.9.0 (the driver has no arc primitive; its hardware curve draws whole quadrants only), so their edges and ends are crisp instead of the beaded thick-line chords. The centre text clears the whole inner disc before redrawing: on the small gauges the percent group and its alarm tile are wider than the inscribed square that used to be cleared, which left a third-digit ghost, tile edges and arrow remnants behind. |
| **3.7.0** | **EVA layout: ring gauges.** On EVA the Main screen hands its content area to `ScreenEVA.ino`: a large 270° ring gauge for EVA Propellant and four small ones for EC, O2, Food and Water, open at the bottom with the label in the gap, filling clockwise from the lower left on a half-brightness track, bands outside the track, bug as a cyan dot, time to empty always shown. Same sidebar, strip and gestures; the touch code asks the Main screen for geometry (`mainHitTest` / `mainLevelAt`) and gets tape rows or ring angles depending on the layout. |
| **3.6.0** | **Presence survives a refresh, rates survive a redraw, new alarms flash, no slot floor, strip in its own tab.** A plain refresh no longer resets presence to unknown, so a Select change does not flash absent meters back as `...`; only a vessel switch or scene entry does. The sampling arrays and alarm timers are discarded only when the slot sequence actually changed, so a redraw for a presence change or a vessel-name message keeps the rate estimate. A tile that has just turned alarm flashes in the strip for three seconds. The Detail selector dims a resource the vessel does not carry and follows presence changes. The four-slot floor is gone. The alert strip and balance indicator moved to `ScreenMainStrip.ino` behind `stripReset` / `stripSetSlot` / `stripUpdate` / `stripHitTest`. `tools/host_compile.py` now covers all three panels. |
| **3.5.0** | **Presence-driven meters, mission presets, CLR BUG, fixed order, bug figures, strip taps, Detail bug bar, time tiers.** The Simpit plugin reports a resource the vessel lacks as total 0 / available 0 and resends on request, so presence is now known per resource: an absent one draws no meter, is inert on Select, is skipped by presets and dimmed in ORDER, and reappears the moment the vessel has some. Presets are renamed and retargeted so every member is aboard the craft type named: SPCT, XPD, SRF (surface: replaces VEH), LSP, ACFT (aircraft: replaces AIR and drops LOx), ADV. DFLT leaves the sidebar (SPCT on Select is the same set) and **CLR BUG** takes its place. Within a group slots follow the Select grid order, so a layout is identical on every vessel. The reserve bug shows its percent beside the index and can be set to a precise value from a **bug bar** on the Detail screen. A tap on an alert-strip message opens Detail on that resource. EC, O2, water and food gain **time-remaining tiers** from the TTE estimate, aligned with the Annunciator's TAC-LS times, reported as `TIME` in the strip. |
| **3.4.1** | **Resource palette audit.** No fill uses an alert colour any more (Solid Fuel was the alarm red, Electric Charge the caution yellow, CO2 a dark red beside its own red band), none uses a signalling colour (Water was the pilot-entry cyan, Oxygen a near-white silver), and every fill survives the stage column's half brightness (Oxidizer's pure blue halved to navy). Colours now run in a family per subsystem group: gold for power, orange/french blue/crimson for the rocket propellants, greens for RCS, steel blue and aqua for O2 and water, earth tones for waste. Four colours were added to KerbalDisplayCommon 3.8.0 for it: `TFT_BRICK` for Solid Fuel, `TFT_PLUM` for CO2, `TFT_STRAW` for Liquid Waste and `TFT_LIME` for Stored Charge. |
| **3.4.0** | **Alert strip, reserve bugs, propellant balance, rate rows, and a host compile check.** An EICAS-style **alert strip** across the top lists every meter in caution or alarm, worst first, plus `REFRESHING` while a channel refresh is pending; a slot that has not yet answered a refresh reads `...` rather than `---`. Alert thresholds gained **hysteresis** (`ALERT_HYST_FRAC`) so a level resting on a threshold no longer flickers. A tap on a tape sets a cyan **reserve bug** that puts the meter in caution when crossed; bugs are saved with the vessel's slot memory. The **propellant balance** indicator at the strip's right end is Apollo's OXID UNBAL for KSP's 9:11 LF:LOx ratio. A tap on a meter's foot opens **Detail** on that resource, and Detail gained **Rate** and **Time** rows for both craft and stage from a new shared `Sampling.ino`; TTE and rate are now corrected to **game time** using the warp index in `FLIGHT_STATUS_MESSAGE`. The Select screen flashes the slot counter when a limit refuses a tap, and keeps slots in subsystem order as they are added. The Simpit handler's fourteen single-resource cases collapsed into a table. `tools/host_compile.py` compiles the sketch on the host the way the IDE does, prototypes hoisted, against stubs of the display, touch and Simpit libraries, so the class of error the IDE finds is caught before a push. |
| **3.3.0** | **Tape meters replace the bar chart.** The filled bars' only warning was the percentage label changing colour — a 12 px target at sixteen bars — and the limits were invisible until crossed. Each slot is now a Shuttle-style tape meter: a narrow thermometer column with the resource's caution and alarm fractions painted as a red/yellow **limit band on the scale itself**, tick marks every 5%, and a frame that takes the alert colour on breach. Thresholds come from a per-resource table (`resLimits()`): consumables alert low at the cross-panel `KCM_RES_LOW_WARN_FRAC` / `KCM_EC_LOW_ALARM_FRAC` fractions, so a meter goes yellow and red where the Annunciator does (previously a fixed 30/10 that matched nothing); waste-type resources (CO2, Waste, Liquid Waste, Depleted Fuel) alert on filling up at the Annunciator's 80/95; Ore and Intake Air have no bands. **The TOTL/STG toggle is gone**: resources with a stage channel split the tape into a wide total column and a narrow half-brightness stage column with a white line at its level, so both are always visible and there is nothing left for a mode key to reveal. Its sidebar position goes to a **TTE** key at the bottom of the column, which swaps the counter row from percent to time-to-empty at the current rate (time-to-full for waste), measured over `TTE_WINDOW_MS` windows and smoothed. Each meter gained a **units counter** and a **trend arrow** beside the percent counter (the DATA screen was the only place with real numbers, and 50% of a small monopropellant tank is a few units). Alarm is now white-on-red across the counter cell, the InfoDisp/Annunciator alarm treatment. Meters spread across the full width in **subsystem order** with a bracketed group label and a divider between groups; the sort is applied to the slot list on entry to Main so Detail and the Select order list agree. A slot with zero capacity shows `---` in grey rather than a 0% alarm. `LOW_RES_THRESHOLD` (reserved, unused) is removed. |
| **3.0.1** | Audit batch C: EVA-mode state reset on EVA exit (stale bars no longer persist into the next vessel), duplicate-logic consolidation, and dead-code cleanup. Built against KerbalDisplayCommon 3.1.2. |
| **3.2.1** | **A panel booting into a running flight no longer sits on standby.** `flightScene` was only ever set by `SCENE_CHANGE_MESSAGE`, which Simpit sends as an *event* — there is no way to ask for the current scene. A panel that boots (or whose USB re-enumerates) while a flight is already running therefore never hears it and sits on the standby screen until the pilot happens to change scene or vessel. Since Simpit sends `FLIGHT_STATUS` only from a flight scene, receiving it while the panel believes it is not in one is proof that the transition was missed, so the panel now adopts the scene there. Both routes go through one new `enterFlightScene()` so they cannot drift apart. The hook sits at the end of the `FLIGHT_STATUS` handler rather than earlier, so the message's own vessel data is already applied, and the slot zero + channel refresh the scene entry already did now run on both routes. Shared with the InfoDisp and Annunciator, which had the same defect. |
| **3.2.0** | **Sidebar moved to the left edge.** On panel B1 this display sits outboard of Info Display 2, whose own sidebar is on its right edge, so the two button columns previously sat a full screen-width apart with this one at B1's outboard corner — the longest reach on the panel. They now meet at the boundary between the two screens and read as one control cluster near the middle of B1. Content occupies `[CONTENT_X, SCREEN_W)`: the Y-axis strip first, then the bars. Everything downstream already derived from `barX()` and `drawAxis()`, so the offset is stated once; `sbDivX()` puts the 1 px divider rule on the sidebar's inboard edge, against the content. Verified for every slot count from 1 to 16 that the bars stay clear of both the axis and the sidebar and inside the right edge. The Select and Detail screens have no sidebar and are unchanged — their BACK keys stay in the top-right corner. |
| **3.1.0** | **Button colour scheme brought in line with the InfoDisp sidebar.** Chrome is now achromatic — white-on-black with a grey border — so the panel's colour budget belongs entirely to the bars and their level thresholds. The mode key had filled `TFT_DARK_GREEN` for TOTAL and `TFT_CORNELL` for STAGE, and both of those are *bar identity colours on this same panel* (MonoPropellant and CO2 respectively, per `resColor()`), so the key wore two resources' colours while sitting beside their bars; `TFT_CORNELL` is also a red (#B51C19), a strong one to park permanently next to percentage labels that turn red below 10%. DFLT / SEL / DATA filled navy, which signals nothing, and the Select and Detail BACK keys filled green. All now draw as plain keys. The mode key keeps the one genuine state: TOTAL draws like any other key, STAGE reverse-videos (black on grey), so it reads as "moved off the default" as well as naming the mode. **CLEAR keeps an affordance but not an alarm** — orange legend and border on off-black, the guard treatment the InfoDisp Ascent Autopilot ARM button already uses for a control with consequences, rather than a red fill. Borders are now load-bearing rather than decorative: with a black fill on a black screen they are what makes a key visible, so the two BACK keys gained the grey border they previously did without. Per-resource bar and Select-cell colours are data, not chrome, and are unchanged. |
| **3.0.0** | Hardware rev 2: Teensy 4.1 / LT7683 (RA8876-compatible) 1024×600 TFT via `KCM_TFT` / FT5316 capacitive touch / Wire2 (`KCM_I2C_BUS`). Requires KerbalDisplayCommon ≥ 3.0.0 and KerbalDisplayAudio 1.1.0. All screens relaid to 1024×600. Main screen: bars always render the resource colour (percentage text carries the level threshold); percentage-flicker fix. Default STD set is 9 (EC, LF, LOx, MP, SF, O2, Food, Water, Ablator). Added EVA Propellant and EVA mode (fixed EC/EVA/O2/Food/Water bar set driven by `FLIGHT_STATUS_MESSAGE`, with the Select grid locked). Standalone-test flag added. |
| **1.3.0** | I2C slave interface and boot handshake with master (Phase 3). I2C constants consolidated to `KCMk1_SystemConfig.h`. Touch count filter changed to `!= 1`. Touch filter constants alias `KCM_TOUCH_*`. Boot screen header shows live version string (sketch + KDC + KDA) via `snprintf`. `switchToScreen()` now records `lastScreenSwitch` timestamp. KDA dependency clarified as direct (not a KDC sub-dependency). Updated to KerbalDisplayCommon 2.1.0 and KerbalDisplayAudio 1.0.1. |
| **1.2.0** | KerbalSimpit integration for live resource telemetry. Per-vessel configuration memory (`vesselCache[]`). Simpit channel refresh on vessel switch and scene entry. |
| **1.1.0** | Select screen presets (STD, XPD, VEH, LSP, AIR, ADV). Detail screen with CRAFT/STAGE sections. Stage data support for LF, LOx, SF, Xenon, Ablator. |
| **1.0.0** | Initial release. Main bar graph with 4-button sidebar, demo mode, 16-slot configuration. |

---

## Notes

- **`debugMode`** defaults to `false`. Set `true` during development for touch coordinates and Simpit message logging.
- **`demoMode`** defaults to `false` (live Simpit). Set `true` for bench testing without KSP.
- **ARP mod** is required for most resource channels. Without it, meters show `---` (no capacity reported).
- **Stage data** is only available for resources with dedicated Simpit stage channels (LF, LOx, SF, Xenon, Ablator). All others mirror vessel totals in the STAGE section and suppress it on the Detail screen.
- **Vessel cache eviction** — the cache is kept in recency order (a save or recall moves its record to the front), so when `VESSEL_CACHE_SIZE` is exceeded the vessel longest unused is the one forgotten. See Per-Vessel Configuration Memory.

Licensed under the GNU General Public License v3.0.
Final code written by J. Rostoker for Jeb's Controller Works.
