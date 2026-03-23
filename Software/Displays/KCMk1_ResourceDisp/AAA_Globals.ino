/***************************************************************************************
   AAA_Globals.ino -- Variable definitions for Kerbal Controller Mk1 Resource Display
   All global variable instances are owned here. Types, enums, and structs are declared
   in KCMk1_ResourceDisp.h; extern declarations there make them visible to all tabs.
   AAA_Config.ino owns tunable constants. This file owns runtime state.
   NOTE: When Simpit integration (Phase 2) adds live telemetry state, this file will
   need an invalidateAllState()-style function (see KCMk1_Annunciator AAA_Globals.ino)
   to force full redraws after vessel switches or scene changes.
****************************************************************************************/
#include "KCMk1_ResourceDisp.h"


/***************************************************************************************
   DISPLAY AND TOUCH
****************************************************************************************/
RA8875      infoDisp  = RA8875(RA8875_CS, RA8875_RESET);
TouchResult lastTouch;


/***************************************************************************************
   SIMPIT OBJECT
   Uses SerialUSB1 for Simpit traffic, leaving Serial free for debug output.
   Only active when demoMode is false (Phase 2 / live KSP connection).
****************************************************************************************/
KerbalSimpit simpit(SerialUSB1);


/***************************************************************************************
   SCREEN STATE
****************************************************************************************/
ScreenType activeScreen = screen_Standby;  // starts on standby; transitions to Main on flight entry
ScreenType prevScreen   = screen_COUNT;    // sentinel -- forces chrome on first loop


/***************************************************************************************
   DISPLAY MODE
   stageMode   -- false = show vessel totals, true = show active stage values.
   flightScene -- true when KSP is in a flight scene (set by SCENE_CHANGE_MESSAGE).
                  Used to guard screen transitions — don't show flight data out of flight.
****************************************************************************************/
bool stageMode    = false;
bool flightScene  = false;


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
   Phase 2 note: when Simpit state is added, extend this to call invalidateAllState()
   as the Annunciator does, ensuring dirty-tracked fields repaint after every transition.
****************************************************************************************/
void switchToScreen(ScreenType s) {
  activeScreen = s;
  prevScreen   = screen_COUNT;
}


/***************************************************************************************
   VESSEL SLOT MEMORY
   In-RAM cache of up to VESSEL_CACHE_SIZE per-vessel slot configurations.
   currentVesselName is populated by VESSEL_NAME_MESSAGE.
   vesselCache is searched/updated on vessel change and name receipt.
****************************************************************************************/
VesselSlotRecord vesselCache[VESSEL_CACHE_SIZE];
String           currentVesselName = "";


/***************************************************************************************
   VESSEL CACHE HELPERS
****************************************************************************************/

// Save the current slot configuration for a given vessel name.
// Overwrites an existing entry if the name matches, or uses the oldest/empty slot.
void saveVesselSlots(const String &name) {
  if (name.length() == 0 || slotCount == 0) return;

  // Look for an existing entry to overwrite
  for (uint8_t i = 0; i < VESSEL_CACHE_SIZE; i++) {
    if (vesselCache[i].vesselName == name) {
      vesselCache[i].count = slotCount;
      for (uint8_t j = 0; j < slotCount; j++) vesselCache[i].types[j] = slots[j].type;
      return;
    }
  }

  // No existing entry — find an empty slot
  for (uint8_t i = 0; i < VESSEL_CACHE_SIZE; i++) {
    if (vesselCache[i].vesselName.length() == 0) {
      vesselCache[i].vesselName = name;
      vesselCache[i].count = slotCount;
      for (uint8_t j = 0; j < slotCount; j++) vesselCache[i].types[j] = slots[j].type;
      return;
    }
  }

  // Cache full — overwrite the last entry (simple eviction)
  uint8_t idx = VESSEL_CACHE_SIZE - 1;
  vesselCache[idx].vesselName = name;
  vesselCache[idx].count = slotCount;
  for (uint8_t j = 0; j < slotCount; j++) vesselCache[idx].types[j] = slots[j].type;
}

// Attempt to recall slot configuration for a given vessel name.
// Returns true and restores slot types if found; returns false if not in cache.
// Values are always zeroed — Simpit will repopulate on next message.
bool recallVesselSlots(const String &name) {
  for (uint8_t i = 0; i < VESSEL_CACHE_SIZE; i++) {
    if (vesselCache[i].vesselName == name && vesselCache[i].count > 0) {
      for (uint8_t j = 0; j < MAX_SLOTS; j++) slots[j] = ResourceSlot();
      slotCount = vesselCache[i].count;
      for (uint8_t j = 0; j < slotCount; j++) {
        slots[j].type = vesselCache[i].types[j];
        // values stay at 0.0f — Simpit will populate them
      }
      return true;
    }
  }
  return false;
}
