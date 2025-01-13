/********************************************************************************************************************************
  Main Application for Kerbal Controller Mk1 

  Adafruit reference examples code written by Limor Fried/Ladyada for Adafruit Industries.
  Also reference UntitledSpaceCraft code written by CodapopKSP (https://github.com/CodapopKSP/UntitledSpaceCraft)

  Licensed under the GNU General Public License v3.0 (GPL-3.0).

  Final code written by Jason Rostoker for Jeb's Controller Works.
********************************************************************************************************************************/
#include <Wire.h>                      // I2C Wire Library
#include <Bounce.h>                    // Button Debounce Library
#include <KerbalSimpit.h>              // Kerbal Simpit Plugin for game interfaces
#include <KerbalSimpitMessageTypes.h>  // Kerbal Simpit message types
#include <PayloadStructs.h>            // Kerbal Simpit payload structures definitions
#include <Adafruit_EMC2101.h>          // Library to support Adafruit EMC2101 Fan Controller
#include <Adafruit_INA219.h>           // Add support for INA219 current sensor
#include <Watchdog_t4.h>               // Inlcude Watchdog Timer library
extern "C" void usb_init(void);

#include "module_variables.h"                                                                                     // Module Register Definitions
#include "C:\Users\jason\OneDrive\Documents\Arduino\KerbalControllerMk1\Software\Common\custom_action_grp_def.h"  // Custom Action Group Definitions
#include "C:\Users\jason\OneDrive\Documents\Arduino\KerbalControllerMk1\Software\Common\keyboard_def.h"           // Keyboard Code Definitions


/***************************************************************************************
   I2C Address Definitions
****************************************************************************************/
#define ANNUN_MC 0x10
#define INFO_MC 0x11
#define RES_MC 0x12

#define Panel_MOD 0x20
#define UI_MOD 0x21
#define StabCtrl_MOD 0x22
#define Function_MOD 0x23
#define EVA_MOD 0x24
#define Translation_MOD 0x25
#define Throttle_MOD 0x26
#define ActionCtrl_MOD 0x27
#define Switch_MOD 0x28
#define Rotation_MOD 0x29
#define VehCtrl_MOD 0x2A
#define Time_MOD 0x2B
#define GPWS_MOD 0x2C

#define INA219_ADDR 0x44
#define EMC2101_ADDR 0x4C


/***************************************************************************************
   Teensy Pin Definitions
****************************************************************************************/
#define ALIVE 2     // Main microcontroller Status
#define MC01_INT 3  // MC01 Interrupt (Annunciator MC)
#define MC02_INT 4  // MC02 Interrupt (Information MC)
#define MC03_INT 5  // MC03 Interrupt (Resource MC)

#define PanelCtrl_INT 25    // Panel Control Interrupt
#define UI_INT 26           // UI Panel Interrupt
#define StabCtrl_INT 27     // Stability Control Panel Interrupt
#define Function_INT 28     // Function Control Panel Interrupt
#define EVA_INT 29          // EVA Panel Interrupt
#define Staging_INT 30      // Staging Interrupt
#define Translation_INT 31  // Translation Module Interrupt
#define Throttle_INT 32     // Throttle Module Interrupt
#define Rotation_INT 33     // Rotation Module Interrupt
#define Switch_INT 34       // Switch Module Interrupt
#define VehCtrl_INT 35      // Vehicle Control Panel Interrupt
#define ActCtrl_INT 36      // Action Control Panel Interrupt
#define Abort_INT 37        // Abort Interrupt
#define GPWS_INT 38         // GPWS Panel Interrupt

#define OFFBOARD_CONNECT 39  // Offboard Connect
#define INTERBOARD_06 40     // Inter-Board Connec 06
#define INTERBOARD_05 41     // Inter-Board Connec 05
#define INTERBOARD_04 13     // Inter-Board Connec 04
#define INTERBOARD_03 14     // Inter-Board Connec 03
#define INTERBOARD_02 15     // Inter-Board Connec 02
#define INTERBOARD_01 20     // Inter-Board Connec 01

