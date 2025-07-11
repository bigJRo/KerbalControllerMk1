#include "MainDisplayCore.h"

/********************************************************************************************************************************
  Main Display Core for Kerbal Controller

  General support for 5" TFT Display (BuyDisplay ER-TFTM050A2-3-3661-3662). Uses 
    RA8875 for display control and GSLX680 for capacitive touch controls.
  Handles common functions for all 5" Display  Modules for use in Kerbal Controller Mk1
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/

/***************************************************************************************
  Global variable setup
****************************************************************************************/
bool demo = false;
bool debug = false;
bool usb_disconnect = false;
bool usb_connect = false;
bool simpit_reset = false;

uint16_t masterW = 240;
uint16_t masterH = 168;
uint16_t indW = 126;
uint16_t indH = 96;
uint16_t nameW;
uint16_t nameH;
uint16_t infoW = 160;
uint16_t infoH;
uint16_t unitW = 100;
uint16_t buttonW = 80;
uint16_t buttonH = 60;
uint16_t width;
uint16_t height;

uint8_t packetID;
uint8_t tftDispMode = 0;

unsigned long tftlastUpdate = 0;
uint16_t tftupdateInt = 100;
unsigned long lastTouch = 0;
uint16_t touchInt = 250;

uint16_t tx, ty;
_ts_event ts_event;

/***************************************************************************************
  Setup for library objects
****************************************************************************************/
RA8875 tft = RA8875(TFT_CS, TFT_RST);  // TFT Display

/***************************************************************************************
  Module configuration helper function
****************************************************************************************/
bool beginModule(uint8_t panel_addr) {
  Serial.begin(115200);      // Begin serial interface for debugging
  SerialUSB1.begin(115200);  // Begin USB interface for SimPit connection

  Serial.println("------------------------------------------------------------------");
  Serial.println("Console initialization...");
  Serial.println("------------------------------------------------------------------");
  Serial.println("Serial communucation established");

  pinMode(INT_OUT, OUTPUT);
  clearInterrupt();

  Serial.println("Begin I2C Wire Communication");
  Wire1.setSDA(CTP_SDA);  // I2c for touch screen
  Wire1.setSCL(CTP_SCL);  // I2c for touch screen
  Wire1.begin();          // join i2c bus (address optional for master)

  Wire.setSDA(MAIN_SDA);               // I2c for main microcontroller connection
  Wire.setSCL(MAIN_SCL);               // I2c for main microcontroller connection
  Wire.begin(panel_addr);              // join i2c bus (address optional for master)
  Wire.onReceive(handleReceiveEvent);  // register event

  elapsedMillis debug_start = 0; //Begin initializations of the RA8875 chips & TFT screens
  if (!tft.begin(RA8875_800x480)) {
    Serial.println("RA8875 initialization failed!");
    return false;
  }
  tft.setRotation(0);
  width = tft.width();
  height = tft.height();

  while (!Serial && debug_start <= 5000) {}
  Serial.print("RA8875 start...");
  nameW = infoW * 3;
  nameW = masterW + 2 * indW;
  infoW = (tft.width() - nameW) / 2;
  nameH = (tft.height() - 4 * indH) / 2;
  infoH = nameH;
  Serial.println(" RA8875 initialization complete");
  Serial.println();

  tft.clearScreen(BLACK);

  Serial.println("Initializing TFT GSL1680...");
  delay(5);

  Serial.println("clr reg");
  _GSLX680_clr_reg();
  Serial.println("reset_chip");
  _GSLX680_reset_chip();
  Serial.println("load_fw");
  _GSLX680_load_fw();
  Serial.println("startup_chip");
  _GSLX680_startup_chip();
  delay(50);
  check_mem_data();
  delay(100);

  Serial.println("TFT GSL1680 initialization complete.");
  return true;
}

/***************************************************************************************
  Helper functions for interrupt processing
****************************************************************************************/
void clearInterrupt() {
  digitalWrite(INT_OUT, HIGH);
}

void setInterrupt() {
  digitalWrite(INT_OUT, LOW);
}

/***************************************************************************************
  I2C Event Handlers

  The Teensy 4.0 acts as an I²C SLAVE device
  It uses a simple 2-byte command protocol from the I²C master

  MASTER WRITES 2 BYTES (Command Packet):
    [0] = Command ID
    [1] = Command Signature (magic number for validation)

***************************************************************************************/
void handleRequestEvent() {  // TO DO: Need to update request event information
  // TO DO: Define function
}

