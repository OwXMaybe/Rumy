#include <string.h>
#include <unity.h>

#include "../fixtures/fixtures.h"
#include "espn.h"
#include "espn_board.h"
#include "timeutil.h"

static uint8_t g_mem[8192];
static rc::JsonArena g_arena(g_mem, sizeof(g_mem));
static rc::Match g_match;
static rc::EspnPage g_page;

void setUp() {}
void tearDown() {}

static void test_paths() {
  char p[160];
  TEST_ASSERT_TRUE(rc::espnSchedulePath(p, sizeof(p), 436) > 0);
  TEST_ASSERT_EQUAL_STRING("/apis/site/v2/sports/soccer/all/teams/436/schedule?fixture=true", p);
  TEST_ASSERT_TRUE(rc::espnStatusPath(p, sizeof(p), 401888301) > 0);
  TEST_ASSERT_EQUAL_STRING("/v2/sports/soccer/leagues/all/events/401888301/competitions/401888301/status", p);
  TEST_ASSERT_TRUE(rc::espnPlaysPath(p, sizeof(p), 401888301, 2) > 0);
  TEST_ASSERT_EQUAL_STRING(
      "/v2/sports/soccer/leagues/all/events/401888301/competitions/401888301/plays?limit=10&page=2", p);
  TEST_ASSERT_TRUE(rc::espnAthletePath(p, sizeof(p), 211923) > 0);
  TEST_ASSERT_EQUAL_STRING("/v2/sports/soccer/athletes/211923", p);
  TEST_ASSERT_EQUAL(0, rc::espnPlaysPath(p, 20, 401888301, 1));
}

static void test_schedule_picks_next_match() {
  rc::MemSource src(FX_ESPN_SCHEDULE, strlen(FX_ESPN_SCHEDULE));
  TEST_ASSERT_TRUE(rc::espnParseSchedule(src, g_arena, 436, rc::utcFromCivil(2026, 10, 3, 12, 0, 0), g_match));
  TEST_ASSERT_EQUAL(1, g_match.valid);
  TEST_ASSERT_EQUAL_UINT32(401888282u, g_match.eventId);
  TEST_ASSERT_EQUAL_UINT32(rc::utcFromCivil(2026, 10, 10, 16, 0, 0), g_match.kickoffUtc);
  TEST_ASSERT_EQUAL(rc::ML_SUPER_LIG, g_match.league);
  TEST_ASSERT_EQUAL(0, g_match.usHome);
  TEST_ASSERT_EQUAL(rc::MP_PRE, g_match.phase);
  TEST_ASSERT_EQUAL_STRING("Çaykur Rizespor", g_match.home.name);
  TEST_ASSERT_EQUAL_STRING("RİZ", g_match.home.abbr);
  TEST_ASSERT_EQUAL_STRING("Fenerbahçe", g_match.away.name);
  TEST_ASSERT_EQUAL_STRING("FB", g_match.away.abbr);
  TEST_ASSERT_EQUAL_UINT32(436u, g_match.away.id);
  TEST_ASSERT_EQUAL_UINT32(436u, g_match.teamId);
}

static void test_schedule_skips_past_and_maps_europe() {
  rc::MemSource src(FX_ESPN_SCHEDULE, strlen(FX_ESPN_SCHEDULE));
  TEST_ASSERT_TRUE(rc::espnParseSchedule(src, g_arena, 436, rc::utcFromCivil(2026, 10, 12, 9, 0, 0), g_match));
  TEST_ASSERT_EQUAL_UINT32(401915409u, g_match.eventId);
  TEST_ASSERT_EQUAL(rc::ML_UCL, g_match.league);
  TEST_ASSERT_EQUAL_STRING("Aston Villa", g_match.home.name);
  TEST_ASSERT_EQUAL_STRING("AVL", g_match.home.abbr);
  TEST_ASSERT_EQUAL_STRING("Şampiyonlar Ligi", rc::matchLeagueTr(g_match.league));
  rc::MemSource src2(FX_ESPN_SCHEDULE, strlen(FX_ESPN_SCHEDULE));
  TEST_ASSERT_TRUE(rc::espnParseSchedule(src2, g_arena, 999999, rc::utcFromCivil(2026, 10, 3, 12, 0, 0), g_match));
  TEST_ASSERT_EQUAL(0, g_match.valid);
}

