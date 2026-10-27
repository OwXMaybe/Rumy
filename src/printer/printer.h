#pragma once

#include <Print.h>
#include <stdint.h>

namespace printer {

void service(uint32_t now);
bool holdForTls();
void releaseAfterTls(uint32_t now);
void report(Print& out);

}
