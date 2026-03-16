/***************************************************************************************
   Audio.ino -- Audio wiring notes for Kerbal Controller Mk1 Annunciator

   Audio triggers live in ScreenMain.ino (C&W bit changes, threshold crossings).
   Scene change and vessel change silencing is handled in SimpitHandler.ino where
   those events are first received.

   The audio state machine is in the KerbalDisplayAudio library.
   All audio is gated by the audioEnabled flag in AAA_Config.ino.
   audioEnabled can also be set at runtime via I2C from the master.
****************************************************************************************/
#include "KCMk1_Annunciator.h"

