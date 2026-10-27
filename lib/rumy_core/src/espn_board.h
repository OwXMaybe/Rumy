#pragma once

#include <stddef.h>
#include <stdint.h>

#include "espn.h"
#include "json_arena.h"
#include "reader.h"

namespace rc {

constexpr int BOARD_GAMES = 18;
constexpr int BOARD_GOALS = 8;
constexpr size_t BOARD_NAME = 18;

enum BoardFlag : uint8_t { BF_HOME_TR = 1, BF_AWAY_TR = 2, BF_OURS = 4 };

struct BoardGame {
  uint32_t id;
  uint32_t kickoffUtc;
  uint32_t homeId;
  uint32_t awayId;
  uint8_t phase;
  uint8_t homeScore;
  uint8_t awayScore;
  uint8_t goals;
  uint8_t flags;
  uint8_t league;
  uint8_t reserved[2];
  char clock[8];
  char home[BOARD_NAME];
  char away[BOARD_NAME];
  char homeAbbr[MATCH_ABBR];
  char awayAbbr[MATCH_ABBR];
};

struct BoardGoal {
  uint16_t seq;
  uint8_t game;
  uint8_t kind;
  uint8_t home;
  uint8_t away;
  uint8_t homeSide;
  uint8_t reserved;
  char minute[8];
  char player[MATCH_PLAYER];
};

struct Board {
  uint8_t valid;
  uint8_t seeded;
  uint8_t n;
  uint8_t ng;
  uint16_t seq;
  uint16_t reserved;
  uint32_t day;
  uint32_t fetchedUtc;
  BoardGame games[BOARD_GAMES];
  BoardGoal goals[BOARD_GOALS];
};

constexpr int STAND_ROWS = 36;
constexpr int STAND_ZONES = 6;
constexpr size_t STAND_ZONE_TEXT = 28;

struct StandRow {
  uint32_t teamId;
  uint8_t rank;
  uint8_t played;
  uint8_t wins;
  uint8_t ties;
  uint8_t losses;
  uint8_t points;
  uint8_t zone;
  uint8_t flags;
  int16_t gd;
  char name[BOARD_NAME];
};

struct StandZone {
  uint32_t color;
  char text[STAND_ZONE_TEXT];
};

struct Standings {
  uint8_t n;
  uint8_t nz;
  uint16_t reserved;
  StandRow rows[STAND_ROWS];
  StandZone zones[STAND_ZONES];
};

enum ViewKind : uint8_t { VK_NONE = 0, VK_DAY, VK_TABLE };
enum ViewState : uint8_t { VS_LOADING = 0, VS_OK, VS_EMPTY, VS_ERROR };

struct LeagueView {
  uint8_t kind;
  uint8_t league;
  uint8_t state;
  uint8_t reserved;
  uint16_t req;
  uint16_t reserved2;
  uint32_t ymd;
  uint32_t fetchedUtc;
  union {
    Board day;
    Standings table;
  };
};

void boardClear(Board& b);
size_t espnBoardPath(char* out, size_t cap, const char* slug, uint32_t yyyymmdd);
bool espnParseBoard(ByteSource& src, JsonArena& arena, uint32_t ourTeam, uint8_t league, Board& io);
int boardCount(const Board& b, uint8_t phase);
uint32_t boardNextKick(const Board& b, uint32_t nowUtc);
bool boardTurkish(const BoardGame& g);
size_t espnStandingsPath(char* out, size_t cap, const char* slug);
bool espnParseStandings(ByteSource& src, JsonArena& arena, uint32_t ourTeam, Standings& out);
void espnZoneTr(const char* description, char* out, size_t cap);

}
