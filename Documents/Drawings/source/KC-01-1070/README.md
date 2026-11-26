# KC-01-1070 Internal Brace: drawing sources

Source files for the KC-frame drawings of the internal brace: assembly KC-01-1070 (Rev A) and
the brace itself, KC-01-1071 (Rev D, part control drawing). They put the released Fusion
drawings in the KC frame: `KC-01-1070_Rev-_Internal Brace Assembly.pdf` and
`KC-01-1071_RevC_Internal Brace.pdf`, which are in git history once replaced. The Fusion PDFs
themselves are the sources.

| File | What it is | Feeds |
|---|---|---|
| `KC-01-1070.json` | Package config: document types, titles, revisions (with the brace's own history), release status, view crops, clips and masks, labels, line weight, parts list, hole table and notes | both drawings |
| `Fusion_KC-01-1070_Internal_Brace_Assembly_2025-05-26.pdf` | The released Fusion assembly drawing (2 sheets): views, balloons and the Fusion parts list | 1070 ASSY |
| `Fusion_KC-01-1071_Internal_Brace_2022-09-27.pdf` | The released Fusion brace drawing (2 sheets): views and overall dimensions; hole locations and hole table | 1071 PART |

## Migration notes (2026-11-26)

- **Revisions:**
  - The assembly was released at revision "-" and becomes Rev A.
  - The brace keeps its own letters (`doc_rev`, `doc_revisions`): Fusion Rev C (issued
    2022-09-27; revisions A and B are in Fusion) becomes Rev D.
- **Views:** each group of orthographic views is one crop of its Fusion sheet, so its
  projection layout and its Fusion dimensions are kept.
  - The isometric views are separate crops, clipped along their edges so the neighbouring views
    stay out.
  - The brace's views are at 0.8 of their Fusion 1:2, so 2:5, to fit beside the KC title and
    revision blocks. The assembly's sheet 1 stays at 1:2; its sheet 2 is not to scale.
  - Sheet 2 of the brace masks the corner of the Fusion hole table that sits above the hole
    locations view.
- **Tables:** the parts list and the hole table are KC tables, re-set from the Fusion text.
  The hole tags are Fusion's (A1 to A7, Ø4.76), so they match the view.
- **Part names:** they drop Fusion's model versions and give the supplier one spelling, TAP
  Plastics.
- **Item 4 (screw):** Fusion named it 6-32 × 1-1/4" but described it as 8-32 thread. The
  brackets (Keystone 617) and the nuts are 6-32, so the description is now 6-32. The owner
  confirmed 6-32 and PN 91772A201 (2026-11-26).
- **Notes:** the assembly notes say how the brackets, screw, washer and nut stack at each hole.
  The brackets' own screws into the box are left to the top-level assembly.

## Regenerating the drawings

```
python Documents/Drawings/tools/kc_package.py \
    Documents/Drawings/source/KC-01-1070/KC-01-1070.json --out <output folder>
```
