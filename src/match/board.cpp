#include "board.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "app/config.h"
#include "app/shared.h"
#include "app/store.h"
#include "clock/clock.h"
#include "diag/diag.h"
#include "espn_board.h"
#include "fast.h"
#include "net/net.h"
#include "settings/settings.h"
#include "timeutil.h"

namespace board {

namespace {

rc::Board s_b;
char s_path[128];
int32_t s_day = 0;
uint32_t s_bytes = 0;
uint32_t s_requests = 0;
uint32_t s_errors = 0;
int16_t s_lastCode = 0;
uint16_t s_reported = 0;
uint32_t s_sig[rc::BOARD_GAMES][8];
uint8_t s_fastHome[rc::BOARD_GAMES];
uint8_t s_fastAway[rc::BOARD_GAMES];
uint32_t s_fastAt[rc::BOARD_GAMES];
uint32_t s_fastGoals = 0;

bool parse(rc::ByteSource& src, void* ctx) {
  return rc::espnParseBoard(src, net::arena(), settings::get().teamId, *static_cast<uint8_t*>(ctx), s_b);
}

uint32_t ymd(uint32_t utc) {
  rc::LocalTime lt = rc::toLocal(utc);
  return static_cast<uint32_t>(lt.year) * 10000u + lt.month * 100u + lt.day;
}

uint32_t untilMidnight(uint32_t utc) {
  rc::LocalTime lt = rc::toLocal(utc);
  return 86400u - (lt.hour * 3600u + lt.minute * 60u + lt.second);
}

uint32_t computePeriod(uint32_t now) {
  if (!settings::get().football) return cfg::BOARD_IDLE_S;
  if (!now) return 60;
  if (s_day != clk::todayDayNum()) return 30;
  if (rc::boardCount(s_b, rc::MP_LIVE) || rc::boardCount(s_b, rc::MP_HALF)) return cfg::BOARD_LIVE_S;
  uint32_t wait = untilMidnight(now) + 120u;
  uint32_t kick = rc::boardNextKick(s_b, now);
  if (kick) {
    uint32_t lead = kick > now + cfg::BOARD_LEAD_S ? kick - cfg::BOARD_LEAD_S - now : 0;
    if (lead < wait) wait = lead;
  }
  if (wait < cfg::BOARD_LIVE_S) wait = cfg::BOARD_LIVE_S;
  return wait > cfg::BOARD_IDLE_S ? cfg::BOARD_IDLE_S : wait;
}

void publish() { store::putT(store::BOARD, s_b); }

uint8_t sigOf(uint8_t home, uint8_t away) { return static_cast<uint8_t>(((home & 15u) << 4) | (away & 15u)); }

bool announced(int game, uint8_t home, uint8_t away) {
  uint8_t s = sigOf(home, away);
  return (s_sig[game][s >> 5] >> (s & 31u)) & 1u;
}

void announce(int game, uint8_t home, uint8_t away) {
  uint8_t s = sigOf(home, away);
  s_sig[game][s >> 5] |= 1u << (s & 31u);
}

void forgetAbove(int game, uint8_t home, uint8_t away) {
  for (int h = 0; h < 16; h++) {
    for (int a = 0; a < 16; a++) {
      if (h <= home && a <= away) continue;
      uint8_t s = sigOf(static_cast<uint8_t>(h), static_cast<uint8_t>(a));
      s_sig[game][s >> 5] &= ~(1u << (s & 31u));
    }
  }
}

void clearFast() {
  memset(s_sig, 0, sizeof(s_sig));
  memset(s_fastAt, 0, sizeof(s_fastAt));
}

bool fastFresh(int game) { return s_fastAt[game] && millis() - s_fastAt[game] < cfg::FAST_FRESH_MS; }

void dedupe(uint16_t before) {
  int k = 0;
  for (int i = 0; i < s_b.ng; i++) {
    const rc::BoardGoal& g = s_b.goals[i];
    bool fresh = g.seq > before && g.game < s_b.n;
    if (fresh && announced(g.game, g.home, g.away)) continue;
    if (fresh) announce(g.game, g.home, g.away);
    s_b.goals[k++] = g;
  }
  s_b.ng = static_cast<uint8_t>(k);
  for (int i = 0; i < s_b.n; i++) {
    if (!fastFresh(i)) continue;
    s_b.games[i].homeScore = s_fastHome[i];
    s_b.games[i].awayScore = s_fastAway[i];
  }
}

void pushGoal(int game, const ::fast::Msg& m) {
  rc::BoardGoal g;
  memset(&g, 0, sizeof(g));
  g.seq = ++s_b.seq;
  g.game = static_cast<uint8_t>(game);
  g.kind = m.kind;
  g.home = m.home;
  g.away = m.away;
  g.homeSide = m.homeSide;
  snprintf(g.minute, sizeof(g.minute), "%s", m.minute);
  snprintf(g.player, sizeof(g.player), "%s", m.player);
  if (s_b.ng >= rc::BOARD_GOALS) {
    memmove(&s_b.goals[0], &s_b.goals[1], sizeof(s_b.goals[0]) * (rc::BOARD_GOALS - 1));
    s_b.ng = rc::BOARD_GOALS - 1;
  }
  s_b.goals[s_b.ng++] = g;
}

}

int16_t fetch(net::FetchStat& st) {
  memset(&st, 0, sizeof(st));
  if (!settings::get().football) {
    if (s_b.n || s_b.ng) {
      rc::boardClear(s_b);
      clearFast();
      publish();
    }
    return 0;
  }
  if (!clk::synced()) return 0;
  uint32_t now = clk::nowUtc();
  int32_t today = clk::todayDayNum();
  if (s_day != today) {
    rc::boardClear(s_b);
    clearFast();
    s_day = today;
    s_reported = 0;
    publish();
  }
  uint16_t before = s_b.seq;
  int16_t code = 200;
  bool any = false;
  for (const char* slug : cfg::BOARD_LEAGUES) {
    uint8_t league = rc::espnLeague(slug);
    net::FetchStat one;
    rc::espnBoardPath(s_path, sizeof(s_path), slug, ymd(now));
    int16_t c = net::get(rc::ESPN_SITE_HOST, s_path, false, parse, &league, one);
    st.bytes += one.bytes;
    st.durMs += one.durMs;
    s_bytes += one.bytes;
    s_requests++;
    s_lastCode = c;
    if (c == 200) {
      any = true;
    } else {
      s_errors++;
      code = c;
    }
  }
  st.code = code;
  if (!any) return code;
  s_b.day = static_cast<uint32_t>(today);
  s_b.fetchedUtc = now;
  dedupe(before);
  for (int i = 0; i < s_b.ng; i++) {
    const rc::BoardGoal& g = s_b.goals[i];
    if (g.seq <= s_reported || g.game >= s_b.n) continue;
    const rc::BoardGame& m = s_b.games[g.game];
    diag::note("lig", "gol %s %s: %s %u-%u %s", g.minute, g.player, m.home, static_cast<unsigned>(g.home),
               static_cast<unsigned>(g.away), m.away);
  }
  s_reported = s_b.seq;
  publish();
  return code;
}

uint32_t periodS() { return computePeriod(clk::synced() ? clk::nowUtc() : 0); }

void refresh() { net::refresh(SRC_BOARD); }

void fastMsg(const ::fast::Msg& m) {
  int game = -1;
  for (int i = 0; i < s_b.n; i++) {
    if (s_b.games[i].id == m.espnId) game = i;
  }
  if (game < 0) return;
  rc::BoardGame& g = s_b.games[game];
  if (m.home < g.homeScore || m.away < g.awayScore) forgetAbove(game, m.home, m.away);
  if (m.type == ::fast::T_GOAL && m.player[0]) {
    for (int i = 0; i < s_b.ng; i++) {
      rc::BoardGoal& old = s_b.goals[i];
      if (old.game == game && old.home == m.home && old.away == m.away && !old.player[0])
        snprintf(old.player, sizeof(old.player), "%s", m.player);
    }
  }
  if (m.type == ::fast::T_GOAL && !announced(game, m.home, m.away) && !(g.flags & rc::BF_OURS)) {
    announce(game, m.home, m.away);
    pushGoal(game, m);
    s_fastGoals++;
    diag::note("lig", "hizli gol: %s %s: %s %u-%u %s", m.minute, m.player, g.home, static_cast<unsigned>(m.home),
               static_cast<unsigned>(m.away), g.away);
  }
  s_fastHome[game] = m.home;
  s_fastAway[game] = m.away;
  s_fastAt[game] = millis() | 1u;
  g.homeScore = m.home;
  g.awayScore = m.away;
  if (m.type == ::fast::T_GOAL && g.goals < m.home + m.away) g.goals = static_cast<uint8_t>(m.home + m.away);
  s_reported = s_b.seq;
  publish();
}

void report(Print& out) {
  const rc::Board& b = s_b;
  uint32_t now = clk::synced() ? clk::nowUtc() : 0;
  out.printf("[lig] %s · mac %u · canli %d · bitti %d · gol %u · istek %lu · hata %lu · %lu B · son kod %d · sonraki %lu sn\n",
             b.valid ? "hazir" : "yok", static_cast<unsigned>(b.n),
             rc::boardCount(b, rc::MP_LIVE) + rc::boardCount(b, rc::MP_HALF), rc::boardCount(b, rc::MP_POST),
             static_cast<unsigned>(b.seq), static_cast<unsigned long>(s_requests),
             static_cast<unsigned long>(s_errors), static_cast<unsigned long>(s_bytes), static_cast<int>(s_lastCode),
             static_cast<unsigned long>(computePeriod(now)));
  out.printf("[lig] hizli gol %lu\n", static_cast<unsigned long>(s_fastGoals));
  for (int i = 0; i < b.n; i++) {
    const rc::BoardGame& g = b.games[i];
    rc::LocalTime lt = rc::toLocal(g.kickoffUtc);
    out.printf("  %s %02u:%02u %-5s %s %u-%u %s\n", g.league == rc::ML_SUPER_LIG ? "SüperLig" : "ŞL",
               static_cast<unsigned>(lt.hour), static_cast<unsigned>(lt.minute),
               g.phase == rc::MP_PRE ? "-" : g.phase == rc::MP_POST ? "MS" : g.phase == rc::MP_HALF ? "IY" : g.clock,
               g.home, static_cast<unsigned>(g.homeScore), static_cast<unsigned>(g.awayScore), g.away);
  }
}

}
