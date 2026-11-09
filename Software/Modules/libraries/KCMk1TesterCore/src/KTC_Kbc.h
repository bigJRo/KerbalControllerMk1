/**
 * @file        KTC_Kbc.h
 * @version     1.0.0
 * @project     Kerbal Controller Mk1 — Module Tester per-module sketches
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Ready-made checks for the KerbalButtonCore modules (UI, Function,
 *              Action, Stability, Vehicle, Time and Auxiliary Control). All
 *              seven run the same library (identity fw 2.4): 12 WS2811
 *              NeoPixels on positions 0-11, inputs reported as a level plane
 *              plus a change plane, LED states set only by the master.
 *
 *              Behaviour the checks expect (KerbalButtonCore 2.4 source):
 *                - ENABLED renders cool white scaled by CMD_SET_BRIGHTNESS
 *                  (default 32), not KMC_BACKLIT.
 *                - ACTIVE_ALT renders the ACTIVE colour (no module passes an
 *                  alternate colour table).
 *                - WARNING flashes amber 500 ms, ALERT red 150 ms; any ALERT
 *                  speeds every flashing LED to 150 ms.
 *                - Bulb test: all 12 full white, states restored on stop.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */
#pragma once
#include "KTC_Steps.h"

#define KTC_KBC_FW_MAJOR   2
#define KTC_KBC_FW_MINOR   4
#define KTC_KBC_BRIGHTNESS_DEFAULT  32

extern const KtcBootCfg    ktcKbcBoot;
extern const KtcVisualSeq  ktcKbcStatesCore;       // OFF, ENABLED, ACTIVE, ACTIVE_ALT
extern const KtcVisualSeq  ktcKbcStatesExtended;   // core + WARNING, ALERT, ARMED, PARTIAL_DEPLOY, CUT
extern const KtcVisualSeq  ktcKbcBrightness;       // ENABLED at brightness 0 / 32 / 128 / 255
extern const KtcVisualSeq  ktcKbcBulb;             // bulb on, bulb off restores the pattern
extern const KtcCurrentCfg ktcKbcCurrent;          // idle dark vs bulb test

/** @brief Cleanup for a KtcModuleDef: bulb off, brightness 32, all LEDs ENABLED. */
void ktcKbcCleanup(KtcCtx& c);
/** @brief LIVE-screen decoder for 16- and 24-input button modules. */
void ktcKbcLive16(KtcCtx& c);
void ktcKbcLive24(KtcCtx& c);
