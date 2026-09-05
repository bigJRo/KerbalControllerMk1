// stub harness: extra globals referenced by pre-existing, mid-integration tabs
#include <Arduino.h>
#include <Wire.h>
#include <master_missing.h>
/********************************************************************************************************************************
  Main Application for Kerbal Controller Mk1 

  Adafruit reference examples code written by Limor Fried/Ladyada for Adafruit Industries.
  Also reference UntitledSpaceCraft code written by CodapopKSP (https://github.com/CodapopKSP/UntitledSpaceCraft)

  Licensed under the GNU General Public License v3.0 (GPL-3.0).

  Final code written by Jason Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <Wire.h>                      // I2C Wire Library
#include <Bounce.h>                    // Button Debounce Library
#include <KerbalSimpit.h>              // Kerbal Simpit Plugin for game interfaces
#include <KerbalSimpitMessageTypes.h>  // Kerbal Simpit message types
#include <PayloadStructs.h>            // Kerbal Simpit payload structures definitions
#include <Adafruit_EMC2101.h>          // Library to support Adafruit EMC2101 Fan Controller
#include <Adafruit_INA219.h>           // Add support for INA219 current sensor
#include <Watchdog_t4.h>               // Inlcude Watchdog Timer library
extern "C" void usb_init(void);

#include "module_packets.h"               // Per-type I2C packet sizing + universal header helpers
#include "module_variables.h"             // Module Register Definitions
#include "attitude_controller.h"          // Shared attitude PID (ascent + hold-mode autopilots)
#include "ascent_autopilot.h"             // Launch-to-orbit ascent autopilot
#include "hold_autopilot.h"               // Aircraft / rover hold-mode autopilot
#include "burn_autopilot.h"               // Orbital: node / apsis / plane-change burns, approach-rate hold
#include "landing_autopilot.h"            // Landing: descent-rate hold, hover, suicide burn, re-entry attitude
#include "control_links.h"                // Throttle Module, rotation joystick and Info Display 2 links
#include "custom_action_grp_def.h"  // Custom Action Group Definitions
#include "keyboard_def.h"           // Keyboard Code Definitions
#include "body_params.h"            // Shared celestial-body table (ascent autopilot)


/***************************************************************************************
   I2C Address Definitions
****************************************************************************************/
#define ANNUN_MC 0x10
#define RES_MC 0x11   // Resource Display slave (KCM_I2C_ADDR_RESDISP)
#define INFO_MC 0x12  // Info Display 1 slave  (KCM_I2C_ADDR_INFODISP)
#define INFO2_MC 0x13 // Info Display 2 slave  (KCM_I2C_ADDR_INFODISP_2) — autopilot console command source

// I2C module addresses — Module UI Reference / I2C Protocol Specification v2.10 registry.
#define UI_MOD 0x20           // UI Control          (KMC_TYPE_UI_CONTROL)
#define Function_MOD 0x21     // Function Control    (KMC_TYPE_FUNCTION_CONTROL, 24-input)
#define ActionCtrl_MOD 0x22   // Action Control      (KMC_TYPE_ACTION_CONTROL)
#define StabCtrl_MOD 0x23     // Stability Control   (KMC_TYPE_STABILITY_CONTROL)
#define VehCtrl_MOD 0x24      // Vehicle Control     (KMC_TYPE_VEHICLE_CONTROL, 24-input)
#define Time_MOD 0x25         // Time Control        (KMC_TYPE_TIME_CONTROL)
#define AUX_MOD 0x26          // Auxiliary Control   (KMC_TYPE_AUX_CTRL)
// 0x27 reserved
#define Rotation_MOD 0x28     // Joystick Rotation   (KMC_TYPE_JOYSTICK_ROTATION)
#define Translation_MOD 0x29  // Joystick Translation(KMC_TYPE_JOYSTICK_TRANS)
#define GPWS_MOD 0x2A         // GPWS Input Panel    (KMC_TYPE_GPWS_INPUT)
#define PreWarp_MOD 0x2B      // Pre-Warp Time       (KMC_TYPE_PRE_WARP_TIME)
#define Throttle_MOD 0x2C     // Throttle Module     (KMC_TYPE_THROTTLE)
#define DualEnc_MOD 0x2D      // Dual Encoder        (KMC_TYPE_DUAL_ENCODER)
// 0x2E retired — was Switch Panel (superseded by Switch Groups 1/2)

#define INA219_ADDR 0x44
#define EMC2101_ADDR 0x4C


/***************************************************************************************
   Teensy Pin Definitions
****************************************************************************************/
#define ALIVE 2     // Main microcontroller Status
#define MC01_INT 3  // MC01 Interrupt (Annunciator MC)
#define MC02_INT 4  // MC02 Interrupt (Information MC)
#define MC03_INT 5  // MC03 Interrupt (Resource MC)

#define PanelCtrl_INT 25    // Panel Control Interrupt
#define UI_INT 26           // UI Panel Interrupt
#define StabCtrl_INT 27     // Stability Control Panel Interrupt
#define Function_INT 28     // Function Control Panel Interrupt
#define EVA_INT 29          // EVA Panel Interrupt
#define Staging_INT 30      // Staging Interrupt
#define Translation_INT 31  // Translation Module Interrupt
#define Throttle_INT 32     // Throttle Module Interrupt
#define Rotation_INT 33     // Rotation Module Interrupt
#define Switch_INT 34       // Switch Module Interrupt
#define VehCtrl_INT 35      // Vehicle Control Panel Interrupt
#define ActCtrl_INT 36      // Action Control Panel Interrupt
#define Abort_INT 37        // Abort Interrupt
#define GPWS_INT 38         // GPWS Panel Interrupt

#define OFFBOARD_CONNECT 39  // Offboard Connect
#define INTERBOARD_06 40     // Inter-Board Connec 06
#define INTERBOARD_05 41     // Inter-Board Connec 05
#define INTERBOARD_04 13     // Inter-Board Connec 04
#define INTERBOARD_03 14     // Inter-Board Connec 03
#define INTERBOARD_02 15     // Inter-Board Connec 02
#define INTERBOARD_01 20     // Inter-Board Connec 01

#define EXT_SCL 16  // Hardware Accelerated i2c Serial Clock (External Connection)
#define EXT_SDA 17  // Hardware Accelerated i2c Serial Data (External Connection)
#define SDA 18      // Hardware Accelerated i2c Serial Data
#define SCL 19      // Hardware Accelerated i2c Serial Clock


/***************************************************************************************
   Declare Spercific Library Objects
****************************************************************************************/
KerbalSimpit mySimpit(SerialUSB1);             // Declare a KerbalSimpit object that will communicate using the "Serial" device
Bounce stageButton = Bounce(Staging_INT, 10);  // 10 ms debounce
Bounce abortButton = Bounce(Abort_INT, 10);    // 10 ms debounce
Adafruit_EMC2101 fan;                          // Object to support Adafruit EMC2101 Fan Controller
Adafruit_INA219 ina219(INA219_ADDR);           // Object to support Voltage/Current Measurements
WDT_T4<WDT1> wdt;                              // Watchdog timer type 1 object


/***************************************************************************************
  Fan Setup Variables
****************************************************************************************/
float tempC;
float dutyCycle;
float rpm;


/***************************************************************************************
   Define custom action group parameters
****************************************************************************************/
// Active control group (1..6). Per-group AGX stride is CTRL_GRP_STRIDE (40),
// defined in custom_action_grp_def.h. ctrlGrpAdd retained as an alias.
const uint8_t ctrlGrpAdd = CTRL_GRP_STRIDE;

// Final (group-offset-applied) CAG numbers issued by the commands. One per
// base in custom_action_grp_def.h. Recomputed by setActionGroups() whenever
// the active control group changes.
uint8_t ag1, ag2, ag3, ag4, ag5, ag6, ag7, ag8, ag9, ag10, ag11, ag12;
uint8_t antenna;
uint8_t fuel_cell;
uint8_t solar_array;
uint8_t cargo_door;
uint8_t radiator;
uint8_t ladder;
uint8_t heat_shield_deploy;
uint8_t heat_shield_release;
uint8_t parachute;            // main chute deploy
uint8_t main_chute_cut;
uint8_t drogue;               // drogue deploy
uint8_t drogue_cut;
uint8_t les;
uint8_t fairing;
uint8_t engineMode;
uint8_t collectSci;
uint8_t engine1;
uint8_t science1;
uint8_t engine2;
uint8_t science2;
uint8_t intake;
uint8_t lock_surfaces;
uint8_t cp_primary;
uint8_t cp_alternate;
uint8_t cp_docking;
uint8_t airbrake;
uint8_t rw_disable;


/***************************************************************************************
   Global Variable Definitions
****************************************************************************************/
String lastAction;  // Storage location for last value in watchdog check
long runtime_start = millis();
float alt_surf;

/***************************************************************************************
   Panel Control Boolean Definitions
****************************************************************************************/
bool demo = false;          // use without needing to be connected to simpit or the MST Arduino
bool debug = false;         // debug sends data to the serial monitor
bool throttleEn = false;    // Throttle Input is enabled
bool audioEn = false;       // indicates audio mode enables.
bool idleMode = false;      // controls whether the idle screen is set
bool mstrMCActive = false;  //active when mstrMCActive is on
bool trimMode = false;      //used to activate trim mode for rotation stick
bool mapRevMode = false;    //used to properly cycle key-pressed when the request is cycle map backward TODO: Need to check need for this
bool camCtrlSet = false;    //used to activate the camera control mode for the translation joystick


/***************************************************************************************
Arduino Setup Function
****************************************************************************************/
void setup() {
  /********************************************************
    Start Serial Interface
  *********************************************************/
  Serial.begin(115200);
  SerialUSB1.begin(115200);

  Serial.println("------------------------------------------------------------------");
  Serial.println("Console initialization...");
  Serial.println("------------------------------------------------------------------");
  Serial.println("Serial communucation established");


  /********************************************************
    Set Pin Modes for necessary inputs/outputs
  *********************************************************/
  Serial.print("Setup pin functions...");
  pinMode(Staging_INT, INPUT);
  pinMode(Abort_INT, INPUT);


  Serial.println("COMPLETE");

  /********************************************************
    I2C bus + module / display links
  *********************************************************/
  Wire.begin();
  Wire.setClock(400000);
  thrInit();   // Throttle Module lever link
  rotInit();   // Rotation joystick link

  /********************************************************
    Initialise the ascent autopilot (loads default config;
    edit via apGetConfig()/apSetTargets() and engage with apArm())
  *********************************************************/
  apInit();

  /********************************************************
    Initialise the hold-mode autopilot (aircraft / rover hold
    modes, engaged from the Info Display 2 consoles) and the
    console link that polls the display and pushes status.
  *********************************************************/
  hpInit();
  arbInit();   // attitude / throttle ownership across the autopilot modules
  bpInit();    // burn autopilot (ORBITAL AUTOPILOT console)
  lpInit();    // landing autopilot (LANDING AUTOPILOT console)
  idlInit();
}


/***************************************************************************************
Arduino Loop Function
****************************************************************************************/
void loop() {

  /********************************************************
    Process Staging Button
  *********************************************************/
  if (stageButton.update()) {
    if (stageButton.fallingEdge()) {
      mySimpit.activateAction(STAGE_ACTION);
    }
  }

  /********************************************************
    Process Abort Button
  *********************************************************/
  if (abortButton.update()) {
    if (abortButton.fallingEdge()) {
      mySimpit.activateAction(ABORT_ACTION);
      apDisarm();  // an abort always drops the autopilot back to manual control
      hpDisconnectAll(HP_REASON_PILOT);
    }
  }

  /********************************************************
    Ascent Autopilot
      - apSerialConsole() lets you ARM/DISARM and set ALT/INC/LOFT
        over the primary Serial link for bench testing.
      - apUpdate() runs the guidance loop and emits control to KSP
        only while the autopilot is armed; it is a no-op otherwise.
  *********************************************************/
  thrService();       // Throttle Module: poll lever, forward pilot throttle, drive the lever
  apSerialConsole();  // bench console (ARM/DISARM/ALT/... and HP <cmd> for the hold autopilot)
  apUpdate();
  hpUpdate();         // hold-mode autopilot loops (no-op unless a mode is engaged)
  bpUpdate();         // burn executor / approach hold
  lpUpdate();         // landing modes
  rotService();       // joystick poll + merged rotation send (silent while the ascent AP is armed)
  idlService();       // Info Display 2 console: command poll / ACK / status push
}

// ---- generated prototypes ----
void arbInit();
static void arbDropAttitude(uint8_t owner);
static void arbDropThrottle(uint8_t owner);
void arbTakeAttitude(uint8_t owner);
void arbTakeThrottle(uint8_t owner);
void arbReleaseAttitude(uint8_t owner);
void arbReleaseThrottle(uint8_t owner);
uint8_t arbAttitudeOwner();
uint8_t arbThrottleOwner();
bool arbCanWarp(uint8_t owner);
void arbAllOff();
static inline bool apBodyKnown();
static float apBodyDefaultOrbit();
static float apBodyMinSafeOrbit();
static inline float apClampf(float v, float lo, float hi);
static inline float apDeg2Rad(float d);
static inline float apRad2Deg(float r);
static float apWrap180(float a);
static float apWrap360(float a);
static float apLaunchAzimuth();
static float apEffectiveTurnEnd();
static float apScheduledPitch();
static float apLimitG(float thr);
static float apManagedThrottle();
static void apSendThrottle(float f);
static void apSteer(float cmdPitch, float cmdHeading, float dt);
static void apSteerPrograde(float dt);
static bool apActiveSteering();
static void apReconcileSAS();
static void apMaybeStage();
AscentConfig apDefaultConfig();
void apInit();
void apSetConfig(const AscentConfig &cfg);
void apSetTargets(float apoapsisM, float inclinationDeg, float loft);
const char *apCurrentBody();
void apArm();
void apDisarm();
bool apIsArmed();
void apArbiterDrop();
AscentPhase apGetPhase();
const char *apPhaseName(AscentPhase phase);
AscentStatus apGetStatus();
bool apSetTargetAltitude(float meters);
bool apSetTargetInclination(float deg);
bool apSetLaunchSoutherly(bool southerly);
bool apSetLoft(float exponent);
bool apSetRoll(bool enabled, float deg);
bool apSetMaxG(float g);
void apUpdate();
void apSerialConsole();
static inline void apStamp();
void apIngestFlightStatus(bool inFlight);
void apIngestAltitude(float sealevel, float surface);
void apIngestVelocity(float orbital, float surface, float vertical);
void apIngestApsides(float apoapsis, float periapsis);
void apIngestApsidesTime(float toAp, float toPe);
void apIngestOrbit(float inclinationDeg);
void apIngestAttitude(float heading, float pitch, float roll,
                      float srfVelHeading, float srfVelPitch,
                      float orbVelHeading, float orbVelPitch);
void apIngestAtmo(float airDensity, bool hasAtmosphere, bool inAtmosphere);
void apIngestGForce(float gForce);
void apIngestSkinTemp(float skinTempFraction);
void apIngestStageDeltaV(float stageDeltaV);
void apIngestSOI(const char *bodyName);
float attClampf(float v, float lo, float hi);
float attWrap180(float a);
float attWrap360(float a);
void attReset(AttState &s);
void attUpdateRates(AttState &s, const AttMeasure &m, float dt);
AttCommand attSteerRocket(AttState &s, const AttGains &g, const AttMeasure &m,
                          float cmdPitch, float cmdHeading,
                          bool holdRoll, float cmdRoll, float maxDeflection, float dt);
AttCommand attSteerAircraft(AttState &s, const AttGains &g, const AttMeasure &m,
                            float cmdPitch, float cmdBank, bool coordinateTurn,
                            float maxDeflection, float dt);
AttGains attRocketGains();
AttGains attAircraftGains();
static void bpSetReason(uint8_t r);
static inline float bpMu();
static inline float d2r(float d);
BurnConfig bpDefaultConfig();
void bpInit();
static void bpThrottle(float t);
static void bpReleaseVehicle(bool sasStability);
static float bpTimeBetween(float nu0, float nu1);
static float bpRadiusAt(float nuDeg);
static float bpSpeedAt(float r);
static BurnPlan bpPlanNode();
static BurnPlan bpPlanApsis(bool changeAp);
static BurnPlan bpPlanInc();
static BurnPlan bpPlan(BpMode m);
static float bpTIgnitionLive();
static float bpRemainingLive();
static void bpPointingRef(float &hdg, float &pitch);
static float bpPointingError();
static bool bpPlanChanged(const BurnPlan &a, const BurnPlan &b);
bool bpArm(BpMode mode, bool on);
bool bpExecute();
void bpAbort(uint8_t reason);
void bpArbiterDrop();
bool bpEngageApproach(bool on);
static bool bpInRange(float v, float lo, float hi);
bool bpSetTargetAp(float v);
bool bpSetTargetPe(float v);
bool bpSetTargetInc(float v);
bool bpSetApprRate(float v);
bool bpSetApprDist(float v);
void bpSetAutoWarp(bool on);
bool bpArmed();
bool bpExecuting();
bool bpAnyEngaged();
static void bpVec(float hdg, float pitch, float v[3]);
static float bpDot(const float a[3], const float b[3]);
static void bpCross(const float a[3], const float b[3], float o[3]);
static void bpUpdateApproach(float dt);
void bpUpdate();
static uint8_t bpAge(uint8_t r, uint32_t ms);
BurnStatus bpGetStatus();
const char *bpPhaseName(uint8_t p);
bool bpConsoleLine(const char *line);
static inline void bpStamp();
void bpIngestNode(float timeTo, float dv, float duration, float heading, float pitch);
void bpIngestOrbit(float ecc, float sma, float inc, float lan, float argPe, float trueAnom, float period);
void bpIngestApsides(float apoapsis, float periapsis);
void bpIngestApsidesTime(float toAp, float toPe);
void bpIngestVelocity(float orbital);
void bpIngestAttitude(float heading, float pitch, float roll, float orbVelHeading, float orbVelPitch);
void bpIngestTarget(bool available, float distance, float velocity, float heading, float pitch, float velHeading, float velPitch);
void bpIngestBody(float radius, float gravity, const char *name);
void bpVesselChanged();
void setFanControl();
bool buttonPressed(uint16_t prevButton, uint16_t newButton, uint16_t mask);
bool buttonReleased(uint16_t prevButton, uint16_t newButton, uint16_t mask);
uint16_t toggleBit(uint16_t registerInput, uint16_t mask);
uint8_t toggleBit(uint8_t registerInput, uint8_t mask);
uint16_t setBit(uint16_t registerInput, uint16_t mask);
uint8_t setBit(uint8_t registerInput, uint8_t mask);
uint16_t clearBit(uint16_t registerInput, uint16_t mask);
uint8_t clearBit(uint8_t registerInput, uint8_t mask);
bool isBitEnabled(uint16_t registerInput, uint16_t mask);
bool isBitEnabled(uint8_t registerInput, uint8_t mask);
void setActionGroups();
uint16_t setSASLEDState(uint8_t SAS_input);
void handlePanelCtrl(uint8_t i2c_addr);
void handleStabAssistPanel(uint8_t i2c_addr);
void handleVehCtrlPanel(uint8_t i2c_addr);
void executeReboot();
void disconnectUSB();
void connectUSB();
static inline bool hpAircraftEngaged();
static inline bool hpRoverDriving();
static inline bool hpOnGround();
static void hpSetReason(uint8_t r);
static void hpSetRoverReason(uint8_t r);
HoldConfig hpDefaultConfig();
void hpInit();
static void hpSendWheels(bool setThrottle, float thr, bool setSteer, float steer);
static void hpReconcileSAS();
static float hpSignedSpeed();
static void hpDropAircraftOutputs();
void hpDisconnectAircraft(uint8_t reason);
void hpArbiterDropAttitude();
void hpArbiterDropThrottle();
void hpDisconnectRover(uint8_t reason);
void hpDisconnectAll(uint8_t reason);
void hpVesselChanged();
static bool hpIsAircraftMode(HpMode m);
bool hpEngage(HpMode mode, bool on);
void hpLevel();
static bool hpInRange(float v, float lo, float hi);
bool hpSetAtt(float v);
bool hpSetAoa(float v);
bool hpSetVs(float v);
bool hpSetAlt(float v);
bool hpSetRoll(float v);
bool hpSetHdg(float v);
bool hpSetIas(float v);
bool hpSetMach(float v);
bool hpSetCruise(float v);
bool hpSetRoverHdg(float v);
bool hpSetMaxSpeed(float v);
bool hpSetMaxSlope(float v);
bool hpSetMaxRoll(float v);
bool hpSetGs(float v);
bool hpSetFollowRange(float v);
bool hpSetStopDist(float v);
bool hpAnyEngaged();
bool hpAttitudeEngaged();
bool hpThrustEngaged();
bool hpRoverEngaged();
static float hpVsLoop(float vsCmd, float dt);
static void hpUpdateAircraft(uint32_t now, float dt);
static void hpUpdateRover(uint32_t now, float dt);
void hpUpdate();
static uint8_t hpAgeSeconds(uint32_t sinceMs, uint8_t reason);
HoldStatus hpGetStatus();
const char *hpModeName(HpMode m);
const char *hpReasonName(uint8_t r);
static int8_t hpModeFromName(const char *n);
bool hpConsoleLine(const char *line);
static inline void hpStamp();
void hpIngestFlightStatus(uint8_t vesselType, uint8_t situation, bool hasTarget);
void hpIngestAltitude(float sealevel);
void hpIngestVelocity(float surface, float vertical);
void hpIngestAirspeed(float ias, float mach);
void hpIngestAttitude(float heading, float pitch, float roll, float srfVelHeading, float srfVelPitch);
void hpIngestAtmo(bool hasAtmosphere, bool inAtmosphere);
void hpIngestBrakes(bool on);
void hpIngestTarget(bool available, float bearingDeg, float elevationDeg, float distance, float closingRate);
void hpIngestThrottle(float t01);
static uint8_t idlControlByte(uint8_t reqType);
static void idlWrite(uint8_t addr, const uint8_t *buf, uint8_t n);
static void idlSendControl(uint8_t addr, uint8_t reqType, uint8_t ackSeq);
void idlInit();
static void idlApply(uint8_t op, float v);
static void idlPoll();
static void idlPutFloat(uint8_t *dst, float f);
static void idlPushAscent();
static void idlPushAircraft();
static void idlPushRover();
static void idlPushOrbital();
static void idlPushLanding();
void idlService();
static void lpSetReason(uint8_t r);
static inline bool lpOnGround();
static inline float lpHorizontalSpeed();
LandingConfig lpDefaultConfig();
void lpInit();
static void lpThrottle(float t);
static void lpSas(uint8_t mode);
static void lpReleaseThrottleOwner();
static void lpReleaseAttitudeOwner(bool sasStability);
static float lpAccel();
static uint8_t lpAccelSource();
static float lpIgnitionAltitude();
static void lpDropThrottleModes(uint8_t reason, bool keepThrottle);
static void lpDropEntry(uint8_t reason);
void lpDisconnectAll(uint8_t reason);
void lpArbiterDropAttitude();
void lpArbiterDropThrottle();
bool lpEngage(uint8_t mode, bool on);
bool lpEngageEntry(bool on);
static bool lpInRange(float v, float lo, float hi);
bool lpSetDescRate(float v);
bool lpSetHovrAlt(float v);
bool lpSetTwr(float v);
bool lpSetMargin(float v);
bool lpSetEntryAoa(float v);
bool lpSetEntryRoll(float v);
void lpSetAttRef(bool radial);
bool lpAnyEngaged();
static float lpDescLoop(float vsCmd, float dt);
static void lpUpdateThrottleModes(uint32_t now, float dt);
static void lpUpdateEntry(uint32_t now, float dt);
void lpUpdate();
LandingStatus lpGetStatus();
bool lpConsoleLine(const char *line);
static inline void lpStamp();
void lpIngestFlightStatus(uint8_t vesselType, uint8_t situation);
void lpIngestAltitude(float sealevel, float surface);
void lpIngestVelocity(float surface, float vertical);
void lpIngestAirspeed(float mach);
void lpIngestAttitude(float heading, float pitch, float roll, float orbVelHeading, float orbVelPitch);
void lpIngestAtmo(float airDensity, bool inAtmosphere);
void lpIngestBody(float gravity, float flyHigh, const char *name);
void lpIngestThrottle(float t01);
void lpVesselChanged();
static inline int16_t rotBE(const uint8_t *p);
static inline float rotNorm(int16_t v);
void rotInit();
static void trnPoll(uint32_t now);
static bool ovrHeld(float mag, uint32_t &since, uint32_t now);
bool pilotOverrideDetected(uint8_t &reason);
static void rotPoll(uint32_t now);
void rotSetAutoAxes(float pitch, float yaw, float roll, uint8_t heldMask);
void rotClearAutoAxes();
float rotPilotPitch();
float rotPilotYaw();
float rotPilotRoll();
void rotService();
void registerInputChannels();
void messageHandler(byte messageType, byte msg[], byte msgSize);
void initSimpitObject();
void asSetEnabled(bool on);
bool asEnabled();
void asMaybeStage(bool enabled, float throttle, float remainingDv);
void aeIngestStage(float stageDv, float stageBurnTime);
void aeIngestGForce(float gForce);
void aeIngestAtmo(bool inAtmosphere);
void aeNoteThrottle(float commanded);
void aeSetTwrOverride(float twr, float g0);
uint8_t aeSource();
float aeAccel();
float aeBurnDuration(float dv);
float aeStageDv();
static void thrSendKsp(float t);
static void thrSendCommand(uint8_t cmd, const uint8_t *payload, uint8_t n);
static void thrDriveLever(float t, uint32_t now);
static void thrLatchOverride();
void thrInit();
void thrSetPrecision(bool fine);
static void thrPoll(uint32_t now);
void thrService();
void thrAutoThrottle(uint8_t owner, float t);
void thrAutoRelease(uint8_t owner);
bool thrTakeOverrideEvent();
bool thrTakeMovedEvent();
bool thrTouched();
bool thrPrecision();
bool thrLeverDriven();
bool thrOverrideLatched();
float thrCurrentThrottle();
uint8_t thrOwner();
// ---- tabs ----
/***************************************************************************************
   ap_arbiter.ino — attitude and throttle ownership across the autopilot modules.
   Contract in control_links.h. Holds only the two owner ids; the modules do the work.

   Taking a resource from another module calls that module's arbiter-drop hook, which
   disconnects with reason OTHER AP and releases through arbRelease*() — ignored here
   because the new owner is already recorded, so there is no recursion.
****************************************************************************************/
#include "control_links.h"
#include "ascent_autopilot.h"
#include "hold_autopilot.h"
#include "burn_autopilot.h"
#include "landing_autopilot.h"

static uint8_t arb_att = AP_OWNER_NONE;
static uint8_t arb_thr = AP_OWNER_NONE;

void arbInit() { arb_att = arb_thr = AP_OWNER_NONE; }

static void arbDropAttitude(uint8_t owner) {
  switch (owner) {
    case AP_OWNER_ASCENT:  apArbiterDrop(); break;
    case AP_OWNER_HOLD:    hpArbiterDropAttitude(); break;
    case AP_OWNER_BURN:    bpArbiterDrop(); break;
    case AP_OWNER_LANDING: lpArbiterDropAttitude(); break;
    default: break;
  }
}
static void arbDropThrottle(uint8_t owner) {
  switch (owner) {
    case AP_OWNER_ASCENT:  apArbiterDrop(); break;
    case AP_OWNER_HOLD:    hpArbiterDropThrottle(); break;
    case AP_OWNER_BURN:    bpArbiterDrop(); break;
    case AP_OWNER_LANDING: lpArbiterDropThrottle(); break;
    default: break;
  }
}

void arbTakeAttitude(uint8_t owner) {
  if (arb_att == owner) return;
  uint8_t prev = arb_att;
  arb_att = owner;
  if (prev != AP_OWNER_NONE) arbDropAttitude(prev);
}
void arbTakeThrottle(uint8_t owner) {
  if (arb_thr == owner) return;
  uint8_t prev = arb_thr;
  arb_thr = owner;
  if (prev != AP_OWNER_NONE) arbDropThrottle(prev);
}
void arbReleaseAttitude(uint8_t owner) { if (arb_att == owner) arb_att = AP_OWNER_NONE; }
void arbReleaseThrottle(uint8_t owner) { if (arb_thr == owner) arb_thr = AP_OWNER_NONE; }
uint8_t arbAttitudeOwner() { return arb_att; }
uint8_t arbThrottleOwner() { return arb_thr; }

bool arbCanWarp(uint8_t owner) {
  return (arb_att == AP_OWNER_NONE || arb_att == owner) && (arb_thr == AP_OWNER_NONE || arb_thr == owner);
}

// A/P OFF on any console drops everything in every module (review decision, q.6).
void arbAllOff() {
  apDisarm();
  hpDisconnectAll(HP_REASON_PILOT);
  bpAbort(HP_REASON_PILOT);
  lpDisconnectAll(HP_REASON_PILOT);
  arb_att = arb_thr = AP_OWNER_NONE;
}

/********************************************************************************************************************************
  Ascent Autopilot — implementation (Master Teensy 4.1)

  Runs a launch-to-orbit guidance loop over the master controller's KerbalSimpit link. See ascent_autopilot.h for the public
  API and the tunable AscentConfig surface.

  Because all sketch .ino files compile as a single translation unit, this file references the global `mySimpit` object and the
  KerbalSimpit message types / action constants declared in Controller_Main.ino directly.

  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Written for Jeb's Controller Works.
********************************************************************************************************************************/
#include "ascent_autopilot.h"
#include "attitude_controller.h"
#include "hold_autopilot.h"
#include "burn_autopilot.h"
#include "landing_autopilot.h"
#include "control_links.h"

/***************************************************************************************
   KerbalSimpit control-axis scaling.
   Rotation axes are int16 with full deflection at +/-INT16_MAX; throttle is 0..INT16_MAX.
   If a particular Simpit build uses a different full-scale, adjust AP_AXIS_FULL only.
****************************************************************************************/
static const int32_t AP_AXIS_FULL = INT16_MAX;

/***************************************************************************************
   Module state
****************************************************************************************/
static AscentConfig g_cfg;                 // Active configuration
static AscentPhase  g_phase   = AP_PHASE_IDLE;
static bool         g_armed   = false;

// Latest commanded outputs (mirrored into AscentStatus)
static float g_cmdPitch    = 90.0f;
static float g_cmdHeading  = 90.0f;
static float g_cmdThrottle = 0.0f;
static float g_dynPressure = 0.0f;

// Phase / event bookkeeping
static uint32_t g_lastStageMs   = 0;
static bool     g_sasIsOff       = false;  // we have commanded stock SAS off (active raw steering)
static bool     g_sasProgradeSet = false;  // we have commanded stock SAS on + prograde hold (coast)

// Attitude loop state (shared attitude controller — attitude_controller.h)
static AttState g_att;
static uint32_t g_lastUpdateMs  = 0;       // wall clock of last apUpdate (for dt)

/***************************************************************************************
   Telemetry snapshot, fed by apIngest*()
****************************************************************************************/
struct ApTelemetry {
  bool     inFlight      = false;
  float    altSurface    = 0.0f;   // m
  float    altSea        = 0.0f;   // m
  float    velSurface    = 0.0f;   // m/s
  float    velVertical   = 0.0f;   // m/s
  float    velOrbital    = 0.0f;   // m/s
  float    apoapsis      = 0.0f;   // m
  float    periapsis     = 0.0f;   // m
  float    timeToAp      = 0.0f;   // s
  float    inclination   = 0.0f;   // deg
  float    heading       = 0.0f;   // deg (current attitude)
  float    pitch         = 0.0f;   // deg (current attitude, above horizon)
  float    roll          = 0.0f;   // deg (current attitude)
  float    srfVelHeading = 0.0f;   // deg (surface prograde heading — used for AoA / gravity turn)
  float    srfVelPitch   = 0.0f;   // deg (surface prograde pitch)
  float    orbVelHeading = 0.0f;   // deg (orbital prograde heading — used for coast / circularization)
  float    orbVelPitch   = 0.0f;   // deg (orbital prograde pitch)
  float    airDensity    = 0.0f;   // kg/m^3
  float    gForce        = 0.0f;   // felt acceleration in g (from AIRSPEED_MESSAGE)
  bool     hasAtmo       = true;   // current body has an atmosphere (from ATMO_CONDITIONS)
  bool     inAtmo        = true;   // vessel is currently within the atmosphere
  float    skinTempFrac  = 0.0f;   // 0..1
  float    stageDV       = 1.0e6f; // m/s (start high so we do not stage before data arrives)
  char     bodyName[24]  = {0};    // current sphere-of-influence body (from SOI_MESSAGE)
  uint32_t lastTelemMs   = 0;      // freshness stamp
};
static ApTelemetry g_tel;

/***************************************************************************************
   Current-body parameters come from the shared celestial-body table (single source of
   truth in Software/Common/body_params.h, included by Controller_Main.ino). getBodyParams()
   is keyed on the SOI_MESSAGE name; an unrecognised body returns an empty entry
   (soiName[0] == '\0') and guidance falls back to telemetry-driven behaviour.
****************************************************************************************/
static BodyParams g_curBody = { "", "", "", "", 0, 0, 0, 0, 0, 0.0,
                                0, 0.0f, 0.0f, 0, 0, 0.0f, false, false, false, 0.0f };
static bool       g_targetLocked = false;   // pilot has set an explicit target apoapsis

static inline bool apBodyKnown() { return g_curBody.soiName != nullptr && g_curBody.soiName[0] != '\0'; }

