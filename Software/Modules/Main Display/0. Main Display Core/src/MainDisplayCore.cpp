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


/***************************************************************************************
   COLOR 5-6-5 CONVERTER
   Converts 3 8-bit RGB balues into a a 5-6-5 color value
   - INPUTS:
    - {uint8_t} r = Red value
    - {uint8_t} g = Green value
    - {uint8_t} b = Blue value
   - OUTPUTS:
    - {uint16_t} resulting 5-6-5 color value
****************************************************************************************/
uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}


/***************************************************************************************
   DECIMAL TO BCD
   Convert normal decimal numbers to binary coded decimal
   - INPUTS:
    - {byte} val = Decimal value to be converted 
   - OUTPUTS:
    - {byte} resulting BCD
****************************************************************************************/
byte decToBcd(byte val) {
  // Convert normal decimal numbers to binary coded decimal
  return ((val / 10 * 16) + (val % 10));
}


/***************************************************************************************
   DRAW Aligned Text Block with Optional Background Fill

   Uses RA8875_t4 to draw left, center, or right aligned text within a bounded box.
   - INPUTS:
     - {uint16_t} x0:      X origin (top-left of print area)
     - {uint16_t} y0:      Y origin (top-left of print area)
     - {uint16_t} w:       Width of print area in pixels
     - {uint16_t} h:       Height of print area in pixels
     - {const char*} text: C-style string to print
     - {uint16_t} foreColor: Foreground/text color
     - {uint16_t} backColor: Background/text-box color
     - {TextAlign} align:  Text alignment (left, center, right)
     - {bool} drawBg:      If true, draws full background rectangle

   - OUTPUT: None
****************************************************************************************/
void drawAlignedText(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                     const char* text, uint16_t foreColor, uint16_t backColor,
                     TextAlign align, bool drawBg = false) {
  const uint8_t border = 8;
  int16_t x, y;
  uint16_t textW, textH;

  // Optional: Fill background area first
  if (drawBg) {
    tft.fillRect(x0, y0, w, h, backColor);
  }

  tft.setTextColor(foreColor, backColor);

  if (align == ALIGN_CENTER) {
    // Use RA8875 internal center-aligning cursor
    tft.setCursor(x0 + w / 2, y0 + h / 2, true);
    tft.print(text);
  } else {
    // Get actual text bounds
    tft.getTextBounds(text, x0, y0 + h, &x, &y, &textW, &textH);
    uint16_t verticalOffset = (h - 2 * textH) / 2;
    uint16_t cursorX = (align == ALIGN_LEFT) 
                       ? x0 + border 
                       : x0 + w - textW - border;
    uint16_t cursorY = y0 + verticalOffset + textH / 2;
    tft.setCursor(cursorX, cursorY);
    tft.print(text);
  }
}

/***************************************************************************************
   CORE DISPLAY RENDERER – DRAW VALUE BLOCK

   Internal logic engine that renders a labeled display block using alignment and 
   conditional updates. Draws parameter and value text using the supplied alignment,
   color, and background settings. Only updates changed content.

   This function is used by all printDisp-style wrapper functions to minimize redundancy.

   - INPUTS (via DisplayValueConfig struct):
     - param        : Current parameter label (left or center aligned)
     - prevParam    : Previously displayed parameter label
     - value        : Current value text (right or left aligned)
     - prevValue    : Previously displayed value text
     - x0, y0       : Top-left corner of display block
     - w, h         : Width and height of the block
     - paramColor   : Color to render parameter text
     - valueColor   : Color to render value text
     - valueBgColor : Optional background color for value (if useValueBg is true)
     - useValueBg   : Enables background color fill behind value text
     - drawBorder   : If true, draws a white border around the block
     - paramAlign   : TextAlign enum for parameter (default: ALIGN_LEFT)
     - valueAlign   : TextAlign enum for value (default: ALIGN_RIGHT)

   - OUTPUT: None
****************************************************************************************/
void drawValueBlock(const DisplayValueConfig& cfg) {
  if (cfg.drawBorder) {
    tft.drawRect(cfg.x0, cfg.y0, cfg.w, cfg.h, WHITE);
  }

  if (strcmp(cfg.prevParam, cfg.param) != 0) {
    drawAlignedText(cfg.x0, cfg.y0, cfg.w, cfg.h, cfg.prevParam, BLACK, BLACK, cfg.paramAlign);
    drawAlignedText(cfg.x0, cfg.y0, cfg.w, cfg.h, cfg.param, cfg.paramColor, BLACK, cfg.paramAlign);
  }

  if (strcmp(cfg.prevValue, cfg.value) != 0) {
    drawAlignedText(cfg.x0, cfg.y0, cfg.w, cfg.h, cfg.prevValue, BLACK, BLACK, cfg.valueAlign);
    drawAlignedText(cfg.x0, cfg.y0, cfg.w, cfg.h, cfg.value, cfg.valueColor,
                    cfg.useValueBg ? cfg.valueBgColor : BLACK, cfg.valueAlign);
  }
}

