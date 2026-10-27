#pragma once

#include <stddef.h>
#include <stdint.h>

namespace sdcard {

bool begin();
bool mounted();
void lock();
void unlock();
bool readFile(const char* path, uint8_t* buf, size_t cap, size_t& len);
bool writeFileAtomic(const char* path, const uint8_t* data, size_t len);
bool exists(const char* path);
bool remove(const char* path);
bool rename(const char* from, const char* to);
bool streamOpen(const char* path, bool append);
bool streamWrite(const uint8_t* data, size_t len);
bool streamClose();
uint64_t totalBytes();
uint64_t usedBytes();
uint32_t readErrors();
uint32_t writeErrors();
uint32_t writes();
bool selfTest(uint32_t iterations, uint32_t& okCount);

class Guard {
 public:
  Guard() { lock(); }
  ~Guard() { unlock(); }
  Guard(const Guard&) = delete;
  Guard& operator=(const Guard&) = delete;
};

}
