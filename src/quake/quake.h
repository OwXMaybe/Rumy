#pragma once

#include <stdint.h>

#include "net/fetch.h"

namespace quake {

void loadCache();
int16_t fetch(net::FetchStat& st);

}
