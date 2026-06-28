#include "KCM_DFPlayer.h"

/***************************************************************************************
   KCM_DFPlayer — implementation. See KCM_DFPlayer.h for protocol notes.
****************************************************************************************/

// DFPlayer command bytes
#define DFP_CMD_PLAY_TRACK    0x03
#define DFP_CMD_SET_VOLUME    0x06
#define DFP_CMD_LOOP_TRACK    0x08
#define DFP_CMD_PLAY_FOLDER   0x0F
#define DFP_CMD_RESET         0x0C
#define DFP_CMD_RESUME        0x0D
#define DFP_CMD_PAUSE         0x0E
#define DFP_CMD_STOP          0x16

void KCM_DFPlayer::begin(uint8_t volume, uint32_t baud) {
  _port.begin(baud);
  delay(20);
  reset();
  delay(1000);          // DFPlayer needs ~1s to mount its SD card after reset
  setVolume(volume);
}

// Build and transmit one 10-byte command frame.
// Frame: 7E FF 06 CMD 00 PARAM_H PARAM_L CK_H CK_L EF
// Checksum = 0 - (FF + 06 + CMD + 00 + PARAM_H + PARAM_L), 16-bit two's complement.
void KCM_DFPlayer::sendCmd(uint8_t cmd, uint16_t param) {
  uint8_t ph = (uint8_t)(param >> 8);
  uint8_t pl = (uint8_t)(param & 0xFF);
  uint16_t sum = 0xFF + 0x06 + cmd + 0x00 + ph + pl;
  uint16_t checksum = (uint16_t)(-sum);

  uint8_t frame[10] = {
    0x7E, 0xFF, 0x06, cmd, 0x00, ph, pl,
    (uint8_t)(checksum >> 8), (uint8_t)(checksum & 0xFF), 0xEF
  };
  _port.write(frame, sizeof(frame));
}

void KCM_DFPlayer::setVolume(uint8_t volume) {
  if (volume > 30) volume = 30;
  sendCmd(DFP_CMD_SET_VOLUME, volume);
}
void KCM_DFPlayer::playTrack(uint16_t track)  { sendCmd(DFP_CMD_PLAY_TRACK, track); }
void KCM_DFPlayer::loopTrack(uint16_t track)  { sendCmd(DFP_CMD_LOOP_TRACK, track); }
void KCM_DFPlayer::playFolderTrack(uint8_t folder, uint8_t track) {
  sendCmd(DFP_CMD_PLAY_FOLDER, (uint16_t)((folder << 8) | track));
}
void KCM_DFPlayer::stop()   { sendCmd(DFP_CMD_STOP,   0); }
void KCM_DFPlayer::pause()  { sendCmd(DFP_CMD_PAUSE,  0); }
void KCM_DFPlayer::resume() { sendCmd(DFP_CMD_RESUME, 0); }
void KCM_DFPlayer::reset()  { sendCmd(DFP_CMD_RESET,  0); }
