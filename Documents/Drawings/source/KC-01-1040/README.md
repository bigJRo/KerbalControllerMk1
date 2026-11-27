# KC-01-1040 Panel B1: drawing sources

Source files for the Rev B drawings of Panel B1, the right display panel: assembly KC-01-1040 and
base plate KC-01-1041 (part control drawing). Rev B supersedes the Rev A Fusion PDFs, which are
in git history. The released PDFs are in `Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1040.json` | Package config: document types, titles, revisions, release status, view crops and placement, line weight, masks over stray parts, balloon relabels, parts list, hole table and notes | both drawings |
| `Fusion_KC-01-1040_Panel_B1_Complete_2026-11-25.pdf` | Fusion assembly drawing export (4 sheets): views, balloons and the Fusion parts list | 1040 ASSY |
| `Fusion_KC-01-1041_Panel_B1_Base_2026-11-25.pdf` | Fusion base plate drawing export (2 sheets): views | 1041 PART sheet 1 |
| `Panel_B1_Production_2026-11-26.ai` | Ponoko production (cut and engrave) file, Illustrator, artwork at 1:1 (red = cut, blue = engrave) | 1041 PART sheet 2 (preview, hole table); fabrication |

## Review notes (2026-11-26)

- **Module set:** two 7" TFT Display Drivers (Resource Display 0x11, the panel hub, and Info
  Display 2 0x13) and the Dual Encoder (0x2D) in the Resource Display's slot P4, as in Hardware
  Inventory §7.3.
- **Display drivers:**
  - Fusion lists the two drivers as labelled variants (items 2 and 5) under auto-generated
    numbers. The parts list replaces them with the base driver, KC-01-1910, as one line: item
    2, qty 2.
  - Balloon 5 on Fusion sheets 3 and 4 is relabelled 2.
- **Dual Encoder:** listed as KC-01-1860, V2.1 with the encoders on the board. Its knobs are in
  its own package.
- **Fasteners:** the 8 × M3 × 14 screws are 4 per driver, into the driver's standoffs. They
  match the 8 × Ø3.6 holes in the production file.
- **Production file:**
  - The outline, display windows and display screw holes are the same as Panel A1.
  - The 4 × Ø3.8 mounting holes are A1's mirror image: 19.6 mm from the left edge, 10.5 mm
    from the right, and the lower left one 27 mm up.
- **Lower brace-side hole (owner, 2026-11-27):** the hole 27 mm up is intentional. It matches the
  as-built internal brace, which was fabricated with its lower bracket hole in that position; the
  owner verified the dimensions on the real part. Do not move it to the KC-01-1071 drawing position.
  - 2 × Ø7.6 encoder holes, engraved CTRL GRP and EXT CAM.
- **Line weight:** the drawings scale every Fusion stroke by `line_scale` 0.35, as on A1 and A2.
- **Stray parts:** none found. `tools/kc_strays.py` checks the views marked `auto_masks`.
- **Hole table:** `tools/kc_holes.py` builds it from the cut layer (base sheet 2).

## Regenerating the drawings

```
python Documents/Drawings/tools/kc_strays.py Documents/Drawings/source/KC-01-1040/KC-01-1040.json --write
python Documents/Drawings/tools/kc_holes.py  Documents/Drawings/source/KC-01-1040/KC-01-1040.json --doc 1 --write
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1040/KC-01-1040.json --out <output folder>
```
