/***************************************************************************************
   I2CSlave.ino -- I2C slave interface for KCMk1 Info Display
   Exposes Info Display state to the KCMk1 master (Teensy 4.1) over I2C.

   Hardware:
     I2C bus    : Wire2 (pins 24/25 on Teensy 4.1) — KCM_I2C_BUS
     Slave addr : 0x12 (Info Display 1) or 0x13 (Info Display 2), selected at
                  compile time by INFO_DISP_UNIT in KCMk1_InfoDisp.h. Same firmware
                  image for both boards; only this address differs.
     INT pin    : pin 0 (KCM_I2C_INT_PIN), OUTPUT, active-LOW
                  InfoDisp asserts LOW when a fresh packet is ready.
                  Master reads via KCM_I2C_BUS.requestFrom(I2C_SLAVE_ADDR, I2C_PACKET_SIZE).
                  Pin returns HIGH after the onRequest handler fires.

   Outbound packet (InfoDisp -> Master), I2C_PACKET_SIZE = 10 bytes:
     Byte 0  : 0xAE  -- sync/magic byte for framing validation (was 0xAD; collision fix)
     Byte 1  : flags
                 bit 0 = simpitConnected
                 bit 1 = flightScene
                 bit 2 = demoMode
                 bits 3-7 reserved (0)
     Byte 2  : activeScreen  -- current ScreenType enum value
     Byte 3  : apCmdSeq   -- Ascent-AP command sequence (0 = no command pending;
                            otherwise 1..255, unique per queued command). The master
                            executes the command in bytes 4-8 exactly once, then echoes
                            this value back in the inbound ackSeq byte to pop the queue.
     Byte 4  : apCmdOp    -- Ascent-AP command opcode (see AP_CMD_* in Screen_LNCH_AscentAP)
     Bytes 5-8 : apCmdPayload -- IEEE-754 float32, little-endian (command argument)
     Byte 9  : xsum       -- XOR of bytes 3..8 (command-frame integrity)

   Inbound command (Master -> InfoDisp), I2C_CMD_SIZE = 2 bytes:
     Byte 0  : controlByte
                 bits 7:4 = requestType
                   0x0 = NOP           -- no operation
                   0x1 = STATUS        -- request immediate status packet (asserts INT)
                   0x2 = PROCEED       -- proceed to main loop (release boot hold)
                   0x3 = MCU_RESET     -- soft reboot the InfoDisp
                   0x4 = DISPLAY_RESET -- reset display state and redraw current screen
                 bit  3   = idle_state  (1 = switch to standby when not in flight)
                 bit  2   = trimEnabled (1 = trim hold engaged; shows "TRIM" on SCFT/ACFT)
                 bit  1   = demoMode    (1 = enable demo mode)
                 bit  0   = debugMode   (1 = enable Serial debug output)
     Byte 1  : ackSeq (0x00 = none) -- Ascent-AP command acknowledgement. The master sets
                                       this to the apCmdSeq it just executed; InfoDisp pops
                                       that command from its queue. 0 leaves the queue as-is.

   Inbound Ascent-AP status (Master -> InfoDisp), I2C_AP_STATUS_SIZE = 40 bytes:
     Master pushes the autopilot's AscentStatus so InfoDisp can render live guidance and
     confirm accepted parameters (clearing the pilot's pending "cyan" edits). Dispatched
     by sync byte in processStatusPush(); see processApStatus() for the field layout.
     Byte 0  : 0xA5 sync
     Byte 1  : flags (bit0 armed, bit1 southerly, bit2 rollEnable)
     Byte 2  : phase (0 IDLE .. 6 ABORT)
     Byte 3  : reserved (0x00)
     Bytes 4-39 : 9 IEEE-754 float32 LE — targetAlt, inclination, loft, rollDeg, maxG,
                  cmdPitch, cmdHeading, cmdThrottle, dynPressure

   Inbound autopilot status pushes (Master -> InfoDisp) are DISPATCHED BY SYNC BYTE
   (Mission_Autopilot.md §8.1): any write of 3 bytes or more lands in one buffer and the
   main thread looks at byte 0. Lengths are fixed per frame but need not be unique.
     0xA5  40 bytes  ascent     (Ascent_Autopilot_Interface.md §5)
     0xA6  48 bytes  aircraft   flags, pitch/lat/thr modes, reason, reasonAge, 10 floats
     0xA7  36 bytes  rover      flags, reason, reasonAge, 8 floats
     0xA8  52 bytes  orbital    flags, mode, phase, reason, reasonAge, 11 floats
     0xA9  44 bytes  landing    flags, mode, entry, reason, reasonAge, accelSource, 9 floats
   The master pushes only the frame the console on screen needs (activeScreen, byte 2 of
   the outbound packet). The 2-byte control write keeps its own path.

   Expanding the protocol:
     Outbound: increment I2C_PACKET_SIZE, add fields to fillI2CPacketBuffer().
     Inbound:  add a new packet length + branch in onI2CReceive(), process in loop.
     Update master sketch to match in both cases.
****************************************************************************************/
#include "KCMk1_InfoDisp.h"

