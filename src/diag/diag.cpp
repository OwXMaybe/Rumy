#include "diag.h"

#include <Arduino.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "agents/agents.h"
#include "app/app.h"
#include "app/config.h"
#include "app/mem.h"
#include "app/shared.h"
#include "app/store.h"
#include "board/display.h"
#include "board/sdcard.h"
#include "clock/clock.h"
#include "match/board.h"
#include "match/leagues.h"
#include "match/match.h"
#include "media/bridge.h"
#include "media/camera.h"
#include "media/fwupdate.h"
#include "media/media.h"
#include "net/fetch.h"
#include "net/net.h"
#include "printer/printer.h"
#include "prof.h"
#include "settings/settings.h"
#include "text_tr.h"
#include "ui/bench.h"
#include "ui/lvmem.h"
#include "ui/screens.h"
#include "ui/slide.h"
#include "ui/ui.h"
#include "net/relay.h"
#include "rumy/look.h"
#include "webui/webui.h"
#include "wiz/wiz.h"
#include "wx_mood.h"

#if RUMY_PI
#include "src/health_pi.h"
#endif

namespace diag {

namespace {

constexpr int TREND_N = 24;
constexpr uint32_t TREND_EVERY_S = 300;
constexpr uint32_t MAINT_CHECK_MS = 30000;

const char* s_reason = "?";
uint32_t s_lastLogS = 0;
uint32_t s_lastTrendS = 0;
uint32_t s_lastMaintMs = 0;
int32_t s_maintDay = -1;
uint32_t s_trendT[TREND_N];
uint32_t s_trendV[TREND_N];
int s_trendN = 0;
int s_trendHead = 0;
struct Input {
  char cmd[64];
  size_t len;
  bool mediaLine;
  bool skipLine;
};

Input s_in[2] = {};
Print* s_out = &Serial;
const char* const kSrcNames[SRC_COUNT] = {"hava+7gun", "deniz", "havakalitesi", "afad", "nager", "mac", "lig", "ligsayfa"};

void sampleTrend(uint32_t up) {
  s_trendT[s_trendHead] = up;
  s_trendV[s_trendHead] = mem::freeHeap();
  s_trendHead = (s_trendHead + 1) % TREND_N;
  if (s_trendN < TREND_N) s_trendN++;
}

void logLine() {
  LvglStats lv;
  store::peekT(store::LVGL, lv);
  NetStatus ns;
  store::peekT(store::NET, ns);
  note("diag", "up=%us heap=%u min=%u blok=%u lvgl=%u%%/%u%% fps=%u yigin ui=%u ag=%u wiz=%u tls=%u ert=%u wifi=%d egilim=%dB/sa",
       static_cast<unsigned>(uptimeS()), static_cast<unsigned>(mem::freeHeap()),
       static_cast<unsigned>(mem::minFreeHeap()), static_cast<unsigned>(mem::largestBlock()),
       static_cast<unsigned>(lv.usedPct), static_cast<unsigned>(lv.fragPct), static_cast<unsigned>(lv.fps),
       static_cast<unsigned>(lv.uiStackFree),
       static_cast<unsigned>(net::stackFree()), static_cast<unsigned>(wiz::stackFree()),
       static_cast<unsigned>(ns.tlsPeak), static_cast<unsigned>(ns.tlsDeferred), static_cast<int>(ns.rssi),
       static_cast<int>(heapTrendPerHour()));
}

void maintenance() {
  Settings s = settings::get();
  if (!s.maintReboot || !clk::synced() || uptimeS() < 3600) return;
  rc::LocalTime lt = clk::local();
  if (lt.hour != 4 || lt.minute >= 10 || s_maintDay == lt.dayNum) return;
  s_maintDay = lt.dayNum;
  note("diag", "04:00 bakim yeniden baslatmasi");
  app::request(app::ACT_REBOOT, 1000);
}

void printBulb() {
  BulbState b;
  store::peekT(store::BULB, b);
  Settings s = settings::get();
  s_out->printf("wiz eslesik %s %s · bulundu %u · cevrimici %u · ip %u.%u.%u.%u · tarama %u · komut izni %s\n",
                s.bulbMac[0] ? s.bulbMac : "yok", s.bulbName, static_cast<unsigned>(b.found),
                static_cast<unsigned>(b.online), static_cast<unsigned>(b.ip & 0xFF),
                static_cast<unsigned>((b.ip >> 8) & 0xFF), static_cast<unsigned>((b.ip >> 16) & 0xFF),
                static_cast<unsigned>((b.ip >> 24) & 0xFF), static_cast<unsigned>(b.scanning),
                b.writeDenied ? "YOK (WiZ uygulamasi > Guvenlik)" : "var");
  if (b.pilot.valid) {
    const char* mode = b.pilot.mode == rc::PILOT_TEMP ? "beyaz" : b.pilot.mode == rc::PILOT_SCENE ? "sahne" : "renk";
    s_out->printf("wiz isik %s · parlaklik %u · kip %s · sahne %u · %u K · ton %u · rgb %u,%u,%u\n",
                  b.pilot.on ? "acik" : "kapali", static_cast<unsigned>(b.pilot.dim), mode,
                  static_cast<unsigned>(b.pilot.scene), static_cast<unsigned>(b.pilot.temp),
                  static_cast<unsigned>(b.pilot.hue), static_cast<unsigned>(b.pilot.r), static_cast<unsigned>(b.pilot.g),
                  static_cast<unsigned>(b.pilot.b));
  }
  for (uint8_t i = 0; i < b.candidateCount && i < rc::BulbList::CAP; i++) {
    uint32_t ip = b.candidates[i].ip;
    s_out->printf("wiz aday %s %u.%u.%u.%u\n", b.candidates[i].mac, static_cast<unsigned>(ip & 0xFF),
                  static_cast<unsigned>((ip >> 8) & 0xFF), static_cast<unsigned>((ip >> 16) & 0xFF),
                  static_cast<unsigned>((ip >> 24) & 0xFF));
  }
}

void wizCommand(const char* a) {
  BulbState b;
  store::peekT(store::BULB, b);
  rc::Pilot p = b.pilot.valid ? b.pilot : rc::pilotTemp(2700, 60);
  unsigned v = 0;
  if (!a[0] || !strcmp(a, "durum")) {
    printBulb();
    return;
  }
  if (!strcmp(a, "tara")) {
    wiz::scan();
  } else if (!strncmp(a, "esle ", 5)) {
    wiz::pair(a + 5);
  } else if (!strcmp(a, "unut")) {
    wiz::forget();
  } else if (!strcmp(a, "uyari")) {
    wiz::alert();
  } else if (!strcmp(a, "ac")) {
    p.valid = 1;
    p.on = 1;
    wiz::setDesired(rc::pilotClamp(p), true);
  } else if (!strcmp(a, "kapat")) {
    wiz::setDesired(rc::pilotOff(p), true);
  } else if (sscanf(a, "parlak %u", &v) == 1 && v <= 100) {
    p.valid = 1;
    p.on = 1;
    p.dim = static_cast<uint8_t>(v);
    wiz::setDesired(rc::pilotClamp(p), true);
  } else if (sscanf(a, "sicak %u", &v) == 1 && v <= 10000) {
    wiz::setDesired(rc::pilotTemp(static_cast<uint16_t>(v), p.dim ? p.dim : 60), true);
  } else if (sscanf(a, "renk %u", &v) == 1 && v < 360) {
    wiz::setDesired(rc::pilotRgb(static_cast<uint16_t>(v), p.dim ? p.dim : 60), true);
  } else if (sscanf(a, "sahne %u", &v) == 1 && v >= 1 && v <= 40) {
    p.valid = 1;
    p.on = 1;
    p.mode = rc::PILOT_SCENE;
    p.scene = static_cast<uint8_t>(v);
    wiz::setDesired(rc::pilotClamp(p), true);
  } else {
    s_out->println(F("kullanim: wiz durum|tara|esle <mac>|unut|ac|kapat|parlak 10-100|sicak 2200-6500|renk 0-359|"
                     "sahne 1-40|uyari"));
    return;
  }
  s_out->printf("wiz %s\n", a);
}

void weatherCommand(const char* a) {
  static const char* const kNames[rc::WXK_COUNT] = {"normal", "gunesli", "sicak",   "bunaltici", "kapali",
                                                    "sisli",  "cisenti", "yagmur",  "kar",       "firtina",
                                                    "ruzgar", "soguk",   "gece"};
  if (!strcmp(a, "kapat")) {
    scr_home::forceWeather(0xFF, 0);
    s_out->println(F("hava ifadesi gercek havaya dondu"));
    return;
  }
  for (uint8_t i = 0; i < rc::WXK_COUNT; i++) {
    if (!strcmp(a, kNames[i])) {
      scr_home::forceWeather(i, cfg::WX_FORCE_MS);
      s_out->printf("hava ifadesi: %s (%u sn)\n", rc::wxKindName(static_cast<rc::WxKind>(i)),
                    static_cast<unsigned>(cfg::WX_FORCE_MS / 1000U));
      return;
    }
  }
  s_out->print(F("kullanim: hava kapat"));
  for (uint8_t i = 0; i < rc::WXK_COUNT; i++) s_out->printf("|%s", kNames[i]);
  s_out->println();
}

ui::ScreenId screenByName(const char* n) {
  static const struct {
    const char* name;
    ui::ScreenId id;
  } kScreens[] = {{"ana", ui::SCR_HOME},        {"hava", ui::SCR_WEATHER},      {"isik", ui::SCR_LIGHT},
                  {"ayarlar", ui::SCR_SETTINGS}, {"tanilama", ui::SCR_DIAG},     {"bilgi", ui::SCR_INFO},
                  {"spotify", ui::SCR_MEDIA},    {"yazici", ui::SCR_PRINTER},    {"konum", ui::SCR_LOCATION},
                  {"gorunum", ui::SCR_DISPLAY_CFG}, {"dokunmatik", ui::SCR_TOUCH_CAL}, {"mac", ui::SCR_MATCH}, {"liste", ui::SCR_PLAYLIST},
                  {"futbol", ui::SCR_FOOTBALL}, {"ajan", ui::SCR_AGENTS}, {"telefon", ui::SCR_PHONE}};
  for (const auto& s : kScreens) {
    if (!strcmp(n, s.name)) return s.id;
  }
  return ui::SCR_NONE;
}

void command(const char* c) {
  if (!strcmp(c, "help") || !strcmp(c, "yardim")) {
    s_out->println(F("komutlar: rapor | mem | yenile | web ac | web kapat | telefon unut | gorunum <i> <a> <e>|oto | futbol ac|kapat | sdtest | dokunmatik 0-7 | "
                     "goruntu ters|duz | goz uykusu kapat|1-60 | kopru | kopru anahtar <32 hex>|sil | bench | "
                     "bench bellek | lvgl yuk <KB> | perf ac | perf kapat | gece 02:00 07:00 | yazici <ip> | "
                     "yazici kapat | ekran ana|hava|isik|ayarlar|tanilama|bilgi|spotify|liste|yazici|konum|gorunum|mac|"
                     "futbol|ajan|dokunmatik | ajan | ajan test calisiyor|bekliyor|bitti|codex|kapat | "
                     "ekran goruntusu | hava <tur>|kapat | "
                     "wiz durum|tara|esle <mac>|unut|ac|kapat|parlak|sicak|renk|sahne|uyari | reboot | "
                     "takim <espn-no>|kapat | mac | mac yenile | lig yenile | ligsayfa <lig> [puan] | "
                     "mac test once|basla|gol [1-4]|rakip|iptal|sari|kirmizi|degisiklik|devre|bitti|kapat | wifi-sifirla"));
#if RUMY_PI
    s_out->println(F("pi: bekci | bekci dur"));
  } else if (!strcmp(c, "bekci")) {
    char line[160];
    pi::watchdogStatus(line, sizeof(line));
    s_out->println(line);
  } else if (!strcmp(c, "bekci dur")) {
    pi::watchdogHold();
    s_out->println(F("bekci bildirimi durdu; systemd Rumy'yi yeniden baslatmali"));
#endif
  } else if (!strcmp(c, "wiz") || !strncmp(c, "wiz ", 4)) {
    wizCommand(c[3] ? c + 4 : "");
  } else if (!strcmp(c, "hava") || !strncmp(c, "hava ", 5)) {
    weatherCommand(c[4] ? c + 5 : "");
  } else if (!strcmp(c, "ekran goruntusu")) {
    display::dumpScreen();
  } else if (!strncmp(c, "ekran ", 6) && screenByName(c + 6) != ui::SCR_NONE) {
    ui::ScreenId id = screenByName(c + 6);
    lv_display_trigger_activity(nullptr);
    ui::go(id, id == ui::SCR_HOME ? 1 : -1);
    s_out->println(c);
  } else if (!strncmp(c, "yazici ", 7)) {
    Settings s = settings::get();
    const char* host = c + 7;
    if (!strcmp(host, "kapat")) host = "";
    snprintf(s.printerHost, sizeof(s.printerHost), "%.*s", static_cast<int>(sizeof(s.printerHost) - 1), host);
    settings::set(s);
    Settings now = settings::get();
    if (host[0] && strcmp(now.printerHost, host) != 0) s_out->println(F("gecersiz adres, ornek: yazici 192.168.1.50"));
    else s_out->printf("yazici %s\n", now.printerHost[0] ? now.printerHost : "kapali");
  } else if (!strncmp(c, "dokunmatik ", 11) && c[11] >= '0' && c[11] <= '7' && !c[12]) {
    Settings s = settings::get();
    s.touchRot = static_cast<uint8_t>(c[11] - '0');
    settings::set(s);
    s_out->printf("dokunmatik yonu %u\n", static_cast<unsigned>(s.touchRot));
#if !RUMY_PI
  } else if (!strcmp(c, "goruntu ters") || !strcmp(c, "goruntu duz")) {
    bool on = c[8] == 't';
    Settings s = settings::get();
    s.flip = on ? 1 : 0;
    settings::set(s);
    display::setFlip(on);
    lv_obj_invalidate(lv_screen_active());
    s_out->println(c);
#endif
  } else if (!strncmp(c, "gece ", 5)) {
    unsigned h1, m1, h2, m2;
    if (sscanf(c + 5, "%u:%u %u:%u", &h1, &m1, &h2, &m2) == 4 && h1 < 24 && m1 < 60 && h2 < 24 && m2 < 60) {
      Settings s = settings::get();
      s.nightMode = NIGHT_HOURS;
      s.nightStart = static_cast<uint16_t>(h1 * 60 + m1);
      s.nightEnd = static_cast<uint16_t>(h2 * 60 + m2);
      settings::set(s);
      s_out->printf("gece modu %02u:%02u - %02u:%02u\n", h1, m1, h2, m2);
    } else {
      s_out->println(F("kullanim: gece 02:00 07:00"));
    }
  } else if (!strcmp(c, "rapor") || !strcmp(c, "mem")) {
    printReport(*s_out);
  } else if (!strcmp(c, "yenile")) {
    net::refreshAll();
    s_out->println(F("veriler yenileniyor"));
  } else if (!strcmp(c, "web ac")) {
    webui::setEnabled(true);
    s_out->println(F("web arayuzu aciliyor"));
  } else if (!strcmp(c, "gorunum oto")) {
    look::clear();
    s_out->println(F("gorunum otomatik"));
  } else if (!strncmp(c, "gorunum ", 8)) {
    unsigned m, a, f;
    if (sscanf(c + 8, "%u %u %u", &m, &a, &f) == 3 && m < 256 && a < 256 && f < 256 &&
        look::set(static_cast<uint8_t>(m), static_cast<uint8_t>(a), static_cast<uint8_t>(f))) {
      s_out->printf("gorunum %u %u %u (hal degisince otomatige doner)\n", m, a, f);
    } else {
      s_out->println(F("kullanim: gorunum <ifade 0-12> <aksesuar 0-3> <efekt 0-13> | gorunum oto"));
    }
  } else if (!strcmp(c, "telefon unut")) {
    webui::forgetPhones();
    s_out->println(F("esli telefonlar siliniyor"));
  } else if (!strcmp(c, "web kapat")) {
    webui::setEnabled(false);
  } else if (!strcmp(c, "bench")) {
    bench::start(bench::ALL);
  } else if (!strcmp(c, "bench gecis")) {
    bench::start(bench::TRANSITIONS);
  } else if (!strcmp(c, "bench bellek")) {
    bench::start(bench::MEMORY);
  } else if (!strncmp(c, "lvgl yuk ", 9)) {
    uint32_t kb = static_cast<uint32_t>(atoi(c + 9));
    bool ok = lvmem::hold(kb * 1024U);
    lvmem::Snap m = lvmem::snap();
    s_out->printf("lvgl yuk %u KB %s · bos %u · en buyuk %u\n", static_cast<unsigned>(lvmem::held() / 1024U),
                  ok ? "tutuluyor" : "ayrilamadi", static_cast<unsigned>(m.free), static_cast<unsigned>(m.biggest));
  } else if (!strcmp(c, "bench kaydir")) {
    bench::start(bench::SCROLLS);
  } else if (!strncmp(c, "spitest ", 8)) {
    uint32_t mhz = static_cast<uint32_t>(atoi(c + 8));
    if (mhz >= 10 && mhz <= 80) {
      uint32_t t0 = millis();
      uint32_t bad = display::spiTest(mhz * 1000000UL);
      s_out->printf("spitest %u MHz: %u hatali piksel / %u, %u ms\n", static_cast<unsigned>(mhz), static_cast<unsigned>(bad),
                    static_cast<unsigned>(cfg::SCREEN_W * cfg::SCREEN_H), static_cast<unsigned>(millis() - t0));
      lv_obj_invalidate(lv_screen_active());
    }
  } else if (!strcmp(c, "dokunma ac") || !strcmp(c, "dokunma kapat")) {
    display::setTouchLog(c[8] == 'a');
    s_out->println(c);
  } else if (!strcmp(c, "kayma ters") || !strcmp(c, "kayma duz")) {
    slide::setInvert(c[6] == 't');
    s_out->println(c);
  } else if (!strcmp(c, "perf ac")) {
    bench::setLive(true);
  } else if (!strcmp(c, "perf kapat")) {
    bench::setLive(false);
#if defined(RUMY_PROFILE) && RUMY_PROFILE
  } else if (!strcmp(c, "profil ac")) {
    prof::start();
    s_out->println(F("profil aciliyor"));
  } else if (!strcmp(c, "profil")) {
    prof::print(*s_out);
#endif
  } else if (!strncmp(c, "goz uykusu ", 11)) {
    unsigned m = 0;
    if (!strcmp(c + 11, "kapat") || (sscanf(c + 11, "%u", &m) == 1 && m <= 60)) {
      Settings s = settings::get();
      s.eyeSleepMin = static_cast<uint8_t>(m);
      settings::set(s);
      if (m) s_out->printf("goz uykusu %u dk\n", m);
      else s_out->println("goz uykusu kapali");
    } else {
      s_out->println("kullanim: goz uykusu kapat|1-60");
    }
  } else if (!strcmp(c, "futbol ac") || !strcmp(c, "futbol kapat")) {
    Settings s = settings::get();
    s.football = c[7] == 'a' ? 1 : 0;
    settings::set(s);
    match::refresh();
    net::refresh(SRC_BOARD);
    s_out->println(s.football ? "mac takibi acik" : "mac takibi kapali");
  } else if (!strncmp(c, "takim ", 6)) {
    unsigned long id = 0;
    unsigned long c1 = 0;
    unsigned long c2 = 0;
    bool off = !strcmp(c + 6, "kapat");
    int got = off ? 0 : sscanf(c + 6, "%lu %6lx %6lx", &id, &c1, &c2);
    if (off || (got >= 1 && id > 0)) {
      Settings s = settings::get();
      s.teamId = off ? 0 : static_cast<uint32_t>(id);
      s.teamC1 = got == 3 ? static_cast<uint32_t>(c1) : 0;
      s.teamC2 = got == 3 ? static_cast<uint32_t>(c2) : 0;
      settings::set(s);
      match::refresh();
      if (off) s_out->println("takim yok (yalnizca diger maclarin golleri)");
      else s_out->printf("takim %lu%s, fikstur yenileniyor\n", id, got == 3 ? " (renkler kaydedildi)" : "");
    } else {
      s_out->println("kullanim: takim <espn-no> [renk1 renk2]|kapat (or. takim 436 13235b ffd60a)");
    }
  } else if (!strcmp(c, "mac")) {
    match::report(*s_out);
    board::report(*s_out);
    leagues::report(*s_out);
  } else if (!strncmp(c, "ligsayfa ", 9)) {
    char slug[24];
    char what[8] = "";
    uint8_t league = 0;
    if (sscanf(c + 9, "%23s %7s", slug, what) >= 1) league = rc::espnLeague(slug);
    if (league >= rc::ML_SUPER_LIG && rc::espnLeagueSlug(league)[0]) {
      leagues::open(league);
      if (!strcmp(what, "puan")) leagues::showTable(true);
      ui::go(ui::SCR_LEAGUE, 1);
      s_out->printf("ligsayfa %s %s\n", slug, what[0] ? what : "gun");
    } else {
      s_out->println("kullanim: ligsayfa tur.1|uefa.champions|eng.1|esp.1|ita.1|ger.1|fra.1 [puan]");
    }
  } else if (!strcmp(c, "lig yenile")) {
    board::refresh();
    s_out->println("super lig ve sampiyonlar ligi skorlari yenileniyor");
  } else if (!strcmp(c, "mac yenile")) {
    match::refresh();
    s_out->println("mac fiksturu yenileniyor");
  } else if (!strncmp(c, "mac test ", 9)) {
    if (match::demo(c + 9)) s_out->printf("mac test %s\n", c + 9);
    else s_out->println("kullanim: mac test once|basla|gol|rakip|sari|kirmizi|degisiklik|devre|bitti|kapat");
  } else if (!strcmp(c, "ajan")) {
    agents::report(*s_out);
  } else if (!strncmp(c, "ajan test ", 10)) {
    if (agents::demo(c + 10)) s_out->printf("ajan test %s\n", c + 10);
    else s_out->println("kullanim: ajan test calisiyor|bekliyor|bitti|codex|kapat");
  } else if (!strcmp(c, "kopru")) {
    bridge::report(*s_out);
  } else if (!strncmp(c, "kopru anahtar ", 14)) {
    const char* k = c + 14;
    bool wipe = !strcmp(k, "sil");
    if (bridge::setKey(wipe ? "" : k)) s_out->println(wipe ? "kopru anahtari silindi" : "kopru anahtari kaydedildi");
    else s_out->println("kopru anahtari 32 onaltilik karakter olmali");
  } else if (!strcmp(c, "sdtest")) {
    uint32_t ok = 0;
    bool pass = sdcard::selfTest(50, ok);
    s_out->printf("sdtest %u/50 %s\n", static_cast<unsigned>(ok), pass ? "OK" : "HATA");
  } else if (!strcmp(c, "reboot")) {
    app::request(app::ACT_REBOOT, 200);
  } else if (!strcmp(c, "wifi-sifirla")) {
    app::request(app::ACT_WIFI_RESET, 200);
  } else if (c[0]) {
    s_out->println(F("bilinmeyen komut, 'help' yaz"));
  }
}

int srcAvailable(int src) { return src ? bridge::tcpAvailable() : Serial.available(); }

int srcRead(int src) { return src ? bridge::tcpRead() : Serial.read(); }

void readFrom(int src) {
  Input& in = s_in[src];
  bool mediaSrc = (src == 1) == bridge::wifi();
  while (srcAvailable(src) > 0) {
    if (mediaSrc && media::discarding()) {
      media::discard();
      continue;
    }
    if (mediaSrc && fwupdate::receiving()) {
      if (!fwupdate::receive()) break;
      continue;
    }
    if (mediaSrc && media::receiving()) {
      if (!media::receive()) break;
      continue;
    }
    int ch = srcRead(src);
    if (ch < 0) break;
    if (in.mediaLine) {
      if (ch == '\n') {
        in.mediaLine = false;
        if (!in.skipLine) media::lineEnd();
      } else if (ch != '\r' && !in.skipLine) {
        media::lineChar(static_cast<char>(ch));
      }
      continue;
    }
    if (ch == '@' && in.len == 0) {
      in.mediaLine = true;
      in.skipLine = !mediaSrc;
      if (mediaSrc) media::lineBegin();
      continue;
    }
    if (ch == '\r') continue;
    if (ch == '\n') {
      in.cmd[in.len] = 0;
      s_out = src ? &bridge::out() : static_cast<Print*>(&Serial);
      command(in.cmd);
      s_out = &Serial;
      memset(in.cmd, 0, sizeof(in.cmd));
      in.len = 0;
      continue;
    }
    if (in.len + 1 < sizeof(in.cmd)) in.cmd[in.len++] = static_cast<char>(ch);
  }
}

void readSerial() {
  readFrom(0);
  if (bridge::wifi()) readFrom(1);
}

}

void begin() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: s_reason = "güç verildi"; break;
    case ESP_RST_SW: s_reason = "yazılım"; break;
    case ESP_RST_PANIC: s_reason = "çökme"; break;
    case ESP_RST_INT_WDT: s_reason = "kesme wdt"; break;
    case ESP_RST_TASK_WDT: s_reason = "görev wdt"; break;
    case ESP_RST_WDT: s_reason = "wdt"; break;
    case ESP_RST_BROWNOUT: s_reason = "düşük voltaj"; break;
    case ESP_RST_EXT: s_reason = "harici"; break;
    case ESP_RST_DEEPSLEEP: s_reason = "uyku"; break;
    default: s_reason = "bilinmiyor"; break;
  }
  note("diag", "Rumy %s basliyor, panel %s, neden: %s, acilis #%u", RUMY_VERSION, RUMY_PANEL_NAME, s_reason,
       static_cast<unsigned>(settings::bootCount()));
}

