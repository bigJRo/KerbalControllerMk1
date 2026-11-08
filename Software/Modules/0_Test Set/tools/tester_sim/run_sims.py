#!/usr/bin/env python3
"""
run_sims.py -- build and run the per-module tester sketches against the real
module firmware on the host.

Each run compiles:
  * the tester sketch + KCMk1TesterCore + the simulation core into an
    executable (Arduino, Wire, ST7789, FT6236 replaced by host stand-ins), and
  * the module sketch + its library into a shared library, compiled the way
    the Arduino IDE builds it (library .cpp files do NOT see the sketch's
    #defines). RST in the simulation reloads that library, like a reset.
A scripted operator reads the tester's screen and works the simulated module.
The run passes if the tester's summary matches the expectation.

Usage:
    python3 run_sims.py                 # every module, every run
    python3 run_sims.py action          # one module
    python3 run_sims.py action --shots  # also save PNG screenshots
"""
import argparse, os, re, shutil, subprocess, sys, tempfile, glob

HERE = os.path.dirname(os.path.abspath(__file__))
TESTSET = os.path.dirname(os.path.dirname(HERE))           # 0_Test Set
MODULES = os.path.dirname(TESTSET)                          # Software/Modules
LIBS = os.path.join(MODULES, "libraries")
TESTERS = os.path.join(TESTSET, "Module Testers")
STUBS = os.path.join(HERE, "stubs")

KBC, KJC, K7SC = "KerbalButtonCore", "KerbalJoystickCore", "Kerbal7SegmentCore"

# name: (tester sketch, module folder, module library, INT pin, runs[, options])
# options: libdefs = #defines also given to the module's library .cpp files. The
# Arduino IDE does NOT do this, so a module that sets library options in its
# sketch is built both ways (see the "-ide" entries).
# runs: list of (label, [--opt ...], expected summary verdict, steps expected to FAIL)
MODULE_TABLE = {
    "action": ("KCMk1_Test_Action_Control", "Action Control", KBC, 1, [
        ("normal", [], "PASS", []),
        ("dead AG5", ["kbc_dead=0x4"], "FAIL", ["Buttons"]),
        ("stuck input 13", ["kbc_stuck=0x2000"], "FAIL", ["Buttons"]),
        ("RST not wired", ["rst_dead=1"], "PASS", []),
    ]),
    "ui":        ("KCMk1_Test_UI_Control", "UI Control", KBC, 1, [("normal", [], "PASS", [])]),
    "function":  ("KCMk1_Test_Function_Control", "Function Control", KBC, 1, [
        ("normal", [], "PASS", []),
        ("SCE switch dead", ["kbc_dead=0x40000"], "FAIL", ["Switches"]),
    ], {"libdefs": ["KBC_INPUT_COUNT=24", "KBC_SHIFTREG_COUNT=3"]}),
    # As the Arduino IDE builds it: the library .cpp files never see the
    # sketch's KBC_INPUT_COUNT 24, so they run the 16-input layout.
    "function-ide": ("KCMk1_Test_Function_Control", "Function Control", KBC, 1, [
        ("IDE build", [], "FAIL", ["Buttons", "Switches"]),
    ]),
    "stability": ("KCMk1_Test_Stability_Control", "Stability Control", KBC, 1, [("normal", [], "PASS", [])]),
    "vehicle":   ("KCMk1_Test_Vehicle_Control", "Vehicle Control", KBC, 1, [
        ("normal", [], "PASS", []),
        ("CHUTE switch left ON", ["kbc_stuck=0x10000"], "FAIL", ["Buttons", "Switches"]),
    ], {"libdefs": ["KBC_INPUT_COUNT=24", "KBC_SHIFTREG_COUNT=3"]}),
    "vehicle-ide": ("KCMk1_Test_Vehicle_Control", "Vehicle Control", KBC, 1, [
        ("IDE build", [], "FAIL", ["Buttons", "Switches"]),
    ]),
    "time":      ("KCMk1_Test_Time_Control", "Time Control", KBC, 1, [
        ("normal", [], "PASS", []),
        ("selector RVR dead", ["kbc_dead=0x2000"], "FAIL", ["Mode selector"]),
    ]),
    "aux":       ("KCMk1_Test_Auxiliary_Control", "Auxiliary Control", KBC, 1, [("normal", [], "PASS", [])]),
    # KerbalJoystickCore 2.x reports every button edge twice (KJC_Buttons.cpp
    # buttonsGetChangeMask re-arms the change mask on the read after an edge),
    # so "Enable and INT" counts 4 packets for one press + release.
    "rotation":  ("KCMk1_Test_Joystick_Rotation", "Rotation", KJC, 1, [
        ("current firmware", [], "FAIL", ["Enable and INT"])]),
    "translation": ("KCMk1_Test_Joystick_Translation", "Translation", KJC, 1, [
        ("current firmware", [], "FAIL", ["Enable and INT"])]),
    # Throttle 2.2 clears the button INT-pending flag when it collects the
    # events (I2C.cpp i2cSyncINT), so a press with the lever still raises no INT.
    "throttle":  ("KCMk1_Test_Throttle", "Throttle", None, 5, [
        ("current firmware", [], "FAIL", ["Enable and INT"])]),
    "gpws":      ("KCMk1_Test_GPWS", "GPWS", K7SC, 16, [
        ("normal", [], "PASS", []),
        ("PROX switch dead", ["dead_pin=18"], "FAIL", ["PROX and RNDZ"]),
        ("encoder wired backwards", ["enc_reversed=1"], "FAIL", ["Encoder", "Encoder speed"]),
    ]),
    "prewarp":   ("KCMk1_Test_PreWarp_Time", "Time Control - Pre-Warp", K7SC, 16, [
        ("normal", [], "PASS", []),
        ("1 DAY switch dead", ["dead_pin=17"], "FAIL", ["Presets"]),
        ("encoder wired backwards", ["enc_reversed=1"], "FAIL", ["Encoder", "Encoder speed"]),
    ]),
    "dualenc":   ("KCMk1_Test_Dual_Encoder", "Dual Encoder", None, 1, [
        ("normal", [], "PASS", []),
        ("encoder wired backwards", ["enc_reversed=1"], "FAIL", ["ENC1 count", "ENC2 count"]),
        ("operator miscounts", ["enc_extra=1"], "FAIL", ["ENC1 count", "ENC2 count"]),
    ]),
}

