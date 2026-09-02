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
                   NOTE: demo mode can only be turned OFF at runtime by the I2C master
                   (see I2CSlave.ino). Simpit messages cannot clear it — while demoMode
                   is true the loop runs stepDemoState() and never services Simpit, so
                   onSimpitMessage()/SCENE_CHANGE never fires.
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
   RESOURCE LIMIT BANDS
   The caution (yellow) and alarm (red) fractions painted on every tape meter's limit
   band, and used to colour its counter and frame. Per-resource assignment is the
   resLimits() table in Resources.ino; these are the two tiers it draws from.

   The generic tiers alias the cross-panel constants in KCMk1_SystemConfig.h so a
   meter here goes yellow at the same fraction the Annunciator lights PROP LOW / RCS
   LOW, and red at the fraction it lights BUS VOLTAGE / the propellant red tier.

   Waste-type resources (CO2, Waste, Liquid Waste, Depleted Fuel) alert on FILLING
   rather than emptying. Their fractions match the Annunciator's TACLS_WASTE_WARN_FRAC
   / TACLS_WASTE_ALARM_FRAC, which have no shared define yet; keep them in step.
****************************************************************************************/
const float RES_WARN_FRAC    = KCM_RES_LOW_WARN_FRAC;   // caution below this fraction of capacity
const float RES_ALARM_FRAC   = KCM_EC_LOW_ALARM_FRAC;   // alarm below this fraction of capacity
const float WASTE_WARN_FRAC  = 0.80f;                   // caution above this fraction full
const float WASTE_ALARM_FRAC = 0.95f;                   // alarm above this fraction full

/***************************************************************************************
   TREND ARROW
   Each meter compares its primary value against the value at the start of a
   TREND_WINDOW_MS window. A move of more than TREND_MIN_FRAC of capacity across the
   window shows a rising or falling arrow beside the percent counter; less shows
   nothing. The window is the arrow's worst-case lag behind a real change; the
   fraction is what keeps it quiet on the per-message jitter Simpit delivers.
****************************************************************************************/
const uint32_t TREND_WINDOW_MS = 2000;      // ms per trend sample window
const float    TREND_MIN_FRAC  = 0.0005f;   // fraction of capacity that counts as movement

/***************************************************************************************
   TIME TO EMPTY
   The TTE key swaps the counter row from percent to the time remaining at the
   current rate. The rate is measured over TTE_WINDOW_MS windows (longer than the
   trend window, so slow drains register) and smoothed 50/50 across windows. A move
   of less than TREND_MIN_FRAC of capacity across one window counts as no rate, so
   the slowest rate the panel will report is TREND_MIN_FRAC per window: with the
   defaults a drain slower than about 5.5 hours to empty reads "---".
****************************************************************************************/
const uint32_t TTE_WINDOW_MS = 10000;       // ms per rate sample window

/***************************************************************************************
   TIME-REMAINING TIERS
   A meter also alerts on the time its resource has left at the current rate (the
   same estimate the TTE counter shows), regardless of the percent: caution below
   the warn time, alarm below the alarm time. Seconds of game time; 0 = no time tier.
   O2, water and food match the Annunciator's TACLS_*_WARN_S / _ALARM_S so the two
   panels agree; there is no shared define yet, keep them in step. EC is this panel's
   own. A tier is left only once the time exceeds its threshold by TIME_HYST_FRAC.
   Note the estimate's floor: a drain slower than about 5.5 hours to empty reads as
   no rate (see TTE_WINDOW_MS), so the water and food warn tiers cannot fire from
   this panel; the Annunciator computes those from consumption rates instead.
****************************************************************************************/
const float TIME_WARN_S_EC    = 15.0f * 60.0f;
const float TIME_ALARM_S_EC   =  5.0f * 60.0f;
const float TIME_WARN_S_O2    = 30.0f * 60.0f;     // TACLS_OXYGEN_WARN_S
const float TIME_ALARM_S_O2   = 10.0f * 60.0f;     // TACLS_OXYGEN_ALARM_S
const float TIME_WARN_S_H2O   = 12.0f * 3600.0f;   // TACLS_WATER_WARN_S
const float TIME_ALARM_S_H2O  =  4.0f * 3600.0f;   // TACLS_WATER_ALARM_S
const float TIME_WARN_S_FOOD  = 72.0f * 3600.0f;   // TACLS_FOOD_WARN_S
const float TIME_ALARM_S_FOOD = 24.0f * 3600.0f;   // TACLS_FOOD_ALARM_S
const float TIME_HYST_FRAC    = 0.10f;

/***************************************************************************************
   ALERT HYSTERESIS
   A level sitting exactly on a caution or alarm fraction would otherwise flip the
   frame and counter colour on every message. Once a meter is in a state it must
   move ALERT_HYST_FRAC of capacity back across the threshold to leave it.
****************************************************************************************/
const float ALERT_HYST_FRAC = 0.01f;        // fraction of capacity

/***************************************************************************************
   RESERVE BUG
   A touch HELD on a meter's tape for BUG_HOLD_MS sets a pilot-defined reserve bug at
   the level first touched (snapped to whole percent); the meter shows the bug colour
   when the level crosses it. A touch that lands within BUG_GRAB_TOL of an existing
   bug grabs it: held still for BUG_HOLD_MS it clears the bug, moved more than
   BUG_DRAG_MIN_PX it drags the bug with the finger until release. The drag threshold
   is the hysteresis that keeps a twitch during a hold from reading as a move. A plain
   tap anywhere on a meter opens the Detail screen. Bugs are saved with the vessel's
   slot configuration.
****************************************************************************************/
const uint32_t BUG_HOLD_MS     = 1500;      // ms a touch must be held still to set or clear a bug
const float    BUG_GRAB_TOL    = 0.08f;     // fraction of capacity within which a touch grabs a bug
const uint16_t BUG_DRAG_MIN_PX = 12;        // px of travel before a grabbed bug starts to move

/***************************************************************************************
   REFRESH TIMEOUT
   After requestResourceRefresh() a slot that has not answered is drawn as "..."
   (awaiting) rather than "---" (not aboard). Simpit answers a refresh within a few
   message cycles; past this timeout the panel stops waiting and "---" means what
   it says.
****************************************************************************************/
const uint32_t REFRESH_TIMEOUT_MS = 3000;   // ms

/***************************************************************************************
   BAR UPDATE HYSTERESIS
   Minimum fractional change in resource level required to trigger a bar redraw.
   Prevents constant redraws from small Simpit value fluctuations.
   0.002 = 0.2% — below this change the bar does not redraw.
****************************************************************************************/
const float BAR_LEVEL_HYSTERESIS = 0.002f;
