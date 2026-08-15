/**
 * @file        ConstructionTest.h
 * @version     2.0.0
 * @project     Kerbal Controller Mk1 — Module Tester
 * @author      J. Rostoker
 * @organization Jeb's Controller Works
 *
 * @brief       Guided per-board-type construction test. For a freshly built
 *              module it walks the operator through a sequence of steps that
 *              drive the board's outputs and verify its inputs, catching
 *              assembly faults (dead pixels, miswired/cold-joint buttons,
 *              swapped axes, motor/encoder/touch failures).
 *
 *              Logic only — it drives I/O via TesterHW and renders via the
 *              uiCt* functions in TesterUI. Entered from the dashboard
 *              "Test" control and runs until the operator finishes or aborts.
 *
 * @license     GNU General Public License v3.0 (GPL-3.0)
 */

#pragma once
#include <Arduino.h>
#include "ModuleCatalog.h"

/** @brief Begin a construction test for the given module. */
void ctBegin(const ModuleInfo* info, uint8_t addr);

/** @brief True while a construction test is running. */
bool ctActive();

/**
 * @brief  Service the construction test: drive outputs, read inputs, render,
 *         and handle touch. Call every loop() while ctActive() is true.
 *         Clears the active flag when the operator finishes or aborts.
 */
void ctUpdate();