CXX = ["g++", "-std=gnu++17", "-O1", "-w"]


def pil_python():
    """A Python that can import Pillow (font generation, PNG screenshots)."""
    for py in (sys.executable, "/usr/bin/python3", "python3"):
        if subprocess.run([py, "-c", "from PIL import Image, ImageFont"], capture_output=True).returncode == 0:
            return py
    raise SystemExit("Pillow is required (pip install pillow)")


def sh(cmd, cwd=None):
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
    if r.returncode != 0:
        sys.stderr.write(" ".join(cmd) + "\n" + r.stdout + r.stderr)
        raise SystemExit("build failed")
    return r


FUNC_RE = re.compile(r"^((?:static\s+|inline\s+)*[A-Za-z_][\w:<>,\s\*&]*?[\s\*&]+(\w+)\s*\(([^;{}]*)\))\s*\{",
                     re.M)


def hoist_prototypes(src_text):
    """Mimic the Arduino IDE: declare every sketch function right after the last
    #include, before any sketch-defined type. A signature that names a type
    defined later in the sketch then fails here as it does in the IDE."""
    protos = []
    for m in FUNC_RE.finditer(src_text):
        name = m.group(2)
        if name in ("if", "for", "while", "switch", "return"):
            continue
        protos.append(m.group(1).strip() + ";")
    lines = src_text.split("\n")
    last_inc = max([i for i, l in enumerate(lines) if l.startswith("#include")] or [0])
    lines.insert(last_inc + 1, "\n".join(protos))
    return "\n".join(lines)


def compile_obj(src, out, incs, defs=(), extra=(), ino=False):
    cmd = CXX + list(extra) + ["-c", "-o", out]
    for d in defs:
        cmd.append("-D" + d)
    for i in incs:
        cmd += ["-I", i]
    if ino:
        hoisted = out + ".ino.cpp"
        with open(src) as f:
            text = hoist_prototypes(f.read())
        with open(hoisted, "w") as f:
            f.write('#line 1 "%s"\n' % src.replace('"', '\\"') + text)
        cmd += ["-include", "Arduino.h", "-x", "c++", "-I", os.path.dirname(src)]
        src = hoisted
    cmd.append(src)
    sh(cmd)