void loop() {
  bridge::service(millis());
  readSerial();
  uint32_t up = uptimeS();
  if (up - s_lastLogS >= cfg::DIAG_LOG_S) {
    s_lastLogS = up;
    logLine();
  }
  if (s_trendN == 0 || up - s_lastTrendS >= TREND_EVERY_S) {
    s_lastTrendS = up;
    sampleTrend(up);
  }
  uint32_t now = millis();
  if (now - s_lastMaintMs >= MAINT_CHECK_MS) {
    s_lastMaintMs = now;
    maintenance();
  }
}

uint32_t uptimeS() { return static_cast<uint32_t>(esp_timer_get_time() / 1000000LL); }

const char* resetReason() { return s_reason; }

int32_t heapTrendPerHour() {
  if (s_trendN < 3) return 0;
  double n = s_trendN, sx = 0, sy = 0, sxx = 0, sxy = 0;
  for (int i = 0; i < s_trendN; i++) {
    double x = s_trendT[i] / 3600.0;
    double y = s_trendV[i];
    sx += x;
    sy += y;
    sxx += x * x;
    sxy += x * y;
  }
  double den = n * sxx - sx * sx;
  if (den <= 0) return 0;
  return static_cast<int32_t>((n * sxy - sx * sy) / den);
}

void note(const char* tag, const char* fmt, ...) {
  char buf[224];
  uint32_t up = uptimeS();
  int n = snprintf(buf, sizeof(buf), "[%5u.%03u] %-5s ", static_cast<unsigned>(up), static_cast<unsigned>(millis() % 1000), tag);
  if (n < 0) return;
  va_list ap;
  va_start(ap, fmt);
  int m = vsnprintf(buf + n, sizeof(buf) - static_cast<size_t>(n) - 1, fmt, ap);
  va_end(ap);
  size_t len = static_cast<size_t>(n) + (m > 0 ? static_cast<size_t>(m) : 0);
  if (len > sizeof(buf) - 2) len = sizeof(buf) - 2;
  buf[len++] = '\n';
  Serial.write(reinterpret_cast<const uint8_t*>(buf), len);
}

