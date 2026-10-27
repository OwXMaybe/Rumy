#include "openmeteo.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace rc {

const char* const OM_HOST_FORECAST = "api.open-meteo.com";
const char* const OM_HOST_AIR = "air-quality-api.open-meteo.com";
const char* const OM_HOST_MARINE = "marine-api.open-meteo.com";

namespace {

size_t done(int n, size_t cap) { return (n < 0 || static_cast<size_t>(n) >= cap) ? 0 : static_cast<size_t>(n); }

int16_t i16OrNa(JsonVariantConst v, float scale) {
  if (v.isNull()) return NA16;
  float f = v.as<float>() * scale;
  if (f > 32767.0f) f = 32767.0f;
  if (f < -32767.0f) f = -32767.0f;
  return static_cast<int16_t>(lroundf(f));
}

uint16_t u16OrNa(JsonVariantConst v, float scale) {
  if (v.isNull()) return NAU16;
  float f = v.as<float>() * scale;
  if (f < 0.0f) f = 0.0f;
  if (f > 65534.0f) f = 65534.0f;
  return static_cast<uint16_t>(lroundf(f));
}

uint8_t u8OrNa(JsonVariantConst v, float scale) {
  if (v.isNull()) return NA8;
  float f = v.as<float>() * scale;
  if (f < 0.0f) f = 0.0f;
  if (f > 254.0f) f = 254.0f;
  return static_cast<uint8_t>(lroundf(f));
}

uint32_t u32(JsonVariantConst v) {
  if (v.isNull()) return 0;
  long x = v.as<long>();
  return x > 0 ? static_cast<uint32_t>(x) : 0;
}

void finishDoc(JsonDocument& doc, JsonArena& arena) {
  doc.clear();
  arena.reset();
}

const char* const kCurrent[] = {"time",           "temperature_2m",     "relative_humidity_2m", "apparent_temperature",
                                "is_day",         "precipitation",      "weather_code",         "pressure_msl",
                                "wind_speed_10m", "wind_direction_10m", "wind_gusts_10m",       "uv_index",
                                "visibility"};
const char* const kHourly[] = {"time", "temperature_2m", "weather_code", "precipitation_probability", "is_day"};
const char* const kDaily[] = {"time",    "weather_code", "temperature_2m_max", "temperature_2m_min",
                              "precipitation_probability_max", "sunrise", "sunset", "uv_index_max"};

void forecastFilter(JsonDocument& filter) {
  JsonObject fc = filter["current"].to<JsonObject>();
  for (const char* k : kCurrent) fc[JsonString(k, true)] = true;
  filter["minutely_15"]["time"] = true;
  filter["minutely_15"]["precipitation"] = true;
  JsonObject fh = filter["hourly"].to<JsonObject>();
  for (const char* k : kHourly) fh[JsonString(k, true)] = true;
}

void dailyFilter(JsonDocument& filter) {
  JsonObject fd = filter["daily"].to<JsonObject>();
  for (const char* k : kDaily) fd[JsonString(k, true)] = true;
}

void readForecast(JsonDocument& doc, Forecast& out) {
  JsonObjectConst c = doc["current"];
  WxNow& n = out.now;
  n.time = u32(c["time"]);
  n.temp10 = i16OrNa(c["temperature_2m"], 10.0f);
  n.feels10 = i16OrNa(c["apparent_temperature"], 10.0f);
  n.rh = u8OrNa(c["relative_humidity_2m"], 1.0f);
  n.isDay = c["is_day"] | 1;
  n.precip100 = u16OrNa(c["precipitation"], 100.0f);
  n.code = u8OrNa(c["weather_code"], 1.0f);
  n.press10 = u16OrNa(c["pressure_msl"], 10.0f);
  n.wind10 = u16OrNa(c["wind_speed_10m"], 10.0f);
  n.windDir = u16OrNa(c["wind_direction_10m"], 1.0f);
  n.gust10 = u16OrNa(c["wind_gusts_10m"], 10.0f);
  n.uv10 = u8OrNa(c["uv_index"], 10.0f);
  n.visDam = u16OrNa(c["visibility"], 0.1f);

  JsonArrayConst mt = doc["minutely_15"]["time"];
  JsonArrayConst mp = doc["minutely_15"]["precipitation"];
  if (!mt.isNull() && mt.size() > 0) out.m15.start = u32(mt[0]);
  int i = 0;
  for (JsonVariantConst v : mp) {
    if (i >= MINUTELY) break;
    out.m15.precip100[i++] = u16OrNa(v, 100.0f);
  }
  out.m15.count = static_cast<uint8_t>(i);

  JsonObjectConst h = doc["hourly"];
  JsonArrayConst ht = h["time"];
  if (!ht.isNull() && ht.size() > 0) out.hourStart = u32(ht[0]);
  int count = 0;
  for (JsonVariantConst v : h["temperature_2m"].as<JsonArrayConst>()) {
    if (count >= HOURS) break;
    out.hours[count++].temp10 = i16OrNa(v, 10.0f);
  }
  i = 0;
  for (JsonVariantConst v : h["weather_code"].as<JsonArrayConst>()) {
    if (i >= HOURS) break;
    out.hours[i++].code = u8OrNa(v, 1.0f);
  }
  i = 0;
  for (JsonVariantConst v : h["precipitation_probability"].as<JsonArrayConst>()) {
    if (i >= HOURS) break;
    out.hours[i++].pop = u8OrNa(v, 1.0f);
  }
  i = 0;
  for (JsonVariantConst v : h["is_day"].as<JsonArrayConst>()) {
    if (i >= HOURS) break;
    out.hours[i++].isDay = v | 1;
  }
  out.hourCount = static_cast<uint8_t>(count);
  out.valid = (n.time != 0 && n.temp10 != NA16) ? 1 : 0;
}

void readDaily(JsonDocument& doc, Daily& out) {
  JsonObjectConst d = doc["daily"];
  int count = 0;
  for (JsonVariantConst v : d["time"].as<JsonArrayConst>()) {
    if (count >= DAYS) break;
    out.days[count++].date = u32(v);
  }
  int i = 0;
  for (JsonVariantConst v : d["weather_code"].as<JsonArrayConst>()) {
    if (i >= DAYS) break;
    out.days[i++].code = u8OrNa(v, 1.0f);
  }
  i = 0;
  for (JsonVariantConst v : d["temperature_2m_max"].as<JsonArrayConst>()) {
    if (i >= DAYS) break;
    out.days[i++].tmax10 = i16OrNa(v, 10.0f);
  }
  i = 0;
  for (JsonVariantConst v : d["temperature_2m_min"].as<JsonArrayConst>()) {
    if (i >= DAYS) break;
    out.days[i++].tmin10 = i16OrNa(v, 10.0f);
  }
  i = 0;
  for (JsonVariantConst v : d["precipitation_probability_max"].as<JsonArrayConst>()) {
    if (i >= DAYS) break;
    out.days[i++].popMax = u8OrNa(v, 1.0f);
  }
  i = 0;
  for (JsonVariantConst v : d["sunrise"].as<JsonArrayConst>()) {
    if (i >= DAYS) break;
    out.days[i++].sunrise = u32(v);
  }
  i = 0;
  for (JsonVariantConst v : d["sunset"].as<JsonArrayConst>()) {
    if (i >= DAYS) break;
    out.days[i++].sunset = u32(v);
  }
  i = 0;
  for (JsonVariantConst v : d["uv_index_max"].as<JsonArrayConst>()) {
    if (i >= DAYS) break;
    out.days[i++].uvMax10 = u8OrNa(v, 10.0f);
  }
  out.count = static_cast<uint8_t>(count);
  out.valid = count > 0 ? 1 : 0;
}

bool parseParts(ByteSource& src, JsonArena& arena, Forecast* fc, Daily* daily) {
  if (fc) clearForecast(*fc);
  if (daily) clearDaily(*daily);
  arena.reset();
  JsonDocument filter(&arena);
  if (fc) forecastFilter(filter);
  if (daily) dailyFilter(filter);
  if (filter.overflowed()) {
    filter.clear();
    arena.reset();
    return false;
  }

  JsonDocument doc(&arena);
  DeserializationError err = deserializeJson(doc, src, DeserializationOption::Filter(filter));
  bool ok = !err && (!fc || doc["current"].is<JsonObjectConst>()) && (!daily || doc["daily"].is<JsonObjectConst>());
  if (ok && fc) {
    readForecast(doc, *fc);
    ok = fc->valid != 0;
  }
  if (ok && daily) {
    readDaily(doc, *daily);
    ok = daily->valid != 0;
  }
  finishDoc(doc, arena);
  filter.clear();
  return ok;
}

}

