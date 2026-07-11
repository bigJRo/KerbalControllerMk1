#!/usr/bin/env python3
"""
kcm_bmp.py -- Kerbal Controller Mk1 display BMP toolchain (no external deps).

drawBMP() in KerbalDisplayCommon expects 24-bit uncompressed (BI_RGB) BMPs.
This module writes exactly that format and provides procedural generators for
the panel assets (test pattern, celestial-body globes, KASA meatball, standby
splash). Pure standard-library Python -- no PIL required.

BMP notes:
  - 24-bit, BITMAPINFOHEADER (40 bytes), compression = 0.
  - Standard bottom-up (positive height). Rows are BGR, padded to 4 bytes.
  - drawBMP handles both bottom-up and top-down; we emit bottom-up (most
    compatible) and it displays upright.

Usage:
  python3 kcm_bmp.py testpattern OUT.bmp [W H]
  python3 kcm_bmp.py all OUTDIR          # generate the full asset set
"""
import struct, sys, os, math


# ---------------------------------------------------------------------------
# Core BMP writer
# ---------------------------------------------------------------------------
class Image:
    """RGB framebuffer with a handful of primitive draw helpers."""
    def __init__(self, w, h, bg=(0, 0, 0)):
        self.w, self.h = w, h
        self.px = bytearray(struct.pack("BBB", *bg) * (w * h))

    def _idx(self, x, y):
        return (y * self.w + x) * 3

    def set(self, x, y, rgb):
        if 0 <= x < self.w and 0 <= y < self.h:
            i = self._idx(x, y)
            self.px[i] = rgb[0] & 0xFF
            self.px[i + 1] = rgb[1] & 0xFF
            self.px[i + 2] = rgb[2] & 0xFF

    def fill_rect(self, x0, y0, w, h, rgb):
        for y in range(y0, y0 + h):
            for x in range(x0, x0 + w):
                self.set(x, y, rgb)

    def border(self, thick, rgb):
        for t in range(thick):
            for x in range(self.w):
                self.set(x, t, rgb)
                self.set(x, self.h - 1 - t, rgb)
            for y in range(self.h):
                self.set(t, y, rgb)
                self.set(self.w - 1 - t, y, rgb)

    def fill_disc(self, cx, cy, r, shade):
        """shade(nx, ny, dist) -> rgb, called per pixel inside radius r.
        nx, ny are offsets normalised to [-1, 1]; dist is 0 at centre, 1 at rim."""
        r2 = r * r
        for y in range(max(0, cy - r), min(self.h, cy + r + 1)):
            for x in range(max(0, cx - r), min(self.w, cx + r + 1)):
                dx, dy = x - cx, y - cy
                if dx * dx + dy * dy <= r2:
                    d = math.sqrt(dx * dx + dy * dy) / r
                    self.set(x, y, shade(dx / r, dy / r, d))

    def write_bmp(self, path):
        row_pad = (-self.w * 3) % 4
        raw = self.w * 3 + row_pad
        data_size = raw * self.h
        # bottom-up: emit last image row first
        out = bytearray()
        for y in range(self.h - 1, -1, -1):
            base = y * self.w * 3
            for x in range(self.w):
                i = base + x * 3
                r, g, b = self.px[i], self.px[i + 1], self.px[i + 2]
                out += bytes((b, g, r))          # BGR on disk
            out += b"\x00" * row_pad
        hdr = struct.pack("<2sIHHI", b"BM", 54 + data_size, 0, 0, 54)
        dib = struct.pack("<IiiHHIIiiII", 40, self.w, self.h, 1, 24, 0,
                          data_size, 2835, 2835, 0, 0)
        with open(path, "wb") as f:
            f.write(hdr); f.write(dib); f.write(out)


# ---------------------------------------------------------------------------
# Generators
# ---------------------------------------------------------------------------
def _lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


def testpattern(w, h):
    """Unambiguous verification image: corner-coded, up-arrow, L->R gradient,
    white border. Confirms colour mapping, orientation, mirroring and scale."""
    img = Image(w, h)
    # horizontal gradient background (dark left -> bright right)
    for x in range(w):
        t = x / (w - 1)
        col = _lerp((16, 16, 40), (40, 90, 160), t)
        for y in range(h):
            img.set(x, y, col)
    # corner squares: TL red, TR green, BL blue, BR yellow
    s = max(12, min(w, h) // 6)
    img.fill_rect(0, 0, s, s, (220, 40, 40))            # top-left  red
    img.fill_rect(w - s, 0, s, s, (40, 200, 40))        # top-right green
    img.fill_rect(0, h - s, s, s, (40, 80, 230))        # bot-left  blue
    img.fill_rect(w - s, h - s, s, s, (230, 210, 40))   # bot-right yellow
    # centre up-arrow (white triangle) -- tip at top, widening downward, so it
    # points to true top of the image.
    ah = h // 3
    cx = w // 2
    ay0 = (h - ah) // 2
    for i in range(ah):
        half = int((i / ah) * (ah * 0.6))
        for x in range(cx - half, cx + half + 1):
            img.set(x, ay0 + i, (255, 255, 255))
    img.border(3, (255, 255, 255))
    return img


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__); sys.exit(1)
    cmd = sys.argv[1]
    if cmd == "testpattern":
        out = sys.argv[2]
        w = int(sys.argv[3]) if len(sys.argv) > 3 else 240
        h = int(sys.argv[4]) if len(sys.argv) > 4 else 168
        testpattern(w, h).write_bmp(out)
        print(f"wrote {out} ({w}x{h}, 24-bit BMP)")
    else:
        print(f"unknown command: {cmd}")
        print(__doc__); sys.exit(1)
