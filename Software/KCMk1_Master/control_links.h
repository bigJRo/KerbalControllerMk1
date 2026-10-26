/********************************************************************************************************************************
  Control Links — the autopilots' contract with the rest of the KCMk1 master (design §11).

  The autopilots (ascent, hold, burn, landing) were ported from Controller_Main with their control laws unchanged; only the
  interfaces they reach the master through are re-pointed:
    - throttle and rotation (thrAutoThrottle, rotSetAutoAxes, …) and the wheels / translation axes they hold are the per-frame
      control channels of ksp_frame.h, which merge them with the pilot and send ONE message per channel;
    - commands to KSP (SAS, action groups, warp-to, messages) go through ksp_out.h's gate (nothing in demo, link down);
    - the Info Display 2 consoles are infodisp_link.cpp, over the master's I2C layer.

  PILOT AUTHORITY (design §11.3, replaces the old pilot-input override): stick or lever input no longer disconnects anything.
  An engaged autopilot keeps authority until it is disconnected deliberately — A/P disconnect (rotation stick button), A/P
  INHIB, a console's A/P OFF, HOLD's own button, ENGINE SAFE (throttle-using modes) or its own disengage rules. While engaged
  the sticks are ignored and the lever follows the autopilot (ksp_frame.h).

  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Written for Jeb's Controller Works.
********************************************************************************************************************************/
#ifndef CONTROL_LINKS_H
#define CONTROL_LINKS_H

#include <Arduino.h>
#include "ksp_frame.h"      // thrAutoThrottle / thrAutoRelease / thr*(), rotSetAutoAxes / rotClearAutoAxes / rotPilot*(),
                            // wheelSetAuto / wheelClearAuto, trnSetAuto / trnClearAuto, ROT_AXIS_*, THR_OWNER_*
#include "ksp_out.h"        // kspActionGroup, kspSasMode, kspTimewarpTo, kspPrint

#include "config.h"         // KSP_SIT_LANDED / KSP_SIT_SPLASHED (Vessel.Situations)

// KSP VesselType values carried by FLIGHT_STATUS_MESSAGE.
static const uint8_t KSP_TYPE_ROVER   = 5;
static const uint8_t KSP_TYPE_PLANE   = 8;

/***************************************************************************************
   ap_arbiter.cpp — ONE attitude owner and ONE throttle owner at a time across the
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
void    arbAllOffReason(uint8_t reason);            // everything off, with this HP_REASON_*

/***************************************************************************************
   stage_helpers.cpp — auto-stage and the acceleration estimate shared by the ascent,
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
