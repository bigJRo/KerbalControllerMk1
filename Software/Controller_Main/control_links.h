/********************************************************************************************************************************
  Control Links — Kerbal Controller Mk1 (Master Teensy 4.1)

  Prototypes for the three master-side link tabs that sit between the autopilots and the hardware:
    throttle_link.ino  — Throttle Module (0x2C): motorised lever follow, pilot-touch override, KSP throttle forwarding
    rotation_link.ino  — Rotation joystick (0x28): pilot axis forwarding merged with autopilot-held axes
    infodisp_link.ino  — Info Display 2 (0x13): autopilot console command poll / ACK / status push

  They are declared here (rather than left to the Arduino auto-prototype pass) so the autopilot tabs and the bench stub build
  see one contract. Design: Documents/Developer/Hold_Mode_Autopilot.md §6.6, §7, §8.

  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Written for Jeb's Controller Works.
********************************************************************************************************************************/
#ifndef CONTROL_LINKS_H
#define CONTROL_LINKS_H

#include <Arduino.h>

/***************************************************************************************
   throttle_link.ino
   One throttle "owner" at a time may command the game throttle and the lever: the
   ascent autopilot's throttle manager or the hold autopilot's autothrottle. The pilot
   grabbing the lever (touch flag, or any lever button) latches an override: the owner's
   commands are dropped and the wiper is forwarded as ordinary pilot throttle until the
   owner releases. The override raises a one-shot event the owner can read to annunciate.
****************************************************************************************/
// KSP enum values carried by FLIGHT_STATUS_MESSAGE (Vessel.Situations / VesselType), one
// definition for every tab of the sketch.
static const uint8_t KSP_SIT_LANDED   = 1;
static const uint8_t KSP_SIT_SPLASHED = 2;
static const uint8_t KSP_TYPE_ROVER   = 5;
static const uint8_t KSP_TYPE_PLANE   = 8;

enum ThrOwner : uint8_t { THR_OWNER_NONE = 0, THR_OWNER_ASCENT, THR_OWNER_HOLD, THR_OWNER_BURN, THR_OWNER_LANDING };

void    thrInit();
void    thrService();                               // poll the module, forward pilot throttle, drive the lever
void    thrAutoThrottle(uint8_t owner, float t);    // owner commands throttle t (0..1) to KSP + lever
void    thrAutoRelease(uint8_t owner);              // owner hands the throttle back to the pilot
bool    thrTakeOverrideEvent();                     // true once after the pilot takes the lever from an owner
bool    thrTouched();                               // pilot's hand is on the slider (module touch flag)
bool    thrPrecision();                             // module is in precision (fine) mode
bool    thrLeverDriven();                           // module enabled and following commands
bool    thrOverrideLatched();                       // pilot holds the lever against the current owner
bool    thrTakeMovedEvent();                        // true once after the pilot MOVES the lever (>2 %) while no owner drives it
float   thrCurrentThrottle();                       // lever position 0..1 (what KSP is being sent)
uint8_t thrOwner();
void    thrSetPrecision(bool fine);                 // CMD_SET_PRECISION to the module (STD / FINE switch)

/***************************************************************************************
   rotation_link.ino
   The hold autopilot hands its held axes here and the link sends ONE rotation message
   per frame: autopilot axes where held, pilot axes elsewhere. The Simpit plugin keeps
   only the latest rotation message, so two senders with partial masks would clobber
   each other — merging is the only safe way to share the channel. The ascent autopilot
   sends its own rotation while armed; the link stays silent then.
****************************************************************************************/
enum { ROT_AXIS_PITCH = 0x01, ROT_AXIS_YAW = 0x02, ROT_AXIS_ROLL = 0x04 };

void    rotInit();
void    rotService();                               // poll the joystick and send the merged rotation
void    rotSetAutoAxes(float pitch, float yaw, float roll, uint8_t heldMask);   // -1..1 each
void    rotClearAutoAxes();
float   rotPilotPitch();                            // raw pilot demand -1..1, read even while held
float   rotPilotYaw();
float   rotPilotRoll();

/***************************************************************************************
   PILOT INPUT OVERRIDE — the one rule every autopilot obeys (review decision):
   any pilot input on the rotation stick, the translation stick or the throttle lever
   disconnects EVERY autopilot. The stick tests are debounced (beyond ROT_OVERRIDE_THRESHOLD
   for ROT_OVERRIDE_MS) so a bumped stick does not drop a burn; the lever test is the
   Throttle Module's own touch / button detection (throttle_link). Returns true while an
   override holds, with the reason (HP_REASON_STICK / HP_REASON_LEVER) for annunciation.
   The translation joystick is polled here as well, for this test only — its forwarding
   to KSP is still to be integrated.
****************************************************************************************/
bool    pilotOverrideDetected(uint8_t &reason);

/***************************************************************************************
   infodisp_link.ino
****************************************************************************************/
void    idlInit();
void    idlService();

/***************************************************************************************
   ap_arbiter.ino — ONE attitude owner and ONE throttle owner at a time across the
   ascent, hold, burn and landing modules (Mission_Autopilot.md §7.6). Engaging a mode
   that needs a resource takes it; the previous owner is told to drop with reason
   OTHER AP. Rover steering / wheels are a separate channel and are not arbitrated.
   Only the burn executor may warp, and never while another owner holds either resource.
****************************************************************************************/
enum ApOwner : uint8_t { AP_OWNER_NONE = 0, AP_OWNER_ASCENT, AP_OWNER_HOLD, AP_OWNER_BURN, AP_OWNER_LANDING };
void    arbInit();
void    arbTakeAttitude(uint8_t owner);
void    arbTakeThrottle(uint8_t owner);
void    arbReleaseAttitude(uint8_t owner);
void    arbReleaseThrottle(uint8_t owner);
uint8_t arbAttitudeOwner();
uint8_t arbThrottleOwner();
bool    arbCanWarp(uint8_t owner);
void    arbAllOff();                                // A/P OFF from any console: everything off

/***************************************************************************************
   stage_helpers.ino — auto-stage and the acceleration estimate shared by the ascent,
   burn and landing modules (Mission_Autopilot.md §2, §7.6).
****************************************************************************************/
enum AeSource : uint8_t { AE_SRC_EST = 0, AE_SRC_MEAS = 1, AE_SRC_TWR = 2 };
void    asSetEnabled(bool on);                      // the shared STAGE option (orbital / landing consoles)
bool    asEnabled();
void    asMaybeStage(bool enabled, float throttle, float remainingDv);   // STAGE_ACTION with lockout
void    aeIngestStage(float stageDv, float stageBurnTime);
void    aeIngestGForce(float gForce);
void    aeIngestAtmo(bool inAtmosphere);
void    aeNoteThrottle(float commanded);            // the current throttle owner reports what it commands
void    aeSetTwrOverride(float twr, float g0);      // 0 = none
float   aeAccel();                                  // m/s² (0 if unknown)
uint8_t aeSource();
float   aeBurnDuration(float dv);                   // s (0 if unknown)
float   aeStageDv();

#endif  // CONTROL_LINKS_H
