# KC-01-1010 Enclosure: drawing sources

Review notes for the enclosure drawings ahead of their migration to the KC frame: the acrylic box
KC-01-1010 (front plate 1011, side panel 1012, bottom plate 1013, back panel 1014), the top cover
plate KC-01-1060, the internal brace assembly KC-01-1070 (brace 1071) and the panel brace
KC-01-1080. The released PDFs are the Fusion drawings in `Documents/Drawings` (2022–2026). The
Fusion exports and the top-level assembly will be added here when the owner sends them.

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
