# KCMk1_ResourceDisp

**Kerbal Controller Mk1 — Resource Display Panel Sketch** · v3.4.1
Teensy 4.1 firmware for the KSP resource monitoring display module.
Part of the KCMk1 controller system. Operates as an I2C slave under a Teensy 4.1 master.

---

## Overview

The Resource Display is a 1024×600 touchscreen panel that presents real-time KSP resource telemetry sourced from KerbalSimpit. It runs on a Teensy 4.1 and receives telemetry over USB serial from a running KSP instance. The panel tracks up to 16 resource slots simultaneously and supports per-vessel configuration memory for up to 20 vessels per session.

The panel provides four screens — Standby, Main, Select, and Detail — navigated by touch. The Main screen is the primary operational view, displaying resource levels as a row of Shuttle-style tape meters grouped by subsystem. The Select screen allows the user to configure which resources are monitored and load preset configurations. The Detail screen provides a numerical breakdown of a single resource by craft and stage values.

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
| KerbalDisplayCommon | ≥ 3.8.0 | Display primitives, fonts, BMP loader, system utils; pulls in KCM_Display (`KCM_TFT`) + SystemConfig |
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
| `DISPLAY_ROTATION` | `AAA_Config.ino` | `0` | `0` = normal (connector at bottom), `2` = 180° (inverted mounting). |
| `RES_WARN_FRAC` | `AAA_Config.ino` | `KCM_RES_LOW_WARN_FRAC` (0.20) | Caution (yellow) band: a consumable meter alerts below this fraction of capacity. Aliases the cross-panel constant so it matches the Annunciator PROP LOW / RCS LOW tier. |
| `RES_ALARM_FRAC` | `AAA_Config.ino` | `KCM_EC_LOW_ALARM_FRAC` (0.05) | Alarm (red) band: a consumable meter alerts below this fraction. Aliases the cross-panel constant so it matches the Annunciator red tier. |
| `WASTE_WARN_FRAC` / `WASTE_ALARM_FRAC` | `AAA_Config.ino` | `0.80` / `0.95` | Waste-type meters (CO2, Waste, Liquid Waste, Depleted Fuel) alert on filling up, at these fractions full. Match the Annunciator `TACLS_WASTE_*` fractions; keep in step by hand, there is no shared define yet. |
| `TREND_WINDOW_MS` | `AAA_Config.ino` | `2000` | Trend-arrow sample window. Each meter compares its value against the value at the start of the window. |
| `TREND_MIN_FRAC` | `AAA_Config.ino` | `0.0005` | Movement across one window, as a fraction of capacity, needed to show a rising/falling arrow, and the deadband below which the TTE rate reads as zero. Keeps both quiet on per-message jitter. |
| `TTE_WINDOW_MS` | `AAA_Config.ino` | `10000` | Rate sample window behind the TTE counter and the Detail Rate/Time rows. Longer than the trend window so slow drains register; with the deadband above, the slowest reportable drain is about 5.5 hours to empty. Rates are measured in real time and converted to game time with the warp index from `FLIGHT_STATUS_MESSAGE`; sampling restarts on a warp change. |
| `ALERT_HYST_FRAC` | `AAA_Config.ino` | `0.01` | Hysteresis on every caution/alarm threshold, as a fraction of capacity. |
| `BUG_CLEAR_TOL` | `AAA_Config.ino` | `0.03` | A tape tap within this fraction of an existing reserve bug clears it instead of moving it. |
| `REFRESH_TIMEOUT_MS` | `AAA_Config.ino` | `3000` | How long after a channel refresh request a silent slot is drawn as `...` (awaiting) before it becomes `---` (not aboard). |
| `MIN_SLOTS` | `KCMk1_ResourceDisp.h` | `4` | Minimum number of active resource slots (enforced by `removeResource`). |
| `MAX_SLOTS` | `KCMk1_ResourceDisp.h` | `16` | Maximum number of active resource slots. |
| `DEFAULT_SLOT_COUNT` | `KCMk1_ResourceDisp.h` | `9` | Number of slots loaded by `initDefaultSlots()` (STD preset). |
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
| 1 | Flags | Bit 0: `simpitConnected`  Bit 1: `flightScene`  Bit 2: `demoMode`  Bits 3–7: reserved (0) |
| 2 | `slotCount` | Number of currently active resource slots (0–16) |
| 3 | Reserved | `0x00` |

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