// Suggested parking-orbit altitude for the current body, derived from the shared table:
// just above the atmosphere on atmospheric bodies, or terrain (minSafe) plus margin on
// airless bodies. Returns 0 for an unknown body (caller keeps the existing target).
static float apBodyDefaultOrbit() {
  if (!apBodyKnown()) return 0.0f;
  if (g_curBody.hasAtmo && g_curBody.lowSpace > 0.0f) return g_curBody.lowSpace + 10000.0f;
  return g_curBody.minSafe + max(8000.0f, g_curBody.minSafe * 0.5f);
}

// Minimum safe orbit altitude (terrain / atmosphere clearance) for the current body.
static float apBodyMinSafeOrbit() {
  if (!apBodyKnown()) return 0.0f;
  if (g_curBody.hasAtmo && g_curBody.lowSpace > 0.0f) return g_curBody.lowSpace + 1000.0f;
  return g_curBody.minSafe + 5000.0f;
}

/***************************************************************************************
   Small math helpers
****************************************************************************************/
static inline float apClampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float apDeg2Rad(float d) { return d * 0.0174532925199f; }
static inline float apRad2Deg(float r) { return r * 57.2957795131f; }

// Wrap an angle error into [-180, 180]
static float apWrap180(float a) {
  while (a > 180.0f)  a -= 360.0f;
  while (a < -180.0f) a += 360.0f;
  return a;
}

// Wrap a heading into [0, 360)
static float apWrap360(float a) {
  while (a >= 360.0f) a -= 360.0f;
  while (a < 0.0f)    a += 360.0f;
  return a;
}

/***************************************************************************************
   Launch azimuth from target inclination and site latitude.
   Standard spherical result: sin(azimuth) = cos(inclination) / cos(latitude), with the
   azimuth measured clockwise from north. This is the inertial azimuth (it does not
   correct for the body's surface rotation velocity); use headingBias to trim, or the
   southerly flag for the descending-node solution. Retrograde targets are approximate.
****************************************************************************************/
static float apLaunchAzimuth() {
  float cosI = cosf(apDeg2Rad(g_cfg.targetInclination));
  float cosL = cosf(apDeg2Rad(g_cfg.launchLatitude));
  if (fabsf(cosL) < 1.0e-4f) cosL = (cosL < 0 ? -1.0e-4f : 1.0e-4f);
  float s = apClampf(cosI / cosL, -1.0f, 1.0f);
  float az = apRad2Deg(asinf(s));         // principal value, [-90, 90] from north
  if (g_cfg.launchSoutherly) az = 180.0f - az;   // descending-node / southerly branch
  return apWrap360(az + g_cfg.headingBias);
}

/***************************************************************************************
   Pitch program: 90 deg (straight up) at the turn start, easing to finalPitch by
   turnEndAltitude. `loft` shapes the curve — exponent < 1 pitches over aggressively,
   > 1 stays steeper longer. An initial kick lowers the starting pitch to commit the turn.
****************************************************************************************/
// Body-relative turn-end altitude: a fraction of the atmosphere top on atmospheric
// bodies, or a fraction of the target apoapsis on airless / unknown-atmosphere bodies.
// Falls back to the manual turnEndAltitude when body-profile mode is off.
static float apEffectiveTurnEnd() {
  if (!g_cfg.autoBodyProfile) return g_cfg.turnEndAltitude;
  if (g_tel.hasAtmo && g_curBody.lowSpace > 0.0f)
    return g_cfg.turnEndAtmoFraction * g_curBody.lowSpace;
  return g_cfg.turnEndAirlessFraction * g_cfg.targetApoapsis;
}

static float apScheduledPitch() {
  float span = apEffectiveTurnEnd() - g_cfg.turnStartAltitude;
  if (span < 1.0f) span = 1.0f;
  float frac = (g_tel.altSurface - g_cfg.turnStartAltitude) / span;
  frac = apClampf(frac, 0.0f, 1.0f);
  float top   = 90.0f - g_cfg.initialPitchKick;                 // pitch just after the kick
  float pitch = top - (top - g_cfg.finalPitch) * powf(frac, g_cfg.loft);
  return pitch;
}

/***************************************************************************************
   Acceleration (g-force) limiter: throttle back to hold the felt acceleration below
   maxG (analogous to MechJeb's "limit acceleration"). Uses the gForces telemetry, so it
   needs no mass/thrust knowledge. Applied to every powered phase, not just the turn.
****************************************************************************************/
static float apLimitG(float thr) {
  if (g_cfg.maxG > 0.0f && g_tel.gForce > g_cfg.maxG) {
    float scale = g_cfg.maxG / g_tel.gForce;                     // proportional back-off
    thr = min(thr, max(g_cfg.maxGThrottleFloor, thr * scale));
  }
  return apClampf(thr, 0.0f, 1.0f);
}

/***************************************************************************************
   Throttle manager for powered flight: starts from launchThrottle and applies the
   max-Q, skin-temperature, apoapsis-approach, and max-G limits, taking the most
   restrictive.
****************************************************************************************/
static float apManagedThrottle() {
  float thr = g_cfg.launchThrottle;

  // Max-Q limiting: q = 1/2 * rho * v^2 (atmospheric bodies only)
  g_dynPressure = 0.5f * g_tel.airDensity * g_tel.velSurface * g_tel.velSurface;
  if (g_tel.hasAtmo && g_cfg.maxQ > 0.0f && g_dynPressure > g_cfg.maxQ) {
    float scale = g_cfg.maxQ / g_dynPressure;                   // proportional back-off
    thr = min(thr, max(g_cfg.maxQThrottleFloor, g_cfg.launchThrottle * scale));
  }

  // Skin-temperature limiting: ease off as skin temp approaches its limit
  if (g_cfg.skinTempLimit > 0.0f && g_tel.skinTempFrac > g_cfg.skinTempLimit) {
    float over = (g_tel.skinTempFrac - g_cfg.skinTempLimit) / max(0.01f, 1.0f - g_cfg.skinTempLimit);
    thr = min(thr, max(g_cfg.maxQThrottleFloor, 1.0f - over));
  }

  // Apoapsis-approach taper: throttle down smoothly as apoapsis nears the target
  float taperFrom = g_cfg.apoTaperStart * g_cfg.targetApoapsis;
  if (g_tel.apoapsis >= taperFrom && g_cfg.targetApoapsis > 0.0f) {
    float remaining = (g_cfg.targetApoapsis - g_tel.apoapsis);
    float band      = max(1.0f, (1.0f - g_cfg.apoTaperStart) * g_cfg.targetApoapsis);
    float t         = apClampf(remaining / band, 0.0f, 1.0f);   // 1 at taper start -> 0 at target
    thr = min(thr, max(g_cfg.apoTaperFloor, t));
  }

  return apLimitG(thr);   // acceleration limit (also clamps to [0,1])
}

/***************************************************************************************
   Emit throttle (0..1) to KSP
****************************************************************************************/
static void apSendThrottle(float f) {
  f = apClampf(f, 0.0f, 1.0f);
  g_cmdThrottle = f;
  // Through the throttle link so the Throttle Module's motorised lever rides the
  // managed throttle (max-Q / max-G / apoapsis taper). A pilot grabbing the lever
  // disarms the autopilot on the next update (pilotOverrideDetected).
  thrAutoThrottle(THR_OWNER_ASCENT, f);
}

/***************************************************************************************
   Steering: drive the vehicle toward (cmdPitch above horizon, cmdHeading).

   Attitude errors are computed in the navball frame, then rotated into the body frame
   by the current roll angle so pitch/yaw commands stay correct regardless of roll.
   A PID on each axis produces the stick command; roll is optionally held at targetRoll.
****************************************************************************************/
static void apSteer(float cmdPitch, float cmdHeading, float dt) {
  g_cmdPitch   = cmdPitch;
  g_cmdHeading = cmdHeading;

  AttMeasure m;
  m.pitch = g_tel.pitch; m.heading = g_tel.heading; m.roll = g_tel.roll;
  attUpdateRates(g_att, m, dt);

  AttGains g;
  g.pitchKp = g_cfg.pitchKp; g.pitchKi = g_cfg.pitchKi; g.pitchKd = g_cfg.pitchKd;
  g.yawKp   = g_cfg.yawKp;   g.yawKi   = g_cfg.yawKi;   g.yawKd   = g_cfg.yawKd;
  g.rollKp  = g_cfg.rollKp;  g.rollKi  = g_cfg.rollKi;  g.rollKd  = g_cfg.rollKd;

  AttCommand c = attSteerRocket(g_att, g, m, cmdPitch, cmdHeading,
                                g_cfg.rollControlEnabled, g_cfg.targetRoll,
                                g_cfg.maxControlDeflection, dt);

  rotationMessage msg;
  msg.setPitch((int16_t)(c.pitch * (float)AP_AXIS_FULL));
  msg.setYaw((int16_t)(c.yaw  * (float)AP_AXIS_FULL));
  if (g_cfg.rollControlEnabled) msg.setRoll((int16_t)(c.roll * (float)AP_AXIS_FULL));
  mySimpit.send(ROTATION_MESSAGE, msg);
}

// Command the vehicle to hold orbital prograde (used during coast / circularization
// when we steer actively rather than delegating to stock SAS).
static void apSteerPrograde(float dt) {
  apSteer(g_tel.orbVelPitch, g_tel.orbVelHeading, dt);
}

// True while the module is driving raw rotation commands (and therefore needs stock
// SAS off). During coast/circularization we defer to stock SAS if useStockSASForCoast.
static bool apActiveSteering() {
  switch (g_phase) {
    case AP_PHASE_VERTICAL:
    case AP_PHASE_GRAVITY_TURN: return true;
    case AP_PHASE_COAST:
    case AP_PHASE_CIRCULARIZE:  return !g_cfg.useStockSASForCoast;
    default:                    return false;
  }
}

// Reconcile stock SAS with the current steering mode: SAS off while we send raw
// rotation, SAS on + prograde hold while we defer to it. Only acts on transitions.
static void apReconcileSAS() {
  if (apActiveSteering()) {
    if (!g_sasIsOff) {
      mySimpit.deactivateAction(SAS_ACTION);
      g_sasIsOff = true;
      g_sasProgradeSet = false;
    }
  } else {
    if (!g_sasProgradeSet) {
      mySimpit.activateAction(SAS_ACTION);
      mySimpit.setSASMode(AP_PROGRADE);
      g_sasProgradeSet = true;
      g_sasIsOff = false;
    }
  }
}

/***************************************************************************************
   Auto-staging: fire STAGE_ACTION when the active stage's delta-V is spent, with a
   lockout so a single depletion does not trigger a burst of stagings.
****************************************************************************************/
static void apMaybeStage() {
  if (!g_cfg.autoStage) return;
  uint32_t now = millis();
  if (g_tel.stageDV < g_cfg.stageDVThreshold && (now - g_lastStageMs) > g_cfg.stageMinInterval) {
    mySimpit.activateAction(STAGE_ACTION);
    g_lastStageMs = now;
  }
}

/***************************************************************************************
   Public API
****************************************************************************************/
AscentConfig apDefaultConfig() {
  AscentConfig c;
  // Mission targets — 80 km circular equatorial from KSC
  c.targetApoapsis     = 80000.0f;
  c.targetInclination  = 0.0f;
  c.launchSoutherly    = false;
  c.launchLatitude     = 0.0f;      // KSC is ~0.1 deg; 0 is a fine approximation
  c.headingBias        = 0.0f;

  // Body / sphere-of-influence handling
  c.autoBodyProfile       = true;   // adapt to whatever SoI the craft is in (atmospheric or airless)
  c.turnEndAtmoFraction   = 0.80f;  // atmospheric: level off by ~80% of atmosphere top
  c.turnEndAirlessFraction = 0.25f; // airless: pitch over within ~25% of target apoapsis
  c.enforceMinSafeAltitude = true;  // clamp target up to the body's minimum safe altitude on arm

  // Ascent shape
  c.turnStartAltitude  = 500.0f;
  c.turnStartVelocity  = 60.0f;
  c.turnEndAltitude    = 55000.0f;  // manual fallback (used only when autoBodyProfile is false)
  c.loft               = 1.0f;      // 1.0 = balanced; lower = aggressive, higher = lofted
  c.initialPitchKick   = 3.0f;
  c.finalPitch         = 0.0f;

  // Throttle management
  c.launchThrottle     = 1.0f;
  c.autoLaunch         = false;
  c.maxQ               = 0.0f;      // 0 = off by default; a typical KSP value is ~18000-25000 Pa
  c.maxQThrottleFloor  = 0.5f;
  c.maxG               = 0.0f;      // 0 = off by default; e.g. 4.0 to cap felt acceleration at 4 g
  c.maxGThrottleFloor  = 0.30f;
  c.skinTempLimit      = 0.0f;      // 0 = off; e.g. 0.85 to ease off at 85% skin temp
  c.apoTaperStart      = 0.92f;
  c.apoTaperFloor      = 0.10f;

  // Steering / control authority
  c.aoaLimit           = 5.0f;      // keep commanded pitch within 5 deg of surface prograde
  c.pitchKp = 0.9f; c.pitchKi = 0.05f; c.pitchKd = 0.0f;
  c.yawKp   = 0.9f; c.yawKi   = 0.05f; c.yawKd   = 0.0f;
  c.rollKp  = 0.6f; c.rollKi  = 0.02f; c.rollKd  = 0.0f;
  c.rollControlEnabled = false;
  c.targetRoll         = 0.0f;
  c.maxControlDeflection = 1.0f;

  // Staging
  c.autoStage          = true;
  c.stageDVThreshold   = 5.0f;
  c.stageMinInterval   = 2000;

  // Circularization
  c.circularize        = true;
  c.circStartLeadTime  = 10.0f;
  c.circPeTolerance    = 1000.0f;

  // Safety
  c.telemetryTimeout   = 2000;
  c.useStockSASForCoast = true;
  return c;
}

void apInit() {
  g_cfg   = apDefaultConfig();
  g_phase = AP_PHASE_IDLE;
  g_armed = false;
  g_lastUpdateMs = millis();
}

AscentConfig &apGetConfig() { return g_cfg; }
void apSetConfig(const AscentConfig &cfg) { g_cfg = cfg; }

void apSetTargets(float apoapsisM, float inclinationDeg, float loft) {
  // Convenience mission set; respects the same disarmed-only guard as the field setters.
  apSetTargetAltitude(apoapsisM);
  apSetTargetInclination(inclinationDeg);
  apSetLoft(loft);
}

const char *apCurrentBody() { return g_tel.bodyName; }

void apArm() {
  g_armed          = true;
  g_phase          = AP_PHASE_VERTICAL;
  attReset(g_att);
  arbTakeAttitude(AP_OWNER_ASCENT);   // arming takes the vehicle from any other autopilot (ap_arbiter.ino)
  arbTakeThrottle(AP_OWNER_ASCENT);
  g_sasIsOff       = false;   // force apReconcileSAS() to command SAS off on the first pass
  g_sasProgradeSet = false;
  g_lastStageMs    = millis();
  g_lastUpdateMs = millis();
  // Terrain-clearance guard: never target below the body's minimum safe altitude.
  float minSafe = apBodyMinSafeOrbit();
  if (g_cfg.enforceMinSafeAltitude && minSafe > 0.0f && g_cfg.targetApoapsis < minSafe) {
    g_cfg.targetApoapsis = minSafe;
    mySimpit.printToKSP("Ascent AP: target raised to min safe altitude", PRINT_TO_SCREEN);
  }
  if (g_cfg.autoLaunch) {
    mySimpit.activateAction(STAGE_ACTION);   // ignite first stage
    g_lastStageMs = millis();
  }
}

void apDisarm() {
  g_armed = false;
  g_phase = AP_PHASE_IDLE;
  apSendThrottle(0.0f);
  thrAutoRelease(THR_OWNER_ASCENT);
  arbReleaseAttitude(AP_OWNER_ASCENT);
  arbReleaseThrottle(AP_OWNER_ASCENT);
  // Zero the control axes so we do not leave the stick deflected.
  rotationMessage msg;
  msg.setPitch(0); msg.setYaw(0); msg.setRoll(0);
  mySimpit.send(ROTATION_MESSAGE, msg);
}

bool        apIsArmed()  { return g_armed; }
void        apArbiterDrop() { if (g_armed) { apDisarm(); mySimpit.printToKSP("Ascent AP: another autopilot engaged - disarmed", PRINT_TO_SCREEN); } }
AscentPhase apGetPhase() { return g_phase; }

const char *apPhaseName(AscentPhase phase) {
  switch (phase) {
    case AP_PHASE_IDLE:         return "IDLE";
    case AP_PHASE_VERTICAL:     return "VERTICAL";
    case AP_PHASE_GRAVITY_TURN: return "GRAVITY TURN";
    case AP_PHASE_COAST:        return "COAST";
    case AP_PHASE_CIRCULARIZE:  return "CIRCULARIZE";
    case AP_PHASE_COMPLETE:     return "COMPLETE";
    case AP_PHASE_ABORT:        return "ABORT";
    default:                    return "?";
  }
}

AscentStatus apGetStatus() {
  AscentStatus s;
  s.armed          = g_armed;
  s.phase          = g_phase;
  s.phaseName      = apPhaseName(g_phase);
  s.body           = g_tel.bodyName;
  s.targetApoapsis = g_cfg.targetApoapsis;
  s.apoapsis       = g_tel.apoapsis;
  s.periapsis      = g_tel.periapsis;
  s.cmdPitch       = g_cmdPitch;
  s.cmdHeading     = g_cmdHeading;
  s.cmdThrottle    = g_cmdThrottle;
  s.gForce         = g_tel.gForce;
  s.dynPressure    = g_dynPressure;
  return s;
}

/***************************************************************************************
   Console-facing setters — apply only while DISARMED (return false, no change, if armed).
****************************************************************************************/
bool apSetTargetAltitude(float meters) {
  if (g_armed || meters < 0.0f) return false;
  g_cfg.targetApoapsis = meters;
  g_targetLocked = true;
  return true;
}
bool apSetTargetInclination(float deg) {
  if (g_armed || deg < 0.0f || deg > 180.0f) return false;
  g_cfg.targetInclination = deg;
  return true;
}
bool apSetLaunchSoutherly(bool southerly) {
  if (g_armed) return false;
  g_cfg.launchSoutherly = southerly;
  return true;
}
bool apSetLoft(float exponent) {
  if (g_armed || exponent <= 0.0f) return false;
  g_cfg.loft = exponent;
  return true;
}
bool apSetRoll(bool enabled, float deg) {
  if (g_armed) return false;
  g_cfg.rollControlEnabled = enabled;
  g_cfg.targetRoll = apClampf(deg, -180.0f, 180.0f);
  return true;
}
bool apSetMaxG(float g) {
  if (g_armed || g < 0.0f) return false;
  g_cfg.maxG = g;
  return true;
}

/***************************************************************************************
   Main guidance update — call every loop().
****************************************************************************************/
void apUpdate() {
  if (!g_armed) return;

  uint32_t now = millis();
  float dt = (now - g_lastUpdateMs) * 0.001f;
  g_lastUpdateMs = now;
  if (dt <= 0.0f || dt > 0.5f) dt = 0.02f;   // guard against stalls / first pass

  // --- Failsafe: telemetry loss ---
  if ((now - g_tel.lastTelemMs) > g_cfg.telemetryTimeout) {
    apSendThrottle(0.0f);
    g_phase = AP_PHASE_ABORT;
    g_armed = false;
    return;
  }

  // The pilot has the vehicle: any input on the rotation stick, the translation stick or
  // the throttle lever disarms the ascent autopilot (global rule, control_links.h).
  {
    uint8_t ovr;
    if (pilotOverrideDetected(ovr)) {
      apDisarm();
      mySimpit.printToKSP(ovr == HP_REASON_LEVER ? "Ascent AP: throttle lever - disarmed"
                                                  : "Ascent AP: stick input - disarmed", PRINT_TO_SCREEN);
      return;
    }
  }

  // Keep stock SAS consistent with the current steering mode (off while we send raw
  // rotation; on + prograde while we defer to it during coast/circularization).
  apReconcileSAS();

  float azimuth = apLaunchAzimuth();
  g_cmdHeading  = azimuth;

  switch (g_phase) {

    case AP_PHASE_VERTICAL: {
      apSendThrottle(apLimitG(g_cfg.launchThrottle));
      apSteer(90.0f, azimuth, dt);
      apMaybeStage();
      bool altTrig = g_tel.altSurface >= g_cfg.turnStartAltitude;
      bool velTrig = (g_cfg.turnStartVelocity > 0.0f) && (g_tel.velSurface >= g_cfg.turnStartVelocity);
      if (altTrig || velTrig) g_phase = AP_PHASE_GRAVITY_TURN;
      break;
    }

    case AP_PHASE_GRAVITY_TURN: {
      float pitch = apScheduledPitch();
      // Angle-of-attack limit: keep the commanded pitch within aoaLimit of surface
      // prograde. Aerodynamic guard only — skipped on airless bodies so the craft can
      // pitch over freely to build horizontal velocity.
      if (g_tel.hasAtmo && g_cfg.aoaLimit > 0.0f && g_tel.velSurface > 30.0f) {
        pitch = apClampf(pitch, g_tel.srfVelPitch - g_cfg.aoaLimit, g_tel.srfVelPitch + g_cfg.aoaLimit);
      }
      apSteer(pitch, azimuth, dt);
      apSendThrottle(apManagedThrottle());
      apMaybeStage();

      if (g_tel.apoapsis >= g_cfg.targetApoapsis) {
        apSendThrottle(0.0f);
        g_phase = g_cfg.circularize ? AP_PHASE_COAST : AP_PHASE_COMPLETE;
      }
      break;
    }

    case AP_PHASE_COAST: {
      apSendThrottle(0.0f);
      // Drag in the upper atmosphere can pull apoapsis back down — relight if so.
      if (g_tel.apoapsis < g_cfg.targetApoapsis * 0.995f && g_tel.airDensity > 1.0e-4f) {
        g_phase = AP_PHASE_GRAVITY_TURN;
        break;
      }
      // Prograde is held either by stock SAS (apReconcileSAS) or by us actively.
      if (!g_cfg.useStockSASForCoast) apSteerPrograde(dt);
      if (g_tel.timeToAp <= g_cfg.circStartLeadTime) g_phase = AP_PHASE_CIRCULARIZE;
      break;
    }

    case AP_PHASE_CIRCULARIZE: {
      if (!g_cfg.useStockSASForCoast) apSteerPrograde(dt);
      // Burn until periapsis reaches the target (circular at target altitude).
      if (g_tel.periapsis >= (g_cfg.targetApoapsis - g_cfg.circPeTolerance)) {
        apSendThrottle(0.0f);
        g_phase = AP_PHASE_COMPLETE;
      } else {
        apSendThrottle(apLimitG(g_cfg.launchThrottle));
      }
      break;
    }

    case AP_PHASE_COMPLETE: {
      apSendThrottle(0.0f);
      // Hand back to the pilot with stock SAS enabled in stability-hold (attitude hold).
      mySimpit.activateAction(SAS_ACTION);
      mySimpit.setSASMode(AP_STABILITYASSIST);
      g_armed = false;
      break;
    }

    default:
      apSendThrottle(0.0f);
      g_armed = false;
      break;
  }
}

/***************************************************************************************
   Optional bench-test console (primary Serial). Line-oriented, whitespace-separated.
   Mirrors the console-facing setters so a bench session exercises the same guarded path
   a panel would use:
     ARM | DISARM | STATUS
     ALT <meters> | INC <deg> | LOFT <x> | ROLL <deg> | ROLLOFF | MAXG <g> | SOUTH <0|1>
   Setters apply only while DISARMED and print "(armed - ignored)" otherwise.
****************************************************************************************/
void apSerialConsole() {
  static char buf[48];
  static uint8_t len = 0;
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\r') continue;
    if (ch == '\n' || len >= sizeof(buf) - 1) {
      buf[len] = '\0';
      len = 0;
      bool ok = true;
      if      (strncasecmp(buf, "ARM", 3) == 0)     apArm();
      else if (strncasecmp(buf, "DISARM", 6) == 0)  apDisarm();
      else if (strncasecmp(buf, "ALT ", 4) == 0)    ok = apSetTargetAltitude(atof(buf + 4));
      else if (strncasecmp(buf, "INC ", 4) == 0)    ok = apSetTargetInclination(atof(buf + 4));
      else if (strncasecmp(buf, "LOFT ", 5) == 0)   ok = apSetLoft(atof(buf + 5));
      else if (strncasecmp(buf, "ROLL ", 5) == 0)   ok = apSetRoll(true, atof(buf + 5));
      else if (strncasecmp(buf, "ROLLOFF", 7) == 0) ok = apSetRoll(false, g_cfg.targetRoll);
      else if (strncasecmp(buf, "MAXG ", 5) == 0)   ok = apSetMaxG(atof(buf + 5));
      else if (strncasecmp(buf, "SOUTH ", 6) == 0)  ok = apSetLaunchSoutherly(atoi(buf + 6) != 0);
      else if (strncasecmp(buf, "HP ", 3) == 0)     ok = hpConsoleLine(buf + 3);   // hold-mode autopilot (hold_autopilot.ino)
      else if (strncasecmp(buf, "BP ", 3) == 0)     ok = bpConsoleLine(buf + 3);   // burn autopilot (burn_autopilot.ino)
      else if (strncasecmp(buf, "LP ", 3) == 0)     ok = lpConsoleLine(buf + 3);   // landing autopilot (landing_autopilot.ino)
      else if (strncasecmp(buf, "STATUS", 6) == 0) {
        Serial.print(F("AP armed=")); Serial.print(g_armed);
        Serial.print(F(" phase="));   Serial.print(apPhaseName(g_phase));
        Serial.print(F(" body="));    Serial.print(g_tel.bodyName[0] ? g_tel.bodyName : "?");
        Serial.print(F(" tgtAp="));   Serial.print(g_cfg.targetApoapsis, 0);
        Serial.print(F(" Ap="));      Serial.print(g_tel.apoapsis, 0);
        Serial.print(F(" Pe="));      Serial.print(g_tel.periapsis, 0);
        Serial.print(F(" pitch="));   Serial.print(g_cmdPitch, 1);
        Serial.print(F(" hdg="));     Serial.print(g_cmdHeading, 1);
        Serial.print(F(" thr="));     Serial.print(g_cmdThrottle, 2);
        Serial.print(F(" g="));       Serial.println(g_tel.gForce, 1);
      }
      if (!ok) Serial.println(F("(armed - ignored)"));
    } else {
      buf[len++] = ch;
    }
  }
}

/***************************************************************************************
   Telemetry ingest
****************************************************************************************/
static inline void apStamp() { g_tel.lastTelemMs = millis(); }

void apIngestFlightStatus(bool inFlight)                { g_tel.inFlight = inFlight; apStamp(); }
void apIngestAltitude(float sealevel, float surface)    { g_tel.altSea = sealevel; g_tel.altSurface = surface; apStamp(); }
void apIngestVelocity(float orbital, float surface, float vertical) {
  g_tel.velOrbital = orbital; g_tel.velSurface = surface; g_tel.velVertical = vertical; apStamp();
}
void apIngestApsides(float apoapsis, float periapsis)   { g_tel.apoapsis = apoapsis; g_tel.periapsis = periapsis; apStamp(); }
void apIngestApsidesTime(float toAp, float toPe)        { g_tel.timeToAp = toAp; (void)toPe; apStamp(); }
void apIngestOrbit(float inclinationDeg)                { g_tel.inclination = inclinationDeg; apStamp(); }
void apIngestAttitude(float heading, float pitch, float roll,
                      float srfVelHeading, float srfVelPitch,
                      float orbVelHeading, float orbVelPitch) {
  g_tel.heading = heading; g_tel.pitch = pitch; g_tel.roll = roll;
  g_tel.srfVelHeading = srfVelHeading; g_tel.srfVelPitch = srfVelPitch;
  g_tel.orbVelHeading = orbVelHeading; g_tel.orbVelPitch = orbVelPitch; apStamp();
}
void apIngestAtmo(float airDensity, bool hasAtmosphere, bool inAtmosphere) {
  g_tel.airDensity = airDensity; g_tel.hasAtmo = hasAtmosphere; g_tel.inAtmo = inAtmosphere; apStamp();
}
void apIngestGForce(float gForce)                       { g_tel.gForce = gForce; apStamp(); }
void apIngestSkinTemp(float skinTempFraction)           { g_tel.skinTempFrac = skinTempFraction; apStamp(); }
void apIngestStageDeltaV(float stageDeltaV)             { g_tel.stageDV = stageDeltaV; apStamp(); }

void apIngestSOI(const char *bodyName) {
  if (!bodyName) return;
  // Only react on an actual body change so we do not repeatedly overwrite the config.
  if (strncmp(bodyName, g_tel.bodyName, sizeof(g_tel.bodyName)) == 0) return;
  strncpy(g_tel.bodyName, bodyName, sizeof(g_tel.bodyName) - 1);
  g_tel.bodyName[sizeof(g_tel.bodyName) - 1] = '\0';
  g_curBody = getBodyParams(g_tel.bodyName);   // shared celestial-body table
  // Adopt the body's default parking orbit unless the pilot has set an explicit target.
  if (g_cfg.autoBodyProfile && !g_targetLocked) {
    float def = apBodyDefaultOrbit();
    if (def > 0.0f) g_cfg.targetApoapsis = def;
  }
}

/***************************************************************************************
   attitude_controller.ino — shared attitude PID for the ascent and hold-mode autopilots.
   See attitude_controller.h for the contract.
****************************************************************************************/
#include "attitude_controller.h"

static const float ATT_RATE_TAU_S = 0.25f;   // rate estimate low-pass time constant

float attClampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

float attWrap180(float a) {
  while (a >  180.0f) a -= 360.0f;
  while (a < -180.0f) a += 360.0f;
  return a;
}

float attWrap360(float a) {
  while (a >= 360.0f) a -= 360.0f;
  while (a <    0.0f) a += 360.0f;
  return a;
}

void attReset(AttState &s) {
  s.iPitch = s.iYaw = s.iRoll = 0.0f;
  s.ratePitch = s.rateHeading = s.rateRoll = 0.0f;
  s.prevPitch = s.prevHeading = s.prevRoll = 0.0f;
  s.ratesValid = false;
}

// Filtered first difference of the attitude samples. The Simpit refresh interval bounds
// this — at 125 ms the raw difference is coarse, hence the low-pass.
void attUpdateRates(AttState &s, const AttMeasure &m, float dt) {
  if (dt <= 0.0f) return;
  if (!s.ratesValid) {
    s.prevPitch = m.pitch; s.prevHeading = m.heading; s.prevRoll = m.roll;
    s.ratesValid = true;
    return;
  }
  float a = dt / (ATT_RATE_TAU_S + dt);
  float rp = (m.pitch - s.prevPitch) / dt;
  float rh = attWrap180(m.heading - s.prevHeading) / dt;
  float rr = attWrap180(m.roll - s.prevRoll) / dt;
  s.ratePitch   += a * (rp - s.ratePitch);
  s.rateHeading += a * (rh - s.rateHeading);
  s.rateRoll    += a * (rr - s.rateRoll);
  s.prevPitch = m.pitch; s.prevHeading = m.heading; s.prevRoll = m.roll;
}

/***************************************************************************************
   Rocket: navball-frame pitch/heading errors rotated into the body frame by the current
   roll, PID per axis, heading corrected with yaw. Identical to the original apSteer().
****************************************************************************************/
AttCommand attSteerRocket(AttState &s, const AttGains &g, const AttMeasure &m,
                          float cmdPitch, float cmdHeading,
                          bool holdRoll, float cmdRoll, float maxDeflection, float dt) {
  float ePitch = cmdPitch - m.pitch;                 // + means pitch up
  float eHead  = attWrap180(cmdHeading - m.heading);

  float phi = m.roll * 0.0174532925199f;
  float bodyPitchErr =  ePitch * cosf(phi) + eHead * sinf(phi);
  float bodyYawErr   = -ePitch * sinf(phi) + eHead * cosf(phi);

  bodyPitchErr *= ATT_ERR_NORM;
  bodyYawErr   *= ATT_ERR_NORM;

  s.iPitch = attClampf(s.iPitch + bodyPitchErr * dt, -1.0f, 1.0f);
  s.iYaw   = attClampf(s.iYaw   + bodyYawErr   * dt, -1.0f, 1.0f);

  // D on measurement: body pitch rate approximated by the navball pitch rate rotated by roll.
  float bodyPitchRate = ( s.ratePitch * cosf(phi) + s.rateHeading * sinf(phi)) * ATT_ERR_NORM;
  float bodyYawRate   = (-s.ratePitch * sinf(phi) + s.rateHeading * cosf(phi)) * ATT_ERR_NORM;

  AttCommand c;
  c.pitch = g.pitchKp * bodyPitchErr + g.pitchKi * s.iPitch - g.pitchKd * bodyPitchRate;
  c.yaw   = g.yawKp   * bodyYawErr   + g.yawKi   * s.iYaw   - g.yawKd   * bodyYawRate;
  c.roll  = 0.0f;
  if (holdRoll) {
    float eRoll = attWrap180(cmdRoll - m.roll) * ATT_ERR_NORM;
    s.iRoll = attClampf(s.iRoll + eRoll * dt, -1.0f, 1.0f);
    c.roll  = g.rollKp * eRoll + g.rollKi * s.iRoll - g.rollKd * (s.rateRoll * ATT_ERR_NORM);
  }
  c.pitch = attClampf(c.pitch, -maxDeflection, maxDeflection);
  c.yaw   = attClampf(c.yaw,   -maxDeflection, maxDeflection);
  c.roll  = attClampf(c.roll,  -maxDeflection, maxDeflection);
  return c;
}

