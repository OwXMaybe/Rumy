#include "leagues.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "app/config.h"
#include "app/shared.h"
#include "app/store.h"
#include "clock/clock.h"
#include "espn_board.h"
#include "net/net.h"
#include "settings/settings.h"
#include "timeutil.h"

namespace leagues {

namespace {

volatile uint8_t s_wantLeague = 0;
volatile uint8_t s_wantKind = rc::VK_NONE;
volatile int8_t s_wantStep = 0;
volatile uint16_t s_wantSeq = 0;

rc::LeagueView s_v;
char s_path[96];
uint16_t s_doneSeq = 0;
uint32_t s_bytes = 0;
uint32_t s_requests = 0;
uint32_t s_errors = 0;
int16_t s_lastCode = 0;

void ask(uint8_t league, uint8_t kind, int8_t dir) {
  s_wantLeague = league;
  s_wantKind = kind;
  s_wantStep = dir;
  s_wantSeq = static_cast<uint16_t>(s_wantSeq + 1);
  net::refresh(SRC_LEAGUE);
}

bool parseDay(rc::ByteSource& src, void*) {
  rc::boardClear(s_v.day);
  return rc::espnParseBoard(src, net::arena(), settings::get().teamId, s_v.league, s_v.day);
}

bool parseTable(rc::ByteSource& src, void*) {
  return rc::espnParseStandings(src, net::arena(), settings::get().teamId, s_v.table);
}

uint32_t ymdOf(int32_t days) {
  rc::CivilDate d = rc::civilFromDays(days);
  return static_cast<uint32_t>(d.year) * 10000u + d.month * 100u + d.day;
}

int32_t daysOf(uint32_t ymd) {
  return rc::daysFromCivil(static_cast<int>(ymd / 10000u), static_cast<int>(ymd / 100u % 100u),
                           static_cast<int>(ymd % 100u));
}

int16_t get(net::ParseFn fn, net::FetchStat& st) {
  net::FetchStat one;
  int16_t code = net::get(rc::ESPN_SITE_HOST, s_path, false, fn, nullptr, one);
  st.bytes += one.bytes;
  st.durMs += one.durMs;
  s_bytes += one.bytes;
  s_requests++;
  s_lastCode = code;
  if (code != 200) s_errors++;
  return code;
}

int16_t fetchDay(int8_t dir, net::FetchStat& st) {
  const char* slug = rc::espnLeagueSlug(s_v.league);
  if (!s_v.ymd) {
    rc::espnBoardPath(s_path, sizeof(s_path), slug, 0);
    int16_t code = get(parseDay, st);
    if (code != 200) return code;
    s_v.ymd = s_v.day.n ? ymdOf(rc::daysFromCivil(1970, 1, 1) + static_cast<int32_t>(s_v.day.games[0].kickoffUtc / 86400u))
                        : ymdOf(clk::todayDayNum());
    return code;
  }
  int tries = dir ? cfg::LEAGUE_SKIP_DAYS : 1;
  uint32_t ymd = s_v.ymd;
  for (int k = 0; k < tries; k++) {
    if (dir) ymd = ymdOf(daysOf(ymd) + dir);
    rc::espnBoardPath(s_path, sizeof(s_path), slug, ymd);
    int16_t code = get(parseDay, st);
    if (code != 200) return code;
    if (s_v.day.n || !dir) {
      s_v.ymd = ymd;
      return code;
    }
  }
  s_v.ymd = ymd;
  return 200;
}

}

void open(uint8_t league) { ask(league, rc::VK_DAY, 0); }

void step(int8_t dir) { ask(s_wantLeague, rc::VK_DAY, dir); }

void showTable(bool table) { ask(s_wantLeague, table ? rc::VK_TABLE : rc::VK_DAY, 0); }

void close() {
  s_wantKind = rc::VK_NONE;
  s_wantSeq = static_cast<uint16_t>(s_wantSeq + 1);
}

uint8_t league() { return s_wantLeague; }

bool tableMode() { return s_wantKind == rc::VK_TABLE; }

uint16_t request() { return s_wantSeq; }

int16_t fetch(net::FetchStat& st) {
  memset(&st, 0, sizeof(st));
  uint16_t seq = s_wantSeq;
  uint8_t kind = s_wantKind;
  uint8_t league = s_wantLeague;
  int8_t dir = seq != s_doneSeq ? s_wantStep : 0;
  if (kind == rc::VK_NONE || !league) {
    s_doneSeq = seq;
    return 0;
  }
  bool switched = league != s_v.league || kind != s_v.kind;
  if (switched) {
    uint32_t keep = league == s_v.league ? s_v.ymd : 0;
    memset(&s_v, 0, sizeof(s_v));
    s_v.league = league;
    s_v.kind = kind;
    s_v.ymd = kind == rc::VK_DAY ? keep : 0;
    s_v.req = seq;
    s_v.state = rc::VS_LOADING;
    store::putT(store::VIEW, s_v);
  }
  int16_t code;
  if (kind == rc::VK_TABLE) {
    rc::espnStandingsPath(s_path, sizeof(s_path), rc::espnLeagueSlug(league));
    code = get(parseTable, st);
  } else {
    code = fetchDay(dir, st);
  }
  s_doneSeq = seq;
  s_v.req = seq;
  s_v.fetchedUtc = clk::synced() ? clk::nowUtc() : 0;
  if (code != 200) s_v.state = rc::VS_ERROR;
  else if (kind == rc::VK_TABLE) s_v.state = s_v.table.n ? rc::VS_OK : rc::VS_EMPTY;
  else s_v.state = s_v.day.n ? rc::VS_OK : rc::VS_EMPTY;
  store::putT(store::VIEW, s_v);
  return code;
}

uint32_t periodS() {
  if (s_wantKind == rc::VK_NONE) return cfg::LEAGUE_IDLE_S;
  if (s_wantSeq != s_doneSeq) return 1;
  if (s_v.kind == rc::VK_TABLE) return cfg::LEAGUE_TABLE_S;
  if (rc::boardCount(s_v.day, rc::MP_LIVE) || rc::boardCount(s_v.day, rc::MP_HALF)) return cfg::BOARD_LIVE_S;
  return cfg::LEAGUE_DAY_S;
}

void report(Print& out) {
  out.printf("[ligsayfa] %s %s · durum %u · gun %lu · satir %u · istek %lu · hata %lu · %lu B · son kod %d\n",
             s_v.league ? rc::espnLeagueSlug(s_v.league) : "-",
             s_v.kind == rc::VK_TABLE ? "puan" : s_v.kind == rc::VK_DAY ? "gun" : "-", static_cast<unsigned>(s_v.state),
             static_cast<unsigned long>(s_v.ymd),
             static_cast<unsigned>(s_v.kind == rc::VK_TABLE ? s_v.table.n : s_v.day.n),
             static_cast<unsigned long>(s_requests), static_cast<unsigned long>(s_errors),
             static_cast<unsigned long>(s_bytes), static_cast<int>(s_lastCode));
}

}
