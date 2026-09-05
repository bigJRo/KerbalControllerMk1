/********************************************************************************************************************************
  Attitude Controller — Kerbal Controller Mk1 (Master Teensy 4.1)

  The one attitude loop both autopilots steer through. The ascent autopilot used to own this as apSteer(); it is factored out
  so the hold-mode autopilot (hold_autopilot.ino) can drive the same PID with a different outer loop.

  Two entry points, one per vehicle class:
    attSteerRocket()   — heading error is corrected with YAW (a rocket turns its nose). The original apSteer() behaviour.
    attSteerAircraft() — heading is NOT corrected here at all: the lateral outer loop supplies a BANK command, the pitch outer
                         loop a PITCH command, and yaw stays at zero (plus an optional turn-coordination term). Rocket gains on
                         a plane oscillate, so each class carries its own gain set.

  Rates: the Simpit rotation-data message carries no angular rates, so the D term uses a low-pass filtered first difference
  of the attitude samples (attUpdateRates), applied on the measurement rather than the error so a setpoint step does not
  kick the surfaces.

  Outputs are normalised stick commands in -1..1; the caller scales to the Simpit axis full-scale and sends them.

  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Written for Jeb's Controller Works.
********************************************************************************************************************************/
#ifndef ATTITUDE_CONTROLLER_H
#define ATTITUDE_CONTROLLER_H

#include <Arduino.h>

struct AttGains {
  float pitchKp, pitchKi, pitchKd;
  float yawKp,   yawKi,   yawKd;
  float rollKp,  rollKi,  rollKd;
};

struct AttMeasure {          // current attitude in the navball frame, degrees
  float pitch;               // above horizon, -90..90
  float heading;             // 0..360
  float roll;                // -180..180
};

struct AttState {
  float iPitch, iYaw, iRoll;              // clamped integrators (normalised units)
  float prevPitch, prevHeading, prevRoll; // last samples for the rate estimate
  float ratePitch, rateHeading, rateRoll; // deg/s, low-pass filtered
  bool  ratesValid;
};

struct AttCommand {          // normalised stick demand, -1..1 per axis
  float pitch, yaw, roll;
};

// Error normalisation: a 30 degree error maps to 1.0 before the PID (matches the original apSteer()).
static const float ATT_ERR_NORM = 1.0f / 30.0f;

void      attReset(AttState &s);
void      attUpdateRates(AttState &s, const AttMeasure &m, float dt);   // once per loop, before steering

AttCommand attSteerRocket(AttState &s, const AttGains &g, const AttMeasure &m,
                          float cmdPitch, float cmdHeading,
                          bool holdRoll, float cmdRoll, float maxDeflection, float dt);

AttCommand attSteerAircraft(AttState &s, const AttGains &g, const AttMeasure &m,
                            float cmdPitch, float cmdBank, bool coordinateTurn,
                            float maxDeflection, float dt);

AttGains  attRocketGains();     // the ascent autopilot's original defaults
AttGains  attAircraftGains();   // conservative aircraft defaults (SAS off, stock Simpit refresh)

float     attWrap180(float deg);
float     attWrap360(float deg);
float     attClampf(float v, float lo, float hi);

#endif  // ATTITUDE_CONTROLLER_H
