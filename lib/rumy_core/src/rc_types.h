#pragma once

#include <stddef.h>
#include <stdint.h>

namespace rc {

constexpr int16_t NA16 = -32768;
constexpr uint8_t NA8 = 0xFF;
constexpr uint16_t NAU16 = 0xFFFF;

constexpr int HOURS = 24;
constexpr int DAYS = 7;
constexpr int MINUTELY = 4;

struct WxNow {
  uint32_t time;
  int16_t temp10;
  int16_t feels10;
  uint16_t press10;
  uint16_t wind10;
  uint16_t gust10;
  uint16_t windDir;
  uint16_t visDam;
  uint16_t precip100;
  uint8_t rh;
  uint8_t code;
  uint8_t isDay;
  uint8_t uv10;
};

struct WxHour {
  int16_t temp10;
  uint8_t code;
  uint8_t pop;
  uint8_t isDay;
  uint8_t reserved;
};

struct WxMinutely {
  uint32_t start;
  uint16_t precip100[MINUTELY];
  uint8_t count;
  uint8_t reserved[3];
};

struct Forecast {
  uint32_t fetchedUtc;
  WxNow now;
  WxMinutely m15;
  uint32_t hourStart;
  uint8_t hourCount;
  uint8_t valid;
  uint8_t reserved[2];
  WxHour hours[HOURS];
};

struct WxDay {
  uint32_t date;
  uint32_t sunrise;
  uint32_t sunset;
  int16_t tmax10;
  int16_t tmin10;
  uint8_t code;
  uint8_t popMax;
  uint8_t uvMax10;
  uint8_t reserved;
};

struct Daily {
  uint32_t fetchedUtc;
  uint8_t count;
  uint8_t valid;
  uint8_t reserved[2];
  WxDay days[DAYS];
};

struct AirQ {
  uint32_t fetchedUtc;
  uint32_t time;
  uint16_t eaqi;
  uint16_t pm25x10;
  uint16_t pm10x10;
  uint8_t valid;
  uint8_t reserved;
};

struct Marine {
  uint32_t fetchedUtc;
  uint32_t time;
  int16_t sst10;
  uint8_t valid;
  uint8_t reserved;
};

struct Quake {
  uint32_t timeUtc;
  int16_t mag10;
  int16_t depth10;
  int32_t lat1e4;
  int32_t lon1e4;
  char place[56];
};

enum QuakeSource : uint8_t {
  QS_AFAD = 0,
  QS_KANDILLI = 1,
};

struct QuakeInfo {
  uint32_t fetchedUtc;
  uint8_t valid;
  uint8_t count24h;
  uint8_t source;
  uint8_t reserved;
  Quake latest;
  Quake strongest;
};

struct Religious {
  uint16_t year;
  uint8_t ramazanMonth;
  uint8_t ramazanDay;
  uint8_t kurbanMonth;
  uint8_t kurbanDay;
  uint8_t valid;
  uint8_t source;
};

enum EventKind : uint8_t {
  EV_NONE = 0,
  EV_NATIONAL = 1,
  EV_COMMEMORATION = 2,
  EV_RELIGIOUS = 3,
  EV_NEWYEAR = 4,
  EV_LABOUR = 5,
  EV_BIRTHDAY = 6,
  EV_ANNIVERSARY = 7,
  EV_OTHER = 8,
};

enum Accessory : uint8_t {
  ACC_NONE = 0,
  ACC_FLAG = 1,
  ACC_PARTY = 2,
  ACC_UMBRELLA = 3,
};

struct PersonalEvent {
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t kind;
  uint8_t reserved[3];
  char name[40];
};

struct EventOcc {
  int32_t dayNum;
  uint16_t year;
  uint16_t ordinal;
  uint8_t month;
  uint8_t day;
  uint8_t kind;
  uint8_t accessory;
  char name[48];
  char dative[56];
};

}
