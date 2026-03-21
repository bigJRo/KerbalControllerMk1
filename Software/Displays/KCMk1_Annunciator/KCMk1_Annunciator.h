#pragma once
/***************************************************************************************
   KCMk1_Annunciator.h -- Shared declarations for Kerbal Controller Mk1 Annunciator
   Included by every .ino tab. Defines types, enums, AppState, C&W bit constants,
   and extern declarations for all globals defined in AAA_Config.ino and AAA_Globals.ino.
****************************************************************************************/

#include <KerbalDisplayCommon.h>
#include <KerbalDisplayAudio.h>
#include <KerbalSimpit.h>


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
  uint16_t   cautionWarningState  = 0x0000;
  uint8_t    vesselSituationState = 0x00;   // display bitmask: bit0=DOCKED..bit6=SPLASH
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

  // Action groups -- gear drives GROUND_PROX C&W; others available for future screens
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
  float apoapsis = 0.0f;

  // Delta-V and burn time
  float stageDV       = 0.0f;
  float stageBurnTime = 0.0f;

  // G-forces
  float gForces = 0.0f;

  // Resources
  float EC       = 0.0f;
  float EC_total = 0.0f;

  // Throttle
  uint8_t throttleCmd = 0;  // 0-100%
};


/***************************************************************************************
   VESSEL SITUATION DISPLAY BITMASK BIT POSITIONS
   state.vesselSituationState is a display bitmask assembled in SimpitHandler from
   Simpit's FLIGHT_STATUS raw situation bits. Named constants here prevent magic
   literals spreading into ScreenMain and CautionWarning.
   Bit 0 (DOCKED) is set/cleared by VESSEL_CHANGE_MESSAGE, not FLIGHT_STATUS.
****************************************************************************************/
static const uint8_t VSIT_DOCKED    = 0;
static const uint8_t VSIT_PRELAUNCH = 1;
static const uint8_t VSIT_FLIGHT    = 2;
static const uint8_t VSIT_SUBORBIT  = 3;
static const uint8_t VSIT_ORBIT     = 4;
static const uint8_t VSIT_ESCAPE    = 5;
static const uint8_t VSIT_SPLASH    = 6;


/***************************************************************************************
   CAUTION & WARNING BIT INDICES
   Defined here (in the header) so masterAlarmMask in AAA_Config.ino can reference
   them directly, keeping the two in guaranteed sync.
   Bit index matches the cautWarn[] array index in ScreenMain.ino (column-major).
****************************************************************************************/
static const uint8_t CW_HIGH_SPACE  = 0;   // Information -- above high space threshold
static const uint8_t CW_LOW_SPACE   = 1;   // Information -- in space, below high space
static const uint8_t CW_FLYING_HIGH = 2;   // Information -- above upper atmosphere
static const uint8_t CW_FLYING_LOW  = 3;   // Information -- airborne, below flyHigh
static const uint8_t CW_ALT         = 4;   // Caution     -- surface alt < 500m, airborne
static const uint8_t CW_DESCENT     = 5;   // Caution     -- descending in non-orbital flight
static const uint8_t CW_GROUND_PROX = 6;   // WARNING     -- <10s to impact, gear up
static const uint8_t CW_MECO        = 7;   // Information -- throttle at 0% (in flight)
static const uint8_t CW_HIGH_G      = 8;   // WARNING     -- gForces > 9g or < -5g
static const uint8_t CW_BUS_VOLTAGE = 9;   // WARNING     -- EC < 10% of capacity
static const uint8_t CW_HIGH_TEMP   = 10;  // WARNING     -- maxTemp or skinTemp > alarm
static const uint8_t CW_LOW_DV      = 11;  // WARNING     -- stageDV < 150m/s or burn < 60s
static const uint8_t CW_WARP        = 12;  // Caution     -- time warp > 1x
static const uint8_t CW_ATMO        = 13;  // Caution     -- vessel in atmosphere
static const uint8_t CW_O2_PRESENT  = 14;  // Information -- atmosphere is breathable
static const uint8_t CW_CONTACT     = 15;  // Information -- landed or splashed



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

// From AAA_Config.ino
extern bool     demoMode;
extern bool     audioEnabled;
extern bool     debugMode;
extern const uint8_t   DISPLAY_ROTATION;
extern const uint16_t  LOW_DV_MECO_HOLDOFF_MS;
extern uint8_t  tempCaution;
extern uint8_t  tempAlarm;
extern uint8_t  commCaution;
extern uint8_t  commAlarm;
extern const uint16_t masterAlarmMask;
extern const float    ALERT_ALT_THRESHOLD;
extern const float    ALERT_VEL_THRESHOLD;

// From AAA_Globals.ino
extern RA8875       infoDisp;
extern TouchResult  lastTouch;
extern KerbalSimpit simpit;
extern AppState     state;
extern AppState     prev;
extern bool         inFlight;
extern bool         inEVA;
extern bool         hasTarget;
extern bool         flightScene;
extern bool         docked;
extern bool         isRecoverable;
extern bool         hasO2;
extern bool         inAtmo;
extern bool         physTW;
extern bool         simpitConnected;
extern bool         idleState;
extern volatile bool i2cProceedReceived;
extern uint8_t      rawSituation;
extern BodyParams   currentBody;
extern ScreenType   activeScreen;
extern ScreenType   prevScreen;
extern uint32_t     lastScreenSwitch;
extern bool         firstPassOnMain;
extern bool         alarmSilenced;

// Screen navigation -- always use this instead of setting activeScreen directly.
// Sets activeScreen, resets prevScreen to screen_COUNT (triggering chrome redraw),
// records lastScreenSwitch timestamp, and calls invalidateAllState().
void switchToScreen(ScreenType s);
