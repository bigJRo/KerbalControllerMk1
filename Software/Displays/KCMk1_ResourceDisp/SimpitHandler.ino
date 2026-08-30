/***************************************************************************************
   SimpitHandler.ino -- KerbalSimpit integration for Kerbal Controller Mk1 Resource Display

   Subscribes to resource channels and populates slots[] with live KSP data.
   When demoMode is true this file is dormant — stepDemoState() drives the values instead.
   When demoMode is false, simpit.update() is called from loop() and this handler fires.

   Channel mapping (KSP1 + ARP mod required for most resource channels):
     Native KSP1 with stage variants:  LF, LOx, SF, Xenon, Ablator
     Native KSP1 vessel-only:          Electric Charge, Monoprop, EVA Propellant, Ore
     TAC Life Support mod:             Food, Water, Oxygen (TACLS_RESOURCE_MESSAGE)
                                       Waste, Liquid Waste, CO2 (TACLS_WASTE_MESSAGE)
     CRP mod via CUSTOM_RESOURCE_1:    Stored Charge, Enriched Uranium,
                                       Depleted Fuel, Fertilizer
     CRP mod via CUSTOM_RESOURCE_2:    Intake Air, Liquid Hydrogen,
                                       Liquid Methane, Lithium

   Resources with no stage variant copy vessel values to stage fields (best effort).

   SCENE_CHANGE_MESSAGE (only reached in live mode — demo never services Simpit):
     msg[0]==0 → entering flight scene → slots zeroed, refresh requested, main screen
     msg[0]==1 → leaving flight scene  → config saved, EVA latch + slots cleared, standby

   -------------------------------------------------------------------------------
   REQUIRED: KSP/GameData/KerbalSimpit/PluginData/Settings.cfg
   -------------------------------------------------------------------------------
   Add the following two CustomResourceMessages blocks to Settings.cfg to enable
   the CRP resource channels. The first block maps to CUSTOM_RESOURCE_1_MESSAGE
   and the second to CUSTOM_RESOURCE_2_MESSAGE. Resource names must match exactly
   the names used by Community Resource Pack (CRP).

   CustomResourceMessages
   {
       resourceName1 = StoredCharge
       resourceName2 = EnrichedUranium
       resourceName3 = DepletedFuel
       resourceName4 = Fertilizer
   }
   CustomResourceMessages
   {
       resourceName1 = IntakeAir
       resourceName2 = LqdHydrogen
       resourceName3 = LqdMethane
       resourceName4 = Lithium
   }
   -------------------------------------------------------------------------------
****************************************************************************************/
#include "KCMk1_ResourceDisp.h"


/***************************************************************************************
   HELPER — update all slots matching a resource type
****************************************************************************************/
static void updateSlot(ResourceType t,
                       float vessel, float vesselMax,
                       float stage,  float stageMax) {
  for (uint8_t i = 0; i < slotCount; i++) {
    if (slots[i].type == t) {
      slots[i].current      = vessel;
      slots[i].maxVal       = vesselMax;
      slots[i].stageCurrent = stage;
      slots[i].stageMax     = stageMax;
    }
  }
}

// Vessel-only variant — mirrors vessel to stage (no stage channel available)
static void updateSlotVesselOnly(ResourceType t, float vessel, float vesselMax) {
  updateSlot(t, vessel, vesselMax, vessel, vesselMax);
}


/***************************************************************************************
   SIMPIT MESSAGE HANDLER
****************************************************************************************/
/***************************************************************************************
   FLIGHT-SCENE ENTRY
   Shared by the explicit SCENE_CHANGE and by the inference in FLIGHT_STATUS below, so
   the two routes into a flight scene cannot drift apart.
****************************************************************************************/
static void enterFlightScene() {
  flightScene = true;
  // Zero slots so stale values don't show before Simpit repopulates them
  zeroAllSlotValues();
  // Request immediate refresh on all subscribed channels. Simpit only sends resource
  // messages when values change — without this, static resources (full tanks, idle
  // engines) won't update until first change.
  simpit.requestMessageOnChannel(0);
  switchToScreen(screen_Main);
}


