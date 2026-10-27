#include <Arduino.h>
#include <esp_random.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/app.h"
#include "app/config.h"
#include "app/mem.h"
#include "app/store.h"
#include "board/display.h"
#include "board/sdcard.h"
#include "clock/clock.h"
#include "diag/diag.h"
#include "media/camera.h"
#include "media/fwupdate.h"
#include "media/media.h"
#include "net/net.h"
#include "settings/settings.h"
#include "sim.h"
#include "match/leagues.h"
#include "sim_paths.h"
#include "webui/webui.h"
#include "wiz/wiz.h"

SimSerial Serial;
SimEsp ESP;

namespace {

bool s_wipe = false;
uint32_t s_ms = 1000;
uint32_t s_utcBase = 0;
uint32_t s_msBase = 1000;
uint32_t s_lastTouch = 0;
BulbState s_bulb;
NetStatus s_net;
bool s_web = false;
app::Action s_action = app::ACT_NONE;
uint8_t* s_bufs[4] = {};

}

uint32_t millis() { return s_ms; }

uint32_t micros() { return s_ms * 1000U; }

void simAdvance(uint32_t ms) { s_ms += ms; }

uint32_t esp_random() { return static_cast<uint32_t>(rand()); }

extern "C" void* rumy_lv_pool_alloc(size_t size) {
  static void* pool = nullptr;
  if (!pool) pool = calloc(1, size);
  return pool;
}

namespace sim {

void setWipe(bool on) { s_wipe = on; }

void setUtc(uint32_t utc) {
  s_utcBase = utc;
  s_msBase = s_ms;
}

uint32_t utc() { return s_utcBase ? s_utcBase + (s_ms - s_msBase) / 1000U : 0; }

void setLastTouch(uint32_t ms) { s_lastTouch = ms; }

BulbState& bulb() { return s_bulb; }

void publishBulb() { store::putT(store::BULB, s_bulb); }

NetStatus& net() { return s_net; }

void publishNet() {
  s_net.webActive = s_web ? 1 : 0;
  s_net.webPin = s_web ? 4821 : 0;
  store::putT(store::NET, s_net);
}

void publishPrinter(const PrinterState& p) { store::putT(store::PRINTER, p); }

}

namespace mem {

bool init() {
  const size_t sizes[4] = {cfg::DRAW_BUF_BYTES, cfg::DRAW_BUF_BYTES, cfg::JSON_ARENA_BYTES, cfg::WIZ_JSON_ARENA_BYTES};
  for (int i = 0; i < 4; i++) {
    if (!s_bufs[i]) s_bufs[i] = static_cast<uint8_t*>(calloc(1, sizes[i]));
  }
  return true;
}

uint8_t* drawBuf1() { return s_bufs[0]; }
uint8_t* drawBuf2() { return s_bufs[1]; }
uint8_t* jsonArena() { return s_bufs[2]; }
uint8_t* wizArena() { return s_bufs[3]; }
void report(Print& out) { out.printf("[mem] onizleme\n"); }
uint32_t freeHeap() { return 101376; }
uint32_t minFreeHeap() { return 78912; }
uint32_t largestBlock() { return 61428; }

}

namespace display {

namespace {
uint8_t s_bri = 80;
}

bool begin(uint8_t) { return true; }
FlushPerf takeFlushPerf() { return FlushPerf{}; }
void setScroll(uint16_t) { simAdvance(4); }
bool hardwareScroll() { return !s_wipe; }
void ignoreUntilRelease() {}
void ledsOff() {}
void startLvgl(uint8_t*, uint8_t*, uint32_t) {}
void setBrightnessPct(uint8_t pct) { s_bri = pct; }
uint8_t brightnessPct() { return s_bri; }
void setTouchRotation(uint8_t) {}
uint32_t lastTouchMs() { return s_lastTouch; }
bool touching() { return false; }
uint32_t touchReads() { return 0; }
bool touchCalSupported() { return true; }
bool rawTouch(int32_t&, int32_t&) { return false; }
bool calibrateTouch(const int32_t*, const int32_t*, const int32_t*, const int32_t*) { return true; }
bool mapRawTouch(int32_t rx, int32_t ry, int32_t& x, int32_t& y) {
  x = rx;
  y = ry;
  return true;
}
void keepTouchCal(bool) {}

}

