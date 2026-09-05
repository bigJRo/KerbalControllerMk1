/***************************************************************************************
   attitude_controller.ino — shared attitude PID for the ascent and hold-mode autopilots.
   See attitude_controller.h for the contract.
****************************************************************************************/
#include "attitude_controller.h"

static const float ATT_RATE_TAU_S = 0.25f;   // rate estimate low-pass time constant

float attClampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

float attWrap180(float a) {
  while (a >  180.0f) a -= 360.0f;
  while (a < -180.0f) a += 360.0f;
  return a;
}

float attWrap360(float a) {
  while (a >= 360.0f) a -= 360.0f;
  while (a <    0.0f) a += 360.0f;
  return a;
}

void attReset(AttState &s) {
  s.iPitch = s.iYaw = s.iRoll = 0.0f;
  s.ratePitch = s.rateHeading = s.rateRoll = 0.0f;
  s.prevPitch = s.prevHeading = s.prevRoll = 0.0f;
  s.ratesValid = false;
}

// Filtered first difference of the attitude samples. The Simpit refresh interval bounds
// this — at 125 ms the raw difference is coarse, hence the low-pass.
void attUpdateRates(AttState &s, const AttMeasure &m, float dt) {
  if (dt <= 0.0f) return;
  if (!s.ratesValid) {
    s.prevPitch = m.pitch; s.prevHeading = m.heading; s.prevRoll = m.roll;
    s.ratesValid = true;
    return;
  }
  float a = dt / (ATT_RATE_TAU_S + dt);
  float rp = (m.pitch - s.prevPitch) / dt;
  float rh = attWrap180(m.heading - s.prevHeading) / dt;
  float rr = attWrap180(m.roll - s.prevRoll) / dt;
  s.ratePitch   += a * (rp - s.ratePitch);
  s.rateHeading += a * (rh - s.rateHeading);
  s.rateRoll    += a * (rr - s.rateRoll);
  s.prevPitch = m.pitch; s.prevHeading = m.heading; s.prevRoll = m.roll;
}

/***************************************************************************************
   Rocket: navball-frame pitch/heading errors rotated into the body frame by the current
   roll, PID per axis, heading corrected with yaw. Identical to the original apSteer().
****************************************************************************************/
AttCommand attSteerRocket(AttState &s, const AttGains &g, const AttMeasure &m,
                          float cmdPitch, float cmdHeading,
                          bool holdRoll, float cmdRoll, float maxDeflection, float dt) {
  float ePitch = cmdPitch - m.pitch;                 // + means pitch up
  float eHead  = attWrap180(cmdHeading - m.heading);

  float phi = m.roll * 0.0174532925199f;
  float bodyPitchErr =  ePitch * cosf(phi) + eHead * sinf(phi);
  float bodyYawErr   = -ePitch * sinf(phi) + eHead * cosf(phi);

  bodyPitchErr *= ATT_ERR_NORM;
  bodyYawErr   *= ATT_ERR_NORM;

  s.iPitch = attClampf(s.iPitch + bodyPitchErr * dt, -1.0f, 1.0f);
  s.iYaw   = attClampf(s.iYaw   + bodyYawErr   * dt, -1.0f, 1.0f);

  // D on measurement: body pitch rate approximated by the navball pitch rate rotated by roll.
  float bodyPitchRate = ( s.ratePitch * cosf(phi) + s.rateHeading * sinf(phi)) * ATT_ERR_NORM;
  float bodyYawRate   = (-s.ratePitch * sinf(phi) + s.rateHeading * cosf(phi)) * ATT_ERR_NORM;

  AttCommand c;
  c.pitch = g.pitchKp * bodyPitchErr + g.pitchKi * s.iPitch - g.pitchKd * bodyPitchRate;
  c.yaw   = g.yawKp   * bodyYawErr   + g.yawKi   * s.iYaw   - g.yawKd   * bodyYawRate;
  c.roll  = 0.0f;
  if (holdRoll) {
    float eRoll = attWrap180(cmdRoll - m.roll) * ATT_ERR_NORM;
    s.iRoll = attClampf(s.iRoll + eRoll * dt, -1.0f, 1.0f);
    c.roll  = g.rollKp * eRoll + g.rollKi * s.iRoll - g.rollKd * (s.rateRoll * ATT_ERR_NORM);
  }
  c.pitch = attClampf(c.pitch, -maxDeflection, maxDeflection);
  c.yaw   = attClampf(c.yaw,   -maxDeflection, maxDeflection);
  c.roll  = attClampf(c.roll,  -maxDeflection, maxDeflection);
  return c;
}

/***************************************************************************************
   Aircraft: elevator holds pitch, aileron holds bank, rudder idle (or a small
   coordination term). Heading is the lateral outer loop's job — it commands the bank.
****************************************************************************************/
AttCommand attSteerAircraft(AttState &s, const AttGains &g, const AttMeasure &m,
                            float cmdPitch, float cmdBank, bool coordinateTurn,
                            float maxDeflection, float dt) {
  float ePitch = (cmdPitch - m.pitch) * ATT_ERR_NORM;
  float eRoll  = attWrap180(cmdBank - m.roll) * ATT_ERR_NORM;

  s.iPitch = attClampf(s.iPitch + ePitch * dt, -1.0f, 1.0f);
  s.iRoll  = attClampf(s.iRoll  + eRoll  * dt, -1.0f, 1.0f);

  AttCommand c;
  c.pitch = g.pitchKp * ePitch + g.pitchKi * s.iPitch - g.pitchKd * (s.ratePitch * ATT_ERR_NORM);
  c.roll  = g.rollKp  * eRoll  + g.rollKi  * s.iRoll  - g.rollKd  * (s.rateRoll  * ATT_ERR_NORM);
  c.yaw   = 0.0f;
  if (coordinateTurn) {
    // A little rudder into the bank damps the adverse-yaw wobble KSP planes show in a turn.
    c.yaw = g.yawKp * (m.roll / 60.0f) * 0.25f;
  }
  c.pitch = attClampf(c.pitch, -maxDeflection, maxDeflection);
  c.yaw   = attClampf(c.yaw,   -maxDeflection, maxDeflection);
  c.roll  = attClampf(c.roll,  -maxDeflection, maxDeflection);
  return c;
}

AttGains attRocketGains() {
  AttGains g;
  g.pitchKp = 0.9f; g.pitchKi = 0.05f; g.pitchKd = 0.0f;
  g.yawKp   = 0.9f; g.yawKi   = 0.05f; g.yawKd   = 0.0f;
  g.rollKp  = 0.6f; g.rollKi  = 0.02f; g.rollKd  = 0.0f;
  return g;
}

AttGains attAircraftGains() {
  AttGains g;
  g.pitchKp = 0.60f; g.pitchKi = 0.03f; g.pitchKd = 0.12f;
  g.yawKp   = 0.40f; g.yawKi   = 0.00f; g.yawKd   = 0.00f;
  g.rollKp  = 0.50f; g.rollKi  = 0.01f; g.rollKd  = 0.08f;
  return g;
}
