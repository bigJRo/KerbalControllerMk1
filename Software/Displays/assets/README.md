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

Annunciator SOI + Main body globes (236×164 each):
```
Kerbol-Display_236x164.bmp   Moho-Display_236x164.bmp    Eve-Display_236x164.bmp
Gilly-Display_236x164.bmp    Kerbin-Display_236x164.bmp  Mun-Display_236x164.bmp
Minmus-Display_236x164.bmp   Duna-Display_236x164.bmp    Ike-Display_236x164.bmp
Dres-Display_236x164.bmp     Jool-Display_236x164.bmp    Laythe-Display_236x164.bmp
Vall-Display_236x164.bmp     Tylo-Display_236x164.bmp    Bop-Display_236x164.bmp
Pol-Display_236x164.bmp      Eeloo-Display_236x164.bmp
```
Other:
```
KASA_Meatball_236x164.bmp     (SOI screen, top-left)
StandbySplash_1024x600.bmp    (standby screen, full panel)
VIcon_00.bmp .. VIcon_16.bmp  (vessel-type icons — see section below)
```

Sizing: the body/meatball art is **236×164** (filenames match the actual pixels).
The sketches centre it via `BODY_IMG_W`/`BODY_IMG_H` (KCMk1_Annunciator.h): in the
274×176 Main globe frame and in the 240×168 SOI image slots. To fill the Main
globe edge-to-edge instead, supply 274×176 art and bump those constants.

## Also included (alternates / spares — not wired to code)

These ship on the card for convenience but no sketch loads them yet:
```
CompanyLogo_1024x600.bmp   CompanyLogo_800x480.bmp
KASA_Meatball_1024x600.bmp KASA_Meatball_800x480.bmp
KASA_Retro_1024x600.bmp    KASA_Retro_800x480.bmp
KASA_Worm_1024x600.bmp     KASA_Worm_800x480.bmp
StandbySplash_800x480.bmp
```
The `*_1024x600` variants are candidates for the standby splash / a logo screen on
the rev-2 panels; the `*_800x480` variants are the rev-1 (RA8875) sizes, kept for
the still-unported InfoDisp / ResourceDisp. Say which KASA logo you want as the
standby splash and I'll point `drawStandbySplash()` at it (or copy your pick to
`StandbySplash_1024x600.bmp`).

## Vessel-type icons (72×72)

The Annunciator Main screen draws a vessel-type icon in the right square of the
SPCFT tile, selected by `state.vesselType` (KSP `VesselType`, 0–16). Files are
indexed by that value:

```
VIcon_00.bmp Debris        VIcon_06.bmp Lander     VIcon_12.bmp Flag
VIcon_01.bmp SpaceObject   VIcon_07.bmp Ship       VIcon_13.bmp ScienceController
VIcon_02.bmp Unknown       VIcon_08.bmp Plane      VIcon_14.bmp SciencePart
VIcon_03.bmp Probe         VIcon_09.bmp Station    VIcon_15.bmp Part
VIcon_04.bmp Relay         VIcon_10.bmp Base       VIcon_16.bmp GroundPart
VIcon_05.bmp Rover         VIcon_11.bmp EVA
```

They are 72×72, pre-composited over **black** (the tile background) since BMP has
no alpha — so any PNG transparency is baked against black at conversion time.
Convert a source PNG set (named `NN_Name.png`) with:

```
python3 ../tools/kcm_bmp.py vesselicons <png_dir> .           # -> VIcon_NN.bmp
python3 ../tools/kcm_bmp.py png2bmp icon.png VIcon_07.bmp      # single, over black
python3 ../tools/kcm_bmp.py png2bmp icon.png out.bmp 202020    # composite over #202020
```

## Tooling — `../tools/kcm_bmp.py`

Dependency-free (no PIL) 24-bit BMP writer. Regenerate a verification test
pattern any time to re-check the blit path:

```
python3 ../tools/kcm_bmp.py testpattern /tmp/pattern.bmp 236 164
```

The test pattern is corner-coded (TL red, TR green, BL blue, BR yellow), a white
up-arrow marking true top, an L→R gradient, and a white border — confirms colour,
orientation, and placement. (The RA8876 blit path was verified with it on
2026-07; renders upright and correct.)
