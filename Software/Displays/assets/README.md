# KCMk1 Display Assets (SD card)

The display panels load artwork from the **root of the SD card** as 24-bit
uncompressed BMPs via `drawBMP()` in `KerbalDisplayCommon`.

## Installing

Copy the `.bmp` files from this folder to the **root** of the microSD card
(the same card the Teensy 4.1 reads via `BUILTIN_SDCARD`). Filenames must match
exactly (they are referenced verbatim in the sketches), e.g.
`/KASA_Meatball_240x168.bmp`, `/StandbySplash_1024x600.bmp`,
`/Kerbin-Display_240x168.bmp`.

## Format

`drawBMP()` accepts **24-bit, uncompressed (BI_RGB)** BMPs only, up to
1024×600. Both bottom-up and top-down are supported. Anything else (paletted,
32-bit, RLE) is rejected and logged to Serial when `debugMode` is on.

## Regenerating

`../tools/kcm_bmp.py` writes exactly this format with no external dependencies
(pure Python — no PIL). Examples:

```
python3 ../tools/kcm_bmp.py testpattern KASA_Meatball_240x168.bmp 240 168
python3 ../tools/kcm_bmp.py testpattern StandbySplash_1024x600.bmp 1024 600
```

## Current contents — PIPELINE TEST PATTERNS (temporary)

`KASA_Meatball_240x168.bmp` and `StandbySplash_1024x600.bmp` are currently
**verification test patterns**, not final art: corner-coded (TL red, TR green,
BL blue, BR yellow), a white up-arrow marking true top, a left→right gradient,
and a white border. Use them to confirm the RA8876 blit path renders with
correct colour, orientation, and placement, then replace with real artwork.
