#ifndef MAIN_DISPLAY_CORE_H
#define MAIN_DISPLAY_CORE_H

/********************************************************************************************************************************
  Main Display Core for Kerbal Controller

  General support for 5" TFT Display (BuyDisplay ER-TFTM050A2-3-3661-3662). Uses 
    RA8875 for display control and GSLX680 for capacitive touch controls.
  Handles common functions for all 5" Display  Modules for use in Kerbal Controller Mk1
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/

#include <Arduino.h>    // Include arduino namespace
#include <Wire.h>       // I2C communication
#include <SPI.h>        // SPI Comm Library used with TFT
#include <RA8875_t4.h>  // Hardware to support TFT graphics
extern "C" void usb_init(void);

#include "fonts.h"              // Additional font inclusion
#include "color_definitions.h"  // TFT color definitions 0xXXXX format (RGB 565)
#include "touchScreen_fw.h"     // GSLX680 firmware for initialization

/***************************************************************************************
  Pin Definitions (ATtiny816 w/ megaTinyCore Pin Mapping)
****************************************************************************************/
#define TFT_CS 10    // TFT Chip Select Pin, for SPI communications
#define TFT_RST 15   // TFT Chip Reset, for SPI communications
#define TFT_WAIT 14  // TFT Chip Reset, for SPI communications
#define CTP_WAKE 3   // Screen Touch Controller wake pin
#define CTP_INT 22   // Screen Touch Controller Interrupt pin

#define MAIN_SCL 19  // Hardware Accelerated i2c Serial Clock (Mainboard Connection)
#define MAIN_SDA 18  // Hardware Accelerated i2c Serial Data (Mainboard Connection)
#define CTP_SDA 17   // Hardware Accelerated i2c Serial Data
#define CTP_SCL 16   // Hardware Accelerated i2c Serial Clock

#define INT_OUT 2  //  Interrupt output to notify host of changes

/***************************************************************************************
  Structs and Enums
****************************************************************************************/
struct kspResource {  // Structure to contain resource value from KSP
  char name[20];
  float current;
  float total;
  float stage_curr;
  float stage_total;
  uint8_t resID;
  uint16_t color;
};

enum TextAlign { // Text alignment options for display
  ALIGN_LEFT,
  ALIGN_CENTER,
  ALIGN_RIGHT
};

struct DisplayValueConfig { // Structure to hold configuration for displaying a parameter and its value
  const char* param;
  const char* prevParam;
  const char* value;
  const char* prevValue;

  uint16_t x0, y0, w, h;

  uint16_t paramColor;
  uint16_t valueColor;
  uint16_t valueBgColor = BLACK;

  bool useValueBg = false;
  bool drawBorder = false;

  TextAlign paramAlign = ALIGN_LEFT;
  TextAlign valueAlign = ALIGN_RIGHT;
};


/***************************************************************************************
  Globals
****************************************************************************************/
// PANEL CONTROL BOOLEANS
extern bool demo;
extern bool debug;
extern bool usb_disconnect;
extern bool usb_connect;
extern bool simpit_reset;

// Drawing support variables
extern uint16_t masterW;
extern uint16_t masterH;
extern uint16_t indW;
extern uint16_t indH;
extern uint16_t nameW;
extern uint16_t nameH;
extern uint16_t infoW;
extern uint16_t infoH;
extern uint16_t unitW;
extern uint16_t buttonW;
extern uint16_t buttonH;
extern uint16_t width;
extern uint16_t height;

// Additional Support Variables
extern uint8_t packetID;
extern uint8_t tftDispMode;

// Time Control Functions
extern unsigned long tftlastUpdate;
extern uint16_t tftupdateInt;
extern unsigned long lastTouch;
extern uint16_t touchInt;

// Capacitive Touchscreen Support
extern uint16_t tx, ty;

struct _ts_event {
  uint16_t x1;
  uint16_t y1;
  uint16_t x2;
  uint16_t y2;
  uint16_t x3;
  uint16_t y3;
  uint16_t x4;
  uint16_t y4;
  uint16_t x5;
  uint16_t y5;
  uint8_t fingers;
};

extern struct _ts_event ts_event;

/***************************************************************************************
  Core Function Prototypes
****************************************************************************************/
bool beginModule(uint8_t panel_addr);       // Function for setup in main sketch
void clearInterrupt();                      // Clears interrupt
void setInterrupt();                        // Set interrupt to indicate to I2C master
void handleRequestEvent();                  // I2C function, responds to master read request with 4-byte status report
void handleReceiveEvent(int16_t howMany);   // I2C function, reacts to master sent LED state change request
void executeReboot();                       // Execute Teensy soft reboot
void disconnectUSB();                       // Disconnect USB interface
void connectUSB();                          // Connect USB interface
static void GSLX680_I2C_Write(uint8_t regAddr, uint8_t *val, uint16_t cnt);  // GSLX680_I2C_Write
uint8_t GSLX680_I2C_Read(uint8_t regAddr, uint8_t *pBuf, uint8_t len);       // GSLX680_I2C_Read
static void _GSLX680_clr_reg(void);                                          // GSLX680 Clear reg
static void _GSLX680_reset_chip(void);                                       // GSLX680 Reset
static void _GSLX680_load_fw(void);                                          // GSLX680 Main Down
static void _GSLX680_startup_chip(void);                                     // GSLX680 Startup chip
uint8_t GSLX680_read_data(void);                                             // Get the most data about capacitive touch panel
uint16_t color565(uint8_t r, uint8_t g, uint8_t b); // Converts 3 8-bit RGB values into a 5-6-5 color value
uint8_t decToBcd(uint8_t val) // Converts normal decimal numbers to binary coded decimal
void drawAlignedText(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                     const char* text, uint16_t foreColor, uint16_t backColor,
                     TextAlign align, bool drawBg = false); // Draws text aligned within a rectangle
void drawValueBlock(const DisplayValueConfig& cfg); // Draws a parameter and its value in a block with optional background and border
void printDispBasic(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                    const char* prevParam, const char* param, uint16_t paramColor,
                    uint16_t prevVal, uint16_t val, uint16_t valColor, bool border); // Displays a parameter and its value in a block
void printDispThreshold(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                        const char* prevParam, const char* param, uint16_t paramColor,
                        uint16_t prevVal, uint16_t val,
                        uint16_t lowVal, uint16_t lowColor, uint16_t lowBack,
                        uint16_t midVal, uint16_t midColor, uint16_t midBack,
                        uint16_t highColor, uint16_t highBack,
                        bool border); // Displays a parameter and its value with conditional coloring based on thresholds
void printDispPerc(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                   const char* prevParam, const char* param, uint16_t paramColor,
                   uint16_t prevVal, uint16_t val,
                   uint16_t lowVal, uint16_t lowColor, uint16_t lowBack,
                   uint16_t midVal, uint16_t midColor, uint16_t midBack,
                   uint16_t highColor, uint16_t highBack,
                   bool border); // Displays a parameter and its percentage value with conditional coloring based on thresholds
void printName(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
               const char* prevVal, const char* val, uint16_t valColor, bool border); // Displays a single value without parameter label
void printTitle(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h,
                const char* prevParam, const char* param,
                uint16_t paramColor, bool border); // Displays a parameter name and value in a block with optional border


#endif  // MAIN_DISPLAY_CORE_H
