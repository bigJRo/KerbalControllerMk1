/***************************************************************************************
   interlocks.h — ENGINE SAFE / ARM and A/P INHIB / ARM (design §8.2), and the "drop"
   notices that tell HOLD and the autopilots to disengage.

   ENGINE (Switch Group 1 B22):
     SAFE    0 % throttle to KSP; the lever is driven to 0 % and kept there: the target
             is re-sent every ENG_LEVER_CMD_MS until the lever reports 0 %, except while
             the pilot touches it (the Throttle module refuses it then), so a lever moved
             while SAFE goes back on release; every throttle input is ignored; a
             throttle-using autopilot is dropped (DROP_ENG_SAFE).
     ARMING  after a flip to ARM: the lever is driven to 0 % and 0 % sent again ...
     ARMED   ... and once the lever reports 0 %, throttle inputs are taken.
   Until Switch Group 1 is read the engine is SAFE. A switch already on ARM when it is
   first read (power-on, or a restart in flight) goes straight to ARMED: the lever is
   where the pilot left it and the throttle follows it (design §12.3).
   THRTL FINE (B23) is passed to the Throttle module only while ARMED and no autopilot
   owns the throttle: in precision mode the lever position is not the throttle, so the
   master drives the lever with FINE off.
   The 0 % itself goes out on the throttle channel (ksp_frame.h), which sends 0 whenever
   the engine is not ARMED; SAFE and ARM flips make it send again.

   A/P (B21): INHIB drops every console autopilot (DROP_AP_INHIB); HOLD ignores it.
   A/P disconnect (rotation stick button, SPC / PLN): DROP_AP_DISC, everything drops.

   Drop notices: HOLD (stage 4) and the autopilots (stage 7) register a listener and
   decide for themselves whether a reason applies to them.
****************************************************************************************/
#pragma once
#include <Arduino.h>

enum DropReason : uint8_t { DROP_AP_INHIB = 0, DROP_ENG_SAFE, DROP_DEMO, DROP_MSTR_RESET, DROP_AP_DISC };
typedef void (*DropFn)(DropReason why);

enum EngineState : uint8_t { ENG_SAFE = 0, ENG_ARMING, ENG_ARMED };

static const uint32_t ENG_LEVER_CMD_MS = 200;     // lever / precision command repeat
static const uint16_t ENG_LEVER_ZERO   = 328;     // 1 % of INT16_MAX: "at 0 %"

void        interlocksInit();
void        interlockService();
bool        interlockOnDrop(DropFn fn);            // false if the table is full
void        interlockDrop(DropReason why);         // tell every listener
const char *dropReasonName(DropReason why);

void        interlockSetEngine(bool arm, bool firstRead);
void        interlockSetApArm(bool arm, bool firstRead);
EngineState interlockEngineState();
bool        interlockThrottleLive();               // ARMED: throttle inputs are taken
bool        interlockEngineSafe();                 // the switch is on SAFE (or not read yet)
