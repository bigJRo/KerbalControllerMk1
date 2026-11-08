/**
 * @file        KTC_Kjc.h
 * @version     1.0.0
 * @project     Kerbal Controller Mk1 — Module Tester per-module sketches
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Checks for the KerbalJoystickCore modules (Joystick Rotation
 *              0x28, Joystick Translation 0x29). Identity fw 2.0, caps 0x08.
 *
 *              Packet (12 bytes): header, events (= change AND state),
 *              change, state, then AXIS1..AXIS3 as signed big-endian int16.
 *              Buttons: bit 0 stick button, bit 1 BTN01, bit 2 BTN02. Two
 *              WS2811 pixels: position 0 = BTN01, position 1 = BTN02.
 *
 *              Axis pipeline (KerbalJoystickCore 2.x source): the centre is
 *              sampled once at power-on (keep hands off for ~0.5 s), a raw
 *              deadzone of +-32 reports 0, and the ends are fixed at raw
 *              0 / 1023 -> -32768 / +32767.
 *
 *              LED states: OFF, ENABLED (white scaled by brightness, default
 *              32), ACTIVE, WARNING (steady amber), ALERT (steady red), ARMED
 *              (cyan). PARTIAL_DEPLOY, CUT and ACTIVE_ALT render OFF.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#pragma once
#include "KTC_Steps.h"

#define KTC_KJC_FW_MAJOR   2
#define KTC_KJC_FW_MINOR   0

extern const KtcBootCfg    ktcKjcBoot;
extern const KtcVisualSeq  ktcKjcStates;      // LED states, brightness and bulb test
extern const KtcCurrentCfg ktcKjcCurrent;

/** @brief Hands off: all three axes read 0 and no INT for 2 s. */
KtcVerdict ktcStepKjcCentre(KtcCtx& c);

/** @brief One axis: full travel both ways, back to centre each time. */
struct KtcAxisCfg {
    uint8_t     axis;       // 0 = AXIS1, 1 = AXIS2, 2 = AXIS3
    const char* posMove;    // e.g. "Push the stick fully RIGHT"
    const char* negMove;    // e.g. "Push the stick fully LEFT"
    const char* posName;    // short name for the result note, e.g. "right"
    int8_t      expect;     // sign expected for posMove (+1 / -1), 0 = record only
};
KtcVerdict ktcStepKjcAxis(KtcCtx& c);

void ktcKjcCleanup(KtcCtx& c);
void ktcKjcLive(KtcCtx& c);