**Alert strip** (top of the content area) — an EICAS-style message line listing every meter in caution or alarm, worst first: `SF LOW` (or `CO2 HIGH` for a waste product) white-on-red for an alarm, `MP CAUT` in yellow for a caution, `LF BUG` in yellow when only a reserve bug is crossed, and `REFRESHING` in white while a channel refresh is pending. Messages that do not fit collapse to `+N`. At its right end, when both Liquid Fuel and Oxidizer are on the panel, the **propellant balance** indicator: Apollo's OXID UNBAL meter for KSP's 9:11 LF:LOx burn ratio. A centre-zero bar deflects toward whichever propellant is in surplus, and a counter says by how many units (`LOx +123`). Stage quantities are used when both have them, else vessel totals.

Each meter, top to bottom:

- **Group label.** Meters are laid out in subsystem order — PWR, PROP, NUC, MISC, LS, AGR — with a bracketed label over each run and a 1 px divider between runs. The ordering is applied to the slot list itself on every entry to the Main screen (`sortSlotsByGroup()`), so the Detail selector and the Select order list agree with the meters. Within a group, selection order is kept.
- **The tape.** A narrow thermometer column, filled in the resource's fixed colour to the vessel total. On its left is a **limit-band column** with the resource's alarm and caution fractions painted red and yellow on the scale, visible whether or not the level is in them. Consumables band at the bottom (below `RES_WARN_FRAC` / `RES_ALARM_FRAC`); waste-type resources band at the top (above `WASTE_WARN_FRAC` / `WASTE_ALARM_FRAC`); Ore and Intake Air have no bands. Tick marks on the right edge: major every 10%, minor every 5%. The frame is grey when nominal and turns yellow or red on breach.
- **Split column for stage.** For the five resources with a separate stage channel (LF, LOx, SF, Xenon, Ablator), the tape is split the way the Shuttle PRPLT QTY meter carried its pointers side by side: the wide left column is the vessel total in the resource colour, the narrow right column is the *active stage* in a half-brightness shade of the same colour, and a white line across the whole tape marks the stage level so it reads against the ticks even where the narrow column is only a few pixels wide. Both values are always visible; there is no mode to toggle. The counters follow the total; stage figures are on the DATA screen. Resources without a stage channel draw one full-width column, which is itself the cue that there is no separate stage figure.
- **Reserve bug.** A tap on the tape sets a cyan index at that level (cyan is this project's colour for pilot-entered values), with a cyan mark on the band. The meter goes to caution when the level crosses it, below for a consumable and above for a waste product; a bug never raises an alarm. A tap within `BUG_CLEAR_TOL` of an existing bug clears it. Bugs are saved with the vessel's slot memory.
- **Label**, then the **counter row**: the percent of capacity, or with the TTE key engaged the **time to empty** at the current rate (`4:35` under ten minutes, `42m` under an hour, `5.5h` beyond; time to *full* for waste-type resources; `---` while steady, filling, or draining too slowly to measure). Coloured by state: white nominal, yellow caution, white-on-red alarm. A **trend arrow** two spaces to its right shows rising or falling (counter, gap and arrow slot are centred in the cell as one group, the arrow slot reserved so the number never shifts) while the value moves more than `TREND_MIN_FRAC` of capacity per `TREND_WINDOW_MS` window. The rate behind TTE is measured over the longer `TTE_WINDOW_MS` window and smoothed across windows, and survives the toggle.
- **Units counter**: the raw resource amount, compacted to fit (1.23 / 12.3 / 123 / 1234 / 12.3k).

A slot whose capacity is zero shows an empty tape and, in grey, `...` while a channel refresh is still pending for it (`REFRESH_TIMEOUT_MS`) or `---` once it is not going to answer — the resource is not aboard — rather than a 0% alarm. Simpit only sends a resource message when a value changes, so a stale-data flag is not possible on this link; the refresh-pending distinction is what can be known.

**Alert hysteresis.** Once a meter is in caution or alarm it must move `ALERT_HYST_FRAC` of capacity back across the threshold to leave it, so a level resting on a threshold cannot flip colour on every message.

**Touch.** A tap on a meter's label or counter rows opens the Detail screen on that resource. A tap on its tape rows sets or clears a reserve bug.

**Spacing.** The meters always spread across the full meter area, so the pitch is the area divided by the slot count. The meter itself stays fixed: tape width, stage column and fonts come in two classes, standard for up to nine meters and compact for ten to sixteen, so a meter reads as the same instrument whether it has the screen to itself or shares it with fifteen others.

Sidebar buttons (top to bottom):
- **DFLT** — resets to the default 9-slot STD configuration (disabled while on EVA)
- **SEL** — opens the Select screen
- **DATA** — opens the Detail screen
- **TTE** — toggles the counter row between percent and time-to-empty; reverse-videos while engaged

**Select** — resource configuration screen. A tap the limits refuse (adding at 16 slots, removing at the 4-slot floor) flashes the slot counter yellow with `MAX` or `MIN` for a moment. Slots are kept in subsystem order as they are added, so the ORDER list shows what the Main screen will draw. Left panel: 5-column grid of all available resources. Right panel: ordered slot list with a CLEAR button. Top row: preset buttons (STD, XPD, VEH, LSP, AIR, ADV) and a BACK button. Tapping a resource toggles it. Presets replace the current configuration entirely. In live mode, a Simpit channel refresh is requested after any configuration change.

**Detail** — numerical readout for a single resource. Left panel: selector column with one button per active slot. Right panel: resource name header, followed by data rows: Available, Total, Remaining %, **Rate** (units per game second, signed) and **Time** (to empty, or to full for waste). Resources with stage data (LF, LOx, SF, Xenon, Ablator) show the five rows in both CRAFT and STAGE sections; the rest show CRAFT only. Rate and Time are the same estimate the Main screen's trend arrows and TTE counters use (`Sampling.ino`).

**EVA Mode** — when KerbalSimpit's `FLIGHT_STATUS_MESSAGE` reports a Kerbal on EVA (the `FLIGHT_IS_EVA` flag), the Main screen switches to a fixed five-meter set — **Electric Charge, EVA Propellant, Oxygen, Food, Water** — and the previous vessel configuration is snapshotted and restored automatically when the Kerbal boards again. While on EVA the Select grid, presets, CLEAR and DFLT are locked so no other resources can be added. **EVA Propellant** is only shown/selectable while on EVA; off-EVA its grid cell is blank and inert.

### Resource Slots

The display tracks between `MIN_SLOTS` (4) and `MAX_SLOTS` (16) resource slots. Each slot holds a `ResourceType` and four float values: `current`, `maxVal`, `stageCurrent`, `stageMax`. Values are zeroed on vessel switch or scene exit.

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

Slot configurations are saved per vessel name in `vesselCache[]` (up to `VESSEL_CACHE_SIZE` entries). On vessel switch, the panel attempts to recall the previous configuration for the new vessel. If not found, the current configuration is retained. On scene exit, the current configuration is saved for the active vessel. The cache is held in RAM only — it does not persist across power cycles.

---

## Tab Structure

| File | Description |
|------|-------------|
| `KCMk1_ResourceDisp.ino` | `setup()` and `loop()` only |
| `AAA_Config.ino` | All tunable constants (thresholds, modes, slot config) |
| `AAA_Globals.ino` | `ResourceSlot` struct, display objects, Simpit object, screen state, vessel cache helpers |
| `Resources.ino` | Resource type definitions, colour map, subsystem groups, limit-band table, alert state with hysteresis, slot initialisation and group sort |
| `Sampling.ino` | Per-slot rate and trend sampling (total and stage), warp-corrected time-to-empty, refresh tracking — shared by Main and Detail |
| `ScreenMain.ino` | Main tape-meter screen with 4-button left-hand sidebar |
| `ScreenSelect.ino` | Resource selection screen (grid + presets + order panel) |
| `ScreenDetail.ino` | Numerical resource detail screen (craft/stage values per resource) |
| `ScreenStandby.ino` | Standby screen — delegates to `drawStandbySplash()` |
| `TouchEvents.ino` | Touch debounce and gesture dispatch |
| `SimpitHandler.ino` | KerbalSimpit message handler and channel registration; single-resource channels are a table, not a case each |
| `I2CSlave.ino` | I2C slave at 0x11 — packet build/fill, command processing, boot handshake |
| `BootScreen.ino` | Jurassic Park-themed terminal boot sequence |
| `Demo.ino` | Demo mode — sine-wave resource values sweeping the full 0–100% range, no KSP connection |

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
| **3.4.1** | **Resource palette audit.** No fill uses an alert colour any more (Solid Fuel was the alarm red, Electric Charge the caution yellow, CO2 a dark red beside its own red band), none uses a signalling colour (Water was the pilot-entry cyan, Oxygen a near-white silver), and every fill survives the stage column's half brightness (Oxidizer's pure blue halved to navy). Colours now run in a family per subsystem group: gold for power, orange/french blue/crimson for the rocket propellants, greens for RCS, steel blue and aqua for O2 and water, earth tones for waste. Three colours were added to KerbalDisplayCommon 3.8.0 for it: `TFT_BRICK` for Solid Fuel, `TFT_PLUM` for CO2 and `TFT_STRAW` for Liquid Waste; Stored Charge moved to purple, off the gold Electric Charge uses. |
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
- **Vessel cache eviction** — when `VESSEL_CACHE_SIZE` is exceeded, the last cache entry (index `VESSEL_CACHE_SIZE - 1`) is overwritten (simple last-slot eviction). No LRU tracking.

Licensed under the GNU General Public License v3.0.
Final code written by J. Rostoker for Jeb's Controller Works.
