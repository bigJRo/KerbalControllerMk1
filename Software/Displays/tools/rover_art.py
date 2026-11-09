#!/usr/bin/env python3
"""
rover_art.py -- the rover graphics for the InfoDisp ROVER screen.

An original near-future pressurized rover: white cabin with a safety-orange
stripe, tinted wraparound windscreen, roof solar array, comms mast with dish,
six treaded wheels on rocker suspension and a rear airlock hatch. Drawn here
with antialiasing (8x supersampled, then downsampled), in three views:

  Rover/Top.bmp        top view, nose up, for the compass centre (static)
  Rover/Pp05.bmp ...   side view, nose right, tilted for pitch: Pm45..Pp45
  Rover/Rm12.bmp ...   rear view, tilted for roll: Rm45..Rp45
                       (p = plus, m = minus; 0 deg is Pp00 / Rp00)

Every image is a 24-bit BMP on black (the screen background), which is what
KerbalDisplayCommon's drawBMP() reads. Pitch +N lifts the nose (right side) by
N degrees; roll +N lowers the right side by N degrees (bank right), matching
the conventions of Screen_ROVR.ino.

Usage:
    python3 tools/rover_art.py                 # write the BMPs to ../assets
    python3 tools/rover_art.py --out DIR       # somewhere else
    python3 tools/rover_art.py --sheet FILE    # also a PNG contact sheet

Needs Pillow.
"""
import argparse
import math
import os
from PIL import Image, ImageDraw

S = 8                     # supersampling factor
TILT_FRAME = 124          # tilt frame size (px): fits the PITCH / ROLL boxes at +-45 deg
SIDE_DY, REAR_DY = 8.0, 10.0   # centre each view's art on the frame (and rotation) centre
TOP_W, TOP_H = 116, 164   # compass-centre image size (px)
TOP_SCALE = 1.25          # the top view is drawn this much larger than the tilt views

# Palette (sRGB). Kept to colours that survive RGB565 well.
WHITE    = (236, 236, 230)
WHITE_LO = (196, 198, 194)     # shaded lower body
PANEL    = (150, 152, 150)     # panel lines
ORANGE   = (255, 118, 26)
GLASS    = (24, 78, 104)
GLASS_HI = (120, 205, 235)
SOLAR    = (24, 44, 104)
SOLAR_LN = (62, 100, 182)
TYRE     = (44, 44, 46)
TREAD    = (86, 86, 90)
HUB      = (160, 162, 166)
HUB_DK   = (92, 94, 98)
STRUT    = (110, 112, 116)
DARK     = (64, 66, 70)
LAMP     = (255, 240, 170)
TAIL     = (230, 40, 30)
MAST     = (180, 182, 186)


class Pen:
    """Draw in final-pixel units around an origin; scales by S."""
    def __init__(self, img, ox, oy, k=1.0):
        self.d = ImageDraw.Draw(img)
        self.ox, self.oy, self.k = ox, oy, k

    def P(self, x, y):
        return ((self.ox + x * self.k) * S, (self.oy + y * self.k) * S)

    def poly(self, pts, fill, outline=None, width=1):
        p = [self.P(x, y) for x, y in pts]
        self.d.polygon(p, fill=fill)
        if outline:
            self.d.line(p + [p[0]], fill=outline, width=int(width * self.k * S), joint="curve")

    def rect(self, x0, y0, x1, y1, fill, outline=None, width=1, r=0):
        box = [self.P(x0, y0), self.P(x1, y1)]
        if r:
            self.d.rounded_rectangle(box, radius=r * self.k * S, fill=fill, outline=outline,
                                     width=int(width * self.k * S) if outline else 0)
        else:
            self.d.rectangle(box, fill=fill, outline=outline, width=int(width * self.k * S) if outline else 0)

    def circle(self, x, y, r, fill, outline=None, width=1):
        self.d.ellipse([self.P(x - r, y - r), self.P(x + r, y + r)], fill=fill, outline=outline,
                       width=int(width * self.k * S) if outline else 0)

    def ellipse(self, x0, y0, x1, y1, fill, outline=None, width=1):
        self.d.ellipse([self.P(x0, y0), self.P(x1, y1)], fill=fill, outline=outline,
                       width=int(width * self.k * S) if outline else 0)

    def line(self, pts, fill, width=1):
        self.d.line([self.P(x, y) for x, y in pts], fill=fill, width=max(1, int(width * self.k * S)), joint="curve")


