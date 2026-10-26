/***************************************************************************************
   panel_ui.cpp — UI Control (0x20) (Inventory §10.7). KSP reports none of these, so
   every button is backlit and flashes on a press.

     B0  SCREEN SHOT  macro F2, F1, F2 (hide the UI, screenshot, UI back)
     B1  DEBUG        Alt+F12            B2  UI        F2
     B3  NAV BALL     numpad .           B4  [MAP] RESET   '
     B5  CYCLE NAV    Simpit navball mode
     B6  [MAP] >      Tab                B7  [SHIP] >  ]
     B8  [MAP] <      Shift+Tab          B9  [SHIP] <  [
     B10 [MAP] ENA    M                  B11 IVA       C
****************************************************************************************/
#include "panels.h"
#include "ksp_out.h"
#include "keys.h"

struct UiKey { int16_t vk; uint8_t mod; };
static const UiKey kKey[12] = {
  { 0, 0 },          { VK_F12, ALT_MOD }, { VK_F2, 0 },  { VK_DECIMAL, 0 }, { VK_OEM_7, 0 }, { 0, 0 },
  { VK_TAB, 0 },     { VK_OEM_6, 0 },     { VK_TAB, SHIFT_MOD }, { VK_OEM_4, 0 }, { VK_M, 0 }, { VK_C, 0 },
};
enum : uint8_t { B_SCREENSHOT = 0, B_CYCLE_NAV = 5 };
static const int16_t kScreenshot[] = { VK_F2, VK_F1, VK_F2 };

static void onEvent(const ModEvent &e) {
  panelNoteEvent(e);
  if (!panelsEnabled() || !panelLive()) return;
  uint32_t act = e.pressed | e.tapped;
  for (uint8_t b = 0; b < 12; b++) {
    if (!(act & (1u << b))) continue;
    if (b == B_SCREENSHOT)      keySequence(kScreenshot, 3);
    else if (b == B_CYCLE_NAV)  kspNavballCycle();
    else                        keyTap(kKey[b].vk, kKey[b].mod);
    panelFlash(MOD_UI, b);
  }
}

void panelUiService() {
  for (uint8_t b = 0; b < 12; b++) panelLed(MOD_UI, b, true, false);
}

void panelUiInit() { moduleBusSetHandler(MOD_UI, onEvent); }
