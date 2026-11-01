/***************************************************************************************
   KCMk1_SysInfo.ino -- Kerbal Controller Mk1 Sys Info Display (I2C 0x14)

   The small portrait screen on Panel A2. No touch, no buttons: it shows what the master
   sends it (I2C spec §15.4), on three screens:
     BOOT        power-on until the master's BOOT_DONE: newest boot line, the OK /
                 warning / error counts, the newest problem
     STATUS      12 V supply, board temperature and fan, KSP links up, modules present,
                 and a rotating two-line message area
     SWITCH ERR  while any switch disagrees with the vessel: each switch and the vessel's
                 state, five a page
   plus MASTER SILENT on either screen when no frame has arrived for 5 s.

   Design: Documents/Developer/Sys_Info_Display_Design.md (Draft 3). Every string is
   Roboto Black 32 or 48, at least 2.8 mm tall on the ER-TFTM020-2 (D4).

   Board:   Seeed XIAO RP2350 on the 2" TFT Display Driver KC-01-1901 / 1902 V2.0
   Core:    arduino-pico (Earle Philhower), board "Seeed XIAO RP2350"
   Library: Adafruit ST7735 and ST7789 (Adafruit_ST7789) + Adafruit GFX;
            KCMk1_SystemConfig (header only, from Software/Displays/libraries)

   Tabs:
     AAA_Config.ino        pins, timings, thresholds, names, icons
     Backlight.ino         flight / standby level, ramp, PWM
     Frames.ino            the frame queue and the model
     Gfx.ino               frame buffer, text, icons, anti-sticking shift, push
     I2CSlave.ino          Wire1 slave at 0x14
     Messages.ino          the status screen's message list
     Screen_Boot.ino, Screen_Status.ino, Screen_SwitchErr.ino, SdSplash.ino

   Host checks: Software/Displays/tools/sysinfo_host.py (build, behaviour, renders).
****************************************************************************************/
#include "KCMk1_SysInfo.h"

static uint32_t g_lastRenderMs;

void setup() {
  Serial.begin(115200);
  pinsBegin();
  backlightBegin();
  gfxBegin();
  splashBegin();          // reads the card once, before anything else uses SPI
  framesBegin();
  i2cBegin();
  Serial.printf("KCMk1 Sys Info Display v%s\n", SYSINFO_VERSION);
  wdtBegin();
}

void loop() {
  uint32_t now = millis();
  framesDrain(now);
  if (now - g_lastRenderMs >= RENDER_MS) {
    g_lastRenderMs = now;
    renderFrame(now);
    gfxPush();
  }
  backlightService(now);
  wdtFeed();
}

// Which screen, then draw it into the frame buffer.
void renderFrame(uint32_t now) {
  gfxBeginFrame(now);
  i2cUpdateFlags(!g_m.bootDone);
  if (splashShowing(now))                     splashDraw();
  else if (!g_m.bootDone)                     drawBoot(now);
  else if (g_m.swN > 0 && !masterSilent(now)) drawSwitchErr(now);
  else                                        drawStatus(now);
}
