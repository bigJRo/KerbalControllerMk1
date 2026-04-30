/***************************************************************************************
   AAA_Config.ino -- Tunable constants for Kerbal Controller Mk1 Annunciator
   Adjust these values to calibrate behaviour without touching application logic.
   All three operating mode flags can also be set at runtime via I2C from the master.
****************************************************************************************/
#include "KCMk1_Annunciator.h"


/***************************************************************************************
   OPERATING MODE
   demoMode     -- set true for bench testing without KSP running.
   audioEnabled -- set true to enable all audio feedback.
   debugMode    -- set true to enable Serial debug output (touch coords, state changes).
   All three can also be set at runtime via the I2C command packet from the master.
****************************************************************************************/
bool demoMode       = true;
bool audioEnabled   = false;
bool debugMode      = false;
bool standaloneMode = false;  // true = bypass boot screen / master controller handshake
                              // for bench testing without the master I2C controller.
bool standaloneTest = false;  // true = enter serial-driven test mode after boot.
                              // Implies standaloneMode -- master handshake is skipped.
                              // Set demoMode = false when using standaloneTest.

/***************************************************************************************
   DISPLAY ROTATION
   0 = normal (connector at bottom)
   2 = 180deg (connector at top -- for inverted debug mounting)
****************************************************************************************/
const uint8_t DISPLAY_ROTATION = 0;

/***************************************************************************************
   TEMPERATURE THRESHOLDS (percent of limit, 0--100)
   tempAlarm: red alarm threshold -- triggers MASTER ALARM.
   A yellow caution tier (tempCaution) is reserved for future implementation.
   Currently CW_HIGH_TEMP fires only on the red condition.
****************************************************************************************/
uint8_t tempAlarm   = 90;

/***************************************************************************************
   COMMUNICATIONS THRESHOLDS
   CW_COMM_LOST fires only on total signal loss (commNet == 0).
   A yellow degraded-signal tier is reserved for future implementation.
   commCaution and commAlarm thresholds would drive that tier when added.
****************************************************************************************/

/***************************************************************************************
   LOW DELTA-V POST-THROTTLE-UP HOLD-OFF
   KSP's stageBurnTime is computed from instantaneous thrust, which is near-zero for
   a brief window after throttle-up. LOW_DV is suppressed for this many milliseconds
   after throttle rises from zero to prevent a false flash on ignition.
****************************************************************************************/
const uint16_t LOW_DV_MECO_HOLDOFF_MS = 1500;

/***************************************************************************************
   MASTER ALARM MASK
   Computed from CW_* constants defined in KCMk1_Annunciator.h.
   Lists every WARNING-level bit that illuminates MASTER ALARM and drives audio.
   To add/remove a warning: change the list here -- no other file needs updating.
   Expanded to uint32_t to match cautionWarningState.

   Two-tier indicators (CW_PE_LOW, CW_PROP_LOW, CW_LIFE_SUPPORT) fire master alarm
   only when in their red tier. The C&W bit is set ONLY when the red threshold is
   breached -- CautionWarning.ino uses companion bools (peLowYellow, propLowYellow,
   lsYellow) for the yellow tier without setting the bit. So the alarm mask entry
   here fires correctly without any additional gating.

   Note: CW_BUS_VOLTAGE requires the Alternate Resource Panel (ARP) mod in KSP1.
   Without ARP, ELECTRIC_MESSAGE is never sent and EC values remain 0 -- the alarm
   will not false-trigger (EC_total guard prevents it) but will also never fire.

   Note: CW_LIFE_SUPPORT requires the TAC Life Support mod in KSP1.
   Without TAC-LS, all tacFood/tacWater/tacOxygen values remain 0 and crewCount
   drives the time calculation -- the alarm will not fire if no TAC-LS resources
   are present because the time-remaining calculation returns infinity.
****************************************************************************************/
const uint32_t masterAlarmMask =
    (1ul << CW_LOW_DV)       |
    (1ul << CW_HIGH_G)       |
    (1ul << CW_HIGH_TEMP)    |
    (1ul << CW_BUS_VOLTAGE)  |
    (1ul << CW_ABORT)        |
    (1ul << CW_GROUND_PROX)  |
    (1ul << CW_PE_LOW)       |   // red tier only -- see note above
    (1ul << CW_PROP_LOW)     |   // red tier only -- see note above
    (1ul << CW_LIFE_SUPPORT);    // red tier only -- see note above

/***************************************************************************************
   ALERT CHIRP THRESHOLDS
   Values at which upward-crossing alert chirps fire in ScreenMain.ino.
****************************************************************************************/
const float ALERT_ALT_THRESHOLD = 3500.0f;   // metres sea level
const float ALERT_VEL_THRESHOLD = 100.0f;    // m/s surface velocity

/***************************************************************************************
   CAUTION & WARNING NUMERIC THRESHOLDS
   Kept here for easy flight-test tuning without touching application logic.
   Cross-panel aligned constants reference KCMk1_SystemConfig.h values directly.
****************************************************************************************/

// --- Altitude and proximity ---
const float CW_ALT_THRESHOLD_M      = 200.0f;           // CW_ALT: surface alt below this (m)
const float CW_GEAR_UP_THRESHOLD_M  = 200.0f;           // CW_GEAR_UP: surface alt below this (m)
const float CW_GROUND_PROX_S        = KCM_GROUND_PROX_S; // CW_GROUND_PROX: time to impact (s) -- cross-panel aligned
const float CW_IMPACT_IMM_S         = 60.0f;            // CW_IMPACT_IMM: upper time-to-impact bound (s)

