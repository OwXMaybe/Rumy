#pragma once

#include <Print.h>
#include <stdint.h>

#include "net/fetch.h"

namespace leagues {

void open(uint8_t league);
void step(int8_t dir);
void showTable(bool table);
void close();
uint8_t league();
bool tableMode();
uint16_t request();

int16_t fetch(net::FetchStat& st);
uint32_t periodS();
void report(Print& out);

}
