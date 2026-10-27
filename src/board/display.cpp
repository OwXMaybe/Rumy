#include "display.h"

#include <Arduino.h>
#include <lgfx/utility/lgfx_tjpgd.h>
#include <lvgl.h>
#include <mbedtls/base64.h>
#include <string.h>

#include "app/config.h"
#include "lgfx_cyd.h"
#include "media/bridge.h"

namespace display {

namespace {

LgfxCyd s_gfx;
lv_display_t* s_disp = nullptr;
lv_indev_t* s_indev = nullptr;
uint8_t s_brightness = 80;
volatile uint32_t s_lastTouch = 0;
volatile bool s_touching = false;
bool s_logTouch = false;
uint32_t s_touchReads = 0;
FlushPerf s_perf = {};

uint32_t tickCb() { return millis(); }

void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* px) {
  int32_t w = lv_area_get_width(area);
  int32_t h = lv_area_get_height(area);
  uint32_t t0 = micros();
  s_gfx.pushImageDMA(area->x1, area->y1, w, h, reinterpret_cast<const lgfx::swap565_t*>(px));
  s_perf.flushUs += micros() - t0;
  s_perf.flushes++;
  s_perf.pixels += static_cast<uint32_t>(w * h);
  lv_display_flush_ready(disp);
}

void touchCb(lv_indev_t* indev, lv_indev_data_t* data) {
  (void)indev;
  int32_t x = 0, y = 0;
  s_touchReads++;
  uint32_t t0 = micros();
  bool pressed = s_gfx.getTouch(&x, &y);
  s_perf.touchUs += micros() - t0;
  if (pressed) {
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= cfg::SCREEN_W) x = cfg::SCREEN_W - 1;
    if (y >= cfg::SCREEN_H) y = cfg::SCREEN_H - 1;
    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
    s_lastTouch = millis();
    if (s_logTouch && !s_touching) bridge::printf("dokunma %ld,%ld\n", static_cast<long>(x), static_cast<long>(y));
    s_touching = true;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
    s_touching = false;
  }
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
  int32_t w = static_cast<int32_t>(r->right - r->left + 1);
  int32_t h = static_cast<int32_t>(r->bottom - r->top + 1);
  s_gfx.pushImage(s->x + static_cast<int32_t>(r->left), s->y + static_cast<int32_t>(r->top), w, h,
                  static_cast<const lgfx::bgr888_t*>(bitmap));
  return 1;
}

uint8_t pctToRaw(uint8_t pct) {
  if (pct > 100) pct = 100;
  uint32_t v = (static_cast<uint32_t>(pct) * pct * 255U) / 10000U;
  if (pct > 0 && v < 2) v = 2;
  return static_cast<uint8_t>(v);
}

}

void ledsOff() {
  pinMode(cfg::PIN_LED_R, OUTPUT);
  pinMode(cfg::PIN_LED_G, OUTPUT);
  pinMode(cfg::PIN_LED_B, OUTPUT);
  digitalWrite(cfg::PIN_LED_R, HIGH);
  digitalWrite(cfg::PIN_LED_G, HIGH);
  digitalWrite(cfg::PIN_LED_B, HIGH);
}

bool begin(uint8_t touchRot) {
  if (!s_gfx.init()) return false;
  s_gfx.setRotation(1);
  s_gfx.setTouchRotation(touchRot);
  s_gfx.setBrightness(0);
  s_gfx.fillScreen(0x0000);
  s_gfx.startWrite();
  s_gfx.writeCommand(0x33);
  s_gfx.writeData16(0);
  s_gfx.writeData16(cfg::SCREEN_W);
  s_gfx.writeData16(0);
  s_gfx.writeCommand(0x37);
  s_gfx.writeData16(0);
  s_gfx.endWrite();
  return true;
}

void setFlip(bool on) {
  s_gfx.waitDMA();
  s_gfx.setRotation(on ? 3 : 1);
  s_gfx.startWrite();
  s_gfx.writeCommand(0x37);
  s_gfx.writeData16(0);
  s_gfx.endWrite();
}

void setScroll(uint16_t line) {
  s_gfx.waitDMA();
  s_gfx.writeCommand(0x37);
  s_gfx.writeData16(line);
}

bool hardwareScroll() { return true; }

void ignoreUntilRelease() {
  if (s_indev) lv_indev_wait_release(s_indev);
}

