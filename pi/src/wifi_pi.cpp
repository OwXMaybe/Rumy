#include <WiFi.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

PiWiFi WiFi;

namespace {

struct Snap {
  bool up;
  uint32_t ip;
  uint32_t mask;
  int8_t rssi;
};

pthread_mutex_t s_mx = PTHREAD_MUTEX_INITIALIZER;
Snap s_snap = {};
uint32_t s_at = 0;
bool s_once = false;

int8_t readRssi() {
  FILE* f = fopen("/proc/net/wireless", "r");
  if (!f) return 0;
  char line[256];
  int8_t out = 0;
  while (fgets(line, sizeof(line), f)) {
    char name[32];
    unsigned status = 0;
    float link = 0, level = 0;
    if (sscanf(line, " %31[^:]: %x %f %f", name, &status, &link, &level) == 4 && !strcmp(name, "wlan0")) {
      if (level > 0) level -= 256;
      out = static_cast<int8_t>(level);
      break;
    }
  }
  fclose(f);
  return out;
}

Snap take() {
  Snap s = {};
  ifaddrs* list = nullptr;
  if (getifaddrs(&list) != 0) return s;
  int best = 0;
  for (ifaddrs* a = list; a; a = a->ifa_next) {
    if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET) continue;
    if (!(a->ifa_flags & IFF_UP) || !(a->ifa_flags & IFF_RUNNING) || (a->ifa_flags & IFF_LOOPBACK)) continue;
    int rank = !strcmp(a->ifa_name, "wlan0") ? 2 : 1;
    if (rank <= best) continue;
    best = rank;
    s.up = true;
    s.ip = reinterpret_cast<sockaddr_in*>(a->ifa_addr)->sin_addr.s_addr;
    s.mask = a->ifa_netmask ? reinterpret_cast<sockaddr_in*>(a->ifa_netmask)->sin_addr.s_addr : 0;
  }
  freeifaddrs(list);
  s.rssi = s.up ? readRssi() : 0;
  return s;
}

Snap snap() {
  pthread_mutex_lock(&s_mx);
  uint32_t now = millis();
  if (!s_once || now - s_at >= 1000) {
    s_once = true;
    s_at = now;
    s_snap = take();
  }
  Snap s = s_snap;
  pthread_mutex_unlock(&s_mx);
  return s;
}

}

WlStatus PiWiFi::status() { return snap().up ? WL_CONNECTED : WL_DISCONNECTED; }

int8_t PiWiFi::RSSI() { return snap().rssi; }

IPAddress PiWiFi::localIP() { return IPAddress(snap().ip); }

IPAddress PiWiFi::subnetMask() { return IPAddress(snap().mask); }

WiFiClient::WiFiClient(int fd)
    : fd_(new int(fd), [](int* p) {
        if (*p >= 0) close(*p);
        delete p;
      }) {}

int WiFiClient::available() {
  if (!fd_ || *fd_ < 0) return 0;
  int n = 0;
  if (ioctl(*fd_, FIONREAD, &n) != 0) return 0;
  return n;
}

int WiFiClient::read() {
  uint8_t c;
  return read(&c, 1) == 1 ? c : -1;
}

int WiFiClient::read(uint8_t* buf, size_t n) {
  if (!fd_ || *fd_ < 0) return -1;
  ssize_t r = recv(*fd_, buf, n, MSG_DONTWAIT);
  return r > 0 ? static_cast<int>(r) : -1;
}

bool WiFiClient::connected() {
  if (!fd_ || *fd_ < 0) return false;
  uint8_t c;
  ssize_t r = recv(*fd_, &c, 1, MSG_PEEK | MSG_DONTWAIT);
  if (r > 0) return true;
  if (r == 0) return false;
  return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
}

size_t WiFiClient::write(const uint8_t* buf, size_t n) {
  if (!fd_ || *fd_ < 0) return 0;
  size_t done = 0;
  int tries = 0;
  while (done < n) {
    ssize_t w = send(*fd_, buf + done, n - done, MSG_NOSIGNAL | MSG_DONTWAIT);
    if (w > 0) {
      done += static_cast<size_t>(w);
      continue;
    }
    if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) && tries++ < 20) {
      pollfd pf = {*fd_, POLLOUT, 0};
      poll(&pf, 1, 50);
      continue;
    }
    break;
  }
  return done;
}

void WiFiClient::stop() {
  if (!fd_) return;
  if (*fd_ >= 0) {
    shutdown(*fd_, SHUT_RDWR);
    close(*fd_);
    *fd_ = -1;
  }
  fd_.reset();
}

void WiFiServer::begin() {
  if (fd_ >= 0) return;
  int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (fd < 0) return;
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  sockaddr_in a = {};
  a.sin_family = AF_INET;
  a.sin_port = htons(port_);
  a.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(fd, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0 || listen(fd, 4) != 0) {
    close(fd);
    return;
  }
  fd_ = fd;
}

void WiFiServer::end() {
  if (fd_ >= 0) close(fd_);
  fd_ = -1;
}

WiFiClient WiFiServer::available() {
  if (fd_ < 0) return WiFiClient();
  int c = accept4(fd_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
  if (c < 0) return WiFiClient();
  if (noDelay_) {
    int one = 1;
    setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  }
  return WiFiClient(c);
}
