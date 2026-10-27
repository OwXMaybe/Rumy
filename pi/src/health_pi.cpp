#include "health_pi.h"

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/timex.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

#include "paths.h"

namespace pi {

namespace {

constexpr unsigned PERIOD_S = 60;
constexpr long POWER_BITS = 0x50005;
constexpr uint32_t WD_MIN_EVERY_MS = 1000;

int s_wdFd = -1;
sockaddr_un s_wdAddr;
socklen_t s_wdLen = 0;
uint32_t s_wdEveryMs = 0;
uint32_t s_wdLastMs = 0;
uint32_t s_wdSent = 0;
uint32_t s_wdFailed = 0;
volatile bool s_wdHeld = false;

uint32_t monoMs() {
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<uint32_t>(ts.tv_sec * 1000ULL + static_cast<uint64_t>(ts.tv_nsec) / 1000000ULL);
}

bool wdSend(const char* msg) {
  ssize_t n = sendto(s_wdFd, msg, strlen(msg), MSG_DONTWAIT | MSG_NOSIGNAL, reinterpret_cast<sockaddr*>(&s_wdAddr),
                     s_wdLen);
  return n == static_cast<ssize_t>(strlen(msg));
}

bool readText(const char* path, char* out, size_t cap) {
  FILE* f = fopen(path, "r");
  if (!f) return false;
  size_t n = fread(out, 1, cap - 1, f);
  fclose(f);
  out[n] = 0;
  return n > 0;
}

long readLong(const char* path, int base) {
  char buf[64];
  if (!readText(path, buf, sizeof(buf))) return -1;
  return strtol(buf, nullptr, base);
}

long throttled() {
  FILE* p = popen("vcgencmd get_throttled 2>/dev/null", "r");
  if (!p) return -1;
  char buf[64] = "";
  bool ok = fgets(buf, sizeof(buf), p) != nullptr;
  pclose(p);
  const char* eq = ok ? strchr(buf, '=') : nullptr;
  return eq ? strtol(eq + 1, nullptr, 16) : -1;
}

long memAvailableKb() {
  FILE* f = fopen("/proc/meminfo", "r");
  if (!f) return -1;
  char line[128];
  long v = -1;
  while (fgets(line, sizeof(line), f)) {
    if (sscanf(line, "MemAvailable: %ld", &v) == 1) break;
  }
  fclose(f);
  return v;
}

void recordPath(char* out, size_t cap) { snprintf(out, cap, "%s/son_durum.txt", dataDir()); }

long snapshot(char* out, size_t cap) {
  char load[64] = "";
  readText("/proc/loadavg", load, sizeof(load));
  load[strcspn(load, " \n")] = 0;
  long temp = readLong("/sys/class/thermal/thermal_zone0/temp", 10);
  long thr = throttled();
  time_t now = time(nullptr);
  struct tm tmv;
  localtime_r(&now, &tmv);
  char ts[32];
  strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
  struct timex tx = {};
  bool synced = adjtimex(&tx) >= 0 && !(tx.status & STA_UNSYNC);
  snprintf(out, cap, "%s saat=%s calisma=%us sicaklik=%ld.%ldC kisma=0x%lx yuk=%s bos_bellek=%ldkB\n", ts,
           synced ? "senkron" : "tahmini", static_cast<unsigned>(systemUptimeS()), temp / 1000, (temp % 1000) / 100,
           thr < 0 ? 0L : thr, load, memAvailableKb());
  return thr;
}

void* healthMain(void*) {
  pthread_setname_np(pthread_self(), "saglik");
  char path[512];
  recordPath(path, sizeof(path));
  bool warned = false;
  for (;;) {
    char line[256];
    long thr = snapshot(line, sizeof(line));
    writeAtomic(path, line, strlen(line), false);
    if (!warned && thr > 0 && (thr & POWER_BITS)) {
      fprintf(stdout, "[saglik] guc/isi uyarisi: kisma=0x%lx (0x1 dusuk voltaj, 0x4 kisildi)\n", thr);
      fflush(stdout);
      warned = true;
    }
    sleep(PERIOD_S);
  }
  return nullptr;
}

}

void healthBegin() {
  char path[512];
  recordPath(path, sizeof(path));
  char prev[256];
  if (readText(path, prev, sizeof(prev))) {
    prev[strcspn(prev, "\n")] = 0;
    fprintf(stdout, "[saglik] onceki son kayit: %s\n", prev);
    fflush(stdout);
  }
  pthread_t t;
  if (pthread_create(&t, nullptr, healthMain, nullptr) == 0) pthread_detach(t);
}

void watchdogBegin() {
  const char* sock = getenv("NOTIFY_SOCKET");
  const char* usec = getenv("WATCHDOG_USEC");
  const char* pid = getenv("WATCHDOG_PID");
  if (!sock || !usec) return;
  if (pid && strtol(pid, nullptr, 10) != static_cast<long>(getpid())) return;
  unsigned long long us = strtoull(usec, nullptr, 10);
  size_t n = strlen(sock);
  if (us == 0 || n < 2 || n >= sizeof(s_wdAddr.sun_path) || (sock[0] != '/' && sock[0] != '@')) return;
  memset(&s_wdAddr, 0, sizeof(s_wdAddr));
  s_wdAddr.sun_family = AF_UNIX;
  memcpy(s_wdAddr.sun_path, sock, n);
  if (sock[0] == '@') s_wdAddr.sun_path[0] = 0;
  s_wdLen = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + n);
  s_wdFd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  if (s_wdFd < 0) return;
  uint64_t every = us / 3000ULL;
  s_wdEveryMs = every < WD_MIN_EVERY_MS ? WD_MIN_EVERY_MS : static_cast<uint32_t>(every);
  wdSend("READY=1");
  fprintf(stdout, "[bekci] systemd bekcisi %llu sn, bildirim %u ms'de bir\n", us / 1000000ULL,
          static_cast<unsigned>(s_wdEveryMs));
  fflush(stdout);
  watchdogKick();
}

void watchdogKick() {
  if (s_wdFd < 0 || s_wdHeld) return;
  uint32_t now = monoMs();
  if (s_wdSent && now - s_wdLastMs < s_wdEveryMs) return;
  s_wdLastMs = now;
  if (wdSend("WATCHDOG=1")) s_wdSent++;
  else s_wdFailed++;
}

void watchdogHold() {
  s_wdHeld = true;
  fprintf(stdout, "[bekci] bildirim durduruldu; systemd Rumy'yi yeniden baslatmali\n");
  fflush(stdout);
}

void watchdogStatus(char* out, size_t cap) {
  if (s_wdFd < 0) {
    snprintf(out, cap, "bekci kapali (servis biriminde WatchdogSec yok)");
    return;
  }
  snprintf(out, cap, "bekci acik · bildirim %u ms'de bir · gonderilen %u · hata %u · son %u ms once%s",
           static_cast<unsigned>(s_wdEveryMs), static_cast<unsigned>(s_wdSent), static_cast<unsigned>(s_wdFailed),
           static_cast<unsigned>(monoMs() - s_wdLastMs), s_wdHeld ? " · DURDURULDU" : "");
}

}