# ------------------------------------------------------------
#  Side view (nose right). Origin = rotation pivot.
# ------------------------------------------------------------
def side_wheel(p, x, y, r=11.0):
    p.circle(x, y, r, TYRE)
    for k in range(14):                       # tread blocks around the rim
        a = k * 2 * math.pi / 14
        c, s = math.cos(a), math.sin(a)
        t = 1.6
        pts = []
        for (rr, aa) in ((r - 2.2, -0.11), (r + 0.2, -0.09), (r + 0.2, 0.09), (r - 2.2, 0.11)):
            pts.append((x + rr * math.cos(a + aa), y + rr * math.sin(a + aa)))
        p.poly(pts, TREAD)
    p.circle(x, y, r - 3.2, (34, 34, 36))      # sidewall
    p.circle(x, y, 5.2, HUB, outline=HUB_DK, width=0.8)
    for k in range(5):                         # hub bolts
        a = k * 2 * math.pi / 5 - math.pi / 2
        p.circle(x + 3.0 * math.cos(a), y + 3.0 * math.sin(a), 0.7, HUB_DK)
    p.circle(x, y, 1.4, HUB_DK)


def draw_side(p):
    wy = 21.0
    wx = (-37.0, 0.0, 37.0)
    # Rocker-bogie suspension behind the wheels
    p.line([(-37, wy), (-18, 9), (0, wy)], STRUT, 2.4)
    p.line([(-18, 9), (-18, 4)], STRUT, 2.4)
    p.line([(37, wy), (20, 8), (20, 4)], STRUT, 2.4)
    p.line([(0, wy), (20, 8)], STRUT, 2.0)
    # Belly skirt
    p.poly([(-46, 4), (44, 4), (40, 10), (-42, 10)], DARK)
    # Cabin: rear rounded, roof, chamfered windscreen, nose
    body = [(-50, -12), (-47, -18), (-42, -21), (26, -21), (44, -9), (49, -4),
            (49, 5), (-50, 5)]
    p.poly(body, WHITE)
    p.poly([(-50, -2), (49, -2), (49, 5), (-50, 5)], WHITE_LO)          # shaded lower half
    # Windscreen (wraps over the chamfer)
    p.poly([(24, -19), (42, -8.5), (46, -4.5), (33, -4.5), (22, -12)], GLASS)
    p.line([(26, -17), (40, -8.8)], GLASS_HI, 1.0)
    # Side port windows
    for x0 in (-6, 6):
        p.rect(x0, -15, x0 + 9, -9, GLASS, r=1.5)
        p.line([(x0 + 1.5, -13.6), (x0 + 5, -13.6)], GLASS_HI, 0.7)
    # Orange stripe and panel lines
    p.poly([(-50, -5), (49, -5), (49, -2.6), (-50, -2.6)], ORANGE)
    p.line([(-30, -20.5), (-30, 4.5)], PANEL, 0.6)
    p.line([(16, -20.5), (16, 4.5)], PANEL, 0.6)
    p.line([(-50, -12), (-47, -18), (-42, -21), (26, -21), (44, -9), (49, -4), (49, 5), (-50, 5), (-50, -12)],
           PANEL, 0.7)
    # Rear airlock hatch edge
    p.rect(-49, -16, -45.5, 1, (210, 212, 208), outline=PANEL, width=0.5, r=1)
    # Headlight and tail light
    p.rect(46, 0, 49.5, 3, LAMP, r=0.8)
    p.rect(-50.5, 0, -48, 3, TAIL, r=0.6)
    # Roof solar array on struts
    p.line([(-36, -21), (-36, -24)], STRUT, 1.2)
    p.line([(14, -21), (14, -24)], STRUT, 1.2)
    p.rect(-44, -26.5, 20, -23.5, SOLAR, outline=SOLAR_LN, width=0.6)
    for xx in range(-38, 20, 6):
        p.line([(xx, -26.3), (xx, -23.7)], SOLAR_LN, 0.5)
    # Comms mast with dish and camera
    p.line([(-38, -26.5), (-38, -41)], MAST, 1.6)
    p.ellipse(-44, -48, -32, -41, (214, 216, 212), outline=PANEL, width=0.6)    # dish (side-on)
    p.line([(-38, -44.5), (-33, -46)], DARK, 0.8)
    p.rect(-35, -33, -30, -30, DARK, r=0.8)                                   # camera head
    p.circle(-30.6, -31.5, 0.9, GLASS_HI)
    for x in wx:
        side_wheel(p, x, wy)


