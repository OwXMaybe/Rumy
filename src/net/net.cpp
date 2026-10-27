#include "net.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

#include "app/config.h"
#include "app/mem.h"
#include "app/store.h"
#include "ca_bundle.h"
#include "clock/clock.h"
#include "diag/diag.h"
#include "events/events.h"
#include "fetch.h"
#include "http_min.h"
#include "match/board.h"
#include "match/leagues.h"
#include "match/match.h"
#include "media/bridge.h"
#include "printer/printer.h"
#include "quake/quake.h"
#include "weather/weather.h"
#include "webui/webui.h"

namespace net {

namespace {

class BundleClient : public WiFiClientSecure {
 public:
  BundleClient() { _use_ca_bundle = true; }
};

using JobFn = int16_t (*)(FetchStat&);

struct Job {
  uint8_t needsTime;
  uint16_t firstS;
  uint32_t periodS;
  JobFn fn;
  const char* name;
  uint32_t (*periodFn)();
};

uint32_t periodOf(const Job& job) { return job.periodFn ? job.periodFn() : job.periodS; }

const Job kJobs[SRC_COUNT] = {
    {0, 3, cfg::PERIOD_FORECAST_S, weather::fetchWeather, "hava+7gun", nullptr},
    {0, 30, cfg::PERIOD_HOURLY_S, weather::fetchMarine, "deniz", nullptr},
    {0, 45, cfg::PERIOD_HOURLY_S, weather::fetchAir, "havakalitesi", nullptr},
    {1, 60, cfg::PERIOD_QUAKE_S, quake::fetch, "afad", nullptr},
    {1, 90, cfg::PERIOD_HOLIDAY_S, events::fetchHolidays, "nager", nullptr},
    {1, 120, cfg::MATCH_SCHED_S, match::fetch, "mac", match::periodS},
    {1, 140, cfg::BOARD_IDLE_S, board::fetch, "lig", board::periodS},
    {1, 5, cfg::LEAGUE_IDLE_S, leagues::fetch, "ligsayfa", leagues::periodS},
};

constexpr uint32_t TLS_DEFER_RETRY_MS = 30000;
constexpr uint32_t TIME_WAIT_RETRY_MS = 5000;

TaskHandle_t s_task = nullptr;
rc::JsonArena s_arena;
NetStatus s_status;
uint32_t s_nextMs[SRC_COUNT] = {};
uint32_t s_backoffS[SRC_COUNT] = {};
bool s_scheduled = false;
bool s_wasUp = false;
uint32_t s_lastRetryMs = 0;
uint32_t s_lastStatusMs = 0;
portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t s_refreshMask = 0;
volatile bool s_locationChanged = false;
uint32_t s_unsynced = 0;

bool due(uint32_t now, uint32_t at) { return static_cast<int32_t>(now - at) >= 0; }

void scheduleAll(uint32_t now) {
  for (int i = 0; i < SRC_COUNT; i++) {
    if (!s_scheduled || due(now, s_nextMs[i])) s_nextMs[i] = now + kJobs[i].firstS * 1000UL;
  }
  s_scheduled = true;
}

void publishStatus(uint32_t now) {
  s_lastStatusMs = now;
  bool up = WiFi.status() == WL_CONNECTED;
  s_status.wifi = up ? 1 : 0;
  s_status.timeSynced = clk::synced() ? 1 : 0;
  s_status.rssi = up ? static_cast<int8_t>(WiFi.RSSI()) : 0;
  s_status.ip = up ? static_cast<uint32_t>(WiFi.localIP()) : 0;
  s_status.webActive = webui::running() ? 1 : 0;
  s_status.webPin = webui::pin();
  for (int i = 0; i < SRC_COUNT; i++) {
    int32_t left = static_cast<int32_t>(s_nextMs[i] - now);
    s_status.src[i].nextInS = left > 0 ? static_cast<uint32_t>(left) / 1000U : 0;
  }
  store::putT(store::NET, s_status);
}

void wifiManage(uint32_t now) {
  bool up = WiFi.status() == WL_CONNECTED;
  if (up && !s_wasUp) {
    scheduleAll(now);
    IPAddress ip = WiFi.localIP();
    diag::note("net", "WiFi bagli %u.%u.%u.%u rssi %d bos=%u blok=%u", static_cast<unsigned>(ip[0]),
               static_cast<unsigned>(ip[1]), static_cast<unsigned>(ip[2]), static_cast<unsigned>(ip[3]),
               static_cast<int>(WiFi.RSSI()), static_cast<unsigned>(mem::freeHeap()),
               static_cast<unsigned>(mem::largestBlock()));
    publishStatus(now);
  }
  if (!up && s_wasUp) {
    diag::note("net", "WiFi koptu");
    s_lastRetryMs = now;
    publishStatus(now);
  }
  if (!up && now - s_lastRetryMs >= cfg::WIFI_RETRY_MS) {
    s_lastRetryMs = now;
    WiFi.reconnect();
  }
  s_wasUp = up;
}

void handleRequests(uint32_t now) {
  uint32_t mask;
  bool loc;
  portENTER_CRITICAL(&s_mux);
  mask = s_refreshMask;
  loc = s_locationChanged;
  s_refreshMask = 0;
  s_locationChanged = false;
  portEXIT_CRITICAL(&s_mux);
  if (loc) {
    weather::clearAll();
    mask |= (1u << SRC_FORECAST) | (1u << SRC_MARINE) | (1u << SRC_AIR) | (1u << SRC_QUAKE);
    diag::note("net", "konum degisti, veriler yenileniyor");
  }
  for (int i = 0; i < SRC_COUNT; i++) {
    if (mask & (1u << i)) {
      s_nextMs[i] = now;
      s_backoffS[i] = 0;
    }
  }
}

void runJob(int i, uint32_t now) {
  const Job& job = kJobs[i];
  SourceStat& ss = s_status.src[i];
  if (job.needsTime && !clk::synced()) {
    s_nextMs[i] = now + TIME_WAIT_RETRY_MS;
    return;
  }
  FetchStat st;
  memset(&st, 0, sizeof(st));
  int16_t code = job.fn(st);
  uint32_t end = millis();
  if (code == 0) {
    s_nextMs[i] = end + periodOf(job) * 1000UL;
    return;
  }
  if (st.deferred) {
    ss.deferred++;
    s_status.tlsDeferred++;
    s_nextMs[i] = end + TLS_DEFER_RETRY_MS;
    diag::note("net", "%s ertelendi (web=%d bos=%u blok=%u)", job.name, webui::busy() ? 1 : 0,
               static_cast<unsigned>(mem::freeHeap()), static_cast<unsigned>(mem::largestBlock()));
    return;
  }
  uint32_t utc = clk::synced() ? clk::nowUtc() : 0;
  if (code == 200 && !utc) s_unsynced |= 1u << i;
  ss.lastTryUtc = utc;
  ss.lastCode = code;
  ss.durMs = st.durMs;
  ss.bytes = st.bytes;
  ss.via = st.via;
  if (code == 200) {
    ss.lastOk = 1;
    ss.lastOkUtc = utc;
    s_backoffS[i] = 0;
    s_nextMs[i] = end + periodOf(job) * 1000UL;
    if (i == SRC_FORECAST) s_status.lastFullOkUtc = utc;
  } else {
    ss.lastOk = 0;
    if (ss.fails < 0xFFFF) ss.fails++;
    uint32_t b = s_backoffS[i] ? s_backoffS[i] * 2 : cfg::RETRY_MIN_S;
    if (b > cfg::RETRY_MAX_S) b = cfg::RETRY_MAX_S;
    s_backoffS[i] = b;
    s_nextMs[i] = end + b * 1000UL;
    diag::note("net", "%s hata %d (%s), %us sonra", job.name, static_cast<int>(code), http::errText(code),
               static_cast<unsigned>(b));
  }
  publishStatus(end);
}

void runDue(uint32_t now) {
  for (int i = 0; i < SRC_COUNT; i++) {
    if (!due(now, s_nextMs[i])) continue;
    runJob(i, now);
    return;
  }
}

int16_t transfer(Client& c, const char* host, const char* path, ParseFn parse, void* ctx, FetchStat& st, bool tls,
                 uint32_t freeBefore) {
  if (tls) {
    uint32_t used = freeBefore > mem::freeHeap() ? freeBefore - mem::freeHeap() : 0;
    if (used > s_status.tlsPeak) s_status.tlsPeak = used;
  }
  http::Response r(c, cfg::HTTP_TIMEOUT_MS);
  int status = r.begin(host, path);
  int16_t code = static_cast<int16_t>(status);
  if (status == 200) {
    s_arena.reset();
    bool ok = parse(r, ctx);
    if (r.timedOut()) code = http::E_TIMEOUT;
    else if (!ok) code = http::E_PARSE;
    if (tls) {
      uint32_t used = freeBefore > mem::freeHeap() ? freeBefore - mem::freeHeap() : 0;
      if (used > s_status.tlsPeak) s_status.tlsPeak = used;
    }
  }
  st.bytes = r.bytes();
  return code;
}

void task(void*) {
  for (;;) {
    uint32_t now = millis();
    wifiManage(now);
    handleRequests(now);
    webui::service(now);
    printer::service(millis());
    events::tick();
    if (s_unsynced && clk::synced()) {
      for (int i = 0; i < SRC_COUNT; i++) {
        if (s_unsynced & (1u << i)) s_nextMs[i] = millis();
      }
      s_unsynced = 0;
    }
    if (s_wasUp && s_scheduled) runDue(millis());
    now = millis();
    if (now - s_lastStatusMs >= cfg::NET_STATUS_MS) publishStatus(now);
    vTaskDelay(pdMS_TO_TICKS(webui::busy() ? 5 : 40));
  }
}

}

rc::JsonArena& arena() { return s_arena; }

bool tlsAllowed() {
  if (webui::busy() || bridge::bulkActive()) return false;
  return mem::largestBlock() >= cfg::TLS_MIN_LARGEST_BLOCK && mem::freeHeap() >= cfg::TLS_MIN_FREE_HEAP;
}

int16_t get(const char* host, const char* path, bool tls, ParseFn parse, void* ctx, FetchStat& st) {
  memset(&st, 0, sizeof(st));
  st.via = tls ? VIA_TLS : VIA_HTTP;
  uint32_t t0 = millis();
  if (WiFi.status() != WL_CONNECTED) {
    st.code = http::E_NO_WIFI;
    return st.code;
  }
  int16_t code;
  if (tls) {
    bool held = printer::holdForTls();
    if (!tlsAllowed()) {
      if (held) printer::releaseAfterTls(millis());
      st.deferred = 1;
      st.code = http::E_TLS_MEM;
      return st.code;
    }
    uint32_t before = mem::freeHeap();
    BundleClient c;
    c.setHandshakeTimeout(cfg::TLS_HANDSHAKE_S);
    if (!c.connect(host, 443, static_cast<int32_t>(cfg::HTTP_CONNECT_MS))) {
      code = http::E_CONNECT;
    } else {
      code = transfer(c, host, path, parse, ctx, st, true, before);
    }
    c.stop();
    if (held) printer::releaseAfterTls(millis());
  } else {
    WiFiClient c;
    if (!c.connect(host, 80, static_cast<int32_t>(cfg::HTTP_CONNECT_MS))) {
      code = http::E_CONNECT;
    } else {
      c.setNoDelay(true);
      code = transfer(c, host, path, parse, ctx, st, false, 0);
    }
    c.stop();
  }
  s_arena.reset();
  st.durMs = millis() - t0;
  st.code = code;
  return code;
}

void begin() {
  if (s_task) return;
  memset(&s_status, 0, sizeof(s_status));
  s_arena.attach(mem::jsonArena(), cfg::JSON_ARENA_BYTES);
  {
    WiFiClientSecure seed;
    seed.setCACertBundle(RUMY_CA_BUNDLE);
  }
  WiFi.setHostname("rumy");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.begin();
  clk::begin();
  s_lastRetryMs = millis();
  xTaskCreatePinnedToCore(task, "net", cfg::NET_STACK, nullptr, cfg::NET_PRIO, &s_task, cfg::NET_CORE);
}

void refreshAll() {
  portENTER_CRITICAL(&s_mux);
  s_refreshMask = (1u << SRC_COUNT) - 1u;
  portEXIT_CRITICAL(&s_mux);
}

void refresh(NetSource s) {
  if (s >= SRC_COUNT) return;
  portENTER_CRITICAL(&s_mux);
  s_refreshMask = s_refreshMask | (1u << s);
  portEXIT_CRITICAL(&s_mux);
}

void locationChanged() {
  portENTER_CRITICAL(&s_mux);
  s_locationChanged = true;
  portEXIT_CRITICAL(&s_mux);
}

bool online() { return WiFi.status() == WL_CONNECTED; }

uint32_t stackFree() { return s_task ? uxTaskGetStackHighWaterMark(s_task) : 0; }

}
