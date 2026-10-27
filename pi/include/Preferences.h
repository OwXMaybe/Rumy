#pragma once

#include <stddef.h>
#include <stdint.h>

class Preferences {
 public:
  ~Preferences();
  bool begin(const char* ns, bool readOnly);
  void end();
  bool clear();
  bool isKey(const char* key);
  size_t getString(const char* key, char* out, size_t cap);
  size_t putString(const char* key, const char* v);
  float getFloat(const char* key, float d);
  size_t putFloat(const char* key, float v);
  uint8_t getUChar(const char* key, uint8_t d);
  size_t putUChar(const char* key, uint8_t v);
  uint16_t getUShort(const char* key, uint16_t d);
  size_t putUShort(const char* key, uint16_t v);
  uint32_t getUInt(const char* key, uint32_t d);
  size_t putUInt(const char* key, uint32_t v);

 private:
  bool number(const char* key, double& out);
  size_t putNumber(const char* key, double v, size_t width);
  void save();

  char ns_[16] = {};
  bool open_ = false;
  bool readOnly_ = true;
  bool dirty_ = false;
  void* doc_ = nullptr;
};
