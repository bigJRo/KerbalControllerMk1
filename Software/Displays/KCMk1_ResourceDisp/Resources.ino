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
   Fixed color per resource type. Colors match TFT_* palette in KerbalDisplayCommon.
   RES_NONE returns TFT_DARK_GREY (empty slot indicator).
****************************************************************************************/
uint16_t resColor(ResourceType t) {
  switch (t) {
    // Power
    case RES_ELEC_CHARGE:       return TFT_YELLOW;
    case RES_STORED_CHARGE:     return TFT_AIR_SUP_BLUE;
    // Propellants
    case RES_LIQUID_FUEL:       return TFT_ORANGE;
    case RES_LIQUID_OX:         return TFT_BLUE;
    case RES_SOLID_FUEL:        return TFT_RED;
    case RES_MONO_PROP:         return TFT_DARK_GREEN;
    case RES_XENON:             return TFT_MAGENTA;
    case RES_LIQUID_H2:         return TFT_FRENCH_BLUE;
    case RES_LIQUID_METHANE:    return TFT_ROYAL;
    case RES_LITHIUM:           return TFT_INT_ORANGE;
    case RES_INTAKE_AIR:        return TFT_AQUA;
    // Nuclear (CRP mod, KSP1)
    case RES_ENRICHED_URANIUM:  return TFT_NEON_GREEN;
    case RES_DEPLETED_URANIUM:  return TFT_SAP_GREEN;
    // Other
    case RES_ORE:               return TFT_MAROON;
    case RES_ABLATOR:           return TFT_VIOLET;
    // Life Support
    case RES_LS_OXYGEN:         return TFT_SILVER;
    case RES_LS_CO2:            return TFT_CORNELL;
    case RES_LS_FOOD:           return TFT_OLIVE;
    case RES_LS_WASTE:          return TFT_BROWN;
    case RES_LS_WATER:          return TFT_CYAN;
    case RES_LS_LIQUID_WASTE:   return TFT_DULL_YELLOW;
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
  };
  static const uint8_t ORDER_LEN = sizeof(ORDER) / sizeof(ORDER[0]);
  if (index >= ORDER_LEN) return RES_NONE;
  return ORDER[index];
}


/***************************************************************************************
   DEFAULT SLOT CONFIGURATION
   Standard group: EC, LF, LOx, MP, SF, O2, Food, Water (8 slots).
   For demo/layout testing initAllSlots() loads every resource type.
****************************************************************************************/
/***************************************************************************************
   DEFAULT SLOT CONFIGURATION
   Matches the STD preset exactly: EC, LF, LOx, MP, SF, O2, Food, Water.
   Called by the DFLT sidebar button and on first boot.
   NOTE: CLEAR on the Select screen bypasses MIN_SLOTS intentionally — this is by
   design so the user can start fresh from slot 1. removeResource() still enforces
   MIN_SLOTS for individual tap-removal.
****************************************************************************************/
void initDefaultSlots() {
  for (uint8_t i = 0; i < MAX_SLOTS; i++) slots[i] = ResourceSlot();
  slotCount = DEFAULT_SLOT_COUNT;  // 8 — matches STD preset count
  // STD preset: EC, LF, LOx, MP, SF, O2, Food, Water
  static const ResourceType STD_TYPES[8] = {
    RES_ELEC_CHARGE, RES_LIQUID_FUEL, RES_LIQUID_OX, RES_MONO_PROP,
    RES_SOLID_FUEL, RES_LS_OXYGEN, RES_LS_FOOD, RES_LS_WATER
  };
  for (uint8_t i = 0; i < DEFAULT_SLOT_COUNT; i++) {
    slots[i].type         = STD_TYPES[i];
    slots[i].current      = demoMode ? 1.0f : 0.0f;
    slots[i].maxVal       = demoMode ? 1.0f : 0.0f;
    slots[i].stageCurrent = demoMode ? 0.4f : 0.0f;
    slots[i].stageMax     = demoMode ? 0.4f : 0.0f;
  }
  // In live mode, request a Simpit refresh so the new slots populate immediately
  if (!demoMode) simpit.requestMessageOnChannel(0);
}

// DEMO ONLY — loads all available resource types into slots for layout testing.
// Fills up to MAX_SLOTS (16) slots in display order with sine-wave initial values.
// Called by initDemoMode() in Demo.ino. Not used in Simpit integration (Phase 2).
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
