/***************************************************************************************
   ksp_frame.h — the per-frame half of ksp_out (design §7, §8.7, §11.3).

   KSP keeps only the latest message on each control channel, so each channel has one
   sender, here, which merges its inputs every pass and sends when the result changes
   (at most every FRAME_MIN_MS per channel), or when forced:
     ROTATION     pilot (SPC / PLN, not EVA) — or the autopilot's held axes; while any
                  autopilot or HOLD is engaged the stick is ignored (axes not held: 0)
     TRANSLATION  pilot (not EVA, not camera mode); 0 while engaged
     WHEEL        pilot (RVR, not EVA) — or the autopilot's held axes; 0 while engaged;
                  the wheel throttle is 0 unless the ENGINE is ARMED
     CAMERA       the translation stick while its button is held (also under autopilot)
     THROTTLE     0 unless the ENGINE is ARMED; else the autopilot that owns it, or the
                  pilot's lever. Under an owner the lever is driven to match (1 %
                  deadband, at most every 200 ms) and the lever, its buttons and touch
                  are ignored (no pilot override, design §11.3).
   Every channel is sent again on entering a flight scene, on the link coming up, on
   leaving demo, and when the autopilot / control mode / EVA state changes, so KSP is
   never left holding an input from before (neutral first after a restart, §12.3).
   Nothing is sent in demo, with the link down, or outside a flight scene.

   The autopilot interface keeps the names the autopilots use today (control_links.h in
   Controller_Main), so the port in stage 7 only re-points includes.
****************************************************************************************/
#pragma once
#include <Arduino.h>

static const uint32_t FRAME_MIN_MS        = 20;
static const int16_t  FRAME_AXIS_NOISE    = 160;   // resend an axis after this much change
static const int16_t  FRAME_THR_NOISE     = 66;    // 0.2 %
static const uint16_t FRAME_LEVER_BAND    = 328;   // 1 %: lever-follow deadband
static const uint32_t FRAME_LEVER_MIN_MS  = 200;

// Pilot inputs, -1..1 (0..1 for the throttle), already mapped, scaled and trimmed by
// the stick and throttle tabs.
void frameSetPilotRotation(float pitch, float yaw, float roll);
void frameSetPilotTranslation(float x, float y, float z);
void frameSetPilotWheel(float steer, float throttle);
void frameSetCamera(bool on, float pitch, float yaw, float zoom);
void frameSetPilotThrottle(float t);

void frameInit();
void frameService();
void frameForce();                     // every channel again on the next pass
void frameForceThrottle();             // the throttle again (ENGINE SAFE / ARM)
uint16_t frameThrottleSent();          // last value sent, 0..INT16_MAX

/*--------------------------------------------------------------------------------------
  Autopilot interface (stage 7)
--------------------------------------------------------------------------------------*/
enum ThrOwner : uint8_t { THR_OWNER_NONE = 0, THR_OWNER_ASCENT, THR_OWNER_HOLD, THR_OWNER_BURN, THR_OWNER_LANDING };
void    thrAutoThrottle(uint8_t owner, float t);   // owner commands throttle t (0..1)
void    thrAutoRelease(uint8_t owner);
uint8_t thrOwner();
bool    thrAutoOwned();
float   thrCurrentThrottle();                      // what KSP is being sent, 0..1
bool    thrTouched();
bool    thrPrecision();
bool    thrLeverDriven();                          // the lever follows commands (ACTIVE, not FINE)

enum { ROT_AXIS_PITCH = 0x01, ROT_AXIS_YAW = 0x02, ROT_AXIS_ROLL = 0x04 };
void    rotSetAutoAxes(float pitch, float yaw, float roll, uint8_t heldMask);
void    rotClearAutoAxes();
float   rotPilotPitch();                           // the pilot's demand, even while ignored
float   rotPilotYaw();
float   rotPilotRoll();

enum { WHEEL_AXIS_STEER = 0x01, WHEEL_AXIS_THROTTLE = 0x02 };
void    wheelSetAuto(float steer, float throttle, uint8_t heldMask);
void    wheelClearAuto();
