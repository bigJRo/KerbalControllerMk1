#!/usr/bin/env python3
"""
Convert BDF bitmap fonts (Terminus Font, SIL OFL 1.1) to ILI9341_t3 1-bit font
.c files for the KCMk1 displays. BDF is plain-text hand-designed pixels, so the
glyphs are exact -- no rasterisation, no off-grid distortion (the flaw of the
earlier outline-rendered set). Pure Python: no PIL/FreeType needed.

Output format matches the other fonts_ili/*.c (see ttf/tfont generators):
  index[] : bit-packed, bits_index-wide, value = BYTE offset of glyph in data[]
  data[]  : per glyph MSB-first [3b enc=0][width][height][xoff][yoff][delta]
            then per row [1b flag=0][width bits], byte-padded per glyph.
  Full-cell, top-aligned (height=cell_h, yoffset=0, cap_height=cell_h) so
  origin_y == cursor_y. Monospace: width == delta == cell_w for every glyph.
Every glyph is round-trip decoded with the ILI9341_t3 fetchbits logic and
asserted pixel-identical before emit. ASCII 0x20..0x7e only.

Usage: bdf_to_ili9341.py OUTDIR NAME SIZE=BDFPATH [SIZE=BDFPATH ...] [N=2xM]
  e.g. bdf_to_ili9341.py ./ KcmTerm 16=ter-u16n.bdf 24=ter-u24n.bdf 40=2x20
"""
import sys, os, re

class BitWriter:
    def __init__(self): self.bits = []
    def put(self, v, n):
        for i in range(n - 1, -1, -1): self.bits.append((v >> i) & 1)
    def put_signed(self, v, n): self.put(v & ((1 << n) - 1), n)
    def pad_to_byte(self):
        while len(self.bits) % 8: self.bits.append(0)
    def bytes(self):
        out = bytearray()
        for i in range(0, len(self.bits), 8):
            b = 0
            for j in range(8): b = (b << 1) | (self.bits[i + j] if i + j < len(self.bits) else 0)
            out.append(b)
        return out

def fb_u(bits, i, n):
    v = 0
    for k in range(n): v = (v << 1) | bits[i + k]
    return v
def fb_s(bits, i, n):
    v = fb_u(bits, i, n)
    return v - (1 << n) if v & (1 << (n - 1)) else v

def parse_bdf(path):
    """Return (cell_w, cell_h, {code: rows[cell_h][cell_w] of 0/1}) for 0x20..0x7e."""
    txt = open(path, encoding='latin-1').read().splitlines()
    fbb = None
    i = 0
    while i < len(txt):
        if txt[i].startswith("FONTBOUNDINGBOX"):
            _, w, h, x, y = txt[i].split()[:5]
            fbb = (int(w), int(h), int(x), int(y)); break
        i += 1
    fbw, fbh, fbx, fby = fbb
    ascent = fbh + fby                      # baseline row from top of cell
    glyphs = {}
    i = 0
    while i < len(txt):
        if txt[i].startswith("STARTCHAR"):
            enc = None; bbx = None; bitmap = []
            j = i + 1
            while j < len(txt) and not txt[j].startswith("ENDCHAR"):
                ln = txt[j]
                if ln.startswith("ENCODING"): enc = int(ln.split()[1])
                elif ln.startswith("BBX"):
                    _, bw, bh, bx, by = ln.split()[:5]
                    bbx = (int(bw), int(bh), int(bx), int(by))
                elif ln.startswith("BITMAP"):
                    k = j + 1
                    while k < len(txt) and not txt[k].startswith("ENDCHAR"):
                        bitmap.append(txt[k].strip()); k += 1
                    j = k - 1
                j += 1
            i = j
            if enc is not None and 0x20 <= enc <= 0x7e and bbx is not None:
                bw, bh, bx, by = bbx
                cell = [[0] * fbw for _ in range(fbh)]
                top = ascent - by - bh          # glyph bitmap top row within the cell
                for r in range(bh):
                    if r >= len(bitmap): break
                    rowbits = int(bitmap[r] or "0", 16)
                    nbytes = (bw + 7) // 8
                    # hex is left-justified in nbytes*8 bits; MSB = leftmost pixel
                    for c in range(bw):
                        bit = (rowbits >> (nbytes * 8 - 1 - c)) & 1
                        cy, cx = top + r, bx - fbx + c
                        if 0 <= cy < fbh and 0 <= cx < fbw:
                            cell[cy][cx] = bit
                glyphs[enc] = cell
        i += 1
    # blanks for any missing ASCII code
    for c in range(0x20, 0x7f):
        glyphs.setdefault(c, [[0] * fbw for _ in range(fbh)])
    return fbw, fbh, glyphs

def double_glyphs(cw, ch, glyphs):
    out = {}
    for c, rows in glyphs.items():
        dr = []
        for r in rows:
            drow = []
            for p in r: drow += [p, p]
            dr.append(list(drow)); dr.append(list(drow))
        out[c] = dr
    return cw * 2, ch * 2, out

