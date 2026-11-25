#!/usr/bin/env python3
"""
kc_holes.py -- build the hole table of a panel part drawing from its production (cut) file.

The production file (Illustrator .ai, read as PDF) is the master for the panel's holes. This
finds every round hole on the cut layer (red stroke) up to a size limit, measures it from the
datum (lower-left corner of the panel outline, X right, Y up), tags the holes by size (A, B,
... from the smallest; numbered in reading order, top-left first) and writes into the config:

  - the view marked "hole_view": {...}: its "tags" (one per hole) and "datum";
  - that sheet's "tables": the hole table, split into columns, and a size summary.

    python3 kc_holes.py CONFIG [--doc 1] [--write]

The view's "hole_view" settings: {"max_dia": 8, "at": [x, y] (sheet mm, top-left of the
tables), "split": 2 (hole table columns), "gap": 6}. The outline is the largest closed path on
the page; Illustrator's PDF layer carries a uniform scale (about 0.9995) on every path, which
is divided out so positions are in the design's mm.
"""
import argparse
import json
import os
import re
import subprocess
import tempfile
from collections import OrderedDict

MM = 25.4 / 72


def paths(pdf, page):
    svgp = os.path.join(tempfile.mkdtemp(prefix="kc_holes_"), "p.svg")
    subprocess.run(["pdftocairo", "-svg", "-f", str(page), "-l", str(page), pdf, svgp], check=True)
    out = []
    for m in re.finditer(r"<path ([^>]*)/>", open(svgp, encoding="utf-8").read()):
        a = m.group(1)
        d = re.search(r' d="([^"]*)"', a).group(1)
        t = re.search(r"matrix\(([^)]*)\)", a)
        A = [float(x) for x in t.group(1).split(",")] if t else [1, 0, 0, 1, 0, 0]
        nums = [float(x) for x in re.findall(r"-?[\d.]+(?:e-?\d+)?", d)]
        pts = [(A[0] * x + A[2] * y + A[4], A[1] * x + A[3] * y + A[5]) for x, y in zip(nums[::2], nums[1::2])]
        xs, ys = [p[0] * MM for p in pts], [p[1] * MM for p in pts]
        st = re.search(r'stroke="rgb\(([\d.]+)%, ([\d.]+)%, ([\d.]+)%\)"', a)
        red = bool(st) and float(st.group(1)) > 80 and float(st.group(2)) < 30 and float(st.group(3)) < 30
        out.append(dict(box=(min(xs), min(ys), max(xs), max(ys)), curves=d.count("C"), lines=d.count("L"),
                        red=red, scale=abs(A[0])))
    return out


def holes(pdf, page, max_dia):
    ps = paths(pdf, page)
    outline = max(ps, key=lambda p: (p["box"][2] - p["box"][0]) * (p["box"][3] - p["box"][1]))
    ox0, oy0, ox1, oy1 = outline["box"]
    f = outline["scale"] or 1.0
    found = []
    for p in ps:
        x0, y0, x1, y1 = p["box"]
        w, h = x1 - x0, y1 - y0
        if p["red"] and p["curves"] == 4 and p["lines"] == 0 and abs(w - h) < 0.05 and w <= max_dia:
            cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
            found.append(dict(dia=round(w / f, 1), x=round((cx - ox0) / f, 2), y=round((oy1 - cy) / f, 2),
                              src=[round(cx, 3), round(cy, 3)], r=w / 2))
    size = (round((ox1 - ox0) / f, 2), round((oy1 - oy0) / f, 2))
    return found, [round(ox0, 3), round(oy1, 3)], size


def fmt(v):
    return f"{v:.2f}".rstrip("0").rstrip(".")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("config")
    ap.add_argument("--doc", default="1")
    ap.add_argument("--write", action="store_true")
    a = ap.parse_args()
    cdir = os.path.dirname(os.path.abspath(a.config))
    cfg = json.load(open(a.config, encoding="utf-8"), object_pairs_hook=OrderedDict)
    vd = cfg["view_docs"][a.doc]
    for sh in vd["sheets"]:
        for v in sh["views"]:
            hv = v.get("hole_view")
            if not hv:
                continue
            found, datum, size = holes(os.path.join(cdir, v.get("pdf", vd["pdf"])), v["page"], hv.get("max_dia", 8))
            dias = sorted({h["dia"] for h in found})
            letter = {d: chr(ord("A") + i) for i, d in enumerate(dias)}
            rows, tags, summary = [], [], []
            for d in dias:
                grp = sorted((h for h in found if h["dia"] == d), key=lambda h: (-round(h["y"]), h["x"]))
                summary.append([letter[d], f"Ø{fmt(d)} THRU", str(len(grp))])
                for i, h in enumerate(grp, 1):
                    tag = f"{letter[d]}{i}"
                    rows.append([tag, fmt(h["x"]), fmt(h["y"]), f"Ø{fmt(d)}"])
                    tags.append({"at": h["src"], "r": round(h["r"], 2), "text": tag})
            print(f"outline {size[0]} x {size[1]} mm, {len(found)} holes: "
                  + ", ".join(f"{letter[d]} Ø{fmt(d)} x{sum(1 for h in found if h['dia'] == d)}" for d in dias))
            if not a.write:
                continue
            v["tags"] = tags
            v["datum"] = {"at": datum, "len": hv.get("datum_len", 10)}
            split, gap = hv.get("split", 2), hv.get("gap", 6)
            cols = [["TAG", 10, "middle"], ["X", 16, "end"], ["Y", 16, "end"], ["SIZE", 14, "middle"]]
            cw = sum(c[1] for c in cols)
            per = -(-len(rows) // split)
            x, y = hv["at"]
            tables = []
            for k in range(split):
                part = rows[k * per:(k + 1) * per]
                if part:
                    tables.append({"at": [x + k * (cw + gap), y], "title": "HOLE TABLE" if k == 0 else "",
                                   "cols": cols, "rows": part, "size": hv.get("size", 2.2)})
            rh = hv.get("size", 2.2) * 1.9
            ys = y + 5.2 + per * rh + 12
            tables.append({"at": [x, ys], "title": "HOLE SIZES",
                           "cols": [["TAG", 10, "middle"], ["SIZE", 30, "start"], ["QTY", 12, "middle"]],
                           "rows": summary, "size": hv.get("size", 2.2)})
            sh["tables"] = tables
    if a.write:
        json.dump(cfg, open(a.config, "w", encoding="utf-8"), indent=2, ensure_ascii=False)
        open(a.config, "a").write("\n")


if __name__ == "__main__":
    main()
