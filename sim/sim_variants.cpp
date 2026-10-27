#include <Arduino.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

#include "app/config.h"
#include "afad.h"
#include "app/store.h"
#include "espn.h"
#include "events_calc.h"
#include "k2.h"
#include "media_proto.h"
#include "nager.h"
#include "openmeteo.h"
#include "settings/settings.h"
#include "sim.h"
#include "timeutil.h"
#include "ui/ui.h"

namespace {

constexpr uint32_t DAY_Y = 2026;

uint32_t s_now = 0;

void put(char* dst, size_t cap, const char* src) {
  size_t n = strlen(src);
  if (n >= cap) n = cap - 1;
  memcpy(dst, src, n);
  dst[n] = 0;
}

void at(int y, int m, int d, int hh, int mm) {
  s_now = sim::localUtc(y, m, d, hh, mm);
  sim::setUtc(s_now);
}

void awake() {
  Settings s = settings::get();
  s.eyeSleepMin = 0;
  settings::set(s);
  lv_display_trigger_activity(nullptr);
}

void home() {
  lv_display_trigger_activity(nullptr);
  ui::go(ui::SCR_HOME, 0);
  sim::run(900);
}

void shotf(const char* fmt, const char* a) {
  char name[80];
  snprintf(name, sizeof(name), fmt, a);
  sim::shot(name);
}

void scrollShots(const char* base) {
  static const int32_t kY[] = {0, 218, 452, 700, 5000};
  static const char* const kN[] = {"ust", "saatlik", "7_gun", "detay", "alt"};
  for (int i = 0; i < 5; i++) {
    lv_obj_scroll_to_y(lv_screen_active(), kY[i], LV_ANIM_OFF);
    sim::run(200);
    char name[80];
    snprintf(name, sizeof(name), "%s_%s", base, kN[i]);
    sim::shot(name);
  }
}

void scrollAll(lv_obj_t* obj) {
  if (lv_obj_has_flag(obj, LV_OBJ_FLAG_SCROLLABLE) && lv_obj_get_scroll_bottom(obj) > 0)
    lv_obj_scroll_to_y(obj, 100000, LV_ANIM_OFF);
  uint32_t n = lv_obj_get_child_count(obj);
  for (uint32_t i = 0; i < n; i++) scrollAll(lv_obj_get_child(obj, static_cast<int32_t>(i)));
}

void clearMatch() {
  rc::Match m;
  rc::matchClear(m);
  store::putT(store::MATCH, m);
}

void clearPrinter() {
  PrinterState none;
  memset(&none, 0, sizeof(none));
  rc::k2Clear(none.st);
  sim::publishPrinter(none);
}

void personal(const char* name, uint8_t kind, int month, int day, uint16_t year) {
  rc::Religious rel[2] = {rc::religiousBest(2026, nullptr), rc::religiousBest(2027, nullptr)};
  rc::PersonalEvent pe;
  memset(&pe, 0, sizeof(pe));
  rc::utf8Copy(pe.name, sizeof(pe.name), name, strlen(name));
  pe.year = year;
  pe.month = static_cast<uint8_t>(month);
  pe.day = static_cast<uint8_t>(day);
  pe.kind = kind;
  rc::EventsSnapshot snap;
  rc::buildSnapshot(rel, 2, &pe, 1, rc::localDayNum(s_now), snap);
  store::putT(store::EVENTS, snap);
}

void reset() {
  sim::defaults();
  awake();
  at(DAY_Y, 9, 26, 14, 32);
  sim::netUp(s_now, true);
  sim::forecast(s_now, false);
  sim::fixtures();
  sim::quake(s_now, 32, 2 * 86400, "Marmara Denizi, Armutlu (Yalova)");
  sim::events(s_now);
  sim::bulbOn(true);
  sim::setMedia(false, false, 0, 0, 0, "", "", "", false);
  clearMatch();
  clearPrinter();
  sim::run(300);
}

void homeIcons() {
  static const struct {
    const char* name;
    uint8_t code;
    uint8_t day;
  } kIcons[] = {{"acik_gunduz", 0, 1},    {"acik_gece", 0, 0},     {"parcali_gunduz", 2, 1}, {"parcali_gece", 2, 0},
                {"kapali", 3, 1},         {"sis", 45, 1},          {"cisenti", 53, 1},       {"yagmur", 63, 1},
                {"karla_karisik", 66, 1}, {"kar", 73, 1},          {"saganak_gunduz", 80, 1}, {"saganak_gece", 80, 0},
                {"firtina", 95, 1},       {"dolu", 99, 1}};
  home();
  for (const auto& k : kIcons) {
    sim::nowWeather(k.code, 220, 150, 40, k.day);
    sim::run(2500);
    shotf("ana_ikon_%s", k.name);
  }
}

void homeTemps() {
  static const struct {
    const char* name;
    int16_t now10, hi10, lo10;
  } kTemps[] = {{"eksi25", -254, -180, -312}, {"eksi9", -96, -50, -121}, {"sifir", 0, 24, -6},
                {"kirk5", 452, 471, 318},     {"yuksek_dusuk_yok", 180, rc::NA16, rc::NA16}};
  home();
  for (const auto& k : kTemps) {
    sim::nowWeather(1, k.now10, 150, 40, 1);
    sim::today(k.hi10, k.lo10);
    sim::run(2500);
    shotf("ana_sicaklik_%s", k.name);
  }
  sim::nowWeather(1, rc::NA16, 150, 40, 1);
  sim::run(1500);
  sim::shot("ana_sicaklik_yok");
}

void homeNoData() {
  home();
  rc::Forecast f;
  rc::clearForecast(f);
  store::putT(store::FORECAST, f);
  rc::Daily d;
  rc::clearDaily(d);
  store::putT(store::DAILY, d);
  rc::AirQ a;
  memset(&a, 0, sizeof(a));
  store::putT(store::AIR, a);
  rc::Marine m;
  memset(&m, 0, sizeof(m));
  store::putT(store::MARINE, m);
  sim::run(1500);
  sim::shot("ana_veri_yok");
  for (int i = 0; i < 3; i++) {
    sim::run(5000);
    shotf("ana_veri_yok_kart_%s", i == 0 ? "1" : (i == 1 ? "2" : "3"));
  }
  ui::go(ui::SCR_WEATHER, 1);
  sim::run(900);
  sim::shot("hava_ekrani_veri_yok");
  home();
  sim::setUtc(0);
  sim::run(1500);
  sim::shot("ana_saat_esitlenmedi");
}

void homeNames() {
  static const struct {
    const char* shot;
    const char* name;
    const char* city;
    int hh;
  } kNames[] = {{"ana_uzun_ad_aksam", "Muhammed Mustafa Kemal", "İstanbul", 20},
                {"ana_uzun_ad_sabah", "Muhammed Mustafa Kemal", "İstanbul", 8},
                {"ana_ad_yok", "", "İstanbul", 14},
                {"ana_uzun_sehir", "Deniz", "Kahramanmaraş Onikişubat", 14},
                {"ana_sehir_yok", "Deniz", "", 14}};
  for (const auto& k : kNames) {
    Settings s = settings::get();
    snprintf(s.name, sizeof(s.name), "%s", k.name);
    snprintf(s.city, sizeof(s.city), "%s", k.city);
    settings::set(s);
    at(DAY_Y, 9, 26, k.hh, 5);
    home();
    sim::run(1500);
    sim::shot(k.shot);
  }
  ui::go(ui::SCR_SETTINGS, 1);
  sim::run(900);
  sim::shot("ayarlar_uzun_sehir");
  ui::go(ui::SCR_LOCATION, 1);
  sim::run(900);
  sim::shot("konum_uzun_sehir");
}

void homeBars() {
  home();
  rc::Forecast f;
  store::peekT(store::FORECAST, f);
  f.now.rh = 100;
  f.now.temp10 = 452;
  f.now.feels10 = 488;
  store::putT(store::FORECAST, f);
  rc::AirQ a;
  store::peekT(store::AIR, a);
  a.valid = 1;
  a.eaqi = 500;
  a.pm25x10 = 9999;
  a.pm10x10 = 9999;
  store::putT(store::AIR, a);
  sim::run(1500);
  sim::shot("ana_cubuk_en_yuksek");
  f.now.rh = 0;
  f.now.temp10 = -254;
  f.now.feels10 = -321;
  store::putT(store::FORECAST, f);
  a.eaqi = 0;
  a.pm25x10 = 0;
  a.pm10x10 = 0;
  store::putT(store::AIR, a);
  sim::run(1500);
  sim::shot("ana_cubuk_en_dusuk");
}

void homeCards() {
  home();
  rc::Forecast f;
  store::peekT(store::FORECAST, f);
  f.now.wind10 = 1234;
  f.now.gust10 = 1876;
  f.now.uv10 = 115;
  store::putT(store::FORECAST, f);
  sim::quake(s_now, 51, 3 * 86400, "Ege Denizi, Gökova Körfezi açıkları (Muğla)");
  personal("Okul arkadaşım Zeynep'in doğum günü", rc::EV_BIRTHDAY, 9, 28, 1994);
  PrinterState p = sim::printer(rc::K2_PRINTING, true);
  snprintf(p.st.file, sizeof(p.st.file), "%s", "Cok_Uzun_Bir_Dosya_Adi_Benchy_Kalibrasyon_Kupu_v12_final_SON.gcode");
  p.st.leftS = 99U * 3600U + 59U * 60U;
  sim::publishPrinter(p);
  sim::run(600);
  for (int i = 1; i <= 14; i++) {
    sim::run(5000);
    char name[40];
    snprintf(name, sizeof(name), "ana_kart_%02d", i);
    sim::shot(name);
  }
  clearPrinter();
}

void homeQuake() {
  home();
  simAdvance(cfg::QUAKE_ALERT_PIN_S * 1000UL + 60000UL);
  sim::run(500);
  sim::quake(sim::utc(), 78, 40, "Ege Denizi, Gökova Körfezi açıkları (Muğla)");
  sim::run(1300);
  sim::shot("ana_deprem_7_8");
  simAdvance(cfg::QUAKE_ALERT_PIN_S * 1000UL + 60000UL);
  sim::run(500);
}

void eventScreens() {
  personal("Okul arkadaşım Zeynep'in doğum günü", rc::EV_BIRTHDAY, 9, 26, 1994);
  home();
  sim::run(3200);
  sim::shot("ana_ozel_gun_uzun_ad");
  ui::go(ui::SCR_EVENT, 1);
  sim::run(900);
  sim::shot("ozel_gun_ekrani_uzun_ad");
  personal("Kulübümüzün kuruluşu ve şenlik", rc::EV_ANNIVERSARY, 9, 26, 1976);
  ui::go(ui::SCR_EVENT, 1);
  sim::run(900);
  sim::shot("ozel_gun_ekrani_50_yil");
}

void weatherCold() {
  at(2027, 1, 14, 7, 40);
  sim::forecast(s_now, true);
  rc::Forecast f;
  store::peekT(store::FORECAST, f);
  f.now.code = 75;
  f.now.temp10 = -184;
  f.now.feels10 = -263;
  f.now.wind10 = 612;
  f.now.gust10 = 1043;
  f.now.visDam = 3;
  f.now.rh = 97;
  f.now.precip100 = 1250;
  for (int i = 0; i < rc::HOURS; i++) {
    f.hours[i].temp10 = static_cast<int16_t>(-120 - i * 8);
    f.hours[i].code = i % 3 ? 73 : 86;
    f.hours[i].pop = 100;
  }
  store::putT(store::FORECAST, f);
  rc::Daily d;
  store::peekT(store::DAILY, d);
  for (int i = 0; i < rc::DAYS; i++) {
    d.days[i].tmax10 = static_cast<int16_t>(-60 - i * 15);
    d.days[i].tmin10 = static_cast<int16_t>(-221 - i * 12);
    d.days[i].code = 75;
    d.days[i].popMax = 100;
  }
  store::putT(store::DAILY, d);
  ui::go(ui::SCR_WEATHER, 1);
  sim::run(900);
  scrollShots("hava_kis");
  home();
  sim::run(1500);
  sim::shot("ana_kis");
}

void weatherMissing() {
  sim::forecast(s_now, false);
  rc::Forecast f;
  store::peekT(store::FORECAST, f);
  f.now.wind10 = rc::NAU16;
  f.now.gust10 = rc::NAU16;
  f.now.windDir = rc::NAU16;
  f.now.press10 = rc::NAU16;
  f.now.visDam = rc::NAU16;
  f.now.precip100 = rc::NAU16;
  f.now.rh = rc::NA8;
  f.now.uv10 = rc::NA8;
  f.now.feels10 = rc::NA16;
  f.m15.count = 0;
  f.hourCount = 5;
  store::putT(store::FORECAST, f);
  rc::Daily d;
  store::peekT(store::DAILY, d);
  d.count = 2;
  d.days[0].sunrise = 0;
  d.days[0].sunset = 0;
  store::putT(store::DAILY, d);
  rc::AirQ a;
  memset(&a, 0, sizeof(a));
  store::putT(store::AIR, a);
  rc::Marine m;
  memset(&m, 0, sizeof(m));
  store::putT(store::MARINE, m);
  ui::go(ui::SCR_WEATHER, 1);
  sim::run(900);
  scrollShots("hava_eksik");
  home();
  sim::run(1500);
  sim::shot("ana_eksik_veri");
}

void light() {
  Settings s = settings::get();
  snprintf(s.bulbName, sizeof(s.bulbName), "%s", "Oturma odası tavanı");
  settings::set(s);
  BulbState& b = sim::bulb();
  b.pilot = rc::pilotRgb(330, 100);
  b.pilot.on = 1;
  sim::publishBulb();
  ui::go(ui::SCR_LIGHT, 1);
  sim::run(900);
  sim::shot("isik_renkli_uzun_ad");
  b.pilot = rc::pilotTemp(6500, 10);
  b.pilot.on = 1;
  sim::publishBulb();
  sim::run(600);
  sim::shot("isik_kisik_soguk");
  b.online = 0;
  b.found = 0;
  sim::publishBulb();
  sim::run(600);
  sim::shot("isik_cevrimdisi");
  sim::bulbOn(true);
  BulbState& c = sim::bulb();
  c.candidateCount = 8;
  for (int i = 0; i < 8; i++) {
    c.candidates[i].ip = 192u | (168u << 8) | (1u << 16) | ((40u + static_cast<uint32_t>(i)) << 24);
    snprintf(c.candidates[i].mac, sizeof(c.candidates[i].mac), "a8bb50%06x", 0x10a0b0 + i * 4099);
  }
  sim::publishBulb();
  ui::go(ui::SCR_PAIR, 1);
  sim::run(900);
  sim::shot("eslestirme_8_aday");
  lv_obj_scroll_to_y(lv_screen_active(), 5000, LV_ANIM_OFF);
  sim::run(200);
  sim::shot("eslestirme_8_aday_alt");
  c.candidateCount = 0;
  c.scanning = 1;
  sim::publishBulb();
  ui::go(ui::SCR_PAIR, 1);
  sim::run(900);
  sim::shot("eslestirme_taraniyor");
  sim::bulbOn(true);
}

void media() {
  static const struct {
    const char* shot;
    bool playing;
    uint32_t pos, dur;
    const char* title;
    const char* artist;
    const char* album;
    bool art;
  } kMedia[] = {
      {"spotify_cok_uzun", true, 61000, 3725000,
       "Supercalifragilisticexpialidocious_Remix_Extended_2026_Version_Final", "Wolfgang Amadeus Mozart, Ludwig van Beethoven ve Arkadaşları",
       "Klasik Müziğin En Uzun Albüm Adı Ödülü Alan Derleme Cilt 12", true},
      {"spotify_emoji", true, 30000, 200000, "Gece \xF0\x9F\x8C\x99 Yolculuğu \xF0\x9F\x9A\x97", "DJ \xE2\x9C\xA8 Yıldız",
       "\xF0\x9F\x94\xA5 Hits", true},
      {"spotify_japonca", true, 30000, 261000, "\xE5\xA4\x9C\xE3\x81\xAB\xE9\xA7\x86\xE3\x81\x91\xE3\x82\x8B", "YOASOBI",
       "THE BOOK", true},
      {"spotify_sure_yok", true, 0, 0, "Canlı Yayın: Sabah Haberleri", "Radyo", "", false},
      {"spotify_sanatci_yok", false, 15000, 180000, "Adsız Kayıt 7", "", "", false},
      {"spotify_sure_asti", true, 250000, 200000, "Bitmeyen Şarkı", "Rumy", "Sonsuz", true},
  };
  for (const auto& k : kMedia) {
    sim::setMedia(true, k.playing, k.pos, k.dur, 0x1ED760, k.title, k.artist, k.album, k.art);
    lv_display_trigger_activity(nullptr);
    ui::go(ui::SCR_MEDIA, 0);
    sim::run(900);
    sim::shot(k.shot);
  }
  sim::setMedia(true, true, 30000, 200000, 0x1ED760, "Ritim", "Rumy", "Dans", true);
  sim::setMediaVol(0);
  sim::run(400);
  sim::shot("spotify_ses_0");
  sim::setMediaVol(100);
  sim::run(400);
  sim::shot("spotify_ses_100");
  sim::run(3000);
  sim::setMedia(true, true, 30000, 200000, 0xFFD60A, "Gece \xF0\x9F\x8C\x99 Yolculuğu \xF0\x9F\x9A\x97", "DJ", "", true);
  home();
  sim::run(600);
  sim::shot("ana_muzik_emoji");
  sim::setMedia(true, true, 30000, 200000, 0xFFD60A, "Supercalifragilisticexpialidocious_Remix_Extended", "Rumy", "", true);
  sim::run(600);
  sim::shot("ana_muzik_bosluksuz");
  sim::setMedia(false, false, 0, 0, 0, "", "", "", false);
  sim::run(300);
}

void playlists() {
  static const char* const kNames[] = {
      "Beğenilen Şarkılar",
      "\xF0\x9F\x94\xA5 Hits 2026 \xF0\x9F\x94\xA5",
      "Çok Çok Çok Uzun Bir Çalma Listesi Adı",
      "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW",
      "a",
      "\xE5\xA4\x9C\xE3\x81\xAE\xE6\x9B\xB2",
      "Boş Liste",
      "Dev Arşiv",
  };
  static const uint16_t kTotals[] = {9999, 50, 120, 1, 2, 12, 0, 10000};
  sim::setMedia(true, true, 30000, 200000, 0x1ED760, "Ritim", "Rumy", "Dans", true);
  sim::setPlaylists(rc::PLS_READY, 8, 2, true, -1, rc::PLR_NONE, "");
  sim::setPlaylistNames(kNames, kTotals, 8);
  lv_display_trigger_activity(nullptr);
  ui::go(ui::SCR_PLAYLIST, -1);
  sim::run(900);
  sim::shot("liste_uc_adlar");
  scrollAll(lv_screen_active());
  sim::run(200);
  sim::shot("liste_uc_adlar_alt");
  sim::setPlaylists(rc::PLS_READY, 8, 2, true, 3, rc::PLR_FAIL,
                    "Bu liste başlatılamadı çünkü cihaz bulunamadı ve zaman aşımı oldu");
  sim::setPlaylistNames(kNames, kTotals, 8);
  sim::run(500);
  sim::shot("liste_uzun_hata");
  static const char* kMany[rc::PL_MAX];
  static uint16_t kManyTotals[rc::PL_MAX];
  static char kManyBuf[rc::PL_MAX][24];
  for (int i = 0; i < rc::PL_MAX; i++) {
    snprintf(kManyBuf[i], sizeof(kManyBuf[i]), "Liste %d", i + 1);
    kMany[i] = kManyBuf[i];
    kManyTotals[i] = static_cast<uint16_t>(i * 7);
  }
  sim::setPlaylists(rc::PLS_READY, 30, 29, false, -1, rc::PLR_NONE, "");
  sim::setPlaylistNames(kMany, kManyTotals, rc::PL_MAX);
  ui::go(ui::SCR_MEDIA, 0);
  sim::run(600);
  lv_display_trigger_activity(nullptr);
  ui::go(ui::SCR_PLAYLIST, -1);
  sim::run(900);
  sim::shot("liste_30_son");
  sim::setMedia(false, false, 0, 0, 0, "", "", "", false);
  sim::run(300);
}

rc::Match s_m;

void matchEvent(uint8_t kind, bool ours, const char* minute, const char* player, const char* player2) {
  bool goal = kind == rc::MK_GOAL || kind == rc::MK_PEN_GOAL || kind == rc::MK_OWN_GOAL;
  if (goal && ours) s_m.homeScore++;
  else if (goal) s_m.awayScore++;
  if (s_m.n >= rc::MATCH_EVENTS) {
    memmove(&s_m.ev[0], &s_m.ev[1], sizeof(s_m.ev[0]) * (rc::MATCH_EVENTS - 1));
    s_m.n--;
  }
  rc::MatchEvent& e = s_m.ev[s_m.n++];
  memset(&e, 0, sizeof(e));
  e.id = 7000u + s_m.seq;
  e.kind = kind;
  e.ours = ours ? 1 : 0;
  e.home = s_m.homeScore;
  e.away = s_m.awayScore;
  snprintf(e.minute, sizeof(e.minute), "%s", minute);
  snprintf(e.player, sizeof(e.player), "%s", player);
  snprintf(e.player2, sizeof(e.player2), "%s", player2);
  e.seq = ++s_m.seq;
  snprintf(s_m.clock, sizeof(s_m.clock), "%s", minute);
}

void matchBase(uint32_t kick, uint8_t phase) {
  rc::matchClear(s_m);
  s_m.valid = 1;
  s_m.eventId = 501;
  s_m.teamId = 436;
  s_m.usHome = 0;
  s_m.league = rc::ML_UEL;
  s_m.kickoffUtc = kick;
  s_m.phase = phase;
  s_m.live = 1;
  s_m.home.id = 380;
  put(s_m.home.name, sizeof(s_m.home.name), "Wolverhampton Wanderers");
  snprintf(s_m.home.abbr, sizeof(s_m.home.abbr), "%s", "WOLVE");
  s_m.away.id = 436;
  snprintf(s_m.away.name, sizeof(s_m.away.name), "%s", "Fenerbahçe");
  snprintf(s_m.away.abbr, sizeof(s_m.away.abbr), "%s", "FB");
  put(s_m.venue, sizeof(s_m.venue), "Molineux Stadyumu, Wolverhampton");
}

void match() {
  at(DAY_Y, 10, 21, 12, 0);
  matchBase(sim::localUtc(DAY_Y, 10, 22, 22, 0), rc::MP_PRE);
  store::putT(store::MATCH, s_m);
  home();
  sim::run(5000);
  sim::shot("mac_yarin_uzun_ad");
  at(DAY_Y, 10, 22, 21, 50);
  sim::run(1200);
  sim::shot("mac_basliyor_uzun_ad");
  at(DAY_Y, 10, 22, 22, 50);
  s_m.phase = rc::MP_LIVE;
  snprintf(s_m.clock, sizeof(s_m.clock), "%s", "44'");
  store::putT(store::MATCH, s_m);
  sim::run(1200);
  static const char* const kPlayers[] = {"Abdülkerim Bardakcı", "Jhon Arias Andrés", "Mattéo Guendouzi",
                                         "Jørgen Strand Larsen", "Kerem Aktürkoğlu", "Rodrigo Gomes"};
  static const uint8_t kKinds[] = {rc::MK_GOAL, rc::MK_YELLOW, rc::MK_SUB, rc::MK_PEN_GOAL, rc::MK_OWN_GOAL, rc::MK_RED};
  char minute[8];
  for (int i = 0; i < 22; i++) {
    snprintf(minute, sizeof(minute), "%d'", 3 + i * 4);
    if (i == 21) snprintf(minute, sizeof(minute), "%s", "90+12'");
    matchEvent(kKinds[i % 6], i % 2 == 0, minute, kPlayers[i % 6], kKinds[i % 6] == rc::MK_SUB ? "İrfan Can Kahveci" : "");
  }
  s_m.homeScore = 7;
  s_m.awayScore = 6;
  store::putT(store::MATCH, s_m);
  sim::run(400);
  sim::shot("mac_gol_uzun_ad");
  sim::run(12000);
  sim::shot("mac_canli_uzun_ad");
  lv_display_trigger_activity(nullptr);
  ui::go(ui::SCR_MATCH, -1);
  sim::run(900);
  sim::shot("mac_ekrani_uzun");
  lv_obj_scroll_to_y(lv_screen_active(), 5000, LV_ANIM_OFF);
  sim::run(200);
  sim::shot("mac_ekrani_uzun_alt");
  s_m.phase = rc::MP_HALF;
  snprintf(s_m.clock, sizeof(s_m.clock), "%s", "İY");
  store::putT(store::MATCH, s_m);
  home();
  sim::run(1200);
  sim::shot("mac_devre_arasi");
  clearMatch();
  sim::run(300);
}

void printerScreens() {
  PrinterState p = sim::printer(rc::K2_PRINTING, true);
  snprintf(p.st.file, sizeof(p.st.file), "%s", "Cok_Uzun_Bir_Dosya_Adi_Benchy_Kalibrasyon_Kupu_v12_final_SON_SON.gcode");
  p.st.progress = 0;
  p.st.leftS = 99U * 3600U + 59U * 60U;
  p.st.jobS = 5;
  p.st.layer = 0;
  p.st.totalLayer = 9999;
  p.st.nozzle10 = 3000;
  p.st.nozzleTarget10 = 3000;
  p.st.bed10 = 1100;
  p.st.bedTarget10 = 1100;
  sim::publishPrinter(p);
  ui::go(ui::SCR_PRINTER, -1);
  sim::run(900);
  sim::shot("yazici_uzun_dosya");
  p.st.errCode = 99999;
  p.st.errKey = 7;
  put(p.st.errText, sizeof(p.st.errText), "E99999 Ekstruder motoru beklenmedik sekilde durdu");
  p.phase = rc::k2Phase(p.st);
  p.problem = 1;
  sim::publishPrinter(p);
  sim::run(600);
  sim::shot("yazici_uzun_hata");
  p = sim::printer(rc::K2_PRINTING, true);
  p.st.state = 77;
  p.phase = rc::k2Phase(p.st);
  sim::publishPrinter(p);
  sim::run(600);
  sim::shot("yazici_bilinmeyen_durum");
  clearPrinter();
  home();
}

void diagFails() {
  NetStatus& n = sim::net();
  static const int16_t kCodes[SRC_COUNT] = {-1, 500, 429, -11, 404, 503, 502, 400};
  for (int i = 0; i < SRC_COUNT; i++) {
    SourceStat& s = n.src[i];
    s.lastCode = kCodes[i];
    s.lastOk = 0;
    s.fails = static_cast<uint16_t>(99 + i * 1000);
    s.deferred = 12;
    s.lastOkUtc = i == 0 ? 0 : s_now - 3U * 86400U;
    s.nextInS = 3600;
  }
  n.rssi = -91;
  sim::publishNet();
  ui::go(ui::SCR_DIAG, 1);
  sim::run(900);
  sim::shot("tanilama_hatali");
  lv_obj_scroll_to_y(lv_screen_active(), 5000, LV_ANIM_OFF);
  sim::run(200);
  sim::shot("tanilama_hatali_alt");
  sim::netUp(s_now, true);
}

void together() {
  at(DAY_Y, 10, 22, 22, 50);
  sim::forecast(s_now, true);
  matchBase(sim::localUtc(DAY_Y, 10, 22, 22, 0), rc::MP_LIVE);
  snprintf(s_m.clock, sizeof(s_m.clock), "%s", "51'");
  store::putT(store::MATCH, s_m);
  PrinterState p = sim::printer(rc::K2_PRINTING, true);
  sim::publishPrinter(p);
  sim::setMedia(true, true, 30000, 200000, 0xFF4FA3, "Gece Yarısı Neon Işıklar Altında (Uzun Versiyon)", "Rumy", "", true);
  personal("Elif'in doğum günü", rc::EV_BIRTHDAY, 10, 24, 1990);
  sim::run(1500);
  home();
  sim::run(1500);
  sim::shot("ana_hepsi_bir_arada");
  sim::quake(sim::utc(), 61, 30, "Marmara Denizi, Silivri (İstanbul)");
  sim::run(1300);
  sim::shot("ana_hepsi_ve_deprem");
  ui::toast("Bellek yetersiz, ekran açılamadı; birkaç saniye sonra yeniden deneyin");
  sim::run(300);
  sim::shot("bildirim_uzun");
  simAdvance(cfg::QUAKE_ALERT_PIN_S * 1000UL + 60000UL);
  sim::run(500);
}

}

namespace sim {

void variants() {
  reset();
  homeIcons();
  reset();
  homeTemps();
  reset();
  homeBars();
  reset();
  homeNames();
  reset();
  homeCards();
  reset();
  homeQuake();
  reset();
  eventScreens();
  reset();
  weatherCold();
  reset();
  weatherMissing();
  reset();
  light();
  reset();
  media();
  reset();
  playlists();
  reset();
  match();
  reset();
  printerScreens();
  reset();
  diagFails();
  reset();
  together();
  reset();
  homeNoData();
}

}
