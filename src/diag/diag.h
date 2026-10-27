#pragma once

#include <Print.h>
#include <stddef.h>
#include <stdint.h>

namespace diag {

void begin();
void loop();
void printReport(Print& out);
uint32_t uptimeS();
const char* resetReason();
int32_t heapTrendPerHour();
void note(const char* tag, const char* fmt, ...);

}
