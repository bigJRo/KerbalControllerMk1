#!/usr/bin/env python3
"""
kc_package.py -- build a board's drawing package (SCH, PWB, BOM, PLACE) in the
common A3 frame (kc_frame.py) from the files EasyEDA Pro exports.

    python3 kc_package.py boards/KC-01-2700.json --out <dir> [--docs 1,2,3,4]

The board config (JSON) names the input files and the title-block fields:
    sch_pdf      EasyEDA schematic PDF (all pages)
    gerber_dir   unzipped Gerber + drill files
    bom_csv      EasyEDA BOM export (UTF-16, tab separated)
    pnp_csv      EasyEDA pick-and-place export (UTF-16, tab separated)
Paths in the config are relative to the config file.

Needs: gerbonara, numpy, scikit-image, pillow (pip), poppler (pdftocairo) and
Playwright with Chromium (node) for printing to PDF.
"""
import argparse
import csv
import glob
import io
import json
import math
import os
import re
import subprocess
import sys
import tempfile

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from kc_frame import (DOC_TYPES, doc_types, GREY, HI, H, INK, M_IN, PAPER, W, Sheet, esc, file_name,  # noqa: E402
                      frame, notes_box, wrap_text)

# --------------------------------------------------------------------------- printing


def print_pdf(sheets, out_pdf, work):
    """sheets: list of Sheet. Writes one HTML page per sheet and prints with Chromium."""
    os.makedirs(work, exist_ok=True)
    body = "".join(f'<div class="pg">{s.svg()}</div>' for s in sheets)
    hp = os.path.join(work, "doc.html")
    open(hp, "w", encoding="utf-8").write(
        '<!doctype html><html><head><meta charset="utf-8"><style>@page{size:420mm 297mm;margin:0}'
        'html,body{margin:0}.pg{width:420mm;height:297mm;page-break-after:always;overflow:hidden}'
        '.pg:last-child{page-break-after:auto}</style></head><body>' + body + '</body></html>')
    js = """const { chromium } = require('playwright');
(async () => { const b = await chromium.launch(); const p = await b.newPage();
await p.goto('file://' + process.argv[2], {waitUntil: 'load', timeout: 0});
await p.pdf({path: process.argv[3], width: '420mm', height: '297mm', printBackground: true, timeout: 0});
await b.close(); })();"""
    jp = os.path.join(work, "print.js")
    open(jp, "w").write(js)
    subprocess.run(["node", jp, os.path.abspath(hp), os.path.abspath(out_pdf)], check=True)


def png_href(img, path):
    img.save(path, optimize=True)
    return "file://" + os.path.abspath(path)

# --------------------------------------------------------------------------- Gerber rendering

_LAYER_N = [0]


def layer_svg(path, bounds, fg, bg="white", pad=1.0):
    """A Gerber layer as an inline SVG fragment (vector), drawn by gerbonara.
    Returns (svg_body_with_unique_ids, (x0, y_top, width, height)) in board mm, y up."""
    from gerbonara import GerberFile
    from gerbonara.utils import MM
    (x0, y0), (x1, y1) = bounds
    fb = ((x0 - pad, y0 - pad), (x1 + pad, y1 + pad))
    svg = str(GerberFile.open(path).to_svg(force_bounds=fb, fg=fg, bg=bg, arg_unit=MM, svg_unit=MM))
    _LAYER_N[0] += 1
    pre = f"g{_LAYER_N[0]}-"
    body = svg[svg.index(">", svg.index("<svg")) + 1: svg.rindex("</svg>")]
    body = re.sub(r'id="([^"]+)"', lambda m: f'id="{pre}{m.group(1)}"', body)
    body = re.sub(r'(xlink:href|href)="#([^"]+)"', lambda m: f'{m.group(1)}="#{pre}{m.group(2)}"', body)
    body = re.sub(r"url\(#([^)]+)\)", lambda m: f"url(#{pre}{m.group(1)})", body)
    vb = re.search(r'viewBox="([^"]+)"', svg).group(1)
    return body, vb, (x0 - pad, y1 + pad, x1 - x0 + 2 * pad, y1 - y0 + 2 * pad)


def place_layer(s, path, bounds, x, y, k, fg, bg="white", mirror=False):
    """Draw a Gerber layer on the sheet with its window's top-left at (x, y), scale k."""
    body, vb, (wx, wy, ww, wh) = layer_svg(path, bounds, fg, bg)
    w, h = ww * k, wh * k
    inner = f'<svg x="0" y="0" width="{w:.3f}" height="{h:.3f}" viewBox="{vb}" overflow="hidden">{body}</svg>'
    if mirror:
        s.add(f'<g transform="translate({x + w:.3f} {y:.3f}) scale(-1 1)">{inner}</g>')
    else:
        s.add(f'<g transform="translate({x:.3f} {y:.3f})">{inner}</g>')
    return wx, wy, ww, wh

# --------------------------------------------------------------------------- board data


def read_utf16_tsv(path):
    raw = open(path, "rb").read()
    txt = raw.decode("utf-16") if raw[:2] in (b"\xff\xfe", b"\xfe\xff") else raw.decode("utf-8-sig")
    return list(csv.DictReader(io.StringIO(txt), delimiter="\t"))


def outline_info(gdir):
    """Board outline: bounds from the outline's centrelines (independent of the aperture), corner
    radius from its arcs, or, for an outline exported as short segments, from where the straight
    top edge starts."""
    from gerbonara import GerberFile
    from gerbonara.utils import MM
    g = GerberFile.open(glob.glob(os.path.join(gdir, "*.GKO"))[0])
    pts, radii = [], []
    for o in g.objects:
        n = type(o).__name__
        if n in ("Line", "Arc"):
            pts += [(o.x1, o.y1), (o.x2, o.y2)]
        if n == "Arc":
            radii.append(round(math.hypot(o.cx, o.cy), 3))
    if pts:
        x0, x1 = min(p[0] for p in pts), max(p[0] for p in pts)
        y0, y1 = min(p[1] for p in pts), max(p[1] for p in pts)
    else:
        (x0, y0), (x1, y1) = g.bounding_box(MM)
    if radii:
        r = max(radii)
    else:
        top = [p[0] for p in pts if abs(p[1] - y1) < 1e-3]
        r = round(min(top) - x0, 2) if top else 0.0
    return {"bounds": ((x0, y0), (x1, y1)), "w": x1 - x0, "h": y1 - y0, "radius": r}


def _read_excellon(path):
    """EasyEDA Excellon (metric, decimal coordinates): yields (tool_dia, plated_flag, x, y, x2, y2);
    x2/y2 are the far end of a routed slot (G85) or None for a drilled hole."""
    tools, cur, plated = {}, None, None
    for raw in open(path, encoding="utf-8", errors="replace"):
        ln = raw.strip()
        if not ln:
            continue
        if ln.startswith(";"):
            if "TYPE=NON_PLATED" in ln.upper():
                plated = False
            elif "TYPE=PLATED" in ln.upper():
                plated = True
            continue
        m = re.fullmatch(r"T(\d+)C([\d.]+)", ln)
        if m:
            tools[int(m.group(1))] = float(m.group(2)); continue
        m = re.fullmatch(r"T(\d+)", ln)
        if m:
            cur = tools.get(int(m.group(1))); continue
        m = re.fullmatch(r"X(-?[\d.]+)Y(-?[\d.]+)(?:G85X(-?[\d.]+)Y(-?[\d.]+))?", ln)
        if m:
            if "." not in m.group(1) + m.group(2):
                raise SystemExit(f"{path}: coordinates without a decimal point are not supported: {ln}")
            x2 = float(m.group(3)) if m.group(3) else None
            y2 = float(m.group(4)) if m.group(4) else None
            yield cur, plated, float(m.group(1)), float(m.group(2)), x2, y2
            continue
        if ln in ("M48", "%", "G05", "G90", "M30", "M95") or ln.startswith(("METRIC", "INCH", "FMAT", "G93")):
            if ln.startswith("INCH"):
                raise SystemExit(f"{path}: inch drill files are not supported")
            continue
        raise SystemExit(f"{path}: unrecognised drill file line: {ln}")


