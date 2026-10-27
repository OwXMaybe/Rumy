#pragma once

#include <stdint.h>

#include "timeutil.h"

namespace clk {

void begin();
bool synced();
uint32_t nowUtc();
rc::LocalTime local();
int32_t todayDayNum();
uint16_t minuteOfDay();

}
