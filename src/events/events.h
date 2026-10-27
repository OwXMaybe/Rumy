#pragma once

#include <stddef.h>
#include <stdint.h>

#include "net/fetch.h"

namespace events {

void begin();
void tick();
bool holidaysNeeded();
int16_t fetchHolidays(net::FetchStat& st);
size_t personalJson(char* out, size_t cap);
bool setPersonal(const char* json, size_t len);
int personalCount();

}
