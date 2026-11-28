# KC-01-1080 Panel Brace: drawing sources

Source files for the Rev B drawing of the panel brace KC-01-1080 (part control drawing), the
3D-printed angle that supports the panels in the enclosure. Rev B supersedes the Rev A Fusion
drawing `KC-01-1080_RevA_Panel Brace.pdf`, which is in git history. The released PDF is in
`Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1080.json` | Package config: document type, title, revisions, release status, view crops and placement, masks, labels, line weight and notes | the drawing |
| `Fusion_KC-01-1080_Panel_Brace_2026-11-25.pdf` | Fusion part drawing export (1 sheet): end, front, top and isometric views with dimensions | KC-01-1080 PART |
| `Panel_Brace_2026-11-25.step` | The part model (owner): one solid, ABS, white | reference |

## Review notes (2026-11-26)

- **Changes from Rev A:**

  | | Rev A | Rev B |
  |---|---|---|
  | Base / lip thickness | 5.00 / 2.50 mm | 4.00 / 4.00 mm |
  | Overall width × height | 25.22 × 12.36 mm | 31.76 × 13.53 mm |
  | Base flat | 18.80 mm | 25.11 mm |
  | Length | 240.00 mm | 240.00 mm |
  | Lip angle | 120.00° | 120.00° |
  | Inserts | 2 × M3 heat-set (94459A769) | none |

- **Model check:** the STEP profile matches the drawing: base 4.00 mm, lip 4.00 mm at 120°
  to the base, 31.76 × 13.53 mm overall, 240.00 mm long, no holes.
- **Fixing:** the brace has no inserts or holes now (owner, 2026-11-26). It is held with
  Dual Lock (3M) reclosable fastener pads, so the drawing has no parts list. The STEP's
  description still says "Installed M3 Brass Inserts"; that text is out of date.
- **Material:** ABS, white, 3D printed, as the STEP gives it.
- **Quantity and position:** two are fitted (owner, 2026-11-26). The part drawing does not
  give the count; the top-level assembly will show where they go, with the Dual Lock pads.
- **Revision:** the Fusion title block reads Rev A (2026-06-27), but the part changed, so the
  KC drawing is Rev B.

## Migration notes

- **Views:** the end view and the front and top views are two crops of the Fusion sheet at
  1:1, kept level as Fusion drew them. The isometric view is its own crop, clipped clear of
  the top view, and drawn as large as the sheet allows (not to scale).
- **Labels:** Fusion gives no view names; END VIEW, FRONT VIEW and TOP VIEW are KC labels.
- **Masks:** two Fusion zone ticks of the sheet border, one above the front view and one beside
  the isometric view.
- **Line weight:** every Fusion stroke is scaled by `line_scale` 0.35, as on the other
  mechanical drawings.

## Regenerating the drawing

```
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1080/KC-01-1080.json --out <output folder>
```
