# KCMk1 Display Assets (SD card)

The display panels load artwork from the **root of the SD card** as 24-bit
uncompressed BMPs via `drawBMP()` in `KerbalDisplayCommon`. Assets are **not**
compiled into the firmware — they live on the microSD card the Teensy 4.1 reads
via `BUILTIN_SDCARD`.

## Format (required)

`drawBMP()` accepts **24-bit, uncompressed (BI_RGB)** BMPs only, up to 1024×600.
Both bottom-up and top-down are supported. Anything else — paletted/8-bit,
16-bit, 32-bit, or RLE-compressed — is rejected and logged to Serial when
`debugMode` is on (e.g. `only 24-bit BMPs are supported`,
`compressed BMP not supported`). If a rev-1 image doesn't render, check its
format first: re-export as "24-bit BMP" from any editor.

## Required files (SD card root)

Copy these to the card root with **exact** filenames (referenced verbatim in the
sketches):

Annunciator SOI + Main body globes (240×168 each):
```
Kerbol-Display_240x168.bmp   Moho-Display_240x168.bmp    Eve-Display_240x168.bmp
Gilly-Display_240x168.bmp    Kerbin-Display_240x168.bmp  Mun-Display_240x168.bmp
Minmus-Display_240x168.bmp   Duna-Display_240x168.bmp    Ike-Display_240x168.bmp
Dres-Display_240x168.bmp     Jool-Display_240x168.bmp    Laythe-Display_240x168.bmp
Vall-Display_240x168.bmp     Tylo-Display_240x168.bmp    Bop-Display_240x168.bmp
Pol-Display_240x168.bmp      Eeloo-Display_240x168.bmp
```
Other:
```
KASA_Meatball_240x168.bmp     (SOI screen, top-left)
StandbySplash_1024x600.bmp    (standby screen, full panel)
```

The 240×168 body/meatball images are unchanged from rev-1 — copy your existing
set across. Only `StandbySplash_1024x600.bmp` is new (rev-1 was 800×480).

Note: on the Main screen the globe *frame* is 274×176 and the 240×168 image is
centered inside it. To fill the frame edge-to-edge, supply 274×176 body art and
ask for the draw calls to be updated.

## Tooling — `../tools/kcm_bmp.py`

Dependency-free (no PIL) 24-bit BMP writer. Regenerate a verification test
pattern any time to re-check the blit path:

```
python3 ../tools/kcm_bmp.py testpattern KASA_Meatball_240x168.bmp 240 168
```

The test pattern is corner-coded (TL red, TR green, BL blue, BR yellow), a white
up-arrow marking true top, an L→R gradient, and a white border — confirms colour,
orientation, and placement. (The RA8876 blit path was verified with it on
2026-07; renders upright and correct.)
