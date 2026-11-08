/**
 * @file        KCMk1_Test_Function_Control.ino
 * @version     1.0.0
 * @date        2026-11-08
 * @project     Kerbal Controller Mk1 — Module Tester
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Bench test for the Function Control module (I2C 0x21), run on the
 *              KC-01-9001 v2.0 Module Tester (XIAO RA4M1).
 *
 *              Module: 12 NeoPixel buttons (inputs 0-11) and Switch Group 1 (inputs 16-23,
 *              one bit per switch, 1 = ON position). Inputs 12-15 are not
 *              connected. 24-input build: 9-byte packet.
 *              KerbalButtonCore firmware, identity fw 2.2, caps 0x00.
 *
 *              Steps: identity, boot handshake, enable + INT, button walk, switch walk,
 *              LED walk, LED states, brightness, bulb test, lifecycle, supply current. Results show on
 *              screen and print to USB serial at 115200 baud.
 *
 * @note        Set every switch to its OFF position before the run.
 *
 * @note        Board: Seeed XIAO RA4M1. Libraries: KCMk1TesterCore,
 *              KerbalModuleCommon, Adafruit GFX, Adafruit ST7735 and ST7789,
 *              Adafruit FT6206.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#include <KCMk1TesterCore.h>

// Buttons in panel order, with their KBC input bit (module sketch KCMk1_Function_Control.ino).
static const KtcWalkItem BUTTONS[] = {
    { "LES", 1 },
    { "FAIRING", 0 },
    { "AIR INTAKE", 3 },
    { "LOCK SURF", 2 },
    { "AIR BRAKE", 5 },
    { "FLAPS", 4 },
    { "ENGINE ALT", 7 },
    { "SCI COLLECT", 6 },
    { "ENG GROUP 1", 9 },
    { "SCI GROUP 1", 8 },
    { "ENG GROUP 2", 11 },
    { "SCI GROUP 2", 10 },
};
static const KtcWalk WALK = { 12, BUTTONS, ktcDecodeKbc24, 0xFFFFFF, 50, 3, ktcDoEnable, nullptr };

// Switch Group: one bit per switch, 1 = the ON position.
static const KtcWalkItem SWITCHES[] = {
    { "MSTR", 16, nullptr, false, "Release MSTR to OPER", "Hold MSTR at RESET" },   // momentary
    { "DISP", 17, nullptr, false, "Release DISP to OPER", "Hold DISP at CLR" },   // momentary
    { "SCE", 18, nullptr, false, "Return SCE to NORM", "Set SCE to AUX" },
    { "UPTLM", 19, nullptr, false, "Return UPTLM to INHIB", "Set UPTLM to XMIT" },
    { "LTG", 20, nullptr, false, "Release LTG to NORM", "Hold LTG at TEST" },   // momentary
    { "A/P", 21, nullptr, false, "Return A/P to INHIB", "Set A/P to ARM" },
    { "ENGINE", 22, nullptr, false, "Return ENGINE to SAFE", "Set ENGINE to ARM" },
    { "THRTL", 23, nullptr, false, "Return THRTL to STD", "Set THRTL to FINE" },
};
static const KtcWalk SWITCH_WALK = { 8, SWITCHES, ktcDecodeKbc24, 0xFFFFFF, 50, 4, nullptr,
    "All switches OFF first. Move each switch when named, then back." };

static const KtcLedItem LEDS[] = {
    { "LES", 1, KMC_RED, "RED" },
    { "FAIRING", 0, KMC_AMBER, "AMBER" },
    { "AIR INTAKE", 3, KMC_TEAL, "TEAL" },
    { "LOCK SURF", 2, KMC_SKY, "SKY" },
    { "AIR BRAKE", 5, KMC_CYAN, "CYAN" },
    { "FLAPS", 4, KMC_SEAFOAM, "SEAFOAM" },
    { "ENGINE ALT", 7, KMC_ORANGE, "ORANGE" },
    { "SCI COLLECT", 6, KMC_PURPLE, "PURPLE" },
    { "ENG GROUP 1", 9, KMC_YELLOW, "YELLOW" },
    { "SCI GROUP 1", 8, KMC_INDIGO, "INDIGO" },
    { "ENG GROUP 2", 11, KMC_CHARTREUSE, "CHARTREUSE" },
    { "SCI GROUP 2", 10, KMC_BLUE, "BLUE" },
};
static const KtcLedWalk LED_WALK = { 12, LEDS, KMC_LED_ENABLED };

static const KtcIntCfg  INT_CFG = { "Press and release LES.", 1, 2, true, ktcDecodeKbc24 };
static const KtcLifeCfg LIFE    = { "LES", 1, ktcDecodeKbc24, "Are all 12 button LEDs dark?" };

static const KtcStep STEPS[] = {
    { "Identity",        ktcStepIdentity,  nullptr },
    { "Boot handshake",  ktcStepBoot,      &ktcKbcBoot },
    { "Enable and INT",  ktcStepEnableInt, &INT_CFG },
    { "Buttons",         ktcStepWalk,      &WALK },
    { "Switches",        ktcStepWalk,      &SWITCH_WALK },
    { "LED walk",        ktcStepLedWalk,   &LED_WALK },
    { "LED states",      ktcStepVisual,    &ktcKbcStatesCore },
    { "Brightness",      ktcStepVisual,    &ktcKbcBrightness },
    { "Bulb test",       ktcStepVisual,    &ktcKbcBulb },
    { "Lifecycle",       ktcStepLifecycle, &LIFE },
    { "Supply current",  ktcStepCurrent,   &ktcKbcCurrent },
};

static const KtcModuleDef MODULE = {
    "Function Control", "KC-01-1811/1812 wide button board",
    KMC_TYPE_FUNCTION_CONTROL, 0x21, KMC_BUTTON24_PACKET_SIZE,
    KTC_KBC_FW_MAJOR, KTC_KBC_FW_MINOR, 0x00,
    STEPS, sizeof(STEPS) / sizeof(STEPS[0]),
    ktcKbcCleanup, ktcKbcLive24,
};

void setup() { ktcBegin(MODULE); }
void loop()  { ktcLoop(); }
