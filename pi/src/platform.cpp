#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_partition.h>
#include <esp_random.h>
#include <esp_rom_crc.h>
#include <esp_timer.h>
#include <fcntl.h>
#include <freertos/task.h>
#include <mbedtls/base64.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <time.h>
#include <unistd.h>

#include "app/config.h"
#include "paths.h"

PiEsp ESP;

namespace {

uint64_t monoUs() {
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<uint64_t>(ts.tv_sec) * 1000000ULL + static_cast<uint64_t>(ts.tv_nsec) / 1000ULL;
}

uint64_t startUs() {
  static const uint64_t s = monoUs();
  return s;
}

struct TaskBox {
  TaskFunction_t fn;
  void* arg;
  char name[16];
};

void* taskMain(void* p) {
  TaskBox* b = static_cast<TaskBox*>(p);
  pthread_setname_np(pthread_self(), b->name);
  b->fn(b->arg);
  return nullptr;
}

uint32_t s_crcTable[256];
bool s_crcReady = false;

void crcInit() {
  for (uint32_t i = 0; i < 256; i++) {
    uint32_t c = i;
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    s_crcTable[i] = c;
  }
  s_crcReady = true;
}

pthread_mutex_t s_memMux = PTHREAD_MUTEX_INITIALIZER;
size_t s_memAvail = 0;
size_t s_memMin = 0;
uint32_t s_memAt = 0;
bool s_memOnce = false;

size_t readMemAvailable() {
  FILE* f = fopen("/proc/meminfo", "r");
  if (!f) return 0;
  char line[128];
  unsigned long kb = 0;
  while (fgets(line, sizeof(line), f)) {
    if (sscanf(line, "MemAvailable: %lu kB", &kb) == 1) break;
  }
  fclose(f);
  uint64_t b = static_cast<uint64_t>(kb) * 1024ULL;
  return b > 0xFFFFFFFFULL ? 0xFFFFFFFFu : static_cast<size_t>(b);
}

size_t memAvailable() {
  pthread_mutex_lock(&s_memMux);
  uint32_t now = millis();
  if (!s_memOnce || now - s_memAt >= 500) {
    s_memAt = now;
    s_memAvail = readMemAvailable();
    if (!s_memOnce || s_memAvail < s_memMin) s_memMin = s_memAvail;
    s_memOnce = true;
  }
  size_t v = s_memAvail;
  pthread_mutex_unlock(&s_memMux);
  return v;
}

constexpr uint32_t ART_BYTES = 24u * cfg::MEDIA_SLOT_BYTES;
esp_partition_t s_art = {ESP_PARTITION_TYPE_DATA, 0x40, 0, ART_BYTES, "art"};
uint8_t* s_artMap = nullptr;

bool artOpen() {
  if (s_artMap) return true;
  char path[512];
  snprintf(path, sizeof(path), "%s/kapaklar.bin", pi::dataDir());
  pi::ensureParent(path);
  int fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC, 0644);
  if (fd < 0) return false;
  struct stat st;
  if (fstat(fd, &st) != 0 || static_cast<uint64_t>(st.st_size) != ART_BYTES) {
    if (ftruncate(fd, ART_BYTES) != 0) {
      close(fd);
      return false;
    }
  }
  void* m = mmap(nullptr, ART_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(fd);
  if (m == MAP_FAILED) return false;
  s_artMap = static_cast<uint8_t*>(m);
  return true;
}

bool artRange(const esp_partition_t* p, size_t off, size_t n) {
  return p == &s_art && s_artMap && off <= ART_BYTES && n <= ART_BYTES - off;
}

uint32_t readUintFile(const char* path) {
  FILE* f = fopen(path, "r");
  if (!f) return 0;
  unsigned long v = 0;
  if (fscanf(f, "%lu", &v) != 1) v = 0;
  fclose(f);
  return static_cast<uint32_t>(v);
}

}

