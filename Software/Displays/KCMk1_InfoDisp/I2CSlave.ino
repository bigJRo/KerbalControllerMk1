/***************************************************************************************
   I2CSlave.ino -- I2C slave interface for KCMk1 Info Display
   Exposes Info Display state to the KCMk1 master (Teensy 4.1) over I2C.

   Hardware:
     I2C bus    : Wire (pins 18/19 on Teensy 4.0)
     Slave addr : 0x12
     INT pin    : pin 2, OUTPUT, active-LOW
                  InfoDisp asserts LOW when a fresh packet is ready.
                  Master reads via Wire.requestFrom(0x12, I2C_PACKET_SIZE).
                  Pin returns HIGH after the onRequest handler fires.

   Outbound packet (InfoDisp -> Master), I2C_PACKET_SIZE = 4 bytes:
     Byte 0  : 0xAD  -- sync/magic byte for framing validation
     Byte 1  : flags
                 bit 0 = simpitConnected
                 bit 1 = flightScene
                 bit 2 = demoMode
                 bits 3-7 reserved (0)
     Byte 2  : activeScreen  -- current ScreenType enum value
     Byte 3  : reserved (0x00)

   Inbound packet (Master -> InfoDisp), I2C_CMD_SIZE = 2 bytes:
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
     Byte 1  : reserved (0x00) -- available for future use

   Expanding the protocol:
     Outbound: increment I2C_PACKET_SIZE, add fields to buildI2CPacket().
     Inbound:  increment I2C_CMD_SIZE, add fields to processI2CCommand().
     Update master sketch to match in both cases.
****************************************************************************************/
#include "KCMk1_InfoDisp.h"
#include <Wire.h>

#define I2C_SLAVE_ADDR   0x12
#define I2C_INT_PIN      2
#define I2C_PACKET_SIZE  4      // outbound: InfoDisp -> Master
#define I2C_CMD_SIZE     2      // inbound:  Master -> InfoDisp
#define I2C_SYNC_BYTE    0xAD

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
   BUILD PACKET
   Assembles the current state into i2cPacket[]. Call whenever state changes.
****************************************************************************************/
static void buildI2CPacket() {
  uint8_t flags = 0;
  if (simpitConnected) flags |= (1 << 0);
  if (flightScene)     flags |= (1 << 1);
  if (demoMode)        flags |= (1 << 2);

  i2cPacket[0] = I2C_SYNC_BYTE;
  i2cPacket[1] = flags;
  i2cPacket[2] = (uint8_t)activeScreen;
  i2cPacket[3] = 0x00;
}


/***************************************************************************************
   PROCESS I2C COMMAND
   Called from loop() to apply a received command packet.
   Runs on the main thread -- safe to modify state, globals, and call Serial.
****************************************************************************************/
static volatile uint8_t i2cCmdBuf[I2C_CMD_SIZE];
static volatile bool i2cCmdReady = false;

static void processI2CCommand() {
  uint8_t controlByte = i2cCmdBuf[0];

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
      // Runtime demo->live: splash will be drawn by loop() _wasDemo transition
    }
  }

  if (newIdle != idleState) {
    idleState = newIdle;
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
      i2cCmdBuf[i] = Wire.read();
    }
    i2cCmdReady = true;
  } else {
    while (Wire.available()) Wire.read();
  }
}


/***************************************************************************************
   ON REQUEST HANDLER -- interrupt context, keep short
****************************************************************************************/
static void onI2CRequest() {
  Wire.write((uint8_t *)i2cPacket, I2C_PACKET_SIZE);
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

  Wire.begin(I2C_SLAVE_ADDR);
  Wire.onRequest(onI2CRequest);
  Wire.onReceive(onI2CReceive);

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

  if (!i2cPacketReady) {
    uint8_t flags = 0;
    if (simpitConnected) flags |= (1 << 0);
    if (flightScene)     flags |= (1 << 1);
    if (demoMode)        flags |= (1 << 2);

    uint8_t candidate[I2C_PACKET_SIZE];
    candidate[0] = I2C_SYNC_BYTE;
    candidate[1] = flags;
    candidate[2] = (uint8_t)activeScreen;
    candidate[3] = 0x00;

    bool changed = false;
    for (uint8_t i = 0; i < I2C_PACKET_SIZE; i++) {
      if (candidate[i] != i2cPacket[i]) { changed = true; break; }
    }

    if (changed) {
      for (uint8_t i = 0; i < I2C_PACKET_SIZE; i++) i2cPacket[i] = candidate[i];
      i2cPacketReady = true;
      digitalWriteFast(I2C_INT_PIN, LOW);
      if (debugMode) Serial.println(F("InfoDisp: I2C packet ready"));
    }
  }
}
