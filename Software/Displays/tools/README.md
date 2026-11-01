# Display sketch tools

Static checks, host renders and behaviour tests, and font utilities for the KCMk1 display firmware. Everything here is
plain Python 3 with no third-party dependencies, so it runs anywhere the repo is
checked out.

## `panel_lint.py`

```
python3 tools/panel_lint.py                                  # all three sketches
python3 tools/panel_lint.py KCMk1_InfoDisp                   # just one
python3 tools/panel_lint.py --check stale-comments           # just one check
```

Run it before tagging a release. It exits non-zero if either of the two hard checks
finds anything.

### Why each check exists

Every one of these is here because the defect it looks for actually shipped.

**`stale-comments` (hard).** Flags a constant whose trailing comment asserts a value the
constant no longer has — `// 133` on something that now evaluates to 76. These are not
cosmetic. When the attitude disc was enlarged from R=150 to R=206, fourteen derived
comments across `Screen_ACFT.ino` and `Screen_SCFT.ino` kept quoting the old numbers,
and two later layout decisions were made by reading those comments instead of the
arithmetic: an attitude-rate bar placed underneath the roll readout, and a `TRIM`
clearance computed from a string width that was never measured.

The evaluator resolves each constant's initialiser for real, iterating to a fixed point
so a tab may reference a constant declared in a later-concatenated tab. **It divides the
way C does** — truncating when both operands are integral — which matters more than it
sounds: evaluating `(SCREEN_H - PANEL_Y) / NROWS` in Python's arithmetic made an early
version of this tool report a perfectly correct comment as stale.

A comment "asserts" a value only when it opens with a bare number that is either the
whole comment or is followed by a dash. Prose that happens to start with a digit —
`// 10 min`, `// 40% wider than the original 96`, `// 52 entries` — is left alone.

**`dead-constants` (hard).** A file-scope constant nothing reads. Two reasons this is
worth failing a build over:

- Forty of them were a shadow copy of the shared `EADI_*` PFD geometry. Editing
  `SCFT_PTAPE_MRK_HW` moved nothing, because the pitch tape is drawn from `EADI_*`.
- A dead *threshold* is worse: it documents a behaviour the firmware does not have.
  `STALL_SPEED_MS` implied a low-speed warning on the aircraft PFD and
  `TGT_VCLOSURE_WARN_MS` implied a yellow closure band on TARGET. Neither existed.

To keep a constant deliberately, say so in the source:

```c
// panel-lint: keep — reason
static const uint8_t SOMETHING = 3;

// panel-lint: keep-begin — reason
...
// panel-lint: keep-end
```

The reason is not optional in spirit: an unexplained suppression is how the next dead
threshold hides.

**`duplicate-consts` (advisory).** Suffixes independently re-derived to the same value
under two or more screen prefixes. Sometimes legitimate — `MNVR` and `TGT` genuinely
both have a bar. Sometimes it is one instrument's geometry written out twice, waiting
for someone to edit one copy. Declarations that are plain aliases (`SCFT_PTAPE_X =
EADI_PTAPE_X`) are excluded, because an alias cannot drift.

**`unreset-statics` (advisory).** A mutable file-scope static that no chrome or reset
function assigns, which on re-entry to a screen can mean an erase at coordinates left
over from the previous visit. **Expect false positives** — the usual correct pattern in
this codebase is a separate `*Valid` flag that *is* reset, with the stale coordinates
harmlessly left alone. The report lists the guard flags it found for exactly this
reason. Read each hit before believing it.

### What a clean run means

"Nothing contradicted", not "everything proved". A constant whose initialiser the
evaluator cannot evaluate is skipped rather than guessed at; the header line reports how
many of the declarations it resolved.

## `render_screen.py`

```
python3 tools/render_screen.py                       # every Annunciator scenario
python3 tools/render_screen.py MainOrbit SOI         # just these
python3 tools/render_screen.py --out /tmp/shots      # default: Documents/User/assets
```

True renderings of a panel's screens as PNGs, for the user manual. It does the same
IDE-faithful host build `host_compile.py` does, but **links** it — sketch,
KerbalDisplayCommon, KCM_Touch, KerbalDisplayAudio — against `host_stubs_fb/`, where
`RA8876_t41_p` keeps a 1024×600 RGB565 framebuffer and `SD` reads the real art from
`../assets`. A scenario (`render_scenarios/<sketch>.inc`, appended to the concatenated
sketch) sets the panel's `AppState`, runs `updateCautionWarningState()` so the C&W
bits come from the real logic, and then calls the firmware's own `loop()`. Layout,
fonts, colours, BMPs and indicator logic are therefore the panel's, not a mockup's.

