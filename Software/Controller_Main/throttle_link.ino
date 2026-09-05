/***************************************************************************************
   throttle_link.ino — Throttle Module (0x2C) link for the master.

   Reads the module's 7-byte packet (I2C Protocol Specification §9.4):
     Byte 0-2 : universal header (status, type ID, tx counter)
     Byte 3   : flags   bit0 enabled, bit1 precision, bit2 pilot touching, bit3 motor moving
     Byte 4   : buttons bit0 100%, bit1 UP, bit2 DOWN, bit3 0%  (rising-edge events)
     Byte 5-6 : throttle value, uint16 big-endian, 0..INT16_MAX

   Forwards the wiper to KSP as the pilot's throttle unless an autopilot owns the
   throttle, in which case the owner's value goes to KSP AND to the motorised lever
   (CMD_SET_THROTTLE) so the physical lever tracks the commanded throttle. The pilot
   grabbing the lever (touch flag or any button) latches an override: the owner's
   commands are dropped, the wiper is forwarded again, and a one-shot event tells the
   owner to annunciate. Design: Hold_Mode_Autopilot.md §7.

   Lever drive is deadbanded (1 %) and rate-limited (200 ms) so the H-bridge does not
   chatter at the setpoint; the module itself refuses CMD_SET_THROTTLE while touched.
****************************************************************************************/
#include "control_links.h"

static const uint32_t THR_POLL_MS        = 50;     // fallback poll when INT is not seen
static const uint32_t THR_LEVER_MIN_MS   = 200;    // min interval between lever commands
static const float    THR_LEVER_DEADBAND = 0.01f;  // lever command deadband (fraction)
static const float    THR_KSP_DEADBAND   = 0.002f; // KSP throttle resend deadband
static const float    THR_SYNC_WINDOW    = 0.03f;  // wiper must pass within this of the held value

static const uint8_t THR_FLAG_ENABLED   = 0x01;
static const uint8_t THR_FLAG_PRECISION = 0x02;
static const uint8_t THR_FLAG_TOUCH     = 0x04;
static const uint8_t THR_FLAG_MOVING    = 0x08;

static uint8_t  g_thrOwner        = THR_OWNER_NONE;
static bool     g_thrOverride     = false;   // pilot holds the lever against the owner
static bool     g_thrOverrideEvt  = false;   // one-shot for the owner
static bool     g_thrMovedEvt     = false;   // one-shot: pilot moved the lever with no owner driving it
static float    g_thrMovedRef     = -1.0f;   // wiper position the movement test is measured from
static const float THR_MOVED_DEADBAND = 0.02f;
static bool     g_thrSyncLatch    = false;   // lever not driven: hold last auto value until the wiper passes it
static float    g_thrSyncValue    = 0.0f;

static uint8_t  g_thrFlags        = 0;
static bool     g_thrPrevTouch    = false;
static float    g_thrLever        = 0.0f;    // wiper 0..1
static float    g_thrLastKsp      = -1.0f;   // last value sent to KSP
static float    g_thrLastLeverCmd = -1.0f;   // last CMD_SET_THROTTLE value
static uint32_t g_thrLastLeverMs  = 0;
static uint32_t g_thrLastPollMs   = 0;
static bool     g_thrEnabledSent  = false;
static bool     g_thrPrecisionCmd = false;

static void thrSendKsp(float t) {
  t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
  if (g_thrLastKsp >= 0.0f && fabsf(t - g_thrLastKsp) < THR_KSP_DEADBAND) return;
  throttleMessage msg;
  msg.throttle = (int16_t)(t * (float)INT16_MAX);
  mySimpit.send(THROTTLE_MESSAGE, msg);
  g_thrLastKsp = t;
}

static void thrSendCommand(uint8_t cmd, const uint8_t *payload, uint8_t n) {
  Wire.beginTransmission(Throttle_MOD);
  Wire.write(cmd);
  for (uint8_t i = 0; i < n; i++) Wire.write(payload[i]);
  Wire.endTransmission();
}

static void thrDriveLever(float t, uint32_t now) {
  if (!thrLeverDriven() || (g_thrFlags & THR_FLAG_TOUCH)) return;
  if (g_thrLastLeverCmd >= 0.0f && fabsf(t - g_thrLastLeverCmd) < THR_LEVER_DEADBAND) return;
  if (now - g_thrLastLeverMs < THR_LEVER_MIN_MS) return;
  uint16_t v = (uint16_t)(t * (float)INT16_MAX);
  uint8_t p[2] = { (uint8_t)(v >> 8), (uint8_t)(v & 0xFF) };
  thrSendCommand(KMC_CMD_SET_THROTTLE, p, 2);
  g_thrLastLeverCmd = t;
  g_thrLastLeverMs  = now;
}

static void thrLatchOverride() {
  if (g_thrOwner == THR_OWNER_NONE || g_thrOverride) return;
  g_thrOverride    = true;
  g_thrOverrideEvt = true;
  // The lever was not following (module disabled / precision): hold the last
  // autopilot value until the wiper passes through it, then hand over — the classic
  // throttle-sync latch. When the lever WAS following there is nothing to sync.
  if (!thrLeverDriven() && g_thrLastKsp >= 0.0f) { g_thrSyncLatch = true; g_thrSyncValue = g_thrLastKsp; }
}

void thrInit() {
  pinMode(Throttle_INT, INPUT);
  g_thrLastPollMs = millis();
}

