#!/usr/bin/env python3
"""
persist_selftest.py -- host round-trip test of KCMk1_ResourceDisp's EEPROM persistence.

Links Persist.ino and the vessel-cache helpers out of AAA_Globals.ino against the
host stubs (tools/host_stubs: a byte-array EEPROM and a settable millis) with a
harness that supplies the few globals they touch, then runs scenarios: fresh block,
settle-timer store, reload and recall, whole-percent bug round trip, name truncation,
recency order and eviction at 20, corrupt-block rejection, demo mode never writing,
the default layout round trip, forgetting one vessel and all, and a change that
reverts before settling. Exit
status is the harness's.

    python3 tools/persist_selftest.py
"""
import os, re, subprocess, sys, tempfile, shutil
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import host_compile as hc

SKETCH = os.path.join(hc.DISPLAYS, "KCMk1_ResourceDisp")

HARNESS = r"""
#include <Arduino.h>
#include "KCMk1_ResourceDisp.h"
HardwareSerial Serial, SerialUSB1, Serial1, Serial2, Serial3;

// Globals the persistence code and cache helpers touch.
bool tteMode = false, demoMode = false, debugMode = false, flightScene = false, evaActive = false;
ResourceSlot slots[MAX_SLOTS];
uint8_t      slotCount = 0;
const uint32_t PERSIST_SETTLE_MS = 30000;
const bool     PERSIST_WIPE      = false;

CACHE_CODE
PERSIST_CODE
const PresetGroup PRESETS[PRESET_COUNT] = { { "SPCT", { RES_ELEC_CHARGE }, 1 }, {}, {}, {}, {}, {} };

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else printf("  ok: %s\n", msg); } while (0)

static void setLayout(const char *name, uint8_t n, int firstType, float bug0) {
  currentVesselName = String(name);
  for (uint8_t i = 0; i < MAX_SLOTS; i++) slots[i] = ResourceSlot();
  slotCount = n;
  for (uint8_t i = 0; i < n; i++) { slots[i].type = (ResourceType)(firstType + i); slots[i].bug = (i == 0) ? bug0 : -1.0f; }
}
static void tick(uint32_t ms) { stubMillis() += ms; persistService(); }

int main() {
  // 1. Fresh EEPROM: nothing to load, a valid empty block is written.
  stubEepromReset(); stubMillis() = 1000;
  persistLoad();
  CHECK(persistVesselCount() == 0, "fresh EEPROM loads empty");
  uint32_t w0 = stubEepromWrites();
  CHECK(w0 > 0, "fresh EEPROM gets a valid block written");
  persistLoad();
  CHECK(stubEepromWrites() == w0, "a valid empty block reloads without a write");

  // 2. Settle timer: a layout change stores 30 s after it stops changing.
  flightScene = true;
  setLayout("Kerbal X", 4, (int)RES_ELEC_CHARGE, 0.37f);
  tick(1000);  tick(20000);
  CHECK(stubEepromWrites() == w0, "no store before the settle time");
  tick(10000);
  CHECK(stubEepromWrites() > w0 && persistVesselCount() == 1, "stored once settled, one vessel in memory");
  uint32_t w1 = stubEepromWrites();
  tick(60000);
  CHECK(stubEepromWrites() == w1, "an unchanged state is not rewritten");

  // 3. Reload from the block and recall.
  tteMode = true; tick(0); tick(31000);
  clearVesselCache(); tteMode = false; slotCount = 0;
  persistLoad();
  CHECK(tteMode == true, "TTE toggle restored");
  CHECK(persistVesselCount() == 1, "one vessel reloaded");
  CHECK(recallVesselSlots(String("Kerbal X")), "recall finds the vessel");
  CHECK(slotCount == 4 && slots[0].type == RES_ELEC_CHARGE && slots[3].type == (ResourceType)(RES_ELEC_CHARGE + 3), "types round-trip");
  CHECK(fabsf(slots[0].bug - 0.37f) < 0.0001f && slots[1].bug < 0.0f, "bugs round-trip as whole percent");

  // 4. Long names truncate and still key.
  const char *longName = "A vessel name that is considerably longer than the forty character field";
  setLayout(longName, 2, (int)RES_LIQUID_FUEL, -1.0f);
  tick(0); tick(31000);
  slotCount = 0;
  CHECK(recallVesselSlots(String(longName)) && slotCount == 2, "over-long name truncates and recalls");

  // 5. Recency order and eviction at VESSEL_CACHE_SIZE.
  clearVesselCache();
  char nm[16];
  for (int v = 0; v < VESSEL_CACHE_SIZE + 1; v++) { snprintf(nm, sizeof(nm), "Craft %02d", v); setLayout(nm, 3, (int)RES_ELEC_CHARGE, -1.0f); saveVesselSlots(currentVesselName); }
  CHECK(persistVesselCount() == VESSEL_CACHE_SIZE, "cache holds VESSEL_CACHE_SIZE vessels");
  CHECK(strcmp(vesselCache[0].name, "Craft 20") == 0, "most recent save is at the front");
  CHECK(!recallVesselSlots(String("Craft 00")), "the oldest vessel was evicted");
  CHECK(recallVesselSlots(String("Craft 05")) && strcmp(vesselCache[0].name, "Craft 05") == 0, "a recall moves its record to the front");
  persistStoreNow();
  clearVesselCache(); persistLoad();
  CHECK(strcmp(vesselCache[0].name, "Craft 05") == 0 && strcmp(vesselCache[1].name, "Craft 20") == 0, "order survives a reload");

  // 6. A corrupt block is rejected.
  stubEeprom()[50] ^= 0x5A;
  persistLoad();
  CHECK(persistVesselCount() == 0, "corrupt block discarded");

  // 7. Demo mode never writes.
  demoMode = true; uint32_t wd = stubEepromWrites();
  setLayout("Demo craft", 5, (int)RES_ELEC_CHARGE, 0.5f); tick(31000); persistStoreNow();
  CHECK(stubEepromWrites() == wd, "demo mode writes nothing");
  demoMode = false;

  // 8. The default layout round-trips and clears.
  setLayout("Any", 3, (int)RES_LIQUID_FUEL, 0.25f);
  setDefaultLayout(); persistStoreNow();
  CHECK(layoutIsDefault(), "a set default matches the layout it was set from");
  clearDefaultLayout(); clearVesselCache(); persistLoad();
  CHECK(defaultLayoutSet() && defaultLayout.count == 3 && defaultLayout.types[0] == RES_LIQUID_FUEL && fabsf(defaultLayout.bugs[0] - 0.25f) < 0.0001f, "default layout reloads with its bug");
  clearDefaultLayout(); persistStoreNow(); persistLoad();
  CHECK(!defaultLayoutSet(), "a cleared default stays cleared");
  slotCount = 0;
  CHECK(!layoutIsDefault(), "an empty layout is not the SPCT default");

  // 9. An empty set at a save point forgets the vessel; a clear forgets all.
  setLayout("Kerbal X", 4, (int)RES_ELEC_CHARGE, -1.0f); saveVesselSlots(currentVesselName);
  setLayout("Kerbal Y", 2, (int)RES_ELEC_CHARGE, -1.0f); saveVesselSlots(currentVesselName);
  CHECK(persistVesselCount() == 2, "two vessels saved");
  currentVesselName = String("Kerbal X"); slotCount = 0; saveVesselSlots(currentVesselName);
  CHECK(persistVesselCount() == 1 && !recallVesselSlots(String("Kerbal X")) && recallVesselSlots(String("Kerbal Y")), "an empty set forgets that vessel only");
  clearVesselCache(); persistStoreNow(); persistLoad();
  CHECK(persistVesselCount() == 0, "clearing forgets all and persists");

  // 10. A change that reverts before settling is not stored.
  persistStoreNow(); uint32_t wr = stubEepromWrites();
  setLayout("Kerbal X", 4, (int)RES_ELEC_CHARGE, 0.37f); persistStoreNow(); wr = stubEepromWrites();
  slots[0].bug = 0.50f; tick(5000); slots[0].bug = 0.37f; tick(5000); tick(31000);
  CHECK(stubEepromWrites() == wr, "a reverted change is not stored");

  printf(fails ? "persist selftest: %d FAILURE(S)\n" : "persist selftest: all checks passed\n", fails);
  return fails ? 1 : 0;
}
"""