def drills(gdir):
    """All drilled holes and routed slots. A hole listed in two drill files (EasyEDA can repeat
    vias in the PTH file and the via file) is counted once, as a via if either file says so.
    A slot has "slot" = its overall length and "x2"/"y2" = the far end of its centreline."""
    found = {}
    for f in sorted(glob.glob(os.path.join(gdir, "*.DRL"))):
        via = "via" in os.path.basename(f).lower()
        npth = "npth" in os.path.basename(f).lower()
        for dt, pl, x, y, x2, y2 in _read_excellon(f):
            d = round(round(dt * 20) / 20, 2)
            key = (round(x, 3), round(y, 3), round(dt, 3))
            if key in found:
                found[key]["via"] |= via
                continue
            h = {"x": x, "y": y, "d": d, "dt": dt, "plated": pl is not False and not npth, "via": via, "slot": 0.0}
            if x2 is not None:
                h.update(x2=x2, y2=y2, slot=round(math.hypot(x2 - x, y2 - y) + dt, 2))
            found[key] = h
    return list(found.values())


def tool_of(h):
    return (h["d"], h["plated"], h.get("slot", 0.0))


def hole_svg(x1, y1, x2, y2, w, fill, stroke, sw, dash=""):
    """A drilled hole (x2, y2 = x1, y1) or a slot, drawn at width w with an outline."""
    if (x1, y1) == (x2, y2):
        return (f'<circle cx="{x1:.3f}" cy="{y1:.3f}" r="{w / 2:.3f}" fill="{fill}" stroke="{stroke}" '
                f'stroke-width="{sw}"{dash}/>')
    line = f'x1="{x1:.3f}" y1="{y1:.3f}" x2="{x2:.3f}" y2="{y2:.3f}" stroke-linecap="round"'
    return (f'<line {line} stroke="{stroke}" stroke-width="{w + sw:.3f}"/>'
            f'<line {line} stroke="{fill}" stroke-width="{max(w - sw, 0.01):.3f}"/>')


# --------------------------------------------------------------------------- symbols


SYMS = ["circle", "square", "triangle", "diamond", "cross", "hex", "star", "xcircle"]


def symbol(s, kind, x, y, r=0.9, sw=0.18, c=INK):
    if kind == "circle":
        s.circle(x, y, r, sw, c=c)
    elif kind == "square":
        s.rect(x - r, y - r, 2 * r, 2 * r, sw, c=c)
    elif kind == "triangle":
        s.add(f'<polygon points="{x},{y - r} {x + r},{y + r * 0.8} {x - r},{y + r * 0.8}" fill="none" stroke="{c}" stroke-width="{sw}"/>')
    elif kind == "diamond":
        s.add(f'<polygon points="{x},{y - r} {x + r},{y} {x},{y + r} {x - r},{y}" fill="none" stroke="{c}" stroke-width="{sw}"/>')
    elif kind == "cross":
        s.line(x - r, y - r, x + r, y + r, sw, c)
        s.line(x - r, y + r, x + r, y - r, sw, c)
    elif kind == "hex":
        pts = " ".join(f"{x + r * math.cos(math.pi / 3 * k):.3f},{y + r * math.sin(math.pi / 3 * k):.3f}" for k in range(6))
        s.add(f'<polygon points="{pts}" fill="none" stroke="{c}" stroke-width="{sw}"/>')
    elif kind == "star":
        s.line(x - r, y, x + r, y, sw, c)
        s.line(x, y - r, x, y + r, sw, c)
        s.line(x - r * 0.7, y - r * 0.7, x + r * 0.7, y + r * 0.7, sw, c)
        s.line(x - r * 0.7, y + r * 0.7, x + r * 0.7, y - r * 0.7, sw, c)
    else:
        s.circle(x, y, r, sw, c=c)
        s.line(x - r * 0.7, y - r * 0.7, x + r * 0.7, y + r * 0.7, sw, c)

# --------------------------------------------------------------------------- dimensions


def dim_h(s, x1, x2, y, ytext, label, ext_from=None, size=2.6):
    """Horizontal dimension between x1 and x2 drawn at y; extension lines from ext_from (y)."""
    if ext_from is not None:
        for x in (x1, x2):
            s.line(x, ext_from + (0.8 if y > ext_from else -0.8), x, y + (1.2 if y > ext_from else -1.2), 0.15)
    s.line(x1, y, x2, y, 0.18)
    for x, d in ((x1, 1), (x2, -1)):
        s.add(f'<polygon points="{x},{y} {x + d * 2.2},{y - 0.6} {x + d * 2.2},{y + 0.6}" fill="{INK}"/>')
    s.text((x1 + x2) / 2, ytext, label, size=size, anchor="middle")


def dim_v(s, y1, y2, x, label, ext_from=None, size=2.6):
    if ext_from is not None:
        for y in (y1, y2):
            s.line(ext_from + (0.8 if x > ext_from else -0.8), y, x + (1.2 if x > ext_from else -1.2), y, 0.15)
    s.line(x, y1, x, y2, 0.18)
    for y, d in ((y1, 1), (y2, -1)):
        s.add(f'<polygon points="{x},{y} {x - 0.6},{y + d * 2.2} {x + 0.6},{y + d * 2.2}" fill="{INK}"/>')
    tx = x - 1.4 if x < ext_from else x + 3.0
    s.text(tx, (y1 + y2) / 2, label, size=size, anchor="middle", rot=-90)

# --------------------------------------------------------------------------- documents


def fields(cfg, d, sheet, sheets, scale):
    return dict(dwg_no=cfg["base"] + d, rev=cfg.get("rev", "-"), title=cfg.get("doc_title", {}).get(d, cfg["title"]), subtitle=cfg["subtitle"],
                doc_type=doc_types(cfg)[d][1], status=cfg.get("doc_status", {}).get(d, cfg.get("status", "DRAFT")), created_by=cfg["created_by"],
                approved_by=cfg["approved_by"], date=cfg["date"], board_version=cfg["board_version"],
                scale=scale, sheet=sheet, sheets=sheets, revisions=cfg.get("revisions"),
                package_docs=cfg.get("package_docs"), doc_types=cfg.get("doc_types"),
                version_label=cfg.get("version_label"))


PWB_K = 1.5             # sheet 1 board view scale (3:2)
LAYER_K = 1.75          # copper layer sheets scale (7:4), one layer per sheet
LAYER_COLOURS = ["#c0392b", "#d68910", "#1e8449", "#2e5fa8"]  # top red, inner 1 amber, inner 2 green, bottom blue


def _ratio(k):
    from fractions import Fraction
    f = Fraction(k).limit_denominator(8)
    return f"{f.numerator}:{f.denominator}"