namespace sdcard {

bool begin() { return true; }
bool mounted() { return true; }
void lock() {}
void unlock() {}
bool readFile(const char*, uint8_t*, size_t, size_t& len) {
  len = 0;
  return false;
}
bool writeFileAtomic(const char*, const uint8_t*, size_t) { return true; }
bool exists(const char* path) {
  char full[512];
  snprintf(full, sizeof(full), "%s%s", RUMY_SIM_SD, path);
  FILE* f = fopen(full, "rb");
  if (!f) return false;
  fclose(f);
  return true;
}
bool remove(const char*) { return true; }
uint64_t totalBytes() { return 31914983424ULL; }
uint64_t usedBytes() { return 1288490188ULL; }
uint32_t readErrors() { return 0; }
uint32_t writeErrors() { return 0; }
uint32_t writes() { return 37; }
bool selfTest(uint32_t iterations, uint32_t& okCount) {
  okCount = iterations;
  return true;
}

}

namespace clk {

void begin() {}
bool synced() { return sim::utc() > 1700000000U; }
uint32_t nowUtc() { return sim::utc(); }
rc::LocalTime local() { return rc::toLocal(nowUtc()); }
int32_t todayDayNum() { return rc::localDayNum(nowUtc()); }
uint16_t minuteOfDay() {
  rc::LocalTime lt = local();
  return static_cast<uint16_t>(lt.hour * 60 + lt.minute);
}

}

namespace fwupdate {

Progress s_update = {FW_IDLE, 0, 0, 0, ""};

Progress progress() { return s_update; }

}

void sim::setUpdate(uint8_t phase, uint32_t got, uint32_t total, const char* why) {
  fwupdate::s_update.phase = static_cast<fwupdate::Phase>(phase);
  fwupdate::s_update.got = got;
  fwupdate::s_update.total = total;
  fwupdate::s_update.why = why;
}

namespace wiz {

void begin() {}
void setDesired(const rc::Pilot& p, bool) {
  s_bulb.pilot = p;
  sim::publishBulb();
}
void setInteracting(bool) {}
void togglePower() {
  s_bulb.pilot.on = s_bulb.pilot.on ? 0 : 1;
  sim::publishBulb();
}
void scan() {}
void celebrate() {}
void alert() {}
void pair(const char* mac) { settings::setBulb(mac, "WiZ ampul"); }
void forget() { settings::setBulb("", ""); }
uint32_t stackFree() { return 1492; }

}

namespace leagues {

namespace {
uint8_t s_league = 0;
bool s_table = false;
uint16_t s_seq = 0;
}

void open(uint8_t league) {
  s_league = league;
  s_table = false;
  s_seq++;
}
void step(int8_t dir) {
  (void)dir;
  s_seq++;
}
void showTable(bool table) {
  s_table = table;
  s_seq++;
}
void close() {}
uint8_t league() { return s_league; }
bool tableMode() { return s_table; }
uint16_t request() { return s_seq; }

}

namespace net {

void begin() {}
void refreshAll() {}
void refresh(NetSource) {}
void locationChanged() {}
bool online() { return s_net.wifi != 0; }
uint32_t stackFree() { return 4872; }

}

namespace webui {

void setEnabled(bool on) {
  s_web = on;
  sim::publishNet();
}
bool enabled() { return s_web; }
bool running() { return s_web; }
void service(uint32_t) {}
uint16_t pin() { return s_web ? 4821 : 0; }
uint32_t lastActivityMs() { return 0; }
bool busy() { return false; }
bool mdnsUp() { return true; }
void report(Print&) {}
void setPairing(bool) {}
uint16_t pairCode() { return 7305; }
uint8_t phones() { return 1; }
void forgetPhones() {}

}

