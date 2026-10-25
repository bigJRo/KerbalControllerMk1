// Host shim over the displays' Arduino stand-in (Software/Displays/tools/host_stubs),
// adding the Teensy 4.1 core registers Controller_Main writes directly.
#pragma once
#include_next <Arduino.h>
extern volatile uint32_t SCB_AIRCR;     // ARM application interrupt / reset control (soft reboot)
extern volatile uint32_t USB1_USBCMD;   // i.MX RT USB1 command register (USB reset)
