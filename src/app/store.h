#pragma once

#include <stddef.h>
#include <stdint.h>

namespace store {

enum Domain : uint8_t {
  FORECAST = 0,
  DAILY,
  AIR,
  MARINE,
  QUAKE,
  EVENTS,
  BULB,
  NET,
  LVGL,
  PRINTER,
  MATCH,
  BOARD,
  VIEW,
  SETTINGS,
  COUNT,
};

void init();
void put(Domain d, const void* src, size_t n);
bool get(Domain d, void* dst, size_t n, uint32_t& seen);
void peek(Domain d, void* dst, size_t n);
uint32_t version(Domain d);
void bump(Domain d);

template <typename T>
void putT(Domain d, const T& v) {
  put(d, &v, sizeof(T));
}

template <typename T>
bool getT(Domain d, T& v, uint32_t& seen) {
  return get(d, &v, sizeof(T), seen);
}

template <typename T>
void peekT(Domain d, T& v) {
  peek(d, &v, sizeof(T));
}

}
