/***************************************************************************************
   ScreenMainStrip.ino -- Alert strip and propellant balance for the Main screen

   The ALERT_H px strip across the top of the content area:
     - an EICAS-style message line, worst first: alarms white-on-red as "<LBL> LOW"
       (or HIGH for a waste product), cautions in yellow as "<LBL> CAUT", a reserve
       bug crossing in the bug's cyan as "<LBL> BUG", a state raised by a
       time-remaining tier as "<LBL> TIME", and "REFRESHING" in white while a channel
       refresh is pending. Messages that do not fit collapse to "+N". A tile that has
       just turned alarm FLASHES for ALARM_FLASH_MS -- the caution-and-warning
       convention for a new alarm -- then holds steady. A tap on a message opens
       Detail on that resource (stripHitTest).
     - at its right end, the PROPELLANT BALANCE indicator, Apollo's OXID UNBAL meter
       for KSP's 9:11 LF:LOx burn ratio: when both are on the panel, a centre-zero
       bar deflects toward the propellant in surplus and a counter says by how many
       units. Stage quantities when both have them, else vessel totals.

   Interface (ScreenMain.ino drives it):
     stripReset()     -- drawn state forgotten; call on a chrome redraw
     stripResetAll()  -- also the per-slot codes and alarm timers; call when the slot
                         sequence changed, since both are indexed by slot
     stripSetSlot()   -- the update pass reports each slot's code every frame
     stripUpdate()    -- redraws whatever changed; call at the end of the update pass

   Uses SCREEN_W, CONTENT_X and ALERT_H from ScreenMain.ino, which precedes this tab
   in the build. Redraws only when the composed line changes (a signature buffer
   detects it), plus the flash phase while a new alarm is flashing.
****************************************************************************************/
#include "KCMk1_ResourceDisp.h"


/***************************************************************************************
   LAYOUT
****************************************************************************************/
// Messages from the content's left edge; the balance indicator, when shown, takes
// the rightmost BAL_W px.
static const tFont   *STRIP_FONT   = &Roboto_Black_16;
static const uint16_t STRIP_X      = CONTENT_X + 4;
static const uint16_t BAL_W        = 270;  // balance indicator cell at the strip's right end
static const uint16_t BAL_BAR_W    = 100;  // centre-zero bar
static const uint16_t BAL_BAR_H    = 10;
static const float    LF_PER_LOX   = 9.0f / 11.0f;   // KSP LF:LOx burn ratio


/***************************************************************************************
   ALERT STRIP
   One line, worst first: alarms white-on-red as "<LBL> LOW" (or HIGH for a waste
   product), cautions in yellow as "<LBL> CAUT", a reserve-bug crossing in the bug's
   cyan as "<LBL> BUG", and "REFRESHING" in white while a channel refresh is pending.
   Messages that do not fit collapse into "+N" in grey. Redrawn only when the
   composed line changes, which the signature buffer detects.
****************************************************************************************/
static uint8_t  _stripCode[MAX_SLOTS];
static bool     _stripTime[MAX_SLOTS];    // the state came from a time-remaining tier
static uint32_t _alarmSince[MAX_SLOTS];   // millis() the slot's tile turned alarm; 0 = not in alarm
static char     _stripSig[200];           // last drawn line, "\x01" = force
static bool     _balShown = false;

// Where each drawn message sits, so a tap on it can open Detail on that resource.
struct StripHit { uint16_t x0, x1; uint8_t slot; };
static StripHit _stripHits[MAX_SLOTS];
static uint8_t  _stripHitCount = 0;

static int16_t _balPx = -1;        // last drawn pointer offset, px from centre; -32768 = force
static char    _balText[24];       // last drawn counter

void stripReset() {
  _stripSig[0] = 1; _stripSig[1] = '\0';
  _stripHitCount = 0;
  _balShown = false;
  _balPx = -32768;
  _balText[0] = 1; _balText[1] = '\0';
}

