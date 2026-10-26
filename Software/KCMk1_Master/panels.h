/***************************************************************************************
   panels.h — the panel tabs (panel_*.cpp) and the rules they share (design §8.1).

   Each tab registers its module's event handler (module_bus) and sets that module's
   LEDs every pass from the current state, so a LED always shows what is true now; the
   bus sends only changes.

   Shared rules:
     - A press reaches KSP only when the panels are live: a flight scene, the link up,
       demo off (panelLive()). ksp_out's gate stops everything else anyway.
     - LEDs (panelLed()): OFF when the button cannot be used now (its press is then
       ignored); ACTIVE while KSP reports its function on; ENABLED (backlit) otherwise.
     - Demo (design §10.1): every usable button sits backlit and shows ACTIVE while it is
       held; switches still gate their buttons on their own panel (CHUTE, SAS). Nothing
       is sent.
     - Buttons whose state KSP does not report flash ACTIVE for PANEL_FLASH_MS on a
       press (panelFlash()).
   Stage 4 tabs: Vehicle, Stability (with HOLD), Action, AUX, Dual Encoder.
****************************************************************************************/
#pragma once
#include <Arduino.h>
#include "module_bus.h"

static const uint32_t PANEL_FLASH_MS = 150;

void panelsInit();                   // registers every tab's handler
void panelsService();                // LEDs and timed work, every pass (before the LED flush)
void panelsSetEnabled(bool on);      // off: tabs ignore events and leave the LEDs alone
                                     // (the bare module-bus selftest; bench LED work)
bool panelsEnabled();

bool panelLive();                    // presses may act on KSP
bool panelDemo();

// Buttons held now on a module, as the tabs last saw them (for demo LEDs).
uint32_t panelDown(ModId id);
void     panelNoteEvent(const ModEvent &e);   // every tab calls this first

// Sets one button's LED by the shared rule: `kspOn` (or a flash) when live; in demo,
// ACTIVE while the button is held.
void panelLed(ModId id, uint8_t button, bool usable, bool kspOn);
void panelFlash(ModId id, uint8_t button);    // ACTIVE for PANEL_FLASH_MS
bool panelFlashing(ModId id, uint8_t button);

// Per-tab entry points (panel_*.cpp).
void panelVehicleInit();   void panelVehicleService();
void panelStabilityInit(); void panelStabilityService();
void panelActionInit();    void panelActionService();
void panelAuxInit();       void panelAuxService();
void panelEncodersInit();

// Vehicle: brakes held by a button (BRAKES, or the RVR stick button from stage 5).
// While any source holds them the brakes are on; when the last lets go they come off
// unless BRAKE is on LOCK (Inventory §8.4).
enum : uint8_t { BRAKE_SRC_BUTTON = 0x01, BRAKE_SRC_STICK = 0x02 };
void vehicleBrakeHold(uint8_t source, bool held);
