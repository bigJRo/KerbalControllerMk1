#ifndef MAIN_DISPLAY_CORE_H
#define MAIN_DISPLAY_CORE_H

/********************************************************************************************************************************
  Main Display Core for Kerbal Controller

  General support for 5" TFT Display (BuyDisplay ER-TFTM050A2-3-3661-3662). Uses 
    RA8875 for display control and GSLX680 for capactive touch controls.
  Handles common functions for all 5" Display  Modules for use in Kerbal Controller Mk1
  Licensed under the GNU General Public License v3.0 (GPL-3.0).
  Final code written by J. Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/

#include <Arduino.h>    // Include arduino namespace
#include <Wire.h>       // I2C communication
#include <SPI.h>        // SPI Comm Library used with TFT
#include <RA8875_t4.h>  // Hardware to support TFT graphics

#include "fonts.h"              // Additional font inclusion
#include "color_definitions.h"  // TFT dolor definitions 0xXXXX format (RGB 565)
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
struct kspResource {  // Structure to contain resouce value from KSP
  char[20] name;
  float current;
  float total;
  float stage_curr;
  float stage_total;
  uint8_t resID;
  uint16_t color;
};


/***************************************************************************************
  Globals
****************************************************************************************/
// PANEL CONTROL BOOLEANS
extern bool demo = false;            // use without needing to be connected to simpit or the MST Arduino
extern bool debug = false;           // debug sends data to the serial monitor
extern bool usb_disconnect = false;  // boolean to support I2C commanded USB disconnects
extern bool usb_connect = false;     // boolean to support I2C commanded USB reconnects
extern bool simpit_reset = false;    // boolean to support simpit resets in

// Drawing support variables
extern uint16_t masterW = 240;
extern uint16_t masterH = 168;
extern uint16_t indW = 126;
extern uint16_t indH = 96;
extern uint16_t nameW;
extern uint16_t nameH;
extern uint16_t infoW = 160;
extern uint16_t infoH;
extern uint16_t unitW = 100;
extern uint16_t buttonW = 80;
extern uint16_t buttonH = 60;
extern uint16_t width;
extern uint16_t height;

// Additional Support Variables
extern uint8_t packetID;
uint8_t tftDispMode = 0;

// Time Control Functions
extern unsigned long tftlastUpdate = 0;
extern uint16_t tftupdateInt = 100;
extern unsigned long lastTouch = 0;
extern uint16_t touchInt = 250;
extern long runtime_start = millis();

// Capactiive Touchscreen Support
uint16_t tx, ty;
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

struct _ts_event ts_event;

// I2C Interface Supprot
extern uint8_t packetID;

/***************************************************************************************
  Core Function Prototypes
****************************************************************************************/
void beginModule(uint8_t panel_addr);       //  Function for setup in main sketch
void handleRequestEvent();                  //  I2C function, responds to master read request with 4-byte status report
void handleReceiveEvent(int16_t howMany));  //  I2C function, reacts to master sent LED state change request
void setInterrupt();                        // Set interrupt to incidate to I2C master
void clearInterrupt();                      // Clears interrupt
void executeReboot();                       // Execute Teensy soft reboot
void disconnectUSB();                       // Disconenct USB interface
void connectUSB();                          // Conenct USB interface
static void GSLX680_I2C_Write(uint8_t regAddr, uint8_t *val, uint16_t cnt);  // GSLX680_I2C_Write
uint8_t GSLX680_I2C_Read(uint8_t regAddr, uint8_t *pBuf, uint8_t len);       // GSLX680_I2C_Read
static void _GSLX680_clr_reg(void);                                          // GSLX680 Clear reg
static void _GSLX680_reset_chip(void);                                       // GSLX680 Reset
static void _GSLX680_load_fw(void);                                          // GSLX680 Main Down
static void _GSLX680_startup_chip(void);                                     // GSLX680 Startup chip
uint8_t GSLX680_read_data(void);                                             // Get the most data about capacitive touchpanel


#endif  // MAIN_DISPLAY_CORE_H