/***************************************************************************************
   Aircraft: elevator holds pitch, aileron holds bank, rudder idle (or a small
   coordination term). Heading is the lateral outer loop's job — it commands the bank.
****************************************************************************************/
AttCommand attSteerAircraft(AttState &s, const AttGains &g, const AttMeasure &m,
                            float cmdPitch, float cmdBank, bool coordinateTurn,
                            float maxDeflection, float dt) {
  float ePitch = (cmdPitch - m.pitch) * ATT_ERR_NORM;
  float eRoll  = attWrap180(cmdBank - m.roll) * ATT_ERR_NORM;

  s.iPitch = attClampf(s.iPitch + ePitch * dt, -1.0f, 1.0f);
  s.iRoll  = attClampf(s.iRoll  + eRoll  * dt, -1.0f, 1.0f);

  AttCommand c;
  c.pitch = g.pitchKp * ePitch + g.pitchKi * s.iPitch - g.pitchKd * (s.ratePitch * ATT_ERR_NORM);
  c.roll  = g.rollKp  * eRoll  + g.rollKi  * s.iRoll  - g.rollKd  * (s.rateRoll  * ATT_ERR_NORM);
  c.yaw   = 0.0f;
  if (coordinateTurn) {
    // A little rudder into the bank damps the adverse-yaw wobble KSP planes show in a turn.
    c.yaw = g.yawKp * (m.roll / 60.0f) * 0.25f;
  }
  c.pitch = attClampf(c.pitch, -maxDeflection, maxDeflection);
  c.yaw   = attClampf(c.yaw,   -maxDeflection, maxDeflection);
  c.roll  = attClampf(c.roll,  -maxDeflection, maxDeflection);
  return c;
}

AttGains attRocketGains() {
  AttGains g;
  g.pitchKp = 0.9f; g.pitchKi = 0.05f; g.pitchKd = 0.0f;
  g.yawKp   = 0.9f; g.yawKi   = 0.05f; g.yawKd   = 0.0f;
  g.rollKp  = 0.6f; g.rollKi  = 0.02f; g.rollKd  = 0.0f;
  return g;
}

AttGains attAircraftGains() {
  AttGains g;
  g.pitchKp = 0.60f; g.pitchKi = 0.03f; g.pitchKd = 0.12f;
  g.yawKp   = 0.40f; g.yawKi   = 0.00f; g.yawKd   = 0.00f;
  g.rollKp  = 0.50f; g.rollKi  = 0.01f; g.rollKd  = 0.08f;
  return g;
}

/***************************************************************************************
   burn_autopilot.ino — burn executor, planners and approach-rate hold.
   Contract in burn_autopilot.h; design in Mission_Autopilot.md §4, §7.1, §7.3, §7.7.
****************************************************************************************/
#include "burn_autopilot.h"
#include "attitude_controller.h"
#include "control_links.h"
#include "hold_autopilot.h"     // HP_REASON_* shared reason codes

static const int32_t BP_AXIS_FULL = INT16_MAX;
static const float   BP_G0 = 9.81f;

struct BpTelemetry {
  bool  nodeAvailable = false; float nodeTimeTo = 0, nodeDv = 0, nodeDuration = 0, nodeHeading = 0, nodePitch = 0;
  float ecc = 0, sma = 0, inc = 0, lan = 0, argPe = 0, trueAnom = 0, period = 0;
  float apoapsis = 0, periapsis = 0, timeToAp = 0, timeToPe = 0, velOrbital = 0;
  float heading = 0, pitch = 0, roll = 0, orbVelHeading = 0, orbVelPitch = 0;
  bool  tgtAvailable = false; float tgtDist = 0, tgtVel = 0, tgtHeading = 0, tgtPitch = 0, tgtVelHeading = 0, tgtVelPitch = 0;
  float bodyRadius = 600000.0f, bodyGravity = 9.81f; char bodyName[24] = {0};
  uint32_t lastMs = 0;
};
static BpTelemetry bp_t;
static BurnConfig  bp_c;

static BpMode   bp_mode  = BP_MODE_NONE;
static BpPhase  bp_phase = BP_PHASE_IDLE;
static BurnPlan bp_plan;
static BurnPlan bp_planAtExec;
static bool     bp_autoWarp = true;
static bool     bp_appr = false;
static float    bp_targetAp = 100000.0f, bp_targetPe = 80000.0f, bp_targetInc = 0.0f;
static float    bp_apprRate = -2.0f, bp_apprDist = 50.0f;
static float    bp_thrOut = 0.0f, bp_dvRemaining = 0.0f;
static uint8_t  bp_reason = HP_REASON_NONE; static uint32_t bp_reasonMs = 0;
static uint32_t bp_lastUpdateMs = 0, bp_alignStartMs = 0, bp_settledSince = 0, bp_burnStartMs = 0, bp_doneMs = 0, bp_lastPlanMs = 0;
static float    bp_prevRemaining = 0.0f;
static AttState bp_att;
static bool     bp_incAtAN = true;    // plane change: which node the plan uses
static float    bp_incNodeDt = 0.0f;  // s to that node at plan time
static uint32_t bp_incPlanMs = 0;

static void bpSetReason(uint8_t r) { if (r == HP_REASON_PILOT) r = HP_REASON_NONE; bp_reason = r; bp_reasonMs = millis(); }
static inline float bpMu() { return bp_t.bodyGravity * bp_t.bodyRadius * bp_t.bodyRadius; }
static inline float d2r(float d) { return d * 0.0174532925199f; }

BurnConfig bpDefaultConfig() {
  BurnConfig c;
  c.alignTolDeg = 2.0f; c.alignSettleRate = 0.5f; c.alignSettleMs = 3000; c.alignTimeoutMs = 30000;
  c.alignLeadS = 20.0f; c.taperS = 3.0f; c.throttleFloor = 0.05f; c.cutDv = 0.2f; c.minBurnS = 1.0f;
  c.replanDvFrac = 0.05f; c.replanDvMin = 2.0f; c.replanTignS = 10.0f;
  c.apprKa = 0.5f; c.apprKl = 0.5f; c.apprDeadband = 0.05f; c.apprRateDivisor = 20.0f; c.apprAbortDivisor = 5.0f;
  c.trnSignX = 1.0f; c.trnSignY = 1.0f; c.trnSignZ = 1.0f;
  c.telemetryTimeout = 2000;
  c.apMin = 0.0f; c.apMax = 2.0e9f; c.peMin = 0.0f; c.peMax = 2.0e9f; c.incMin = 0.0f; c.incMax = 180.0f;
  c.apprRateMin = -20.0f; c.apprRateMax = 5.0f; c.apprDistMin = 5.0f; c.apprDistMax = 5000.0f;
  return c;
}

void bpInit() { bp_c = bpDefaultConfig(); bp_mode = BP_MODE_NONE; bp_phase = BP_PHASE_IDLE; bp_plan.valid = false; attReset(bp_att); bp_lastUpdateMs = millis(); }
BurnConfig &bpGetConfig() { return bp_c; }

/***************************************************************************************
   Outputs
****************************************************************************************/
static void bpThrottle(float t) {
  bp_thrOut = attClampf(t, 0.0f, 1.0f);
  thrAutoThrottle(THR_OWNER_BURN, bp_thrOut);
  aeNoteThrottle(bp_thrOut);
}

static void bpReleaseVehicle(bool sasStability) {
  bpThrottle(0.0f);
  thrAutoRelease(THR_OWNER_BURN);
  if (sasStability) { mySimpit.activateAction(SAS_ACTION); mySimpit.setSASMode(AP_STABILITYASSIST); }
  arbReleaseAttitude(AP_OWNER_BURN);
  arbReleaseThrottle(AP_OWNER_BURN);
}

/***************************************************************************************
   Orbital mechanics helpers
****************************************************************************************/
// Time from true anomaly nu0 to nu1 (degrees) on the current orbit, seconds, >= 0.
static float bpTimeBetween(float nu0, float nu1) {
  float e = bp_t.ecc; if (e >= 0.999f || bp_t.period <= 0.0f) return 0.0f;
  auto meanAnom = [&](float nuDeg) {
    float nu = d2r(nuDeg);
    float E = 2.0f * atanf(sqrtf((1.0f - e) / (1.0f + e)) * tanf(nu * 0.5f));
    return E - e * sinf(E);
  };
  float dM = meanAnom(nu1) - meanAnom(nu0);
  while (dM < 0.0f) dM += 2.0f * PI;
  return dM / (2.0f * PI / bp_t.period);
}
static float bpRadiusAt(float nuDeg) {
  float e = bp_t.ecc, a = bp_t.sma;
  return a * (1.0f - e * e) / (1.0f + e * cosf(d2r(nuDeg)));
}
static float bpSpeedAt(float r) { return sqrtf(bpMu() * (2.0f / r - 1.0f / bp_t.sma)); }

/***************************************************************************************
   Planners
****************************************************************************************/
static BurnPlan bpPlanNode() {
  BurnPlan p = {}; p.valid = false;
  if (!bp_t.nodeAvailable || bp_t.nodeDv <= 0.1f) return p;
  p.sasMode = AP_MANEUVER; p.dvTotal = bp_t.nodeDv;
  p.duration = aeBurnDuration(bp_t.nodeDv);
  if (p.duration <= 0.0f) p.duration = bp_t.nodeDuration;    // Simpit's own estimate as fallback
  p.tIgnition = bp_t.nodeTimeTo - p.duration * 0.5f;
  p.warpInstant = TIMEWARP_TO_NEXT_MANEUVER; p.warpDelay = -(p.duration * 0.5f + bp_c.alignLeadS);
  p.valid = true; return p;
}

// AP: burn at periapsis to put apoapsis at the target. PE: mirror at apoapsis.
static BurnPlan bpPlanApsis(bool changeAp) {
  BurnPlan p = {}; p.valid = false;
  if (bp_t.sma <= 0.0f) return p;
  float R = bp_t.bodyRadius, mu = bpMu();
  float rBurn = R + (changeAp ? bp_t.periapsis : bp_t.apoapsis);
  float rOther = R + (changeAp ? bp_targetAp : bp_targetPe);
  if (rBurn <= 0.0f || rOther <= 0.0f) return p;
  float v  = sqrtf(mu * (2.0f / rBurn - 1.0f / bp_t.sma));
  float a2 = (rBurn + rOther) * 0.5f;
  float v2 = sqrtf(mu * (2.0f / rBurn - 1.0f / a2));
  float dv = v2 - v;
  if (fabsf(dv) < 0.5f) return p;
  p.sasMode = (dv > 0.0f) ? AP_PROGRADE : AP_RETROGRADE;
  p.dvTotal = dv;
  p.duration = aeBurnDuration(dv);
  float tTo = changeAp ? bp_t.timeToPe : bp_t.timeToAp;
  p.tIgnition = tTo - p.duration * 0.5f;
  p.warpInstant = changeAp ? TIMEWARP_TO_PERIAPSIS : TIMEWARP_TO_APOAPSIS;
  p.warpDelay = -(p.duration * 0.5f + bp_c.alignLeadS);
  p.valid = true; return p;
}

static BurnPlan bpPlanInc() {
  BurnPlan p = {}; p.valid = false;
  if (bp_t.sma <= 0.0f || bp_t.period <= 0.0f) return p;
  float di = bp_targetInc - bp_t.inc;
  if (fabsf(di) < 0.05f) return p;
  float nuAN = attWrap360(360.0f - bp_t.argPe), nuDN = attWrap360(180.0f - bp_t.argPe);
  float tAN = bpTimeBetween(bp_t.trueAnom, nuAN), tDN = bpTimeBetween(bp_t.trueAnom, nuDN);
  bp_incAtAN = (tAN <= tDN);
  float nuNode = bp_incAtAN ? nuAN : nuDN;
  float tNode  = bp_incAtAN ? tAN : tDN;
  float vNode  = bpSpeedAt(bpRadiusAt(nuNode));
  float dv = 2.0f * vNode * sinf(d2r(fabsf(di)) * 0.5f);
  // Raising inclination at the ascending node is a normal burn; at the descending node it is mirrored.
  bool normal = (di > 0.0f) == bp_incAtAN;
  p.sasMode = normal ? AP_NORMAL : AP_ANTINORMAL;
  p.dvTotal = dv;
  p.duration = aeBurnDuration(dv);
  p.tIgnition = tNode - p.duration * 0.5f;
  p.warpInstant = TIMEWARP_TO_NOW; p.warpDelay = p.tIgnition - bp_c.alignLeadS;
  bp_incNodeDt = tNode; bp_incPlanMs = millis();
  p.valid = true; return p;
}

static BurnPlan bpPlan(BpMode m) {
  switch (m) {
    case BP_MODE_NODE: return bpPlanNode();
    case BP_MODE_AP:   return bpPlanApsis(true);
    case BP_MODE_PE:   return bpPlanApsis(false);
    case BP_MODE_INC:  return bpPlanInc();
    default: { BurnPlan p = {}; p.valid = false; return p; }
  }
}

// Live ignition time and remaining delta-V for the armed plan.
static float bpTIgnitionLive() {
  switch (bp_mode) {
    case BP_MODE_NODE: return bp_t.nodeTimeTo - bp_plan.duration * 0.5f;
    case BP_MODE_AP:   return bp_t.timeToPe - bp_plan.duration * 0.5f;
    case BP_MODE_PE:   return bp_t.timeToAp - bp_plan.duration * 0.5f;
    case BP_MODE_INC:  return bp_incNodeDt - (millis() - bp_incPlanMs) * 0.001f - bp_plan.duration * 0.5f;
    default: return 0.0f;
  }
}

static float bpRemainingLive() {
  float mu = bpMu(), a = bp_t.sma;
  switch (bp_mode) {
    case BP_MODE_NODE: return bp_t.nodeDv;
    case BP_MODE_AP: {   // burning at Pe: d(Ap)/dv = 4 a^2 v / mu
      float v = bp_t.velOrbital > 1.0f ? bp_t.velOrbital : 1.0f;
      float dAp = bp_targetAp - bp_t.apoapsis;
      return (bp_plan.dvTotal >= 0.0f ? dAp : -dAp) * mu / (4.0f * a * a * v);
    }
    case BP_MODE_PE: {
      float v = bp_t.velOrbital > 1.0f ? bp_t.velOrbital : 1.0f;
      float dPe = bp_targetPe - bp_t.periapsis;
      return (bp_plan.dvTotal >= 0.0f ? dPe : -dPe) * mu / (4.0f * a * a * v);
    }
    case BP_MODE_INC: {
      float di = bp_targetInc - bp_t.inc;
      float sign = (bp_plan.dvTotal >= 0.0f && ((di > 0.0f) == bp_incAtAN) == (bp_plan.sasMode == AP_NORMAL)) ? 1.0f : 1.0f;
      (void)sign;
      return 2.0f * bp_t.velOrbital * sinf(d2r(fabsf(di)) * 0.5f) * ((fabsf(di) > 0.0f) ? 1.0f : 0.0f);
    }
    default: return 0.0f;
  }
}

/***************************************************************************************
   Pointing reference for alignment (navball frame heading / pitch)
****************************************************************************************/
static void bpPointingRef(float &hdg, float &pitch) {
  switch (bp_plan.sasMode) {
    case AP_MANEUVER:   hdg = bp_t.nodeHeading; pitch = bp_t.nodePitch; break;
    case AP_PROGRADE:   hdg = bp_t.orbVelHeading; pitch = bp_t.orbVelPitch; break;
    case AP_RETROGRADE: hdg = attWrap360(bp_t.orbVelHeading + 180.0f); pitch = -bp_t.orbVelPitch; break;
    // The orbit normal is perpendicular to the radius vector, so it is horizontal, at the
    // orbital prograde heading -90 deg (anti-normal +90). Review decision q.2.
    case AP_NORMAL:     hdg = attWrap360(bp_t.orbVelHeading - 90.0f); pitch = 0.0f; break;
    default:            hdg = attWrap360(bp_t.orbVelHeading + 90.0f); pitch = 0.0f; break;
  }
}

static float bpPointingError() {
  float h, p; bpPointingRef(h, p);
  float a1 = d2r(bp_t.pitch), a2 = d2r(p), dh = d2r(attWrap180(bp_t.heading - h));
  float c = sinf(a1) * sinf(a2) + cosf(a1) * cosf(a2) * cosf(dh);
  return acosf(attClampf(c, -1.0f, 1.0f)) * 57.2957795131f;
}

/***************************************************************************************
   Arm / execute / abort
****************************************************************************************/
static bool bpPlanChanged(const BurnPlan &a, const BurnPlan &b) {
  float dvTol = fmaxf(bp_c.replanDvMin, fabsf(a.dvTotal) * bp_c.replanDvFrac);
  return fabsf(a.dvTotal - b.dvTotal) > dvTol || fabsf(a.tIgnition - b.tIgnition) > bp_c.replanTignS || a.sasMode != b.sasMode;
}

bool bpArm(BpMode mode, bool on) {
  if (!on) {
    if (bp_mode == mode) { if (bp_phase >= BP_PHASE_ALIGN && bp_phase <= BP_PHASE_BURN) bpAbort(HP_REASON_PILOT); bp_mode = BP_MODE_NONE; bp_phase = BP_PHASE_IDLE; bp_plan.valid = false; }
    return true;
  }
  if (mode == BP_MODE_NONE || mode > BP_MODE_INC) return false;
  if ((millis() - bp_t.lastMs) > bp_c.telemetryTimeout) { bpSetReason(HP_REASON_REFUSED); return false; }
  if (bp_phase >= BP_PHASE_ALIGN && bp_phase <= BP_PHASE_BURN) bpAbort(HP_REASON_PILOT);
  BurnPlan p = bpPlan(mode);
  if (!p.valid) { bpSetReason(mode == BP_MODE_NODE ? HP_REASON_NO_NODE : HP_REASON_REFUSED); return false; }
  if (fabsf(p.dvTotal) > aeStageDv() + 1.0f && !asEnabled()) { bpSetReason(HP_REASON_FUEL); return false; }
  if (bp_appr) bpEngageApproach(false);          // exclusive with the burn modes
  bp_mode = mode; bp_plan = p; bp_phase = BP_PHASE_PLANNED; bp_lastPlanMs = millis();
  return true;
}

bool bpExecute() {
  uint32_t now = millis();
  if (bp_phase == BP_PHASE_PLANNED) {
    if (!bp_plan.valid) return false;
    arbTakeAttitude(AP_OWNER_BURN); arbTakeThrottle(AP_OWNER_BURN);
    mySimpit.activateAction(SAS_ACTION);
    mySimpit.setSASMode(bp_plan.sasMode);
    attReset(bp_att);
    bp_planAtExec = bp_plan;
    bp_phase = BP_PHASE_ALIGN; bp_alignStartMs = now; bp_settledSince = 0;
    return true;
  }
  if (bp_phase == BP_PHASE_WARP_READY && bp_autoWarp) {
    if (!arbCanWarp(AP_OWNER_BURN)) return false;
    timewarpToMessage w;
    w.instant = bp_plan.warpInstant;
    w.delay   = bp_plan.warpDelay;
    mySimpit.send(TIMEWARP_TO_MESSAGE, w);
    bp_phase = BP_PHASE_WARP;
    return true;
  }
  return false;
}

void bpAbort(uint8_t reason) {
  bool was = (bp_phase >= BP_PHASE_ALIGN && bp_phase <= BP_PHASE_BURN);
  if (was) { bpReleaseVehicle(true); bp_phase = BP_PHASE_ABORT; bp_doneMs = millis(); bpSetReason(reason); }
  else if (bp_phase != BP_PHASE_IDLE) { bp_phase = BP_PHASE_IDLE; }
  bp_mode = BP_MODE_NONE; bp_plan.valid = false;
  if (bp_appr) { bp_appr = false; bpReleaseVehicle(true); if (!was) bpSetReason(reason); }
}

void bpArbiterDrop() { bpAbort(HP_REASON_OTHER_AP); }

bool bpEngageApproach(bool on) {
  if (!on) { if (bp_appr) { bp_appr = false; bpReleaseVehicle(true); } return true; }
  if (!bp_t.tgtAvailable) { bpSetReason(HP_REASON_NO_TARGET); return false; }
  if (bp_phase != BP_PHASE_IDLE) { bp_mode = BP_MODE_NONE; bp_phase = BP_PHASE_IDLE; bp_plan.valid = false; }
  arbTakeAttitude(AP_OWNER_BURN);
  mySimpit.activateAction(SAS_ACTION);
  mySimpit.setSASMode(AP_TARGET);
  bp_appr = true;
  return true;
}

static bool bpInRange(float v, float lo, float hi) { return !(v < lo || v > hi); }
bool bpSetTargetAp(float v)  { if (!bpInRange(v, bp_c.apMin, bp_c.apMax)) return false; bp_targetAp = v; if (bp_mode == BP_MODE_AP && bp_phase == BP_PHASE_PLANNED) bp_plan = bpPlan(bp_mode); return true; }
bool bpSetTargetPe(float v)  { if (!bpInRange(v, bp_c.peMin, bp_c.peMax)) return false; bp_targetPe = v; if (bp_mode == BP_MODE_PE && bp_phase == BP_PHASE_PLANNED) bp_plan = bpPlan(bp_mode); return true; }
bool bpSetTargetInc(float v) { if (!bpInRange(v, bp_c.incMin, bp_c.incMax)) return false; bp_targetInc = v; if (bp_mode == BP_MODE_INC && bp_phase == BP_PHASE_PLANNED) bp_plan = bpPlan(bp_mode); return true; }
bool bpSetApprRate(float v)  { if (!bpInRange(v, bp_c.apprRateMin, bp_c.apprRateMax)) return false; bp_apprRate = v; return true; }
bool bpSetApprDist(float v)  { if (!bpInRange(v, bp_c.apprDistMin, bp_c.apprDistMax)) return false; bp_apprDist = v; return true; }
void bpSetAutoWarp(bool on)  { bp_autoWarp = on; }

bool bpArmed()      { return bp_phase >= BP_PHASE_PLANNED && bp_phase <= BP_PHASE_BURN; }
bool bpExecuting()  { return bp_phase >= BP_PHASE_ALIGN && bp_phase <= BP_PHASE_BURN; }
bool bpAnyEngaged() { return bpArmed() || bp_appr; }

/***************************************************************************************
   Approach-rate hold: SAS target mode keeps the nose on the target, so the body frame is
   roughly the line-of-sight frame. Relative velocity and line of sight are rotated into
   the body frame with the vessel's heading, pitch and roll; forward translation closes
   the rate error, lateral translation nulls the sideways components.
****************************************************************************************/
static void bpVec(float hdg, float pitch, float v[3]) {   // navball frame: N, E, Up
  float p = d2r(pitch), h = d2r(hdg);
  v[0] = cosf(p) * cosf(h); v[1] = cosf(p) * sinf(h); v[2] = sinf(p);
}
static float bpDot(const float a[3], const float b[3]) { return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; }
static void  bpCross(const float a[3], const float b[3], float o[3]) {
  o[0] = a[1]*b[2] - a[2]*b[1]; o[1] = a[2]*b[0] - a[0]*b[2]; o[2] = a[0]*b[1] - a[1]*b[0];
}

static void bpUpdateApproach(float dt) {
  (void)dt;
  if (!bp_t.tgtAvailable) { bpEngageApproach(false); bpSetReason(HP_REASON_NO_TARGET); return; }
  float range = bp_t.tgtDist;
  // Body axes from attitude: forward, right, up (roll rotates right/up about forward)
  float fwd[3], r0[3], up0[3], right[3], up[3];
  bpVec(bp_t.heading, bp_t.pitch, fwd);
  bpVec(bp_t.heading + 90.0f, 0.0f, r0);
  bpCross(r0, fwd, up0);
  float cr = cosf(d2r(bp_t.roll)), sr = sinf(d2r(bp_t.roll));
  for (int i = 0; i < 3; i++) { right[i] = r0[i] * cr + up0[i] * sr; up[i] = up0[i] * cr - r0[i] * sr; }
  float los[3], rel[3];
  bpVec(bp_t.tgtHeading, bp_t.tgtPitch, los);
  bpVec(bp_t.tgtVelHeading, bp_t.tgtVelPitch, rel);
  for (int i = 0; i < 3; i++) rel[i] *= bp_t.tgtVel;
  float vAlong = bpDot(rel, los);                    // + = opening
  float vLat[3]; for (int i = 0; i < 3; i++) vLat[i] = rel[i] - vAlong * los[i];
  if (vAlong < -range / bp_c.apprAbortDivisor && range > bp_apprDist) {
    bpEngageApproach(false); bpSetReason(HP_REASON_REFUSED); return;   // closing too fast for the range
  }
  float rateSp = bp_apprRate;                                          // negative = closing
  float maxClose = -range / bp_c.apprRateDivisor;
  if (rateSp < maxClose) rateSp = maxClose;
  if (range <= bp_apprDist) rateSp = 0.0f;
  float eAlong = rateSp - vAlong;
  float cmdFwd  = attClampf(bp_c.apprKa * eAlong, -1.0f, 1.0f);
  float cmdRight = attClampf(-bp_c.apprKl * bpDot(vLat, right), -1.0f, 1.0f);
  float cmdUp    = attClampf(-bp_c.apprKl * bpDot(vLat, up),    -1.0f, 1.0f);
  if (fabsf(eAlong) < bp_c.apprDeadband) cmdFwd = 0.0f;
  if (fabsf(bpDot(vLat, right)) < bp_c.apprDeadband) cmdRight = 0.0f;
  if (fabsf(bpDot(vLat, up)) < bp_c.apprDeadband) cmdUp = 0.0f;
  translationMessage t;
  t.setX((int16_t)(cmdRight * bp_c.trnSignX * (float)BP_AXIS_FULL));
  t.setY((int16_t)(cmdUp    * bp_c.trnSignY * (float)BP_AXIS_FULL));
  t.setZ((int16_t)(cmdFwd   * bp_c.trnSignZ * (float)BP_AXIS_FULL));
  mySimpit.send(TRANSLATION_MESSAGE, t);
}

/***************************************************************************************
   Executor
****************************************************************************************/
void bpUpdate() {
  uint32_t now = millis();
  float dt = (now - bp_lastUpdateMs) * 0.001f;
  bp_lastUpdateMs = now;
  if (dt <= 0.0f || dt > 0.5f) dt = 0.02f;

  if ((bp_phase == BP_PHASE_DONE || bp_phase == BP_PHASE_ABORT) && now - bp_doneMs > 5000) bp_phase = BP_PHASE_IDLE;
  if (!bpAnyEngaged()) return;

  if ((now - bp_t.lastMs) > bp_c.telemetryTimeout) { bpAbort(HP_REASON_TELEMETRY); return; }
  uint8_t ovr;
  if (pilotOverrideDetected(ovr)) { bpAbort(ovr); return; }

  if (bp_appr) { bpUpdateApproach(dt); if (!bpArmed()) return; }

  // Node vanished / stage spent
  if (bp_mode == BP_MODE_NODE && !bp_t.nodeAvailable && bp_phase != BP_PHASE_BURN) { bpAbort(HP_REASON_NO_NODE); return; }

  // Re-plan while the pilot can still see it: a material change drops back to PLANNED.
  if (bp_phase == BP_PHASE_PLANNED || bp_phase == BP_PHASE_ALIGN || bp_phase == BP_PHASE_WARP_READY) {
    if (now - bp_lastPlanMs >= 1000) {
      bp_lastPlanMs = now;
      BurnPlan p = bpPlan(bp_mode);
      if (p.valid) {
        if (bp_phase != BP_PHASE_PLANNED && bpPlanChanged(p, bp_planAtExec)) {
          bpReleaseVehicle(true); bp_phase = BP_PHASE_PLANNED; bpSetReason(HP_REASON_REPLAN);
        }
        bp_plan = p;
      }
    }
  }

  AttMeasure m; m.pitch = bp_t.pitch; m.heading = bp_t.heading; m.roll = bp_t.roll;
  attUpdateRates(bp_att, m, dt);

  switch (bp_phase) {
    case BP_PHASE_ALIGN: {
      float err = bpPointingError();
      float rate = fmaxf(fabsf(bp_att.ratePitch), fmaxf(fabsf(bp_att.rateHeading), fabsf(bp_att.rateRoll)));
      if (rate < bp_c.alignSettleRate) { if (bp_settledSince == 0) bp_settledSince = now; } else bp_settledSince = 0;
      bool settled = bp_settledSince && (now - bp_settledSince >= bp_c.alignSettleMs);
      if (err < bp_c.alignTolDeg && settled) bp_phase = BP_PHASE_WARP_READY;
      else if (now - bp_alignStartMs > bp_c.alignTimeoutMs) { bpAbort(HP_REASON_ALIGN); return; }
      break;
    }
    case BP_PHASE_WARP_READY:
    case BP_PHASE_WARP: {
      // Alignment is re-checked continuously (a pilot warp can disturb it); ignition when due.
      if (bpPointingError() > bp_c.alignTolDeg * 2.0f && bp_phase == BP_PHASE_WARP_READY) { bp_phase = BP_PHASE_ALIGN; bp_alignStartMs = now; bp_settledSince = 0; break; }
      if (bpTIgnitionLive() <= 0.0f) {
        bp_phase = BP_PHASE_BURN; bp_burnStartMs = now; bp_prevRemaining = bpRemainingLive();
      }
      break;
    }
    case BP_PHASE_BURN: {
      float rem = bpRemainingLive();
      bp_dvRemaining = rem;
      float a = aeAccel();
      float taperBand = (a > 0.0f) ? a * bp_c.taperS : 5.0f;
      float thr = 1.0f;
      if (rem < taperBand) thr = fmaxf(bp_c.throttleFloor, rem / taperBand);
      bool overshoot = (now - bp_burnStartMs > (uint32_t)(bp_c.minBurnS * 1000)) && (rem > bp_prevRemaining + 0.05f);
      if (rem <= bp_c.cutDv || overshoot) {
        bpReleaseVehicle(true);
        bp_phase = BP_PHASE_DONE; bp_doneMs = now; bp_mode = BP_MODE_NONE; bp_plan.valid = false;
        break;
      }
      bp_prevRemaining = rem;
      if (aeStageDv() < 5.0f && !asEnabled() && rem > 5.0f) { bpAbort(HP_REASON_FUEL); return; }
      bpThrottle(thr);
      asMaybeStage(asEnabled(), thr, rem);
      break;
    }
    default: break;
  }
}

/***************************************************************************************
   Status / console
****************************************************************************************/
static uint8_t bpAge(uint8_t r, uint32_t ms) { if (r == HP_REASON_NONE) return 255; uint32_t a = (millis() - ms) / 1000UL; return a > 255 ? 255 : (uint8_t)a; }

BurnStatus bpGetStatus() {
  BurnStatus s;
  s.mode = bp_mode; s.phase = bp_phase; s.reason = bp_reason; s.reasonAge = bpAge(bp_reason, bp_reasonMs);
  s.armed = bpArmed(); s.executing = bpExecuting(); s.autoWarp = bp_autoWarp; s.autoStage = asEnabled();
  s.targetAvailable = bp_t.tgtAvailable; s.nodeAvailable = bp_t.nodeAvailable; s.apprEngaged = bp_appr;
  s.targetAp = bp_targetAp; s.targetPe = bp_targetPe; s.targetInc = bp_targetInc; s.apprRate = bp_apprRate; s.apprDist = bp_apprDist;
  s.dvTotal = bp_plan.valid ? fabsf(bp_plan.dvTotal) : 0.0f;
  s.dvRemaining = (bp_phase == BP_PHASE_BURN) ? bp_dvRemaining : s.dvTotal;
  s.tIgnition = bp_plan.valid ? bpTIgnitionLive() : 0.0f;
  s.burnDuration = bp_plan.valid ? bp_plan.duration : 0.0f;
  s.accelEst = aeAccel(); s.cmdThrottle = bp_thrOut;
  return s;
}

const char *bpPhaseName(uint8_t p) {
  switch (p) {
    case BP_PHASE_PLANNED: return "PLANNED"; case BP_PHASE_ALIGN: return "ALIGNING"; case BP_PHASE_WARP_READY: return "WARP READY";
    case BP_PHASE_WARP: return "WARPING"; case BP_PHASE_BURN: return "BURNING"; case BP_PHASE_DONE: return "DONE"; case BP_PHASE_ABORT: return "ABORT";
    default: return "IDLE";
  }
}

