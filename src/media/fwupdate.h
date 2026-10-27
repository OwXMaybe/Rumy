#pragma once

#include <Print.h>
#include <stddef.h>
#include <stdint.h>

namespace fwupdate {

enum Phase : uint8_t { FW_IDLE, FW_RX, FW_DONE, FW_FAIL };

struct Progress {
  Phase phase;
  uint32_t got;
  uint32_t total;
  uint32_t atMs;
  const char* why;
};

void offer(const char* args);
bool receiving();
bool receive();
void loop();
Progress progress();
void runningElf(char* out, size_t cap);
void updaterElf(char* out, size_t cap);
void report(Print& out);

}