void printReport(Print& out) {
  LvglStats lv;
  store::peekT(store::LVGL, lv);
  NetStatus ns;
  store::peekT(store::NET, ns);
  BulbState b;
  store::peekT(store::BULB, b);
  char dur[24];
  rc::durationTr(uptimeS(), dur, sizeof(dur));
  out.printf("===== Rumy %s (%s) =====\n", RUMY_VERSION, RUMY_PANEL_NAME);
  out.printf("calisma %s · acilis #%u · neden %s\n", dur, static_cast<unsigned>(settings::bootCount()), s_reason);
  mem::report(out);
  out.printf("[mem] egilim %d B/sa (%d ornek)\n", static_cast<int>(heapTrendPerHour()), s_trendN);
  out.printf("[lvgl] %u/%u B · tepe %u · %%%u parca · en buyuk %u · fps %u · reddedilen ekran %u\n",
             static_cast<unsigned>(lv.used), static_cast<unsigned>(lv.total), static_cast<unsigned>(lv.maxUsed),
             static_cast<unsigned>(lv.fragPct), static_cast<unsigned>(lv.biggestFree), static_cast<unsigned>(lv.fps),
             static_cast<unsigned>(ui::lowMemRefusals()));
  out.printf("[yigin bos] ui %u · ag %u · wiz %u\n", static_cast<unsigned>(lv.uiStackFree),
             static_cast<unsigned>(net::stackFree()), static_cast<unsigned>(wiz::stackFree()));
  out.printf("[ag] wifi %d · rssi %d · saat %s · tls tepe %u · ertelenen %u · web %s · json tepe %u/%u B\n", ns.wifi,
             ns.rssi, ns.timeSynced ? "eşitli" : "yok", static_cast<unsigned>(ns.tlsPeak),
             static_cast<unsigned>(ns.tlsDeferred), ns.webActive ? "açık" : "kapalı",
             static_cast<unsigned>(net::arena().peak()), static_cast<unsigned>(net::arena().capacity()));
  uint32_t now = clk::nowUtc();
  for (int i = 0; i < SRC_COUNT; i++) {
    const SourceStat& s = ns.src[i];
    static const char* const kVia[] = {"http", "https", "kopru", "kandilli"};
    out.printf("  %-13s kod %4d · %s · %-8s · %5u ms · %6u B · hata %u · ert %u · son OK %lds · sonraki %us\n",
               kSrcNames[i], s.lastCode, s.lastOk ? "OK " : "---", s.via < 4 ? kVia[s.via] : "?",
               static_cast<unsigned>(s.durMs), static_cast<unsigned>(s.bytes),
               static_cast<unsigned>(s.fails), static_cast<unsigned>(s.deferred),
               s.lastOkUtc && now ? static_cast<long>(now - s.lastOkUtc) : -1L, static_cast<unsigned>(s.nextInS));
  }
  out.printf("[sd] %s · %u/%u MB · yazma %u · okuma hata %u · yazma hata %u\n", sdcard::mounted() ? "takili" : "yok",
             static_cast<unsigned>(sdcard::usedBytes() / 1048576ULL),
             static_cast<unsigned>(sdcard::totalBytes() / 1048576ULL), static_cast<unsigned>(sdcard::writes()),
             static_cast<unsigned>(sdcard::readErrors()), static_cast<unsigned>(sdcard::writeErrors()));
  media::report(out);
  cam::report(out);
  bridge::report(out);
  agents::report(out);
  fwupdate::report(out);
  look::Pick lk = look::shown();
  out.printf("[gorunum] %s · ifade %u · aksesuar %u · efekt %u\n", lk.mine ? "secim" : "otomatik",
             static_cast<unsigned>(lk.mood), static_cast<unsigned>(lk.acc), static_cast<unsigned>(lk.fx));
  webui::report(out);
  relay::report(out);
  out.printf("[ampul] esli %u · bulundu %u · cevrimici %u · aday %u · mac %s\n", b.paired, b.found, b.online,
             b.candidateCount, b.mac[0] ? b.mac : "-");
  printer::report(out);
  match::report(out);
  board::report(out);
}

}
