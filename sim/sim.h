#pragma once

#include <stdint.h>

#include "app/shared.h"

namespace sim {

void setWipe(bool on);
void setUtc(uint32_t utc);
uint32_t utc();
void setLastTouch(uint32_t ms);
BulbState& bulb();
void publishBulb();
NetStatus& net();
void publishNet();
void publishPrinter(const PrinterState& p);
void setMedia(bool link, bool playing, uint32_t posMs, uint32_t durMs, uint32_t accent, const char* title,
              const char* artist, const char* album, bool art);
void setMediaVol(int8_t vol);
void setPlaylists(uint8_t status, int n, int playing, bool shuffle, int pick, uint8_t result, const char* msg);
void setPlaylistNames(const char* const* names, const uint16_t* totals, int n);
void setUpdate(uint8_t phase, uint32_t got, uint32_t total, const char* why);

void run(uint32_t ms);
void shot(const char* name);
void film(const char* name, uint32_t ms);
uint32_t localUtc(int y, int m, int d, int hh, int mm);
void forecast(uint32_t now, bool rain);
void nowWeather(uint8_t code, int16_t feels10, uint16_t gust10, uint8_t uv10, uint8_t isDay);
void today(int16_t hi10, int16_t lo10);
void fixtures();
void quake(uint32_t now, int16_t mag10, uint32_t ago, const char* place);
void events(uint32_t now);
void bulbOn(bool on);
void netUp(uint32_t now, bool wifi);
void defaults();
PrinterState printer(uint8_t phase, bool connected);
void variants();
void films();

}
