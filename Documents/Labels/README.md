# KCMk1 Panel Label Artwork

Printed artwork for the button labels, keycap legends and 7-segment window graphics of
panels A2 and B2. Each file is a 600 dpi PNG on a transparent background (the 7-segment
graphics are on black), at the finished size of the label.

| Set | Folder | Size | Style |
|---|---|---|---|
| Action Control AG 1–12 | `A2/Action_Control_0x22` | 14.5 × 14.5 mm | Bold 12 pt, black, two rows ("AG" / number) |
| UI Control | `A2/UI_Control_0x20` | 14.5 × 14.5 mm | Bold 12 pt, black; arrows as solid triangles |
| Function Control | `A2/Function_Control_0x21` | 25.5 × 14.5 mm | Bold 12 pt, black; long words set smaller to fit |
| Translation joystick buttons | `A2/Translation_Joystick_0x29` | 14.5 × 14.5 mm | Bold 12 pt, black |
| GPWS keycaps (MBK 1u) | `A2/GPWS_0x2A` | 17.5 × 16.5 mm | Regular 12 pt, white, upper legend position |
| GPWS 7-segment window | `A2/GPWS_0x2A` | 40.4 × 16 mm | LED green on black |
| Stability Control | `B2/Stability_Control_0x23` | 14.5 × 14.5 mm | KSP navball markers, in two sets: `colour` and `black`; HOLD and INVERT as text |
| Vehicle Control | `B2/Vehicle_Control_0x24` | 25.5 × 14.5 mm | Bold 12 pt, black |
| Time Control | `B2/Time_Control_0x25` | 14.5 × 14.5 mm | Bold 12 pt, black; arrows as solid triangles |
| AUX CTRL | `B2/AUX_CTRL_0x26` | 14.5 × 14.5 mm | Bold 12 pt, black |
| Rotation joystick buttons | `B2/Rotation_Joystick_0x28` | 14.5 × 14.5 mm | Bold 12 pt, black |
| Pre-Warp Time keycaps (MBK 1u) | `B2/PreWarp_Time_0x2B` | 17.5 × 16.5 mm | Regular 12 pt, white, upper legend position |
| Pre-Warp Time 7-segment window | `B2/PreWarp_Time_0x2B` | 40.4 × 16 mm | LED green on black |

## File names

- **B2 sets:** named by the module's button input, `B00`–`B11` (Hardware Inventory §10), or `BTN01` / `BTN02` on the joystick. `_layout.png` in each folder is a preview of the labels in their panel positions.
- **A2 sets:** numbered in panel order, row by row, left to right as seen from the front (for example `01_ENA` … `06_SCREEN_SHOT` on the top row of UI Control).
- **7-segment graphics:** `GPWS_7seg_200.png` and `PreWarp_7seg_60.png` show the default values. `GPWS_7seg_8888_fit.png` shows `8.8.8.8.` to check that every digit and decimal point fits the window; it serves both modules.

The Action Control set replaces an earlier set labelled "AP", and the Stability Control icons replace an earlier text-only set.

## Typeface

The panel typeface is **Futura PT**. Futura PT is a commercial font and was not available when this artwork was made. The labels were therefore rendered with **Jost**, an open-source typeface modelled on Futura: Jost Bold for the button labels and Jost Regular for the keycap legends. To produce the artwork in Futura PT, run the scripts below with the Futura PT font file.

## Regenerating

The scripts in `tools/` produced this artwork. They need Python 3 with Pillow.

| Script | Produces | Usage |
|---|---|---|
| `ag_labels.py` | Action Control | `ag_labels.py FONT OUTDIR` (bold weight fixed at 700) |
| `btn_labels.py` | UI Control | `btn_labels.py FONT OUTDIR` |
| `cam_labels.py` | Translation joystick buttons | `cam_labels.py FONT OUTDIR` |
| `wide_labels.py` | Function Control | `wide_labels.py FONT OUTDIR` |
| `mbk_labels.py` | GPWS keycaps | `mbk_labels.py FONT OUTDIR [--weight 400]` |
| `mbk_warp_labels.py` | Pre-Warp Time keycaps | `mbk_warp_labels.py FONT OUTDIR [--weight 400]` |
| `b2_labels.py` | Vehicle, Time, AUX CTRL and Rotation joystick, with layout previews | `b2_labels.py FONT OUTDIR` |
| `sas_labels.py` | Stability Control, both sets, with layout previews | `sas_labels.py FONT OUTDIR` |
| `seg7.py` | 7-segment window graphic | `seg7.py VALUE OUT.png` |

`FONT` is the path to a `.ttf` / `.otf` file. With a variable font, the keycap scripts take `--weight` to select the weight. Some scripts' usage lines still give the name of the script they were copied from.