The stub follows the real library (wwatson4506 TeensyRA8876-GFX-Common,
`RA8876_common.cpp`) wherever it decides what reaches the glass, so a render shows the
panel's quirks too: coordinates wrap to 16 bits and the controller's 13-bit registers;
rectangles clamp to the screen and an inverted one draws nothing; lines are dropped when
their start is off screen; `setTextColor(fg, bg)` prints opaque and `setTextColor(fg)`
transparent; a glyph the font lacks prints as its first glyph; `print(double)` gives two
decimals. It reproduces the ILI9341_t3 glyph renderer (`drawFontChar`) because
`drawButton()`, `drawVerticalText()` and a few screens print through the display
library; every other text path already rasterises in `kcmDrawString()` and blits with
`writeRect`. The one behaviour not confirmed on hardware is a zero-size or inverted
box, which the stub treats as drawing nothing.
Two host-only accommodations: pins read HIGH (the FT5316 software-I2C driver would
otherwise wait forever on a clock line the syntax stub holds low), and the build is
`-O0` (glibc's fortified `strlcpy` collides with the stub's inline one above that).

Annunciator scenarios: `MainOrbit` (nominal), `MainReentry` (master alarm, two-tier
and chute-envelope colours), `MainLampTest` (every tile lit — a colour key), `SOI`,
`Standby`. `KCM_SOI_BODY=<name>` selects the body.

Info Display scenarios are one per screen, each a physically consistent flight state
(a ship in a 115×125 km, 6° Kerbin orbit for the PFD and orbit screens, a jet for
AIRCRAFT and NAVIGATION, a Mun rover driving to a flag 850 m away, a booster coasting to
apoapsis with a circularisation node placed for CIRCULARIZATION, a Mun lander for the
descent screens, a capsule at 58 km for RE-ENTRY, engaged autopilots for the consoles): `SCFT ACFT ROVR VEH LNCHPRE LNCH LNCHCIRC ORB
ORBADV MNVR TGT DOCK NAV LNDG LNDGRE LNCHAP ORBTAP LNDGAP ACFTAP ROVRAP Standby`, plus
`INPUT`: the input test screen (unit 1), a bench state with a few inputs down, the
throttle touched, the Pre-Warp module missing, TIME reporting a FAULT, Translation
sleeping, the Dual Encoder disabled and the four displays answering, opened by the real
two-finger gesture and fed one frame of each of the three pages. The
demo stepper runs once to populate every field, the scenario overrides what matters,
and `millis()` is frozen so the context ladder's answer cannot change; the manual latch
pins the screen only when the ladder would not choose it itself, so the AUTO / MAN chip
reads as it would in flight. Build the mission panel with `-D INFO_DISP_UNIT=2`;
`--suffix` tags the output (`InfoDisp1_SCFT.png`, `InfoDisp2_LNCH.png`):

```
python3 tools/render_screen.py --sketch KCMk1_InfoDisp --suffix 1 SCFT ACFT ROVR VEH Standby
python3 tools/render_screen.py --sketch KCMk1_InfoDisp -D INFO_DISP_UNIT=2 --suffix 2 LNCHPRE LNCH LNCHCIRC ORB ORBADV MNVR TGT DOCK NAV \
    LNDG LNDGRE LNCHAP ORBTAP LNDGAP ACFTAP ROVRAP
```

The framebuffer stub honours the canvas base address and active window, which is how
unit 1 shifts its drawing origin past the left-hand sidebar (`canvasContentRegion()`).

Resource Display scenarios: `Main` (the SPC set on a ship in orbit: LF past a reserve bug,
MP in caution, food on its time tier, Solid Fuel not aboard and collapsed), `MainTTE` (the
same with the TTE key engaged), `EVA` (the ring-gauge layout), `Select`, `Detail` (Liquid Fuel
after ten minutes of a slow burn, history trace filled) and `Standby`. The scenario feeds the
slots as the Simpit handler would and runs the panel's own sampling before the frame, so
trends, time to empty, time tiers and the history come from the real logic:

```
python3 tools/render_screen.py --sketch KCMk1_ResourceDisp
```

Output is `<Panel><suffix>_<Scenario>.png`. Adding a panel is a scenario file for it
plus its name list in `SCENARIO_NAMES`.

## `behaviour_selftest.py`

```
python3 tools/behaviour_selftest.py                       # all three panels
python3 tools/behaviour_selftest.py KCMk1_Annunciator     # just one
```

Host behaviour tests for the three display sketches. Each panel is built the way
`render_screen.py` builds it (the IDE's tab concatenation and prototype hoisting, the
real in-repo libraries, the framebuffer stubs), with `selftest_scenarios/common.inc` and
the panel's own `selftest_scenarios/<sketch>.inc` appended so the tests reach every
tab's globals. `KCM_Touch.cpp` is left out of the link and faked in `common.inc`, so a
test can put one or two fingers on the glass. The tests feed Simpit messages, the
master's I2C control byte, BACKLIGHT and input test frames and touches, run the panel's
own `loop()` (each frame advances the stubbed `millis()` by 50 ms), and check its state.
It prints one `ok` / `FAIL` line per check and exits non-zero if a check or a build
fails. Run it after changing demo, touch, backlight or I2C handling.

What it covers (63 checks):

