/***************************************************************************************
   cag.cpp — custom action group numbering.
****************************************************************************************/
#include "cag.h"
#include "state.h"

uint8_t cagNumber(uint8_t base) {
  uint8_t g = (g_panel.ctrlGrp >= 1 && g_panel.ctrlGrp <= CAG_GROUPS) ? g_panel.ctrlGrp : 1;
  return (uint8_t)(base + (g - 1) * CAG_GROUP_STRIDE);
}

bool cagKnown() { return g_vessel.cagValid; }

bool cagActive(uint8_t n) { return g_vessel.cagValid && ((g_vessel.cag[n / 8] >> (n % 8)) & 1); }