/***************************************************************************************
   PRINT DISPLAY BLOCK – BASIC VALUE

   Displays a parameter name (left aligned) and a value (right aligned) within a 
   rectangular area. Only redraws if value or label has changed.

   - INPUTS:
     - x0, y0       : Top-left corner of block
     - w, h         : Width and height of block
     - prevParam    : Previously displayed parameter label
     - param        : Current parameter label
     - paramColor   : Text color for parameter label
     - prevVal      : Previously displayed numeric value
     - val          : Current numeric value to display
     - valColor     : Text color for value
     - border       : Draw border rectangle if true
****************************************************************************************/
void printDispBasic(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                    const char* prevParam, const char* param, uint16_t paramColor,
                    uint16_t prevVal, uint16_t val, uint16_t valColor, bool border) {
  char prevBuf[12], valBuf[12];
  snprintf(prevBuf, sizeof(prevBuf), "%u", prevVal);
  snprintf(valBuf, sizeof(valBuf), "%u", val);

  DisplayValueConfig cfg = {
    .param = param, .prevParam = prevParam,
    .value = valBuf, .prevValue = prevBuf,
    .x0 = x0, .y0 = y0, .w = w, .h = h,
    .paramColor = paramColor, .valueColor = valColor,
    .drawBorder = border
  };
  drawValueBlock(cfg);
}

/***************************************************************************************
   PRINT DISPLAY BLOCK – COLOR BY VALUE THRESHOLD

   Displays a parameter label (left) and a value (right) with conditional coloring
   based on thresholds (low/mid/high). Only redraws if value or label has changed.

   - INPUTS:
     - x0, y0       : Top-left corner of block
     - w, h         : Width and height of block
     - prevParam    : Previously displayed parameter label
     - param        : Current parameter label
     - paramColor   : Text color for parameter label
     - prevVal      : Previously displayed numeric value
     - val          : Current numeric value to display
     - lowVal       : Threshold for low range
     - lowColor     : Text color for low values
     - lowBack      : Background color for low values
     - midVal       : Threshold for mid range
     - midColor     : Text color for mid values
     - midBack      : Background color for mid values
     - highColor    : Text color for high values
     - highBack     : Background color for high values
     - border       : Draw border rectangle if true
****************************************************************************************/
void printDispThreshold(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                        const char* prevParam, const char* param, uint16_t paramColor,
                        uint16_t prevVal, uint16_t val,
                        uint16_t lowVal, uint16_t lowColor, uint16_t lowBack,
                        uint16_t midVal, uint16_t midColor, uint16_t midBack,
                        uint16_t highColor, uint16_t highBack,
                        bool border) {
  char prevBuf[12], valBuf[12];
  snprintf(prevBuf, sizeof(prevBuf), "%u", prevVal);
  snprintf(valBuf, sizeof(valBuf), "%u", val);

  uint16_t fore, back;
  if (val < lowVal) {
    fore = lowColor;
    back = lowBack;
  } else if (val < midVal) {
    fore = midColor;
    back = midBack;
  } else {
    fore = highColor;
    back = highBack;
  }

  DisplayValueConfig cfg = {
    .param = param, .prevParam = prevParam,
    .value = valBuf, .prevValue = prevBuf,
    .x0 = x0, .y0 = y0, .w = w, .h = h,
    .paramColor = paramColor,
    .valueColor = fore, .valueBgColor = back,
    .useValueBg = true,
    .drawBorder = border
  };
  drawValueBlock(cfg);
}