def build(name, work):
    tester, modfolder, modlib, intpin = MODULE_TABLE[name][:4]
    opts = MODULE_TABLE[name][5] if len(MODULE_TABLE[name]) > 5 else {}
    libdefs = opts.get("libdefs", [])
    font = os.path.join(work, "sim_font.h")
    if not os.path.exists(font):
        sh([pil_python(), os.path.join(HERE, "make_font.py"), font])

    # ---- module shared library (Arduino-style: separate translation units) ----
    mdir = os.path.join(MODULES, modfolder)
    incs = [STUBS, os.path.join(LIBS, "KerbalModuleCommon", "src"), mdir]
    if modlib:
        incs.append(os.path.join(LIBS, modlib, "src"))
    pic = ["-fPIC", "-fno-gnu-unique", "-DF_CPU=10000000UL", "-DMEGATINYCORE"]
    objs = []
    for ino in glob.glob(os.path.join(mdir, "*.ino")):
        o = os.path.join(work, "mod_ino.o")
        compile_obj(ino, o, incs, ["setup=module_setup", "loop=module_loop"], pic, ino=True)
        objs.append(o)
    srcs = glob.glob(os.path.join(mdir, "*.cpp"))
    if modlib:
        srcs += glob.glob(os.path.join(LIBS, modlib, "src", "*.cpp"))
    srcs.append(os.path.join(HERE, "module_shim.cpp"))
    for i, s in enumerate(srcs):
        o = os.path.join(work, "mod_%d.o" % i)
        compile_obj(s, o, incs, libdefs, extra=pic)
        objs.append(o)
    so = os.path.join(work, "module.so")
    sh(CXX + ["-shared", "-Wl,-Bsymbolic", "-o", so] + objs)

    # ---- tester executable ----
    tdir = os.path.join(TESTERS, tester)
    tincs = [STUBS, work, HERE, os.path.join(LIBS, "KerbalModuleCommon", "src"),
             os.path.join(LIBS, "KCMk1TesterCore", "src"), tdir]
    tobjs = []
    o = os.path.join(work, "tester_ino.o")
    compile_obj(os.path.join(tdir, tester + ".ino"), o, tincs,
                ["setup=tester_setup", "loop=tester_loop"], ino=True)
    tobjs.append(o)
    tsrcs = glob.glob(os.path.join(LIBS, "KCMk1TesterCore", "src", "*.cpp"))
    tsrcs += [os.path.join(HERE, "sim_core.cpp"), os.path.join(HERE, "scenarios.cpp")]
    for i, s in enumerate(tsrcs):
        o = os.path.join(work, "t_%d.o" % i)
        compile_obj(s, o, tincs)
        tobjs.append(o)
    exe = os.path.join(work, "sim")
    sh(CXX + ["-rdynamic", "-o", exe] + tobjs + ["-ldl"])
    return exe, so, intpin


def to_png(shotdir):
    code = ("import glob, os, sys\nfrom PIL import Image\n"
            "for p in sorted(glob.glob(os.path.join(sys.argv[1], '*.ppm'))):\n"
            "    Image.open(p).resize((480, 640), Image.NEAREST).save(p[:-4] + '.png'); os.remove(p)\n")
    subprocess.run([pil_python(), "-c", code, shotdir])


def run(name, shots, keep):
    work = tempfile.mkdtemp(prefix="ktcsim_")
    ok = True
    try:
        exe, so, intpin = build(name, work)
        for label, opts, want, wantFail in MODULE_TABLE[name][4]:
            args = [exe, so, name.split("-")[0], "--int-pin", str(intpin)]
            shotdir = None
            if shots and label == "normal":
                shotdir = os.path.join(HERE, "shots", name)
                shutil.rmtree(shotdir, ignore_errors=True)
                os.makedirs(shotdir)
                args += ["--shots", shotdir]
            for o in opts:
                args += ["--opt", o]
            r = subprocess.run(args, capture_output=True, text=True, timeout=600)
            out = r.stdout
            m = re.search(r"SUMMARY (\S+)", out)
            got = m.group(1) if m else "NO-SUMMARY"
            failed = re.findall(r"STEP FAILED: (.*?) --", out)
            good = (got == want) and sorted(failed) == sorted(wantFail)
            ok &= good
            print("%-10s %-22s %-12s %s" % (name, label, got, "ok" if good else "UNEXPECTED (want %s, fails %s, got fails %s)" % (want, wantFail, failed)))
            if not good or keep:
                print(out[-6000:])
                if r.stderr:
                    print(r.stderr[-3000:])
            if shotdir:
                to_png(shotdir)
    finally:
        if not keep:
            shutil.rmtree(work, ignore_errors=True)
        else:
            print("build dir:", work)
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("modules", nargs="*")
    ap.add_argument("--shots", action="store_true")
    ap.add_argument("--keep", action="store_true")
    a = ap.parse_args()
    names = a.modules or list(MODULE_TABLE)
    ok = True
    for n in names:
        ok &= run(n, a.shots, a.keep)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
