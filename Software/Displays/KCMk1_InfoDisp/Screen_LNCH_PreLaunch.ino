/***************************************************************************************
   Screen_LNCH_PreLaunch.ino -- Pre-launch board (one of three modes of the LNCH screen)

   Status board shown automatically when state.situation has sit_PreLaunch set.
   Eight rows of pre-flight readiness checks: vessel name + type, SAS/RCS,
   throttle/EC%, crew/CommNet, total ΔV, drogue+main deploy/cut arming.

   Phase membership (LNCH screen has three phases):
     - PRE-LAUNCH (this file)        — auto, when sit_PreLaunch is set
     - ASCENT     (Screen_LNCH_Ascent.ino)  — when not pre-launch and below ~6% bodyR
     - CIRC       (Screen_LNCH_Circ.ino)    — when above ~6% bodyR (with hysteresis)

   Top-level dispatcher Screen_LNCH.ino selects which of these to draw.

   Public-to-the-sketch entry points (called by the LNCH dispatcher):
     - _lnchPrelaunchDrawChrome(tft)  — labels, dividers, title bar
     - _lnchPrelaunchDrawValues(tft)  — per-frame value updates, change-detected
****************************************************************************************/

// ── Pre-launch layout constants ───────────────────────────────────────────────────────
// All Black_40 cells, 8 equal-height rows. Several rows split into left/right halves
// with a 2-px central vertical divider (drawn in chrome).
static const tFont   *_lnchPlFont   = &Roboto_Black_40;
static const uint8_t  _lnchPlNumRows = 8;
static const uint16_t _lnchPlSectW  = 26;                                    // section indent (matches other screens)
static const uint16_t _lnchPlAX     = ROW_PAD + _lnchPlSectW;                 // first cell X
static const uint16_t _lnchPlAW     = CONTENT_W - _lnchPlAX - ROW_PAD;        // full row width
static const uint16_t _lnchPlAHW    = _lnchPlAW / 2;                           // half-width for split rows


// ── Chrome ─────────────────────────────────────────────────────────────────────────────
// Draws labels, dividers, and title bar. Called once per screen-entry. Values are
// updated separately by _lnchPrelaunchDrawValues each frame.
static void _lnchPrelaunchDrawChrome(RA8875 &tft) {
    const uint8_t  NR   = _lnchPlNumRows;
    const tFont   *F    = _lnchPlFont;
    const uint16_t AX   = _lnchPlAX;
    const uint16_t AW   = _lnchPlAW;
    const uint16_t AHW  = _lnchPlAHW;
    uint16_t rowH       = rowHFor(NR);

    // No section labels — pure status board. 8 rows, all split except rows 0, 1, 5.
    drawTitleBar(tft, "PRE-LAUNCH");  // no toggle indicator

    printDispChrome(tft, F, AX, rowYFor(0,NR), AW, rowH, "Vessel:",  COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(1,NR), AW, rowH, "Type:",    COL_LABEL, COL_BACK, COL_NO_BDR);

    // Split rows 2-7: vertical dividers and left/right labels
    auto plSplit = [&](uint8_t row, const char *lLabel, const char *rLabel) {
        uint16_t y = rowYFor(row, NR), h = rowH;
        printDispChrome(tft, F, AX,                 y, AHW - ROW_PAD, h, lLabel, COL_LABEL, COL_BACK, COL_NO_BDR);
        printDispChrome(tft, F, AX + AHW + ROW_PAD, y, AHW - ROW_PAD, h, rLabel, COL_LABEL, COL_BACK, COL_NO_BDR);
        uint16_t nextY = (row < NR - 1) ? rowYFor(row + 1, NR) : SCREEN_H;
        for (int8_t dx = -1; dx <= 1; dx++)
            tft.drawLine(AX + AHW + dx, y, AX + AHW + dx, nextY - 1, TFT_GREY);
    };
    plSplit(2, "SAS:",      "RCS:");
    plSplit(3, "Throttle:", "EC%:");
    plSplit(4, "Crew:",     "Comm:");
    printDispChrome(tft, F, AX, rowYFor(5,NR), AW, rowH, "\xCE\x94V.Tot:", COL_LABEL, COL_BACK, COL_NO_BDR);
    plSplit(6, "Drogue:",   "Main:");
    plSplit(7, "D.Cut:",    "M.Cut:");

    // Horizontal dividers between every row — makes board easier to read as a checklist
    for (uint8_t row = 1; row < NR; row++) {
        uint16_t dy = TITLE_TOP + rowHFor(NR) * row;
        tft.drawLine(0, dy,   CONTENT_W, dy,   TFT_GREY);
        tft.drawLine(0, dy+1, CONTENT_W, dy+1, TFT_GREY);
    }
}


