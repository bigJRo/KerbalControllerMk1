#pragma once
/***************************************************************************************
   KCMk1_ResourceDisp.h -- Shared declarations for Kerbal Controller Mk1 Resource Display
   Included by every .ino tab. Defines types, enums, ResourceSlot struct, resource
   type enum, and extern declarations for all globals.
****************************************************************************************/

// Requires KerbalDisplayCommon >= 3.0.0 (rev-2: RA8876 KCM_TFT + ILI9341_t3 fonts)
#include <KerbalDisplayCommon.h>   // pulls in KCM_Display (KCM_TFT) + fonts + SystemConfig
#include <KCM_Touch.h>             // FT5316 capacitive touch (rev-2; replaces GSL1680F)
// KerbalDisplayAudio is a direct sketch dependency (not a KDC sub-dependency).
// Audio is never used on this panel: no setupAudio()/audio*() call is made, so the
// library configures and drives no pins. Its AUDIO_PIN default is the TONE buzzer on
// pin 2 (moved from the old rev-1 pin 9, which is now the TFT backlight) — no conflict.
#include <KerbalDisplayAudio.h>
#include <KerbalSimpit.h>
#include <KCMk1_SystemConfig.h>   // shared hardware/threshold constants (KCMk1_SystemConfig library)

// rev-2 compat: screen/boot code declares font pointers as the old sumotoy `tFont`
// type. KerbalDisplayCommon v3 uses ILI9341_t3_font_t; alias so those signatures compile.
typedef ILI9341_t3_font_t tFont;


/***************************************************************************************
   SKETCH VERSION
   Follows semantic versioning: MAJOR.MINOR.PATCH
     MAJOR — incompatible structural changes
     MINOR — new features or screens added
     PATCH — bug fixes, threshold tuning, comment/style changes
   This sketch requires KerbalDisplayCommon >= 3.0.0
****************************************************************************************/
static const uint8_t SKETCH_VERSION_MAJOR = 3;   // rev-2: RA8876/Teensy 4.1, 1024x600 relayout
static const uint8_t SKETCH_VERSION_MINOR = 0;
static const uint8_t SKETCH_VERSION_PATCH = 0;


/***************************************************************************************
   RESOURCE TYPE ENUM
   One entry per supported KSP resource. RES_NONE = empty slot.
   Order matches the selection grid layout in ScreenSelect.ino.
****************************************************************************************/
enum ResourceType : uint8_t {
  RES_NONE = 0,
  // Power
  RES_ELEC_CHARGE,
  RES_STORED_CHARGE,
  // Propellants (KSP1 native)
  RES_LIQUID_FUEL,
  RES_LIQUID_OX,
  RES_SOLID_FUEL,
  RES_MONO_PROP,
  RES_EVA_PROP,
  RES_XENON,
  // Propellants (CRP mod, KSP1 — via CUSTOM_RESOURCE_2_MESSAGE)
  RES_LIQUID_H2,
  RES_LIQUID_METHANE,
  RES_LITHIUM,
  RES_INTAKE_AIR,
  // Nuclear (CRP mod, KSP1)
  RES_ENRICHED_URANIUM,
  RES_DEPLETED_URANIUM,
  // Other (KSP1)
  RES_ORE,
  RES_ABLATOR,
  // Life Support (TAC-LS mod, KSP1)
  RES_LS_OXYGEN,
  RES_LS_CO2,
  RES_LS_FOOD,
  RES_LS_WASTE,
  RES_LS_WATER,
  RES_LS_LIQUID_WASTE,
  // Agriculture (CRP mod, KSP1)
  RES_FERTILIZER,

  RES_COUNT  // sentinel -- total number of real resource types (not including RES_NONE)
};

// Number of selectable resource types (excludes RES_NONE)
static const uint8_t RESOURCE_TYPE_COUNT = (uint8_t)RES_COUNT - 1;


/***************************************************************************************
   RESOURCE SLOT STRUCT
   One instance per active bar on the main screen.
   current and maxVal are 0.0–1.0 floats for demo; will be actual units from Simpit later.
****************************************************************************************/
struct ResourceSlot {
  ResourceType type         = RES_NONE;
  float        current      = 0.0f;   // vessel total current amount
  float        maxVal       = 1.0f;   // vessel total max capacity
  float        stageCurrent = 0.0f;   // active stage current amount
  float        stageMax     = 1.0f;   // active stage max capacity
};


/***************************************************************************************
   SCREEN TYPE ENUM
****************************************************************************************/
enum ScreenType : uint8_t {
  screen_Standby = 0,
  screen_Main    = 1,
  screen_Select  = 2,
  screen_Detail  = 3,   // numerical resource detail — craft/stage values per resource
  screen_COUNT   = 4    // sentinel
};


/***************************************************************************************
   EXTERN DECLARATIONS
   Defined in AAA_Config.ino and AAA_Globals.ino.
****************************************************************************************/

