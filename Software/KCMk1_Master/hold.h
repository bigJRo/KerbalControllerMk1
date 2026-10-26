/***************************************************************************************
   hold.h — HOLD (Stability Control B10): a temporary hold of the current condition
   (design §8.4, Inventory §8.7). Not gated by A/P ARM; A/P INHIB does not drop it.

     SPC  attitude: KSP SAS on, mode Stability Assist. On release SAS goes back to what
          it was (off, or its previous mode).
     PLN  altitude + heading: the hold autopilot's ALT + HDG at the current values.
     RVR  heading + speed: its RHDG + CRUISE at the current values; heading only while
          the ENGINE is not ARMED (ENGINE SAFE later drops CRUISE and HOLD stays).
   Engaging HOLD first disconnects any console autopilot (OTHER AP): HOLD takes the
   vehicle as it is. The hold autopilot may refuse (e.g. PLN outside an atmosphere, RVR
   not on the ground); then HOLD does not engage.

   Released by: its button again, the A/P disconnect (rotation stick button, stage 5),
   a console autopilot engaging (stage 7), demo, MSTR RESET, and anything that ends the
   condition it holds: leaving the flight scene, the link going down, a vessel change,
   EVA, a control-mode change, or the hold autopilot letting go by itself.
   While engaged: the LED is ACTIVE, the Annunciator's AUTOPILOT is lit, SWITCH ERR does
   not check SAS, and (stage 5) the sticks are ignored.
****************************************************************************************/
#pragma once
#include <Arduino.h>

void holdInit();
void holdService();                     // release conditions, every pass
bool holdAvailable();                   // in this control mode, now
bool holdEngaged();
void holdToggle();                      // the HOLD button
void holdRelease(const char *why);      // A/P disconnect, console autopilots, …
void holdNoteVesselChange();            // simpit_link: VESSEL_CHANGE
bool holdUsesHoldAutopilot();           // engaged in PLN or RVR (its modes are HOLD's, not a console's)
