#include "board/display.h"

#include <Arduino.h>
#include <dirent.h>
#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <lgfx_tjpgd.h>
#include <lvgl.h>
#include <math.h>
#include <mbedtls/base64.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include "app/config.h"
#include "paths.h"

namespace display {

namespace {

constexpr int W = cfg::SCREEN_W;
constexpr int H = cfg::SCREEN_H;
constexpr uint32_t FB_RETRY_MS = 2000;
constexpr uint32_t TOUCH_RETRY_MS = 5000;
constexpr int MAX_RECTS = 4;

enum Backend : uint8_t { BK_NONE, BK_FAKE, BK_FBDEV, BK_DRM };

struct Rect {
  int x1;
  int y1;
  int x2;
  int y2;
};

uint16_t s_gram[W * H];
pthread_mutex_t s_mx = PTHREAD_MUTEX_INITIALIZER;

Backend s_backend = BK_NONE;
int s_fbFd = -1;
uint8_t* s_fb = nullptr;
size_t s_fbLen = 0;
int s_fbW = 0;
int s_fbH = 0;
int s_fbBpp = 0;
int s_fbStride = 0;
fb_var_screeninfo s_var = {};
uint32_t s_fbTryAt = 0;
bool s_fbTried = false;
char s_fbName[96] = "yok";
bool s_emulateScroll = false;

uint32_t s_drmFb = 0;
pthread_mutex_t s_dmx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t s_dcv = PTHREAD_COND_INITIALIZER;
pthread_cond_t s_taken;
constexpr long PACE_WAIT_MS = 250;
Rect s_rects[MAX_RECTS];
int s_rectN = 0;
bool s_flushThread = false;
uint32_t s_panelErrors = 0;

int s_dstW = W;
int s_dstH = H;
int s_offX = 0;
int s_offY = 0;
int16_t s_colX0[640];
uint8_t s_colW[640];
int16_t s_rowY0[480];
uint8_t s_rowW[480];
int16_t s_gx0[640];
int16_t s_gx1[640];
uint16_t s_scroll = 0;
bool s_flip = false;
bool s_sharp = false;
uint16_t s_brightLevel = 256;
uint8_t s_brightness = 80;

lv_display_t* s_disp = nullptr;
lv_indev_t* s_indev = nullptr;

int s_touchFd = -1;
uint32_t s_touchTryAt = 0;
bool s_touchTried = false;
int s_rawX = 0;
int s_rawY = 0;
bool s_down = false;
int s_minX = 0;
int s_maxX = 4095;
int s_minY = 0;
int s_maxY = 4095;
uint8_t s_touchRot = 0;
volatile uint32_t s_lastTouch = 0;
volatile bool s_touching = false;
bool s_logTouch = false;
uint32_t s_touchReads = 0;
FlushPerf s_perf = {};

struct TouchCal {
  bool set;
  bool swap;
  int32_t a0;
  int32_t a1;
  int32_t b0;
  int32_t b1;
};

TouchCal s_cal = {};
TouchCal s_calPrev = {};
bool s_calTrial = false;

uint32_t tickCb() { return millis(); }

bool readSmall(const char* path, char* out, size_t cap) {
  FILE* f = fopen(path, "r");
  if (!f) return false;
  bool ok = fgets(out, static_cast<int>(cap), f) != nullptr;
  fclose(f);
  if (ok) out[strcspn(out, "\r\n")] = 0;
  return ok;
}

void buildTables() {
  if (s_dstW > 640) s_dstW = 640;
  if (s_dstH > 480) s_dstH = 480;
  for (int x = 0; x < s_dstW; x++) {
    int32_t fx = ((2 * x + 1) * W * 256) / (2 * s_dstW) - 128;
    if (fx < 0) fx = 0;
    int32_t x0 = fx >> 8;
    if (x0 > W - 1) x0 = W - 1;
    s_colX0[x] = static_cast<int16_t>(x0);
    s_colW[x] = x0 >= W - 1 ? 0 : static_cast<uint8_t>(fx & 0xFF);
  }
  for (int y = 0; y < s_dstH; y++) {
    int32_t fy = ((2 * y + 1) * H * 256) / (2 * s_dstH) - 128;
    if (fy < 0) fy = 0;
    int32_t y0 = fy >> 8;
    if (y0 > H - 1) y0 = H - 1;
    s_rowY0[y] = static_cast<int16_t>(y0);
    s_rowW[y] = y0 >= H - 1 ? 0 : static_cast<uint8_t>(fy & 0xFF);
  }
}

void buildScrollTables() {
  for (int x = 0; x < s_dstW; x++) {
    int x0 = s_colX0[x];
    int x1 = x0 + 1 < W ? x0 + 1 : x0;
    s_gx0[x] = static_cast<int16_t>((x0 + s_scroll) % W);
    s_gx1[x] = static_cast<int16_t>((x1 + s_scroll) % W);
  }
}

void loadScreenCfg() {
  char path[512];
  snprintf(path, sizeof(path), "%s/ekran.json", pi::configDir());
  uint8_t buf[512];
  size_t len = 0;
  if (pi::readAll(path, buf, sizeof(buf) - 1, len)) {
    buf[len] = 0;
    const char* text = reinterpret_cast<const char*>(buf);
    const char* d = strstr(text, "\"dondur\"");
    if (d && strchr(d, ':')) s_flip = strtol(strchr(d, ':') + 1, nullptr, 10) == 180;
    const char* o = strstr(text, "\"olcek\"");
    if (o && strchr(o, ':')) s_sharp = strtol(strchr(o, ':') + 1, nullptr, 10) == 1;
  }
  const char* env = getenv("RUMY_DONDUR");
  if (env) s_flip = !strcmp(env, "180");
  env = getenv("RUMY_OLCEK");
  if (env) s_sharp = !strcmp(env, "1");
  env = getenv("RUMY_KAYDIR");
  s_emulateScroll = env && !strcmp(env, "1");
}

void geometry() {
  if (!s_fb) {
    s_dstW = W;
    s_dstH = H;
    s_offX = 0;
    s_offY = 0;
  } else if (s_sharp) {
    s_dstW = W;
    s_dstH = H;
    s_offX = (s_fbW - W) / 2;
    s_offY = (s_fbH - H) / 2;
  } else {
    int maxW = s_fbW < 640 ? s_fbW : 640;
    int maxH = s_fbH < 480 ? s_fbH : 480;
    if (maxW * H <= maxH * W) {
      s_dstW = maxW;
      s_dstH = maxW * H / W;
    } else {
      s_dstH = maxH;
      s_dstW = maxH * W / H;
    }
    s_offX = (s_fbW - s_dstW) / 2;
    s_offY = (s_fbH - s_dstH) / 2;
  }
  buildTables();
  buildScrollTables();
}

constexpr uint32_t SPREAD_MASK = 0x07E0F81Fu;

inline uint32_t spread565(uint32_t p) { return (p | (p << 16)) & SPREAD_MASK; }

inline uint32_t pack565(uint32_t s) {
  s &= SPREAD_MASK;
  return (s | (s >> 16)) & 0xFFFFu;
}

inline uint32_t mix565(uint32_t a, uint32_t b, uint32_t w) {
  const uint32_t w5 = (w + 4) >> 3;
  if (w5 == 0) return a;
  if (w5 >= 32) return b;
  return pack565((spread565(a) * (32 - w5) + spread565(b) * w5 + 0x02008010u) >> 5);
}

inline uint32_t dim565(uint32_t p, uint32_t bl) {
  const uint32_t b5 = (bl + 4) >> 3;
  return b5 >= 32 ? p : pack565((spread565(p) * b5) >> 5);
}

inline void putPixel(uint8_t* row, int x, uint32_t r, uint32_t g, uint32_t b) {
  if (s_fbBpp == 16) {
    reinterpret_cast<uint16_t*>(row)[x] = static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
  } else {
    uint32_t v = (r << s_var.red.offset) | (g << s_var.green.offset) | (b << s_var.blue.offset);
    reinterpret_cast<uint32_t*>(row)[x] = v;
  }
}

int64_t rectArea(const Rect& r) { return static_cast<int64_t>(r.x2 - r.x1 + 1) * (r.y2 - r.y1 + 1); }

Rect unite(const Rect& a, const Rect& b) {
  Rect u = a;
  if (b.x1 < u.x1) u.x1 = b.x1;
  if (b.y1 < u.y1) u.y1 = b.y1;
  if (b.x2 > u.x2) u.x2 = b.x2;
  if (b.y2 > u.y2) u.y2 = b.y2;
  return u;
}

void addRectLocked(Rect r) {
  for (;;) {
    int best = -1;
    int64_t bestCost = 0;
    for (int i = 0; i < s_rectN; i++) {
      int64_t cost = rectArea(unite(s_rects[i], r)) - rectArea(s_rects[i]) - rectArea(r);
      if (best < 0 || cost < bestCost) {
        best = i;
        bestCost = cost;
      }
    }
    if (best >= 0 && (bestCost * 4 <= rectArea(s_rects[best]) + rectArea(r) || s_rectN == MAX_RECTS)) {
      r = unite(s_rects[best], r);
      s_rects[best] = s_rects[--s_rectN];
      continue;
    }
    s_rects[s_rectN++] = r;
    return;
  }
}

void sendRect(const Rect& r) {
  drm_clip_rect clip;
  clip.x1 = static_cast<unsigned short>(r.x1);
  clip.y1 = static_cast<unsigned short>(r.y1);
  clip.x2 = static_cast<unsigned short>(r.x2 + 1);
  clip.y2 = static_cast<unsigned short>(r.y2 + 1);
  drm_mode_fb_dirty_cmd cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.fb_id = s_drmFb;
  cmd.num_clips = 1;
  cmd.clips_ptr = reinterpret_cast<uintptr_t>(&clip);
  if (ioctl(s_fbFd, DRM_IOCTL_MODE_DIRTYFB, &cmd) == 0) return;
  if (s_panelErrors++ == 0) {
    fprintf(stdout, "[ekran] panele aktarim basarisiz: %s\n", strerror(errno));
    fflush(stdout);
  }
}

void* flushMain(void*) {
  pthread_setname_np(pthread_self(), "panel");
  Rect work[MAX_RECTS];
  for (;;) {
    pthread_mutex_lock(&s_dmx);
    while (s_rectN == 0) pthread_cond_wait(&s_dcv, &s_dmx);
    int n = s_rectN;
    memcpy(work, s_rects, sizeof(Rect) * static_cast<size_t>(n));
    s_rectN = 0;
    pthread_cond_broadcast(&s_taken);
    pthread_mutex_unlock(&s_dmx);
    for (int i = 0; i < n; i++) sendRect(work[i]);
  }
  return nullptr;
}

void waitPanel() {
  if (s_backend != BK_DRM || !s_flushThread) return;
  timespec until;
  clock_gettime(CLOCK_MONOTONIC, &until);
  until.tv_nsec += PACE_WAIT_MS * 1000000L;
  if (until.tv_nsec >= 1000000000L) {
    until.tv_sec++;
    until.tv_nsec -= 1000000000L;
  }
  pthread_mutex_lock(&s_dmx);
  while (s_rectN > 0) {
    if (pthread_cond_timedwait(&s_taken, &s_dmx, &until) != 0) break;
  }
  pthread_mutex_unlock(&s_dmx);
}

void markDirty(int x1, int y1, int x2, int y2) {
  if (s_backend != BK_DRM) return;
  if (s_flip) {
    int t = s_fbW - 1 - x2;
    x2 = s_fbW - 1 - x1;
    x1 = t;
    t = s_fbH - 1 - y2;
    y2 = s_fbH - 1 - y1;
    y1 = t;
  }
  if (x1 < 0) x1 = 0;
  if (y1 < 0) y1 = 0;
  if (x2 > s_fbW - 1) x2 = s_fbW - 1;
  if (y2 > s_fbH - 1) y2 = s_fbH - 1;
  if (x1 > x2 || y1 > y2) return;
  if (!s_flushThread) {
    sendRect(Rect{x1, y1, x2, y2});
    return;
  }
  pthread_mutex_lock(&s_dmx);
  addRectLocked(Rect{x1, y1, x2, y2});
  pthread_cond_signal(&s_dcv);
  pthread_mutex_unlock(&s_dmx);
}

void clearFb() {
  if (!s_fb) return;
  memset(s_fb, 0, s_fbLen);
  markDirty(0, 0, s_fbW - 1, s_fbH - 1);
}

void present(int gx1, int gy1, int gx2, int gy2) {
  if (!s_fb) return;
  int sx1 = 0, sx2 = W - 1;
  if (!(gx1 == 0 && gx2 == W - 1)) {
    int a = ((gx1 - s_scroll) % W + W) % W;
    int b = ((gx2 - s_scroll) % W + W) % W;
    if (a <= b) {
      sx1 = a;
      sx2 = b;
    }
  }
  const bool unit = s_dstW == W && s_dstH == H && s_scroll == 0;
  int dx1 = unit ? sx1 : (sx1 - 1) * s_dstW / W - 1;
  int dx2 = unit ? sx2 : (sx2 + 2) * s_dstW / W + 1;
  int dy1 = unit ? gy1 : (gy1 - 1) * s_dstH / H - 1;
  int dy2 = unit ? gy2 : (gy2 + 2) * s_dstH / H + 1;
  if (dx1 < 0) dx1 = 0;
  if (dy1 < 0) dy1 = 0;
  if (dx2 > s_dstW - 1) dx2 = s_dstW - 1;
  if (dy2 > s_dstH - 1) dy2 = s_dstH - 1;
  const uint32_t bl = s_brightLevel;
  if (s_fbBpp == 16) {
    const bool dim = bl < 256;
    const int step = s_flip ? -1 : 1;
    if (unit) {
      const size_t n = static_cast<size_t>(dx2 - dx1 + 1);
      for (int y = dy1; y <= dy2; y++) {
        const uint16_t* src = s_gram + y * W + dx1;
        const int ty = s_flip ? s_fbH - 1 - (y + s_offY) : y + s_offY;
        const int tx = s_flip ? s_fbW - 1 - (dx1 + s_offX) : dx1 + s_offX;
        uint16_t* out = reinterpret_cast<uint16_t*>(s_fb + static_cast<size_t>(ty) * s_fbStride) + tx;
        if (!dim && !s_flip) {
          memcpy(out, src, n * 2U);
          continue;
        }
        for (size_t i = 0; i < n; i++, out += step) *out = static_cast<uint16_t>(dim ? dim565(src[i], bl) : src[i]);
      }
      markDirty(dx1 + s_offX, dy1 + s_offY, dx2 + s_offX, dy2 + s_offY);
      return;
    }
    for (int y = dy1; y <= dy2; y++) {
      const int y0 = s_rowY0[y];
      const int y1 = y0 + 1 < H ? y0 + 1 : y0;
      const uint32_t wy = s_rowW[y];
      const uint16_t* r0 = s_gram + y0 * W;
      const uint16_t* r1 = s_gram + y1 * W;
      const int ty = s_flip ? s_fbH - 1 - (y + s_offY) : y + s_offY;
      const int tx = s_flip ? s_fbW - 1 - (dx1 + s_offX) : dx1 + s_offX;
      uint16_t* out = reinterpret_cast<uint16_t*>(s_fb + static_cast<size_t>(ty) * s_fbStride) + tx;
      for (int x = dx1; x <= dx2; x++, out += step) {
        const uint32_t wx = s_colW[x];
        const int g0 = s_gx0[x];
        const int g1 = s_gx1[x];
        uint32_t p = mix565(r0[g0], r0[g1], wx);
        if (wy) p = mix565(p, mix565(r1[g0], r1[g1], wx), wy);
        *out = static_cast<uint16_t>(dim ? dim565(p, bl) : p);
      }
    }
    markDirty(dx1 + s_offX, dy1 + s_offY, dx2 + s_offX, dy2 + s_offY);
    return;
  }
  for (int y = dy1; y <= dy2; y++) {
    const int y0 = s_rowY0[y];
    const int y1 = y0 + 1 < H ? y0 + 1 : y0;
    const uint32_t wy = s_rowW[y];
    const uint16_t* r0 = s_gram + y0 * W;
    const uint16_t* r1 = s_gram + y1 * W;
    int ty = s_flip ? s_fbH - 1 - (y + s_offY) : y + s_offY;
    uint8_t* out = s_fb + static_cast<size_t>(ty) * s_fbStride;
    for (int x = dx1; x <= dx2; x++) {
      const uint32_t wx = s_colW[x];
      const uint16_t a = r0[s_gx0[x]], b = r0[s_gx1[x]], c = r1[s_gx0[x]], d = r1[s_gx1[x]];
      uint32_t ra = a >> 11, ga = (a >> 5) & 63, ba = a & 31;
      uint32_t rb = b >> 11, gb = (b >> 5) & 63, bb = b & 31;
      uint32_t rc = c >> 11, gc = (c >> 5) & 63, bc = c & 31;
      uint32_t rd = d >> 11, gd = (d >> 5) & 63, bd = d & 31;
      uint32_t rt = ra * (256 - wx) + rb * wx, gt = ga * (256 - wx) + gb * wx, bt = ba * (256 - wx) + bb * wx;
      uint32_t rbm = rc * (256 - wx) + rd * wx, gbm = gc * (256 - wx) + gd * wx, bbm = bc * (256 - wx) + bd * wx;
      uint32_t r = (rt * (256 - wy) + rbm * wy) >> 16;
      uint32_t g = (gt * (256 - wy) + gbm * wy) >> 16;
      uint32_t bl5 = (bt * (256 - wy) + bbm * wy) >> 16;
      r = (r << 3) | (r >> 2);
      g = (g << 2) | (g >> 4);
      bl5 = (bl5 << 3) | (bl5 >> 2);
      if (bl < 256) {
        r = (r * bl) >> 8;
        g = (g * bl) >> 8;
        bl5 = (bl5 * bl) >> 8;
      }
      putPixel(out, s_flip ? s_fbW - 1 - (x + s_offX) : x + s_offX, r, g, bl5);
    }
  }
  markDirty(dx1 + s_offX, dy1 + s_offY, dx2 + s_offX, dy2 + s_offY);
}

void presentAll() { present(0, 0, W - 1, H - 1); }

bool sliding() { return s_scroll != 0; }

bool tryFb(const char* dev, const char* name) {
  int fd = open(dev, O_RDWR | O_CLOEXEC);
  if (fd < 0) return false;
  fb_var_screeninfo var;
  fb_fix_screeninfo fix;
  if (ioctl(fd, FBIOGET_VSCREENINFO, &var) != 0 || ioctl(fd, FBIOGET_FSCREENINFO, &fix) != 0 ||
      (var.bits_per_pixel != 16 && var.bits_per_pixel != 32) || var.xres < static_cast<uint32_t>(W) ||
      var.yres < static_cast<uint32_t>(H)) {
    close(fd);
    return false;
  }
  size_t len = static_cast<size_t>(fix.line_length) * var.yres_virtual;
  if (fix.smem_len && len > fix.smem_len) len = fix.smem_len;
  void* m = mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (m == MAP_FAILED) {
    close(fd);
    return false;
  }
  fb_var_screeninfo on = var;
  on.activate = FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;
  if (ioctl(fd, FBIOPUT_VSCREENINFO, &on) != 0) fprintf(stdout, "[ekran] etkinlestirme basarisiz: %s\n", strerror(errno));
  s_backend = BK_FBDEV;
  s_fbFd = fd;
  s_fb = static_cast<uint8_t*>(m);
  s_fbLen = len;
  s_var = var;
  s_fbW = static_cast<int>(var.xres);
  s_fbH = static_cast<int>(var.yres);
  s_fbBpp = static_cast<int>(var.bits_per_pixel);
  s_fbStride = static_cast<int>(fix.line_length);
  snprintf(s_fbName, sizeof(s_fbName), "%s fbdev %dx%d %dbpp", name, s_fbW, s_fbH, s_fbBpp);
  return true;
}

bool sysLinkBase(const char* path, char* out, size_t cap) {
  char link[256];
  ssize_t n = readlink(path, link, sizeof(link) - 1);
  if (n <= 0) return false;
  link[n] = 0;
  const char* base = strrchr(link, '/');
  const char* src = base ? base + 1 : link;
  size_t len = strlen(src);
  if (len >= cap) len = cap - 1;
  memcpy(out, src, len);
  out[len] = 0;
  return true;
}

void dropDrm(int fd, void* m, size_t len, const char* why) {
  fprintf(stdout, "[ekran] DRM kullanilamadi (%s): %s\n", why, strerror(errno));
  if (m && m != MAP_FAILED) munmap(m, len);
  close(fd);
}

bool tryDrm(const char* dev, const char* name) {
  int fd = open(dev, O_RDWR | O_CLOEXEC);
  if (fd < 0) return false;
  uint32_t crtcs[8] = {}, conns[8] = {}, encs[8] = {}, fbs[8] = {};
  drm_mode_card_res res;
  memset(&res, 0, sizeof(res));
  if (ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res) != 0 || res.count_crtcs < 1 || res.count_connectors < 1 ||
      res.count_crtcs > 8 || res.count_connectors > 8 || res.count_encoders > 8 || res.count_fbs > 8) {
    dropDrm(fd, nullptr, 0, "kaynaklar");
    return false;
  }
  res.crtc_id_ptr = reinterpret_cast<uintptr_t>(crtcs);
  res.connector_id_ptr = reinterpret_cast<uintptr_t>(conns);
  res.encoder_id_ptr = reinterpret_cast<uintptr_t>(encs);
  res.fb_id_ptr = reinterpret_cast<uintptr_t>(fbs);
  if (ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res) != 0) {
    dropDrm(fd, nullptr, 0, "kaynaklar");
    return false;
  }
  drm_mode_get_connector conn;
  memset(&conn, 0, sizeof(conn));
  conn.connector_id = conns[0];
  if (ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn) != 0 || conn.count_modes < 1 || conn.count_modes > 16) {
    dropDrm(fd, nullptr, 0, "baglayici");
    return false;
  }
  drm_mode_modeinfo modes[16];
  conn.modes_ptr = reinterpret_cast<uintptr_t>(modes);
  conn.count_props = 0;
  conn.props_ptr = 0;
  conn.prop_values_ptr = 0;
  conn.count_encoders = 0;
  conn.encoders_ptr = 0;
  if (ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn) != 0 || modes[0].hdisplay < W || modes[0].vdisplay < H ||
      modes[0].hdisplay > 640 || modes[0].vdisplay > 480) {
    dropDrm(fd, nullptr, 0, "kip");
    return false;
  }
  drm_mode_modeinfo mode = modes[0];
  drm_mode_create_dumb cd;
  memset(&cd, 0, sizeof(cd));
  cd.width = mode.hdisplay;
  cd.height = mode.vdisplay;
  cd.bpp = 16;
  if (ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &cd) != 0) {
    dropDrm(fd, nullptr, 0, "tampon");
    return false;
  }
  drm_mode_fb_cmd fbc;
  memset(&fbc, 0, sizeof(fbc));
  fbc.width = cd.width;
  fbc.height = cd.height;
  fbc.pitch = cd.pitch;
  fbc.bpp = 16;
  fbc.depth = 16;
  fbc.handle = cd.handle;
  drm_mode_map_dumb md;
  memset(&md, 0, sizeof(md));
  md.handle = cd.handle;
  if (ioctl(fd, DRM_IOCTL_MODE_ADDFB, &fbc) != 0 || ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &md) != 0) {
    dropDrm(fd, nullptr, 0, "cerceve");
    return false;
  }
  size_t len = static_cast<size_t>(cd.size);
  void* m = mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, static_cast<off_t>(md.offset));
  if (m == MAP_FAILED) {
    dropDrm(fd, m, len, "eslem");
    return false;
  }
  memset(m, 0, len);
  drm_mode_crtc crtc;
  memset(&crtc, 0, sizeof(crtc));
  crtc.set_connectors_ptr = reinterpret_cast<uintptr_t>(&conns[0]);
  crtc.count_connectors = 1;
  crtc.crtc_id = crtcs[0];
  crtc.fb_id = fbc.fb_id;
  crtc.mode_valid = 1;
  crtc.mode = mode;
  if (ioctl(fd, DRM_IOCTL_MODE_SETCRTC, &crtc) != 0) {
    dropDrm(fd, m, len, "kip ayari");
    return false;
  }
  s_backend = BK_DRM;
  s_fbFd = fd;
  s_fb = static_cast<uint8_t*>(m);
  s_fbLen = len;
  memset(&s_var, 0, sizeof(s_var));
  s_var.bits_per_pixel = 16;
  s_fbW = mode.hdisplay;
  s_fbH = mode.vdisplay;
  s_fbBpp = 16;
  s_fbStride = static_cast<int>(cd.pitch);
  s_drmFb = fbc.fb_id;
  if (!s_flushThread) {
    pthread_condattr_t ca;
    pthread_condattr_init(&ca);
    pthread_condattr_setclock(&ca, CLOCK_MONOTONIC);
    pthread_cond_init(&s_taken, &ca);
    pthread_condattr_destroy(&ca);
    pthread_t t;
    if (pthread_create(&t, nullptr, flushMain, nullptr) == 0) {
      pthread_detach(t);
      s_flushThread = true;
    }
  }
  snprintf(s_fbName, sizeof(s_fbName), "%s drm %dx%d 16bpp", name, s_fbW, s_fbH);
  return true;
}