void thrSetPrecision(bool fine) {
  g_thrPrecisionCmd = fine;
  uint8_t p = fine ? 1 : 0;
  thrSendCommand(KMC_CMD_SET_PRECISION, &p, 1);
}

static void thrPoll(uint32_t now) {
  bool intLow = (digitalRead(Throttle_INT) == LOW);
  if (!intLow && (now - g_thrLastPollMs) < THR_POLL_MS) return;
  g_thrLastPollMs = now;

  uint8_t pkt[KMC_THROTTLE_PACKET_SIZE];
  uint8_t got = 0;
  Wire.requestFrom((uint8_t)Throttle_MOD, (uint8_t)KMC_THROTTLE_PACKET_SIZE);
  while (Wire.available() && got < KMC_THROTTLE_PACKET_SIZE) pkt[got++] = Wire.read();
  if (got < KMC_THROTTLE_PACKET_SIZE) return;
  if (pkt[KMC_PKT_TYPEID_OFFSET] != KMC_TYPE_THROTTLE) return;

  g_thrFlags = pkt[3];
  uint8_t buttons = pkt[4];
  uint16_t raw = ((uint16_t)pkt[5] << 8) | pkt[6];
  g_thrLever = (float)raw / (float)INT16_MAX;
  if (g_thrLever > 1.0f) g_thrLever = 1.0f;

  bool touch = (g_thrFlags & THR_FLAG_TOUCH) != 0;
  if ((touch && !g_thrPrevTouch) || buttons != 0) thrLatchOverride();
  g_thrPrevTouch = touch;

  // Lever MOVEMENT is pilot input even when no owner drives the lever (an attitude-only
  // hold, a burn aligning). A resting hand is not: the touch flag alone does not count
  // here, only a change of more than the deadband, or a lever button.
  if (g_thrOwner == THR_OWNER_NONE || !thrLeverDriven()) {
    if (g_thrMovedRef < 0.0f) g_thrMovedRef = g_thrLever;
    if (fabsf(g_thrLever - g_thrMovedRef) > THR_MOVED_DEADBAND || buttons != 0) {
      g_thrMovedEvt = true;
      g_thrMovedRef = g_thrLever;
    }
  } else {
    g_thrMovedRef = g_thrLever;    // lever is following the owner: its motion is not the pilot's
  }
}

void thrService() {
  uint32_t now = millis();

  // Module enable follows the master's throttle-enable state.
  if (!g_thrEnabledSent || throttleEn != ((g_thrFlags & THR_FLAG_ENABLED) != 0)) {
    thrSendCommand(throttleEn ? KMC_CMD_ENABLE : KMC_CMD_DISABLE, nullptr, 0);
    g_thrEnabledSent = true;
  }

  thrPoll(now);

  bool pilotHasIt = (g_thrOwner == THR_OWNER_NONE) || g_thrOverride;
  if (!pilotHasIt) return;                       // owner drives KSP via thrAutoThrottle()

  if (g_thrSyncLatch) {
    if (fabsf(g_thrLever - g_thrSyncValue) <= THR_SYNC_WINDOW) g_thrSyncLatch = false;
    else return;                                 // KSP keeps the held value until the lever catches up
  }
  if (throttleEn) thrSendKsp(g_thrLever);
}

void thrAutoThrottle(uint8_t owner, float t) {
  if (owner == THR_OWNER_NONE) return;
  if (owner != g_thrOwner) {                     // new owner takes the throttle
    g_thrOwner = owner;
    g_thrOverride = false; g_thrOverrideEvt = false; g_thrSyncLatch = false;
    if (g_thrFlags & THR_FLAG_TOUCH) thrLatchOverride();   // pilot already has the lever
  }
  if (g_thrOverride) return;
  thrSendKsp(t);
  thrDriveLever(t, millis());
}

void thrAutoRelease(uint8_t owner) {
  if (owner != g_thrOwner) return;
  g_thrOwner = THR_OWNER_NONE;
  g_thrOverride = false; g_thrOverrideEvt = false;
  // Lever was following: the wiper already equals the last command, so the pilot's
  // throttle is continuous. Lever not following: sync-latch on the last value.
  if (!thrLeverDriven() && g_thrLastKsp >= 0.0f && fabsf(g_thrLever - g_thrLastKsp) > THR_SYNC_WINDOW) {
    g_thrSyncLatch = true; g_thrSyncValue = g_thrLastKsp;
  }
}

bool    thrTakeOverrideEvent() { bool e = g_thrOverrideEvt; g_thrOverrideEvt = false; return e; }
bool    thrTakeMovedEvent()    { bool e = g_thrMovedEvt;    g_thrMovedEvt    = false; return e; }
bool    thrTouched()           { return (g_thrFlags & THR_FLAG_TOUCH) != 0; }
bool    thrPrecision()         { return g_thrPrecisionCmd || (g_thrFlags & THR_FLAG_PRECISION) != 0; }
bool    thrLeverDriven()       { return throttleEn && (g_thrFlags & THR_FLAG_ENABLED) && !thrPrecision(); }
bool    thrOverrideLatched()   { return g_thrOverride; }
float   thrCurrentThrottle()   { return (g_thrOwner != THR_OWNER_NONE && !g_thrOverride && g_thrLastKsp >= 0.0f) ? g_thrLastKsp : g_thrLever; }
uint8_t thrOwner()             { return g_thrOwner; }
