# KC-01-1000 Final Assembly: drawing sources

Source files for the top-level assembly drawing of the Kerbal Controller Mk1, KC-01-1000. Fusion
cannot export the whole assembly drawing at once, so it comes in parts; the KC drawing combines
them. Draft: parts 1 to 7 of the export; more parts follow. The released PDF will be in
`Documents/Drawings`.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1000.json` | Package config: title, revision, status, view crops, placement and masks, parts list, step tables and notes | the drawing |
| `Fusion_KC-01-1000_Final_Assembly_Pt1_2026-11-26.pdf` | Fusion export part 1 (3 sheets): overall views (sheets 1 and 2) and the full parts list (sheet 3) | sheets 1 to 3 |
| `Fusion_KC-01-1000_Final_Assembly_Pt2_2026-11-26.pdf` to `_Pt7_` | Fusion export parts 2 to 7 (1 sheet each): one assembly step each, with its own parts list | sheets 4 to 9 |

## Layout of the combined drawing

- **Sheets 1 and 2:** the overall views of part 1, scaled to sit between the KC revision and title blocks.
  Sheet 1 carries the notes.
- **Sheet 3:** the parts list as a KC table, from part 1's sheet 3. The isometric view on that sheet is
  left out (it repeats sheet 1).
- **Sheets 4 on:** one sheet per step, in assembly order, each with a step title and a table of the
  parts-list items it fits (from the Fusion step's own list). The Fusion tables, title blocks and
  centring marks are masked; each view is scaled (0.86 to 1.0) to fit.
## Review notes (2026-11-27, parts 1 to 7)

Corrected in the draft:

- **Numbers in the KC form:** Fusion gives KC-01-0170 for the internal brace (it is KC-01-1070),
  "01-xxxx" for the Control Module, Extension, panel brace and panels (KC-01-xxxx), and its own
  auto number for the top panel (KC-01-1060).
- **Item 2:** Fusion's part number starts with a hidden left-to-right mark; the KC table drops it.
- **Items 8 and 9 (owner, 2026-11-27):** the fitted fan guard is the Bnafes B07NYDWPX6 and the
  filter cover the JAMHER B0G2WNNYL9 (magnetic). Fusion's QM092 and B07FYY6641 were stale.
- **Item 7:** Fusion's part name is "Arctic F12 fan"; the description and part number are the Antec
  Storm fan, so the name is now "Fan - 120mm PWM".

Open (for the owner):

- **Item 25 (GX16):** a Fusion auto number, with the hole plug's description. It is the 22 mm hole
  plug drilled Ø16 mm carrying the GX16-10 connector.
- **Item 21 (panel brace):** the description still says "installed M3 brass inserts"; Rev B has no
  inserts and is held with Dual Lock, which is not in the parts list.
- **Standoffs (items 15 to 17):** 16 are fitted, but the Control Module and Extension use 8.
- **Quantities so far:** parts 2 to 7 use 38 of the 62 washers (item 4) and 30 of the 54 1/2"
  screws (item 5); the rest should appear in the remaining parts.
- **Manual Table 21-1:** the McMaster fasteners here (91772A148, 91772A160, 91841A007,
  92141A008) replace the Fastenal original-build rows; 94868A164 and 93475A210 are new, and
  92000A120 and 92000A114 gain 16 and 8.

## Regenerating the drawing

```
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1000/KC-01-1000.json --out <output folder>
```
