#pragma once

#include <Print.h>
#include <stdint.h>

namespace cam {

void want(bool on, uint32_t printerIp, int32_t x, int32_t y);
void loop();
void start(char* args);
bool live();
void report(Print& out);

}
