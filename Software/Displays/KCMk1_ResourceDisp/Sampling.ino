/***************************************************************************************
   Sampling.ino -- Per-slot rate sampling for Kerbal Controller Mk1 Resource Display

   Runs once per loop() pass over slots[], independent of which screen is showing, so
   the Main screen's trend arrows and TTE counters and the Detail screen's rate and
   time rows all read from one estimate. Two samples per slot: vessel total and
   active stage.

   Three windows per sample, each restarting when it completes:
     Trend (TREND_WINDOW_MS): a move of more than TREND_MIN_FRAC of capacity across
       the window sets the arrow direction, so it lags a real change by at most one
       window and does not chatter on the per-message fluctuations Simpit delivers.
     Rate (TTE_WINDOW_MS): units per REAL second across the window, smoothed 50/50
       with the previous estimate. The same deadband applies, so a drain too slow to
       move the trend arrow in a long window reads as no rate rather than a wild TTE.
     Long rate (TTE_LONG_WINDOW_MS): the same measurement over a much longer window,
       used only when the short one sees nothing. Food and water on a small crew
       move a fraction of a percent an hour, invisible to a ten-second window at 1x
       but plain over five minutes; without this their time to empty read "---" and
       their time tiers could only ever fire under warp.

   Time warp: the rate is measured against the panel's clock, but the pilot wants
   game time. warpFactor (game seconds per real second, from FLIGHT_STATUS) converts:
   rate per game second = rate / warpFactor, and so time-to-empty in game seconds =
   real-time TTE * warpFactor. The Simpit handler resets all sampling when the warp
   rate changes, since a window straddling the change would blend two rates.

   Refresh tracking: requestResourceRefresh() asks Simpit to resend every channel
   and stamps refreshRequestMs. A slot whose last message predates that stamp is
   "awaiting" until it answers or REFRESH_TIMEOUT_MS passes -- the Main screen
   draws those as "..." rather than "---", so "waiting for data" and "not aboard"
   read differently.
****************************************************************************************/
#include "KCMk1_ResourceDisp.h"

SlotSample sampleTot[MAX_SLOTS];
SlotSample sampleStg[MAX_SLOTS];


static void resetSample(SlotSample &s, uint32_t now) {
  s.trendRef   = -1.0f;   // < 0 = window not started
  s.trendRefMs = now;
  s.trend      = 0;
  s.tteRef     = -1.0f;
  s.tteRefMs   = now;
  s.rate       = 0.0f;
  s.rateValid  = false;
  s.longRef    = -1.0f;
  s.longRefMs  = now;
  s.longRate   = 0.0f;
  s.longValid  = false;
}

void resetAllSampling() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < MAX_SLOTS; i++) {
    resetSample(sampleTot[i], now);
    resetSample(sampleStg[i], now);
  }
}