// --- G-forces ---
const float CW_HIGH_G_ALARM         = KCM_HIGH_G_ALARM_POS;  // CW_HIGH_G: positive G threshold (g) -- cross-panel aligned
const float CW_HIGH_G_WARN          = KCM_HIGH_G_ALARM_NEG;  // CW_HIGH_G: negative G threshold (g) -- cross-panel aligned

// --- Electric charge ---
// Standardised to 5% consistent with two-tier resource warning red threshold.
// Note: original was 10%; reduced to 5% to align with PROP_LOW and LIFE_SUPPORT red tiers.
const float CW_EC_LOW_FRAC          = 0.05f;            // CW_BUS_VOLTAGE: EC fraction (5%)

// --- Delta-V ---
const float CW_LOW_DV_MS            = KCM_LOW_DV_MS;    // CW_LOW_DV: stage delta-v (m/s) -- cross-panel aligned
const float CW_LOW_BURN_S           = KCM_LOW_BURN_S;   // CW_LOW_DV: stage burn time (s) -- cross-panel aligned

// --- Propellant ---
const float CW_RCS_LOW_FRAC         = 0.20f;            // CW_RCS_LOW: MonoProp fraction (20%) -- standardised yellow threshold
const float CW_PROP_LOW_WARN_FRAC   = 0.20f;            // CW_PROP_LOW: yellow tier (20%)
const float CW_PROP_LOW_ALARM_FRAC  = 0.05f;            // CW_PROP_LOW: red tier (5%)
// Expected LF/OX mass ratio for standard KSP engines (9 LF : 11 OX by mass = 0.8182)
const float CW_PROP_NOMINAL_RATIO   = 0.8182f;          // CW_PROP_IMBAL: nominal LF/OX ratio
const float CW_PROP_IMBAL_TOL       = 0.10f;            // CW_PROP_IMBAL: allowed deviation (10%)

// --- Chute envelope (Kerbin defaults, m/s) ---
// Thresholds represent maximum safe airspeed for deployment.
// Drogue chutes tolerate higher speeds than main chutes.
// Body-specific values should be added to BodyParams if flying on other atmospheric bodies.
const float CW_CHUTE_MAIN_MAX_SPEED   = 250.0f;         // CHUTE_ENV: max speed for main chute (m/s)
const float CW_CHUTE_DROGUE_MAX_SPEED = 500.0f;         // CHUTE_ENV: max speed for drogue (m/s)

/***************************************************************************************
   TAC LIFE SUPPORT CONSUMPTION RATES
   Source: TacLifeSupport source code (GlobalSettings.cs), units per Earth second per Kerbal.
   These are the default values from the TAC-LS config. If the player has modified
   consumption rates in their save, these values will be inaccurate. Most players use
   defaults; document this limitation in the README.
   Used in CautionWarning.ino to compute time remaining from current resource amounts.
****************************************************************************************/
const double TACLS_FOOD_RATE        = 0.000016927083333; // Food units/s/Kerbal
const double TACLS_WATER_RATE       = 0.000011188078704; // Water units/s/Kerbal
const double TACLS_OXYGEN_RATE      = 0.001713537562385; // Oxygen units/s/Kerbal
const double TACLS_CO2_RATE         = 0.001480128898760; // CO2 units/s/Kerbal (production)
const double TACLS_WASTE_RATE       = 0.000001539351852; // Waste units/s/Kerbal (production)
const double TACLS_WASTEWATER_RATE  = 0.000014247685185; // WasteWater units/s/Kerbal (production)

/***************************************************************************************
   TAC LIFE SUPPORT WARNING THRESHOLDS
   Time-remaining thresholds for CW_LIFE_SUPPORT yellow and red tiers.
   Thresholds scaled to the survival window for each resource:
     Food:   360h window -> warn at 72h, alarm at 24h
     Water:   36h window -> warn at 12h, alarm at  4h
     Oxygen:   2h window -> warn at 30m, alarm at 10m
   Waste resources use fractional fill thresholds (fraction of total capacity).
   The indicator fires on the worst-case condition across all resources.
   Yellow tier: warn threshold breached on any resource.
   Red tier:    alarm threshold breached on any resource (triggers MASTER ALARM).
****************************************************************************************/
const float TACLS_FOOD_WARN_S       = 72.0f  * 3600.0f; // 72 hours in seconds
const float TACLS_FOOD_ALARM_S      = 24.0f  * 3600.0f; // 24 hours in seconds
const float TACLS_WATER_WARN_S      = 12.0f  * 3600.0f; // 12 hours in seconds
const float TACLS_WATER_ALARM_S     =  4.0f  * 3600.0f; //  4 hours in seconds
const float TACLS_OXYGEN_WARN_S     = 30.0f  *   60.0f; // 30 minutes in seconds
const float TACLS_OXYGEN_ALARM_S    = 10.0f  *   60.0f; // 10 minutes in seconds
const float TACLS_WASTE_WARN_FRAC   = 0.80f;             // warn when waste 80% full
const float TACLS_WASTE_ALARM_FRAC  = 0.95f;             // alarm when waste 95% full
