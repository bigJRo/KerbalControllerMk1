# KC-01-1030 Panel A2: drawing sources

Source files for the drawings of Panel A2, the left input panel: assembly KC-01-1030 and
base plate KC-01-1031 (part control drawing). Rev B supersedes the Rev A Fusion PDFs, which are
in git history. The released PDFs are in `Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1030.json` | Package config: document types, titles, revisions, release status, view crops and placement, line weight, masks over stray parts, balloon relabels, parts list, hole table and notes | both drawings |
| `Fusion_KC-01-1030_Panel_A2_Complete_2026-11-25.pdf` | Fusion assembly drawing export (4 sheets): views, balloons and the Fusion parts list | 1030 ASSY |
| `Fusion_KC-01-1031_Panel_A2_Base_2026-11-25.pdf` | Fusion base plate drawing export (2 sheets): views and overall dimensions | 1031 PART sheet 1 |
| `Panel_A2_Production_2026-11-26.ai` | Ponoko production (cut and engrave) file, Illustrator, artwork at 1:1 (red = cut, blue = engrave) | 1031 PART sheets 2 (preview) and 3 (hole table); fabrication |

## Review notes (2026-11-25)

- The module set matches Hardware Inventory §7.2: Action Control and UI Control (Button Module),
  Function Control (Wide Button Module, with the Panel Hub mounted behind it), Translation
  (Joystick Module), GPWS (7-Segment Display Module), Sys Info Display (2in TFT Display Driver),
  Throttle (Throttle Module); slot P5 empty.
- Fusion lists each module under an auto-generated number for its labelled variant. The parts
  list in the config replaces them with the base module (KC-01-1800, 1810, 1830, 1840, 1900,
  1870) and names the role in the description. The two Button Modules are one line, item 13,
  qty 2; balloon 19 on sheet 4 is relabelled 13.
- The Fusion views carry stray single parts (label inserts and similar) below and to the right of
  several views. `tools/kc_strays.py` finds them and writes white-out `masks` (source-page mm)
  into the views marked `auto_masks`; strays too close to a leader are in `extra_masks`.
- Fusion exports its visible lines at 0.5 mm, which fills in the engraved legends at 2:5; the
  drawings scale every Fusion stroke by `line_scale` 0.35.
- Front, side and top views keep Fusion's projection alignment: each is moved by the same
  offset along its shared axis, with the gaps closed up.
- The hole table (1031 sheet 3) is built from the cut layer of the production file by
  `tools/kc_holes.py`: round holes up to Ø8, tagged by size (A Ø3.0, B Ø3.2, C Ø3.8,
  D Ø7.6), positions from the lower-left corner of the outline.
  With the 2026-11-26 production file: A Ø3.0 × 4 (joystick), B Ø3.2 × 32, C Ø3.8 × 7,
  D Ø7.6 × 1. That file also lines up the two lower display holes at Y 108.8.
- Hole review (owner, 2026-11-26): the two throttle holes go from Ø3.6 to Ø3.2. The four 2"
  display holes go from Ø2.5 to Ø3.2: the 2" display keeps its M3 standoffs, spacers and
  screws (checked by the owner), so KC-01-1900 is unchanged. The Fusion exports are not
  regenerated for these changes; the production file carries them.
- Outline: the production file's 425 × 270 mm is correct (owner, 2026-11-25). The Fusion model
  body is about 429.7 × 274.7 mm, about 2.3 mm oversize all round, with the holes in the right
  places; the views are left as exported until the Fusion body is fixed.
- The outline is drawn in dark grey in the production file and is cut as it stands (owner).
- The Ø3.8 mounting holes (C) are offset on purpose: 10.5 mm from the left edge, 19.6 mm and
  22.5 mm from the right, to suit the enclosure (owner).
- Switch Group 1 is engraved MSTR · DISPL · SCE · UPTLM / LTG · A/P · ENGINE · THRTL (B16–B23).
  DISPL and RST are the panel abbreviations of DISP and RESET (Module UI Reference §6.2).
- The 22 mm cover over the ABORT button is part 90000002 (Fusion listed 90000001, which is the MBK keycap
  on the 7-Segment modules and the Expansion Keyboard).
- The A/P switch is engraved INHIB / ENAB (renamed from ARM, 2026-11-25).
- The stick is the JH-DX400B-R4 (4-axis, three axes used). The fasteners in the parts list are
  complete (owner).
- Sheet 2 of the base drawing is drawn from the archived production file, not from the Fusion
  export (whose sheet 2 image was out of date).

## Rev C (2026-11-26, draft)

- **Toggle switches:** the Fusion parts list gives all nine toggles as item 10, 10000001
  (SPST on/off). Three of them, MSTR, DISPL and LTG, are momentary (owner, 2026-11-26).
  - Item 10 is now the six latching switches: SCE, UPTLM, A/P, ENGINE, THRTL and the abort
    arming switch.
  - New item 19 is the three momentary switches. Both kinds are 12 mm SPST with 2 solder pins,
    from one AliExpress listing.
  - The Fusion file is not changed. The split is made in `KC-01-1030.json`.
- **Balloon:** item 19 gets its own balloon on the rear view (sheet 3). It is drawn by the
  generator's `balloons` option and points at MSTR.
- **AliExpress part numbers:** the owner decided on 2026-11-26 that a part bought on AliExpress
  with no manufacturer part number is numbered by its AliExpress item, as "AE <item>". The
  variant is named in the description. This replaces the internal numbers and Fusion's
  Amazon number:

  | Item | Part | Was | Now |
  |---|---|---|---|
  | 7 | Button cover, 22 mm | 90000002 | AE 2251832816396454 |
  | 9 | Toggle switch cover, red | 11000001 | AE 3256805484145709 (HC Electrical Store) |
  | 10 | Toggle switch, latching | 10000001 | FLP12-FJ-1021P-L |
  | 14 | 16 mm blue push button | B079FKG8B1 | AE 3256807837994773 |
  | 19 | Toggle switch, momentary | none (new item) | FLP12-FJ-1021P-M |

  Panels A1 and B2 take the same numbers at their next revision.
- **Toggle switches (owner, 2026-11-26):** the toggles are FILN FLP12-FJ, rated 15 A 250 V,
  not Omval. The owner's part numbers are FLP12-FJ-1021P-L (latching) and FLP12-FJ-1021P-M
  (momentary). The switch covers come from HC Electrical Store. The metal push buttons
  (16 mm blue here; 22 mm green and power on B2 and A1) come from Hanhai Electric Appliances
  and Accessories Store.
- **Item 18:** its part number is corrected to 91223A412, McMaster's M3 × 6 mm ultra-low-profile
  socket head screw (owner, 2026-11-26). Fusion gave 91223QA412.
- **Base drawing:** the base, KC-01-1031, is unchanged and stays at Rev B (`doc_rev`).

## Regenerating the drawings

```
python Documents/Drawings/tools/kc_strays.py Documents/Drawings/source/KC-01-1030/KC-01-1030.json --write
python Documents/Drawings/tools/kc_holes.py  Documents/Drawings/source/KC-01-1030/KC-01-1030.json --doc 1 --write
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1030/KC-01-1030.json --out <output folder>
```

Run the first two after a new Fusion export or production file; they rewrite the masks, hole
tags and hole table in the config.