def doc_pwb(cfg, work):
    gdir = cfg["gerber_dir"]
    ol = outline_info(gdir)
    holes = drills(gdir)
    (bx0, by0), (bx1, by1) = ol["bounds"]
    layers = cfg["layers"]
    sep = bool(cfg.get("pwb_tables_sheet"))     # drill chart and stackup on a sheet of their own
    total = 1 + sep + len(layers)
    sheets = []

    # ---- sheet 1: outline, holes, drill chart, stackup, notes
    k = cfg.get("pwb_scale", PWB_K)
    s = Sheet()
    a = frame(s, fields(cfg, "2", 1, total, _ratio(k)))
    r = ol["radius"]
    bw, bh = ol["w"] * k, ol["h"] * k
    ox = a["x0"] + 16.0                        # board top-left on the sheet
    oy = a["rev_y"] + 21.0

    def P(x, y):                               # board mm (y up) -> sheet mm
        return ox + (x - bx0) * k, oy + (by1 - y) * k
    s.text(ox - 10, a["y0"] + 8, "BOARD OUTLINE AND HOLES (TOP VIEW)", size=3.4, weight="bold")
    s.text(ox - 10, a["y0"] + 13, f"Scale {_ratio(k)} at {PAPER}. Datum: board corner at top left. All holes are drilled "
           f"from the top. {cfg.get('corner_note', f'Corners R{r:.2f} (4 places)' if r > 0 else 'Square corners')}. "
           f"Outline tolerance ±0.2.", size=2.3, c=GREY)
    if cfg.get("pwb_outline_layer"):            # draw the outline Gerber itself: internal cutouts included
        gko = glob.glob(os.path.join(gdir, "*.GKO"))[0]
        place_layer(s, gko, ol["bounds"], ox - 1.0 * k, oy - 1.0 * k, k, INK, bg="none")
    else:
        s.rect(ox, oy, bw, bh, sw=0.5, rx=r * k)
    tools = sorted({tool_of(h) for h in holes})
    sym = {t: SYMS[n % len(SYMS)] for n, t in enumerate(tools)}
    for hl in holes:
        x, y = P(hl["x"], hl["y"])
        if hl.get("slot"):
            x2, y2 = P(hl["x2"], hl["y2"])
            s.add(hole_svg(x, y, x2, y2, hl["dt"] * k, "white", INK, 0.2))
            x, y = (x + x2) / 2, (y + y2) / 2
        elif hl["d"] >= 2.0:
            s.circle(x, y, hl["d"] / 2 * k, 0.2)
        symbol(s, sym[tool_of(hl)], x, y, r=0.75 if hl["d"] < 0.5 else 1.2, sw=0.18)
    s.circle(ox, oy, 1.6, 0.25)
    s.line(ox - 3, oy, ox + 3, oy, 0.25)
    s.line(ox, oy - 3, ox, oy + 3, 0.25)
    s.text(ox - 2.4, oy - 3.0, "DATUM 0,0", size=2.2, anchor="end")
    dim_h(s, ox, ox + bw, oy - 6, oy - 7.2, f"{ol['w']:.2f}", ext_from=oy)
    dim_v(s, oy, oy + bh, ox - 7, f"{ol['h']:.2f}", ext_from=ox)
    for dm in cfg.get("pwb_dims", []):          # extra dimensions (board mm), e.g. an outline notch
        if "h" in dm:
            xa, _ = P(dm["h"][0], 0)
            xb, _ = P(dm["h"][1], 0)
            yl = oy - 6 - 7 * (dm.get("tier", 2) - 1)
            dim_h(s, xa, xb, yl, yl - 1.2, f"{abs(dm['h'][1] - dm['h'][0]):.2f}", ext_from=P(0, dm.get("ext_y", by1))[1])
        else:
            _, ya = P(0, dm["v"][0])
            _, yb_ = P(0, dm["v"][1])
            xl = ox + bw + 7 + 7 * (dm.get("tier", 1) - 1)
            dim_v(s, ya, yb_, xl, f"{abs(dm['v'][1] - dm['v'][0]):.2f}", ext_from=P(dm.get("ext_x", bx1), 0)[0])
    big = cfg["mount_hole_d"] if "mount_hole_d" in cfg else max(h["d"] for h in holes)
    mh = sorted([h for h in holes if h["d"] == big and (h["plated"] or "mount_hole_d" not in cfg)],
                key=lambda h: (-h["y"], h["x"]))
    xs_ = sorted({round(h["x"], 2) for h in mh})
    ys_ = sorted({round(h["y"], 2) for h in mh})
    grid = len(mh) == 4 and len(xs_) == 2 and len(ys_) == 2
    if mh and not grid:
        mc = cfg.get("mount_callout", {})
        hsel = mh[mc.get("index", 1) - 1]
        hx, hy = P(hsel["x"], hsel["y"])
        ddx, ddy = mc.get("dx", 14.5), mc.get("dy", 9.8)
        s.line(hx + 1.5, hy + 1.2, hx + ddx, hy + ddy, 0.15)
        hx, hy = hx + ddx - 16, hy + ddy - 11
        s.text(hx + 16.5, hy + 11.8, f"{len(mh)}× Ø{big:.2f} {'PTH' if mh[0]['plated'] else 'NPTH'}"
               + (f", {cfg['mount_hole_note']}" if cfg.get("mount_hole_note") else ""), size=2.6)
        s.text(hx + 16.5, hy + 15.2, "Positions in the mounting hole table", size=2.3, c=GREY)
    if mh and grid:
        mc = cfg.get("mount_callout", {})
        hsel = mh[mc.get("index", 1) - 1]
        hx, hy = P(hsel["x"], hsel["y"])
        dx, dy = mh[0]["x"] - bx0, by1 - mh[0]["y"]
        ddx, ddy = mc.get("dx", 16), mc.get("dy", 11)
        s.line(hx + 1.5, hy + 1.2, hx + ddx, hy + ddy, 0.15)
        hx, hy = hx + ddx - 16, hy + ddy - 11
        s.text(hx + 16.5, hy + 11.8, f"{len(mh)}× Ø{big:.2f} {'PTH' if mh[0]['plated'] else 'NPTH'}"
               + (f", {cfg['mount_hole_note']}" if cfg.get("mount_hole_note") else ""), size=2.6)
        dx2, dy2 = bx1 - mh[-1]["x"], mh[-1]["y"] - by0
        sides = (f"{dx:.2f} from the side edges" if abs(dx - dx2) < 0.01
                 else f"{dx:.2f} from the left edge, {dx2:.2f} from the right edge")
        ends = (f"{dy:.2f} from the top and bottom edges" if abs(dy - dy2) < 0.01
                else f"{dy:.2f} from the top edge, {dy2:.2f} from the bottom edge")
        s.text(hx + 16.5, hy + 15.2, f"{sides}, {ends}", size=2.3, c=GREY)
        xs = sorted({round(h["x"], 2) for h in mh})
        ys = sorted({round(h["y"], 2) for h in mh})
        if len(xs) > 1:
            ax_, _ = P(xs[0], 0)
            bx_, _ = P(xs[-1], 0)
            dim_h(s, ax_, bx_, oy + bh + 7, oy + bh + 11.2, f"{xs[-1] - xs[0]:.2f}", ext_from=oy + bh - 4)
        if len(ys) > 1:
            _, ay_ = P(0, ys[-1])
            _, by_ = P(0, ys[0])
            dim_v(s, ay_, by_, ox + bw + 7, f"{ys[-1] - ys[0]:.2f}", ext_from=ox + bw - 4)

    # drill chart and stackup: right of the board, or on their own sheet for a wide board
    if sep:
        ts = Sheet()
        a2 = frame(ts, fields(cfg, "2", 2, total, "NTS"))
        ts.text(a2["x0"] + 6, a2["y0"] + 8, "DRILL CHART AND LAYER STACKUP", size=3.4, weight="bold")
        ts.text(a2["x0"] + 6, a2["y0"] + 13, "Hole positions: sheet 1 and the drill files. Plane and pour use per layer: "
                "the copper layer sheets.", size=2.3, c=GREY)
        tx = a2["x0"] + 2
        tw = (a2["x1"] - a2["x0"] - 14) / 2
        ty = a2["rev_y"] + 9
        sx2 = tx + tw + 10
    else:
        ts = s
        tx = ox + bw + 16
        tw = a["x1"] - 2 - tx
        ty = a["rev_y"] + 9
        sx2 = tx
    ts.text(tx, ty - 2, "DRILL CHART", size=3.0, weight="bold")
    rows = []
    for t in tools:
        n = sum(1 for h in holes if tool_of(h) == t)
        vias = sum(1 for h in holes if tool_of(h) == t and h["via"])
        kind = "Via" if vias == n else ("Component / via" if vias else ("Mounting" if t[0] == big else "Component"))
        tkey = f"{t[0]:.2f} {'PTH' if t[1] else 'NPTH'}" + (" SLOT" if t[2] else "")
        kind = cfg.get("drill_use", {}).get(tkey, kind)
        rows.append(["", f"{t[0]:.2f}" + (f" × {t[2]:.2f} slot" if t[2] else ""), "PTH" if t[1] else "NPTH", str(n), kind])
    f = tw / 90
    cols = [("SYM", 9 * f, "middle"), ("DIA (mm)", 15 * f, "middle"), ("PLATING", 15 * f, "middle"),
            ("QTY", 11 * f, "middle"), ("USE", 40 * f, "start")]
    yb = ts.table(tx, ty, cols, rows, rh=5.0)
    for n, t in enumerate(tools):
        symbol(ts, sym[t], tx + 4.5 * f, ty + 5.2 + 5.0 * n + 2.5, r=1.1, sw=0.2)
    ts.text(tx, yb + 4, f"Total holes: {len(holes)}.  Finished hole sizes; tolerance ±0.08.", size=2.2, c=GREY)
    sy = (ty if sep else yb + 13)
    ts.text(sx2, sy - 2, "LAYER STACKUP", size=3.0, weight="bold")
    srows = [[l["layer"], l["name"], l["material"], l["thickness"], l.get("use", "")] for l in cfg["stackup"]]
    scols = [("No.", 8 * f, "middle"), ("NAME", 18 * f, "start"), ("MATERIAL", 21 * f, "start"),
             ("THK (mm)", 13 * f, "middle"), ("USE", 30 * f, "start")]
    yb2 = ts.table(sx2, sy, scols, srows, rh=4.6, size=2.2, wrap=wrap_text)
    note_lines = wrap_text(cfg.get("stackup_note", ""), tw, 2.2)
    for n, part in enumerate(note_lines):
        ts.text(sx2, yb2 + 4 + 3 * n, part, size=2.2, c=GREY)
    if mh and not grid:                        # holes not on a simple rectangle: list them
        my = yb2 + 4 + 3 * len(note_lines) + 9
        ts.text(sx2, my - 2, "MOUNTING HOLES", size=3.0, weight="bold")
        mrows = [[f"M{i}", f"{h['x'] - bx0:.2f}", f"{h['y'] - by1:.2f}", f"Ø{h['d']:.2f} {'PTH' if h['plated'] else 'NPTH'}"]
                 for i, h in enumerate(sorted(mh, key=lambda h: (-h["y"], h["x"])), 1)]
        ts.table(sx2, my, [("No.", 12 * f, "middle"), ("X", 24 * f, "end"), ("Y", 24 * f, "end"), ("HOLE", 30 * f, "start")],
                mrows, rh=4.6, size=2.2)
        for i, h in enumerate(sorted(mh, key=lambda h: (-h["y"], h["x"])), 1):
            hx, hy = P(h["x"], h["y"])
            s.text(hx + h["d"] / 2 * k + 1.0, hy - h["d"] / 2 * k - 0.6, f"M{i}", size=2.4, weight="bold")
    for c in cfg.get("pwb_callouts", []):     # leader + note at a board point (board mm, y up)
        cx, cy = P(*c["at"])
        ex, ey = cx + c.get("dx", 14), cy + c.get("dy", 10)
        s.line(cx, cy, ex, ey, 0.15)
        s.text(ex + 0.6, ey + 0.9, c["text"], size=2.6)
        if c.get("sub"):
            s.text(ex + 0.6, ey + 4.3, c["sub"], size=2.3, c=GREY)

    notes_box(s, a["x0"] + 2, a["tb_y"], a["tb_x"] - a["x0"] - 6, a["y1"] - a["tb_y"] - 2, cfg["fab_notes"])
    sheets.append(s)
    if sep:
        sheets.append(ts)

    # ---- then one copper layer per sheet
    k = cfg.get("layer_scale", LAYER_K)
    for n, lay in enumerate(layers, 2 + sep):
        s = Sheet()
        a = frame(s, fields(cfg, "2", n, total, _ratio(k)))
        pad = 1.0
        ww, wh = (ol["w"] + 2 * pad) * k, (ol["h"] + 2 * pad) * k
        lx = a["x0"] + (a["x1"] - a["x0"] - ww) / 2
        ly = a["rev_y"] + 2 + ((a["tb_y"] - 2) - (a["rev_y"] + 2) - wh) / 2
        s.text(a["x0"] + 6, a["y0"] + 8, f"COPPER LAYER {lay['layer']} OF {len(layers)}  ·  {lay['name'].upper()}  "
               f"(TOP VIEW, LOOKING DOWN THROUGH THE BOARD)", size=3.4, weight="bold")
        colour = lay.get("color", LAYER_COLOURS[(n - 2 - sep) % len(LAYER_COLOURS)])
        s.rect(a["x0"] + 6, a["y0"] + 10.6, 6, 3.2, sw=0.2, fill=colour)
        s.text(a["x0"] + 14, a["y0"] + 13, f"Scale {_ratio(k)} at {PAPER}. Layer {lay['layer']} copper is shown in "
               f"this colour; clearances, anti-pads and gaps are white. {lay.get('net', '')}.", size=2.3, c=GREY)
        bx, by_, _, _ = place_layer(s, os.path.join(gdir, lay["file"]), ol["bounds"], lx, ly, k, colour)
        if cfg.get("pwb_outline_layer"):
            place_layer(s, glob.glob(os.path.join(gdir, "*.GKO"))[0], ol["bounds"], lx, ly, k, INK, bg="none")
        else:
            s.rect(lx + (bx0 - bx) * k, ly + (by_ - by1) * k, ol["w"] * k, ol["h"] * k, sw=0.35, rx=r * k)
        # drilled holes on top of the copper: white = no material. Plated: solid outline; NPTH: dashed.
        for hl in holes:
            hx, hy = lx + (hl["x"] - bx) * k, ly + (by_ - hl["y"]) * k
            hx2, hy2 = (lx + (hl["x2"] - bx) * k, ly + (by_ - hl["y2"]) * k) if hl.get("slot") else (hx, hy)
            dash = "" if hl["plated"] else ' stroke-dasharray="0.6 0.4"'
            s.add(hole_svg(hx, hy, hx2, hy2, hl["dt"] * k, "white", INK, 0.1, dash))
        has_npth = any(not h["plated"] for h in holes)
        notes = [f"1. Layer {lay['layer']} of {len(layers)}, {lay['name']}, {lay.get('copper', '')} copper: "
                 f"{lay.get('net', '')}."]
        notes += [f"{i}. {t}" for i, t in enumerate(lay.get("notes", []), 2)]
        notes.append(f"{len(notes) + 1}. Drilled holes are drawn white over the copper, from the drill files: "
                     + ("solid outline = plated, dashed outline = non-plated." if has_npth
                        else f"all {len(holes)} holes are plated (solid outline)."))
        notes_box(s, a["x0"] + 2, a["tb_y"], a["tb_x"] - a["x0"] - 6, a["y1"] - a["tb_y"] - 2, notes,
                  title="LAYER NOTES")
        sheets.append(s)
    return sheets


