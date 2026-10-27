#pragma once

#include <stddef.h>
#include <stdint.h>

namespace rc {

constexpr size_t PHONE_TOKEN_HEX = 32;
constexpr uint8_t PHONE_MAX = 4;

inline uint32_t sleepUntilUtc(uint32_t nowUtc, uint16_t minuteOfDay, uint8_t second, uint16_t endMin) {
  uint32_t delta = (static_cast<uint32_t>(endMin % 1440u) + 1440u - minuteOfDay % 1440u) % 1440u;
  if (delta == 0) delta = 1440;
  return nowUtc + delta * 60u - second;
}

inline bool sleepActive(uint32_t until, uint32_t nowUtc) { return until != 0 && nowUtc < until; }

inline bool phoneTokenWellFormed(const char* t) {
  if (!t) return false;
  for (size_t i = 0; i < PHONE_TOKEN_HEX; i++) {
    char c = t[i];
    bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    if (!hex) return false;
  }
  return t[PHONE_TOKEN_HEX] == 0;
}

inline bool phoneTokenEqual(const char* a, const char* b) {
  if (!phoneTokenWellFormed(a) || !phoneTokenWellFormed(b)) return false;
  uint8_t diff = 0;
  for (size_t i = 0; i < PHONE_TOKEN_HEX; i++) diff |= static_cast<uint8_t>(a[i] ^ b[i]);
  return diff == 0;
}

inline bool pairCodeParse(const char* s, uint16_t& out) {
  if (!s) return false;
  uint16_t v = 0;
  for (int i = 0; i < 4; i++) {
    if (s[i] < '0' || s[i] > '9') return false;
    v = static_cast<uint16_t>(v * 10 + (s[i] - '0'));
  }
  if (s[4] != 0 || v < 1000) return false;
  out = v;
  return true;
}

inline bool hexColorParse(const char* s, uint32_t& out) {
  if (!s) return false;
  uint32_t c = 0;
  for (int i = 0; i < 6; i++) {
    char ch = s[i];
    uint32_t d = 16;
    if (ch >= '0' && ch <= '9') d = static_cast<uint32_t>(ch - '0');
    else if (ch >= 'a' && ch <= 'f') d = static_cast<uint32_t>(ch - 'a' + 10);
    else if (ch >= 'A' && ch <= 'F') d = static_cast<uint32_t>(ch - 'A' + 10);
    if (d > 15) return false;
    c = c << 4 | d;
  }
  if (s[6] != 0) return false;
  out = c;
  return true;
}

}
