#include "wiz.h"

#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lwip/sockets.h>
#include <string.h>

#include "app/config.h"
#include "app/mem.h"
#include "app/shared.h"
#include "app/store.h"
#include "color.h"
#include "diag/diag.h"
#include "json_arena.h"
#include "pairing.h"
#include "settings/settings.h"

namespace wiz {

namespace {

constexpr uint32_t SCAN_WINDOW_MS = 6000;
constexpr uint32_t LOOP_MS = 20;
constexpr uint32_t BLINK_GAP_MS = 450;
constexpr uint32_t PENDING_DROP_MS = 4000;

struct Mailbox {
  rc::Pilot desired;
  uint8_t hasDesired;
  uint8_t final;
  uint8_t toggle;
  uint8_t scan;
  uint8_t forget;
  uint8_t pair;
  uint8_t celebrate;
  uint8_t alert;
  char mac[13];
};

portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
Mailbox s_box = {};
volatile bool s_interacting = false;
volatile uint32_t s_interactEndMs = 0;
TaskHandle_t s_task = nullptr;

int s_sock = -1;
rc::JsonArena s_arena;
rc::BulbList s_found;
rc::SendThrottle s_throttle(cfg::WIZ_SEND_MIN_MS);
rc::OnlineTracker s_tracker(cfg::WIZ_REPLY_TIMEOUT_MS, 2);
char s_target[13] = {};
uint32_t s_ip = 0;
bool s_scanning = false;
uint32_t s_scanUntil = 0;
uint32_t s_lastDiscovery = 0;
uint32_t s_lastHeartbeat = 0;
uint32_t s_lastSendMs = 0;
bool s_everSent = false;
rc::Pilot s_desired = {};
bool s_pending = false;
bool s_denied = false;
bool s_pendingFinal = false;
uint32_t s_pendingSince = 0;
rc::Pilot s_actual = {};
uint8_t s_blinkStep = 0;
uint32_t s_blinkAt = 0;
rc::Pilot s_blinkRestore = {};
uint32_t s_partyUntil = 0;
uint32_t s_partyAt = 0;
uint8_t s_partyStep = 0;
bool s_partyAlert = false;
rc::Rgb s_partyA = {255, 200, 0};
rc::Rgb s_partyB = {0, 20, 255};
rc::Pilot s_partyRestore = {};
BulbState s_published = {};
bool s_publishedOnce = false;
char s_rx[512];
char s_tx[200];

uint32_t broadcastAddr() {
  IPAddress ip = WiFi.localIP();
  IPAddress mask = WiFi.subnetMask();
  uint32_t a = static_cast<uint32_t>(ip);
  uint32_t m = static_cast<uint32_t>(mask);
  if (!a || !m) return 0xFFFFFFFFu;
  return a | ~m;
}

bool openSocket() {
  if (s_sock >= 0) return true;
  int s = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s < 0) return false;
  int yes = 1;
  lwip_setsockopt(s, SOL_SOCKET, SO_BROADCAST, &yes, sizeof(yes));
  sockaddr_in local = {};
  local.sin_family = AF_INET;
  local.sin_port = 0;
  local.sin_addr.s_addr = htonl(INADDR_ANY);
  if (lwip_bind(s, reinterpret_cast<sockaddr*>(&local), sizeof(local)) < 0) {
    lwip_close(s);
    return false;
  }
  s_sock = s;
  return true;
}

void closeSocket() {
  if (s_sock >= 0) lwip_close(s_sock);
  s_sock = -1;
}

bool sendTo(uint32_t ip, const char* data, size_t len) {
  if (s_sock < 0 || !ip || !len) return false;
  sockaddr_in to = {};
  to.sin_family = AF_INET;
  to.sin_port = htons(rc::WIZ_PORT);
  to.sin_addr.s_addr = ip;
  int n = lwip_sendto(s_sock, data, len, 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));
  return n == static_cast<int>(len);
}

