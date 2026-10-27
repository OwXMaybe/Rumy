#pragma once

#include <Print.h>
#include <stdint.h>

#include "rc_types.h"

namespace relay {

void line(const char* text);
void service();
bool linked();
bool quake(const char* afadPath, rc::QuakeInfo& out);
bool holidays(int year, rc::Religious& out);
uint32_t lastMs();
void report(Print& out);

}