/***************************************************************************************
   PRINT DISPLAY BLOCK – PERCENTAGE WITH COLOR THRESHOLDS

   Similar to printDispThreshold but appends a '%' sign to the numeric value. 
   Text and background color depend on threshold range.

   - INPUTS:
     - x0, y0       : Top-left corner of block
     - w, h         : Width and height of block
     - prevParam    : Previously displayed parameter label
     - param        : Current parameter label
     - paramColor   : Text color for parameter label
     - prevVal      : Previously displayed percentage value
     - val          : Current percentage value to display
     - lowVal       : Threshold for low range
     - lowColor     : Text color for low values
     - lowBack      : Background color for low values
     - midVal       : Threshold for mid range
     - midColor     : Text color for mid values
     - midBack      : Background color for mid values
     - highColor    : Text color for high values
     - highBack     : Background color for high values
     - border       : Draw border rectangle if true
****************************************************************************************/
void printDispPerc(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                   const char* prevParam, const char* param, uint16_t paramColor,
                   uint16_t prevVal, uint16_t val,
                   uint16_t lowVal, uint16_t lowColor, uint16_t lowBack,
                   uint16_t midVal, uint16_t midColor, uint16_t midBack,
                   uint16_t highColor, uint16_t highBack,
                   bool border) {
  char prevBuf[12], valBuf[12];
  snprintf(prevBuf, sizeof(prevBuf), "%u%%", prevVal);
  snprintf(valBuf, sizeof(valBuf), "%u%%", val);

  uint16_t fore, back;
  if (val < lowVal) {
    fore = lowColor;
    back = lowBack;
  } else if (val < midVal) {
    fore = midColor;
    back = midBack;
  } else {
    fore = highColor;
    back = highBack;
  }

  DisplayValueConfig cfg = {
    .param = param, .prevParam = prevParam,
    .value = valBuf, .prevValue = prevBuf,
    .x0 = x0, .y0 = y0, .w = w, .h = h,
    .paramColor = paramColor,
    .valueColor = fore, .valueBgColor = back,
    .useValueBg = true,
    .drawBorder = border
  };
  drawValueBlock(cfg);
}

/***************************************************************************************
   PRINT NAME BLOCK

   Displays a single value (left aligned) without any parameter label. 
   Used for label-only regions. Only redraws if the value changes.

   - INPUTS:
     - x0, y0     : Top-left corner of block
     - w, h       : Width and height of block
     - prevVal    : Previously displayed string
     - val        : Current string to display
     - valColor   : Text color
     - border     : Draw border rectangle if true
****************************************************************************************/
void printName(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
               const char* prevVal, const char* val, uint16_t valColor, bool border) {
  DisplayValueConfig cfg = {
    .param = "", .prevParam = "",
    .value = val, .prevValue = prevVal,
    .x0 = x0, .y0 = y0, .w = w, .h = h,
    .paramColor = 0, .valueColor = valColor,
    .valueAlign = ALIGN_LEFT,
    .drawBorder = border
  };
  drawValueBlock(cfg);
}

/***************************************************************************************
   PRINT TITLE BLOCK – CENTERED TEXT

   Displays a single centered string label. Used for section titles or headings.
   Only redraws if the label has changed.

   - INPUTS:
     - x0, y0       : Top-left corner of block
     - w, h         : Width and height of block
     - prevParam    : Previously displayed title
     - param        : Current title to display
     - paramColor   : Text color
     - border       : Draw border rectangle if true
****************************************************************************************/
void printTitle(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                const char* prevParam, const char* param,
                uint16_t paramColor, bool border) {
  DisplayValueConfig cfg = {
    .param = param, .prevParam = prevParam,
    .value = "", .prevValue = "",
    .x0 = x0, .y0 = y0, .w = w, .h = h,
    .paramColor = paramColor,
    .paramAlign = ALIGN_CENTER,
    .drawBorder = border
  };
  drawValueBlock(cfg);
}
