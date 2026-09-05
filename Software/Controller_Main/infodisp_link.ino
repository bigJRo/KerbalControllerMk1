/***************************************************************************************
   infodisp_link.ino — Info Display 2 (0x13) autopilot console link for the master.

   Byte-level contract: Documents/Developer/Ascent_Autopilot_Interface.md (transport,
   ascent opcodes, 40-byte ascent status) and Hold_Mode_Autopilot.md §8 (hold-mode
   opcodes, 44-byte aircraft status, 28-byte rover status).

     Poll  (every IDL_POLL_MS)  : read the 10-byte outbound packet; a non-zero cmdSeq that
                                  differs from the last executed one, with a good XOR
                                  checksum, is applied ONCE and then acknowledged.
     ACK   (2-byte control write): controlByte carries the master's mode bits; byte 1 is
                                  the sequence just executed (0 = nothing to ack).
     Push  (every IDL_PUSH_MS)  : the status frame the console on screen needs, chosen by
                                  the activeScreen byte. Skipped while the display is in
                                  demo mode (it generates its own status then).

   Info Display 1 (0x12) never carries a command (its console is compiled out), so only
   the mission panel is polled. Both displays are sent PROCEED at init so they leave
   their boot hold; the rest of the display-carrier handshake is still to be integrated.
****************************************************************************************/
#include "control_links.h"
#include "ascent_autopilot.h"
#include "hold_autopilot.h"

static const uint32_t IDL_POLL_MS = 50;
static const uint32_t IDL_PUSH_MS = 100;

static const uint8_t IDL_SYNC_OUT      = 0xAE;   // InfoDisp -> master framing byte
static const uint8_t IDL_SYNC_ASCENT   = 0xA5;
static const uint8_t IDL_SYNC_AIRCRAFT = 0xA6;
static const uint8_t IDL_SYNC_ROVER    = 0xA7;
static const uint8_t IDL_LEN_ASCENT    = 40;
static const uint8_t IDL_LEN_AIRCRAFT  = 44;
static const uint8_t IDL_LEN_ROVER     = 28;

// InfoDisp ScreenType values for the three consoles (KCMk1_InfoDisp.h)
static const uint8_t IDL_SCREEN_LNCHAP = 12;
static const uint8_t IDL_SCREEN_ACFTAP = 14;
static const uint8_t IDL_SCREEN_ROVRAP = 15;

// requestType nibble (I2C Protocol Specification §15.3)
static const uint8_t IDL_REQ_NOP     = 0x0;
static const uint8_t IDL_REQ_PROCEED = 0x2;

// Command opcodes (Ascent_Autopilot_Interface.md §4, Hold_Mode_Autopilot.md §8.1)
enum {
  IDL_CMD_SET_TARGET_ALT = 0x01, IDL_CMD_SET_INCLINATION = 0x02, IDL_CMD_SET_LAUNCH_DIR = 0x03,
  IDL_CMD_SET_LOFT = 0x04, IDL_CMD_SET_ROLL = 0x05, IDL_CMD_SET_MAXG = 0x06,
  IDL_CMD_ARM = 0x10, IDL_CMD_DISARM = 0x11,
  IDL_CMD_HOLD_AP_OFF = 0x12, IDL_CMD_HOLD_LVL = 0x13,
  IDL_CMD_ENGAGE_ATT = 0x20, IDL_CMD_ENGAGE_AOA, IDL_CMD_ENGAGE_VS, IDL_CMD_ENGAGE_ALT,
  IDL_CMD_ENGAGE_ROLL, IDL_CMD_ENGAGE_HDG, IDL_CMD_ENGAGE_IAS, IDL_CMD_ENGAGE_MACH,
  IDL_CMD_SET_ATT = 0x28, IDL_CMD_SET_AOA, IDL_CMD_SET_VS, IDL_CMD_SET_ALT,
  IDL_CMD_SET_HROLL, IDL_CMD_SET_HDG, IDL_CMD_SET_IAS, IDL_CMD_SET_MACH,
  IDL_CMD_ENGAGE_CRUISE = 0x30, IDL_CMD_ENGAGE_RHDG, IDL_CMD_ENGAGE_RTGT,
  IDL_CMD_SET_CRUISE = 0x33, IDL_CMD_SET_RHDG, IDL_CMD_SET_MAXSPD, IDL_CMD_SET_MAXSLOPE, IDL_CMD_SET_MAXROLL
};

static uint8_t  g_idlLastSeq    = 0;
static uint8_t  g_idlAckPending = 0;
static uint8_t  g_idlScreen     = 0xFF;
static bool     g_idlDemo       = false;
static uint32_t g_idlLastPollMs = 0, g_idlLastPushMs = 0;