// Info Display unit -> I2C slave address. INFO_DISP_UNIT is set per board in
// KCMk1_InfoDisp.h (1 = Info Display 1, 2 = Info Display 2), which also validates it.
// Same firmware image; the address differs so the master can talk to each board on
// the shared bus, and the unit additionally selects the panel's role — see the header.
#if INFO_DISP_IS_PFD_UNIT
#define I2C_SLAVE_ADDR   KCM_I2C_ADDR_INFODISP      // 0x12 (Info Display 1)
#else
#define I2C_SLAVE_ADDR   KCM_I2C_ADDR_INFODISP_2    // 0x13 (Info Display 2)
#endif
#define I2C_INT_PIN      KCM_I2C_INT_PIN             // #3C from SystemConfig
#define I2C_PACKET_SIZE  10     // outbound: InfoDisp -> Master (panel-specific; 3 status + AP cmd frame)
#define I2C_CMD_SIZE     2      // inbound:  Master -> InfoDisp control/ack (panel-specific)
#define I2C_AP_STATUS_SIZE 40   // inbound:  Master -> InfoDisp Ascent-AP status push
#define I2C_AP_STATUS_SYNC 0xA5 // framing byte for the AP status push
#define I2C_HA_STATUS_SIZE 48   // inbound:  Master -> InfoDisp aircraft hold-mode status push
#define I2C_HA_STATUS_SYNC 0xA6
#define I2C_RA_STATUS_SIZE 36   // inbound:  Master -> InfoDisp rover hold-mode status push
#define I2C_RA_STATUS_SYNC 0xA7
#define I2C_OB_STATUS_SIZE 52   // inbound:  Master -> InfoDisp orbital autopilot status push
#define I2C_OB_STATUS_SYNC 0xA8
#define I2C_LD_STATUS_SIZE 44   // inbound:  Master -> InfoDisp landing autopilot status push
#define I2C_LD_STATUS_SYNC 0xA9
#define I2C_PUSH_MAX       64   // one buffer for every status push; dispatched by sync byte
#define I2C_SYNC_BYTE    KCM_I2C_SYNC_INFODISP      // #3C from SystemConfig (0xAE, collision fix)

// requestType values (bits 7:4 of controlByte)
#define I2C_REQ_NOP           0x0
#define I2C_REQ_STATUS        0x1
#define I2C_REQ_PROCEED       0x2
#define I2C_REQ_MCU_RESET     0x3
#define I2C_REQ_DISPLAY_RESET 0x4


/***************************************************************************************
   PACKET BUFFER
   Built by buildI2CPacket() and consumed by onI2CRequest().
   Declared volatile because it is written on the main thread and read
   from the Wire interrupt context.
****************************************************************************************/
static volatile uint8_t i2cPacket[I2C_PACKET_SIZE];
static volatile bool i2cPacketReady = false;