int16_t toTenths(float v) { return static_cast<int16_t>(lroundf(v * 10.0f)); }

void clearForecast(Forecast& f) {
  memset(&f, 0, sizeof(f));
  f.now.temp10 = NA16;
  f.now.feels10 = NA16;
  f.now.press10 = NAU16;
  f.now.wind10 = NAU16;
  f.now.gust10 = NAU16;
  f.now.windDir = NAU16;
  f.now.visDam = NAU16;
  f.now.precip100 = NAU16;
  f.now.rh = NA8;
  f.now.code = NA8;
  f.now.uv10 = NA8;
  for (int i = 0; i < HOURS; i++) {
    f.hours[i].temp10 = NA16;
    f.hours[i].code = NA8;
    f.hours[i].pop = NA8;
  }
  for (int i = 0; i < MINUTELY; i++) f.m15.precip100[i] = NAU16;
}

void clearDaily(Daily& d) {
  memset(&d, 0, sizeof(d));
  for (int i = 0; i < DAYS; i++) {
    d.days[i].tmax10 = NA16;
    d.days[i].tmin10 = NA16;
    d.days[i].code = NA8;
    d.days[i].popMax = NA8;
    d.days[i].uvMax10 = NA8;
  }
}

size_t omWeatherPath(char* out, size_t cap, float lat, float lon) {
  return done(snprintf(out, cap,
                       "/v1/forecast?latitude=%.3f&longitude=%.3f"
                       "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,precipitation,"
                       "weather_code,pressure_msl,wind_speed_10m,wind_direction_10m,wind_gusts_10m,uv_index,visibility"
                       "&minutely_15=precipitation&forecast_minutely_15=4"
                       "&hourly=temperature_2m,weather_code,precipitation_probability,is_day&forecast_hours=24"
                       "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,"
                       "sunrise,sunset,uv_index_max&forecast_days=7"
                       "&wind_speed_unit=kmh&timezone=Europe%%2FIstanbul&timeformat=unixtime",
                       static_cast<double>(lat), static_cast<double>(lon)),
              cap);
}

