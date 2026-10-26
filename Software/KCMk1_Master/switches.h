/***************************************************************************************
   switches.h — Switch Group 1 (Function Control B16–B23), Switch Group 2 (Vehicle
   Control B16–B23), the control-mode selector (Time Control B12 / B13) and SWITCH ERR.
   Design §8.2, §8.3, §8.6; Inventory §7.2, §7.4, §8.

   Rules:
     - A switch acts only when it is flipped. The position found when a switch module is
       first read (power-on, a restart, a module that came back) is taken as it is: the
       master's own modes follow it, and nothing is sent to KSP.
     - Switch Group 2's GEAR, BRAKE, EXT LT, SAS and RCS act on KSP only in a flight
       scene, with the link up and demo off. A flip at any other time is not acted on.
     - SWITCH ERR: in a flight scene (not demo, not EVA, once KSP has reported the
       action groups), any of those five that disagrees with the vessel for
       SWITCH_ERR_SETTLE_MS lights SWITCH ERR until they agree. The settle time covers
       KSP's reply to a flip. The Sys Info Display names each one (I2C spec §15.4).
     - Exceptions are set by the code that causes them, with switchErrSuppress():
       INVERT held (SAS), an autopilot or HOLD that changes SAS / RCS, the BRAKES button
       or the RVR stick button held (BRAKE).
     - Mode selector: B12 = SPC, neither = PLN (centre), B13 = RVR. Both on is a switch
       fault: a debug message, and the last good mode is kept.
     - MSTR RESET, DISP CLR and LTG TEST are momentary: MSTR and DISP act on the press
       (a flick between two reads still counts), LTG holds the bulb test while on.
****************************************************************************************/
#pragma once
#include <Arduino.h>

// Ids as in the Sys Info SWITCH_ERR frame: 1–8 Switch Group 1 B16–B23, 9–16 Group 2.
enum SwitchId : uint8_t {
  SW_NONE = 0,
  SW_MSTR, SW_DISP, SW_SCE, SW_UPTLM, SW_LTG, SW_AP, SW_ENGINE, SW_THRTL,
  SW_CHUTE, SW_GEAR, SW_BRAKE, SW_EXTLT, SW_SAS, SW_RCS, SW_PREC, SW_AUDIO,
  SW_COUNT
};

// Who is asking SWITCH ERR to look away from a switch (several can at once).
enum : uint8_t { SUPP_INVERT = 0x01, SUPP_AUTOPILOT = 0x02, SUPP_BRAKE_HELD = 0x04, SUPP_CONSOLE_AP = 0x08 };

static const uint32_t SWITCH_ERR_SETTLE_MS = 1000;

void        switchesInit();
void        switchesBoot();                        // boot: first read, one summary line
void        switchesService();                     // after moduleBusService()
bool        switchKnown(SwitchId id);              // its module has been read
bool        switchOn(SwitchId id);                 // on = the second position (RESET, AUX, ARM, …)
bool        switchErr(SwitchId id);
void        switchErrSuppress(SwitchId id, uint8_t source, bool on);
const char *switchName(SwitchId id);
