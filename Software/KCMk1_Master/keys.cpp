/***************************************************************************************
   keys.cpp — keyboard emulation and held-key tracking.
****************************************************************************************/
#include "keys.h"
#include "ksp_out.h"
#include "debug.h"

static const uint8_t MAX_HELD = 12;
static int16_t g_held[MAX_HELD];
static uint8_t g_heldN;

static int16_t  g_seq[KEY_SEQ_MAX];
static uint8_t  g_seqN, g_seqPos;
static uint32_t g_seqNextMs;

void keysInit() { g_heldN = 0; g_seqN = g_seqPos = 0; }

void keySequence(const int16_t *vks, uint8_t n) {
  if (n > KEY_SEQ_MAX) n = KEY_SEQ_MAX;
  memcpy(g_seq, vks, n * sizeof vks[0]);
  g_seqN = n; g_seqPos = 0;
  g_seqNextMs = millis();
  dbg(DBG_KEYS, "macro of %u keys", n);
  keysService();
}

void keysService() {
  while (g_seqPos < g_seqN && (int32_t)(millis() - g_seqNextMs) >= 0) {
    keyTap(g_seq[g_seqPos++]);
    g_seqNextMs = millis() + KEY_SEQ_GAP_MS;
  }
}

bool keyHeld(int16_t vk) {
  for (uint8_t i = 0; i < g_heldN; i++) if (g_held[i] == vk) return true;
  return false;
}

bool keyTap(int16_t vk, uint8_t modifier) {
  dbg(DBG_KEYS, "key %02X tap (mod %02X)", vk, modifier);
  return kspKey(vk, modifier);
}

bool keyHold(int16_t vk, bool down) {
  if (down == keyHeld(vk)) return false;
  if (down) {
    if (g_heldN >= MAX_HELD) return false;
    if (!kspKey(vk, KEY_DOWN_MOD)) return false;   // not sent: not held either
    g_held[g_heldN++] = vk;
  } else {
    kspKey(vk, KEY_UP_MOD);
    for (uint8_t i = 0; i < g_heldN; i++) if (g_held[i] == vk) { g_held[i] = g_held[--g_heldN]; break; }
  }
  dbg(DBG_KEYS, "key %02X %s", vk, down ? "down" : "up");
  return true;
}

void keysReleaseAll() {
  while (g_heldN) keyHold(g_held[g_heldN - 1], false);
  g_seqN = g_seqPos = 0;                           // and drop a macro still running
}
