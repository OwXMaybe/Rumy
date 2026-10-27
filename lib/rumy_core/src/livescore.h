#pragma once

#include <stddef.h>
#include <stdint.h>

#include "espn.h"
#include "json_arena.h"
#include "reader.h"

namespace rc {

extern const char* const LS_HOST;

constexpr int LS_GOALS = 16;
constexpr size_t LS_TEAM = 32;

struct LsGoal {
  uint8_t home;
  uint8_t away;
  uint8_t homeSide;
  uint8_t kind;
  char minute[8];
  char player[MATCH_PLAYER];
};

struct LsMatch {
  uint32_t eid;
  uint8_t valid;
  uint8_t home;
  uint8_t away;
  uint8_t n;
  LsGoal goals[LS_GOALS];
};

struct LsEvent {
  uint32_t eid;
  uint8_t swapped;
  uint8_t quality;
  uint8_t home;
  uint8_t away;
};

size_t lsIncidentsPath(char* out, size_t cap, uint32_t eid);
size_t lsLivePath(char* out, size_t cap);
bool lsParseIncidents(ByteSource& src, JsonArena& arena, LsMatch& out);
bool lsFindLive(ByteSource& src, JsonArena& arena, const char* home, const char* away, LsEvent& out);
uint8_t lsTeamMatch(const char* a, const char* b);
void lsFold(const char* in, char* out, size_t cap);

}