void sendDiscovery(uint32_t now) {
  size_t n = rc::wizBuildRegistration(s_tx, sizeof(s_tx));
  sendTo(broadcastAddr(), s_tx, n);
  s_lastDiscovery = now;
}

void sendPilot(const rc::Pilot& p, uint32_t now) {
  size_t n = rc::wizBuildSetPilot(p, s_tx, sizeof(s_tx));
  if (sendTo(s_ip, s_tx, n)) {
    s_throttle.markSent(now, p);
    s_lastSendMs = now;
    s_everSent = true;
    s_tracker.onRequest(now);
  }
}

void sendGetPilot(uint32_t now) {
  size_t n = rc::wizBuildGetPilot(s_tx, sizeof(s_tx));
  if (sendTo(s_ip, s_tx, n)) s_tracker.onRequest(now);
  s_lastHeartbeat = now;
}

uint32_t s_settingsVer = 0xFFFFFFFFu;

void loadTarget() {
  uint32_t v = settings::version();
  if (v == s_settingsVer) return;
  s_settingsVer = v;
  Settings st = settings::get();
  if (!rc::macEqual(st.bulbMac, s_target)) {
    memcpy(s_target, st.bulbMac, sizeof(s_target));
    s_target[12] = 0;
    s_ip = 0;
    s_denied = false;
    s_tracker = rc::OnlineTracker(cfg::WIZ_REPLY_TIMEOUT_MS, 2);
    s_actual = rc::Pilot{};
    int idx = s_target[0] ? s_found.find(s_target) : -1;
    if (idx >= 0) s_ip = s_found.at(idx).ip;
  }
}

void takeMailbox(uint32_t now) {
  Mailbox m;
  portENTER_CRITICAL(&s_mux);
  m = s_box;
  s_box.hasDesired = 0;
  s_box.final = 0;
  s_box.toggle = 0;
  s_box.scan = 0;
  s_box.forget = 0;
  s_box.pair = 0;
  s_box.celebrate = 0;
  s_box.alert = 0;
  portEXIT_CRITICAL(&s_mux);
  if ((m.celebrate || m.alert) && !s_partyUntil && s_ip && s_tracker.online()) {
    rc::Pilot base = s_pending ? s_desired : (s_actual.valid ? s_actual : s_throttle.lastSent());
    Settings st = settings::get();
    bool want = m.celebrate ? st.wizGoal != 0 : st.wizAgent != 0;
    if (want && st.wizOnlyOn && !base.on) want = false;
    if (!want) diag::note("wiz", "%s atlandi (ayar)", m.celebrate ? "kutlama" : "uyari");
    if (want && base.valid) {
      s_partyRestore = base;
      s_partyAlert = !m.celebrate;
      rc::TeamPalette pal = rc::teamPalette(st.teamC1, st.teamC2);
      s_partyA = pal.bulbA;
      s_partyB = pal.bulbB;
      uint32_t ms = s_partyAlert ? cfg::WIZ_ALERT_STEP_MS * cfg::WIZ_ALERT_STEPS : cfg::WIZ_PARTY_MS;
      s_partyUntil = (now + ms) | 1u;
      s_partyAt = now;
      s_partyStep = 0;
    }
  }

  if (m.forget) {
    s_target[0] = 0;
    s_ip = 0;
    s_actual = rc::Pilot{};
    s_pending = false;
    s_denied = false;
    s_tracker = rc::OnlineTracker(cfg::WIZ_REPLY_TIMEOUT_MS, 2);
  }
  if (m.scan) {
    s_found.clear();
    s_scanning = true;
    s_scanUntil = now + SCAN_WINDOW_MS;
    s_lastDiscovery = 0;
  }
  if (m.pair) {
    memcpy(s_target, m.mac, sizeof(s_target));
    s_target[12] = 0;
    s_denied = false;
    int idx = s_found.find(s_target);
    s_ip = idx >= 0 ? s_found.at(idx).ip : 0;
    s_tracker = rc::OnlineTracker(cfg::WIZ_REPLY_TIMEOUT_MS, 2);
    s_actual = rc::Pilot{};
    s_blinkStep = 1;
    s_blinkAt = now;
  }
  if (m.toggle) {
    rc::Pilot base = s_pending ? s_desired : (s_actual.valid ? s_actual : s_throttle.lastSent());
    if (!base.valid) base = rc::pilotTemp(2700, 60);
    rc::Pilot p = base;
    p.valid = 1;
    p.on = base.on ? 0 : 1;
    s_desired = p;
    s_pending = true;
    s_pendingFinal = true;
    s_pendingSince = now;
  }
  if (m.hasDesired) {
    s_desired = m.desired;
    s_pending = true;
    s_pendingFinal = m.final != 0;
    s_pendingSince = now;
  }
}

