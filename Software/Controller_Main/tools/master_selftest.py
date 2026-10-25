#!/usr/bin/env python3
"""
master_selftest.py -- run Controller_Main's setup() and loop() on the host and check
the start-up and link behaviour that no compile can show.

Builds the sketch exactly as tools/host_compile.py does (same concatenation, same
stand-ins), links it with a test main(), and runs it. The KerbalSimpit and Wire
stand-ins are controllable: whether KSP answers the handshake, and a log of every I2C
transmission.

Checks:
  - setup() returns with KSP not running (no blocking handshake);
  - the display carriers get PROCEED before KSP is up: Info Display 1/2 (0x12/0x13),
    the Annunciator's 9-byte command (0x10) and the Resource Display's (0x11);
  - the handshake is retried every SIMPIT_RETRY_MS, not every loop;
  - once KSP answers: channels registered, handler installed, update() every loop;
  - an inbound FLIGHT_STATUS with time warp sets the Annunciator's WARP flag, and
    leaving the flight scene clears it;
  - the Annunciator command keeps going out while KSP is down.

Usage:
    python3 Software/Controller_Main/tools/master_selftest.py

Exit status 0 when every check passes.
"""

import os
import subprocess
import sys
import tempfile
import shutil

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from host_compile import sketch_source, include_dirs, GXX_FLAGS   # noqa: E402

TEST_MAIN = r'''
#include <cstdio>
#include <cstring>
HardwareSerial Serial, SerialUSB1, Serial1, Serial2, Serial3;
TwoWire Wire, Wire1, Wire2;
volatile uint32_t ARM_DWT_CYCCNT, ARM_DWT_CTRL, ARM_DEMCR, SCB_AIRCR, USB1_USBCMD;
extern "C" void usb_init(void) {}

static int fails = 0;
#define CHECK(c, m) do { if (c) printf("  ok: %s\n", m); else { printf("  FAIL: %s\n", m); fails++; } } while (0)

static int writesTo(uint8_t addr, int len, int from = 0) {
  int n = 0;
  for (int i = from; i < Wire.logN; i++) if (Wire.log[i].addr == addr && Wire.log[i].n == len) n++;
  return n;
}
static TwoWire::Rec *lastTo(uint8_t addr) {
  for (int i = Wire.logN - 1; i >= 0; i--) if (Wire.log[i].addr == addr) return &Wire.log[i];
  return nullptr;
}
static int annunCmds = 0;   // 9-byte Annunciator commands seen by runFor()
static void runFor(uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += 10) {
    if (Wire.logN > 48) Wire.logN = 0;   // keep room; checks read the latest writes
    int from = Wire.logN;
    stubMillis() += 10;
    loop();
    annunCmds += writesTo(0x10, 9, from);
  }
}

int main() {
  // GPWS / throttle / joystick INT lines idle high (no module asserting)
  for (int p = 0; p < 64; p++) stubPin(p) = HIGH;
  stubSimpit().kspUp = false;
  stubMillis() = 1000;

  printf("setup with KSP not running\n");
  setup();
  CHECK(stubSimpit().initCalls == 1, "one handshake attempt in setup");
  CHECK(!simpitConnected(), "not connected");
  CHECK(writesTo(0x12, 2) >= 1 && writesTo(0x13, 2) >= 1, "Info Displays sent their 2-byte control (PROCEED at init)");
  TwoWire::Rec *a = lastTo(0x10);
  CHECK(a && a->n == 9 && (a->b[0] >> 4) == 0x2, "Annunciator got the 9-byte command with PROCEED");
  TwoWire::Rec *r = lastTo(0x11);
  CHECK(r && r->n == 2 && (r->b[0] >> 4) == 0x2, "Resource Display got its command with PROCEED");

  printf("loop while KSP is down\n");
  Wire.logN = 0; annunCmds = 0;
  runFor(1500);
  CHECK(stubSimpit().initCalls == 1, "no retry before SIMPIT_RETRY_MS");
  CHECK(stubSimpit().updateCalls == 0, "update() not called while disconnected");
  CHECK(annunCmds >= 14 && annunCmds <= 16, "Annunciator command every 100 ms while KSP is down");
  runFor(600);
  CHECK(stubSimpit().initCalls == 2, "retried once after 2 s");

  printf("KSP comes up\n");
  stubSimpit().kspUp = true;
  runFor(2100);
  CHECK(simpitConnected(), "connected after the next retry");
  CHECK(stubSimpit().registered > 10, "channels registered");
  CHECK(stubSimpit().handler != nullptr, "message handler installed");
  int u0 = stubSimpit().updateCalls;
  runFor(100);
  CHECK(stubSimpit().updateCalls - u0 == 10, "update() once per loop when connected");
  int tries = stubSimpit().initCalls;
  runFor(5000);
  CHECK(stubSimpit().initCalls == tries, "no handshake attempts once connected");

  printf("inbound messages\n");
  byte scene = 0;   // SCENE_CHANGE: 0 = entering flight
  stubSimpit().handler(SCENE_CHANGE_MESSAGE, &scene, 1);
  CHECK(simpitFlightScene(), "flight scene entered");
  flightStatusMessage fs; memset(&fs, 0, sizeof fs);
  fs.flightStatusFlags = FLIGHT_IN_FLIGHT; fs.currentTWIndex = 3;
  stubSimpit().handler(FLIGHT_STATUS_MESSAGE, (byte *)&fs, sizeof fs);
  CHECK(simpitWarping(), "time warp seen from FLIGHT_STATUS");
  runFor(20);
  a = lastTo(0x10);
  CHECK(a && (a->b[3] & 0x02), "Annunciator WARP flag set");
  scene = 1;        // leaving flight
  stubSimpit().handler(SCENE_CHANGE_MESSAGE, &scene, 1);
  runFor(20);
  a = lastTo(0x10);
  CHECK(!simpitWarping() && a && !(a->b[3] & 0x02), "leaving flight clears WARP");
  byte junk[3] = { 1, 2, 3 };   // a SAS_MODE_INFO of the wrong size must not reach FLIGHT_STATUS
  fs.currentTWIndex = 0; stubSimpit().handler(FLIGHT_STATUS_MESSAGE, (byte *)&fs, sizeof fs);
  stubSimpit().handler(SAS_MODE_INFO_MESSAGE, junk, sizeof junk);
  CHECK(!simpitWarping(), "short SAS_MODE_INFO ignored");

  printf(fails ? "master selftest: %d FAILED\n" : "master selftest: all checks passed\n", fails);
  return fails != 0;
}
'''


def main():
    out = tempfile.mkdtemp(prefix="kcm_master_test_")
    try:
        src, _ = sketch_source()
        cpp = os.path.join(out, "selftest.cpp")
        open(cpp, "w").write(src + TEST_MAIN)
        exe = os.path.join(out, "selftest")
        cmd = ["g++"] + GXX_FLAGS + ["-w"] + ["-I" + d for d in include_dirs(None)] + \
              ["-x", "c++", cpp, "-o", exe]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stderr)
            print("master selftest: BUILD FAILED")
            return 1
        return subprocess.run([exe]).returncode
    finally:
        shutil.rmtree(out, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
