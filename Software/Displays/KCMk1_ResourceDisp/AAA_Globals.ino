/***************************************************************************************
   AAA_Globals.ino -- Variable definitions for Kerbal Controller Mk1 Resource Display
   All global variable instances are owned here. Types, enums, and structs are declared
   in KCMk1_ResourceDisp.h; extern declarations there make them visible to all tabs.
   AAA_Config.ino owns tunable constants. This file owns runtime state.
****************************************************************************************/
#include "KCMk1_ResourceDisp.h"


/***************************************************************************************
   DISPLAY AND TOUCH
****************************************************************************************/
KCM_TFT     infoDisp(KCM_TFT_RS, KCM_TFT_CS, KCM_TFT_RESET);
TouchResult lastTouch;


/***************************************************************************************
   SIMPIT OBJECT
   Uses SerialUSB1 for Simpit traffic, leaving Serial free for debug output.
   Only active when demoMode is false.
****************************************************************************************/
KerbalSimpit simpit(SerialUSB1);


/***************************************************************************************
   SCREEN STATE
****************************************************************************************/
ScreenType activeScreen = screen_Standby;  // starts on standby; transitions to Main on flight entry
ScreenType prevScreen   = screen_COUNT;    // sentinel -- forces chrome on first loop


/***************************************************************************************
   DISPLAY MODE
   tteMode     -- false = counter row shows percent, true = time-to-empty at the
                  current rate (time-to-full for waste-type resources).
   flightScene -- true when KSP is in a flight scene (set by SCENE_CHANGE_MESSAGE).
                  Used to guard screen transitions — don't show flight data out of flight.
   simpitConnected -- true once the Simpit handshake succeeds.
   idleState   -- when true and not in a flight scene, show standby screen.
                  Set by I2C master command in Phase 3.
****************************************************************************************/
bool tteMode          = false;
float    warpFactor       = 1.0f;   // updated from FLIGHT_STATUS; 1.0 until the first arrives
bool     refreshPending   = false;
uint32_t refreshRequestMs = 0;
bool flightScene      = false;
bool simpitConnected  = false;
bool idleState        = false;
bool evaActive        = false;   // true once the EVA bar set is applied (see loop() reconcile)
bool layoutRecalled   = false;   // the current layout came from vessel memory (shown in the strip)
bool evaFlag          = false;   // raw EVA flag from FLIGHT_STATUS_MESSAGE (reconciled in loop())


/***************************************************************************************
   RESOURCE SLOTS
   slots[] holds the active bar configuration. slotCount is how many are active.
   Entries beyond slotCount are ignored. All slots initialise to RES_NONE.
****************************************************************************************/
ResourceSlot slots[MAX_SLOTS];
uint8_t      slotCount = DEFAULT_SLOT_COUNT;


/***************************************************************************************
   SWITCH TO SCREEN
   Sets activeScreen and forces a full chrome redraw on the next loop pass by
   resetting prevScreen to the sentinel value screen_COUNT.
   Always use this function — never set activeScreen directly.
****************************************************************************************/
void switchToScreen(ScreenType s) {
  activeScreen     = s;
  prevScreen       = screen_COUNT;
}


/***************************************************************************************
   NEEDS MAIN REDRAW FLAG
   Set by SimpitHandler when a vessel switch or name receipt requires the main screen
   chrome to be redrawn immediately (e.g. slot count or types changed). loop() checks
   this flag and calls drawStaticMain() directly, keeping display logic out of the
   message handler. Cleared by loop() after the redraw.
   This avoids calling drawStaticMain() from inside simpit.update(), which would risk
   drawing stale content if further messages arrive in the same update call.
****************************************************************************************/
bool needsMainRedraw = false;


/***************************************************************************************
   VESSEL SLOT MEMORY
   Cache of up to VESSEL_CACHE_SIZE per-vessel slot configurations, keyed by the name
   VESSEL_NAME_MESSAGE reports (truncated to VESSEL_NAME_LEN - 1 characters). It is
   loaded from EEPROM at boot and written back by Persist.ino, so it survives a power
   cycle. Kept in recency order: a save or a recall moves its record to the front,
   and a full cache evicts the last record, the one longest unused.
****************************************************************************************/
VesselSlotRecord vesselCache[VESSEL_CACHE_SIZE];
String           currentVesselName = "";
VesselSlotRecord defaultLayout;      // count 0 until the pilot sets one


/***************************************************************************************
   VESSEL CACHE HELPERS
****************************************************************************************/
static int8_t vesselCacheFind(const char *name) {
  for (uint8_t i = 0; i < VESSEL_CACHE_SIZE; i++) {
    if (vesselCache[i].name[0] != '\0' && strcmp(vesselCache[i].name, name) == 0) return (int8_t)i;
  }
  return -1;
}

