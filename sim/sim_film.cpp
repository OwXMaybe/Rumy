#include <lvgl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "agents/agents.h"
#include "app/config.h"
#include "app/store.h"
#include "espn.h"
#include "espn_board.h"
#include "settings/settings.h"
#include "sim.h"
#include "sim_paths.h"
#include "ui/agents_ui.h"
#include "ui/goal_screen.h"
#include "match/photo.h"
#include "match/leagues.h"
#include "ui/match_ui.h"
#include "ui/screens.h"
#include "rumy/rumy.h"
#include "ui/theme.h"
#include "timeutil.h"
#include "ui/ui.h"

namespace {

rc::Match s_m;
uint32_t s_now = 0;

void at(int y, int m, int d, int hh, int mm) {
  s_now = sim::localUtc(y, m, d, hh, mm);
  sim::setUtc(s_now);
}

void base(uint8_t league, uint8_t phase, bool usHome) {
  rc::matchClear(s_m);
  s_m.valid = 1;
  s_m.eventId = 9100u + league * 10u + phase;
  s_m.teamId = 436;
  s_m.usHome = usHome ? 1 : 0;
  s_m.league = league;
  s_m.kickoffUtc = s_now + (phase == rc::MP_PRE ? 2u * 3600u : 0u) - (phase == rc::MP_PRE ? 0u : 30u * 60u);
  s_m.phase = phase;
  s_m.live = 1;
  rc::MatchSide fb = {};
  fb.id = 436;
  snprintf(fb.name, sizeof(fb.name), "%s", "Fenerbahçe");
  snprintf(fb.abbr, sizeof(fb.abbr), "%s", "FB");
  rc::MatchSide other = {};
  other.id = 362;
  snprintf(other.name, sizeof(other.name), "%s", league == rc::ML_SUPER_LIG ? "Galatasaray" : "Aston Villa");
  snprintf(other.abbr, sizeof(other.abbr), "%s", league == rc::ML_SUPER_LIG ? "GS" : "AVL");
  s_m.home = usHome ? fb : other;
  s_m.away = usHome ? other : fb;
  snprintf(s_m.venue, sizeof(s_m.venue), "%s", usHome ? "Ülker Stadyumu" : "Villa Park");
  snprintf(s_m.clock, sizeof(s_m.clock), "%s", phase == rc::MP_PRE ? "" : "31'");
}

void event(uint8_t kind, bool ours, const char* minute, const char* player, const char* player2) {
  bool goal = kind == rc::MK_GOAL || kind == rc::MK_PEN_GOAL || kind == rc::MK_OWN_GOAL;
  bool homeSide = ours == (s_m.usHome == 1);
  if (goal && homeSide) s_m.homeScore++;
  else if (goal) s_m.awayScore++;
  if (s_m.n >= rc::MATCH_EVENTS) {
    memmove(&s_m.ev[0], &s_m.ev[1], sizeof(s_m.ev[0]) * (rc::MATCH_EVENTS - 1));
    s_m.n--;
  }
  rc::MatchEvent& e = s_m.ev[s_m.n++];
  memset(&e, 0, sizeof(e));
  e.id = 8000u + s_m.seq;
  e.kind = kind;
  e.ours = ours ? 1 : 0;
  e.home = s_m.homeScore;
  e.away = s_m.awayScore;
  snprintf(e.minute, sizeof(e.minute), "%s", minute);
  snprintf(e.player, sizeof(e.player), "%s", player);
  snprintf(e.player2, sizeof(e.player2), "%s", player2);
  e.seq = ++s_m.seq;
  snprintf(s_m.clock, sizeof(s_m.clock), "%s", minute);
  store::putT(store::MATCH, s_m);
}

void home() {
  lv_display_trigger_activity(nullptr);
  ui::go(ui::SCR_HOME, 0);
  sim::run(900);
}

void goalFilm(uint8_t variant, uint8_t kind, const char* minute, const char* player) {
  char name[48];
  match_ui::forceGoalVariant(static_cast<int8_t>(variant));
  lv_display_trigger_activity(nullptr);
  event(kind, true, minute, player, "");
  snprintf(name, sizeof(name), "gol_%s", goal_screen::variantName(variant));
  sim::film(name, 6200);
  snprintf(name, sizeof(name), "gol_%s_son", goal_screen::variantName(variant));
  sim::shot(name);
  match_ui::close();
  snprintf(name, sizeof(name), "gol_%s_cikis", goal_screen::variantName(variant));
  sim::film(name, 520);
  sim::run(400);
}

void teamDraft(const char* key, uint32_t id, const char* name, const char* abbr, uint32_t c1, uint32_t c2,
               const char* player) {
  Settings st = settings::get();
  st.teamId = id;
  st.teamC1 = c1;
  st.teamC2 = c2;
  settings::set(st);
  sim::run(300);
  base(rc::ML_UCL, rc::MP_LIVE, true);
  s_m.eventId = 9500u + static_cast<uint32_t>(key[0]);
  s_m.teamId = id;
  s_m.home.id = id;
  snprintf(s_m.home.name, sizeof(s_m.home.name), "%s", name);
  snprintf(s_m.home.abbr, sizeof(s_m.home.abbr), "%s", abbr);
  store::putT(store::MATCH, s_m);
  home();
  sim::run(1500);
  char shot[48];
  snprintf(shot, sizeof(shot), "takim_%s_1kart", key);
  sim::shot(shot);
  const uint8_t variants[2] = {goal_screen::V_LETTERS, goal_screen::V_RUMY};
  for (int i = 0; i < 2; i++) {
    match_ui::forceGoalVariant(static_cast<int8_t>(variants[i]));
    lv_display_trigger_activity(nullptr);
    event(rc::MK_GOAL, true, i ? "67'" : "51'", player, "");
    snprintf(shot, sizeof(shot), "takim_%s_%dgol_%s", key, i + 2, goal_screen::variantName(variants[i]));
    sim::film(shot, 6200);
    sim::shot(shot);
    match_ui::close();
    sim::run(900);
  }
  match_ui::forceGoalVariant(-1);
  lv_display_trigger_activity(nullptr);
  ui::go(ui::SCR_MATCH, -1);
  sim::run(900);
  snprintf(shot, sizeof(shot), "takim_%s_4mac", key);
  sim::shot(shot);
  ui::go(ui::SCR_HOME, 1);
  sim::run(600);
}

void teamDrafts() {
  at(2026, 10, 20, 20, 5);
  sim::netUp(s_now, true);
  sim::forecast(s_now, false);
  sim::setMedia(false, false, 0, 0, 0, "", "", "", false);
  teamDraft("a_fb_bugun", 436, "Fenerbahçe", "FB", 0, 0, "Kerem Aktürkoğlu");
  teamDraft("b_fb_yeni", 436, "Fenerbahçe", "FB", 0x13235B, 0xFFD60A, "Kerem Aktürkoğlu");
  teamDraft("c_gs", 432, "Galatasaray", "GS", 0xA6192E, 0xFDB913, "Victor Osimhen");
  teamDraft("d_arsenal", 359, "Arsenal", "ARS", 0xE20520, 0x003399, "Bukayo Saka");
  teamDraft("e_real", 86, "Real Madrid", "RMA", 0xFFFFFF, 0x00529F, "Jude Bellingham");
  teamDraft("f_juve", 111, "Juventus", "JUV", 0x111111, 0xFFFFFF, "Kenan Yıldız");
}

void table(rc::Match& m) {
  rc::Table& t = m.table;
  memset(&t, 0, sizeof(t));
  t.valid = 1;
  t.league = rc::ML_SUPER_LIG;
  t.n = 3;
  t.ours = 1;
  t.wins = 4;
  t.ties = 1;
  t.losses = 2;
  t.gd = 13;
  t.teams = 18;
  static const struct {
    uint8_t rank, pts;
    const char* name;
  } kRows[3] = {{2, 14, "Amedspor"}, {3, 13, "Fenerbahçe"}, {4, 13, "Trabzonspor"}};
  for (int i = 0; i < 3; i++) {
    t.rows[i].rank = kRows[i].rank;
    t.rows[i].points = kRows[i].pts;
    t.rows[i].played = 7;
    snprintf(t.rows[i].name, sizeof(t.rows[i].name), "%s", kRows[i].name);
  }
}

void standings() {
  base(rc::ML_SUPER_LIG, rc::MP_LIVE, true);
  table(s_m);
  event(rc::MK_GOAL, true, "12'", "Kerem Aktürkoğlu", "");
  event(rc::MK_YELLOW, false, "30'", "Barış Alper Yılmaz", "");
  sim::run(12000);
  for (int style = 1; style <= 2; style++) {
    scr_match::setTableStyle(style);
    lv_display_trigger_activity(nullptr);
    ui::go(ui::SCR_MATCH, -1);
    sim::run(900);
    sim::shot(style == 1 ? "puan_A_satir" : "puan_B_tablo");
    ui::go(ui::SCR_HOME, 1);
    sim::run(600);
  }
  base(rc::ML_SUPER_LIG, rc::MP_PRE, true);
  table(s_m);
  store::putT(store::MATCH, s_m);
  for (int style = 1; style <= 2; style++) {
    scr_match::setTableStyle(style);
    ui::go(ui::SCR_MATCH, -1);
    sim::run(900);
    sim::shot(style == 1 ? "puan_A_mac_oncesi" : "puan_B_mac_oncesi");
    ui::go(ui::SCR_HOME, 1);
    sim::run(600);
  }
  scr_match::setTableStyle(0);
  base(rc::ML_SUPER_LIG, rc::MP_LIVE, true);
  store::putT(store::MATCH, s_m);
  sim::run(600);
  event(rc::MK_GOAL, false, "55'", "Mauro Icardi", "");
  sim::run(9000);
  s_m.awayScore--;
  s_m.n--;
  store::putT(store::MATCH, s_m);
  for (int i = 0; i < 40 && !match_ui::busy(); i++) sim::run(500);
  sim::run(500);
  sim::shot("gol_iptal_bildirimi");
  sim::run(8000);
}

rc::Board s_b;
uint8_t s_league = rc::ML_UCL;
int s_day = 13;

void game(const char* home, const char* away, int hh, int mm, uint8_t phase, int hs, int as, const char* clock,
          uint8_t flags) {
  rc::BoardGame& g = s_b.games[s_b.n++];
  memset(&g, 0, sizeof(g));
  g.id = 7000u + s_b.n;
  g.kickoffUtc = sim::localUtc(2026, 10, s_day, hh, mm);
  g.league = s_league;
  g.phase = phase;
  g.homeScore = static_cast<uint8_t>(hs);
  g.awayScore = static_cast<uint8_t>(as);
  g.goals = static_cast<uint8_t>(hs + as);
  g.flags = flags;
  snprintf(g.clock, sizeof(g.clock), "%s", clock);
  snprintf(g.home, sizeof(g.home), "%s", home);
  snprintf(g.away, sizeof(g.away), "%s", away);
  snprintf(g.homeAbbr, sizeof(g.homeAbbr), "%.3s", home);
  snprintf(g.awayAbbr, sizeof(g.awayAbbr), "%.3s", away);
}

void goal(int idx, bool homeSide, const char* minute, const char* player) {
  rc::BoardGame& g = s_b.games[idx];
  if (homeSide) g.homeScore++;
  else g.awayScore++;
  g.goals++;
  rc::BoardGoal& b = s_b.goals[s_b.ng++];
  memset(&b, 0, sizeof(b));
  b.seq = ++s_b.seq;
  b.game = static_cast<uint8_t>(idx);
  b.kind = rc::MK_GOAL;
  b.home = g.homeScore;
  b.away = g.awayScore;
  b.homeSide = homeSide ? 1 : 0;
  snprintf(b.minute, sizeof(b.minute), "%s", minute);
  snprintf(b.player, sizeof(b.player), "%s", player);
}

void tonight() {
  rc::boardClear(s_b);
  s_b.valid = 1;
  s_b.day = static_cast<uint32_t>(rc::localDayNum(s_now));
  game("Lens", "Sporting", 19, 45, rc::MP_POST, 1, 2, "90'", 0);
  game("Sabah", "Slavia Prague", 19, 45, rc::MP_POST, 0, 3, "90'", 0);
  game("Arsenal", "Lille", 22, 0, rc::MP_LIVE, 1, 0, "41'", 0);
  game("Atlético", "Man United", 22, 0, rc::MP_LIVE, 0, 0, "41'", 0);
  game("Galatasaray", "Barcelona", 22, 0, rc::MP_LIVE, 0, 1, "41'", rc::BF_HOME_TR);
  game("Inter Milan", "Club Brugge", 22, 0, rc::MP_LIVE, 2, 0, "41'", 0);
  game("RB Leipzig", "PSV", 22, 0, rc::MP_LIVE, 0, 1, "41'", 0);
  game("Viking FK", "Bayern", 22, 0, rc::MP_LIVE, 0, 2, "41'", 0);
  game("Villarreal", "Napoli", 22, 0, rc::MP_LIVE, 1, 1, "41'", 0);
}

void boardFilms() {
  Settings st = settings::get();
  st.teamId = 436;
  settings::set(st);
  at(2026, 10, 13, 22, 41);
  sim::netUp(s_now, true);
  base(rc::ML_UCL, rc::MP_PRE, false);
  s_m.kickoffUtc = sim::localUtc(2026, 10, 14, 22, 0);
  store::putT(store::MATCH, s_m);
  tonight();
  store::putT(store::BOARD, s_b);
  home();
  sim::run(1500);
  goal(4, true, "41'", "Victor Osimhen");
  store::putT(store::BOARD, s_b);
  sim::film("sl_gol_bildirimi", 700);
  sim::shot("sl_gol_bildirimi");
  sim::run(7000);
  goal(2, true, "43'", "Bukayo Saka");
  store::putT(store::BOARD, s_b);
  sim::run(700);
  sim::shot("sl_gol_bildirimi_2");
  sim::run(7000);
  lv_display_trigger_activity(nullptr);
  ui::go(ui::SCR_MATCH, -1);
  sim::run(900);
  sim::shot("sl_mac_ekrani");
  lv_obj_scroll_to_y(lv_screen_active(), 5000, LV_ANIM_OFF);
  sim::run(300);
  sim::shot("sl_mac_ekrani_alt");
  at(2026, 10, 14, 22, 30);
  base(rc::ML_UCL, rc::MP_LIVE, false);
  s_m.kickoffUtc = sim::localUtc(2026, 10, 14, 22, 0);
  s_m.awayScore = 1;
  store::putT(store::MATCH, s_m);
  rc::boardClear(s_b);
  s_b.valid = 1;
  s_b.day = static_cast<uint32_t>(rc::localDayNum(s_now));
  game("Feyenoord", "Como", 19, 45, rc::MP_POST, 2, 2, "90'", 0);
  game("LASK Linz", "Liverpool", 19, 45, rc::MP_POST, 0, 4, "90'", 0);
  game("AS Roma", "Real Madrid", 22, 0, rc::MP_LIVE, 1, 0, "31'", 0);
  game("Aston Villa", "Fenerbahçe", 22, 0, rc::MP_LIVE, 0, 1, "31'", rc::BF_AWAY_TR | rc::BF_OURS);
  game("Bodo/Glimt", "Dortmund", 22, 0, rc::MP_LIVE, 0, 0, "31'", 0);
  game("Man City", "PSG", 22, 0, rc::MP_LIVE, 1, 1, "31'", 0);
  game("Betis", "FC Porto", 22, 0, rc::MP_LIVE, 0, 0, "31'", 0);
  game("Shakhtar", "AEK Athens", 22, 0, rc::MP_LIVE, 2, 0, "31'", 0);
  game("S Bratislava", "Stuttgart", 22, 0, rc::MP_LIVE, 0, 1, "31'", 0);
  s_b.games[3].awayId = 436;
  store::putT(store::BOARD, s_b);
  sim::run(600);
  lv_obj_scroll_to_y(lv_screen_active(), 5000, LV_ANIM_OFF);
  sim::run(300);
  sim::shot("sl_yarin_fb_maci");
  s_day = 17;
  s_league = rc::ML_SUPER_LIG;
  at(2026, 10, 17, 19, 52);
  base(rc::ML_SUPER_LIG, rc::MP_LIVE, true);
  s_m.away.id = 3712;
  snprintf(s_m.away.name, sizeof(s_m.away.name), "%s", "Alanyaspor");
  snprintf(s_m.away.abbr, sizeof(s_m.away.abbr), "%s", "ALN");
  s_m.kickoffUtc = sim::localUtc(2026, 10, 17, 19, 0);
  s_m.homeScore = 1;
  store::putT(store::MATCH, s_m);
  rc::boardClear(s_b);
  s_b.valid = 1;
  s_b.day = static_cast<uint32_t>(rc::localDayNum(s_now));
  game("Erzurumspor", "Eyüpspor", 13, 30, rc::MP_POST, 1, 1, "90'", rc::BF_HOME_TR | rc::BF_AWAY_TR);
  game("Amedspor", "Konyaspor", 16, 0, rc::MP_POST, 2, 0, "90'", rc::BF_HOME_TR | rc::BF_AWAY_TR);
  game("Gençlerbirliği", "Galatasaray", 16, 0, rc::MP_POST, 1, 1, "90'", rc::BF_HOME_TR | rc::BF_AWAY_TR);
  game("Fenerbahçe", "Alanyaspor", 19, 0, rc::MP_LIVE, 1, 0, "47'", rc::BF_HOME_TR | rc::BF_AWAY_TR | rc::BF_OURS);
  s_b.games[3].homeId = 436;
  store::putT(store::BOARD, s_b);
  home();
  sim::run(1500);
  s_b.games[2].phase = rc::MP_LIVE;
  snprintf(s_b.games[2].clock, sizeof(s_b.games[2].clock), "%s", "88'");
  s_b.games[2].homeScore = 0;
  s_b.games[2].goals = 1;
  store::putT(store::BOARD, s_b);
  sim::run(300);
  goal(2, true, "88'", "Oğulcan Ülgün");
  store::putT(store::BOARD, s_b);
  sim::run(700);
  sim::shot("superlig_gol_bildirimi");
  sim::run(7000);
  lv_display_trigger_activity(nullptr);
  ui::go(ui::SCR_MATCH, -1);
  sim::run(900);
  sim::shot("superlig_mac_ekrani");
  lv_obj_scroll_to_y(lv_screen_active(), 5000, LV_ANIM_OFF);
  sim::run(300);
  sim::shot("superlig_mac_ekrani_alt");
  s_league = rc::ML_UCL;
  s_day = 13;
  rc::boardClear(s_b);
  store::putT(store::BOARD, s_b);
  home();
}

bool loadJson(const char* name, char*& buf, size_t& len) {
  char path[512];
  snprintf(path, sizeof(path), "%s/%s", RUMY_SIM_FIXTURES, name);
  FILE* f = fopen(path, "rb");
  if (!f) return false;
  fseek(f, 0, SEEK_END);
  long n = ftell(f);
  fseek(f, 0, SEEK_SET);
  buf = static_cast<char*>(malloc(static_cast<size_t>(n) + 1));
  size_t got = fread(buf, 1, static_cast<size_t>(n), f);
  fclose(f);
  buf[got] = 0;
  len = got;
  return true;
}

uint8_t s_jsonMem[32768];

rc::LeagueView s_view;

void viewTable(uint8_t league, const char* file) {
  memset(&s_view, 0, sizeof(s_view));
  s_view.kind = rc::VK_TABLE;
  s_view.league = league;
  s_view.req = leagues::request();
  char* buf = nullptr;
  size_t len = 0;
  rc::JsonArena arena(s_jsonMem, sizeof(s_jsonMem));
  bool ok = loadJson(file, buf, len);
  if (ok) {
    rc::MemSource src(buf, len);
    ok = rc::espnParseStandings(src, arena, 436, s_view.table);
  }
  s_view.state = ok ? rc::VS_OK : rc::VS_ERROR;
  free(buf);
  store::putT(store::VIEW, s_view);
}

void viewDay(uint8_t league, const char* file, uint32_t ymd) {
  memset(&s_view, 0, sizeof(s_view));
  s_view.kind = rc::VK_DAY;
  s_view.league = league;
  s_view.req = leagues::request();
  s_view.ymd = ymd;
  char* buf = nullptr;
  size_t len = 0;
  rc::JsonArena arena(s_jsonMem, sizeof(s_jsonMem));
  bool ok = loadJson(file, buf, len);
  if (ok) {
    rc::MemSource src(buf, len);
    ok = rc::espnParseBoard(src, arena, 436, league, s_view.day);
  }
  s_view.state = !ok ? rc::VS_ERROR : s_view.day.n ? rc::VS_OK : rc::VS_EMPTY;
  free(buf);
  store::putT(store::VIEW, s_view);
}

void viewBoard(uint8_t league, const rc::Board& b, uint32_t ymd) {
  memset(&s_view, 0, sizeof(s_view));
  s_view.kind = rc::VK_DAY;
  s_view.league = league;
  s_view.req = leagues::request();
  s_view.ymd = ymd;
  s_view.day = b;
  s_view.state = rc::VS_OK;
  store::putT(store::VIEW, s_view);
}

void scrollList(int32_t y) {
  lv_obj_t* cont = lv_obj_get_child(lv_screen_active(), 3);
  if (cont) lv_obj_scroll_to_y(cont, y, LV_ANIM_OFF);
  sim::run(300);
}

void footballFilms() {
  Settings st = settings::get();
  st.teamId = 436;
  settings::set(st);
  s_day = 13;
  s_league = rc::ML_UCL;
  at(2026, 10, 13, 22, 41);
  sim::netUp(s_now, true);
  tonight();
  store::putT(store::BOARD, s_b);
  lv_display_trigger_activity(nullptr);
  ui::go(ui::SCR_FOOTBALL, 1);
  sim::run(900);
  sim::shot("futbol_sayfasi");
  leagues::open(rc::ML_UCL);
  viewBoard(rc::ML_UCL, s_b, 20261013);
  ui::go(ui::SCR_LEAGUE, 1);
  sim::run(900);
  sim::shot("futbol_sampiyonlar_bugun");
  leagues::showTable(true);
  viewTable(rc::ML_UCL, "espn_standings_ucl.json");
  sim::run(600);
  sim::shot("futbol_sampiyonlar_puan");
  scrollList(5000);
  sim::shot("futbol_sampiyonlar_puan_alt");
  ui::go(ui::SCR_FOOTBALL, -1);
  sim::run(900);
  leagues::open(rc::ML_SUPER_LIG);
  viewDay(rc::ML_SUPER_LIG, "espn_board_tur.json", 20261010);
  ui::go(ui::SCR_LEAGUE, 1);
  sim::run(900);
  sim::shot("futbol_superlig_mac_gunu");
  leagues::showTable(true);
  viewTable(rc::ML_SUPER_LIG, "espn_standings_tur.json");
  sim::run(600);
  sim::shot("futbol_superlig_puan");
  scrollList(5000);
  sim::shot("futbol_superlig_puan_alt");
  ui::go(ui::SCR_FOOTBALL, -1);
  sim::run(900);
  leagues::open(rc::ML_EPL);
  viewDay(rc::ML_EPL, "espn_day_eng.json", 20261012);
  ui::go(ui::SCR_LEAGUE, 1);
  sim::run(900);
  sim::shot("futbol_premier_dun");
  rc::boardClear(s_b);
  store::putT(store::BOARD, s_b);
  home();
}

uint32_t s_agVer = 0;

void agentsSend(const char* const* rows, int n, int work, int wait, int idle) {
  char head[48];
  snprintf(head, sizeof(head), "%lx %d %d %d %d", static_cast<unsigned long>(++s_agVer), n, work, wait, idle);
  agents::head(head);
  for (int i = 0; i < n; i++) {
    char buf[200];
    snprintf(buf, sizeof(buf), "%d\t%s", i, rows[i]);
    agents::row(buf);
  }
}

void updateFilms() {
  at(2026, 10, 25, 17, 45);
  sim::netUp(s_now, true);
  sim::forecast(s_now, false);
  sim::setMedia(false, false, 0, 0, 0, "", "", "", false);
  agentsSend(nullptr, 0, 0, 0, 0);
  home();
  sim::run(600);
  const uint32_t total = 2150512;
  sim::setUpdate(1, 0, total, "");
  sim::run(1100);
  sim::shot("guncelleme_basladi");
  sim::setUpdate(1, total * 42 / 100, total, "");
  sim::run(1100);
  sim::shot("guncelleme_aliniyor");
  sim::setUpdate(2, total, total, "");
  sim::run(1100);
  sim::shot("guncelleme_alindi");
  sim::setUpdate(0, 0, 0, "");
  sim::run(1100);
  sim::setUpdate(1, total * 63 / 100, total, "");
  sim::run(1100);
  sim::setUpdate(3, total * 63 / 100, total, "zaman asimi");
  sim::run(1100);
  sim::shot("guncelleme_hata");
  sim::setUpdate(0, 0, 0, "");
  sim::run(1100);
  sim::shot("guncelleme_sonrasi_ana");
}

void lookThumbs() {
  lv_obj_t* scr = theme::makeScreen();
  for (int m = 0; m < rumy::MOOD_COUNT; m++) {
    lv_obj_t* face = rumy::create(scr, theme::S(80), theme::S(56), theme::S(22));
    lv_obj_set_pos(face, (m % 4) * theme::S(80), (m / 4) * theme::S(60));
    rumy::setAnimated(face, false);
    rumy::setMood(face, static_cast<rumy::Mood>(m));
  }
  lv_screen_load(scr);
  sim::run(400);
  sim::shot("gorunum_yuzler");
}

void agentFilms() {
  at(2026, 10, 25, 14, 20);
  sim::netUp(s_now, true);
  sim::forecast(s_now, false);
  sim::setMedia(false, false, 0, 0, 0, "", "", "", false);
  home();
  static const char* const kOne[] = {"a1b2c3d4\tc\t95\t95\tclaude\tRumy\tDosya düzenliyor"};
  agentsSend(kOne, 1, 1, 0, 0);
  sim::run(1200);
  sim::shot("ajan_ana_calisiyor");
  sim::film("ajan_isik_calisiyor", 1600);
  static const char* const kTwo[] = {"a1b2c3d4\tc\t140\t140\tclaude\tRumy\tKomut çalıştırıyor",
                                     "b2c3d4e5\tc\t60\t60\tcodex\tRapor tablosu\tKod okuyor"};
  agentsSend(kTwo, 2, 2, 0, 0);
  sim::run(800);
  sim::shot("ajan_ana_iki_calisiyor");
  static const char* const kWait[] = {"a1b2c3d4\tb\t0\t150\tclaude\tRumy\tİzin istiyor: Bash",
                                      "b2c3d4e5\tc\t70\t70\tcodex\tRapor tablosu\tKod okuyor"};
  agentsSend(kWait, 2, 1, 1, 0);
  sim::film("ajan_bildirim_bekliyor_giris", 520);
  sim::shot("ajan_bildirim_bekliyor");
  sim::run(11000);
  sim::shot("ajan_ana_bekliyor");
  sim::film("ajan_isik_bekliyor", 1100);
  static const char* const kWork[] = {"a1b2c3d4\tc\t5\t5\tclaude\tRumy\tKomut çalıştırıyor",
                                      "b2c3d4e5\tc\t420\t420\tcodex\tRapor tablosu\tDosya düzenliyor"};
  agentsSend(kWork, 2, 2, 0, 0);
  sim::run(1500);
  static const char* const kDone[] = {"a1b2c3d4\tc\t20\t20\tclaude\tRumy\tDüşünüyor",
                                      "b2c3d4e5\ti\t0\t425\tcodex\tRapor tablosu\t"};
  agentsSend(kDone, 2, 1, 0, 1);
  sim::run(600);
  sim::shot("ajan_bildirim_bitti");
  sim::run(1200);
  sim::shot("ajan_ana_bitti_rumy_mutlu");
  sim::run(6000);
  static const char* const kIdle[] = {"b2c3d4e5\ti\t30\t425\tcodex\tRapor tablosu\t"};
  agentsSend(kIdle, 1, 0, 0, 1);
  sim::run(800);
  sim::shot("ajan_ana_bosta");
  static const char* const kCodex[] = {"f6a7b8c9\tb\t0\t60\tcodex\tSatın alma PDF'si\tİzin istiyor: Bash",
                                       "b2c3d4e5\ti\t40\t425\tcodex\tRapor tablosu\t"};
  agentsSend(kCodex, 2, 0, 1, 1);
  sim::run(500);
  sim::shot("ajan_bildirim_codex_bekliyor");
  sim::run(11000);
  static const char* const kPage[] = {"a1b2c3d4\tb\t40\t150\tclaude\tRumy\tİzin istiyor: Bash",
                                      "c3d4e5f6\tb\t95\t30\tcodex\tSatın alma PDF'si\tİzin istiyor: dosya düzenleme",
                                      "b2c3d4e5\tc\t185\t185\tclaude\tweb-sitesi\tDosya düzenliyor",
                                      "e5f6a7b8\tc\t1260\t1260\tcodex\tRapor tablosu\tKomut çalıştırıyor",
                                      "d4e5f6a7\ti\t720\t420\tclaude\tnotlar\t"};
  agentsSend(kPage, 5, 2, 2, 1);
  sim::run(300);
  agents_ui::close();
  sim::run(600);
  agents_ui::close();
  sim::run(600);
  agents_ui::close();
  sim::run(600);
  lv_display_trigger_activity(nullptr);
  ui::go(ui::SCR_AGENTS, -1);
  sim::run(900);
  sim::shot("ajan_sayfa");
  sim::film("ajan_sayfa_isik", 1100);
  agentsSend(nullptr, 0, 0, 0, 0);
  sim::run(600);
  sim::shot("ajan_sayfa_bos");
  home();
  sim::run(31000);
  sim::shot("ajan_ana_kopru_yok");
  lv_display_trigger_activity(nullptr);
  ui::go(ui::SCR_AGENTS, -1);
  sim::run(900);
  sim::shot("ajan_sayfa_kopru_yok");
  home();
}

void leagueCards() {
  static const struct {
    uint8_t league;
    const char* name;
  } kLeagues[] = {{rc::ML_SUPER_LIG, "super_lig"}, {rc::ML_UCL, "sampiyonlar"}, {rc::ML_UEL, "avrupa"},
                  {rc::ML_UECL, "konferans"},     {rc::ML_CUP, "kupa"},         {rc::ML_SUPER_CUP, "super_kupa"},
                  {rc::ML_FRIENDLY, "hazirlik"}};
  char name[48];
  for (const auto& k : kLeagues) {
    base(k.league, rc::MP_PRE, true);
    store::putT(store::MATCH, s_m);
    home();
    sim::run(400);
    snprintf(name, sizeof(name), "kart_%s_once", k.name);
    sim::shot(name);
    base(k.league, rc::MP_LIVE, k.league != rc::ML_UCL);
    s_m.homeScore = 1;
    s_m.awayScore = 0;
    store::putT(store::MATCH, s_m);
    sim::run(1300);
    snprintf(name, sizeof(name), "kart_%s_canli", k.name);
    sim::shot(name);
  }
  base(rc::ML_SUPER_LIG, rc::MP_POST, true);
  s_m.homeScore = 2;
  s_m.awayScore = 1;
  s_m.endUtc = s_now;
  store::putT(store::MATCH, s_m);
  sim::run(1300);
  sim::shot("kart_super_lig_bitti");
  base(rc::ML_UCL, rc::MP_LIVE, true);
  event(rc::MK_YELLOW, false, "12'", "Ollie Watkins", "");
  event(rc::MK_SUB, true, "46'", "Kerem Aktürkoğlu", "İrfan Can Kahveci");
  sim::run(7000);
  lv_display_trigger_activity(nullptr);
  ui::go(ui::SCR_MATCH, -1);
  sim::run(900);
  sim::shot("mac_ekrani_sampiyonlar");
  base(rc::ML_SUPER_LIG, rc::MP_LIVE, true);
  store::putT(store::MATCH, s_m);
  sim::run(600);
  sim::shot("mac_ekrani_super_lig");
  home();
}

}

