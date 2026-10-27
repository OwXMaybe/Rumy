#include "printer.h"

#include <Arduino.h>
#include <WiFi.h>
#include <errno.h>
#include <esp_random.h>
#include <lwip/sockets.h>
#include <mbedtls/base64.h>
#include <string.h>

#include "app/config.h"
#include "app/shared.h"
#include "app/store.h"
#include "diag/diag.h"
#include "k2.h"
#include "net/fetch.h"
#include "settings/settings.h"

namespace printer {

namespace {

enum Conn : uint8_t { C_IDLE = 0, C_CONNECTING, C_HANDSHAKE, C_OPEN };

enum Op : uint8_t { OP_CONT = 0x0, OP_TEXT = 0x1, OP_BINARY = 0x2, OP_CLOSE = 0x8, OP_PING = 0x9, OP_PONG = 0xA };

int s_sock = -1;
Conn s_conn = C_IDLE;
uint32_t s_ip = 0;
uint32_t s_settingsVer = 0xFFFFFFFFu;
uint32_t s_phaseStart = 0;
uint32_t s_nextTry = 0;
uint32_t s_backoff = 0;
uint32_t s_lastRx = 0;
uint32_t s_lastReq = 0;
uint32_t s_lastPub = 0;
bool s_dirty = true;
bool s_wifi = false;
bool s_held = false;
bool s_resume = false;
uint32_t s_holds = 0;
PrinterState s_pub;
rc::K2Status s_st;

bool waitSock(bool write, uint32_t ms) {
  fd_set set;
  FD_ZERO(&set);
  FD_SET(s_sock, &set);
  timeval tv;
  tv.tv_sec = static_cast<long>(ms / 1000U);
  tv.tv_usec = static_cast<long>((ms % 1000U) * 1000U);
  int r = write ? lwip_select(s_sock + 1, nullptr, &set, nullptr, &tv) : lwip_select(s_sock + 1, &set, nullptr, nullptr, &tv);
  return r > 0 && FD_ISSET(s_sock, &set);
}

bool readExact(uint8_t* dst, size_t n) {
  size_t got = 0;
  uint32_t t0 = millis();
  while (got < n) {
    int r = lwip_recv(s_sock, dst + got, n - got, MSG_DONTWAIT);
    if (r > 0) {
      got += static_cast<size_t>(r);
      continue;
    }
    if (r == 0) return false;
    if (errno != EAGAIN && errno != EWOULDBLOCK) return false;
    uint32_t spent = millis() - t0;
    if (spent >= cfg::PRINTER_READ_MS) return false;
    waitSock(false, cfg::PRINTER_READ_MS - spent);
  }
  return true;
}

bool sendAll(const uint8_t* data, size_t n) {
  size_t sent = 0;
  uint32_t t0 = millis();
  while (sent < n) {
    int r = lwip_send(s_sock, data + sent, n - sent, MSG_DONTWAIT);
    if (r > 0) {
      sent += static_cast<size_t>(r);
      continue;
    }
    if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK) return false;
    uint32_t spent = millis() - t0;
    if (spent >= cfg::PRINTER_READ_MS) return false;
    waitSock(true, cfg::PRINTER_READ_MS - spent);
  }
  return true;
}

bool sendFrame(uint8_t op, const uint8_t* payload, size_t n) {
  if (n > 125) return false;
  uint8_t frame[6 + 125];
  uint32_t key = esp_random();
  frame[0] = static_cast<uint8_t>(0x80 | op);
  frame[1] = static_cast<uint8_t>(0x80 | n);
  memcpy(frame + 2, &key, 4);
  for (size_t i = 0; i < n; i++) frame[6 + i] = static_cast<uint8_t>(payload[i] ^ frame[2 + (i & 3)]);
  return sendAll(frame, 6 + n);
}

bool sendText(const char* s) { return sendFrame(OP_TEXT, reinterpret_cast<const uint8_t*>(s), strlen(s)); }

bool sendRequest(uint32_t now) {
  char req[64];
  size_t n = rc::k2BuildStatusRequest(req, sizeof(req));
  s_lastReq = now;
  return n && sendText(req);
}

struct Header {
  uint8_t op;
  bool fin;
  uint32_t len;
};

bool readHeader(Header& h) {
  uint8_t b[8];
  if (!readExact(b, 2)) return false;
  h.op = b[0] & 0x0F;
  h.fin = (b[0] & 0x80) != 0;
  if (b[1] & 0x80) return false;
  uint32_t len = b[1] & 0x7F;
  if (len == 126) {
    if (!readExact(b, 2)) return false;
    len = (static_cast<uint32_t>(b[0]) << 8) | b[1];
  } else if (len == 127) {
    if (!readExact(b, 8)) return false;
    if (b[0] || b[1] || b[2] || b[3]) return false;
    len = (static_cast<uint32_t>(b[4]) << 24) | (static_cast<uint32_t>(b[5]) << 16) | (static_cast<uint32_t>(b[6]) << 8) | b[7];
  }
  h.len = len;
  return true;
}

bool skip(uint32_t n) {
  uint8_t tmp[64];
  while (n) {
    uint32_t k = n < sizeof(tmp) ? n : sizeof(tmp);
    if (!readExact(tmp, k)) return false;
    n -= k;
  }
  return true;
}

bool control(const Header& h) {
  if (h.op == OP_CLOSE) return false;
  if (h.len > 125) return false;
  uint8_t payload[125];
  if (!readExact(payload, h.len)) return false;
  if (h.op == OP_PING) return sendFrame(OP_PONG, payload, h.len);
  return true;
}

class WsSource final : public rc::ByteSource {
 public:
  WsSource(uint32_t len, bool fin) : left_(len), fin_(fin) {}

