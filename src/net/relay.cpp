#include "relay.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>
#include <string.h>

#include "app/config.h"
#include "kandilli.h"
#include "media/bridge.h"
#include "media/media.h"

namespace relay {

namespace {

enum Kind : uint8_t {
  K_NONE = 0,
  K_QUAKE,
  K_HOLIDAY,
};

enum Result : uint8_t {
  RES_WAIT = 0,
  RES_OK,
  RES_FAIL,
};

portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
char s_req[440];
constexpr size_t REQ_ROOM = sizeof(s_req) - 1;
volatile bool s_reqReady = false;
volatile uint8_t s_wait = K_NONE;
volatile uint8_t s_result = RES_WAIT;
int s_year = 0;
rc::QuakeInfo s_quake;
rc::Religious s_rel;
uint32_t s_asked = 0;
uint32_t s_ok = 0;
uint32_t s_fail = 0;
uint32_t s_timeout = 0;
uint32_t s_lastMs = 0;

bool ask(uint8_t kind) {
  portENTER_CRITICAL(&s_mux);
  s_wait = kind;
  s_result = RES_WAIT;
  portEXIT_CRITICAL(&s_mux);
  s_reqReady = true;
  s_asked++;
  uint32_t t0 = millis();
  while (s_result == RES_WAIT && millis() - t0 < cfg::RELAY_TIMEOUT_MS) vTaskDelay(pdMS_TO_TICKS(25));
  portENTER_CRITICAL(&s_mux);
  uint8_t r = s_result;
  s_wait = K_NONE;
  portEXIT_CRITICAL(&s_mux);
  s_reqReady = false;
  s_lastMs = millis() - t0;
  if (r == RES_OK) s_ok++;
  else if (r == RES_FAIL) s_fail++;
  else s_timeout++;
  return r == RES_OK;
}

}

void line(const char* text) {
  bool quakeLine = !strncmp(text, "deprem ", 7);
  bool holidayLine = !strncmp(text, "bayram ", 7);
  if (!quakeLine && !holidayLine) return;
  if (quakeLine) {
    rc::QuakeInfo q;
    bool ok = rc::relayQuakeLine(text + 7, q);
    portENTER_CRITICAL(&s_mux);
    if (s_wait == K_QUAKE && s_result == RES_WAIT) {
      if (ok) s_quake = q;
      s_result = ok ? RES_OK : RES_FAIL;
    }
    portEXIT_CRITICAL(&s_mux);
    return;
  }
  rc::Religious r;
  bool ok = rc::relayHolidayLine(text + 7, s_year, r);
  portENTER_CRITICAL(&s_mux);
  if (s_wait == K_HOLIDAY && s_result == RES_WAIT) {
    if (ok) s_rel = r;
    s_result = ok ? RES_OK : RES_FAIL;
  }
  portEXIT_CRITICAL(&s_mux);
}

void service() {
  if (!s_reqReady) return;
  char buf[sizeof(s_req)];
  portENTER_CRITICAL(&s_mux);
  memcpy(buf, s_req, sizeof(buf));
  s_reqReady = false;
  portEXIT_CRITICAL(&s_mux);
  size_t n = strnlen(buf, sizeof(buf) - 1);
  buf[n++] = '\n';
  bridge::write(reinterpret_cast<const uint8_t*>(buf), n);
}

bool linked() { return media::state().link; }

bool quake(const char* afadPath, rc::QuakeInfo& out) {
  if (!linked()) return false;
  portENTER_CRITICAL(&s_mux);
  snprintf(s_req, REQ_ROOM, "@veri afad %s", afadPath);
  portEXIT_CRITICAL(&s_mux);
  if (!ask(K_QUAKE)) return false;
  portENTER_CRITICAL(&s_mux);
  out = s_quake;
  portEXIT_CRITICAL(&s_mux);
  return true;
}

bool holidays(int year, rc::Religious& out) {
  if (!linked()) return false;
  portENTER_CRITICAL(&s_mux);
  s_year = year;
  snprintf(s_req, REQ_ROOM, "@veri nager %d", year);
  portEXIT_CRITICAL(&s_mux);
  if (!ask(K_HOLIDAY)) return false;
  portENTER_CRITICAL(&s_mux);
  out = s_rel;
  portEXIT_CRITICAL(&s_mux);
  return true;
}

uint32_t lastMs() { return s_lastMs; }

void report(Print& out) {
  out.printf("[veri] kopru %s · istek %lu · tamam %lu · hata %lu · cevapsiz %lu · son %lu ms\n",
             linked() ? "bagli" : "yok", static_cast<unsigned long>(s_asked), static_cast<unsigned long>(s_ok),
             static_cast<unsigned long>(s_fail), static_cast<unsigned long>(s_timeout),
             static_cast<unsigned long>(s_lastMs));
}

}
