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
enum ThrOwner : uint8_t { THR_OWNER_NONE = 0, THR_OWNER_ASCENT, THR_OWNER_HOLD };

void    thrInit();
void    thrService();                               // poll the module, forward pilot throttle, drive the lever
void    thrAutoThrottle(uint8_t owner, float t);    // owner commands throttle t (0..1) to KSP + lever
void    thrAutoRelease(uint8_t owner);              // owner hands the throttle back to the pilot
bool    thrTakeOverrideEvent();                     // true once after the pilot takes the lever from an owner
bool    thrTouched();                               // pilot's hand is on the slider (module touch flag)
bool    thrPrecision();                             // module is in precision (fine) mode
bool    thrLeverDriven();                           // module enabled and following commands
bool    thrOverrideLatched();                       // pilot holds the lever against the current owner
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
   infodisp_link.ino
****************************************************************************************/
void    idlInit();
void    idlService();

#endif  // CONTROL_LINKS_H
