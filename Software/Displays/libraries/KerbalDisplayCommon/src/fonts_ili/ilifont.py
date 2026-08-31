"""Parse / decode / re-emit ILI9341_t3 1-bit font .c files.

Format (as documented by fonts_ili/bdf_to_ili9341.py and as read by
KerbalDisplayCommon's kcmDrawString):
  index[] : bit-packed, bits_index wide, value = BYTE offset of the glyph in data[]
  data[]  : per glyph, MSB-first  [3b enc=0][width][height][xoff][yoff][delta]
            then per row [1b flag][width bits]; each glyph byte-padded.
The renderer ignores xoffset/yoffset and draws from row 0 of the line, so a
glyph's vertical position lives inside its own bitmap.
"""
import re

class Font:
    FIELDS = ["index1_first","index1_last","index2_first","index2_last","bits_index",
              "bits_width","bits_height","bits_xoffset","bits_yoffset","bits_delta",
              "line_space","cap_height"]

    def __init__(self, path):
        self.path = path
        src = open(path).read()
        self.src = src
        self.name = re.search(r'const ILI9341_t3_font_t (\w+) =', src).group(1)
        self.index = self._array(src, self.name + "_index")
        self.data  = self._array(src, self.name + "_data")
        tail = src[src.rindex('const ILI9341_t3_font_t'):]
        self.f = {}
        for k in self.FIELDS:
            m = re.search(r'(-?\d+),\s*//\s*' + k, tail)
            self.f[k] = int(m.group(1)) if m else 0
        self.version  = int(re.search(r'(\d+),\s*//\s*version', tail).group(1))
        self.reversed = int(re.search(r'(\d+),\s*//\s*reversed', tail).group(1))
        self.unicode  = int(re.search(r'(\d+),\s*//\s*unicode',  tail).group(1))

    @staticmethod
    def _array(src, sym):
        m = re.search(re.escape(sym) + r'\s*\[\s*\]\s*=\s*\{(.*?)\n\};', src, re.S)
        return bytearray(int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', m.group(1)))

    # --- bit access, matching _fetchbits_unsigned ---
    @staticmethod
    def bits(buf, pos, n):
        v = 0
        for k in range(n):
            i = pos + k
            v = (v << 1) | ((buf[i >> 3] >> (7 - (i & 7))) & 1)
        return v

    def codes(self):
        out = list(range(self.f["index1_first"], self.f["index1_last"] + 1))
        if self.f["index2_last"] >= self.f["index2_first"] and self.f["index2_first"]:
            out += list(range(self.f["index2_first"], self.f["index2_last"] + 1))
        return out

    def slot(self, code):
        f = self.f
        if f["index1_first"] <= code <= f["index1_last"]:
            return code - f["index1_first"]
        if f["index2_first"] and f["index2_first"] <= code <= f["index2_last"]:
            return (f["index1_last"] - f["index1_first"] + 1) + (code - f["index2_first"])
        return None

    def offset(self, code):
        s = self.slot(code)
        return None if s is None else self.bits(self.index, s * self.f["bits_index"],
                                                self.f["bits_index"])

    def glyph(self, code):
        """-> dict(w,h,xoff,yoff,delta,rows[list of int bitmasks, MSB=leftmost])"""
        off = self.offset(code)
        if off is None: return None
        f = self.f
        d = self.data[off:]
        bo = 3
        w = self.bits(d, bo, f["bits_width"]);   bo += f["bits_width"]
        h = self.bits(d, bo, f["bits_height"]);  bo += f["bits_height"]
        xo = self.bits(d, bo, f["bits_xoffset"]); bo += f["bits_xoffset"]
        yo = self.bits(d, bo, f["bits_yoffset"]); bo += f["bits_yoffset"]
        dl = self.bits(d, bo, f["bits_delta"]);  bo += f["bits_delta"]
        rows = []
        for r in range(h):
            base = bo + r * (1 + w) + 1          # skip the per-row flag, as the renderer does
            rows.append([self.bits(d, base + c, 1) for c in range(w)])
        return dict(w=w, h=h, xoff=xo, yoff=yo, delta=dl, rows=rows, off=off)
