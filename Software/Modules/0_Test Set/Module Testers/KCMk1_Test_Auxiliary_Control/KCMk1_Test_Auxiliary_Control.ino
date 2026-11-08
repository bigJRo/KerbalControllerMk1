/**
 * @file        KCMk1_Test_Auxiliary_Control.ino
 * @version     1.0.0
 * @date        2026-11-08
 * @project     Kerbal Controller Mk1 — Module Tester
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Bench test for the Auxiliary Control module (I2C 0x26), run on the
 *              KC-01-9001 v2.0 Module Tester (XIAO RA4M1).
 *
 *              Module: 12 momentary NeoPixel buttons for EVA, docking and control-point
 *              functions. Control-point PRI/ALT exclusivity is the master's job.
 *              Inputs 12-15 are not connected and must stay 0.
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

// Buttons in panel order, with their KBC input bit (module sketch KCMk1_Auxiliary_Control.ino).
static const KtcWalkItem BUTTONS[] = {
    { "EVA LIGHTS", 0 },
    { "JET PACK", 1 },
    { "BOARD", 2 },
    { "CONST", 3 },
    { "JUMP", 4 },
    { "GRAB", 5 },
    { "EVA CHUTE", 6 },
    { "HELMET", 7 },
    { "UNDOCK", 8 },
    { "FLAG", 9 },
    { "CP PRI", 10 },
    { "CP ALT", 11 },
};
static const KtcWalk WALK = { 12, BUTTONS, ktcDecodeKbc16, 0xFFFF, 50, 3, ktcDoEnable, nullptr };

static const KtcLedItem LEDS[] = {
    { "EVA LIGHTS", 0, KMC_MINT, "MINT" },
    { "JET PACK", 1, KMC_LIME, "LIME" },
    { "BOARD", 2, KMC_GREEN, "GREEN" },
    { "CONST", 3, KMC_TEAL, "TEAL" },
    { "JUMP", 4, KMC_CHARTREUSE, "CHARTREUSE" },
    { "GRAB", 5, KMC_SEAFOAM, "SEAFOAM" },
    { "EVA CHUTE", 6, KMC_AMBER, "AMBER" },
    { "HELMET", 7, KMC_SKY, "SKY" },
    { "UNDOCK", 8, KMC_GOLD, "GOLD" },
    { "FLAG", 9, KMC_SEAFOAM, "SEAFOAM" },
    { "CP PRI", 10, KMC_ROSE, "ROSE" },
    { "CP ALT", 11, KMC_PINK, "PINK" },
};
static const KtcLedWalk LED_WALK = { 12, LEDS, KMC_LED_ENABLED };

static const KtcIntCfg  INT_CFG = { "Press and release EVA LIGHTS.", 0, 2, true, ktcDecodeKbc16 };
static const KtcLifeCfg LIFE    = { "EVA LIGHTS", 0, ktcDecodeKbc16, "Are all 12 button LEDs dark?" };

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
    "Auxiliary Control", "KC-01-1801/1802 button board",
    KMC_TYPE_AUX_CTRL, 0x26, KMC_BUTTON_PACKET_SIZE,
    KTC_KBC_FW_MAJOR, KTC_KBC_FW_MINOR, 0x00,
    STEPS, sizeof(STEPS) / sizeof(STEPS[0]),
    ktcKbcCleanup, ktcKbcLive16,
};

void setup() { ktcBegin(MODULE); }
void loop()  { ktcLoop(); }
