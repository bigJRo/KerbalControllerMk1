/**
 * @file        KCMk1_Test_Joystick_Rotation.ino
 * @version     1.0.0
 * @date        2026-11-08
 * @project     Kerbal Controller Mk1 — Module Tester
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Bench test for the Joystick Rotation module (I2C 0x28), run on
 *              the KC-01-9001 v2.0 Module Tester (XIAO RA4M1).
 *
 *              Module: Rotation hand controller (RHC): three axes (AXIS1 right/left,
 *              AXIS2 forward/back, AXIS3 twist), the stick button, and two
 *              NeoPixel buttons RESET TRIM (GREEN) and TRIM SET (AMBER).
 *              KerbalJoystickCore firmware, identity fw 2.0, caps 0x08.
 *
 *              Steps: identity, boot handshake (hands off: the module samples
 *              its centre at power-on), enable + INT, buttons, centre and
 *              deadzone, full travel on each axis (the sign of each direction
 *              is recorded in the result), LED walk, LED states / brightness /
 *              bulb test, lifecycle, supply current. Results show on screen
 *              and print to USB serial at 115200 baud.
 *
 * @note        Enable + INT expects exactly two packets for one press and
 *              release. KerbalJoystickCore 2.x re-arms the change mask on the
 *              read after each edge, so it may report every edge twice; that
 *              shows here as "4 packets".
 *
 * @note        Board: Seeed XIAO RA4M1. Libraries: KCMk1TesterCore,
 *              KerbalModuleCommon, Adafruit GFX, Adafruit ST7735 and ST7789,
 *              Adafruit FT6206.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#include <KCMk1TesterCore.h>

static const KtcWalkItem BUTTONS[] = {
    { "STICK BTN", 0 }, { "RESET TRIM", 1 }, { "TRIM SET", 2 },
};
static const KtcWalk WALK = { 3, BUTTONS, ktcDecodeKjc, 0xFF, 50, 3, ktcDoEnable, nullptr };

static const KtcAxisCfg AXIS_X     = { 0, "Push the stick fully RIGHT", "Push the stick fully LEFT", "right", 0 };
static const KtcAxisCfg AXIS_Y     = { 1, "Push the stick fully FORWARD", "Pull the stick fully BACK", "forward", 0 };
static const KtcAxisCfg AXIS_TWIST = { 2, "Twist the stick fully CLOCKWISE", "Twist the stick fully ANTI-CLOCKWISE", "clockwise", 0 };

static const KtcLedItem LEDS[] = {
    { "RESET TRIM", 0, KMC_GREEN, "GREEN" },
    { "TRIM SET", 1, KMC_AMBER, "AMBER" },
};
static const KtcLedWalk LED_WALK = { 2, LEDS, KMC_LED_ENABLED };

static const KtcIntCfg  INT_CFG = { "Press and release RESET TRIM.", 1, 2, true, ktcDecodeKjc };
static const KtcLifeCfg LIFE    = { "RESET TRIM", 1, ktcDecodeKjc, "Are both button LEDs dark?" };

static const KtcStep STEPS[] = {
    { "Identity",        ktcStepIdentity,  nullptr },
    { "Boot handshake",  ktcStepBoot,      &ktcKjcBoot },
    { "Enable and INT",  ktcStepEnableInt, &INT_CFG },
    { "Buttons",         ktcStepWalk,      &WALK },
    { "Centre",          ktcStepKjcCentre, nullptr },
    { "AXIS1 right/left", ktcStepKjcAxis,  &AXIS_X },
    { "AXIS2 fwd/back",  ktcStepKjcAxis,   &AXIS_Y },
    { "AXIS3 twist",     ktcStepKjcAxis,   &AXIS_TWIST },
    { "LED walk",        ktcStepLedWalk,   &LED_WALK },
    { "LED states",      ktcStepVisual,    &ktcKjcStates },
    { "Lifecycle",       ktcStepLifecycle, &LIFE },
    { "Supply current",  ktcStepCurrent,   &ktcKjcCurrent },
};

static const KtcModuleDef MODULE = {
    "Joystick Rotation", "KC-01-1831/1832 joystick board",
    KMC_TYPE_JOYSTICK_ROTATION, 0x28, KMC_JOYSTICK_PACKET_SIZE,
    KTC_KJC_FW_MAJOR, KTC_KJC_FW_MINOR, KMC_CAP_JOYSTICK,
    STEPS, sizeof(STEPS) / sizeof(STEPS[0]),
    ktcKjcCleanup, ktcKjcLive,
};

void setup() { ktcBegin(MODULE); }
void loop()  { ktcLoop(); }