void setWriteHz(uint32_t hz) {
  s_gfx.waitDMA();
  s_gfx.endWrite();
  s_gfx.setWriteHz(hz);
  s_gfx.startWrite();
}

uint32_t spiTest(uint32_t hz) {
  setWriteHz(hz);
  uint16_t line[cfg::SCREEN_W];
  uint16_t back[cfg::SCREEN_W];
  uint32_t seed = 0x2545F491u;
  for (int32_t y = 0; y < cfg::SCREEN_H; y++) {
    for (int32_t x = 0; x < cfg::SCREEN_W; x++) {
      seed = seed * 1664525u + 1013904223u;
      line[x] = static_cast<uint16_t>(seed >> 16);
    }
    s_gfx.pushImage(0, y, cfg::SCREEN_W, 1, line);
  }
  s_gfx.waitDMA();
  uint32_t bad = 0;
  seed = 0x2545F491u;
  for (int32_t y = 0; y < cfg::SCREEN_H; y++) {
    s_gfx.readRect(0, y, cfg::SCREEN_W, 1, back);
    for (int32_t x = 0; x < cfg::SCREEN_W; x++) {
      seed = seed * 1664525u + 1013904223u;
      if (back[x] != static_cast<uint16_t>(seed >> 16)) bad++;
    }
  }
  return bad;
}

LgfxCyd& gfx() { return s_gfx; }

FlushPerf takeFlushPerf() {
  FlushPerf p = s_perf;
  s_perf = FlushPerf{};
  return p;
}

void startLvgl(uint8_t* buf1, uint8_t* buf2, uint32_t bytes) {
  lv_tick_set_cb(tickCb);
  s_disp = lv_display_create(cfg::SCREEN_W, cfg::SCREEN_H);
  lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
  lv_display_set_flush_cb(s_disp, flushCb);
  lv_display_set_buffers(s_disp, buf1, buf2, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  s_indev = lv_indev_create();
  lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(s_indev, touchCb);
  lv_indev_set_display(s_indev, s_disp);
  s_gfx.startWrite();
}

void setBrightnessPct(uint8_t pct) {
  s_brightness = pct;
  s_gfx.setBrightness(pctToRaw(pct));
}

uint8_t brightnessPct() { return s_brightness; }

void setTouchRotation(uint8_t rot) { s_gfx.setTouchRotation(rot); }

uint32_t lastTouchMs() { return s_lastTouch; }

bool touching() { return s_touching; }

void setTouchLog(bool on) { s_logTouch = on; }

uint32_t touchReads() { return s_touchReads; }

bool touchCalSupported() { return false; }

bool rawTouch(int32_t&, int32_t&) { return false; }

bool calibrateTouch(const int32_t*, const int32_t*, const int32_t*, const int32_t*) { return false; }

bool mapRawTouch(int32_t, int32_t, int32_t&, int32_t&) { return false; }

void keepTouchCal(bool) {}

void dumpScreen() {
  constexpr int32_t PIECE = 64;
  uint16_t px[PIECE];
  unsigned char b64[((PIECE * 2 + 2) / 3) * 4 + 4];
  s_gfx.waitDMA();
  bridge::bulk(true);
  bridge::printf("@shot %d %d\n", cfg::SCREEN_W, cfg::SCREEN_H);
  for (int32_t y = 0; y < cfg::SCREEN_H; y++) {
    for (int32_t x = 0; x < cfg::SCREEN_W; x += PIECE) {
      s_gfx.readRect(x, y, PIECE, 1, px);
      size_t n = 0;
      mbedtls_base64_encode(b64, sizeof(b64), &n, reinterpret_cast<const unsigned char*>(px), sizeof(px));
      b64[n] = 0;
      bridge::pace();
      bridge::printf("@px %ld %ld %s\n", static_cast<long>(y), static_cast<long>(x), b64);
    }
  }
  bridge::println("@shotend");
  bridge::bulk(false);
}

bool drawJpeg(JpegReader read, void* ctx, int32_t x, int32_t y, uint8_t* pool, size_t poolBytes) {
  if (!read || !pool) return false;
  lgfxJdec jd;
  JpegSrc src = {read, ctx, x, y};
  if (lgfx_jd_prepare(&jd, jpegIn, pool, static_cast<uint_fast16_t>(poolBytes), &src) != JDR_OK) return false;
  return lgfx_jd_decomp(&jd, jpegOut, 0) == JDR_OK;
}

}
