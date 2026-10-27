#pragma once

#include <lvgl.h>
#include <stdint.h>

#include "app/shared.h"
#include "espn.h"
#include "espn_board.h"
#include "rc_types.h"
#include "settings/settings.h"

namespace ui {

enum ScreenId : uint8_t {
  SCR_HOME = 0,
  SCR_WEATHER,
  SCR_LIGHT,
  SCR_SETTINGS,
  SCR_MAIN_COUNT,
  SCR_DIAG = SCR_MAIN_COUNT,
  SCR_PAIR,
  SCR_LOCATION,
  SCR_NIGHT,
  SCR_INFO,
  SCR_EVENT,
  SCR_DISPLAY_CFG,
  SCR_NIGHT_CFG,
  SCR_MEDIA,
  SCR_PRINTER,
  SCR_TOUCH_CAL,
  SCR_MATCH,
  SCR_PLAYLIST,
  SCR_FOOTBALL,
  SCR_LEAGUE,
  SCR_AGENTS,
  SCR_PHONE,
  SCR_NONE = 0xFF,
};

enum DataMask : uint32_t {
  M_FORECAST = 1u << 0,
  M_DAILY = 1u << 1,
  M_AIR = 1u << 2,
  M_MARINE = 1u << 3,
  M_QUAKE = 1u << 4,
  M_EVENTS = 1u << 5,
  M_BULB = 1u << 6,
  M_NET = 1u << 7,
  M_SETTINGS = 1u << 8,
  M_MINUTE = 1u << 9,
  M_NIGHT = 1u << 10,
  M_SECOND = 1u << 11,
  M_MEDIA = 1u << 12,
  M_PRINTER = 1u << 13,
  M_MATCH = 1u << 14,
  M_BOARD = 1u << 15,
  M_VIEW = 1u << 16,
  M_AGENTS = 1u << 17,
  M_ALL = 0xFFFFFFFFu,
};

struct Data {
  rc::Forecast fc;
  rc::Daily daily;
  rc::AirQ air;
  rc::Marine sea;
  rc::QuakeInfo quake;
  rc::EventsSnapshot events;
  BulbState bulb;
  NetStatus net;
  PrinterState printer;
  rc::Match match;
  rc::Board board;
  rc::LeagueView view;
  Settings settings;
};

void begin();
void loop();
void go(ScreenId id, int dir);
void back();
ScreenId current();
bool busy();
uint32_t lastCreateUs();
uint32_t lowMemRefusals();
const Data& data();
bool night();
bool nightAwake();
void wake();
void toast(const char* text);
void attachGestures(lv_obj_t* scr);
bool quakeAlertActive();
bool quakeSurprise();
bool rainSoon();
bool printerActive();

}
