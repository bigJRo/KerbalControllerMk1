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
