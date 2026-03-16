/***************************************************************************************
   I2CSlave.ino -- I2C slave interface for KCMk1 Annunciator
   Exposes Annunciator state to the KCMk1 master (Teensy 4.1) over I2C.

   Hardware:
     I2C bus    : Wire (pins 18/19 on Teensy 4.0)
     Slave addr : 0x10
     INT pin    : pin 2, OUTPUT, active-LOW
                  Annunciator asserts LOW when a fresh packet is ready.
                  Master reads via Wire.requestFrom(0x10, I2C_PACKET_SIZE).
                  Pin returns HIGH after the onRequest handler fires.

   Outbound packet (Annunciator -> Master), I2C_PACKET_SIZE = 4 bytes:
     Byte 0  : 0xAC  -- sync/magic byte for framing validation
     Byte 1  : flags
                 bit 0 = simpitConnected
                 bit 1 = flightScene
                 bit 2 = masterAlarmOn
                 bits 3-7 reserved (0)
     Byte 2  : cautionWarningState low byte
     Byte 3  : cautionWarningState high byte

   Inbound packet (Master -> Annunciator), I2C_CMD_SIZE = 3 bytes:
     Byte 0  : controlByte
                 bits 7:4 = requestType
                   0x0 = NOP           -- no operation
                   0x1 = STATUS        -- request immediate status packet (asserts INT)
                   0x2 = PROCEED       -- proceed to main loop (release boot hold)
                   0x3 = MCU_RESET     -- soft reboot the Annunciator
                   0x4 = DISPLAY_RESET -- reset display state and redraw all screens
                 bit  3   = idle_state  (1 = switch to standby screen when not in flight)
                 bit  2   = audioOn     (1 = enable audio)
                 bit  1   = demoMode    (1 = enable demo mode)
                 bit  0   = debugMode   (1 = enable Serial debug output)
     Byte 1  : ctrlModeByte -- CtrlMode enum value (ctrl_Rover=0, ctrl_Plane=1, ctrl_Spacecraft=2)
     Byte 2  : ctrlGrpByte  -- active control group, 1-based (matches state.ctrlGrp)

   Expanding the protocol:
     Outbound: increment I2C_PACKET_SIZE, add fields to buildI2CPacket().
     Inbound:  increment I2C_CMD_SIZE, add fields to processI2CCommand().
     Update master sketch to match in both cases.
****************************************************************************************/
#include "KCMk1_Annunciator.h"

#define I2C_SLAVE_ADDR   0x10
#define I2C_INT_PIN      2
#define I2C_PACKET_SIZE  4      // outbound: Annunciator -> Master
#define I2C_CMD_SIZE     3      // inbound:  Master -> Annunciator
#define I2C_SYNC_BYTE    0xAC

// requestType values (bits 7:4 of controlByte)
#define I2C_REQ_NOP           0x0   // no operation
#define I2C_REQ_STATUS        0x1   // request immediate status packet
#define I2C_REQ_PROCEED       0x2   // proceed to main loop (release boot hold if waiting)
#define I2C_REQ_MCU_RESET     0x3   // soft reboot the Annunciator MCU
#define I2C_REQ_DISPLAY_RESET 0x4   // reset display state (redraw all screens)


/***************************************************************************************
   PACKET BUFFER
   Built by buildI2CPacket() and consumed by onI2CRequest().
   Declared volatile because it is written on the main thread and read
   from the Wire interrupt context.
****************************************************************************************/
static volatile uint8_t i2cPacket[I2C_PACKET_SIZE];
static volatile bool i2cPacketReady = false;

// Set to true when the master sends I2C_REQ_PROCEED. setup() spins on this
// flag after initialisation before clearing the screen and entering loop().
volatile bool i2cProceedReceived = false;


/***************************************************************************************
   BUILD PACKET
   Assembles the current state into i2cPacket[]. Call whenever state changes.
****************************************************************************************/
static void buildI2CPacket() {
  uint8_t flags = 0;
  if (simpitConnected)          flags |= (1 << 0);
  if (flightScene)              flags |= (1 << 1);
  if (state.masterAlarmOn)      flags |= (1 << 2);

  i2cPacket[0] = I2C_SYNC_BYTE;
  i2cPacket[1] = flags;
  i2cPacket[2] = (uint8_t)(state.cautionWarningState & 0xFF);
  i2cPacket[3] = (uint8_t)(state.cautionWarningState >> 8);
}


