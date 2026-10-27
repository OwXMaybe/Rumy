#include "paths.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <esp_system.h>

namespace pi {

namespace {

char s_config[256] = {};
esp_reset_reason_t s_reason = ESP_RST_UNKNOWN;
bool s_reasonSet = false;

void markerPath(char* out, size_t cap) { snprintf(out, cap, "%s/calisiyor", RUMY_DATA_DIR); }

bool readLine(const char* path, char* out, size_t cap) {
  out[0] = 0;
  FILE* f = fopen(path, "r");
  if (!f) return false;
  bool ok = fgets(out, static_cast<int>(cap), f) != nullptr;
  fclose(f);
  if (!ok) out[0] = 0;
  out[strcspn(out, "\r\n")] = 0;
  return true;
}

}

const char* dataDir() { return RUMY_DATA_DIR; }

const char* configDir() {
  if (!s_config[0]) {
    const char* xdg = getenv("XDG_CONFIG_HOME");
    const char* home = getenv("HOME");
    if (xdg && xdg[0]) snprintf(s_config, sizeof(s_config), "%s/rumy", xdg);
    else snprintf(s_config, sizeof(s_config), "%s/.config/rumy", home && home[0] ? home : "/tmp");
  }
  return s_config;
}

bool ensureDir(const char* path) {
  char tmp[512];
  snprintf(tmp, sizeof(tmp), "%s", path);
  size_t n = strlen(tmp);
  if (!n) return false;
  for (size_t i = 1; i < n; i++) {
    if (tmp[i] != '/') continue;
    tmp[i] = 0;
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return false;
    tmp[i] = '/';
  }
  return mkdir(tmp, 0755) == 0 || errno == EEXIST;
}

bool ensureParent(const char* path) {
  char tmp[512];
  snprintf(tmp, sizeof(tmp), "%s", path);
  char* slash = strrchr(tmp, '/');
  if (!slash || slash == tmp) return true;
  *slash = 0;
  return ensureDir(tmp);
}

bool writeAtomic(const char* path, const void* data, size_t len, bool secret) {
  if (!ensureParent(path)) return false;
  char tmp[520];
  snprintf(tmp, sizeof(tmp), "%s.yeni", path);
  int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, secret ? 0600 : 0644);
  if (fd < 0) return false;
  const uint8_t* p = static_cast<const uint8_t*>(data);
  size_t done = 0;
  while (done < len) {
    ssize_t w = write(fd, p + done, len - done);
    if (w < 0) {
      if (errno == EINTR) continue;
      close(fd);
      unlink(tmp);
      return false;
    }
    done += static_cast<size_t>(w);
  }
  if (fsync(fd) != 0 || close(fd) != 0) {
    unlink(tmp);
    return false;
  }
  if (rename(tmp, path) != 0) {
    unlink(tmp);
    return false;
  }
  return true;
}

bool readAll(const char* path, uint8_t* buf, size_t cap, size_t& len) {
  len = 0;
  int fd = open(path, O_RDONLY | O_CLOEXEC);
  if (fd < 0) return false;
  bool ok = true;
  while (len < cap) {
    ssize_t r = read(fd, buf + len, cap - len);
    if (r < 0) {
      if (errno == EINTR) continue;
      ok = false;
      break;
    }
    if (r == 0) break;
    len += static_cast<size_t>(r);
  }
  if (ok && len == cap) {
    uint8_t extra;
    if (read(fd, &extra, 1) > 0) ok = false;
  }
  close(fd);
  return ok;
}

uint32_t systemUptimeS() {
  FILE* f = fopen("/proc/uptime", "r");
  if (!f) return 0;
  double up = 0;
  int n = fscanf(f, "%lf", &up);
  fclose(f);
  return n == 1 && up > 0 ? static_cast<uint32_t>(up) : 0;
}

void bootInit() {
  ensureDir(RUMY_DATA_DIR);
  ensureDir(RUMY_SD_ROOT);
  ensureDir(configDir());
  char marker[512];
  markerPath(marker, sizeof(marker));
  char boot[64];
  char prev[64];
  readLine("/proc/sys/kernel/random/boot_id", boot, sizeof(boot));
  bool had = readLine(marker, prev, sizeof(prev));
  if (had && (!boot[0] || !prev[0] || !strcmp(prev, boot))) s_reason = ESP_RST_PANIC;
  else if (had || systemUptimeS() < 180) s_reason = ESP_RST_POWERON;
  else s_reason = ESP_RST_SW;
  s_reasonSet = true;
  int fd = open(marker, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  if (fd >= 0) {
    size_t n = strlen(boot);
    ssize_t w = n ? write(fd, boot, n) : 0;
    (void)w;
    close(fd);
  }
}

void markCleanExit() {
  char marker[512];
  markerPath(marker, sizeof(marker));
  unlink(marker);
}

}

esp_reset_reason_t esp_reset_reason() { return pi::s_reasonSet ? pi::s_reason : ESP_RST_UNKNOWN; }
