#pragma once

#ifdef ARDUINO
#include <Arduino.h>
#else

#include <ctype.h>
#include <string.h>

#include <cstdint>

#define PROGMEM

#define memcpy_P(to, from, len) memcpy(to, from, len)
#define strncmp_P(s1, s2, size) strncmp(s1, s2, size)
#define strlen_P(str) strlen(str)
#define strncpy_P(dest, source, size) strncpy(dest, source, size)

#define pgm_read_byte(addr) (*(const unsigned char*)(addr))

class __FlashStringHelper;
#define F(string_literal) \
  (reinterpret_cast<const __FlashStringHelper*>(string_literal))

#define abs(x) ((x) > 0 ? (x) : -(x))

#endif
