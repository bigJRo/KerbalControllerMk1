#!/usr/bin/env python3
"""MBK 1u keycap legends: 17.5 x 16.5 mm, white text on transparent, 12 pt regular,
centred horizontally, the text's ink centred at LEGEND_Y of the height (the upper
legend position, as in the owner's drawing).
Usage: mbk_labels.py FONT OUTDIR [--weight 400]"""
import sys, os
from PIL import Image, ImageDraw, ImageFont

font_path, out = sys.argv[1], sys.argv[2]
WEIGHT = int(sys.argv[sys.argv.index("--weight") + 1]) if "--weight" in sys.argv else 400
W_MM, H_MM, DPI = 17.5, 16.5, 600
W, H = round(W_MM / 25.4 * DPI), round(H_MM / 25.4 * DPI)
EM = round(12 / 72 * DPI)                 # 12 pt
LEGEND_Y = 0.35                           # ink centre, from the top (the drawing: about a third down)
INK = (255, 255, 255, 255)
LABELS = [("01_ENA", "ENA"), ("02_PROX", "PROX"), ("03_RNDZ", "RNDZ")]

f = ImageFont.truetype(font_path, EM)
try:
    f.set_variation_by_axes([WEIGHT])
except Exception:
    pass
os.makedirs(out, exist_ok=True)
for name, text in LABELS:
    im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    l, t, r, b = d.textbbox((0, 0), text, font=f, anchor="ls")
    x = (W - (r - l)) / 2 - l
    y = H * LEGEND_Y - (t + b) / 2
    d.text((x, y), text, font=f, fill=INK, anchor="ls")
    im.save(os.path.join(out, name + ".png"), dpi=(W / (W_MM / 25.4), H / (H_MM / 25.4)))
print("%d x %d px, weight %d" % (W, H, WEIGHT))
