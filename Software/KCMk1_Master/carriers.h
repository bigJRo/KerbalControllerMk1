/***************************************************************************************
   carriers.h — the four 7" display carriers (design §13; I2C spec §15.3).

   Every CARRIER_PUSH_MS, and at once when a field changes, each display gets its
   command with requestType PROCEED (harmless to repeat; it releases a display that
   boots after the master or reboots by itself):
     Annunciator 0x10   rev-3 9-byte command
                        [ctrl, ctrlMode, ctrlGrp, modeFlags lo, hi, cap 0, gpwsCfg,
                         gpwsThr hi, lo]
     Resource 0x11      [ctrl & ~audio, 0]
     Info Display 0x12 / 0x13   [ctrl (bit 2 = trim hold), ackSeq 0]
   ctrl = requestType << 4 | idle 0x08 | audio-or-trim 0x04 | demo 0x02 | debug 0x01.

   Backlight: [0xB8, flight %, standby %, 0] to all four at once on a change and every
   CARRIER_BACKLIGHT_MS; the `backlight` console command sets the levels (GitHub #47).

   Every CARRIER_STATUS_MS each display's status packet is read (sync byte checked); bit
   0 of its flags byte is that display's own Simpit link, shown on the Sys Info Display.
****************************************************************************************/
#pragma once
#include <Arduino.h>

enum CarrierId : uint8_t { CAR_ANNUNCIATOR = 0, CAR_INFODISP_1, CAR_INFODISP_2, CAR_RESDISP, CAR_COUNT };

// Annunciator MF_* bit positions (KCMk1_Annunciator.h). Bit 3 is A/P ARM (was THRTL ENA).
enum : uint8_t {
  MF_DEMO = 0, MF_WARP, MF_AUDIO, MF_AP_ARM, MF_TRIM, MF_AUTOPILOT, MF_DEBUG, MF_SWITCH_ERR,
  MF_SIMPIT_LOST, MF_THRTL_PREC, MF_INPUT_PREC, MF_ENG_ARM
};

void     carriersInit();
bool     carrierRequestStatus(CarrierId c);   // its command with requestType STATUS (asserts its INT)
bool     carrierReadStatus(CarrierId c);      // read its status packet now
void     carriersBoot();                   // boot step 7: release each, wait for its status
void     carrierService();
uint16_t carrierModeFlags();               // the Annunciator mode flags as sent
void     carrierSetGpws(uint8_t cfg, int16_t thresholdM);   // from the GPWS panel (stage 6)
void     carrierDisplayReset();            // requestType DISPLAY_RESET to all four, once
void     carrierSetBacklight(uint8_t flightPct, uint8_t standbyPct);   // BACKLIGHT frame (I2C spec §15.3)
uint8_t  carrierBacklightFlight();
uint8_t  carrierInfoDisp1Screen();          // its status byte 2, read every CARRIER_STATUS_MS (0xFF unknown)
uint8_t  carrierBacklightStandby();
uint8_t  carrierInfoDispControl(uint8_t requestType);   // an Info Display's control byte (consoles' ACK)
const char *carrierName(CarrierId c);