/***************************************************************************************
   PROCESS I2C COMMAND
   Called from loop() to apply a received command packet.
   Runs on the main thread -- safe to modify state, globals, and call Serial.
****************************************************************************************/
static volatile uint8_t i2cCmdBuf[I2C_CMD_SIZE];  // volatile: written in ISR, read on main thread
static volatile bool i2cCmdReady = false;

static void processI2CCommand() {
  uint8_t controlByte  = i2cCmdBuf[0];
  uint8_t ctrlModeByte = i2cCmdBuf[1];
  uint8_t ctrlGrpByte  = i2cCmdBuf[2];

  // --- Lower nibble: mode configuration bits ---
  bool newDebug  = (controlByte >> 0) & 1;
  bool newDemo   = (controlByte >> 1) & 1;
  bool newAudio  = (controlByte >> 2) & 1;
  bool newIdle   = (controlByte >> 3) & 1;

  if (newDebug != debugMode) {
    debugMode = newDebug;
    setKDCDebugMode(debugMode);
    if (debugMode) Serial.println(F("Annunciator: I2C -- debugMode on"));
  }
  if (newAudio != audioEnabled) {
    audioEnabled = newAudio;
    if (!audioEnabled) audioSilence();
    if (debugMode) Serial.println(F("Annunciator: I2C -- audioEnabled updated"));
  }
  if (newDemo != demoMode) {
    demoMode = newDemo;
    if (debugMode) Serial.println(F("Annunciator: I2C -- demoMode updated"));
  }
  if (newIdle != idleState) {
    idleState = newIdle;
    // If idle_state is now asserted and we are not in a flight scene,
    // switch to standby immediately.
    if (idleState && !flightScene && activeScreen != screen_Standby) {
      switchToScreen(screen_Standby);
    }
    if (debugMode) {
      Serial.print(F("Annunciator: I2C -- idleState="));
      Serial.println(idleState);
    }
  }

  // --- ctrlModeByte: update vehCtrlMode if valid ---
  if (ctrlModeByte <= ctrl_Spacecraft) {
    state.vehCtrlMode = (CtrlMode)ctrlModeByte;
  }

  // --- ctrlGrpByte: update active control group (1-based) ---
  if (ctrlGrpByte >= 1) {
    state.ctrlGrp = ctrlGrpByte;
  }

  // --- Upper nibble: requestType ---
  uint8_t reqType = (controlByte >> 4) & 0x0F;
  switch (reqType) {

    case I2C_REQ_NOP:
      break;

    case I2C_REQ_STATUS:
      // Force an immediate status packet -- assert INT so master can read us now.
      buildI2CPacket();
      i2cPacketReady = true;
      digitalWriteFast(I2C_INT_PIN, LOW);
      if (debugMode) Serial.println(F("Annunciator: I2C -- status requested"));
      break;

    case I2C_REQ_PROCEED:
      // Release the boot hold. setup() spins on i2cProceedReceived after
      // initialisation; setting it here allows setup() to continue.
      i2cProceedReceived = true;
      if (debugMode) Serial.println(F("Annunciator: I2C -- proceed"));
      break;

    case I2C_REQ_MCU_RESET:
      // Soft reboot via ARM AIRCR -- does not return.
      if (debugMode) {
        Serial.println(F("Annunciator: I2C -- MCU reset requested"));
        Serial.flush();
      }
      disconnectUSB();
      executeReboot();
      break;

    case I2C_REQ_DISPLAY_RESET:
      // Reset display state so all screens redraw from scratch on next loop pass.
      // switchToScreen() sets prevScreen = screen_COUNT internally.
      if (debugMode) Serial.println(F("Annunciator: I2C -- display reset"));
      resetDisplays();
      switchToScreen(activeScreen);
      break;

    default:
      if (debugMode) {
        Serial.print(F("Annunciator: I2C -- unknown reqType 0x"));
        Serial.println(reqType, HEX);
      }
      break;
  }

  if (debugMode) {
    Serial.print(F("Annunciator: I2C cmd ctrl=0x"));
    Serial.print(controlByte, HEX);
    Serial.print(F(" mode="));
    Serial.print(ctrlModeByte);
    Serial.print(F(" grp="));
    Serial.println(ctrlGrpByte);
  }
}