  int read() override {
    int c = peek();
    if (c >= 0) pos_++;
    return c;
  }

  int peek() override {
    if (pos_ >= n_ && !fill()) return -1;
    return buf_[pos_];
  }

  size_t readBytes(char* out, size_t len) override {
    size_t k = 0;
    while (k < len) {
      int c = read();
      if (c < 0) break;
      out[k++] = static_cast<char>(c);
    }
    return k;
  }

  bool drain() {
    while (!failed_) {
      pos_ = n_;
      if (!fill()) break;
    }
    return !failed_;
  }

 private:
  bool fill() {
    while (left_ == 0) {
      if (fin_) return false;
      Header h;
      if (!readHeader(h)) {
        failed_ = true;
        return false;
      }
      if (h.op >= OP_CLOSE) {
        if (!control(h)) {
          failed_ = true;
          return false;
        }
        continue;
      }
      if (h.op != OP_CONT) {
        failed_ = true;
        return false;
      }
      left_ = h.len;
      fin_ = h.fin;
    }
    size_t k = left_ < sizeof(buf_) ? left_ : sizeof(buf_);
    if (!readExact(buf_, k)) {
      failed_ = true;
      return false;
    }
    left_ -= static_cast<uint32_t>(k);
    n_ = k;
    pos_ = 0;
    return true;
  }