void stripResetAll() {
  stripReset();
  for (uint8_t i = 0; i < MAX_SLOTS; i++) {
    _stripCode[i]  = STRIP_NONE;
    _stripTime[i]  = false;
    _alarmSince[i] = 0;
  }
}

// The update pass reports every visible slot each frame. A tile entering alarm
// starts its flash clock; leaving alarm stops it.
void stripSetSlot(uint8_t i, uint8_t code, bool timeFlag) {
  if (i >= MAX_SLOTS) return;
  if (code == STRIP_ALARM) {
    if (_stripCode[i] != STRIP_ALARM) _alarmSince[i] = millis() | 1;   // never 0
  } else {
    _alarmSince[i] = 0;
  }
  _stripCode[i] = code;
  _stripTime[i] = timeFlag;
}

// Flash phase for a slot's alarm tile: 0 = steady or tile on, 1 = tile off. Only
// nonzero during the first ALARM_FLASH_MS after the tile turned alarm.
static uint8_t alarmFlashPhase(uint8_t i, uint32_t now) {
  if (_alarmSince[i] == 0) return 0;
  uint32_t age = now - _alarmSince[i];
  if (age >= ALARM_FLASH_MS) return 0;
  return (uint8_t)((age / ALARM_FLASH_HALF_MS) & 1);
}

static uint16_t stripAvailW() {
  return (uint16_t)(SCREEN_W - STRIP_X - (_balShown ? BAL_W : 0));
}

// Append one message to the line; returns false when it would not fit. A message
// with a non-black background gets a filled tile behind it, STRIP_PAD wider than the
// text each side -- the alarm treatment is white on red, as on the counter cells.
static const uint16_t STRIP_PAD = 4;

static bool stripPut(KCM_TFT &tft, uint16_t &x, uint16_t xEnd, const char *text,
                     uint16_t fore, uint16_t back) {
  int16_t w = getFontStringWidth(STRIP_FONT, text);
  if (x + w + STRIP_PAD > xEnd) return false;
  if (back != TFT_BLACK) tft.fillRect(x - STRIP_PAD, 1, w + STRIP_PAD * 2, ALERT_H - 2, back);
  tft.setFont(*STRIP_FONT);
  tft.setTextColor(fore, back);
  tft.setCursor(x, (ALERT_H - STRIP_FONT->cap_height) / 2);
  tft.print(text);
  x += w + STRIP_PAD + getFontStringWidth(STRIP_FONT, "  ");
  return true;
}

