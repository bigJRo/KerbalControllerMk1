# KCMk1_SysInfo — Sys Info Display

The small portrait screen on Panel A2 (I2C 0x14, slot P2). It has no touch and no buttons, and shows what the master sends it.

| | |
|---|---|
| **Version** | 1.0.0 (`SYSINFO_VERSION`, `KCMk1_SysInfo.h`) |
| **Board** | 2" TFT Display Driver KC-01-1901 / 1902 V2.0: Seeed XIAO RP2350, ER-TFTM020-2 240 × 320 IPS (ST7789) used in portrait, microSD on the same SPI bus |
| **Design** | `Documents/Developer/Sys_Info_Display_Design.md` (Draft 3, built) |
| **Protocol** | `Documents/Developer/I2C_Protocol_Specification.md` §15.4 (v2.21); master side `Software/KCMk1_Master/sysinfo.*` (v0.10.0 or later) |
| **Renders** | `Documents/User/assets/sysinfo/SysInfo_*.png`, drawn by this sketch on the host |

## Screens

| Screen | When | Content |
|---|---|---|
| Splash | Power-on, 2–5 s | `KCMk1 / SYS INFO / v1.0.0`, or `/KCMK1/SPLASH.BMP` from the card |
| Boot | Until BOOT_DONE | Newest boot line, OK / warning / error counts, newest problem |
| Status | After boot, demo included | Supply volts, amps and watts; board temperature and fan; KSP links up; modules present; a message area naming each problem in turn, or `ALL SYSTEMS NOMINAL` and service items |
| SWITCH ERR | While the master reports switch errors | Each switch in error and the vessel's state, five a page |
| MASTER SILENT | No frame for 5 s, either screen | A red block with the seconds since the last frame; values kept in grey |

Every string is Roboto Black 32 or 48, so every capital is at least 22 px (2.8 mm on this panel, 16 arcmin at 600 mm). `Gfx.ino` counts any string that breaks this rule or the 2 px margins, and the host tests fail on either.

## Build

- **Arduino IDE / arduino-cli** with Earle Philhower's **arduino-pico** core; board **Seeed XIAO RP2350**.
- **Libraries:**
  - *Adafruit ST7735 and ST7789 Library* and *Adafruit GFX Library*, from the Library Manager;
  - *KCMk1_SystemConfig*, header only, from `Software/Displays/libraries` (with the sketchbook at `Software/Displays`, as for the other displays).
- **Fonts:** Roboto Black 32 and 48 are copies of the shared fonts in `libraries/KerbalDisplayCommon/src/fonts_ili`, kept in `src/fonts/`. This sketch cannot use KerbalDisplayCommon, which is Teensy / RA8876 only. After a font change, copy them again; `tools/sysinfo_host.py` fails if they differ.

## Tabs

| Tab | Content |
|---|---|
| `KCMk1_SysInfo.ino` | `setup()`, `loop()`, which screen to draw |
| `KCMk1_SysInfo.h` | Version, screen size, colours, protocol constants, the model |
| `AAA_Config.ino` | Pins, timings, thresholds, names (links, modules, switches), icons |
| `Backlight.ino` | Flight / standby level from the BACKLIGHT frame, 300 ms ramp, PWM |
| `Frames.ino` | The 32-deep frame queue, frame parsing, the model, the 5 s silence rule |
| `Gfx.ino` | Frame buffer, font glyphs, icons, word wrap, anti-sticking shift, push on change |
| `I2CSlave.ino` | `Wire1` slave at 0x14; the 4-byte status read |
| `Messages.ino` | The status screen's message list, worst first |
| `Screen_Boot.ino`, `Screen_Status.ino`, `Screen_SwitchErr.ino` | The screens |
| `SdSplash.ino` | The power-on splash, built in or from the card |

## Pins (KC-01-1901 V2.0)

| XIAO | GPIO | Use |
|---|---|---|
| D0 | 26 | Backlight PWM (panel BLK, 20 kHz) |
| D1 / D2 / D3 | 27 / 28 / 5 | ST7789 CS / DC / RESET |
| D4 / D5 | 6 / 7 | I2C1 SDA / SCL (`Wire1`) |
| D6 | 0 | INT_BUS, held high (not used by this protocol) |
| D7 | 1 | microSD CS |
| D8 / D9 / D10 | 2 / 4 / 3 | SPI0 SCK / MISO / MOSI |

The GPIO numbers come from Seeed's pinout, and are the first bench check.

## Host checks

```
python3 tools/sysinfo_host.py          # from Software/Displays: fonts, build, tests, renders
python3 tools/panel_lint.py KCMk1_SysInfo
```

## Bench checks

The full list is in the design, §9.3:
- the pin map;
- the ST7789 at 62.5 MHz (`TFT_SPI_HZ`; 40 MHz if it tears);
- the image upright (`TFT_ROTATION` 0, else 2) and the colours right (the panel is IPS, so check for inverted colours);
- readability at the real eye distance;
- the backlight floor;
- the splash card;
- power-on order;
- the watchdog;
- image sticking after a day;
- power draw.
