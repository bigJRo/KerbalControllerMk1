# KC-01-1010 Enclosure: drawing sources

Source files for the Rev A drawings of the acrylic box: assembly KC-01-1010 and its plates, the
front plate KC-01-1011, side panel 1012, bottom plate 1013 and back panel 1014 (part control
drawings). Rev A puts the released 2022 Fusion drawings (revision "-", in git history) in the KC
frame; the Fusion PDFs themselves are the sources. This file also keeps the review notes for the
rest of the enclosure: the top panel (now KC-01-1060 / 1061, own package), the internal brace
KC-01-1070 / 1071 (own package) and the panel brace KC-01-1080.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1010.json` | Package config: document types, titles, revisions, release status, view crops, clips and masks, labels, line weight, parts list, hole tables and notes | all five drawings |
| `Fusion_KC-01-1010_Acrylic_Box_2022-09-18.pdf` | The released Fusion assembly drawing (2 sheets) | 1010 ASSY |
| `Fusion_KC-01-1011_Front_Plate_2022-09-17.pdf` | The released Fusion front plate drawing (1 sheet) | 1011 PART |
| `Fusion_KC-01-1012_Side_Panel_2022-09-18.pdf` | The released Fusion side panel drawing (2 sheets) | 1012 PART |
| `Fusion_KC-01-1013_Bottom_Plate_2022-09-18.pdf` | The released Fusion bottom plate drawing (2 sheets) | 1013 PART |
| `Fusion_KC-01-1014_Back_Panel_2022-09-18.pdf` | The released Fusion back panel drawing (3 sheets) | 1014 PART |

## Migration notes (2026-11-26)

- **Views:** each group of orthographic views is one crop of its Fusion sheet, so its
  projection layout and its Fusion dimensions are kept.
  - The isometric views are separate crops, clipped along their edges (`clip`) so the
    neighbouring views stay out.
  - Fusion's large "Front View" titles are masked and replaced by KC labels (`texts`).
- **Tables:** the parts list and the hole tables are KC tables, re-set from the Fusion text.
  The hole tags are Fusion's, so they match the views. "Ø4.76 x 6.00" (depth = the plate
  thickness) is written Ø4.76 THRU.
- **Part names:** they drop Fusion's model versions (v2, v3). The back panel is v2, as the 1014
  drawing has it; the old 1010 parts list said v3.
- **Notes:** the Fusion notes ("Apply Acryllic Glue along all joints", "Finish exposed edges")
  become note 1 of 1010. Material and outline are notes on each plate.
- **Scales:** the 1010 sheet 2, 1012 sheet 1 and 1013 sheet 2 views are shown at 0.8 of
  their Fusion scale (1:5, 2:5 and 1:5) to fit beside the KC title and revision blocks.

## Review notes (2026-11-25)

**Checked and consistent:**

- The box's inside width of 850 mm takes two 425 mm panels side by side.
- The side profile's 276 mm flat is the 270 mm input panel (A2, B2) plus the 6 mm front plate.
  Its 115.5 mm slope carries the 115.2 mm display panels (A1, B1). Its 78.3 mm top flat matches
  the top cover.
- Bottom plate holes A3, A4, A7, A8 and A9, A10, A13, A14 (Ø3.18) are two 320 × 80 mm patterns.
  They match the 4 × Ø3.50 mounting holes of the Control Module and the Ctrl Module Extension
  (330 × 90 mm boards, holes 5 mm in from each corner).
- The back panel's 2 × Ø116 cut-outs are the two fans (Ctrl Module CN1, Extension CN3).

**Owner's answers (2026-11-25):**

- **Bottom plate:**
  - Holes A1, A2, A5, A6 and A11, A12, A15, A16 (two 230 × 90 mm patterns) are not used. They
    were for a board that was dropped.
  - B1 to B4 (Ø4.76) fix the internal brace.
- **Back panel:**
  - The current back panel is v2, as in the 1014 drawing. The 1010 parts list says v3 and is
    wrong.
  - Its five Ø22.7 holes take panel-mount inserts:

    | Insert | Part |
    |---|---|
    | 12 V power jack | Adafruit 5607 |
    | USB-C to the PC | Adafruit 4259 |
    | USB-C to the expansion | Adafruit 4259 |
    | Close-out plug (blank) | Amazon B07DYWNZHY |
    | GX16-10 expansion connector | Close-out plug B07DYWNZHY, modified with a Ø16 mm hole |
- **Top cover (1060):** it is screwed down, not glued, despite the drawing note. The part has
  been modified since the 2022 drawing; the owner will supply the new file and diagrams.
- **Top-level assembly:** the owner has started a Fusion assembly of the whole controller. It
  needs updating before export. It is the place for the panel and enclosure fasteners and the
  back-panel inserts, which no drawing lists today.
- **Speaker:** the Annunciator's speaker (701715519671, driven by the DFPlayer on A1) mounts in
  the enclosure and goes on the top-level assembly (owner, 2026-11-26).

**To fix on migration:**

- **1070:** item 4 is named 6-32 × 1-1/4" but described as 8-32 thread. The brackets
  (Keystone 617) and nuts are 6-32.
- **1080:** the parts list has only the 2 heat-set inserts. The brace's own material and the
  number fitted (and where) are not recorded; the top-level assembly should show them.
- **Names and suppliers:**
  - Remove the Fusion version suffixes from part names (v1, v2, v3, v18).
  - Use one spelling of the supplier, TAP Plastics.
- **Revisions:**
  - Revision "-" on released parts becomes Rev A.
  - 1071 Rev C is dated as approved (2022-09-29) after its issue date (2022-09-27).
