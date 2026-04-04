/***************************************************************************************
   I2CSlave.ino -- I2C slave interface for KCMk1 Resource Display
   Exposes Resource Display state to the KCMk1 master (Teensy 4.1) over I2C.

   Hardware:
     I2C bus    : Wire (pins 18/19 on Teensy 4.0)
     Slave addr : 0x11
     INT pin    : pin 2, OUTPUT, active-LOW
                  ResourceDisp asserts LOW when a fresh packet is ready.
                  Master reads via Wire.requestFrom(0x11, I2C_PACKET_SIZE).
                  Pin returns HIGH after the onRequest handler fires.

   Outbound packet (ResourceDisp -> Master), I2C_PACKET_SIZE = 4 bytes:
     Byte 0  : 0xAD  -- sync/magic byte for framing validation
     Byte 1  : flags
                 bit 0 = simpitConnected
                 bit 1 = flightScene
                 bit 2 = demoMode
                 bits 3-7 reserved (0)
     Byte 2  : slotCount  -- number of currently active resource slots (0-16)
     Byte 3  : reserved (0x00)

   Inbound packet (Master -> ResourceDisp), I2C_CMD_SIZE = 2 bytes:
     Byte 0  : controlByte
                 bits 7:4 = requestType
                   0x0 = NOP           -- no operation
                   0x1 = STATUS        -- request immediate status packet (asserts INT)
                   0x2 = PROCEED       -- proceed to main loop (release boot hold)
                   0x3 = MCU_RESET     -- soft reboot the ResourceDisp
                   0x4 = DISPLAY_RESET -- reset display state and redraw current screen
                 bit  3   = idle_state  (1 = switch to standby screen when not in flight)
                 bit  1   = demoMode    (1 = enable demo mode)
                 bit  0   = debugMode   (1 = enable Serial debug output)
     Byte 1  : reserved (0x00) -- available for future use

   Expanding the protocol:
     Outbound: increment I2C_PACKET_SIZE, add fields to buildI2CPacket().
     Inbound:  increment I2C_CMD_SIZE, add fields to processI2CCommand().
     Update master sketch to match in both cases.
****************************************************************************************/
#include "KCMk1_ResourceDisp.h"

#define I2C_SLAVE_ADDR   KCM_I2C_ADDR_RESDISP       // #3C from SystemConfig
#define I2C_INT_PIN      KCM_I2C_INT_PIN             // #3C from SystemConfig
#define I2C_PACKET_SIZE  4      // outbound: ResourceDisp -> Master (panel-specific)
#define I2C_CMD_SIZE     2      // inbound:  Master -> ResourceDisp (panel-specific)
#define I2C_SYNC_BYTE    KCM_I2C_SYNC_RESDISP       // #3C from SystemConfig

// requestType values (bits 7:4 of controlByte)
#define I2C_REQ_NOP           0x0   // no operation
#define I2C_REQ_STATUS        0x1   // request immediate status packet
#define I2C_REQ_PROCEED       0x2   // proceed to main loop (release boot hold if waiting)
#define I2C_REQ_MCU_RESET     0x3   // soft reboot the ResourceDisp MCU
#define I2C_REQ_DISPLAY_RESET 0x4   // reset display state (redraw current screen)


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
  buf[2] = slotCount;
  buf[3] = 0x00;
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
static volatile uint8_t i2cCmdBuf[I2C_CMD_SIZE];  // volatile: written in ISR, read on main thread
static volatile bool i2cCmdReady = false;

