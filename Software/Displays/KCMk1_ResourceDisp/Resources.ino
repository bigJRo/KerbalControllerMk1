/***************************************************************************************
   Resources.ino -- Resource type metadata for Kerbal Controller Mk1 Resource Display
   Defines the label, color, and ordering for all supported KSP resource types.
   resLabel() and resColor() are the single source of truth for resource presentation.
   resTypeByIndex() maps the selection grid position to a ResourceType.
****************************************************************************************/
#include "KCMk1_ResourceDisp.h"


/***************************************************************************************
   RESOURCE LABEL
   Short display label for each resource type. Used on main screen bar charts.
****************************************************************************************/
const char* resLabel(ResourceType t) {
  switch (t) {
    case RES_SOLID_FUEL:        return "SF";
    case RES_LIQUID_FUEL:       return "LF";
    case RES_LIQUID_OX:         return "LOx";
    case RES_MONO_PROP:         return "MP";
    case RES_EVA_PROP:          return "EVA";
    case RES_XENON:             return "XE";
    case RES_LIQUID_H2:         return "LH2";
    case RES_LIQUID_METHANE:    return "LMe";
    case RES_LITHIUM:           return "Li";
    case RES_INTAKE_AIR:        return "AIR";
    case RES_ELEC_CHARGE:       return "EC";
    case RES_STORED_CHARGE:     return "StC";
    case RES_ENRICHED_URANIUM:  return "EUr";
    case RES_DEPLETED_URANIUM:  return "DFu";
    case RES_ORE:               return "ORE";
    case RES_ABLATOR:           return "ABL";
    case RES_LS_OXYGEN:         return "O2";
    case RES_LS_WATER:          return "H2O";
    case RES_LS_FOOD:           return "FD";
    case RES_LS_WASTE:          return "WST";
    case RES_LS_LIQUID_WASTE:   return "LWS";
    case RES_LS_CO2:            return "CO2";
    case RES_FERTILIZER:        return "FER";
    default:                    return "";
  }
}


/***************************************************************************************
   RESOURCE FULL NAME
   Full display name for each resource type. Used on selection screen buttons.
****************************************************************************************/
const char* resFullName(ResourceType t) {
  switch (t) {
    case RES_SOLID_FUEL:        return "Solid Fuel";
    case RES_LIQUID_FUEL:       return "Liquid Fuel";
    case RES_LIQUID_OX:         return "Oxidizer";
    case RES_MONO_PROP:         return "Mono Propellant";
    case RES_EVA_PROP:          return "EVA Propellant";
    case RES_XENON:             return "Xenon Gas";
    case RES_LIQUID_H2:         return "Liquid Hydrogen";
    case RES_LIQUID_METHANE:    return "Liquid Methane";
    case RES_LITHIUM:           return "Lithium";
    case RES_INTAKE_AIR:        return "Intake Air";
    case RES_ELEC_CHARGE:       return "Electric Charge";
    case RES_STORED_CHARGE:     return "Stored Charge";
    case RES_ENRICHED_URANIUM:  return "Enriched Uranium";
    case RES_DEPLETED_URANIUM:  return "Depleted Fuel";
    case RES_ORE:               return "Ore";
    case RES_ABLATOR:           return "Ablator";
    case RES_LS_OXYGEN:         return "Oxygen";
    case RES_LS_WATER:          return "Water";
    case RES_LS_FOOD:           return "Food";
    case RES_LS_WASTE:          return "Waste";
    case RES_LS_LIQUID_WASTE:   return "Liquid Waste";
    case RES_LS_CO2:            return "Carbon Dioxide";
    case RES_FERTILIZER:        return "Fertilizer";
    default:                    return "";
  }
}