void onSimpitMessage(byte messageType, byte msg[], byte msgSize) {

  if (debugMode) {
    const char *msgName;
    switch (messageType) {
      case VESSEL_NAME_MESSAGE:     msgName = "VESSEL_NAME";  break;
      case LF_MESSAGE:              msgName = "LF";           break;
      case LF_STAGE_MESSAGE:        msgName = "LF_STAGE";     break;
      case OX_MESSAGE:              msgName = "OX";           break;
      case OX_STAGE_MESSAGE:        msgName = "OX_STAGE";     break;
      case SF_MESSAGE:              msgName = "SF";           break;
      case SF_STAGE_MESSAGE:        msgName = "SF_STAGE";     break;
      case MONO_MESSAGE:            msgName = "MONO";         break;
      case XENON_GAS_MESSAGE:       msgName = "XENON";        break;
      case XENON_GAS_STAGE_MESSAGE: msgName = "XENON_STAGE";  break;
      case ELECTRIC_MESSAGE:        msgName = "ELECTRIC";     break;
      case EVA_MESSAGE:             msgName = "EVA";          break;
      case ORE_MESSAGE:             msgName = "ORE";          break;
      case AB_MESSAGE:              msgName = "AB";           break;
      case AB_STAGE_MESSAGE:        msgName = "AB_STAGE";     break;
      case TACLS_RESOURCE_MESSAGE:  msgName = "TACLS_RES";    break;
      case TACLS_WASTE_MESSAGE:     msgName = "TACLS_WASTE";  break;
      case CUSTOM_RESOURCE_1_MESSAGE: msgName = "CUSTOM_1";   break;
      case CUSTOM_RESOURCE_2_MESSAGE: msgName = "CUSTOM_2";   break;
      case SCENE_CHANGE_MESSAGE:    msgName = "SCENE_CHANGE"; break;
      case VESSEL_CHANGE_MESSAGE:   msgName = "VESSEL_CHANGE";break;
      case FLIGHT_STATUS_MESSAGE:   msgName = "FLIGHT_STATUS";break;
      default:                      msgName = "UNKNOWN";      break;
    }
    Serial.print(F("ResourceDisp: Simpit msg="));
    Serial.println(msgName);
  }

  switch (messageType) {

    // -------------------------------------------------------------------------
    // Vessel name — used to key the per-vessel slot cache
    // -------------------------------------------------------------------------

    case VESSEL_NAME_MESSAGE: {
      // Copy to a null-terminated buffer first — Teensy WString does not implement
      // the String(const char*, length) constructor that standard Arduino does.
      char nameBuf[msgSize + 1];
      memcpy(nameBuf, msg, msgSize);
      nameBuf[msgSize] = '\0';
      String newName(nameBuf);
      if (newName != currentVesselName) {
        if (debugMode) { Serial.print(F("ResourceDisp: vessel name = ")); Serial.println(newName); }
        currentVesselName = newName;
        // Attempt to recall this vessel's last slot configuration.
        // If not in cache, keep the current slot layout.
        // On EVA the bar set is fixed (EC/EVA/O2/Food/Water) — skip per-vessel
        // recall so the EVA Kerbal's name can't override it.
        if (evaActive) {
          if (debugMode) Serial.println(F("ResourceDisp: EVA active — keeping EVA bar set"));
        } else if (recallVesselSlots(currentVesselName)) {
          if (debugMode) Serial.println(F("ResourceDisp: vessel slot config recalled"));
          simpit.requestMessageOnChannel(0);
        } else {
          if (debugMode) Serial.println(F("ResourceDisp: vessel not in cache, keeping current layout"));
        }
        // Request chrome redraw regardless — slot count/types may have changed.
        // needsMainRedraw is checked by loop() after simpit.update() returns,
        // keeping display calls out of the message handler.
        if (activeScreen == screen_Main) needsMainRedraw = true;
      }
      break;
    }

    case ELECTRIC_MESSAGE:
      if (msgSize == sizeof(resourceMessage)) {
        resourceMessage r = parseMessage<resourceMessage>(msg);
        updateSlotVesselOnly(RES_ELEC_CHARGE, r.available, r.total);
      }
      break;

    case MONO_MESSAGE:
      if (msgSize == sizeof(resourceMessage)) {
        resourceMessage r = parseMessage<resourceMessage>(msg);
        updateSlotVesselOnly(RES_MONO_PROP, r.available, r.total);
      }
      break;

    case EVA_MESSAGE:
      if (msgSize == sizeof(resourceMessage)) {
        resourceMessage r = parseMessage<resourceMessage>(msg);
        updateSlotVesselOnly(RES_EVA_PROP, r.available, r.total);
      }
      break;

    case ORE_MESSAGE:
      if (msgSize == sizeof(resourceMessage)) {
        resourceMessage r = parseMessage<resourceMessage>(msg);
        updateSlotVesselOnly(RES_ORE, r.available, r.total);
      }
      break;

    // -------------------------------------------------------------------------
    // Resources with separate vessel and stage channels
    // Vessel message fills current/maxVal; stage message fills stageCurrent/stageMax
    // -------------------------------------------------------------------------

    case LF_MESSAGE:
      if (msgSize == sizeof(resourceMessage)) {
        resourceMessage r = parseMessage<resourceMessage>(msg);
        for (uint8_t i = 0; i < slotCount; i++) {
          if (slots[i].type == RES_LIQUID_FUEL) {
            slots[i].current = r.available;
            slots[i].maxVal  = r.total;
          }
        }
      }
      break;

    case LF_STAGE_MESSAGE:
      if (msgSize == sizeof(resourceMessage)) {
        resourceMessage r = parseMessage<resourceMessage>(msg);
        for (uint8_t i = 0; i < slotCount; i++) {
          if (slots[i].type == RES_LIQUID_FUEL) {
            slots[i].stageCurrent = r.available;
            slots[i].stageMax     = r.total;
          }
        }
      }
      break;

    case OX_MESSAGE:
      if (msgSize == sizeof(resourceMessage)) {
        resourceMessage r = parseMessage<resourceMessage>(msg);
        for (uint8_t i = 0; i < slotCount; i++) {
          if (slots[i].type == RES_LIQUID_OX) {
            slots[i].current = r.available;
            slots[i].maxVal  = r.total;
          }
        }
      }
      break;

    case OX_STAGE_MESSAGE:
      if (msgSize == sizeof(resourceMessage)) {
        resourceMessage r = parseMessage<resourceMessage>(msg);
        for (uint8_t i = 0; i < slotCount; i++) {
          if (slots[i].type == RES_LIQUID_OX) {
            slots[i].stageCurrent = r.available;
            slots[i].stageMax     = r.total;
          }
        }
      }
      break;

    case SF_MESSAGE:
      if (msgSize == sizeof(resourceMessage)) {
        resourceMessage r = parseMessage<resourceMessage>(msg);
        for (uint8_t i = 0; i < slotCount; i++) {
          if (slots[i].type == RES_SOLID_FUEL) {
            slots[i].current = r.available;
            slots[i].maxVal  = r.total;
          }
        }
      }
      break;

    case SF_STAGE_MESSAGE:
      if (msgSize == sizeof(resourceMessage)) {
        resourceMessage r = parseMessage<resourceMessage>(msg);
        for (uint8_t i = 0; i < slotCount; i++) {
          if (slots[i].type == RES_SOLID_FUEL) {
            slots[i].stageCurrent = r.available;
            slots[i].stageMax     = r.total;
          }
        }
      }
      break;

    case XENON_GAS_MESSAGE:
      if (msgSize == sizeof(resourceMessage)) {
        resourceMessage r = parseMessage<resourceMessage>(msg);
        for (uint8_t i = 0; i < slotCount; i++) {
          if (slots[i].type == RES_XENON) {
            slots[i].current = r.available;
            slots[i].maxVal  = r.total;
          }
        }
      }
      break;

    case XENON_GAS_STAGE_MESSAGE:
      if (msgSize == sizeof(resourceMessage)) {
        resourceMessage r = parseMessage<resourceMessage>(msg);
        for (uint8_t i = 0; i < slotCount; i++) {
          if (slots[i].type == RES_XENON) {
            slots[i].stageCurrent = r.available;
            slots[i].stageMax     = r.total;
          }
        }
      }
      break;

    case AB_MESSAGE:
      if (msgSize == sizeof(resourceMessage)) {
        resourceMessage r = parseMessage<resourceMessage>(msg);
        for (uint8_t i = 0; i < slotCount; i++) {
          if (slots[i].type == RES_ABLATOR) {
            slots[i].current = r.available;
            slots[i].maxVal  = r.total;
          }
        }
      }
      break;

    case AB_STAGE_MESSAGE:
      if (msgSize == sizeof(resourceMessage)) {
        resourceMessage r = parseMessage<resourceMessage>(msg);
        for (uint8_t i = 0; i < slotCount; i++) {
          if (slots[i].type == RES_ABLATOR) {
            slots[i].stageCurrent = r.available;
            slots[i].stageMax     = r.total;
          }
        }
      }
      break;

    // -------------------------------------------------------------------------
    // TAC Life Support resources (vessel-only, no stage variant)
    // TACLS_RESOURCE_MESSAGE: Food, Water, Oxygen
    // TACLS_WASTE_MESSAGE:    Waste, Liquid Waste, CO2
    // -------------------------------------------------------------------------

    case TACLS_RESOURCE_MESSAGE:
      if (msgSize == sizeof(TACLSResourceMessage)) {
        TACLSResourceMessage r = parseMessage<TACLSResourceMessage>(msg);
        updateSlotVesselOnly(RES_LS_FOOD,   r.currentFood,   r.maxFood);
        updateSlotVesselOnly(RES_LS_WATER,  r.currentWater,  r.maxWater);
        updateSlotVesselOnly(RES_LS_OXYGEN, r.currentOxygen, r.maxOxygen);
      }
      break;

    case TACLS_WASTE_MESSAGE:
      if (msgSize == sizeof(TACLSWasteMessage)) {
        TACLSWasteMessage r = parseMessage<TACLSWasteMessage>(msg);
        updateSlotVesselOnly(RES_LS_WASTE,        r.currentWaste,       r.maxWaste);
        updateSlotVesselOnly(RES_LS_LIQUID_WASTE, r.currentLiquidWaste, r.maxLiquidWaste);
        updateSlotVesselOnly(RES_LS_CO2,          r.currentCO2,         r.maxCO2);
      }
      break;

    // -------------------------------------------------------------------------
    // Custom Resource channels (CRP mod, configured in KerbalSimpit Settings.cfg)
    // Slot assignments (configure these resource names in Settings.cfg):
    //   Resource1 = StoredCharge
    //   Resource2 = EnrichedUranium
    //   Resource3 = DepletedFuel
    //   Resource4 = Fertilizer
    // -------------------------------------------------------------------------

    case CUSTOM_RESOURCE_1_MESSAGE:
      if (msgSize == sizeof(CustomResourceMessage)) {
        CustomResourceMessage r = parseMessage<CustomResourceMessage>(msg);
        updateSlotVesselOnly(RES_STORED_CHARGE,    r.currentResource1, r.maxResource1);
        updateSlotVesselOnly(RES_ENRICHED_URANIUM, r.currentResource2, r.maxResource2);
        updateSlotVesselOnly(RES_DEPLETED_URANIUM,  r.currentResource3, r.maxResource3);
        updateSlotVesselOnly(RES_FERTILIZER,        r.currentResource4, r.maxResource4);
      }
      break;

    case CUSTOM_RESOURCE_2_MESSAGE:
      // Configure in KerbalSimpit Settings.cfg:
      //   CustomResource2_1 = IntakeAir
      //   CustomResource2_2 = LqdHydrogen
      //   CustomResource2_3 = LqdMethane
      //   CustomResource2_4 = Lithium
      if (msgSize == sizeof(CustomResourceMessage)) {
        CustomResourceMessage r = parseMessage<CustomResourceMessage>(msg);
        updateSlotVesselOnly(RES_INTAKE_AIR,      r.currentResource1, r.maxResource1);
        updateSlotVesselOnly(RES_LIQUID_H2,       r.currentResource2, r.maxResource2);
        updateSlotVesselOnly(RES_LIQUID_METHANE,  r.currentResource3, r.maxResource3);
        updateSlotVesselOnly(RES_LITHIUM,         r.currentResource4, r.maxResource4);
      }
      break;

    // -------------------------------------------------------------------------
    // Scene and vessel events
    // -------------------------------------------------------------------------

    case FLIGHT_STATUS_MESSAGE:
      // EVA detection: the FLIGHT_IS_EVA bit of the flags byte (msg[0]) is set
      // while a Kerbal is on EVA. Latch it into evaFlag; loop() reconciles it
      // into evaActive and swaps between the vessel bars and the EVA bar set.
      if (msgSize >= 1) evaFlag = (msg[0] & FLIGHT_IS_EVA) != 0;
      // Simpit sends FLIGHT_STATUS only from a flight scene, so receiving it while we
      // believe we are not in one means the SCENE_CHANGE that would have told us never
      // arrived — the normal case when a panel boots into a flight already in progress.
      // SCENE_CHANGE is an event, not a state you can ask for, so without this the
      // panel would sit on the standby splash until the pilot changed scene or vessel.
      if (!flightScene) {
        if (debugMode) Serial.println(F("ResourceDisp: flight scene inferred from FLIGHT_STATUS"));
        enterFlightScene();
      }
      break;

    case SCENE_CHANGE_MESSAGE:
      if (msgSize < 1) break;
      // 0 = entering flight, 1 = leaving flight
      if (msg[0] == 0) {
        if (debugMode) Serial.println(F("ResourceDisp: Simpit entering flight scene"));
        enterFlightScene();
      } else {
        flightScene = false;
        if (debugMode) Serial.println(F("ResourceDisp: Simpit leaving flight scene"));
        // Save current config before leaving flight
        saveVesselSlots(currentVesselName);
        // Clear EVA latch so an EVA that ended off-scene doesn't stay engaged, then
        // zero slots and return to standby.
        evaFlag = evaActive = false;
        zeroAllSlotValues();
        switchToScreen(screen_Standby);
      }
      break;

    case VESSEL_CHANGE_MESSAGE:
      if (msgSize < 1) break;
      if (msg[0] == 1) {
        if (debugMode) Serial.println(F("ResourceDisp: Simpit vessel switch — saving and zeroing slots"));
        // Save current config before it's overwritten by the new vessel
        saveVesselSlots(currentVesselName);
        currentVesselName = "";  // will be repopulated by VESSEL_NAME_MESSAGE
        // Clear the EVA latch — if the new vessel is still an EVA Kerbal, the next
        // FLIGHT_STATUS re-sets evaFlag and loop() reconciles it back on.
        evaFlag = evaActive = false;
        zeroAllSlotValues();
        // Request main screen redraw via flag — loop() will call drawStaticMain()
        // after simpit.update() returns, ensuring the screen is cleared before any
        // subsequent resource messages for the new vessel are drawn.
        if (activeScreen == screen_Main) {
          needsMainRedraw = true;
        } else {
          switchToScreen(screen_Main);
        }
      }
      break;
  }
}


