#ifndef _COLOR_DEFINITIONS_H
#define _COLOR_DEFINITIONS_H

/***************************************************************************************
  Color definitions that can be used during the various drawing functions
  Bits 0..4 -> Blue 0..4
  Bits 5..10 -> Green 0..5
  Bits 11..15 -> Red 0..4
  Ref: https://rgbcolorpicker.com/565
  Ref: https://github.com/newdigate/rgb565_colors
****************************************************************************************/
#define BLACK 0x0000        /*   0,   0,   0 */
#define OFF_BLACK 0x2104    /*   4,   8,   4 */
#define DARK_GREY 0x39E7    /*   7,  15,   7 */
#define GREY 0x8410         /*  16,  16,  16 */
#define LIGHT_GREY 0xBDF7   /*  23,  47,  23 */
#define WHITE 0xFFFF        /*  31,  63,  31 */
#define GREEN 0x07E0        /*   0,  63,   0 */
#define DARK_GREEN 0x03E0   /*   0,  31,   0 */
#define JUNGLE 0x01E0       /*   0,  15,   0 */
#define RED 0xF800          /*  31,   0,   0 */
#define MAROON 0x7800       /*  15,   0,   0 */
#define CORNELL 0xB0E3      /*  22,   7,   3 */
#define DARK_RED 0x6000     /*  12,   0,   0 */
#define BLUE 0x001F         /*   0,   0,  31 */
#define SKY 0x761F          /*  14,  48,  31 */
#define ROYAL 0x010C        /*   0,   8,  12 */
#define AQUA 0x5D1C         /*  11,  40,  28 */
#define NAVY 0x000F         /*   0,   0,  15 */
#define CYAN 0x07FF         /*   0,  63,  31 */
#define FRENCH_BLUE 0x347C  /*   6,  35,  28 */
#define MAGENTA 0xF81F      /*  31,   0,  31 */
#define PURPLE 0x8010       /*  16,   0,  16 */
#define VIOLET 0x901A       /*  18,   0,  26 */
#define YELLOW 0xFDC2       /*  31,  46,   2 */
#define DULL_YELLOW 0xEEEB  /*  29,  55,  11 */
#define DARK_YELLOW 0xA500  /*  20,  40,   0 */
#define OLIVE 0x8400        /*  16,  32,   0 */
#define BROWN 0x8200        /*  16,  16,  40 */
#define SILVER 0xC618       /*  24,  48,  24 */
#define GOLD 0xD566         /*  26,  43,   6 */
#define ORANGE 0xFBE0       /*  31,  31,   0 */
#define AIR_SUP_BLUE 0x7517 /* 14,   40,  23 */
#define NEON_GREEN 0x3FE2   /*  7,   63,   2 */
#define SAP_GREEN 0x53E5    /* 10,   31,   5 */
#define INT_ORANGE 0xFA80   /* 31,   20,   0 */
#define UPS_BROWN 0x6203    /* 12,   16,   3 */

#endif  // _COLOR_DEFINITIONS_H
