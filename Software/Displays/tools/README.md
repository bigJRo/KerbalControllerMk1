# Display sketch tools

Static checks and font utilities for the KCMk1 display firmware. Everything here is
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

The stub reproduces the ILI9341_t3 glyph renderer (`drawFontChar`) because
`drawButton()` and `drawVerticalText()` print through the display library; every
other text path already rasterises in `kcmDrawString()` and blits with `writeRect`.
Two host-only accommodations: pins read HIGH (the FT5316 software-I2C driver would
otherwise wait forever on a clock line the syntax stub holds low), and the build is
`-O0` (glibc's fortified `strlcpy` collides with the stub's inline one above that).

Annunciator scenarios: `MainOrbit` (nominal), `MainReentry` (master alarm, two-tier
and chute-envelope colours), `MainLampTest` (every tile lit — a colour key), `SOI`,
`Standby`. `KCM_SOI_BODY=<name>` selects the body.

Info Display scenarios are one per screen, each a physically consistent flight state
(a ship in a 115×125 km Kerbin orbit for the PFD and orbit screens, a jet for AIRCRAFT
and NAVIGATION, a Mun lander for the descent screens, a capsule at 58 km for RE-ENTRY,
engaged autopilots for the consoles): `SCFT ACFT ROVR VEH LNCHPRE LNCH LNCHCIRC ORB
ORBADV MNVR TGT DOCK NAV LNDG LNDGRE LNCHAP ORBTAP LNDGAP ACFTAP ROVRAP Standby`. The
demo stepper runs once to populate every field, the scenario overrides what matters,
and `millis()` is frozen so the context ladder's answer cannot change; the manual latch
pins the screen only when the ladder would not choose it itself, so the AUTO / MAN chip
reads as it would in flight. Build the mission panel with `-D INFO_DISP_UNIT=2`;
`--suffix` tags the output (`InfoDisp1_SCFT.png`, `InfoDisp2_LNCH.png`):

```
python3 tools/render_screen.py --sketch KCMk1_InfoDisp --suffix 1 SCFT ACFT ROVR VEH Standby
python3 tools/render_screen.py --sketch KCMk1_InfoDisp -D INFO_DISP_UNIT=2 --suffix 2 LNCH ORB ...
```

The framebuffer stub honours the canvas base address and active window, which is how
unit 1 shifts its drawing origin past the left-hand sidebar (`canvasContentRegion()`).

Output is `<Panel><suffix>_<Scenario>.png`. Adding a panel is a scenario file for it
plus its name list in `SCENARIO_NAMES`.

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
