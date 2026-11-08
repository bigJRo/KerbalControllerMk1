/**
 * @file        KCMk1_Test_Action_Control.ino
 * @version     1.0.0
 * @date        2026-11-08
 * @project     Kerbal Controller Mk1 — Module Tester
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Bench test for the Action Control module (I2C 0x22, type 0x03),
 *              run on the KC-01-9001 v2.0 Module Tester (XIAO RA4M1).
 *
 *              Module: 12 momentary NeoPixel buttons AG1-AG12, all GREEN when
 *              ACTIVE. KerbalButtonCore 2.4 firmware, identity fw 2.4, caps 0x00.
 *              Inputs 12-15 are not connected and must stay 0.
 *
 *              Steps: identity, boot handshake, enable + INT, button walk,
 *              LED walk, LED states, brightness, bulb test, lifecycle, supply
 *              current. Results show on screen and print to USB serial at
 *              115200 baud.
 *
 * @note        Board: Seeed XIAO RA4M1. Libraries: KCMk1TesterCore,
 *              KerbalModuleCommon, Adafruit GFX, Adafruit ST7735 and ST7789,
 *              Adafruit FT6206.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#include <KCMk1TesterCore.h>

// KBC input bit for each action group (sketch KCMk1_Action_Control.ino).
static const KtcWalkItem BUTTONS[] = {
    { "AG1", 10 }, { "AG2", 8 },  { "AG3", 6 },  { "AG4", 4 },  { "AG5", 2 },  { "AG6", 0 },
    { "AG7", 11 }, { "AG8", 9 },  { "AG9", 7 },  { "AG10", 5 }, { "AG11", 3 }, { "AG12", 1 },
};
static const KtcWalk WALK = { 12, BUTTONS, ktcDecodeKbc16, 0xFFFF, 50, 4, ktcDoEnable };

static const KtcLedItem LEDS[] = {
    { "AG1", 10, KMC_GREEN, "GREEN" }, { "AG2", 8, KMC_GREEN, "GREEN" },
    { "AG3", 6, KMC_GREEN, "GREEN" },  { "AG4", 4, KMC_GREEN, "GREEN" },
    { "AG5", 2, KMC_GREEN, "GREEN" },  { "AG6", 0, KMC_GREEN, "GREEN" },
    { "AG7", 11, KMC_GREEN, "GREEN" }, { "AG8", 9, KMC_GREEN, "GREEN" },
    { "AG9", 7, KMC_GREEN, "GREEN" },  { "AG10", 5, KMC_GREEN, "GREEN" },
    { "AG11", 3, KMC_GREEN, "GREEN" }, { "AG12", 1, KMC_GREEN, "GREEN" },
};
static const KtcLedWalk LED_WALK = { 12, LEDS, KMC_LED_ENABLED };

static const KtcIntCfg   INT_CFG = { "Press and release AG1.", 10, 2, true, ktcDecodeKbc16 };
static const KtcLifeCfg  LIFE    = { "AG1", 10, ktcDecodeKbc16, "Are all 12 button LEDs dark?" };

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
    "Action Control", "KC-01-1801/1802 button board",
    KMC_TYPE_ACTION_CONTROL, 0x22, KMC_BUTTON_PACKET_SIZE,
    KTC_KBC_FW_MAJOR, KTC_KBC_FW_MINOR, 0x00,
    STEPS, sizeof(STEPS) / sizeof(STEPS[0]),
    ktcKbcCleanup, ktcKbcLive16,
};

void setup() { ktcBegin(MODULE); }
void loop()  { ktcLoop(); }
