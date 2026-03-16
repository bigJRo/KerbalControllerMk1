/***************************************************************************************
   AAA_Config.ino -- Tunable constants for Kerbal Controller Mk1 Annunciator
   Adjust these values to calibrate behaviour without touching application logic.
   All three operating mode flags can also be set at runtime via I2C from the master.
****************************************************************************************/
#include "KCMk1_Annunciator.h"


/***************************************************************************************
   OPERATING MODE
   demoMode    -- set true for bench testing without KSP running.
   audioEnabled -- set true to enable all audio feedback.
   debugMode    -- set true to enable Serial debug output (touch coords, state changes).
   All three can also be set at runtime via the I2C command packet from the master.
****************************************************************************************/
bool demoMode     = false;
bool audioEnabled = false;
bool debugMode    = false;

/***************************************************************************************
   DISPLAY ROTATION
   0 = normal (connector at bottom)
   2 = 180deg (connector at top -- for inverted debug mounting)
****************************************************************************************/
const uint8_t DISPLAY_ROTATION = 0;

/***************************************************************************************
   TEMPERATURE THRESHOLDS (percent of limit, 0--100)
   tempCaution: yellow warning threshold
   tempAlarm:   red alarm threshold -- triggers MASTER ALARM
****************************************************************************************/
uint8_t tempCaution = 70;
uint8_t tempAlarm   = 90;

/***************************************************************************************
   COMMUNICATIONS THRESHOLDS (percent signal strength, 0--100)
   commAlarm:   below this -> red (poor comms)
   commCaution: below this -> yellow (degraded comms)
****************************************************************************************/
uint8_t commCaution = 75;
uint8_t commAlarm   = 25;

/***************************************************************************************
   LOW DELTA-V POST-MECO HOLD-OFF
   After throttling up from a MECO condition, KSP's stageBurnTime estimate is
   unreliable for a brief window (it's computed from instantaneous thrust, which
   is near-zero at the start of a burn). LOW_DV is suppressed for this many
   milliseconds after MECO clears to prevent a false flash on throttle-up.
****************************************************************************************/
const uint16_t LOW_DV_MECO_HOLDOFF_MS = 1500;

/***************************************************************************************
   MASTER ALARM MASK
   Computed from CW_* constants defined in KCMk1_Annunciator.h.
   Lists every WARNING-level bit that should illuminate MASTER ALARM and drive audio.
   To add/remove a warning: change the list here -- no other file needs updating.

   Note: CW_BUS_VOLTAGE requires the Alternate Resource Panel (ARP) mod in KSP1.
   Without ARP, ELECTRIC_MESSAGE is never sent and EC values remain 0 -- the alarm
   will not false-trigger (EC_total guard prevents it) but will also never fire.
****************************************************************************************/
const uint16_t masterAlarmMask = (1u << CW_GROUND_PROX) |
                                  (1u << CW_HIGH_G)      |
                                  (1u << CW_BUS_VOLTAGE) |
                                  (1u << CW_HIGH_TEMP)   |
                                  (1u << CW_LOW_DV);

/***************************************************************************************
   ALERT CHIRP THRESHOLDS
   Values at which upward-crossing alert chirps fire.
****************************************************************************************/
const float ALERT_ALT_THRESHOLD = 3500.0f;   // metres sea level
const float ALERT_VEL_THRESHOLD = 100.0f;    // m/s surface velocity