bool findDrm() {
  const char* forced = getenv("RUMY_DRM");
  if (forced && forced[0]) return tryDrm(forced, forced);
  for (int i = 0; i < 8; i++) {
    char path[64], sub[32], drv[64];
    snprintf(path, sizeof(path), "/sys/class/drm/card%d/device/subsystem", i);
    if (!sysLinkBase(path, sub, sizeof(sub)) || strcmp(sub, "spi") != 0) continue;
    snprintf(path, sizeof(path), "/sys/class/drm/card%d/device/driver", i);
    if (!sysLinkBase(path, drv, sizeof(drv))) snprintf(drv, sizeof(drv), "spi");
    char dev[32];
    snprintf(dev, sizeof(dev), "/dev/dri/card%d", i);
    if (tryDrm(dev, drv)) return true;
  }
  return false;
}

void findFb() {
  uint32_t now = millis();
  if (s_fb || (s_fbTried && now - s_fbTryAt < FB_RETRY_MS)) return;
  s_fbTried = true;
  s_fbTryAt = now;
  const char* forced = getenv("RUMY_FB");
  const char* fake = getenv("RUMY_SAHTE_EKRAN");
  const char* fbdevOnly = getenv("RUMY_FBDEV");
  bool found = false;
  int fw = 0, fh = 0;
  if (fake && sscanf(fake, "%dx%d", &fw, &fh) == 2 && fw >= W && fh >= H && fw <= 640 && fh <= 480) {
    s_fbW = fw;
    s_fbH = fh;
    s_fbBpp = 16;
    s_fbStride = fw * 2;
    s_fbLen = static_cast<size_t>(s_fbStride) * fh;
    s_fb = static_cast<uint8_t*>(calloc(1, s_fbLen));
    memset(&s_var, 0, sizeof(s_var));
    snprintf(s_fbName, sizeof(s_fbName), "sahte %dx%d", fw, fh);
    found = s_fb != nullptr;
    if (found) s_backend = BK_FAKE;
  } else if (forced && forced[0]) {
    found = tryFb(forced, forced);
  } else if (!(fbdevOnly && !strcmp(fbdevOnly, "1")) && findDrm()) {
    found = true;
  } else {
    for (int i = 0; i < 8 && !found; i++) {
      char path[64], name[64] = "";
      snprintf(path, sizeof(path), "/sys/class/graphics/fb%d/name", i);
      if (!readSmall(path, name, sizeof(name))) continue;
      bool spi = strstr(name, "ili9486") || strstr(name, "ILI9486") || strstr(name, "fb_ili") || strstr(name, "piscreen");
      if (!spi) continue;
      char dev[32];
      snprintf(dev, sizeof(dev), "/dev/fb%d", i);
      found = tryFb(dev, name);
    }
  }
  if (!found) return;
  geometry();
  clearFb();
  presentAll();
  fprintf(stdout, "[ekran] %s, cizim %dx%d (+%d,+%d)\n", s_fbName, s_dstW, s_dstH, s_offX, s_offY);
  fflush(stdout);
}