static uint8_t idlControlByte(uint8_t reqType) {
  uint8_t b = (uint8_t)(reqType << 4);
  if (debug)    b |= 0x01;
  if (demo)     b |= 0x02;
  if (trimMode) b |= 0x04;
  if (idleMode) b |= 0x08;
  return b;
}

static void idlWrite(uint8_t addr, const uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(addr);
  Wire.write(buf, n);
  Wire.endTransmission();
}

static void idlSendControl(uint8_t addr, uint8_t reqType, uint8_t ackSeq) {
  uint8_t c[2] = { idlControlByte(reqType), ackSeq };
  idlWrite(addr, c, 2);
}

void idlInit() {
  // Release both display carriers from their boot hold.
  idlSendControl(INFO_MC,  IDL_REQ_PROCEED, 0);
  idlSendControl(INFO2_MC, IDL_REQ_PROCEED, 0);
  g_idlLastPollMs = g_idlLastPushMs = millis();
}

/***************************************************************************************
   Apply one console command
****************************************************************************************/
static void idlApply(uint8_t op, float v) {
  switch (op) {
    // ---- Ascent autopilot ----
    case IDL_CMD_SET_TARGET_ALT:  apSetTargetAltitude(v); break;
    case IDL_CMD_SET_INCLINATION: apSetTargetInclination(v); break;
    case IDL_CMD_SET_LAUNCH_DIR:  apSetLaunchSoutherly(v != 0.0f); break;
    case IDL_CMD_SET_LOFT:        apSetLoft(v); break;
    case IDL_CMD_SET_ROLL:        if (v >= 1.0e8f) apSetRoll(false, 0.0f); else apSetRoll(true, v); break;
    case IDL_CMD_SET_MAXG:        apSetMaxG(v); break;
    case IDL_CMD_ARM:             apArm(); break;
    case IDL_CMD_DISARM:          apDisarm(); break;
    // ---- Hold-mode autopilot ----
    case IDL_CMD_HOLD_AP_OFF:     hpDisconnectAll(HP_REASON_PILOT); break;
    case IDL_CMD_HOLD_LVL:        hpLevel(); break;
    case IDL_CMD_ENGAGE_ATT:      hpEngage(HP_MODE_ATT,    v != 0.0f); break;
    case IDL_CMD_ENGAGE_AOA:      hpEngage(HP_MODE_AOA,    v != 0.0f); break;
    case IDL_CMD_ENGAGE_VS:       hpEngage(HP_MODE_VS,     v != 0.0f); break;
    case IDL_CMD_ENGAGE_ALT:      hpEngage(HP_MODE_ALT,    v != 0.0f); break;
    case IDL_CMD_ENGAGE_ROLL:     hpEngage(HP_MODE_ROLL,   v != 0.0f); break;
    case IDL_CMD_ENGAGE_HDG:      hpEngage(HP_MODE_HDG,    v != 0.0f); break;
    case IDL_CMD_ENGAGE_IAS:      hpEngage(HP_MODE_IAS,    v != 0.0f); break;
    case IDL_CMD_ENGAGE_MACH:     hpEngage(HP_MODE_MACH,   v != 0.0f); break;
    case IDL_CMD_SET_ATT:         hpSetAtt(v); break;
    case IDL_CMD_SET_AOA:         hpSetAoa(v); break;
    case IDL_CMD_SET_VS:          hpSetVs(v); break;
    case IDL_CMD_SET_ALT:         hpSetAlt(v); break;
    case IDL_CMD_SET_HROLL:       hpSetRoll(v); break;
    case IDL_CMD_SET_HDG:         hpSetHdg(v); break;
    case IDL_CMD_SET_IAS:         hpSetIas(v); break;
    case IDL_CMD_SET_MACH:        hpSetMach(v); break;
    case IDL_CMD_ENGAGE_CRUISE:   hpEngage(HP_MODE_CRUISE, v != 0.0f); break;
    case IDL_CMD_ENGAGE_RHDG:     hpEngage(HP_MODE_RHDG,   v != 0.0f); break;
    case IDL_CMD_ENGAGE_RTGT:     hpEngage(HP_MODE_RTGT,   v != 0.0f); break;
    case IDL_CMD_SET_CRUISE:      hpSetCruise(v); break;
    case IDL_CMD_SET_RHDG:        hpSetRoverHdg(v); break;
    case IDL_CMD_SET_MAXSPD:      hpSetMaxSpeed(v); break;
    case IDL_CMD_SET_MAXSLOPE:    hpSetMaxSlope(v); break;
    case IDL_CMD_SET_MAXROLL:     hpSetMaxRoll(v); break;
    default: break;
  }
}