namespace photo {

namespace {

constexpr int32_t PX = cfg::PHOTO_PX;
constexpr int SLOTS = 8;

struct Cached {
  char key[48];
  bool found;
  uint16_t px[PX * PX];
  lv_image_dsc_t img;
};

Cached s_cache[SLOTS];
int s_used = 0;

void slug(const char* name, char* out, size_t cap) {
  static const struct {
    const char* from;
    char to;
  } kMap[] = {{"\xC3\xBC", 'u'}, {"\xC3\x9C", 'u'}, {"\xC3\xB6", 'o'}, {"\xC3\x96", 'o'}, {"\xC3\xA7", 'c'},
              {"\xC3\x87", 'c'}, {"\xC5\x9F", 's'}, {"\xC5\x9E", 's'}, {"\xC4\x9F", 'g'}, {"\xC4\x9E", 'g'},
              {"\xC4\xB1", 'i'}, {"\xC4\xB0", 'i'}, {"\xC3\xA9", 'e'}, {"\xC3\xA8", 'e'}, {"\xC3\xA1", 'a'},
              {"\xC5\xBE", 'z'}, {"\xC4\x87", 'c'}, {"\xC4\x8D", 'c'}, {"\xC3\xB1", 'n'}, {"\xC3\xAD", 'i'}};
  size_t n = 0;
  bool gap = false;
  for (const char* p = name; *p && n + 2 < cap;) {
    char c = 0;
    size_t len = 1;
    unsigned char u = static_cast<unsigned char>(*p);
    if (u < 0x80) {
      c = static_cast<char>(u >= 'A' && u <= 'Z' ? u + 32 : u);
      if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) c = 0;
    } else {
      len = u >= 0xF0 ? 4 : u >= 0xE0 ? 3 : 2;
      for (const auto& m : kMap)
        if (!strncmp(p, m.from, 2)) c = m.to;
    }
    p += len;
    if (!c) {
      gap = n > 0;
      continue;
    }
    if (gap) out[n++] = '_';
    gap = false;
    out[n++] = c;
  }
  out[n] = 0;
}

}