def _parts_library(cfg):
    lib = json.load(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "parts.json"), encoding="utf-8"))
    lib.update(cfg.get("bom_parts", {}))      # board overrides, keyed by LCSC number or first designator
    return lib


def doc_bom(cfg, work):
    """Bill of materials. Descriptions, packages and values come from parts.json (keyed by LCSC
    number) where present, so each line states its ratings instead of a supplier category."""
    rows_in = read_utf16_tsv(cfg["bom_csv"])
    pnp = {r["Designator"].strip('"'): r for r in read_utf16_tsv(cfg["pnp_csv"])}
    lib = _parts_library(cfg)
    rows = []
    for k, r in enumerate(rows_in, 1):
        des = r["Designator"]
        first = des.split(",")[0]
        lcsc = r.get("Supplier Part", "")
        p = lib.get(lcsc) or lib.get(first) or {}
        smd = pnp.get(first, {}).get("SMD", "").strip('"')
        side = pnp.get(first, {}).get("Layer", "").strip('"')
        asm = ("SMT" if smd == "Yes" else "THT") + (f" ({'top' if side == 'T' else 'bottom'})" if side else "")
        later = set(cfg.get("after_delivery", []))
        by = "After delivery" if any(d.strip() in later for d in des.split(",")) else cfg.get("assembler", "JLCPCB")
        desc = p.get("desc") or r.get("Secondary Category") or r.get("Primary Category", "")
        mpn = p.get("mpn") or re.sub(r"[^\x00-\x7f]+", "", r.get("Manufacturer Part", "")).strip() or "-"
        mfr = p.get("mfr") or r.get("Manufacturer", "").split("(")[0] or "-"
        pkg = p.get("pkg") or r.get("Footprint", "").split("_")[0]
        val = re.sub(r"^([\d.]+)\s*([pnuµkM]?)(F|Ω|H)$", lambda m: f"{m.group(1)} {m.group(2).replace('u', 'µ')}{m.group(3)}",
                     r.get("Value", "")) or p.get("value", "") or "-"
        rows.append([str(k), r["Quantity"], des.replace(",", ", "), val, pkg, desc, mfr, mpn,
                     lcsc or p.get("lcsc", "-"), (r.get("JLCPCB Part Class", "") or p.get("class", "-")).replace(" Part", ""), asm, by])
    for e in cfg.get("bom_extra", []):          # board parts the EDA BOM leaves out (e.g. self-sourced)
        rows.append([str(len(rows) + 1), str(e["qty"]), e["designators"], e.get("value", "-"), e.get("pkg", "-"),
                     e["desc"], e.get("mfr", "-"), e.get("mpn", "-"), e.get("lcsc", "-"), e.get("class", "-"),
                     e.get("process", "-"), e.get("by", "After delivery")])
    base = [("ITEM", 10, "middle"), ("QTY", 9, "middle"), ("DESIGNATORS", 62, "start"), ("VALUE", 16, "start"),
            ("PACKAGE", 21, "start"), ("DESCRIPTION", 86, "start"), ("MANUFACTURER", 27, "start"),
            ("MFR PART No.", 44, "start"), ("LCSC", 18, "start"), ("CLASS", 16, "middle"), ("PROCESS", 22, "start"),
            ("INSTALLED BY", 24, "start")]
    size, rh, hh = 2.75, 6.6, 6.4
    probe = frame(Sheet(), fields(cfg, "3", 1, 1, "NTS"))
    x = probe["x0"] + 2
    tw = probe["x1"] - 2 - x
    f = tw / sum(c[1] for c in base)
    cols = [(t, w * f, al) for t, w, al in base]
    room = probe["tb_y"] - 2 - (probe["rev_y"] + 6) - hh

    def row_h(r):                               # as Sheet.table lays a wrapped row out
        lines = max(len(wrap_text(str(v), w, size)) for v, (_, w, _) in zip(r, cols))
        return rh + (lines - 1) * size * 1.15
    pages, cur, used = [], [], 0.0
    for r in rows:                              # as many sheets as the rows need
        h = row_h(r)
        if cur and used + h > room:
            pages.append(cur); cur, used = [], 0.0
        cur.append(r); used += h
    pages.append(cur)
    sheets = []
    for n, chunk in enumerate(pages, 1):
        s = Sheet()
        a = frame(s, fields(cfg, "3", n, len(pages), "NTS"))
        s.text(x, a["y0"] + 8, "BILL OF MATERIALS  ·  PRINTED CIRCUIT CARD ASSEMBLY"
               + (f"  ({n} of {len(pages)})" if len(pages) > 1 else ""), size=3.4, weight="bold")
        s.text(x, a["y0"] + 13, f"Board {cfg['base']}2 {cfg['board_version']}. Quantities per assembled card. "
               f"{len(rows)} line items, {sum(int(r[1]) for r in rows)} parts.", size=2.3, c=GREY)
        yb = s.table(x, a["rev_y"] + 6, cols, chunk, rh=rh, hh=hh, size=size, hsize=2.5, wrap=wrap_text)
        if yb > a["tb_y"] - 2:
            raise SystemExit(f"BOM table runs into the title block ({yb:.1f} > {a['tb_y'] - 2:.1f})")
        notes_box(s, a["x0"] + 2, a["tb_y"], a["tb_x"] - a["x0"] - 6, a["y1"] - a["tb_y"] - 2, cfg["bom_notes"])
        sheets.append(s)
    return sheets


