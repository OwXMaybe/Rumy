#pragma once

#include <Print.h>
#include <stdint.h>

namespace webui {

void setEnabled(bool on);
bool enabled();
bool running();
bool busy();
void service(uint32_t nowMs);
uint16_t pin();
uint32_t lastActivityMs();
void setPairing(bool on);
uint16_t pairCode();
uint8_t phones();
void forgetPhones();
bool mdnsUp();
void report(Print& out);

}
