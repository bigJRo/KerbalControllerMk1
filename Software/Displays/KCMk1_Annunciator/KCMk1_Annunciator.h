#pragma once
/***************************************************************************************
   KCMk1_Annunciator.h -- Shared declarations for Kerbal Controller Mk1 Annunciator
   Included by every .ino tab. Defines types, enums, AppState, C&W bit constants,
   and extern declarations for all globals defined in AAA_Config.ino and AAA_Globals.ino.
****************************************************************************************/

// Requires KerbalDisplayCommon >= 2.1.0
#include <KerbalDisplayCommon.h>
#include <KerbalDisplayAudio.h>
#include <KerbalSimpit.h>
#include <KCMk1_SystemConfig.h>   // shared hardware/threshold constants (KCMk1_SystemConfig library)


/***************************************************************************************
   CONTROL MODE ENUM
   Panel-specific -- not part of the KSP API.
****************************************************************************************/
enum CtrlMode : uint8_t {
  ctrl_Rover      = 0,
  ctrl_Plane      = 1,
  ctrl_Spacecraft = 2
};


/***************************************************************************************
   APPLICATION STATE STRUCT
   All telemetry-driven display values.
   Two instances declared in Globals.ino: state (current) and prev (last drawn).
****************************************************************************************/
struct AppState {
  bool       masterAlarmOn        = false;
  uint32_t   cautionWarningState  = 0x00000000;  // expanded to 32-bit for 25 C&W bits
  uint8_t    vesselSituationState = 0x00;         // display bitmask: bit0=DOCKED..bit7=LANDED
  CtrlMode   vehCtrlMode          = ctrl_Spacecraft;
  VesselType vesselType           = type_Ship;
  String     vesselName           = "TEST CRAFT NAME";  // note: String heap -- low risk on Teensy 4.0
  uint8_t    maxTemp    = 0;
  uint8_t    crewCount  = 0;
  uint8_t    twIndex    = 0;
  uint8_t    commNet    = 0;
  uint8_t    stage      = 0;
  uint8_t    skinTemp   = 100;
  uint8_t    ctrlGrp    = 1;
  String     gameSOI    = "";                           // note: String heap -- low risk on Teensy 4.0

  // Action groups
  bool gear_on   = false;
  bool brakes_on = false;
  bool lights_on = false;
  bool RCS_on    = false;
  bool SAS_on    = false;
  bool abort_on  = false;

  // Altitude (metres)
  float alt_sl   = 0.0f;
  float alt_surf = 0.0f;

  // Velocity (m/s)
  float vel_surf = 0.0f;
  float vel_vert = 0.0f;

  // Apsides (metres)
  float apoapsis  = 0.0f;
  float periapsis = 0.0f;

  // Delta-V and burn time
  float stageDV       = 0.0f;
  float stageBurnTime = 0.0f;

  // G-forces
  float gForces = 0.0f;

  // Resources -- electric charge
  float EC       = 0.0f;
  float EC_total = 0.0f;

  // Resources -- propellant (current stage)
  float LF_stage     = 0.0f;
  float LF_stage_tot = 0.0f;
  float OX_stage     = 0.0f;
  float OX_stage_tot = 0.0f;
  float SF_stage     = 0.0f;   // solid fuel -- current stage (for SRB ACTIVE)
  float SF_stage_tot = 0.0f;

  // Resources -- MonoPropellant (total vessel)
  float mono     = 0.0f;
  float mono_tot = 0.0f;

  // Resources -- TAC-LS (total vessel)
  // Consumables: available units remaining and total capacity
  float tacFood        = 0.0f;
  float tacFood_tot    = 0.0f;
  float tacWater       = 0.0f;
  float tacWater_tot   = 0.0f;
  float tacOxygen      = 0.0f;
  float tacOxygen_tot  = 0.0f;
  // Waste resources: current fill level and total capacity
  float tacCO2       = 0.0f;
  float tacCO2_tot   = 0.0f;
  float tacWaste     = 0.0f;
  float tacWaste_tot = 0.0f;
  float tacWW        = 0.0f;   // WasteWater (LiquidWaste in Simpit)
  float tacWW_tot    = 0.0f;

  // Atmosphere
  float atmoPressure = 0.0f;  // kPa -- for CHUTE ENV and HIGH Q
  float atmoTemp     = 0.0f;  // K   -- available for future use

  // Throttle
  uint8_t throttleCmd = 0;  // 0-100%
};


