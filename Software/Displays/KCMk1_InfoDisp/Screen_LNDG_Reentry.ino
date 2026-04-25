/***************************************************************************************
   Screen_LNDG_Reentry.ino -- Re-entry mode (one of two modes of the LNDG screen).

   Active when _lndgReentryMode is TRUE. Pilot toggles via tap (handled in
   TouchEvents.ino). Text-only readout-board layout, similar in style to the
   PRE-LAUNCH board.

   Phase membership (LNDG screen has two modes):
     - POWERED DESCENT (Screen_LNDG_Powered.ino) — default
     - RE-ENTRY (this file)                       — pilot toggle

   Top-level dispatcher Screen_LNDG.ino selects which to draw based on
   _lndgReentryMode.

   Public-to-the-sketch entry points (called by the LNDG dispatcher):
     - _lndgChromeReentry(tft) — initial chrome (labels + dividers)
     - _lndgDrawReentry(tft)   — per-frame value updates with row-label swapping

   Re-entry-mode-specific row-toggle state (_lndgReentryRow0TPe, _lndgReentryRow1SL,
   _lndgReentryRow3PeA) and parachute deploy/cut state (_drogueDeployed etc.) are
   defined in Screen_LNDG.ino as extern globals — accessed here directly.
****************************************************************************************/

/***************************************************************************************
   CHROME: RE-ENTRY (text-only, unchanged from original)
****************************************************************************************/
static void _lndgChromeReentry(RA8875 &tft) {
    static const tFont   *F      = &Roboto_Black_40;
    static const uint8_t  NR     = 8;
    static const uint16_t SECT_W = 26;
    static const uint16_t AX     = ROW_PAD + SECT_W;
    static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
    static const uint16_t AHW    = AW / 2;
    uint16_t rowH = rowHFor(NR);

    drawVerticalText(tft, 0, TITLE_TOP, SECT_W, rowH*5,
                     &Roboto_Black_16, "STATE", TFT_LIGHT_GREY, TFT_BLACK);

    const char *row0Label = _lndgReentryRow0TPe ? "T+Atm:" : "T.Grnd:";
    const char *row1Label = _lndgReentryRow1SL  ? "Alt.SL:" : "Alt.Rdr:";
    printDispChrome(tft, F, AX, rowYFor(0,NR), AW, rowH, row0Label,  COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(1,NR), AW, rowH, row1Label,  COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(2,NR), AW, rowH, "V.Vrt:",   COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(3,NR), AW, rowH,
                    _lndgReentryRow3PeA ? "PeA:" : "V.Hrz:", COL_LABEL, COL_BACK, COL_NO_BDR);
    printDispChrome(tft, F, AX, rowYFor(4,NR), AW, rowH, "V.Srf:", COL_LABEL, COL_BACK, COL_NO_BDR);

    drawVerticalText(tft, 0, TITLE_TOP + rowH*6 + 2, SECT_W, rowH*2 - 4,
                     &Roboto_Black_16, "VEH", TFT_LIGHT_GREY, TFT_BLACK);

    auto halfRow = [&](uint8_t row, const char *lbl, const char *rbl) {
        uint16_t y = rowYFor(row, NR), h = rowH;
        printDispChrome(tft, F, AX,             y, AHW-ROW_PAD, h, lbl, COL_LABEL, COL_BACK, COL_NO_BDR);
        printDispChrome(tft, F, AX+AHW+ROW_PAD, y, AHW-ROW_PAD, h, rbl, COL_LABEL, COL_BACK, COL_NO_BDR);
        for (int8_t dx = -1; dx <= 1; dx++)
            tft.drawLine(AX+AHW+dx, y, AX+AHW+dx, rowYFor(row+1,NR)-1, TFT_GREY);
    };
    halfRow(5, "Mach:", "G:");
    halfRow(6, "Drogue:", "Main:");
    // Row 7: Gear | SAS — buttons drawn in update function, only need the vertical divider
    {
        uint16_t y = rowYFor(7, NR);
        for (int8_t dx = -1; dx <= 1; dx++)
            tft.drawLine(AX+AHW+dx, y, AX+AHW+dx, SCREEN_H - 1, TFT_GREY);
    }

    uint16_t d1 = TITLE_TOP + rowH*5 + 1;
    uint16_t d2 = TITLE_TOP + rowH*6 + 1;
    tft.drawLine(0, d1,   CONTENT_W, d1,   TFT_GREY);
    tft.drawLine(0, d1+1, CONTENT_W, d1+1, TFT_GREY);
    tft.drawLine(0, d2,   CONTENT_W, d2,   TFT_GREY);
    tft.drawLine(0, d2+1, CONTENT_W, d2+1, TFT_GREY);

    drawVerticalText(tft, 0, TITLE_TOP + rowH*5 + 2, SECT_W, rowH - 4,
                     &Roboto_Black_16, "AT", TFT_LIGHT_GREY, TFT_BLACK);
}