bool bpConsoleLine(const char *line) {
  if (strncasecmp(line, "ARM ", 4) == 0) {
    const char *n = line + 4;
    BpMode m = strncasecmp(n, "NODE", 4) == 0 ? BP_MODE_NODE : strncasecmp(n, "AP", 2) == 0 ? BP_MODE_AP :
               strncasecmp(n, "PE", 2) == 0 ? BP_MODE_PE : strncasecmp(n, "INC", 3) == 0 ? BP_MODE_INC : BP_MODE_NONE;
    return bpArm(m, true);
  }
  if (strncasecmp(line, "EXEC", 4) == 0)  return bpExecute();
  if (strncasecmp(line, "APPR ", 5) == 0) return bpEngageApproach(atoi(line + 5) != 0);
  if (strncasecmp(line, "WARP ", 5) == 0) { bpSetAutoWarp(atoi(line + 5) != 0); return true; }
  if (strncasecmp(line, "STAGE ", 6) == 0) { asSetEnabled(atoi(line + 6) != 0); return true; }
  if (strncasecmp(line, "SET ", 4) == 0) {
    const char *a = line + 4; const char *sp = strchr(a, ' '); if (!sp) return false; float v = atof(sp + 1);
    if (strncasecmp(a, "AP", 2) == 0 && a[2] == ' ')  return bpSetTargetAp(v);
    if (strncasecmp(a, "PE", 2) == 0 && a[2] == ' ')  return bpSetTargetPe(v);
    if (strncasecmp(a, "INC", 3) == 0) return bpSetTargetInc(v);
    if (strncasecmp(a, "RATE", 4) == 0) return bpSetApprRate(v);
    if (strncasecmp(a, "DIST", 4) == 0) return bpSetApprDist(v);
    return false;
  }
  if (strncasecmp(line, "OFF", 3) == 0) { bpAbort(HP_REASON_PILOT); return true; }
  if (strncasecmp(line, "STATUS", 6) == 0) {
    BurnStatus s = bpGetStatus();
    Serial.print(F("BP mode=")); Serial.print(s.mode); Serial.print(F(" phase=")); Serial.print(bpPhaseName(s.phase));
    Serial.print(F(" dv=")); Serial.print(s.dvTotal, 1); Serial.print(F(" rem=")); Serial.print(s.dvRemaining, 1);
    Serial.print(F(" tign=")); Serial.print(s.tIgnition, 0); Serial.print(F(" dur=")); Serial.print(s.burnDuration, 0);
    Serial.print(F(" a=")); Serial.print(s.accelEst, 2); Serial.print(F(" reason=")); Serial.println(hpReasonName(s.reason));
    return true;
  }
  return false;
}

/***************************************************************************************
   Telemetry ingest
****************************************************************************************/
static inline void bpStamp() { bp_t.lastMs = millis(); }
void bpIngestNode(float timeTo, float dv, float duration, float heading, float pitch) {
  bp_t.nodeAvailable = (dv > 0.01f); bp_t.nodeTimeTo = timeTo; bp_t.nodeDv = dv; bp_t.nodeDuration = duration;
  bp_t.nodeHeading = heading; bp_t.nodePitch = pitch; bpStamp();
}
void bpIngestOrbit(float ecc, float sma, float inc, float lan, float argPe, float trueAnom, float period) {
  bp_t.ecc = ecc; bp_t.sma = sma; bp_t.inc = inc; bp_t.lan = lan; bp_t.argPe = argPe; bp_t.trueAnom = trueAnom; bp_t.period = period; bpStamp();
}
void bpIngestApsides(float apoapsis, float periapsis) { bp_t.apoapsis = apoapsis; bp_t.periapsis = periapsis; bpStamp(); }
void bpIngestApsidesTime(float toAp, float toPe)      { bp_t.timeToAp = toAp; bp_t.timeToPe = toPe; bpStamp(); }
void bpIngestVelocity(float orbital)                  { bp_t.velOrbital = orbital; bpStamp(); }
void bpIngestAttitude(float heading, float pitch, float roll, float orbVelHeading, float orbVelPitch) {
  bp_t.heading = heading; bp_t.pitch = pitch; bp_t.roll = roll; bp_t.orbVelHeading = orbVelHeading; bp_t.orbVelPitch = orbVelPitch; bpStamp();
}
void bpIngestTarget(bool available, float distance, float velocity, float heading, float pitch, float velHeading, float velPitch) {
  bp_t.tgtAvailable = available; bp_t.tgtDist = distance; bp_t.tgtVel = velocity; bp_t.tgtHeading = heading; bp_t.tgtPitch = pitch;
  bp_t.tgtVelHeading = velHeading; bp_t.tgtVelPitch = velPitch;
}
void bpIngestBody(float radius, float gravity, const char *name) {
  bool changed = name && strncmp(name, bp_t.bodyName, sizeof(bp_t.bodyName)) != 0;
  if (radius > 0.0f) bp_t.bodyRadius = radius;
  if (gravity > 0.0f) bp_t.bodyGravity = gravity;
  if (name) { strncpy(bp_t.bodyName, name, sizeof(bp_t.bodyName) - 1); bp_t.bodyName[sizeof(bp_t.bodyName) - 1] = '\0'; }
  if (changed && bpAnyEngaged()) bpAbort(HP_REASON_SOI);
}
void bpVesselChanged() {
  bp_mode = BP_MODE_NONE; bp_phase = BP_PHASE_IDLE; bp_plan.valid = false; bp_appr = false;
  arbReleaseAttitude(AP_OWNER_BURN); arbReleaseThrottle(AP_OWNER_BURN);
  bp_reason = HP_REASON_NONE; bp_t.lastMs = millis();
}

/***************************************************************************************
   SET FAN CONTROL
   Gets fan information and then adjusts the fan duty cycle based on internal temp
   - No inputs
   - No outputs
****************************************************************************************/
void setFanControl() {

  uint8_t dutyCycle = 30;

  tempC = fan.getInternalTemperature();
  dutyCycle = fan.getDutyCycle();
  rpm = fan.getFanRPM();

  if (tempC > 50) {
    dutyCycle = 100;
  } else if (tempC > 40) {
    dutyCycle = 70;
  } else if (tempC > 30) {
    dutyCycle = 50;
  } else if (tempC > 25) {
    dutyCycle = 40;
  } else {
    dutyCycle = 30;
  }

  fan.setDutyCycle(dutyCycle);
}


/***************************************************************************************
   BUTTON PRESSED CHECK
   Function to check whether a button (already debounced) has transitioned from off
   (returned 0) to on (returned 1) indicating user has pushed it down
   - INPUTS:
    - {uint16_t} prevButton = 16 bit register of previous button conditions
    - {uint16_t} newButton = 16 bit register of updated button conditions
    - {uint16_t} mask = mask to use for the compare; ensure coorect function
      definition is being operated on
   - Returns bool
****************************************************************************************/
bool buttonPressed(uint16_t prevButton, uint16_t newButton, uint16_t mask) {
  //logic check that the toggle has been activated but that the previous state was 0
  if (newButton & mask && ~prevButton & mask) {
    return true;
  } else {
    return false;
  }
}


/***************************************************************************************
   BUTTON RELEASED CHECK
   Function to check whether a button (already debounced) has transitioned from on
   (returned 1) to off (returned 0) indicating user has let it go
   - INPUTS:
    - {uint16_t} prevButton = 16 bit register of previous button conditions
    - {uint16_t} newButton = 16 bit register of updated button conditions
    - {uint16_t} mask = mask to use for the compare; ensure coorect function
      definition is being operated on
   - Returns bool
****************************************************************************************/
bool buttonReleased(uint16_t prevButton, uint16_t newButton, uint16_t mask) {
  //logic check that the toggle has been activated but that the previous state was 1
  if (~newButton & mask && prevButton & mask) {
    return true;
  } else {
    return false;
  }
}


/***************************************************************************************
   TOGGLE REGISTER BIT
   Function to change bit state in declared register. Uses logical XOR operator
   - INPUTS:
    - {uint16_t} registerInput = value of register that needs to be operated on
    - {uint16_t} mask = mask to use determin which bits are operated on; ensure
      correct function definition is being operated on
   - OUTPUTS:
    - {uint16_t} registerOutput = resultant register with modified bit
****************************************************************************************/
uint16_t toggleBit(uint16_t registerInput, uint16_t mask) {
  uint16_t registerOutput;
  registerOutput = registerInput ^ mask;

  return registerOutput;
}


/***************************************************************************************
   TOGGLE REGISTER BIT
   Function to change bit state in declared register. Uses logical XOR operator
   - INPUTS:
    - {uint8_t} registerInput = value of register that needs to be operated on
    - {uint8_t} mask = mask to use determin which bits are operated on; ensure
      correct function definition is being operated on
   - OUTPUTS:
    - {uint8_t} registerOutput = resultant register with modified bit
****************************************************************************************/
uint8_t toggleBit(uint8_t registerInput, uint8_t mask) {
  uint8_t registerOutput;
  registerOutput = registerInput ^ mask;

  return registerOutput;
}


/***************************************************************************************
   SET REGISTER BIT
   Function to change bit state to 1 in declared register. Uses logical OR operator
   - INPUTS:
    - {uint16_t} registerInput = value of register that needs to be operated on
    - {uint16_t} mask = mask to use determin which bits are operated on; ensure
      correct function definition is being operated on
   - OUTPUTS:
    - {uint16_t} registerOutput = resultant register with modified bit
****************************************************************************************/
uint16_t setBit(uint16_t registerInput, uint16_t mask) {
  uint16_t registerOutput;
  registerOutput = registerInput | mask;

  return registerOutput;
}


/***************************************************************************************
   SET REGISTER BIT
   Function to change bit state to 1 in declared register. Uses logical OR operator
   - INPUTS:
    - {uint8_t} registerInput = value of register that needs to be operated on
    - {uint8_t} mask = mask to use determin which bits are operated on; ensure
      correct function definition is being operated on
   - OUTPUTS:
    - {uint8_t} registerOutput = resultant register with modified bit
****************************************************************************************/
uint8_t setBit(uint8_t registerInput, uint8_t mask) {
  uint8_t registerOutput;
  registerOutput = registerInput | mask;

  return registerOutput;
}


/***************************************************************************************
   CLEAR REGISTER BIT
   Function to change bit state to 0 in declared register. Uses logical AND operator
   - INPUTS:
    - {uint16_t} registerInput = value of register that needs to be operated on
    - {uint16_t} mask = mask to use determin which bits are operated on; ensure
      correct function definition is being operated on
   - OUTPUTS:
    - {uint16_t} registerOutput = resultant register with modified bit
****************************************************************************************/
uint16_t clearBit(uint16_t registerInput, uint16_t mask) {
  uint16_t registerOutput;
  registerOutput = registerInput & ~mask;

  return registerOutput;
}


/***************************************************************************************
   CLEAR REGISTER BIT
   Function to change bit state to 0 in declared register. Uses logical AND operator
   - INPUTS:
    - {uint8_t} registerInput = value of register that needs to be operated on
    - {uint8_t} mask = mask to use determin which bits are operated on; ensure
      correct function definition is being operated on
   - OUTPUTS:
    - {uint8_t} registerOutput = resultant register with modified bit
****************************************************************************************/
uint8_t clearBit(uint8_t registerInput, uint8_t mask) {
  uint8_t registerOutput;
  registerOutput = registerInput & ~mask;

  return registerOutput;
}


/***************************************************************************************
   IS A BIT ENABLED CHECK
   Function to check whether a bit in the referenced register is set
   - INPUTS:
    - {uint16_t} registerInput = value of register that needs to be operated on
    - {uint16_t} mask = mask to use determin which bits are operated on; ensure
      correct function definition is being operated on
   - Returns bool
****************************************************************************************/
bool isBitEnabled(uint16_t registerInput, uint16_t mask) {
  if (registerInput & mask) {
    return true;
  } else {
    return false;
  }
}


/***************************************************************************************
   IS A BIT ENABLED CHECK
   Function to check whether a bit in the referenced register is set
   - INPUTS:
    - {uint8_t} registerInput = value of register that needs to be operated on
    - {uint8_t} mask = mask to use determin which bits are operated on; ensure
      correct function definition is being operated on
   - Returns bool
****************************************************************************************/
bool isBitEnabled(uint8_t registerInput, uint8_t mask) {
  if (registerInput & mask) {
    return true;
  } else {
    return false;
  }
}


/***************************************************************************************
   SET ACTION GROUPS
   Sets action group inputs based on current Control Group
   - No inputs
   - No outputs
****************************************************************************************/
void setActionGroups() {
  // AGX = base + (controlGroup - 1) * stride, with control group 1..6
  // (Module UI Reference v5.4). ctrlGrp is clamped to that range.
  uint8_t grp = (ctrlGrp >= 1 && ctrlGrp <= 6) ? ctrlGrp : 1;
  uint8_t grpMod = (uint8_t)((grp - 1) * ctrlGrpAdd);

  ag1  = grpMod + ag1_base;
  ag2  = grpMod + ag2_base;
  ag3  = grpMod + ag3_base;
  ag4  = grpMod + ag4_base;
  ag5  = grpMod + ag5_base;
  ag6  = grpMod + ag6_base;
  ag7  = grpMod + ag7_base;
  ag8  = grpMod + ag8_base;
  ag9  = grpMod + ag9_base;
  ag10 = grpMod + ag10_base;
  ag11 = grpMod + ag11_base;
  ag12 = grpMod + ag12_base;

  antenna             = grpMod + antenna_base;
  fuel_cell           = grpMod + fuel_cell_base;
  solar_array         = grpMod + solar_array_base;
  cargo_door          = grpMod + cargo_door_base;
  radiator            = grpMod + radiator_base;
  ladder              = grpMod + ladder_base;
  heat_shield_deploy  = grpMod + heat_shield_deploy_base;
  heat_shield_release = grpMod + heat_shield_release_base;
  parachute           = grpMod + parachute_base;       // main chute deploy
  main_chute_cut      = grpMod + main_chute_cut_base;
  drogue              = grpMod + drogue_base;           // drogue deploy
  drogue_cut          = grpMod + drogue_cut_base;

  les           = grpMod + les_base;
  fairing       = grpMod + fairing_base;
  engineMode    = grpMod + engineMode_base;
  collectSci    = grpMod + collectSci_base;
  engine1       = grpMod + engine1_base;
  science1      = grpMod + science1_base;
  engine2       = grpMod + engine2_base;
  science2      = grpMod + science2_base;
  intake        = grpMod + intake_base;
  lock_surfaces = grpMod + lock_surfaces_base;

  cp_primary   = grpMod + cp_primary_base;
  cp_alternate = grpMod + cp_alternate_base;
  cp_docking   = grpMod + cp_docking_base;
  airbrake     = grpMod + airbrake_base;
  rw_disable   = grpMod + rw_disable_base;
}


/***************************************************************************************
   SET SAS LED State
   Sets the SAS leds based on the SAS mode definition
   - INPUTS:
    - {byte} SAS_input = value of SAS definition from Simpit
   - No outputs
****************************************************************************************/
uint16_t setSASLEDState(uint8_t SAS_input) {
  uint16_t ledOutput = 0;
  switch (SAS_input) {
    case 255:  //SMOFF
      break;
    case AP_STABILITYASSIST:  //SAS Stability Assit
      ledOutput = setBit(ledOutput, pSASEnable);
      ledOutput = setBit(ledOutput, pStabAssist);
      break;
    case AP_MANEUVER:  //SAS Maneuver
      ledOutput = setBit(ledOutput, pSASEnable);
      ledOutput = setBit(ledOutput, pManeuver);
      break;
    case AP_PROGRADE:  //SAS Prograde
      ledOutput = setBit(ledOutput, pSASEnable);
      ledOutput = setBit(ledOutput, pPrograde);
      break;
    case AP_RETROGRADE:  //SAS Retrograde
      ledOutput = setBit(ledOutput, pSASEnable);
      ledOutput = setBit(ledOutput, pRetrograde);
      break;
    case AP_NORMAL:  //SAS Normal
      ledOutput = setBit(ledOutput, pSASEnable);
      ledOutput = setBit(ledOutput, pNormal);
      break;
    case AP_ANTINORMAL:  //SAS Anti-Normal
      ledOutput = setBit(ledOutput, pSASEnable);
      ledOutput = setBit(ledOutput, pAntiNormal);
      break;
    case AP_RADIALIN:  //SAS Radial In
      ledOutput = setBit(ledOutput, pSASEnable);
      ledOutput = setBit(ledOutput, pRadialIn);
      break;
    case AP_RADIALOUT:  //SAS Radial Out
      ledOutput = setBit(ledOutput, pSASEnable);
      ledOutput = setBit(ledOutput, pRadialOut);
      break;
    case AP_TARGET:  //SAS Target
      ledOutput = setBit(ledOutput, pSASEnable);
      ledOutput = setBit(ledOutput, pTarget);
      break;
    case AP_ANTITARGET:  //SAS Anti-Target
      ledOutput = setBit(ledOutput, pSASEnable);
      ledOutput = setBit(ledOutput, pAntiTarget);
      break;
  }

  return ledOutput;
}


/***************************************************************************************
   PROCESS PANEL CONTROL INPUTS/OUTPUT
   Read incoming button/switch presses from the Panel Control module and update the
   LED states to match
   - INPUTS:
    - {uint8_t} i2c_addr = I2C address of target module
   - No outputs
****************************************************************************************/
void handlePanelCtrl(uint8_t i2c_addr) {  //Determin panel control mode via switch selected position

  /***************************************************************
    Receive button/switch inputs
  ****************************************************************/
  uint8_t Enc1_high, Enc1_low, Enc2_high, Enc2_low;

  int16_t interrupt_val = digitalRead(PanelCtrl_INT);
  if (interrupt_val == LOW) {
    if (debug) { Serial.println("Panel Control Interrupt Detected!"); }
    // TODO (controller v5.x): the standalone "Panel Control" module was
    // removed in the v5.x design — its encoder/control functions moved to the
    // Dual Encoder (0x2D) and direct-wired panel controls. This handler reads
    // a non-conformant 5-byte encoder frame and predates the universal
    // 3-byte header; rework or remove it during the controller migration.
    // Conformant size for the replacement module is moduleTotalPacketSize().
    Wire.requestFrom(i2c_addr, 3);   // request 3 bytes from target i2c device
    while (Wire.available()) {       // slave may send less than requested
      newButtonPanel = Wire.read();  // receive first byte
      Enc1_high = Wire.read();       // receive second byte
      Enc1_low = Wire.read();        // receive third byte
      Enc2_high = Wire.read();       // receive fourth byte
      Enc2_low = Wire.read();        // receive five byte
    }

    enc1_pos = (Enc1_high << 8) + Enc1_low;
    enc2_pos = (Enc2_high << 8) + Enc2_low;


    /***************************************************************
    Handle button input information
  ****************************************************************/
  }
}


/***************************************************************************************
   PROCESS STABILITY ASSIST PANEL INPUTS/OUTPUT
   Read incoming button/switch presses from the Stability Assist panel and update the
   LED states to match
   Stability Assist panel contains Stability Assist Group
   - INPUTS:
    - {uint8_t} i2c_addr = I2C address of target module
   - No outputs
****************************************************************************************/
void handleStabAssistPanel(uint8_t i2c_addr) {

  /***************************************************************
    Receive button/switch inputs
  ****************************************************************/
  uint16_t ledStabCtrl = 0;

  int16_t interrupt_val = digitalRead(StabCtrl_INT);
  if (interrupt_val == LOW) {
    if (debug) { Serial.println("Stability Control Module Interrupt Detected!"); }
    // Read the full conformant packet (3-byte universal header + 4-byte
    // standard button payload = 7 bytes) and parse past the header.
    uint8_t n = moduleTotalPacketSize(KMC_TYPE_STABILITY_CONTROL);
    Wire.requestFrom(i2c_addr, n);
    uint8_t pkt[12];
    uint8_t got = 0;
    while (Wire.available() && got < n) pkt[got++] = Wire.read();
    // pkt[0..2] = status, type ID, transaction counter (universal header).
    // Payload: byte0 events 0-7, byte1 events 8-15, byte2 change 0-7, byte3 change 8-15.
    // Events carry current state; bit 0 = button 0.
    // TODO (controller v5.x): the pStabAssist/pSASEnable bit layout in
    // module_variables.h predates the v5.x Stability Control panel (RCS at
    // B10, no SAS/RCS enable inputs) and still needs reconciliation.
    newButtonStabCtrl = (uint16_t)pkt[KMC_PKT_PAYLOAD_OFFSET]
                      | ((uint16_t)pkt[KMC_PKT_PAYLOAD_OFFSET + 1] << 8);
    if (debug) {
      Serial.print("newButtonStabCtrl = ");
      Serial.println(newButtonStabCtrl, BIN);
    }

    /***************************************************************
    Process button/switch inputs
  ****************************************************************/
    //Check if SAS Enabled Switch is in position
    if (buttonPressed(prevButtonStabCtrl, newButtonStabCtrl, pSASEnable)) {
      mySimpit.activateAction(SAS_ACTION);
      mySimpit.setSASMode(AP_STABILITYASSIST);
      if (demo) { SAS_mode = AP_STABILITYASSIST; }
      if (debug) {
        lastAction = "SAS Enable ACTIVATED";
        Serial.println(lastAction);
      }
    } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pSASEnable)) {
      mySimpit.deactivateAction(SAS_ACTION);
      if (demo) { SAS_mode = 255; }
      if (debug) {
        lastAction = "SAS Enable DEACTIVATED";
        Serial.println(lastAction);
      }
    }

    //If SAS is on, the process whichever SAS mode is pressed. Only 1 can be activated at a time
    if (SAS_on) {
      if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pStabAssist) && !isBitEnabled(ledStabCtrl, pStabAssist)) {
        mySimpit.setSASMode(AP_STABILITYASSIST);
        if (demo) { SAS_mode = AP_STABILITYASSIST; }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pManeuver) && !isBitEnabled(ledStabCtrl, pManeuver)) {
        mySimpit.setSASMode(AP_MANEUVER);
        if (demo) { SAS_mode = AP_MANEUVER; }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pPrograde) && !isBitEnabled(ledStabCtrl, pPrograde)) {
        mySimpit.setSASMode(AP_PROGRADE);
        if (demo) { SAS_mode = AP_PROGRADE; }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pRetrograde) && !isBitEnabled(ledStabCtrl, pRetrograde)) {
        mySimpit.setSASMode(AP_RETROGRADE);
        if (demo) { SAS_mode = AP_RETROGRADE; }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pNormal) && !isBitEnabled(ledStabCtrl, pNormal)) {
        mySimpit.setSASMode(AP_NORMAL);
        if (demo) { SAS_mode = AP_NORMAL; }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pAntiNormal) && !isBitEnabled(ledStabCtrl, pAntiNormal)) {
        mySimpit.setSASMode(AP_ANTINORMAL);
        if (demo) { SAS_mode = AP_ANTINORMAL; }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pRadialIn) && !isBitEnabled(ledStabCtrl, pRadialIn)) {
        mySimpit.setSASMode(AP_RADIALIN);
        if (demo) { SAS_mode = AP_RADIALIN; }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pRadialOut) && !isBitEnabled(ledStabCtrl, pRadialOut)) {
        mySimpit.setSASMode(AP_RADIALOUT);
        if (demo) { SAS_mode = AP_RADIALOUT; }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pTarget) && !isBitEnabled(ledStabCtrl, pTarget)) {
        mySimpit.setSASMode(AP_TARGET);
        if (demo) { SAS_mode = AP_TARGET; }
      } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pAntiTarget) && !isBitEnabled(ledStabCtrl, pAntiTarget)) {
        mySimpit.setSASMode(AP_ANTITARGET);
        if (demo) { SAS_mode = AP_ANTITARGET; }
      }
      if (debug) {
        lastAction = "SAS Mode Set";
        Serial.println(lastAction);
        Serial.print("ledStabCtrl = ");
        Serial.println(ledStabCtrl, BIN);
      }
    }

    //Check if RCS Enabled Switch is in position
    if (buttonPressed(prevButtonStabCtrl, newButtonStabCtrl, pRCSEnable)) {
      mySimpit.activateAction(RCS_ACTION);
      if (demo) { RCS_on = true; }
      if (debug) {
        lastAction = "RCS Enable ACTIVATED";
        Serial.println(lastAction);
      }
    } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pRCSEnable)) {
      mySimpit.deactivateAction(RCS_ACTION);
      ledStabCtrl = clearBit(ledStabCtrl, pRCSEnable);
      if (demo) { RCS_on = false; }
      if (debug) {
        lastAction = "RCS Enable DEACTIVATED";
        Serial.println(lastAction);
      }
    }

    //Operate the Precision Control Selector
    if (buttonPressed(prevButtonStabCtrl, newButtonStabCtrl, pPrecision)) {
      precisionEn = true;  // Apply the precision factor to the translation and rotation inputs
      if (debug) {
        lastAction = "Precision Mode ACTIVATED";
        Serial.println(lastAction);
      }
    } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pPrecision)) {
      precisionEn = false;
      if (debug) {
        lastAction = "Precision Mode DEACTIVATED";
        Serial.println(lastAction);
      }
    }

    //Operate the Invert SAS button
    if (buttonPressed(prevButtonStabCtrl, newButtonStabCtrl, pInvertSAS)) {
      keyboardEmulatorMessage msg(F_KEY, KEY_DOWN_MOD);
      mySimpit.send(KEYBOARD_EMULATOR, msg);
      if (debug) {
        lastAction = "Invert SAS button PRESSED";
        Serial.println(lastAction);
      }
    } else if (buttonReleased(prevButtonStabCtrl, newButtonStabCtrl, pInvertSAS)) {
      keyboardEmulatorMessage msg(F_KEY, KEY_UP_MOD);
      mySimpit.send(KEYBOARD_EMULATOR, msg);
      if (debug) {
        lastAction = "Invert SAS button RELEASED";
        Serial.println(lastAction);
      }
    }
  }

  /***************************************************************
     Write the LED states based on the function definition
  ****************************************************************/
  ledStabCtrl = setSASLEDState(SAS_mode);
  if (RCS_on) { ledStabCtrl = setBit(ledStabCtrl, pRCSEnable); }
  if (precisionEn) { ledStabCtrl = setBit(ledStabCtrl, pPrecision); }

  Wire.beginTransmission(i2c_addr);   // send data to the i2c target
  Wire.write(highByte(ledStabCtrl));  // send LED state high byte
  Wire.write(lowByte(ledStabCtrl));   // send LED state low byte
  Wire.endTransmission();             // COmplete transmission
  if (debug) {
    lastAction = "LEDS written";
    Serial.println(lastAction);
  }

  prevButtonStabCtrl = newButtonStabCtrl;
}


/***************************************************************************************
   PROCESS VEHCILE CONTROL PANEL INPUTS/OUTPUT
   Read incoming button/switch presses from the Vehicle Control panel and update the
   LED states to match
   Stability Assist panel contains Stability Assist Group
   - INPUTS:
    - {uint8_t} i2c_addr = I2C address of target module
   - No outputs
****************************************************************************************/
void handleVehCtrlPanel(uint8_t i2c_addr) {

  /***************************************************************
    Receive button/switch inputs
  ****************************************************************/
  uint16_t ledVehCtrl = 0;

  int16_t interrupt_val = digitalRead(VehCtrl_INT);
  if (interrupt_val == LOW) {
    if (debug) { Serial.println("Vehicle Control Module Interrupt Detected!"); }
    // Vehicle Control is a 24-input switch-group module: 3-byte universal
    // header + 6-byte payload (events 0-7/8-15/16-23, change 0-7/8-15/16-23)
    // = 9 bytes. Parse past the header and reconstruct the 24-bit input word.
    uint8_t n = moduleTotalPacketSize(KMC_TYPE_VEHICLE_CONTROL);
    Wire.requestFrom(i2c_addr, n);
    uint8_t pkt[12];
    uint8_t got = 0;
    while (Wire.available() && got < n) pkt[got++] = Wire.read();
    uint32_t vehInputs = (uint32_t)pkt[KMC_PKT_PAYLOAD_OFFSET]
                       | ((uint32_t)pkt[KMC_PKT_PAYLOAD_OFFSET + 1] << 8)
                       | ((uint32_t)pkt[KMC_PKT_PAYLOAD_OFFSET + 2] << 16);  // bits 16-23 = Switch Group 2
    // Switch Group 2 (KBC 16-23) is exposed via newSwitchGrp2 for edge tests
    // with the pSG2_* masks (e.g. buttonPressed(prevSwitchGrp2, newSwitchGrp2,
    // pSG2_CHUTE)). TODO (controller v5.x): newButtonVehCtrl is 16-bit and the
    // module_variables.h button bit layout predates the v5.x Vehicle Control
    // panel; remap the B0-B11 actions when the handler is reworked.
    newSwitchGrp2 = vehInputs;
    newButtonVehCtrl = (uint16_t)(vehInputs & 0xFFFF);
    if (debug) {
      Serial.print("newButtonVehCtrl = ");
      Serial.println(newButtonVehCtrl, BIN);
    }

    /***************************************************************
    Process button/switch inputs
  ****************************************************************/
    // Toggle Light Group if push button is pressed
    if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pLights) && !lights_lock) {
      mySimpit.toggleAction(LIGHT_ACTION);
      if (demo) { lights_on = !lights_on; }
      if (debug) {
        lastAction = "Lights button TOGGLED";
        Serial.println(lastAction);
      }
    }

    // Toggle Gear Group if push button is pressed
    if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pGear) && !gear_lock) {
      mySimpit.toggleAction(GEAR_ACTION);
      if (demo) { gear_on = !gear_on; }
      if (debug) {
        lastAction = "Gear button TOGGLED";
        Serial.println(lastAction);
      }
    }

    // Activate or Deactivate Brakes Group if push button is pressed & released
    if (buttonPressed(prevButtonVehCtrl, newButtonVehCtrl, pBrakes) && !brakes_lock) {
      mySimpit.activateAction(BRAKES_ACTION);
      if (demo) { brakes_on = true; }
      if (debug) {
        lastAction = "Brakes button PRESSED";
        Serial.println(lastAction);
      }
    } else if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pBrakes) && !brakes_lock) {
      mySimpit.deactivateAction(BRAKES_ACTION);
      if (demo) { brakes_on = false; }
      if (debug) {
        lastAction = "Brakes button RELEASED";
        Serial.println(lastAction);
      }
    }

    // Toggle Ladder Group if push button is pressed
    if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pLadder)) {
      mySimpit.toggleCAG(ladder);
      if (demo) { ladder_on = !ladder_on; }
      if (debug) {
        lastAction = "Ladder button TOGGLED";
        Serial.println(lastAction);
      }
    }

    // Toggle Solar Array Group if push button is pressed
    if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pSolarArray)) {
      mySimpit.toggleCAG(solar_array);
      if (demo) { solarArray_on = !solarArray_on; }
      if (debug) {
        lastAction = "Solar Array button TOGGLED";
        Serial.println(lastAction);
      }
    }

    // Toggle Antenna Group if push button is pressed
    if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pAntenna)) {
      mySimpit.toggleCAG(antenna);
      if (demo) { antenna_on = !antenna_on; }
      if (debug) {
        lastAction = "Antenna button TOGGLED";
        Serial.println(lastAction);
      }
    }

    // Toggle Radiator Group if push button is pressed
    if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pRadiator)) {
      mySimpit.toggleCAG(radiator);
      if (demo) { radiator_on = !radiator_on; }
      if (debug) {
        lastAction = "Radiator button TOGGLED";
        Serial.println(lastAction);
      }
    }

    // Toggle Cargo Door Group if push button is pressed
    if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pCargoDoor)) {
      mySimpit.toggleCAG(cargo_door);
      if (demo) { cargoDoor_on = !cargoDoor_on; }
      if (debug) {
        lastAction = "Cargo Door button TOGGLED";
        Serial.println(lastAction);
      }
    }

    // Toggle Parachute Group if push button is pressed
    if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pParachute) && !parachute_auto) {
      mySimpit.toggleCAG(parachute);
      if (demo) { parachute_on = !parachute_on; }
      if (debug) {
        lastAction = "Parachute button TOGGLED";
        Serial.println(lastAction);
      }
    }

    // Toggle Drogue Parachute Group if push button is pressed (and drogue not deployed)
    if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pDrogue) && !parachute_auto) {
      mySimpit.toggleCAG(drogue);
      if (demo) { drogue_on = !drogue_on; }
      if (debug) {
        lastAction = "Drogue button TOGGLED";
        Serial.println(lastAction);
      }
    }

    // Toggle Drogue Cut Parachute Group if push button is pressed (and drogue is deployed)
    if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pDrogue) && !parachute_auto && drogue_on) {
      mySimpit.toggleCAG(drogue_cut);
      if (demo) { drogue_on = !drogue_on; }
      if (debug) {
        lastAction = "Drogue button TOGGLED";
        Serial.println(lastAction);
      }
    }

    // Activate or Deactivate Brake & Brake Lock Group if switch is pressed & released
    if (buttonPressed(prevButtonVehCtrl, newButtonVehCtrl, pBrakeLock)) {
      mySimpit.activateAction(BRAKES_ACTION);
      brakes_lock = true;
      if (demo) { brakes_on = true; }
      if (debug) {
        lastAction = "Brakes Lock ACTIVATED";
        Serial.println(lastAction);
      }
    } else if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pBrakeLock)) {
      mySimpit.deactivateAction(BRAKES_ACTION);
      brakes_lock = false;
      if (demo) { brakes_on = false; }
      if (debug) {
        lastAction = "Brakes Lock DEACTIVATED";
        Serial.println(lastAction);
      }
    }

    // Activate or Deactivate Gear & Gear Lock Group if switch is pressed & released
    if (buttonPressed(prevButtonVehCtrl, newButtonVehCtrl, pGearLock)) {
      mySimpit.activateAction(GEAR_ACTION);
      gear_lock = true;
      if (demo) { gear_on = true; }
      if (debug) {
        lastAction = "Gear Lock ACTIVATED";
        Serial.println(lastAction);
      }
    } else if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pGearLock)) {
      mySimpit.deactivateAction(GEAR_ACTION);
      gear_lock = false;
      if (demo) { gear_on = false; }
      if (debug) {
        lastAction = "Gear Lock DEACTIVATED";
        Serial.println(lastAction);
      }
    }

    // Activate or Deactivate Lights & Lights Lock Group if switch is pressed & released
    if (buttonPressed(prevButtonVehCtrl, newButtonVehCtrl, pLightsLock)) {
      mySimpit.activateAction(LIGHT_ACTION);
      lights_lock = true;
      if (demo) { lights_on = true; }
      if (debug) {
        lastAction = "Lights Lock ACTIVATED";
        Serial.println(lastAction);
      }
    } else if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pLightsLock)) {
      mySimpit.deactivateAction(LIGHT_ACTION);
      lights_lock = false;
      if (demo) { lights_on = false; }
      if (debug) {
        lastAction = "Lights Lock DEACTIVATED";
        Serial.println(lastAction);
      }
    }

    // Activate or Deactivate Parachute Auto mode if switch is pressed & released
    if (buttonPressed(prevButtonVehCtrl, newButtonVehCtrl, pChutesAuto)) {
      parachute_auto = true;
      if (debug) {
        lastAction = "Parachute Auto Mode ACTIVATED";
        Serial.println(lastAction);
      }
    } else if (buttonReleased(prevButtonVehCtrl, newButtonVehCtrl, pChutesAuto)) {
      parachute_auto = false;
      if (debug) {
        lastAction = "Parachute Auto Mode DEACTIVATED";
        Serial.println(lastAction);
      }
    }
  }

  //Process Parachute Auto
  if (parachute_auto) {
    if (alt_surf < 1500.) {  //Deploy Parachutes if altitude is below 1500m above terrain
      mySimpit.activateCAG(parachute);
      if (demo) { parachute_on = true; }
      if (debug) {
        lastAction = "Parachute ACTIVATED (Parachute Auto)";
        Serial.println(lastAction);
      }
      mySimpit.activateCAG(drogue_cut);
      if (demo) { drogue_on = !drogue_on; }
      if (debug) {
        lastAction = "Drogue CUT (Parachute Auto)";
        Serial.println(lastAction);
      }
    } else if (alt_surf < 3000.) {  //Deploy Drogues chutes if altitude is velow 3000m above terrain
      mySimpit.activateCAG(drogue);
      if (demo) { drogue_on = true; }
      if (debug) {
        lastAction = "Drogue ACTIVATED (Parachute Auto)";
        Serial.println(lastAction);
      }
    }
  }

  /***************************************************************
     Write the LED states based on the function definition
  ****************************************************************/
  if (lights_on) { ledVehCtrl = setBit(ledVehCtrl, pLights); }
  if (gear_on) { ledVehCtrl = setBit(ledVehCtrl, pGear); }
  if (brakes_on) { ledVehCtrl = setBit(ledVehCtrl, pBrakes); }
  if (ladder_on) { ledVehCtrl = setBit(ledVehCtrl, pLadder); }
  if (solarArray_on) { ledVehCtrl = setBit(ledVehCtrl, pSolarArray); }
  if (antenna_on) { ledVehCtrl = setBit(ledVehCtrl, pAntenna); }
  if (radiator_on) { ledVehCtrl = setBit(ledVehCtrl, pRadiator); }
  if (cargoDoor_on) { ledVehCtrl = setBit(ledVehCtrl, pCargoDoor); }
  if (parachute_on) { ledVehCtrl = setBit(ledVehCtrl, pParachute); }
  if (drogue_on) { ledVehCtrl = setBit(ledVehCtrl, pDrogue); }
  if (lights_lock) { ledVehCtrl = setBit(ledVehCtrl, pLightsLock); }
  if (gear_lock) { ledVehCtrl = setBit(ledVehCtrl, pGearLock); }
  if (brakes_lock) { ledVehCtrl = setBit(ledVehCtrl, pBrakeLock); }
  if (parachute_auto) { ledVehCtrl = setBit(ledVehCtrl, pChutesAuto); }


  Wire.beginTransmission(i2c_addr);  // send data to the i2c target
  Wire.write(highByte(ledVehCtrl));  // send LED state high byte
  Wire.write(lowByte(ledVehCtrl));   // send LED state low byte
  Wire.endTransmission();            // COmplete transmission
  if (debug) {
    lastAction = "LEDS written";
    Serial.println(lastAction);
  }

  prevButtonVehCtrl = newButtonVehCtrl;
  prevSwitchGrp2 = newSwitchGrp2;
}


