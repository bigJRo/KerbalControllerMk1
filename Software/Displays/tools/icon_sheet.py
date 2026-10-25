#!/usr/bin/env python3
"""
icon_sheet.py -- the vessel-type icons as manual graphics.

The Annunciator's SPCFT tile shows one of seventeen 72x72 icons (assets/VIcon_NN.bmp,
indexed by the KSP VesselType, white over black). This writes each one as a PNG
thumbnail and lays all of them out on one labelled contact sheet, captioned in the
panel's own Roboto Black font so the sheet matches the screen renders.

Usage:
    python3 tools/icon_sheet.py                 # -> Documents/User/assets/vessel_icons/
    python3 tools/icon_sheet.py --out DIR

Plain Python 3, no third-party modules.
"""
import argparse
import os
import struct
import sys
import zlib

HERE     = os.path.dirname(os.path.abspath(__file__))
DISPLAYS = os.path.dirname(HERE)
ASSETS   = os.path.join(DISPLAYS, "assets")
FONTS    = os.path.join(DISPLAYS, "libraries", "KerbalDisplayCommon", "src", "fonts_ili")
REPO     = os.path.dirname(os.path.dirname(DISPLAYS))
DEFAULT_OUT = os.path.join(REPO, "Documents", "User", "assets", "vessel_icons")
sys.path.insert(0, FONTS)
from ilifont import Font  # noqa: E402

# KSP VesselType index -> (file suffix, caption). Matches assets/README.md.
TYPES = [
    ("Debris", "DEBRIS"), ("SpaceObject", "SPACE OBJECT"), ("Unknown", "UNKNOWN"),
    ("Probe", "PROBE"), ("Relay", "RELAY"), ("Rover", "ROVER"), ("Lander", "LANDER"),
    ("Ship", "SHIP"), ("Plane", "PLANE"), ("Station", "STATION"), ("Base", "BASE"),
    ("EVA", "EVA"), ("Flag", "FLAG"), ("ScienceController", "SCIENCE CTRL"),
    ("SciencePart", "SCIENCE PART"), ("Part", "PART"), ("GroundPart", "GROUND PART"),
]


class Canvas:
    def __init__(self, w, h, rgb=(0, 0, 0)):
        self.w, self.h = w, h
        self.px = bytearray(bytes(rgb) * (w * h))

    def set(self, x, y, rgb):
        if 0 <= x < self.w and 0 <= y < self.h:
            i = (y * self.w + x) * 3
            self.px[i:i + 3] = bytes(rgb)

    def blit(self, other, x0, y0):
        for y in range(other.h):
            for x in range(other.w):
                i = (y * other.w + x) * 3
                self.set(x0 + x, y0 + y, other.px[i:i + 3])

    def text(self, font, x, y, s, rgb):
        """Left edge x, top y; advances by each glyph's delta like the firmware."""
        for ch in s:
            g = font.glyph(ord(ch))
            if g is None:
                continue
            for r, row in enumerate(g["rows"]):
                for c, bit in enumerate(row):
                    if bit:
                        self.set(x + c, y + r, rgb)
            x += g["delta"]

    def write_png(self, path):
        rows = b"".join(b"\x00" + bytes(self.px[y * self.w * 3:(y + 1) * self.w * 3]) for y in range(self.h))

        def chunk(tag, body):
            return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF)

        with open(path, "wb") as f:
            f.write(b"\x89PNG\r\n\x1a\n")
            f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", self.w, self.h, 8, 2, 0, 0, 0)))
            f.write(chunk(b"IDAT", zlib.compress(rows, 9)))
            f.write(chunk(b"IEND", b""))


def read_bmp24(path):
    d = open(path, "rb").read()
    if d[:2] != b"BM":
        raise ValueError("%s: not a BMP" % path)
    off = struct.unpack_from("<I", d, 10)[0]
    w, h, _, bpp, comp = struct.unpack_from("<iiHHI", d, 18)
    if bpp != 24 or comp != 0:
        raise ValueError("%s: only 24-bit uncompressed BMPs are supported" % path)
    top_down = h < 0
    h = abs(h)
    stride = (w * 3 + 3) & ~3
    c = Canvas(w, h)
    for row in range(h):
        src = off + (row if top_down else h - 1 - row) * stride
        for x in range(w):
            b, g, r = d[src + x * 3:src + x * 3 + 3]
            c.set(x, row, (r, g, b))
    return c


def text_width(font, s):
    return sum((font.glyph(ord(ch)) or {"delta": 0})["delta"] for ch in s)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default=DEFAULT_OUT)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    font = Font(os.path.join(FONTS, "Roboto_Black_12.c"))
    icons = []
    for idx, (suffix, caption) in enumerate(TYPES):
        icon = read_bmp24(os.path.join(ASSETS, "VIcon_%02d.bmp" % idx))
        icon.write_png(os.path.join(args.out, "VIcon_%02d_%s.png" % (idx, suffix)))
        icons.append((idx, caption, icon))
        print("VIcon_%02d_%s.png  %dx%d" % (idx, suffix, icon.w, icon.h))

    # Contact sheet: 6 columns, icon centred over its index and caption.
    cols, cell_w, cell_h, pad = 6, 120, 72 + 8 + font.f["cap_height"] + 12, 8
    rows = (len(icons) + cols - 1) // cols
    sheet = Canvas(cols * cell_w + pad * 2, rows * cell_h + pad * 2)
    white, grey = (255, 255, 255), (132, 130, 132)
    for n, (idx, caption, icon) in enumerate(icons):
        cx = pad + (n % cols) * cell_w
        cy = pad + (n // cols) * cell_h
        sheet.blit(icon, cx + (cell_w - icon.w) // 2, cy)
        label = "%d  %s" % (idx, caption)
        sheet.text(font, cx + (cell_w - text_width(font, label)) // 2, cy + icon.h + 8, label, grey)
    sheet_path = os.path.join(os.path.dirname(args.out), "Annunciator_VesselIcons.png")
    sheet.write_png(sheet_path)
    print("sheet -> %s  %dx%d" % (os.path.relpath(sheet_path, REPO), sheet.w, sheet.h))


if __name__ == "__main__":
    main()
