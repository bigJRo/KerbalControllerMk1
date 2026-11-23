"""
kc_frame.py -- the common A3 drawing frame for the KC-01 drawing package.

Every board has five documents, numbered from the board's base number:
    KC-01-XXX0  CCA Assembly Drawing (the finished module, from Fusion 360)
    KC-01-XXX1  Schematic                    (SCH)
    KC-01-XXX2  PCB Fabrication Drawing      (PWB)
    KC-01-XXX3  Bill of Materials            (BOM)
    KC-01-XXX4  Assembly Placement Drawing   (PLACE)
File names: KC-01-XXXn_RevY_<Name>_<TYPE>.pdf (no type suffix for XXX0).
Revisions are letters, first release "-", then A, B, ...

The frame is drawn in millimetres on an A3 landscape sheet: border with
zones 1-8 / A-F, revision block (top right), drawing-package strip and title
block (bottom right) with the KASA and Jeb's Controller Works logos.
"""
import base64
import html
import os

W, H = 420.0, 297.0          # A3 landscape, mm
PAPER = "A3"                 # sheet size shown in the title block; scales are true at this size
M_OUT, M_IN = 5.0, 10.0      # outer trim line, inner frame (zone strip between)
COLS, ROWS = 8, 6
FONT = "Liberation Sans, Arial, Helvetica, sans-serif"
INK = "#1a1a1a"
GREY = "#6b6b6b"
HI = "#e8eef5"
CUR = "#d9e6f2"
HERE = os.path.dirname(os.path.abspath(__file__))


def _b64(name):
    return base64.b64encode(open(os.path.join(HERE, "assets", name), "rb").read()).decode()


JCW = _b64("jcw_logo.png")          # 640 x 555
KASA = _b64("kasa_meatball.png")    # 900 x 776 (2160 x 1862 source)

DOC_TYPES = {"0": ("ASSY", "CCA Assembly Drawing"), "1": ("SCH", "Schematic"),
             "2": ("PWB", "PCB Fabrication Drawing"), "3": ("BOM", "Bill of Materials"),
             "4": ("PLACE", "Assembly Placement Drawing")}

TB_W, TB_H, PKG_H = 190.0, 50.0, 7.0     # title block and package strip sizes


def esc(s):
    return html.escape(str(s))