/***************************************************************************************
   EXECUTE REBOOT
   Function performs a soft reboot on the teensy
   - No inputs
   - No outputs
****************************************************************************************/
void executeReboot() {
  // send reboot command -----
  SCB_AIRCR = 0x05FA0004;
}


/***************************************************************************************
   DISCONNECT USB
   Function disconnects the USB connection
   - No inputs
   - No outputs
****************************************************************************************/
void disconnectUSB() {
  // turnoff USB controller and reset it.
  USB1_USBCMD = 0;  // turn off USB controller
  USB1_USBCMD = 2;  // begin USB controller reset
  delay(20);
}


/***************************************************************************************
   CONNECT USB
   Function reruns the USB Connection
   - No inputs
   - No outputs
****************************************************************************************/
void connectUSB() {
  // turnoff USB controller and reset it.
  usb_init();
  delay(2000);
}


/************************************
/***************************************************************************************
   hold_autopilot.ino — Hold-mode autopilot: mode manager, cascades, autothrottle,
   rover cruise / steering / guards, disconnect rules, status, bench console.
   Contract and design notes in hold_autopilot.h; design document in
   Documents/Developer/Hold_Mode_Autopilot.md.
****************************************************************************************/
#include "hold_autopilot.h"
#include "attitude_controller.h"
#include "control_links.h"
#include "ascent_autopilot.h"

static const int32_t HP_AXIS_FULL = INT16_MAX;   // Simpit axis full-scale (wheel steer/throttle)


/***************************************************************************************
   Telemetry snapshot
****************************************************************************************/
struct HpTelemetry {
  uint8_t  vesselType    = 0;
  uint8_t  situation     = 0;
  bool     hasTarget     = false;
  float    altSea        = 0.0f;
  float    velSurface    = 0.0f;
  float    velVertical   = 0.0f;
  float    ias           = 0.0f;
  float    mach          = 0.0f;
  float    heading       = 0.0f;
  float    pitch         = 0.0f;
  float    roll          = 0.0f;
  float    srfVelHeading = 0.0f;
  float    srfVelPitch   = 0.0f;
  float    tgtBearing    = 0.0f;
  float    tgtElev       = 0.0f;   // deg, negative = below
  float    tgtDist       = 0.0f;   // m
  float    tgtClosing    = 0.0f;   // m/s along the line of sight, > 0 = opening
  float    throttle      = 0.0f;   // game throttle echo 0..1
  bool     hasAtmo       = true;
  bool     inAtmo        = true;
  bool     brakes        = false;
  uint32_t lastMs        = 0;
};
static HpTelemetry hp_t;
static HoldConfig  hp_c;

/***************************************************************************************
   Mode + loop state
****************************************************************************************/
static HpPitchMode hp_pitchMode = HP_PITCH_OFF;
static HpLatMode   hp_latMode   = HP_LAT_OFF;
static HpThrMode   hp_thrMode   = HP_THR_OFF;
static bool        hp_cruise = false, hp_rhdg = false, hp_rtgt = false;

// Setpoints (initialised by hpDefaultConfig/hpInit; captured on engage)
static float hp_spAtt = 0.0f, hp_spAoa = 3.0f, hp_spVs = 0.0f, hp_spAlt = 1000.0f;
static float hp_spRoll = 0.0f, hp_spHdg = 90.0f, hp_spIas = 120.0f, hp_spMach = 0.5f;
static float hp_spCruise = 5.0f, hp_spRhdg = 0.0f;
static float hp_spGs = 3.0f, hp_followRange = 30.0f, hp_stopDist = 15.0f;
static bool  hp_follow = false;
static float hp_maxSpeed = 20.0f, hp_maxSlope = 20.0f, hp_maxRoll = 25.0f;

static AttState hp_att;
static float    hp_holdHeading = 0.0f;   // rocket-entry heading reference when only ROLL is held
static float    hp_vsInt = 0.0f;          // V/S loop integrator (deg of pitch)
static float    hp_thrInt = 0.0f, hp_thrOut = 0.0f;
static float    hp_wheelInt = 0.0f, hp_wheelOut = 0.0f, hp_steerOut = 0.0f;
static bool     hp_slopeGuard = false;
static bool     hp_sasIsOff = false;     // we switched stock SAS off for attitude holding
static float    hp_cmdPitch = 0.0f, hp_cmdBank = 0.0f;

static uint8_t  hp_reason = HP_REASON_NONE,      hp_roverReason = HP_REASON_NONE;
static uint32_t hp_reasonMs = 0,                 hp_roverReasonMs = 0;
static uint32_t hp_lastUpdateMs = 0;
static uint32_t hp_airborneSince = 0, hp_noAtmoSince = 0;

static inline bool hpAircraftEngaged() { return hp_pitchMode != HP_PITCH_OFF || hp_latMode != HP_LAT_OFF || hp_thrMode != HP_THR_OFF; }
static inline bool hpRoverDriving()    { return hp_cruise || hp_follow; }
static inline bool hpOnGround()        { return hp_t.situation == KSP_SIT_LANDED || hp_t.situation == KSP_SIT_SPLASHED; }

static void hpSetReason(uint8_t r)      { if (r == HP_REASON_PILOT) r = HP_REASON_NONE; hp_reason = r;      hp_reasonMs = millis(); }
static void hpSetRoverReason(uint8_t r) { if (r == HP_REASON_PILOT) r = HP_REASON_NONE; hp_roverReason = r; hp_roverReasonMs = millis(); }

/***************************************************************************************
   Configuration
****************************************************************************************/
HoldConfig hpDefaultConfig() {
  HoldConfig c;
  c.acftGains   = attAircraftGains();
  c.rocketGains = attRocketGains();
  c.steerLikeRocket      = false;
  c.coordinateTurn       = false;
  c.maxControlDeflection = 1.0f;

  c.vsMax    = 30.0f;
  c.pitchMax = 25.0f;
  c.bankMax  = 30.0f;
  c.altKp    = 0.20f;
  c.vsKp     = 0.50f;  c.vsKi = 0.15f;
  c.hdgKp    = 1.50f;

  c.iasKp  = 0.010f;  c.iasKi  = 0.004f;
  c.machKp = 3.00f;   c.machKi = 1.00f;
  c.throttleSlew = 0.25f;

  c.gsKp = 0.10f; c.gsMinRange = 200.0f;
  c.cruiseKp = 0.08f; c.cruiseKi = 0.04f;
  c.stopDecel = 2.0f; c.followKp = 0.5f;
  c.wheelSlew = 0.5f;
  c.steerKpLow = 0.05f; c.steerKpHigh = 0.02f; c.steerKpSpeed = 20.0f;
  c.steerSign = 1.0f;

  c.telemetryTimeout = 2000;
  c.airborneMs = 500;
  c.noAtmoMs   = 2000;

  c.attMin = -45.0f;  c.attMax = 45.0f;
  c.aoaMin = -10.0f;  c.aoaMax = 25.0f;
  c.vsMin  = -100.0f; c.vsMaxSp = 100.0f;
  c.altMin = 0.0f;    c.altMax = 70000.0f;
  c.rollMin = -60.0f; c.rollMax = 60.0f;
  c.iasMin = 20.0f;   c.iasMax = 1500.0f;
  c.machMin = 0.10f;  c.machMax = 6.0f;
  c.cruiseMin = -10.0f; c.cruiseMax = 60.0f;
  c.maxSpeedMin = 1.0f;  c.maxSpeedMax = 60.0f;
  c.maxSlopeMin = 5.0f;  c.maxSlopeMax = 45.0f;
  c.maxRollMin  = 5.0f;  c.maxRollMax  = 60.0f;
  c.gsMin = 1.0f; c.gsMax = 10.0f; c.followMin = 5.0f; c.followMax = 500.0f; c.stopMin = 2.0f; c.stopMax = 200.0f;
  return c;
}

void hpInit() {
  hp_c = hpDefaultConfig();
  attReset(hp_att);
  hp_pitchMode = HP_PITCH_OFF; hp_latMode = HP_LAT_OFF; hp_thrMode = HP_THR_OFF;
  hp_cruise = hp_rhdg = hp_rtgt = hp_follow = false;
  hp_lastUpdateMs = millis();
}

HoldConfig &hpGetConfig() { return hp_c; }

/***************************************************************************************
   Outputs
****************************************************************************************/
static void hpSendWheels(bool setThrottle, float thr, bool setSteer, float steer) {
  wheelMessage w;
  if (setThrottle) w.setThrottle((int16_t)(attClampf(thr,   -1.0f, 1.0f) * (float)HP_AXIS_FULL));
  if (setSteer)    w.setSteer   ((int16_t)(attClampf(steer, -1.0f, 1.0f) * (float)HP_AXIS_FULL));
  mySimpit.send(WHEEL_MESSAGE, w);
}

// Stock SAS fights raw rotation: off while we hold an attitude axis, back to stability
// assist when we let go so the pilot never inherits a plane with SAS off.
static void hpReconcileSAS() {
  bool holding = (hp_pitchMode != HP_PITCH_OFF || hp_latMode != HP_LAT_OFF);
  if (holding && !hp_sasIsOff) {
    mySimpit.deactivateAction(SAS_ACTION);
    hp_sasIsOff = true;
  } else if (!holding && hp_sasIsOff) {
    mySimpit.activateAction(SAS_ACTION);
    mySimpit.setSASMode(AP_STABILITYASSIST);
    hp_sasIsOff = false;
  }
}

/***************************************************************************************
   Rover speed sign: VELOCITY_MESSAGE.surface is unsigned; more than 90 deg between the
   nose and the surface-velocity vector means reverse (the ROVER screen's derivation).
****************************************************************************************/
static float hpSignedSpeed() {
  if (hp_t.velSurface < 0.3f) return hp_t.velSurface;
  float d = fabsf(attWrap180(hp_t.srfVelHeading - hp_t.heading));
  return (d > 90.0f) ? -hp_t.velSurface : hp_t.velSurface;
}

/***************************************************************************************
   Engage / disconnect
****************************************************************************************/
static void hpDropAircraftOutputs() {
  rotClearAutoAxes();
  if (hp_thrMode != HP_THR_OFF) thrAutoRelease(THR_OWNER_HOLD);
}

void hpDisconnectAircraft(uint8_t reason) {
  bool was = hpAircraftEngaged();
  hp_pitchMode = HP_PITCH_OFF; hp_latMode = HP_LAT_OFF;
  if (hp_thrMode != HP_THR_OFF) { hp_thrMode = HP_THR_OFF; thrAutoRelease(THR_OWNER_HOLD); }
  rotClearAutoAxes();
  hpReconcileSAS();
  arbReleaseAttitude(AP_OWNER_HOLD); arbReleaseThrottle(AP_OWNER_HOLD);
  if (was) hpSetReason(reason);
}

// Another module took a resource (ap_arbiter.ino): drop the modes that used it.
void hpArbiterDropAttitude() {
  if (hp_pitchMode == HP_PITCH_OFF && hp_latMode == HP_LAT_OFF) return;
  hp_pitchMode = HP_PITCH_OFF; hp_latMode = HP_LAT_OFF;
  rotClearAutoAxes();
  hp_sasIsOff = false;                // the new owner decides what SAS does
  hpSetReason(HP_REASON_OTHER_AP);
}
void hpArbiterDropThrottle() {
  if (hp_thrMode == HP_THR_OFF) return;
  hp_thrMode = HP_THR_OFF; thrAutoRelease(THR_OWNER_HOLD);
  hpSetReason(HP_REASON_OTHER_AP);
}

void hpDisconnectRover(uint8_t reason) {
  bool was = hpRoverEngaged();
  bool hadCruise = hpRoverDriving();
  hp_cruise = hp_rhdg = hp_rtgt = hp_follow = false;
  hp_slopeGuard = false;
  hp_wheelOut = hp_wheelInt = 0.0f; hp_steerOut = 0.0f;
  if (hadCruise) hpSendWheels(true, 0.0f, false, 0.0f);   // never leave the motors driving
  if (was) hpSetRoverReason(reason);
}

void hpDisconnectAll(uint8_t reason) {
  hpDisconnectAircraft(reason);
  hpDisconnectRover(reason);
}

void hpVesselChanged() {
  // Silent: a vessel or scene change is not a fault, and the new vessel starts clean.
  hp_pitchMode = HP_PITCH_OFF; hp_latMode = HP_LAT_OFF;
  if (hp_thrMode != HP_THR_OFF) { hp_thrMode = HP_THR_OFF; thrAutoRelease(THR_OWNER_HOLD); }
  hp_cruise = hp_rhdg = hp_rtgt = hp_follow = false;
  hp_slopeGuard = false;
  rotClearAutoAxes();
  arbReleaseAttitude(AP_OWNER_HOLD); arbReleaseThrottle(AP_OWNER_HOLD);
  hp_sasIsOff = false;           // do not touch SAS on a vessel we have not flown
  hp_reason = hp_roverReason = HP_REASON_NONE;
  hp_t.lastMs = millis();
}

static bool hpIsAircraftMode(HpMode m) { return m <= HP_MODE_MACH || m == HP_MODE_NAV || m == HP_MODE_GS; }

bool hpEngage(HpMode mode, bool on) {
  if (mode >= HP_MODE_COUNT) return false;
  uint32_t now = millis();

  if (!on) {
    switch (mode) {
      case HP_MODE_ATT:  if (hp_pitchMode == HP_PITCH_ATT) hp_pitchMode = HP_PITCH_OFF; break;
      case HP_MODE_AOA:  if (hp_pitchMode == HP_PITCH_AOA) hp_pitchMode = HP_PITCH_OFF; break;
      case HP_MODE_VS:   if (hp_pitchMode == HP_PITCH_VS)  hp_pitchMode = HP_PITCH_OFF; break;
      case HP_MODE_ALT:  if (hp_pitchMode == HP_PITCH_ALT) hp_pitchMode = HP_PITCH_OFF; break;
      case HP_MODE_ROLL: if (hp_latMode == HP_LAT_ROLL)    hp_latMode   = HP_LAT_OFF;   break;
      case HP_MODE_HDG:  if (hp_latMode == HP_LAT_HDG)     hp_latMode   = HP_LAT_OFF;   break;
      case HP_MODE_IAS:  if (hp_thrMode == HP_THR_IAS)  { hp_thrMode = HP_THR_OFF; thrAutoRelease(THR_OWNER_HOLD); } break;
      case HP_MODE_MACH: if (hp_thrMode == HP_THR_MACH) { hp_thrMode = HP_THR_OFF; thrAutoRelease(THR_OWNER_HOLD); } break;
      case HP_MODE_CRUISE: if (hp_cruise) { hp_cruise = false; hp_slopeGuard = false; hp_wheelOut = 0.0f; hpSendWheels(true, 0.0f, false, 0.0f); } break;
      case HP_MODE_RHDG: hp_rhdg = false; break;
      case HP_MODE_RTGT: hp_rtgt = false; break;
      case HP_MODE_NAV:  if (hp_latMode == HP_LAT_NAV)   hp_latMode   = HP_LAT_OFF;   break;
      case HP_MODE_GS:   if (hp_pitchMode == HP_PITCH_GS) hp_pitchMode = HP_PITCH_OFF; break;
      case HP_MODE_FOLLOW: if (hp_follow) { hp_follow = false; hp_wheelOut = 0.0f; hpSendWheels(true, 0.0f, false, 0.0f); } break;
      default: break;
    }
    if (hp_pitchMode == HP_PITCH_OFF && hp_latMode == HP_LAT_OFF) { rotClearAutoAxes(); arbReleaseAttitude(AP_OWNER_HOLD); }
    if (hp_thrMode == HP_THR_OFF) arbReleaseThrottle(AP_OWNER_HOLD);
    hpReconcileSAS();
    return true;
  }

  // ---- Refusals ----
  bool stale = (now - hp_t.lastMs) > hp_c.telemetryTimeout;
  if (hpIsAircraftMode(mode)) {
    if (stale || !hp_t.hasAtmo) { hpSetReason(HP_REASON_REFUSED); return false; }
    if ((mode == HP_MODE_IAS || mode == HP_MODE_MACH) && thrPrecision()) { hpSetReason(HP_REASON_REFUSED); return false; }
    if ((mode == HP_MODE_NAV || mode == HP_MODE_GS) && !hp_t.hasTarget) { hpSetReason(HP_REASON_NO_TARGET); return false; }
  } else {
    if (stale || !hpOnGround()) { hpSetRoverReason(HP_REASON_REFUSED); return false; }
    if ((mode == HP_MODE_RTGT || mode == HP_MODE_FOLLOW) && !hp_t.hasTarget) { hpSetRoverReason(HP_REASON_NO_TARGET); return false; }
  }
  // One attitude owner and one throttle owner at a time (ap_arbiter.ino): engaging takes
  // the resource from the ascent, burn or landing module.
  if (hpIsAircraftMode(mode)) {
    if (mode == HP_MODE_IAS || mode == HP_MODE_MACH) arbTakeThrottle(AP_OWNER_HOLD);
    else                                             arbTakeAttitude(AP_OWNER_HOLD);
  }

  bool wasHoldingAttitude = (hp_pitchMode != HP_PITCH_OFF || hp_latMode != HP_LAT_OFF);

  switch (mode) {
    // ---- pitch group: capture, then bumpless integrator init ----
    case HP_MODE_ATT: hp_spAtt = attClampf(hp_t.pitch, hp_c.attMin, hp_c.attMax); hp_pitchMode = HP_PITCH_ATT; break;
    case HP_MODE_AOA: hp_spAoa = attClampf(hp_t.pitch - hp_t.srfVelPitch, hp_c.aoaMin, hp_c.aoaMax); hp_pitchMode = HP_PITCH_AOA; break;
    case HP_MODE_VS:  hp_spVs  = attClampf(roundf(hp_t.velVertical), hp_c.vsMin, hp_c.vsMaxSp); hp_vsInt = hp_t.pitch; hp_pitchMode = HP_PITCH_VS; break;
    case HP_MODE_ALT: hp_spAlt = attClampf(roundf(hp_t.altSea / 10.0f) * 10.0f, hp_c.altMin, hp_c.altMax); hp_vsInt = hp_t.pitch; hp_pitchMode = HP_PITCH_ALT; break;
    // ---- lateral group ----
    case HP_MODE_ROLL: hp_spRoll = attClampf(hp_t.roll, hp_c.rollMin, hp_c.rollMax); hp_holdHeading = hp_t.heading; hp_latMode = HP_LAT_ROLL; break;
    case HP_MODE_HDG:  hp_spHdg  = attWrap360(roundf(hp_t.heading)); hp_latMode = HP_LAT_HDG; break;
    // ---- thrust group ----
    case HP_MODE_IAS:
    case HP_MODE_MACH: {
      if (mode == HP_MODE_IAS) { hp_spIas  = attClampf(hp_t.ias,  hp_c.iasMin,  hp_c.iasMax);  hp_thrMode = HP_THR_IAS; }
      else                     { hp_spMach = attClampf(hp_t.mach, hp_c.machMin, hp_c.machMax); hp_thrMode = HP_THR_MACH; }
      float cur = thrLeverDriven() ? thrCurrentThrottle() : hp_t.throttle;
      hp_thrInt = hp_thrOut = attClampf(cur, 0.0f, 1.0f);
      thrAutoThrottle(THR_OWNER_HOLD, hp_thrOut);
      break;
    }
    // ---- rover ----
    case HP_MODE_CRUISE: {
      hp_spCruise = attClampf(roundf(hpSignedSpeed() * 2.0f) * 0.5f, hp_c.cruiseMin, hp_c.cruiseMax);
      hp_wheelInt = hp_wheelOut = 0.0f;
      hp_cruise = true;
      break;
    }
    case HP_MODE_RHDG: hp_spRhdg = attWrap360(roundf(hp_t.heading)); hp_rhdg = true; hp_rtgt = false; hp_follow = false; break;
    case HP_MODE_RTGT: hp_rtgt = true; hp_rhdg = false; hp_follow = false; break;
    // ---- flag approach ----
    case HP_MODE_NAV:  hp_latMode = HP_LAT_NAV; break;
    case HP_MODE_GS:   hp_vsInt = hp_t.pitch; hp_pitchMode = HP_PITCH_GS; break;
    // ---- follow: FOLLOW replaces CRUISE's throttle and steers as TGT ----
    case HP_MODE_FOLLOW: hp_wheelInt = hp_wheelOut = 0.0f; hp_follow = true; hp_cruise = false; hp_rhdg = hp_rtgt = false; break;
    default: return false;
  }

  bool holdingAttitude = (hp_pitchMode != HP_PITCH_OFF || hp_latMode != HP_LAT_OFF);
  if (holdingAttitude && !wasHoldingAttitude) attReset(hp_att);
  hp_lastUpdateMs = now;
  hpReconcileSAS();
  return true;
}

void hpLevel() {
  // Wings level + zero vertical speed. Goes through hpEngage for the refusals and SAS
  // handling, then overrides the captured setpoints.
  if (hpEngage(HP_MODE_ROLL, true)) hp_spRoll = 0.0f;
  if (hpEngage(HP_MODE_VS,   true)) hp_spVs   = 0.0f;
}

/***************************************************************************************
   Setpoints — range-checked, apply engaged or not
****************************************************************************************/
static bool hpInRange(float v, float lo, float hi) { return !(v < lo || v > hi); }
bool hpSetAtt(float v)      { if (!hpInRange(v, hp_c.attMin, hp_c.attMax)) return false; hp_spAtt = v; return true; }
bool hpSetAoa(float v)      { if (!hpInRange(v, hp_c.aoaMin, hp_c.aoaMax)) return false; hp_spAoa = v; return true; }
bool hpSetVs(float v)       { if (!hpInRange(v, hp_c.vsMin, hp_c.vsMaxSp)) return false; hp_spVs = v; return true; }
bool hpSetAlt(float v)      { if (!hpInRange(v, hp_c.altMin, hp_c.altMax)) return false; hp_spAlt = v; return true; }
bool hpSetRoll(float v)     { if (!hpInRange(v, hp_c.rollMin, hp_c.rollMax)) return false; hp_spRoll = v; return true; }
bool hpSetHdg(float v)      { hp_spHdg = attWrap360(v); return true; }
bool hpSetIas(float v)      { if (!hpInRange(v, hp_c.iasMin, hp_c.iasMax)) return false; hp_spIas = v; return true; }
bool hpSetMach(float v)     { if (!hpInRange(v, hp_c.machMin, hp_c.machMax)) return false; hp_spMach = v; return true; }
bool hpSetCruise(float v)   { if (!hpInRange(v, hp_c.cruiseMin, hp_c.cruiseMax)) return false; hp_spCruise = v; return true; }
bool hpSetRoverHdg(float v) { hp_spRhdg = attWrap360(v); return true; }
bool hpSetMaxSpeed(float v) { if (!hpInRange(v, hp_c.maxSpeedMin, hp_c.maxSpeedMax)) return false; hp_maxSpeed = v; return true; }
bool hpSetMaxSlope(float v) { if (!hpInRange(v, hp_c.maxSlopeMin, hp_c.maxSlopeMax)) return false; hp_maxSlope = v; return true; }
bool hpSetMaxRoll(float v)  { if (!hpInRange(v, hp_c.maxRollMin, hp_c.maxRollMax)) return false; hp_maxRoll = v; return true; }
bool hpSetGs(float v)          { if (!hpInRange(v, hp_c.gsMin, hp_c.gsMax)) return false; hp_spGs = v; return true; }
bool hpSetFollowRange(float v) { if (!hpInRange(v, hp_c.followMin, hp_c.followMax)) return false; hp_followRange = v; return true; }
bool hpSetStopDist(float v)    { if (!hpInRange(v, hp_c.stopMin, hp_c.stopMax)) return false; hp_stopDist = v; return true; }

bool hpAnyEngaged()      { return hpAircraftEngaged() || hpRoverEngaged(); }
bool hpAttitudeEngaged() { return hp_pitchMode != HP_PITCH_OFF || hp_latMode != HP_LAT_OFF; }
bool hpThrustEngaged()   { return hp_thrMode != HP_THR_OFF; }
bool hpRoverEngaged()    { return hp_cruise || hp_rhdg || hp_rtgt || hp_follow; }

/***************************************************************************************
   Loops
****************************************************************************************/
static float hpVsLoop(float vsCmd, float dt) {
  float err = vsCmd - hp_t.velVertical;
  hp_vsInt = attClampf(hp_vsInt + hp_c.vsKi * err * dt, -hp_c.pitchMax, hp_c.pitchMax);
  return attClampf(hp_c.vsKp * err + hp_vsInt, -hp_c.pitchMax, hp_c.pitchMax);
}

static void hpUpdateAircraft(uint32_t now, float dt) {
  // ---- Disconnect rules ----
  if (!hp_t.inAtmo) {
    if (hp_noAtmoSince == 0) hp_noAtmoSince = now;
    else if (now - hp_noAtmoSince >= hp_c.noAtmoMs) { hpDisconnectAircraft(HP_REASON_NO_ATMO); return; }
  } else hp_noAtmoSince = 0;

  // (Pilot stick / lever input is handled globally in hpUpdate: it drops everything.)
  if (!hp_t.hasTarget && (hp_latMode == HP_LAT_NAV || hp_pitchMode == HP_PITCH_GS)) {
    if (hp_latMode == HP_LAT_NAV)   hp_latMode = HP_LAT_OFF;
    if (hp_pitchMode == HP_PITCH_GS) hp_pitchMode = HP_PITCH_OFF;
    hpSetReason(HP_REASON_NO_TARGET);
  }
  if (hp_pitchMode == HP_PITCH_GS && hp_t.tgtDist < hp_c.gsMinRange) { hp_pitchMode = HP_PITCH_OFF; hpSetReason(HP_REASON_FLARE); }
  hpReconcileSAS();
  if (!hpAircraftEngaged()) { rotClearAutoAxes(); arbReleaseAttitude(AP_OWNER_HOLD); return; }

  // ---- Pitch group -> commanded pitch ----
  AttMeasure m; m.pitch = hp_t.pitch; m.heading = hp_t.heading; m.roll = hp_t.roll;
  attUpdateRates(hp_att, m, dt);

  switch (hp_pitchMode) {
    case HP_PITCH_ATT: hp_cmdPitch = hp_spAtt; break;
    case HP_PITCH_AOA: hp_cmdPitch = attClampf(hp_t.srfVelPitch + hp_spAoa, -89.0f, 89.0f); break;
    case HP_PITCH_VS:  hp_cmdPitch = hpVsLoop(hp_spVs, dt); break;
    case HP_PITCH_ALT: {
      float vsCmd = attClampf(hp_c.altKp * (hp_spAlt - hp_t.altSea), -hp_c.vsMax, hp_c.vsMax);
      hp_cmdPitch = hpVsLoop(vsCmd, dt);
      break;
    }
    case HP_PITCH_GS: {
      // Hold the depression angle to the targeted flag: nominal descent for the glide angle
      // plus a correction on the angle error, both scaled by ground speed.
      float vg = hp_t.velSurface;
      float depression = -hp_t.tgtElev;
      float vsCmd = -vg * tanf(hp_spGs * 0.0174532925199f) + hp_c.gsKp * vg * (hp_spGs - depression);
      hp_cmdPitch = hpVsLoop(attClampf(vsCmd, -hp_c.vsMax, hp_c.vsMax), dt);
      break;
    }
    default: hp_cmdPitch = hp_t.pitch; break;
  }

  // ---- Lateral group -> commanded bank (or heading, rocket entry) ----
  switch (hp_latMode) {
    case HP_LAT_ROLL: hp_cmdBank = hp_spRoll; break;
    case HP_LAT_HDG:  hp_cmdBank = attClampf(hp_c.hdgKp * attWrap180(hp_spHdg - hp_t.heading), -hp_c.bankMax, hp_c.bankMax); break;
    case HP_LAT_NAV:  hp_cmdBank = attClampf(hp_c.hdgKp * attWrap180(hp_t.tgtBearing - hp_t.heading), -hp_c.bankMax, hp_c.bankMax); break;
    default:          hp_cmdBank = hp_t.roll; break;
  }

  uint8_t mask = 0;
  if (hp_pitchMode != HP_PITCH_OFF) mask |= ROT_AXIS_PITCH;
  if (hp_latMode   != HP_LAT_OFF)   mask |= ROT_AXIS_ROLL;

  if (mask) {
    AttCommand c;
    if (hp_c.steerLikeRocket) {
      float cmdHeading = (hp_latMode == HP_LAT_HDG) ? hp_spHdg : hp_holdHeading;
      c = attSteerRocket(hp_att, hp_c.rocketGains, m, hp_cmdPitch, cmdHeading,
                         hp_latMode == HP_LAT_ROLL, hp_spRoll, hp_c.maxControlDeflection, dt);
      if (hp_latMode != HP_LAT_OFF) mask |= ROT_AXIS_YAW;
    } else {
      c = attSteerAircraft(hp_att, hp_c.acftGains, m, hp_cmdPitch, hp_cmdBank,
                           hp_c.coordinateTurn, hp_c.maxControlDeflection, dt);
      if (hp_c.coordinateTurn && hp_latMode != HP_LAT_OFF) mask |= ROT_AXIS_YAW;
    }
    rotSetAutoAxes(c.pitch, c.yaw, c.roll, mask);
  } else {
    rotClearAutoAxes();
  }

  // ---- Thrust group -> throttle ----
  if (hp_thrMode != HP_THR_OFF) {
    float err, kp, ki;
    if (hp_thrMode == HP_THR_IAS) { err = hp_spIas - hp_t.ias;   kp = hp_c.iasKp;  ki = hp_c.iasKi;  }
    else                         { err = hp_spMach - hp_t.mach; kp = hp_c.machKp; ki = hp_c.machKi; }
    hp_thrInt = attClampf(hp_thrInt + ki * err * dt, 0.0f, 1.0f);
    float target = attClampf(kp * err + hp_thrInt, 0.0f, 1.0f);
    float step = hp_c.throttleSlew * dt;
    if      (target > hp_thrOut + step) hp_thrOut += step;
    else if (target < hp_thrOut - step) hp_thrOut -= step;
    else                               hp_thrOut  = target;
    thrAutoThrottle(THR_OWNER_HOLD, hp_thrOut);
  }
}