/***************************************************************************************
   VESSEL SITUATION DISPLAY BITMASK BIT POSITIONS
   state.vesselSituationState is a display bitmask assembled in SimpitHandler from
   Simpit's FLIGHT_STATUS raw situation bits. Named constants here prevent magic
   literals spreading into ScreenMain and CautionWarning.
   Bit 0 (DOCKED) is set/cleared by VESSEL_CHANGE_MESSAGE, not FLIGHT_STATUS.
   Bit 7 (LANDED) is set from sit_Landed in rawSituation.
****************************************************************************************/
static const uint8_t VSIT_DOCKED    = 0;
static const uint8_t VSIT_PRELAUNCH = 1;
static const uint8_t VSIT_FLIGHT    = 2;
static const uint8_t VSIT_SUBORBIT  = 3;
static const uint8_t VSIT_ORBIT     = 4;
static const uint8_t VSIT_ESCAPE    = 5;
static const uint8_t VSIT_SPLASH    = 6;
static const uint8_t VSIT_LANDED    = 7;


/***************************************************************************************
   CAUTION & WARNING BIT INDICES
   Defined here (in the header) so masterAlarmMask in AAA_Config.ino can reference
   them directly, keeping the two in guaranteed sync.
   Bit index matches the cautWarn[] array index in ScreenMain.ino (row-major,
   row 0 left to right, then row 1, etc.).

   Layout (5 columns x 5 rows):
     Row 0: LOW_DV    HIGH_G    HIGH_TEMP  BUS_VOLTAGE  ABORT
     Row 1: GRND_PROX PE_LOW    PROP_LOW   LIFE_SUPPORT O2_PRESENT
     Row 2: IMPACT    ALT       DESCENT    GEAR_UP      ATMO
     Row 3: RCS_LOW   PROP_IMBAL COMM_LOST  Ap_LOW       HIGH_Q
     Row 4: ORBIT_STB ELEC_GEN  CHUTE_ENV  SRB_ACTIVE   EVA_ACTIVE

   WARNING (red, master alarm):
     CW_LOW_DV, CW_HIGH_G, CW_HIGH_TEMP, CW_BUS_VOLTAGE, CW_ABORT,
     CW_GROUND_PROX, CW_PE_LOW (red tier), CW_PROP_LOW (red tier),
     CW_LIFE_SUPPORT (red tier)

   CAUTION (yellow):
     CW_IMPACT_IMM, CW_ALT, CW_DESCENT, CW_GEAR_UP, CW_ATMO,
     CW_RCS_LOW, CW_PROP_IMBAL, CW_COMM_LOST, CW_Ap_LOW, CW_HIGH_Q,
     CW_PE_LOW (yellow tier), CW_PROP_LOW (yellow tier),
     CW_LIFE_SUPPORT (yellow tier)

   STATE / POSITIVE:
     CW_ORBIT_STABLE (green), CW_ELEC_GEN (green),
     CW_CHUTE_ENV (state-dependent, see ChuteEnvState),
     CW_SRB_ACTIVE (orange), CW_EVA_ACTIVE (orange), CW_O2_PRESENT (blue)
****************************************************************************************/

// Row 0
static const uint8_t CW_LOW_DV       =  0;  // WARNING     -- stageDV < threshold or burn < threshold
static const uint8_t CW_HIGH_G       =  1;  // WARNING     -- gForces > +9g or < -5g
static const uint8_t CW_HIGH_TEMP    =  2;  // WARNING     -- part temp exceeds alarm threshold
static const uint8_t CW_BUS_VOLTAGE  =  3;  // WARNING     -- EC < 5% of capacity
static const uint8_t CW_ABORT        =  4;  // WARNING     -- abort action is active

// Row 1
static const uint8_t CW_GROUND_PROX  =  5;  // WARNING     -- airborne, descending, <10s to impact
static const uint8_t CW_PE_LOW       =  6;  // WARNING/CAU -- periapsis low (red=reentry, yellow=in atmo)
static const uint8_t CW_PROP_LOW     =  7;  // WARNING/CAU -- stage LF/OX low (red=5%, yellow=20%)
static const uint8_t CW_LIFE_SUPPORT =  8;  // WARNING/CAU -- TAC-LS resource critical (red/yellow tiers)
static const uint8_t CW_O2_PRESENT   =  9;  // Information -- atmosphere contains breathable oxygen

// Row 2
static const uint8_t CW_IMPACT_IMM   = 10;  // Caution     -- airborne, descending, <60s to terrain impact
static const uint8_t CW_ALT          = 11;  // Caution     -- surface altitude < 200m while airborne
static const uint8_t CW_DESCENT      = 12;  // Caution     -- vessel airborne and descending
static const uint8_t CW_GEAR_UP      = 13;  // Caution     -- gear retracted below 200m while descending
static const uint8_t CW_ATMO         = 14;  // Caution     -- vessel inside atmosphere