static void sampleOne(SlotSample &c, bool hasData, float cur, float max, uint32_t now) {
  if (!hasData) {
    c.trend = 0;  c.trendRef = -1.0f;
    c.rateValid = false; c.rate = 0.0f; c.tteRef = -1.0f;
    c.longValid = false; c.longRate = 0.0f; c.longRef = -1.0f;
    return;
  }
  if (c.trendRef < 0.0f) { c.trendRef = cur; c.trendRefMs = now; }
  else if (now - c.trendRefMs >= TREND_WINDOW_MS) {
    float frac = (cur - c.trendRef) / max;
    c.trend      = (frac > TREND_MIN_FRAC) ? 1 : (frac < -TREND_MIN_FRAC) ? -1 : 0;
    c.trendRef   = cur;
    c.trendRefMs = now;
  }
  if (c.longRef < 0.0f) { c.longRef = cur; c.longRefMs = now; }
  else if (now - c.longRefMs >= TTE_LONG_WINDOW_MS) {
    float dt   = (now - c.longRefMs) / 1000.0f;
    float move = cur - c.longRef;
    c.longRate  = (fabsf(move) / max < TREND_MIN_FRAC) ? 0.0f : move / dt;
    c.longValid = true;
    c.longRef   = cur;
    c.longRefMs = now;
    // A drain the short window has been calling zero now has a rate.
    if (c.rateValid && c.rate == 0.0f && c.longRate != 0.0f) c.rate = c.longRate;
  }
  if (c.tteRef < 0.0f) { c.tteRef = cur; c.tteRefMs = now; }
  else if (now - c.tteRefMs >= TTE_WINDOW_MS) {
    float dt   = (now - c.tteRefMs) / 1000.0f;
    float move = cur - c.tteRef;
    float r    = (fabsf(move) / max < TREND_MIN_FRAC) ? 0.0f : move / dt;
    // Nothing in ten seconds: fall back to the long window's answer rather than
    // smoothing a real slow drain down to nothing.
    if (r == 0.0f && c.longValid) r = c.longRate;
    c.rate      = c.rateValid ? 0.5f * c.rate + 0.5f * r : r;
    c.rateValid = true;
    c.tteRef    = cur;
    c.tteRefMs  = now;
  }
}


/***************************************************************************************
   LEVEL HISTORY
   One ring of HIST_LEN sample slots shared by every resource type, so all traces
   share a timeline; a type not aboard at a sample holds HIST_NONE there. The warp
   in force at each sample is kept so the span can be reported in game time.
****************************************************************************************/
static uint16_t _hist[RES_COUNT][HIST_LEN];
static float    _histWarp[HIST_LEN];
static uint16_t _histHead   = 0;     // next slot to write
static uint16_t _histCount  = 0;
static uint32_t _histLastMs = 0;
static uint32_t _histSeq    = 0;
static float    _histGame   = 0.0f;  // game seconds spanned by the held samples

void resetHistory() {
  _histHead = _histCount = 0;
  _histGame = 0.0f;
  _histLastMs = millis();
  _histSeq++;
}

static void histPush(uint32_t now) {
  if (_histCount == HIST_LEN) _histGame -= _histWarp[_histHead] * (HIST_PERIOD_MS / 1000.0f);   // dropping the oldest
  for (uint16_t t = 0; t < (uint16_t)RES_COUNT; t++) _hist[t][_histHead] = HIST_NONE;
  for (uint8_t i = 0; i < slotCount; i++) {
    const ResourceSlot &s = slots[i];
    if (s.type == RES_NONE || s.maxVal <= 0.0f) continue;
    _hist[s.type][_histHead] = (uint16_t)(constrain(s.current / s.maxVal, 0.0f, 1.0f) * 1000.0f);
  }
  _histWarp[_histHead] = warpFactor;
  _histGame += warpFactor * (HIST_PERIOD_MS / 1000.0f);
  _histHead = (_histHead + 1) % HIST_LEN;
  if (_histCount < HIST_LEN) _histCount++;
  _histLastMs = now;
  _histSeq++;
}

uint16_t histCount()    { return _histCount; }

// Game time from sample k back to the newest: the warp in force at each later
// sample, times the period. Exact across warp changes, which is why the time scale
// under the trace is labelled from this rather than assumed linear.
float histGameAgo(uint16_t k) {
  float secs = 0.0f;
  for (uint16_t j = k + 1; j < _histCount; j++) {
    uint16_t idx = (uint16_t)((_histHead + HIST_LEN - _histCount + j) % HIST_LEN);
    secs += _histWarp[idx] * (HIST_PERIOD_MS / 1000.0f);
  }
  return secs;
}
float    histGameSecs() { return _histGame; }
uint32_t histSeq()      { return _histSeq; }

uint16_t histLevel(ResourceType t, uint16_t k) {
  if (k >= _histCount || (uint16_t)t >= (uint16_t)RES_COUNT) return HIST_NONE;
  uint16_t idx = (uint16_t)((_histHead + HIST_LEN - _histCount + k) % HIST_LEN);
  return _hist[t][idx];
}