/***************************************************************************************
   Poll the console's outbound packet
****************************************************************************************/
static void idlPoll() {
  uint8_t buf[10];
  uint8_t got = 0;
  Wire.requestFrom((uint8_t)INFO2_MC, (uint8_t)10);
  while (Wire.available() && got < 10) buf[got++] = Wire.read();
  if (got < 10 || buf[0] != IDL_SYNC_OUT) return;

  g_idlDemo   = (buf[1] & 0x04) != 0;
  g_idlScreen = buf[2];

  uint8_t seq = buf[3];
  if (seq != 0 && seq != g_idlLastSeq) {
    uint8_t xs = 0;
    for (uint8_t i = 3; i < 9; i++) xs ^= buf[i];
    if (xs == buf[9]) {
      float payload;
      memcpy(&payload, &buf[5], 4);
      idlApply(buf[4], payload);
      g_idlLastSeq    = seq;
      g_idlAckPending = seq;
    }
  }
  if (g_idlAckPending) {
    idlSendControl(INFO2_MC, IDL_REQ_NOP, g_idlAckPending);
    g_idlAckPending = 0;
  }
}

/***************************************************************************************
   Status pushes
****************************************************************************************/
static void idlPutFloat(uint8_t *dst, float f) { memcpy(dst, &f, 4); }

static void idlPushAscent() {
  AscentStatus s = apGetStatus();
  AscentConfig &c = apGetConfig();
  uint8_t st[IDL_LEN_ASCENT] = {0};
  st[0] = IDL_SYNC_ASCENT;
  st[1] = (s.armed ? 0x01 : 0) | (c.launchSoutherly ? 0x02 : 0) | (c.rollControlEnabled ? 0x04 : 0);
  st[2] = (uint8_t)s.phase;
  float f[9] = { c.targetApoapsis, c.targetInclination, c.loft, c.targetRoll, c.maxG,
                 s.cmdPitch, s.cmdHeading, s.cmdThrottle, s.dynPressure };
  memcpy(&st[4], f, sizeof(f));
  idlWrite(INFO2_MC, st, IDL_LEN_ASCENT);
}

static void idlPushAircraft() {
  HoldStatus s = hpGetStatus();
  uint8_t st[IDL_LEN_AIRCRAFT] = {0};
  st[0] = IDL_SYNC_AIRCRAFT;
  st[1] = (s.anyEngaged ? 0x01 : 0) | (s.thrustEngaged ? 0x02 : 0) | (s.leverTouched ? 0x04 : 0) |
          (s.leverDriven ? 0x08 : 0) | (s.ascentArmed ? 0x10 : 0);
  st[2] = s.pitchMode; st[3] = s.latMode; st[4] = s.thrMode;
  st[5] = s.reason;    st[6] = s.reasonAge; st[7] = 0;
  float f[9] = { s.att, s.aoa, s.vs, s.alt, s.roll, s.hdg, s.ias, s.mach, s.cmdThrottle };
  memcpy(&st[8], f, sizeof(f));
  idlWrite(INFO2_MC, st, IDL_LEN_AIRCRAFT);
}

static void idlPushRover() {
  HoldStatus s = hpGetStatus();
  uint8_t st[IDL_LEN_ROVER] = {0};
  st[0] = IDL_SYNC_ROVER;
  st[1] = (s.cruise ? 0x01 : 0) | (s.rhdg ? 0x02 : 0) | (s.rtgt ? 0x04 : 0) | (s.brakes ? 0x08 : 0) |
          (s.slopeGuard ? 0x10 : 0) | (s.targetAvailable ? 0x20 : 0);
  st[2] = s.roverReason; st[3] = s.roverReasonAge;
  float f[6] = { s.cruiseSp, s.rhdgSp, s.maxSpeed, s.maxSlope, s.maxRoll, s.cmdWheel };
  memcpy(&st[4], f, sizeof(f));
  idlWrite(INFO2_MC, st, IDL_LEN_ROVER);
}

void idlService() {
  uint32_t now = millis();
  if (now - g_idlLastPollMs >= IDL_POLL_MS) { g_idlLastPollMs = now; idlPoll(); }
  if (now - g_idlLastPushMs >= IDL_PUSH_MS) {
    g_idlLastPushMs = now;
    if (g_idlDemo) return;                       // display drives its own status in demo
    switch (g_idlScreen) {
      case IDL_SCREEN_LNCHAP: idlPushAscent();   break;
      case IDL_SCREEN_ACFTAP: idlPushAircraft(); break;
      case IDL_SCREEN_ROVRAP: idlPushRover();    break;
      default: break;
    }
  }
}
