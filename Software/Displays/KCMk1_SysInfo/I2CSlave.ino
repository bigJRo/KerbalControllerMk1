/***************************************************************************************
   I2CSlave.ino -- Wire1 slave at 0x14 (I2C spec §15.4)

   SDA / SCL are GPIO 6 / 7, the RP2350's I2C1, so this is Wire1 in arduino-pico.

   Receive (interrupt context): every master write is one frame; it is copied into the
   frame queue (Frames.ino) and handled by loop().

   Request -> 4 bytes [0xAF, flags, fwMajor, fwMinor]:
     flags bit 0 ready (setup() done), bit 1 card mounted, bit 2 boot screen showing.
   The master probes with this read and ignores the display until byte 0 is 0xAF.
****************************************************************************************/
static volatile uint8_t g_readFlags;

static void onI2CReceive(int n) {
  uint8_t b[SI_FRAME_MAX];
  uint8_t k = 0;
  while (Wire1.available()) {
    int v = Wire1.read();
    if (k < SI_FRAME_MAX) b[k++] = (uint8_t)v;
  }
  (void)n;
  if (k) framesPush(b, k);
}

static void onI2CRequest() {
  uint8_t r[4] = { SI_SYNC, g_readFlags, SYSINFO_VERSION_MAJOR, SYSINFO_VERSION_MINOR };
  Wire1.write(r, 4);
}

// The flags the next read reports; refreshed by renderFrame() via i2cUpdateFlags().
void i2cUpdateFlags(bool bootShowing) {
  g_readFlags = (uint8_t)(0x01 | (splashCardMounted() ? 0x02 : 0) | (bootShowing ? 0x04 : 0));
}

void i2cBegin() {
  i2cUpdateFlags(true);
  Wire1.setSDA(PIN_SDA);
  Wire1.setSCL(PIN_SCL);
  Wire1.onReceive(onI2CReceive);
  Wire1.onRequest(onI2CRequest);
  Wire1.begin(KCM_I2C_ADDR_SYSINFODISP);
}