  uint8_t buf_[128];
  size_t n_ = 0;
  size_t pos_ = 0;
  uint32_t left_;
  bool fin_;
  bool failed_ = false;
};

void publish(uint32_t now, bool force) {
  if (!force && !s_dirty) return;
  if (!force && now - s_lastPub < cfg::PRINTER_PUBLISH_MS) return;
  rc::K2Phase phase = s_pub.connected ? rc::k2Phase(s_st) : rc::K2_OFFLINE;
  s_pub.phase = phase;
  s_pub.problem = s_pub.connected && rc::k2Problem(s_st) ? 1 : 0;
  s_pub.st = s_st;
  s_pub.arenaPeak = static_cast<uint16_t>(net::arena().peak());
  s_lastPub = now;
  s_dirty = false;
  store::putT(store::PRINTER, s_pub);
}

void closeSock(uint32_t now, bool retry) {
  if (s_sock >= 0) lwip_close(s_sock);
  s_sock = -1;
  bool was = s_conn == C_OPEN;
  s_conn = C_IDLE;
  s_resume = false;
  if (retry) {
    s_backoff = s_backoff ? s_backoff * 2 : cfg::PRINTER_RETRY_MIN_MS;
    if (s_backoff > cfg::PRINTER_RETRY_MAX_MS) s_backoff = cfg::PRINTER_RETRY_MAX_MS;
    s_nextTry = now + s_backoff;
  }
  if (was) diag::note("yazici", "baglanti kapandi");
  if (s_pub.connected) {
    s_pub.connected = 0;
    publish(now, true);
  }
}

void loadSettings(uint32_t now) {
  uint32_t v = settings::version();
  if (v == s_settingsVer) return;
  s_settingsVer = v;
  Settings st = settings::get();
  IPAddress ip;
  uint32_t addr = st.printerHost[0] && ip.fromString(st.printerHost) ? static_cast<uint32_t>(ip) : 0;
  if (addr == s_ip && s_pub.configured == (addr ? 1 : 0)) return;
  closeSock(now, false);
  s_ip = addr;
  rc::k2Clear(s_st);
  s_backoff = 0;
  s_nextTry = now;
  s_pub.configured = addr ? 1 : 0;
  s_pub.ip = addr;
  publish(now, true);
}

void startConnect(uint32_t now) {
  s_sock = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s_sock < 0) {
    closeSock(now, true);
    return;
  }
  lwip_fcntl(s_sock, F_SETFL, lwip_fcntl(s_sock, F_GETFL, 0) | O_NONBLOCK);
  int one = 1;
  lwip_setsockopt(s_sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  sockaddr_in to = {};
  to.sin_family = AF_INET;
  to.sin_port = htons(static_cast<uint16_t>(rc::K2_WS_PORT));
  to.sin_addr.s_addr = s_ip;
  s_phaseStart = now;
  int r = lwip_connect(s_sock, reinterpret_cast<sockaddr*>(&to), sizeof(to));
  if (r == 0 || errno == EINPROGRESS) {
    s_conn = C_CONNECTING;
    return;
  }
  closeSock(now, true);
}

bool sendUpgrade() {
  uint8_t raw[16];
  for (int i = 0; i < 16; i += 4) {
    uint32_t r = esp_random();
    memcpy(raw + i, &r, 4);
  }
  unsigned char key[32];
  size_t klen = 0;
  if (mbedtls_base64_encode(key, sizeof(key), &klen, raw, sizeof(raw)) != 0) return false;
  key[klen] = 0;
  IPAddress ip(s_ip);
  char req[224];
  int n = snprintf(req, sizeof(req),
                   "GET / HTTP/1.1\r\nHost: %u.%u.%u.%u:%u\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                   "Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n",
                   static_cast<unsigned>(ip[0]), static_cast<unsigned>(ip[1]), static_cast<unsigned>(ip[2]),
                   static_cast<unsigned>(ip[3]), static_cast<unsigned>(rc::K2_WS_PORT), key);
  if (n <= 0 || static_cast<size_t>(n) >= sizeof(req)) return false;
  return sendAll(reinterpret_cast<const uint8_t*>(req), static_cast<size_t>(n));
}

void pollConnect(uint32_t now) {
  if (waitSock(true, 0)) {
    int err = 0;
    socklen_t len = sizeof(err);
    lwip_getsockopt(s_sock, SOL_SOCKET, SO_ERROR, &err, &len);
    if (err == 0 && sendUpgrade()) {
      s_conn = C_HANDSHAKE;
      s_phaseStart = now;
      return;
    }
    closeSock(now, true);
    return;
  }
  if (now - s_phaseStart >= cfg::PRINTER_CONNECT_MS) closeSock(now, true);
}

void pollHandshake(uint32_t now) {
  char head[256];
  int n = lwip_recv(s_sock, head, sizeof(head) - 1, MSG_PEEK | MSG_DONTWAIT);
  if (n > 0) {
    head[n] = 0;
    char* end = strstr(head, "\r\n\r\n");
    if (end) {
      size_t take = static_cast<size_t>(end - head) + 4;
      uint8_t sink[256];
      if (!readExact(sink, take) || strncmp(head, "HTTP/1.1 101", 12) != 0) {
        diag::note("yazici", "el sikisma reddedildi");
        closeSock(now, true);
        return;
      }
      s_conn = C_OPEN;
      s_backoff = 0;
      s_lastRx = now;
      if (!s_resume && s_pub.reconnects < 0xFFFF) s_pub.reconnects++;
      s_resume = false;
      s_pub.connected = 1;
      IPAddress ip(s_ip);
      diag::note("yazici", "baglandi %u.%u.%u.%u", static_cast<unsigned>(ip[0]), static_cast<unsigned>(ip[1]),
                 static_cast<unsigned>(ip[2]), static_cast<unsigned>(ip[3]));
      if (!sendRequest(now)) closeSock(now, true);
      else publish(now, true);
      return;
    }
    if (static_cast<size_t>(n) >= sizeof(head) - 1) {
      closeSock(now, true);
      return;
    }
  } else if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
    closeSock(now, true);
    return;
  }
  if (now - s_phaseStart >= cfg::PRINTER_CONNECT_MS) closeSock(now, true);
}

