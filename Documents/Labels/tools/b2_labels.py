#!/usr/bin/env python3
"""Button labels for the B2 panel modules (Hardware Inventory §10). The AG-label style:
black bold 12 pt on transparent, centred both ways by the ink, one or two rows; a
label whose widest row would leave less than a 5 % margin each side is set smaller,
alone. "<" / ">" are drawn as solid triangles, as the UI Control arrows.
Each file is named by its button input (B00..B11). A preview sheet per module lays
the labels out as the buttons sit on the panel, viewed from the front.
Usage: b2_labels.py FONT OUTDIR"""
import sys, os
from PIL import Image, ImageDraw, ImageFont

font_path, outdir = sys.argv[1], sys.argv[2]
DPI = 600
EM = round(12 / 72 * DPI)
LEAD = 1.2
INK = (0, 0, 0, 255)

SQUARE = (14.5, 14.5)       # Button Module (1801) and the joystick buttons
WIDE = (25.5, 14.5)         # Wide Button Module (1811), as the Function Control set
# Panel layouts, viewed from the front (Inventory §10): rows of button inputs.
GRID_1801 = [[10, 8, 6, 4, 2, 0], [11, 9, 7, 5, 3, 1]]
GRID_1811 = [[1, 0], [3, 2], [5, 4], [7, 6], [9, 8], [11, 10]]

MODULES = [
  ("Stability_Control_0x23", SQUARE, GRID_1801, {
    0: ["TARGET"], 1: ["ANTI", "TARGET"], 2: ["RADIAL", "IN"], 3: ["RADIAL", "OUT"],
    4: ["NORMAL"], 5: ["ANTI", "NORMAL"], 6: ["PRO", "GRADE"], 7: ["RETRO", "GRADE"],
    8: ["STABILITY", "ASSIST"], 9: ["MANEUVER"], 10: ["HOLD"], 11: ["INVERT"]}),
  ("Time_Control_0x25", SQUARE, GRID_1801, {
    0: ["ApA"], 1: ["STOP"], 2: ["PeA"], 3: ["PHYS"], 4: ["MNVR"], 5: ">",
    6: ["SOI"], 7: "<", 8: ["MORN"], 9: ["LOAD"], 10: ["PAUSE"], 11: ["SAVE"]}),
  ("AUX_CTRL_0x26", SQUARE, GRID_1801, {
    0: ["EVA", "LIGHTS"], 1: ["JET", "PACK"], 2: ["BOARD"], 3: ["CONST"], 4: ["JUMP /", "LET GO"],
    5: ["GRAB"], 6: ["EVA", "CHUTE"], 7: ["HELMET"], 8: ["UN", "DOCK"], 9: ["FLAG"], 10: ["PRI"], 11: ["ALT"]}),
  ("Vehicle_Control_0x24", WIDE, GRID_1811, {
    0: ["BRAKES"], 1: ["LIGHTS"], 2: ["ANTENNA"], 3: ["LANDING", "GEAR"], 4: ["FUEL", "CELL"],
    5: ["SOLAR", "ARRAY"], 6: ["CARGO", "DOOR"], 7: ["RADIATOR"], 8: ["HEAT", "SHIELD"],
    9: ["LADDER"], 10: ["MAIN"], 11: ["DROGUE"]}),
  ("Rotation_Joystick_0x28", SQUARE, [[1, 2]], {
    1: ["RESET", "TRIM"], 2: ["TRIM", "SET"]}),
]

def font(em):
    f = ImageFont.truetype(font_path, em)
    try: f.set_variation_by_axes([700])
    except Exception: pass
    return f

def label(size_mm, rows):
    W, H = round(size_mm[0] / 25.4 * DPI), round(size_mm[1] / 25.4 * DPI)
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    note = ""
    if isinstance(rows, str):                     # an arrow: a solid triangle, 45 % of the height
        th, tw = H * 0.44, H * 0.38
        cx, cy = W / 2, H / 2
        pts = [(cx + tw / 2, cy), (cx - tw / 2, cy - th / 2), (cx - tw / 2, cy + th / 2)] if rows == ">" else \
              [(cx - tw / 2, cy), (cx + tw / 2, cy - th / 2), (cx + tw / 2, cy + th / 2)]
        d.polygon(pts, fill=INK)
        return im, (W, H), note
    em, f = EM, font(EM)
    while max(d.textlength(t, font=f) for t in rows) > W * 0.90:
        em -= 1; f = font(em)
    if em != EM: note = "%.1f pt" % (em * 72 / DPI)
    pitch = round(em * LEAD)
    boxes = [d.textbbox((0, 0), t, font=f, anchor="ls") for t in rows]
    top = boxes[0][1]
    bottom = (len(rows) - 1) * pitch + boxes[-1][3]
    y0 = (H - (bottom - top)) / 2 - top
    for i, (t, b) in enumerate(zip(rows, boxes)):
        d.text(((W - (b[2] - b[0])) / 2 - b[0], y0 + i * pitch), t, font=f, fill=INK, anchor="ls")
    return im, (W, H), note

for name, size, grid, labels in MODULES:
    out = os.path.join(outdir, name)
    os.makedirs(out, exist_ok=True)
    ims = {}
    for b, rows in sorted(labels.items()):
        im, (W, H), note = label(size, rows)
        text = rows if isinstance(rows, str) else " ".join(rows)
        fn = "B%02d_%s.png" % (b, "".join(c if c.isalnum() else "_" for c in text).strip("_").replace("__", "_") or ("RIGHT" if rows == ">" else "LEFT"))
        if rows in (">", "<"): fn = "B%02d_%s.png" % (b, "FASTER" if rows == ">" else "SLOWER")
        if name.startswith("Rotation"): fn = "BTN%02d_%s.png" % (b, "_".join(rows))
        im.save(os.path.join(out, fn), dpi=(W / (size[0] / 25.4), H / (size[1] / 25.4)))
        ims[b] = im
        if note: print("  %s %s: %s to fit" % (name, text, note))
    # preview: the panel layout, labels on grey caps
    gap = 30
    W, H = ims[next(iter(ims))].size
    cols, rws = len(grid[0]), len(grid)
    sheet = Image.new("RGBA", (cols * (W + gap) + gap, rws * (H + gap) + gap), (255, 255, 255, 255))
    for r, row in enumerate(grid):
        for c, b in enumerate(row):
            x, y = gap + c * (W + gap), gap + r * (H + gap)
            sheet.paste((215, 215, 215, 255), (x, y, x + W, y + H))
            sheet.alpha_composite(ims[b], (x, y))
    sheet.convert("RGB").save(os.path.join(outdir, name + "_layout.png"))
    print(name, len(ims), "labels", "%.1f x %.1f mm" % size)
