# KC-01-1030 Panel A2: drawing sources

Source files for the Rev B drawings of Panel A2, the left input panel: assembly KC-01-1030 and
base plate KC-01-1031 (part control drawing). Rev B supersedes the Rev A Fusion PDFs, which are
in git history. The released PDFs are in `Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1030.json` | Package config: document types, titles, revisions, release status, view crops, masks over stray parts, balloon relabels, parts list and notes | both drawings |
| `Fusion_KC-01-1030_Panel_A2_Complete_2026-11-25.pdf` | Fusion assembly drawing export (4 sheets): views, balloons and the Fusion parts list | 1030 ASSY |
| `Fusion_KC-01-1031_Panel_A2_Base_2026-11-25.pdf` | Fusion base plate drawing export (2 sheets): views and overall dimensions | 1031 PART sheet 1 |
| `Panel_A2_Production_2026-11-25.ai` | Ponoko production (cut and engrave) file, Illustrator, artwork at 1:1 (red = cut, blue = engrave) | 1031 PART sheet 2; fabrication |

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
  several views; the config masks them (`masks`, in source-page mm).
- Switch Group 1 is engraved MSTR · DISPL · SCE · UPTLM / LTG · A/P · ENGINE · THRTL (B16–B23).
  DISPL and RST are the panel abbreviations of DISP and RESET (Module UI Reference §6.2).
- The stick is the JH-DX400B-R4 (4-axis, three axes used). The fasteners in the parts list are
  complete (owner).
- Sheet 2 of the base drawing is drawn from the archived production file, not from the Fusion
  export (whose sheet 2 image was out of date).

## Regenerating the drawings

```
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1030/KC-01-1030.json --out <output folder>
```
