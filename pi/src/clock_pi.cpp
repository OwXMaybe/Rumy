#include "clock/clock.h"

#include <Arduino.h>
#include <sys/timex.h>
#include <time.h>

namespace clk {

namespace {

constexpr uint32_t kValidEpoch = 1735689600u;
constexpr uint32_t kTrustAfterMs = 10U * 60U * 1000U;

}

void begin() {}

bool synced() {
  if (nowUtc() < kValidEpoch) return false;
  timex tx = {};
  int state = adjtimex(&tx);
  if (state != TIME_ERROR && !(tx.status & STA_UNSYNC)) return true;
  return millis() > kTrustAfterMs;
}

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
