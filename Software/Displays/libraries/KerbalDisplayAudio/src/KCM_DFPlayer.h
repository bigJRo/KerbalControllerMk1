#ifndef KCM_DFPLAYER_H
#define KCM_DFPLAYER_H

/***************************************************************************************
   KCM_DFPlayer — minimal, dependency-free DFPlayer Mini driver for the
   Kerbal Controller Mk1 7" TFT Display Driver board (KC-01-1911).

   The DFPlayer Mini (U8) plays numbered .mp3/.wav clips from its own microSD
   card for sampled audio (voice callouts, ambience, alert stingers) that the
   tone() buzzer in KerbalDisplayAudio can't synthesise. It is fire-and-forget
   over a 9600-baud UART:

       Teensy Serial2 TX2 (pin 8 / AUDIO_TX) -> DFPlayer RX
       Teensy Serial2 RX2 (pin 7 / AUDIO_RX) <- DFPlayer TX

   On the carrier board the DFPlayer BUSY line drives a status LED via Q2; it is
   NOT wired back to the Teensy, so playback state is not polled here — commands
   are sent open-loop. This driver implements the DFPlayer 10-byte command frame
   directly, so no third-party DFPlayer library is required.

   Frame: 7E FF 06 CMD FB PARAM_H PARAM_L CK_H CK_L EF
          checksum = -(sum of bytes FF..PARAM_L)

   Usage:
     #include <KCM_DFPlayer.h>
     KCM_DFPlayer dfp(KCM_DFPLAYER_SERIAL);   // Serial2
     void setup() { dfp.begin(); dfp.setVolume(22); }
     dfp.playTrack(1);            // play 0001.mp3 in the root / first index
     dfp.playFolderTrack(2, 5);   // play /02/005.mp3

   Licensed under the GNU General Public License v3.0 (GPL-3.0).
   Written for Jeb's Controller Works.
****************************************************************************************/

#include <Arduino.h>

class KCM_DFPlayer {
 public:
  explicit KCM_DFPlayer(HardwareSerial &port) : _port(port) {}

  // Open the UART and apply an initial volume (0..30). Allow ~1s after power-on
  // for the DFPlayer to boot before the first play command.
  void begin(uint8_t volume = 20, uint32_t baud = 9600);

  void setVolume(uint8_t volume);      // 0..30
  void playTrack(uint16_t track);      // global index (0x03)
  void playFolderTrack(uint8_t folder, uint8_t track);  // /FF/TTT (0x0F)
  void loopTrack(uint16_t track);      // 0x08
  void stop();                         // 0x16
  void pause();                        // 0x0E
  void resume();                       // 0x0D
  void reset();                        // 0x0C

 private:
  void sendCmd(uint8_t cmd, uint16_t param);
  HardwareSerial &_port;
};

#endif // KCM_DFPLAYER_H