static void hpUpdateRover(uint32_t now, float dt) {
  // ---- Guards and disconnects ----
  if (!hpOnGround()) {
    if (hp_airborneSince == 0) hp_airborneSince = now;
    else if (now - hp_airborneSince >= hp_c.airborneMs) { hpDisconnectRover(HP_REASON_AIRBORNE); return; }
  } else hp_airborneSince = 0;

  if (fabsf(hp_t.roll) > hp_maxRoll) {
    hpDisconnectRover(HP_REASON_ROLL_LIMIT);
    mySimpit.activateAction(BRAKES_ACTION);      // stays applied until the pilot releases it
    return;
  }
  if (hpRoverDriving() && hp_t.brakes) {
    hp_cruise = false; hp_follow = false; hp_slopeGuard = false; hp_wheelOut = 0.0f;
    hpSendWheels(true, 0.0f, false, 0.0f);
    hpSetRoverReason(HP_REASON_BRAKES);
  }
  if (hp_rtgt && !hp_t.hasTarget) { hp_rtgt = false; hpSetRoverReason(HP_REASON_REFUSED); }
  if (!hpRoverEngaged()) return;

  float speed = hpSignedSpeed();
  bool reversing = speed < -0.3f;

  // Target lost: drive-to-target and follow have nothing to steer to.
  if ((hp_rtgt || hp_follow) && !hp_t.hasTarget) {
    hp_rtgt = false;
    if (hp_follow) { hp_follow = false; hp_wheelOut = 0.0f; hpSendWheels(true, 0.0f, false, 0.0f); }
    hpSetRoverReason(HP_REASON_NO_TARGET);
    if (!hpRoverEngaged()) return;
  }

  // ---- Speed setpoint: CRUISE (with arrive-and-stop under TGT) or FOLLOW ----
  bool driving = hpRoverDriving();
  float sp = 0.0f;
  if (hp_cruise) {
    sp = attClampf(hp_spCruise, -hp_maxSpeed, hp_maxSpeed);
    if (hp_rtgt) {
      // Arrive-and-stop: cap the setpoint by the distance left, stop at STOP with the brakes on.
      float left = hp_t.tgtDist - hp_stopDist;
      if (left <= 0.0f) {
        hp_cruise = hp_rtgt = false; hp_wheelOut = hp_wheelInt = 0.0f; hp_slopeGuard = false;
        hpSendWheels(true, 0.0f, true, 0.0f);
        mySimpit.activateAction(BRAKES_ACTION);
        hpSetRoverReason(HP_REASON_ARRIVED);
        return;
      }
      float cap = sqrtf(2.0f * hp_c.stopDecel * left);
      if (sp > cap) sp = cap;
    }
  } else if (hp_follow) {
    sp = attClampf(hp_c.followKp * (hp_t.tgtDist - hp_followRange) + hp_t.tgtClosing, -hp_maxSpeed, hp_maxSpeed);
  }

  if (driving) {
    float slope = fabsf(hp_t.pitch);
    float half = hp_maxSlope * 0.5f;
    float scale = 1.0f;
    if (slope >= hp_maxSlope)  scale = 0.0f;
    else if (slope > half)    scale = 1.0f - (slope - half) / (hp_maxSlope - half);
    hp_slopeGuard = (scale < 1.0f);
    sp *= scale;

    float err = sp - speed;
    hp_wheelInt = attClampf(hp_wheelInt + hp_c.cruiseKi * err * dt, -1.0f, 1.0f);
    float target = attClampf(hp_c.cruiseKp * err + hp_wheelInt, -1.0f, 1.0f);
    float step = hp_c.wheelSlew * dt;
    if      (target > hp_wheelOut + step) hp_wheelOut += step;
    else if (target < hp_wheelOut - step) hp_wheelOut -= step;
    else                                  hp_wheelOut  = target;
  }

  // ---- Steering: HDG setpoint, or the target bearing for TGT and FOLLOW ----
  bool steering = hp_rhdg || hp_rtgt || hp_follow;
  if (steering) {
    float tgt = hp_rhdg ? hp_spRhdg : hp_t.tgtBearing;
    float err = attWrap180(tgt - hp_t.heading);
    if (reversing) err = -err;
    float f = attClampf(fabsf(speed) / hp_c.steerKpSpeed, 0.0f, 1.0f);
    float kp = hp_c.steerKpLow + (hp_c.steerKpHigh - hp_c.steerKpLow) * f;
    hp_steerOut = attClampf(hp_c.steerSign * kp * err, -1.0f, 1.0f);
  }

  hpSendWheels(driving, hp_wheelOut, steering, hp_steerOut);
}

void hpUpdate() {
  uint32_t now = millis();
  float dt = (now - hp_lastUpdateMs) * 0.001f;
  hp_lastUpdateMs = now;
  if (dt <= 0.0f || dt > 0.5f) dt = 0.02f;

  if (!hpAnyEngaged()) return;

  // Failsafe: telemetry loss. Aircraft: hand the airframe to stability assist at the
  // current throttle (cutting throttle is the rocket failsafe, not the aircraft one).
  // Rover: wheel throttle to zero, no brakes.
  if ((now - hp_t.lastMs) > hp_c.telemetryTimeout) {
    hpDisconnectAll(HP_REASON_TELEMETRY);
    return;
  }

  // The pilot has the vehicle: any input on the rotation stick, the translation stick or
  // the throttle lever disconnects every hold mode (rover modes included).
  uint8_t ovr;
  if (pilotOverrideDetected(ovr)) {
    hpDisconnectAll(ovr);
    return;
  }

  if (hpAircraftEngaged()) hpUpdateAircraft(now, dt);
  if (hpRoverEngaged())    hpUpdateRover(now, dt);
}

/***************************************************************************************
   Status
****************************************************************************************/
static uint8_t hpAgeSeconds(uint32_t sinceMs, uint8_t reason) {
  if (reason == HP_REASON_NONE) return 255;
  uint32_t a = (millis() - sinceMs) / 1000UL;
  return a > 255 ? 255 : (uint8_t)a;
}

HoldStatus hpGetStatus() {
  HoldStatus s;
  s.pitchMode = hp_pitchMode; s.latMode = hp_latMode; s.thrMode = hp_thrMode;
  s.reason = hp_reason; s.reasonAge = hpAgeSeconds(hp_reasonMs, hp_reason);
  s.anyEngaged = hpAnyEngaged();
  s.thrustEngaged = hpThrustEngaged();
  s.leverTouched = thrTouched();
  s.leverDriven  = thrLeverDriven();
  s.ascentArmed  = apIsArmed();
  s.att = hp_spAtt; s.aoa = hp_spAoa; s.vs = hp_spVs; s.alt = hp_spAlt;
  s.roll = hp_spRoll; s.hdg = hp_spHdg; s.ias = hp_spIas; s.mach = hp_spMach;
  s.gs = hp_spGs;
  s.cmdThrottle = hp_thrOut;
  s.cruise = hp_cruise; s.rhdg = hp_rhdg; s.rtgt = hp_rtgt; s.follow = hp_follow;
  s.brakes = hp_t.brakes; s.slopeGuard = hp_slopeGuard; s.targetAvailable = hp_t.hasTarget;
  s.roverReason = hp_roverReason; s.roverReasonAge = hpAgeSeconds(hp_roverReasonMs, hp_roverReason);
  s.cruiseSp = hp_spCruise; s.rhdgSp = hp_spRhdg;
  s.maxSpeed = hp_maxSpeed; s.maxSlope = hp_maxSlope; s.maxRoll = hp_maxRoll;
  s.followRange = hp_followRange; s.stopDist = hp_stopDist;
  s.cmdWheel = hp_wheelOut;
  return s;
}

const char *hpModeName(HpMode m) {
  switch (m) {
    case HP_MODE_ATT: return "ATT";   case HP_MODE_AOA: return "AOA";  case HP_MODE_VS:   return "VS";
    case HP_MODE_ALT: return "ALT";   case HP_MODE_ROLL: return "ROLL"; case HP_MODE_HDG:  return "HDG";
    case HP_MODE_IAS: return "IAS";   case HP_MODE_MACH: return "MACH";
    case HP_MODE_CRUISE: return "CRUISE"; case HP_MODE_RHDG: return "RHDG"; case HP_MODE_RTGT: return "RTGT";
    case HP_MODE_NAV: return "NAV"; case HP_MODE_GS: return "GS"; case HP_MODE_FOLLOW: return "FOLLOW";
    default: return "?";
  }
}

const char *hpReasonName(uint8_t r) {
  switch (r) {
    case HP_REASON_STICK: return "STICK";     case HP_REASON_LEVER: return "LEVER";
    case HP_REASON_BRAKES: return "BRAKES";   case HP_REASON_AIRBORNE: return "AIRBORNE";
    case HP_REASON_ROLL_LIMIT: return "ROLL LIMIT"; case HP_REASON_NO_ATMO: return "NO ATMO";
    case HP_REASON_TELEMETRY: return "TELEMETRY"; case HP_REASON_ASCENT: return "ASCENT";
    case HP_REASON_REFUSED: return "REFUSED";
    case HP_REASON_NO_NODE: return "NO NODE";     case HP_REASON_NO_TARGET: return "NO TARGET";
    case HP_REASON_SOI: return "SOI";             case HP_REASON_FUEL: return "FUEL";
    case HP_REASON_LANDED: return "LANDED";       case HP_REASON_OTHER_AP: return "OTHER AP";
    case HP_REASON_FLARE: return "FLARE";         case HP_REASON_ARRIVED: return "ARRIVED";
    case HP_REASON_ALIGN: return "ALIGN";         case HP_REASON_HANDOFF: return "HANDOFF";
    case HP_REASON_REPLAN: return "REPLAN";       default: return "";
  }
}

/***************************************************************************************
   Bench console — lines arrive from apSerialConsole() with the "HP " prefix stripped:
     ENG <MODE> [0|1]   engage (default) / disengage a mode by name
     SET <MODE> <v>     setpoint (MODE also accepts MAXSPD, MAXSLOPE, MAXROLL)
     LVL | OFF | STATUS
****************************************************************************************/
static int8_t hpModeFromName(const char *n) {
  for (uint8_t i = 0; i < HP_MODE_COUNT; i++)
    if (strncasecmp(n, hpModeName((HpMode)i), strlen(hpModeName((HpMode)i))) == 0) return (int8_t)i;
  return -1;
}

bool hpConsoleLine(const char *line) {
  if (strncasecmp(line, "ENG ", 4) == 0) {
    int8_t m = hpModeFromName(line + 4);
    if (m < 0) return false;
    const char *sp = strchr(line + 4, ' ');
    bool on = sp ? (atoi(sp + 1) != 0) : true;
    return hpEngage((HpMode)m, on);
  }
  if (strncasecmp(line, "SET ", 4) == 0) {
    const char *arg = line + 4;
    const char *sp = strchr(arg, ' ');
    if (!sp) return false;
    float v = atof(sp + 1);
    if      (strncasecmp(arg, "MAXSPD", 6) == 0)   return hpSetMaxSpeed(v);
    else if (strncasecmp(arg, "MAXSLOPE", 8) == 0) return hpSetMaxSlope(v);
    else if (strncasecmp(arg, "MAXROLL", 7) == 0)  return hpSetMaxRoll(v);
    else if (strncasecmp(arg, "STOP", 4) == 0)     return hpSetStopDist(v);
    int8_t m = hpModeFromName(arg);
    switch (m) {
      case HP_MODE_ATT: return hpSetAtt(v);   case HP_MODE_AOA: return hpSetAoa(v);
      case HP_MODE_VS:  return hpSetVs(v);    case HP_MODE_ALT: return hpSetAlt(v);
      case HP_MODE_ROLL: return hpSetRoll(v); case HP_MODE_HDG: return hpSetHdg(v);
      case HP_MODE_IAS: return hpSetIas(v);   case HP_MODE_MACH: return hpSetMach(v);
      case HP_MODE_CRUISE: return hpSetCruise(v); case HP_MODE_RHDG: return hpSetRoverHdg(v);
      case HP_MODE_GS: return hpSetGs(v); case HP_MODE_FOLLOW: return hpSetFollowRange(v);
      default: return false;
    }
  }
  if (strncasecmp(line, "LVL", 3) == 0)  { hpLevel(); return true; }
  if (strncasecmp(line, "OFF", 3) == 0)  { hpDisconnectAll(HP_REASON_PILOT); return true; }
  if (strncasecmp(line, "STATUS", 6) == 0) {
    HoldStatus s = hpGetStatus();
    Serial.print(F("HP pitch=")); Serial.print(s.pitchMode);
    Serial.print(F(" lat="));     Serial.print(s.latMode);
    Serial.print(F(" thr="));     Serial.print(s.thrMode);
    Serial.print(F(" reason="));  Serial.print(hpReasonName(s.reason));
    Serial.print(F(" att="));     Serial.print(s.att, 1);
    Serial.print(F(" vs="));      Serial.print(s.vs, 1);
    Serial.print(F(" alt="));     Serial.print(s.alt, 0);
    Serial.print(F(" hdg="));     Serial.print(s.hdg, 0);
    Serial.print(F(" ias="));     Serial.print(s.ias, 0);
    Serial.print(F(" thrOut="));  Serial.print(s.cmdThrottle, 2);
    Serial.print(F(" | cruise=")); Serial.print(s.cruise);
    Serial.print(F(" sp="));      Serial.print(s.cruiseSp, 1);
    Serial.print(F(" wheel="));   Serial.print(s.cmdWheel, 2);
    Serial.print(F(" rreason=")); Serial.println(hpReasonName(s.roverReason));
    return true;
  }
  return false;
}

/***************************************************************************************
   Telemetry ingest
****************************************************************************************/
static inline void hpStamp() { hp_t.lastMs = millis(); }

void hpIngestFlightStatus(uint8_t vesselType, uint8_t situation, bool hasTarget) {
  hp_t.vesselType = vesselType; hp_t.situation = situation; hp_t.hasTarget = hasTarget; hpStamp();
}
void hpIngestAltitude(float sealevel)                   { hp_t.altSea = sealevel; hpStamp(); }
void hpIngestVelocity(float surface, float vertical)    { hp_t.velSurface = surface; hp_t.velVertical = vertical; hpStamp(); }
void hpIngestAirspeed(float ias, float mach)            { hp_t.ias = ias; hp_t.mach = mach; hpStamp(); }
void hpIngestAttitude(float heading, float pitch, float roll, float srfVelHeading, float srfVelPitch) {
  hp_t.heading = heading; hp_t.pitch = pitch; hp_t.roll = roll;
  hp_t.srfVelHeading = srfVelHeading; hp_t.srfVelPitch = srfVelPitch; hpStamp();
}
void hpIngestAtmo(bool hasAtmosphere, bool inAtmosphere) { hp_t.hasAtmo = hasAtmosphere; hp_t.inAtmo = inAtmosphere; hpStamp(); }
void hpIngestBrakes(bool on)                             { hp_t.brakes = on; }
void hpIngestTarget(bool available, float bearingDeg, float elevationDeg, float distance, float closingRate) {
  hp_t.hasTarget = available; hp_t.tgtBearing = attWrap360(bearingDeg); hp_t.tgtElev = elevationDeg;
  hp_t.tgtDist = distance; hp_t.tgtClosing = closingRate;
}
void hpIngestThrottle(float t01)                         { hp_t.throttle = attClampf(t01, 0.0f, 1.0f); }

/***************************************************************************************
   infodisp_link.ino — Info Display 2 (0x13) autopilot console link for the master.

   Byte-level contract: Documents/Developer/Ascent_Autopilot_Interface.md (transport,
   ascent opcodes, 40-byte ascent status) and Hold_Mode_Autopilot.md §8 (hold-mode
   opcodes, 44-byte aircraft status, 28-byte rover status).

     Poll  (every IDL_POLL_MS)  : read the 10-byte outbound packet; a non-zero cmdSeq that
                                  differs from the last executed one, with a good XOR
                                  checksum, is applied ONCE and then acknowledged.
     ACK   (2-byte control write): controlByte carries the master's mode bits; byte 1 is
                                  the sequence just executed (0 = nothing to ack).
     Push  (every IDL_PUSH_MS)  : the status frame the console on screen needs, chosen by
                                  the activeScreen byte. Skipped while the display is in
                                  demo mode (it generates its own status then).

   Info Display 1 (0x12) never carries a command (its console is compiled out), so only
   the mission panel is polled. Both displays are sent PROCEED at init so they leave
   their boot hold; the rest of the display-carrier handshake is still to be integrated.
****************************************************************************************/
#include "control_links.h"
#include "ascent_autopilot.h"
#include "hold_autopilot.h"
#include "burn_autopilot.h"
#include "landing_autopilot.h"

static const uint32_t IDL_POLL_MS = 50;
static const uint32_t IDL_PUSH_MS = 100;

static const uint8_t IDL_SYNC_OUT      = 0xAE;   // InfoDisp -> master framing byte
static const uint8_t IDL_SYNC_ASCENT   = 0xA5;
static const uint8_t IDL_SYNC_AIRCRAFT = 0xA6;
static const uint8_t IDL_SYNC_ROVER    = 0xA7;
static const uint8_t IDL_SYNC_ORBITAL  = 0xA8;
static const uint8_t IDL_SYNC_LANDING  = 0xA9;
static const uint8_t IDL_LEN_ASCENT    = 40;
static const uint8_t IDL_LEN_AIRCRAFT  = 48;   // grew from 44: gs angle (Mission_Autopilot.md §8.1)
static const uint8_t IDL_LEN_ROVER     = 36;   // grew from 28: followRange, stopDist
static const uint8_t IDL_LEN_ORBITAL   = 52;
static const uint8_t IDL_LEN_LANDING   = 44;

// InfoDisp ScreenType values for the three consoles (KCMk1_InfoDisp.h)
static const uint8_t IDL_SCREEN_LNCHAP = 12;
static const uint8_t IDL_SCREEN_ACFTAP = 14;
static const uint8_t IDL_SCREEN_ROVRAP = 15;
static const uint8_t IDL_SCREEN_ORBTAP = 16;
static const uint8_t IDL_SCREEN_LNDGAP = 17;

// requestType nibble (I2C Protocol Specification §15.3)
static const uint8_t IDL_REQ_NOP     = 0x0;
static const uint8_t IDL_REQ_PROCEED = 0x2;

// Command opcodes (Ascent_Autopilot_Interface.md §4, Hold_Mode_Autopilot.md §8.1)
enum {
  IDL_CMD_SET_TARGET_ALT = 0x01, IDL_CMD_SET_INCLINATION = 0x02, IDL_CMD_SET_LAUNCH_DIR = 0x03,
  IDL_CMD_SET_LOFT = 0x04, IDL_CMD_SET_ROLL = 0x05, IDL_CMD_SET_MAXG = 0x06,
  IDL_CMD_ARM = 0x10, IDL_CMD_DISARM = 0x11,
  IDL_CMD_HOLD_AP_OFF = 0x12, IDL_CMD_HOLD_LVL = 0x13,
  IDL_CMD_ENGAGE_ATT = 0x20, IDL_CMD_ENGAGE_AOA, IDL_CMD_ENGAGE_VS, IDL_CMD_ENGAGE_ALT,
  IDL_CMD_ENGAGE_ROLL, IDL_CMD_ENGAGE_HDG, IDL_CMD_ENGAGE_IAS, IDL_CMD_ENGAGE_MACH,
  IDL_CMD_SET_ATT = 0x28, IDL_CMD_SET_AOA, IDL_CMD_SET_VS, IDL_CMD_SET_ALT,
  IDL_CMD_SET_HROLL, IDL_CMD_SET_HDG, IDL_CMD_SET_IAS, IDL_CMD_SET_MACH,
  IDL_CMD_ENGAGE_CRUISE = 0x30, IDL_CMD_ENGAGE_RHDG, IDL_CMD_ENGAGE_RTGT,
  IDL_CMD_SET_CRUISE = 0x33, IDL_CMD_SET_RHDG, IDL_CMD_SET_MAXSPD, IDL_CMD_SET_MAXSLOPE, IDL_CMD_SET_MAXROLL,
  // Mission autopilot (Mission_Autopilot.md §8)
  IDL_CMD_ENGAGE_NAV = 0x14, IDL_CMD_ENGAGE_GS = 0x15, IDL_CMD_SET_GS = 0x16,
  IDL_CMD_ENGAGE_FOLLOW = 0x38, IDL_CMD_SET_FOLLOW_RANGE = 0x39, IDL_CMD_SET_STOP_DIST = 0x3A,
  IDL_CMD_ARM_NODE = 0x40, IDL_CMD_ARM_AP, IDL_CMD_ARM_PE, IDL_CMD_ARM_INC, IDL_CMD_ENGAGE_APPR = 0x44,
  IDL_CMD_SET_AP = 0x48, IDL_CMD_SET_PE, IDL_CMD_SET_INC, IDL_CMD_SET_APPR_RATE, IDL_CMD_SET_APPR_DIST,
  IDL_CMD_SET_WARP = 0x4D, IDL_CMD_SET_AUTOSTAGE = 0x4E, IDL_CMD_EXEC = 0x4F,
  IDL_CMD_ENGAGE_DESC = 0x50, IDL_CMD_ENGAGE_HOVR, IDL_CMD_ENGAGE_BRAKE, IDL_CMD_ENGAGE_ENTRY,
  IDL_CMD_SET_DESC_RATE = 0x58, IDL_CMD_SET_HOVR_ALT, IDL_CMD_SET_TWR, IDL_CMD_SET_MARGIN, IDL_CMD_SET_ENTRY_AOA, IDL_CMD_SET_ENTRY_ROLL,
  IDL_CMD_SET_ATT_REF = 0x5E
};

static uint8_t  g_idlLastSeq    = 0;
static uint8_t  g_idlAckPending = 0;
static uint8_t  g_idlScreen     = 0xFF;
static bool     g_idlDemo       = false;
static uint32_t g_idlLastPollMs = 0, g_idlLastPushMs = 0;

static uint8_t idlControlByte(uint8_t reqType) {
  uint8_t b = (uint8_t)(reqType << 4);
  if (debug)    b |= 0x01;
  if (demo)     b |= 0x02;
  if (trimMode) b |= 0x04;
  if (idleMode) b |= 0x08;
  return b;
}

static void idlWrite(uint8_t addr, const uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(addr);
  Wire.write(buf, n);
  Wire.endTransmission();
}

static void idlSendControl(uint8_t addr, uint8_t reqType, uint8_t ackSeq) {
  uint8_t c[2] = { idlControlByte(reqType), ackSeq };
  idlWrite(addr, c, 2);
}

void idlInit() {
  // Release both display carriers from their boot hold.
  idlSendControl(INFO_MC,  IDL_REQ_PROCEED, 0);
  idlSendControl(INFO2_MC, IDL_REQ_PROCEED, 0);
  g_idlLastPollMs = g_idlLastPushMs = millis();
}

/***************************************************************************************
   Apply one console command
****************************************************************************************/
static void idlApply(uint8_t op, float v) {
  switch (op) {
    // ---- Ascent autopilot ----
    case IDL_CMD_SET_TARGET_ALT:  apSetTargetAltitude(v); break;
    case IDL_CMD_SET_INCLINATION: apSetTargetInclination(v); break;
    case IDL_CMD_SET_LAUNCH_DIR:  apSetLaunchSoutherly(v != 0.0f); break;
    case IDL_CMD_SET_LOFT:        apSetLoft(v); break;
    case IDL_CMD_SET_ROLL:        if (v >= 1.0e8f) apSetRoll(false, 0.0f); else apSetRoll(true, v); break;
    case IDL_CMD_SET_MAXG:        apSetMaxG(v); break;
    case IDL_CMD_ARM:             apArm(); break;
    case IDL_CMD_DISARM:          apDisarm(); break;
    // ---- Hold-mode autopilot ----
    case IDL_CMD_HOLD_AP_OFF:     arbAllOff(); break;   // A/P OFF from any console: everything (review q.6)
    case IDL_CMD_HOLD_LVL:        hpLevel(); break;
    case IDL_CMD_ENGAGE_ATT:      hpEngage(HP_MODE_ATT,    v != 0.0f); break;
    case IDL_CMD_ENGAGE_AOA:      hpEngage(HP_MODE_AOA,    v != 0.0f); break;
    case IDL_CMD_ENGAGE_VS:       hpEngage(HP_MODE_VS,     v != 0.0f); break;
    case IDL_CMD_ENGAGE_ALT:      hpEngage(HP_MODE_ALT,    v != 0.0f); break;
    case IDL_CMD_ENGAGE_ROLL:     hpEngage(HP_MODE_ROLL,   v != 0.0f); break;
    case IDL_CMD_ENGAGE_HDG:      hpEngage(HP_MODE_HDG,    v != 0.0f); break;
    case IDL_CMD_ENGAGE_IAS:      hpEngage(HP_MODE_IAS,    v != 0.0f); break;
    case IDL_CMD_ENGAGE_MACH:     hpEngage(HP_MODE_MACH,   v != 0.0f); break;
    case IDL_CMD_SET_ATT:         hpSetAtt(v); break;
    case IDL_CMD_SET_AOA:         hpSetAoa(v); break;
    case IDL_CMD_SET_VS:          hpSetVs(v); break;
    case IDL_CMD_SET_ALT:         hpSetAlt(v); break;
    case IDL_CMD_SET_HROLL:       hpSetRoll(v); break;
    case IDL_CMD_SET_HDG:         hpSetHdg(v); break;
    case IDL_CMD_SET_IAS:         hpSetIas(v); break;
    case IDL_CMD_SET_MACH:        hpSetMach(v); break;
    case IDL_CMD_ENGAGE_CRUISE:   hpEngage(HP_MODE_CRUISE, v != 0.0f); break;
    case IDL_CMD_ENGAGE_RHDG:     hpEngage(HP_MODE_RHDG,   v != 0.0f); break;
    case IDL_CMD_ENGAGE_RTGT:     hpEngage(HP_MODE_RTGT,   v != 0.0f); break;
    case IDL_CMD_SET_CRUISE:      hpSetCruise(v); break;
    case IDL_CMD_SET_RHDG:        hpSetRoverHdg(v); break;
    case IDL_CMD_SET_MAXSPD:      hpSetMaxSpeed(v); break;
    case IDL_CMD_SET_MAXSLOPE:    hpSetMaxSlope(v); break;
    case IDL_CMD_SET_MAXROLL:     hpSetMaxRoll(v); break;
    // ---- Mission autopilot ----
    case IDL_CMD_ENGAGE_NAV:      hpEngage(HP_MODE_NAV, v != 0.0f); break;
    case IDL_CMD_ENGAGE_GS:       hpEngage(HP_MODE_GS,  v != 0.0f); break;
    case IDL_CMD_SET_GS:          hpSetGs(v); break;
    case IDL_CMD_ENGAGE_FOLLOW:   hpEngage(HP_MODE_FOLLOW, v != 0.0f); break;
    case IDL_CMD_SET_FOLLOW_RANGE: hpSetFollowRange(v); break;
    case IDL_CMD_SET_STOP_DIST:   hpSetStopDist(v); break;
    case IDL_CMD_ARM_NODE:        bpArm(BP_MODE_NODE, v != 0.0f); break;
    case IDL_CMD_ARM_AP:          bpArm(BP_MODE_AP,   v != 0.0f); break;
    case IDL_CMD_ARM_PE:          bpArm(BP_MODE_PE,   v != 0.0f); break;
    case IDL_CMD_ARM_INC:         bpArm(BP_MODE_INC,  v != 0.0f); break;
    case IDL_CMD_ENGAGE_APPR:     bpEngageApproach(v != 0.0f); break;
    case IDL_CMD_SET_AP:          bpSetTargetAp(v); break;
    case IDL_CMD_SET_PE:          bpSetTargetPe(v); break;
    case IDL_CMD_SET_INC:         bpSetTargetInc(v); break;
    case IDL_CMD_SET_APPR_RATE:   bpSetApprRate(v); break;
    case IDL_CMD_SET_APPR_DIST:   bpSetApprDist(v); break;
    case IDL_CMD_SET_WARP:        bpSetAutoWarp(v != 0.0f); break;
    case IDL_CMD_SET_AUTOSTAGE:   asSetEnabled(v != 0.0f); break;
    case IDL_CMD_EXEC:            bpExecute(); break;
    case IDL_CMD_ENGAGE_DESC:     lpEngage(LP_MODE_DESC,  v != 0.0f); break;
    case IDL_CMD_ENGAGE_HOVR:     lpEngage(LP_MODE_HOVR,  v != 0.0f); break;
    case IDL_CMD_ENGAGE_BRAKE:    lpEngage(LP_MODE_BRAKE, v != 0.0f); break;
    case IDL_CMD_ENGAGE_ENTRY:    lpEngageEntry(v != 0.0f); break;
    case IDL_CMD_SET_DESC_RATE:   lpSetDescRate(v); break;
    case IDL_CMD_SET_HOVR_ALT:    lpSetHovrAlt(v); break;
    case IDL_CMD_SET_TWR:         lpSetTwr(v); break;
    case IDL_CMD_SET_MARGIN:      lpSetMargin(v); break;
    case IDL_CMD_SET_ENTRY_AOA:   lpSetEntryAoa(v); break;
    case IDL_CMD_SET_ENTRY_ROLL:  lpSetEntryRoll(v); break;
    case IDL_CMD_SET_ATT_REF:     lpSetAttRef(v != 0.0f); break;
    default: break;
  }
}

/***************************************************************************************
   Poll the console's outbound packet
****************************************************************************************/
static void idlPoll() {
  uint8_t buf[10];
  uint8_t got = 0;
  Wire.requestFrom((uint8_t)INFO2_MC, (uint8_t)10);
  while (Wire.available() && got < 10) buf[got++] = Wire.read();
  if (got < 10 || buf[0] != IDL_SYNC_OUT) return;

  g_idlDemo   = (buf[1] & 0x04) != 0;
  g_idlScreen = buf[2];

  uint8_t seq = buf[3];
  if (seq != 0 && seq != g_idlLastSeq) {
    uint8_t xs = 0;
    for (uint8_t i = 3; i < 9; i++) xs ^= buf[i];
    if (xs == buf[9]) {
      float payload;
      memcpy(&payload, &buf[5], 4);
      idlApply(buf[4], payload);
      g_idlLastSeq    = seq;
      g_idlAckPending = seq;
    }
  }
  if (g_idlAckPending) {
    idlSendControl(INFO2_MC, IDL_REQ_NOP, g_idlAckPending);
    g_idlAckPending = 0;
  }
}

/***************************************************************************************
   Status pushes
****************************************************************************************/
static void idlPutFloat(uint8_t *dst, float f) { memcpy(dst, &f, 4); }

static void idlPushAscent() {
  AscentStatus s = apGetStatus();
  AscentConfig &c = apGetConfig();
  uint8_t st[IDL_LEN_ASCENT] = {0};
  st[0] = IDL_SYNC_ASCENT;
  st[1] = (s.armed ? 0x01 : 0) | (c.launchSoutherly ? 0x02 : 0) | (c.rollControlEnabled ? 0x04 : 0);
  st[2] = (uint8_t)s.phase;
  float f[9] = { c.targetApoapsis, c.targetInclination, c.loft, c.targetRoll, c.maxG,
                 s.cmdPitch, s.cmdHeading, s.cmdThrottle, s.dynPressure };
  memcpy(&st[4], f, sizeof(f));
  idlWrite(INFO2_MC, st, IDL_LEN_ASCENT);
}

static void idlPushAircraft() {
  HoldStatus s = hpGetStatus();
  uint8_t st[IDL_LEN_AIRCRAFT] = {0};
  st[0] = IDL_SYNC_AIRCRAFT;
  st[1] = (s.anyEngaged ? 0x01 : 0) | (s.thrustEngaged ? 0x02 : 0) | (s.leverTouched ? 0x04 : 0) |
          (s.leverDriven ? 0x08 : 0) | (s.ascentArmed ? 0x10 : 0);
  st[2] = s.pitchMode; st[3] = s.latMode; st[4] = s.thrMode;
  st[5] = s.reason;    st[6] = s.reasonAge; st[7] = 0;
  float f[10] = { s.att, s.aoa, s.vs, s.alt, s.roll, s.hdg, s.ias, s.mach, s.gs, s.cmdThrottle };
  memcpy(&st[8], f, sizeof(f));
  idlWrite(INFO2_MC, st, IDL_LEN_AIRCRAFT);
}

static void idlPushRover() {
  HoldStatus s = hpGetStatus();
  uint8_t st[IDL_LEN_ROVER] = {0};
  st[0] = IDL_SYNC_ROVER;
  st[1] = (s.cruise ? 0x01 : 0) | (s.rhdg ? 0x02 : 0) | (s.rtgt ? 0x04 : 0) | (s.brakes ? 0x08 : 0) |
          (s.slopeGuard ? 0x10 : 0) | (s.targetAvailable ? 0x20 : 0) | (s.follow ? 0x40 : 0);
  st[2] = s.roverReason; st[3] = s.roverReasonAge;
  float f[8] = { s.cruiseSp, s.rhdgSp, s.maxSpeed, s.maxSlope, s.maxRoll, s.followRange, s.stopDist, s.cmdWheel };
  memcpy(&st[4], f, sizeof(f));
  idlWrite(INFO2_MC, st, IDL_LEN_ROVER);
}

