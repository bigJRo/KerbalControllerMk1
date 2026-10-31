#!/usr/bin/env python3
"""
host_build.py -- build and test KCMk1_Master on the host, without a Teensy toolchain.

The sketch is one main tab (KCMk1_Master.ino: setup() and loop()) plus .cpp / .h pairs,
so the Arduino IDE compiles each .cpp on its own and nothing depends on its prototype
generation. This tool does the same with g++ against the stand-ins in tools/host/
(Arduino / Teensy core, Wire, WDT_T4, KerbalSimpit), which simulate time, pins,
Serial and the I2C bus.

  python3 tools/host_build.py                    # compile every file (stand-in Simpit)
  python3 tools/host_build.py --simpit DIR       # compile against the real KerbalSimpit src/
  python3 tools/host_build.py --test             # build and run the selftest (tools/tests/)
  python3 tools/host_build.py --all [--simpit DIR]   # both, plus the config check

The config check compares the display addresses and sync bytes in config.h with
Software/Displays/libraries/KCMk1_SystemConfig, and the sketch's copy of body_params.h
with Software/Common/body_params.h (the IDE cannot include outside the sketch), so
neither can drift apart.

Exit status 0 when everything passes.
"""

import argparse
import glob
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE     = os.path.dirname(os.path.abspath(__file__))
SKETCH   = os.path.dirname(HERE)
SOFTWARE = os.path.dirname(SKETCH)
HOST     = os.path.join(HERE, "host")
KMC      = os.path.join(SOFTWARE, "Modules", "libraries", "KerbalModuleCommon", "src")
TESTS    = os.path.join(HERE, "tests")
SYSCFG   = os.path.join(SOFTWARE, "Displays", "libraries", "KCMk1_SystemConfig", "src", "KCMk1_SystemConfig.h")
COMMON   = os.path.join(SOFTWARE, "Common")
COPIES   = ["body_params.h"]                 # sketch copies of Software/Common headers

FLAGS = ["-std=gnu++17", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
         "-Wno-format-truncation", "-DKCM_HOST=1"]


def sketch_sources():
    srcs = sorted(glob.glob(os.path.join(SKETCH, "*.cpp")))
    return srcs + [os.path.join(SKETCH, "KCMk1_Master.ino")]


def includes(simpit):
    dirs = ([simpit] if simpit else []) + [HOST, SKETCH, KMC]
    return ["-I" + d for d in dirs]


def compile_one(src, out_dir, simpit, syntax_only):
    # The IDE adds #include <Arduino.h> to the .ino only; every .cpp must include what it
    # uses, so only the .ino gets it here too.
    ino = src.endswith(".ino")
    cmd = ["g++"] + FLAGS + includes(simpit) + (["-include", "Arduino.h", "-x", "c++"] if ino else []) + [src]
    if simpit:
        # The real library's headers are not ours to keep warning-free.
        cmd = [c for c in cmd if c != "-Werror"]
    if syntax_only:
        cmd += ["-fsyntax-only"]
        obj = None
    else:
        obj = os.path.join(out_dir, os.path.basename(src) + ".o")
        cmd += ["-c", "-o", obj]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0 or r.stderr:
        sys.stdout.write(r.stderr)
    return r.returncode == 0, obj


def build(simpit):
    print("== KCMk1_Master: compile, Simpit: %s" % (simpit or "stand-in"))
    ok = True
    for s in sketch_sources():
        good, _ = compile_one(s, None, simpit, True)
        ok = ok and good
    print("== compile: %s" % ("OK" if ok else "FAILED"))
    return ok


def selftest():
    print("== KCMk1_Master: selftest")
    out = tempfile.mkdtemp(prefix="kcm_master_")
    try:
        objs = []
        srcs = sketch_sources() + [os.path.join(HOST, "host_globals.cpp")] + \
            sorted(glob.glob(os.path.join(TESTS, "*.cpp")))
        for s in srcs:
            good, obj = compile_one(s, out, None, False)
            if not good:
                print("== selftest: BUILD FAILED")
                return False
            objs.append(obj)
        exe = os.path.join(out, "selftest")
        r = subprocess.run(["g++", "-o", exe] + objs, capture_output=True, text=True)
        if r.returncode != 0:
            sys.stdout.write(r.stderr)
            print("== selftest: LINK FAILED")
            return False
        r = subprocess.run([exe])
        return r.returncode == 0
    finally:
        shutil.rmtree(out, ignore_errors=True)


def config_check():
    cfg = open(os.path.join(SKETCH, "config.h")).read()
    sc = open(SYSCFG).read()
    pairs = {
        "ADDR_ANNUNCIATOR": "KCM_I2C_ADDR_ANNUNCIATOR", "ADDR_RESDISP": "KCM_I2C_ADDR_RESDISP",
        "ADDR_INFODISP_1": "KCM_I2C_ADDR_INFODISP", "ADDR_INFODISP_2": "KCM_I2C_ADDR_INFODISP_2",
        "ADDR_SYSINFO": "KCM_I2C_ADDR_SYSINFODISP", "SYNC_ANNUNCIATOR": "KCM_I2C_SYNC_ANNUNCIATOR",
        "SYNC_RESDISP": "KCM_I2C_SYNC_RESDISP", "SYNC_INFODISP": "KCM_I2C_SYNC_INFODISP",
        "SYNC_BACKLIGHT": "KCM_I2C_SYNC_BACKLIGHT", "BACKLIGHT_LEN": "KCM_I2C_BACKLIGHT_LEN",
        "BL_FLIGHT_PCT": "KCM_BL_BRIGHTNESS_PCT", "BL_STANDBY_PCT": "KCM_BL_STANDBY_PCT",
    }
    ok = True
    for mine, theirs in pairs.items():
        a = re.search(r"\b%s\s*=\s*(0x[0-9A-Fa-f]+|\d+)" % mine, cfg)
        b = re.search(r"#define\s+%s\s+(0x[0-9A-Fa-f]+|\d+)" % theirs, sc)
        if not a or not b or int(a.group(1), 0) != int(b.group(1), 0):
            print("  config mismatch: %s %s vs %s %s" % (mine, a and a.group(1), theirs, b and b.group(1)))
            ok = False
    for name in COPIES:
        if open(os.path.join(SKETCH, name)).read() != open(os.path.join(COMMON, name)).read():
            print("  %s differs from Software/Common/%s: copy it again" % (name, name))
            ok = False
    print("== config check (vs KCMk1_SystemConfig, Software/Common): %s" % ("OK" if ok else "FAILED"))
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--simpit", help="KerbalSimpit library src/ directory (default: the stand-in)")
    ap.add_argument("--test", action="store_true", help="build and run the selftest")
    ap.add_argument("--all", action="store_true", help="compile (stand-in, and --simpit if given), selftest, config check")
    a = ap.parse_args()
    if a.all:
        ok = build(None)
        if a.simpit:
            ok = build(a.simpit) and ok
        ok = config_check() and ok
        ok = selftest() and ok
    elif a.test:
        ok = selftest()
    else:
        ok = build(a.simpit)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
