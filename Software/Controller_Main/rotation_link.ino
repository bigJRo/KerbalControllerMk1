/***************************************************************************************
   rotation_link.ino — Rotation joystick (0x28) link for the master.

   Reads the 12-byte KerbalJoystickCore packet:
     Byte 0-2  : universal header
     Byte 3    : button events   Byte 4 : change mask   Byte 5 : button state
     Byte 6-7  : AXIS1 (X)  -> roll      int16 big-endian
     Byte 8-9  : AXIS2 (Y)  -> pitch
     Byte 10-11: AXIS3 (Zr) -> yaw

   Sends ONE rotation message per frame that merges the pilot's stick with the axes the
   hold autopilot holds (rotSetAutoAxes). The Simpit plugin keeps only the latest
   rotation message, so a held axis and a pilot axis from two senders would clobber
   each other. While the ascent autopilot is armed it sends its own rotation and this
   link stays silent. The raw pilot demand is exposed (rotPilot*) even for held axes so
   the hold autopilot can detect a stick override. Design: Hold_Mode_Autopilot.md §6.6.

   Trim, precision scaling and the joystick buttons are not handled here yet — this tab
   is the initial forwarding path; the ROTATION module's buttons still need their
   controller-side sequencing.
****************************************************************************************/
#include "control_links.h"
#include "ascent_autopilot.h"

static const uint32_t ROT_POLL_MS   = 20;
static const uint32_t ROT_SEND_MS   = 20;     // max merged-message rate
static const float    ROT_PRECISION = 0.3f;   // stick scale in precision mode

static float    g_rotPitch = 0.0f, g_rotYaw = 0.0f, g_rotRoll = 0.0f;   // pilot, -1..1
static float    g_autoPitch = 0.0f, g_autoYaw = 0.0f, g_autoRoll = 0.0f;
static uint8_t  g_autoMask = 0;
static bool     g_rotDirty = false;
static uint32_t g_rotLastPollMs = 0, g_rotLastSendMs = 0;
static int16_t  g_lastSentP = 0, g_lastSentY = 0, g_lastSentR = 0;
static uint8_t  g_lastSentMask = 0xFF;

static inline int16_t rotBE(const uint8_t *p) { return (int16_t)(((uint16_t)p[0] << 8) | p[1]); }
static inline float   rotNorm(int16_t v)      { float f = (float)v / 32767.0f; return f < -1.0f ? -1.0f : (f > 1.0f ? 1.0f : f); }

void rotInit() {
  pinMode(Rotation_INT, INPUT);
  g_rotLastPollMs = millis();
}

static void rotPoll(uint32_t now) {
  bool intLow = (digitalRead(Rotation_INT) == LOW);
  if (!intLow && (now - g_rotLastPollMs) < ROT_POLL_MS) return;
  g_rotLastPollMs = now;

  uint8_t pkt[KMC_JOYSTICK_PACKET_SIZE];
  uint8_t got = 0;
  Wire.requestFrom((uint8_t)Rotation_MOD, (uint8_t)KMC_JOYSTICK_PACKET_SIZE);
  while (Wire.available() && got < KMC_JOYSTICK_PACKET_SIZE) pkt[got++] = Wire.read();
  if (got < KMC_JOYSTICK_PACKET_SIZE) return;
  if (pkt[KMC_PKT_TYPEID_OFFSET] != KMC_TYPE_JOYSTICK_ROTATION) return;

  float roll  = rotNorm(rotBE(&pkt[6]));
  float pitch = rotNorm(rotBE(&pkt[8]));
  float yaw   = rotNorm(rotBE(&pkt[10]));
  if (pitch != g_rotPitch || yaw != g_rotYaw || roll != g_rotRoll) g_rotDirty = true;
  g_rotPitch = pitch; g_rotYaw = yaw; g_rotRoll = roll;
}

void rotSetAutoAxes(float pitch, float yaw, float roll, uint8_t heldMask) {
  g_autoPitch = pitch; g_autoYaw = yaw; g_autoRoll = roll;
  g_autoMask = heldMask;
  g_rotDirty = true;
}

void rotClearAutoAxes() {
  if (g_autoMask != 0) g_rotDirty = true;
  g_autoMask = 0;
  g_autoPitch = g_autoYaw = g_autoRoll = 0.0f;
}

float rotPilotPitch() { return g_rotPitch; }
float rotPilotYaw()   { return g_rotYaw; }
float rotPilotRoll()  { return g_rotRoll; }

void rotService() {
  uint32_t now = millis();
  rotPoll(now);

  if (apIsArmed()) return;                              // ascent autopilot owns the channel
  if (!g_rotDirty || (now - g_rotLastSendMs) < ROT_SEND_MS) return;

  float scale = precisionEn ? ROT_PRECISION : 1.0f;
  float p = (g_autoMask & ROT_AXIS_PITCH) ? g_autoPitch : g_rotPitch * scale;
  float y = (g_autoMask & ROT_AXIS_YAW)   ? g_autoYaw   : g_rotYaw   * scale;
  float r = (g_autoMask & ROT_AXIS_ROLL)  ? g_autoRoll  : g_rotRoll  * scale;
  int16_t ip = (int16_t)(p * (float)INT16_MAX);
  int16_t iy = (int16_t)(y * (float)INT16_MAX);
  int16_t ir = (int16_t)(r * (float)INT16_MAX);

  // Resend only when something moved by more than the noise, or the held set changed.
  if (g_autoMask == g_lastSentMask &&
      abs(ip - g_lastSentP) < 160 && abs(iy - g_lastSentY) < 160 && abs(ir - g_lastSentR) < 160) {
    g_rotDirty = false;
    return;
  }
  rotationMessage msg;
  msg.setPitch(ip); msg.setYaw(iy); msg.setRoll(ir);
  mySimpit.send(ROTATION_MESSAGE, msg);
  g_lastSentP = ip; g_lastSentY = iy; g_lastSentR = ir; g_lastSentMask = g_autoMask;
  g_rotLastSendMs = now;
  g_rotDirty = false;
}