void updateAllSampling() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < slotCount; i++) {
    const ResourceSlot &s = slots[i];
    sampleOne(sampleTot[i], s.maxVal   > 0.0f, s.current,      s.maxVal,   now);
    sampleOne(sampleStg[i], s.stageMax > 0.0f, s.stageCurrent, s.stageMax, now);
  }

  if (DETAIL_HISTORY && now - _histLastMs >= HIST_PERIOD_MS) histPush(now);

  // A refresh is over when every active slot has answered or the timeout passes.
  if (refreshPending) {
    bool allAnswered = true;
    for (uint8_t i = 0; i < slotCount; i++) {
      if (slots[i].type != RES_NONE && slots[i].updatedMs < refreshRequestMs) {
        allAnswered = false;
        break;
      }
    }
    if (allAnswered || now - refreshRequestMs > REFRESH_TIMEOUT_MS) {
      refreshPending = false;
      // A channel that never answered is not going to: the mod behind it is not
      // installed. Treat it as absent so its meter collapses like any other.
      for (uint8_t t = 1; t < (uint8_t)RES_COUNT; t++) {
        if (resPresence[t] == PRES_UNKNOWN) resPresence[t] = PRES_ABSENT;
      }
    }
  }
}


bool slotAwaiting(uint8_t i) {
  return refreshPending && i < slotCount && slots[i].updatedMs < refreshRequestMs;
}


/***************************************************************************************
   TIME TO EMPTY
   Game seconds until the resource is exhausted (or, for a waste-type resource, full)
   at the smoothed rate; -1 when there is no usable rate in the bad direction.
****************************************************************************************/
float sampleTteSeconds(const SlotSample &c, ResourceType t, float cur, float max) {
  if (!c.rateValid || c.rate == 0.0f) return -1.0f;
  ResLimits lim = resLimits(t);
  float realSecs;
  if (lim.highIsBad) {
    if (c.rate <= 0.0f) return -1.0f;
    realSecs = (max - cur) / c.rate;
  } else {
    if (c.rate >= 0.0f) return -1.0f;
    realSecs = cur / -c.rate;
  }
  return realSecs * warpFactor;
}


/***************************************************************************************
   FORMATTERS
****************************************************************************************/

// Seconds to a string that fits the compact counter cell: "4:35" under ten minutes,
// "42m" under an hour, "5.5h" under a hundred hours, "4d 3h" under ten days, "27d"
// under a hundred. secs < 0 gives "---".
void fmtTte(float secs, char *buf, size_t n) {
  if (secs < 0.0f) {
    strlcpy(buf, "---", n);
  } else if (secs < 600.0f) {
    uint16_t s = (uint16_t)secs;
    snprintf(buf, n, "%d:%02d", s / 60, s % 60);
  } else if (secs < 3600.0f) {
    snprintf(buf, n, "%dm", (int)(secs / 60.0f));
  } else if (secs < 360000.0f) {
    dtostrf(secs / 3600.0f, 1, 1, buf);
    strlcat(buf, "h", n);
  } else if (secs < 864000.0f) {
    uint32_t h = (uint32_t)(secs / 3600.0f);
    snprintf(buf, n, "%dd %dh", (int)(h / 24), (int)(h % 24));
  } else if (secs < 8640000.0f) {
    snprintf(buf, n, "%dd", (int)(secs / 86400.0f));
  } else {
    strlcpy(buf, ">99d", n);
  }
}

