#include <string.h>
#include <unity.h>

#include "../fixtures/fixtures.h"
#include "espn.h"
#include "livescore.h"

static uint8_t g_mem[8192];
static rc::JsonArena g_arena(g_mem, sizeof(g_mem));
static uint8_t g_big[16384];
static rc::JsonArena g_bigArena(g_big, sizeof(g_big));
static rc::LsMatch g_ls;
static rc::Match g_match;

void setUp() {}
void tearDown() {}

static void test_paths_and_fold() {
  char p[96];
  TEST_ASSERT_TRUE(rc::lsIncidentsPath(p, sizeof(p), 1821028) > 0);
  TEST_ASSERT_EQUAL_STRING("/v1/api/app/incidents/soccer/1821028?locale=en", p);
  TEST_ASSERT_TRUE(rc::lsLivePath(p, sizeof(p)) > 0);
  TEST_ASSERT_EQUAL_STRING("/v1/api/app/live/soccer/3?MD=1", p);
  char f[64];
  rc::lsFold("Fenerbahçe", f, sizeof(f));
  TEST_ASSERT_EQUAL_STRING("fenerbahce", f);
  rc::lsFold("Gençlerbirliği S.K.", f, sizeof(f));
  TEST_ASSERT_EQUAL_STRING("genclerbirligi s k", f);
  rc::lsFold("İstanbul Başakşehir", f, sizeof(f));
  TEST_ASSERT_EQUAL_STRING("istanbul basaksehir", f);
}

static void test_team_match() {
  TEST_ASSERT_EQUAL(3, rc::lsTeamMatch("Fenerbahçe", "Fenerbahce"));
  TEST_ASSERT_EQUAL(2, rc::lsTeamMatch("Amedspor", "Amed Sportif"));
  TEST_ASSERT_EQUAL(2, rc::lsTeamMatch("Başakşehir", "Istanbul Basaksehir"));
  TEST_ASSERT_EQUAL(2, rc::lsTeamMatch("Man City", "Manchester City"));
  TEST_ASSERT_EQUAL(1, rc::lsTeamMatch("Slavia Prague", "Slavia Praha"));
  TEST_ASSERT_EQUAL(2, rc::lsTeamMatch("Fenerbahçe", "Fenerbahce W"));
  TEST_ASSERT_EQUAL(0, rc::lsTeamMatch("Fenerbahçe", "Galatasaray"));
  TEST_ASSERT_EQUAL(0, rc::lsTeamMatch("Real Madrid", "Real Betis"));
}

static void test_incidents_goals_and_disallowed() {
  rc::MemSource src(FX_LS_INCIDENTS_PEN, strlen(FX_LS_INCIDENTS_PEN));
  TEST_ASSERT_TRUE(rc::lsParseIncidents(src, g_bigArena, g_ls));
  TEST_ASSERT_EQUAL_UINT32(1812726u, g_ls.eid);
  TEST_ASSERT_EQUAL(4, g_ls.home);
  TEST_ASSERT_EQUAL(3, g_ls.away);
  TEST_ASSERT_EQUAL(7, g_ls.n);
  TEST_ASSERT_EQUAL_STRING("Lukas Petkov", g_ls.goals[0].player);
  TEST_ASSERT_EQUAL(1, g_ls.goals[0].home);
  TEST_ASSERT_EQUAL(0, g_ls.goals[0].away);
  TEST_ASSERT_EQUAL(1, g_ls.goals[0].homeSide);
  TEST_ASSERT_EQUAL_STRING("4'", g_ls.goals[0].minute);
  const rc::LsGoal& pen = g_ls.goals[3];
  TEST_ASSERT_EQUAL(rc::MK_PEN_GOAL, pen.kind);
  TEST_ASSERT_EQUAL_STRING("45+9'", pen.minute);
  TEST_ASSERT_EQUAL(0, pen.homeSide);
  TEST_ASSERT_EQUAL(3, pen.home);
  TEST_ASSERT_EQUAL(1, pen.away);
  TEST_ASSERT_EQUAL_STRING("Mert Komur", g_ls.goals[6].player);
}

static void test_incidents_own_goal_and_short() {
  rc::MemSource own(FX_LS_INCIDENTS_OWN, strlen(FX_LS_INCIDENTS_OWN));
  TEST_ASSERT_TRUE(rc::lsParseIncidents(own, g_arena, g_ls));
  TEST_ASSERT_EQUAL(1, g_ls.n);
  TEST_ASSERT_EQUAL(rc::MK_OWN_GOAL, g_ls.goals[0].kind);
  TEST_ASSERT_EQUAL(0, g_ls.goals[0].homeSide);
  TEST_ASSERT_EQUAL(0, g_ls.goals[0].home);
  TEST_ASSERT_EQUAL(1, g_ls.goals[0].away);
  rc::MemSource fb(FX_LS_INCIDENTS_FB, strlen(FX_LS_INCIDENTS_FB));
  TEST_ASSERT_TRUE(rc::lsParseIncidents(fb, g_arena, g_ls));
  TEST_ASSERT_EQUAL(1, g_ls.n);
  TEST_ASSERT_EQUAL_STRING("Romelu Lukaku", g_ls.goals[0].player);
  TEST_ASSERT_EQUAL_STRING("40'", g_ls.goals[0].minute);
}