// Set to true when the master sends I2C_REQ_PROCEED. setup() spins on this
// flag after initialisation before entering loop().
volatile bool i2cProceedReceived = false;


/***************************************************************************************
   PACKET FILL HELPER (#21)
   Writes current state into any I2C_PACKET_SIZE (10-byte) buffer. Used by both buildI2CPacket()
   and the change-detection path in updateI2CState() to avoid duplicated assembly.
****************************************************************************************/
static void fillI2CPacketBuffer(uint8_t *buf) {
  uint8_t flags = 0;
  if (simpitConnected) flags |= (1 << 0);
  if (flightScene)     flags |= (1 << 1);
  if (demoMode)        flags |= (1 << 2);
  buf[0] = I2C_SYNC_BYTE;
  buf[1] = flags;
  buf[2] = (uint8_t)activeScreen;
  apFillOutboundCmd(&buf[3]);            // bytes 3..8 : [seq][op][payload float LE]
  uint8_t xsum = 0;
  for (uint8_t i = 3; i < 9; i++) xsum ^= buf[i];
  buf[9] = xsum;                         // command-frame integrity check
}

/***************************************************************************************
   BUILD PACKET
   Thin wrapper around fillI2CPacketBuffer() — writes into the live packet buffer.
****************************************************************************************/
static void buildI2CPacket() {
  fillI2CPacketBuffer((uint8_t *)i2cPacket);
}


/***************************************************************************************
   PROCESS I2C COMMAND
   Called from loop() to apply a received command packet.
   Runs on the main thread -- safe to modify state, globals, and call Serial.
   Takes a snapshot of the command, copied out of the ISR buffer under noInterrupts by
   updateI2CState(), so a command arriving mid-read cannot be torn.
****************************************************************************************/
static volatile uint8_t i2cCmdBuf[I2C_CMD_SIZE];
static volatile bool i2cCmdReady = false;

// Inbound autopilot status push (Master -> InfoDisp), any console. Filled in the Wire
// ISR, snapshotted and dispatched on its sync byte by the main thread.
static volatile uint8_t pushBuf[I2C_PUSH_MAX];
static volatile uint8_t pushLen = 0;
static volatile bool    pushReady = false;

static void processI2CCommand(const uint8_t *cmd) {
  uint8_t controlByte = cmd[0];

  // --- Ascent-AP command acknowledgement (byte 1) ---
  // Master echoes the apCmdSeq it just executed; pop that command from the AP queue.
  apAckCommand(cmd[1]);

  // --- Lower nibble: mode configuration bits ---
  bool newDebug = (controlByte >> 0) & 1;
  bool newDemo  = (controlByte >> 1) & 1;
  bool newTrim  = (controlByte >> 2) & 1;   // trim-hold enabled (SCFT/ACFT "TRIM" annunciation)
  bool newIdle  = (controlByte >> 3) & 1;
  state.trimEnabled = newTrim;

  if (newDebug != debugMode) {
    debugMode = newDebug;
    setKDCDebugMode(debugMode);
    if (debugMode) Serial.println(F("InfoDisp: I2C -- debugMode on"));
  }

  if (newDemo != demoMode) {
    demoMode = newDemo;
    if (demoMode) {
      initDemoMode();
      if (debugMode) Serial.println(F("InfoDisp: I2C -- demoMode on"));
    } else {
      if (debugMode) Serial.println(F("InfoDisp: I2C -- demoMode off"));
      // #24 runtime demo->live: connect Simpit if not already connected
      if (!simpitConnected) {
        initSimpit();
      } else {
        simpit.requestMessageOnChannel(0);
      }
      // Standby splash drawn by loop() _wasDemo transition block
    }
  }

  if (newIdle != idleState) {
    idleState = newIdle;
    if (idleState && !flightScene) {
      drawStandbyScreen(infoDisp);   // #4 show standby immediately when idle asserted
    }
    if (debugMode) {
      Serial.print(F("InfoDisp: I2C -- idleState="));
      Serial.println(idleState);
    }
  }

  // --- Upper nibble: requestType ---
  uint8_t reqType = (controlByte >> 4) & 0x0F;
  switch (reqType) {

    case I2C_REQ_NOP:
      break;

    case I2C_REQ_STATUS:
      publishI2CPacket();
      if (debugMode) Serial.println(F("InfoDisp: I2C -- status requested"));
      break;

    case I2C_REQ_PROCEED:
      i2cProceedReceived = true;
      if (debugMode) Serial.println(F("InfoDisp: I2C -- proceed"));
      break;

    case I2C_REQ_MCU_RESET:
      if (debugMode) {
        Serial.println(F("InfoDisp: I2C -- MCU reset"));
        Serial.flush();
      }
      disconnectUSB();
      executeReboot();
      break;

    case I2C_REQ_DISPLAY_RESET:
      // switchToScreen() only — unlike the Annunciator, InfoDisp has no per-flight
      // boolean flags that need clearing on a display-only reset.
      if (debugMode) Serial.println(F("InfoDisp: I2C -- display reset"));
      switchToScreen(activeScreen);
      break;

    default:
      if (debugMode) {
        Serial.print(F("InfoDisp: I2C -- unknown reqType 0x"));
        Serial.println(reqType, HEX);
      }
      break;
  }

  if (debugMode) {
    Serial.print(F("InfoDisp: I2C cmd ctrl=0x"));
    Serial.println(controlByte, HEX);
  }
}


