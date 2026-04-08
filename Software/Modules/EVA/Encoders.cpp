/**
 * @file        Encoders.cpp
 * @version     1.0
 * @date        2026-04-08
 * @project     Kerbal Controller Mk1 — EVA Module
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Stub implementation of rotary encoder interface.
 *              Encoders are present on the PCB but not currently
 *              used. All functions are safe no-ops or return zero.
 *
 *              To implement quadrature decoding in a future revision:
 *                1. Attach pin change interrupts to ENC1_A and ENC2_A
 *                2. In ISR, read B pin to determine direction
 *                3. Increment/decrement _delta1 / _delta2 accordingly
 *                4. Assert INT when delta changes (optional)
 *
 * @license     Licensed under the GNU General Public License v3.0 (GPL-3.0)
 *              https://www.gnu.org/licenses/gpl-3.0.html
 */

#include "Encoders.h"

static volatile int8_t _delta1 = 0;
static volatile int8_t _delta2 = 0;

void encodersBegin() {
    // Configure encoder pins as inputs
    // No pull-ups — hardware pull-downs fitted on this PCB
    // Uncomment INPUT_PULLUP if common-ground encoders are fitted
    pinMode(EVA_ENC1_PIN_A,  INPUT);
    pinMode(EVA_ENC1_PIN_B,  INPUT);
    pinMode(EVA_ENC1_PIN_SW, INPUT);
    pinMode(EVA_ENC2_PIN_A,  INPUT);
    pinMode(EVA_ENC2_PIN_B,  INPUT);
    pinMode(EVA_ENC2_PIN_SW, INPUT);

    _delta1 = 0;
    _delta2 = 0;

    // Future: attach pin change interrupts here
    // attachInterrupt(digitalPinToInterrupt(EVA_ENC1_PIN_A), enc1ISR, CHANGE);
    // attachInterrupt(digitalPinToInterrupt(EVA_ENC2_PIN_A), enc2ISR, CHANGE);
}

int8_t encodersGetDelta1() {
    return _delta1;
}

int8_t encodersGetDelta2() {
    return _delta2;
}

void encodersClearDeltas() {
    _delta1 = 0;
    _delta2 = 0;
}