void handleReceiveEvent(int16_t howMany) {
  while (1 < Wire.available()) {
    uint8_t packetID = Wire.read();

    switch (packetID) {
      case 0x00:  // reset display request
        if (Wire.read() == 0xAA) { tftDispMode = 0; }
        break;
      case 0x01:  // USB Disconnect Request
        if (Wire.read() == 0xAB) { usb_disconnect = true; }
        break;
      case 0x02:  // USB Connection Request
        if (Wire.read() == 0xAC) { usb_connect = true; }
        break;
      case 0x03:  // Simpit Reset Request
        if (Wire.read() == 0xAD) { simpit_reset = true; }
        break;
    }
  }
}

/***************************************************************************************
   EXECUTE REBOOT
****************************************************************************************/
void executeReboot() {
  // send reboot command -----
  SCB_AIRCR = 0x05FA0004;
}


/***************************************************************************************
   DISCONNECT USB
****************************************************************************************/
void disconnectUSB() {
  // turnoff USB controller and reset it.
  USB1_USBCMD = 0;  // turn off USB controller
  USB1_USBCMD = 2;  // begin USB controller reset
  delay(20);
}


/***************************************************************************************
   CONNECT USB
****************************************************************************************/
void connectUSB() {
  // turnoff USB controller and reset it.
  usb_init();
  delay(2000);
}

/**************************************************************************************
GSLX680 Capacitive Touchscreen support functions taken from BuyDisplay Page
//Web: http://www.buydisplay.com
EastRising Technology Co.,LTD
Examples for ER-TFTM050A2-3 with Capacitive Touch Panel 
Display is Hardware SPI 4-Wire SPI Interface and 3.3V Power Supply
Capacitive Touch Panel is I2C Interface
Example page from Web: https://www.buydisplay.com/5-inch-tft-display-arduino-lcd-shield-w-capacitive-touch-screen-800x480
***************************************************************************************/

//GSLX680_I2C_Write
static void GSLX680_I2C_Write(uint8_t regAddr, uint8_t *val, uint16_t cnt) {
  uint16_t i = 0;

  Wire1.beginTransmission(TOUCH_ADDR);
  Wire1.write(regAddr);             // register 0
  for (i = 0; i < cnt; i++, val++)  //data
  {
    Wire1.write(*val);  // value
  }
  //uint8_t retVal =
  Wire1.endTransmission();
}

//GSLX680_I2C_Read
uint8_t GSLX680_I2C_Read(uint8_t regAddr, uint8_t *pBuf, uint8_t len) {
  Wire1.beginTransmission(TOUCH_ADDR);
  Wire1.write(regAddr);  // register 0
  //uint8_t retVal =
  Wire1.endTransmission();

  //uint8_t returned =
  Wire1.requestFrom(TOUCH_ADDR, len);  // request 1 bytes from slave device #2

  uint8_t i;
  for (i = 0; (i < len) && Wire1.available(); i++) {
    pBuf[i] = Wire1.read();
  }

  return i;
}

//GSLX680 Clear reg
static void _GSLX680_clr_reg(void) {
  uint8_t regAddr;
  uint8_t Wrbuf[4] = { 0x00 };
  //uint8_t len = 1;

  regAddr = 0xe0;
  Wrbuf[0] = 0x88;
  GSLX680_I2C_Write(regAddr, Wrbuf, 1);
  delay(1);
  regAddr = 0x80;
  Wrbuf[0] = 0x03;
  GSLX680_I2C_Write(regAddr, Wrbuf, 1);
  delay(1);
  regAddr = 0xe4;
  Wrbuf[0] = 0x04;
  GSLX680_I2C_Write(regAddr, Wrbuf, 1);
  delay(1);
  regAddr = 0xe0;
  Wrbuf[0] = 0x00;
  GSLX680_I2C_Write(regAddr, Wrbuf, 1);
  delay(1);
}

//_GSLX680 Reset
static void _GSLX680_reset_chip(void) {
  uint8_t regAddr;
  uint8_t Wrbuf[4] = { 0x00 };


  regAddr = 0xe0;
  Wrbuf[0] = 0x88;
  GSLX680_I2C_Write(regAddr, Wrbuf, 1);
  delay(1);

  regAddr = 0xe4;
  Wrbuf[0] = 0x04;
  GSLX680_I2C_Write(regAddr, Wrbuf, 1);
  delay(1);

  regAddr = 0xbc;
  Wrbuf[0] = 0x00;
  Wrbuf[1] = 0x00;
  Wrbuf[2] = 0x00;
  Wrbuf[3] = 0x00;
  GSLX680_I2C_Write(regAddr, Wrbuf, 4);
  delay(1);
}

