#include <Arduino.h>
#include <arpa/inet.h>
#include <errno.h>
#include <esp_random.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "app/config.h"
#include "paths.h"

#ifndef RUMY_BRIDGE_PORT
#define RUMY_BRIDGE_PORT 7777
#endif

PiSerial Serial;

namespace {

constexpr size_t RING = 65536;
constexpr size_t AUTH_LINE = 128;
constexpr uint32_t LISTEN_RETRY_MS = 5000;
constexpr int SEND_WAIT_MS = 100;
constexpr int SEND_TRIES = 20;

pthread_mutex_t s_mx = PTHREAD_MUTEX_INITIALIZER;
int s_listen = -1;
int s_client = -1;
bool s_authed = false;
uint32_t s_lastListenTry = 0;
bool s_listenTried = false;
char s_token[65] = {};
bool s_tokenLoaded = false;
uint8_t s_ring[RING];
size_t s_head = 0;
size_t s_count = 0;
char s_auth[AUTH_LINE];
size_t s_authLen = 0;
char s_out[1024];
size_t s_outLen = 0;

void say(const char* fmt, const char* a) {
  fprintf(stdout, fmt, a);
  fflush(stdout);
}

void loadToken() {
  if (s_tokenLoaded) return;
  s_tokenLoaded = true;
  char path[512];
  snprintf(path, sizeof(path), "%s/gizli.json", pi::configDir());
  uint8_t buf[512];
  size_t len = 0;
  if (pi::readAll(path, buf, sizeof(buf) - 1, len)) {
    buf[len] = 0;
    const char* k = strstr(reinterpret_cast<char*>(buf), "\"kopru_anahtari\"");
    if (k) {
      const char* q = strchr(k + 16, '"');
      if (q) {
        const char* e = strchr(q + 1, '"');
        if (e && e > q + 1 && static_cast<size_t>(e - q - 1) < sizeof(s_token)) {
          memcpy(s_token, q + 1, static_cast<size_t>(e - q - 1));
          s_token[e - q - 1] = 0;
          return;
        }
      }
    }
  }
  for (int i = 0; i < 16; i++) snprintf(s_token + i * 2, 3, "%02x", static_cast<unsigned>(esp_random() & 0xFFu));
  char body[160];
  int n = snprintf(body, sizeof(body), "{\n  \"kopru_anahtari\": \"%s\"\n}\n", s_token);
  if (n > 0) pi::writeAtomic(path, body, static_cast<size_t>(n), true);
}

void closeClient(const char* why) {
  if (s_client < 0) return;
  close(s_client);
  s_client = -1;
  s_authed = false;
  s_count = 0;
  s_head = 0;
  s_authLen = 0;
  say("[kopru] baglanti kapandi (%s)\n", why);
}

void ringPush(const uint8_t* p, size_t n) {
  for (size_t i = 0; i < n && s_count < RING; i++) {
    s_ring[(s_head + s_count) % RING] = p[i];
    s_count++;
  }
}

bool sendRaw(const uint8_t* p, size_t n) {
  size_t done = 0;
  int tries = 0;
  while (done < n && s_client >= 0) {
    ssize_t w = send(s_client, p + done, n - done, MSG_NOSIGNAL | MSG_DONTWAIT);
    if (w > 0) {
      done += static_cast<size_t>(w);
      continue;
    }
    if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) && tries++ < SEND_TRIES) {
      pollfd pf = {s_client, POLLOUT, 0};
      poll(&pf, 1, SEND_WAIT_MS);
      continue;
    }
    closeClient("gonderilemedi");
    return false;
  }
  return true;
}

void authByte(uint8_t c, const uint8_t* rest, size_t restLen) {
  if (c == '\r') return;
  if (c != '\n') {
    if (s_authLen + 1 >= AUTH_LINE) {
      closeClient("anahtar yok");
      return;
    }
    s_auth[s_authLen++] = static_cast<char>(c);
    return;
  }
  s_auth[s_authLen] = 0;
  char want[96];
  snprintf(want, sizeof(want), "@anahtar %s", s_token);
  if (strcmp(s_auth, want) != 0) {
    closeClient("anahtar yanlis");
    return;
  }
  s_authed = true;
  s_authLen = 0;
  char hello[32];
  int n = snprintf(hello, sizeof(hello), "@merhaba %ld %ldx%ld\n", static_cast<long>(cfg::MEDIA_ART),
                   static_cast<long>(cfg::CAM_W), static_cast<long>(cfg::CAM_H));
  sendRaw(reinterpret_cast<const uint8_t*>(hello), static_cast<size_t>(n));
  say("[kopru] %s\n", "dogrulandi");
  ringPush(rest, restLen);
}

