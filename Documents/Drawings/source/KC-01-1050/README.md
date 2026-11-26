# KC-01-1050 Panel B2: drawing sources

Source files for the Rev B drawings of Panel B2, the right input panel: assembly KC-01-1050 and
base plate KC-01-1051 (part control drawing). Rev B supersedes the Rev A Fusion PDFs, which are
in git history. The released PDFs are in `Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1050.json` | Package config: document types, titles, revisions, release status, view crops and placement, line weight, masks over stray parts, balloon relabels, parts list, hole table and notes | both drawings |
| `Fusion_KC-01-1050_Panel_B2_Complete_2026-11-25.pdf` | Fusion assembly drawing export (4 sheets): views, balloons and the Fusion parts list | 1050 ASSY |
| `Fusion_KC-01-1051_Panel_B2_Base_2026-11-25.pdf` | Fusion base plate drawing export (2 sheets): views and overall dimensions | 1051 PART sheet 1 |
| `Panel_B2_Production_2026-11-26.ai` | Ponoko production (cut and engrave) file, Illustrator, artwork at 1:1 (red = cut, blue = engrave) | 1051 PART sheets 2 (preview) and 3 (hole table); fabrication |

## Review notes (2026-11-26)

- **Module set:** matches Hardware Inventory §7.4. Pre-Warp Time (7-Segment Module); Time
  Control, Stability Control and AUX CTRL (Button Module); Vehicle Control (Wide Button Module,
  with the Panel Hub mounted behind it); Rotation (Joystick Module). Slots P1 and P5 are empty.
- **Module variants:**
  - Fusion lists each module under an auto-generated number for its labelled variant. The parts
    list replaces them with the base module: KC-01-1840, 1800, 1830 and 1810.
  - The three Button Modules (Fusion items 4, 14 and 15) are one line, item 4, qty 3.
  - Balloons 14 and 15 on Fusion sheets 3 and 4 are relabelled 4.
- **Descriptions:** the new export leaves the Fusion descriptions blank. The parts list gives
  them in the style of Panel A2.
- **Fasteners:**
  - 28 × M3 × 8: 3 Button Modules × 6, plus the Wide Button × 6 and the Joystick × 4.
  - 4 × M3 × 5: the 7-Segment module.
  - Together they match the 32 × Ø3.2 holes in the production file.
- **Switches:** 9 toggle switches (Switch Group 2 and the stage arming switch under the green
  cover) and 8 switch guards.
- **Production file:**
  - Outline 425 × 270 mm.
  - The 7 × Ø3.8 mounting holes are A2's mirror image: 19.6 mm from the left edge and 10.5 mm
    from the right, with the lower holes at 22.5, 212.5 and 414.5 mm.
  - 4 × Ø3.0 around the joystick, as on A2, and a Ø7.6 hole for the Pre-Warp Time encoder.
- **Balloon views:** they are at 1:4 in Fusion. The drawing shows them at 1:5 so that both fit
  beside the parts list. Their crops are clipped clear of the Fusion title block.
- **Line weight:** the drawings scale every Fusion stroke by `line_scale` 0.35, as on A1, A2
  and B1.
- **Stray parts:** `tools/kc_strays.py` masks them in the views marked `auto_masks`.
- **Hole table:** `tools/kc_holes.py` builds it from the cut layer (base sheet 3).

## Regenerating the drawings

```
python Documents/Drawings/tools/kc_strays.py Documents/Drawings/source/KC-01-1050/KC-01-1050.json --write
python Documents/Drawings/tools/kc_holes.py  Documents/Drawings/source/KC-01-1050/KC-01-1050.json --doc 1 --write
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1050/KC-01-1050.json --out <output folder>
```

## To correct at the next revision

These are the owner's part decisions of 2026-11-26. Panel A2 Rev C already applies them.

| Item | Part | Now | Becomes |
|---|---|---|---|
| 11 | Toggle switch cover, green | 11000002 (Omval) | AE 3256805484145709, HC Electrical Store, green variant |
| 12 | 9 toggle switches | 10000001 (Omval) | FLP12-FJ-1021P-L, FILN 12 mm SPST latching, 15 A 250 V |
| 10 | STAGE push button, 22 mm green | 31000001 | AE 3256807837994773, Hanhai Electric Appliances and Accessories Store, 22 mm green 3–6 V variant (lit from +5 V) |
| 7 | Joystick | JH-DX400B-R4 | JH-D400X-R4, SaiDian (Amazon B08CGYGMJL) |

None of the nine B2 toggles is momentary (owner, 2026-11-26).
