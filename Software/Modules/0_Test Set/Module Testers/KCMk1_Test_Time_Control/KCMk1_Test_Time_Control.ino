/**
 * @file        KCMk1_Test_Time_Control.ino
 * @version     1.0.0
 * @date        2026-11-08
 * @project     Kerbal Controller Mk1 — Module Tester
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Bench test for the Time Control module (I2C 0x25), run on the
 *              KC-01-9001 v2.0 Module Tester (XIAO RA4M1).
 *
 *              Module: 12 momentary NeoPixel buttons for time warp, quicksave and pause,
 *              plus the 3-position control-mode selector on the CN2 terminal:
 *              input 12 = SPC, input 13 = RVR, neither = PLN (centre). Both at
 *              once is a wiring fault. Inputs 14-15 are not connected.
 *              KerbalButtonCore firmware, identity fw 2.2, caps 0x00.
 *
 *              Steps: identity, boot handshake, enable + INT, button walk, mode selector,
 *              LED walk, LED states, brightness, bulb test, lifecycle, supply current. Results show on
 *              screen and print to USB serial at 115200 baud.
 *
 * @note        Set the mode selector to PLN (centre) before the run.
 *
 * @note        Board: Seeed XIAO RA4M1. Libraries: KCMk1TesterCore,
 *              KerbalModuleCommon, Adafruit GFX, Adafruit ST7735 and ST7789,
 *              Adafruit FT6206.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#include <KCMk1TesterCore.h>

// Buttons in panel order, with their KBC input bit (module sketch KCMk1_Time_Control.ino).
static const KtcWalkItem BUTTONS[] = {
    { "ApA", 0 },
    { "STOP", 1 },
    { "PeA", 2 },
    { "PHYS", 3 },
    { "MNVR", 4 },
    { "FASTER", 5 },
    { "SOI", 6 },
    { "SLOWER", 7 },
    { "MORN", 8 },
    { "LOAD", 9 },
    { "PAUSE", 10 },
    { "SAVE", 11 },
};
static const KtcWalk WALK = { 12, BUTTONS, ktcDecodeKbc16, 0xFFFF, 50, 3, ktcDoEnable, nullptr };

// Control-mode selector on CN2: input 12 = SPC, input 13 = RVR, neither = PLN.
static const KtcWalkItem SELECTOR[] = {
    { "SPC", 12, nullptr, false, "Turn the selector back to PLN", "Turn the selector to SPC" },
    { "RVR", 13, nullptr, false, "Turn the selector back to PLN", "Turn the selector to RVR" },
};
static const KtcWalk SELECTOR_WALK = { 2, SELECTOR, ktcDecodeKbc16, 0xFFFF, 50, 2, nullptr,
    "Mode selector at PLN (centre) first. SPC and RVR must never read at once." };

static const KtcLedItem LEDS[] = {
    { "ApA", 0, KMC_YELLOW, "YELLOW" },
    { "STOP", 1, KMC_AMBER, "AMBER" },
    { "PeA", 2, KMC_YELLOW, "YELLOW" },
    { "PHYS", 3, KMC_CHARTREUSE, "CHARTREUSE" },
    { "MNVR", 4, KMC_YELLOW, "YELLOW" },
    { "FASTER", 5, KMC_WHITE_WARM, "WHITE WARM" },
    { "SOI", 6, KMC_YELLOW, "YELLOW" },
    { "SLOWER", 7, KMC_WHITE_WARM, "WHITE WARM" },
    { "MORN", 8, KMC_YELLOW, "YELLOW" },
    { "LOAD", 9, KMC_MINT, "MINT" },
    { "PAUSE", 10, KMC_AMBER, "AMBER" },
    { "SAVE", 11, KMC_LIME, "LIME" },
};
static const KtcLedWalk LED_WALK = { 12, LEDS, KMC_LED_ENABLED };

static const KtcIntCfg  INT_CFG = { "Press and release ApA.", 0, 2, true, ktcDecodeKbc16 };
static const KtcLifeCfg LIFE    = { "ApA", 0, ktcDecodeKbc16, "Are all 12 button LEDs dark?" };

static const KtcStep STEPS[] = {
    { "Identity",        ktcStepIdentity,  nullptr },
    { "Boot handshake",  ktcStepBoot,      &ktcKbcBoot },
    { "Enable and INT",  ktcStepEnableInt, &INT_CFG },
    { "Buttons",         ktcStepWalk,      &WALK },
    { "Mode selector",   ktcStepWalk,      &SELECTOR_WALK },
    { "LED walk",        ktcStepLedWalk,   &LED_WALK },
    { "LED states",      ktcStepVisual,    &ktcKbcStatesCore },
    { "Brightness",      ktcStepVisual,    &ktcKbcBrightness },
    { "Bulb test",       ktcStepVisual,    &ktcKbcBulb },
    { "Lifecycle",       ktcStepLifecycle, &LIFE },
    { "Supply current",  ktcStepCurrent,   &ktcKbcCurrent },
};

static const KtcModuleDef MODULE = {
    "Time Control", "KC-01-1801/1802 button board",
    KMC_TYPE_TIME_CONTROL, 0x25, KMC_BUTTON_PACKET_SIZE,
    KTC_KBC_FW_MAJOR, KTC_KBC_FW_MINOR, 0x00,
    STEPS, sizeof(STEPS) / sizeof(STEPS[0]),
    ktcKbcCleanup, ktcKbcLive16,
};

void setup() { ktcBegin(MODULE); }
void loop()  { ktcLoop(); }