#define EXT_SCL 16  // Hardware Accelerated i2c Serial Clock (External Connection)
#define EXT_SDA 17  // Hardware Accelerated i2c Serial Data (External Connection)
#define SDA 18      // Hardware Accelerated i2c Serial Data
#define SCL 19      // Hardware Accelerated i2c Serial Clock


/***************************************************************************************
   Declare Spercific Library Objects
****************************************************************************************/
KerbalSimpit mySimpit(SerialUSB1);             // Declare a KerbalSimpit object that will communicate using the "Serial" device
Bounce stageButton = Bounce(Staging_INT, 10);  // 10 ms debounce
Bounce abortButton = Bounce(Abort_INT, 10);    // 10 ms debounce
Adafruit_EMC2101 fan;                          // Object to support Adafruit EMC2101 Fan Controller
Adafruit_INA219 ina219(INA219_ADDR);           // Object to support Voltage/Current Measurements
WDT_T4<WDT1> wdt;                              // Watchdog timer type 1 object


/***************************************************************************************
  Fan Setup Variables
****************************************************************************************/
float tempC;
float dutyCycle;
float rpm;


/***************************************************************************************
   Define custom action group parameters
****************************************************************************************/
// Value added based on the current selected ctrlGrp
const uint8_t ctrlGrpAdd = 40;

// Final group definitons that will be issued by the commands
uint8_t ag1;
uint8_t ag2;
uint8_t ag3;
uint8_t ag4;
uint8_t ag5;
uint8_t ag6;
uint8_t ag7;
uint8_t ag8;
uint8_t ag9;
uint8_t ag10;
uint8_t solar_array;
uint8_t antenna;
uint8_t cargo_door;
uint8_t radiator;
uint8_t drogue;
uint8_t parachute;
uint8_t ladder;
uint8_t drogue_cut;
uint8_t les;
uint8_t science1;
uint8_t science2;
uint8_t collectSci;
uint8_t engine1;
uint8_t engine2;
uint8_t engineMode;
uint8_t intake;
uint8_t ctrlPt1;
uint8_t ctrlPt2;
uint8_t airbrake;


/***************************************************************************************
   Global Variable Definitions
****************************************************************************************/
String lastAction;  // Storage location for last value in watchdog check
long runtime_start = millis();
float alt_surf;

/***************************************************************************************
   Panel Control Boolean Definitions
****************************************************************************************/
bool demo = false;          // use without needing to be connected to simpit or the MST Arduino
bool debug = false;         // debug sends data to the serial monitor
bool throttleEn = false;    // Throttle Input is enabled
bool audioEn = false;       // indicates audio mode enables.
bool idleMode = false;      // controls whether the idle screen is set
bool mstrMCActive = false;  //active when mstrMCActive is on
bool trimMode = false;      //used to activate trim mode for rotation stick
bool mapRevMode = false;    //used to properly cycle key-pressed when the request is cycle map backward TODO: Need to check need for this
bool camCtrlSet = false;    //used to activate the camera control mode for the translation joystick


/***************************************************************************************
Arduino Setup Function
****************************************************************************************/
void setup() {
  /********************************************************
    Start Serial Interface
  *********************************************************/
  Serial.begin(115200);
  SerialUSB1.begin(115200);

  Serial.println("------------------------------------------------------------------");
  Serial.println("Console initialization...");
  Serial.println("------------------------------------------------------------------");
  Serial.println("Serial communucation established");


  /********************************************************
    Set Pin Modes for necessary inputs/outputs
  *********************************************************/
  Serial.print("Setup pin functions...");
  pinMode(Staging_INT, INPUT);
  pinMode(Abort_INT, INPUT);


  Serial.println("COMPLETE");
}


/***************************************************************************************
Arduino Loop Function
****************************************************************************************/
void loop() {

  /********************************************************
    Process Staging Button
  *********************************************************/
  if (stageButton.update()) {
    if (stageButton.fallingEdge()) {
      mySimpit.activateAction(STAGE_ACTION);
    }
  }

  /********************************************************
    Process Abort Button
  *********************************************************/
  if (abortButton.update()) {
    if (abortButton.fallingEdge()) {
      mySimpit.activateAction(ABORT_ACTION);
    }
  }
}