static void test_full_arena_fails_cleanly() {
  static uint8_t small[3072];
  rc::JsonArena arena(small, sizeof(small));
  rc::MemSource src(FX_LS_INCIDENTS_PEN, strlen(FX_LS_INCIDENTS_PEN));
  TEST_ASSERT_FALSE(rc::lsParseIncidents(src, arena, g_ls));
  TEST_ASSERT_TRUE(arena.failures() > 0);
  TEST_ASSERT_EQUAL(0, g_ls.n);
}

static void test_find_live() {
  rc::LsEvent ev;
  rc::MemSource src(FX_LS_LIVE, strlen(FX_LS_LIVE));
  TEST_ASSERT_TRUE(rc::lsFindLive(src, g_bigArena, "Fenerbahçe", "Alanyaspor", ev));
  TEST_ASSERT_EQUAL_UINT32(1821028u, ev.eid);
  TEST_ASSERT_EQUAL(0, ev.swapped);
  TEST_ASSERT_EQUAL(1, ev.home);
  TEST_ASSERT_EQUAL(0, ev.away);
  rc::MemSource src2(FX_LS_LIVE, strlen(FX_LS_LIVE));
  TEST_ASSERT_TRUE(rc::lsFindLive(src2, g_bigArena, "Barcelona", "Betis", ev));
  TEST_ASSERT_EQUAL_UINT32(1810726u, ev.eid);
  TEST_ASSERT_EQUAL(1, ev.swapped);
  rc::MemSource src3(FX_LS_LIVE, strlen(FX_LS_LIVE));
  TEST_ASSERT_FALSE(rc::lsFindLive(src3, g_bigArena, "Fenerbahçe", "Galatasaray", ev));
}

static void test_fast_merge() {
  rc::matchClear(g_match);
  g_match.valid = 1;
  g_match.usHome = 1;
  TEST_ASSERT_TRUE(rc::matchAddFast(g_match, rc::MK_GOAL, true, 1, 0, "40'", "Romelu Lukaku"));
  TEST_ASSERT_FALSE(rc::matchAddFast(g_match, rc::MK_GOAL, true, 1, 0, "40'", "Romelu Lukaku"));
  TEST_ASSERT_EQUAL(1, g_match.n);
  TEST_ASSERT_EQUAL(1, g_match.ev[0].ours);
  TEST_ASSERT_EQUAL(1, g_match.homeScore);
  uint16_t seq = g_match.ev[0].seq;
  rc::EspnPage page;
  memset(&page, 0, sizeof(page));
  page.count = 1;
  page.pageIndex = 1;
  page.pageCount = 1;
  page.n = 1;
  page.items[0].id = 53807917;
  page.items[0].kind = rc::MK_GOAL;
  page.items[0].home = 1;
  page.items[0].away = 0;
  page.items[0].teamId = 436;
  strcpy(page.items[0].minute, "40'");
  strcpy(page.items[0].player, "Romelu Lukaku");
  g_match.teamId = 436;
  TEST_ASSERT_EQUAL(0, rc::matchApply(g_match, page));
  TEST_ASSERT_EQUAL(1, g_match.n);
  TEST_ASSERT_EQUAL_UINT32(53807917u, g_match.ev[0].id);
  TEST_ASSERT_EQUAL(seq, g_match.ev[0].seq);
  TEST_ASSERT_TRUE(rc::matchAddFast(g_match, rc::MK_GOAL, true, 2, 0, "47'", ""));
  TEST_ASSERT_FALSE(rc::matchAddFast(g_match, rc::MK_GOAL, true, 2, 0, "47'", "Irfan Can Kahveci"));
  TEST_ASSERT_EQUAL(2, g_match.n);
  TEST_ASSERT_EQUAL_STRING("Irfan Can Kahveci", g_match.ev[1].player);
  TEST_ASSERT_EQUAL(1, rc::matchDropAbove(g_match, 1, 0));
  TEST_ASSERT_EQUAL(1, g_match.n);
  TEST_ASSERT_EQUAL(1, g_match.ev[0].home);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_paths_and_fold);
  RUN_TEST(test_team_match);
  RUN_TEST(test_incidents_goals_and_disallowed);
  RUN_TEST(test_incidents_own_goal_and_short);
  RUN_TEST(test_full_arena_fails_cleanly);
  RUN_TEST(test_find_live);
  RUN_TEST(test_fast_merge);
  return UNITY_END();
}
