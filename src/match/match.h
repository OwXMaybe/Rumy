#pragma once

#include <Print.h>
#include <stdint.h>

#include "net/fetch.h"

namespace match {

int16_t fetch(net::FetchStat& st);
uint32_t periodS();
void refresh();
void report(Print& out);
bool demo(const char* what);

}
