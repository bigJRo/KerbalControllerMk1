# Porting the KCMk1 Displays to the 7" TFT Driver (hardware rev 2)

Branch: `claude/display-7inch-tft-upgrade-yvadv7`
Board: **KC-01-1911 / KC-01-1912 "7\" TFT Display Driver"** (schematic V2.0, J. Rostoker)

This document is the source of truth for moving the Annunciator, Info, and
Resource panels from the rev-1 display stack to the new 7" TFT carrier board.

---

## 1. What changed in the hardware

| Subsystem | rev 1 (old) | rev 2 (this board) |
|---|---|---|
| MCU | Teensy 4.0 | **Teensy 4.1** |
| Display controller | RA8875, SPI, 800×480 | **RA8876**, 16-bit 8080 **parallel**, **1024×600** (BuyDisplay ER-TFTM070-6) |
| Display library | sumotoy `RA8875` | **`wwatson4506/TeensyRA8876-8080`** (`RA8876_t41_p`, FlexIO3) + `TeensyRA8876-GFX-Common` |
| Fonts | sumotoy `tFont` (proportional 1bpp) | **ILI9341_t3** (`PaulStoffregen/ILI9341_fonts`) |
| Touch | GSL1680F on Wire1 (16/17) | **FT5316** (FT5x06 family) on **software I2C** (pins 4/5) |
| Audio | `tone()` buzzer on pin 9 | `tone()` buzzer on **TONE / pin 2** + **DFPlayer Mini** (Serial2, sampled) |
| Asset storage | SD over SPI (CS 5) | **Teensy 4.1 on-board SD** (SDIO / `BUILTIN_SDCARD`) |
| Backlight | RA8875 internal | **BL_CTRL on pin 9** |
| Slave I2C (to master) | Wire (18/19) | **Wire2** (24/25) |
| INT to master | pin 2 | **INT_BUS / pin 0** |

---

## 2. The key finding: the board targets a specific library

The TFT data bus on this board is **not an arbitrary pin choice** — DB0–DB15,
`/WR`, and `/RD` are exactly the **Teensy 4.1 FlexIO3 8080 parallel pin set** used
by `wwatson4506/TeensyRA8876-8080`:

| Signal | Schematic pin | FlexIO3 driver default | Match |
|---|---|---|---|
| D0–D15 | 19,18,14,15,40,41,17,16,22,23,20,21,38,39,26,27 | identical | ✅ |
| /WR | 36 | 36 | ✅ |
| /RD | 37 | 37 | ✅ |
| /CS | 34 | 11 (default) | remapped (GPIO) |
| RS (=DC) | 33 | 13 (default) | remapped (GPIO) |
| /RESET | 35 | 12 (default) | remapped (GPIO) |

The data/strobe lines are FlexIO-bound (hardware-fixed); only CS/RS/RESET are
plain GPIO and were moved off the library defaults. **Conclusion: drive this
board with `RA8876_t41_p` over FlexIO3 — that is what it was designed for.**

---

## 3. Full Teensy 4.1 pin map (transcribed from the schematic)

```
DISPLAY (RA8876, FlexIO3 16-bit 8080 — owned by the driver)
  DB0=19  DB1=18  DB2=14  DB3=15  DB4=40  DB5=41  DB6=17  DB7=16
  DB8=22  DB9=23  DB10=20 DB11=21 DB12=38 DB13=39 DB14=26 DB15=27
  /WR=36  /RD=37
DISPLAY CONTROL (GPIO)
  /CS=34  RS/DC=33  /RESET=35  WAIT=32  INT=31  BL_CTRL=9
TOUCH (FT5316, software I2C)
  SCL_LOCAL=4  SDA_LOCAL=5  CTP_/RST=3  CTP_INT=6   (addr 0x38)
AUDIO
  TONE buzzer=2   DFPlayer: AUDIO_RX=7 (RX2)  AUDIO_TX=8 (TX2)  [Serial2]
MODULE / SLAVE I2C (to master controller) — Wire2
  SCL_BUS=24 (SCL2)  SDA_BUS=25 (SDA2)  INT_BUS=0  RST=1
SD CARD
  Teensy 4.1 on-board socket (SDIO) — SD.begin(BUILTIN_SDCARD)
```

All of the above are defined in
`libraries/KCMk1_SystemConfig/src/KCMk1_SystemConfig.h`.

---

## 4. Software architecture

```
sketch (Annunciator / InfoDisp / ResourceDisp)
  ├── KCMk1_SystemConfig   pins, resolution, I2C addrs, bus speed  [updated]
  ├── KCM_Display          KCM_TFT = RA8876_t41_p + kcmDisplayBegin()  [new]
  │     └── (dep) TeensyRA8876-8080 + TeensyRA8876-GFX-Common + ILI9341_fonts
  ├── KCM_Touch            FT5316 software-I2C driver (TouchResult API)  [new]
  ├── KerbalDisplayAudio   tone() state machine (AUDIO_PIN=2) + KCM_DFPlayer  [updated]
  └── KerbalDisplayCommon  UI toolkit — buttons/text/formatters/BMP/bodies  [pending migration]
```

- **`KCM_TFT`** is a controller-agnostic typedef (`= RA8876_t41_p`). The rest of
  the firmware refers only to `KCM_TFT`, so a future controller swap is one line.
- Construct in each sketch's `AAA_Globals`:
  `KCM_TFT infoDisp(KCM_TFT_RS, KCM_TFT_CS, KCM_TFT_RESET);`
  then `kcmDisplayBegin(infoDisp, TFT_BLACK);` in `setup()`.

---