PLACE_K = 1.75          # placement views scale (7:4), one board side per sheet
REFDES = "#1f4e9a"      # reference designator colour
PIN1 = "#c0392b"        # pin 1 marker colour


def _mm(v):
    return float(v.strip('"').replace("mm", ""))


def _pnp_rows(cfg):
    rows = []
    for p in read_utf16_tsv(cfg["pnp_csv"]):
        q = {k: v.strip('"') for k, v in p.items()}
        q["x"], q["y"] = _mm(q["Mid X"]), _mm(q["Mid Y"])
        q["px"], q["py"] = _mm(q["Pad X"]), _mm(q["Pad Y"])
        q["rot"] = float(q["Rotation"] or 0)
        q["pins"] = int(q.get("Pins") or 0)
        rows.append(q)
    return rows


def _des_key(d):
    m = re.match(r"([A-Z]+)(\d+)", d)
    return (m.group(1), int(m.group(2))) if m else (d, 0)


def doc_place(cfg, work):
    """Placement drawing: one sheet per board side that carries parts, at PLACE_K, then a
    placement table. Views show the solder mask openings (pads) and silkscreen, the drilled
    holes, reference designators along each part's axis, and a pin 1 marker on every part
    with more than two pins and on polarised two-pin parts (config place_polarised)."""
    gdir = cfg["gerber_dir"]
    ol = outline_info(gdir)
    (bx0, by0), (bx1, by1) = ol["bounds"]
    holes = drills(gdir)
    r = ol["radius"]
    k = cfg.get("place_scale", PLACE_K)
    pnp = _pnp_rows(cfg)
    polar = set(cfg.get("place_polarised", []))
    sides = [("B", "BOTTOM SIDE", "viewed from the bottom (board flipped left to right)", True),
             ("T", "TOP SIDE", "viewed from the top", False)]
    sides = [sd for sd in sides if any(p["Layer"] == sd[0] for p in pnp)]
    probe = frame(Sheet(), fields(cfg, "4", 1, 1, "NTS"))        # frame geometry for the table sizing
    cap = int((probe["tb_y"] - 2 - (probe["rev_y"] + 6) - 6.4) / 6.6)     # rows per table column
    n_tab = max(1, math.ceil(len(pnp) / (2 * cap)))
    segs = cfg.get("place_segments") or [None]      # a wide board can be shown in parts, [x0, x1] in board mm
    ysplit = cfg.get("place_split_axis") == "y"     # or a tall one, [y0, y1] in mm down from the top edge
    views = [(sd, i, sg) for sd in sides for i, sg in enumerate(segs)]
    total = len(views) + n_tab
    sheets = []
    for n, ((side, name, how, mirror), si, sg) in enumerate(views, 1):
        s = Sheet()
        a = frame(s, fields(cfg, "4", n, total, _ratio(k)))
        pad = 1.0
        if sg and ysplit:
            wb = ((bx0, max(by1 - sg[1], by0)), (bx1, min(by1 - sg[0], by1)))
        else:
            wb = ((max(sg[0], bx0), by0), (min(sg[1], bx1), by1)) if sg else ol["bounds"]
        ww, wh = (wb[1][0] - wb[0][0] + 2 * pad) * k, (wb[1][1] - wb[0][1] + 2 * pad) * k
        vx = a["x0"] + (a["x1"] - a["x0"] - ww) / 2
        vy = a["rev_y"] + 2 + ((a["tb_y"] - 2) - (a["rev_y"] + 2) - wh) / 2
        on_side = [p for p in pnp if p["Layer"] == side]
        n_side = len(on_side)
        if sg and ysplit:
            on_side = [p for p in on_side if wb[0][1] - 0.5 <= p["y"] <= wb[1][1] + 0.5]
            how += f" · part {si + 1} of {len(segs)}, Y {by1 - wb[1][1]:.0f} to {by1 - wb[0][1]:.0f} mm down"
        elif sg:
            on_side = [p for p in on_side if wb[0][0] - 0.5 <= p["x"] <= wb[1][0] + 0.5]
            how += f" · part {si + 1} of {len(segs)}, X {wb[0][0] - bx0:.0f} to {wb[1][0] - bx0:.0f} mm"
        s.text(a["x0"] + 6, a["y0"] + 8, f"{name}  ·  {how.upper()}", size=3.4, weight="bold")
        s.text(a["x0"] + 6, a["y0"] + 13, f"Scale {_ratio(k)} at {PAPER}. {n_side} part"
               f"{'s' if n_side != 1 else ''} on this side" + (f", {len(on_side)} in this part" if sg else "") + ". Pads (solder mask openings) grey, silkscreen black, "
               f"reference designators blue, pin 1 red.", size=2.3, c=GREY)
        mask = glob.glob(os.path.join(gdir, "*.GTS" if side == "T" else "*.GBS"))[0]
        silk = glob.glob(os.path.join(gdir, "*.GTO" if side == "T" else "*.GBO"))[0]
        bx, by_, _, _ = place_layer(s, mask, wb, vx, vy, k, "#bec4cc", mirror=mirror)
        place_layer(s, silk, wb, vx, vy, k, "#141414", bg="white", mirror=mirror)

        def P(x, y):
            u = (x - bx) * k
            return (vx + (ww - u) if mirror else vx + u), vy + (by_ - y) * k
        ox, oy = P(bx0, by1)
        if cfg.get("pwb_outline_layer") or sg:
            place_layer(s, glob.glob(os.path.join(gdir, "*.GKO"))[0], wb, vx, vy, k, INK, bg="none",
                        mirror=mirror)
        else:
            s.rect(min(ox, ox + (-1 if mirror else 1) * ol["w"] * k), oy, ol["w"] * k, ol["h"] * k, sw=0.4, rx=r * k)
        for hl in holes:
            if sg and not (wb[0][0] - pad <= hl["x"] <= wb[1][0] + pad and wb[0][1] - pad <= hl["y"] <= wb[1][1] + pad):
                continue
            hx, hy = P(hl["x"], hl["y"])
            hx2, hy2 = P(hl["x2"], hl["y2"]) if hl.get("slot") else (hx, hy)
            s.add(hole_svg(hx, hy, hx2, hy2, hl["dt"] * k, "#fff", "#555", 0.1))
        size = cfg.get("place_label_size", 1.6)
        for p in on_side:
            x, y = P(p["x"], p["y"])
            d = p["Designator"]
            if p["pins"] > 2 or d in polar:
                qx, qy = P(p["px"], p["py"])
                s.circle(qx, qy, 0.55 * size / 1.6, 0.15, fill=PIN1, c="white")
            vert = round(p["rot"]) % 180 == 90 and p["pins"] <= 2
            if vert:
                s.text(x + size * 0.36, y, d, size=size, anchor="middle", weight="bold", c=REFDES, rot=-90)
            else:
                s.text(x, y + size * 0.36, d, size=size, anchor="middle", weight="bold", c=REFDES)
        # datum: board top-left corner in the top view
        if not sg or (wb[1][1] >= by1 - 1e-6 if ysplit else wb[0][0] <= bx0 + 1e-6):
            dx, dy = P(bx0, by1)
            s.circle(dx, dy, 1.4, 0.25)
            s.line(dx - 2.6, dy, dx + 2.6, dy, 0.25)
            s.line(dx, dy - 2.6, dx, dy + 2.6, 0.25)
            s.text(dx + (2.8 if mirror else -2.8), dy - 2.8, "DATUM 0,0", size=2.2, anchor="start" if mirror else "end")
        notes_box(s, a["x0"] + 2, a["tb_y"], a["tb_x"] - a["x0"] - 6, a["y1"] - a["tb_y"] - 2, cfg["place_notes"])
        sheets.append(s)

    # placement table: two columns per sheet, as many sheets as needed
    bom = {}
    lib = _parts_library(cfg)
    for b in read_utf16_tsv(cfg["bom_csv"]):
        for d in b["Designator"].split(","):
            bom[d.strip()] = b
    rows = []
    for p in sorted(pnp, key=lambda q: _des_key(q["Designator"])):
        d = p["Designator"]
        b = bom.get(d, {})
        lp = lib.get(b.get("Supplier Part", "")) or lib.get(d) or {}
        mpn = lp.get("mpn") or re.sub(r"[^\x00-\x7f]+", "", b.get("Manufacturer Part", "") or p["Device"]).strip()
        pkg = lp.get("pkg") or p["Footprint"].split("_")[0]
        rows.append([d, mpn, pkg, "Top" if p["Layer"] == "T" else "Bottom", f"{p['x']:.3f}", f"{p['y']:.3f}",
                     f"{p['rot']:g}", "SMT" if p["SMD"] == "Yes" else "THT"])
    per = (len(rows) + 1) // 2 if n_tab == 1 else cap
    chunks = [rows[i:i + per] for i in range(0, len(rows), per)]
    base = [("REF", 13, "start"), ("PART", 44, "start"), ("PKG", 24, "start"), ("SIDE", 16, "middle"),
            ("X", 20, "end"), ("Y", 20, "end"), ("ROT", 13, "end"), ("PROCESS", 18, "middle")]
    for t in range(n_tab):
        s = Sheet()
        a = frame(s, fields(cfg, "4", len(views) + 1 + t, total, "NTS"))
        s.text(a["x0"] + 6, a["y0"] + 8, "PLACEMENT TABLE" + (f"  ({t + 1} of {n_tab})" if n_tab > 1 else ""),
               size=3.4, weight="bold")
        s.text(a["x0"] + 6, a["y0"] + 13, "X, Y: part centre from the datum (board top-left corner, top view), mm; "
               "Y is negative into the board.", size=2.3, c=GREY)
        s.text(a["x0"] + 6, a["y0"] + 17, f"Rotation in degrees as in the EasyEDA Pro pick-and-place file of the same "
               f"revision. Part numbers and packages from {cfg['base']}3 BOM.", size=2.3, c=GREY)
        gap = 8
        tw = (a["x1"] - a["x0"] - 4 - gap) / 2
        f = tw / sum(c[1] for c in base)
        cols = [(tt, w * f, al) for tt, w, al in base]
        top = a["rev_y"] + 6
        for c, chunk in enumerate(chunks[2 * t:2 * t + 2]):
            yb = s.table(a["x0"] + 2 + c * (tw + gap), top, cols, chunk, rh=6.6, hh=6.4, size=2.75, hsize=2.5)
            if yb > a["tb_y"] - 2:
                raise SystemExit("placement table runs into the title block")
        sheets.append(s)
    return sheets