void request(const char* player) { (void)player; }

const lv_image_dsc_t* get(const char* player) {
  if (!player || !player[0]) return nullptr;
  char key[48];
  slug(player, key, sizeof(key));
  for (int i = 0; i < s_used; i++)
    if (!strcmp(s_cache[i].key, key)) return s_cache[i].found ? &s_cache[i].img : nullptr;
  if (s_used >= SLOTS) return nullptr;
  Cached& c = s_cache[s_used++];
  snprintf(c.key, sizeof(c.key), "%s", key);
  char path[700];
  snprintf(path, sizeof(path), "%s/%s.r565", RUMY_SIM_FOTO, key);
  FILE* f = fopen(path, "rb");
  c.found = f && fread(c.px, 1, sizeof(c.px), f) == sizeof(c.px);
  if (f) fclose(f);
  memset(&c.img, 0, sizeof(c.img));
  c.img.header.magic = LV_IMAGE_HEADER_MAGIC;
  c.img.header.cf = LV_COLOR_FORMAT_RGB565;
  c.img.header.w = PX;
  c.img.header.h = PX;
  c.img.header.stride = PX * 2;
  c.img.data_size = sizeof(c.px);
  c.img.data = reinterpret_cast<const uint8_t*>(c.px);
  return c.found ? &c.img : nullptr;
}

}