/***************************************************************************************
   BUILD PACKET AND ASSERT INT
   Public helper called from setup() after initialisation is complete.
   Builds a fresh packet with current state and asserts the INT pin so the
   master knows a status update is ready to read.
****************************************************************************************/
void buildI2CPacketAndAssert() {
  buildI2CPacket();
  i2cPacketReady = true;
  digitalWriteFast(I2C_INT_PIN, LOW);
  if (debugMode) Serial.println(F("Annunciator: I2C -- init packet ready, asserting INT"));
}


/***************************************************************************************
   ON RECEIVE HANDLER
   Called by the Wire library when the master writes to us.
   Copies bytes into the command buffer -- processing deferred to loop().
   Must complete quickly -- runs in interrupt context.
****************************************************************************************/
static void onI2CReceive(int numBytes) {
  if (numBytes == I2C_CMD_SIZE) {
    for (int i = 0; i < I2C_CMD_SIZE; i++) {
      i2cCmdBuf[i] = Wire.read();
    }
    i2cCmdReady = true;
  } else {
    // Drain unexpected bytes
    while (Wire.available()) Wire.read();
  }
}


/***************************************************************************************
   ON REQUEST HANDLER
   Called by the Wire library when the master issues a requestFrom(0x10, N).
   Writes the packet and deasserts the interrupt pin.
   Must complete quickly -- runs in interrupt context.
****************************************************************************************/
static void onI2CRequest() {
  Wire.write((uint8_t *)i2cPacket, I2C_PACKET_SIZE);
  digitalWriteFast(I2C_INT_PIN, HIGH);   // deassert interrupt
  i2cPacketReady = false;
}


/***************************************************************************************
   SETUP I2C SLAVE
   Call from setup() after Wire is ready.
****************************************************************************************/
void setupI2CSlave() {
  pinMode(I2C_INT_PIN, OUTPUT);
  digitalWriteFast(I2C_INT_PIN, HIGH);   // idle high

  Wire.begin(I2C_SLAVE_ADDR);
  Wire.onRequest(onI2CRequest);
  Wire.onReceive(onI2CReceive);

  buildI2CPacket();

  if (debugMode) Serial.println(F("Annunciator: I2C slave ready at 0x10"));
}


/***************************************************************************************
   UPDATE I2C STATE
   Call from loop(). Two responsibilities:
   1. Apply any pending inbound command from the master (deferred from interrupt).
   2. Detect outbound state changes and assert INT when a fresh packet is ready.
   Change detection builds a fresh candidate packet and compares it byte-by-byte
   against the last transmitted packet, keeping all assembly logic in one place.
****************************************************************************************/
void updateI2CState() {
  // --- Apply any pending inbound command ---
  if (i2cCmdReady) {
    i2cCmdReady = false;
    processI2CCommand();
  }

  // --- Detect outbound state changes ---
  if (!i2cPacketReady) {
    uint8_t flags = 0;
    if (simpitConnected)     flags |= (1 << 0);
    if (flightScene)         flags |= (1 << 1);
    if (state.masterAlarmOn) flags |= (1 << 2);

    uint8_t candidate[I2C_PACKET_SIZE];
    candidate[0] = I2C_SYNC_BYTE;
    candidate[1] = flags;
    candidate[2] = (uint8_t)(state.cautionWarningState & 0xFF);
    candidate[3] = (uint8_t)(state.cautionWarningState >> 8);

    bool changed = false;
    for (uint8_t i = 0; i < I2C_PACKET_SIZE; i++) {
      if (candidate[i] != i2cPacket[i]) { changed = true; break; }
    }

    if (changed) {
      for (uint8_t i = 0; i < I2C_PACKET_SIZE; i++) i2cPacket[i] = candidate[i];
      i2cPacketReady = true;
      digitalWriteFast(I2C_INT_PIN, LOW);
      if (debugMode) Serial.println(F("Annunciator: I2C packet ready"));
    }
  }
}