/***************************************************************************************
   RESOURCE COLOR
   Fixed colour per resource type, from the TFT_* palette in KerbalDisplayCommon.
   RES_NONE returns TFT_DARK_GREY (empty slot indicator).

   Rules (3.4.1 audit):
     - No fill uses an alert colour. TFT_RED is the alarm colour and TFT_YELLOW the
       caution colour, and a tape sits beside its own red/yellow limit band; a fill in
       either would read as a condition rather than an identity.
     - No fill uses a signalling colour: TFT_CYAN is pilot-entered (the reserve bug),
       white/silver is the stage marker line and the counters.
     - Every fill must survive half brightness, since the stage column of LF, LOx,
       SF, Xenon and Ablator draws at dimColor(). Pure blue halved to navy.
     - One colour family per subsystem group where the palette allows, so a run of
       meters under one label also reads as one family.
   TFT_BRICK, TFT_PLUM and TFT_STRAW (KerbalDisplayCommon 3.8.0) exist for this
   table: Solid Fuel needed a red that is not the alarm red, CO2 a bright non-red the
   life-support family had not used, and Liquid Waste a straw yellow. Liquid Methane's
   ocean is the darkest fill left; it is a rare CRP resource.

   Two of the life-support colours are a joke and are meant to stay one: Waste is
   brown and Liquid Waste is straw.
****************************************************************************************/
uint16_t resColor(ResourceType t) {
  switch (t) {
    // Power
    case RES_ELEC_CHARGE:       return TFT_GOLD;
    case RES_STORED_CHARGE:     return TFT_PURPLE;      // charged-capacitor purple; dull yellow sat too close to EC
    // Propellants -- rocket pair, RCS greens, exotics
    case RES_LIQUID_FUEL:       return TFT_ORANGE;
    case RES_LIQUID_OX:         return TFT_FRENCH_BLUE;
    case RES_SOLID_FUEL:        return TFT_BRICK;
    case RES_MONO_PROP:         return TFT_MED_GREEN;
    case RES_EVA_PROP:          return TFT_MINT;
    case RES_XENON:             return TFT_MAGENTA;
    case RES_LIQUID_H2:         return TFT_SKY;
    case RES_LIQUID_METHANE:    return TFT_OCEAN;
    case RES_LITHIUM:           return TFT_ROSE;
    case RES_INTAKE_AIR:        return TFT_SILVER;
    // Nuclear (CRP mod, KSP1) -- greens, the waste product dimmer
    case RES_ENRICHED_URANIUM:  return TFT_NEON_GREEN;
    case RES_DEPLETED_URANIUM:  return TFT_SAP_GREEN;
    // Other
    case RES_ORE:               return TFT_TAN;
    case RES_ABLATOR:           return TFT_VIOLET;
    // Life Support -- steel blue for O2, aqua for water, earth tones for the rest
    case RES_LS_OXYGEN:         return TFT_AIR_SUP_BLUE;
    case RES_LS_CO2:            return TFT_PLUM;
    case RES_LS_FOOD:           return TFT_OLIVE;
    case RES_LS_WASTE:          return TFT_BROWN;
    case RES_LS_WATER:          return TFT_AQUA;
    case RES_LS_LIQUID_WASTE:   return TFT_STRAW;
    // Agriculture
    case RES_FERTILIZER:        return TFT_UPS_BROWN;
    default:                    return TFT_DARK_GREY;
  }
}


