/***************************************************************************************
   Persist.ino -- EEPROM persistence for KCMk1 Resource Display

   What survives a power cycle: the per-vessel slot memory (vesselCache, up to
   VESSEL_CACHE_SIZE vessels, each its slot types and reserve bugs, keyed by name),
   the pilot-set default layout (defaultLayout, same shape) and the TTE toggle.
   Everything else the panel shows comes from KSP or from compile-time configuration.

   Where: the Teensy 4.1's emulated EEPROM, 4284 bytes of wear-levelled flash behind
   the EEPROM library. The image below is about 1.5 KB. The SD card was the other
   candidate and lost: a card can be absent or corrupt, FAT writes stall for tens of
   milliseconds, and the bench unit would behave differently from the installed one
   whenever the card was out. The core only programs a byte whose value changes, so a
   store that changes nothing costs nothing in wear.

   Image: a header (magic, schema, the TTE toggle, the number of records), the
   default layout record, the vessel records in recency order, and a CRC-16 over all
   of it. Bugs go out as whole percent (255 = none), which is lossless since every
   gesture sets whole percent.
   A block whose magic, schema or CRC does not check out is discarded and a fresh
   empty one written, which is also how a schema bump or PERSIST_WIPE forgets.

   When: persistStoreNow() is called on a vessel switch and on leaving the flight
   scene, moments where the write's few milliseconds cannot be seen. Between those,
   persistService() watches a signature of the live state (slot types, bug percents,
   the toggle) and, once it has held still for PERSIST_SETTLE_MS, folds the current
   vessel's layout into the cache and stores, so a power cut mid-flight loses at most
   that long of edits. Demo mode never writes.
****************************************************************************************/
#include "KCMk1_ResourceDisp.h"
#include <EEPROM.h>


/***************************************************************************************
   IMAGE LAYOUT
****************************************************************************************/
static const uint32_t PERSIST_MAGIC  = 0x4B524431UL;   // 'KRD1'
static const uint16_t PERSIST_SCHEMA = 2;              // bump when the layout below changes
static const uint8_t  BUG_NONE       = 255;

struct __attribute__((packed)) PersistVessel {
  char    name[VESSEL_NAME_LEN];
  uint8_t count;
  uint8_t types[MAX_SLOTS];
  uint8_t bugs[MAX_SLOTS];           // whole percent, BUG_NONE = no bug
};

struct __attribute__((packed)) PersistImage {
  uint32_t      magic;
  uint16_t      schema;
  uint8_t       tteMode;
  uint8_t       vesselCount;
  PersistVessel dflt;                // the pilot-set default layout; count 0 = none
  PersistVessel vessels[VESSEL_CACHE_SIZE];
  uint16_t      crc;                 // CRC-16/CCITT over everything above
};
static_assert(sizeof(PersistImage) <= 4284, "persist image must fit the Teensy 4.1 EEPROM");

static PersistImage _img;           // staging buffer for load and store
static uint32_t     _lastSig     = 0;
static uint32_t     _pendingSig  = 0;
static uint32_t     _pendingMs   = 0;
static bool         _pending     = false;

// Explicit prototypes: the IDE hoists one for every function, above the tabs where
// PersistImage is not yet visible, unless the sketch declares it itself.
static uint16_t persistCrc(const PersistImage &img);
static void     persistPack(PersistVessel &v, const VesselSlotRecord &r);
static void     persistUnpack(VesselSlotRecord &r, const PersistVessel &v);
static void     persistBuild(PersistImage &img);
static void     persistWrite(const PersistImage &img);

static uint16_t persistCrc(const PersistImage &img) {
  const uint8_t *p = (const uint8_t *)&img;
  size_t n = sizeof(PersistImage) - sizeof(img.crc);
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < n; i++) {
    crc ^= (uint16_t)p[i] << 8;
    for (uint8_t b = 0; b < 8; b++) crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
  }
  return crc;
}

// One record each way. Types are range-checked on the way in, bugs clamped.
static void persistPack(PersistVessel &v, const VesselSlotRecord &r) {
  memset(&v, 0, sizeof(v));
  strlcpy(v.name, r.name, sizeof(v.name));
  v.count = r.count > MAX_SLOTS ? MAX_SLOTS : r.count;
  for (uint8_t j = 0; j < MAX_SLOTS; j++) {
    v.types[j] = (j < v.count) ? (uint8_t)r.types[j] : (uint8_t)RES_NONE;
    v.bugs[j]  = (j < v.count && r.bugs[j] >= 0.0f) ? (uint8_t)roundf(constrain(r.bugs[j], 0.0f, 1.0f) * 100.0f) : BUG_NONE;
  }
}