void loadTouchCal() {
  char path[512];
  snprintf(path, sizeof(path), "%s/dokunmatik.json", pi::configDir());
  uint8_t buf[512];
  size_t len = 0;
  if (!pi::readAll(path, buf, sizeof(buf) - 1, len)) return;
  buf[len] = 0;
  const char* text = reinterpret_cast<const char*>(buf);
  const char* keys[4] = {"\"xmin\"", "\"xmax\"", "\"ymin\"", "\"ymax\""};
  int32_t* dst[4] = {&s_cal.a0, &s_cal.a1, &s_cal.b0, &s_cal.b1};
  int found = 0;
  for (int i = 0; i < 4; i++) {
    const char* k = strstr(text, keys[i]);
    const char* c = k ? strchr(k, ':') : nullptr;
    if (!c) continue;
    *dst[i] = static_cast<int32_t>(strtol(c + 1, nullptr, 10));
    found++;
  }
  s_cal.set = found == 4 && s_cal.a0 != s_cal.a1 && s_cal.b0 != s_cal.b1;
  const char* t = strstr(text, "\"takas\"");
  const char* tc = t ? strchr(t, ':') : nullptr;
  s_cal.swap = tc && strtol(tc + 1, nullptr, 10) != 0;
}

bool saveTouchCal() {
  char path[512];
  snprintf(path, sizeof(path), "%s/dokunmatik.json", pi::configDir());
  char buf[160];
  int n = snprintf(buf, sizeof(buf), "{\"xmin\": %ld, \"xmax\": %ld, \"ymin\": %ld, \"ymax\": %ld, \"takas\": %d}\n",
                   static_cast<long>(s_cal.a0), static_cast<long>(s_cal.a1), static_cast<long>(s_cal.b0),
                   static_cast<long>(s_cal.b1), s_cal.swap ? 1 : 0);
  return n > 0 && pi::writeAtomic(path, buf, static_cast<size_t>(n), false);
}

