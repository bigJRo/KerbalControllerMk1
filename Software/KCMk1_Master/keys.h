/***************************************************************************************
   keys.h — keyboard events to KSP through Simpit's keyboard emulator (Windows virtual
   key codes). A tap sends one press-and-release; a held key sends KEY_DOWN, then KEY_UP
   when let go. Held keys are tracked, so none is left down when a flight ends, the
   link drops or demo starts (keysReleaseAll()). Logged as debug category KEYS.
****************************************************************************************/
#pragma once
#include <Arduino.h>

// The keys the panels use (VK_*).
enum : int16_t {
  VK_BACKSPACE = 0x08, VK_SPACE = 0x20,
  VK_A = 0x41, VK_B = 0x42, VK_D = 0x44, VK_E = 0x45, VK_F = 0x46, VK_G = 0x47, VK_I = 0x49, VK_O = 0x4F,
  VK_P = 0x50, VK_Q = 0x51, VK_R = 0x52, VK_S = 0x53, VK_U = 0x55, VK_W = 0x57,
  VK_LSHIFT = 0xA0, VK_LCONTROL = 0xA2, VK_OEM_PLUS = 0xBB, VK_OEM_MINUS = 0xBD
};

void keysInit();
bool keyTap(int16_t vk, uint8_t modifier = 0);
bool keyHold(int16_t vk, bool down);       // down / up; repeats are ignored
bool keyHeld(int16_t vk);
void keysReleaseAll();
