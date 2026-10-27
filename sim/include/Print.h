#pragma once

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

class Print {
 public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t c) = 0;
  virtual size_t write(const uint8_t* b, size_t n) {
    size_t k = 0;
    while (k < n && write(b[k])) k++;
    return k;
  }
  size_t print(const char* s) { return write(reinterpret_cast<const uint8_t*>(s), strlen(s)); }
  size_t println(const char* s) { return print(s) + print("\n"); }
  size_t println() { return print("\n"); }
  size_t printf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return 0;
    if (static_cast<size_t>(n) >= sizeof(buf)) n = static_cast<int>(sizeof(buf) - 1);
    return write(reinterpret_cast<const uint8_t*>(buf), static_cast<size_t>(n));
  }
};