SCH_K = 0.50            # mm per schematic pt (config "sch_scale"): one scale on every sheet
SCH_BAND = 5.0          # mm label band at the bottom of each block
SCH_GAP = 2.0           # mm clearance to the frame, revision block and title block
SCH_DASH = (2.5, 1.5)   # separator dash pattern, mm
SCH_LABEL = "#a10000"   # label colour (EasyEDA symbol red)


def _sch_page_body(cfg, page, work, cache={}):
    """EasyEDA page as SVG, with EasyEDA's heavy black block borders removed."""
    if page in cache:
        return cache[page]
    svgp = os.path.join(work, f"sch_p{page}.svg")
    subprocess.run(["pdftocairo", "-svg", "-f", str(page), "-l", str(page), cfg["sch_pdf"], svgp],
                   check=True, stderr=subprocess.DEVNULL)
    svg = open(svgp, encoding="utf-8").read()
    body = svg[svg.index(">", svg.index("<svg")) + 1: svg.rindex("</svg>")]
    body = re.sub(r'<path(?=[^>]*stroke="rgb\(0%, 0%, 0%\)")(?=[^>]*stroke-width="1\.44")[^>]*/>', "", body)
    pre = f"p{page}-"
    body = re.sub(r'id="([^"]+)"', lambda m: f'id="{pre}{m.group(1)}"', body)
    body = re.sub(r'(xlink:href|href)="#([^"]+)"', lambda m: f'{m.group(1)}="#{pre}{m.group(2)}"', body)
    body = re.sub(r"url\(#([^)]+)\)", lambda m: f"url(#{pre}{m.group(1)})", body)
    cache[page] = body
    return body


def _dashed_segments(s, boxes):
    """Draw the edges of all boxes once each: shared edges are merged, and the dash
    pattern is phased to absolute position so collinear runs line up."""
    runs = {}
    for x0, y0, x1, y1 in boxes:
        for key, a, b in ((("h", round(y0, 2)), x0, x1), (("h", round(y1, 2)), x0, x1),
                          (("v", round(x0, 2)), y0, y1), (("v", round(x1, 2)), y0, y1)):
            runs.setdefault(key, []).append((a, b))
    period = sum(SCH_DASH)
    for (axis, c), segs in runs.items():
        segs.sort()
        merged = [list(segs[0])]
        for a, b in segs[1:]:
            if a <= merged[-1][1] + 1e-6:
                merged[-1][1] = max(merged[-1][1], b)
            else:
                merged.append([a, b])
        for a, b in merged:
            x1, y1, x2, y2 = (a, c, b, c) if axis == "h" else (c, a, c, b)
            s.add(f'<line x1="{x1:.3f}" y1="{y1:.3f}" x2="{x2:.3f}" y2="{y2:.3f}" stroke="#7a7a7a" '
                  f'stroke-width="0.25" stroke-dasharray="{SCH_DASH[0]} {SCH_DASH[1]}" '
                  f'stroke-dashoffset="{a % period:.3f}"/>')


