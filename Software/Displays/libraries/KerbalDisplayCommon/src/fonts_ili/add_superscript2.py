#!/usr/bin/env python3
"""
Add U+00B2 SUPERSCRIPT TWO (0xB2) to the Roboto_Black ILI9341_t3 fonts.

Why: 0xB2 sits inside index2 (0x80..0xB7) but its entry was a zero-size glyph, so
"m/s\\xB2" measured and drew as "m/s" -- the ACCEL readout on the Orbital Autopilot
and the Annunciator's SURF. GRAVITY row lost their units' exponent without a trace.

How: like add_middot.py, the glyph is not drawn by hand. It is the font's OWN digit
two, area-sampled down to SCALE of its height and set with its top on the cap line
(measured from the same font's capital H), so every size gets a superscript with that
size's own shape and weight. The advance is the scaled ink plus the scaled side
bearings of the full-size 2, so spacing matches the font's.

Only 0xB2's index entry changes, to point at the new glyph appended to data[];
bits_index is recomputed (appending can push the largest offset past the width the
file was generated with). Every other glyph is decoded before and after and asserted
identical, and the new one is decoded back and compared with what was encoded.

Usage: add_superscript2.py FONTDIR
"""
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ilifont import Font
from add_middot import BitWriter, encode_glyph

SUP2  = 0xB2
SCALE = 0.62          # superscript height / full digit height (typographic norm ~0.6)


def build_sup2(F):
    two = F.glyph(ord('2'))
    cap = F.glyph(ord('H'))
    rows = two['rows']
    ink_r = [i for i, r in enumerate(rows) if any(r)]
    ink_c = [c for c in range(two['w']) if any(r[c] for r in rows)]
    r0, r1, c0, c1 = ink_r[0], ink_r[-1], ink_c[0], ink_c[-1]
    src = [row[c0:c1 + 1] for row in rows[r0:r1 + 1]]
    sh, sw = len(src), len(src[0])
    dh, dw = max(3, round(sh * SCALE)), max(2, round(sw * SCALE))
    # Area sampling: each destination pixel is on when at least half of the source
    # area it covers is ink. Keeps stroke weight proportional instead of the dropout
    # nearest-neighbour gives at small sizes.
    out = []
    for y in range(dh):
        ya, yb = y * sh / dh, (y + 1) * sh / dh
        line = []
        for x in range(dw):
            xa, xb = x * sw / dw, (x + 1) * sw / dw
            cov = tot = 0.0
            for sy in range(int(ya), min(sh, int(yb + 0.9999))):
                wy = min(yb, sy + 1) - max(ya, sy)
                if wy <= 0: continue
                for sx in range(int(xa), min(sw, int(xb + 0.9999))):
                    wx = min(xb, sx + 1) - max(xa, sx)
                    if wx <= 0: continue
                    tot += wx * wy
                    cov += wx * wy * src[sy][sx]
            line.append(1 if tot and cov / tot >= 0.5 else 0)
        out.append(line)
    capr = [i for i, r in enumerate(cap['rows']) if any(r)]
    top = capr[0]
    lb = round(c0 * SCALE)                                # scaled left bearing
    rb = round((two['delta'] - 1 - c1) * SCALE)           # scaled right bearing
    w = lb + dw
    rows_out = [[0] * w for _ in range(top)] + [[0] * lb + r for r in out]
    return w, len(rows_out), w + rb, rows_out


def patch(path):
    F = Font(path)
    codes = F.codes()
    assert SUP2 in codes, "0xB2 not inside the font's index ranges"
    before = {c: F.glyph(c) for c in codes}
    f = dict(F.f)

    w, h, delta, rows = build_sup2(F)
    data = bytearray(F.data)
    new_off = len(data)
    data += encode_glyph(f, w, h, 0, 0, delta, rows)

    offs = [new_off if c == SUP2 else F.offset(c) for c in codes]
    bits_index = max(f["bits_index"], max(offs).bit_length())
    bw = BitWriter()
    for o in offs: bw.put(o, bits_index)
    bw.pad()
    index = bw.bytes()

    def contents(buf):
        return "\n" + "\n".join("  " + " ".join("0x%02X," % b for b in buf[i:i + 16])
                                 for i in range(0, len(buf), 16))

    def put_array(src, sym, buf):
        # Replace only what is between the braces, so the declaration -- including the
        # KCM_FONT_FLASH section attribute between "[]" and "=" -- is kept as it is.
        pat = re.escape(sym) + r'(\s*\[\s*\]\s*(?:\w+\s*)*=\s*\{)(.*?)(\n\};)'
        out, n = re.subn(pat, lambda m: sym + m.group(1) + contents(buf) + m.group(3),
                         src, count=1, flags=re.S)
        assert n == 1, "array %s not found" % sym
        return out

    src = F.src
    src = put_array(src, F.name + "_data",  data)
    src = put_array(src, F.name + "_index", index)
    src = re.sub(r'\d+,(\s*//\s*bits_index)', lambda m: "%d,%s" % (bits_index, m.group(1)),
                 src, count=1)
    src = src.replace("#include \"kcm_ili9341_font.h\"",
                      "// U+00B2 SUPERSCRIPT TWO added by add_superscript2.py.\n"
                      "#include \"kcm_ili9341_font.h\"", 1)
    tmp = path + ".new"
    open(tmp, "w").write(src)

    G = Font(tmp)
    for c, g in before.items():
        if c == SUP2: continue
        n = G.glyph(c)
        assert n and (n['w'], n['h'], n['delta'], n['rows']) == (g['w'], g['h'], g['delta'], g['rows']), \
            "glyph 0x%02X changed in %s" % (c, path)
    m = G.glyph(SUP2)
    assert (m['w'], m['h'], m['delta'], m['rows']) == (w, h, delta, rows), "superscript mismatch"
    os.replace(tmp, path)
    return len(before), w, h, delta, F.f["bits_index"], bits_index, rows


if __name__ == "__main__":
    d = sys.argv[1]
    for fn in sorted(os.listdir(d)):
        if not re.match(r'Roboto_Black_\d+\.c$', fn): continue
        n, w, h, dl, bi0, bi, rows = patch(os.path.join(d, fn))
        print("%-22s %3d glyphs verified | sup2 w=%-2d h=%-2d delta=%-2d | bits_index %d%s"
              % (fn, n, w, h, dl, bi, "" if bi == bi0 else " (was %d)" % bi0))
