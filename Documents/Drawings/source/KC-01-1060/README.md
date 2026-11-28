# KC-01-1060 Top Panel: drawing sources

Source files for the Rev A drawings of the top panel: assembly KC-01-1060 (Top Panel Complete)
and plate KC-01-1061 (Top Cover Plate, part control drawing). They replace the 2022 Fusion
drawing `KC-01-1060_Rev-_Top Cover Plate.pdf`, which numbered the plate KC-01-1060 and is in git
history. The released PDFs are in `Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1060.json` | Package config: document types, titles, revisions, release status, view crops and placement, line weight, parts list and notes | both drawings |
| `Fusion_KC-01-1060_Top_Panel_Complete_2026-11-25.pdf` | Fusion assembly drawing export (4 sheets): views, balloons and the Fusion parts list | 1060 ASSY |
| `Fusion_KC-01-1061_Top_Cover_Plate_2026-11-25.pdf` | Fusion plate drawing export (2 sheets): views, overall dimensions, hole locations and hole table | 1061 PART sheet 1 (views) |
| `Top_Cover_Plate_2026-11-25.step` | The plate model (owner): the plate and the two grilles, each grille a separate body in its opening | the cut file |
| `Top_Cover_Plate_Cut_2026-11-26.pdf` | The plate's top face flattened to 1:1 by `tools/kc_step2d.py`: outline, cuts in red (like a production file's cut layer), grille openings as grey reference | 1061 PART sheet 2 (hole locations, hole table, detail A) |

## Review notes (2026-11-26)

- **Numbering:** the panel scheme is used. KC-01-1060 is the assembly and KC-01-1061 the
  plate. The Fusion title blocks carry placeholders (KC-01-1XX0, KC-01-1XX2), and the plate
  drawing carries KC-01-1031, which is the Panel A2 base. The KC frame replaces them.
- **Scales:** the Fusion title blocks give "X:X" and "1:4". The assembly views are at 1:4 and
  the plate views at 1:3, as drawn.
- **Speakers:**
  - 2 × XIITIA 3 W 4 Ω enclosed speaker with a JST-PH2.0 lead, Amazon B0D5HPZB4Y (owner).
  - The Fusion list gives no part number.
  - They moved here from Panel A1, whose Rev A list had one 3 W 8 Ω speaker (701715519671).
  - Both plug into the Annunciator's 7" TFT Display Driver: SPKR1 on the PAM8302A amplifier
    (alarm tones) and SPKR2 on the DFPlayer (GPWS voice).
- **Fasteners:** each speaker is held by 4 × M3 × 14 screws (90116A160) and M3 nuts (94150A325)
  through the Ø3.4 holes A1–A8, which form two 64 × 24 mm patterns.
- **Plate:**
  - Same outline as the 2022 drawing: 862 × 78.27 × 6 mm, opaque black acrylic, TAP Plastics.
  - It is screwed down (owner, 2026-11-25), not glued: the 2022 note "all edges machined for
    gluing" is dropped.
  - The six Ø4.76 bracket holes are unchanged.
- **Hole table:**
  - Generated like the panels' (owner, 2026-11-26), by `tools/kc_holes.py` from the cut file.
  - A Ø3.4 × 8 (speaker screws) and B Ø4.76 × 6 (enclosure brackets).
  - Datum: the lower-left corner, as in the model.
- **Speaker grilles:**
  - The grilles are cut in one piece with the plate (owner, 2026-11-26), although the model
    has each as its own body.
  - Each is a 55.00 × 29.00 obround (R14.50) centred at X 349.00 / 513.00, Y 45.13: the
    centres of the speaker screw patterns.
  - In it are 100 diamond holes (squares at 45°, 4.00 across corners), at alternate positions
    of a 2.475 mm square grid centred on the grille. Every position whose diamond lies inside
    the obround is cut.
  - The webs between diamonds are about 0.67 mm.
  - Detail A on sheet 2 gives the grille. The pattern centre is 0.02 mm off the obround centre
    in the model, which is within tolerance.
- **Dimensions:** overall 862.00 × 78.27 × 6.00 on sheet 1, and again on the hole locations
  view.
- **Line weight:** the drawings scale every Fusion stroke by `line_scale` 0.35, as on the
  panels.

## Regenerating the drawings

```
python Documents/Drawings/tools/kc_step2d.py Documents/Drawings/source/KC-01-1060/Top_Cover_Plate_2026-11-25.step \
    Documents/Drawings/source/KC-01-1060/Top_Cover_Plate_Cut_2026-11-26.pdf
python Documents/Drawings/tools/kc_strays.py Documents/Drawings/source/KC-01-1060/KC-01-1060.json --write
python Documents/Drawings/tools/kc_holes.py  Documents/Drawings/source/KC-01-1060/KC-01-1060.json --doc 1 --write
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1060/KC-01-1060.json --out <output folder>
```
