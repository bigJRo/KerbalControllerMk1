#!/usr/bin/env python3
"""
kc_step2d.py -- flatten a plate part (STEP) into a 1:1 cut-layer PDF for the drawing tools.

The panels have an Illustrator production file at 1:1 that the hole table and the hole views
are drawn from. A machined plate has none, so this makes the equivalent from its STEP model:
the faces on the top of the plate (the largest Z) are flattened into one PDF page at 1:1 with

  - the plate outline (outer wire of the largest face) in dark grey,
  - every cut (an inner wire of a top face) in red, like the cut layer of a production file,
  - inner wires that are filled by another body's face (a grille modelled as its own body
    inside an opening) in light grey: they are the outline of that area, not a cut.

The datum is the model origin; the plate sits MARGIN mm in from the page's lower-left corner,
so page coordinates (mm, top-down, as the generator's crops use them) are
x_page = X + MARGIN and y_page = page_height - MARGIN - Y.

    python3 kc_step2d.py PART.step OUT.pdf

Prints the page size and the datum's page position. Circles become four Bezier arcs (as an
Illustrator export has them), so kc_holes.py finds round holes in the result.
"""
import math
import sys

from OCP.BRep import BRep_Tool
from OCP.BRepAdaptor import BRepAdaptor_Curve, BRepAdaptor_Surface
from OCP.BRepBndLib import BRepBndLib
from OCP.BRepTools import BRepTools, BRepTools_WireExplorer
from OCP.Bnd import Bnd_Box
from OCP.GeomAbs import GeomAbs_Circle, GeomAbs_Line, GeomAbs_Plane
from OCP.STEPControl import STEPControl_Reader
from OCP.TopAbs import TopAbs_FACE, TopAbs_REVERSED, TopAbs_WIRE
from OCP.TopExp import TopExp_Explorer
from OCP.TopoDS import TopoDS

PT = 72 / 25.4
MARGIN = 10.0
K = 0.5522847498          # Bezier handle for a quarter circle


def box(shape):
    b = Bnd_Box()
    BRepBndLib.Add_s(shape, b)
    a, c = b.CornerMin(), b.CornerMax()
    return a.X(), a.Y(), a.Z(), c.X(), c.Y(), c.Z()


def top_faces(shape):
    zmax = box(shape)[5]
    out = []
    ex = TopExp_Explorer(shape, TopAbs_FACE)
    while ex.More():
        f = TopoDS.Face(ex.Current())
        s = BRepAdaptor_Surface(f)
        if s.GetType() == GeomAbs_Plane and abs(abs(s.Plane().Axis().Direction().Z()) - 1) < 1e-6 \
                and abs(box(f)[5] - zmax) < 1e-4 and abs(box(f)[2] - zmax) < 1e-4:
            out.append(f)
        ex.Next()
    return out


def wire_path(w):
    """A wire as PDF path segments in model mm: ('M', x, y), ('L', x, y), ('C', x1, y1, x2, y2, x, y)."""
    segs = []
    we = BRepTools_WireExplorer(w)
    while we.More():
        e = we.Current()
        c = BRepAdaptor_Curve(e)
        rev = e.Orientation() == TopAbs_REVERSED
        u0, u1 = c.FirstParameter(), c.LastParameter()
        if rev:
            u0, u1 = u1, u0
        p0 = c.Value(u0)
        if not segs:
            segs.append(("M", p0.X(), p0.Y()))
        if c.GetType() == GeomAbs_Circle:
            circ = c.Circle()
            r = circ.Radius()
            ctr = circ.Location()
            # walk the arc in up to 90 degree pieces, each a cubic Bezier
            n = max(1, math.ceil(abs(u1 - u0) / (math.pi / 2) - 1e-9))
            du = (u1 - u0) / n
            for i in range(n):
                a0, a1 = u0 + i * du, u0 + (i + 1) * du
                q0, q1 = c.Value(a0), c.Value(a1)
                d0, d1 = c.DN(a0, 1), c.DN(a1, 1)
                h = 4 / 3 * math.tan((a1 - a0) / 4)
                segs.append(("C", q0.X() + h * d0.X(), q0.Y() + h * d0.Y(),
                             q1.X() - h * d1.X(), q1.Y() - h * d1.Y(), q1.X(), q1.Y()))
        elif c.GetType() == GeomAbs_Line:
            p1 = c.Value(u1)
            segs.append(("L", p1.X(), p1.Y()))
        else:
            for i in range(1, 25):
                p = c.Value(u0 + (u1 - u0) * i / 24)
                segs.append(("L", p.X(), p.Y()))
        we.Next()
    return segs


