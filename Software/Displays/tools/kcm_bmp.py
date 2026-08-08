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
import struct, sys, os, math, zlib, glob


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


# ---------------------------------------------------------------------------
# Minimal PNG decoder (stdlib zlib only -- no PIL). Handles 8-bit
# grayscale/RGB/palette/gray+alpha/RGBA, non-interlaced. Returns RGBA bytes.
# ---------------------------------------------------------------------------
def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def decode_png(path):
    """Return (w, h, rgba bytearray) for an 8-bit, non-interlaced PNG."""
    d = open(path, "rb").read()
    if d[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: not a PNG")
    i = 8
    w = h = bit = ct = 0
    idat = bytearray()
    plte = None
    trns = None
    while i < len(d):
        (ln,) = struct.unpack(">I", d[i:i + 4])
        typ = d[i + 4:i + 8]
        chunk = d[i + 8:i + 8 + ln]
        i += 12 + ln
        if typ == b"IHDR":
            w, h, bit, ct, comp, filt, interlace = struct.unpack(">IIBBBBB", chunk)
            if bit != 8:
                raise ValueError(f"{path}: only 8-bit channels supported (got {bit})")
            if interlace != 0:
                raise ValueError(f"{path}: interlaced PNG not supported")
        elif typ == b"PLTE":
            plte = chunk
        elif typ == b"tRNS":
            trns = chunk
        elif typ == b"IDAT":
            idat += chunk
        elif typ == b"IEND":
            break
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ct]
    stride = w * channels
    raw = zlib.decompress(bytes(idat))
    # unfilter
    recon = bytearray()
    prev = bytearray(stride)
    pos = 0
    for _ in range(h):
        ft = raw[pos]; pos += 1
        line = bytearray(raw[pos:pos + stride]); pos += stride
        for x in range(stride):
            a = line[x - channels] if x >= channels else 0
            b = prev[x]
            c = prev[x - channels] if x >= channels else 0
            v = line[x]
            if   ft == 1: v = (v + a) & 0xFF
            elif ft == 2: v = (v + b) & 0xFF
            elif ft == 3: v = (v + ((a + b) >> 1)) & 0xFF
            elif ft == 4: v = (v + _paeth(a, b, c)) & 0xFF
            line[x] = v
        recon += line
        prev = line
    # expand to RGBA
    rgba = bytearray(w * h * 4)
    for p in range(w * h):
        s = p * channels
        if ct == 6:                                   # RGBA
            rgba[p*4:p*4+4] = recon[s:s+4]
        elif ct == 2:                                 # RGB
            rgba[p*4:p*4+3] = recon[s:s+3]; rgba[p*4+3] = 255
        elif ct == 0:                                 # gray
            g = recon[s]; rgba[p*4:p*4+3] = bytes((g, g, g)); rgba[p*4+3] = 255
        elif ct == 4:                                 # gray+alpha
            g = recon[s]; rgba[p*4:p*4+3] = bytes((g, g, g)); rgba[p*4+3] = recon[s+1]
        elif ct == 3:                                 # palette
            idx = recon[s]
            rgba[p*4:p*4+3] = plte[idx*3:idx*3+3]
            rgba[p*4+3] = trns[idx] if (trns and idx < len(trns)) else 255
    return w, h, rgba


def png_to_image(path, bg=(0, 0, 0)):
    """Decode a PNG and composite its alpha over `bg` into an opaque Image."""
    w, h, rgba = decode_png(path)
    img = Image(w, h, bg)
    for p in range(w * h):
        r, g, b, a = rgba[p*4], rgba[p*4+1], rgba[p*4+2], rgba[p*4+3]
        if a == 0:
            continue
        if a == 255:
            out = (r, g, b)
        else:
            t = a / 255.0
            out = (int(r*t + bg[0]*(1-t)),
                   int(g*t + bg[1]*(1-t)),
                   int(b*t + bg[2]*(1-t)))
        img.set(p % w, p // w, out)
    return img


def _hex_rgb(s):
    s = s.lstrip("#")
    return (int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16))


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
    elif cmd == "png2bmp":
        # png2bmp IN.png OUT.bmp [bgHex]
        inp, out = sys.argv[2], sys.argv[3]
        bg = _hex_rgb(sys.argv[4]) if len(sys.argv) > 4 else (0, 0, 0)
        img = png_to_image(inp, bg)
        img.write_bmp(out)
        print(f"wrote {out} ({img.w}x{img.h}, 24-bit BMP, alpha over #{bg[0]:02x}{bg[1]:02x}{bg[2]:02x})")
    elif cmd == "vesselicons":
        # vesselicons INDIR OUTDIR [bgHex]
        # Converts NN_Name.png -> VIcon_NN.bmp (composited over bg, default black).
        indir, outdir = sys.argv[2], sys.argv[3]
        bg = _hex_rgb(sys.argv[4]) if len(sys.argv) > 4 else (0, 0, 0)
        os.makedirs(outdir, exist_ok=True)
        n = 0
        for src in sorted(glob.glob(os.path.join(indir, "*.png"))):
            base = os.path.basename(src)
            idx = base.split("_", 1)[0]
            if not idx.isdigit():
                print(f"  skip (no leading index): {base}"); continue
            out = os.path.join(outdir, f"VIcon_{int(idx):02d}.bmp")
            png_to_image(src, bg).write_bmp(out)
            print(f"  {base:28s} -> {os.path.basename(out)}")
            n += 1
        print(f"converted {n} vessel-type icons into {outdir}")
    else:
        print(f"unknown command: {cmd}")
        print(__doc__); sys.exit(1)