static void test_status() {
  rc::MatchStatus st;
  rc::MemSource src(FX_ESPN_STATUS, strlen(FX_ESPN_STATUS));
  TEST_ASSERT_TRUE(rc::espnParseStatus(src, g_arena, st));
  TEST_ASSERT_EQUAL(rc::MP_POST, st.phase);
  TEST_ASSERT_EQUAL(2, st.period);
  TEST_ASSERT_EQUAL_STRING("90+8'", st.clock);
  const char* live =
      "{\"clock\":2400.0,\"displayClock\":\"40'\",\"period\":1,\"type\":{\"id\":\"2\",\"name\":\"STATUS_FIRST_HALF\","
      "\"state\":\"in\",\"completed\":false}}";
  rc::MemSource s2(live, strlen(live));
  TEST_ASSERT_TRUE(rc::espnParseStatus(s2, g_arena, st));
  TEST_ASSERT_EQUAL(rc::MP_LIVE, st.phase);
  TEST_ASSERT_EQUAL_STRING("40'", st.clock);
  const char* half = "{\"displayClock\":\"45'\",\"period\":1,\"type\":{\"name\":\"STATUS_HALFTIME\",\"state\":\"in\"}}";
  rc::MemSource s3(half, strlen(half));
  TEST_ASSERT_TRUE(rc::espnParseStatus(s3, g_arena, st));
  TEST_ASSERT_EQUAL(rc::MP_HALF, st.phase);
}

static void fenerbahceHome() {
  rc::matchClear(g_match);
  g_match.valid = 1;
  g_match.teamId = 436;
  g_match.usHome = 1;
}

static void test_plays_page_and_apply() {
  fenerbahceHome();
  rc::MemSource p1(FX_ESPN_PLAYS_P1, strlen(FX_ESPN_PLAYS_P1));
  TEST_ASSERT_TRUE(rc::espnParsePlays(p1, g_arena, g_page));
  TEST_ASSERT_EQUAL(19, g_page.count);
  TEST_ASSERT_EQUAL(1, g_page.pageIndex);
  TEST_ASSERT_EQUAL(2, g_page.pageCount);
  TEST_ASSERT_EQUAL(10, g_page.n);
  TEST_ASSERT_EQUAL(10, rc::matchApply(g_match, g_page));
  TEST_ASSERT_EQUAL(10, g_match.plays);
  const rc::MatchEvent& g1 = g_match.ev[0];
  TEST_ASSERT_EQUAL(rc::MK_GOAL, g1.kind);
  TEST_ASSERT_EQUAL_STRING("Milan Skriniar", g1.player);
  TEST_ASSERT_EQUAL_STRING("26'", g1.minute);
  TEST_ASSERT_EQUAL(1, g1.ours);
  TEST_ASSERT_EQUAL(1, g1.home);
  TEST_ASSERT_EQUAL(0, g1.away);
  TEST_ASSERT_EQUAL(rc::MK_GOAL, g_match.ev[1].kind);
  TEST_ASSERT_EQUAL(0, g_match.ev[1].ours);
  TEST_ASSERT_EQUAL(rc::MK_YELLOW, g_match.ev[3].kind);
  TEST_ASSERT_EQUAL_STRING("45+1'", g_match.ev[3].minute);
  TEST_ASSERT_EQUAL(1, g_match.ev[3].ours);
  const rc::MatchEvent& sub = g_match.ev[4];
  TEST_ASSERT_EQUAL(rc::MK_SUB, sub.kind);
  TEST_ASSERT_EQUAL(1, sub.ours);
  TEST_ASSERT_EQUAL_UINT32(211923u, sub.offAthlete);
  TEST_ASSERT_EQUAL_PTR(&g_match.ev[4], rc::matchFindOff(g_match));

  rc::MemSource p2(FX_ESPN_PLAYS_P2, strlen(FX_ESPN_PLAYS_P2));
  TEST_ASSERT_TRUE(rc::espnParsePlays(p2, g_arena, g_page));
  TEST_ASSERT_EQUAL(2, g_page.pageIndex);
  TEST_ASSERT_EQUAL(9, rc::matchApply(g_match, g_page));
  TEST_ASSERT_EQUAL(19, g_match.n);
  TEST_ASSERT_EQUAL(19, g_match.seq);
  TEST_ASSERT_EQUAL(1, g_match.homeScore);
  TEST_ASSERT_EQUAL(2, g_match.awayScore);
  rc::MemSource again(FX_ESPN_PLAYS_P2, strlen(FX_ESPN_PLAYS_P2));
  TEST_ASSERT_TRUE(rc::espnParsePlays(again, g_arena, g_page));
  TEST_ASSERT_EQUAL(0, rc::matchApply(g_match, g_page));
  int goals = 0;
  for (int i = 0; i < g_match.n; i++)
    if (g_match.ev[i].kind == rc::MK_GOAL) goals++;
  TEST_ASSERT_EQUAL(3, goals);
}