/***************************************************************************************
   PUBLISH PACKET
   Assembles the current state into a scratch buffer, then copies it into the live
   packet under noInterrupts() and asserts INT. The guarded copy is what keeps
   onI2CRequest() from transmitting a half-written packet: bytes 0-2 sit outside the
   byte-9 command checksum, so a torn packet would pass the master's framing check.
   Every path that raises INT goes through here.
****************************************************************************************/
static void publishI2CPacket() {
  uint8_t candidate[I2C_PACKET_SIZE];
  fillI2CPacketBuffer(candidate);
  noInterrupts();
  memcpy((uint8_t *)i2cPacket, candidate, I2C_PACKET_SIZE);
  i2cPacketReady = true;
  interrupts();
  digitalWriteFast(I2C_INT_PIN, LOW);
}

/***************************************************************************************
   BUILD PACKET AND ASSERT INT
   Public helper called from setup() after initialisation is complete.
****************************************************************************************/
void buildI2CPacketAndAssert() {
  publishI2CPacket();
  if (debugMode) Serial.println(F("InfoDisp: I2C -- init packet ready, asserting INT"));
}


/***************************************************************************************
   ON RECEIVE HANDLER -- interrupt context, keep short
****************************************************************************************/
static void onI2CReceive(int numBytes) {
  if (numBytes == I2C_CMD_SIZE) {
    for (int i = 0; i < I2C_CMD_SIZE; i++) {
      i2cCmdBuf[i] = KCM_I2C_BUS.read();
    }
    i2cCmdReady = true;
  } else if (numBytes >= 3 && numBytes <= I2C_PUSH_MAX) {
    for (int i = 0; i < numBytes; i++) {
      pushBuf[i] = KCM_I2C_BUS.read();
    }
    pushLen = (uint8_t)numBytes;
    pushReady = true;
  } else {
    while (KCM_I2C_BUS.available()) KCM_I2C_BUS.read();
  }
}


