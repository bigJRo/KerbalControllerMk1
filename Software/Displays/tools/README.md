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
