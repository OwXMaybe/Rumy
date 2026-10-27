#pragma once

#include <stddef.h>
#include <stdint.h>

class Preferences {
 public:
  bool begin(const char*, bool) { return true; }
  void end() {}
  bool clear() { return true; }
  bool isKey(const char*) { return false; }
  size_t getString(const char*, char*, size_t) { return 0; }
  size_t putString(const char*, const char* v) { return v ? 1 : 0; }
  float getFloat(const char*, float d) { return d; }
  size_t putFloat(const char*, float) { return 4; }
  uint8_t getUChar(const char*, uint8_t d) { return d; }
  size_t putUChar(const char*, uint8_t) { return 1; }
  uint16_t getUShort(const char*, uint16_t d) { return d; }
  size_t putUShort(const char*, uint16_t) { return 2; }
  uint32_t getUInt(const char*, uint32_t d) { return d; }
  size_t putUInt(const char*, uint32_t) { return 4; }
};