uint32_t millis() { return static_cast<uint32_t>((monoUs() - startUs()) / 1000ULL); }

uint32_t micros() { return static_cast<uint32_t>(monoUs() - startUs()); }

void delay(uint32_t ms) {
  timespec ts;
  ts.tv_sec = static_cast<time_t>(ms / 1000U);
  ts.tv_nsec = static_cast<long>(ms % 1000U) * 1000000L;
  while (nanosleep(&ts, &ts) != 0) {
  }
}

void vTaskDelay(uint32_t ms) {
  if (!ms) {
    sched_yield();
    return;
  }
  delay(ms);
}

uint32_t uxTaskGetStackHighWaterMark(TaskHandle_t) { return 65536; }

int xTaskCreatePinnedToCore(TaskFunction_t fn, const char* name, uint32_t, void* arg, int, TaskHandle_t* handle, int) {
  TaskBox* b = new TaskBox;
  b->fn = fn;
  b->arg = arg;
  snprintf(b->name, sizeof(b->name), "%s", name ? name : "gorev");
  pthread_t t;
  if (pthread_create(&t, nullptr, taskMain, b) != 0) {
    delete b;
    return pdFAIL;
  }
  pthread_detach(t);
  if (handle) *handle = b;
  return pdPASS;
}

uint32_t esp_random() {
  uint32_t v = 0;
  if (getrandom(&v, sizeof(v), 0) != static_cast<ssize_t>(sizeof(v))) v = static_cast<uint32_t>(monoUs() * 2654435761ULL);
  return v;
}

int64_t esp_timer_get_time() { return static_cast<int64_t>(monoUs() - startUs()); }

void* heap_caps_malloc(size_t size, uint32_t) { return malloc(size); }

void* heap_caps_aligned_alloc(size_t align, size_t size, uint32_t) {
  void* p = nullptr;
  if (align < sizeof(void*)) align = sizeof(void*);
  return posix_memalign(&p, align, size) == 0 ? p : nullptr;
}

void heap_caps_free(void* p) { free(p); }

size_t heap_caps_get_free_size(uint32_t) { return memAvailable(); }

size_t heap_caps_get_minimum_free_size(uint32_t) {
  memAvailable();
  pthread_mutex_lock(&s_memMux);
  size_t v = s_memMin;
  pthread_mutex_unlock(&s_memMux);
  return v;
}

size_t heap_caps_get_largest_free_block(uint32_t) { return memAvailable(); }

uint32_t esp_rom_crc32_le(uint32_t crc, const uint8_t* buf, uint32_t len) {
  if (!s_crcReady) crcInit();
  crc = ~crc;
  for (uint32_t i = 0; i < len; i++) crc = s_crcTable[(crc ^ buf[i]) & 0xFFu] ^ (crc >> 8);
  return ~crc;
}

int mbedtls_base64_encode(unsigned char* dst, size_t dlen, size_t* olen, const unsigned char* src, size_t slen) {
  static const char kAlpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t need = ((slen + 2) / 3) * 4;
  if (dlen < need + 1) {
    *olen = need + 1;
    return MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL;
  }
  size_t o = 0;
  size_t i = 0;
  while (i + 2 < slen) {
    uint32_t v = (static_cast<uint32_t>(src[i]) << 16) | (static_cast<uint32_t>(src[i + 1]) << 8) | src[i + 2];
    dst[o++] = static_cast<unsigned char>(kAlpha[(v >> 18) & 63]);
    dst[o++] = static_cast<unsigned char>(kAlpha[(v >> 12) & 63]);
    dst[o++] = static_cast<unsigned char>(kAlpha[(v >> 6) & 63]);
    dst[o++] = static_cast<unsigned char>(kAlpha[v & 63]);
    i += 3;
  }
  if (i < slen) {
    uint32_t v = static_cast<uint32_t>(src[i]) << 16;
    if (i + 1 < slen) v |= static_cast<uint32_t>(src[i + 1]) << 8;
    dst[o++] = static_cast<unsigned char>(kAlpha[(v >> 18) & 63]);
    dst[o++] = static_cast<unsigned char>(kAlpha[(v >> 12) & 63]);
    dst[o++] = i + 1 < slen ? static_cast<unsigned char>(kAlpha[(v >> 6) & 63]) : '=';
    dst[o++] = '=';
  }
  dst[o] = 0;
  *olen = o;
  return 0;
}

