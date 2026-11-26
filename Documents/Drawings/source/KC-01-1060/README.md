# KC-01-1060 Top Panel: drawing sources

Source files for the Rev A drawings of the top panel: assembly KC-01-1060 (Top Panel Complete)
and plate KC-01-1061 (Top Cover Plate, part control drawing). They replace the 2022 Fusion
drawing `KC-01-1060_Rev-_Top Cover Plate.pdf`, which numbered the plate KC-01-1060 and is in git
history. The released PDFs are in `Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1060.json` | Package config: document types, titles, revisions, release status, view crops and placement, line weight, parts list and notes | both drawings |
| `Fusion_KC-01-1060_Top_Panel_Complete_2026-11-25.pdf` | Fusion assembly drawing export (4 sheets): views, balloons and the Fusion parts list | 1060 ASSY |
| `Fusion_KC-01-1061_Top_Cover_Plate_2026-11-25.pdf` | Fusion plate drawing export (2 sheets): views, overall dimensions, hole locations and hole table | 1061 PART |

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
  through the Ø3.4 holes B1–B8, which form two 64 × 24 mm patterns.
- **Plate:**
  - Same outline as the 2022 drawing: 862 × 78.27 × 6 mm, opaque black acrylic, TAP Plastics.
  - It is screwed down (owner, 2026-11-25), not glued: the 2022 note "all edges machined for
    gluing" is dropped.
  - The six Ø4.76 bracket holes are unchanged.
- **Hole table:** the plate's hole table is Fusion's own, cropped from its sheet 2. Fusion
  gives depths of 6.00 and 10.00 mm; both mean through the 6 mm plate.
- **Open: speaker grilles.** The two obround perforated grilles are not dimensioned in either
  Fusion drawing (pattern size, hole size and pitch). Note 3 of the plate drawing refers to the
  model until they are defined.
- **Line weight:** the drawings scale every Fusion stroke by `line_scale` 0.35, as on the
  panels.

## Regenerating the drawings

```
python Documents/Drawings/tools/kc_strays.py Documents/Drawings/source/KC-01-1060/KC-01-1060.json --write
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1060/KC-01-1060.json --out <output folder>
```