void findTouch() {
  uint32_t now = millis();
  if (s_touchFd >= 0 || (s_touchTried && now - s_touchTryAt < TOUCH_RETRY_MS)) return;
  s_touchTried = true;
  s_touchTryAt = now;
  DIR* d = opendir("/dev/input");
  if (!d) return;
  dirent* e;
  while ((e = readdir(d)) != nullptr) {
    if (strncmp(e->d_name, "event", 5) != 0) continue;
    char dev[300];
    snprintf(dev, sizeof(dev), "/dev/input/%s", e->d_name);
    int fd = open(dev, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) continue;
    char name[128] = "";
    ioctl(fd, EVIOCGNAME(sizeof(name)), name);
    if (!strstr(name, "ADS7846") && !strstr(name, "ads7846") && !strstr(name, "Touch") && !strstr(name, "touch")) {
      close(fd);
      continue;
    }
    input_absinfo ax = {}, ay = {};
    if (ioctl(fd, EVIOCGABS(ABS_X), &ax) == 0 && ax.maximum > ax.minimum) {
      s_minX = ax.minimum;
      s_maxX = ax.maximum;
    }
    if (ioctl(fd, EVIOCGABS(ABS_Y), &ay) == 0 && ay.maximum > ay.minimum) {
      s_minY = ay.minimum;
      s_maxY = ay.maximum;
    }
    s_touchFd = fd;
    loadTouchCal();
    fprintf(stdout, "[dokunmatik] %s (%s) x %d..%d y %d..%d\n", dev, name, s_minX, s_maxX, s_minY, s_maxY);
    fflush(stdout);
    break;
  }
  closedir(d);
}

