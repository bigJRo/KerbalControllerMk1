/***************************************************************************************
   I2CSlave.ino -- I2C slave interface for KCMk1 Info Display
   Exposes Info Display state to the KCMk1 master (Teensy 4.1) over I2C.

   Hardware:
     I2C bus    : Wire (pins 18/19 on Teensy 4.0)
     Slave addr : 0x12
     INT pin    : pin 2, OUTPUT, active-LOW
                  InfoDisp asserts LOW when a fresh packet is ready.
                  Master reads via KCM_I2C_BUS.requestFrom(0x12, I2C_PACKET_SIZE).
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
                 bit  1   = demoMode    (1 = enable demo mode)
                 bit  0   = debugMode   (1 = enable Serial debug output)
     Byte 1  : ackSeq (0x00 = none) -- Ascent-AP command acknowledgement. The master sets
                                       this to the apCmdSeq it just executed; InfoDisp pops
                                       that command from its queue. 0 leaves the queue as-is.

   Inbound Ascent-AP status (Master -> InfoDisp), I2C_AP_STATUS_SIZE = 40 bytes:
     Master pushes the autopilot's AscentStatus so InfoDisp can render live guidance and
     confirm accepted parameters (clearing the pilot's pending "cyan" edits). Dispatched
     by write length in onI2CReceive(); see processApStatus() for the field layout.
     Byte 0  : 0xA5 sync
     Byte 1  : flags (bit0 armed, bit1 southerly, bit2 rollEnable)
     Byte 2  : phase (0 IDLE .. 6 ABORT)
     Byte 3  : reserved (0x00)
     Bytes 4-39 : 9 IEEE-754 float32 LE — targetAlt, inclination, loft, rollDeg, maxG,
                  cmdPitch, cmdHeading, cmdThrottle, dynPressure

   Expanding the protocol:
     Outbound: increment I2C_PACKET_SIZE, add fields to fillI2CPacketBuffer().
     Inbound:  add a new packet length + branch in onI2CReceive(), process in loop.
     Update master sketch to match in both cases.
****************************************************************************************/
#include "KCMk1_InfoDisp.h"

#define I2C_SLAVE_ADDR   KCM_I2C_ADDR_INFODISP      // #3C from SystemConfig
#define I2C_INT_PIN      KCM_I2C_INT_PIN             // #3C from SystemConfig
#define I2C_PACKET_SIZE  10     // outbound: InfoDisp -> Master (panel-specific; 3 status + AP cmd frame)
#define I2C_CMD_SIZE     2      // inbound:  Master -> InfoDisp control/ack (panel-specific)
#define I2C_AP_STATUS_SIZE 40   // inbound:  Master -> InfoDisp Ascent-AP status push
#define I2C_AP_STATUS_SYNC 0xA5 // framing byte for the AP status push
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
   Writes current state into any 4-byte buffer. Used by both buildI2CPacket()
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
****************************************************************************************/
static volatile uint8_t i2cCmdBuf[I2C_CMD_SIZE];
static volatile bool i2cCmdReady = false;

// Inbound Ascent-AP status push (Master -> InfoDisp). Filled in the Wire ISR,
// applied on the main thread by processApStatus().
static volatile uint8_t apStatusBuf[I2C_AP_STATUS_SIZE];
static volatile bool     apStatusReady = false;

static void processI2CCommand() {
  uint8_t controlByte = i2cCmdBuf[0];

  // --- Ascent-AP command acknowledgement (byte 1) ---
  // Master echoes the apCmdSeq it just executed; pop that command from the AP queue.
  apAckCommand(i2cCmdBuf[1]);

  // --- Lower nibble: mode configuration bits ---
  bool newDebug = (controlByte >> 0) & 1;
  bool newDemo  = (controlByte >> 1) & 1;
  bool newIdle  = (controlByte >> 3) & 1;

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
      buildI2CPacket();
      i2cPacketReady = true;
      digitalWriteFast(I2C_INT_PIN, LOW);
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
   BUILD PACKET AND ASSERT INT
   Public helper called from setup() after initialisation is complete.
****************************************************************************************/
void buildI2CPacketAndAssert() {
  buildI2CPacket();
  i2cPacketReady = true;
  digitalWriteFast(I2C_INT_PIN, LOW);
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
  } else if (numBytes == I2C_AP_STATUS_SIZE) {
    for (int i = 0; i < I2C_AP_STATUS_SIZE; i++) {
      apStatusBuf[i] = KCM_I2C_BUS.read();
    }
    apStatusReady = true;
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
static void processApStatus() {
  if (apStatusBuf[0] != I2C_AP_STATUS_SYNC) return;   // framing guard

  uint8_t fl = apStatusBuf[1];
  state.apArmed      = (fl & 0x01) != 0;
  state.apSoutherly  = (fl & 0x02) != 0;
  state.apRollEnable = (fl & 0x04) != 0;
  state.apPhase      = apStatusBuf[2];

  float f[9];
  memcpy(f, (const void *)&apStatusBuf[4], sizeof(f));   // 9 floats, bytes 4..39
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

  if (debugMode) Serial.println(F("InfoDisp: I2C slave ready at 0x12"));
}


/***************************************************************************************
   UPDATE I2C STATE
   Call from loop(). Applies pending inbound commands and detects outbound
   state changes, asserting INT when a fresh packet is ready.
****************************************************************************************/
void updateI2CState() {
  if (i2cCmdReady) {
    i2cCmdReady = false;
    processI2CCommand();
  }

  // --- Apply inbound Ascent-AP status push (live mode only) ---
  if (apStatusReady) {
    apStatusReady = false;
    if (!demoMode) processApStatus();
  }

  // --- Ascent-AP outbound command queue: expose next command, retire confirmed edits ---
  apPumpCommandQueue();
  apReconcilePending();

  // --- Detect outbound state changes (#21) ---
  if (!i2cPacketReady) {
    uint8_t candidate[I2C_PACKET_SIZE];
    fillI2CPacketBuffer(candidate);
    if (memcmp((uint8_t *)i2cPacket, candidate, I2C_PACKET_SIZE) != 0) {
      memcpy((uint8_t *)i2cPacket, candidate, I2C_PACKET_SIZE);
      i2cPacketReady = true;
      digitalWriteFast(I2C_INT_PIN, LOW);
      if (debugMode) Serial.println(F("InfoDisp: I2C packet ready"));
    }
  }
}
