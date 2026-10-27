#include "bridge.h"

#include <Arduino.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "app/config.h"
#include "diag/diag.h"
#include "fwupdate.h"

#if !RUMY_PI
#include "app/mem.h"
#include <Preferences.h>
#include <WiFi.h>
#include <errno.h>
#include <lwip/sockets.h>
#endif

namespace bridge {

namespace {

class Out : public Print {
 public:
  size_t write(uint8_t c) override {
    bridge::write(&c, 1);
    return 1;
  }
  size_t write(const uint8_t* data, size_t n) override {
    bridge::write(data, n);
    return n;
  }
};

Out s_out;

#if !RUMY_PI

constexpr size_t KEY_LEN = 32;
constexpr size_t RX_BUF = 256;
constexpr const char* NS = "kopru";

char s_key[KEY_LEN + 1] = {};
int s_listen = -1;
int s_sock = -1;
int s_cand = -1;
uint32_t s_candSince = 0;
uint32_t s_candPeer = 0;
uint32_t s_peer = 0;
uint32_t s_lastRx = 0;
volatile bool s_bulk = false;
uint32_t s_pauseUntil = 0;
char s_auth[48];
size_t s_authLen = 0;
uint8_t s_rx[RX_BUF];
size_t s_rxPos = 0;
size_t s_rxLen = 0;
uint32_t s_sessions = 0;
uint32_t s_rejects = 0;
uint32_t s_rxBytes = 0;
uint32_t s_txBytes = 0;

void ipText(uint32_t ip, char* buf, size_t n) {
  snprintf(buf, n, "%u.%u.%u.%u", static_cast<unsigned>(ip & 0xFF), static_cast<unsigned>((ip >> 8) & 0xFF),
           static_cast<unsigned>((ip >> 16) & 0xFF), static_cast<unsigned>((ip >> 24) & 0xFF));
}

void nonBlocking(int fd) { lwip_fcntl(fd, F_SETFL, lwip_fcntl(fd, F_GETFL, 0) | O_NONBLOCK); }

void closeClient(const char* why) {
  if (s_sock < 0) return;
  lwip_close(s_sock);
  s_sock = -1;
  s_rxPos = 0;
  s_rxLen = 0;
  diag::note("kopru", "wifi baglantisi kapandi (%s)", why);
}

void closeCand() {
  if (s_cand >= 0) lwip_close(s_cand);
  s_cand = -1;
  memset(s_auth, 0, sizeof(s_auth));
  s_authLen = 0;
}

void closeListen() {
  if (s_listen >= 0) lwip_close(s_listen);
  s_listen = -1;
}

bool openListen() {
  int fd = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (fd < 0) return false;
  int one = 1;
  lwip_setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  sockaddr_in a;
  memset(&a, 0, sizeof(a));
  a.sin_family = AF_INET;
  a.sin_port = htons(static_cast<uint16_t>(cfg::BRIDGE_PORT));
  a.sin_addr.s_addr = htonl(INADDR_ANY);
  if (lwip_bind(fd, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0 || lwip_listen(fd, 1) != 0) {
    lwip_close(fd);
    return false;
  }
  nonBlocking(fd);
  s_listen = fd;
  return true;
}

bool fill() {
  if (s_sock < 0) return false;
  if (s_rxPos == s_rxLen) {
    s_rxPos = 0;
    s_rxLen = 0;
  }
  if (s_rxLen == RX_BUF) return true;
  int n = lwip_recv(s_sock, s_rx + s_rxLen, RX_BUF - s_rxLen, MSG_DONTWAIT);
  if (n > 0) {
    s_rxLen += static_cast<size_t>(n);
    s_rxBytes += static_cast<uint32_t>(n);
    s_lastRx = millis();
    return true;
  }
  if (n == 0) {
    closeClient("karsi taraf kapatti");
    return false;
  }
  if (errno != EAGAIN && errno != EWOULDBLOCK) {
    closeClient("okuma hatasi");
    return false;
  }
  return true;
}

int queued() {
  int n = 0;
  if (s_sock < 0 || lwip_ioctl(s_sock, FIONREAD, &n) != 0) return 0;
  return n;
}

size_t tcpReadBuf(uint8_t* buf, size_t n) {
  size_t got = 0;
  while (got < n && s_sock >= 0) {
    if (s_rxPos == s_rxLen) {
      if (n - got >= RX_BUF) {
        int r = lwip_recv(s_sock, buf + got, n - got, MSG_DONTWAIT);
        if (r > 0) {
          got += static_cast<size_t>(r);
          s_rxBytes += static_cast<uint32_t>(r);
          s_lastRx = millis();
          continue;
        }
        if (r == 0) closeClient("karsi taraf kapatti");
        else if (errno != EAGAIN && errno != EWOULDBLOCK) closeClient("okuma hatasi");
        break;
      }
      if (!fill() || s_rxPos == s_rxLen) break;
    }
    size_t k = s_rxLen - s_rxPos;
    if (k > n - got) k = n - got;
    memcpy(buf + got, s_rx + s_rxPos, k);
    s_rxPos += k;
    got += k;
  }
  return got;
}

void sendRaw(int fd, const uint8_t* data, size_t n) {
  uint32_t t0 = millis();
  while (n && fd >= 0 && fd == s_sock) {
    int r = lwip_send(fd, data, n, MSG_DONTWAIT);
    if (r > 0) {
      data += r;
      n -= static_cast<size_t>(r);
      s_txBytes += static_cast<uint32_t>(r);
      continue;
    }
    if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
      closeClient("yazma hatasi");
      return;
    }
    if (millis() - t0 >= cfg::BRIDGE_SEND_MS) {
      closeClient("yazma zaman asimi");
      return;
    }
    vTaskDelay(1);
  }
}

bool keyMatches(const char* got) {
  if (strlen(got) != KEY_LEN || strlen(s_key) != KEY_LEN) return false;
  uint8_t d = 0;
  for (size_t i = 0; i < KEY_LEN; i++) d |= static_cast<uint8_t>(got[i] ^ s_key[i]);
  return d == 0;
}

void reject(uint32_t now, const char* why) {
  char ip[16];
  ipText(s_candPeer, ip, sizeof(ip));
  s_rejects++;
  s_pauseUntil = now + cfg::BRIDGE_FAIL_PAUSE_MS;
  closeCand();
  diag::note("kopru", "%s baglantisi reddedildi (%s)", ip, why);
}

void promote(uint32_t now) {
  if (s_sock >= 0) closeClient("yeni baglanti");
  s_sock = s_cand;
  s_peer = s_candPeer;
  s_cand = -1;
  memset(s_auth, 0, sizeof(s_auth));
  s_authLen = 0;
  s_rxPos = 0;
  s_rxLen = 0;
  s_lastRx = now;
  s_sessions++;
  char elf[9];
  char upd[9];
  fwupdate::runningElf(elf, sizeof(elf));
  fwupdate::updaterElf(upd, sizeof(upd));
  char hello[64];
  int n = snprintf(hello, sizeof(hello), "@merhaba %d %dx%d %s %s\n", static_cast<int>(cfg::MEDIA_ART),
                   static_cast<int>(cfg::CAM_W), static_cast<int>(cfg::CAM_H), elf[0] ? elf : "-",
                   upd[0] ? upd : "-");
  if (n > 0) sendRaw(s_sock, reinterpret_cast<const uint8_t*>(hello), static_cast<size_t>(n));
  char ip[16];
  ipText(s_peer, ip, sizeof(ip));
  diag::note("kopru", "wifi baglandi %s", ip);
}

void authStep(uint32_t now) {
  for (;;) {
    char c;
    int r = lwip_recv(s_cand, &c, 1, MSG_DONTWAIT);
    if (r == 0 || (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
      closeCand();
      return;
    }
    if (r < 0) break;
    if (c == '\r') continue;
    if (c == '\n') {
      s_auth[s_authLen] = 0;
      bool ok = !strncmp(s_auth, "@anahtar ", 9) && keyMatches(s_auth + 9);
      if (ok) promote(now);
      else reject(now, "anahtar");
      return;
    }
    if (s_authLen + 1 >= sizeof(s_auth)) {
      reject(now, "uzun satir");
      return;
    }
    s_auth[s_authLen++] = c;
  }
  if (now - s_candSince >= cfg::BRIDGE_AUTH_MS) reject(now, "zaman asimi");
}

void acceptOne(uint32_t now) {
  if (static_cast<int32_t>(now - s_pauseUntil) < 0) return;
  sockaddr_in a;
  socklen_t len = sizeof(a);
  int fd = lwip_accept(s_listen, reinterpret_cast<sockaddr*>(&a), &len);
  if (fd < 0) return;
  if (s_cand >= 0) closeCand();
  nonBlocking(fd);
  int one = 1;
  lwip_setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  lwip_setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
  s_cand = fd;
  s_candSince = now;
  s_candPeer = a.sin_addr.s_addr;
  s_authLen = 0;
}

#endif

}

#if !RUMY_PI

void begin() {
  Preferences p;
  if (!p.begin(NS, false)) return;
  char k[KEY_LEN + 1] = {};
  size_t n = p.isKey("anahtar") ? p.getString("anahtar", k, sizeof(k)) : 0;
  p.end();
  if (n == KEY_LEN + 1 || n == KEY_LEN) memcpy(s_key, k, sizeof(s_key));
  s_key[KEY_LEN] = 0;
  if (strlen(s_key) != KEY_LEN) s_key[0] = 0;
}

void service(uint32_t now) {
  bool up = s_key[0] && WiFi.status() == WL_CONNECTED;
  if (!up) {
    closeCand();
    closeClient("wifi yok");
    closeListen();
    return;
  }
  if (s_listen < 0 && !openListen()) return;
  acceptOne(now);
  if (s_cand >= 0) authStep(now);
  if (s_sock >= 0 && now - s_lastRx >= cfg::BRIDGE_IDLE_MS) closeClient("sessizlik");
}

bool wifi() { return s_sock >= 0; }

int tcpAvailable() {
  if (s_sock < 0) return 0;
  if (s_rxPos == s_rxLen && !fill()) return 0;
  return static_cast<int>(s_rxLen - s_rxPos) + queued();
}

int tcpRead() {
  uint8_t b;
  return tcpReadBuf(&b, 1) == 1 ? b : -1;
}

int available() { return wifi() ? tcpAvailable() : Serial.available(); }

size_t read(uint8_t* buf, size_t n) { return wifi() ? tcpReadBuf(buf, n) : Serial.read(buf, n); }

void write(const uint8_t* data, size_t n) {
  if (wifi()) sendRaw(s_sock, data, n);
  else Serial.write(data, n);
}

bool setKey(const char* hex) {
  char k[KEY_LEN + 1] = {};
  if (hex[0]) {
    if (strlen(hex) != KEY_LEN) return false;
    for (size_t i = 0; i < KEY_LEN; i++) {
      if (!isxdigit(static_cast<unsigned char>(hex[i]))) return false;
      k[i] = static_cast<char>(tolower(static_cast<unsigned char>(hex[i])));
    }
  }
  Preferences p;
  if (!p.begin(NS, false)) return false;
  bool ok = hex[0] ? p.putString("anahtar", k) == KEY_LEN : (!p.isKey("anahtar") || p.remove("anahtar"));
  p.end();
  memset(s_key, 0, sizeof(s_key));
  if (!ok) return false;
  memcpy(s_key, k, sizeof(s_key));
  closeCand();
  closeClient("anahtar degisti");
  closeListen();
  return true;
}

bool hasKey() { return s_key[0] != 0; }

uint32_t byteTimeoutMs() { return wifi() ? cfg::CAM_BYTE_TIMEOUT_WIFI_MS : cfg::CAM_BYTE_TIMEOUT_MS; }

void bulk(bool on) {
  s_bulk = on;
  if (!on) s_lastRx = millis();
}

bool bulkActive() { return s_bulk; }

void pace() {
  uint32_t t0 = millis();
  while (wifi()) {
    uint32_t heap = mem::freeHeap();
    uint32_t waited = millis() - t0;
    if (heap >= cfg::BRIDGE_PACE_HEAP || waited >= cfg::BRIDGE_PACE_LOW_MS) return;
    if (heap >= cfg::BRIDGE_PACE_LOW_HEAP && waited >= cfg::BRIDGE_PACE_MS) return;
    vTaskDelay(2);
  }
}

void report(Print& out) {
  char ip[16] = "-";
  if (s_sock >= 0) ipText(s_peer, ip, sizeof(ip));
  out.printf("[kopru] %s %s · anahtar %s · port %u · oturum %lu · reddedilen %lu · alinan %lu B · gonderilen %lu B\n",
             s_sock >= 0 ? "wifi" : "usb", s_sock >= 0 ? ip : "", s_key[0] ? "var" : "yok",
             static_cast<unsigned>(s_listen >= 0 ? cfg::BRIDGE_PORT : 0), static_cast<unsigned long>(s_sessions),
             static_cast<unsigned long>(s_rejects), static_cast<unsigned long>(s_rxBytes),
             static_cast<unsigned long>(s_txBytes));
}

#else

void begin() {}

void service(uint32_t) {}

bool wifi() { return false; }

int tcpAvailable() { return 0; }

int tcpRead() { return -1; }

int available() { return Serial.available(); }

size_t read(uint8_t* buf, size_t n) { return Serial.read(buf, n); }

void write(const uint8_t* data, size_t n) { Serial.write(data, n); }

bool setKey(const char*) { return false; }

bool hasKey() { return false; }

uint32_t byteTimeoutMs() { return cfg::CAM_BYTE_TIMEOUT_MS; }

void pace() {}

void bulk(bool on) { (void)on; }

bool bulkActive() { return false; }

void report(Print& out) { out.println("[kopru] pi: seri hat zaten tcp uzerinden"); }

#endif

void printf(const char* fmt, ...) {
  char buf[224];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n <= 0) return;
  if (static_cast<size_t>(n) >= sizeof(buf)) n = static_cast<int>(sizeof(buf) - 1);
  write(reinterpret_cast<const uint8_t*>(buf), static_cast<size_t>(n));
}

void println(const char* s) { printf("%s\n", s); }

Print& out() { return s_out; }

}
