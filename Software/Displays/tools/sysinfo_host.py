#!/usr/bin/env python3
"""
sysinfo_host.py -- host build, behaviour tests and renders for KCMk1_SysInfo, the
Sys Info Display (XIAO RP2350 + ER-TFTM020-2). It is not a Teensy / RA8876 sketch, so
host_compile.py and render_screen.py do not cover it.

The sketch is concatenated the way the Arduino IDE does it (main tab first, the rest
alphabetically, a prototype hoisted for every function; host_compile.py's code), then
compiled with g++ -Wall -Wextra -Werror against tools/host_stubs_sysinfo/ (arduino-pico
core, Wire, SPI, SD, Adafruit_ST7789) and the real KCMk1_SystemConfig header. The two
font .c files are compiled as C, as the IDE does. tools/sysinfo_host.inc plays the master.

    python3 tools/sysinfo_host.py                 # fonts check, build, every test, every render
    python3 tools/sysinfo_host.py test            # tests only
    python3 tools/sysinfo_host.py render [NAME..] # renders only -> Documents/User/assets/sysinfo/
    python3 tools/sysinfo_host.py --out DIR render

Checks, in order:
  fonts   src/fonts/ in the sketch is byte-identical to the shared fonts_ili copies
  build   no warnings
  tests   each behaviour check passes, and no string anywhere is below the 22 px cap
          rule or outside the 2 px margins (Gfx.ino's counters)
  renders the same two counters, for every scenario; PNGs written

Plain Python 3 and g++ / gcc; no third-party modules. Exit status non-zero on any failure.
"""
import argparse
import filecmp
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import host_compile as hc            # noqa: E402
from render_screen import png_from_ppm  # noqa: E402

SKETCH = "KCMk1_SysInfo"
SKETCH_DIR = os.path.join(hc.DISPLAYS, SKETCH)
STUBS = os.path.join(HERE, "host_stubs_sysinfo")
HARNESS = os.path.join(HERE, "sysinfo_host.inc")
SYSCFG = os.path.join(hc.LIB_ROOT, "KCMk1_SystemConfig", "src")
SHARED_FONTS = os.path.join(hc.LIB_ROOT, "KerbalDisplayCommon", "src", "fonts_ili")
SKETCH_FONTS = os.path.join(SKETCH_DIR, "src", "fonts")
REPO = os.path.dirname(os.path.dirname(hc.DISPLAYS))
DEFAULT_OUT = os.path.join(REPO, "Documents", "User", "assets", "sysinfo")


def check_fonts():
    bad = []
    for f in sorted(os.listdir(SKETCH_FONTS)):
        shared = os.path.join(SHARED_FONTS, f)
        if not os.path.isfile(shared) or not filecmp.cmp(shared, os.path.join(SKETCH_FONTS, f), shallow=False):
            bad.append(f)
    if bad:
        print("== fonts: FAILED -- differ from fonts_ili (copy them again): " + ", ".join(bad))
        return False
    print("== fonts: %d files identical to fonts_ili" % len(os.listdir(SKETCH_FONTS)))
    return True


def build(out):
    files = hc.tabs(SKETCH_DIR)
    parts = ['#include <Arduino.h>', '#include "%s.h"' % SKETCH] + hc.hoisted_prototypes(files)
    for fp in files:
        parts += ['#line 1 "%s"' % fp, open(fp, encoding="utf-8").read()]
    parts += ['#line 1 "%s"' % HARNESS, open(HARNESS, encoding="utf-8").read()]
    cpp = os.path.join(out, SKETCH + ".cpp")
    open(cpp, "w").write("\n".join(parts) + "\n")
    inc = ["-I" + STUBS, "-I" + SYSCFG, "-I" + SKETCH_DIR]
    objs = []
    for f in ("Roboto_Black_32.c", "Roboto_Black_48.c"):
        o = os.path.join(out, f + ".o")
        r = subprocess.run(["gcc", "-std=c11", "-c", "-Wall", "-Werror", "-o", o, os.path.join(SKETCH_FONTS, f)],
                           capture_output=True, text=True)
        if r.returncode:
            sys.stderr.write(r.stderr)
            return None
        objs.append(o)
    exe = os.path.join(out, "sysinfo_host")
    cmd = ["g++", "-std=gnu++17", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
           "-Wno-missing-field-initializers"] + inc + ["-o", exe, cpp] + objs
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        sys.stdout.write(r.stdout)
        sys.stderr.write(r.stderr)
        print("== build: FAILED")
        return None
    print("== build: OK (%d tabs, -Wall -Wextra -Werror)" % len(files))
    return exe


def names(exe, kind):
    r = subprocess.run([exe, "list"], capture_output=True, text=True, check=True)
    return [l.split(" ", 1)[1] for l in r.stdout.split("\n") if l.startswith(kind + " ")]


def run_tests(exe, only):
    fails = 0
    for n in only or names(exe, "test"):
        r = subprocess.run([exe, "test", n], capture_output=True, text=True)
        ok = r.returncode == 0
        print("  %-32s %s" % (n, "ok" if ok else "FAIL"))
        if not ok:
            sys.stdout.write(r.stderr)
            fails += 1
    print("== tests: %s" % ("all passed" if not fails else "%d FAILED" % fails))
    return fails == 0


def run_renders(exe, only, out_dir, build_dir):
    os.makedirs(out_dir, exist_ok=True)
    fails = 0
    env = dict(os.environ)
    env.pop("KCM_SD_ROOT", None)          # no card: the built-in splash
    for n in only or names(exe, "render"):
        ppm = os.path.join(build_dir, n + ".ppm")
        r = subprocess.run([exe, "render", n, ppm], capture_output=True, text=True, env=env)
        if r.returncode:
            print("  %-32s FAIL" % n)
            sys.stdout.write(r.stderr)
            fails += 1
            continue
        png = os.path.join(out_dir, "SysInfo_%s.png" % n)
        png_from_ppm(ppm, png)
        print("  %-32s -> %s" % (n, os.path.relpath(png, REPO)))
    print("== renders: %s" % ("all clean" if not fails else "%d FAILED" % fails))
    return fails == 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("what", nargs="?", choices=["all", "test", "render"], default="all")
    ap.add_argument("names", nargs="*")
    ap.add_argument("--out", default=DEFAULT_OUT)
    ap.add_argument("--keep", action="store_true")
    args = ap.parse_args()
    ok = check_fonts()
    d = tempfile.mkdtemp(prefix="kcm_sysinfo_")
    try:
        exe = build(d)
        if not exe:
            sys.exit(1)
        if args.what in ("all", "test"):
            ok &= run_tests(exe, args.names if args.what == "test" else [])
        if args.what in ("all", "render"):
            ok &= run_renders(exe, args.names if args.what == "render" else [], args.out, d)
        if args.keep:
            print("build dir kept:", d)
    finally:
        if not args.keep:
            shutil.rmtree(d, ignore_errors=True)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
