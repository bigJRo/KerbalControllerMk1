#!/usr/bin/env python3
"""Stability Control (0x23) labels from the owner's guide: KSP navball markers for the
ten SAS modes, HOLD and INVERT as text (the AG style, bold 12 pt, set smaller to fit).
14.5 x 14.5 mm, transparent. Two sets: markers in the guide's KSP colours, and all black.
Usage: sas_labels.py FONT OUTDIR"""
import sys, os, math
from PIL import Image, ImageDraw, ImageFont

font_path, outdir = sys.argv[1], sys.argv[2]
DPI, MM = 600, 14.5
PX = round(MM / 25.4 * DPI)              # 343
SS = 4
S = PX * SS
BLACK = (0, 0, 0, 255)
# the guide's colours (KSP navball): prograde / retrograde yellow-green, normal purple,
# radial cyan, target magenta, maneuver and stability assist blue
COL = {"blue": (31, 61, 153, 255), "green": (176, 207, 52, 255), "purple": (137, 57, 145, 255),
       "cyan": (57, 186, 186, 255), "magenta": (156, 82, 163, 255)}

# Every marker is drawn from measurements of the owner's guide, in fractions of the label
# (0..1 across and down), with one stroke weight and flat line ends, as the guide.
LW = S * 0.058

def P(x, y):
    return (x * S, y * S)

def bl(d, p, q, col, w=1.0):              # a stroke with flat ends
    d.line([P(*p), P(*q)], fill=col, width=round(LW * w))

def ring(d, cx, cy, r, col, w=1.0):       # r: to the stroke's centre
    rr, half = r * S, LW * w / 2
    d.ellipse((cx * S - rr - half, cy * S - rr - half, cx * S + rr + half, cy * S + rr + half),
              outline=col, width=round(LW * w))

def dot(d, cx, cy, r, col):
    d.ellipse(((cx - r) * S, (cy - r) * S, (cx + r) * S, (cy + r) * S), fill=col)

def at(cx, cy, a_deg, r):
    a = math.radians(a_deg)
    return (cx + r * math.cos(a), cy - r * math.sin(a))

def tri(d, pts, col):
    d.polygon([P(*p) for p in pts], outline=col, width=round(LW))

def stability_assist(d, col):              # ring, a sine wave across it
    ring(d, 0.5, 0.5, 0.300, col)
    n = 400
    for i in range(n + 1):
        x = 0.335 + i * 0.33 / n
        y = 0.5 - 0.050 * math.sin(i / n * 2 * math.pi)
        d.ellipse((x * S - LW / 2, y * S - LW / 2, x * S + LW / 2, y * S + LW / 2), fill=col)

def prograde(d, col):                      # ring and dot, legs up, left and right
    cx, cy, r = 0.5, 0.58, 0.185
    ring(d, cx, cy, r, col)
    dot(d, cx, cy, 0.035, col)
    bl(d, (cx, cy - r), (cx, 0.20), col)
    bl(d, (cx - r, cy), (0.11, cy), col)
    bl(d, (cx + r, cy), (0.89, cy), col)

def retrograde(d, col):                    # ring with an X, legs up, down-left, down-right
    cx, cy, r = 0.5, 0.58, 0.190
    ring(d, cx, cy, r, col)
    k = (r - 0.015) / math.sqrt(2)
    bl(d, (cx - k, cy - k), (cx + k, cy + k), col, 0.80)
    bl(d, (cx - k, cy + k), (cx + k, cy - k), col, 0.80)
    bl(d, (cx, cy - r), (cx, 0.20), col)
    for a in (210, 330):
        bl(d, at(cx, cy, a, r), at(cx, cy, a, r + 0.20), col)

def normal(d, col):                        # a large triangle, point up, small dot
    tri(d, [(0.5, 0.15), (0.13, 0.82), (0.87, 0.82)], col)
    dot(d, 0.5, 0.60, 0.032, col)

def anti_normal(d, col):                   # triangle point down, dot, legs off the side midpoints
    top, bot, xl, xr = 0.38, 0.86, 0.255, 0.745
    pts = [(xl, top), (xr, top), (0.5, bot)]
    tri(d, pts, col)
    cy = top + (bot - top) / 3
    dot(d, 0.5, cy, 0.032, col)
    bl(d, (0.5, top), (0.5, 0.17), col)
    for (p, q) in ((pts[0], pts[2]), (pts[1], pts[2])):
        m = ((p[0] + q[0]) / 2, (p[1] + q[1]) / 2)
        a = 210 if p[0] < 0.5 else 330
        bl(d, m, at(m[0], m[1], a, 0.20), col)

def radial_in(d, col):                     # ring, four short ticks pointing in
    ring(d, 0.5, 0.5, 0.300, col)
    for a in (45, 135, 225, 315):
        bl(d, at(0.5, 0.5, a, 0.300), at(0.5, 0.5, a, 0.165), col)

