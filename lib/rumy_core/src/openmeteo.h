#pragma once

#include <stddef.h>
#include <stdint.h>

#include "json_arena.h"
#include "rc_types.h"
#include "reader.h"

namespace rc {

extern const char* const OM_HOST_FORECAST;
extern const char* const OM_HOST_AIR;
extern const char* const OM_HOST_MARINE;

constexpr size_t OM_PATH_MAX = 640;

size_t omWeatherPath(char* out, size_t cap, float lat, float lon);
size_t omAirPath(char* out, size_t cap, float lat, float lon);
size_t omMarinePath(char* out, size_t cap, float lat, float lon);

bool omParseWeather(ByteSource& src, JsonArena& arena, Forecast& fc, Daily& daily);
bool omParseForecast(ByteSource& src, JsonArena& arena, Forecast& out);
bool omParseDaily(ByteSource& src, JsonArena& arena, Daily& out);
bool omParseAir(ByteSource& src, JsonArena& arena, AirQ& out);
bool omParseMarine(ByteSource& src, JsonArena& arena, Marine& out);

int16_t toTenths(float v);
void clearForecast(Forecast& f);
void clearDaily(Daily& d);

}
