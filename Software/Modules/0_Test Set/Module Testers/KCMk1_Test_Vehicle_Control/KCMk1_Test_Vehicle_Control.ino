/**
 * @file        KCMk1_Test_Vehicle_Control.ino
 * @version     1.0.0
 * @date        2026-11-08
 * @project     Kerbal Controller Mk1 — Module Tester
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Bench test for the Vehicle Control module (I2C 0x24), run on the
 *              KC-01-9001 v2.0 Module Tester (XIAO RA4M1).
 *
 *              Module: 12 NeoPixel buttons (inputs 0-11) and Switch Group 2 (inputs 16-23,
 *              all latching, one bit per switch, 1 = ON position). Inputs 12-15
 *              are not connected. 24-input build: 9-byte packet. Reports
 *              EXTENDED_STATES (caps 0x01): WARNING, ALERT, ARMED, PARTIAL_DEPLOY
 *              and CUT are checked as well.
 *              KerbalButtonCore firmware, identity fw 2.2, caps 0x01.
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

// Buttons in panel order, with their KBC input bit (module sketch KCMk1_Vehicle_Control.ino).
static const KtcWalkItem BUTTONS[] = {
    { "BRAKES", 0 },
    { "LIGHTS", 1 },
    { "ANTENNA", 2 },
    { "GEAR", 3 },
    { "FUEL CELL", 4 },
    { "SOLAR", 5 },
    { "CARGO DOOR", 6 },
    { "RADIATOR", 7 },
    { "HEAT SHIELD", 8 },
    { "LADDER", 9 },
    { "MAIN CHUTE", 10 },
    { "DROGUE", 11 },
};
static const KtcWalk WALK = { 12, BUTTONS, ktcDecodeKbc24, 0xFFFFFF, 50, 3, ktcDoEnable, nullptr };

// Switch Group: one bit per switch, 1 = the ON position.
static const KtcWalkItem SWITCHES[] = {
    { "CHUTE", 16, nullptr, false, "Return CHUTE to SAFE", "Set CHUTE to ARM" },
    { "GEAR", 17, nullptr, false, "Return GEAR to UP", "Set GEAR to DOWN" },
    { "BRAKE", 18, nullptr, false, "Return BRAKE to REL", "Set BRAKE to LOCK" },
    { "EXT LT", 19, nullptr, false, "Return EXT LT to OFF", "Set EXT LT to ILLUM" },
    { "SAS", 20, nullptr, false, "Return SAS to OFF", "Set SAS to ENAB" },
    { "RCS", 21, nullptr, false, "Return RCS to OFF", "Set RCS to ENAB" },
    { "THC/RHC", 22, nullptr, false, "Return THC/RHC to NORM", "Set THC/RHC to PREC" },
    { "AUDIO", 23, nullptr, false, "Return AUDIO to MUTE", "Set AUDIO to LIVE" },
};
static const KtcWalk SWITCH_WALK = { 8, SWITCHES, ktcDecodeKbc24, 0xFFFFFF, 50, 4, nullptr,
    "All switches OFF first. Move each switch when named, then back." };

static const KtcLedItem LEDS[] = {
    { "BRAKES", 0, KMC_RED, "RED" },
    { "LIGHTS", 1, KMC_YELLOW, "YELLOW" },
    { "ANTENNA", 2, KMC_PINK, "PINK" },
    { "GEAR", 3, KMC_GREEN, "GREEN" },
    { "FUEL CELL", 4, KMC_CYAN, "CYAN" },
    { "SOLAR", 5, KMC_GOLD, "GOLD" },
    { "CARGO DOOR", 6, KMC_TEAL, "TEAL" },
    { "RADIATOR", 7, KMC_ORANGE, "ORANGE" },
    { "HEAT SHIELD", 8, KMC_GREEN, "GREEN" },
    { "LADDER", 9, KMC_LIME, "LIME" },
    { "MAIN CHUTE", 10, KMC_GREEN, "GREEN" },
    { "DROGUE", 11, KMC_GREEN, "GREEN" },
};
static const KtcLedWalk LED_WALK = { 12, LEDS, KMC_LED_ENABLED };

static const KtcIntCfg  INT_CFG = { "Press and release BRAKES.", 0, 2, true, ktcDecodeKbc24 };
static const KtcLifeCfg LIFE    = { "BRAKES", 0, ktcDecodeKbc24, "Are all 12 button LEDs dark?" };

static const KtcStep STEPS[] = {
    { "Identity",        ktcStepIdentity,  nullptr },
    { "Boot handshake",  ktcStepBoot,      &ktcKbcBoot },
    { "Enable and INT",  ktcStepEnableInt, &INT_CFG },
    { "Buttons",         ktcStepWalk,      &WALK },
    { "Switches",        ktcStepWalk,      &SWITCH_WALK },
    { "LED walk",        ktcStepLedWalk,   &LED_WALK },
    { "LED states",      ktcStepVisual,    &ktcKbcStatesExtended },
    { "Brightness",      ktcStepVisual,    &ktcKbcBrightness },
    { "Bulb test",       ktcStepVisual,    &ktcKbcBulb },
    { "Lifecycle",       ktcStepLifecycle, &LIFE },
    { "Supply current",  ktcStepCurrent,   &ktcKbcCurrent },
};

static const KtcModuleDef MODULE = {
    "Vehicle Control", "KC-01-1811/1812 wide button board",
    KMC_TYPE_VEHICLE_CONTROL, 0x24, KMC_BUTTON24_PACKET_SIZE,
    KTC_KBC_FW_MAJOR, KTC_KBC_FW_MINOR, 0x01,
    STEPS, sizeof(STEPS) / sizeof(STEPS[0]),
    ktcKbcCleanup, ktcKbcLive24,
};

void setup() { ktcBegin(MODULE); }
void loop()  { ktcLoop(); }
