#pragma once

#include <stddef.h>
#include <stdint.h>

#include "json_arena.h"
#include "reader.h"

namespace rc {

extern const char* const ESPN_SITE_HOST;
extern const char* const ESPN_CORE_HOST;

constexpr int MATCH_EVENTS = 20;
constexpr size_t MATCH_TEAM = 22;
constexpr size_t MATCH_ABBR = 6;
constexpr size_t MATCH_PLAYER = 22;
constexpr int ESPN_PAGE = 10;
constexpr uint32_t MATCH_FAST_ID = 0xF0000000u;

enum MatchKind : uint8_t { MK_GOAL = 0, MK_PEN_GOAL, MK_OWN_GOAL, MK_YELLOW, MK_RED, MK_SUB, MK_NONE };
enum MatchPhase : uint8_t { MP_NONE = 0, MP_PRE, MP_LIVE, MP_HALF, MP_POST };
enum MatchLeague : uint8_t {
  ML_OTHER = 0,
  ML_SUPER_LIG,
  ML_UCL,
  ML_UEL,
  ML_UECL,
  ML_CUP,
  ML_SUPER_CUP,
  ML_FRIENDLY,
  ML_EPL,
  ML_LALIGA,
  ML_SERIE_A,
  ML_BUNDESLIGA,
  ML_LIGUE1,
  ML_COUNT
};

struct MatchEvent {
  uint32_t id;
  uint32_t offAthlete;
  uint16_t seq;
  uint8_t kind;
  uint8_t ours;
  uint8_t home;
  uint8_t away;
  char minute[8];
  char player[MATCH_PLAYER];
  char player2[MATCH_PLAYER];
};

struct MatchSide {
  uint32_t id;
  char name[MATCH_TEAM];
  char abbr[MATCH_ABBR];
};

constexpr int TABLE_ROWS = 3;

struct TableRow {
  uint32_t teamId;
  uint8_t rank;
  uint8_t points;
  uint8_t played;
  uint8_t reserved;
  char name[MATCH_TEAM];
};

struct Table {
  uint8_t valid;
  uint8_t league;
  uint8_t n;
  uint8_t ours;
  uint8_t wins;
  uint8_t ties;
  uint8_t losses;
  uint8_t teams;
  int16_t gd;
  uint16_t reserved;
  uint32_t fetchedUtc;
  TableRow rows[TABLE_ROWS];
};

struct Match {
  uint8_t valid;
  uint8_t phase;
  uint8_t league;
  uint8_t usHome;
  uint8_t homeScore;
  uint8_t awayScore;
  uint8_t n;
  uint8_t live;
  uint16_t seq;
  uint16_t seedSeq;
  uint16_t plays;
  uint16_t reserved;
  uint32_t teamId;
  uint32_t eventId;
  uint32_t kickoffUtc;
  uint32_t endUtc;
  uint32_t fetchedUtc;
  MatchSide home;
  MatchSide away;
  char clock[10];
  char venue[26];
  MatchEvent ev[MATCH_EVENTS];
  Table table;
};

struct MatchStatus {
  uint8_t phase;
  uint8_t period;
  char clock[10];
};

struct EspnPlay {
  uint32_t id;
  uint32_t offAthlete;
  uint32_t teamId;
  uint8_t kind;
  uint8_t home;
  uint8_t away;
  char minute[8];
  char player[MATCH_PLAYER];
};

struct EspnPage {
  uint16_t count;
  uint8_t pageIndex;
  uint8_t pageCount;
  uint8_t n;
  EspnPlay items[ESPN_PAGE];
};

void matchClear(Match& m);
size_t espnSchedulePath(char* out, size_t cap, uint32_t teamId);
size_t espnStatusPath(char* out, size_t cap, uint32_t eventId);
size_t espnPlaysPath(char* out, size_t cap, uint32_t eventId, int page);
size_t espnAthletePath(char* out, size_t cap, uint32_t athleteId);
bool espnParseSchedule(ByteSource& src, JsonArena& arena, uint32_t teamId, uint32_t nowUtc, Match& out);
bool espnParseStatus(ByteSource& src, JsonArena& arena, MatchStatus& out);
bool espnParsePlays(ByteSource& src, JsonArena& arena, EspnPage& out);
bool espnParseAthlete(ByteSource& src, JsonArena& arena, char* out, size_t cap);
int matchApply(Match& m, const EspnPage& page);
MatchEvent* matchFindOff(Match& m);
MatchEvent* matchFindGoal(Match& m, uint8_t home, uint8_t away);
bool matchAddFast(Match& m, uint8_t kind, bool homeSide, uint8_t home, uint8_t away, const char* minute,
                  const char* player);
int matchDropAbove(Match& m, uint8_t home, uint8_t away);
uint8_t espnLeague(const char* slug);
const char* espnLeagueSlug(uint8_t league);
const char* matchLeagueTr(uint8_t league);
void espnTeamTr(const char* name, const char* abbr, MatchSide& out);
bool espnTeamTurkish(const char* name, MatchSide& out);
void espnMinute(const char* display, char* out, size_t cap);
void espnPlayer(const char* text, char* out, size_t cap);

}