class Sheet:
    """One A3 sheet built as SVG primitives in mm."""

    def __init__(self):
        self.o = []

    def add(self, s):
        self.o.append(s)

    def line(self, x1, y1, x2, y2, w=0.25, c=INK, dash=None, cap="butt"):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        self.o.append(f'<line x1="{x1:.3f}" y1="{y1:.3f}" x2="{x2:.3f}" y2="{y2:.3f}" stroke="{c}" '
                      f'stroke-width="{w}" stroke-linecap="{cap}"{d}/>')

    def rect(self, x, y, w, h, sw=0.25, fill="none", c=INK, rx=0):
        self.o.append(f'<rect x="{x:.3f}" y="{y:.3f}" width="{w:.3f}" height="{h:.3f}" rx="{rx}" '
                      f'fill="{fill}" stroke="{c}" stroke-width="{sw}"/>')

    def circle(self, x, y, r, sw=0.25, fill="none", c=INK):
        self.o.append(f'<circle cx="{x:.3f}" cy="{y:.3f}" r="{r:.3f}" fill="{fill}" stroke="{c}" stroke-width="{sw}"/>')

    def text(self, x, y, s, size=2.5, anchor="start", weight="normal", c=INK, rot=None, italic=False):
        r = f' transform="rotate({rot} {x:.3f} {y:.3f})"' if rot is not None else ""
        it = ' font-style="italic"' if italic else ""
        self.o.append(f'<text x="{x:.3f}" y="{y:.3f}" font-size="{size}" font-weight="{weight}" '
                      f'text-anchor="{anchor}" fill="{c}"{r}{it}>{esc(s)}</text>')

    def image(self, x, y, w, h, href):
        self.o.append(f'<image x="{x:.3f}" y="{y:.3f}" width="{w:.3f}" height="{h:.3f}" '
                      f'preserveAspectRatio="none" href="{href}"/>')

    def cell(self, x, y, w, h, label, value="", vsize=3.2, vweight="normal", align="start", fill="none"):
        self.rect(x, y, w, h, fill=fill)
        self.text(x + 1.2, y + 2.6, label, size=1.9, c=GREY)
        if value != "":
            vx = x + 1.6 if align == "start" else (x + w / 2 if align == "middle" else x + w - 1.6)
            self.text(vx, y + h - 1.8, value, size=vsize, weight=vweight, anchor=align)

    def table(self, x, y, cols, rows, rh=4.6, hh=5.2, size=2.3, hsize=2.2, zebra=True, wrap=None):
        """cols: [(title, width, align)], rows: list of lists. Returns the bottom y."""
        tw = sum(c[1] for c in cols)
        self.rect(x, y, tw, hh, fill=HI)
        cx = x
        for title, w, al in cols:
            self.text(cx + w / 2, y + hh - 1.6, title, size=hsize, anchor="middle", weight="bold")
            cx += w
        yy = y + hh
        for k, r in enumerate(rows):
            lines = 1
            if wrap:
                lines = max(len(wrap(str(v), w, size)) for v, (_, w, _) in zip(r, cols))
            h = rh + (lines - 1) * size * 1.15
            if zebra and k % 2:
                self.rect(x, yy, tw, h, fill="#f5f7f9", sw=0)
            cx = x
            for v, (_, w, al) in zip(r, cols):
                parts = wrap(str(v), w, size) if wrap else [str(v)]
                for li, part in enumerate(parts):
                    ty = yy + rh - 1.4 + li * size * 1.15
                    if al == "middle":
                        self.text(cx + w / 2, ty, part, size=size, anchor="middle")
                    elif al == "end":
                        self.text(cx + w - 1.2, ty, part, size=size, anchor="end")
                    else:
                        self.text(cx + 1.2, ty, part, size=size)
                cx += w
            yy += h
            self.line(x, yy, x + tw, yy, 0.15, "#9aa3ad")
        self.rect(x, y, tw, yy - y)
        cx = x
        for _, w, _ in cols[:-1]:
            cx += w
            self.line(cx, y, cx, yy, 0.15, "#9aa3ad")
        return yy

    def svg(self):
        return (f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
                f'width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}" font-family="{FONT}">\n'
                f'<rect width="{W}" height="{H}" fill="#fff"/>\n' + "\n".join(self.o) + "\n</svg>")


def wrap_text(s, width_mm, size):
    """Crude word wrap for table cells: about 0.5 x size mm per character."""
    maxc = max(4, int((width_mm - 2.4) / (size * 0.5)))
    words, out, cur = s.split(" "), [], ""
    for wd in words:
        if len(cur) + len(wd) + (1 if cur else 0) <= maxc:
            cur = (cur + " " + wd) if cur else wd
        else:
            if cur:
                out.append(cur)
            cur = wd
    out.append(cur)
    return out or [""]