// ── Per-frame value updates ────────────────────────────────────────────────────────────
// Updates the eight rows of values with change detection. Horizontal dividers are
// repainted every frame because printValue's fillRect sometimes nibbles them (same
// pattern as ATT and ROVR screens).
static void _lnchPrelaunchDrawValues(RA8875 &tft) {
    const uint8_t  NR   = _lnchPlNumRows;
    const tFont   *F    = _lnchPlFont;
    const uint16_t AX   = _lnchPlAX;
    const uint16_t AW   = _lnchPlAW;
    const uint16_t AHW  = _lnchPlAHW;
    uint16_t fg, bg;

    // plVal -> drawValue() full-row variant (slot=cache idx, row=display row) (#6C/#44)
    auto plVal = [&](uint8_t row, uint8_t slot, const char *label, const String &val,
                     uint16_t fgc, uint16_t bgc) {
        RowCache &rc = rowCache[0][slot];
        if (rc.value == val && rc.fg == fgc && rc.bg == bgc) return;
        printValue(tft, F, AX, rowYFor(row, NR), AW, rowHFor(NR),
                   label, val, fgc, bgc, COL_BACK, printState[0][slot]);
        rc.value = val; rc.fg = fgc; rc.bg = bgc;
    };

    // plValL -> left-half split, slot=cache idx (#6C/#44)
    auto plValL = [&](uint8_t row, uint8_t slot, const char *label, const String &val,
                      uint16_t fgc, uint16_t bgc) {
        uint16_t xL = AX, wL = AHW - ROW_PAD;
        RowCache &rc = rowCache[0][slot];
        if (rc.value == val && rc.fg == fgc && rc.bg == bgc) return;
        printValue(tft, F, xL, rowYFor(row, NR), wL, rowHFor(NR),
                   label, val, fgc, bgc, COL_BACK, printState[0][slot]);
        rc.value = val; rc.fg = fgc; rc.bg = bgc;
    };

    // plValR -> right-half split, slot=cache idx (#6C/#44)
    auto plValR = [&](uint8_t row, uint8_t slot, const char *label, const String &val,
                      uint16_t fgc, uint16_t bgc) {
        uint16_t xR = AX + AHW + ROW_PAD, wR = AHW - ROW_PAD;
        RowCache &rc = rowCache[0][slot];
        if (rc.value == val && rc.fg == fgc && rc.bg == bgc) return;
        printValue(tft, F, xR, rowYFor(row, NR), wR, rowHFor(NR),
                   label, val, fgc, bgc, COL_BACK, printState[0][slot]);
        rc.value = val; rc.fg = fgc; rc.bg = bgc;
    };

    // Row 0 — Vessel name (full width, always dark green)
    plVal(0, 14, "Vessel:", state.vesselName, TFT_DARK_GREEN, TFT_BLACK);

    // Row 1 — Vessel type (full width, always dark green — informational confirmation)
    {
        const char *typeName;
        switch (state.vesselType) {
            case type_Ship:     typeName = "Ship";      break;
            case type_Probe:    typeName = "Probe";     break;
            case type_Relay:    typeName = "Relay";     break;
            case type_Rover:    typeName = "Rover";     break;
            case type_Lander:   typeName = "Lander";    break;
            case type_Plane:    typeName = "Plane";     break;
            case type_Station:  typeName = "Station";   break;
            case type_Base:     typeName = "Base";      break;
            case type_EVA:      typeName = "EVA";       break;
            default:            typeName = "Unknown";   break;
        }
        plVal(1, 15, "Type:", typeName, TFT_DARK_GREEN, TFT_BLACK);
    }

    // Row 2 — SAS (left) | RCS (right)
    {
        // SAS: green=STAB, yellow=other mode on, red=OFF
        const char *sasStr;
        if (state.sasMode == 255)     { sasStr = "OFF";  fg = TFT_WHITE;     bg = TFT_RED;   }
        else if (state.sasMode == 0)  { sasStr = "STAB"; fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
        else {
            // Any other SAS mode — show mode name, yellow
            switch (state.sasMode) {
                case 1: sasStr = "PROGRADE"; break; case 2: sasStr = "RETRO";    break;
                case 3: sasStr = "NORMAL";   break; case 4: sasStr = "ANTI-NRM"; break;
                case 5: sasStr = "RAD-OUT";  break; case 6: sasStr = "RAD-IN";   break;
                case 7: sasStr = "TARGET";   break; case 8: sasStr = "ANTI-TGT"; break;
                case 9: sasStr = "MANEUVER"; break; default: sasStr = "OTHER";   break;
            }
            fg = TFT_YELLOW; bg = TFT_BLACK;
        }
        plValL(2, 2, "SAS:", sasStr, fg, bg);

        // RCS: green=OFF, red=ON (no RCS during launch)
        const char *rcsStr = state.rcs_on ? "ON" : "OFF";
        uint16_t rcsFg     = state.rcs_on ? TFT_WHITE     : TFT_DARK_GREEN;
        uint16_t rcsBg     = state.rcs_on ? TFT_RED        : TFT_BLACK;
        plValR(2, 3, "RCS:", rcsStr, rcsFg, rcsBg);
    }

    // Row 3 — Throttle (left) | EC% (right)
    {
        // Throttle: green=0%, red=any non-zero
        uint8_t thrPct = (uint8_t)constrain(state.throttle * 100.0f, 0.0f, 100.0f);
        String thrStr = formatPerc(thrPct);
        if (thrPct == 0) { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
        else             { fg = TFT_WHITE;       bg = TFT_RED;   }
        plValL(3, 4, "Throttle:", thrStr, fg, bg);

        // EC%: green≥90%, yellow=75-89%, red=<75%
        uint8_t ecPct = (uint8_t)constrain(state.electricChargePercent, 0.0f, 100.0f);
        String ecStr = formatPerc(ecPct);
        if      (ecPct >= 90) { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
        else if (ecPct >= 75) { fg = TFT_YELLOW;     bg = TFT_BLACK; }
        else                  { fg = TFT_WHITE;       bg = TFT_RED;   }
        plValR(3, 5, "EC%:", ecStr, fg, bg);
    }

    // Row 4 — Crew (left) | CommNet (right)
    {
        // Crew: always dark green — pilot reads and confirms count
        char crewBuf[10];
        snprintf(crewBuf, sizeof(crewBuf), "%d / %d", state.crewCount, state.crewCapacity);
        plValL(4, 6, "Crew:", crewBuf, TFT_DARK_GREEN, TFT_BLACK);

        // CommNet: green=>50%, yellow=10-50%, red=<10%
        uint8_t sig = (uint8_t)constrain(state.commNetSignal, 0, 100);
        String sigStr = formatPerc(sig);
        if      (sig >= 50) { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
        else if (sig >= 10) { fg = TFT_YELLOW;     bg = TFT_BLACK; }
        else                { fg = TFT_WHITE;       bg = TFT_RED;   }
        plValR(4, 7, "CommNet:", sigStr, fg, bg);
    }

    // Row 5 — ΔV.Tot (full width) — total mission delta-V at a glance
    {
        thresholdColor(state.totalDeltaV,
                       DV_STG_ALARM_MS, TFT_WHITE, TFT_RED,
                       DV_TOT_WARN_MS,  TFT_YELLOW, TFT_BLACK,
                            TFT_DARK_GREEN, TFT_BLACK, fg, bg);
        plVal(5, 16, "\xCE\x94V.Tot:", fmtMs(state.totalDeltaV), fg, bg);
    }

    // Row 6 — Drogue deploy (CAG 1) | Main deploy (CAG 3)
    {
        // Green = STOWED (CAG off), Red = ARMED! (CAG on)
        const char *dStr = state.drogueDeploy ? "ARMED!" : "STOWED";
        uint16_t dFg = state.drogueDeploy ? TFT_WHITE : TFT_DARK_GREEN;
        uint16_t dBg = state.drogueDeploy ? TFT_RED   : TFT_BLACK;
        plValL(6, 10, "Drogue:", dStr, dFg, dBg);

        const char *mStr = state.mainDeploy ? "ARMED!" : "STOWED";
        uint16_t mFg = state.mainDeploy ? TFT_WHITE : TFT_DARK_GREEN;
        uint16_t mBg = state.mainDeploy ? TFT_RED   : TFT_BLACK;
        plValR(6, 11, "Main:", mStr, mFg, mBg);
    }

    // Row 7 — Drogue cut (CAG 2) | Main cut (CAG 4)
    {
        // Green = SAFE (CAG off), Red = FIRED! (CAG on)
        const char *dcStr = state.drogueCut ? "FIRED!" : "SAFE";
        uint16_t dcFg = state.drogueCut ? TFT_WHITE : TFT_DARK_GREEN;
        uint16_t dcBg = state.drogueCut ? TFT_RED   : TFT_BLACK;
        plValL(7, 12, "D.Cut:", dcStr, dcFg, dcBg);

        const char *mcStr = state.mainCut ? "FIRED!" : "SAFE";
        uint16_t mcFg = state.mainCut ? TFT_WHITE : TFT_DARK_GREEN;
        uint16_t mcBg = state.mainCut ? TFT_RED   : TFT_BLACK;
        plValR(7, 13, "M.Cut:", mcStr, mcFg, mcBg);
    }

    // Redraw horizontal dividers every frame — printValue fillRect overwrites them.
    // Same pattern as ATT and ROVR screens.
    {
        uint16_t rH = rowHFor(NR);
        for (uint8_t row = 1; row < NR; row++) {
            uint16_t dy = TITLE_TOP + rH * row;
            tft.drawLine(0, dy,   CONTENT_W, dy,   TFT_GREY);
            tft.drawLine(0, dy+1, CONTENT_W, dy+1, TFT_GREY);
        }
    }
}
