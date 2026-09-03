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
// library configures and drives no pins. Its AUDIO_PIN default is the TONE line on
// pin 29 (→ PAM8302A amp; from KCM_AUDIO_TONE_PIN) and amp-enable TONE_EN on pin 30 —
// neither is touched here since no audio function is called, so no conflict.
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
static const uint8_t SKETCH_VERSION_MINOR = 12;  // 3.12.0: level history trace on the Detail screen
static const uint8_t SKETCH_VERSION_PATCH = 1;   // 3.12.1: trace stays inside its box; game-time scale


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
   RESOURCE PRESENCE
   Whether the current vessel carries each resource. The Simpit plugin sends a
   resource message with total = 0 and available = 0 when the vessel has no such
   resource (it is not in ARP's vessel resource table), and a channel request forces
   that message even when nothing changed -- so after a refresh, a zero total is a
   definitive "not aboard", not a missing answer. Unknown until the first message
   since the last refresh; a channel that never answers (mod not installed) becomes
   absent when the refresh times out.
   An absent resource draws no meter and is inert on the Select screen.
   A plain refresh keeps the last known presence until the answer arrives; only a
   vessel switch or scene entry resets it to unknown.
****************************************************************************************/
enum ResPresence : uint8_t { PRES_UNKNOWN = 0, PRES_PRESENT, PRES_ABSENT };
extern uint8_t resPresence[];              // indexed by ResourceType
bool resAbsent(ResourceType t);            // true only once known absent (never in demo)
void resetResourcePresence();              // all unknown -- on vessel switch / scene entry
void noteResourcePresence(ResourceType t, float available, float total);


/***************************************************************************************
   RESOURCE SUBSYSTEM GROUP
   The Main screen lays meters out in this order, with a bracketed label over each
   run and a divider between runs, the way a cockpit panel clusters its meters by
   system. Ranks follow the Select grid's grouping.
****************************************************************************************/
enum ResGroup : uint8_t {
  GRP_POWER = 0,   // Electric Charge, Stored Charge
  GRP_PROP,        // propellants, native and CRP, plus EVA Propellant and Intake Air
  GRP_NUCLEAR,     // Enriched Uranium, Depleted Fuel
  GRP_MISC,        // Ore, Ablator
  GRP_LIFE,        // TAC-LS resources
  GRP_AGRI,        // Fertilizer
  GRP_COUNT
};


/***************************************************************************************
   RESOURCE LIMIT BANDS
   Caution and alarm fractions a meter paints on its scale and alerts against.
   highIsBad flips the sense for waste-type resources, which alert on filling up.
   enabled=false gives a plain scale with no bands (cargo, intake air).
****************************************************************************************/
struct ResLimits {
  float warn;       // caution fraction
  float alarm;      // alarm fraction
  bool  highIsBad;  // true: alert above the fractions; false: alert below them
  bool  enabled;    // false: no bands, never alerts
};

// Time-remaining tiers, game seconds; warn == 0 means the resource has none.
struct ResTimeLimits {
  float warn;
  float alarm;
};


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
  uint32_t     updatedMs    = 0;      // millis() of the last Simpit message for this slot
  float        bug          = -1.0f;  // pilot-set reserve bug, fraction of capacity; < 0 = none
};


/***************************************************************************************
   SLOT SAMPLE (Sampling.ino)
   Rate and trend estimate for one value stream (a slot's total or its stage).
****************************************************************************************/
struct SlotSample {
  float    trendRef;    // value at the start of the trend window; < 0 = not started
  uint32_t trendRefMs;  // trend window start
  int8_t   trend;       // -1 falling, 0 steady, +1 rising
  float    tteRef;      // value at the start of the rate window; < 0 = not started
  uint32_t tteRefMs;    // rate window start
  float    rate;        // smoothed rate, units per REAL second, negative = depleting
  bool     rateValid;   // at least one full window has been measured
  float    longRef;     // value at the start of the long window; < 0 = not started
  uint32_t longRefMs;   // long window start
  float    longRate;    // rate over the last full long window, units per REAL second
  bool     longValid;   // one full long window has been measured
};