// Rate in units per GAME second, signed, "---" when no estimate yet.
// Signed rate per game second, or per minute or hour when the per-second figure
// would round to nothing, so a slow life-support drain reads "-0.06/m" rather than
// "0.00/s".
void fmtRate(const SlotSample &c, char *buf, size_t n) {
  if (!c.rateValid) { strlcpy(buf, "---", n); return; }
  float r   = c.rate / warpFactor;
  float mag = fabsf(r);
  const char *unit = "/s";
  if (mag > 0.0f && mag < 0.01f) { mag *= 60.0f; unit = "/m"; }
  if (mag > 0.0f && mag < 0.01f) { mag *= 60.0f; unit = "/h"; }
  char num[16];
  if      (mag >= 1000.0f) snprintf(num, sizeof(num), "%d", (int)(mag + 0.5f));
  else if (mag >= 10.0f)   dtostrf(mag, 1, 1, num);
  else                     dtostrf(mag, 1, 2, num);
  snprintf(buf, n, "%s%s%s", (r < 0.0f) ? "-" : (r > 0.0f) ? "+" : "", num, unit);
}


/***************************************************************************************
   TIME-REMAINING TIERS
   A resource with time tiers (EC, O2, water, food) alerts on the game time it has
   left, on top of its level state: caution below the warn time, alarm below the
   alarm time, each left only once the time exceeds the threshold by TIME_HYST_FRAC.
   timeFlag reports whether the returned state is the time tier's doing, so the
   strip can say TIME rather than LOW.
****************************************************************************************/
uint8_t applyTimeTiers(ResourceType t, float tteS, uint8_t state,
                       uint8_t prevState, bool prevTime, bool &timeFlag) {
  timeFlag = false;
  ResTimeLimits tl = resTimeLimits(t);
  if (tl.warn <= 0.0f || tteS < 0.0f) return state;
  float alarmT = tl.alarm * ((prevTime && prevState == ALERT_ALARM)   ? 1.0f + TIME_HYST_FRAC : 1.0f);
  float warnT  = tl.warn  * ((prevTime && prevState == ALERT_CAUTION) ? 1.0f + TIME_HYST_FRAC : 1.0f);
  if (tteS < alarmT) {
    timeFlag = (state != ALERT_ALARM);
    return ALERT_ALARM;
  }
  if (tteS < warnT && state != ALERT_ALARM && state != ALERT_CAUTION) {
    timeFlag = true;
    return ALERT_CAUTION;
  }
  return state;
}


/***************************************************************************************
   ALERT SUMMARY
   The vessel's alert picture for the master's status packet, evaluated from the
   slots and the sampling directly so it is the same whatever screen is up. Keeps its
   own last-state per slot for the hysteresis. Worst = the first slot in alarm, else
   the first in caution or across its bug.
****************************************************************************************/
static uint8_t _sumState[MAX_SLOTS];
static bool    _sumTime[MAX_SLOTS];

void alertSummary(bool &caution, bool &alarm, bool &timeTier, ResourceType &worst) {
  caution = alarm = timeTier = false;
  worst = RES_NONE;
  ResourceType worstCaution = RES_NONE;
  for (uint8_t i = 0; i < slotCount; i++) {
    const ResourceSlot &s = slots[i];
    if (s.type == RES_NONE || s.maxVal <= 0.0f) { _sumState[i] = ALERT_NOMINAL; _sumTime[i] = false; continue; }
    float   level = constrain(s.current / s.maxVal, 0.0f, 1.0f);
    uint8_t state = alertState(s.type, level, s.bug, _sumState[i]);
    float   tteS  = sampleTteSeconds(sampleTot[i], s.type, s.current, s.maxVal);
    bool    tf    = false;
    state = applyTimeTiers(s.type, tteS, state, _sumState[i], _sumTime[i], tf);
    _sumState[i] = state;
    _sumTime[i]  = tf;
    if (tf) timeTier = true;
    if (state == ALERT_ALARM) { alarm = true; if (worst == RES_NONE) worst = s.type; }
    else if (state == ALERT_CAUTION || state == ALERT_BUG) { caution = true; if (worstCaution == RES_NONE) worstCaution = s.type; }
  }
  if (worst == RES_NONE) worst = worstCaution;
}