// Move record idx to the front, shifting the ones before it down by one.
static void vesselCacheToFront(uint8_t idx) {
  if (idx == 0) return;
  VesselSlotRecord tmp = vesselCache[idx];
  memmove(&vesselCache[1], &vesselCache[0], idx * sizeof(VesselSlotRecord));
  vesselCache[0] = tmp;
}

void clearVesselCache() {
  for (uint8_t i = 0; i < VESSEL_CACHE_SIZE; i++) {
    vesselCache[i].name[0] = '\0';
    vesselCache[i].count   = 0;
  }
}

// Forget one vessel: drop its record and close the gap so recency order holds.
void forgetVesselSlots(const String &name) {
  char key[VESSEL_NAME_LEN];
  strlcpy(key, name.c_str(), sizeof(key));
  int8_t idx = vesselCacheFind(key);
  if (idx < 0) return;
  uint8_t n = VESSEL_CACHE_SIZE - 1 - (uint8_t)idx;
  if (n) memmove(&vesselCache[idx], &vesselCache[idx + 1], n * sizeof(VesselSlotRecord));
  vesselCache[VESSEL_CACHE_SIZE - 1].name[0] = '\0';
  vesselCache[VESSEL_CACHE_SIZE - 1].count   = 0;
}

// Save the current slot configuration for a given vessel name: overwrite its record
// if it has one, else take the first empty one, else evict the last. Either way the
// record ends up at the front. An EMPTY set is not a layout: CLEAR is the explicit
// "forget" for a vessel, so saving one removes the vessel's record instead, and the
// next visit starts from the default like any vessel not in memory.
void saveVesselSlots(const String &name) {
  if (name.length() == 0) return;
  if (slotCount == 0) { forgetVesselSlots(name); return; }
  char key[VESSEL_NAME_LEN];
  strlcpy(key, name.c_str(), sizeof(key));

  int8_t idx = vesselCacheFind(key);
  if (idx < 0) {
    for (uint8_t i = 0; i < VESSEL_CACHE_SIZE; i++) {
      if (vesselCache[i].name[0] == '\0') { idx = (int8_t)i; break; }
    }
  }
  if (idx < 0) idx = VESSEL_CACHE_SIZE - 1;   // full: the last record is the least recently used

  VesselSlotRecord &r = vesselCache[idx];
  strlcpy(r.name, key, sizeof(r.name));
  r.count = slotCount;
  for (uint8_t j = 0; j < slotCount; j++) {
    r.types[j] = slots[j].type;
    r.bugs[j]  = slots[j].bug;
  }
  vesselCacheToFront((uint8_t)idx);
}

/***************************************************************************************
   DEFAULT LAYOUT
****************************************************************************************/
bool defaultLayoutSet() { return defaultLayout.count > 0; }

void setDefaultLayout() {
  defaultLayout.name[0] = '\0';
  defaultLayout.count   = slotCount;
  for (uint8_t j = 0; j < slotCount; j++) {
    defaultLayout.types[j] = slots[j].type;
    defaultLayout.bugs[j]  = slots[j].bug;
  }
}

void clearDefaultLayout() {
  defaultLayout.name[0] = '\0';
  defaultLayout.count   = 0;
}

// The current slot types, in order, against the effective default's.
bool layoutIsDefault() {
  const ResourceType *types; uint8_t n;
  if (defaultLayoutSet()) { types = defaultLayout.types; n = defaultLayout.count; }
  else                    { types = PRESETS[0].types;    n = PRESETS[0].count;    }
  if (slotCount != n) return false;
  for (uint8_t j = 0; j < n; j++) if (slots[j].type != types[j]) return false;
  return true;
}

// Attempt to recall slot configuration for a given vessel name.
// Returns true and restores slot types if found; returns false if not in cache.
// Values are always zeroed — Simpit will repopulate on next message.
bool recallVesselSlots(const String &name) {
  char key[VESSEL_NAME_LEN];
  strlcpy(key, name.c_str(), sizeof(key));
  int8_t idx = vesselCacheFind(key);
  if (idx < 0 || vesselCache[idx].count == 0) return false;
  vesselCacheToFront((uint8_t)idx);
  const VesselSlotRecord &r = vesselCache[0];
  for (uint8_t j = 0; j < MAX_SLOTS; j++) slots[j] = ResourceSlot();
  slotCount = r.count;
  for (uint8_t j = 0; j < slotCount; j++) {
    slots[j].type = r.types[j];
    slots[j].bug  = r.bugs[j];
    // values stay at 0.0f — Simpit will populate them
  }
  return true;
}
