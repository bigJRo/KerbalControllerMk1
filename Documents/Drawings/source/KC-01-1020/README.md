# KC-01-1020 Panel A1: drawing sources

Source files for the Rev B drawings of Panel A1, the left display panel: assembly KC-01-1020 and
base plate KC-01-1021 (part control drawing). Rev B supersedes the Rev A Fusion PDFs, which are
in git history. The released PDFs are in `Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1020.json` | Package config: document types, titles, revisions, release status, view crops and placement, line weight, masks over stray parts, balloon relabels, parts list, hole table and notes | both drawings |
| `Fusion_KC-01-1020_Panel_A1_Complete_2026-11-25.pdf` | Fusion assembly drawing export (4 sheets): views, balloons and the Fusion parts list | 1020 ASSY |
| `Fusion_KC-01-1021_Panel_A1_Base_2026-11-25.pdf` | Fusion base plate drawing export (2 sheets): views | 1021 PART sheet 1 |
| `Panel_A1_Production_2026-11-26.ai` | Ponoko production (cut) file, Illustrator, artwork at 1:1 (red = cut; no engraving) | 1021 PART sheet 2 (preview, hole table); fabrication |

## Review notes (2026-11-26)

- **Module set:** two 7" TFT Display Drivers (Annunciator 0x10 and Info Display 1 0x12, the
  panel hub) and the power button, as in Hardware Inventory §7.1.
- **Display drivers:**
  - Fusion lists the two drivers as labelled variants under auto-generated numbers. The parts
    list replaces them with the base driver, KC-01-1910, as one line: item 3, qty 2.
  - Balloon 6 on Fusion sheets 3 and 4 is relabelled 3.
- **Speaker:** the speaker (701715519671) on the Rev A parts list now mounts in the enclosure
  and belongs to the top-level assembly (owner, 2026-11-26).
- **Fasteners:** the 8 × M3 × 14 screws are 4 per driver, into the driver's standoffs. They
  match the 8 × Ø3.6 holes in the production file.
- **Production file:**
  - Outline 425 × 115.2 mm, two display windows of 154.2 × 86.0 mm, a Ø22 power button hole,
    8 × Ø3.6 screw holes and 4 × Ø3.8 mounting holes.
  - The mounting holes are offset as on A2: 10.5 mm from the left edge, 19.6 mm from the right,
    and the lower right one 27 mm up.
  - The Fusion body agrees with the production file to within about 0.7 mm per edge. The
    production file is the master, as for A2.
- **Line weight:** the drawings scale every Fusion stroke by `line_scale` 0.35, as on A2.
- **Stray parts:** `tools/kc_strays.py` masks them in the views marked `auto_masks`.
  A stray touching the panel's bottom edge, about 365 mm from the left, is masked by hand
  (`extra_masks`) in the front, rear and front balloon views (owner: stray, 2026-11-26).
- **Hole table:** `tools/kc_holes.py` builds it from the cut layer (base sheet 2).

## Regenerating the drawings

```
python Documents/Drawings/tools/kc_strays.py Documents/Drawings/source/KC-01-1020/KC-01-1020.json --write
python Documents/Drawings/tools/kc_holes.py  Documents/Drawings/source/KC-01-1020/KC-01-1020.json --doc 1 --write
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1020/KC-01-1020.json --out <output folder>
```