/***************************************************************************************
   RESOURCE TYPE BY INDEX
   Maps a 0-based grid index to a ResourceType for the selection screen.
   Index 0 = RES_ELEC_CHARGE (first in display order — Power group).
   Returns RES_NONE if index is out of range.
   NOTE: The ORDER array and ResourceType enum are intentionally independent —
   the enum defines storage identity, ORDER defines display sequence. They can
   diverge without breaking anything as long as ORDER remains complete.
****************************************************************************************/
ResourceType resTypeByIndex(uint8_t index) {
  // Display order for the selection grid, left-to-right, row by row.
  // Groups: Power | Propellants | Nuclear | Other | Life Support | Agriculture
  static const ResourceType ORDER[] = {
    // Power (2)
    RES_ELEC_CHARGE, RES_STORED_CHARGE,
    // Propellants (5) — KSP1 native
    RES_LIQUID_FUEL, RES_LIQUID_OX, RES_SOLID_FUEL, RES_MONO_PROP, RES_XENON,
    // Propellants (4) — CRP mod, KSP1
    RES_LIQUID_H2, RES_LIQUID_METHANE, RES_LITHIUM, RES_INTAKE_AIR,
    // Nuclear (2) — CRP mod, KSP1
    RES_ENRICHED_URANIUM, RES_DEPLETED_URANIUM,
    // Other (2)
    RES_ORE, RES_ABLATOR,
    // Life Support (6) — TAC-LS mod, KSP1
    RES_LS_OXYGEN, RES_LS_CO2, RES_LS_FOOD, RES_LS_WASTE, RES_LS_WATER, RES_LS_LIQUID_WASTE,
    // Agriculture (1) — CRP mod, KSP1
    RES_FERTILIZER,
    // EVA (1) — only selectable while a Kerbal is on EVA; kept last so its cell is
    // blank at the end of the grid (no mid-grid hole) when hidden.
    RES_EVA_PROP,
  };
  static const uint8_t ORDER_LEN = sizeof(ORDER) / sizeof(ORDER[0]);
  if (index >= ORDER_LEN) return RES_NONE;
  return ORDER[index];
}


/***************************************************************************************
   DEFAULT SLOT CONFIGURATION
   Matches the STD preset exactly: EC, LF, LOx, MP, SF, O2, Food, Water, Ablator.
   Called by the DFLT sidebar button and on first boot.
   NOTE: CLEAR on the Select screen bypasses MIN_SLOTS intentionally — this is by
   design so the user can start fresh from slot 1. removeResource() still enforces
   MIN_SLOTS for individual tap-removal.
****************************************************************************************/
// Seed one slot's fill values: 0 in live mode (Simpit repopulates on refresh),
// visible demo values otherwise. Shared by all the slot-loading paths.
void initSlotValues(ResourceSlot &s) {
  s.current      = demoMode ? 1.0f : 0.0f;
  s.maxVal       = demoMode ? 1.0f : 0.0f;
  s.stageCurrent = demoMode ? 0.4f : 0.0f;
  s.stageMax     = demoMode ? 0.4f : 0.0f;
}

// Zero the fill values of every active slot (leaves type/config intact). Used on
// scene/vessel transitions so stale values don't show before Simpit repopulates.
void zeroAllSlotValues() {
  for (uint8_t i = 0; i < slotCount; i++) {
    slots[i].current = slots[i].maxVal = slots[i].stageCurrent = slots[i].stageMax = 0.0f;
  }
}

void initDefaultSlots() {
  for (uint8_t i = 0; i < MAX_SLOTS; i++) slots[i] = ResourceSlot();
  slotCount = DEFAULT_SLOT_COUNT;  // 9 — matches STD preset count
  // STD preset: EC, LF, LOx, MP, SF, O2, Food, Water, Ablator
  static const ResourceType STD_TYPES[DEFAULT_SLOT_COUNT] = {
    RES_ELEC_CHARGE, RES_LIQUID_FUEL, RES_LIQUID_OX, RES_MONO_PROP, RES_SOLID_FUEL,
    RES_LS_OXYGEN, RES_LS_FOOD, RES_LS_WATER, RES_ABLATOR
  };
  for (uint8_t i = 0; i < DEFAULT_SLOT_COUNT; i++) {
    slots[i].type = STD_TYPES[i];
    initSlotValues(slots[i]);
  }
  requestResourceRefresh();
}

// True for the fixed EVA bar set. When a Kerbal is on EVA the display shows only
// these five, and nothing else is selectable (see loop() reconcile + ScreenSelect).
bool isEvaResource(ResourceType t) {
  return t == RES_ELEC_CHARGE || t == RES_EVA_PROP || t == RES_LS_OXYGEN ||
         t == RES_LS_FOOD      || t == RES_LS_WATER;
}

