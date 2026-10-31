/***************************************************************************************
   module_bus.h — the ATtiny816 input modules (design §6; I2C spec §4–§12).

   One table row per module. At boot each module is identified, its INT line learned,
   its LEDs set OFF and it is put in DISABLED. After that the bus:
     - reads every module whose INT line is low (or polls one with no known line);
     - checks each packet (type, lifecycle, fault, transaction counter);
     - turns button packets into events (pressed / released / tapped / held) and hands
       every packet to that module's handler (the panel tabs, from stage 3);
     - keeps each module's lifecycle where it should be: ACTIVE in a flight scene or in
       demo, DISABLED otherwise; a module that reboots is brought back;
     - sends LED changes, at most every MODBUS_LED_MIN_MS per module;
     - re-probes absent modules every MODBUS_REPROBE_MS.

   Panel switches (stage 3): a module that is not ACTIVE asserts no INT but still answers
   a read with its current inputs (KerbalButtonCore keeps debouncing). The three modules
   that carry switches (Function: Switch Group 1, Vehicle: Switch Group 2, Time: the mode
   selector) are polled every MODBUS_SWITCH_POLL_MS while not ACTIVE, so demo, debug and
   the interlocks follow their switches in any scene. moduleInputs() gives the levels from
   the last packet of any module (whatever its lifecycle), moduleTakePressed() the presses
   since last taken (so a quick flick of a momentary switch between two reads is not lost).
   Events (ModHandler) are still only delivered while a module is ACTIVE.

   INT line learning (§6.2): at power-on every module holds INT low until its first data
   read, so reading the modules one at a time shows which line each one is on. The
   display carriers are learned the same way after they are released: a STATUS request
   makes a carrier pull its INT low. The map is kept in EEPROM for restarts (when the
   modules are already running and not signalling); config.h's defaults cover a first
   boot.
****************************************************************************************/
#pragma once
#include <Arduino.h>

enum ModId : uint8_t {
  MOD_UI = 0, MOD_FUNCTION, MOD_ACTION, MOD_STABILITY, MOD_VEHICLE, MOD_TIME, MOD_AUX,
  MOD_ROTATION, MOD_TRANSLATION, MOD_GPWS, MOD_PREWARP, MOD_THROTTLE, MOD_DUALENC, MOD_COUNT
};

// Owners of an INT line: a module (ModId), a display carrier, or nobody.
static const uint8_t OWNER_ANNUNCIATOR = 0x40, OWNER_INFODISP_1 = 0x41, OWNER_INFODISP_2 = 0x42,
                     OWNER_RESDISP = 0x43, OWNER_SYSINFO = 0x44, OWNER_NONE = 0xFF;

enum ModKind : uint8_t { KIND_BUTTONS, KIND_JOYSTICK, KIND_DISPLAY, KIND_THROTTLE, KIND_ENCODER };

enum MapSource : uint8_t { SRC_DEFAULT = 0, SRC_STORED, SRC_LEARNED };

static const uint32_t MODBUS_LED_MIN_MS  = 50;
static const uint32_t MODBUS_POLL_MS     = 20;     // module with no known INT line
static const uint32_t MODBUS_REPROBE_MS  = 5000;
static const uint32_t MODBUS_HOLD_MS     = 2000;   // a "held" event (state-machine reset)
static const uint32_t MODBUS_SWITCH_POLL_MS = 100;  // switch modules while not ACTIVE
static const uint32_t MODBUS_BULB_MIN_MS    = 500;  // shortest bulb test, so a flick of LTG still shows

// What a handler gets for each packet (and for a hold, with only `held` set).
struct ModEvent {
  ModId    id;
  const uint8_t *pkt;       // the whole packet (header + payload); nullptr for a hold
  uint8_t  len;
  uint32_t down;            // buttons / inputs down now (button, joystick and encoder kinds)
  uint32_t pressed;         // went down (including a re-press between two reads)
  uint32_t released;        // went up after the master saw it down
  uint32_t tapped;          // down and up again between two reads
  uint32_t held;            // down for MODBUS_HOLD_MS (reported once per press)
};
typedef void (*ModHandler)(const ModEvent &e);

void     moduleBusInit();
void     moduleBusBoot();                       // boot steps 5–6: INT map, discovery and learning
void     moduleBusLearnCarriers();              // after the carriers are released (boot step 7)
void     moduleBusReadSwitches();               // boot: one read of each switch module
void     moduleBusService();
void     moduleBusFlushLeds();
void     moduleBusSetHandler(ModId id, ModHandler h);

bool     modulePresent(ModId id);
bool     moduleHasLeds(ModId id);
uint8_t  moduleAddr(ModId id);
const char *moduleName(ModId id);
uint8_t  moduleLifecycle(ModId id);             // as last reported (KMC_STATUS_*)
bool     moduleWantActive();                    // the lifecycle the modules should be in

void     moduleSetLed(ModId id, uint8_t button, uint8_t state);   // KMC_LED_*
uint8_t  moduleLed(ModId id, uint8_t button);
void     moduleSetAllLeds(ModId id, uint8_t state);
void     moduleBulbTest(bool on);
void     moduleResetAll();                      // CMD_RESET to every module (MSTR RESET)
void     moduleRequestRead(ModId id);           // read it on the next pass

// Inputs, whatever the lifecycle (see above). moduleInputs() is false while the levels
// are unknown; `epoch` changes each time they were lost, so a reader can re-baseline.
bool     moduleInputs(ModId id, uint32_t *levels, uint8_t *epoch = nullptr);
uint32_t moduleTakePressed(ModId id, uint32_t mask);
uint8_t  moduleLastPacket(ModId id, uint8_t *buf, uint32_t *ms = nullptr);   // its length, 0 if none
bool     moduleCommand(ModId id, uint8_t cmd, const uint8_t *payload = nullptr, uint8_t n = 0);

uint8_t  intLineOwner(uint8_t line);            // line index 0..23 → owner
uint8_t  intLineOf(uint8_t owner);              // owner → line index, or 0xFF
MapSource intLineSource(uint8_t line);
void     intMapClearStored();                   // erase the EEPROM copy (back to defaults next boot)