/***************************************************************************************
   INIT SIMPIT
   Registers all supported resource channels. All channels are subscribed upfront
   so switching presets works without needing to re-register.
   Channels that don't match any active slot type are silently ignored by KSP.
****************************************************************************************/
void initSimpit() {
  simpit.inboundHandler(onSimpitMessage);
  while (!simpit.init()) {
    if (debugMode) Serial.println(F("ResourceDisp: Simpit handshake failed, retrying..."));
    delay(500);
  }
  if (debugMode) Serial.println(F("ResourceDisp: Simpit connected."));
  simpitConnected = true;

  // Native propellants — vessel totals
  simpit.registerChannel(LF_MESSAGE);
  simpit.registerChannel(OX_MESSAGE);
  simpit.registerChannel(SF_MESSAGE);
  simpit.registerChannel(MONO_MESSAGE);
  simpit.registerChannel(XENON_GAS_MESSAGE);

  // Native propellants — stage values
  simpit.registerChannel(LF_STAGE_MESSAGE);
  simpit.registerChannel(OX_STAGE_MESSAGE);
  simpit.registerChannel(SF_STAGE_MESSAGE);
  simpit.registerChannel(XENON_GAS_STAGE_MESSAGE);

  // Power and mining
  simpit.registerChannel(ELECTRIC_MESSAGE);
  simpit.registerChannel(EVA_MESSAGE);
  simpit.registerChannel(ORE_MESSAGE);
  simpit.registerChannel(AB_MESSAGE);
  simpit.registerChannel(AB_STAGE_MESSAGE);

  // TAC Life Support (requires ARP + TACLS mods)
  simpit.registerChannel(TACLS_RESOURCE_MESSAGE);
  simpit.registerChannel(TACLS_WASTE_MESSAGE);

  // Custom resources (requires ARP + CRP; configure names in KerbalSimpit Settings.cfg)
  // CUSTOM_1: Resource1=StoredCharge, Resource2=EnrichedUranium, Resource3=DepletedFuel, Resource4=Fertilizer
  // CUSTOM_2: Resource1=IntakeAir, Resource2=LqdHydrogen, Resource3=LqdMethane, Resource4=Lithium
  simpit.registerChannel(CUSTOM_RESOURCE_1_MESSAGE);
  simpit.registerChannel(CUSTOM_RESOURCE_2_MESSAGE);

  // Scene, vessel events, and vessel name (for slot cache key)
  simpit.registerChannel(VESSEL_NAME_MESSAGE);
  simpit.registerChannel(SCENE_CHANGE_MESSAGE);
  simpit.registerChannel(VESSEL_CHANGE_MESSAGE);
  simpit.registerChannel(FLIGHT_STATUS_MESSAGE);   // EVA detection (FLIGHT_IS_EVA bit)
}

