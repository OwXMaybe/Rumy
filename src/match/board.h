#pragma once

#include <Print.h>
#include <stdint.h>

#include "match/fast.h"
#include "net/fetch.h"

namespace board {

int16_t fetch(net::FetchStat& st);
uint32_t periodS();
void refresh();
void fastMsg(const ::fast::Msg& m);
void report(Print& out);

}
