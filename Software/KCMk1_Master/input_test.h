/***************************************************************************************
   input_test.h — the input test screen on Information Display 1 (Hardware Inventory
   §8.11, I2C spec §15.3). The display opens it with a two-finger pull down from its
   top-right corner and reports screen INPUT_TEST_SCREEN in its status byte 2; while it
   does, the master pushes what it reads from every module, raw:

   Page 0, every INPUT_TEST_INPUTS_MS (40 bytes):
     [0] 0xAB  [1] 0
     [2..3] present, [4..5] ACTIVE, [6..7] inputs known: a bit per module (ModId order)
     [8..23] button levels, little-endian: UI 2, FUNCTION 3, ACTION 2, STABILITY 2,
             VEHICLE 3, TIME 2, AUX 2 bytes (FUNCTION / VEHICLE bits 16–23 = switches)
     [24] ROTATION buttons (bit 0 stick, 1 BTN01, 2 BTN02)  [25..27] AXIS1..3, int8 %
     [28] TRANSLATION buttons                               [29..31] AXIS1..3, int8 %
     [32] GPWS: low nibble recent presses (BTN01..03, EN), high nibble module state
     [33] PRE-WARP: the same
     [34] lever 0..100 % (0xFF unknown)
     [35] throttle: bit 0 touched, 1 precision, 2 ACTIVE; high nibble recent presses
          (100 %, UP, DOWN, 0 %)
     [36] Dual Encoder: low nibble recent presses (ENC1, ENC2), high nibble control group
     [37] ENC2 count, int8 (accumulated by the master since boot)
     [38] control mode (CTRL_MODE_*)    [39] safe lines: bit 0 ABORT, bit 1 STAGE
   Page 1, every INPUT_TEST_MODULES_MS (20 bytes):
     [0] 0xAB  [1] 1  [2..14] INT line of each module (0..11 INT_1..12, 12..23
     B_INT_1..12, 0xFF none)  [15..16] GPWS value  [17..18] Pre-Warp value (uint16 LE)
     [19] 0
   "Recent" = a rising edge in a packet read within INPUT_TEST_RECENT_MS.
****************************************************************************************/
#pragma once
#include <Arduino.h>

void inputTestInit();
void inputTestService();                 // every pass, after carrierService()
bool inputTestOpen();                    // Info Display 1 shows the screen
void inputTestBuildInputs(uint8_t *f);   // INPUT_TEST_INPUTS_LEN bytes (for the selftest)
void inputTestBuildModules(uint8_t *f);  // INPUT_TEST_MODULES_LEN bytes
