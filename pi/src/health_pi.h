#pragma once

#include <stddef.h>

namespace pi {

void healthBegin();

void watchdogBegin();
void watchdogKick();
void watchdogHold();
void watchdogStatus(char* out, size_t cap);

}
