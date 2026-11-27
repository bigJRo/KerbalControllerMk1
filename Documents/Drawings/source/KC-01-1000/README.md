# KC-01-1000 Final Assembly: drawing sources

Source files for the top-level assembly drawing of the Kerbal Controller Mk1, KC-01-1000. Fusion
cannot export the whole assembly drawing at once, so it comes in parts; the KC drawing combines
them: parts 1 to 11 of the export, which fit every parts-list item. The released PDF will be in
`Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1000.json` | Package config: title, revision, status, view crops, placement and masks, parts list, step tables and notes | the drawing |
| `Fusion_KC-01-1000_Final_Assembly_Pt1_2026-11-26.pdf` | Fusion export part 1 (3 sheets): overall views (sheets 1 and 2) and the full parts list (sheet 3) | sheets 1 to 3 |
| `Fusion_KC-01-1000_Final_Assembly_Pt2_2026-11-26.pdf` to `_Pt11_` | Fusion export parts 2 to 11 (1 sheet each): one assembly step each, with its own parts list | sheets 4 to 13 |

## Layout of the combined drawing

- **Sheets 1 and 2:** the four overall views of part 1, each placed separately at 1.04: the side
  view in line with the front (rear) view, the top view under it, and the isometric view under the
  side view. The side view is centred between the frame and the front (rear) view, about 61 mm
  each side (owner, 2026-11-27). The isometric crop is clipped clear of the top view.
- **Sheet 3:** the parts list as a KC table, from part 1's sheet 3, and the notes. The isometric view on that sheet is
  left out (it repeats sheet 1).
- **Sheets 4 on:** one sheet per step, in assembly order, each with a step title and a table of the
  parts-list items it fits (from the Fusion step's own list). The Fusion tables, title blocks and
  centring marks are masked; each view is scaled (0.84 to 1.0) to fit. Steps 1 to 10: brackets;
  feet and internal brace; rear panel; standoffs; Control Module and Extension; panel brace and
  Panel A1; Panel A2; panel brace and Panel B1; Panel B2; top panel.

## Review notes (2026-11-27, parts 1 to 11)

Corrected in the draft:

- **Numbers in the KC form:** Fusion gives KC-01-0170 for the internal brace (it is KC-01-1070),
  "01-xxxx" for the Control Module, Extension, panel brace and panels (KC-01-xxxx), and its own
  auto number for the top panel (KC-01-1060).
- **Item 2:** Fusion's part number starts with a hidden left-to-right mark; the KC table drops it.
- **Items 8 and 9 (owner, 2026-11-27):** the fitted fan guard is the Bnafes B07NYDWPX6 and the
  filter cover the JAMHER B0G2WNNYL9 (magnetic). Fusion's QM092 and B07FYY6641 were stale.
- **Items 14 and 25 (owner, 2026-11-27):** Fusion listed the GX16 as one item under an auto
  number, with the hole plug's description. Item 14 is now the hole plug, qty 2 (one blank, one
  drilled Ø16 mm), and item 25 the GX16-10 connector fitted in it; step 3 notes the drilling.
- **Standoffs (items 15 to 17, owner, 2026-11-27):** all 16 are fitted; the Control Module and
  Extension use 8 and the other 8 are left free for future boards. Step 4 says so.
- **Items 21 and 28 (owner, 2026-11-27):** Fusion's brace description said "installed M3 brass
  inserts"; Rev B has none. Item 21 now says so, and new item 28 is the Dual Lock (3M SJ3560, as
  required) that holds the brace. Note 4 (sheet 3) gives the fitting, and steps 6 and 8 point to it.
- **Item 7:** Fusion's part name is "Arctic F12 fan"; the description and part number are the Antec
  Storm fan, so the name is now "Fan - 120mm PWM".
- **Quantities:** the steps add up to the parts list: 62 washers (item 4) and 54 1/2" screws
  (item 5), 20 + 6 + 8 + 4 + 7 + 4 + 7 + 6 and 20 + 6 + 4 + 7 + 4 + 7 + 6; both panel braces
  (steps 6 and 8).

For the manual (r19):

- **Manual Table 21-1:** the McMaster fasteners here (91772A148, 91772A160, 91841A007,
  92141A008) replace the Fastenal original-build rows; 94868A164 and 93475A210 are new, and
  92000A120 and 92000A114 gain 16 and 8.

## Regenerating the drawing

```
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1000/KC-01-1000.json --out <output folder>
```
