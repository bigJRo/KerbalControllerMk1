/**
 * @file        KCMk1_Test_UI_Control.ino
 * @version     1.0.0
 * @date        2026-11-08
 * @project     Kerbal Controller Mk1 — Module Tester
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Bench test for the UI Control module (I2C 0x20), run on the
 *              KC-01-9001 v2.0 Module Tester (XIAO RA4M1).
 *
 *              Module: 12 momentary NeoPixel buttons for the map, camera and UI functions. Inputs
 *              12-15 are not connected and must stay 0.
 *              KerbalButtonCore 2.4 firmware, identity fw 2.4, caps 0x00.
 *
 *              Steps: identity, boot handshake, enable + INT, button walk, LED walk,
 *              LED states, brightness, bulb test, lifecycle, supply current. Results show on
 *              screen and print to USB serial at 115200 baud.
 *
 * @note        Board: Seeed XIAO RA4M1. Libraries: KCMk1TesterCore,
 *              KerbalModuleCommon, Adafruit GFX, Adafruit ST7735 and ST7789,
 *              Adafruit FT6206.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#include <KCMk1TesterCore.h>

// Buttons in panel order, with their KBC input bit (module sketch KCMk1_UI_Control.ino).
static const KtcWalkItem BUTTONS[] = {
    { "MAP ENA", 10 },
    { "MAP <", 8 },
    { "MAP >", 6 },
    { "MAP RESET", 4 },
    { "UI", 2 },
    { "SCREENSHOT", 0 },
    { "IVA", 11 },
    { "SHIP <", 9 },
    { "SHIP >", 7 },
    { "CYCLE NAV", 5 },
    { "NAV BALL", 3 },
    { "DEBUG", 1 },
};
static const KtcWalk WALK = { 12, BUTTONS, ktcDecodeKbc16, 0xFFFF, 50, 3, ktcDoEnable, nullptr };

static const KtcLedItem LEDS[] = {
    { "MAP ENA", 10, KMC_SKY, "SKY" },
    { "MAP <", 8, KMC_SKY, "SKY" },
    { "MAP >", 6, KMC_SKY, "SKY" },
    { "MAP RESET", 4, KMC_SKY, "SKY" },
    { "UI", 2, KMC_GREEN, "GREEN" },
    { "SCREENSHOT", 0, KMC_GREEN, "GREEN" },
    { "IVA", 11, KMC_CORAL, "CORAL" },
    { "SHIP <", 9, KMC_TEAL, "TEAL" },
    { "SHIP >", 7, KMC_TEAL, "TEAL" },
    { "CYCLE NAV", 5, KMC_GREEN, "GREEN" },
    { "NAV BALL", 3, KMC_AMBER, "AMBER" },
    { "DEBUG", 1, KMC_RED, "RED" },
};
static const KtcLedWalk LED_WALK = { 12, LEDS, KMC_LED_ENABLED };

static const KtcIntCfg  INT_CFG = { "Press and release MAP ENA.", 10, 2, true, ktcDecodeKbc16 };
static const KtcLifeCfg LIFE    = { "MAP ENA", 10, ktcDecodeKbc16, "Are all 12 button LEDs dark?" };

static const KtcStep STEPS[] = {
    { "Identity",        ktcStepIdentity,  nullptr },
    { "Boot handshake",  ktcStepBoot,      &ktcKbcBoot },
    { "Enable and INT",  ktcStepEnableInt, &INT_CFG },
    { "Buttons",         ktcStepWalk,      &WALK },
    { "LED walk",        ktcStepLedWalk,   &LED_WALK },
    { "LED states",      ktcStepVisual,    &ktcKbcStatesCore },
    { "Brightness",      ktcStepVisual,    &ktcKbcBrightness },
    { "Bulb test",       ktcStepVisual,    &ktcKbcBulb },
    { "Lifecycle",       ktcStepLifecycle, &LIFE },
    { "Supply current",  ktcStepCurrent,   &ktcKbcCurrent },
};

static const KtcModuleDef MODULE = {
    "UI Control", "KC-01-1801/1802 button board",
    KMC_TYPE_UI_CONTROL, 0x20, KMC_BUTTON_PACKET_SIZE,
    KTC_KBC_FW_MAJOR, KTC_KBC_FW_MINOR, 0x00,
    STEPS, sizeof(STEPS) / sizeof(STEPS[0]),
    ktcKbcCleanup, ktcKbcLive16,
};

void setup() { ktcBegin(MODULE); }
void loop()  { ktcLoop(); }
