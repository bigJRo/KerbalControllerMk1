#!/usr/bin/env python3
"""Button labels: 14.5 x 14.5 mm, 12 pt bold, black on transparent, text centred both ways
(by its ink, one or two rows). Arrow labels are solid triangles.
A label whose widest row would not fit with a 5 % margin is set smaller, alone.
Usage: btn_labels.py FONT OUTDIR"""
import sys, os
from PIL import Image, ImageDraw, ImageFont

font_path, out = sys.argv[1], sys.argv[2]
SIZE_MM, PX = 14.5, 343
DPI = PX / (SIZE_MM / 25.4)
EM = round(12 / 72 * DPI)          # 12 pt
LEAD = 1.2
TRI_H, TRI_W = 150, 130            # arrow: about 45 % of the label's height, as in the drawing

LABELS = [  # file name, rows (or an arrow), in the drawing's order: top row, then bottom row
    ("01_ENA", ["ENA"]), ("02_LEFT", "<"), ("03_RIGHT", ">"), ("04_RESET", ["RESET"]),
    ("05_UI", ["UI"]), ("06_SCREEN_SHOT", ["SCREEN", "SHOT"]),
    ("07_IVA", ["IVA"]), ("08_LEFT", "<"), ("09_RIGHT", ">"), ("10_CYCLE_NAV", ["CYCLE", "NAV"]),
    ("11_NAV_BALL", ["NAV", "BALL"]), ("12_DEBUG", ["DEBUG"]),
]

f = ImageFont.truetype(font_path, EM)
try:
    f.set_variation_by_axes([700])
except Exception:
    pass
os.makedirs(out, exist_ok=True)
INK = (0, 0, 0, 255)
for name, rows in LABELS:
    im = Image.new("RGBA", (PX, PX), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    if isinstance(rows, str):
        cx, cy, hw, hh = PX / 2, PX / 2, TRI_W / 2, TRI_H / 2
        pts = [(cx + hw, cy), (cx - hw, cy - hh), (cx - hw, cy + hh)] if rows == ">" else \
              [(cx - hw, cy), (cx + hw, cy - hh), (cx + hw, cy + hh)]
        d.polygon(pts, fill=INK)
    else:
        # 12 pt, unless the widest row would leave less than a 5 % margin each side:
        # then that label alone is set smaller until it fits
        em, ff = EM, f
        while max(d.textlength(t, font=ff) for t in rows) > PX * 0.90:
            em -= 1
            ff = ImageFont.truetype(font_path, em)
            try: ff.set_variation_by_axes([700])
            except Exception: pass
        if em != EM: print("%s: %.1f pt to fit" % (name, em * 72 / DPI))
        f_, pitch = ff, round(em * LEAD)
        boxes = [d.textbbox((0, 0), t, font=f_, anchor="ls") for t in rows]
        top = min(b[1] for b in boxes[:1])
        bottom = (len(rows) - 1) * pitch + boxes[-1][3]
        y0 = (PX - (bottom - top)) / 2 - top
        for i, (t, b) in enumerate(zip(rows, boxes)):
            x = (PX - (b[2] - b[0])) / 2 - b[0]
            d.text((x, y0 + i * pitch), t, font=f_, fill=INK, anchor="ls")
    im.save(os.path.join(out, name + ".png"), dpi=(DPI, DPI))
print("ok", EM, "%.2f dpi" % DPI)
