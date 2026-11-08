/**
 * @file        KCMk1_Test_Stability_Control.ino
 * @version     1.0.0
 * @date        2026-11-08
 * @project     Kerbal Controller Mk1 — Module Tester
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Bench test for the Stability Control module (I2C 0x23), run on the
 *              KC-01-9001 v2.0 Module Tester (XIAO RA4M1).
 *
 *              Module: 12 momentary NeoPixel buttons: ten SAS modes, HOLD and INVERT.
 *              SAS-mode exclusivity is the master's job; the module reports
 *              raw levels. Inputs 12-15 are not connected and must stay 0.
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

// Buttons in panel order, with their KBC input bit (module sketch KCMk1_Stability_Control.ino).
static const KtcWalkItem BUTTONS[] = {
    { "TARGET", 0 },
    { "ANTI TARGET", 1 },
    { "RADIAL IN", 2 },
    { "RADIAL OUT", 3 },
    { "NORMAL", 4 },
    { "ANTI NORMAL", 5 },
    { "PROGRADE", 6 },
    { "RETROGRADE", 7 },
    { "STAB ASSIST", 8 },
    { "MANEUVER", 9 },
    { "HOLD", 10 },
    { "INVERT", 11 },
};
static const KtcWalk WALK = { 12, BUTTONS, ktcDecodeKbc16, 0xFFFF, 50, 3, ktcDoEnable, nullptr };

static const KtcLedItem LEDS[] = {
    { "TARGET", 0, KMC_GREEN, "GREEN" },
    { "ANTI TARGET", 1, KMC_GREEN, "GREEN" },
    { "RADIAL IN", 2, KMC_GREEN, "GREEN" },
    { "RADIAL OUT", 3, KMC_GREEN, "GREEN" },
    { "NORMAL", 4, KMC_GREEN, "GREEN" },
    { "ANTI NORMAL", 5, KMC_GREEN, "GREEN" },
    { "PROGRADE", 6, KMC_GREEN, "GREEN" },
    { "RETROGRADE", 7, KMC_GREEN, "GREEN" },
    { "STAB ASSIST", 8, KMC_GREEN, "GREEN" },
    { "MANEUVER", 9, KMC_GREEN, "GREEN" },
    { "HOLD", 10, KMC_MAGENTA, "MAGENTA" },
    { "INVERT", 11, KMC_AMBER, "AMBER" },
};
static const KtcLedWalk LED_WALK = { 12, LEDS, KMC_LED_ENABLED };

static const KtcIntCfg  INT_CFG = { "Press and release TARGET.", 0, 2, true, ktcDecodeKbc16 };
static const KtcLifeCfg LIFE    = { "TARGET", 0, ktcDecodeKbc16, "Are all 12 button LEDs dark?" };

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
    "Stability Control", "KC-01-1801/1802 button board",
    KMC_TYPE_STABILITY_CONTROL, 0x23, KMC_BUTTON_PACKET_SIZE,
    KTC_KBC_FW_MAJOR, KTC_KBC_FW_MINOR, 0x00,
    STEPS, sizeof(STEPS) / sizeof(STEPS[0]),
    ktcKbcCleanup, ktcKbcLive16,
};

void setup() { ktcBegin(MODULE); }
void loop()  { ktcLoop(); }