void handleBlink(uint32_t now) {
  if (!s_blinkStep || !s_ip || static_cast<int32_t>(now - s_blinkAt) < 0) return;
  if (s_blinkStep == 1) {
    s_blinkRestore = s_actual.valid ? s_actual : rc::Pilot{};
    rc::Pilot flash = rc::pilotTemp(6500, 100);
    if (s_actual.valid && s_actual.on) flash = rc::pilotOff(s_actual);
    sendPilot(flash, now);
    s_blinkStep = 2;
    s_blinkAt = now + BLINK_GAP_MS;
  } else if (s_blinkStep == 2) {
    rc::Pilot back = s_blinkRestore.valid ? s_blinkRestore : rc::pilotOff(rc::pilotTemp(2700, 60));
    sendPilot(back, now);
    s_blinkStep = 0;
  }
}

bool handleParty(uint32_t now) {
  if (!s_partyUntil) return false;
  if (!s_ip) {
    s_partyUntil = 0;
    return false;
  }
  if (static_cast<int32_t>(now - s_partyUntil) >= 0) {
    rc::Pilot back = s_pending ? s_desired : s_partyRestore;
    sendPilot(back, now);
    s_actual = back;
    s_pending = false;
    s_pendingFinal = false;
    s_partyUntil = 0;
    return true;
  }
  if (static_cast<int32_t>(now - s_partyAt) < 0) return true;
  rc::Rgb c = s_partyStep & 1 ? s_partyB : s_partyA;
  rc::Pilot p = rc::pilotRgbRaw(c.r, c.g, c.b, 100);
  if (s_partyAlert) p = s_partyStep & 1 ? s_partyRestore : rc::pilotRgbRaw(255, 0, 0, 100);
  sendPilot(p, now);
  s_partyStep++;
  s_partyAt = now + (s_partyAlert ? cfg::WIZ_ALERT_STEP_MS : cfg::WIZ_PARTY_STEP_MS);
  return true;
}

void writeDenied() {
  s_actual = rc::Pilot{};
  s_lastHeartbeat = 0;
  if (s_denied) return;
  s_denied = true;
  diag::note("wiz", "ampul komutlari reddediyor (-32602): WiZ uygulamasi > Ayarlar > ev > Guvenlik > Allow all controls");
}

