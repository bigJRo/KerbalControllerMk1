/***************************************************************************************
   annun_link.ino — Annunciator (0x10) and Resource Display (0x11) links for the master,
   and the GPWS Input Panel (0x2A) whose settings the Annunciator's GPWS function needs.

   Until this tab the master never wrote to either carrier, so both sat in their boot
   hold (setup() spins until PROCEED), and the Annunciator, had it been released, would
   have kept every master-owned field at its power-on default: audio off, GPWS off, the
   panel condition block dark, the control-mode tile and CTRLGRP frozen.

   Annunciator: the rev-3 9-byte command (I2C Protocol Specification §15.3,
   KCMk1_Annunciator/I2CSlave.ino), every ANL_PUSH_MS and at once when a field changes:
     0    controlByte  request type (bits 7:4) | idle 0x08 | audio 0x04 | demo 0x02 | debug 0x01
     1    ctrlMode     0 Rover, 1 Plane, 2 Spacecraft (the Annunciator's CtrlMode enum)
     2    ctrlGrp      active control group, 1..6
     3-4  modeFlags    MF_* bits, low byte first (KCMk1_Annunciator.h)
     5    capValue     reserved readout; no source yet, sent as 0
     6    GPWS config  the GPWS panel's state byte as it reports it: bits 1:0 mode
                       (0 OFF, 1 ACTIVE, 2 PROX), bit 2 proxAlarm, bit 3 rdvRadar
     7-8  GPWS bug     the panel's display value, int16 big-endian metres
   Resource Display: its 2-byte command (controlByte, reserved) on the same cadence.

   The request type on every periodic write is PROCEED. A carrier treats it as "leave
   the boot hold" and nothing else, so repeating it is harmless, and it releases a
   carrier that powers up after the master or reboots on its own, which a single
   PROCEED at init would leave hanging.

   GPWS Input Panel: a display module (Kerbal7SegmentCore, §9.3, 8-byte packet). It is
   read only when it asserts INT (a read without one returns a zeroed header). A
   BOOT_READY or DISABLED report is answered with CMD_DISABLE / CMD_ENABLE so the panel
   runs; the state byte and display value of every packet are kept and relayed.

   Mode-flag sources. Bits with no master source yet are sent as 0 and stay dark on the
   Annunciator: WARP and SIMPIT LOST (the master's Simpit link is not integrated) and
   SWITCH ERR (nothing detects one).
****************************************************************************************/
#include "control_links.h"
#include "ascent_autopilot.h"
#include "hold_autopilot.h"
#include "burn_autopilot.h"
#include "landing_autopilot.h"

static const uint32_t ANL_PUSH_MS = 100;

static const uint8_t ANL_REQ_PROCEED = 0x2;   // requestType nibble (§15.3)
static const uint8_t ANL_CMD_LEN     = 9;     // Annunciator rev-3 command
static const uint8_t ANL_RES_CMD_LEN = 2;     // Resource Display command

// Annunciator MF_* bit positions (KCMk1_Annunciator.h)
enum {
  ANL_MF_DEMO = 0, ANL_MF_WARP, ANL_MF_AUDIO, ANL_MF_THRTL_ENA, ANL_MF_TRIM, ANL_MF_AUTOPILOT,
  ANL_MF_DEBUG, ANL_MF_SWITCH_ERR, ANL_MF_SIMPIT_LOST, ANL_MF_THRTL_PREC, ANL_MF_INPUT_PREC,
  ANL_MF_ENG_ARM
};

static const int16_t ANL_GPWS_DEFAULT_M = 200;   // the panel's power-on / reset value

static uint8_t  g_anlGpwsCfg   = 0;               // OFF until the panel reports
static int16_t  g_anlGpwsThr   = ANL_GPWS_DEFAULT_M;
static uint8_t  g_anlLast[ANL_CMD_LEN];
static bool     g_anlSent      = false;
static uint32_t g_anlLastPushMs = 0;

static void anlWrite(uint8_t addr, const uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(addr);
  Wire.write(buf, n);
  Wire.endTransmission();
}

static uint8_t anlControlByte() {
  uint8_t b = (uint8_t)(ANL_REQ_PROCEED << 4);
  if (debug)    b |= 0x01;
  if (demo)     b |= 0x02;
  if (audioEn)  b |= 0x04;
  if (idleMode) b |= 0x08;
  return b;
}