// Load the fixed EVA bar set: Electric Charge, EVA Propellant, Oxygen, Food, Water.
// Called on the transition into EVA mode. Values zero in live mode (Simpit
// repopulates on the refresh request); 1.0 in demo so bars are immediately visible.
void loadEvaSlots() {
  for (uint8_t i = 0; i < MAX_SLOTS; i++) slots[i] = ResourceSlot();
  static const ResourceType EVA_TYPES[5] = {
    RES_ELEC_CHARGE, RES_EVA_PROP, RES_LS_OXYGEN, RES_LS_FOOD, RES_LS_WATER
  };
  slotCount = 5;
  for (uint8_t i = 0; i < 5; i++) {
    slots[i].type = EVA_TYPES[i];
    initSlotValues(slots[i]);
  }
  requestResourceRefresh();
}

// DEMO ONLY — loads all available resource types into slots for layout testing.
// Fills up to MAX_SLOTS (16) slots in display order with sine-wave initial values.
// Called by initDemoMode() in Demo.ino. Not used in live Simpit mode.
void initAllSlots() {
  for (uint8_t i = 0; i < MAX_SLOTS; i++) slots[i] = ResourceSlot();
  slotCount = 0;
  for (uint8_t i = 0; slotCount < MAX_SLOTS; i++) {
    ResourceType t = resTypeByIndex(i);
    if (t == RES_NONE) break;
    slots[slotCount].type         = t;
    slots[slotCount].maxVal       = 1.0f;
    slots[slotCount].stageMax     = 0.4f;
    slots[slotCount].current      = 0.5f + 0.4f * sinf((float)slotCount * 0.7f);
    slots[slotCount].stageCurrent = 0.3f + 0.2f * sinf((float)slotCount * 0.7f);
    slotCount++;
  }
}


/***************************************************************************************
   RESOURCE SUBSYSTEM GROUP
   Drives the Main screen's meter order and group labels. Same grouping as the
   Select grid, so a resource sits under the same heading on both screens.
****************************************************************************************/
ResGroup resGroup(ResourceType t) {
  switch (t) {
    case RES_ELEC_CHARGE:
    case RES_STORED_CHARGE:     return GRP_POWER;
    case RES_LIQUID_FUEL:
    case RES_LIQUID_OX:
    case RES_SOLID_FUEL:
    case RES_MONO_PROP:
    case RES_EVA_PROP:
    case RES_XENON:
    case RES_LIQUID_H2:
    case RES_LIQUID_METHANE:
    case RES_LITHIUM:
    case RES_INTAKE_AIR:        return GRP_PROP;
    case RES_ENRICHED_URANIUM:
    case RES_DEPLETED_URANIUM:  return GRP_NUCLEAR;
    case RES_ORE:
    case RES_ABLATOR:           return GRP_MISC;
    case RES_LS_OXYGEN:
    case RES_LS_CO2:
    case RES_LS_FOOD:
    case RES_LS_WASTE:
    case RES_LS_WATER:
    case RES_LS_LIQUID_WASTE:   return GRP_LIFE;
    case RES_FERTILIZER:        return GRP_AGRI;
    default:                    return GRP_MISC;
  }
}

// Short enough to fit over a single compact-pitch meter in Roboto_Black_12.
const char* resGroupLabel(ResGroup g) {
  switch (g) {
    case GRP_POWER:   return "PWR";
    case GRP_PROP:    return "PROP";
    case GRP_NUCLEAR: return "NUC";
    case GRP_MISC:    return "MISC";
    case GRP_LIFE:    return "LS";
    case GRP_AGRI:    return "AGR";
    default:          return "";
  }
}


