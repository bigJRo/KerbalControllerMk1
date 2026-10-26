/***************************************************************************************
   cag.h — custom action group numbering (design §8.1, Inventory §10.8).

   CAG = base + (control group − 1) × 40, control group 1–6 from the Dual Encoder's
   ENC1. The offset applies to every CAG button except Action Control's AG 1–12, which
   always send CAG 1–12 (cagFixed()). KSP's report of each group (CAGSTATUS) is read
   with cagActive().
****************************************************************************************/
#pragma once
#include <Arduino.h>

static const uint8_t CAG_GROUP_STRIDE = 40;
static const uint8_t CAG_GROUPS       = 6;

uint8_t cagNumber(uint8_t base);      // with the control-group offset
inline uint8_t cagFixed(uint8_t n) { return n; }
bool    cagActive(uint8_t n);         // as KSP last reported (false until reported)
bool    cagKnown();
