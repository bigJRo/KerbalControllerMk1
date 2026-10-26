/***************************************************************************************
   config.h — KCMk1 Master: pins, addresses and timings, all in one place.

   Hardware truth: Documents/Developer/Hardware_Inventory.md (from the KC-01-1701 V2.0
   Ctrl Module and KC-01-1711 V2.0 Extension schematics). Design:
   Documents/Developer/Master_Firmware_Design.md.

   Licensed under the GNU General Public License v3.0 (GPL-3.0).
   Written for Jeb's Controller Works.
****************************************************************************************/
#pragma once
#include <Arduino.h>

#define KCM_MASTER_VERSION "0.6.0"
static const uint8_t KCM_MASTER_VERSION_MAJOR = 0;
static const uint8_t KCM_MASTER_VERSION_MINOR = 6;

/***************************************************************************************
   Pins — Teensy 4.1 on the Ctrl Module (Inventory §2–§3, design Appendix A)
****************************************************************************************/
// Module interrupt lines, in line order. A-side lines reach the Ctrl Module directly
// (INT_1–8: A2 Panel Hub slots P1–P8; INT_9–11: Info Display 1 slots P2–P4; INT_12:
// Info Display 1 itself). B-side lines come through the Extension (B_INT_1–8: B2
// Panel Hub; B_INT_9–11: Resource Display slots; B_INT_12: Resource Display).
static const uint8_t INT_LINE_COUNT = 24;
static const uint8_t PIN_INT_LINE[INT_LINE_COUNT] = {
  9, 10, 25, 26, 27, 28, 29, 30,          // INT_1  … INT_8
  4, 5, 6, 7,                             // INT_9  … INT_12
  23, 22, 21, 20, 15, 14, 41, 40,         // B_INT_1 … B_INT_8
  39, 38, 37, 36                          // B_INT_9 … B_INT_12
};
static const char *const INT_LINE_NAME[INT_LINE_COUNT] = {
  "INT_1", "INT_2", "INT_3", "INT_4", "INT_5", "INT_6", "INT_7", "INT_8",
  "INT_9", "INT_10", "INT_11", "INT_12",
  "B_INT_1", "B_INT_2", "B_INT_3", "B_INT_4", "B_INT_5", "B_INT_6", "B_INT_7", "B_INT_8",
  "B_INT_9", "B_INT_10", "B_INT_11", "B_INT_12"
};

// Safe-switch lines: plain inputs, active-HIGH (10k/20k divider on the hub; no pull-up).
static const uint8_t PIN_SAFE_ABORT  = 31;   // INT_SAFE    (A2 Panel Hub)  — ABORT
static const uint8_t PIN_SAFE_A1     = 8;    // INT_SAFE2   (A1)            — unused
static const uint8_t PIN_SAFE_STAGE  = 35;   // B_INT_SAFE  (B2 Panel Hub)  — STAGE
static const uint8_t PIN_SAFE_B1     = 34;   // B_INT_SAFE2 (B1)            — unused

static const uint8_t PIN_RST         = 2;    // global reset to the 7" displays (out, idle high)
static const uint8_t PIN_SDA_BUS     = 18;   // Wire  (BUS)
static const uint8_t PIN_SCL_BUS     = 19;
static const uint8_t PIN_EXT_INT_1   = 33;   // expansion — reserved, left as input
static const uint8_t PIN_EXT_INT_2   = 32;   // expansion — reserved, left as input

/***************************************************************************************
   I2C addresses and display sync bytes. The display values mirror
   Software/Displays/libraries/KCMk1_SystemConfig (the selftest checks they agree).
****************************************************************************************/
static const uint8_t ADDR_ANNUNCIATOR = 0x10;
static const uint8_t ADDR_RESDISP     = 0x11;
static const uint8_t ADDR_INFODISP_1  = 0x12;
static const uint8_t ADDR_INFODISP_2  = 0x13;
static const uint8_t ADDR_SYSINFO     = 0x14;
static const uint8_t ADDR_INA228      = 0x40;
static const uint8_t ADDR_EMC2101     = 0x4C;

static const uint8_t SYNC_ANNUNCIATOR = 0xAC;
static const uint8_t SYNC_RESDISP     = 0xAD;
static const uint8_t SYNC_INFODISP    = 0xAE;
static const uint8_t SYNC_SYSINFO     = 0xAF;

static const uint32_t I2C_CLOCK_HZ    = 400000;

/***************************************************************************************
   Timings
****************************************************************************************/
static const uint32_t SIMPIT_RETRY_MS        = 2000;   // handshake retry while KSP is down
static const uint32_t SIMPIT_LOST_MS         = 5000;   // in flight, no inbound message → SIMPIT LOST
static const uint32_t CARRIER_PUSH_MS        = 100;    // display commands
static const uint32_t CARRIER_STATUS_MS      = 500;    // display status reads (link state)
static const uint32_t SYSINFO_PROBE_MS       = 3000;   // boot: wait this long for the Sys Info Display
static const uint32_t SYSINFO_STATUS_MS      = 500;    // status frame / heartbeat
static const uint32_t SYSINFO_REPROBE_MS     = 5000;   // re-check an absent Sys Info Display
static const uint32_t CARRIER_BOOT_WAIT_MS   = 500;    // boot: wait for each display's status
static const uint32_t BOOT_KSP_HINT_MS       = 60000;  // add the "KSP running?" hint
static const uint32_t BOOT_WAIT_UPDATE_MS    = 1000;   // refresh the WAITING FOR KSP line

static const float    WDT_TIMEOUT_S          = 5.0f;   // design §12.3
static const float    WDT_WARN_BEFORE_S      = 0.5f;   // trigger callback this long before the reset
static const uint8_t  WDT_LOOP_GUARD_COUNT   = 3;      // this many watchdog resets …
static const uint32_t WDT_LOOP_GUARD_S       = 60;     // … within this many seconds → halt
static const uint32_t WDT_STREAK_CLEAR_MS    = 60000;  // a clean run this long clears the streak

// KSP Vessel.Situations bits, as FLIGHT_STATUS carries them
static const uint8_t KSP_SIT_LANDED = 0x01, KSP_SIT_SPLASHED = 0x02, KSP_SIT_PRELAUNCH = 0x04;

// Annunciator control mode (its CtrlMode enum)
static const uint8_t CTRL_MODE_RVR = 0, CTRL_MODE_PLN = 1, CTRL_MODE_SPC = 2;