# ------------------------------------------------------------
#  Rear view. Origin = rotation pivot.
# ------------------------------------------------------------
def rear_tyre(p, x0, x1, y0, y1):
    p.rect(x0, y0, x1, y1, TYRE, r=3)
    for k in range(7):                         # tread across the face
        yy = y0 + 2.5 + k * (y1 - y0 - 5) / 6
        p.line([(x0 + 1.5, yy), (x1 - 1.5, yy)], TREAD, 1.1)
    p.rect(x0, y0, x1, y1, None, outline=(28, 28, 30), width=0.7, r=3)


def draw_rear(p):
    # Track and wheel size match the top and side views: wheels 12 wide at
    # +-33, 22 tall (side-view diameter), cabin 50 wide.
    # Suspension arms to the wheels
    p.line([(-22, 6), (-30, 15)], STRUT, 2.4)
    p.line([(22, 6), (30, 15)], STRUT, 2.4)
    # Belly skirt
    p.poly([(-23, 4), (23, 4), (20, 10), (-20, 10)], DARK)
    # Wheels (the near wheel of each side; tread face)
    rear_tyre(p, -39, -27, 10, 32)
    rear_tyre(p, 27, 39, 10, 32)
    # Cabin
    p.rect(-25, -21, 25, 6, WHITE, r=6)
    p.rect(-25, -1, 25, 6, WHITE_LO, r=3)
    p.rect(-25, -5, 25, -2.6, ORANGE)
    p.rect(-25, -21, 25, 6, None, outline=PANEL, width=0.7, r=6)
    # Airlock hatch with porthole
    p.rect(-9, -18, 9, 4, (214, 216, 212), outline=PANEL, width=0.7, r=3)
    p.circle(0, -11, 3.6, GLASS, outline=PANEL, width=0.5)
    p.circle(-1.0, -12.0, 1.0, GLASS_HI)
    p.rect(5, -6, 7.5, -2, DARK, r=0.5)                                   # hatch handle
    # Tail lights
    p.rect(-23.5, 0.5, -18.5, 3.5, TAIL, r=0.8)
    p.rect(18.5, 0.5, 23.5, 3.5, TAIL, r=0.8)
    # Roof solar array (seen edge-on, wider than the cabin)
    p.line([(-16, -21), (-16, -24)], STRUT, 1.2)
    p.line([(16, -21), (16, -24)], STRUT, 1.2)
    p.rect(-32, -26.5, 32, -23.5, SOLAR, outline=SOLAR_LN, width=0.6)
    # Mast with dish (seen from behind) and camera
    p.line([(-14, -26.5), (-14, -41)], MAST, 1.6)
    p.circle(-14, -45, 6.0, (214, 216, 212), outline=PANEL, width=0.6)
    p.circle(-14, -45, 1.6, DARK)
    p.rect(-17, -34, -11, -31, DARK, r=0.8)


# ------------------------------------------------------------
#  Top view (nose up). Origin = image centre.
# ------------------------------------------------------------
def top_wheel(p, x, y):
    p.rect(x - 6, y - 12, x + 6, y + 12, TYRE, r=3.5)
    for k in range(9):
        yy = y - 10 + k * 2.5
        p.line([(x - 4.5, yy), (x + 4.5, yy)], TREAD, 1.0)
    p.rect(x - 6, y - 12, x + 6, y + 12, None, outline=(28, 28, 30), width=0.7, r=3.5)


