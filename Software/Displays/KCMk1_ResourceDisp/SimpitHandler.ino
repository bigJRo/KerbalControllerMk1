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
   HELPER — apply one resource message to every slot of that type
   stage=true writes the stage fields; stage=false writes the vessel fields and, for a
   resource with no stage channel (mirror=true), copies them to the stage fields so
   the stage side is never left at zero. Every message stamps updatedMs for the
   refresh tracking in Sampling.ino.
****************************************************************************************/
static void applyResource(ResourceType t, bool stage, bool mirror, float avail, float total) {
  uint32_t now = millis();
  if (!stage) noteResourcePresence(t, avail, total);   // vessel messages decide presence
  for (uint8_t i = 0; i < slotCount; i++) {
    if (slots[i].type != t) continue;
    if (stage) {
      slots[i].stageCurrent = avail;
      slots[i].stageMax     = total;
    } else {
      slots[i].current = avail;
      slots[i].maxVal  = total;
      if (mirror) {
        slots[i].stageCurrent = avail;
        slots[i].stageMax     = total;
      }
    }
    slots[i].updatedMs = now;
  }
}

// Vessel-only resource — mirrors vessel to stage (no stage channel available)
static void updateSlotVesselOnly(ResourceType t, float vessel, float vesselMax) {
  applyResource(t, false, true, vessel, vesselMax);
}


/***************************************************************************************
   SINGLE-RESOURCE CHANNEL TABLE
   Every channel that carries one resourceMessage. The handler walks this before its
   switch, so adding a native resource is one line here plus a registerChannel().
****************************************************************************************/
struct ResChannel {
  byte         msgType;
  ResourceType type;
  bool         stage;    // this message is the stage variant
  bool         mirror;   // vessel message also fills the stage fields (no stage channel)
};

static const ResChannel RES_CHANNELS[] = {
  { LF_MESSAGE,              RES_LIQUID_FUEL, false, false },
  { LF_STAGE_MESSAGE,        RES_LIQUID_FUEL, true,  false },
  { OX_MESSAGE,              RES_LIQUID_OX,   false, false },
  { OX_STAGE_MESSAGE,        RES_LIQUID_OX,   true,  false },
  { SF_MESSAGE,              RES_SOLID_FUEL,  false, false },
  { SF_STAGE_MESSAGE,        RES_SOLID_FUEL,  true,  false },
  { XENON_GAS_MESSAGE,       RES_XENON,       false, false },
  { XENON_GAS_STAGE_MESSAGE, RES_XENON,       true,  false },
  { AB_MESSAGE,              RES_ABLATOR,     false, false },
  { AB_STAGE_MESSAGE,        RES_ABLATOR,     true,  false },
  { ELECTRIC_MESSAGE,        RES_ELEC_CHARGE, false, true  },
  { MONO_MESSAGE,            RES_MONO_PROP,   false, true  },
  { EVA_MESSAGE,             RES_EVA_PROP,    false, true  },
  { ORE_MESSAGE,             RES_ORE,         false, true  },
};
static const uint8_t RES_CHANNEL_COUNT = sizeof(RES_CHANNELS) / sizeof(RES_CHANNELS[0]);