/***************************************************************************************
   RESOURCE LIMIT BANDS
   Which resources alert, in which direction, and at what fractions. The two tiers
   come from AAA_Config.ino (generic: cross-panel aligned with the Annunciator;
   waste: fraction full).
     - Consumables (power, propellants, nuclear fuel, ablator, O2/food/water,
       fertilizer): alert on running LOW.
     - Waste products (CO2, Waste, Liquid Waste, Depleted Fuel): alert on filling UP.
     - Ore and Intake Air: no bands. Ore is cargo with no operational floor, and
       intake air is a flow that tracks airspeed rather than a stock that depletes.
****************************************************************************************/
ResLimits resLimits(ResourceType t) {
  switch (t) {
    case RES_ORE:
    case RES_INTAKE_AIR:
      return { 0.0f, 0.0f, false, false };
    case RES_LS_CO2:
    case RES_LS_WASTE:
    case RES_LS_LIQUID_WASTE:
    case RES_DEPLETED_URANIUM:
      return { WASTE_WARN_FRAC, WASTE_ALARM_FRAC, true, true };
    case RES_NONE:
      return { 0.0f, 0.0f, false, false };
    default:
      return { RES_WARN_FRAC, RES_ALARM_FRAC, false, true };
  }
}


/***************************************************************************************
   ALERT STATE WITH HYSTERESIS
   0 nominal, 1 caution, 2 alarm, 3 reserve bug crossed. `prev` is the state the caller
   last drew (255 or 0 for none). Once in a state, the threshold that would leave it is moved
   ALERT_HYST_FRAC further into the nominal region, so a value resting on a
   threshold cannot toggle the colour every message. A pilot-set reserve bug (bug >= 0)
   adds a threshold of its own, below it for a consumable and above it for a waste
   product, reported as ALERT_BUG so it draws in the bug colour rather than yellow. The
   fixed limits outrank it: a level inside both a limit band and the bug shows the
   limit's state.
****************************************************************************************/
uint8_t alertState(ResourceType t, float level, float bug, uint8_t prev) {
  ResLimits lim = resLimits(t);
  if (prev > ALERT_BUG) prev = ALERT_NOMINAL;
  float h = ALERT_HYST_FRAC;
  bool highIsBad = lim.enabled && lim.highIsBad;
  bool inCaution = (prev == ALERT_CAUTION || prev == ALERT_ALARM);
  uint8_t state = ALERT_NOMINAL;
  if (lim.enabled) {
    if (highIsBad) {
      float a = lim.alarm - ((prev == ALERT_ALARM) ? h : 0.0f);
      float w = lim.warn  - (inCaution ? h : 0.0f);
      if      (level > a) state = ALERT_ALARM;
      else if (level > w) state = ALERT_CAUTION;
    } else {
      float a = lim.alarm + ((prev == ALERT_ALARM) ? h : 0.0f);
      float w = lim.warn  + (inCaution ? h : 0.0f);
      if      (level < a) state = ALERT_ALARM;
      else if (level < w) state = ALERT_CAUTION;
    }
  }
  // The reserve bug: its own threshold in the bad direction, its own state and
  // colour. It never raises an alarm; the fixed limit table owns red.
  if (bug >= 0.0f && state == ALERT_NOMINAL) {
    float b = bug + ((prev == ALERT_BUG) ? (highIsBad ? -h : h) : 0.0f);
    if (highIsBad ? (level > b) : (level < b)) state = ALERT_BUG;
  }
  return state;
}


/***************************************************************************************
   SORT SLOTS BY GROUP
   Stable insertion sort of the active slots by resGroup() rank; within a group the
   existing order (selection order) is kept. Values travel with their slot, so this
   is safe to call whenever the slot set may have changed. Called by drawStaticMain()
   so every path onto the Main screen lays the meters out in subsystem order.
****************************************************************************************/
void sortSlotsByGroup() {
  for (uint8_t i = 1; i < slotCount; i++) {
    ResourceSlot key = slots[i];
    uint8_t rank = (uint8_t)resGroup(key.type);
    int16_t j = (int16_t)i - 1;
    while (j >= 0 && (uint8_t)resGroup(slots[j].type) > rank) {
      slots[j + 1] = slots[j];
      j--;
    }
    slots[j + 1] = key;
  }
}
