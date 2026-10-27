#include "clock.h"

#include <Arduino.h>
#include <time.h>

namespace clk {

namespace {

constexpr uint32_t kValidEpoch = 1735689600u;
bool s_begun = false;

}

void begin() {
  if (s_begun) return;
  configTzTime("<+03>-3", "pool.ntp.org", "time.google.com", "tr.pool.ntp.org");
  s_begun = true;
}

bool synced() { return nowUtc() >= kValidEpoch; }

uint32_t nowUtc() {
  time_t t = time(nullptr);
  return t > 0 ? static_cast<uint32_t>(t) : 0;
}

rc::LocalTime local() { return rc::toLocal(nowUtc()); }

int32_t todayDayNum() { return rc::localDayNum(nowUtc()); }

uint16_t minuteOfDay() {
  rc::LocalTime lt = local();
  return static_cast<uint16_t>(lt.hour * 60 + lt.minute);
}

}