const esp_partition_t* esp_partition_find_first(esp_partition_type_t, esp_partition_subtype_t, const char* label) {
  if (!label || strcmp(label, "art") != 0) return nullptr;
  return artOpen() ? &s_art : nullptr;
}

esp_err_t esp_partition_read(const esp_partition_t* p, size_t off, void* dst, size_t n) {
  if (!artRange(p, off, n)) return ESP_FAIL;
  memcpy(dst, s_artMap + off, n);
  return ESP_OK;
}

esp_err_t esp_partition_write(const esp_partition_t* p, size_t off, const void* src, size_t n) {
  if (!artRange(p, off, n)) return ESP_FAIL;
  memcpy(s_artMap + off, src, n);
  return ESP_OK;
}

esp_err_t esp_partition_erase_range(const esp_partition_t* p, size_t off, size_t n) {
  if (!artRange(p, off, n)) return ESP_FAIL;
  memset(s_artMap + off, 0xFF, n);
  return ESP_OK;
}

esp_err_t esp_partition_mmap(const esp_partition_t* p, size_t off, size_t n, spi_flash_mmap_memory_t, const void** out,
                             spi_flash_mmap_handle_t* handle) {
  if (!artRange(p, off, n)) return ESP_FAIL;
  *out = s_artMap + off;
  if (handle) *handle = 1;
  return ESP_OK;
}

uint64_t PiEsp::getEfuseMac() {
  FILE* f = fopen("/sys/class/net/wlan0/address", "r");
  if (!f) return 0;
  unsigned b[6] = {};
  int n = fscanf(f, "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]);
  fclose(f);
  if (n != 6) return 0;
  uint64_t v = 0;
  for (int i = 5; i >= 0; i--) v = (v << 8) | (b[i] & 0xFFu);
  return v;
}

uint32_t PiEsp::getChipRevision() {
  FILE* f = fopen("/proc/cpuinfo", "r");
  if (!f) return 0;
  char line[128];
  unsigned long rev = 0;
  while (fgets(line, sizeof(line), f)) {
    if (sscanf(line, "Revision : %lx", &rev) == 1) break;
  }
  fclose(f);
  return static_cast<uint32_t>(rev);
}

uint32_t PiEsp::getCpuFreqMHz() {
  uint32_t khz = readUintFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
  return khz ? khz / 1000U : 1000U;
}

uint32_t PiEsp::getFlashChipSize() {
  struct statvfs v;
  if (statvfs("/", &v) != 0) return 0;
  uint64_t b = static_cast<uint64_t>(v.f_blocks) * v.f_frsize;
  return b > 0xFFFFFFFFULL ? 0xFFFFFFFFu : static_cast<uint32_t>(b);
}

uint32_t PiEsp::getSketchSize() {
  struct stat st;
  if (stat("/proc/self/exe", &st) != 0) return 0;
  return static_cast<uint32_t>(st.st_size);
}

uint32_t PiEsp::getFreeSketchSpace() {
  struct statvfs v;
  if (statvfs(pi::dataDir(), &v) != 0) return 0;
  uint64_t b = static_cast<uint64_t>(v.f_bavail) * v.f_frsize;
  return b > 0xFFFFFFFFULL ? 0xFFFFFFFFu : static_cast<uint32_t>(b);
}

void PiEsp::restart() {
  Serial.flush();
  pi::markCleanExit();
  fflush(stdout);
  _exit(0);
}