static void processI2CCommand() {
  uint8_t controlByte = i2cCmdBuf[0];
  // byte 1 reserved for future use

  // --- Lower nibble: mode configuration bits ---
  bool newDebug = (controlByte >> 0) & 1;
  bool newDemo  = (controlByte >> 1) & 1;
  bool newIdle  = (controlByte >> 3) & 1;

  if (newDebug != debugMode) {
    debugMode = newDebug;
    setKDCDebugMode(debugMode);
    if (debugMode) Serial.println(F("ResourceDisp: I2C -- debugMode on"));
  }
  if (newDemo != demoMode) {
    demoMode = newDemo;
    if (demoMode) {
      // Switching into demo mode — reinitialise demo state so bars animate
      // from sensible values rather than whatever Simpit last sent.
      initDemoMode();
      if (debugMode) Serial.println(F("ResourceDisp: I2C -- demoMode on, demo state reinitialised"));
    } else {
      // Switching into live mode — connect Simpit if not already connected.
      // This handles the case where the unit booted in demo mode and the master
      // is now enabling live telemetry without a hardware reset.
      if (!simpitConnected) {
        initSimpit();
        if (debugMode) Serial.println(F("ResourceDisp: I2C -- demoMode off, Simpit initialised"));
      } else {
        // Simpit already connected — request a full channel refresh so values
        // repopulate immediately without waiting for a change event.
        simpit.requestMessageOnChannel(0);
        if (debugMode) Serial.println(F("ResourceDisp: I2C -- demoMode off, channel refresh requested"));
      }
    }
  }
  if (newIdle != idleState) {
    idleState = newIdle;
    // If idle_state is now asserted and we are not in a flight scene,
    // switch to standby immediately.
    if (idleState && !flightScene && activeScreen != screen_Standby) {
      switchToScreen(screen_Standby);
    }
    if (debugMode) {
      Serial.print(F("ResourceDisp: I2C -- idleState="));
      Serial.println(idleState);
    }
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
      if (debugMode) Serial.println(F("ResourceDisp: I2C -- status requested"));
      break;

    case I2C_REQ_PROCEED:
      // Release the boot hold. setup() spins on i2cProceedReceived after
      // initialisation; setting it here allows setup() to continue.
      i2cProceedReceived = true;
      if (debugMode) Serial.println(F("ResourceDisp: I2C -- proceed"));
      break;

    case I2C_REQ_MCU_RESET:
      // Soft reboot via ARM AIRCR -- does not return.
      if (debugMode) {
        Serial.println(F("ResourceDisp: I2C -- MCU reset requested"));
        Serial.flush();
      }
      disconnectUSB();
      executeReboot();
      break;

    case I2C_REQ_DISPLAY_RESET:
      // Reset display state so the current screen redraws from scratch on next loop pass.
      // switchToScreen() only — unlike the Annunciator, ResourceDisp has no per-flight
      // boolean flags that need clearing on a display-only reset.
      if (debugMode) Serial.println(F("ResourceDisp: I2C -- display reset"));
      switchToScreen(activeScreen);
      break;

    default:
      if (debugMode) {
        Serial.print(F("ResourceDisp: I2C -- unknown reqType 0x"));
        Serial.println(reqType, HEX);
      }
      break;
  }

  if (debugMode) {
    Serial.print(F("ResourceDisp: I2C cmd ctrl=0x"));
    Serial.println(controlByte, HEX);
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
  if (debugMode) Serial.println(F("ResourceDisp: I2C -- init packet ready, asserting INT"));
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
   Called by the Wire library when the master issues a requestFrom(0x11, N).
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

  if (debugMode) Serial.println(F("ResourceDisp: I2C slave ready at 0x11"));
}


/***************************************************************************************
   UPDATE I2C STATE
   Call from loop(). Two responsibilities:
   1. Apply any pending inbound command from the master (deferred from interrupt).
   2. Detect outbound state changes and assert INT when a fresh packet is ready.
****************************************************************************************/
void updateI2CState() {
  // --- Apply any pending inbound command ---
  if (i2cCmdReady) {
    i2cCmdReady = false;
    processI2CCommand();
  }

  // --- Detect outbound state changes (#21) ---
  if (!i2cPacketReady) {
    uint8_t candidate[I2C_PACKET_SIZE];
    fillI2CPacketBuffer(candidate);
    if (memcmp((uint8_t *)i2cPacket, candidate, I2C_PACKET_SIZE) != 0) {
      memcpy((uint8_t *)i2cPacket, candidate, I2C_PACKET_SIZE);
      i2cPacketReady = true;
      digitalWriteFast(I2C_INT_PIN, LOW);
      if (debugMode) Serial.println(F("ResourceDisp: I2C packet ready"));
    }
  }
}