static void test_away_goal_is_ours_when_away() {
  rc::matchClear(g_match);
  g_match.teamId = 1895;
  g_match.usHome = 0;
  rc::MemSource p1(FX_ESPN_PLAYS_P1, strlen(FX_ESPN_PLAYS_P1));
  TEST_ASSERT_TRUE(rc::espnParsePlays(p1, g_arena, g_page));
  rc::matchApply(g_match, g_page);
  TEST_ASSERT_EQUAL(0, g_match.ev[0].ours);
  TEST_ASSERT_EQUAL(1, g_match.ev[1].ours);
  TEST_ASSERT_EQUAL(1, g_match.ev[2].ours);
}

static void test_keeps_latest_events() {
  fenerbahceHome();
  memset(&g_page, 0, sizeof(g_page));
  g_page.pageIndex = 1;
  g_page.n = rc::ESPN_PAGE;
  for (int round = 0; round < 3; round++) {
    g_page.pageIndex = static_cast<uint8_t>(round + 1);
    for (int i = 0; i < rc::ESPN_PAGE; i++) {
      rc::EspnPlay& p = g_page.items[i];
      memset(&p, 0, sizeof(p));
      p.id = static_cast<uint32_t>(1000 + round * rc::ESPN_PAGE + i);
      p.kind = rc::MK_YELLOW;
      p.teamId = 436;
    }
    rc::matchApply(g_match, g_page);
  }
  TEST_ASSERT_EQUAL(rc::MATCH_EVENTS, g_match.n);
  TEST_ASSERT_EQUAL_UINT32(1010u, g_match.ev[0].id);
  TEST_ASSERT_EQUAL_UINT32(1029u, g_match.ev[rc::MATCH_EVENTS - 1].id);
  TEST_ASSERT_EQUAL(30, g_match.seq);
}

