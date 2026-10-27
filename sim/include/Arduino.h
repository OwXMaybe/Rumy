#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Print.h"

#define F(x) (x)
#define pdMS_TO_TICKS(ms) (ms)

typedef void* TaskHandle_t;

uint32_t millis();
uint32_t micros();
void simAdvance(uint32_t ms);
inline void vTaskDelay(uint32_t t) { simAdvance(t ? t : 1); }
inline uint32_t uxTaskGetStackHighWaterMark(void*) { return 2304; }

class SimSerial : public Print {
 public:
  size_t write(uint8_t c) override { return fputc(c, stdout) == EOF ? 0 : 1; }
  size_t write(const uint8_t* b, size_t n) override { return fwrite(b, 1, n, stdout); }
  int available() { return 0; }
  int read() { return -1; }
  void flush() { fflush(stdout); }
};

class SimEsp {
 public:
  uint64_t getEfuseMac() { return 0x3AF2C6A1B2C3ULL; }
  uint32_t getChipRevision() { return 3; }
  uint32_t getCpuFreqMHz() { return 240; }
  uint32_t getFlashChipSize() { return 4u * 1048576u; }
  uint32_t getSketchSize() { return 1532224u; }
  uint32_t getFreeSketchSpace() { return 3080192u; }
  void restart() { exit(0); }
};

extern SimSerial Serial;
extern SimEsp ESP;
