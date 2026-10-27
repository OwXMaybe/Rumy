#pragma once

#include <stdint.h>

namespace app {

enum Action : uint8_t {
  ACT_NONE = 0,
  ACT_REBOOT,
  ACT_WIFI_RESET,
  ACT_FACTORY_RESET,
};

void request(Action a, uint32_t delayMs = 1500);
void poll();
Action pending();
const char* resetReasonText();
uint32_t bootCount();

}
