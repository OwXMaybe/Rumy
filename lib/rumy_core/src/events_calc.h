#pragma once

#include <stddef.h>
#include <stdint.h>

#include "json_arena.h"
#include "rc_types.h"
#include "reader.h"

namespace rc {

constexpr int MAX_PERSONAL = 24;
constexpr int MAX_UPCOMING = 6;
constexpr int EVENT_HORIZON_DAYS = 30;

enum CountdownTier : uint8_t {
  TIER_NONE = 0,
  TIER_RARE = 1,
  TIER_FREQUENT = 2,
  TIER_TODAY = 3,
};

struct EventsSnapshot {
  int32_t computedDay;
  uint8_t count;
  uint8_t todayCount;
  uint8_t reserved[2];
  EventOcc items[MAX_UPCOMING];
};

int collectEvents(const Religious* rel, int relCount, const PersonalEvent* personal, int personalCount,
                  int32_t todayDayNum, int horizonDays, EventOcc* out, int cap);
void buildSnapshot(const Religious* rel, int relCount, const PersonalEvent* personal, int personalCount,
                   int32_t todayDayNum, EventsSnapshot& out);

uint8_t countdownTier(int days);
size_t eventCountdownText(const EventOcc& e, int32_t todayDayNum, char* out, size_t cap);
size_t eventGreeting(const EventOcc& e, char* out, size_t cap);
size_t eventSubtitle(const EventOcc& e, char* out, size_t cap);

uint8_t personalKindFromText(const char* s);
const char* personalKindText(uint8_t kind);
bool personalParse(ByteSource& src, JsonArena& arena, PersonalEvent* out, int cap, int& count);
size_t personalSerialize(const PersonalEvent* items, int count, char* out, size_t cap);
size_t jsonEscape(const char* in, char* out, size_t cap);

}
