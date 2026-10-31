#!/usr/bin/env python3
"""
behaviour_selftest.py -- host behaviour tests for the three display sketches.

Builds each sketch the way render_screen.py does (the IDE's tab concatenation and
prototype hoisting, the real in-repo libraries, the framebuffer stubs), appends
selftest_scenarios/common.inc and the panel's own selftest_scenarios/<sketch>.inc, and
runs it. The scenarios feed Simpit messages, the master's I2C control byte and touches
(KCM_Touch.cpp is left out and faked) and check the panel's state after its own loop().

    python3 tools/behaviour_selftest.py                    # all three panels
    python3 tools/behaviour_selftest.py KCMk1_Annunciator  # one

Covers: demo mode following the master's flag only (Hardware Inventory §8.8), the
Annunciator's DOCK tile touch, backlight dimming and the Info Display input test screen.
Exit status is non-zero if any check fails or a build fails.
"""
import argparse, os, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import render_screen as rs

SCEN = os.path.join(HERE, "selftest_scenarios")
SKETCHES = ["KCMk1_Annunciator", "KCMk1_InfoDisp", "KCMk1_ResourceDisp"]


def run(sketch):
    work = tempfile.mkdtemp(prefix="selftest_")
    scen_dir = os.path.join(work, "scen")
    os.makedirs(scen_dir)
    common = open(os.path.join(SCEN, "common.inc"), encoding="utf-8").read()
    own = open(os.path.join(SCEN, sketch + ".inc"), encoding="utf-8").read()
    open(os.path.join(scen_dir, sketch + ".inc"), "w").write(common + "\n" + own)
    saved = (rs.SCENARIOS, rs.LIB_CPPS)
    rs.SCENARIOS = scen_dir
    rs.LIB_CPPS = [c for c in rs.LIB_CPPS if "KCM_Touch" not in c]
    try:
        exe = rs.build(sketch, work)
    finally:
        rs.SCENARIOS, rs.LIB_CPPS = saved
    r = subprocess.run([exe], capture_output=True, text=True)
    sys.stdout.write(r.stdout)
    if r.returncode:
        sys.stderr.write(r.stderr[-2000:])
    return r.returncode


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("sketches", nargs="*", default=SKETCHES)
    args = ap.parse_args()
    bad = 0
    for s in args.sketches:
        print("== %s" % s)
        rc = run(s)
        print("== %s: %s" % (s, "OK" if rc == 0 else "FAILED"))
        bad += rc != 0
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