/***************************************************************************************
   PROCESS ASCENT-AP STATUS
   Applies a received AscentStatus push into the shared `state` struct so the Ascent
   Autopilot screen renders live guidance and confirms accepted parameters. Runs on the
   main thread. Skipped in demo mode (demo drives state.ap* locally).
****************************************************************************************/
static void processApStatus(const uint8_t *buf) {
  if (buf[0] != I2C_AP_STATUS_SYNC) return;   // framing guard

  uint8_t fl = buf[1];
  state.apArmed      = (fl & 0x01) != 0;
  state.apSoutherly  = (fl & 0x02) != 0;
  state.apRollEnable = (fl & 0x04) != 0;
  state.apPhase      = buf[2];

  float f[9];
  memcpy(f, &buf[4], sizeof(f));   // 9 floats, bytes 4..39
  state.apTargetAlt   = f[0];
  state.apInclination = f[1];
  state.apLoft        = f[2];
  state.apRollDeg     = f[3];
  state.apMaxG        = f[4];
  state.apCmdPitch    = f[5];
  state.apCmdHeading  = f[6];
  state.apCmdThrottle = f[7];
  state.apDynPressure = f[8];
}


/***************************************************************************************
   PROCESS HOLD-MODE STATUS (aircraft / rover)
   Applies the master's HoldStatus pushes into `state` so the AIRCRAFT AUTOPILOT and
   ROVER AUTOPILOT consoles render modes, setpoint echoes and disconnect reasons.
   Main thread; skipped in demo mode (Demo.ino drives state.hp* / state.rv* locally).
****************************************************************************************/
static void processHaStatus(const uint8_t *buf) {
  state.hpFlags     = buf[1];
  state.hpPitchMode = buf[2];
  state.hpLatMode   = buf[3];
  state.hpThrMode   = buf[4];
  state.hpReason    = buf[5];
  state.hpReasonAge = buf[6];
  float f[10];
  memcpy(f, &buf[8], sizeof(f));   // 10 floats, bytes 8..47
  state.hpAtt = f[0]; state.hpAoa = f[1]; state.hpVs = f[2]; state.hpAlt = f[3];
  state.hpRoll = f[4]; state.hpHdg = f[5]; state.hpIas = f[6]; state.hpMach = f[7];
  state.hpGs = f[8]; state.hpCmdThrottle = f[9];
}

static void processRaStatus(const uint8_t *buf) {
  state.rvFlags     = buf[1];
  state.rvReason    = buf[2];
  state.rvReasonAge = buf[3];
  float f[8];
  memcpy(f, &buf[4], sizeof(f));   // 8 floats, bytes 4..35
  state.rvCruise = f[0]; state.rvHdg = f[1]; state.rvMaxSpeed = f[2];
  state.rvMaxSlope = f[3]; state.rvMaxRoll = f[4]; state.rvFollowRange = f[5];
  state.rvStopDist = f[6]; state.rvCmdWheel = f[7];
}

static void processObStatus(const uint8_t *buf) {
  state.obFlags = buf[1]; state.obMode = buf[2]; state.obPhase = buf[3];
  state.obReason = buf[4]; state.obReasonAge = buf[5];
  float f[11];
  memcpy(f, &buf[8], sizeof(f));   // 11 floats, bytes 8..51
  state.obTargetAp = f[0]; state.obTargetPe = f[1]; state.obTargetInc = f[2];
  state.obApprRate = f[3]; state.obApprDist = f[4]; state.obDvTotal = f[5]; state.obDvRemaining = f[6];
  state.obTIgnition = f[7]; state.obBurnDuration = f[8]; state.obAccelEst = f[9]; state.obCmdThrottle = f[10];
}

static void processLdStatus(const uint8_t *buf) {
  state.ldFlags = buf[1]; state.ldMode = buf[2]; state.ldEntry = buf[3];
  state.ldReason = buf[4]; state.ldReasonAge = buf[5]; state.ldAccelSource = buf[6];
  float f[9];
  memcpy(f, &buf[8], sizeof(f));   // 9 floats, bytes 8..43
  state.ldDescRate = f[0]; state.ldHovrAlt = f[1]; state.ldTwr = f[2]; state.ldMargin = f[3];
  state.ldEntryAoa = f[4]; state.ldEntryRoll = f[5]; state.ldIgnAlt = f[6]; state.ldAccelEst = f[7]; state.ldCmdThrottle = f[8];
}