void openListen() {
  uint32_t now = millis();
  if (s_listen >= 0 || (s_listenTried && now - s_lastListenTry < LISTEN_RETRY_MS)) return;
  s_listenTried = true;
  s_lastListenTry = now;
  int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (fd < 0) return;
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  sockaddr_in a = {};
  a.sin_family = AF_INET;
  a.sin_port = htons(RUMY_BRIDGE_PORT);
  a.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(fd, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0 || listen(fd, 2) != 0) {
    close(fd);
    return;
  }
  s_listen = fd;
}

void pump() {
  pthread_mutex_lock(&s_mx);
  loadToken();
  openListen();
  if (s_listen >= 0) {
    for (;;) {
      sockaddr_in from = {};
      socklen_t fl = sizeof(from);
      int fd = accept4(s_listen, reinterpret_cast<sockaddr*>(&from), &fl, SOCK_NONBLOCK | SOCK_CLOEXEC);
      if (fd < 0) break;
      if (s_client >= 0) closeClient("yeni baglanti");
      int one = 1;
      setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
      s_client = fd;
      s_authed = s_token[0] == 0;
      s_count = 0;
      s_head = 0;
      s_authLen = 0;
      char ip[INET_ADDRSTRLEN] = "?";
      inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));
      say("[kopru] baglandi %s\n", ip);
    }
  }
  uint8_t tmp[4096];
  while (s_client >= 0 && s_count < RING) {
    size_t room = RING - s_count;
    ssize_t r = recv(s_client, tmp, room < sizeof(tmp) ? room : sizeof(tmp), MSG_DONTWAIT);
    if (r == 0) {
      closeClient("karsi taraf kapatti");
      break;
    }
    if (r < 0) {
      if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) closeClient("okuma hatasi");
      break;
    }
    size_t n = static_cast<size_t>(r);
    if (s_authed) {
      ringPush(tmp, n);
      continue;
    }
    for (size_t i = 0; i < n && s_client >= 0 && !s_authed; i++) {
      bool last = tmp[i] == '\n';
      authByte(tmp[i], tmp + i + 1, last ? n - i - 1 : 0);
      if (last && s_authed) break;
    }
  }
  pthread_mutex_unlock(&s_mx);
}

void mirror(const uint8_t* b, size_t n) {
  for (size_t i = 0; i < n; i++) {
    char c = static_cast<char>(b[i]);
    if (c == '\n') {
      if (s_outLen && s_out[0] != '@') {
        fwrite(s_out, 1, s_outLen, stdout);
        fputc('\n', stdout);
      }
      s_outLen = 0;
      continue;
    }
    if (s_outLen + 1 < sizeof(s_out)) s_out[s_outLen++] = c;
  }
  fflush(stdout);
}

}

size_t PiSerial::write(uint8_t c) { return write(&c, 1); }

size_t PiSerial::write(const uint8_t* b, size_t n) {
  pthread_mutex_lock(&s_mx);
  mirror(b, n);
  if (s_client >= 0 && s_authed) sendRaw(b, n);
  pthread_mutex_unlock(&s_mx);
  return n;
}

int PiSerial::available() {
  pump();
  pthread_mutex_lock(&s_mx);
  int n = static_cast<int>(s_count);
  pthread_mutex_unlock(&s_mx);
  return n;
}

int PiSerial::read() {
  pthread_mutex_lock(&s_mx);
  bool empty = s_count == 0;
  pthread_mutex_unlock(&s_mx);
  if (empty) pump();
  pthread_mutex_lock(&s_mx);
  int c = -1;
  if (s_count) {
    c = s_ring[s_head];
    s_head = (s_head + 1) % RING;
    s_count--;
  }
  pthread_mutex_unlock(&s_mx);
  return c;
}

size_t PiSerial::read(uint8_t* buf, size_t n) {
  pthread_mutex_lock(&s_mx);
  size_t k = 0;
  while (k < n && s_count) {
    buf[k++] = s_ring[s_head];
    s_head = (s_head + 1) % RING;
    s_count--;
  }
  pthread_mutex_unlock(&s_mx);
  return k;
}

void PiSerial::flush() { fflush(stdout); }