def slice_between(text, start_marker, end_regex):
    i = text.index(start_marker)
    m = re.compile(end_regex, re.M).search(text, i)
    return text[i:m.end()]

def main():
    globs = open(os.path.join(SKETCH, "AAA_Globals.ino"), encoding="utf-8", errors="replace").read()
    # The cache helpers: from the VESSEL SLOT MEMORY banner through the end of recallVesselSlots().
    cache = slice_between(globs, "/***************************************************************************************\n   VESSEL SLOT MEMORY", r"^bool recallVesselSlots[^\n]*\n(?:.*\n)*?^}\n")
    cache = cache.replace('#include "KCMk1_ResourceDisp.h"', "")
    persist = open(os.path.join(SKETCH, "Persist.ino"), encoding="utf-8", errors="replace").read()
    persist = persist.replace('#include "KCMk1_ResourceDisp.h"', "")
    out = tempfile.mkdtemp(prefix="kcm_persist_")
    try:
        kdc = open(hc.KDC_HDR, encoding="utf-8", errors="replace").read()
        kdc = re.sub(r'#include\s+"[A-Za-z]:\\[^"]*body_params\.h"', '#include <body_params.h>', kdc)
        open(os.path.join(out, "KerbalDisplayCommon.h"), "w").write(kdc)
        src = HARNESS.replace("CACHE_CODE", cache).replace("PERSIST_CODE", persist)
        cpp = os.path.join(out, "persist_selftest.cpp")
        open(cpp, "w").write(src)
        exe = os.path.join(out, "persist_selftest")
        cmd = ["g++", "-std=gnu++17", "-O0", "-U_FORTIFY_SOURCE", "-Wall", "-Wno-unused-function", "-Wno-unused-variable", "-Wno-unused-parameter",
               "-I" + out, "-I" + hc.STUBS] + ["-I" + d for d in hc.library_include_dirs()] + ["-I" + SKETCH, "-x", "c++", cpp, "-o", exe]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stdout, r.stderr); return 2
        r = subprocess.run([exe], capture_output=True, text=True)
        print(r.stdout, end=""); print(r.stderr, end="")
        return r.returncode
    finally:
        shutil.rmtree(out, ignore_errors=True)

if __name__ == "__main__":
    sys.exit(main())