void pollTouch() {
  if (s_touchFd < 0) return;
  input_event ev[32];
  for (;;) {
    ssize_t r = read(s_touchFd, ev, sizeof(ev));
    if (r <= 0) {
      if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
        close(s_touchFd);
        s_touchFd = -1;
      }
      return;
    }
    size_t n = static_cast<size_t>(r) / sizeof(input_event);
    for (size_t i = 0; i < n; i++) {
      if (ev[i].type == EV_ABS && ev[i].code == ABS_X) s_rawX = ev[i].value;
      else if (ev[i].type == EV_ABS && ev[i].code == ABS_Y) s_rawY = ev[i].value;
      else if (ev[i].type == EV_KEY && ev[i].code == BTN_TOUCH) s_down = ev[i].value != 0;
    }
  }
}

int32_t mapRange(int32_t v, int32_t lo, int32_t hi, int32_t out) {
  if (hi == lo) return 0;
  int32_t r = (v - lo) * out / (hi - lo);
  if (r < 0) r = 0;
  if (r > out - 1) r = out - 1;
  return r;
}

void defaultRange(bool rawY, int32_t& lo, int32_t& hi) {
  if (rawY) {
    int32_t r = s_maxY - s_minY;
    lo = s_minY + r * 2 / 40;
    hi = s_maxY - r * 4 / 40;
  } else {
    int32_t r = s_maxX - s_minX;
    lo = s_minX + r * 3 / 40;
    hi = s_maxX - r * 2 / 40;
  }
}

