// Minimal host shim so Adafruit_GFX + ui.cpp compile on a PC.
#pragma once
#ifndef ARDUINO
#define ARDUINO 100
#endif
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "Print.h"
#define PROGMEM
#define pgm_read_byte(a) (*(const uint8_t*)(a))
#define pgm_read_word(a) (*(const uint16_t*)(a))
#define pgm_read_dword(a) (*(const uint32_t*)(a))
#define pgm_read_pointer(a) (*(void* const*)(a))
typedef bool boolean;
class __FlashStringHelper;
class String {
 public:
  String(const char* s = "") : s_(s) {}
  unsigned length() const { return strlen(s_); }
  const char* c_str() const { return s_; }
 private:
  const char* s_;
};
#ifndef _swap_int16_t
#define _swap_int16_t(a, b) { int16_t t = a; a = b; b = t; }
#endif