namespace sim {

void films() {
  Settings st = settings::get();
  st.eyeSleepMin = 0;
  settings::set(st);
  const char* only = getenv("RUMY_SIM_ONLY");
  if (only && !strcmp(only, "futbol")) {
    rc::Match none;
    rc::matchClear(none);
    store::putT(store::MATCH, none);
    footballFilms();
    return;
  }
  if (only && !strcmp(only, "ajan")) {
    rc::Match none;
    rc::matchClear(none);
    store::putT(store::MATCH, none);
    agentFilms();
    return;
  }
  if (only && !strcmp(only, "guncelle")) {
    rc::Match none;
    rc::matchClear(none);
    store::putT(store::MATCH, none);
    updateFilms();
    return;
  }
  if (only && !strcmp(only, "gorunum")) {
    lookThumbs();
    return;
  }
  if (only && !strcmp(only, "takim")) {
    teamDrafts();
    return;
  }
  if (only && !strcmp(only, "sl")) {
    rc::Match none;
    rc::matchClear(none);
    store::putT(store::MATCH, none);
    boardFilms();
    return;
  }
  at(2026, 10, 20, 20, 5);
  sim::netUp(s_now, true);
  sim::forecast(s_now, false);
  sim::setMedia(false, false, 0, 0, 0, "", "", "", false);
  rc::Match none;
  rc::matchClear(none);
  store::putT(store::MATCH, none);
  home();
  base(rc::ML_UCL, rc::MP_LIVE, true);
  store::putT(store::MATCH, s_m);
  sim::run(1500);
  sim::shot("canli_kart");
  goalFilm(goal_screen::V_LETTERS, rc::MK_GOAL, "26'", "Edin Džeko");
  goalFilm(goal_screen::V_NET, rc::MK_GOAL, "51'", "Kerem Aktürkoğlu");
  goalFilm(goal_screen::V_FIREWORKS, rc::MK_PEN_GOAL, "64'", "Youssef En-Nesyri");
  goalFilm(goal_screen::V_RUMY, rc::MK_GOAL, "88'", "Mattéo Guendouzi");
  match_ui::forceGoalVariant(-1);
  lv_display_trigger_activity(nullptr);
  event(rc::MK_GOAL, false, "90+2'", "Ollie Watkins", "");
  sim::film("rakip_gol_giris", 520);
  sim::shot("rakip_gol");
  sim::run(2000);
  sim::film("rakip_gol_cikis", 2600);
  sim::run(300);
  sim::shot("rumy_rakip_gol_sonrasi");
  s_m.phase = rc::MP_HALF;
  snprintf(s_m.clock, sizeof(s_m.clock), "%s", "İY");
  store::putT(store::MATCH, s_m);
  sim::run(500);
  sim::shot("devre_bildirimi");
  sim::run(7000);
  s_m.phase = rc::MP_LIVE;
  snprintf(s_m.clock, sizeof(s_m.clock), "%s", "90+5'");
  store::putT(store::MATCH, s_m);
  sim::run(500);
  s_m.phase = rc::MP_POST;
  s_m.endUtc = s_now;
  store::putT(store::MATCH, s_m);
  sim::run(500);
  sim::shot("mac_sonu_bildirimi");
  sim::run(7000);
  sim::shot("rumy_galibiyet");
  base(rc::ML_SUPER_LIG, rc::MP_PRE, true);
  s_m.kickoffUtc = s_now;
  store::putT(store::MATCH, s_m);
  sim::run(600);
  s_m.phase = rc::MP_LIVE;
  snprintf(s_m.clock, sizeof(s_m.clock), "%s", "1'");
  store::putT(store::MATCH, s_m);
  sim::run(500);
  sim::shot("mac_basladi_bildirimi");
  sim::run(7000);
  standings();
  leagueCards();
  rc::matchClear(none);
  store::putT(store::MATCH, none);
  sim::run(300);
  boardFilms();
  footballFilms();
  agentFilms();
}

}