bool swapAxes() { return ((s_touchRot & 4) != 0) != s_cal.swap; }

int32_t panelW() { return s_fb ? s_fbW : W; }

int32_t panelH() { return s_fb ? s_fbH : H; }

void rawToScreen(int32_t rx, int32_t ry, int32_t& x, int32_t& y) {
  bool swap = swapAxes();
  int32_t ra = swap ? ry : rx;
  int32_t rb = swap ? rx : ry;
  int32_t a0, a1, b0, b1;
  if (s_cal.set) {
    a0 = s_cal.a0;
    a1 = s_cal.a1;
    b0 = s_cal.b0;
    b1 = s_cal.b1;
  } else {
    defaultRange(swap, a0, a1);
    defaultRange(!swap, b0, b1);
  }
  int32_t pw = panelW();
  int32_t ph = panelH();
  int32_t px = mapRange(ra, a0, a1, pw);
  int32_t py = mapRange(rb, b0, b1, ph);
  if (s_touchRot & 1) px = pw - 1 - px;
  if (s_touchRot & 2) py = ph - 1 - py;
  if (s_flip) {
    px = pw - 1 - px;
    py = ph - 1 - py;
  }
  x = (px - s_offX) * W / (s_dstW ? s_dstW : W);
  y = (py - s_offY) * H / (s_dstH ? s_dstH : H);
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x >= W) x = W - 1;
  if (y >= H) y = H - 1;
}

bool touchPoint(int32_t& x, int32_t& y) {
  rawToScreen(s_rawX, s_rawY, x, y);
  return true;
}

void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* px) {
  uint32_t t0 = micros();
  int32_t w = lv_area_get_width(area);
  int32_t h = lv_area_get_height(area);
  const uint16_t* src = reinterpret_cast<const uint16_t*>(px);
  pthread_mutex_lock(&s_mx);
  for (int32_t y = 0; y < h; y++) {
    int32_t gy = area->y1 + y;
    if (gy < 0 || gy >= H) continue;
    memcpy(s_gram + gy * W + area->x1, src + y * w, static_cast<size_t>(w) * 2U);
  }
  if (!sliding()) present(area->x1, area->y1, area->x2, area->y2);
  pthread_mutex_unlock(&s_mx);
  s_perf.flushUs += micros() - t0;
  s_perf.flushes++;
  s_perf.pixels += static_cast<uint32_t>(w * h);
  if (lv_display_flush_is_last(disp)) waitPanel();
  lv_display_flush_ready(disp);
}

void touchCb(lv_indev_t*, lv_indev_data_t* data) {
  uint32_t t0 = micros();
  s_touchReads++;
  findTouch();
  pollTouch();
  if (s_down) {
    int32_t x = 0, y = 0;
    touchPoint(x, y);
    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
    s_lastTouch = millis();
    if (s_logTouch && !s_touching)
      Serial.printf("dokunma %ld,%ld ham %d,%d\n", static_cast<long>(x), static_cast<long>(y), s_rawX, s_rawY);
    s_touching = true;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
    s_touching = false;
  }
  s_perf.touchUs += micros() - t0;
}

