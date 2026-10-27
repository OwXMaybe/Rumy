#include "camera.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_rom_crc.h>
#include <stdlib.h>
#include <string.h>

#include "app/config.h"
#include "board/display.h"
#include "diag/diag.h"
#include "media/bridge.h"
#include "media/media.h"

namespace cam {

namespace {

struct Stream {
  uint32_t len;
  uint32_t got;
  uint32_t crc;
  bool timedOut;
};

uint8_t* s_pool = nullptr;
bool s_want = false;
uint32_t s_ip = 0;
int32_t s_x = 0;
int32_t s_y = 0;
uint32_t s_lastAsk = 0;
uint32_t s_lastFrameMs = 0;
uint32_t s_frames = 0;
uint32_t s_errors = 0;
uint32_t s_frameUs = 0;
uint32_t s_lastBytes = 0;

void ask(uint32_t now) {
  s_lastAsk = now;
  bridge::printf("@cam on %u.%u.%u.%u\n", static_cast<unsigned>(s_ip & 0xFF),
                 static_cast<unsigned>((s_ip >> 8) & 0xFF), static_cast<unsigned>((s_ip >> 16) & 0xFF),
                 static_cast<unsigned>((s_ip >> 24) & 0xFF));
}

bool waitBytes() {
  uint32_t t0 = millis();
  while (bridge::available() <= 0) {
    if (millis() - t0 >= bridge::byteTimeoutMs()) return false;
    vTaskDelay(1);
  }
  return true;
}

uint32_t readIn(void* ctx, uint8_t* buf, uint32_t len) {
  Stream* s = static_cast<Stream*>(ctx);
  uint8_t sink[64];
  uint32_t done = 0;
  while (done < len && s->got < s->len) {
    if (!waitBytes()) {
      s->timedOut = true;
      break;
    }
    uint32_t n = len - done;
    uint32_t left = s->len - s->got;
    uint32_t toChunk = cfg::MEDIA_CHUNK - s->got % cfg::MEDIA_CHUNK;
    uint32_t avail = static_cast<uint32_t>(bridge::available());
    if (n > left) n = left;
    if (n > toChunk) n = toChunk;
    if (n > avail) n = avail;
    if (!buf && n > sizeof(sink)) n = sizeof(sink);
    uint8_t* dst = buf ? buf + done : sink;
    size_t r = bridge::read(dst, n);
    if (!r) {
      s->timedOut = true;
      break;
    }
    s->crc = esp_rom_crc32_le(s->crc, dst, r);
    s->got += static_cast<uint32_t>(r);
    done += static_cast<uint32_t>(r);
    if (s->got % cfg::MEDIA_CHUNK == 0 && s->got < s->len) bridge::printf("@ok %lu\n", static_cast<unsigned long>(s->got));
  }
  return done;
}

}

void want(bool on, uint32_t printerIp, int32_t x, int32_t y) {
  if (!printerIp) on = false;
  s_x = x;
  s_y = y;
  if (on == s_want && (!on || printerIp == s_ip)) return;
  s_want = on;
  s_ip = printerIp;
  if (!on) {
    bridge::println("@cam off");
    if (s_pool) heap_caps_free(s_pool);
    s_pool = nullptr;
    s_lastFrameMs = 0;
    return;
  }
  if (!s_pool) s_pool = static_cast<uint8_t*>(heap_caps_malloc(cfg::CAM_POOL_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (!s_pool) {
    s_want = false;
    diag::note("kamera", "bellek ayrilamadi");
    return;
  }
  s_lastFrameMs = 0;
  ask(millis());
}

void loop() {
  if (s_want && millis() - s_lastAsk >= cfg::CAM_KEEPALIVE_MS) ask(millis());
}

void start(char* args) {
  char* end = nullptr;
  unsigned long len = strtoul(args, &end, 10);
  uint32_t crc = strtoul(end, &end, 16);
  if (!s_want || !s_pool) {
    bridge::println("@cam err kapali");
    return;
  }
  if (!len || len > cfg::CAM_JPEG_MAX) {
    bridge::println("@cam err boyut");
    return;
  }
  Stream st = {static_cast<uint32_t>(len), 0, 0, false};
  uint32_t t0 = micros();
  bridge::printf("@go %08lx\n", static_cast<unsigned long>(crc));
  bool ok = display::drawJpeg(readIn, &st, s_x, s_y, s_pool, cfg::CAM_POOL_BYTES);
  while (!st.timedOut && st.got < st.len) {
    if (!readIn(&st, nullptr, st.len - st.got)) break;
  }
  s_frameUs = micros() - t0;
  s_lastBytes = st.got;
  if (st.timedOut || st.got < st.len) {
    s_errors++;
    bridge::println("@cam err zaman");
    media::discardUntilQuiet();
    return;
  }
  if (st.crc != crc) {
    s_errors++;
    bridge::println("@cam err crc");
    media::discardUntilQuiet();
    return;
  }
  if (!ok) {
    s_errors++;
    bridge::println("@cam err cozme");
    media::discardUntilQuiet();
    return;
  }
  s_frames++;
  s_lastFrameMs = millis();
  bridge::println("@cam ok");
}

bool live() { return s_want && s_lastFrameMs && millis() - s_lastFrameMs < cfg::CAM_STALE_MS; }

void report(Print& out) {
  uint32_t ago = s_lastFrameMs ? (millis() - s_lastFrameMs) / 1000U : 0;
  out.printf("[kamera] %s · kare %lu · hata %lu · son kare %s%lu sn · kare suresi %lu ms · %lu B · cozucu alani %s\n",
             s_want ? "istendi" : "kapali", static_cast<unsigned long>(s_frames), static_cast<unsigned long>(s_errors),
             s_lastFrameMs ? "" : "- ", static_cast<unsigned long>(ago), static_cast<unsigned long>(s_frameUs / 1000U),
             static_cast<unsigned long>(s_lastBytes), s_pool ? "ayrildi" : "yok");
}

}
