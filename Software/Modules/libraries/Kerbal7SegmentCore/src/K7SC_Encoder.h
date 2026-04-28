/**
 * @file        K7SC_Encoder.h
 * @version     2.0.0
 * @date        2026-04-27
 * @project     Kerbal Controller Mk1
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Rotary encoder input driver for KC-01-1881/1882.
 *
 *              Reports raw signed delta to inputState.encoderDelta.
 *              All value tracking, acceleration, and display updates
 *              are the sketch's responsibility.
 *
 *              Hardware: PEC11R-4220F-S0024, ENC_A on PB4 (interrupt),
 *              ENC_B on PB5 (sampled in ISR). Hardware RC debounce
 *              (10nF) on both channels.
 *
 * @license     GNU General Public License v3.0
 */

#pragma once
#include "K7SC_Config.h"

/** @brief Initialise encoder pins and interrupt. */
void encoderBegin();

/** @brief Poll encoder, accumulate delta into inputState.encoderDelta.
 *         Called by k7scUpdate() each loop. */
void encoderPoll();

/** @brief Discard any accumulated encoder movement. */
void encoderClearDelta();
