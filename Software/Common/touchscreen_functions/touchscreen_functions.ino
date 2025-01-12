/**************************************************************************************
GSLX680 Capacitive Touchscreen support functions taken from BuyDisplay pAGE
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
  Wire1.write(regAddr);              // register 0
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
    digitalWrite(TFT_TOUCH_WAKE, LOW);
    delay(20);
    digitalWrite(TFT_TOUCH_WAKE, HIGH);
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