static void test_athlete_and_text_helpers() {
  char name[rc::MATCH_PLAYER];
  rc::MemSource src(FX_ESPN_ATHLETE, strlen(FX_ESPN_ATHLETE));
  TEST_ASSERT_TRUE(rc::espnParseAthlete(src, g_arena, name, sizeof(name)));
  TEST_ASSERT_EQUAL_STRING("Irfan Can Kahveci", name);
  char out[16];
  rc::espnMinute("90'+8'", out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("90+8'", out);
  rc::espnPlayer("Kerem Aktürkoglu (Fenerbahce) Substitution at 45'", name, sizeof(name));
  TEST_ASSERT_EQUAL_STRING("Kerem Aktürkoglu", name);
  rc::espnPlayer("Abcdefghij Klmnopqrstuvwxyz (X) Goal", out, 8);
  TEST_ASSERT_EQUAL_STRING("Abcdefg", out);
  rc::espnPlayer("Yi\xCC\x87\xC4\x9Ei\xCC\x87T \xC3\x87olak (Fenerbahce) Goal at 70'", name, sizeof(name));
  TEST_ASSERT_EQUAL_STRING("Yi\xC4\x9F" "it \xC3\x87olak", name);
  rc::espnPlayer("N'Golo Kant\xC3\xA9 (Fenerbahce) Goal", name, sizeof(name));
  TEST_ASSERT_EQUAL_STRING("N'Golo Kant\xC3\xA9", name);
  TEST_ASSERT_EQUAL(rc::ML_UEL, rc::espnLeague("uefa.europa"));
  TEST_ASSERT_EQUAL(rc::ML_UECL, rc::espnLeague("uefa.europa.conf"));
  TEST_ASSERT_EQUAL(rc::ML_CUP, rc::espnLeague("tur.cup"));
  TEST_ASSERT_EQUAL(rc::ML_OTHER, rc::espnLeague("ned.1"));
  rc::MatchSide s;
  rc::espnTeamTr("Besiktas", "BES", s);
  TEST_ASSERT_EQUAL_STRING("Beşiktaş", s.name);
  TEST_ASSERT_EQUAL_STRING("BJK", s.abbr);
  rc::espnTeamTr("Liverpool", "LIV", s);
  TEST_ASSERT_EQUAL_STRING("Liverpool", s.name);
  TEST_ASSERT_EQUAL_STRING("LIV", s.abbr);
}

static rc::Board g_board;
static uint8_t g_boardMem[16384];
static rc::JsonArena g_boardArena(g_boardMem, sizeof(g_boardMem));

static void test_board_path_and_seed() {
  char p[160];
  TEST_ASSERT_TRUE(rc::espnBoardPath(p, sizeof(p), "uefa.champions", 20261013) > 0);
  TEST_ASSERT_EQUAL_STRING("/apis/site/v2/sports/soccer/uefa.champions/scoreboard?dates=20261013", p);
  rc::boardClear(g_board);
  rc::MemSource src(FX_ESPN_BOARD_LIVE, strlen(FX_ESPN_BOARD_LIVE));
  TEST_ASSERT_TRUE(rc::espnParseBoard(src, g_boardArena, 436, rc::ML_UCL, g_board));
  TEST_ASSERT_EQUAL(1, g_board.valid);
  TEST_ASSERT_EQUAL(3, g_board.n);
  TEST_ASSERT_EQUAL(0, g_board.ng);
  TEST_ASSERT_EQUAL(0, g_board.seq);
  const rc::BoardGame& fb = g_board.games[0];
  TEST_ASSERT_EQUAL_UINT32(401915444u, fb.id);
  TEST_ASSERT_EQUAL_STRING("Fenerbahçe", fb.home);
  TEST_ASSERT_EQUAL_STRING("FB", fb.homeAbbr);
  TEST_ASSERT_EQUAL_STRING("AS Roma", fb.away);
  TEST_ASSERT_EQUAL(rc::BF_HOME_TR | rc::BF_OURS, fb.flags);
  TEST_ASSERT_TRUE(rc::boardTurkish(fb));
  TEST_ASSERT_EQUAL(rc::MP_POST, fb.phase);
  TEST_ASSERT_EQUAL(1, fb.homeScore);
  TEST_ASSERT_EQUAL(1, fb.awayScore);
  const rc::BoardGame& como = g_board.games[1];
  TEST_ASSERT_EQUAL_STRING("Como", como.home);
  TEST_ASSERT_EQUAL_STRING("RB Leipzig", como.away);
  TEST_ASSERT_FALSE(rc::boardTurkish(como));
  TEST_ASSERT_EQUAL(rc::MP_LIVE, como.phase);
  TEST_ASSERT_EQUAL_STRING("58'", como.clock);
  TEST_ASSERT_EQUAL(2, como.homeScore);
  TEST_ASSERT_EQUAL(0, como.awayScore);
  TEST_ASSERT_EQUAL(2, como.goals);
  TEST_ASSERT_EQUAL_UINT32(rc::utcFromCivil(2026, 9, 10, 19, 0, 0), como.kickoffUtc);
  TEST_ASSERT_EQUAL(2, rc::boardCount(g_board, rc::MP_LIVE));
  TEST_ASSERT_EQUAL(1, rc::boardCount(g_board, rc::MP_POST));
}

static void test_board_new_goals() {
  rc::MemSource src(FX_ESPN_BOARD, strlen(FX_ESPN_BOARD));
  TEST_ASSERT_TRUE(rc::espnParseBoard(src, g_boardArena, 436, rc::ML_UCL, g_board));
  TEST_ASSERT_EQUAL(3, g_board.n);
  TEST_ASSERT_EQUAL(6, g_board.ng);
  TEST_ASSERT_EQUAL(6, g_board.seq);
  const rc::BoardGoal& g0 = g_board.goals[0];
  TEST_ASSERT_EQUAL(1, g0.game);
  TEST_ASSERT_EQUAL(1, g0.seq);
  TEST_ASSERT_EQUAL(rc::MK_GOAL, g0.kind);
  TEST_ASSERT_EQUAL_STRING("54'", g0.minute);
  TEST_ASSERT_EQUAL_STRING("Assane Diao", g0.player);
  TEST_ASSERT_EQUAL(1, g0.homeSide);
  TEST_ASSERT_EQUAL(3, g0.home);
  TEST_ASSERT_EQUAL(0, g0.away);
  TEST_ASSERT_EQUAL(0, g_board.goals[1].homeSide);
  TEST_ASSERT_EQUAL(3, g_board.goals[1].home);
  TEST_ASSERT_EQUAL(1, g_board.goals[1].away);
  TEST_ASSERT_EQUAL(4, g_board.goals[2].home);
  TEST_ASSERT_EQUAL(1, g_board.goals[2].away);
  const rc::BoardGoal& g5 = g_board.goals[5];
  TEST_ASSERT_EQUAL(2, g5.game);
  TEST_ASSERT_EQUAL_STRING("90+3'", g5.minute);
  TEST_ASSERT_EQUAL_STRING("Ruben Aguilar", g5.player);
  TEST_ASSERT_EQUAL(2, g5.home);
  TEST_ASSERT_EQUAL(3, g5.away);
  TEST_ASSERT_EQUAL(rc::MP_POST, g_board.games[2].phase);
  rc::MemSource again(FX_ESPN_BOARD, strlen(FX_ESPN_BOARD));
  TEST_ASSERT_TRUE(rc::espnParseBoard(again, g_boardArena, 436, rc::ML_UCL, g_board));
  TEST_ASSERT_EQUAL(6, g_board.seq);
  rc::MemSource back(FX_ESPN_BOARD_LIVE, strlen(FX_ESPN_BOARD_LIVE));
  TEST_ASSERT_TRUE(rc::espnParseBoard(back, g_boardArena, 436, rc::ML_UCL, g_board));
  TEST_ASSERT_EQUAL(6, g_board.seq);
  TEST_ASSERT_EQUAL(2, g_board.games[1].goals);
  TEST_ASSERT_EQUAL(0u, rc::boardNextKick(g_board, rc::utcFromCivil(2026, 9, 10, 12, 0, 0)));
}

static void test_board_deep_prematch() {
  rc::Board b;
  rc::boardClear(b);
  rc::MemSource src(FX_ESPN_BOARD_PRE, strlen(FX_ESPN_BOARD_PRE));
  TEST_ASSERT_TRUE(rc::espnParseBoard(src, g_boardArena, 436, rc::ML_UCL, b));
  TEST_ASSERT_EQUAL(1, b.n);
  TEST_ASSERT_EQUAL_STRING("Galatasaray", b.games[0].home);
  TEST_ASSERT_EQUAL_STRING("Barcelona", b.games[0].away);
  TEST_ASSERT_EQUAL(rc::MP_PRE, b.games[0].phase);
  TEST_ASSERT_EQUAL(rc::BF_HOME_TR, b.games[0].flags);
  TEST_ASSERT_EQUAL(rc::ML_UCL, b.games[0].league);
  rc::MemSource tur(FX_ESPN_BOARD_TUR, strlen(FX_ESPN_BOARD_TUR));
  TEST_ASSERT_TRUE(rc::espnParseBoard(tur, g_boardArena, 436, rc::ML_SUPER_LIG, b));
  TEST_ASSERT_EQUAL(5, b.n);
  TEST_ASSERT_EQUAL(0, b.ng);
  TEST_ASSERT_EQUAL(rc::ML_SUPER_LIG, b.games[1].league);
  TEST_ASSERT_EQUAL_STRING("Gençlerbirliği", b.games[1].home);
  TEST_ASSERT_EQUAL_STRING("Amedspor", b.games[1].away);
  TEST_ASSERT_EQUAL(rc::MP_POST, b.games[1].phase);
  TEST_ASSERT_EQUAL(rc::BF_HOME_TR | rc::BF_AWAY_TR | rc::BF_OURS, b.games[4].flags);
  TEST_ASSERT_EQUAL_STRING("Çaykur Rizespor", b.games[4].home);
  TEST_ASSERT_EQUAL(rc::ML_SUPER_LIG, rc::espnLeague("tur.1"));
  TEST_ASSERT_EQUAL_UINT32(rc::utcFromCivil(2026, 10, 13, 19, 0, 0), b.games[0].kickoffUtc);
}

static rc::Standings g_table;

static void test_standings_super_lig() {
  rc::MemSource src(FX_ESPN_STANDINGS_TUR, strlen(FX_ESPN_STANDINGS_TUR));
  TEST_ASSERT_TRUE(rc::espnParseStandings(src, g_arena, 436, g_table));
  TEST_ASSERT_EQUAL(18, g_table.n);
  TEST_ASSERT_EQUAL(5, g_table.nz);
  const rc::StandRow& gs = g_table.rows[0];
  TEST_ASSERT_EQUAL_STRING("Galatasaray", gs.name);
  TEST_ASSERT_EQUAL(1, gs.rank);
  TEST_ASSERT_EQUAL(7, gs.played);
  TEST_ASSERT_EQUAL(5, gs.wins);
  TEST_ASSERT_EQUAL(1, gs.ties);
  TEST_ASSERT_EQUAL(1, gs.losses);
  TEST_ASSERT_EQUAL(16, gs.points);
  TEST_ASSERT_EQUAL(5, gs.gd);
  TEST_ASSERT_EQUAL(1, gs.zone);
  TEST_ASSERT_EQUAL_STRING("Şampiyonlar Ligi", g_table.zones[0].text);
  TEST_ASSERT_EQUAL_HEX32(0x81D6AC, g_table.zones[0].color);
  const rc::StandRow& fb = g_table.rows[3];
  TEST_ASSERT_EQUAL_STRING("Fenerbahçe", fb.name);
  TEST_ASSERT_EQUAL(rc::BF_HOME_TR | rc::BF_OURS, fb.flags);
  TEST_ASSERT_EQUAL(4, fb.rank);
  TEST_ASSERT_EQUAL(13, fb.points);
  TEST_ASSERT_EQUAL(13, fb.gd);
  TEST_ASSERT_EQUAL_STRING("Konferans Ligi ön eleme", g_table.zones[fb.zone - 1].text);
  const rc::StandRow& last = g_table.rows[17];
  TEST_ASSERT_EQUAL_STRING("Eyüpspor", last.name);
  TEST_ASSERT_EQUAL(18, last.rank);
  TEST_ASSERT_EQUAL(-18, last.gd);
  TEST_ASSERT_EQUAL_STRING("Küme düşme", g_table.zones[last.zone - 1].text);
}

static void test_standings_ucl_and_leagues() {
  rc::MemSource src(FX_ESPN_STANDINGS_UCL, strlen(FX_ESPN_STANDINGS_UCL));
  TEST_ASSERT_TRUE(rc::espnParseStandings(src, g_arena, 436, g_table));
  TEST_ASSERT_EQUAL(36, g_table.n);
  TEST_ASSERT_EQUAL(4, g_table.nz);
  TEST_ASSERT_EQUAL_STRING("Sporting", g_table.rows[0].name);
  TEST_ASSERT_EQUAL_STRING("Bodo/Glimt", g_table.rows[35].name);
  TEST_ASSERT_EQUAL(36, g_table.rows[35].rank);
  TEST_ASSERT_EQUAL(-5, g_table.rows[35].gd);
  TEST_ASSERT_EQUAL_STRING("Elendi", g_table.zones[g_table.rows[35].zone - 1].text);
  char p[96];
  TEST_ASSERT_TRUE(rc::espnStandingsPath(p, sizeof(p), "eng.1") > 0);
  TEST_ASSERT_EQUAL_STRING("/apis/v2/sports/soccer/eng.1/standings", p);
  TEST_ASSERT_EQUAL(rc::ML_EPL, rc::espnLeague("eng.1"));
  TEST_ASSERT_EQUAL(rc::ML_LIGUE1, rc::espnLeague("fra.1"));
  TEST_ASSERT_EQUAL_STRING("ger.1", rc::espnLeagueSlug(rc::ML_BUNDESLIGA));
  TEST_ASSERT_EQUAL_STRING("Serie A", rc::matchLeagueTr(rc::ML_SERIE_A));
  TEST_ASSERT_EQUAL_STRING("Premier Lig", rc::matchLeagueTr(rc::ML_EPL));
  rc::Board b;
  rc::boardClear(b);
  rc::MemSource day(FX_ESPN_DAY_ENG, strlen(FX_ESPN_DAY_ENG));
  TEST_ASSERT_TRUE(rc::espnParseBoard(day, g_arena, 436, rc::ML_EPL, b));
  TEST_ASSERT_EQUAL(1, b.n);
  TEST_ASSERT_EQUAL(rc::ML_EPL, b.games[0].league);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_paths);
  RUN_TEST(test_schedule_picks_next_match);
  RUN_TEST(test_schedule_skips_past_and_maps_europe);
  RUN_TEST(test_status);
  RUN_TEST(test_plays_page_and_apply);
  RUN_TEST(test_away_goal_is_ours_when_away);
  RUN_TEST(test_keeps_latest_events);
  RUN_TEST(test_athlete_and_text_helpers);
  RUN_TEST(test_board_path_and_seed);
  RUN_TEST(test_board_new_goals);
  RUN_TEST(test_board_deep_prematch);
  RUN_TEST(test_standings_super_lig);
  RUN_TEST(test_standings_ucl_and_leagues);
  return UNITY_END();
}
