#!/usr/bin/env python3
"""4-digit 7-segment graphic, 40.4 x 16 mm: LED green on black, unlit segments faintly
visible, leading zeros blank (as Kerbal7SegmentCore). The layout follows the owner's
reference drawing, so a full "8.8.8.8." -- four digits and four decimal points -- fits across.

Usage: seg7.py VALUE OUT.png      e.g. 200
       seg7.py 8.8.8.8. OUT.png   every segment and decimal point lit (fit check)"""
import sys
from PIL import Image, ImageDraw, ImageFilter

arg, out = sys.argv[1], sys.argv[2]
W_MM, H_MM, DPI = 40.4, 16.0, 600
W, H = round(W_MM / 25.4 * DPI), round(H_MM / 25.4 * DPI)
SS = 4                                            # supersample, then down for smooth edges
w, h = W * SS, H * SS

LIT = (70, 255, 90, 255)                          # LED green
OFF = (7, 16, 8, 255)                             # unlit segment, just visible on black
BG = (0, 0, 0, 255)                               # solid black

# Proportions from the owner's reference drawing (1121 x 454 px), as fractions of the
# window and of the digit height:
DH = h * 0.612                                    # digit height (278 / 454)
y0 = h * 0.198                                    # digit top (90 / 454)
DW = DH * 0.54                                    # upright digit width (150 / 278)
T = DH * 0.095                                    # segment thickness (26 / 278)
GAP = T * 0.10                                    # gap between segments
SLANT = 0.155                                     # x shift per unit of height (43 / 278, about 9 deg)
CELL = w * 0.2487                                 # digit pitch (279 / 1121)
x0 = w * 0.040                                    # bottom-left of the first digit (45 / 1121)
R_DP = DH * 0.047                                 # decimal point radius (13 / 278)
DP_DX = DH * 0.13                                 # decimal point centre, right of the digit's bottom-right corner
def seg_poly(cx, cy, length, horizontal):
    hl, ht = length / 2 - GAP, T / 2
    if horizontal:
        return [(cx - hl, cy), (cx - hl + ht, cy - ht), (cx + hl - ht, cy - ht), (cx + hl, cy),
                (cx + hl - ht, cy + ht), (cx - hl + ht, cy + ht)]
    return [(cx, cy - hl), (cx + ht, cy - hl + ht), (cx + ht, cy + hl - ht), (cx, cy + hl),
            (cx - ht, cy + hl - ht), (cx - ht, cy - hl + ht)]

def digit_segments(left, top):
    xl, xr = left + T / 2, left + DW - T / 2
    yt, ym, yb = top + T / 2, top + DH / 2, top + DH - T / 2
    hw, hv = xr - xl, ym - yt
    segs = {
        "a": seg_poly((xl + xr) / 2, yt, hw, True), "b": seg_poly(xr, (yt + ym) / 2, hv, False),
        "c": seg_poly(xr, (ym + yb) / 2, hv, False), "d": seg_poly((xl + xr) / 2, yb, hw, True),
        "e": seg_poly(xl, (ym + yb) / 2, hv, False), "f": seg_poly(xl, (yt + ym) / 2, hv, False),
        "g": seg_poly((xl + xr) / 2, ym, hw, True),
    }
    def slant(p):
        x, y = p
        return (x + (top + DH - y) * SLANT, y)
    out = {k: [slant(p) for p in v] for k, v in segs.items()}
    cx, cy = (left + DW + DP_DX, top + DH - R_DP)    # bottom row: no slant offset
    out["dp"] = [(cx - R_DP, cy - R_DP), (cx + R_DP, cy + R_DP)]
    return out

FONT = {0: "abcdef", 1: "bc", 2: "abdeg", 3: "abcdg", 4: "bcfg", 5: "acdfg", 6: "acdefg",
        7: "abc", 8: "abcdefg", 9: "abcdfg"}

if arg == "8.8.8.8.":
    cells = [("abcdefg", True)] * 4
    label = "8.8.8.8."
else:
    v = max(0, min(9999, int(arg)))
    digits = [v // 1000, v // 100 % 10, v // 10 % 10, v % 10]
    cells, leading = [], True
    for i, d in enumerate(digits):
        if leading and d == 0 and i < 3: cells.append(("", False))
        else: leading = False; cells.append((FONT[d], False))
    label = "".join(" " if not c[0] else "x" for c in cells)

bg = Image.new("RGBA", (w, h), BG)
off = Image.new("RGBA", (w, h), (0, 0, 0, 0))
lit = Image.new("RGBA", (w, h), (0, 0, 0, 0))
do, dl = ImageDraw.Draw(off), ImageDraw.Draw(lit)
right = 0
for i, (on, dp) in enumerate(cells):
    segs = digit_segments(x0 + i * CELL, y0)
    for k, poly in segs.items():
        if k == "dp":
            (dl if dp else do).ellipse(poly, fill=LIT if dp else OFF)
            right = max(right, poly[1][0])
            continue
        (dl if k in on else do).polygon(poly, fill=LIT if k in on else OFF)
        right = max(right, max(p[0] for p in poly))

glow = lit.filter(ImageFilter.GaussianBlur(T * 0.45))
g = glow.split()
glow = Image.merge("RGBA", (g[0], g[1], g[2], g[3].point(lambda a: int(a * 0.55))))
img = Image.alpha_composite(Image.alpha_composite(Image.alpha_composite(bg, off), glow), lit)
img = img.resize((W, H), Image.LANCZOS)
img.save(out, dpi=(W / (W_MM / 25.4), H / (H_MM / 25.4)))
print("%s: %d x %d px; digits %.1f x %.1f mm; 8.8.8.8. spans %.1f-%.1f mm of %.1f" % (
    out, W, H, DW / SS / DPI * 25.4, DH / SS / DPI * 25.4, x0 / SS / DPI * 25.4, right / SS / DPI * 25.4, W_MM))
