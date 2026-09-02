/***************************************************************************************
   Sampling.ino -- Per-slot rate sampling for Kerbal Controller Mk1 Resource Display

   Runs once per loop() pass over slots[], independent of which screen is showing, so
   the Main screen's trend arrows and TTE counters and the Detail screen's rate and
   time rows all read from one estimate. Two samples per slot: vessel total and
   active stage.

   Two windows per sample, both restarting on every sample:
     Trend (TREND_WINDOW_MS): a move of more than TREND_MIN_FRAC of capacity across
       the window sets the arrow direction, so it lags a real change by at most one
       window and does not chatter on the per-message fluctuations Simpit delivers.
     Rate (TTE_WINDOW_MS): units per REAL second across the window, smoothed 50/50
       with the previous estimate. The same deadband applies, so a drain too slow to
       move the trend arrow in a long window reads as no rate rather than a wild TTE.

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
    return;
  }
  if (c.trendRef < 0.0f) { c.trendRef = cur; c.trendRefMs = now; }
  else if (now - c.trendRefMs >= TREND_WINDOW_MS) {
    float frac = (cur - c.trendRef) / max;
    c.trend      = (frac > TREND_MIN_FRAC) ? 1 : (frac < -TREND_MIN_FRAC) ? -1 : 0;
    c.trendRef   = cur;
    c.trendRefMs = now;
  }
  if (c.tteRef < 0.0f) { c.tteRef = cur; c.tteRefMs = now; }
  else if (now - c.tteRefMs >= TTE_WINDOW_MS) {
    float dt   = (now - c.tteRefMs) / 1000.0f;
    float move = cur - c.tteRef;
    float r    = (fabsf(move) / max < TREND_MIN_FRAC) ? 0.0f : move / dt;
    c.rate      = c.rateValid ? 0.5f * c.rate + 0.5f * r : r;
    c.rateValid = true;
    c.tteRef    = cur;
    c.tteRefMs  = now;
  }
}


void updateAllSampling() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < slotCount; i++) {
    const ResourceSlot &s = slots[i];
    sampleOne(sampleTot[i], s.maxVal   > 0.0f, s.current,      s.maxVal,   now);
    sampleOne(sampleStg[i], s.stageMax > 0.0f, s.stageCurrent, s.stageMax, now);
  }

  // A refresh is over when every active slot has answered or the timeout passes.
  if (refreshPending) {
    bool allAnswered = true;
    for (uint8_t i = 0; i < slotCount; i++) {
      if (slots[i].type != RES_NONE && slots[i].updatedMs < refreshRequestMs) {
        allAnswered = false;
        break;
      }
    }
    if (allAnswered || now - refreshRequestMs > REFRESH_TIMEOUT_MS) refreshPending = false;
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
// "42m" under an hour, "5.5h" under a hundred hours. secs < 0 gives "---".
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
  } else {
    strlcpy(buf, ">99h", n);
  }
}

// Rate in units per GAME second, signed, "---" when no estimate yet.
void fmtRate(const SlotSample &c, char *buf, size_t n) {
  if (!c.rateValid) { strlcpy(buf, "---", n); return; }
  float r   = c.rate / warpFactor;
  float mag = fabsf(r);
  char num[16];
  if      (mag >= 1000.0f) snprintf(num, sizeof(num), "%d", (int)(mag + 0.5f));
  else if (mag >= 10.0f)   dtostrf(mag, 1, 1, num);
  else                     dtostrf(mag, 1, 2, num);
  snprintf(buf, n, "%s%s/s", (r < 0.0f) ? "-" : (r > 0.0f) ? "+" : "", num);
}