static uint16_t anlModeFlags() {
  uint16_t f = 0;
  if (demo)                                f |= (1u << ANL_MF_DEMO);
  if (audioEn)                             f |= (1u << ANL_MF_AUDIO);
  if (throttleEn)                          f |= (1u << ANL_MF_THRTL_ENA);
  if (trimMode)                            f |= (1u << ANL_MF_TRIM);
  if (apIsArmed() || hpAnyEngaged() || bpAnyEngaged() || lpAnyEngaged())
                                           f |= (1u << ANL_MF_AUTOPILOT);
  if (debug)                               f |= (1u << ANL_MF_DEBUG);
  if (thrPrecision())                      f |= (1u << ANL_MF_THRTL_PREC);
  if (precisionEn)                         f |= (1u << ANL_MF_INPUT_PREC);
  if (thrModuleEnabled())                  f |= (1u << ANL_MF_ENG_ARM);
  return f;
}

// GPWS Input Panel: read on INT, bring it up from BOOT_READY / DISABLED, keep its state.
static void anlPollGpws() {
  if (digitalRead(GPWS_INT) != LOW) return;
  uint8_t pkt[KMC_DISPLAY_PACKET_SIZE];
  uint8_t got = 0;
  Wire.requestFrom((uint8_t)GPWS_MOD, (uint8_t)KMC_DISPLAY_PACKET_SIZE);
  while (Wire.available() && got < KMC_DISPLAY_PACKET_SIZE) pkt[got++] = Wire.read();
  if (got < KMC_DISPLAY_PACKET_SIZE) return;
  if (pkt[KMC_PKT_TYPEID_OFFSET] != KMC_TYPE_GPWS_INPUT) return;

  uint8_t life = moduleLifecycle(pkt[KMC_PKT_STATUS_OFFSET]);
  if (life == KMC_STATUS_BOOT_READY || life == KMC_STATUS_DISABLED) {
    // §5 init: BOOT_READY is answered with CMD_DISABLE, then CMD_ENABLE runs the panel.
    // The boot packet carries no payload, so its bytes are not used.
    uint8_t c = KMC_CMD_DISABLE;
    if (life == KMC_STATUS_BOOT_READY) anlWrite(GPWS_MOD, &c, 1);
    c = KMC_CMD_ENABLE;
    anlWrite(GPWS_MOD, &c, 1);
    return;
  }
  g_anlGpwsCfg = pkt[KMC_PKT_PAYLOAD_OFFSET + 2] & 0x0F;
  g_anlGpwsThr = (int16_t)(((uint16_t)pkt[KMC_PKT_PAYLOAD_OFFSET + 3] << 8) |
                           pkt[KMC_PKT_PAYLOAD_OFFSET + 4]);
}

void anlInit() {
  pinMode(GPWS_INT, INPUT);
  g_anlSent = false;
  g_anlLastPushMs = millis() - ANL_PUSH_MS;   // first service pushes at once
}

void anlService() {
  anlPollGpws();

  uint8_t grp = (ctrlGrp >= 1 && ctrlGrp <= 6) ? ctrlGrp : 1;
  uint8_t mode = (ctrlMode <= ANL_CTRL_SPACECRAFT) ? ctrlMode : ANL_CTRL_SPACECRAFT;
  uint16_t mf = anlModeFlags();
  uint8_t cmd[ANL_CMD_LEN] = {
    anlControlByte(), mode, grp,
    (uint8_t)(mf & 0xFF), (uint8_t)(mf >> 8),
    0,                                              // capValue: reserved
    g_anlGpwsCfg,
    (uint8_t)((uint16_t)g_anlGpwsThr >> 8), (uint8_t)((uint16_t)g_anlGpwsThr & 0xFF)
  };

  uint32_t now = millis();
  bool changed = !g_anlSent || memcmp(cmd, g_anlLast, ANL_CMD_LEN) != 0;
  if (!changed && (now - g_anlLastPushMs) < ANL_PUSH_MS) return;

  anlWrite(ANNUN_MC, cmd, ANL_CMD_LEN);
  // Same control byte; bit 2 (audio) has no meaning on the Resource Display, so it is cleared.
  uint8_t res[ANL_RES_CMD_LEN] = { (uint8_t)(cmd[0] & ~0x04), 0 };
  anlWrite(RES_MC, res, ANL_RES_CMD_LEN);

  memcpy(g_anlLast, cmd, ANL_CMD_LEN);
  g_anlSent = true;
  g_anlLastPushMs = now;
}
