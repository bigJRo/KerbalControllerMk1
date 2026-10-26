/***************************************************************************************
   simpit_link.h — the master's own KerbalSimpit link to KSP (SerialUSB1; Teensy USB
   type Dual Serial).

   Never blocks the loop for long: one handshake attempt (the library waits about 1 s
   for KSP) every SIMPIT_RETRY_MS while down, update() every pass while up.
   No handshake is attempted in demo, since nothing may go to KSP then (design §10.1).

   KSP gives no disconnect signal. While connected, in a flight scene, with nothing
   inbound for SIMPIT_LOST_MS, the Annunciator's SIMPIT LOST is set; after
   SIMPIT_REHANDSHAKE_MS of silence the link is treated as down and re-handshaken (the
   plugin treats a new handshake as a new session; channels are registered again).
   Bench check: that a parked vessel still produces inbound traffic (design §18).

   Inbound messages are dispatched from one table (simpit_link.cpp) into g_vessel and,
   from stage 7, the autopilots' ingest functions.
****************************************************************************************/
#pragma once
#include <Arduino.h>
#include <KerbalSimpit.h>

static const uint32_t SIMPIT_REHANDSHAKE_MS = 10000;

void          simpitInit();
bool          simpitAttempt();            // one handshake attempt (boot step 8, and retries)
void          simpitService();
bool          simpitConnected();
uint32_t      simpitAttempts();
KerbalSimpit &simpitClient();             // for ksp_out only
