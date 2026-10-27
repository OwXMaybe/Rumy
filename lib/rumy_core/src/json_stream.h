#pragma once

#include <ArduinoJson.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "afad.h"
#include "json_arena.h"
#include "reader.h"

namespace rc {
namespace js {

constexpr uint8_t ESPN_NESTING = 24;

inline size_t fin(int n, size_t cap) { return (n < 0 || static_cast<size_t>(n) >= cap) ? 0 : static_cast<size_t>(n); }

inline bool readString(ByteSource& s, char* buf, size_t cap) {
  s.read();
  size_t k = 0;
  for (;;) {
    int c = s.read();
    if (c < 0) return false;
    if (c == '\\') {
      c = s.read();
      if (c < 0) return false;
    } else if (c == '"') {
      break;
    }
    if (k + 1 < cap) buf[k++] = static_cast<char>(c);
  }
  if (cap) buf[k] = 0;
  return true;
}

class TopKeys {
 public:
  explicit TopKeys(ByteSource& s) : s_(s) {}

  bool next(char* key, size_t cap) {
    for (;;) {
      int c = s_.peek();
      if (c < 0) return false;
      if (c == '"') {
        if (!readString(s_, key, cap)) return false;
        if (depth_ == 1 && skipSpaces(s_) == ':') {
          s_.read();
          return true;
        }
        continue;
      }
      s_.read();
      if (c == '{' || c == '[') {
        depth_++;
      } else if (c == '}' || c == ']') {
        if (--depth_ <= 0) return false;
      }
    }
  }

 private:
  ByteSource& s_;
  int depth_ = 0;
};

inline uint32_t readUint(ByteSource& s) {
  int c = skipSpaces(s);
  uint32_t v = 0;
  while (c >= '0' && c <= '9') {
    v = v * 10u + static_cast<uint32_t>(c - '0');
    s.read();
    c = s.peek();
  }
  if (c == '.') {
    s.read();
    c = s.peek();
    while (c >= '0' && c <= '9') {
      s.read();
      c = s.peek();
    }
  }
  return v;
}

template <typename F>
inline bool eachElement(ByteSource& src, JsonArena& arena, JsonDocument& doc, JsonDocument& filter, size_t base, F fn) {
  if (!expectChar(src, '[')) return false;
  int c = skipSpaces(src);
  if (c == ']') {
    src.read();
    return true;
  }
  for (;;) {
    doc.clear();
    arena.rewind(base);
    if (deserializeJson(doc, src, DeserializationOption::Filter(filter), DeserializationOption::NestingLimit(ESPN_NESTING)))
      return false;
    fn(doc.as<JsonObjectConst>());
    c = skipSpaces(src);
    if (c == ',') {
      src.read();
      continue;
    }
    if (c == ']') {
      src.read();
      return true;
    }
    return false;
  }
}

inline uint32_t idAfter(const char* ref, const char* marker) {
  if (!ref) return 0;
  const char* p = strstr(ref, marker);
  if (!p) return 0;
  return static_cast<uint32_t>(strtoul(p + strlen(marker), nullptr, 10));
}

inline uint32_t idOf(JsonVariantConst v) {
  if (v.is<const char*>()) return static_cast<uint32_t>(strtoul(v.as<const char*>(), nullptr, 10));
  return v.as<uint32_t>();
}

inline void copyText(char* out, size_t cap, const char* in) { utf8Copy(out, cap, in ? in : "", in ? strlen(in) : 0); }

inline bool seekKey(ByteSource& s, const char* want) {
  char key[24];
  for (;;) {
    int c = s.peek();
    if (c < 0) return false;
    if (c != '"') {
      s.read();
      continue;
    }
    if (!readString(s, key, sizeof(key))) return false;
    if (strcmp(key, want)) continue;
    if (skipSpaces(s) == ':') {
      s.read();
      return true;
    }
  }
}

}
}
