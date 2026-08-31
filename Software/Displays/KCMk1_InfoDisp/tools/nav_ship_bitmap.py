#!/usr/bin/env python3
"""Rasterise the NAVIGATION own-ship silhouette to a 1-bit bitmap.

The planform is defined once, as the right half of an airliner outline in a normalised
box: x=0 is the centreline, y=0 the nose, y=1 the tail, x in units of the same length.
It is mirrored, supersampled 8x and thresholded, so the curves and the swept leading
edges come out clean at the small size the card can spare -- which drawing it from
fillTriangle primitives could not do.
"""
import sys, zlib, struct

W, H = 110, 122           # bitmap size in device pixels
SS   = 8                 # supersampling factor

# Right half, nose to tail. Fractions of the bitmap height for y, of the height for x too
# (so the aspect is honest), mirrored about x=0.
HALF = [
    (0.000, 0.000),      # nose tip
    (0.030, 0.012),
    (0.055, 0.040),
    (0.072, 0.090),
    (0.080, 0.160),
    (0.082, 0.360),      # fuselage side, wing root leading edge
    (0.440, 0.620),      # wing tip, leading edge
    (0.440, 0.720),      # wing tip, trailing edge (short vertical tip chord)
    (0.082, 0.645),      # wing root trailing edge -- this is the notch
    (0.082, 0.800),      # fuselage side, tailplane root leading edge
    (0.205, 0.930),      # tailplane tip, leading edge
    (0.205, 0.980),      # tailplane tip, trailing edge
    (0.070, 0.940),      # tailplane root trailing edge
    (0.070, 1.000),      # tail
    (0.000, 1.000),
]

def polygon():
    pts = [(x * H, y * H) for x, y in HALF]
    left = [(-x, y) for x, y in reversed(pts[1:-1])]
    return pts + left

def inside(poly, px, py):
    n = len(poly); c = False; j = n - 1
    for i in range(n):
        xi, yi = poly[i]; xj, yj = poly[j]
        if (yi > py) != (yj > py) and px < (xj - xi) * (py - yi) / (yj - yi) + xi:
            c = not c
        j = i
    return c

def render():
    poly = polygon()
    cx = W / 2.0
    grid = [[0] * W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            hits = 0
            for sy in range(SS):
                py = y + (sy + 0.5) / SS
                for sx in range(SS):
                    px = x + (sx + 0.5) / SS - cx
                    if inside(poly, px, py):
                        hits += 1
            grid[y][x] = 1 if hits * 2 >= SS * SS else 0
    return grid

def png(grid, out, scale=6):
    rows = []
    for r in grid:
        row = bytearray()
        for v in r:
            c = 255 if v else 0
            for _ in range(scale): row += bytes((c, c, c))
        for _ in range(scale): rows.append(b'\x00' + bytes(row))
    raw = b''.join(rows)
    def chunk(t, d):
        c = t + d; return struct.pack('>I', len(d)) + c + struct.pack('>I', zlib.crc32(c))
    open(out, 'wb').write(b'\x89PNG\r\n\x1a\n'
        + chunk(b'IHDR', struct.pack('>IIBBBBB', W * scale, H * scale, 8, 2, 0, 0, 0))
        + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))

def emit(grid, out):
    stride = (W + 7) // 8
    lines = []
    for r in grid:
        b = bytearray(stride)
        for x, v in enumerate(r):
            if v: b[x >> 3] |= 0x80 >> (x & 7)
        lines.append(', '.join('0x%02X' % v for v in b))
    with open(out, 'w') as f:
        for i, l in enumerate(lines):
            f.write('  %s,%s\n' % (l, '' if i else '   // row 0 = nose'))

if __name__ == '__main__':
    g = render()
    png(g, 'ship.png')
    emit(g, 'ship.inc')
    print('%dx%d, %d bytes, %d set pixels' %
          (W, H, H * ((W + 7) // 8), sum(map(sum, g))))