def radial_out(d, col):                    # ring and dot, four ticks pointing out
    ring(d, 0.5, 0.5, 0.265, col)
    dot(d, 0.5, 0.5, 0.035, col)
    for a in (45, 135, 225, 315):
        bl(d, at(0.5, 0.5, a, 0.265), at(0.5, 0.5, a, 0.395), col)

def target(d, col):                        # ring broken at N, E, S, W; a dot
    r, gap = 0.290, 20
    box = (0.5 * S - r * S - LW / 2, 0.5 * S - r * S - LW / 2, 0.5 * S + r * S + LW / 2, 0.5 * S + r * S + LW / 2)
    for a0 in (0, 90, 180, 270):
        d.arc(box, start=a0 + gap / 2, end=a0 + 90 - gap / 2, fill=col, width=round(LW))
    dot(d, 0.5, 0.5, 0.070, col)

def anti_target(d, col):                   # a large dot, three spokes standing off it
    cx, cy = 0.5, 0.43
    dot(d, cx, cy, 0.068, col)
    for a, r0, r1 in ((150, 0.105, 0.315), (30, 0.105, 0.315), (270, 0.130, 0.380)):
        bl(d, at(cx, cy, a, r0), at(cx, cy, a, r1), col)

def maneuver(d, col):                      # a dot, three spokes each ending in a cross bar
    cx, cy = 0.5, 0.48
    dot(d, cx, cy, 0.035, col)
    for a in (90, 210, 330):
        tip = at(cx, cy, a, 0.285)
        bl(d, at(cx, cy, a, 0.075), tip, col)
        bl(d, at(tip[0], tip[1], a + 90, 0.170), at(tip[0], tip[1], a - 90, 0.170), col, 1.15)

ICONS = {   # input: (name, draw, colour)
    0: ("TARGET", target, "magenta"), 1: ("ANTI_TARGET", anti_target, "magenta"),
    2: ("RADIAL_IN", radial_in, "cyan"), 3: ("RADIAL_OUT", radial_out, "cyan"),
    4: ("NORMAL", normal, "purple"), 5: ("ANTI_NORMAL", anti_normal, "purple"),
    6: ("PROGRADE", prograde, "green"), 7: ("RETROGRADE", retrograde, "green"),
    8: ("STABILITY_ASSIST", stability_assist, "blue"), 9: ("MANEUVER", maneuver, "blue"),
}
TEXT = {10: "HOLD", 11: "INVERT"}
GRID = [[10, 8, 6, 4, 2, 0], [11, 9, 7, 5, 3, 1]]

def text_label(t):
    im = Image.new("RGBA", (PX, PX), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    em = round(12 / 72 * DPI)
    def f(e):
        ft = ImageFont.truetype(font_path, e)
        try: ft.set_variation_by_axes([700])
        except Exception: pass
        return ft
    ft = f(em)
    while d.textlength(t, font=ft) > PX * 0.90: em -= 1; ft = f(em)
    l, tp, r, b = d.textbbox((0, 0), t, font=ft, anchor="ls")
    d.text(((PX - (r - l)) / 2 - l, (PX - (b - tp)) / 2 - tp), t, font=ft, fill=BLACK, anchor="ls")
    return im, em * 72 / DPI

for setname, coloured in (("colour", True), ("black", False)):
    out = os.path.join(outdir, "Stability_Control_0x23_" + setname)
    os.makedirs(out, exist_ok=True)
    ims = {}
    for b, (name, fn, col) in ICONS.items():
        big = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        fn(ImageDraw.Draw(big), COL[col] if coloured else BLACK)
        im = big.resize((PX, PX), Image.LANCZOS)
        im.save(os.path.join(out, "B%02d_%s.png" % (b, name)), dpi=(DPI * PX / (MM / 25.4 * DPI),) * 2)
        ims[b] = im
    for b, t in TEXT.items():
        im, pt = text_label(t)
        im.save(os.path.join(out, "B%02d_%s.png" % (b, t)), dpi=(PX / (MM / 25.4),) * 2)
        ims[b] = im
        if setname == "colour": print("%s: %.1f pt" % (t, pt))
    gap = 30
    sheet = Image.new("RGBA", (6 * (PX + gap) + gap, 2 * (PX + gap) + gap), (255, 255, 255, 255))
    for r, row in enumerate(GRID):
        for c, b in enumerate(row):
            x, y = gap + c * (PX + gap), gap + r * (PX + gap)
            sheet.paste((235, 235, 235, 255), (x, y, x + PX, y + PX))
            sheet.alpha_composite(ims[b], (x, y))
    sheet.convert("RGB").save(os.path.join(outdir, "Stability_Control_0x23_%s_layout.png" % setname))
print("ok")
