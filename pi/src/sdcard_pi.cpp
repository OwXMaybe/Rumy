#include "board/sdcard.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include "paths.h"

namespace sdcard {

namespace {

pthread_mutex_t s_mx;
bool s_mxReady = false;
bool s_mounted = false;
uint32_t s_readErr = 0;
uint32_t s_writeErr = 0;
uint32_t s_writes = 0;

void initMutex() {
  if (s_mxReady) return;
  pthread_mutexattr_t a;
  pthread_mutexattr_init(&a);
  pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(&s_mx, &a);
  pthread_mutexattr_destroy(&a);
  s_mxReady = true;
}

void full(const char* path, char* out, size_t cap) { snprintf(out, cap, "%s%s", RUMY_SD_ROOT, path); }

}

bool begin() {
  initMutex();
  s_mounted = pi::ensureDir(RUMY_SD_ROOT);
  return s_mounted;
}

bool mounted() { return s_mounted; }

void lock() {
  initMutex();
  pthread_mutex_lock(&s_mx);
}

void unlock() { pthread_mutex_unlock(&s_mx); }

bool readFile(const char* path, uint8_t* buf, size_t cap, size_t& len) {
  len = 0;
  if (!s_mounted) return false;
  Guard g;
  char p[512];
  full(path, p, sizeof(p));
  if (access(p, F_OK) != 0) return false;
  if (!pi::readAll(p, buf, cap, len)) {
    s_readErr++;
    len = 0;
    return false;
  }
  return true;
}

bool writeFileAtomic(const char* path, const uint8_t* data, size_t len) {
  if (!s_mounted) return false;
  Guard g;
  char p[512];
  full(path, p, sizeof(p));
  if (!pi::writeAtomic(p, data, len, false)) {
    s_writeErr++;
    return false;
  }
  s_writes++;
  return true;
}

bool exists(const char* path) {
  char p[512];
  full(path, p, sizeof(p));
  return access(p, F_OK) == 0;
}

bool remove(const char* path) {
  Guard g;
  char p[512];
  full(path, p, sizeof(p));
  return unlink(p) == 0;
}

uint64_t totalBytes() {
  struct statvfs v;
  if (statvfs(RUMY_SD_ROOT, &v) != 0) return 0;
  return static_cast<uint64_t>(v.f_blocks) * v.f_frsize;
}

uint64_t usedBytes() {
  struct statvfs v;
  if (statvfs(RUMY_SD_ROOT, &v) != 0) return 0;
  return static_cast<uint64_t>(v.f_blocks - v.f_bfree) * v.f_frsize;
}

uint32_t readErrors() { return s_readErr; }

uint32_t writeErrors() { return s_writeErr; }

uint32_t writes() { return s_writes; }

bool selfTest(uint32_t iterations, uint32_t& okCount) {
  okCount = 0;
  uint8_t data[512];
  uint8_t back[600];
  for (uint32_t i = 0; i < iterations; i++) {
    for (size_t k = 0; k < sizeof(data); k++) data[k] = static_cast<uint8_t>((k * 31u + i * 7u) & 0xFFu);
    if (!writeFileAtomic("/rumy/sdtest.bin", data, sizeof(data))) continue;
    size_t len = 0;
    if (readFile("/rumy/sdtest.bin", back, sizeof(back), len) && len == sizeof(data) && !memcmp(data, back, len)) okCount++;
  }
  remove("/rumy/sdtest.bin");
  return okCount == iterations;
}

}
