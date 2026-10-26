/***************************************************************************************
   infodisp_link.h — the Info Display 2 autopilot consoles (infodisp_link.cpp).
****************************************************************************************/
#pragma once
#include <Arduino.h>

void    idlInit();
void    idlService();          // poll the console, apply its command, push the status frames
uint8_t idlScreen();           // the Info Display 2 screen on view (0xFF until read)
