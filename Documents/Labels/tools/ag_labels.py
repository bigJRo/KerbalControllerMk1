#!/usr/bin/env python3
"""Button labels: 14.5 x 14.5 mm, two centred rows ("AG" / 1..12), 12 pt bold, black on
transparent. Usage: ap_labels.py FONT OUTDIR [--weight 700]"""
import sys, os
from PIL import Image, ImageDraw, ImageFont

font_path, out = sys.argv[1], sys.argv[2]
SIZE_MM, PX = 14.5, 343                       # 343 px across 14.5 mm (~601 dpi)
DPI = PX / (SIZE_MM / 25.4)
PT = 12
EM = round(PT / 72 * DPI)                     # 12 pt in pixels at this resolution
LEAD = 1.2                                    # line pitch, x the em

f = ImageFont.truetype(font_path, EM)
try:
    f.set_variation_by_axes([700])            # variable font: Bold
except Exception:
    pass
os.makedirs(out, exist_ok=True)
for n in range(1, 13):
    im = Image.new("RGBA", (PX, PX), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    rows = ["AG", str(n)]
    pitch = round(EM * LEAD)
    # ink boxes at a baseline of y = 0 (anchor "ls"), so both rows share one baseline grid
    boxes = [d.textbbox((0, 0), t, font=f, anchor="ls") for t in rows]
    top = boxes[0][1]
    bottom = pitch + boxes[1][3]
    y0 = (PX - (bottom - top)) / 2 - top     # first baseline, centring the ink block
    for i, (t, b) in enumerate(zip(rows, boxes)):
        x = (PX - (b[2] - b[0])) / 2 - b[0]
        d.text((x, y0 + i * pitch), t, font=f, fill=(0, 0, 0, 255), anchor="ls")
    im.save(os.path.join(out, "AG_%02d.png" % n), dpi=(DPI, DPI))
print("em", EM, "px; dpi %.2f" % DPI)