bool readMessage(uint32_t now) {
  Header h;
  if (!readHeader(h)) return false;
  s_lastRx = now;
  if (h.op >= OP_CLOSE) return control(h);
  if (h.op == OP_CONT) return skip(h.len);
  WsSource src(h.len, h.fin);
  uint32_t present = 0;
  rc::K2Phase before = rc::k2Phase(s_st);
  bool problemBefore = rc::k2Problem(s_st);
  bool ok = rc::k2Parse(src, net::arena(), s_st, present);
  if (!src.drain()) return false;
  if (s_pub.msgs < 0xFFFFFFFFu) s_pub.msgs++;
  if (!ok) {
    if (s_pub.parseFails < 0xFFFF) s_pub.parseFails++;
    return true;
  }
  if (present & rc::K2F_HEARTBEAT) {
    if (!sendText("ok")) return false;
  }
  if (present & ~rc::K2F_HEARTBEAT) {
    s_dirty = true;
    s_pub.updatedMs = now;
    bool urgent = rc::k2Phase(s_st) != before || rc::k2Problem(s_st) != problemBefore;
    publish(now, urgent);
  }
  return true;
}

void pollOpen(uint32_t now) {
  for (int k = 0; k < cfg::PRINTER_MSGS_PER_SERVICE; k++) {
    if (!waitSock(false, 0)) break;
    if (!readMessage(now)) {
      closeSock(now, true);
      return;
    }
  }
  if (now - s_lastRx >= cfg::PRINTER_SILENCE_MS) {
    diag::note("yazici", "yazicidan ses yok, yeniden baglaniliyor");
    closeSock(now, true);
    return;
  }
  if (now - s_lastReq >= cfg::PRINTER_REQUEST_MS && !sendRequest(now)) {
    closeSock(now, true);
    return;
  }
  publish(now, false);
}

}

void service(uint32_t now) {
  loadSettings(now);
  bool wifi = WiFi.status() == WL_CONNECTED;
  if (!wifi || !s_ip) {
    if (s_sock >= 0) closeSock(now, false);
    if (wifi != s_wifi) s_nextTry = now;
    s_wifi = wifi;
    return;
  }
  s_wifi = wifi;
  switch (s_conn) {
    case C_IDLE:
      if (static_cast<int32_t>(now - s_nextTry) >= 0) startConnect(now);
      break;
    case C_CONNECTING: pollConnect(now); break;
    case C_HANDSHAKE: pollHandshake(now); break;
    case C_OPEN: pollOpen(now); break;
  }
}

bool holdForTls() {
  if (s_sock < 0) return false;
  rc::K2Phase ph = rc::k2Phase(s_st);
  if (ph != rc::K2_PRINTING && ph != rc::K2_PAUSED && ph != rc::K2_SELFTEST) return false;
  lwip_close(s_sock);
  s_sock = -1;
  s_conn = C_IDLE;
  s_held = true;
  if (s_holds < 0xFFFFFFFFu) s_holds++;
  return true;
}

void releaseAfterTls(uint32_t now) {
  if (!s_held) return;
  s_held = false;
  s_resume = true;
  s_backoff = 0;
  s_nextTry = now;
}

void report(Print& out) {
  PrinterState p;
  store::peekT(store::PRINTER, p);
  if (!p.configured) {
    out.println(F("[yazici] ayarlanmadi · 'yazici <ip>' ile ayarla"));
    return;
  }
  IPAddress ip(p.ip);
  char name[48];
  char mat[12];
  rc::k2JobName(p.st.file, name, sizeof(name), mat, sizeof(mat));
  out.printf("[yazici] %u.%u.%u.%u · %s · %s · %s · %%%u · kalan %u sn · katman %u/%u · hata %d/%d · dosya %s\n",
             static_cast<unsigned>(ip[0]), static_cast<unsigned>(ip[1]), static_cast<unsigned>(ip[2]),
             static_cast<unsigned>(ip[3]), p.connected ? "bagli" : "bagli degil", p.st.host[0] ? p.st.host : "-",
             rc::k2PhaseTr(static_cast<rc::K2Phase>(p.phase)), static_cast<unsigned>(p.st.progress),
             static_cast<unsigned>(p.st.leftS), static_cast<unsigned>(p.st.layer), static_cast<unsigned>(p.st.totalLayer),
             static_cast<int>(p.st.errCode), static_cast<int>(p.st.errKey), name[0] ? name : "-");
  out.printf("[yazici] mesaj %u · baglanti %u · ayristirma hatasi %u · json alani tepe %u · https arasi %lu\n",
             static_cast<unsigned>(p.msgs), static_cast<unsigned>(p.reconnects), static_cast<unsigned>(p.parseFails),
             static_cast<unsigned>(p.arenaPeak), static_cast<unsigned long>(s_holds));
}

}