static void updateAlertStrip(KCM_TFT &tft) {
  uint32_t now = millis();
  // Compose the signature: one token per message in display order, with the flash
  // phase of a new alarm so the redraw follows it.
  char sig[sizeof(_stripSig)];
  size_t n = 0;
  if (refreshPending) n += snprintf(sig + n, sizeof(sig) - n, "R|");
  for (uint8_t sev = STRIP_ALARM; sev >= STRIP_CAUTION; sev--) {
    for (uint8_t i = 0; i < slotCount; i++) {
      uint8_t code = _stripCode[i];
      bool pick = (sev == STRIP_ALARM) ? (code == STRIP_ALARM)
                                       : (code == STRIP_CAUTION || code == STRIP_BUG);
      if (!pick || n >= sizeof(sig) - 10) continue;
      n += snprintf(sig + n, sizeof(sig) - n, "%d%d%c%d|", code, i, _stripTime[i] ? 't' : '-',
                    alarmFlashPhase(i, now));
    }
  }
  if (strcmp(sig, _stripSig) == 0) return;
  strlcpy(_stripSig, sig, sizeof(_stripSig));

  uint16_t xEnd = STRIP_X + stripAvailW();
  tft.fillRect(CONTENT_X, 0, xEnd - CONTENT_X, ALERT_H, TFT_BLACK);
  uint16_t x = STRIP_X + STRIP_PAD;
  uint8_t  dropped = 0;
  _stripHitCount = 0;
  if (refreshPending) stripPut(tft, x, xEnd, "REFRESHING", TFT_WHITE, TFT_BLACK);
  for (uint8_t sev = STRIP_ALARM; sev >= STRIP_CAUTION; sev--) {
    for (uint8_t i = 0; i < slotCount; i++) {
      uint8_t code = _stripCode[i];
      bool pick = (sev == STRIP_ALARM) ? (code == STRIP_ALARM)
                                       : (code == STRIP_CAUTION || code == STRIP_BUG);
      if (!pick) continue;
      char msg[16];
      // Red names the condition (LOW, or HIGH for a waste product); yellow names the
      // tier (CAUT), matching the panel's caution/alarm vocabulary; a bug crossing
      // says so; a state raised by a time-remaining tier says TIME in either colour.
      const char *what = _stripTime[i]           ? "TIME"
                       : (code == STRIP_BUG)     ? "BUG"
                       : (code == STRIP_CAUTION) ? "CAUT"
                       : (resLimits(slots[i].type).highIsBad ? "HIGH" : "LOW");
      snprintf(msg, sizeof(msg), "%s %s", resLabel(slots[i].type), what);
      bool alarm = (code == STRIP_ALARM);
      bool off   = alarm && alarmFlashPhase(i, now) == 1;   // flash: tile off, red text
      uint16_t fore = alarm ? (off ? TFT_RED : TFT_WHITE) : (code == STRIP_BUG) ? TFT_CYAN : TFT_YELLOW;
      uint16_t back = (alarm && !off) ? TFT_RED : TFT_BLACK;
      uint16_t xStart = x;
      if (dropped || !stripPut(tft, x, xEnd, msg, fore, back)) {
        dropped++;
      } else if (_stripHitCount < MAX_SLOTS) {
        _stripHits[_stripHitCount++] = { (uint16_t)(xStart - STRIP_PAD), x, i };
      }
    }
  }
  if (dropped) {
    char more[8];
    snprintf(more, sizeof(more), "+%d", dropped);
    int16_t w = getFontStringWidth(STRIP_FONT, more);
    tft.setFont(*STRIP_FONT);
    tft.setTextColor(TFT_GREY, TFT_BLACK);
    tft.setCursor(xEnd - w, (ALERT_H - STRIP_FONT->cap_height) / 2);
    tft.print(more);
  }
}


int8_t stripHitTest(uint16_t x, uint16_t y) {
  if (y >= ALERT_H) return -1;
  for (uint8_t k = 0; k < _stripHitCount; k++) {
    if (x >= _stripHits[k].x0 && x < _stripHits[k].x1) return (int8_t)_stripHits[k].slot;
  }
  return -1;
}


/***************************************************************************************
   PROPELLANT BALANCE
   Surplus of one propellant over what the other can burn with it, at 9:11. Positive
   = LOx in surplus (pointer right), negative = LF in surplus (pointer left). The bar
   is scaled so a surplus equal to half the propellant aboard is full deflection.
****************************************************************************************/
static int8_t findSlot(ResourceType t) {
  for (uint8_t i = 0; i < slotCount; i++) if (slots[i].type == t) return (int8_t)i;
  return -1;
}

// Returns true when the indicator applies, filling surplus (units, signed) and the
// pointer fraction (-1..1).
static bool balanceCompute(float &surplus, float &frac) {
  int8_t lf = findSlot(RES_LIQUID_FUEL), lox = findSlot(RES_LIQUID_OX);
  if (lf < 0 || lox < 0) return false;
  const ResourceSlot &a = slots[lf], &b = slots[lox];
  if (a.maxVal <= 0.0f || b.maxVal <= 0.0f) return false;
  bool useStage = (a.stageMax > 0.0f && b.stageMax > 0.0f);
  float fuel = useStage ? a.stageCurrent : a.current;
  float ox   = useStage ? b.stageCurrent : b.current;
  float total = fuel + ox;
  if (total <= 0.0f) return false;
  float loxNeeded = fuel / LF_PER_LOX;          // LOx the LF on hand can burn
  if (ox >= loxNeeded) surplus = ox - loxNeeded;          // LOx left over
  else                 surplus = -(fuel - ox * LF_PER_LOX); // LF left over, negative
  frac = constrain(surplus / (0.5f * total), -1.0f, 1.0f);
  return true;
}

