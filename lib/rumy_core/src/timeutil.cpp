#include "timeutil.h"

namespace rc {

int32_t daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  const int32_t era = (y >= 0 ? y : y - 399) / 400;
  const uint32_t yoe = static_cast<uint32_t>(y - era * 400);
  const uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int32_t>(doe) - 719468;
}

CivilDate civilFromDays(int32_t z) {
  z += 719468;
  const int32_t era = (z >= 0 ? z : z - 146096) / 146097;
  const uint32_t doe = static_cast<uint32_t>(z - era * 146097);
  const uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const int32_t y = static_cast<int32_t>(yoe) + era * 400;
  const uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const uint32_t mp = (5 * doy + 2) / 153;
  const uint32_t d = doy - (153 * mp + 2) / 5 + 1;
  const uint32_t m = mp < 10 ? mp + 3 : mp - 9;
  CivilDate c;
  c.year = static_cast<int16_t>(y + (m <= 2));
  c.month = static_cast<uint8_t>(m);
  c.day = static_cast<uint8_t>(d);
  return c;
}

uint8_t weekdayFromDays(int32_t z) {
  return static_cast<uint8_t>(z >= -4 ? (z + 4) % 7 : (z + 5) % 7 + 6);
}

bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

uint8_t daysInMonth(int y, int m) {
  static const uint8_t kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (m < 1 || m > 12) return 30;
  if (m == 2 && isLeap(y)) return 29;
  return kDays[m - 1];
}

uint32_t utcFromCivil(int y, int m, int d, int hh, int mm, int ss) {
  int32_t days = daysFromCivil(y, m, d);
  return static_cast<uint32_t>(days) * 86400u + static_cast<uint32_t>(hh * 3600 + mm * 60 + ss);
}

LocalTime toLocal(uint32_t utc, int32_t offsetSec) {
  int64_t t = static_cast<int64_t>(utc) + offsetSec;
  int32_t days = static_cast<int32_t>(t / 86400);
  int32_t secs = static_cast<int32_t>(t % 86400);
  if (secs < 0) {
    secs += 86400;
    days -= 1;
  }
  CivilDate c = civilFromDays(days);
  LocalTime lt;
  lt.year = c.year;
  lt.month = c.month;
  lt.day = c.day;
  lt.hour = static_cast<uint8_t>(secs / 3600);
  lt.minute = static_cast<uint8_t>((secs % 3600) / 60);
  lt.second = static_cast<uint8_t>(secs % 60);
  lt.wday = weekdayFromDays(days);
  lt.dayNum = days;
  return lt;
}

int32_t localDayNum(uint32_t utc, int32_t offsetSec) { return toLocal(utc, offsetSec).dayNum; }

namespace {

bool digits(const char* s, int n, int& out) {
  int v = 0;
  for (int i = 0; i < n; i++) {
    if (s[i] < '0' || s[i] > '9') return false;
    v = v * 10 + (s[i] - '0');
  }
  out = v;
  return true;
}

}

bool parseIsoDate(const char* s, int& y, int& m, int& d) {
  if (!s) return false;
  if (!digits(s, 4, y) || s[4] != '-' || !digits(s + 5, 2, m) || s[7] != '-' || !digits(s + 8, 2, d)) return false;
  if (m < 1 || m > 12 || d < 1 || d > daysInMonth(y, m)) return false;
  return true;
}

bool parseIsoDateTime(const char* s, uint32_t& utcOut) {
  int y, m, d, hh = 0, mm = 0, ss = 0;
  if (!parseIsoDate(s, y, m, d)) return false;
  if (s[10] == 'T' || s[10] == ' ') {
    if (!digits(s + 11, 2, hh) || s[13] != ':' || !digits(s + 14, 2, mm)) return false;
    if (s[16] == ':' && !digits(s + 17, 2, ss)) return false;
  }
  if (hh > 23 || mm > 59 || ss > 60) return false;
  utcOut = utcFromCivil(y, m, d, hh, mm, ss);
  return true;
}

}