def frame(s, f):
    """Draw the border, zones, revision block, package strip and title block.
    f: dict with dwg_no, rev, title, subtitle, doc_type, status, created_by, approved_by,
    date, board_version, scale, sheet, sheets, revisions. Returns the free drawing area."""
    s.rect(M_OUT, M_OUT, W - 2 * M_OUT, H - 2 * M_OUT, sw=0.35)
    s.rect(M_IN, M_IN, W - 2 * M_IN, H - 2 * M_IN, sw=0.7)
    cw = (W - 2 * M_IN) / COLS
    rh = (H - 2 * M_IN) / ROWS
    for i in range(COLS):
        cx = M_IN + (i + 0.5) * cw
        for y in (M_OUT + 3.6, H - M_OUT - 1.4):
            s.text(cx, y, str(i + 1), size=2.8, anchor="middle")
        if i:
            x = M_IN + i * cw
            s.line(x, M_OUT, x, M_IN, 0.35)
            s.line(x, H - M_IN, x, H - M_OUT, 0.35)
    for j in range(ROWS):
        cy = M_IN + (j + 0.5) * rh + 1
        for x in (M_OUT + 2.5, W - M_OUT - 2.5):
            s.text(x, cy, "ABCDEF"[j], size=2.8, anchor="middle")
        if j:
            y = M_IN + j * rh
            s.line(M_OUT, y, M_IN, y, 0.35)
            s.line(W - M_IN, y, W - M_OUT, y, 0.35)
    s.line(W / 2, M_OUT, W / 2, M_IN + 4, 0.5)
    s.line(W / 2, H - M_IN - 4, W / 2, H - M_OUT, 0.5)
    s.line(M_OUT, H / 2, M_IN + 4, H / 2, 0.5)
    s.line(W - M_IN - 4, H / 2, W - M_OUT, H / 2, 0.5)

    # revision block, top right
    rx, ry, rw = W - M_IN - 170, M_IN, 170
    s.rect(rx, ry, rw, 6, fill=HI)
    s.text(rx + rw / 2, ry + 4.3, "REVISIONS", size=2.8, anchor="middle", weight="bold")
    cols = [("REV", 12), ("DESCRIPTION", 96), ("DATE", 30), ("APPROVED", 32)]
    x = rx
    for name, w in cols:
        s.rect(x, ry + 6, w, 5)
        s.text(x + w / 2, ry + 9.6, name, size=2.1, anchor="middle", weight="bold")
        x += w
    revs = f.get("revisions") or [("-", "Initial release", f.get("date", ""), f.get("approved_by", ""))]
    for k in range(max(3, len(revs))):
        y = ry + 11 + k * 5.5
        x = rx
        r = revs[k] if k < len(revs) else ("", "", "", "")
        for (name, w), v in zip(cols, r):
            s.rect(x, y, w, 5.5)
            if name == "DESCRIPTION":
                s.text(x + 1.5, y + 3.9, v, size=2.4)
            else:
                s.text(x + w / 2, y + 3.9, v, size=2.4, anchor="middle")
            x += w
    rev_bottom = ry + 11 + max(3, len(revs)) * 5.5

    # title block, bottom right
    tw, th = TB_W, TB_H
    tx, ty = W - M_IN - tw, H - M_IN - th
    s.cell(tx, ty, 24, 9, "Project", f.get("project_short", "KCMk1"), vsize=2.8)
    s.cell(tx + 24, ty, 14, 9, "Size", PAPER, vsize=2.8, vweight="bold")
    s.cell(tx + 38, ty, 28, 9, "Scale / Units", f"{f.get('scale', 'NTS')}  ·  mm", vsize=2.8)
    s.cell(tx + 66, ty, 62, 9, "Created by", f.get("created_by", ""), vsize=2.8)
    s.text(tx + 126.5, ty + 7.2, f.get("created_date", f.get("date", "")), size=2.5, anchor="end")
    s.cell(tx + 128, ty, 62, 9, "Approved by", f.get("approved_by", ""), vsize=2.8)
    if f.get("status") == "RELEASED":
        s.text(tx + 188.5, ty + 7.2, f.get("approved_date", f.get("date", "")), size=2.5, anchor="end")
    lx, ly, lw, lh = tx, ty + 9, 62, th - 9
    s.rect(lx, ly, lw, lh)
    kw = 26.5
    kh = kw * 1862 / 2160
    s.add(f'<image x="{lx + 2:.2f}" y="{ly + (lh - kh) / 2:.2f}" width="{kw:.2f}" height="{kh:.2f}" '
          f'href="data:image/png;base64,{KASA}"/>')
    jw = 30.5
    jh = jw * 555 / 640
    s.add(f'<image x="{lx + lw - jw - 1.5:.2f}" y="{ly + (lh - jh) / 2:.2f}" width="{jw:.2f}" height="{jh:.2f}" '
          f'href="data:image/png;base64,{JCW}"/>')
    rx2, rw2 = tx + lw, tw - lw
    A = 72
    s.cell(rx2, ty + 9, A, 10, "Document type", f.get("doc_type", ""), vsize=3.2)
    st = f.get("status", "DRAFT")
    s.cell(rx2 + A, ty + 9, rw2 - A, 10, "Document status", st, vsize=3.2, vweight="bold",
           fill="#fff4d6" if st != "RELEASED" else "none")
    s.rect(rx2, ty + 19, A, 20)
    s.text(rx2 + 1.2, ty + 21.6, "Title", size=1.9, c=GREY)
    s.text(rx2 + 1.6, ty + 28.5, f.get("title", ""), size=4.0, weight="bold")
    s.text(rx2 + 1.6, ty + 35.5, f.get("subtitle", "Kerbal Controller Mk1"), size=3.0)
    s.rect(rx2 + A, ty + 19, rw2 - A, 20)
    s.text(rx2 + A + 1.2, ty + 21.6, "DWG No.", size=1.9, c=GREY)
    s.text(rx2 + A + (rw2 - A) / 2, ty + 33, f.get("dwg_no", ""), size=5.6, anchor="middle", weight="bold")
    y4, h4 = ty + 39, th - 39
    s.cell(rx2, y4, 16, h4, "Rev.", f.get("rev", "-"), vsize=5.0, vweight="bold", align="middle")
    s.cell(rx2 + 16, y4, 26, h4, "Board version", f.get("board_version", ""), vsize=3.4, align="middle")
    s.cell(rx2 + 42, y4, 30, h4, "Date of issue", f.get("date", ""), vsize=3.4, align="middle")
    s.cell(rx2 + 72, y4, rw2 - 72, h4, "Sheet", f"{f.get('sheet', 1)} / {f.get('sheets', 1)}", vsize=4.2, align="middle")

    # drawing package strip, one row above the title block
    py = ty - PKG_H
    s.rect(tx, py, 20, PKG_H, fill=HI)
    s.text(tx + 10, py + 3.0, "DRAWING", size=2.0, anchor="middle", weight="bold")
    s.text(tx + 10, py + 5.6, "PACKAGE", size=2.0, anchor="middle", weight="bold")
    base = f.get("dwg_no", "KC-01-0000")[:-1]
    cw5 = (tw - 20) / 5
    for k in range(5):
        d = str(k)
        code, name = DOC_TYPES[d]
        cur = f.get("dwg_no", "").endswith(d)
        cx = tx + 20 + k * cw5
        s.rect(cx, py, cw5, PKG_H, fill=CUR if cur else "none", sw=0.5 if cur else 0.25)
        s.text(cx + cw5 / 2, py + 3.0, f"{base}{d} {code}", size=2.4, anchor="middle", weight="bold" if cur else "normal")
        s.text(cx + cw5 / 2, py + 5.7, name, size=1.7, anchor="middle", c=GREY)
    return {"x0": M_IN + 4, "y0": M_IN + 4, "x1": W - M_IN - 4, "y1": H - M_IN - 4,
            "tb_x": tx, "tb_y": py, "rev_x": rx, "rev_y": rev_bottom}


def file_name(f, name):
    d = f["dwg_no"][-1]
    suffix = "" if d == "0" else "_" + DOC_TYPES[d][0]
    return f"{f['dwg_no']}_Rev{f.get('rev', '-')}_{name}{suffix}.pdf"


def notes_box(s, x, y, w, h, notes, title="NOTES", size=2.5):
    s.rect(x, y, w, h)
    s.rect(x, y, w, 6, fill=HI)
    s.text(x + 2, y + 4.2, title, size=2.6, weight="bold")
    yy = y + 10.5
    for n in notes:
        for k, part in enumerate(wrap_text(n, w - 4, size)):
            s.text(x + 2 + (3.2 if k else 0), yy, part, size=size)
            yy += size * 1.45
        yy += 0.6
    return yy