void handleReply(uint32_t fromIp, size_t len, uint32_t now) {
  rc::WizReply r;
  if (!rc::wizParseReply(s_rx, len, s_arena, r)) {
    if (r.method == rc::WizMethod::SetPilot && r.errorCode == rc::WIZ_ERR_INVALID_PARAMS && s_ip && fromIp == s_ip)
      writeDenied();
    return;
  }
  if (r.method == rc::WizMethod::SetPilot && r.success && s_ip && fromIp == s_ip) s_denied = false;
  if (r.hasMac) {
    s_found.add(fromIp, r.mac, now);
    if (s_target[0] && rc::macEqual(r.mac, s_target) && s_ip != fromIp) {
      s_ip = fromIp;
      diag::note("wiz", "ampul adresi guncellendi");
    }
  }
  if (fromIp == s_ip && s_ip) s_tracker.onReply(now);
  if (r.hasPilot && fromIp == s_ip) {
    bool touching = s_interacting || static_cast<int32_t>(s_interactEndMs - now) > 0;
    if (rc::syncAllowed(now, touching, s_lastSendMs, s_everSent, cfg::WIZ_SYNC_QUIET_MS) && !s_pending) {
      s_actual = r.pilot;
    }
  }
  if (r.method == rc::WizMethod::SetPilot && r.success && fromIp == s_ip && !s_pending) {
    s_actual = s_throttle.lastSent();
  }
}

void pollSocket(uint32_t now) {
  for (int guard = 0; guard < 8; guard++) {
    sockaddr_in from = {};
    socklen_t fl = sizeof(from);
    int n = lwip_recvfrom(s_sock, s_rx, sizeof(s_rx) - 1, MSG_DONTWAIT, reinterpret_cast<sockaddr*>(&from), &fl);
    if (n <= 0) return;
    s_rx[n] = 0;
    handleReply(from.sin_addr.s_addr, static_cast<size_t>(n), now);
  }
}

void waitSocket(uint32_t ms) {
  if (s_sock < 0) {
    vTaskDelay(pdMS_TO_TICKS(ms));
    return;
  }
  fd_set rd;
  FD_ZERO(&rd);
  FD_SET(s_sock, &rd);
  timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = static_cast<long>(ms) * 1000L;
  lwip_select(s_sock + 1, &rd, nullptr, nullptr, &tv);
}

void publish(uint32_t now) {
  BulbState b;
  memset(&b, 0, sizeof(b));
  b.paired = s_target[0] ? 1 : 0;
  b.found = s_ip ? 1 : 0;
  b.online = (s_ip && s_tracker.online()) ? 1 : 0;
  b.scanning = s_scanning ? 1 : 0;
  b.writeDenied = s_denied ? 1 : 0;
  b.ip = s_ip;
  b.lastSeenMs = s_tracker.lastReplyMs();
  memcpy(b.mac, s_target, sizeof(b.mac));
  b.pilot = s_pending ? s_desired : s_actual;
  if (!s_actual.valid && !s_pending && s_throttle.everSent()) b.pilot = s_throttle.lastSent();
  int n = s_found.count();
  b.candidateCount = static_cast<uint8_t>(n);
  for (int i = 0; i < n && i < rc::BulbList::CAP; i++) {
    b.candidates[i] = s_found.at(i);
    b.candidates[i].seenMs = 0;
  }
  b.lastSeenMs = 0;
  if (s_publishedOnce && memcmp(&b, &s_published, sizeof(b)) == 0) return;
  s_published = b;
  s_publishedOnce = true;
  store::putT(store::BULB, b);
  (void)now;
}