def main():
    step, out = sys.argv[1], sys.argv[2]
    r = STEPControl_Reader()
    r.ReadFile(step)
    r.TransferRoots()
    shape = r.OneShape()
    x0, y0, _, x1, y1, _ = box(shape)
    faces = top_faces(shape)
    plate = max(faces, key=lambda f: (box(f)[3] - box(f)[0]) * (box(f)[4] - box(f)[1]))
    others = [f for f in faces if not f.IsSame(plate)]

    def key(w):
        return tuple(round(v, 1) for v in box(w)[:2] + box(w)[3:5])
    filled = {key(BRepTools.OuterWire_s(f)) for f in others}
    near = lambda k: any(all(abs(a - b) <= 0.3 for a, b in zip(k, f)) for f in filled)

    outline, cuts, refs = [], [], []
    for f in faces:
        outer = BRepTools.OuterWire_s(f)
        if f.IsSame(plate):
            outline.append(wire_path(outer))
        ex = TopExp_Explorer(f, TopAbs_WIRE)
        while ex.More():
            w = TopoDS.Wire(ex.Current())
            if not w.IsSame(outer):
                (refs if f.IsSame(plate) and near(key(w)) else cuts).append(wire_path(w))
            ex.Next()

    # whole points: pdftocairo rescales a page whose size is not, which would shrink the drawing
    W = math.ceil(((x1 - x0) + 2 * MARGIN) * PT) / PT
    H = math.ceil(((y1 - y0) + 2 * MARGIN) * PT) / PT

    def P(x, y):
        return f"{(x - x0 + MARGIN) * PT:.4f} {(y - y0 + MARGIN) * PT:.4f}"

    def emit(paths, rgb, width_mm, dash=""):
        o = [f"{rgb} RG {width_mm * PT:.3f} w {dash}"]
        for segs in paths:
            for s in segs:
                if s[0] == "M":
                    o.append(f"{P(s[1], s[2])} m")
                elif s[0] == "L":
                    o.append(f"{P(s[1], s[2])} l")
                else:
                    o.append(f"{P(s[1], s[2])} {P(s[3], s[4])} {P(s[5], s[6])} c")
            o.append("h S")
        return "\n".join(o)

    content = "\n".join([emit(outline, "0.137 0.122 0.125", 0.18),
                         emit(refs, "0.6 0.6 0.6", 0.12, "[1.2 0.8] 0 d"),
                         "[] 0 d",
                         emit(cuts, "0.929 0.110 0.141", 0.12)]).encode()
    objs = [b"<< /Type /Catalog /Pages 2 0 R >>",
            b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
            f"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 {round(W * PT)} {round(H * PT)}] /Contents 4 0 R >>".encode(),
            b"<< /Length %d >>\nstream\n" % len(content) + content + b"\nendstream"]
    pdf = bytearray(b"%PDF-1.4\n")
    offs = []
    for i, o in enumerate(objs, 1):
        offs.append(len(pdf))
        pdf += b"%d 0 obj\n" % i + o + b"\nendobj\n"
    xref = len(pdf)
    pdf += b"xref\n0 %d\n0000000000 65535 f \n" % (len(objs) + 1)
    pdf += b"".join(b"%010d 00000 n \n" % o for o in offs)
    pdf += b"trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n" % (len(objs) + 1, xref)
    open(out, "wb").write(pdf)
    print(f"page {W:.2f} x {H:.2f} mm; datum (model 0,0) at page x {MARGIN - x0:.2f}, y {H - MARGIN + y0:.2f} (top-down)")
    print(f"outline {len(outline)}, cuts {len(cuts)}, filled openings {len(refs)}")


if __name__ == "__main__":
    main()
