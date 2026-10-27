#include "store.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>

#include "espn.h"
#include "espn_board.h"
#include "openmeteo.h"
#include "shared.h"

namespace store {

namespace {

rc::Forecast s_forecast;
rc::Daily s_daily;
rc::AirQ s_air;
rc::Marine s_marine;
rc::QuakeInfo s_quake;
rc::EventsSnapshot s_events;
BulbState s_bulb;
NetStatus s_net;
LvglStats s_lvgl;
PrinterState s_printer;
rc::Match s_match;
rc::Board s_board;
rc::LeagueView s_view;

struct Slot {
  void* data;
  size_t size;
  volatile uint32_t ver;
};

Slot s_slots[COUNT] = {
    {&s_forecast, sizeof(s_forecast), 0}, {&s_daily, sizeof(s_daily), 0}, {&s_air, sizeof(s_air), 0},
    {&s_marine, sizeof(s_marine), 0},     {&s_quake, sizeof(s_quake), 0}, {&s_events, sizeof(s_events), 0},
    {&s_bulb, sizeof(s_bulb), 0},         {&s_net, sizeof(s_net), 0},     {&s_lvgl, sizeof(s_lvgl), 0},
    {&s_printer, sizeof(s_printer), 0},   {&s_match, sizeof(s_match), 0}, {&s_board, sizeof(s_board), 0},
    {&s_view, sizeof(s_view), 0},         {nullptr, 0, 0},
};

SemaphoreHandle_t s_mutex = nullptr;
StaticSemaphore_t s_mutexBuf;

void lock() { xSemaphoreTake(s_mutex, portMAX_DELAY); }
void unlock() { xSemaphoreGive(s_mutex); }

}

void init() {
  if (s_mutex) return;
  s_mutex = xSemaphoreCreateMutexStatic(&s_mutexBuf);
  rc::clearForecast(s_forecast);
  rc::clearDaily(s_daily);
  memset(&s_air, 0, sizeof(s_air));
  s_air.eaqi = rc::NAU16;
  memset(&s_marine, 0, sizeof(s_marine));
  s_marine.sst10 = rc::NA16;
  memset(&s_quake, 0, sizeof(s_quake));
  memset(&s_events, 0, sizeof(s_events));
  memset(&s_bulb, 0, sizeof(s_bulb));
  memset(&s_net, 0, sizeof(s_net));
  memset(&s_lvgl, 0, sizeof(s_lvgl));
  memset(&s_printer, 0, sizeof(s_printer));
  rc::matchClear(s_match);
  rc::boardClear(s_board);
  memset(&s_view, 0, sizeof(s_view));
  rc::k2Clear(s_printer.st);
}

void put(Domain d, const void* src, size_t n) {
  if (d >= COUNT) return;
  Slot& s = s_slots[d];
  lock();
  if (s.data && n == s.size) memcpy(s.data, src, n);
  s.ver = s.ver + 1;
  unlock();
}

bool get(Domain d, void* dst, size_t n, uint32_t& seen) {
  if (d >= COUNT) return false;
  Slot& s = s_slots[d];
  if (s.ver == seen) return false;
  lock();
  if (s.data && n == s.size) memcpy(dst, s.data, n);
  seen = s.ver;
  unlock();
  return true;
}

void peek(Domain d, void* dst, size_t n) {
  if (d >= COUNT) return;
  Slot& s = s_slots[d];
  lock();
  if (s.data && n == s.size) memcpy(dst, s.data, n);
  unlock();
}

uint32_t version(Domain d) { return d < COUNT ? s_slots[d].ver : 0; }

void bump(Domain d) {
  if (d >= COUNT) return;
  lock();
  s_slots[d].ver = s_slots[d].ver + 1;
  unlock();
}

}
