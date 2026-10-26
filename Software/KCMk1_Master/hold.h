/***************************************************************************************
   hold.h — HOLD (Stability Control B10): a temporary hold of the current condition
   (design §8.4, Inventory §8.7). Not gated by A/P ARM; A/P INHIB does not drop it.

     SPC  attitude: KSP SAS on, mode Stability Assist. On release SAS goes back to what
          it was (off, or its previous mode).
     PLN  altitude + heading   } the hold autopilot (stage 7). Until it is ported HOLD
     RVR  heading + speed      } is not available in these modes: its LED is dark.

   Released by: its button again, the A/P disconnect (rotation stick button, stage 5),
   a console autopilot engaging (stage 7), demo, MSTR RESET, and anything that ends the
   condition it holds: leaving the flight scene, the link going down, a vessel change,
   EVA, or a control-mode change.
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
