#pragma once

#include <stdint.h>

#include "net/fetch.h"

namespace weather {

void loadCache();
void clearAll();
int16_t fetchWeather(net::FetchStat& st);
int16_t fetchAir(net::FetchStat& st);
int16_t fetchMarine(net::FetchStat& st);

}
