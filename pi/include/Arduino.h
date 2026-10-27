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
void delay(uint32_t ms);
void vTaskDelay(uint32_t ms);
uint32_t uxTaskGetStackHighWaterMark(TaskHandle_t task);

class PiSerial : public Print {
 public:
  size_t write(uint8_t c) override;
  size_t write(const uint8_t* b, size_t n) override;
  int available();
  int read();
  size_t read(uint8_t* buf, size_t n);
  void flush();
};

class PiEsp {
 public:
  uint64_t getEfuseMac();
  uint32_t getChipRevision();
  uint32_t getCpuFreqMHz();
  uint32_t getFlashChipSize();
  uint32_t getSketchSize();
  uint32_t getFreeSketchSpace();
  void restart();
};

extern PiSerial Serial;
extern PiEsp ESP;
