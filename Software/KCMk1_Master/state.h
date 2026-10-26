/***************************************************************************************
   state.h — the state everything else reads (design §4.1).

   Each struct has one writer:
     g_vessel  — simpit_link (what KSP reports)
     g_panel   — the panel handlers (switch positions and the master's own modes)
     g_master  — the master core (boot, links, demo / debug, faults)
   Readers never write. stateReset() puts all three back to their power-on values; it is
   called first in setup(), so a restart never inherits stale values.
****************************************************************************************/
#pragma once
#include <Arduino.h>

struct VesselState {
  bool     flightScene;     // SCENE_CHANGE: in a flight scene
  bool     inFlight;        // FLIGHT_STATUS: vessel in flight
  bool     eva;
  bool     hasTarget;
  uint8_t  twIndex;         // time-warp index, 0 = 1x
  uint8_t  situation;
  uint8_t  vesselType;
  uint8_t  actions;         // ACTIONSTATUS: *_ACTION bits (gear, lights, RCS, SAS, brakes, …)
  bool     actionsValid;    // an ACTIONSTATUS arrived since this flight scene began
  uint32_t lastRxMs;        // millis() of the last inbound Simpit message
  uint32_t rxCount;
};

// Switch positions are written by switches.cpp; until a switch is read it keeps the
// safe value below (ENGINE SAFE, A/P INHIB, CHUTE SAFE).
struct PanelState {
  uint8_t ctrlMode;         // CTRL_MODE_* (mode selector)
  uint8_t ctrlGrp;          // 1..6 (Dual Encoder ENC1, stage 4)
  bool    audio;            // AUDIO switch; LIVE until read
  bool    apArm;            // A/P INHIB / ARM
  bool    engArm;           // ENGINE SAFE / ARM (the switch; interlocks.h has the lever sync)
  bool    trim;             // master trim set
  bool    thrPrec;          // THRTL STD / FINE (the switch)
  bool    inputPrec;        // THC / RHC PREC
  bool    chuteArm;         // CHUTE SAFE / ARM
  bool    brakeLock;        // BRAKE REL / LOCK
  bool    switchErr;        // any switch disagreeing with the vessel
  bool    autopilot;        // any autopilot or HOLD engaged
};

enum BootPhase : uint8_t {
  BOOT_PHASE_SETUP = 0,     // inside setup()
  BOOT_PHASE_WAIT_KSP,      // loop running, Sys Info still on its boot screen
  BOOT_PHASE_RUN,           // boot screen released (KSP connected or demo)
  BOOT_PHASE_HALT           // watchdog reboot-loop guard tripped (design §12.3)
};

struct MasterState {
  BootPhase bootPhase;
  bool     demo;            // SCE AUX
  bool     debug;           // UPTLM XMIT, or the console until the next flip
  bool     idle;            // display idle_state bit
  bool     kspConnected;    // the master's own Simpit handshake succeeded
  bool     simpitLost;      // connected, in flight, nothing inbound for SIMPIT_LOST_MS
  uint8_t  displayLinks;    // bit per display with its own Simpit link up (carriers.h)
  uint8_t  displaysKnown;   // bit per display that answered its status read
  bool     sysInfoPresent;
  uint32_t setupDoneMs;
};

extern VesselState g_vessel;
extern PanelState  g_panel;
extern MasterState g_master;

void stateReset();
