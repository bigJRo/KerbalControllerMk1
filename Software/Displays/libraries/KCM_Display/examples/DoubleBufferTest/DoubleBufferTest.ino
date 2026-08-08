/***************************************************************************************
   DoubleBufferTest.ino -- RA8876 hardware page-flip bring-up / benchmark

   Validates the KCMDoubleBuffer layer in KCM_Display.h on real hardware and prints
   timing so we can see how fast the panel can actually render.

   What it checks (watch the panel + the Serial monitor @ 115200):
     PHASE 1 - flip sanity / canvas redirect:
       * screen goes RED (drawn to the visible page)
       * GREEN is drawn to the HIDDEN page -> screen must STAY RED (proves draws go
         to the back page, not the visible one)
       * flip -> screen turns GREEN (proves the flip presents the back page)
     PHASE 2 - benchmark (Serial): hardware fillScreen, per-pixel writeRect
       throughput (the real bus number), and the flip cost.
     PHASE 3 - continuous sweep: a bar sweeps L->R, full-redraw-per-frame to the
       back page then flip. Should be perfectly smooth with NO tearing. Reports fps.

   Requires: KCM_Display + wwatson RA8876 libraries + KCMk1_SystemConfig. No SD/fonts.
****************************************************************************************/
#include <KCM_Display.h>

KCM_TFT          tft(KCM_TFT_RS, KCM_TFT_CS, KCM_TFT_RESET);
KCMDoubleBuffer  db;

// RGB565 colors
static const uint16_t C_BLACK = 0x0000, C_RED = 0xF800, C_GREEN = 0x07E0;
static const uint16_t C_CYAN  = 0x07FF, C_GREY = 0x39C7, C_WHITE = 0xFFFF;

// Per-pixel blit benchmark source (200x200 gradient = 80KB in RAM2).
DMAMEM static uint16_t patch[200 * 200];

static void fillPatch() {
  for (int y = 0; y < 200; y++)
    for (int x = 0; x < 200; x++)
      patch[y * 200 + x] = (uint16_t)(((x & 0x1F) << 11) | ((y & 0x3F) << 5) | ((x ^ y) & 0x1F));
}

static void benchmark() {
  db.beginFrame(tft);                 // draw to the hidden page for the benchmark

  uint32_t t0 = micros();
  tft.fillScreen(C_BLACK);            // RA8876 hardware fill of the full 1024x600
  uint32_t tFill = micros() - t0;

  const int N = 12;                   // 12 x 200x200 blits = 480,000 px pushed
  t0 = micros();
  for (int i = 0; i < N; i++)
    tft.writeRect((i % 5) * 205, (i / 5) * 200, 200, 200, patch);
  uint32_t tBlit = micros() - t0;
  uint32_t px = (uint32_t)200 * 200 * N;

  t0 = micros();
  db.flip(tft);
  uint32_t tFlip = micros() - t0;

  Serial.println(F("\n--- BENCHMARK ---"));
  Serial.printf("fillScreen 1024x600 (hw fill): %lu us  (%.1f fps if repeated)\n",
                tFill, 1000000.0f / (float)tFill);
  Serial.printf("writeRect push %lu px:          %lu us  => %.2f Mpx/s\n",
                px, tBlit, px / (float)tBlit);
  float mpxs = px / (float)tBlit;                    // px per microsecond = Mpx/s
  Serial.printf("  -> full-screen (614400 px) push ~ %.1f ms => %.1f fps\n",
                614400.0f / mpxs / 1000.0f, mpxs * 1e6f / 614400.0f);
  Serial.printf("flip():                        %lu us\n", tFlip);
  Serial.println(F("-----------------\n"));
}

void setup() {
  Serial.begin(115200);
  uint32_t t = millis();
  while (!Serial && (millis() - t < 2500)) {}
  Serial.println(F("RA8876 double-buffer bring-up"));

  fillPatch();
  kcmDisplayBegin(tft, C_BLACK);      // single-buffer bring-up (page 0)
  db.begin(tft);                      // set up page flip; display + canvas on page 0

  // PHASE 1 -- flip sanity / canvas redirect ------------------------------------
  tft.fillScreen(C_RED);              // canvas is page 0 (visible) -> RED shows
  Serial.println(F("PHASE 1: screen should be RED"));
  delay(1500);

  db.beginFrame(tft);                 // canvas -> hidden back page
  tft.fillScreen(C_GREEN);            // GREEN goes to the HIDDEN page
  Serial.println(F("  drew GREEN to hidden page -- screen should STILL be RED"));
  delay(1500);

  db.flip(tft);                       // present the back page
  Serial.println(F("  flipped -- screen should now be GREEN"));
  delay(1500);

  // PHASE 2 -- benchmark --------------------------------------------------------
  benchmark();
  delay(500);

  Serial.println(F("PHASE 3: smooth sweep (should be tear-free). fps below:"));
}

void loop() {
  static int      x = -40;
  static uint32_t frames = 0, tWin = millis();

  db.beginFrame(tft);                 // draw target -> hidden back page
  tft.fillScreen(C_BLACK);            // fully define the frame (Model A)
  tft.fillRect(0, 250, KCM_SCREEN_W, 100, C_GREY);   // static band: tearing shows here
  tft.fillRect(x, 0, 40, KCM_SCREEN_H, C_CYAN);       // sweeping bar
  db.flip(tft);                       // present; back/front swap

  x += 12;
  if (x > (int)KCM_SCREEN_W) x = -40;

  if (++frames >= 120) {
    uint32_t dt = millis() - tWin;
    Serial.printf("sweep: %.1f fps (%lu frames / %lu ms)\n",
                  frames * 1000.0f / (float)dt, frames, dt);
    frames = 0; tWin = millis();
  }
}