/***************************************************************************************
   REQUEST RESOURCE REFRESH
   Simpit only sends a resource message when the value changes, so after any slot
   change the panel asks for every channel again. Stamps the request so Sampling.ino
   can tell "waiting for the answer" from "not aboard". No-op in demo mode, where
   there is no Simpit link and the demo generator drives every slot.
****************************************************************************************/
void requestResourceRefresh() {
  if (demoMode) return;
  refreshPending   = true;
  refreshRequestMs = millis();
  // Presence is NOT reset here: the last known answer stands until a new one
  // arrives, so a refresh after a Select change does not flash absent meters back
  // as "..." for three seconds. A vessel switch or scene entry resets it, since
  // the new vessel's answers may differ.
  simpit.requestMessageOnChannel(0);
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
  // Zero slots so stale values don't show before Simpit repopulates them, and
  // forget which resources the previous vessel carried.
  zeroAllSlotValues();
  resetResourcePresence();
  resetHistory();
  // Request immediate refresh on all subscribed channels. Simpit only sends resource
  // messages when values change — without this, static resources (full tanks, idle
  // engines) won't update until first change.
  requestResourceRefresh();
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

  // Single-resource channels: one table lookup covers the native propellants,
  // their stage variants, and the vessel-only resources.
  for (uint8_t i = 0; i < RES_CHANNEL_COUNT; i++) {
    if (RES_CHANNELS[i].msgType != messageType) continue;
    if (msgSize == sizeof(resourceMessage)) {
      resourceMessage r = parseMessage<resourceMessage>(msg);
      applyResource(RES_CHANNELS[i].type, RES_CHANNELS[i].stage, RES_CHANNELS[i].mirror,
                    r.available, r.total);
    }
    return;
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
        // A vessel in memory gets its own layout back; one that is not starts from
        // the default (the pilot's stored one, else SPCT), so a new craft never
        // inherits whatever the previous vessel happened to show. On EVA the bar
        // set is fixed (EC/EVA/O2/Food/Water) — skip both so the EVA Kerbal's name
        // can't override it.
        if (evaActive) {
          if (debugMode) Serial.println(F("ResourceDisp: EVA active — keeping EVA bar set"));
        } else if (recallVesselSlots(currentVesselName)) {
          if (debugMode) Serial.println(F("ResourceDisp: vessel slot config recalled"));
          layoutRecalled = true;
          requestResourceRefresh();
        } else {
          if (debugMode) Serial.println(F("ResourceDisp: vessel not in memory, default layout"));
          layoutRecalled = false;
          initDefaultSlots();   // also requests a refresh
        }
        // Request chrome redraw regardless — slot count/types may have changed.
        // needsMainRedraw is checked by loop() after simpit.update() returns,
        // keeping display calls out of the message handler.
        if (activeScreen == screen_Main) needsMainRedraw = true;
      }
      break;
    }

    // Single-resource channels are handled by the table walk above the switch.

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
      // Time warp: byte 2 is KSP's warp rate index. KSP only allows physics warp
      // inside an atmosphere (flag bit 3, FLIGHT_IS_ATMO_TW), so the flag
      // selects which rate table the index refers to. The sampling windows are
      // restarted on a change so no window blends two rates.
      if (msgSize >= 3) {
        static const float RAILS[8] = { 1, 5, 10, 50, 100, 1000, 10000, 100000 };
        static const float PHYS[4]  = { 1, 2, 3, 4 };
        uint8_t idx    = msg[2];
        bool    inAtmo = (msg[0] & 0x08) != 0;
        float   wf     = inAtmo ? PHYS[idx < 4 ? idx : 3] : RAILS[idx < 8 ? idx : 7];
        if (wf != warpFactor) {
          warpFactor = wf;
          resetAllSampling();
          if (debugMode) { Serial.print(F("ResourceDisp: warp factor ")); Serial.println(warpFactor); }
        }
      }
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
        // Save current config before leaving flight, and write the memory out now,
        // while a few milliseconds' hitch costs nothing. On EVA the slots are the
        // fixed EVA set, not this vessel's layout, so there is nothing to save.
        if (!evaActive) saveVesselSlots(currentVesselName);
        persistStoreNow();
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
        // Save current config before it's overwritten by the new vessel, and write
        // the memory out now. On EVA the slots are the fixed EVA set, not a layout
        // worth remembering under the Kerbal's name.
        if (!evaActive) saveVesselSlots(currentVesselName);
        persistStoreNow();
        currentVesselName = "";  // will be repopulated by VESSEL_NAME_MESSAGE
        layoutRecalled    = false;
        // Clear the EVA latch — if the new vessel is still an EVA Kerbal, the next
        // FLIGHT_STATUS re-sets evaFlag and loop() reconciles it back on.
        evaFlag = evaActive = false;
        zeroAllSlotValues();
        resetResourcePresence();   // a different vessel carries different resources
        resetHistory();
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