/***************************************************************************************
   MAIN-SCREEN METER TYPES (ScreenMain.ino)
   Defined here rather than in the tab because the Arduino build hoists a prototype
   for every function above the concatenated tabs, so any type that appears in a
   function signature must be visible from this header.
****************************************************************************************/
// Everything that differs between the two meter classes (standard / compact).
struct MeterStyle {
  uint16_t     tapeW;       // thermometer column width, frame included
  uint16_t     secW;        // secondary (stage/total) column width inside the tape
  uint16_t     tickL;       // major tick length right of the tape (minor = half)
  uint16_t     arrowW;      // trend-arrow glyph width
  uint16_t     arrowHalfH;  // trend-arrow half height
  const tFont *labelFont;
  const tFont *percFont;
  const tFont *unitsFont;
  const tFont *groupFont;
};

// Per-meter x geometry. The scale group (band | tape | ticks) is centred in the
// pitch cell.
struct MeterGeom {
  uint16_t px;      // pitch cell left edge
  uint16_t bandX;   // limit-band column left edge
  uint16_t tapeX;   // tape frame left edge
  uint16_t tickX;   // first tick pixel, right of the frame
};

// Per-meter drawn-state cache. One entry per slot; reset on chrome redraw and on
// the TTE toggle. Rates live in Sampling.ino, not here, so a toggle keeps them.
struct MeterCache {
  float    level;       // last drawn total level; -1 = force
  float    mark;        // last drawn stage level; -2 = force (-1 is "no stage column")
  uint8_t  state;       // last drawn alert state; 255 = force
  bool     hasData;     // last drawn capacity-present flag
  bool     awaiting;    // last drawn refresh-pending flag
  bool     timeFlag;    // last state was raised by a time-remaining tier
  float    bug;         // last drawn reserve bug; -2 = force (-1 is "no bug")
  int8_t   trend;       // last drawn trend arrow; 127 = force
  char     counter[8];  // last drawn counter-row string (% or TTE); "\x01" = force
  char     units[12];   // last drawn units string; "\x01" = force
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
extern const bool     DEMO_EVA;          // demo starts on EVA (ring-gauge layout)
extern const uint8_t  DISPLAY_ROTATION;
// Slot count limit — constexpr so it can be used as a compile-time array size.
// There is no floor: with absent resources collapsing, the panel copes with any
// number of meters, and CLEAR could always empty the set anyway.
static constexpr uint8_t MAX_SLOTS          = 16;
static constexpr uint8_t DEFAULT_SLOT_COUNT = 9;
extern const float    RES_WARN_FRAC;     // caution band top (fraction of capacity)
extern const float    RES_ALARM_FRAC;    // alarm band top (fraction of capacity)
extern const float    WASTE_WARN_FRAC;   // waste-type caution: fraction full
extern const float    WASTE_ALARM_FRAC;  // waste-type alarm: fraction full
extern const uint32_t TREND_WINDOW_MS;   // trend arrow sample window
extern const float    TREND_MIN_FRAC;    // trend arrow deadband, fraction of capacity per window
extern const uint32_t TTE_WINDOW_MS;     // time-to-empty rate sample window
extern const float    ALERT_HYST_FRAC;   // hysteresis on the caution/alarm thresholds
extern const float    TIME_WARN_S_EC,  TIME_ALARM_S_EC;
extern const float    TIME_WARN_S_O2,  TIME_ALARM_S_O2;
extern const float    TIME_WARN_S_H2O, TIME_ALARM_S_H2O;
extern const float    TIME_WARN_S_FOOD, TIME_ALARM_S_FOOD;
extern const float    TIME_HYST_FRAC;    // fraction above a time threshold needed to leave its tier
extern const uint32_t ALARM_FLASH_MS;    // how long a new alarm tile flashes in the strip
extern const uint32_t ALARM_FLASH_HALF_MS; // half period of that flash
extern const uint32_t BUG_HOLD_MS;       // hold time to set or clear a bug
extern const uint8_t  BUG_SNAP_PCT;      // a hold places a bug on a multiple of this percent; a drag moves it by 1%
extern const float    BUG_GRAB_TOL;      // a touch this close to an existing bug grabs it
extern const uint16_t BUG_DRAG_MIN_PX;   // travel before a grabbed bug starts to move
extern const uint32_t REFRESH_TIMEOUT_MS; // how long a slot may be "awaiting" after a refresh

// From AAA_Globals.ino
extern KCM_TFT       infoDisp;
extern TouchResult  lastTouch;
extern KerbalSimpit simpit;
extern ScreenType   activeScreen;
extern ScreenType   prevScreen;
extern ResourceSlot slots[];        // active resource slots (MAX_SLOTS entries)
extern uint8_t      slotCount;      // number of currently active slots (4-16)
extern bool         tteMode;        // false = counter row shows %, true = time-to-empty
extern float        warpFactor;     // game seconds per real second (from FLIGHT_STATUS warp index)
extern bool         refreshPending; // a channel refresh has been requested and not all slots answered
extern uint32_t     refreshRequestMs; // millis() of that request
extern bool         flightScene;    // true when KSP is in a flight scene
extern bool         simpitConnected; // true once Simpit handshake succeeds
extern bool         idleState;      // true = show standby when not in flight (set by I2C master)
extern bool         needsMainRedraw; // set by SimpitHandler to request main screen chrome redraw
extern bool         evaActive;      // true = a Kerbal is on EVA (mode applied); drives the EVA bar set
extern bool         layoutRecalled; // true = the current layout came from vessel memory, false = default
extern bool         evaFlag;        // raw latched EVA flag from the last FLIGHT_STATUS_MESSAGE

// Resource type metadata (from Resources.ino)
const char*    resLabel(ResourceType t);
const char*    resFullName(ResourceType t);
uint16_t       resColor(ResourceType t);
ResourceType   resTypeByIndex(uint8_t index);  // 0-based index into selectable types
bool           isEvaResource(ResourceType t);  // true for the fixed EVA bar set (EC/EVA/O2/Food/Water)
ResGroup       resGroup(ResourceType t);       // subsystem group for Main-screen ordering
const char*    resGroupLabel(ResGroup g);      // short group label drawn over a run of meters
ResLimits      resLimits(ResourceType t);      // caution/alarm bands for the meter scale
ResTimeLimits  resTimeLimits(ResourceType t);  // time-remaining tiers, 0 = none
// 0 nominal, 1 caution, 2 alarm, 3 reserve-bug crossed (caution severity, bug colour);
// with hysteresis against the previously drawn state.
uint8_t        alertState(ResourceType t, float level, float bug, uint8_t prev);
static const uint8_t ALERT_NOMINAL = 0, ALERT_CAUTION = 1, ALERT_ALARM = 2, ALERT_BUG = 3;
uint8_t        resOrderIndex(ResourceType t); // canonical position (the Select grid order)
void           clearAllBugs();               // remove every reserve bug on the vessel
void           sortSlotsByGroup();             // stable in-place sort of slots[] by resGroup()

// Sampling (Sampling.ino)
extern SlotSample sampleTot[];   // per-slot total-value sample
extern SlotSample sampleStg[];   // per-slot stage-value sample
void   updateAllSampling();      // once per loop pass
void   resetAllSampling();       // after any slot reorder or warp change
bool   slotAwaiting(uint8_t i);  // slot has not answered the current refresh yet

// Level history (Sampling.ino): every HIST_PERIOD_MS the level of every resource
// aboard is pushed into a ring HIST_LEN deep, keyed by resource type so a layout
// change does not scramble it. The Detail screen draws it as a trace when
// DETAIL_HISTORY is on; set it false and the Detail rows take the full width again.
static constexpr bool     DETAIL_HISTORY = true;
static constexpr uint16_t HIST_LEN       = 120;    // samples kept (10 min at 5 s)
static constexpr uint32_t HIST_PERIOD_MS = 5000;   // real ms between samples
static constexpr uint16_t HIST_NONE      = 0xFFFF; // no data for that resource at that sample
void     resetHistory();                                // vessel switch / scene entry
uint16_t histCount();                                   // samples held, 0..HIST_LEN
uint16_t histLevel(ResourceType t, uint16_t k);         // k = 0 oldest .. histCount()-1 newest; level x1000 or HIST_NONE
float    histGameSecs();                                // game time the held samples span
float    histGameAgo(uint16_t k);                       // game seconds between sample k and the newest
uint32_t histSeq();                                     // bumps on every push or reset
// Time-remaining tiers on top of a level state, with hysteresis against the state the
// caller last used; timeFlag reports the tier did it. Shared by Main, EVA and the summary.
uint8_t applyTimeTiers(ResourceType t, float tteS, uint8_t state, uint8_t prevState, bool prevTime, bool &timeFlag);
// Vessel-wide alert summary for the master: any caution, any alarm, any time tier, and
// the worst resource (RES_NONE when nominal). Screen-independent.
void   alertSummary(bool &caution, bool &alarm, bool &timeTier, ResourceType &worst);
float  sampleTteSeconds(const SlotSample &c, ResourceType t, float cur, float max);
void   fmtTte(float secs, char *buf, size_t n);
void   fmtRate(const SlotSample &c, char *buf, size_t n);

// Simpit (SimpitHandler.ino)
void   requestResourceRefresh(); // ask Simpit to resend every channel; stamps refreshRequestMs

// Screen management
// Always use switchToScreen() to change screens — never set activeScreen directly.
// switchToScreen() sets activeScreen and resets prevScreen to screen_COUNT, which
// triggers the chrome redraw block on the next loop pass. (Call sites clear any
// queued touch themselves; switchToScreen() does not.)
void switchToScreen(ScreenType s);

// Per-tab functions
void processTouchEvents();
void initDemoMode();
bool demoResourceAbsent(ResourceType t);   // demo's scripted "not aboard" phases
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
// EVA layout of the Main screen (ScreenEVA.ino); Main hands over while evaActive.
void   drawStaticEVA(KCM_TFT &tft);
void   updateScreenEVA(KCM_TFT &tft);
int8_t evaHitTest(uint16_t x, uint16_t y, bool &onRing, float &level);
float  evaLevelAt(uint8_t slot, uint16_t x, uint16_t y);
// Level under a touch on the current Main layout (tape row or ring angle).
float  mainLevelAt(uint8_t slot, uint16_t x, uint16_t y);
int8_t mainHitTest(uint16_t x, uint16_t y, bool &onGauge, float &level);
uint16_t dimColor(uint16_t c);   // half brightness of an RGB565 colour
void   fmtUnits(float v, char *buf, size_t n);   // compact units counter
void redrawTteButton(KCM_TFT &tft);
int8_t sidebarHitTest(uint16_t x, uint16_t y);
// Meter under (x,y), or -1. onTape is true for a hit on the tape rows (level is the
// 0..1 scale position of the touch), false for a hit on the label/counter rows.
int8_t meterHitTest(uint16_t x, uint16_t y, bool &onTape, float &level);

// Alert strip + balance (ScreenMainStrip.ino)
enum StripCode : uint8_t { STRIP_NONE = 0, STRIP_CAUTION, STRIP_ALARM, STRIP_BUG };
void   stripReset();                              // forget drawn state (chrome redraw)
void   stripResetAll();                           // ... and the per-slot codes / alarm timers (slot sequence changed)
void   stripSetSlot(uint8_t i, uint8_t code, bool timeFlag);   // per-slot report, every update pass
void   stripUpdate(KCM_TFT &tft);                 // redraw whatever changed
int8_t stripHitTest(uint16_t x, uint16_t y);     // slot whose alert-strip message is under (x,y), or -1
float  meterLevelAtY(uint16_t y);                // 0..1 scale position of a tape row (clamped)
bool   meterBugNear(uint8_t i, float level);     // slot i has a bug within BUG_GRAB_TOL of level
void   setMeterBug(uint8_t i, float level, uint8_t snapPct);  // place slot i's bug on a snapPct grid
void   clearMeterBug(uint8_t i);                 // remove slot i's reserve bug
void drawStaticSelect(KCM_TFT &tft);
void updateScreenSelect(KCM_TFT &tft);
bool handleSelectTouch(uint16_t x, uint16_t y);
void drawStaticDetail(KCM_TFT &tft);
void updateScreenDetail(KCM_TFT &tft);
bool handleDetailTouch(uint16_t x, uint16_t y);
void setDetailSlot(uint8_t i);   // choose which slot the Detail screen opens on


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
   Slot types and reserve bugs are stored — values are always repopulated from Simpit.
****************************************************************************************/
static constexpr uint8_t VESSEL_CACHE_SIZE = 20;
static constexpr uint8_t VESSEL_NAME_LEN   = 40;   // stored name, including the terminator

// One remembered vessel. Fixed-width so the record is also the EEPROM image (see
// Persist.ino); a name longer than the field is truncated, which is how it is keyed.
struct VesselSlotRecord {
  char         name[VESSEL_NAME_LEN];     // "" = unused entry
  ResourceType types[MAX_SLOTS];
  float        bugs[MAX_SLOTS];           // reserve bug per slot, < 0 = none
  uint8_t      count = 0;
};

// The cache is kept in recency order: index 0 is the vessel most recently saved or
// recalled, so when it is full the last entry is the one to give up.
extern VesselSlotRecord vesselCache[VESSEL_CACHE_SIZE];
extern String           currentVesselName;

// Vessel slot cache helpers (AAA_Globals.ino)
void saveVesselSlots(const String &name);   // an empty set forgets the vessel instead
bool recallVesselSlots(const String &name);
void forgetVesselSlots(const String &name);
void clearVesselCache();

// The pilot-set default layout: what a vessel not in memory starts with, set from the
// Select screen's DFLT key and persisted with the cache. count 0 = none set, in which
// case the SPCT preset is the default. (AAA_Globals.ino)
extern VesselSlotRecord defaultLayout;
void setDefaultLayout();        // the current slots (types and bugs) become the default
void clearDefaultLayout();      // back to the SPCT preset
bool defaultLayoutSet();
bool layoutIsDefault();         // current slot types match the effective default, in order

// Mission presets (Resources.ino): the Select screen's keys, and PRESETS[0] (SPCT) is
// the built-in default layout.
struct PresetGroup {
  const char*  label;
  ResourceType types[MAX_SLOTS];
  uint8_t      count;
};
static constexpr uint8_t PRESET_COUNT = 6;
extern const PresetGroup PRESETS[PRESET_COUNT];

// Persistence (Persist.ino): the vessel cache and the TTE toggle live in the Teensy's
// emulated EEPROM. persistLoad() at boot, persistService() every loop pass (stores a
// settled change), persistStoreNow() at the moments a hitch does not matter.
void    persistLoad();
void    persistService();
void    persistStoreNow();
uint8_t persistVesselCount();
extern const uint32_t PERSIST_SETTLE_MS;
extern const bool     PERSIST_WIPE;
extern const uint32_t MEM_CLEAR_HOLD_MS;
extern const uint32_t TTE_LONG_WINDOW_MS;

// Select screen key hold (ScreenSelect.ino, driven by TouchEvents.ino): holding CLEAR
// for MEM_CLEAR_HOLD_MS forgets every vessel. Target says whether a touch-down is on
// that key; progress draws the countdown; fire and cancel end it either way.
bool selectHoldTarget(uint16_t x, uint16_t y);
void selectHoldProgress(uint32_t heldMs);
void selectHoldFire();
void selectHoldCancel();

// From AAA_Config.ino — bar update hysteresis
extern const float BAR_LEVEL_HYSTERESIS;  // minimum level change fraction to trigger redraw

// PrintState instances for KDC v2 printValue() rendering (ScreenDetail.ino).
// One per detail row (max 10 rows: 5 craft + 5 stage).
static constexpr uint8_t DET_MAX_ROWS = 10;
extern PrintState psDetailRows[DET_MAX_ROWS];