size_t omAirPath(char* out, size_t cap, float lat, float lon) {
  return done(snprintf(out, cap,
                       "/v1/air-quality?latitude=%.3f&longitude=%.3f&current=european_aqi,pm2_5,pm10"
                       "&timezone=Europe%%2FIstanbul&timeformat=unixtime",
                       static_cast<double>(lat), static_cast<double>(lon)),
              cap);
}

size_t omMarinePath(char* out, size_t cap, float lat, float lon) {
  return done(snprintf(out, cap,
                       "/v1/marine?latitude=%.3f&longitude=%.3f&current=sea_surface_temperature"
                       "&cell_selection=sea&timezone=Europe%%2FIstanbul&timeformat=unixtime",
                       static_cast<double>(lat), static_cast<double>(lon)),
              cap);
}

bool omParseWeather(ByteSource& src, JsonArena& arena, Forecast& fc, Daily& daily) {
  return parseParts(src, arena, &fc, &daily);
}

bool omParseForecast(ByteSource& src, JsonArena& arena, Forecast& out) { return parseParts(src, arena, &out, nullptr); }

bool omParseDaily(ByteSource& src, JsonArena& arena, Daily& out) { return parseParts(src, arena, nullptr, &out); }

bool omParseAir(ByteSource& src, JsonArena& arena, AirQ& out) {
  memset(&out, 0, sizeof(out));
  out.eaqi = NAU16;
  out.pm25x10 = NAU16;
  out.pm10x10 = NAU16;
  arena.reset();
  JsonDocument filter(&arena);
  filter["current"]["time"] = true;
  filter["current"]["european_aqi"] = true;
  filter["current"]["pm2_5"] = true;
  filter["current"]["pm10"] = true;
  JsonDocument doc(&arena);
  DeserializationError err = deserializeJson(doc, src, DeserializationOption::Filter(filter));
  if (err || !doc["current"].is<JsonObjectConst>()) {
    finishDoc(doc, arena);
    filter.clear();
    return false;
  }
  JsonObjectConst c = doc["current"];
  out.time = u32(c["time"]);
  out.eaqi = u16OrNa(c["european_aqi"], 1.0f);
  out.pm25x10 = u16OrNa(c["pm2_5"], 10.0f);
  out.pm10x10 = u16OrNa(c["pm10"], 10.0f);
  out.valid = out.eaqi != NAU16 ? 1 : 0;
  finishDoc(doc, arena);
  filter.clear();
  return out.valid != 0;
}

bool omParseMarine(ByteSource& src, JsonArena& arena, Marine& out) {
  memset(&out, 0, sizeof(out));
  out.sst10 = NA16;
  arena.reset();
  JsonDocument filter(&arena);
  filter["current"]["time"] = true;
  filter["current"]["sea_surface_temperature"] = true;
  JsonDocument doc(&arena);
  DeserializationError err = deserializeJson(doc, src, DeserializationOption::Filter(filter));
  if (err || !doc["current"].is<JsonObjectConst>()) {
    finishDoc(doc, arena);
    filter.clear();
    return false;
  }
  JsonObjectConst c = doc["current"];
  out.time = u32(c["time"]);
  out.sst10 = i16OrNa(c["sea_surface_temperature"], 10.0f);
  out.valid = out.sst10 != NA16 ? 1 : 0;
  finishDoc(doc, arena);
  filter.clear();
  return out.valid != 0;
}

}
