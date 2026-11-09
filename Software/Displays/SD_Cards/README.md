# KCMk1 SD Cards

One folder for each firmware that reads a microSD card. Each folder holds exactly what that firmware loads, arranged as it goes on the card: **copy the folder's contents (not the folder itself) to the root of the card.**

| Folder | Firmware | Board(s) | Card slot | Contents |
|---|---|---|---|---|
| `KCMk1_Annunciator/` | `KCMk1_Annunciator` | Annunciator (0x10) | Teensy 4.1 built-in slot | 17 SOI body globes (`*-Display_236x164.bmp`), `KASA_Meatball_236x164.bmp`, `StandbySplash_1024x600.bmp`, vessel icons `VIcon_00`–`VIcon_16.bmp` (72×72) |
| `KCMk1_InfoDisp/` | `KCMk1_InfoDisp` | Information Display 1 (0x12) **and** Information Display 2 (0x13): one card each, same contents | Teensy 4.1 built-in slot | `StandbySplash_1024x600.bmp`, vessel icons `VIcon60_00`–`VIcon60_16.bmp` (60×60, VEH screen), the `Rover/` folder (ROVER screen art: `Top.bmp` and the pitch / roll frames) |
| `KCMk1_ResourceDisp/` | `KCMk1_ResourceDisp` | Resource Display (0x11) | Teensy 4.1 built-in slot | `StandbySplash_1024x600.bmp` |
| `KCMk1_Annunciator_DFPlayer/` | `KCMk1_Annunciator` (GPWS voice) | Annunciator's DFPlayer Mini | The DFPlayer's own microSD slot | `01/001_PULL_UP.mp3` … `01/034_GROUND_PROX.mp3`: the 34 GPWS voice clips (0.5 MB). The folder's README gives the track list |

The Annunciator therefore takes **two** cards: one in the Teensy, one in the DFPlayer.

Totals: Annunciator 36 files (4.2 MB), Info Display 201 files (10.5 MB), Resource Display 1 file (1.8 MB). Any microSD card formatted FAT32 (or exFAT) will do.

## Firmware with nothing to load

| Firmware | Board | Card |
|---|---|---|
| `KCMk1_SysInfo` | Sys Info Display (0x14), XIAO RP2350, 2" TFT Display Driver | Optional. The power-on splash is built in. To replace it, put a 240×320 BMP (24-bit, or 16-bit RGB565 BI_BITFIELDS) on the card as `/KCMK1/SPLASH.BMP`. With no card, or a file it can't use, the built-in splash shows; neither is an error |
| `KCMk1_Master` | Control Module Teensy 4.1 | None. The firmware does not use the card |
| ATtiny816 modules | All module boards | No card slot |

## What happens without the card

Nothing stops working. A missing file is skipped and logged to Serial when debug mode is on:

- **Annunciator:** the SOI globe, meatball and vessel icon are left blank.
- **Information Display:** the VEH icon is left blank, and the ROVER screen uses its plain vector rover.
- **All three:** the standby screen is plain black.
- **Annunciator DFPlayer card:** the GPWS voice is silent; the master-alarm tones still sound.

## Keeping the folders up to date

`make_sd_cards.py` builds the three display folders. The DFPlayer folder holds recorded clips, so it is kept by hand, but `--check` also confirms it has exactly one clip for every number the GPWS firmware plays.

The art is generated and edited in `../assets/` (see its README). That is the only source: these folders are copies made by

```
python3 tools/make_sd_cards.py           # rebuild (run in Software/Displays)
python3 tools/make_sd_cards.py --check   # complete, in sync with assets/, and every
                                         # literal path in each sketch is on its card
```

Rerun it after changing anything in `assets/`, after `tools/rover_art.py`, or when a sketch starts reading a new file (add the file to the manifest in the script). The body globes are taken from the shared body table, `Software/Common/body_params.h`, so a new body is picked up automatically.

To check a card folder on its own in the host renderer: `python3 tools/render_screen.py --sketch KCMk1_InfoDisp --sd SD_Cards/KCMk1_InfoDisp ...`