def draw_top(p):
    wx, wy = 33.0, (-38.0, 0.0, 38.0)
    # Suspension arms
    for y in wy:
        p.line([(-wx, y), (-24, y)], STRUT, 2.6)
        p.line([(wx, y), (24, y)], STRUT, 2.6)
    for y in wy:
        top_wheel(p, -wx, y)
        top_wheel(p, wx, y)
    # Cabin hull
    hull = [(-18, -60), (18, -60), (25, -50), (26, 52), (21, 58), (-21, 58), (-26, 52), (-25, -50)]
    p.poly(hull, WHITE)
    # Orange side stripes
    p.poly([(-25.5, -44), (-22.5, -44), (-22.5, 50), (-25.5, 50)], ORANGE)
    p.poly([(22.5, -44), (25.5, -44), (25.5, 50), (22.5, 50)], ORANGE)
    # Wraparound windscreen
    p.poly([(-17, -58), (17, -58), (23, -49), (21, -42), (-21, -42), (-23, -49)], GLASS)
    p.line([(-12, -55), (10, -55)], GLASS_HI, 1.0)
    # Headlights
    p.rect(-17, -61.5, -10, -59.5, LAMP, r=0.8)
    p.rect(10, -61.5, 17, -59.5, LAMP, r=0.8)
    # Roof solar array: two panels with cell grid
    for (y0, y1) in ((-36, 6), (10, 40)):
        p.rect(-20, y0, 20, y1, SOLAR, outline=SOLAR_LN, width=0.7)
        for xx in (-10, 0, 10):
            p.line([(xx, y0 + 0.5), (xx, y1 - 0.5)], SOLAR_LN, 0.5)
        yy = y0 + 7
        while yy < y1 - 1:
            p.line([(-19.5, yy), (19.5, yy)], SOLAR_LN, 0.5)
            yy += 7
    # Comms mast dish (from above) at the rear quarter
    p.circle(-11, 47, 6.5, (214, 216, 212), outline=PANEL, width=0.6)
    p.circle(-11, 47, 2.0, DARK)
    # Rear airlock hatch
    p.rect(2, 44, 18, 56, (214, 216, 212), outline=PANEL, width=0.6, r=2)
    # Tail lights
    p.rect(-19, 57, -13, 59, TAIL, r=0.6)
    p.rect(13, 57, 19, 59, TAIL, r=0.6)
    # Hull outline
    p.line(hull + [hull[0]], PANEL, 0.7)


# ------------------------------------------------------------
#  Rendering
# ------------------------------------------------------------
def render(drawfn, w, h, dy=0.0, angle=0.0, k=1.0):
    """Draw at S x with the art shifted down by dy, rotate about the frame centre
    (PIL: +angle = counter-clockwise), downsample."""
    pivot = (0.5, 0.5)
    big = Image.new("RGB", (w * S, h * S), (0, 0, 0))
    p = Pen(big, w * pivot[0], h * pivot[1] + dy, k)
    drawfn(p)
    if angle:
        big = big.rotate(angle, resample=Image.BICUBIC, center=(w * pivot[0] * S, h * pivot[1] * S),
                         fillcolor=(0, 0, 0))
    return big.resize((w, h), Image.LANCZOS)


def save_bmp(img, path):
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    img.convert("RGB").save(path, format="BMP")       # 24-bit uncompressed


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(here, "..", "assets"))
    ap.add_argument("--sheet", help="also write a PNG contact sheet here")
    a = ap.parse_args()

    top = render(draw_top, TOP_W, TOP_H, k=TOP_SCALE)
    save_bmp(top, os.path.join(a.out, "Rover", "Top.bmp"))
    for deg in range(-45, 46):
        tag = "%s%02d" % ("m" if deg < 0 else "p", abs(deg))
        # Pitch +N: nose (right) up = counter-clockwise on screen.
        save_bmp(render(draw_side, TILT_FRAME, TILT_FRAME, SIDE_DY, angle=deg),
                 os.path.join(a.out, "Rover", "P%s.bmp" % tag))
        # Roll +N: bank right = right side down = clockwise on screen.
        save_bmp(render(draw_rear, TILT_FRAME, TILT_FRAME, REAR_DY, angle=-deg),
                 os.path.join(a.out, "Rover", "R%s.bmp" % tag))

    if a.sheet:
        big = 3
        tiles = [top.resize((TOP_W * big, TOP_H * big), Image.NEAREST)]
        for fn, dy in ((draw_side, SIDE_DY), (draw_rear, REAR_DY)):
            for ang in (0, 45, -45):
                tiles.append(render(fn, TILT_FRAME, TILT_FRAME, dy, angle=ang)
                             .resize((TILT_FRAME * big,) * 2, Image.NEAREST))
        W = sum(t.width for t in tiles) + 40 * (len(tiles) + 1)
        H = max(t.height for t in tiles) + 80
        sheet = Image.new("RGB", (W, H), (0, 0, 0))
        d = ImageDraw.Draw(sheet)
        x = 40
        for i, t in enumerate(tiles):
            sheet.paste(t, (x, 40))
            if i:                                   # outline the tilt frame
                d.rectangle([x - 1, 39, x + t.width, 40 + t.height], outline=(70, 70, 70))
            x += t.width + 40
        sheet.save(a.sheet)
    print("rover art written to", os.path.normpath(a.out))


if __name__ == "__main__":
    main()