/***************************************************************************************
   DRAW: RE-ENTRY (unchanged from original)
****************************************************************************************/
static void _lndgDrawReentry(RA8875 &tft) {
    static const tFont   *F      = &Roboto_Black_40;
    static const uint8_t  NR     = 8;
    static const uint16_t SECT_W = 26;
    static const uint16_t AX     = ROW_PAD + SECT_W;
    static const uint16_t AW     = CONTENT_W - AX - ROW_PAD;
    static const uint16_t AHW    = AW / 2;
    uint16_t fg, bg;
    char buf[16];

    auto lndgVal = [&](uint8_t row, const char *label, const String &val,
                       uint16_t fgc, uint16_t bgc) {
        drawValue(tft, 6, row, AX, AW, label, val, fgc, bgc, F, NR);
    };

    bool inOrbitOrEscape = (state.situation == sit_Orbit || state.situation == sit_Escaping);
    float tGround = (!inOrbitOrEscape && state.verticalVel < -0.05f && state.radarAlt > 0.0f)
                    ? fabsf(state.radarAlt / state.verticalVel) : -1.0f;
    float vSq  = state.surfaceVel * state.surfaceVel - state.verticalVel * state.verticalVel;
    float hSpd = (vSq > 0.0f) ? sqrtf(vSq) : 0.0f;
    float atmoAlt = (currentBody.lowSpace > 0.0f) ? currentBody.lowSpace : 70000.0f;
    bool peaBelowAtmo = (state.periapsis < atmoAlt);
    bool aboveAtmo    = !state.inAtmo;
    bool wantTPe      = (aboveAtmo && peaBelowAtmo);
    if (wantTPe != _lndgReentryRow0TPe) {
        _lndgReentryRow0TPe = wantTPe; rowCache[6][0].value = "\x01";
        switchToScreen(screen_LNDG); return;
    }
    bool wantSL = aboveAtmo;
    if (wantSL != _lndgReentryRow1SL) {
        _lndgReentryRow1SL = wantSL; rowCache[6][1].value = "\x01";
        switchToScreen(screen_LNDG); return;
    }

    if (wantTPe) {
        float tAtmo = (state.verticalVel < -5.0f)
                      ? fabsf((state.altitude - atmoAlt) / state.verticalVel) : -1.0f;
        if      (tAtmo >= 0.0f)             lndgVal(0, "T+Atm:", formatTime(tAtmo), TFT_DARK_GREEN, TFT_BLACK);
        else if (state.verticalVel <= 5.0f) lndgVal(0, "T+Atm:", "---", TFT_DARK_GREEN, TFT_BLACK);
        else                                lndgVal(0, "T+Atm:", "---", TFT_DARK_GREY,  TFT_BLACK);
    } else if (!aboveAtmo) {
        if (tGround >= 0.0f) {
            fg = (tGround < LNDG_TGRND_ALARM_S) ? TFT_WHITE  :
                 (tGround < LNDG_TGRND_WARN_S)  ? TFT_YELLOW : TFT_DARK_GREEN;
            bg = (tGround < LNDG_TGRND_ALARM_S) ? TFT_RED    : TFT_BLACK;
            lndgVal(0, "T.Grnd:", formatTime(tGround), fg, bg);
        } else { lndgVal(0, "T.Grnd:", "---", TFT_DARK_GREY, TFT_BLACK); }
    } else { lndgVal(0, "T.Grnd:", "---", TFT_DARK_GREY, TFT_BLACK); }

    if (wantSL) {
        lndgVal(1, "Alt.SL:", formatAlt(state.altitude), TFT_DARK_GREEN, TFT_BLACK);
    } else {
        fg = (state.radarAlt < ALT_RDR_ALARM_M) ? TFT_WHITE  :
             (state.radarAlt < ALT_RDR_WARN_M)  ? TFT_YELLOW : TFT_DARK_GREEN;
        bg = (state.radarAlt < ALT_RDR_ALARM_M) ? TFT_RED    : TFT_BLACK;
        lndgVal(1, "Alt.Rdr:", formatAlt(state.radarAlt), fg, bg);
    }

    {
        if (tGround >= 0.0f) {
            fg = (state.verticalVel < LNDG_VVRT_ALARM_MS) ? TFT_WHITE  :
                 (state.verticalVel < LNDG_VVRT_WARN_MS)  ? TFT_YELLOW : TFT_DARK_GREEN;
            bg = (state.verticalVel < LNDG_VVRT_ALARM_MS) ? TFT_RED    : TFT_BLACK;
        } else { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
        lndgVal(2, "V.Vrt:", fmtMs(state.verticalVel), fg, bg);
    }

    {
        bool wantPeA = (state.radarAlt > 20000.0f || !state.inAtmo);
        if (wantPeA != _lndgReentryRow3PeA) {
            _lndgReentryRow3PeA = wantPeA; rowCache[6][3].value = "\x01";
            switchToScreen(screen_LNDG); return;
        }
        if (wantPeA) {
            if      (state.periapsis < 0.0f)     { fg = TFT_WHITE;     bg = TFT_DARK_GREEN; }
            else if (state.periapsis < atmoAlt)   { fg = TFT_YELLOW;    bg = TFT_BLACK; }
            else                                  { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
            lndgVal(3, "PeA:", formatAlt(state.periapsis), fg, bg);
        } else {
            if      (hSpd > LNDG_REENTRY_VHRZ_ALARM_MS) { fg = TFT_WHITE;     bg = TFT_RED; }
            else if (hSpd > LNDG_REENTRY_VHRZ_WARN_MS)  { fg = TFT_YELLOW;    bg = TFT_BLACK; }
            else                                         { fg = TFT_DARK_GREEN; bg = TFT_BLACK; }
            lndgVal(3, "V.Hrz:", fmtMs(hSpd), fg, bg);
        }
    }

    fg = (state.surfaceVel < 0) ? TFT_RED : TFT_DARK_GREEN;
    lndgVal(4, "V.Srf:", fmtMs(state.surfaceVel), fg, TFT_BLACK);

    float spd = state.surfaceVel;
    {
        uint16_t xL=AX, wL=AHW-ROW_PAD, xR=AX+AHW+ROW_PAD, wR=AHW-ROW_PAD;
        float m = state.machNumber;
        snprintf(buf, sizeof(buf), "%.2f", m);
        bool transonic = (m >= 0.85f && m <= 1.2f);
        { String ms=buf; uint16_t mfg = transonic ? TFT_YELLOW : TFT_DARK_GREEN;
          RowCache &mc=rowCache[6][9];
          if (mc.value!=ms||mc.fg!=mfg||mc.bg!=TFT_BLACK) {
              printValue(tft,F,xL,rowYFor(5,NR),wL,rowHFor(NR),"Mach:",ms,mfg,TFT_BLACK,COL_BACK,printState[6][9]);
              mc.value=ms;mc.fg=mfg;mc.bg=TFT_BLACK; } }
        float g=state.gForce; snprintf(buf,sizeof(buf),"%.2f",g);
        fg=(g>G_ALARM_POS||g<G_ALARM_NEG)?TFT_WHITE:(g>G_WARN_POS||g<G_WARN_NEG)?TFT_YELLOW:TFT_DARK_GREEN;
        bg=(g>G_ALARM_POS||g<G_ALARM_NEG)?TFT_RED:TFT_BLACK;
        { String gs=buf; RowCache &gc=rowCache[6][10];
          if (gc.value!=gs||gc.fg!=fg||gc.bg!=bg) {
              printValue(tft,F,xR,rowYFor(5,NR),wR,rowHFor(NR),"G:",gs,fg,bg,COL_BACK,printState[6][10]);
              gc.value=gs;gc.fg=fg;gc.bg=bg; } }
    }

    if (state.drogueDeploy&&!_drogueDeployed){_drogueDeployed=true;_drogueArmedSafe=(!state.inAtmo||spd<=LNDG_DROGUE_RISKY_MS);}
    if (state.drogueCut   &&!_drogueCut)     {_drogueCut=true;_drogueDeployed=false;_drogueArmedSafe=false;}
    if (state.mainDeploy  &&!_mainDeployed)  {_mainDeployed=true;_mainArmedSafe=(!state.inAtmo||spd<=LNDG_MAIN_RISKY_MS);}
    if (state.mainCut     &&!_mainCut)       {_mainCut=true;_mainDeployed=false;_mainArmedSafe=false;}

    auto chuteState=[&](bool dep,bool cut,bool safe,float safeSpd,float riskySpd,float fullAlt,
                        const char*&lbl,uint16_t&cfg,uint16_t&cbg){
        if(cut){lbl="CUT";cfg=TFT_RED;cbg=TFT_BLACK;return;}
        if(dep){
            if(!safe&&state.inAtmo&&spd>riskySpd){lbl="OPEN";cfg=TFT_WHITE;cbg=TFT_RED;return;}
            if(state.airDensity<LNDG_CHUTE_SEMI_DENSITY){lbl="ARMED";cfg=TFT_SKY;cbg=TFT_BLACK;return;}
            lbl="OPEN";cfg=(state.radarAlt>fullAlt)?TFT_YELLOW:TFT_DARK_GREEN;cbg=TFT_BLACK;return;
        }
        lbl="STOWED";
        if(!state.inAtmo){cfg=TFT_DARK_GREEN;cbg=TFT_BLACK;}
        else if(spd>riskySpd){cfg=TFT_WHITE;cbg=TFT_RED;}
        else if(spd>safeSpd) {cfg=TFT_YELLOW;cbg=TFT_BLACK;}
        else                 {cfg=TFT_DARK_GREEN;cbg=TFT_BLACK;}
    };

    {
        uint16_t xL=AX,wL=AHW-ROW_PAD,xR=AX+AHW+ROW_PAD,wR=AHW-ROW_PAD;
        uint16_t y6=rowYFor(6,NR),h6=rowHFor(NR);
        const char*dv;uint16_t dfg,dbg;
        chuteState(_drogueDeployed,_drogueCut,_drogueArmedSafe,LNDG_DROGUE_SAFE_MS,LNDG_DROGUE_RISKY_MS,LNDG_DROGUE_FULL_ALT,dv,dfg,dbg);
        {String ds=dv;RowCache&dc=rowCache[6][6];
         if(dc.value!=ds||dc.fg!=dfg||dc.bg!=dbg){printValue(tft,F,xL,y6,wL,h6,"Drogue:",ds,dfg,dbg,COL_BACK,printState[6][6]);dc.value=ds;dc.fg=dfg;dc.bg=dbg;}}
        const char*mv;uint16_t mfg,mbg;
        chuteState(_mainDeployed,_mainCut,_mainArmedSafe,LNDG_MAIN_SAFE_MS,LNDG_MAIN_RISKY_MS,LNDG_MAIN_FULL_ALT,mv,mfg,mbg);
        {String ms=mv;RowCache&mc=rowCache[6][11];
         if(mc.value!=ms||mc.fg!=mfg||mc.bg!=mbg){printValue(tft,F,xR,y6,wR,h6,"Main:",ms,mfg,mbg,COL_BACK,printState[6][11]);mc.value=ms;mc.fg=mfg;mc.bg=mbg;}}
    }
    {
        uint16_t y  = rowYFor(7,NR);
        uint16_t rh = SCREEN_H - y;
        uint16_t xL = AX - 2;          // extend left to close gap against panel divider
        uint16_t wL = AHW + 2;
        uint16_t xR = AX + AHW;
        uint16_t wR = CONTENT_W - xR;

        // Gear button (slot 13)
        {
            bool gearDown = state.gear_on;
            String gv = gearDown ? "DOWN" : "UP";
            RowCache &gc = rowCache[6][13];
            if (gc.value != gv) {
                ButtonLabel btn = gearDown
                    ? ButtonLabel{ "GEAR", TFT_WHITE,     TFT_WHITE,     TFT_DARK_GREEN, TFT_DARK_GREEN, TFT_GREY, TFT_GREY }
                    : ButtonLabel{ "GEAR", TFT_DARK_GREY, TFT_DARK_GREY, TFT_OFF_BLACK,  TFT_OFF_BLACK,  TFT_GREY, TFT_GREY };
                drawButton(tft, xL, y, wL, rh, btn, F, false);
                gc.value = gv;
            }
        }

        // SAS button (slot 14 — re-entry context: STAB/RETR=good, OFF=alarm above aerostable Mach)
        {
            const char *ss; uint16_t sfg, sbg;
            if (state.sasMode == 255) {
                ss = "SAS OFF";
                bool aeroUnstable = (state.machNumber > REENTRY_SAS_AERO_STABLE_MACH);
                sfg = aeroUnstable ? TFT_WHITE     : TFT_DARK_GREY;
                sbg = aeroUnstable ? TFT_RED        : TFT_OFF_BLACK;
            } else {
                switch (state.sasMode) {
                    case 0:  ss = "STABILITY";  sfg = TFT_WHITE; sbg = TFT_DARK_GREEN; break;
                    case 2:  ss = "RETROGRADE"; sfg = TFT_WHITE; sbg = TFT_DARK_GREEN; break;
                    case 1:  ss = "PROGRADE";   sfg = TFT_WHITE; sbg = TFT_RED;        break;
                    case 3:  ss = "NORMAL";     sfg = TFT_WHITE; sbg = TFT_RED;        break;
                    case 4:  ss = "ANTI-NRM";   sfg = TFT_WHITE; sbg = TFT_RED;        break;
                    case 5:  ss = "RADIAL+";    sfg = TFT_WHITE; sbg = TFT_RED;        break;
                    case 6:  ss = "RADIAL-";    sfg = TFT_WHITE; sbg = TFT_RED;        break;
                    case 7:  ss = "TARGET";     sfg = TFT_WHITE; sbg = TFT_RED;        break;
                    case 8:  ss = "ANTI-TGT";   sfg = TFT_WHITE; sbg = TFT_RED;        break;
                    case 9:  ss = "MANEUVER";   sfg = TFT_WHITE; sbg = TFT_RED;        break;
                    default: ss = "SAS OFF";    sfg = TFT_DARK_GREY; sbg = TFT_OFF_BLACK; break;
                }
            }
            RowCache &sc = rowCache[6][14];
            String ssv = ss;
            if (sc.value != ssv || sc.fg != sfg || sc.bg != sbg) {
                ButtonLabel btn = { ss, sfg, sfg, sbg, sbg, TFT_GREY, TFT_GREY };
                drawButton(tft, xR, y, wR, rh, btn, F, false);
                sc.value = ssv; sc.fg = sfg; sc.bg = sbg;
            }
        }
    }
}