void step(uint32_t now) {
  takeMailbox(now);
  if (WiFi.status() != WL_CONNECTED) {
    if (s_sock >= 0) closeSocket();
    s_scanning = false;
    publish(now);
    return;
  }
  if (!openSocket()) {
    publish(now);
    return;
  }
  loadTarget();
  pollSocket(now);

  if (s_scanning) {
    if (static_cast<int32_t>(now - s_scanUntil) >= 0) s_scanning = false;
    else if (now - s_lastDiscovery >= cfg::WIZ_DISCOVERY_MS) sendDiscovery(now);
  }

  bool needFind = s_target[0] && (!s_ip || !s_tracker.online());
  uint32_t findEvery = s_ip ? cfg::WIZ_REDISCOVER_MS : cfg::WIZ_DISCOVERY_MS * 3;
  if (needFind && !s_scanning && (s_lastDiscovery == 0 || now - s_lastDiscovery >= findEvery)) sendDiscovery(now);

  handleBlink(now);
  bool party = handleParty(now);

  if (s_pending && s_ip && !s_blinkStep && !party) {
    bool sameAsSent = s_throttle.everSent() && rc::pilotSame(s_desired, s_throttle.lastSent());
    bool send = false;
    if (s_pendingFinal) send = !(s_everSent && now - s_lastSendMs < cfg::WIZ_SEND_MIN_MS / 2);
    else if (sameAsSent) s_pending = false;
    else send = s_throttle.shouldSend(now, s_desired, false);
    if (send) {
      sendPilot(s_desired, now);
      s_actual = s_desired;
      s_pending = false;
      s_pendingFinal = false;
    }
  } else if (s_pending && (!s_target[0] || now - s_pendingSince > PENDING_DROP_MS)) {
    s_pending = false;
    s_pendingFinal = false;
  }

  bool touching = s_interacting || static_cast<int32_t>(s_interactEndMs - now) > 0;
  if (s_ip && !party && now - s_lastHeartbeat >= cfg::WIZ_HEARTBEAT_MS &&
      rc::syncAllowed(now, touching, s_lastSendMs, s_everSent, cfg::WIZ_SYNC_QUIET_MS)) {
    sendGetPilot(now);
  }
  s_tracker.tick(now);
  publish(now);
}

void task(void*) {
  s_arena.attach(mem::wizArena(), cfg::WIZ_JSON_ARENA_BYTES);
  loadTarget();
  s_lastDiscovery = 0;
  for (;;) {
    step(millis());
    waitSocket(LOOP_MS);
  }
}

}

void begin() {
  if (s_task) return;
  xTaskCreatePinnedToCore(task, "wiz", cfg::WIZ_STACK, nullptr, cfg::WIZ_PRIO, &s_task, cfg::WIZ_CORE);
}

void setDesired(const rc::Pilot& p, bool final) {
  portENTER_CRITICAL(&s_mux);
  s_box.desired = p;
  s_box.hasDesired = 1;
  if (final) s_box.final = 1;
  portEXIT_CRITICAL(&s_mux);
}

void setInteracting(bool on) {
  s_interacting = on;
  if (!on) s_interactEndMs = millis() + cfg::WIZ_SYNC_QUIET_MS;
}

void togglePower() {
  portENTER_CRITICAL(&s_mux);
  s_box.toggle = 1;
  portEXIT_CRITICAL(&s_mux);
}

void celebrate() {
  portENTER_CRITICAL(&s_mux);
  s_box.celebrate = 1;
  portEXIT_CRITICAL(&s_mux);
}

void alert() {
  portENTER_CRITICAL(&s_mux);
  s_box.alert = 1;
  portEXIT_CRITICAL(&s_mux);
}

void scan() {
  portENTER_CRITICAL(&s_mux);
  s_box.scan = 1;
  portEXIT_CRITICAL(&s_mux);
}

void pair(const char* mac) {
  char m[13];
  if (!rc::macNormalize(mac, m)) return;
  char name[24];
  snprintf(name, sizeof(name), "WiZ %c%c%c%c", m[8] >= 'a' ? m[8] - 32 : m[8], m[9] >= 'a' ? m[9] - 32 : m[9],
           m[10] >= 'a' ? m[10] - 32 : m[10], m[11] >= 'a' ? m[11] - 32 : m[11]);
  settings::setBulb(m, name);
  portENTER_CRITICAL(&s_mux);
  memcpy(s_box.mac, m, sizeof(s_box.mac));
  s_box.pair = 1;
  portEXIT_CRITICAL(&s_mux);
}

void forget() {
  settings::setBulb("", "");
  portENTER_CRITICAL(&s_mux);
  s_box.forget = 1;
  portEXIT_CRITICAL(&s_mux);
}

uint32_t stackFree() { return s_task ? uxTaskGetStackHighWaterMark(s_task) : 0; }

}
