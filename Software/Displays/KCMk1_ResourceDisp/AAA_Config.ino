/***************************************************************************************
   AAA_Config.ino -- Tunable constants for Kerbal Controller Mk1 Resource Display
   Adjust these values to calibrate behaviour without touching application logic.
****************************************************************************************/
#include "KCMk1_ResourceDisp.h"


/***************************************************************************************
   OPERATING MODE
   debugMode    -- set true to enable Serial debug output.
   demoMode     -- controls whether the display uses live KSP data or demo sine waves.
                   true  = demo mode: sine-wave resource values, no KSP connection needed.
                           Use for bench testing without KSP running.
                   false = live mode: Simpit connects via SerialUSB1 and populates slots
                           from KSP telemetry. Set this before deploying with KSP.
                   NOTE: SCENE_CHANGE_MESSAGE from Simpit will clear demoMode at runtime
                   when a flight scene is entered, matching the Annunciator pattern.
                   Can also be toggled at runtime by the I2C master — see I2CSlave.ino.
****************************************************************************************/
bool debugMode = false;  // set true to enable Serial debug output during development
bool demoMode  = false;  // set true for bench testing without KSP; false for production


/***************************************************************************************
   DISPLAY ROTATION
   0 = normal (connector at bottom)
   2 = 180deg (connector at top -- for inverted debug mounting)
****************************************************************************************/
const uint8_t DISPLAY_ROTATION = 0;


/***************************************************************************************
   SLOT CONFIGURATION
   MIN_SLOTS / MAX_SLOTS -- hard limits on how many bars can be active.
   DEFAULT_SLOT_COUNT    -- how many slots are pre-populated on first boot.
   These are defined as constexpr in KCMk1_ResourceDisp.h so they can be used
   as compile-time array sizes (e.g. in VesselSlotRecord).
****************************************************************************************/


/***************************************************************************************
   LOW RESOURCE THRESHOLD
   Bar fill color shifts to red when resource level falls below this percentage.
   Uses the bar's own resource color above the threshold.
****************************************************************************************/
const uint16_t LOW_RES_THRESHOLD = 20;  // percent
