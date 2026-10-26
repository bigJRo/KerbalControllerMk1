/***************************************************************************************
   autopilots.h — the ported autopilots in the master (design §11): ascent, hold, burn,
   landing, their arbiter and helpers, and the Info Display 2 consoles.

   autopilotsService() runs every module's update and the console link, once per pass
   (loop step 6). autopilotsIngest() is simpit_link's hook: every inbound message, fed to
   the modules' ingest functions exactly as Controller_Main's message handler did.

   The master's rules on top of them (design §8.2, §11.3):
     - A/P INHIB refuses every engage from a console (infodisp_link.cpp), and switching to
       INHIB disconnects every console autopilot (reason A/P INHIB) — but not HOLD's own.
     - ENGINE SAFE refuses throttle-using modes, and drops the engaged ones (ENG SAFE):
       the ascent, a planned or running burn, IAS / MACH, rover CRUISE / FOLLOW, the
       landing throttle modes. Attitude-only modes stay.
     - The A/P disconnect (rotation stick button) drops everything (A/P DISC); demo and
       MSTR RESET drop everything silently.
     - Stick and lever input disconnect nothing (control_links.h).
     - While any is engaged: the Annunciator's AUTOPILOT is lit, the sticks are ignored
       (ksp_frame.h), and SWITCH ERR does not check SAS (the modules change it).
   Bench consoles on the serial console: `ap`, `hp`, `bp`, `lp` + the module's own line.
****************************************************************************************/
#pragma once
#include <Arduino.h>

void autopilotsInit();
void autopilotsService();
bool autopilotsEngaged();                          // any console autopilot (not counting HOLD)
void autopilotsIngest(byte type, const byte *msg, byte size);
void autopilotsChannels(void (*reg)(byte channel));   // the channels the modules need