// Row 3
static const uint8_t CW_RCS_LOW      = 15;  // Caution     -- total vessel MonoProp < 15%
static const uint8_t CW_PROP_IMBAL   = 16;  // Caution     -- stage LF/OX ratio outside expected range
static const uint8_t CW_COMM_LOST    = 17;  // Caution     -- CommNet signal lost
static const uint8_t CW_Ap_LOW       = 18;  // Caution     -- apoapsis inside atmosphere (SUB-ORB or ORBIT)
static const uint8_t CW_HIGH_Q       = 19;  // Caution     -- dynamic pressure exceeds body threshold

// Row 4
static const uint8_t CW_ORBIT_STABLE = 20;  // Positive    -- Pe and Ap both between atmo/minSafe and soiAlt, sit=ORBIT
static const uint8_t CW_ELEC_GEN     = 21;  // Positive    -- EC actively increasing
static const uint8_t CW_CHUTE_ENV    = 22;  // State       -- chute deployment envelope (see ChuteEnvState)
static const uint8_t CW_SRB_ACTIVE   = 23;  // State       -- solid fuel stage burning
static const uint8_t CW_EVA_ACTIVE   = 24;  // State       -- Kerbal is on EVA

// Total C&W button count
static const uint8_t CW_COUNT        = 25;


/***************************************************************************************
   CHUTE ENVELOPE STATE
   CW_CHUTE_ENV uses four states rather than standard on/off.
   The button color is driven by this enum, evaluated in CautionWarning.ino and
   applied in ScreenMain.ino via a separate chuteEnvState variable.
   OFF    = not in atmosphere
   RED    = in atmosphere, above safe deployment speed for any chute
   YELLOW = within drogue safe envelope but not main chute envelope
   GREEN  = within main chute safe deployment envelope
****************************************************************************************/
enum ChuteEnvState : uint8_t {
  chute_Off    = 0,
  chute_Red    = 1,
  chute_Yellow = 2,
  chute_Green  = 3
};


/***************************************************************************************
   SKETCH VERSION
   Follows semantic versioning: MAJOR.MINOR.PATCH
     MAJOR -- incompatible structural changes
     MINOR -- new features or screens added
     PATCH -- bug fixes, threshold tuning, comment/style changes
   This sketch requires KerbalDisplayCommon >= 2.1.0
****************************************************************************************/
static const uint8_t SKETCH_VERSION_MAJOR = 2;
static const uint8_t SKETCH_VERSION_MINOR = 1;
static const uint8_t SKETCH_VERSION_PATCH = 0;


/***************************************************************************************
   SCREEN TYPE ENUM
   Identifies which screen is currently active.
   screen_COUNT is used as a sentinel value for prevScreen to force a chrome redraw
   on the first loop pass after a screen transition.
   Always use switchToScreen() to change activeScreen -- never set it directly.
****************************************************************************************/
enum ScreenType : uint8_t {
  screen_Standby = 0,
  screen_Main    = 1,
  screen_SOI     = 2,
  screen_COUNT   = 3    // sentinel -- not a real screen
};


/***************************************************************************************
   EXTERN DECLARATIONS
   Variables are defined in AAA_Config.ino and AAA_Globals.ino.
   Declaring them here makes them visible to all tabs via the shared include.
****************************************************************************************/

// From AAA_Config.ino -- operating modes
extern bool           demoMode;
extern bool           audioEnabled;
extern bool           debugMode;
extern bool           standaloneMode;  // bypass boot/master handshake for bench testing
extern bool           standaloneTest;  // serial-driven test mode
extern const uint8_t  DISPLAY_ROTATION;
extern const uint16_t LOW_DV_MECO_HOLDOFF_MS;
extern uint8_t        tempAlarm;
extern const uint32_t masterAlarmMask;       // expanded to 32-bit
extern const float    ALERT_ALT_THRESHOLD;
extern const float    ALERT_VEL_THRESHOLD;

// From AAA_Config.ino -- C&W thresholds
extern const float  CW_ALT_THRESHOLD_M;      // CW_ALT: surface alt below this (m)
extern const float  CW_GEAR_UP_THRESHOLD_M;  // CW_GEAR_UP: surface alt below this (m)
extern const float  CW_GROUND_PROX_S;        // CW_GROUND_PROX: time to impact (s)
extern const float  CW_IMPACT_IMM_S;         // CW_IMPACT_IMM: time to impact upper bound (s)
extern const float  CW_HIGH_G_ALARM;         // CW_HIGH_G: positive G threshold
extern const float  CW_HIGH_G_WARN;          // CW_HIGH_G: negative G threshold
extern const float  CW_EC_LOW_FRAC;          // CW_BUS_VOLTAGE: EC fraction (0.05 = 5%)
extern const float  CW_LOW_DV_MS;            // CW_LOW_DV: stage delta-v threshold (m/s)
extern const float  CW_LOW_BURN_S;           // CW_LOW_DV: stage burn time threshold (s)
extern const float  CW_RCS_LOW_FRAC;         // CW_RCS_LOW: MonoProp fraction (0.15 = 15%)
extern const float  CW_PROP_LOW_WARN_FRAC;   // CW_PROP_LOW: yellow tier fraction (0.20)
extern const float  CW_PROP_LOW_ALARM_FRAC;  // CW_PROP_LOW: red tier fraction (0.05)
extern const float  CW_PROP_IMBAL_TOL;       // CW_PROP_IMBAL: ratio deviation tolerance (0.10)
extern const float  CW_PROP_NOMINAL_RATIO;   // CW_PROP_IMBAL: expected LF/OX ratio (0.818)
extern const float  CW_CHUTE_MAIN_MAX_SPEED; // CHUTE_ENV: max speed for main chute deploy (m/s)
extern const float  CW_CHUTE_DROGUE_MAX_SPEED; // CHUTE_ENV: max speed for drogue deploy (m/s)