| Panel | Checks |
|---|---|
| All three | **Demo follows the master's flag only** (Hardware Inventory §8.8): a scene change in demo is tracked but does not end demo or change the screen; clearing the flag in flight re-enters the flight screen, and out of flight shows standby. **Backlight**: the configured flight and standby levels before any frame; a new level ramps rather than steps and arrives within the ramp time; the master's standby level on the standby screen; 0 % clamped to the 5 % floor, over 100 % to 100 % |
| Annunciator (17) | A dock seen during demo is applied on leaving it. **DOCK tile touch**: a tap sets DOCK, a second clears it, a real undock overrides a tap, a tap beside the tile does nothing, no effect out of flight |
| Info Display (31) | Leaving demo in flight waits for the real vessel's context screen; out of flight it parks on the home screen. **Input test screen**: the two-finger pull from the top-right opens it, status byte 2 reads 18, the screen underneath is kept; a page 0 frame is taken, in demo too through the I2C receive buffer; a wrong-length frame is dropped; page 1 is taken at 24 bytes with the lifecycles and the old 20-byte page 1 is dropped; page 2 (the four displays) is taken, and its rows go to "--" when it stops for 1.5 s; a tap while open does nothing; fingers outside the corner zone, or too short a pull, do nothing; the same pull closes it, the lifting fingers are not taken as a tap, and the screen underneath is repainted; it opens on standby too; NO DATA after 1.5 s without frames |
| Resource Display (15) | The vessel's own bar layout survives demo: demo shows its own bars and ignores `VESSEL_NAME`, and after demo the next `VESSEL_NAME` recalls the vessel's layout, not demo's |

To add a test, add a `CHECK(condition, "message")` to the panel's `.inc`; `SECTION()`
prints a heading. The helpers in `common.inc` (`frames()`, `feed()`, `scene()`,
`vesselChange()`, `tap()`, `touch2()`, `untouch()`, `backlightChecks()`) cover the common
moves.

## `sysinfo_host.py`

```
python3 tools/sysinfo_host.py                     # fonts, build, every test, every render
python3 tools/sysinfo_host.py test [NAME ...]     # behaviour tests only
python3 tools/sysinfo_host.py render [NAME ...]   # renders only -> Documents/User/assets/sysinfo/
```

The Sys Info Display (`KCMk1_SysInfo`, XIAO RP2350 + ER-TFTM020-2) is not a Teensy /
RA8876 sketch, so `host_compile.py`, `render_screen.py` and `behaviour_selftest.py` do not
cover it; this does all three for it. The tabs are concatenated and the prototypes hoisted
as the IDE does (`host_compile.py`'s code), then built with `-Wall -Wextra -Werror` against
`host_stubs_sysinfo/` (arduino-pico core, `Wire1`, `SPI`, `SD`, an `Adafruit_ST7789` that
keeps the pushed frame) and the real `KCMk1_SystemConfig.h`. `sysinfo_host.inc` plays the
master: it writes CONTROL, BOOT_LINE, BOOT_DONE, STATUS, SWITCH_ERR and BACKLIGHT frames
through the `Wire1` stub and reads the 4-byte status back.

It checks, and exits non-zero on any failure:
- **fonts:** the sketch's `src/fonts/` copies are byte-identical to `fonts_ili` (the sketch
  cannot use KerbalDisplayCommon, which is Teensy-only);
- **build:** no warnings;
- **tests (16):** splash timing; boot counts with REPLACE and REPLAY; word wrap; every
  short boot line of the design's §8.2 in two rows; MASTER SILENT on both screens and its
  recovery; STATUS parsing (13 and 27 bytes, bad frames); message order, rotation, the 7 / 8 A
  current edges, demo and the I2C-errors entry; SWITCH ERR paging and unknown ids; the
  backlight levels, floor and ramp; the anti-sticking shift; no screen saver over an hour;
  a push only when the frame changes; a full queue; the status read and pins; the demo KSP
  row on the DEMO blue; and a worst-case layout run;
- **the readability rule on every string** of every test and render: `Gfx.ino` counts any
  font whose capital H is under 22 px (2.8 mm on this panel) and any string outside the
  2 px margins, and the run fails if either is non-zero;
- **renders (14):** `SysInfo_<SCENARIO>.png`, 240 × 320, the design's mockups drawn by the
  firmware.

## `../libraries/KerbalDisplayCommon/src/fonts_ili/`

The font pipeline lives with the fonts: `ilifont.py` (parse/decode an ILI9341_t3 `.c`),
`bdf_to_ili9341.py`, `tfont_to_ili9341.py`, `add_middot.py`. `ilifont.py` is also the
way to measure a string the way the firmware will:

```python
import sys; sys.path.insert(0, 'libraries/KerbalDisplayCommon/src/fonts_ili')
from ilifont import Font
f = Font('libraries/KerbalDisplayCommon/src/fonts_ili/Roboto_Black_36.c')
width = sum(f.glyph(ord(c))['delta'] for c in "T+Tgt:")
```

Do this before sizing any value cell. Guessing a string width is how `T+Tgt` came to
overlap its own label: the cell was budgeted for `"59m 30s"` at 142 px, but
`formatTimeCompact` below one hour emits `formatTime`'s `"59 m: 30 s"`, which is 171.
