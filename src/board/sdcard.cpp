#include "sdcard.h"

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdio.h>
#include <string.h>

#include "app/config.h"

namespace sdcard {

namespace {

SPIClass s_spi(VSPI);
bool s_mounted = false;
SemaphoreHandle_t s_mutex = nullptr;
StaticSemaphore_t s_mutexBuf;
uint32_t s_readErr = 0;
uint32_t s_writeErr = 0;
uint32_t s_writes = 0;
File s_stream;
bool s_streamOk = false;

void ensureDir(const char* path) {
  if (!SD.exists(path)) SD.mkdir(path);
}

}

void lock() {
  if (!s_mutex) s_mutex = xSemaphoreCreateRecursiveMutexStatic(&s_mutexBuf);
  xSemaphoreTakeRecursive(s_mutex, portMAX_DELAY);
}

void unlock() { xSemaphoreGiveRecursive(s_mutex); }

bool begin() {
  Guard g;
  s_spi.begin(cfg::PIN_SD_SCK, cfg::PIN_SD_MISO, cfg::PIN_SD_MOSI, cfg::PIN_SD_CS);
  s_mounted = SD.begin(cfg::PIN_SD_CS, s_spi, cfg::SD_HZ, "/sd", cfg::SD_MAX_FILES, false);
  if (!s_mounted) {
    SD.end();
    s_mounted = SD.begin(cfg::PIN_SD_CS, s_spi, cfg::SD_HZ_SLOW, "/sd", cfg::SD_MAX_FILES, false);
  }
  if (!s_mounted) return false;
  if (SD.cardType() == CARD_NONE) {
    s_mounted = false;
    return false;
  }
  ensureDir(cfg::SD_ROOT);
  ensureDir(cfg::SD_CACHE_DIR);
  ensureDir(cfg::SD_HOLIDAY_DIR);
  return true;
}

bool mounted() { return s_mounted; }

bool readFile(const char* path, uint8_t* buf, size_t cap, size_t& len) {
  len = 0;
  if (!s_mounted) return false;
  Guard g;
  if (!SD.exists(path)) return false;
  File f = SD.open(path, FILE_READ);
  if (!f) return false;
  size_t size = f.size();
  if (size > cap) {
    f.close();
    return false;
  }
  len = f.read(buf, size);
  f.close();
  if (len != size) {
    s_readErr++;
    return false;
  }
  return true;
}

bool writeFileAtomic(const char* path, const uint8_t* data, size_t len) {
  if (!s_mounted) return false;
  char tmp[72];
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  Guard g;
  File f = SD.open(tmp, FILE_WRITE);
  if (!f) {
    s_writeErr++;
    return false;
  }
  size_t w = f.write(data, len);
  f.flush();
  f.close();
  if (w != len) {
    SD.remove(tmp);
    s_writeErr++;
    return false;
  }
  if (SD.exists(path)) SD.remove(path);
  if (!SD.rename(tmp, path)) {
    s_writeErr++;
    return false;
  }
  s_writes++;
  return true;
}

bool exists(const char* path) {
  if (!s_mounted) return false;
  Guard g;
  return SD.exists(path);
}

bool remove(const char* path) {
  if (!s_mounted) return false;
  Guard g;
  return SD.remove(path);
}

bool rename(const char* from, const char* to) {
  if (!s_mounted) return false;
  Guard g;
  if (SD.exists(to)) SD.remove(to);
  if (SD.rename(from, to)) return true;
  s_writeErr++;
  return false;
}

bool streamOpen(const char* path, bool append) {
  if (!s_mounted) return false;
  lock();
  s_stream = SD.open(path, append ? FILE_APPEND : FILE_WRITE);
  if (!s_stream) {
    s_writeErr++;
    unlock();
    return false;
  }
  s_streamOk = true;
  return true;
}

bool streamWrite(const uint8_t* data, size_t len) {
  if (!s_stream) return false;
  if (s_stream.write(data, len) == len) return true;
  s_streamOk = false;
  return false;
}

bool streamClose() {
  if (!s_stream) return false;
  s_stream.flush();
  s_stream.close();
  bool ok = s_streamOk;
  if (ok) s_writes++;
  else s_writeErr++;
  unlock();
  return ok;
}

uint64_t totalBytes() {
  if (!s_mounted) return 0;
  Guard g;
  return SD.totalBytes();
}

uint64_t usedBytes() {
  if (!s_mounted) return 0;
  Guard g;
  return SD.usedBytes();
}

uint32_t readErrors() { return s_readErr; }
uint32_t writeErrors() { return s_writeErr; }
uint32_t writes() { return s_writes; }

bool selfTest(uint32_t iterations, uint32_t& okCount) {
  okCount = 0;
  if (!s_mounted) return false;
  static const char* kPath = "/rumy/cache/selftest.bin";
  uint8_t pattern[256];
  for (int i = 0; i < 256; i++) pattern[i] = static_cast<uint8_t>(i * 7 + 3);
  if (!writeFileAtomic(kPath, pattern, sizeof(pattern))) return false;
  uint8_t back[256];
  for (uint32_t i = 0; i < iterations; i++) {
    size_t n = 0;
    if (readFile(kPath, back, sizeof(back), n) && n == sizeof(back) && memcmp(back, pattern, n) == 0) okCount++;
    vTaskDelay(1);
  }
  remove(kPath);
  return okCount == iterations;
}

}
