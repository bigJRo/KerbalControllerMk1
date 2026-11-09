# Annunciator DFPlayer card (GPWS voice)

This card goes in the **DFPlayer Mini's own microSD slot** on the Annunciator, not in the Teensy (that card is `../KCMk1_Annunciator/`). The GPWS plays its voice callouts from folder `01` (`GPWS.ino`, `GPWS_FOLDER 1`), and nothing else uses this card.

**The clips are not in the repository yet.** Record or obtain them, then lay the card out like this (FAT32, file names exactly as shown):

```
01/001.mp3
01/002.mp3
 ...
01/034.mp3
```

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

When the clips exist, put them in `01/` here, and this folder becomes ready to copy like the others.
