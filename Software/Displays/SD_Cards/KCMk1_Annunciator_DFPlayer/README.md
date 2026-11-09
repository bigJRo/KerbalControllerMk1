# Annunciator DFPlayer card (GPWS voice)

This folder is the card for the **DFPlayer Mini's own microSD slot** on the Annunciator, not the Teensy's slot (that card is `../KCMk1_Annunciator/`). The GPWS plays its voice callouts from folder `01` (`GPWS.ino`, `GPWS_FOLDER 1`), and nothing else uses this card.

Copy the `01` folder to the root of the DFPlayer's card (FAT32), so the clips sit at `/01/001_PULL_UP.mp3` … `/01/034_GROUND_PROX.mp3`.

The DFPlayer picks a track by the **three leading digits** of the file name; the text after them is only a label. MP3 and WAV both play: 012 ROTATE and 032 GEAR UP are WAV, the rest MP3. 34 clips, about 0.5 MB.

What each track says:

| Track | Spoken | | Track | Spoken |
|-------|--------|---|-------|--------|
| 001 | "WHOOP WHOOP PULL UP" | | 018 | "FOUR HUNDRED" |
| 002 | "TERRAIN, TERRAIN" | | 019 | "THREE HUNDRED" |
| 003 | "SINK RATE" | | 020 | "TWO HUNDRED" |
| 004 | "TOO LOW, GEAR" | | 021 | "ONE HUNDRED" |
| 005 | "TOO LOW, TERRAIN" | | 022 | "NINETY" |
| 006 | "DON'T SINK" (once; the firmware repeats it) | | 023 | "EIGHTY" |
| 007 | "BANK ANGLE, BANK ANGLE" | | 024 | "SEVENTY" |
| 008 | Stall buzzer (the firmware loops it) | | 025 | "SIXTY" |
| 009 | "MINIMUMS" | | 026 | "FIFTY" |
| 010 | "APPROACHING MINIMUMS" | | 027 | "FORTY" |
| 011 | "V ONE" | | 028 | "THIRTY" |
| 012 | "ROTATE" | | 029 | "TWENTY" |
| 013 | "RETARD" | | 030 | "TEN" |
| 014 | Bug tone / beep | | 031 | "FIVE" |
| 015 | "TWO THOUSAND FIVE HUNDRED" | | 032 | "GEAR UP" / "LANDING GEAR" |
| 016 | "ONE THOUSAND" | | 033 | "HORIZONTAL SPEED" |
| 017 | "FIVE HUNDRED" | | 034 | "GROUND PROXIMITY" |

The full callout logic, which clip plays when, is in `KCMk1_Annunciator/README.md` (GPWS section). Without the clips the master-alarm tones still work; only the voice is silent.

To replace a clip, keep its three-digit number and remove the old file: two files with the same number confuse the DFPlayer. `python3 tools/make_sd_cards.py --check` confirms there is exactly one MP3 or WAV in `01/` for every `GPWS_CLIP_*` number in `GPWS.ino`, and nothing else.