void fbTimer(lv_timer_t*) {
  if (s_fb) return;
  pthread_mutex_lock(&s_mx);
  findFb();
  pthread_mutex_unlock(&s_mx);
  if (s_fb && s_disp) lv_obj_invalidate(lv_screen_active());
}

struct JpegSrc {
  JpegReader read;
  void* ctx;
  int32_t x;
  int32_t y;
};

uint32_t jpegIn(void* dev, uint8_t* buf, uint32_t len) {
  JpegSrc* s = static_cast<JpegSrc*>(dev);
  return s->read(s->ctx, buf, len);
}

uint32_t jpegOut(void* dev, void* bitmap, JRECT* r) {
  JpegSrc* s = static_cast<JpegSrc*>(dev);
  const uint8_t* p = static_cast<const uint8_t*>(bitmap);
  int32_t x1 = s->x + static_cast<int32_t>(r->left), y1 = s->y + static_cast<int32_t>(r->top);
  int32_t x2 = s->x + static_cast<int32_t>(r->right), y2 = s->y + static_cast<int32_t>(r->bottom);
  pthread_mutex_lock(&s_mx);
  for (int32_t y = y1; y <= y2; y++) {
    for (int32_t x = x1; x <= x2; x++, p += 3) {
      if (x < 0 || y < 0 || x >= W || y >= H) continue;
      s_gram[y * W + x] = static_cast<uint16_t>(((p[0] & 0xF8) << 8) | ((p[1] & 0xFC) << 3) | (p[2] >> 3));
    }
  }
  if (!sliding()) present(x1 < 0 ? 0 : x1, y1 < 0 ? 0 : y1, x2 >= W ? W - 1 : x2, y2 >= H ? H - 1 : y2);
  pthread_mutex_unlock(&s_mx);
  return 1;
}

uint16_t levelFor(uint8_t pct) {
  if (pct >= 100) return 256;
  if (pct == 0) return 0;
  float v = 256.0f * powf(static_cast<float>(pct) / 100.0f, 1.0f / 2.2f);
  return static_cast<uint16_t>(v + 0.5f);
}

}

void ledsOff() {}

bool begin(uint8_t touchRot) {
  s_touchRot = touchRot;
  loadScreenCfg();
  pthread_mutex_lock(&s_mx);
  memset(s_gram, 0, sizeof(s_gram));
  findFb();
  if (!s_fb) geometry();
  pthread_mutex_unlock(&s_mx);
  if (!s_fb) {
    fprintf(stdout, "[ekran] SPI ekran bulunamadi, ekransiz calisiyor\n");
    fflush(stdout);
  }
  return true;
}

void setScroll(uint16_t line) {
  pthread_mutex_lock(&s_mx);
  s_scroll = static_cast<uint16_t>(line % W);
  buildScrollTables();
  presentAll();
  pthread_mutex_unlock(&s_mx);
}

void ignoreUntilRelease() {
  if (s_indev) lv_indev_wait_release(s_indev);
}

bool hardwareScroll() { return s_emulateScroll; }

void setWriteHz(uint32_t) {}

uint32_t spiTest(uint32_t) { return 0; }

FlushPerf takeFlushPerf() {
  FlushPerf p = s_perf;
  s_perf = FlushPerf{};
  return p;
}