## 5. Dependencies (install into the Arduino libraries path)

External git is not reachable from this environment, so these are **not vendored**
into the repo — install them on the build machine:

| Library | Source | Notes |
|---|---|---|
| TeensyRA8876-8080 | `github.com/wwatson4506/TeensyRA8876-8080` | FlexIO3 8080 driver, class `RA8876_t41_p` |
| TeensyRA8876-GFX-Common | `github.com/wwatson4506/TeensyRA8876-GFX-Common` | Adafruit_GFX-style API base |
| ILI9341_fonts | `github.com/PaulStoffregen/ILI9341_fonts` | ILI9341_t3 font format |

After install, in `RA8876_Config_8080.h` confirm the data pins (D0=19, /WR=36,
/RD=37 — already correct) and either edit the CS/DC/RESET defaults to 34/33/35 or
rely on the constructor args (this firmware passes them via `KCM_TFT(...)`).
Bus width 16, start at `KCM_TFT_BUS_SPEED_MHZ = 20` and raise once stable.

---

## 6. Status — what's done vs remaining

**Done — foundation:**
- [x] `KCMk1_SystemConfig.h` — Teensy 4.1, 1024×600, full pin map, Wire2 slave bus, bus speed.
- [x] `KCM_Display` — `KCM_TFT` typedef + `kcmDisplayBegin()` glue over `RA8876_t41_p`.
- [x] `KCM_Touch` — FT5316 software-I2C driver, API-compatible with the old touch surface
      (orientation + protocol confirmed against the vendor demo).
- [x] `KerbalDisplayAudio` — `AUDIO_PIN` → TONE/pin 2; added `KCM_DFPlayer` (Serial2).

**Done — fonts + common library:**
- [x] **Fonts converted** sumotoy `tFont` → ILI9341_t3 (`fonts_ili/`, all 12 fonts),
      via `tfont_to_ili9341.py` with per-glyph round-trip verification.
- [x] **`KerbalDisplayCommon` migrated** (v3.0.0): `KCM_TFT` type, ILI9341_t3 fonts,
      `getFontCharWidth` via fetchbits, `drawBMP` → `writeRect`, `setupSD` →
      `BUILTIN_SDCARD`, GSL1680 touch removed (now `KCM_Touch`). Not yet compiled —
      needs the RA8876 libraries installed (see Dependencies).

**Remaining:**
- [ ] **Port + redesign the Annunciator** for 1024×600 (chosen layout strategy:
      full redesign, not letterbox): `AAA_Globals` (KCM_TFT object), `I2CSlave`
      (Wire2 + INT_BUS pin 0), audio repoint, touch via KCM_Touch, and new screen
      geometry for `ScreenMain/ScreenSOI/ScreenStandby/BootScreen/CautionWarning/TouchEvents`.
- [ ] Regenerate BMP assets at 1024×600 (standby splash; SOI globe/KASA images).
- [ ] Then repeat the sketch port for InfoDisp and ResourceDisp.

---

## 7. First-power-up bring-up checklist (suggested order)

1. **Display**: minimal sketch — `kcmDisplayBegin()` + `fillScreen()` color cycle +
   a few `drawRect`/`print`. Confirm the panel syncs (no tearing/offset). If it
   doesn't sync, the suspect is the FlexIO bus speed or the module's PLL/timing —
   compare against the BuyDisplay ER-TFTM070-6 demo's init.
2. **Backlight**: confirm BL_CTRL (pin 9) turns the panel on/off.
3. **Touch**: `setTouchDebug(true)`; confirm the FT5316 ACKs and prints raw
   coordinates. Set `KCM_CTP_SWAP_XY / INVERT_X / INVERT_Y` so a touch lands under
   the finger.
4. **SD**: `SD.begin(BUILTIN_SDCARD)`; draw a 1024×600 test BMP.
5. **Audio**: `tone()` on pin 2 buzzer; DFPlayer `playTrack(1)` from its microSD.
6. **Slave I2C**: bring up the Wire2 slave + INT_BUS handshake with the master.
7. Only then layer the full Annunciator UI back on.

---

## 8. Authoritative RA8876 init (from the BuyDisplay ER-TFT070A2-6 vendor demo)

The `wwatson4506` library owns the init, but if the panel does not sync on first
power-up, these are the **confirmed** values for this exact module — paste/match
them into the driver's config:

```
Crystal (OSC_FREQ)        = 10 MHz
SCLK (pixel clock)        = 50 MHz     (PLL OD=2, R=5, N=70)
CCLK (core)               = 100 MHz    (PLL OD=2, R=5, N=100)
MCLK (SDRAM)              = 100 MHz    (PLL OD=2, R=5, N=100)

1024x600 panel timing:
  HBPD (h back porch)   = 144      VBPD (v back porch) = 20
  HFPD (h front porch)  = 160      VFPD (v front porch)= 12
  HSPW (hsync width)    = 20       VSPW (vsync width)  = 3
Polarity:
  PCLK  = falling edge
  HSYNC = active low
  VSYNC = active low
  DE    = active high
Colour: 16bpp; main image @ SDRAM addr 0; interface = 8080 parallel.
```

Touch (FT5316), confirmed from the demo:
- I2C address 0x38; no init register writes needed (reset pulse only).
- `TD_STATUS` reg 0x02 low nibble = point count; points from reg 0x03, 6 bytes each.
- INT is active-low. Raw→display mapping inverts both axes: `(LCD_W - x, LCD_H - y)`
  → `KCM_CTP_INVERT_X = KCM_CTP_INVERT_Y = 1`, no swap (already set in KCM_Touch).
```