def doc_sch(cfg, work):
    """Schematic sheets built from blocks cut out of the EasyEDA pages.

    Each sheet is {"rows": [row, ...]}; a row is a list of columns and a column is a list of
    blocks stacked top to bottom. Block: {page, crop:[x0,y0,x1,y1] in page pt (top-down), label}.
    Every block is drawn at one scale (sch_scale, mm per pt) on every sheet. Rows are stretched
    to the full drawing width, columns to the row height, and each circuit is centred in its
    box with its label at the bottom left. The block group is centred vertically between the
    revision block and the title block."""
    base_k = cfg.get("sch_scale", SCH_K)
    sheets = []
    total = len(cfg["sch_sheets"])
    mixed = len({sp.get("scale", base_k) for sp in cfg["sch_sheets"]}) > 1
    for n, spec in enumerate(cfg["sch_sheets"], 1):
        k = spec.get("scale", base_k)          # a sheet may have its own scale; it is then stated on it
        s = Sheet()
        a = frame(s, fields(cfg, "1", n, total, "NTS"))
        if mixed:
            s.text(a["x0"] + 6, a["y0"] + 8, f"Drawn at {k / (25.4 / 72) * 100:.0f} % of the EasyEDA schematic "
                   f"({k:.2f} mm per point). Symbol sizes differ between sheets of this drawing.", size=2.6, c=GREY)
        ax0, ax1 = a["x0"] + SCH_GAP, a["x1"] - SCH_GAP
        ay0, ay1 = a["rev_y"] + SCH_GAP, a["tb_y"] - SCH_GAP

        def nat(b):
            x0, y0, x1, y1 = b["crop"]
            return (x1 - x0) * k, (y1 - y0) * k + SCH_BAND
        rows = []
        for row in spec["rows"]:
            cols = [(max(nat(b)[0] for b in col), sum(nat(b)[1] for b in col)) for col in row]
            rows.append((row, cols, max(h for _, h in cols)))
        total_h = sum(r[2] for r in rows)
        if total_h > ay1 - ay0 + 1e-6:
            raise SystemExit(f"SCH sheet {n}: blocks need {total_h:.1f} mm of height, "
                             f"{ay1 - ay0:.1f} mm available; lower sch_scale")
        y = ay0 + ((ay1 - ay0) - total_h) / 2
        boxes = []
        pages = sorted({b["page"] for row in spec["rows"] for col in row for b in col})
        defs = "".join(f'<g id="sch-page-{pg}">{_sch_page_body(cfg, pg, work)}</g>' for pg in pages)
        s.add(f"<defs>{defs}</defs>")
        for row, cols, rh in rows:
            nat_w = sum(w for w, _ in cols)
            if nat_w > ax1 - ax0 + 1e-6:
                raise SystemExit(f"SCH sheet {n}: a row needs {nat_w:.1f} mm of width, "
                                 f"{ax1 - ax0:.1f} mm available; lower sch_scale")
            x = ax0
            for col, (cw, ch) in zip(row, cols):
                cw *= (ax1 - ax0) / nat_w            # stretch columns to the full width
                by = y
                for b in col:
                    bw, bh = nat(b)
                    bh *= rh / ch                    # stretch blocks to the row height
                    boxes.append((x, by, x + cw, by + bh))
                    x0, y0, x1, y1 = b["crop"]
                    w, h = (x1 - x0) * k, (y1 - y0) * k
                    cx, cy = x + (cw - w) / 2, by + (bh - SCH_BAND - h) / 2
                    s.add(f'<svg x="{cx:.3f}" y="{cy:.3f}" width="{w:.3f}" height="{h:.3f}" '
                          f'viewBox="{x0} {y0} {x1 - x0} {y1 - y0}" overflow="hidden">'
                          f'<use href="#sch-page-{b["page"]}" xlink:href="#sch-page-{b["page"]}"/></svg>')
                    s.text(x + 2.0, by + bh - 1.7, b["label"], size=3.0, c=SCH_LABEL)
                    by += bh
                x += cw
            y += rh
        _dashed_segments(s, boxes)
        sheets.append(s)
    return sheets


PT = 72 / 25.4           # pt per mm


def _pdf_page_body(pdf, page, work, tag, lw=1.0, keep=None):
    """One PDF page as an SVG fragment with unique ids. lw scales every stroke width (Fusion
    exports its visible lines at 0.5 mm, too heavy for engraved text at 2:5); keep, a regex,
    keeps only the <path> elements whose attributes match it (e.g. the cut layer of a
    production file) and drops everything else drawn on the page."""
    svgp = os.path.join(work, f"{tag}_p{page}.svg")
    subprocess.run(["pdftocairo", "-svg", "-f", str(page), "-l", str(page), pdf, svgp],
                   check=True, stderr=subprocess.DEVNULL)
    svg = open(svgp, encoding="utf-8").read()
    body = svg[svg.index(">", svg.index("<svg")) + 1: svg.rindex("</svg>")]
    if keep:
        body = "\n".join(m.group(0) for m in re.finditer(r"<path [^>]*/>", body) if re.search(keep, m.group(0)))
    if lw != 1.0:
        body = re.sub(r'stroke-width="([0-9.]+)"', lambda m: f'stroke-width="{float(m.group(1)) * lw:.4g}"', body)
    pre = f"{tag}{page}-"
    body = re.sub(r'id="([^"]+)"', lambda m: f'id="{pre}{m.group(1)}"', body)
    body = re.sub(r'(xlink:href|href)="#([^"]+)"', lambda m: f'{m.group(1)}="#{pre}{m.group(2)}"', body)
    body = re.sub(r"url\(#([^)]+)\)", lambda m: f"url(#{pre}{m.group(1)})", body)
    return body


def doc_views(cfg, work, d):
    """A drawing made of Fusion views: the package's `view_docs[d]` entry ({"pdf", "sheets",
    "parts", "notes"}), or for the assembly drawing "0" the assy_* keys. Used for the CCA
    assembly drawing and for mechanical assembly and part drawings."""
    vd = (cfg.get("view_docs") or {}).get(d)
    if vd is None:
        vd = {"pdf": cfg["assy_pdf"], "sheets": cfg["assy_sheets"], "parts": cfg.get("assy_parts", []),
              "notes": cfg.get("assy_notes", [])}
    return doc_assy(cfg, work, d, vd)


