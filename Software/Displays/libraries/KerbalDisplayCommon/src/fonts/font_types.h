#ifndef FONT_TYPES_H
#define FONT_TYPES_H

// Pull in tImage, tChar, tFont from the RA8875 bundled font header.
// This file exists so the font .c files have a single stable include
// regardless of RA8875 library version or location.
#include <stdint.h>
#include <stdbool.h>   // font.h uses 'bool' — required for C compilation
#include "_includes/font.h"

// __PRGMTAG_ is used in the font data arrays for PROGMEM on AVR.
// On Teensy (ARM) it is a no-op.
#ifndef __PRGMTAG_
  #define __PRGMTAG_
#endif

#endif // FONT_TYPES_H