static void persistUnpack(VesselSlotRecord &r, const PersistVessel &v) {
  strlcpy(r.name, v.name, sizeof(r.name));          // terminates a full-width name
  r.count = v.count > MAX_SLOTS ? MAX_SLOTS : v.count;
  for (uint8_t j = 0; j < MAX_SLOTS; j++) {
    uint8_t t = v.types[j];
    r.types[j] = (t < (uint8_t)RES_COUNT) ? (ResourceType)t : RES_NONE;
    r.bugs[j]  = (v.bugs[j] == BUG_NONE || v.bugs[j] > 100) ? -1.0f : v.bugs[j] / 100.0f;
  }
}

// The live cache, default layout and toggle as an image.
static void persistBuild(PersistImage &img) {
  memset(&img, 0, sizeof(img));
  img.magic   = PERSIST_MAGIC;
  img.schema  = PERSIST_SCHEMA;
  img.tteMode = tteMode ? 1 : 0;
  persistPack(img.dflt, defaultLayout);
  uint8_t n = 0;
  for (uint8_t i = 0; i < VESSEL_CACHE_SIZE; i++) {
    const VesselSlotRecord &r = vesselCache[i];
    if (r.name[0] == '\0' || r.count == 0) continue;
    persistPack(img.vessels[n++], r);
  }
  img.vesselCount = n;
  img.crc = persistCrc(img);
}

static void persistWrite(const PersistImage &img) {
  EEPROM.put(0, img);
  if (debugMode) {
    Serial.print(F("ResourceDisp: persist: stored "));
    Serial.print(img.vesselCount);
    Serial.println(F(" vessel(s)"));
  }
}

// Signature of what persistence cares about, so a settle timer can tell a change
// from telemetry churn: slot types and bug percents in order, plus the toggle.
static uint32_t persistSignature() {
  uint32_t h = 2166136261UL;
  auto mix = [&h](uint8_t b) { h ^= b; h *= 16777619UL; };
  mix(tteMode ? 1 : 0);
  mix(slotCount);
  for (uint8_t i = 0; i < slotCount; i++) {
    mix((uint8_t)slots[i].type);
    mix(slots[i].bug < 0.0f ? BUG_NONE : (uint8_t)roundf(constrain(slots[i].bug, 0.0f, 1.0f) * 100.0f));
  }
  return h;
}


/***************************************************************************************
   PUBLIC
****************************************************************************************/
uint8_t persistVesselCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < VESSEL_CACHE_SIZE; i++) if (vesselCache[i].name[0] != '\0' && vesselCache[i].count > 0) n++;
  return n;
}

// Boot: read the block, take it if it checks out, otherwise start empty and write a
// fresh block so the next boot finds a valid one. PERSIST_WIPE forces the latter.
void persistLoad() {
  EEPROM.get(0, _img);
  bool valid = _img.magic == PERSIST_MAGIC && _img.schema == PERSIST_SCHEMA &&
               _img.vesselCount <= VESSEL_CACHE_SIZE && _img.crc == persistCrc(_img);
  clearVesselCache();
  clearDefaultLayout();
  if (PERSIST_WIPE || !valid) {
    if (debugMode) Serial.println(PERSIST_WIPE ? F("ResourceDisp: persist: WIPE requested, memory cleared")
                                              : F("ResourceDisp: persist: no valid block, starting empty"));
    persistBuild(_img);
    persistWrite(_img);
  } else {
    tteMode = _img.tteMode != 0;
    persistUnpack(defaultLayout, _img.dflt);
    for (uint8_t i = 0; i < _img.vesselCount; i++) {
      VesselSlotRecord &r = vesselCache[i];
      persistUnpack(r, _img.vessels[i]);
      if (r.name[0] == '\0' || r.count == 0) { r.name[0] = '\0'; r.count = 0; }
    }
    if (debugMode) {
      Serial.print(F("ResourceDisp: persist: loaded "));
      Serial.print(persistVesselCount());
      Serial.println(defaultLayoutSet() ? F(" vessel(s), pilot default layout") : F(" vessel(s), SPCT default"));
    }
  }
  _lastSig = persistSignature();
  _pending = false;
}

// Write the cache and toggle out now. Only bytes that differ are programmed.
void persistStoreNow() {
  _pending  = false;
  _lastSig  = persistSignature();
  if (demoMode) return;
  persistBuild(_img);
  persistWrite(_img);
}

// Every loop pass: notice a change, wait for it to settle, then fold the current
// vessel's layout into the cache and store.
void persistService() {
  if (demoMode) return;
  uint32_t sig = persistSignature();
  uint32_t now = millis();
  if (sig != _lastSig) {
    if (!_pending || sig != _pendingSig) { _pendingSig = sig; _pendingMs = now; _pending = true; }
    if (now - _pendingMs >= PERSIST_SETTLE_MS) {
      if (flightScene && !evaActive) saveVesselSlots(currentVesselName);
      persistStoreNow();
    }
  } else if (_pending) {
    _pending = false;   // changed and changed back before settling: nothing to store
  }
}
