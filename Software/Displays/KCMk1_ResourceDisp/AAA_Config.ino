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

// STANDALONE_TEST: true = no I2C master connected — skip the boot PROCEED handshake
// and enter loop() immediately. Safe to leave true for bench/UI testing; set false
// for production (master will send PROCEED after reading the status packet).
const bool STANDALONE_TEST = false;


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
   Reserved / currently unused. Bars now always render in their designated
   resource color regardless of level; low-resource warning is shown only by the
   percentage text color (fixed 10% critical / 30% caution thresholds in
   ScreenMain). Kept for a possible future low-level cue.
****************************************************************************************/
const uint16_t LOW_RES_THRESHOLD = 20;  // percent (reserved — not currently used)

/***************************************************************************************
   BAR UPDATE HYSTERESIS
   Minimum fractional change in resource level required to trigger a bar redraw.
   Prevents constant redraws from small Simpit value fluctuations.
   0.002 = 0.2% — below this change the bar does not redraw.
****************************************************************************************/
const float BAR_LEVEL_HYSTERESIS = 0.002f;