namespace diag {

void begin() {}
void loop() {}
void printReport(Print& out) { out.printf("[diag] onizleme raporu\n"); }
uint32_t uptimeS() { return 3U * 86400U + 4U * 3600U + 12U * 60U + millis() / 1000U; }
const char* resetReason() { return "güç verildi"; }
int32_t heapTrendPerHour() { return -14; }
void note(const char* tag, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  printf("[%s] ", tag);
  vprintf(fmt, ap);
  printf("\n");
  va_end(ap);
}

}

namespace app {

void request(Action a, uint32_t) { s_action = a; }
void poll() {}
Action pending() { return s_action; }
const char* resetReasonText() { return diag::resetReason(); }
uint32_t bootCount() { return 42; }

}

namespace {

media::State s_media;
uint32_t s_mediaVer = 1;
uint32_t s_mediaPos = 0;
uint32_t s_mediaPosAt = 0;
uint16_t s_artPx[cfg::MEDIA_ART * cfg::MEDIA_ART];
lv_image_dsc_t s_artImg;
bool s_hasArt = false;

uint16_t rgb565(int r, int g, int b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

void makeArt() {
  const int n = cfg::MEDIA_ART;
  for (int y = 0; y < n; y++) {
    for (int x = 0; x < n; x++) {
      int dx = x - n / 2, dy = y - n / 2;
      int d2 = dx * dx + dy * dy;
      int r = 40 + x * 180 / n, g = 10 + y * 60 / n, b = 120 + (n - x) * 120 / n;
      if (d2 < 52 * 52 && d2 > 40 * 40) r = 255, g = 80, b = 200;
      if (d2 < 16 * 16) r = 255, g = 230, b = 60;
      bool corner = (x < 12 || x >= n - 12) && (y < 12 || y >= n - 12);
      if (corner) {
        int cx = x < 12 ? 12 : n - 13, cy = y < 12 ? 12 : n - 13;
        if ((x - cx) * (x - cx) + (y - cy) * (y - cy) > 144) r = g = b = 0;
      }
      s_artPx[y * n + x] = rgb565(r, g, b);
    }
  }
  memset(&s_artImg, 0, sizeof(s_artImg));
  s_artImg.header.magic = LV_IMAGE_HEADER_MAGIC;
  s_artImg.header.cf = LV_COLOR_FORMAT_RGB565;
  s_artImg.header.w = n;
  s_artImg.header.h = n;
  s_artImg.header.stride = n * 2;
  s_artImg.data = reinterpret_cast<const uint8_t*>(s_artPx);
  s_artImg.data_size = sizeof(s_artPx);
}

media::Playlists s_pl;
uint16_t s_sheetPx[rc::PL_SHEETS][cfg::MEDIA_ART * cfg::MEDIA_ART];
lv_image_dsc_t s_sheetImg[rc::PL_SHEETS];
bool s_sheetsMade = false;

const char* const kPlNames[] = {"Beğenilen Şarkılar", "Gece Sürüşü", "Çalışırken", "Türkçe Rock", "Sakin Pazar",
                                "Spor", "Yol Şarkıları", "90'lar Pop", "Odaklan", "Yağmurlu Gün", "Kahvaltı Caz",
                                "Rumy'nin Seçtikleri", "Akustik Akşamlar ve Çok Uzun Bir Liste", "Hafta Sonu"};
const uint16_t kPlTotals[] = {812, 64, 120, 87, 41, 56, 133, 75, 38, 29, 47, 18, 102, 66};
const uint32_t kPlColors[] = {0x5A30D0, 0x2B3FD0, 0xE09030, 0xE04030, 0x30C060, 0xE03060, 0xD0B020,
                              0xB040E0, 0x30B0C0, 0x4070C0, 0xC07040, 0x40C0A0, 0x8060C0, 0xD05090};
constexpr int PL_NAMES = static_cast<int>(sizeof(kPlNames) / sizeof(kPlNames[0]));

void makeCover(uint16_t* px, int stride, int i) {
  const int n = cfg::MEDIA_PL_COVER;
  uint32_t c = kPlColors[i % PL_NAMES];
  int r0 = static_cast<int>(c >> 16 & 0xFF), g0 = static_cast<int>(c >> 8 & 0xFF), b0 = static_cast<int>(c & 0xFF);
  const int rc4 = n / 10;
  for (int y = 0; y < n; y++) {
    for (int x = 0; x < n; x++) {
      int shade = 100 - y * 45 / n;
      int r = r0 * shade / 100, g = g0 * shade / 100, b = b0 * shade / 100;
      double fx = (x - n * 0.5) / (n * 0.26), fy = -(y - n * 0.5) / (n * 0.26);
      if (i == 0) {
        double q = fx * fx + fy * fy - 1.0;
        if (q * q * q - fx * fx * fy * fy * fy <= 0) r = g = b = 255;
      } else {
        double d2 = fx * fx + fy * fy;
        if (d2 < 1.2) r = 255 - r0 / 3, g = 255 - g0 / 3, b = 255 - b0 / 3;
        if (d2 < 0.15) r = r0 / 3, g = g0 / 3, b = b0 / 3;
      }
      int cx = x < rc4 ? rc4 : (x >= n - rc4 ? n - 1 - rc4 : x);
      int cy = y < rc4 ? rc4 : (y >= n - rc4 ? n - 1 - rc4 : y);
      if ((x - cx) * (x - cx) + (y - cy) * (y - cy) > rc4 * rc4) r = g = b = 0;
      px[y * stride + x] = rgb565(r, g, b);
    }
  }
}

void makeSheets() {
  const int a = cfg::MEDIA_ART;
  const int cell = cfg::MEDIA_PL_CELL;
  for (int k = 0; k < rc::PL_SHEETS; k++) {
    memset(s_sheetPx[k], 0, sizeof(s_sheetPx[k]));
    for (int c = 0; c < rc::PL_PER_SHEET; c++) {
      int i = k * rc::PL_PER_SHEET + c;
      if (i >= rc::PL_MAX) break;
      makeCover(&s_sheetPx[k][(c / 3) * cell * a + (c % 3) * cell], a, i);
    }
    lv_image_dsc_t& d = s_sheetImg[k];
    memset(&d, 0, sizeof(d));
    d.header.magic = LV_IMAGE_HEADER_MAGIC;
    d.header.cf = LV_COLOR_FORMAT_RGB565;
    d.header.w = a;
    d.header.h = a;
    d.header.stride = a * 2;
    d.data = reinterpret_cast<const uint8_t*>(s_sheetPx[k]);
    d.data_size = sizeof(s_sheetPx[k]);
  }
  s_sheetsMade = true;
}

}

namespace sim {

void setPlaylists(uint8_t status, int n, int playing, bool shuffle, int pick, uint8_t result, const char* msg) {
  if (!s_sheetsMade) makeSheets();
  memset(&s_pl, 0, sizeof(s_pl));
  s_pl.ver = 1;
  s_pl.status = status;
  s_pl.n = static_cast<uint8_t>(n < PL_NAMES ? n : PL_NAMES);
  s_pl.playing = static_cast<int8_t>(playing);
  s_pl.shuffle = shuffle;
  s_pl.pick = static_cast<int8_t>(pick);
  s_pl.pickResult = result;
  snprintf(s_pl.msg, sizeof(s_pl.msg), "%s", msg);
  snprintf(s_pl.pickMsg, sizeof(s_pl.pickMsg), "%s", msg);
  for (int i = 0; i < s_pl.n; i++) {
    snprintf(s_pl.item[i].name, sizeof(s_pl.item[i].name), "%s", kPlNames[i]);
    s_pl.item[i].total = kPlTotals[i];
    s_pl.item[i].sheet = static_cast<uint8_t>(i / rc::PL_PER_SHEET);
    s_pl.item[i].cell = static_cast<uint8_t>(i % rc::PL_PER_SHEET);
  }
  s_mediaVer++;
}

void setPlaylistNames(const char* const* names, const uint16_t* totals, int n) {
  if (!s_sheetsMade) makeSheets();
  s_pl.n = static_cast<uint8_t>(n < rc::PL_MAX ? n : rc::PL_MAX);
  for (int i = 0; i < s_pl.n; i++) {
    snprintf(s_pl.item[i].name, sizeof(s_pl.item[i].name), "%s", names[i]);
    s_pl.item[i].total = totals[i];
    s_pl.item[i].sheet = static_cast<uint8_t>(i / rc::PL_PER_SHEET);
    s_pl.item[i].cell = static_cast<uint8_t>(i % rc::PL_PER_SHEET);
  }
  s_pl.ver++;
  s_mediaVer++;
}

void setMediaVol(int8_t vol) {
  s_media.vol = vol;
  s_mediaVer++;
}

void setMedia(bool link, bool playing, uint32_t posMs, uint32_t durMs, uint32_t accent, const char* title,
              const char* artist, const char* album, bool art) {
  int8_t vol = s_media.vol;
  memset(&s_media, 0, sizeof(s_media));
  s_media.vol = link ? vol : -1;
  s_media.link = link;
  s_media.active = title && title[0];
  s_media.playing = playing;
  s_media.durMs = durMs;
  s_media.accent = accent;
  s_media.track = static_cast<uint32_t>(strlen(title ? title : "")) * 131u + durMs;
  snprintf(s_media.app, sizeof(s_media.app), "%s", s_media.active ? "Spotify" : "");
  snprintf(s_media.title, sizeof(s_media.title), "%s", title ? title : "");
  snprintf(s_media.artist, sizeof(s_media.artist), "%s", artist ? artist : "");
  snprintf(s_media.album, sizeof(s_media.album), "%s", album ? album : "");
  s_mediaPos = posMs;
  s_mediaPosAt = s_ms;
  if (art && !s_hasArt) makeArt();
  s_hasArt = art;
  s_mediaVer++;
}

}

namespace media {

void begin() {}
void loop() {}
void lineBegin() {}
void lineChar(char) {}
void lineEnd() {}
bool receiving() { return false; }
bool receive() { return false; }
const State& state() { return s_media; }
uint32_t version() { return s_mediaVer; }
uint32_t positionMs() {
  if (!s_media.playing) return s_mediaPos;
  uint32_t p = s_mediaPos + (s_ms - s_mediaPosAt);
  return s_media.durMs && p > s_media.durMs ? s_media.durMs : p;
}
const lv_image_dsc_t* art() { return s_hasArt ? &s_artImg : nullptr; }
bool tempo(uint32_t& periodMs, uint32_t& beatMs) {
  if (!s_media.playing) return false;
  periodMs = 500;
  beatMs = s_mediaPosAt;
  return true;
}
void send(Cmd) {}

void seek(uint32_t) {}
void report(Print&) {}

const Playlists& playlists() { return s_pl; }
void playlistsOpen(bool) {}
void playPlaylist(int index) {
  s_pl.pick = static_cast<int8_t>(index);
  s_pl.pickResult = rc::PLR_STARTING;
  s_mediaVer++;
}
void setShuffle(bool on) {
  s_pl.shuffle = on;
  s_mediaVer++;
}
const lv_image_dsc_t* playlistSheet(int index, int32_t& dx, int32_t& dy) {
  if (index < 0 || index >= s_pl.n || !s_sheetsMade) return nullptr;
  const PlaylistItem& it = s_pl.item[index];
  if (it.sheet >= rc::PL_SHEETS) return nullptr;
  dx = (it.cell % 3) * cfg::MEDIA_PL_CELL;
  dy = (it.cell / 3) * cfg::MEDIA_PL_CELL;
  return &s_sheetImg[it.sheet];
}

}

namespace cam {

void want(bool, uint32_t, int32_t, int32_t) {}
void loop() {}
void start(char*) {}
bool live() { return false; }
void report(Print&) {}

}