static void drawBalanceChrome(KCM_TFT &tft) {
  uint16_t x0 = SCREEN_W - BAL_W;
  tft.fillRect(x0, 0, BAL_W, ALERT_H, TFT_BLACK);
  tft.setFont(Roboto_Black_12);
  tft.setTextColor(TFT_GREY, TFT_BLACK);
  tft.setCursor(x0 + 4, (ALERT_H - 14) / 2);
  tft.print("BAL");
  uint16_t barX = x0 + 60, barY = (ALERT_H - BAL_BAR_H) / 2;
  tft.setCursor(barX - 18, (ALERT_H - 14) / 2);
  tft.print("LF");
  tft.setCursor(barX + BAL_BAR_W + 4, (ALERT_H - 14) / 2);
  tft.print("LOx");
  tft.drawRect(barX, barY, BAL_BAR_W, BAL_BAR_H, TFT_GREY);
  tft.drawLine(barX + BAL_BAR_W / 2, barY - 2, barX + BAL_BAR_W / 2, barY + BAL_BAR_H + 1, TFT_LIGHT_GREY);
}

static void updateBalance(KCM_TFT &tft) {
  float surplus, frac;
  bool show = balanceCompute(surplus, frac);
  if (show != _balShown) {
    _balShown = show;
    _stripSig[0] = 1; _stripSig[1] = '\0';   // message width changed: relay the strip
    if (show) drawBalanceChrome(tft);
    else      tft.fillRect(SCREEN_W - BAL_W, 0, BAL_W, ALERT_H, TFT_BLACK);
    _balPx = -32768;
    _balText[0] = 1; _balText[1] = '\0';
  }
  if (!show) return;

  uint16_t x0 = SCREEN_W - BAL_W;
  uint16_t barX = x0 + 60, barY = (ALERT_H - BAL_BAR_H) / 2;
  int16_t px = (int16_t)(frac * (BAL_BAR_W / 2 - 3));
  if (px != _balPx) {
    if (_balPx != -32768) {
      tft.fillRect(barX + BAL_BAR_W / 2 + _balPx - 1, barY + 1, 3, BAL_BAR_H - 2, TFT_BLACK);
      tft.drawLine(barX + BAL_BAR_W / 2, barY + 1, barX + BAL_BAR_W / 2, barY + BAL_BAR_H - 2, TFT_LIGHT_GREY);
    }
    _balPx = px;
    tft.fillRect(barX + BAL_BAR_W / 2 + px - 1, barY + 1, 3, BAL_BAR_H - 2, TFT_WHITE);
  }

  char text[24], num[12];
  fmtUnits(fabsf(surplus), num, sizeof(num));
  snprintf(text, sizeof(text), "%s +%s", (surplus >= 0.0f) ? "LOx" : "LF", num);
  if (strcmp(text, _balText) != 0) {
    strlcpy(_balText, text, sizeof(_balText));
    uint16_t tx = barX + BAL_BAR_W + 34;
    tft.fillRect(tx, 0, SCREEN_W - tx, ALERT_H, TFT_BLACK);
    tft.setFont(Roboto_Black_12);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(tx, (ALERT_H - 14) / 2);
    tft.print(text);
  }
}




/***************************************************************************************
   UPDATE -- once per Main-screen update pass, after every slot has reported
****************************************************************************************/
void stripUpdate(KCM_TFT &tft) {
  updateBalance(tft);
  updateAlertStrip(tft);
}
