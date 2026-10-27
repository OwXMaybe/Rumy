#pragma once

#include <stdint.h>

namespace rc {

constexpr int32_t TZ_OFFSET_SEC = 3 * 3600;

struct CivilDate {
  int16_t year;
  uint8_t month;
  uint8_t day;
};

struct LocalTime {
  int16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t wday;
  int32_t dayNum;
};

int32_t daysFromCivil(int y, int m, int d);
CivilDate civilFromDays(int32_t z);
uint8_t weekdayFromDays(int32_t z);
bool isLeap(int y);
uint8_t daysInMonth(int y, int m);
uint32_t utcFromCivil(int y, int m, int d, int hh, int mm, int ss);
LocalTime toLocal(uint32_t utc, int32_t offsetSec = TZ_OFFSET_SEC);
int32_t localDayNum(uint32_t utc, int32_t offsetSec = TZ_OFFSET_SEC);
bool parseIsoDateTime(const char* s, uint32_t& utcOut);
bool parseIsoDate(const char* s, int& y, int& m, int& d);

}