def doc_assy(cfg, work, d="0", vd=None):
    """CCA assembly drawing from a Fusion drawing PDF. Each view is a crop of a Fusion page
    (src: [x0, y0, x1, y1] in mm on that page, top-down) placed at sheet position at:[x, y]
    (top-left, mm) and drawn at the sheet's scale k (Fusion views are drawn at 1:1) or at its
    own fit width w for not-to-scale views. Sheets: {"scale": k or null, "views": [...],
    "parts_list": bool, "notes": bool}."""
    vd = vd or {"pdf": cfg["assy_pdf"], "sheets": cfg["assy_sheets"], "parts": cfg.get("assy_parts", []),
                "notes": cfg.get("assy_notes", [])}
    pdf = vd["pdf"]
    sheets_cfg = vd["sheets"]
    total = len(sheets_cfg)
    sheets = []
    for n, sc in enumerate(sheets_cfg, 1):
        k = sc.get("scale")
        s = Sheet()
        a = frame(s, fields(cfg, d, n, total, _ratio(k) if k else "NTS"))
        # each view may come from its own file (e.g. a cut file) and page
        def src_key(v):
            return (v.get("pdf", pdf), v["page"], v.get("line_scale", vd.get("line_scale", 1.0)), v.get("keep_paths"))
        srcs = sorted({src_key(v) for v in sc["views"]}, key=str)
        sid = {src: f"assy-{d}-{n}-src{i}" for i, src in enumerate(srcs)}
        s.add("<defs>" + "".join(f'<g id="{sid[src]}">{_pdf_page_body(src[0], src[1], work, f"assy{d}s{n}x{i}", src[2], src[3])}</g>'
                                 for i, src in enumerate(srcs)) + "</defs>")
        if sc.get("parts_list"):
            # true: full width under the revision block; or {"at": [x, y], "w": mm, "size": pt}
            pl = sc["parts_list"] if isinstance(sc["parts_list"], dict) else {}
            rows = [[str(i), str(p["qty"]), p["pn"], p["name"], p["desc"]] for i, p in enumerate(vd["parts"], 1)]
            x, ty = pl.get("at", [a["x0"] + 2, a["rev_y"] + 6])
            tw = pl.get("w", a["x1"] - 2 - x)
            base = pl.get("cols", [11, 10, 42, 62, 155])
            base = [("ITEM", base[0], "middle"), ("QTY", base[1], "middle"), ("PART NUMBER", base[2], "start"),
                    ("PART NAME", base[3], "start"), ("DESCRIPTION", base[4], "start")]
            f = tw / sum(c[1] for c in base)
            fs = pl.get("size", 2.75)
            s.text(x, ty - 2, "PARTS LIST", size=3.0, weight="bold")
            yb = s.table(x, ty, [(t, w * f, al) for t, w, al in base], rows, rh=fs * 2.4, hh=6.4,
                         size=fs, hsize=2.5, wrap=wrap_text)
            sc.setdefault("_top", yb)
        for tb in sc.get("tables", []):       # extra tables: {"at", "title", "cols": [[title, w, align]], "rows", "size"}
            tx, ty = tb["at"]
            fs = tb.get("size", 2.3)
            if tb.get("title"):
                s.text(tx, ty - 2, tb["title"], size=3.0, weight="bold")
            s.table(tx, ty, [tuple(c) for c in tb["cols"]], tb["rows"], rh=fs * 1.9, hh=5.2, size=fs, hsize=2.2)
        for v in sc["views"]:
            x0, y0, x1, y1 = v["src"]
            vk = v.get("k", k)
            w, h = (x1 - x0) * vk, (y1 - y0) * vk
            X, Y = v["at"]
            ref = sid[src_key(v)]
            use = f'<use href="#{ref}" xlink:href="#{ref}"/>'
            if v.get("clip"):          # polygon on the source page (mm) to leave out neighbouring views
                cid = f"assy-clip-{n}-{len(s.o)}"
                pts = " ".join(f"{px * PT:.2f},{py * PT:.2f}" for px, py in v["clip"])
                use = f'<clipPath id="{cid}"><polygon points="{pts}"/></clipPath><g clip-path="url(#{cid})">{use}</g>'
            s.add(f'<svg x="{X:.3f}" y="{Y:.3f}" width="{w:.3f}" height="{h:.3f}" '
                  f'viewBox="{x0 * PT:.3f} {y0 * PT:.3f} {(x1 - x0) * PT:.3f} {(y1 - y0) * PT:.3f}" overflow="hidden">'
                  f'{use}</svg>')
            def to_sheet(px, py):
                return X + (px - x0) * vk, Y + (py - y0) * vk
            for poly in v.get("masks", []):    # white-out stray parts (polygon or [x0, y0, x1, y1], source mm)
                if len(poly) == 4 and not isinstance(poly[0], list):
                    a0, b0, a1, b1 = poly
                    poly = [[a0, b0], [a1, b0], [a1, b1], [a0, b1]]
                pts = " ".join(f"{qx:.3f},{qy:.3f}" for qx, qy in (to_sheet(px, py) for px, py in poly))
                s.add(f'<polygon points="{pts}" fill="#ffffff" stroke="none"/>')
            for rl in v.get("relabel", []):     # re-number a balloon: {"at": [x, y], "r": mm, "text", "size"}
                cx, cy = to_sheet(*rl["at"])
                s.add(f'<circle cx="{cx:.3f}" cy="{cy:.3f}" r="{rl.get("r", 3.0) * vk:.3f}" fill="#ffffff" stroke="none"/>')
                fs = rl.get("size", 3.6) * vk
                s.text(cx, cy + fs * 0.36, rl["text"], size=fs, anchor="middle")
            for tg in v.get("tags", []):        # hole tags: {"at": [x, y] source mm, "r": hole radius mm, "text"}
                cx, cy = to_sheet(*tg["at"])
                o = tg.get("r", 1.5) * vk + 0.5
                s.add(f'<text x="{cx + o:.3f}" y="{cy - o:.3f}" font-size="{tg.get("size", 1.9)}" fill="{INK}" '
                      f'stroke="#ffffff" stroke-width="0.6" paint-order="stroke" font-weight="bold">{esc(tg["text"])}</text>')
            if v.get("datum"):                  # X/Y datum arrows at a source point: {"at": [x, y], "len": mm}
                cx, cy = to_sheet(*v["datum"]["at"])
                ln = v["datum"].get("len", 10)
                for x2, y2, t, tx, ty in ((cx + ln, cy, "X", cx + ln + 1.2, cy + 1.0), (cx, cy - ln, "Y", cx - 1.0, cy - ln - 1.2)):
                    s.line(cx, cy, x2, y2, 0.3, "#c0392b")
                    ang = 0 if y2 == cy else -90
                    s.add(f'<polygon points="0,0 -1.8,-0.6 -1.8,0.6" fill="#c0392b" transform="translate({x2:.3f} {y2:.3f}) rotate({ang})"/>')
                    s.text(tx, ty, t, size=2.4, weight="bold", c="#c0392b")
                s.circle(cx, cy, 0.7, sw=0.3, c="#c0392b")
                s.text(cx - 1.4, cy + 3.2, "0,0", size=2.0, anchor="end", c="#c0392b")
            if v.get("label"):
                s.text(X + w / 2, Y + h + 5, v["label"], size=2.8, anchor="middle", weight="bold")
                if v.get("sublabel"):
                    s.text(X + w / 2, Y + h + 8.6, v["sublabel"], size=2.3, anchor="middle", c=GREY)
        if sc.get("notes"):
            notes_box(s, a["x0"] + 2, a["tb_y"], a["tb_x"] - a["x0"] - 6, a["y1"] - a["tb_y"] - 2, vd["notes"])
        sheets.append(s)
    return sheets


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("config")
    ap.add_argument("--out", required=True)
    ap.add_argument("--docs", default=None, help="e.g. 1,2 (default: the package's documents)")
    a = ap.parse_args()
    cdir = os.path.dirname(os.path.abspath(a.config))
    cfg = json.load(open(a.config, encoding="utf-8"))
    for k in ("sch_pdf", "assy_pdf", "gerber_dir", "gerber_zip", "bom_csv", "pnp_csv"):
        if k in cfg and not os.path.isabs(cfg[k]):
            cfg[k] = os.path.normpath(os.path.join(cdir, cfg[k]))
    for vd in (cfg.get("view_docs") or {}).values():
        if not os.path.isabs(vd["pdf"]):
            vd["pdf"] = os.path.normpath(os.path.join(cdir, vd["pdf"]))
        for sh in vd["sheets"]:
            for v in sh["views"]:
                if "pdf" in v and not os.path.isabs(v["pdf"]):
                    v["pdf"] = os.path.normpath(os.path.join(cdir, v["pdf"]))
    os.makedirs(a.out, exist_ok=True)
    if "gerber_zip" in cfg and "gerber_dir" not in cfg:   # the fab zip as exported: unpack it for this run
        import zipfile
        gdir = tempfile.mkdtemp(prefix="kc_gerber_", dir=a.out)
        with zipfile.ZipFile(cfg["gerber_zip"]) as z:
            for m in z.namelist():
                if "/" not in m and ".." not in m:
                    z.extract(m, gdir)
        cfg["gerber_dir"] = gdir
    builders = {"0": lambda c, w: doc_views(c, w, "0"), "1": doc_sch, "2": doc_pwb, "3": doc_bom, "4": doc_place}
    for vd_key in (cfg.get("view_docs") or {}):          # documents drawn from Fusion views
        builders[vd_key] = (lambda key: lambda c, w: doc_views(c, w, key))(vd_key)
    docs = a.docs.split(",") if a.docs else list(cfg.get("package_docs") or "01234")
    for d in docs:
        work = tempfile.mkdtemp(prefix=f"kc_{d}_", dir=a.out)
        sheets = builders[d](cfg, work)
        f = {"dwg_no": cfg["base"] + d, "rev": cfg.get("rev", "-"), "doc_types": cfg.get("doc_types")}
        out = os.path.join(a.out, file_name(f, cfg.get("doc_name", {}).get(d, cfg["name"])))
        print_pdf(sheets, out, work)
        print("wrote", out, f"({len(sheets)} sheet{'s' if len(sheets) > 1 else ''})")


if __name__ == "__main__":
    main()