// Dispatch a status push on its sync byte; a length mismatch is a framing error and is dropped.
static void processStatusPush(const uint8_t *buf, uint8_t len) {
  switch (buf[0]) {
    case I2C_AP_STATUS_SYNC: if (len == I2C_AP_STATUS_SIZE) processApStatus(buf); break;
    case I2C_HA_STATUS_SYNC: if (len == I2C_HA_STATUS_SIZE) processHaStatus(buf); break;
    case I2C_RA_STATUS_SYNC: if (len == I2C_RA_STATUS_SIZE) processRaStatus(buf); break;
    case I2C_OB_STATUS_SYNC: if (len == I2C_OB_STATUS_SIZE) processObStatus(buf); break;
    case I2C_LD_STATUS_SYNC: if (len == I2C_LD_STATUS_SIZE) processLdStatus(buf); break;
    default: break;
  }
}


/***************************************************************************************
   ON REQUEST HANDLER -- interrupt context, keep short
****************************************************************************************/
static void onI2CRequest() {
  KCM_I2C_BUS.write((uint8_t *)i2cPacket, I2C_PACKET_SIZE);
  digitalWriteFast(I2C_INT_PIN, HIGH);
  i2cPacketReady = false;
}


/***************************************************************************************
   SETUP I2C SLAVE
   Call from setup() after display and touch are initialised.
****************************************************************************************/
void setupI2CSlave() {
  pinMode(I2C_INT_PIN, OUTPUT);
  digitalWriteFast(I2C_INT_PIN, HIGH);   // idle high

  KCM_I2C_BUS.begin(I2C_SLAVE_ADDR);
  KCM_I2C_BUS.onRequest(onI2CRequest);
  KCM_I2C_BUS.onReceive(onI2CReceive);

  buildI2CPacket();

  if (debugMode) {
    Serial.print(F("InfoDisp: I2C slave ready at 0x"));
    Serial.println(I2C_SLAVE_ADDR, HEX);
  }
}


/***************************************************************************************
   UPDATE I2C STATE
   Call from loop(). Applies pending inbound commands and detects outbound
   state changes, asserting INT when a fresh packet is ready.

   Both inbound buffers are snapshotted under noInterrupts() before use. The Wire ISR
   can land a new write at any point of a main-thread read; the 40-byte status push
   in particular would otherwise be read half old, half new, and its floats torn.
****************************************************************************************/
void updateI2CState() {
  uint8_t cmd[I2C_CMD_SIZE];
  uint8_t push[I2C_PUSH_MAX];
  uint8_t pushN = 0;
  bool haveCmd = false, havePush = false;
  noInterrupts();
  if (i2cCmdReady) {
    for (uint8_t i = 0; i < I2C_CMD_SIZE; i++) cmd[i] = i2cCmdBuf[i];
    i2cCmdReady = false;
    haveCmd = true;
  }
  if (pushReady) {
    pushN = pushLen;
    for (uint8_t i = 0; i < pushN; i++) push[i] = pushBuf[i];
    pushReady = false;
    havePush = true;
  }
  interrupts();
  if (haveCmd) processI2CCommand(cmd);

  // --- Apply inbound Ascent-AP status push (live mode only) ---
  if (havePush && !demoMode) processStatusPush(push, pushN);

  // --- Console command queue: expose next command, retire confirmed edits ---
  apPumpCommandQueue();
  apReconcilePending();    // ascent console
  hpReconcilePending();    // aircraft + rover consoles

  // --- Detect outbound state changes (#21) ---
  if (!i2cPacketReady) {
    uint8_t candidate[I2C_PACKET_SIZE];
    fillI2CPacketBuffer(candidate);
    if (memcmp((uint8_t *)i2cPacket, candidate, I2C_PACKET_SIZE) != 0) {
      publishI2CPacket();
      if (debugMode) Serial.println(F("InfoDisp: I2C packet ready"));
    }
  }
}