// From AAA_Config.ino
extern bool     debugMode;
extern bool     demoMode;
extern const bool     STANDALONE_TEST;   // true = skip I2C master handshake (no master connected)
extern const uint8_t  DISPLAY_ROTATION;
// Slot count limits — constexpr so they can be used as compile-time array sizes
static constexpr uint8_t MIN_SLOTS          = 4;
static constexpr uint8_t MAX_SLOTS          = 16;
static constexpr uint8_t DEFAULT_SLOT_COUNT = 9;
extern const uint16_t LOW_RES_THRESHOLD;   // percent (0-100); reserved — bars no longer recolor by level

// From AAA_Globals.ino
extern KCM_TFT       infoDisp;
extern TouchResult  lastTouch;
extern KerbalSimpit simpit;
extern ScreenType   activeScreen;
extern ScreenType   prevScreen;
extern uint32_t     lastScreenSwitch;   // #8 timestamp of last switchToScreen() call
extern ResourceSlot slots[];        // active resource slots (MAX_SLOTS entries)
extern uint8_t      slotCount;      // number of currently active slots (4-16)
extern bool         stageMode;      // false = TOTAL (whole craft), true = STAGE (current stage)
extern bool         flightScene;    // true when KSP is in a flight scene
extern bool         simpitConnected; // true once Simpit handshake succeeds
extern bool         idleState;      // true = show standby when not in flight (set by I2C master)
extern bool         needsMainRedraw; // set by SimpitHandler to request main screen chrome redraw
extern bool         evaActive;      // true = a Kerbal is on EVA (mode applied); drives the EVA bar set
extern bool         evaFlag;        // raw latched EVA flag from the last FLIGHT_STATUS_MESSAGE

// Resource type metadata (from Resources.ino)
const char*    resLabel(ResourceType t);
const char*    resFullName(ResourceType t);
uint16_t       resColor(ResourceType t);
ResourceType   resTypeByIndex(uint8_t index);  // 0-based index into selectable types
bool           isEvaResource(ResourceType t);  // true for the fixed EVA bar set (EC/EVA/O2/Food/Water)

// Screen management
// Always use switchToScreen() to change screens — never set activeScreen directly.
// switchToScreen() sets activeScreen, resets prevScreen to screen_COUNT (which
// triggers the chrome redraw block on the next loop pass), and calls clearTouchISR()
// to discard any touches queued during the transition redraw.
void switchToScreen(ScreenType s);

// Per-tab functions
void processTouchEvents();
void initDemoMode();
void initDefaultSlots();
void loadEvaSlots();     // load the fixed EVA bar set: EC, EVA Propellant, O2, Food, Water
void initAllSlots();
void stepDemoState();
void initSimpit();
void bootSimText(KCM_TFT &tft);
void setupI2CSlave();
void updateI2CState();
void buildI2CPacketAndAssert();
extern volatile bool i2cProceedReceived;
void drawStaticMain(KCM_TFT &tft);
void updateScreenMain(KCM_TFT &tft);
void redrawStageModeButton(KCM_TFT &tft);
int8_t sidebarHitTest(uint16_t x, uint16_t y);
void drawStaticSelect(KCM_TFT &tft);
void updateScreenSelect(KCM_TFT &tft);
bool handleSelectTouch(uint16_t x, uint16_t y);
void drawStaticDetail(KCM_TFT &tft);
void updateScreenDetail(KCM_TFT &tft);
bool handleDetailTouch(uint16_t x, uint16_t y);


/***************************************************************************************
   RESOURCE STAGE DATA FLAG
   Returns true if this resource type has a separate Simpit stage channel.
   Resources that return false have stage fields that mirror vessel fields — the
   detail screen hides the STAGE section for these to avoid showing duplicate data.
   Resources WITH real stage channels: LF, LOx, SF, Xenon, Ablator.
   All others (EC, Mono, Ore, TAC-LS, CRP custom) have no stage channel.
****************************************************************************************/
inline bool resHasStageData(ResourceType t) {
  return t == RES_LIQUID_FUEL  ||
         t == RES_LIQUID_OX    ||
         t == RES_SOLID_FUEL   ||
         t == RES_XENON        ||
         t == RES_ABLATOR;
}


/***************************************************************************************
   VESSEL SLOT MEMORY
   In-RAM cache of per-vessel slot configurations.
   Persists for the duration of the session (until power cycle or reset).
   Keyed by vessel name (from VESSEL_NAME_MESSAGE). Up to VESSEL_CACHE_SIZE entries.
   Slot types only are stored — values are always repopulated from Simpit on recall.
****************************************************************************************/
static constexpr uint8_t VESSEL_CACHE_SIZE = 20;

struct VesselSlotRecord {
  String       vesselName;                // empty = unused entry
  ResourceType types[MAX_SLOTS];
  uint8_t      count = 0;
};

extern VesselSlotRecord vesselCache[VESSEL_CACHE_SIZE];
extern String           currentVesselName;

// Vessel slot cache helpers (AAA_Globals.ino)
void saveVesselSlots(const String &name);
bool recallVesselSlots(const String &name);

// From AAA_Config.ino — bar update hysteresis
extern const float BAR_LEVEL_HYSTERESIS;  // minimum level change fraction to trigger redraw

// PrintState instances for KDC v2 printValue() rendering (ScreenDetail.ino).
// One per detail row (max 6 rows: 3 craft + 3 stage).
extern PrintState psDetailRows[6];