//GSLX680 Main Down
static void _GSLX680_load_fw(void) {

  uint8_t regAddr;
  uint8_t Wrbuf[4] = { 0x00 };
  uint16_t source_line = 0;
  uint16_t source_len = sizeof(GSLX680_FW) / sizeof(struct fw_data);

  if (debug) {
    Serial.print("GSLX680 Firmware Size = ");
    Serial.println(source_len);
  }
  for (source_line = 0; source_line < source_len; source_line++) {
    regAddr = GSLX680_FW[source_line].offset;
    Wrbuf[0] = (char)(GSLX680_FW[source_line].val & 0x000000ff);
    Wrbuf[1] = (char)((GSLX680_FW[source_line].val & 0x0000ff00) >> 8);
    Wrbuf[2] = (char)((GSLX680_FW[source_line].val & 0x00ff0000) >> 16);
    Wrbuf[3] = (char)((GSLX680_FW[source_line].val & 0xff000000) >> 24);

    GSLX680_I2C_Write(regAddr, Wrbuf, 4);
  }
}

//startup chip
static void _GSLX680_startup_chip(void) {

  uint8_t Wrbuf[4] = { 0x00 };

  Wrbuf[0] = 0x00;
  GSLX680_I2C_Write(0xe0, Wrbuf, 1);
}

static void check_mem_data(void) {
  //uint8_t write_buf;
  uint8_t read_buf[4] = { 0 };

  delay(30);

  GSLX680_I2C_Read(0xb0, read_buf, 4);


  if ((read_buf[3] != 0x5a) & (read_buf[2] != 0x5a) & (read_buf[1] != 0x5a) & (read_buf[0] != 0x5a)) {
    if (debug) {
      Serial.println("init failure,Reinitialize");
      Serial.println("CTP initialization failure  Reinitialize.");
    }
    digitalWrite(CTP_WAKE, LOW);
    delay(20);
    digitalWrite(CTP_WAKE, HIGH);
    delay(20);
    //test_i2c();
    _GSLX680_clr_reg();
    _GSLX680_reset_chip();
    _GSLX680_load_fw();
    _GSLX680_startup_chip();

    if ((read_buf[3] != 0x5a) & (read_buf[2] != 0x5a) & (read_buf[1] != 0x5a) & (read_buf[0] != 0x5a)) {  //tft.setCursor(0,16);

      if (debug) { Serial.println("CTP initialization failure... Reinitialize."); }
      while (1)
        ;
    }

  } else {
    if (debug) {
      Serial.println("CTP initialization OK.");
      Serial.println("init done");
    }
  }
}

//get the most data about capacitive touchpanel.
uint8_t GSLX680_read_data(void) {
  uint8_t touch_data[24] = { 0 };
  uint8_t reg = 0x80;
  GSLX680_I2C_Read(reg, touch_data, 24);
  ts_event.fingers = touch_data[0];
  ts_event.y5 = (uint16_t)(touch_data[23]) << 8 | (uint16_t)touch_data[22];
  ts_event.x5 = (uint16_t)(touch_data[21]) << 8 | (uint16_t)touch_data[20];
  ts_event.y4 = (uint16_t)(touch_data[19]) << 8 | (uint16_t)touch_data[18];
  ts_event.x4 = (uint16_t)(touch_data[17]) << 8 | (uint16_t)touch_data[16];
  ts_event.y3 = (uint16_t)(touch_data[15]) << 8 | (uint16_t)touch_data[14];
  ts_event.x3 = (uint16_t)(touch_data[13]) << 8 | (uint16_t)touch_data[12];
  ts_event.y2 = (uint16_t)(touch_data[11]) << 8 | (uint16_t)touch_data[10];
  ts_event.x2 = (uint16_t)(touch_data[9]) << 8 | (uint16_t)touch_data[8];
  ts_event.y1 = (uint16_t)(touch_data[7]) << 8 | (uint16_t)touch_data[6];
  ts_event.x1 = (uint16_t)(touch_data[5]) << 8 | (uint16_t)touch_data[4];
  return 0;
}