void startLvgl(uint8_t* buf1, uint8_t* buf2, uint32_t bytes) {
  lv_tick_set_cb(tickCb);
  s_disp = lv_display_create(W, H);
  lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(s_disp, flushCb);
  lv_display_set_buffers(s_disp, buf1, buf2, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  s_indev = lv_indev_create();
  lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(s_indev, touchCb);
  lv_indev_set_display(s_indev, s_disp);
  lv_timer_create(fbTimer, 2000, nullptr);
}

void setBrightnessPct(uint8_t pct) {
  s_brightness = pct;
  uint16_t lvl = levelFor(pct);
  if (lvl == s_brightLevel) return;
  pthread_mutex_lock(&s_mx);
  s_brightLevel = lvl;
  presentAll();
  pthread_mutex_unlock(&s_mx);
}

uint8_t brightnessPct() { return s_brightness; }

void setTouchRotation(uint8_t rot) { s_touchRot = rot; }

uint32_t lastTouchMs() { return s_lastTouch; }

bool touching() { return s_touching; }

void setTouchLog(bool on) { s_logTouch = on; }

uint32_t touchReads() { return s_touchReads; }

bool touchCalSupported() { return s_touchFd >= 0; }

bool rawTouch(int32_t& x, int32_t& y) {
  pollTouch();
  x = s_rawX;
  y = s_rawY;
  return s_down && s_touchFd >= 0;
}

bool calibrateTouch(const int32_t* tx, const int32_t* ty, const int32_t* rx, const int32_t* ry) {
  const int32_t pw = panelW();
  const int32_t ph = panelH();
  double qx[4], qy[4];
  for (int i = 0; i < 4; i++) {
    double px = s_offX + (tx[i] + 0.5) * s_dstW / W;
    double py = s_offY + (ty[i] + 0.5) * s_dstH / H;
    if (s_flip) {
      px = pw - 1 - px;
      py = ph - 1 - py;
    }
    if (s_touchRot & 1) px = pw - 1 - px;
    if (s_touchRot & 2) py = ph - 1 - py;
    qx[i] = px;
    qy[i] = py;
  }
  double spreadX = fabs(static_cast<double>(rx[1] + rx[2] - rx[0] - rx[3]));
  double spreadY = fabs(static_cast<double>(ry[1] + ry[2] - ry[0] - ry[3]));
  bool swap = spreadY > spreadX;
  double ra[4], rb[4];
  for (int i = 0; i < 4; i++) {
    ra[i] = swap ? ry[i] : rx[i];
    rb[i] = swap ? rx[i] : ry[i];
  }
  double qL = (qx[0] + qx[3]) / 2, qR = (qx[1] + qx[2]) / 2;
  double rL = (ra[0] + ra[3]) / 2, rR = (ra[1] + ra[2]) / 2;
  double qT = (qy[0] + qy[1]) / 2, qB = (qy[2] + qy[3]) / 2;
  double rT = (rb[0] + rb[1]) / 2, rB = (rb[2] + rb[3]) / 2;
  if (fabs(qR - qL) < 1.0 || fabs(qB - qT) < 1.0) return false;
  double kx = (rR - rL) / (qR - qL);
  double ky = (rB - rT) / (qB - qT);
  if (fabs(kx) < 1.5 || fabs(ky) < 1.5) return false;
  TouchCal c;
  c.set = true;
  c.swap = swap != ((s_touchRot & 4) != 0);
  double a0 = rL - kx * qL;
  double b0 = rT - ky * qT;
  c.a0 = static_cast<int32_t>(lround(a0));
  c.a1 = static_cast<int32_t>(lround(a0 + kx * pw));
  c.b0 = static_cast<int32_t>(lround(b0));
  c.b1 = static_cast<int32_t>(lround(b0 + ky * ph));
  if (c.a0 == c.a1 || c.b0 == c.b1) return false;
  if (!s_calTrial) {
    s_calPrev = s_cal;
    s_calTrial = true;
  }
  s_cal = c;
  return true;
}

bool mapRawTouch(int32_t rx, int32_t ry, int32_t& x, int32_t& y) {
  rawToScreen(rx, ry, x, y);
  return true;
}

void keepTouchCal(bool save) {
  if (!s_calTrial) return;
  s_calTrial = false;
  if (!save) {
    s_cal = s_calPrev;
    return;
  }
  bool ok = saveTouchCal();
  fprintf(stdout, "[dokunmatik] ayar %s: x %ld..%ld y %ld..%ld takas %d\n", ok ? "kaydedildi" : "KAYDEDILEMEDI",
          static_cast<long>(s_cal.a0), static_cast<long>(s_cal.a1), static_cast<long>(s_cal.b0),
          static_cast<long>(s_cal.b1), s_cal.swap ? 1 : 0);
  fflush(stdout);
}

void dumpScreen() {
  static uint16_t shot[W * H];
  pthread_mutex_lock(&s_mx);
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) shot[y * W + x] = s_gram[y * W + (x + s_scroll) % W];
  }
  pthread_mutex_unlock(&s_mx);
  char path[512];
  snprintf(path, sizeof(path), "%s/ekran.ppm", pi::dataDir());
  static uint8_t ppm[32 + W * H * 3];
  int n = snprintf(reinterpret_cast<char*>(ppm), 32, "P6\n%d %d\n255\n", W, H);
  uint8_t* o = ppm + n;
  for (int i = 0; i < W * H; i++) {
    uint16_t v = shot[i];
    uint8_t r = static_cast<uint8_t>((v >> 11) & 31), g = static_cast<uint8_t>((v >> 5) & 63), b = static_cast<uint8_t>(v & 31);
    *o++ = static_cast<uint8_t>((r << 3) | (r >> 2));
    *o++ = static_cast<uint8_t>((g << 2) | (g >> 4));
    *o++ = static_cast<uint8_t>((b << 3) | (b >> 2));
  }
  pi::writeAtomic(path, ppm, static_cast<size_t>(o - ppm), false);
  if (s_fb) {
    size_t cap = 32 + static_cast<size_t>(s_fbW) * s_fbH * 3;
    uint8_t* big = static_cast<uint8_t*>(malloc(cap));
    if (big) {
      int m = snprintf(reinterpret_cast<char*>(big), 32, "P6\n%d %d\n255\n", s_fbW, s_fbH);
      uint8_t* q = big + m;
      pthread_mutex_lock(&s_mx);
      for (int y = 0; y < s_fbH; y++) {
        const uint8_t* row = s_fb + static_cast<size_t>(y) * s_fbStride;
        for (int x = 0; x < s_fbW; x++) {
          uint32_t r, g, b;
          if (s_fbBpp == 16) {
            uint16_t v = reinterpret_cast<const uint16_t*>(row)[x];
            r = (v >> 11) & 31;
            g = (v >> 5) & 63;
            b = v & 31;
            r = (r << 3) | (r >> 2);
            g = (g << 2) | (g >> 4);
            b = (b << 3) | (b >> 2);
          } else {
            uint32_t v = reinterpret_cast<const uint32_t*>(row)[x];
            r = (v >> s_var.red.offset) & 0xFF;
            g = (v >> s_var.green.offset) & 0xFF;
            b = (v >> s_var.blue.offset) & 0xFF;
          }
          *q++ = static_cast<uint8_t>(r);
          *q++ = static_cast<uint8_t>(g);
          *q++ = static_cast<uint8_t>(b);
        }
      }
      pthread_mutex_unlock(&s_mx);
      char pp[512];
      snprintf(pp, sizeof(pp), "%s/ekran_panel.ppm", pi::dataDir());
      pi::writeAtomic(pp, big, static_cast<size_t>(q - big), false);
      free(big);
    }
  }
  constexpr int32_t PIECE = 64;
  uint16_t line[PIECE];
  unsigned char b64[((PIECE * 2 + 2) / 3) * 4 + 4];
  Serial.printf("@shot %d %d\n", W, H);
  for (int32_t y = 0; y < H; y++) {
    for (int32_t x = 0; x < W; x += PIECE) {
      for (int32_t k = 0; k < PIECE; k++) {
        uint16_t v = shot[y * W + x + k];
        line[k] = static_cast<uint16_t>((v >> 8) | (v << 8));
      }
      size_t len = 0;
      mbedtls_base64_encode(b64, sizeof(b64), &len, reinterpret_cast<const unsigned char*>(line), sizeof(line));
      b64[len] = 0;
      Serial.printf("@px %ld %ld %s\n", static_cast<long>(y), static_cast<long>(x), b64);
    }
  }
  Serial.println(F("@shotend"));
  fprintf(stdout, "[ekran] goruntu %s\n", path);
  fflush(stdout);
}

bool drawJpeg(JpegReader read, void* ctx, int32_t x, int32_t y, uint8_t* pool, size_t poolBytes) {
  if (!read || !pool) return false;
  lgfxJdec jd;
  JpegSrc src = {read, ctx, x, y};
  if (lgfx_jd_prepare(&jd, jpegIn, pool, static_cast<uint_fast16_t>(poolBytes), &src) != JDR_OK) return false;
  return lgfx_jd_decomp(&jd, jpegOut, 0) == JDR_OK;
}

}