// From AAA_Config.ino -- TAC-LS consumption rates (Earth seconds per Kerbal)
extern const double TACLS_FOOD_RATE;
extern const double TACLS_WATER_RATE;
extern const double TACLS_OXYGEN_RATE;
extern const double TACLS_CO2_RATE;
extern const double TACLS_WASTE_RATE;
extern const double TACLS_WASTEWATER_RATE;

// From AAA_Config.ino -- TAC-LS warning thresholds
extern const float  TACLS_FOOD_WARN_S;      // yellow: 72 hours
extern const float  TACLS_FOOD_ALARM_S;     // red:    24 hours
extern const float  TACLS_WATER_WARN_S;     // yellow: 12 hours
extern const float  TACLS_WATER_ALARM_S;    // red:     4 hours
extern const float  TACLS_OXYGEN_WARN_S;    // yellow: 30 minutes
extern const float  TACLS_OXYGEN_ALARM_S;   // red:    10 minutes
extern const float  TACLS_WASTE_WARN_FRAC;  // yellow: waste capacity 80% full
extern const float  TACLS_WASTE_ALARM_FRAC; // red:    waste capacity 95% full

// From AAA_Globals.ino
extern RA8875        infoDisp;
extern TouchResult   lastTouch;
extern KerbalSimpit  simpit;
extern AppState      state;
extern AppState      prev;
extern bool          inFlight;
extern bool          inEVA;
extern bool          hasTarget;
extern bool          flightScene;
extern bool          docked;
extern bool          isRecoverable;
extern bool          hasO2;
extern bool          inAtmo;
extern bool          physTW;
extern bool          simpitConnected;
extern bool          idleState;
extern volatile bool i2cProceedReceived;
extern uint8_t       rawSituation;
extern BodyParams    currentBody;
extern ScreenType    activeScreen;
extern ScreenType    prevScreen;
extern uint32_t      lastScreenSwitch;
extern bool          firstPassOnMain;
extern bool          alarmSilenced;
extern ChuteEnvState chuteEnvState;      // current chute envelope state
extern ChuteEnvState prevChuteEnvState;  // previous state for dirty tracking

// From Audio.ino -- master alarm condition tracking
// alarmActiveMask expanded to uint16_t to accommodate 9 alarm conditions
extern const uint16_t ALARM_GROUND_PROX;
extern const uint16_t ALARM_HIGH_G;
extern const uint16_t ALARM_BUS_VOLTAGE;
extern const uint16_t ALARM_HIGH_TEMP;
extern const uint16_t ALARM_LOW_DV;
extern const uint16_t ALARM_ABORT;
extern const uint16_t ALARM_PE_LOW;
extern const uint16_t ALARM_PROP_LOW;
extern const uint16_t ALARM_LIFE_SUPPORT;
extern uint16_t        alarmActiveMask;
void updateAlarmMask(uint16_t condBit, bool on);

// PrintState instances for flicker-free printDisp/printValue rendering (KDC v2 API).
// One per logical display slot that uses printDisp() or printValue().
// Defined in ScreenMain.ino and ScreenSOI.ino.
extern PrintState psSOILabel;
extern PrintState psSOIRows[];

// I2C slave interface (I2CSlave.ino)
void setupI2CSlave();
void buildI2CPacketAndAssert();
void updateI2CState();

// Screen navigation -- always use this instead of setting activeScreen directly.
void switchToScreen(ScreenType s);

// Test mode (TestMode.ino) -- serial-driven logic and display test framework.
void initTestMode();
void runTestMode();
extern uint16_t testPsForceOn;  // force-on mask for panel status buttons during display walk-through
void resetSitAndPanelState();   // force full redraw of situation column and panel status strip
void forceContactState(bool newContact); // force CONTACT dirty tracker for walk-through
void forceDockState(bool isDocked);      // force DOCK dirty tracker for walk-through