def emit(name, cw, ch, glyphs, outdir, srcnote):
    codes = sorted(glyphs.keys())
    bits_width = max(1, cw.bit_length()); bits_height = max(1, ch.bit_length())
    bits_xoffset = 1; bits_yoffset = 1; bits_delta = max(1, cw.bit_length())
    def enc(rows):
        bw = BitWriter(); bw.put(0, 3); bw.put(cw, bits_width); bw.put(ch, bits_height)
        bw.put_signed(0, bits_xoffset); bw.put_signed(0, bits_yoffset); bw.put(cw, bits_delta)
        for r in range(ch):
            bw.put(0, 1)
            for c in range(cw): bw.put(rows[r][c], 1)
        bw.pad_to_byte(); return bw.bytes()
    data = bytearray(); data += enc([[0]*cw for _ in range(ch)])
    off = {}
    for c in codes:
        off[c] = len(data); data += enc(glyphs[c])
    bits_index = max(1, len(data).bit_length())
    iw = BitWriter()
    for c in range(0x20, 0x7f): iw.put(off.get(c, 0), bits_index)
    iw.pad_to_byte(); index = iw.bytes()
    # round-trip verify
    dbits = []
    for b in data:
        for j in range(7, -1, -1): dbits.append((b >> j) & 1)
    for c in codes:
        bo = off[c] * 8
        assert fb_u(dbits, bo, 3) == 0; bo += 3
        gw = fb_u(dbits, bo, bits_width); bo += bits_width
        gh = fb_u(dbits, bo, bits_height); bo += bits_height
        xo = fb_s(dbits, bo, bits_xoffset); bo += bits_xoffset
        yo = fb_s(dbits, bo, bits_yoffset); bo += bits_yoffset
        dl = fb_u(dbits, bo, bits_delta); bo += bits_delta
        assert (gw, gh, xo, yo, dl) == (cw, ch, 0, 0, cw), f"0x{c:02x} hdr"
        for r in range(gh):
            assert dbits[bo] == 0; bo += 1
            for c2 in range(gw):
                assert dbits[bo] == glyphs[c][r][c2], f"0x{c:02x} px({r},{c2})"; bo += 1
    ibits = []
    for b in index:
        for j in range(7, -1, -1): ibits.append((b >> j) & 1)
    for n, c in enumerate(range(0x20, 0x7f)):
        assert fb_u(ibits, n * bits_index, bits_index) == off.get(c, 0)
    line_space = ch + max(2, ch // 8); cap_height = ch
    def carr(nm, b):
        s = [f"static const unsigned char {nm}[] = {{"]
        for i in range(0, len(b), 16):
            s.append("  " + ", ".join(f"0x{x:02X}" for x in b[i:i+16]) + ",")
        s.append("};"); return "\n".join(s)
    o = []
    o.append("// Auto-generated by bdf_to_ili9341.py — DO NOT EDIT.")
    o.append("// Glyph bitmaps from Terminus Font 4.49 (SIL OFL 1.1, RFN \"Terminus Font\");")
    o.append(f"// {srcnote}  See OFL.txt in this directory.")
    o.append(f"// Font: {name}  (cell {cw}x{ch}, monospace, {len(codes)} glyphs, ASCII 0x20-0x7e)")
    o.append("// Round-trip verified: every glyph decodes identically via ILI9341_t3 bit logic.")
    o.append('#include "kcm_ili9341_font.h"')
    o.append(""); o.append(carr(f"{name}_data", bytes(data)))
    o.append(""); o.append(carr(f"{name}_index", bytes(index)))
    o.append(""); o.append(f"const ILI9341_t3_font_t {name} = {{")
    o.append(f"  {name}_index,"); o.append("  0, // unicode"); o.append(f"  {name}_data,")
    o.append("  1, // version"); o.append("  0, // reversed")
    o.append("  32, // index1_first"); o.append("  126, // index1_last")
    o.append("  0, // index2_first"); o.append("  0, // index2_last")
    o.append(f"  {bits_index}, // bits_index"); o.append(f"  {bits_width}, // bits_width")
    o.append(f"  {bits_height}, // bits_height"); o.append(f"  {bits_xoffset}, // bits_xoffset")
    o.append(f"  {bits_yoffset}, // bits_yoffset"); o.append(f"  {bits_delta}, // bits_delta")
    o.append(f"  {line_space}, // line_space"); o.append(f"  {cap_height}, // cap_height")
    o.append("};"); o.append("")
    os.makedirs(outdir, exist_ok=True)
    open(os.path.join(outdir, f"{name}.c"), "w").write("\n".join(o))
    return cw, ch, len(data), len(index), bits_index

if __name__ == "__main__":
    outdir, base = sys.argv[1], sys.argv[2]
    native = {}   # size -> (cw,ch,glyphs)
    specs = sys.argv[3:]
    # first pass: native sizes (SIZE=path)
    for tok in specs:
        if "=2x" in tok: continue
        size, path = tok.split("=")
        cw, ch, g = parse_bdf(path)
        native[int(size)] = (cw, ch, g)
        rc, rh, dl, il, bi = emit(f"{base}_{size}", cw, ch, g, outdir, f"native {os.path.basename(path)}")
        print(f"  OK {base}_{size:<3} cell={rc}x{rh} data={dl}B index={il}B bits_index={bi}")
    # second pass: doubled sizes (N=2xM), M must be a generated native
    for tok in specs:
        if "=2x" not in tok: continue
        n, m = tok.split("=2x"); n, m = int(n), int(m)
        assert n == 2 * m and m in native, f"{tok}: need native {m}"
        cw, ch, g = double_glyphs(*native[m])
        rc, rh, dl, il, bi = emit(f"{base}_{n}", cw, ch, g, outdir, f"2x the native {m}px strike")
        print(f"  OK {base}_{n:<3} (2x {m}) cell={rc}x{rh} data={dl}B index={il}B bits_index={bi}")
    print("All fonts generated and round-trip verified.")
