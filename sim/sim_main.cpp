#include <Arduino.h>
#include <direct.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

#include "../test/fixtures/fixtures.h"
#include "agents/agents.h"
#include "app/config.h"
#include "app/mem.h"
#include "app/store.h"
#include "clock/clock.h"
#include "espn.h"
#include "events_calc.h"
#include "json_arena.h"
#include "media_proto.h"
#include "net/fetch.h"
#include "nager.h"
#include "openmeteo.h"
#include "reader.h"
#include "rumy/rumy.h"
#include "settings/settings.h"
#include "sim.h"
#include "sim_audit.h"
#include "timeutil.h"
#include "ui/theme.h"
#include "ui/ui.h"

namespace {

constexpr int W = cfg::SCREEN_W;
constexpr int H = cfg::SCREEN_H;

uint16_t s_fb[W * H];
const char* s_out = ".";
uint8_t s_arenaBuf[16384];
rc::JsonArena s_arena(s_arenaBuf, sizeof(s_arenaBuf));
int s_shots = 0;
FILE* s_audit = nullptr;

int s_capEvery = 0;
int s_capBands = 0;
int s_capShots = 0;
const char* s_capName = "";

char s_filmDir[600];
bool s_filming = false;
uint32_t s_filmStart = 0;
int s_filmFrames = 0;
uint64_t s_refrPx = 0;
uint64_t s_filmPx = 0;
uint64_t s_filmMax = 0;
uint32_t s_filmRefr = 0;
uint32_t s_filmFull = 0;

uint32_t tickCb() { return millis(); }

void writeFrame(const char* path) {
  FILE* f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P6\n%d %d\n255\n", W, H);
  for (int i = 0; i < W * H; i++) {
    uint16_t c = s_fb[i];
    uint8_t rgb[3] = {static_cast<uint8_t>(((c >> 11) & 31) * 255 / 31), static_cast<uint8_t>(((c >> 5) & 63) * 255 / 63),
                      static_cast<uint8_t>((c & 31) * 255 / 31)};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}

void filmFrame() {
  char path[700];
  snprintf(path, sizeof(path), "%s/%04d_%06u.ppm", s_filmDir, s_filmFrames++,
           static_cast<unsigned>(millis() - s_filmStart));
  writeFrame(path);
}

void writePpm(const char* name) {
  char path[600];
  snprintf(path, sizeof(path), "%s/%02d_%s.ppm", s_out, ++s_shots, name);
  FILE* f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P6\n%d %d\n255\n", W, H);
  for (int i = 0; i < W * H; i++) {
    uint16_t c = s_fb[i];
    uint8_t rgb[3] = {static_cast<uint8_t>(((c >> 11) & 31) * 255 / 31), static_cast<uint8_t>(((c >> 5) & 63) * 255 / 63),
                      static_cast<uint8_t>((c & 31) * 255 / 31)};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
  printf("goruntu %s\n", path);
}

void flushCb(lv_display_t* d, const lv_area_t* a, uint8_t* px) {
  int32_t w = lv_area_get_width(a);
  for (int32_t y = a->y1; y <= a->y2; y++) {
    memcpy(&s_fb[y * W + a->x1], px + static_cast<size_t>(y - a->y1) * static_cast<size_t>(w) * 2U,
           static_cast<size_t>(w) * 2U);
  }
  s_refrPx += static_cast<uint64_t>(w) * static_cast<uint64_t>(lv_area_get_height(a));
  if (lv_display_flush_is_last(d)) {
    if (s_filming) {
      s_filmPx += s_refrPx;
      s_filmRefr++;
      if (s_refrPx > s_filmMax) s_filmMax = s_refrPx;
      if (s_refrPx >= static_cast<uint64_t>(W) * H * 9 / 10) s_filmFull++;
      filmFrame();
    }
    s_refrPx = 0;
  }
  if (s_capEvery && a->y2 == H - 1 && ++s_capBands % s_capEvery == 0 && s_capShots < 3) {
    char name[64];
    snprintf(name, sizeof(name), "%s_%d", s_capName, ++s_capShots);
    writePpm(name);
  }
  lv_display_flush_ready(d);
}

void captureSlide(const char* name, ui::ScreenId id, int dir) {
  s_capName = name;
  s_capBands = 0;
  s_capShots = 0;
  s_capEvery = 6;
  ui::go(id, dir);
  for (int i = 0; i < 5; i++) {
    simAdvance(10);
    ui::loop();
  }
  s_capEvery = 0;
}

void run(uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += 10) {
    simAdvance(10);
    ui::loop();
    agents::loop();
  }
}

void film(const char* name, uint32_t ms) {
  snprintf(s_filmDir, sizeof(s_filmDir), "%s/film_%s", s_out, name);
  _mkdir(s_filmDir);
  s_filmStart = millis();
  s_filmFrames = 0;
  s_filmPx = s_filmMax = 0;
  s_filmRefr = s_filmFull = 0;
  filmFrame();
  s_filming = true;
  uint32_t end = millis() + ms;
  while (static_cast<int32_t>(end - millis()) > 0) ui::loop();
  s_filming = false;
  uint64_t screen = static_cast<uint64_t>(W) * H;
  unsigned avg = s_filmRefr ? static_cast<unsigned>(s_filmPx * 100 / s_filmRefr / screen) : 0;
  printf("film %-22s %3u kare / %4u ms, kare basina ort %%%u, en cok %%%u ekran, tam ekran %u kare\n", name,
         static_cast<unsigned>(s_filmRefr), static_cast<unsigned>(ms), avg,
         static_cast<unsigned>(s_filmMax * 100 / screen), static_cast<unsigned>(s_filmFull));
}

void memLine(const char* name) {
  lv_mem_monitor_t m;
  lv_mem_monitor(&m);
  printf("bellek %-24s kullanilan %6u / %6u  tepe %6u  parca %%%u\n", name,
         static_cast<unsigned>(m.total_size - m.free_size), static_cast<unsigned>(m.total_size),
         static_cast<unsigned>(m.max_used), static_cast<unsigned>(m.frag_pct));
}

alignas(64) uint16_t s_whole[W * H];

void shot(const char* name) {
  memLine(name);
  lv_display_t* d = lv_display_get_default();
  lv_display_set_buffers(d, s_whole, nullptr, sizeof(s_whole), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_obj_invalidate(lv_screen_active());
  lv_obj_invalidate(lv_layer_top());
  audit::start();
  lv_refr_now(nullptr);
  audit::finish(name, s_audit);
  lv_display_set_buffers(d, mem::drawBuf1(), mem::drawBuf2(), cfg::DRAW_BUF_BYTES, LV_DISPLAY_RENDER_MODE_PARTIAL);
  writePpm(name);
}

uint32_t localUtc(int y, int m, int d, int hh, int mm) { return rc::utcFromCivil(y, m, d, hh, mm, 0) - 3U * 3600U; }

struct HourSeed {
  uint8_t code;
  int16_t temp10;
  uint8_t pop;
};

void publishForecast(uint32_t now, bool rain) {
  rc::Forecast f;
  rc::clearForecast(f);
  f.fetchedUtc = now - 240;
  f.valid = 1;
  f.now.time = now - (now % 900);
  f.now.temp10 = 234;
  f.now.feels10 = 228;
  f.now.press10 = 10162;
  f.now.wind10 = 183;
  f.now.gust10 = 321;
  f.now.windDir = 42;
  f.now.visDam = 2400;
  f.now.precip100 = 0;
  f.now.rh = 58;
  f.now.code = 2;
  f.now.isDay = 1;
  f.now.uv10 = 45;
  f.m15.start = f.now.time;
  f.m15.count = rc::MINUTELY;
  const uint16_t wet[rc::MINUTELY] = {0, 12, 40, 90};
  for (int i = 0; i < rc::MINUTELY; i++) f.m15.precip100[i] = rain ? wet[i] : 0;
  static const HourSeed kRain[rc::HOURS] = {
      {2, 234, 10}, {2, 238, 20}, {3, 231, 40}, {61, 205, 70}, {63, 192, 80}, {80, 184, 60}, {3, 178, 30},
      {3, 172, 20}, {2, 166, 10}, {1, 161, 0},  {0, 158, 0},   {0, 155, 0},   {0, 153, 0},   {0, 151, 0},
      {0, 150, 0},  {1, 149, 0},  {1, 152, 0},  {1, 160, 0},   {2, 172, 0},   {2, 188, 5},   {2, 201, 5},
      {3, 212, 10}, {3, 219, 10}, {2, 224, 5}};
  static const HourSeed kDry[rc::HOURS] = {
      {1, 234, 0}, {1, 238, 0}, {2, 236, 0}, {2, 229, 0}, {2, 214, 5}, {1, 199, 5}, {0, 188, 0}, {0, 180, 0},
      {0, 174, 0}, {0, 168, 0}, {0, 163, 0}, {0, 160, 0}, {0, 157, 0}, {0, 155, 0}, {0, 154, 0}, {0, 153, 0},
      {1, 157, 0}, {1, 164, 0}, {1, 175, 0}, {2, 190, 0}, {2, 203, 0}, {2, 214, 0}, {1, 221, 0}, {1, 226, 0}};
  const HourSeed* seed = rain ? kRain : kDry;
  f.hourStart = now - (now % 3600);
  f.hourCount = rc::HOURS;
  for (int i = 0; i < rc::HOURS; i++) {
    rc::LocalTime lt = rc::toLocal(f.hourStart + static_cast<uint32_t>(i) * 3600U);
    f.hours[i].code = seed[i].code;
    f.hours[i].temp10 = seed[i].temp10;
    f.hours[i].pop = seed[i].pop;
    f.hours[i].isDay = (lt.hour >= 7 && lt.hour < 19) ? 1 : 0;
  }
  if (!rain) f.now.code = 1;
  store::putT(store::FORECAST, f);
}

void publishFixtures() {
  rc::Daily daily;
  rc::MemSource sd(FX_OPENMETEO_DAILY, strlen(FX_OPENMETEO_DAILY));
  if (rc::omParseDaily(sd, s_arena, daily)) {
    daily.fetchedUtc = sim::utc();
    store::putT(store::DAILY, daily);
  }
  rc::AirQ air;
  rc::MemSource sa(FX_OPENMETEO_AIR, strlen(FX_OPENMETEO_AIR));
  if (rc::omParseAir(sa, s_arena, air)) store::putT(store::AIR, air);
  rc::Marine sea;
  rc::MemSource sm(FX_OPENMETEO_MARINE, strlen(FX_OPENMETEO_MARINE));
  if (rc::omParseMarine(sm, s_arena, sea)) store::putT(store::MARINE, sea);
}

void publishQuake(uint32_t now, int16_t mag10, uint32_t ago, const char* place) {
  rc::QuakeInfo q;
  memset(&q, 0, sizeof(q));
  q.fetchedUtc = now - 60;
  q.valid = 1;
  q.count24h = 4;
  q.latest.timeUtc = now - ago;
  q.latest.mag10 = mag10;
  q.latest.depth10 = 112;
  q.latest.lat1e4 = 406883;
  q.latest.lon1e4 = 286648;
  snprintf(q.latest.place, sizeof(q.latest.place), "%s", place);
  q.strongest = q.latest;
  store::putT(store::QUAKE, q);
}

void publishEvents(uint32_t now) {
  rc::Religious rel[2] = {rc::religiousBest(2026, nullptr), rc::religiousBest(2027, nullptr)};
  rc::PersonalEvent pe[2];
  memset(pe, 0, sizeof(pe));
  snprintf(pe[0].name, sizeof(pe[0].name), "Elif'in doğum günü");
  pe[0].year = 1990;
  pe[0].month = 10;
  pe[0].day = 3;
  pe[0].kind = rc::EV_BIRTHDAY;
  snprintf(pe[1].name, sizeof(pe[1].name), "Kuruluş yıl dönümü");
  pe[1].year = 2018;
  pe[1].month = 11;
  pe[1].day = 12;
  pe[1].kind = rc::EV_ANNIVERSARY;
  rc::EventsSnapshot snap;
  rc::buildSnapshot(rel, 2, pe, 2, rc::localDayNum(now), snap);
  store::putT(store::EVENTS, snap);
}

void publishBulb(bool on) {
  BulbState& b = sim::bulb();
  memset(&b, 0, sizeof(b));
  b.paired = 1;
  b.found = 1;
  b.online = 1;
  b.ip = 192u | (168u << 8) | (1u << 16) | (37u << 24);
  snprintf(b.mac, sizeof(b.mac), "a8bb5012d4f0");
  b.pilot = rc::pilotTemp(2700, 70);
  b.pilot.on = on ? 1 : 0;
  b.candidateCount = 2;
  b.candidates[0].ip = b.ip;
  snprintf(b.candidates[0].mac, sizeof(b.candidates[0].mac), "a8bb5012d4f0");
  b.candidates[1].ip = 192u | (168u << 8) | (1u << 16) | (52u << 24);
  snprintf(b.candidates[1].mac, sizeof(b.candidates[1].mac), "a8bb50c39a17");
  sim::publishBulb();
}

void publishNet(uint32_t now, bool wifi) {
  NetStatus& n = sim::net();
  memset(&n, 0, sizeof(n));
  n.wifi = wifi ? 1 : 0;
  n.timeSynced = 1;
  n.rssi = wifi ? -58 : 0;
  n.ip = wifi ? (192u | (168u << 8) | (1u << 16) | (40u << 24)) : 0;
  n.tlsPeak = 44912;
  n.lastFullOkUtc = wifi ? now - 240 : now - 7200;
  const uint32_t dur[SRC_COUNT] = {341, 183, 179, 1041, 3496, 412, 1180, 420};
  const uint32_t bytes[SRC_COUNT] = {2461, 476, 502, 700, 3960, 18988, 115369, 61119};
  for (int i = 0; i < SRC_COUNT; i++) {
    SourceStat& s = n.src[i];
    s.lastCode = 200;
    s.lastOk = 1;
    s.via = i == SRC_QUAKE || i == SRC_HOLIDAY ? net::VIA_TLS : net::VIA_HTTP;
    s.lastTryUtc = now - 180;
    s.lastOkUtc = now - 180;
    s.durMs = dur[i];
    s.bytes = bytes[i];
    s.nextInS = 600;
  }
  sim::publishNet();
}

void setupSettings() {
  Settings s = settings::get();
  snprintf(s.name, sizeof(s.name), "Deniz");
  snprintf(s.city, sizeof(s.city), "İstanbul");
  s.lat = 41.008f;
  s.lon = 28.978f;
  s.province = 34;
  snprintf(s.bulbMac, sizeof(s.bulbMac), "a8bb5012d4f0");
  snprintf(s.bulbName, sizeof(s.bulbName), "Salon lambası");
  s.wifiConfigured = 1;
  settings::set(s);
}

PrinterState printerSample(uint8_t phase, bool connected) {
  PrinterState p;
  memset(&p, 0, sizeof(p));
  rc::k2Clear(p.st);
  p.configured = 1;
  p.connected = connected ? 1 : 0;
  p.ip = 192u | (168u << 8) | (31u << 16) | (19u << 24);
  snprintf(p.st.host, sizeof(p.st.host), "K2-YAZICI");
  snprintf(p.st.model, sizeof(p.st.model), "F021");
  snprintf(p.st.file, sizeof(p.st.file), "Telefon_Standi_V2.stl_PLA_54m36s.gcode");
  p.st.seen = rc::K2F_STATE;
  p.st.state = 1;
  p.st.progress = 45;
  p.st.leftS = 4320;
  p.st.jobS = 3480;
  p.st.startUtc = sim::utc() - 3480;
  p.st.layer = 120;
  p.st.totalLayer = 340;
  p.st.nozzle10 = 2099;
  p.st.nozzleTarget10 = 2100;
  p.st.bed10 = 599;
  p.st.bedTarget10 = 600;
  p.st.box10 = 350;
  p.st.selfTest = 100;
  p.phase = phase;
  return p;
}

void printerScenario() {
  PrinterState p = printerSample(rc::K2_PRINTING, true);
  sim::publishPrinter(p);
  ui::go(ui::SCR_PRINTER, -1);
  run(900);
  shot("yazici_baski");
  p.st.errCode = 2845;
  p.st.errKey = 3;
  snprintf(p.st.errText, sizeof(p.st.errText), "E2845");
  p.phase = rc::k2Phase(p.st);
  p.problem = rc::k2Problem(p.st) ? 1 : 0;
  sim::publishPrinter(p);
  run(500);
  shot("yazici_hata");
  p = printerSample(rc::K2_PAUSED, true);
  p.st.state = 5;
  p.st.runout = 1;
  p.problem = rc::k2Problem(p.st) ? 1 : 0;
  sim::publishPrinter(p);
  run(500);
  shot("yazici_filament_bitti");
  p = printerSample(rc::K2_DONE, true);
  p.st.state = 2;
  p.st.progress = 100;
  p.st.leftS = 0;
  p.st.jobS = 3413;
  p.st.startUtc = sim::utc() - 3600;
  sim::publishPrinter(p);
  lv_display_trigger_activity(nullptr);
  ui::go(ui::SCR_PRINTER, -1);
  run(500);
  shot("yazici_bitti");
  p.connected = 0;
  sim::publishPrinter(p);
  lv_display_trigger_activity(nullptr);
  run(500);
  shot("yazici_baglanti_yok");
  PrinterState none;
  memset(&none, 0, sizeof(none));
  rc::k2Clear(none.st);
  sim::publishPrinter(none);
  lv_display_trigger_activity(nullptr);
  run(500);
  shot("yazici_ayarsiz");
  p = printerSample(rc::K2_PRINTING, true);
  sim::publishPrinter(p);
  ui::go(ui::SCR_HOME, 1);
  run(1000);
  p.st.state = 5;
  p.st.runout = 1;
  p.phase = rc::k2Phase(p.st);
  p.problem = rc::k2Problem(p.st) ? 1 : 0;
  sim::publishPrinter(p);
  run(900);
  shot("yazici_hata_otomatik");
  p = printerSample(rc::K2_PRINTING, true);
  sim::publishPrinter(p);
  ui::go(ui::SCR_HOME, 1);
  run(600);
  uint32_t nightPrint = localUtc(2027, 7, 31, 3, 10);
  sim::setUtc(nightPrint);
  run(2600);
  shot("gece_yazici");
  p.st.errCode = 2845;
  p.phase = rc::k2Phase(p.st);
  p.problem = 1;
  sim::publishPrinter(p);
  run(1200);
  shot("gece_yazici_hata");
}

rc::Match s_match;

void matchBase(uint32_t kickUtc, uint8_t phase) {
  rc::matchClear(s_match);
  s_match.valid = 1;
  s_match.eventId = 401915403;
  s_match.teamId = 436;
  s_match.usHome = 1;
  s_match.league = rc::ML_UCL;
  s_match.kickoffUtc = kickUtc;
  s_match.phase = phase;
  s_match.live = 1;
  s_match.home.id = 436;
  snprintf(s_match.home.name, sizeof(s_match.home.name), "Fenerbahçe");
  snprintf(s_match.home.abbr, sizeof(s_match.home.abbr), "FB");
  s_match.away.id = 362;
  snprintf(s_match.away.name, sizeof(s_match.away.name), "Aston Villa");
  snprintf(s_match.away.abbr, sizeof(s_match.away.abbr), "AVL");
  snprintf(s_match.venue, sizeof(s_match.venue), "Ülker Stadyumu");
}

void matchEvent(uint8_t kind, bool ours, const char* minute, const char* player, const char* player2) {
  bool goal = kind == rc::MK_GOAL || kind == rc::MK_PEN_GOAL || kind == rc::MK_OWN_GOAL;
  if (goal && ours) s_match.homeScore++;
  else if (goal) s_match.awayScore++;
  rc::MatchEvent& e = s_match.ev[s_match.n++];
  memset(&e, 0, sizeof(e));
  e.id = 5000u + s_match.seq;
  e.kind = kind;
  e.ours = ours ? 1 : 0;
  e.home = s_match.homeScore;
  e.away = s_match.awayScore;
  snprintf(e.minute, sizeof(e.minute), "%s", minute);
  snprintf(e.player, sizeof(e.player), "%s", player);
  snprintf(e.player2, sizeof(e.player2), "%s", player2);
  e.seq = ++s_match.seq;
  snprintf(s_match.clock, sizeof(s_match.clock), "%s", minute);
  store::putT(store::MATCH, s_match);
}

void playlistScenario() {
  Settings awake = settings::get();
  awake.eyeSleepMin = 0;
  settings::set(awake);
  sim::setUtc(localUtc(2026, 10, 4, 20, 30));
  sim::setMedia(true, true, 83000, 214000, 0xFF4FA3, "Gece Yarısı Neon Işıklar Altında (Uzun Versiyon)",
                "Rumy & Arkadaşları", "Masaüstü Şarkıları, Cilt 1", true);
  lv_display_trigger_activity(nullptr);
  ui::go(ui::SCR_MEDIA, 0);
  run(900);
  shot("spotify_liste_dugmesi");
  sim::setPlaylists(rc::PLS_LOADING, 0, -1, true, -1, rc::PLR_NONE, "");
  lv_display_trigger_activity(nullptr);
  ui::go(ui::SCR_PLAYLIST, -1);
  run(900);
  shot("liste_yukleniyor");
  sim::setPlaylists(rc::PLS_READY, 14, 1, true, -1, rc::PLR_NONE, "");
  run(500);
  shot("liste");
  sim::setPlaylists(rc::PLS_READY, 14, 1, true, 3, rc::PLR_STARTING, "");
  run(500);
  shot("liste_baslatiliyor");
  sim::setPlaylists(rc::PLS_READY, 14, 1, false, 2, rc::PLR_FAIL, "Spotify Premium gerekli");
  run(500);
  shot("liste_hata");
  sim::setPlaylists(rc::PLS_LOGIN, 0, -1, true, -1, rc::PLR_NONE, "");
  run(500);
  shot("liste_giris");
  sim::setPlaylists(rc::PLS_FAIL, 0, -1, true, -1, rc::PLR_NONE, "Spotify'a ulaşılamadı");
  run(500);
  shot("liste_ag_hatasi");
  ui::go(ui::SCR_MEDIA, 0);
  run(600);
  sim::setPlaylists(rc::PLS_READY, 14, 12, true, -1, rc::PLR_NONE, "");
  lv_display_trigger_activity(nullptr);
  ui::go(ui::SCR_PLAYLIST, -1);
  run(900);
  shot("liste_asagisi");
  ui::go(ui::SCR_HOME, 0);
  run(600);
}

void matchScenario() {
  sim::setMedia(false, false, 0, 0, 0, "", "", "", false);
  simAdvance(cfg::QUAKE_ALERT_PIN_S * 1000UL + 60000UL);
  Settings awake = settings::get();
  awake.eyeSleepMin = 0;
  settings::set(awake);
  lv_display_trigger_activity(nullptr);
  uint32_t day = localUtc(2026, 10, 20, 17, 10);
  sim::setUtc(day);
  publishForecast(day, false);
  ui::go(ui::SCR_HOME, 0);
  run(800);
  matchBase(localUtc(2026, 10, 20, 19, 45), rc::MP_PRE);
  store::putT(store::MATCH, s_match);
  run(400);
  shot("mac_gunu");
  sim::setUtc(localUtc(2026, 10, 20, 19, 58));
  s_match.phase = rc::MP_LIVE;
  snprintf(s_match.clock, sizeof(s_match.clock), "12'");
  store::putT(store::MATCH, s_match);
  run(1200);
  shot("mac_canli");
  matchEvent(rc::MK_GOAL, true, "26'", "Edin Džeko", "");
  run(400);
  shot("mac_gol");
  run(9000);
  shot("mac_gol_sonrasi");
  matchEvent(rc::MK_GOAL, false, "38'", "Ollie Watkins", "");
  run(400);
  shot("mac_rakip_gol");
  run(7000);
  matchEvent(rc::MK_YELLOW, true, "45+1'", "Mason Greenwood", "");
  run(400);
  shot("mac_sari_kart");
  run(7000);
  matchEvent(rc::MK_SUB, true, "61'", "Kerem Aktürkoğlu", "İrfan Can Kahveci");
  run(400);
  shot("mac_degisiklik");
  run(7000);
  matchEvent(rc::MK_GOAL, true, "67'", "Kerem Aktürkoğlu", "");
  run(9000);
  matchEvent(rc::MK_RED, false, "81'", "Ezri Konsa", "");
  run(400);
  shot("mac_kirmizi_kart");
  run(7000);
  shot("mac_canli_2");
  lv_display_trigger_activity(nullptr);
  ui::go(ui::SCR_MATCH, -1);
  run(900);
  shot("mac_ekrani");
  ui::go(ui::SCR_HOME, 1);
  run(600);
  s_match.phase = rc::MP_POST;
  s_match.endUtc = localUtc(2026, 10, 20, 21, 40);
  sim::setUtc(localUtc(2026, 10, 20, 21, 45));
  snprintf(s_match.clock, sizeof(s_match.clock), "90+4'");
  store::putT(store::MATCH, s_match);
  run(600);
  shot("mac_bitti");
  rc::matchClear(s_match);
  store::putT(store::MATCH, s_match);
  run(300);
}

void moodGallery() {
  lv_obj_t* scr = theme::makeScreen();
  static const rumy::Mood kMoods[9] = {rumy::NORMAL, rumy::HAPPY,     rumy::SLEEPY,    rumy::ASLEEP, rumy::SAD,
                                       rumy::SURPRISED, rumy::LOOKUP, rumy::CELEBRATE, rumy::CALM};
  static const uint8_t kAcc[9] = {rc::ACC_NONE, rc::ACC_NONE, rc::ACC_NONE, rc::ACC_NONE, rc::ACC_NONE,
                                  rc::ACC_NONE, rc::ACC_UMBRELLA, rc::ACC_PARTY, rc::ACC_FLAG};
  for (int i = 0; i < 9; i++) {
    int32_t x = (i % 3) * theme::S(106) + theme::S(1);
    int32_t y = (i / 3) * theme::S(80);
    lv_obj_t* face = rumy::create(scr, theme::S(104), theme::S(64), theme::S(22));
    lv_obj_set_pos(face, x, y);
    rumy::setAnimated(face, false);
    rumy::setMood(face, kMoods[i]);
    rumy::setAccessory(face, kAcc[i]);
    lv_obj_t* l = theme::makeLabel(scr, theme::fontS(), theme::C_TEXT2);
    lv_label_set_text(l, rumy::moodName(kMoods[i]));
    lv_obj_set_width(l, theme::S(104));
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(l, x, y + theme::S(62));
  }
  lv_screen_load(scr);
  lv_timer_handler();
}

void runToPhase(uint32_t cycle, uint32_t at) {
  while ((lv_tick_get() % cycle + cycle - at) % cycle >= 25) run(10);
}

void phaseFor(rumy::Effect fx) {
  if (fx == rumy::FX_STORM) runToPhase(4800, 40);
  else if (fx == rumy::FX_SUNGLASSES) runToPhase(5200, 220);
  else if (fx == rumy::FX_SWEAT) runToPhase(3200, 900);
  else if (fx == rumy::FX_SHIVER) runToPhase(2600, 1300);
}

void moodPage(int page) {
  if (page == 1) runToPhase(4200, 900);
  lv_obj_t* scr = theme::makeScreen();
  for (int i = 0; i < 4; i++) {
    int m = page * 4 + i;
    if (m >= rumy::MOOD_COUNT) break;
    lv_obj_t* face = rumy::create(scr, theme::S(180), theme::S(88), theme::S(52));
    lv_obj_set_pos(face, (i % 2) * theme::S(180), (i / 2) * theme::S(110));
    rumy::setAnimated(face, false);
    rumy::setMood(face, static_cast<rumy::Mood>(m));
    if (m == rumy::MUSIC) rumy::setMusic(face, true, lv_color_hex(0xFF4FA3), lv_color_hex(0xFFD60A));
  }
  lv_screen_load(scr);
  lv_timer_handler();
}

void publishNowWeather(uint8_t code, int16_t feels10, uint16_t gust10, uint8_t uv10, uint8_t isDay) {
  rc::Forecast f;
  store::peekT(store::FORECAST, f);
  f.now.code = code;
  f.now.temp10 = feels10;
  f.now.feels10 = feels10;
  f.now.gust10 = gust10;
  f.now.uv10 = uv10;
  f.now.isDay = isDay;
  store::putT(store::FORECAST, f);
}

struct DateCase {
  const char* name;
  int y, m, d, hh, mm;
  int16_t temp10, hi10, lo10;
  bool music;
};

void setToday(int16_t hi10, int16_t lo10) {
  rc::Daily daily;
  store::peekT(store::DAILY, daily);
  daily.days[0].tmax10 = hi10;
  daily.days[0].tmin10 = lo10;
  store::putT(store::DAILY, daily);
}

void dateCase(const DateCase& k) {
  sim::setUtc(localUtc(k.y, k.m, k.d, k.hh, k.mm));
  publishNowWeather(2, k.temp10, 150, 30, (k.hh >= 7 && k.hh < 19) ? 1 : 0);
  setToday(k.hi10, k.lo10);
  if (k.music) sim::setMedia(true, true, 30000, 200000, 0xFFD60A, "Ritim", "Rumy", "Dans", true);
  else sim::setMedia(false, false, 0, 0, 0, "", "", "", false);
  run(500);
  ui::go(ui::SCR_HOME, 0);
  run(600);
}

void dateGallery() {
  static const DateCase kCases[] = {
      {"pzt_24_agustos", 2026, 8, 24, 11, 11, 234, 251, 152, true},
      {"sal_25_agustos", 2026, 8, 25, 0, 0, -120, -50, -150, false},
      {"car_26_agustos", 2026, 8, 26, 8, 5, 50, 80, -30, true},
      {"per_27_agustos", 2026, 8, 27, 14, 32, 381, 402, 270, false},
      {"cum_28_agustos", 2026, 8, 28, 19, 47, 0, 40, -10, true},
      {"cmt_29_agustos", 2026, 8, 29, 23, 59, -31, 12, -88, false},
      {"paz_30_agustos", 2026, 8, 30, 7, 0, 171, 240, 110, true},
      {"car_7_ocak", 2026, 1, 7, 20, 48, 12, 35, -21, false},
      {"cmt_14_subat", 2026, 2, 14, 10, 10, 88, 112, 41, true},
      {"sal_3_mart", 2026, 3, 3, 13, 13, 145, 160, 70, false},
      {"sal_28_nisan", 2026, 4, 28, 18, 30, 199, 220, 120, true},
      {"sal_19_mayis", 2026, 5, 19, 9, 41, 226, 270, 160, false},
      {"pzt_1_haziran", 2026, 6, 1, 21, 21, 288, 330, 200, true},
      {"per_30_temmuz", 2026, 7, 30, 15, 55, 372, 391, 268, false},
      {"cum_28_temmuz", 2028, 7, 28, 16, 40, 355, 370, 251, true},
      {"paz_9_agustos", 2026, 8, 9, 12, 0, 310, 335, 220, true},
      {"cmt_26_eylul", 2026, 9, 26, 7, 30, 160, 230, 150, false},
      {"cmt_3_ekim", 2026, 10, 3, 19, 23, 170, 210, 140, true},
      {"sal_10_kasim", 2026, 11, 10, 9, 5, 110, 150, 60, false},
      {"per_31_aralik", 2026, 12, 31, 23, 45, -48, 20, -72, true},
  };
  for (const auto& k : kCases) {
    dateCase(k);
    shot(k.name);
  }
  printf("tamam: %d goruntu\n", s_shots);
}

}

namespace sim {

void run(uint32_t ms) { ::run(ms); }
void shot(const char* name) { ::shot(name); }
void film(const char* name, uint32_t ms) { ::film(name, ms); }
uint32_t localUtc(int y, int m, int d, int hh, int mm) { return ::localUtc(y, m, d, hh, mm); }
void forecast(uint32_t now, bool rain) { publishForecast(now, rain); }
void nowWeather(uint8_t code, int16_t feels10, uint16_t gust10, uint8_t uv10, uint8_t isDay) {
  publishNowWeather(code, feels10, gust10, uv10, isDay);
}
void today(int16_t hi10, int16_t lo10) { setToday(hi10, lo10); }
void fixtures() { publishFixtures(); }
void quake(uint32_t now, int16_t mag10, uint32_t ago, const char* place) { publishQuake(now, mag10, ago, place); }
void events(uint32_t now) { publishEvents(now); }
void bulbOn(bool on) { ::publishBulb(on); }
void netUp(uint32_t now, bool wifi) { ::publishNet(now, wifi); }
void defaults() { setupSettings(); }
PrinterState printer(uint8_t phase, bool connected) { return printerSample(phase, connected); }

}

int main(int argc, char** argv) {
  if (argc > 1) s_out = argv[1];
  setvbuf(stdout, nullptr, _IONBF, 0);
  store::init();
  settings::load();
  mem::init();
  lv_init();
  lv_tick_set_cb(tickCb);
  lv_display_t* disp = lv_display_create(W, H);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(disp, flushCb);
  lv_display_set_buffers(disp, mem::drawBuf1(), mem::drawBuf2(), cfg::DRAW_BUF_BYTES, LV_DISPLAY_RENDER_MODE_PARTIAL);
  audit::begin();
  char auditPath[600];
  snprintf(auditPath, sizeof(auditPath), "%s/denetim.txt", s_out);
  s_audit = fopen(auditPath, "w");

  uint32_t day = localUtc(2026, 9, 26, 14, 32);
  sim::setUtc(day);
  setupSettings();
  publishNet(day, true);
  publishForecast(day, true);
  publishFixtures();
  publishQuake(day, 32, 25 * 60, "Marmara Denizi, Armutlu (Yalova)");
  publishEvents(day);
  publishBulb(true);

  ui::begin();
  run(1500);
  if (argc > 2 && !strcmp(argv[2], "tarih")) {
    dateGallery();
    return 0;
  }
  if (argc > 2 && !strcmp(argv[2], "film")) {
    sim::films();
    printf("tamam: %d goruntu\n", s_shots);
    return 0;
  }
  if (argc > 2 && !strcmp(argv[2], "cesit")) {
    sim::variants();
    printf("tamam: %d goruntu\n", s_shots);
    return 0;
  }
  shot("ana_yagmur_yaklasiyor");

  publishForecast(sim::utc(), false);
  run(1200);
  shot("ana_isik_acik");

  publishBulb(false);
  run(5200);
  shot("ana_normal_kart");

  ui::go(ui::SCR_WEATHER, 1);
  publishForecast(sim::utc(), true);
  run(900);
  shot("hava_ust");
  lv_obj_scroll_to_y(lv_screen_active(), 218, LV_ANIM_OFF);
  run(200);
  shot("hava_saatlik");
  lv_obj_scroll_to_y(lv_screen_active(), 452, LV_ANIM_OFF);
  run(200);
  shot("hava_7_gun");
  lv_obj_scroll_to_y(lv_screen_active(), 700, LV_ANIM_OFF);
  run(200);
  shot("hava_detay");
  lv_obj_scroll_to_y(lv_screen_active(), 5000, LV_ANIM_OFF);
  run(200);
  shot("hava_alt");

  publishBulb(true);
  ui::go(ui::SCR_LIGHT, 1);
  run(900);
  shot("isik");
  Settings keep = settings::get();
  Settings unpaired = keep;
  unpaired.bulbMac[0] = 0;
  settings::set(unpaired);
  run(600);
  shot("isik_eslesmemis");
  settings::set(keep);
  run(300);

  ui::go(ui::SCR_SETTINGS, 1);
  run(900);
  shot("ayarlar_1");
  lv_obj_scroll_to_y(lv_screen_active(), 236, LV_ANIM_OFF);
  run(200);
  shot("ayarlar_2");
  lv_obj_scroll_to_y(lv_screen_active(), 5000, LV_ANIM_OFF);
  run(200);
  shot("ayarlar_3");
  ui::go(ui::SCR_DISPLAY_CFG, 1);
  run(900);
  shot("ayarlar_ekran");
  ui::go(ui::SCR_NIGHT_CFG, 1);
  run(900);
  shot("ayarlar_gece_modu");
  sim::publishNet();
  ui::go(ui::SCR_PHONE, 1);
  run(900);
  shot("ayarlar_telefon");

  ui::go(ui::SCR_DIAG, 1);
  run(900);
  shot("tanilama");
  lv_obj_scroll_to_y(lv_screen_active(), 5000, LV_ANIM_OFF);
  run(200);
  shot("tanilama_kaynaklar");
  ui::go(ui::SCR_PAIR, 1);
  run(900);
  shot("ampul_eslestirme");
  ui::go(ui::SCR_LOCATION, 1);
  run(900);
  shot("konum");
  ui::go(ui::SCR_INFO, 1);
  run(900);
  shot("hakkinda");

  ui::go(ui::SCR_HOME, -1);
  run(900);
  uint32_t quakeNow = sim::utc();
  publishQuake(quakeNow, 46, 50, "Marmara Denizi, Silivri (İstanbul)");
  run(1300);
  shot("ana_deprem_uyarisi");

  run(31000);
  publishNet(sim::utc(), false);
  run(1300);
  shot("ana_baglanti_yok");
  publishNet(sim::utc(), true);

  uint32_t night = localUtc(2026, 9, 27, 1, 30);
  sim::setUtc(night);
  publishEvents(night);
  run(2600);
  shot("gece");
  run(200);
  shot("gece_z2");
  run(200);
  shot("gece_z3");

  uint32_t fest = localUtc(2026, 10, 29, 10, 5);
  sim::setUtc(fest);
  publishEvents(fest);
  publishForecast(fest, false);
  run(3200);
  shot("ozel_gun_cumhuriyet");

  uint32_t anma = localUtc(2026, 11, 10, 9, 5);
  sim::setUtc(anma);
  publishEvents(anma);
  ui::go(ui::SCR_HOME, 0);
  run(3200);
  shot("anma_10_kasim");

  Settings sleepy = settings::get();
  sleepy.eyeSleepMin = 1;
  settings::set(sleepy);
  run(62000);
  shot("ana_goz_uykusu");

  sim::setMedia(true, true, 83000, 214000, 0xFF4FA3, "Gece Yarısı Neon Işıklar Altında (Uzun Versiyon)",
                "Rumy & Arkadaşları", "Masaüstü Şarkıları, Cilt 1", true);
  run(1500);
  shot("spotify");
  sim::setMediaVol(65);
  run(300);
  sim::setMediaVol(60);
  run(300);
  shot("spotify_ses");
  run(3000);
  sim::setMedia(true, false, 83000, 214000, 0x1ED760, "Árvíztűrő Tükörfúrógép", "Zoë Façade", "Ñandú", false);
  ui::go(ui::SCR_MEDIA, 0);
  run(900);
  shot("spotify_kapaksiz");
  sim::setMedia(false, false, 0, 0, 0, "", "", "", false);
  run(300);
  shot("spotify_kopru_yok");

  sim::setMedia(true, true, 30000, 200000, 0xFFD60A, "Ritim", "Rumy", "Dans", true);
  run(400);
  ui::go(ui::SCR_HOME, 1);
  run(700);
  shot("ana_muzik");
  run(130);
  shot("ana_muzik_2");
  run(130);
  shot("ana_muzik_3");
  const char* longTitle = "Gece Yarısı Neon Işıklar Altında (Uzun Versiyon)";
  sim::setMedia(true, true, 30000, 200000, 0xFFD60A, longTitle, "Rumy", "Dans", true);
  run(300);
  shot("ana_muzik_uzun_ad");
  run(4000);
  shot("ana_muzik_kayan");
  sim::setMedia(true, false, 30000, 200000, 0xFFD60A, longTitle, "Rumy", "Dans", true);
  run(300);
  shot("ana_muzik_duraklatildi");
  run(56000);
  shot("ana_muzik_gizlendi");
  sim::setMedia(true, true, 30000, 200000, 0xFFD60A, "Ritim", "Rumy", "Dans", true);
  uint32_t lateNight = localUtc(2026, 11, 11, 3, 10);
  sim::setUtc(lateNight);
  run(2600);
  shot("gece_muzik");
  uint32_t monday = localUtc(2026, 9, 28, 10, 0);
  sim::setUtc(monday);
  run(2600);
  shot("ana_muzik_kisa_gun");
  uint32_t july = localUtc(2027, 7, 30, 14, 0);
  sim::setUtc(july);
  run(1500);
  shot("ana_muzik_uzun_tarih");
  matchScenario();
  printerScenario();

  moodGallery();
  run(100);
  shot("rumy_ifadeler");

  ui::go(ui::SCR_HOME, 0);
  run(600);
  sim::setWipe(true);
  captureSlide("pi_gecis_ileri", ui::SCR_WEATHER, 1);
  run(600);
  captureSlide("pi_gecis_geri", ui::SCR_HOME, -1);
  sim::setWipe(false);
  run(300);

  publishBulb(true);
  sim::bulb().writeDenied = 1;
  sim::publishBulb();
  ui::go(ui::SCR_LIGHT, 1);
  run(900);
  shot("isik_komut_izni_yok");
  publishBulb(true);
  run(300);

  ui::go(ui::SCR_TOUCH_CAL, 1);
  run(600);
  shot("dokunmatik_ayari");
  ui::go(ui::SCR_HOME, -1);
  run(300);

  sim::setMedia(false, false, 0, 0, 0, "", "", "", false);
  sim::setUtc(localUtc(2026, 9, 26, 11, 0));
  ui::go(ui::SCR_WEATHER, 1);
  run(600);
  ui::go(ui::SCR_HOME, -1);
  run(600);
  publishForecast(sim::utc(), false);
  static const struct {
    const char* name;
    uint8_t code;
    int16_t feels10;
    uint16_t gust10;
    uint8_t uv10;
    uint8_t isDay;
  } kHomeWx[] = {{"hava_gunesli", 0, 220, 150, 40, 1},       {"hava_gunesli_sicak", 0, 290, 150, 70, 1},
                 {"hava_bunaltici", 1, 350, 100, 80, 1},     {"hava_kapali", 3, 180, 150, 20, 1},
                 {"hava_sisli", 45, 120, 80, 5, 1},          {"hava_cisenti", 53, 140, 200, 5, 1},
                 {"hava_yagmurlu", 63, 150, 250, 10, 1},     {"hava_karli", 73, -30, 200, 5, 1},
                 {"hava_firtinali", 95, 180, 400, 5, 1},     {"hava_ruzgarli", 2, 160, 650, 30, 1},
                 {"hava_soguk", 2, 0, 150, 10, 1},           {"hava_acik_gece", 0, 120, 100, 0, 0}};
  for (const auto& w : kHomeWx) {
    lv_display_trigger_activity(nullptr);
    publishNowWeather(w.code, w.feels10, w.gust10, w.uv10, w.isDay);
    run(2500);
    rc::Forecast now;
    store::peekT(store::FORECAST, now);
    phaseFor(rumy::weatherLook(rc::wxKind(now.now)).fx);
    shot(w.name);
  }
  publishForecast(sim::utc(), false);
  for (int page = 0; page < 4; page++) {
    moodPage(page);
    run(50);
    static const char* const kPages[4] = {"rumy_ifadeler_tum_1", "rumy_ifadeler_tum_2", "rumy_ifadeler_tum_3",
                                          "rumy_ifadeler_tum_4"};
    shot(kPages[page]);
  }
  publishForecast(sim::utc(), false);
  run(300);
  playlistScenario();
  printf("tamam: %d goruntu\n", s_shots);
  return 0;
}