static void idlPushOrbital() {
  BurnStatus s = bpGetStatus();
  uint8_t st[IDL_LEN_ORBITAL] = {0};
  st[0] = IDL_SYNC_ORBITAL;
  st[1] = (s.armed ? 0x01 : 0) | (s.executing ? 0x02 : 0) | (s.autoWarp ? 0x04 : 0) | (s.autoStage ? 0x08 : 0) |
          (s.targetAvailable ? 0x10 : 0) | (s.nodeAvailable ? 0x20 : 0) | (s.apprEngaged ? 0x40 : 0);
  st[2] = s.mode; st[3] = s.phase; st[4] = s.reason; st[5] = s.reasonAge;
  float f[11] = { s.targetAp, s.targetPe, s.targetInc, s.apprRate, s.apprDist, s.dvTotal, s.dvRemaining,
                  s.tIgnition, s.burnDuration, s.accelEst, s.cmdThrottle };
  memcpy(&st[8], f, sizeof(f));
  idlWrite(INFO2_MC, st, IDL_LEN_ORBITAL);
}

static void idlPushLanding() {
  LandingStatus s = lpGetStatus();
  uint8_t st[IDL_LEN_LANDING] = {0};
  st[0] = IDL_SYNC_LANDING;
  st[1] = (s.engaged ? 0x01 : 0) | (s.brakeArmed ? 0x02 : 0) | (s.brakeFiring ? 0x04 : 0) | (s.attRefRadial ? 0x08 : 0) |
          (s.autoStage ? 0x10 : 0) | (s.landed ? 0x20 : 0) | (s.brakeMarginal ? 0x40 : 0);
  st[2] = s.mode; st[3] = s.entry ? 1 : 0; st[4] = s.reason; st[5] = s.reasonAge; st[6] = s.accelSource;
  float f[9] = { s.descRate, s.hovrAlt, s.twrOverride, s.margin, s.entryAoa, s.entryRoll, s.ignitionAlt, s.accelEst, s.cmdThrottle };
  memcpy(&st[8], f, sizeof(f));
  idlWrite(INFO2_MC, st, IDL_LEN_LANDING);
}

void idlService() {
  uint32_t now = millis();
  if (now - g_idlLastPollMs >= IDL_POLL_MS) { g_idlLastPollMs = now; idlPoll(); }
  if (now - g_idlLastPushMs >= IDL_PUSH_MS) {
    g_idlLastPushMs = now;
    if (g_idlDemo) return;                       // display drives its own status in demo
    switch (g_idlScreen) {
      case IDL_SCREEN_LNCHAP: idlPushAscent();   break;
      case IDL_SCREEN_ACFTAP: idlPushAircraft(); break;
      case IDL_SCREEN_ROVRAP: idlPushRover();    break;
      case IDL_SCREEN_ORBTAP: idlPushOrbital();  break;
      case IDL_SCREEN_LNDGAP: idlPushLanding();  break;
      default: break;
    }
  }
}

/***************************************************************************************
   landing_autopilot.ino — DESC / HOVR / BRAKE / ENTRY.
   Contract in landing_autopilot.h; design in Mission_Autopilot.md §5, §7.2, §7.7.
****************************************************************************************/
#include "landing_autopilot.h"
#include "attitude_controller.h"
#include "control_links.h"
#include "hold_autopilot.h"

static const int32_t LP_AXIS_FULL = INT16_MAX;

struct LpTelemetry {
  uint8_t vesselType = 0, situation = 0;
  float altSea = 0, radarAlt = 0, velSurface = 0, velVertical = 0, mach = 0;
  float heading = 0, pitch = 0, roll = 0, orbVelHeading = 0, orbVelPitch = 0;
  float airDensity = 0; bool inAtmo = false;
  float gravity = 9.81f, flyHigh = 18000.0f; char bodyName[24] = {0};
  float throttle = 0;
  uint32_t lastMs = 0;
};
static LpTelemetry lp_t;
static LandingConfig lp_c;

static LpMode  lp_mode = LP_MODE_OFF;
static bool    lp_entry = false;
static bool    lp_brakeFiring = false;
static bool    lp_attRefRadial = false, lp_attRefRadialAuto = false;
static float   lp_descRate = -3.0f, lp_hovrAlt = 50.0f, lp_twr = 0.0f, lp_margin = 150.0f, lp_entryAoa = 8.0f, lp_entryRoll = 0.0f;
static float   lp_thrInt = 0.0f, lp_thrOut = 0.0f, lp_ignAlt = 0.0f;
static bool    lp_brakeMarginal = false;
static uint8_t lp_sasMode = 255;
static uint8_t lp_reason = HP_REASON_NONE; static uint32_t lp_reasonMs = 0;
static uint32_t lp_lastUpdateMs = 0;
static AttState lp_att;

static void lpSetReason(uint8_t r) { if (r == HP_REASON_PILOT) r = HP_REASON_NONE; lp_reason = r; lp_reasonMs = millis(); }
static inline bool lpOnGround() { return lp_t.situation == KSP_SIT_LANDED || lp_t.situation == KSP_SIT_SPLASHED; }
static inline float lpHorizontalSpeed() { float h2 = lp_t.velSurface * lp_t.velSurface - lp_t.velVertical * lp_t.velVertical; return h2 > 0.0f ? sqrtf(h2) : 0.0f; }

LandingConfig lpDefaultConfig() {
  LandingConfig c;
  c.entryGains = attRocketGains();
  c.descKp = 0.05f; c.descKi = 0.02f; c.descSlew = 0.5f;
  c.hovrKp = 0.3f; c.hovrVsCap = 10.0f;
  c.radialSwitchSpeed = 1.0f;
  c.brakeFactorEst = 0.25f; c.brakeFactorMeas = 0.10f; c.brakeFactorTwr = 0.05f;
  c.brakeLatencyS = 2.0f; c.brakeMinAccelG = 1.2f; c.brakeMarginalFrac = 0.9f;
  c.entryHandoffSpeed = 250.0f; c.entryPlaneMach = 2.5f; c.entryPlaneQ = 5000.0f;
  c.maxControlDeflection = 1.0f;
  c.telemetryTimeout = 2000;
  c.descMin = -50.0f; c.descMax = 20.0f; c.hovrMin = 2.0f; c.hovrMax = 5000.0f; c.twrMin = 0.0f; c.twrMax = 20.0f;
  c.marginMin = 0.0f; c.marginMax = 5000.0f; c.aoaMin = -30.0f; c.aoaMax = 40.0f; c.rollMin = -180.0f; c.rollMax = 180.0f;
  return c;
}

void lpInit() { lp_c = lpDefaultConfig(); lp_mode = LP_MODE_OFF; lp_entry = false; attReset(lp_att); lp_lastUpdateMs = millis(); }
LandingConfig &lpGetConfig() { return lp_c; }

/***************************************************************************************
   Outputs
****************************************************************************************/
static void lpThrottle(float t) {
  lp_thrOut = attClampf(t, 0.0f, 1.0f);
  thrAutoThrottle(THR_OWNER_LANDING, lp_thrOut);
  aeNoteThrottle(lp_thrOut);
}

static void lpSas(uint8_t mode) {
  if (lp_sasMode == mode) return;
  mySimpit.activateAction(SAS_ACTION);
  mySimpit.setSASMode(mode);
  lp_sasMode = mode;
}

static void lpReleaseThrottleOwner() { thrAutoRelease(THR_OWNER_LANDING); arbReleaseThrottle(AP_OWNER_LANDING); }
static void lpReleaseAttitudeOwner(bool sasStability) {
  rotClearAutoAxes();
  if (sasStability) { mySimpit.activateAction(SAS_ACTION); mySimpit.setSASMode(AP_STABILITYASSIST); }
  lp_sasMode = 255;
  arbReleaseAttitude(AP_OWNER_LANDING);
}

/***************************************************************************************
   Ignition altitude (Mission_Autopilot.md §7.2, review decision q.4)
****************************************************************************************/
static float lpAccel() { return (lp_twr > 0.0f) ? lp_twr * lp_t.gravity : aeAccel(); }
static uint8_t lpAccelSource() { return (lp_twr > 0.0f) ? AE_SRC_TWR : aeSource(); }

static float lpIgnitionAltitude() {
  float a = lpAccel(), g = lp_t.gravity;
  if (a <= g) return 0.0f;
  float k = lp_c.brakeFactorEst;
  switch (lpAccelSource()) { case AE_SRC_MEAS: k = lp_c.brakeFactorMeas; break; case AE_SRC_TWR: k = lp_c.brakeFactorTwr; break; default: break; }
  float v = lp_t.velSurface;
  float h = v * v / (2.0f * (a - g));
  return h * (1.0f + k) + fabsf(lp_t.velVertical) * lp_c.brakeLatencyS + lp_margin;
}

/***************************************************************************************
   Engage / disconnect
****************************************************************************************/
static void lpDropThrottleModes(uint8_t reason, bool keepThrottle) {
  if (lp_mode == LP_MODE_OFF) return;
  lp_mode = LP_MODE_OFF; lp_brakeFiring = false;
  if (!keepThrottle) lpThrottle(0.0f);
  lpReleaseThrottleOwner();
  if (!lp_entry) lpReleaseAttitudeOwner(true);
  lpSetReason(reason);
}

static void lpDropEntry(uint8_t reason) {
  if (!lp_entry) return;
  lp_entry = false;
  if (lp_mode == LP_MODE_OFF) lpReleaseAttitudeOwner(true);
  lpSetReason(reason);
}

void lpDisconnectAll(uint8_t reason) {
  bool keep = (reason == HP_REASON_TELEMETRY || reason == HP_REASON_STICK || reason == HP_REASON_LEVER || reason == HP_REASON_OTHER_AP);
  lpDropThrottleModes(reason, keep);
  lpDropEntry(reason);
}
void lpArbiterDropAttitude() { lpDropEntry(HP_REASON_OTHER_AP); lpDropThrottleModes(HP_REASON_OTHER_AP, true); }
void lpArbiterDropThrottle() { lpDropThrottleModes(HP_REASON_OTHER_AP, true); }

bool lpEngage(uint8_t mode, bool on) {
  if (!on) { if (lp_mode == mode) lpDropThrottleModes(HP_REASON_PILOT, false); return true; }
  if (mode < LP_MODE_DESC || mode > LP_MODE_BRAKE) return false;
  if ((millis() - lp_t.lastMs) > lp_c.telemetryTimeout) { lpSetReason(HP_REASON_REFUSED); return false; }
  if (lpOnGround()) { lpSetReason(HP_REASON_REFUSED); return false; }
  if (mode == LP_MODE_BRAKE && lpAccel() < lp_c.brakeMinAccelG * lp_t.gravity) { lpSetReason(HP_REASON_REFUSED); return false; }

  bool wasOff = (lp_mode == LP_MODE_OFF);
  arbTakeAttitude(AP_OWNER_LANDING);
  arbTakeThrottle(AP_OWNER_LANDING);
  if (wasOff) {
    float cur = thrLeverDriven() ? thrCurrentThrottle() : lp_t.throttle;
    lp_thrInt = lp_thrOut = attClampf(cur, 0.0f, 1.0f);
  }
  switch (mode) {
    case LP_MODE_HOVR:  lp_hovrAlt = attClampf(roundf(lp_t.radarAlt), lp_c.hovrMin, lp_c.hovrMax); break;
    case LP_MODE_BRAKE: lp_brakeFiring = false; break;
    default: break;
  }
  lp_mode = (LpMode)mode;
  lp_sasMode = 255;                     // force the SAS mode to be re-asserted
  return true;
}

bool lpEngageEntry(bool on) {
  if (!on) { lpDropEntry(HP_REASON_PILOT); return true; }
  if ((millis() - lp_t.lastMs) > lp_c.telemetryTimeout || lpOnGround()) { lpSetReason(HP_REASON_REFUSED); return false; }
  arbTakeAttitude(AP_OWNER_LANDING);
  mySimpit.deactivateAction(SAS_ACTION);   // raw rotation for ENTRY
  lp_sasMode = 255;
  attReset(lp_att);
  lp_entry = true;
  return true;
}

static bool lpInRange(float v, float lo, float hi) { return !(v < lo || v > hi); }
bool lpSetDescRate(float v)  { if (!lpInRange(v, lp_c.descMin, lp_c.descMax)) return false; lp_descRate = v; return true; }
bool lpSetHovrAlt(float v)   { if (!lpInRange(v, lp_c.hovrMin, lp_c.hovrMax)) return false; lp_hovrAlt = v; return true; }
bool lpSetTwr(float v)       { if (!lpInRange(v, lp_c.twrMin, lp_c.twrMax)) return false; lp_twr = v; aeSetTwrOverride(v, lp_t.gravity); return true; }
bool lpSetMargin(float v)    { if (!lpInRange(v, lp_c.marginMin, lp_c.marginMax)) return false; lp_margin = v; return true; }
bool lpSetEntryAoa(float v)  { if (!lpInRange(v, lp_c.aoaMin, lp_c.aoaMax)) return false; lp_entryAoa = v; return true; }
bool lpSetEntryRoll(float v) { if (!lpInRange(v, lp_c.rollMin, lp_c.rollMax)) return false; lp_entryRoll = v; return true; }
void lpSetAttRef(bool radial) { lp_attRefRadial = radial; }

bool lpAnyEngaged() { return lp_mode != LP_MODE_OFF || lp_entry; }

/***************************************************************************************
   Loops
****************************************************************************************/
static float lpDescLoop(float vsCmd, float dt) {
  float err = vsCmd - lp_t.velVertical;
  lp_thrInt = attClampf(lp_thrInt + lp_c.descKi * err * dt, 0.0f, 1.0f);
  float target = attClampf(lp_c.descKp * err + lp_thrInt, 0.0f, 1.0f);
  float step = lp_c.descSlew * dt;
  if      (target > lp_thrOut + step) return lp_thrOut + step;
  else if (target < lp_thrOut - step) return lp_thrOut - step;
  return target;
}

static void lpUpdateThrottleModes(uint32_t now, float dt) {
  (void)now;
  // Attitude reference: retrograde kills horizontal velocity; radial-out is pure vertical.
  // Retrograde swings wildly at low speed, so switch to radial-out automatically.
  bool radial = lp_attRefRadial || (lpHorizontalSpeed() < lp_c.radialSwitchSpeed);
  lp_attRefRadialAuto = radial && !lp_attRefRadial;
  if (!lp_entry) lpSas(radial ? AP_RADIALOUT : AP_RETROGRADE);

  lp_ignAlt = lpIgnitionAltitude();
  float a = lpAccel(), g = lp_t.gravity;
  float need = (a > g && lp_t.radarAlt > 0.0f) ? (lp_t.velSurface * lp_t.velSurface / (2.0f * fmaxf(lp_t.radarAlt, 1.0f)) + g) : 0.0f;
  lp_brakeMarginal = (need > lp_c.brakeMarginalFrac * a);

  switch (lp_mode) {
    case LP_MODE_DESC: lpThrottle(lpDescLoop(lp_descRate, dt)); break;
    case LP_MODE_HOVR: {
      float vsCmd = attClampf(lp_c.hovrKp * (lp_hovrAlt - lp_t.radarAlt), -lp_c.hovrVsCap, lp_c.hovrVsCap);
      lpThrottle(lpDescLoop(vsCmd, dt));
      break;
    }
    case LP_MODE_BRAKE: {
      if (!lp_brakeFiring) {
        lpThrottle(0.0f);
        if (lp_t.radarAlt <= lp_ignAlt && lp_t.velVertical < 0.0f) lp_brakeFiring = true;
      } else {
        lpThrottle(1.0f);
        if (lp_t.velVertical >= lp_descRate) {          // descent rate killed: hand off to DESC
          lp_mode = LP_MODE_DESC; lp_brakeFiring = false;
          lp_thrInt = lp_thrOut;
        }
      }
      break;
    }
    default: break;
  }
  asMaybeStage(asEnabled(), lp_thrOut, 50.0f);
}

static void lpUpdateEntry(uint32_t now, float dt) {
  (void)now;
  // Hand-off by vessel type (review decision q.5)
  if (lp_t.vesselType == KSP_TYPE_PLANE) {
    float q = 0.5f * lp_t.airDensity * lp_t.velSurface * lp_t.velSurface;
    if (lp_t.mach < lp_c.entryPlaneMach && q > lp_c.entryPlaneQ && lp_t.altSea < lp_t.flyHigh) {
      lp_entry = false;
      lpReleaseAttitudeOwner(false);
      lpSetReason(HP_REASON_HANDOFF);
      hpEngage(HP_MODE_ATT, true);     // captures the current pitch and bank on the aircraft console
      hpEngage(HP_MODE_ROLL, true);
      return;
    }
  } else if (lp_t.velSurface < lp_c.entryHandoffSpeed) {
    lpDropEntry(HP_REASON_HANDOFF);
    return;
  }
  AttMeasure m; m.pitch = lp_t.pitch; m.heading = lp_t.heading; m.roll = lp_t.roll;
  attUpdateRates(lp_att, m, dt);
  float refHdg = attWrap360(lp_t.orbVelHeading + 180.0f);
  float refPitch = -lp_t.orbVelPitch;
  AttCommand c = attSteerRocket(lp_att, lp_c.entryGains, m, attClampf(refPitch + lp_entryAoa, -89.0f, 89.0f), refHdg,
                                true, lp_entryRoll, lp_c.maxControlDeflection, dt);
  rotSetAutoAxes(c.pitch, c.yaw, c.roll, ROT_AXIS_PITCH | ROT_AXIS_YAW | ROT_AXIS_ROLL);
}

void lpUpdate() {
  uint32_t now = millis();
  float dt = (now - lp_lastUpdateMs) * 0.001f;
  lp_lastUpdateMs = now;
  if (dt <= 0.0f || dt > 0.5f) dt = 0.02f;
  if (!lpAnyEngaged()) return;

  // Failsafe: telemetry loss keeps the throttle where it is and hands over.
  if ((now - lp_t.lastMs) > lp_c.telemetryTimeout) { lpDisconnectAll(HP_REASON_TELEMETRY); return; }
  uint8_t ovr;
  if (pilotOverrideDetected(ovr)) { lpDisconnectAll(ovr); return; }
  if (lpOnGround()) { lpThrottle(0.0f); lpDisconnectAll(HP_REASON_LANDED); lpThrottle(0.0f); return; }

  if (lp_mode != LP_MODE_OFF) lpUpdateThrottleModes(now, dt);
  if (lp_entry) lpUpdateEntry(now, dt);
}

/***************************************************************************************
   Status / console
****************************************************************************************/
LandingStatus lpGetStatus() {
  LandingStatus s;
  s.mode = lp_mode; s.entry = lp_entry; s.engaged = lpAnyEngaged();
  s.brakeArmed = (lp_mode == LP_MODE_BRAKE && !lp_brakeFiring); s.brakeFiring = lp_brakeFiring;
  s.attRefRadial = lp_attRefRadial || lp_attRefRadialAuto; s.autoStage = asEnabled(); s.landed = lpOnGround();
  s.brakeMarginal = lp_brakeMarginal;
  s.reason = lp_reason; uint32_t a = (millis() - lp_reasonMs) / 1000UL; s.reasonAge = (lp_reason == HP_REASON_NONE) ? 255 : (a > 255 ? 255 : (uint8_t)a);
  s.accelSource = lpAccelSource();
  s.descRate = lp_descRate; s.hovrAlt = lp_hovrAlt; s.twrOverride = lp_twr; s.margin = lp_margin;
  s.entryAoa = lp_entryAoa; s.entryRoll = lp_entryRoll;
  s.ignitionAlt = lpIgnitionAltitude(); s.accelEst = lpAccel(); s.cmdThrottle = lp_thrOut;
  return s;
}

bool lpConsoleLine(const char *line) {
  if (strncasecmp(line, "ENG ", 4) == 0) {
    const char *n = line + 4; const char *sp = strchr(n, ' '); bool on = sp ? (atoi(sp + 1) != 0) : true;
    if (strncasecmp(n, "DESC", 4) == 0)  return lpEngage(LP_MODE_DESC, on);
    if (strncasecmp(n, "HOVR", 4) == 0)  return lpEngage(LP_MODE_HOVR, on);
    if (strncasecmp(n, "BRAKE", 5) == 0) return lpEngage(LP_MODE_BRAKE, on);
    if (strncasecmp(n, "ENTRY", 5) == 0) return lpEngageEntry(on);
    return false;
  }
  if (strncasecmp(line, "SET ", 4) == 0) {
    const char *a = line + 4; const char *sp = strchr(a, ' '); if (!sp) return false; float v = atof(sp + 1);
    if (strncasecmp(a, "RATE", 4) == 0)   return lpSetDescRate(v);
    if (strncasecmp(a, "ALT", 3) == 0)    return lpSetHovrAlt(v);
    if (strncasecmp(a, "TWR", 3) == 0)    return lpSetTwr(v);
    if (strncasecmp(a, "MARGIN", 6) == 0) return lpSetMargin(v);
    if (strncasecmp(a, "AOA", 3) == 0)    return lpSetEntryAoa(v);
    if (strncasecmp(a, "ROLL", 4) == 0)   return lpSetEntryRoll(v);
    if (strncasecmp(a, "RADIAL", 6) == 0) { lpSetAttRef(v != 0.0f); return true; }
    return false;
  }
  if (strncasecmp(line, "OFF", 3) == 0) { lpDisconnectAll(HP_REASON_PILOT); return true; }
  if (strncasecmp(line, "STATUS", 6) == 0) {
    LandingStatus s = lpGetStatus();
    Serial.print(F("LP mode=")); Serial.print(s.mode); Serial.print(F(" entry=")); Serial.print(s.entry);
    Serial.print(F(" ign=")); Serial.print(s.ignitionAlt, 0); Serial.print(F(" a=")); Serial.print(s.accelEst, 2);
    Serial.print(F(" src=")); Serial.print(s.accelSource); Serial.print(F(" thr=")); Serial.print(s.cmdThrottle, 2);
    Serial.print(F(" reason=")); Serial.println(hpReasonName(s.reason));
    return true;
  }
  return false;
}

/***************************************************************************************
   Telemetry ingest
****************************************************************************************/
static inline void lpStamp() { lp_t.lastMs = millis(); }
void lpIngestFlightStatus(uint8_t vesselType, uint8_t situation) { lp_t.vesselType = vesselType; lp_t.situation = situation; lpStamp(); }
void lpIngestAltitude(float sealevel, float surface) { lp_t.altSea = sealevel; lp_t.radarAlt = surface; lpStamp(); }
void lpIngestVelocity(float surface, float vertical) { lp_t.velSurface = surface; lp_t.velVertical = vertical; lpStamp(); }
void lpIngestAirspeed(float mach)                    { lp_t.mach = mach; lpStamp(); }
void lpIngestAttitude(float heading, float pitch, float roll, float orbVelHeading, float orbVelPitch) {
  lp_t.heading = heading; lp_t.pitch = pitch; lp_t.roll = roll; lp_t.orbVelHeading = orbVelHeading; lp_t.orbVelPitch = orbVelPitch; lpStamp();
}
void lpIngestAtmo(float airDensity, bool inAtmosphere) { lp_t.airDensity = airDensity; lp_t.inAtmo = inAtmosphere; lpStamp(); }
void lpIngestBody(float gravity, float flyHigh, const char *name) {
  bool changed = name && strncmp(name, lp_t.bodyName, sizeof(lp_t.bodyName)) != 0;
  if (gravity > 0.0f) lp_t.gravity = gravity;
  lp_t.flyHigh = (flyHigh > 0.0f) ? flyHigh : 18000.0f;
  if (name) { strncpy(lp_t.bodyName, name, sizeof(lp_t.bodyName) - 1); lp_t.bodyName[sizeof(lp_t.bodyName) - 1] = '\0'; }
  if (changed && lpAnyEngaged()) lpDisconnectAll(HP_REASON_SOI);
}
void lpIngestThrottle(float t01) { lp_t.throttle = attClampf(t01, 0.0f, 1.0f); }
void lpVesselChanged() {
  lp_mode = LP_MODE_OFF; lp_entry = false; lp_brakeFiring = false; lp_sasMode = 255;
  rotClearAutoAxes(); arbReleaseAttitude(AP_OWNER_LANDING); arbReleaseThrottle(AP_OWNER_LANDING);
  lp_reason = HP_REASON_NONE; lp_t.lastMs = millis();
}

/***************************************************************************************
   rotation_link.ino — Rotation joystick (0x28) link for the master.

   Reads the 12-byte KerbalJoystickCore packet:
     Byte 0-2  : universal header
     Byte 3    : button events   Byte 4 : change mask   Byte 5 : button state
     Byte 6-7  : AXIS1 (X)  -> roll      int16 big-endian
     Byte 8-9  : AXIS2 (Y)  -> pitch
     Byte 10-11: AXIS3 (Zr) -> yaw

   Sends ONE rotation message per frame that merges the pilot's stick with the axes the
   hold autopilot holds (rotSetAutoAxes). The Simpit plugin keeps only the latest
   rotation message, so a held axis and a pilot axis from two senders would clobber
   each other. While the ascent autopilot is armed it sends its own rotation and this
   link stays silent. The raw pilot demand is exposed (rotPilot*) even for held axes so
   the hold autopilot can detect a stick override. Design: Hold_Mode_Autopilot.md §6.6.

   Trim, precision scaling and the joystick buttons are not handled here yet — this tab
   is the initial forwarding path; the ROTATION module's buttons still need their
   controller-side sequencing.
****************************************************************************************/
#include "control_links.h"
#include "ascent_autopilot.h"
#include "hold_autopilot.h"     // HP_REASON_* for the override reason

static const uint32_t ROT_POLL_MS   = 20;
static const uint32_t ROT_SEND_MS   = 20;     // max merged-message rate
static const float    ROT_PRECISION = 0.3f;   // stick scale in precision mode
static const float    ROT_OVERRIDE_THRESHOLD = 0.10f;   // |axis| beyond this (module deadzone already applied) ...
static const uint32_t ROT_OVERRIDE_MS        = 150;     // ... for this long = the pilot wants the vehicle

static float    g_trnX = 0.0f, g_trnY = 0.0f, g_trnZ = 0.0f;   // translation stick, -1..1
static uint32_t g_trnLastPollMs = 0;
static uint32_t g_ovrRotSince = 0, g_ovrTrnSince = 0;

static float    g_rotPitch = 0.0f, g_rotYaw = 0.0f, g_rotRoll = 0.0f;   // pilot, -1..1
static float    g_autoPitch = 0.0f, g_autoYaw = 0.0f, g_autoRoll = 0.0f;
static uint8_t  g_autoMask = 0;
static bool     g_rotDirty = false;
static uint32_t g_rotLastPollMs = 0, g_rotLastSendMs = 0;
static int16_t  g_lastSentP = 0, g_lastSentY = 0, g_lastSentR = 0;
static uint8_t  g_lastSentMask = 0xFF;

static inline int16_t rotBE(const uint8_t *p) { return (int16_t)(((uint16_t)p[0] << 8) | p[1]); }
static inline float   rotNorm(int16_t v)      { float f = (float)v / 32767.0f; return f < -1.0f ? -1.0f : (f > 1.0f ? 1.0f : f); }

void rotInit() {
  pinMode(Rotation_INT, INPUT);
  pinMode(Translation_INT, INPUT);
  g_rotLastPollMs = g_trnLastPollMs = millis();
}

// Translation joystick (0x29): same KerbalJoystickCore packet. Read only for the pilot
// override test until translation forwarding is integrated.
static void trnPoll(uint32_t now) {
  bool intLow = (digitalRead(Translation_INT) == LOW);
  if (!intLow && (now - g_trnLastPollMs) < ROT_POLL_MS) return;
  g_trnLastPollMs = now;
  uint8_t pkt[KMC_JOYSTICK_PACKET_SIZE];
  uint8_t got = 0;
  Wire.requestFrom((uint8_t)Translation_MOD, (uint8_t)KMC_JOYSTICK_PACKET_SIZE);
  while (Wire.available() && got < KMC_JOYSTICK_PACKET_SIZE) pkt[got++] = Wire.read();
  if (got < KMC_JOYSTICK_PACKET_SIZE) return;
  if (pkt[KMC_PKT_TYPEID_OFFSET] != KMC_TYPE_JOYSTICK_TRANS) return;
  g_trnX = rotNorm(rotBE(&pkt[6]));
  g_trnY = rotNorm(rotBE(&pkt[8]));
  g_trnZ = rotNorm(rotBE(&pkt[10]));
}

static bool ovrHeld(float mag, uint32_t &since, uint32_t now) {
  if (mag < ROT_OVERRIDE_THRESHOLD) { since = 0; return false; }
  if (since == 0) { since = now; return false; }
  return (now - since) >= ROT_OVERRIDE_MS;
}

bool pilotOverrideDetected(uint8_t &reason) {
  uint32_t now = millis();
  float rot = fmaxf(fabsf(g_rotPitch), fmaxf(fabsf(g_rotYaw), fabsf(g_rotRoll)));
  float trn = fmaxf(fabsf(g_trnX),     fmaxf(fabsf(g_trnY),   fabsf(g_trnZ)));
  bool stick = ovrHeld(rot, g_ovrRotSince, now);
  bool trans = ovrHeld(trn, g_ovrTrnSince, now);
  if (stick || trans) { reason = HP_REASON_STICK; return true; }
  if (thrOverrideLatched() || thrTakeMovedEvent()) { reason = HP_REASON_LEVER; return true; }
  reason = HP_REASON_NONE;
  return false;
}

static void rotPoll(uint32_t now) {
  bool intLow = (digitalRead(Rotation_INT) == LOW);
  if (!intLow && (now - g_rotLastPollMs) < ROT_POLL_MS) return;
  g_rotLastPollMs = now;

  uint8_t pkt[KMC_JOYSTICK_PACKET_SIZE];
  uint8_t got = 0;
  Wire.requestFrom((uint8_t)Rotation_MOD, (uint8_t)KMC_JOYSTICK_PACKET_SIZE);
  while (Wire.available() && got < KMC_JOYSTICK_PACKET_SIZE) pkt[got++] = Wire.read();
  if (got < KMC_JOYSTICK_PACKET_SIZE) return;
  if (pkt[KMC_PKT_TYPEID_OFFSET] != KMC_TYPE_JOYSTICK_ROTATION) return;

  float roll  = rotNorm(rotBE(&pkt[6]));
  float pitch = rotNorm(rotBE(&pkt[8]));
  float yaw   = rotNorm(rotBE(&pkt[10]));
  if (pitch != g_rotPitch || yaw != g_rotYaw || roll != g_rotRoll) g_rotDirty = true;
  g_rotPitch = pitch; g_rotYaw = yaw; g_rotRoll = roll;
}

void rotSetAutoAxes(float pitch, float yaw, float roll, uint8_t heldMask) {
  g_autoPitch = pitch; g_autoYaw = yaw; g_autoRoll = roll;
  g_autoMask = heldMask;
  g_rotDirty = true;
}

void rotClearAutoAxes() {
  if (g_autoMask != 0) g_rotDirty = true;
  g_autoMask = 0;
  g_autoPitch = g_autoYaw = g_autoRoll = 0.0f;
}

float rotPilotPitch() { return g_rotPitch; }
float rotPilotYaw()   { return g_rotYaw; }
float rotPilotRoll()  { return g_rotRoll; }

void rotService() {
  uint32_t now = millis();
  rotPoll(now);
  trnPoll(now);

  if (apIsArmed()) return;                              // ascent autopilot owns the channel
  if (!g_rotDirty || (now - g_rotLastSendMs) < ROT_SEND_MS) return;

  float scale = precisionEn ? ROT_PRECISION : 1.0f;
  float p = (g_autoMask & ROT_AXIS_PITCH) ? g_autoPitch : g_rotPitch * scale;
  float y = (g_autoMask & ROT_AXIS_YAW)   ? g_autoYaw   : g_rotYaw   * scale;
  float r = (g_autoMask & ROT_AXIS_ROLL)  ? g_autoRoll  : g_rotRoll  * scale;
  int16_t ip = (int16_t)(p * (float)INT16_MAX);
  int16_t iy = (int16_t)(y * (float)INT16_MAX);
  int16_t ir = (int16_t)(r * (float)INT16_MAX);

  // Resend only when something moved by more than the noise, or the held set changed.
  if (g_autoMask == g_lastSentMask &&
      abs(ip - g_lastSentP) < 160 && abs(iy - g_lastSentY) < 160 && abs(ir - g_lastSentR) < 160) {
    g_rotDirty = false;
    return;
  }
  rotationMessage msg;
  msg.setPitch(ip); msg.setYaw(iy); msg.setRoll(ir);
  mySimpit.send(ROTATION_MESSAGE, msg);
  g_lastSentP = ip; g_lastSentY = iy; g_lastSentR = ir; g_lastSentMask = g_autoMask;
  g_rotLastSendMs = now;
  g_rotDirty = false;
}

/***************************************************************************************
   SIMPIT REGISTER INPUT CHANNELS
   Contains simpit register channel objections for all the items that are required by
   this microcontroller
   - No inputs
   - No outputs
****************************************************************************************/
void registerInputChannels() {  //Game message that registers the necessary messages with the Simput Plugin
  mySimpit.registerChannel(ACTIONSTATUS_MESSAGE);
  mySimpit.registerChannel(CAGSTATUS_MESSAGE);
  mySimpit.registerChannel(SAS_MODE_INFO_MESSAGE);
  mySimpit.registerChannel(FLIGHT_STATUS_MESSAGE);
  mySimpit.registerChannel(ALTITUDE_MESSAGE);
  mySimpit.registerChannel(VELOCITY_MESSAGE);
  mySimpit.registerChannel(AIRSPEED_MESSAGE);
  mySimpit.registerChannel(APSIDES_MESSAGE);
  mySimpit.registerChannel(APSIDESTIME_MESSAGE);  // Time-to-apoapsis for circularization timing (ascent autopilot)
  mySimpit.registerChannel(ORBIT_MESSAGE);        // Inclination for the ascent autopilot azimuth target
  mySimpit.registerChannel(ROTATION_DATA_MESSAGE);// Attitude + surface prograde for the ascent autopilot steering loop
  mySimpit.registerChannel(TARGETINFO_MESSAGE);   // Target bearing for the rover drive-to-target hold (hold autopilot)
  mySimpit.registerChannel(MANEUVER_MESSAGE);     // Node time / dV / pointing for the burn autopilot
  mySimpit.registerChannel(DELTAV_MESSAGE);
  mySimpit.registerChannel(BURNTIME_MESSAGE);
  mySimpit.registerChannel(TEMP_LIMIT_MESSAGE);
  mySimpit.registerChannel(SOI_MESSAGE);
  mySimpit.registerChannel(THROTTLE_CMD_MESSAGE);
  mySimpit.registerChannel(ATMO_CONDITIONS_MESSAGE);
  mySimpit.registerChannel(VESSEL_NAME_MESSAGE);
  mySimpit.registerChannel(ELECTRIC_MESSAGE);
  mySimpit.registerChannel(EVA_MESSAGE);
  mySimpit.registerChannel(SCENE_CHANGE_MESSAGE);
  mySimpit.registerChannel(VESSEL_CHANGE_MESSAGE);
}


/***************************************************************************************
   SIMPIT MESSAGE HANDLER
   Established the simpit message handler for the main microncontroller
   - No inputs
   - No outputs
****************************************************************************************/
void messageHandler(byte messageType, byte msg[], byte msgSize) {
  switch (messageType) {
    case ACTIONSTATUS_MESSAGE:
      if (msgSize == 1) {
        gear_on = msg[0] & GEAR_ACTION;
        brakes_on = msg[0] & BRAKES_ACTION;
        lights_on = msg[0] & LIGHT_ACTION;
        RCS_on = msg[0] & RCS_ACTION;
        SAS_on = msg[0] & SAS_ACTION;
        hpIngestBrakes(brakes_on);  // brakes drop rover cruise (hold autopilot)
      }
      break;
    case CAGSTATUS_MESSAGE:
      if (msgSize == sizeof(cagStatusMessage)) {
        cagStatusMessage cagStatus;
        cagStatus = parseCAGStatusMessage(msg);
        action1_on = cagStatus.is_action_activated(ag1);
        action2_on = cagStatus.is_action_activated(ag2);
        action3_on = cagStatus.is_action_activated(ag3);
        action4_on = cagStatus.is_action_activated(ag4);
        action5_on = cagStatus.is_action_activated(ag5);
        action6_on = cagStatus.is_action_activated(ag6);
        action7_on = cagStatus.is_action_activated(ag7);
        action8_on = cagStatus.is_action_activated(ag8);
        action9_on = cagStatus.is_action_activated(ag9);
        action10_on = cagStatus.is_action_activated(ag10);
        solarArray_on = cagStatus.is_action_activated(solar_array);
        antenna_on = cagStatus.is_action_activated(antenna);
        cargoDoor_on = cagStatus.is_action_activated(cargo_door);
        radiator_on = cagStatus.is_action_activated(radiator);
        drogue_on = cagStatus.is_action_activated(drogue);
        parachute_on = cagStatus.is_action_activated(parachute);
        ladder_on = cagStatus.is_action_activated(ladder);
      }
      break;
    case SAS_MODE_INFO_MESSAGE:
      if (msgSize == sizeof(SASInfoMessage)) {
        SASInfoMessage sasMode;
        sasMode = parseMessage<SASInfoMessage>(msg);
        SAS_mode = sasMode.currentSASMode;
        break;
      }
    case FLIGHT_STATUS_MESSAGE:
      if (msgSize == sizeof(flightStatusMessage)) {
        flightStatusMessage myFlightStatus;
        myFlightStatus = parseMessage<flightStatusMessage>(msg);
        inFlight = myFlightStatus.isInFlight();
        apIngestFlightStatus(myFlightStatus.isInFlight());  // feed ascent autopilot
        inEVA = myFlightStatus.isInEVA();
        physTW = myFlightStatus.isInAtmoTW();
        hasTarget = myFlightStatus.hasTarget();
        stage = myFlightStatus.currentStage;
        crewCount = myFlightStatus.crewCount;
        crewCapacity = myFlightStatus.crewCapacity;
        commNet = myFlightStatus.commNetSignalStrenghPercentage;
        twIndex = myFlightStatus.currentTWIndex;
        vesselType = myFlightStatus.vesselType;
        vesselSituation = myFlightStatus.vesselSituation;
        isRecoverable = myFlightStatus.isRecoverable();
        vesselCtrlLvl = myFlightStatus.getControlLevel();
        hpIngestFlightStatus(myFlightStatus.vesselType, myFlightStatus.vesselSituation, myFlightStatus.hasTarget());  // feed hold autopilot
        lpIngestFlightStatus(myFlightStatus.vesselType, myFlightStatus.vesselSituation);  // feed landing autopilot
      }
      break;
    case SCENE_CHANGE_MESSAGE:
      flightScene = !msg[0];
      if (flightScene) {
        if (debug) { Serial.println("***In the flight scene***"); }
        tftDispMode = 1;
      } else {
        if (debug) { Serial.println("***Leaving the fight scene***"); }
        //tftDispMode = 0;
        resetDisplays();
        noTone(AUDIO_PIN);
        hpVesselChanged();   // leaving the flight scene drops every hold mode, silently
        bpVesselChanged(); lpVesselChanged();
      }
      break;
    case VESSEL_CHANGE_MESSAGE:
      if (msg[0] == 1) {
        resetDisplays();
        hpVesselChanged();   // a new vessel starts with no hold modes
        bpVesselChanged(); lpVesselChanged();
        if (debug) { Serial.println("*******Vessel Change Message*******"); }
      } else if (msg[0] == 2) {
        if (debug) { Serial.println("*******Craft has docked*******"); }
        docked = true;
        dockUpdate = true;
      } else if (msg[0] == 3) {
        if (debug) { Serial.println("*******Craft has un-docked*******"); }
        docked = false;
        dockUpdate = true;
      }
      break;
    case THROTTLE_CMD_MESSAGE:
      if (msgSize == sizeof(throttleMessage)) {
        throttleMessage myThrottle;
        myThrottle = parseMessage<throttleMessage>(msg);
        gameThrottleCmd = map(myThrottle.throttle, 0, INT16_MAX, 0, 1023);
        hpIngestThrottle((float)myThrottle.throttle / (float)INT16_MAX);  // autothrottle bumpless engage
        lpIngestThrottle((float)myThrottle.throttle / (float)INT16_MAX);
        Serial.println("********THROTTLE CMD MESSAGE RECEIVED********");
        Serial.print("myThrottle.throttle = ");
        Serial.println(myThrottle.throttle);
        Serial.print("gameThrottleCmd = ");
        Serial.println(gameThrottleCmd);
      }
      break;
    case ALTITUDE_MESSAGE:
      if (msgSize == sizeof(altitudeMessage)) {
        altitudeMessage myAltitude;
        myAltitude = parseMessage<altitudeMessage>(msg);
        alt_sl = myAltitude.sealevel;
        alt_surf = myAltitude.surface;
        apIngestAltitude(myAltitude.sealevel, myAltitude.surface);  // feed ascent autopilot
        hpIngestAltitude(myAltitude.sealevel);                      // feed hold autopilot (ALT hold)
        lpIngestAltitude(myAltitude.sealevel, myAltitude.surface);   // feed landing autopilot (radar altitude)
      }
      break;
    case VELOCITY_MESSAGE:
      if (msgSize == sizeof(velocityMessage)) {
        velocityMessage myVelocity;
        myVelocity = parseMessage<velocityMessage>(msg);
        vel_orb = myVelocity.orbital;
        vel_surf = myVelocity.surface;
        vel_vert = myVelocity.vertical;
        apIngestVelocity(myVelocity.orbital, myVelocity.surface, myVelocity.vertical);  // feed ascent autopilot
        hpIngestVelocity(myVelocity.surface, myVelocity.vertical);                     // feed hold autopilot (V/S, CRUISE)
        lpIngestVelocity(myVelocity.surface, myVelocity.vertical);                     // feed landing autopilot
        bpIngestVelocity(myVelocity.orbital);                                          // feed burn autopilot
      }
      break;
    case AIRSPEED_MESSAGE:
      if (msgSize == sizeof(airspeedMessage)) {
        airspeedMessage myAirspeed;
        myAirspeed = parseMessage<airspeedMessage>(msg);
        gForces = myAirspeed.gForces;
        apIngestGForce(myAirspeed.gForces);  // feed ascent autopilot max-G limiter
        hpIngestAirspeed(myAirspeed.IAS, myAirspeed.mach);  // feed hold autopilot (IAS / MACH)
        lpIngestAirspeed(myAirspeed.mach);                  // feed landing autopilot (plane hand-off)
        aeIngestGForce(myAirspeed.gForces);                 // acceleration estimate (measured during a burn)
      }
      break;
    case APSIDES_MESSAGE:
      if (msgSize == sizeof(apsidesMessage)) {
        apsidesMessage myApsides;
        myApsides = parseMessage<apsidesMessage>(msg);
        periapsis = myApsides.periapsis;
        apoapsis = myApsides.apoapsis;
        apIngestApsides(myApsides.apoapsis, myApsides.periapsis);  // feed ascent autopilot
        bpIngestApsides(myApsides.apoapsis, myApsides.periapsis);  // feed burn autopilot
      }
      break;
    case APSIDESTIME_MESSAGE:
      if (msgSize == sizeof(apsidesTimeMessage)) {
        apsidesTimeMessage myApsidesTime;
        myApsidesTime = parseMessage<apsidesTimeMessage>(msg);
        apIngestApsidesTime((float)myApsidesTime.apoapsis, (float)myApsidesTime.periapsis);  // feed ascent autopilot
        bpIngestApsidesTime((float)myApsidesTime.apoapsis, (float)myApsidesTime.periapsis);  // feed burn autopilot
      }
      break;
    case ORBIT_MESSAGE:
      if (msgSize == sizeof(orbitInfoMessage)) {
        orbitInfoMessage myOrbit;
        myOrbit = parseMessage<orbitInfoMessage>(msg);
        apIngestOrbit(myOrbit.inclination);  // feed ascent autopilot azimuth target
        bpIngestOrbit(myOrbit.eccentricity, myOrbit.semiMajorAxis, myOrbit.inclination, myOrbit.longAscendingNode,
                      myOrbit.argPeriapsis, myOrbit.trueAnomaly, myOrbit.period);  // feed burn autopilot planners
      }
      break;
    case ROTATION_DATA_MESSAGE:
      if (msgSize == sizeof(vesselPointingMessage)) {
        vesselPointingMessage myPointing;
        myPointing = parseMessage<vesselPointingMessage>(msg);
        apIngestAttitude(myPointing.heading, myPointing.pitch, myPointing.roll,
                         myPointing.surfaceVelocityHeading, myPointing.surfaceVelocityPitch,
                         myPointing.orbitalVelocityHeading, myPointing.orbitalVelocityPitch);  // feed ascent autopilot steering
        hpIngestAttitude(myPointing.heading, myPointing.pitch, myPointing.roll,
                         myPointing.surfaceVelocityHeading, myPointing.surfaceVelocityPitch);  // feed hold autopilot
        bpIngestAttitude(myPointing.heading, myPointing.pitch, myPointing.roll,
                         myPointing.orbitalVelocityHeading, myPointing.orbitalVelocityPitch);  // feed burn autopilot
        lpIngestAttitude(myPointing.heading, myPointing.pitch, myPointing.roll,
                         myPointing.orbitalVelocityHeading, myPointing.orbitalVelocityPitch);  // feed landing autopilot (ENTRY)
      }
      break;
    case DELTAV_MESSAGE:
      if (msgSize == sizeof(deltaVMessage)) {
        deltaVMessage myDV;
        myDV = parseMessage<deltaVMessage>(msg);
        stageDV = myDV.stageDeltaV;
        totalDV = myDV.totalDeltaV;
        apIngestStageDeltaV(myDV.stageDeltaV);  // feed ascent autopilot auto-staging
        aeIngestStage(myDV.stageDeltaV, stageBurnTime);  // acceleration estimate + shared auto-stage
      }
      break;
    case BURNTIME_MESSAGE:
      if (msgSize == sizeof(burnTimeMessage)) {
        burnTimeMessage myBurn;
        myBurn = parseMessage<burnTimeMessage>(msg);
        stageBurnTime = myBurn.stageBurnTime;
        totalBurnTime = myBurn.totalBurnTime;
        aeIngestStage(stageDV, myBurn.stageBurnTime);
      }
      break;
    case TEMP_LIMIT_MESSAGE:
      if (msgSize == sizeof(tempLimitMessage)) {
        tempLimitMessage myTemp;
        myTemp = parseMessage<tempLimitMessage>(msg);
        maxTemp = myTemp.tempLimitPercentage;
        skinTemp = myTemp.skinTempLimitPercentage;
        apIngestSkinTemp(myTemp.skinTempLimitPercentage / 100.0f);  // feed ascent autopilot heat limiter
      }
      break;
    case SOI_MESSAGE: {
      strSOI = "";
      char bodyBuf[24];
      uint8_t n = 0;
      for (uint8_t i = 0; i < msgSize; i++) {
        strSOI += char(msg[i]);
        if (n < sizeof(bodyBuf) - 1) bodyBuf[n++] = char(msg[i]);
      }
      bodyBuf[n] = '\0';
      apIngestSOI(bodyBuf);  // feed ascent autopilot body-profile / SoI adaptation
      {
        BodyParams b = getBodyParams(bodyBuf);   // shared celestial-body table
        bpIngestBody(b.radius, b.gravity, bodyBuf);        // mu for the planners; SOI change aborts a burn
        lpIngestBody(b.gravity, b.flyHigh, bodyBuf);       // ignition altitude; plane hand-off ceiling
      }
      break;
    }
    case ATMO_CONDITIONS_MESSAGE:
      if (msgSize == sizeof(atmoConditionsMessage)) {
        atmoConditionsMessage myAtmoConditions;
        myAtmoConditions = parseMessage<atmoConditionsMessage>(msg);
        airDensity = myAtmoConditions.airDensity;
        airTemp = myAtmoConditions.temperature;
        airPressure = myAtmoConditions.pressure;
        hasAtmo = myAtmoConditions.hasAtmosphere();
        hasO2 = myAtmoConditions.hasOxygen();
        inAtmo = myAtmoConditions.isVesselInAtmosphere();
        apIngestAtmo(myAtmoConditions.airDensity, myAtmoConditions.hasAtmosphere(),
                     myAtmoConditions.isVesselInAtmosphere());  // feed ascent autopilot (max-Q + airless/atmospheric branch)
        hpIngestAtmo(myAtmoConditions.hasAtmosphere(), myAtmoConditions.isVesselInAtmosphere());  // feed hold autopilot
        lpIngestAtmo(myAtmoConditions.airDensity, myAtmoConditions.isVesselInAtmosphere());      // feed landing autopilot
        aeIngestAtmo(myAtmoConditions.isVesselInAtmosphere());                                    // measured accel only in vacuum
      }
      break;
    case TARGETINFO_MESSAGE:
      if (msgSize == sizeof(targetMessage)) {
        targetMessage myTarget;
        myTarget = parseMessage<targetMessage>(msg);
        // Simpit's target velocity is the unsigned magnitude of the whole relative velocity.
        // Project it on the line of sight for a signed closing rate (> 0 = opening).
        float losN, losE, losU, vN, vE, vU;
        {
          float p = myTarget.pitch * 0.0174532925199f, h = myTarget.heading * 0.0174532925199f;
          losN = cosf(p) * cosf(h); losE = cosf(p) * sinf(h); losU = sinf(p);
          p = myTarget.velocityPitch * 0.0174532925199f; h = myTarget.velocityHeading * 0.0174532925199f;
          vN = cosf(p) * cosf(h); vE = cosf(p) * sinf(h); vU = sinf(p);
        }
        float closing = myTarget.velocity * (losN * vN + losE * vE + losU * vU);
        hpIngestTarget(hasTarget, myTarget.heading, myTarget.pitch, myTarget.distance, closing);   // NAV / GS / TGT / FOLLOW
        bpIngestTarget(hasTarget, myTarget.distance, myTarget.velocity, myTarget.heading, myTarget.pitch,
                       myTarget.velocityHeading, myTarget.velocityPitch);                          // approach-rate hold
      }
      break;
    case MANEUVER_MESSAGE:
      if (msgSize == sizeof(maneuverMessage)) {
        maneuverMessage myNode;
        myNode = parseMessage<maneuverMessage>(msg);
        bpIngestNode(myNode.timeToNextManeuver, myNode.deltaVNextManeuver, myNode.durationNextManeuver,
                     myNode.headingNextManeuver, myNode.pitchNextManeuver);   // node executor
      }
      break;
    case VESSEL_NAME_MESSAGE:
      gameVesselName = "";
      for (uint8_t i = 0; i < msgSize; i++) {
        gameVesselName += char(msg[i]);
      }
      break;
    case ELECTRIC_MESSAGE:
      if (msgSize == sizeof(resourceMessage)) {
        resourceMessage myEC;
        myEC = parseResource(msg);
        EC = myEC.available;
        EC_total = myEC.total;
      }
      break;
    case EVA_MESSAGE:
      if (msgSize == sizeof(resourceMessage)) {
        resourceMessage myEVA;
        myEVA = parseResource(msg);
        eva_fuel = myEVA.available;
        eva_fuel_total = myEVA.total;
      }
      break;
  }
}


/***************************************************************************************
   Initialize SIMPIT OBJECT
   Executes the simpit object, registers input channels and establishes the Simpit
   message handler
   - No inputs
   - No outputs
****************************************************************************************/
void initSimpitObject() {
  mySimpit.init();                                              //Init Simpit Object
  mySimpit.printToKSP("Connected Master MC", PRINT_TO_SCREEN);  // Display a message in KSP to indicate handshaking is complete.
  mySimpit.inboundHandler(messageHandler);                      // Sets our callback function. The KerbalSimpit library will call this function every time a packet is received
  registerInputChannels();                                      // Register the required channels with Simpit

  tft.println("  Kerbal Simpit Object Initialized");
}

/***************************************************************************************
   stage_helpers.ino — auto-stage and the acceleration estimate.

   Auto-stage: fire STAGE_ACTION when the current throttle owner is above 10 % throttle,
   the stage's delta-V is spent, and the owner still has delta-V to deliver; 2 s lockout.

   Acceleration estimate (Mission_Autopilot.md §2): Simpit sends neither thrust nor mass.
     EST  — stage delta-V / stage burn time, the stage average, available before any burn.
     MEAS — felt g during a vacuum burn is the thrust acceleration; after 2 s of steady
            throttle above 50 % the measurement (scaled to full throttle) replaces EST
            for the rest of the stage. A staging event (stage delta-V jumping up) resets it.
     TWR  — a pilot-entered thrust-to-weight override bypasses both.
****************************************************************************************/
#include "control_links.h"

static bool     as_enabled     = true;
static uint32_t as_lastStageMs = 0;
static const uint32_t AS_LOCKOUT_MS   = 2000;
static const float    AS_STAGE_DV_MIN = 5.0f;

static float    ae_stageDv = 0.0f, ae_burnTime = 0.0f, ae_prevStageDv = 0.0f;
static float    ae_gForce = 0.0f;
static bool     ae_inAtmo = true;
static float    ae_throttle = 0.0f;
static uint32_t ae_steadySince = 0;
static float    ae_meas = 0.0f;              // m/s² at full throttle, 0 = none
static float    ae_twr = 0.0f, ae_g0 = 9.81f;
static const float AE_MEAS_MIN_THROTTLE = 0.5f;
static const uint32_t AE_MEAS_SETTLE_MS = 2000;
static const float AE_MEAS_FILTER = 0.2f;

void asSetEnabled(bool on) { as_enabled = on; }
bool asEnabled()           { return as_enabled; }

void asMaybeStage(bool enabled, float throttle, float remainingDv) {
  if (!enabled || throttle < 0.10f) return;
  if (ae_stageDv > AS_STAGE_DV_MIN || remainingDv < AS_STAGE_DV_MIN) return;
  uint32_t now = millis();
  if (now - as_lastStageMs < AS_LOCKOUT_MS) return;
  mySimpit.activateAction(STAGE_ACTION);
  as_lastStageMs = now;
}

void aeIngestStage(float stageDv, float stageBurnTime) {
  if (stageDv > ae_prevStageDv + 50.0f) ae_meas = 0.0f;   // new stage: the old measurement is void
  ae_prevStageDv = ae_stageDv;
  ae_stageDv = stageDv; ae_burnTime = stageBurnTime;
}
void aeIngestGForce(float gForce)     { ae_gForce = gForce; }
void aeIngestAtmo(bool inAtmosphere)  { ae_inAtmo = inAtmosphere; }

void aeNoteThrottle(float commanded) {
  ae_throttle = commanded;
  uint32_t now = millis();
  if (commanded < AE_MEAS_MIN_THROTTLE || ae_inAtmo) { ae_steadySince = 0; return; }
  if (ae_steadySince == 0) { ae_steadySince = now; return; }
  if (now - ae_steadySince < AE_MEAS_SETTLE_MS) return;
  float a = (ae_gForce * 9.81f) / commanded;       // felt g under thrust in vacuum, scaled to full throttle
  if (a < 0.5f) return;
  ae_meas = (ae_meas <= 0.0f) ? a : ae_meas + AE_MEAS_FILTER * (a - ae_meas);
}

void aeSetTwrOverride(float twr, float g0) { ae_twr = twr; if (g0 > 0.0f) ae_g0 = g0; }

uint8_t aeSource() {
  if (ae_twr > 0.0f) return AE_SRC_TWR;
  if (ae_meas > 0.0f) return AE_SRC_MEAS;
  return AE_SRC_EST;
}

float aeAccel() {
  switch (aeSource()) {
    case AE_SRC_TWR:  return ae_twr * ae_g0;
    case AE_SRC_MEAS: return ae_meas;
    default:          return (ae_burnTime > 0.5f) ? ae_stageDv / ae_burnTime : 0.0f;
  }
}

float aeBurnDuration(float dv) { float a = aeAccel(); return (a > 0.0f) ? fabsf(dv) / a : 0.0f; }
float aeStageDv()              { return ae_stageDv; }

/***************************************************************************************
   throttle_link.ino — Throttle Module (0x2C) link for the master.

   Reads the module's 7-byte packet (I2C Protocol Specification §9.4):
     Byte 0-2 : universal header (status, type ID, tx counter)
     Byte 3   : flags   bit0 enabled, bit1 precision, bit2 pilot touching, bit3 motor moving
     Byte 4   : buttons bit0 100%, bit1 UP, bit2 DOWN, bit3 0%  (rising-edge events)
     Byte 5-6 : throttle value, uint16 big-endian, 0..INT16_MAX

   Forwards the wiper to KSP as the pilot's throttle unless an autopilot owns the
   throttle, in which case the owner's value goes to KSP AND to the motorised lever
   (CMD_SET_THROTTLE) so the physical lever tracks the commanded throttle. The pilot
   grabbing the lever (touch flag or any button) latches an override: the owner's
   commands are dropped, the wiper is forwarded again, and a one-shot event tells the
   owner to annunciate. Design: Hold_Mode_Autopilot.md §7.

   Lever drive is deadbanded (1 %) and rate-limited (200 ms) so the H-bridge does not
   chatter at the setpoint; the module itself refuses CMD_SET_THROTTLE while touched.
****************************************************************************************/
#include "control_links.h"

static const uint32_t THR_POLL_MS        = 50;     // fallback poll when INT is not seen
static const uint32_t THR_LEVER_MIN_MS   = 200;    // min interval between lever commands
static const float    THR_LEVER_DEADBAND = 0.01f;  // lever command deadband (fraction)
static const float    THR_KSP_DEADBAND   = 0.002f; // KSP throttle resend deadband
static const float    THR_SYNC_WINDOW    = 0.03f;  // wiper must pass within this of the held value

static const uint8_t THR_FLAG_ENABLED   = 0x01;
static const uint8_t THR_FLAG_PRECISION = 0x02;
static const uint8_t THR_FLAG_TOUCH     = 0x04;
static const uint8_t THR_FLAG_MOVING    = 0x08;

static uint8_t  g_thrOwner        = THR_OWNER_NONE;
static bool     g_thrOverride     = false;   // pilot holds the lever against the owner
static bool     g_thrOverrideEvt  = false;   // one-shot for the owner
static bool     g_thrMovedEvt     = false;   // one-shot: pilot moved the lever with no owner driving it
static float    g_thrMovedRef     = -1.0f;   // wiper position the movement test is measured from
static const float THR_MOVED_DEADBAND = 0.02f;
static bool     g_thrSyncLatch    = false;   // lever not driven: hold last auto value until the wiper passes it
static float    g_thrSyncValue    = 0.0f;

static uint8_t  g_thrFlags        = 0;
static bool     g_thrPrevTouch    = false;
static float    g_thrLever        = 0.0f;    // wiper 0..1
static float    g_thrLastKsp      = -1.0f;   // last value sent to KSP
static float    g_thrLastLeverCmd = -1.0f;   // last CMD_SET_THROTTLE value
static uint32_t g_thrLastLeverMs  = 0;
static uint32_t g_thrLastPollMs   = 0;
static bool     g_thrEnabledSent  = false;
static bool     g_thrPrecisionCmd = false;

static void thrSendKsp(float t) {
  t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
  if (g_thrLastKsp >= 0.0f && fabsf(t - g_thrLastKsp) < THR_KSP_DEADBAND) return;
  throttleMessage msg;
  msg.throttle = (int16_t)(t * (float)INT16_MAX);
  mySimpit.send(THROTTLE_MESSAGE, msg);
  g_thrLastKsp = t;
}

static void thrSendCommand(uint8_t cmd, const uint8_t *payload, uint8_t n) {
  Wire.beginTransmission(Throttle_MOD);
  Wire.write(cmd);
  for (uint8_t i = 0; i < n; i++) Wire.write(payload[i]);
  Wire.endTransmission();
}

static void thrDriveLever(float t, uint32_t now) {
  if (!thrLeverDriven() || (g_thrFlags & THR_FLAG_TOUCH)) return;
  if (g_thrLastLeverCmd >= 0.0f && fabsf(t - g_thrLastLeverCmd) < THR_LEVER_DEADBAND) return;
  if (now - g_thrLastLeverMs < THR_LEVER_MIN_MS) return;
  uint16_t v = (uint16_t)(t * (float)INT16_MAX);
  uint8_t p[2] = { (uint8_t)(v >> 8), (uint8_t)(v & 0xFF) };
  thrSendCommand(KMC_CMD_SET_THROTTLE, p, 2);
  g_thrLastLeverCmd = t;
  g_thrLastLeverMs  = now;
}

static void thrLatchOverride() {
  if (g_thrOwner == THR_OWNER_NONE || g_thrOverride) return;
  g_thrOverride    = true;
  g_thrOverrideEvt = true;
  // The lever was not following (module disabled / precision): hold the last
  // autopilot value until the wiper passes through it, then hand over — the classic
  // throttle-sync latch. When the lever WAS following there is nothing to sync.
  if (!thrLeverDriven() && g_thrLastKsp >= 0.0f) { g_thrSyncLatch = true; g_thrSyncValue = g_thrLastKsp; }
}

void thrInit() {
  pinMode(Throttle_INT, INPUT);
  g_thrLastPollMs = millis();
}

void thrSetPrecision(bool fine) {
  g_thrPrecisionCmd = fine;
  uint8_t p = fine ? 1 : 0;
  thrSendCommand(KMC_CMD_SET_PRECISION, &p, 1);
}

static void thrPoll(uint32_t now) {
  bool intLow = (digitalRead(Throttle_INT) == LOW);
  if (!intLow && (now - g_thrLastPollMs) < THR_POLL_MS) return;
  g_thrLastPollMs = now;

  uint8_t pkt[KMC_THROTTLE_PACKET_SIZE];
  uint8_t got = 0;
  Wire.requestFrom((uint8_t)Throttle_MOD, (uint8_t)KMC_THROTTLE_PACKET_SIZE);
  while (Wire.available() && got < KMC_THROTTLE_PACKET_SIZE) pkt[got++] = Wire.read();
  if (got < KMC_THROTTLE_PACKET_SIZE) return;
  if (pkt[KMC_PKT_TYPEID_OFFSET] != KMC_TYPE_THROTTLE) return;

  g_thrFlags = pkt[3];
  uint8_t buttons = pkt[4];
  uint16_t raw = ((uint16_t)pkt[5] << 8) | pkt[6];
  g_thrLever = (float)raw / (float)INT16_MAX;
  if (g_thrLever > 1.0f) g_thrLever = 1.0f;

  bool touch = (g_thrFlags & THR_FLAG_TOUCH) != 0;
  if ((touch && !g_thrPrevTouch) || buttons != 0) thrLatchOverride();
  g_thrPrevTouch = touch;

  // Lever MOVEMENT is pilot input even when no owner drives the lever (an attitude-only
  // hold, a burn aligning). A resting hand is not: the touch flag alone does not count
  // here, only a change of more than the deadband, or a lever button.
  if (g_thrOwner == THR_OWNER_NONE || !thrLeverDriven()) {
    if (g_thrMovedRef < 0.0f) g_thrMovedRef = g_thrLever;
    if (fabsf(g_thrLever - g_thrMovedRef) > THR_MOVED_DEADBAND || buttons != 0) {
      g_thrMovedEvt = true;
      g_thrMovedRef = g_thrLever;
    }
  } else {
    g_thrMovedRef = g_thrLever;    // lever is following the owner: its motion is not the pilot's
  }
}

void thrService() {
  uint32_t now = millis();

  // Module enable follows the master's throttle-enable state.
  if (!g_thrEnabledSent || throttleEn != ((g_thrFlags & THR_FLAG_ENABLED) != 0)) {
    thrSendCommand(throttleEn ? KMC_CMD_ENABLE : KMC_CMD_DISABLE, nullptr, 0);
    g_thrEnabledSent = true;
  }

  thrPoll(now);

  bool pilotHasIt = (g_thrOwner == THR_OWNER_NONE) || g_thrOverride;
  if (!pilotHasIt) return;                       // owner drives KSP via thrAutoThrottle()

  if (g_thrSyncLatch) {
    if (fabsf(g_thrLever - g_thrSyncValue) <= THR_SYNC_WINDOW) g_thrSyncLatch = false;
    else return;                                 // KSP keeps the held value until the lever catches up
  }
  if (throttleEn) thrSendKsp(g_thrLever);
}

void thrAutoThrottle(uint8_t owner, float t) {
  if (owner == THR_OWNER_NONE) return;
  if (owner != g_thrOwner) {                     // new owner takes the throttle
    g_thrOwner = owner;
    g_thrOverride = false; g_thrOverrideEvt = false; g_thrSyncLatch = false;
    if (g_thrFlags & THR_FLAG_TOUCH) thrLatchOverride();   // pilot already has the lever
  }
  if (g_thrOverride) return;
  thrSendKsp(t);
  thrDriveLever(t, millis());
}

void thrAutoRelease(uint8_t owner) {
  if (owner != g_thrOwner) return;
  g_thrOwner = THR_OWNER_NONE;
  g_thrOverride = false; g_thrOverrideEvt = false;
  // Lever was following: the wiper already equals the last command, so the pilot's
  // throttle is continuous. Lever not following: sync-latch on the last value.
  if (!thrLeverDriven() && g_thrLastKsp >= 0.0f && fabsf(g_thrLever - g_thrLastKsp) > THR_SYNC_WINDOW) {
    g_thrSyncLatch = true; g_thrSyncValue = g_thrLastKsp;
  }
}

bool    thrTakeOverrideEvent() { bool e = g_thrOverrideEvt; g_thrOverrideEvt = false; return e; }
bool    thrTakeMovedEvent()    { bool e = g_thrMovedEvt;    g_thrMovedEvt    = false; return e; }
bool    thrTouched()           { return (g_thrFlags & THR_FLAG_TOUCH) != 0; }
bool    thrPrecision()         { return g_thrPrecisionCmd || (g_thrFlags & THR_FLAG_PRECISION) != 0; }
bool    thrLeverDriven()       { return throttleEn && (g_thrFlags & THR_FLAG_ENABLED) && !thrPrecision(); }
bool    thrOverrideLatched()   { return g_thrOverride; }
float   thrCurrentThrottle()   { return (g_thrOwner != THR_OWNER_NONE && !g_thrOverride && g_thrLastKsp >= 0.0f) ? g_thrLastKsp : g_thrLever; }
uint8_t thrOwner()             { return g_thrOwner; }
