#include "weather.h"

#include <string.h>

#include "app/store.h"
#include "cache_fmt.h"
#include "clock/clock.h"
#include "net/cache.h"
#include "net/http_min.h"
#include "openmeteo.h"
#include "settings/settings.h"

namespace weather {

namespace {

rc::Forecast s_fc;
rc::Daily s_daily;
rc::AirQ s_air;
rc::Marine s_sea;

bool parseWeather(rc::ByteSource& src, void*) { return rc::omParseWeather(src, net::arena(), s_fc, s_daily); }

bool parseAir(rc::ByteSource& src, void* ctx) {
  return rc::omParseAir(src, net::arena(), *static_cast<rc::AirQ*>(ctx));
}

bool parseMarine(rc::ByteSource& src, void* ctx) {
  return rc::omParseMarine(src, net::arena(), *static_cast<rc::Marine*>(ctx));
}

template <typename T>
int16_t run(const char* host, size_t (*pathFn)(char*, size_t, float, float), net::ParseFn parse, T& work,
            store::Domain dom, uint16_t kind, bool (*valid)(const T&), bool allowEmpty, net::FetchStat& st) {
  Settings s = settings::get();
  char path[rc::OM_PATH_MAX];
  if (!pathFn(path, sizeof(path), s.lat, s.lon)) {
    st.code = http::E_BUILD;
    return st.code;
  }
  int16_t code = net::get(host, path, false, parse, &work, st);
  if (code != 200) return code;
  uint32_t now = clk::synced() ? clk::nowUtc() : 0;
  work.fetchedUtc = now;
  if (!valid(work)) {
    if (!allowEmpty) {
      st.code = http::E_PARSE;
      return st.code;
    }
    store::putT(dom, work);
    return code;
  }
  store::putT(dom, work);
  if (now) cache::save(kind, &work, sizeof(work), now);
  return code;
}

bool fcValid(const rc::Forecast& f) { return f.valid != 0; }
bool dailyValid(const rc::Daily& d) { return d.valid != 0 && d.count > 0; }
bool airValid(const rc::AirQ& a) { return a.valid != 0; }
bool seaValid(const rc::Marine& m) { return m.valid != 0; }

template <typename T>
void loadOne(uint16_t kind, T& v, store::Domain dom, bool (*valid)(const T&)) {
  if (cache::load(kind, &v, sizeof(v)) && valid(v)) store::putT(dom, v);
}

}

void loadCache() {
  loadOne(rc::BLOB_FORECAST, s_fc, store::FORECAST, fcValid);
  loadOne(rc::BLOB_DAILY, s_daily, store::DAILY, dailyValid);
  loadOne(rc::BLOB_AIR, s_air, store::AIR, airValid);
  loadOne(rc::BLOB_MARINE, s_sea, store::MARINE, seaValid);
}

void clearAll() {
  rc::clearForecast(s_fc);
  rc::clearDaily(s_daily);
  memset(&s_air, 0, sizeof(s_air));
  s_air.eaqi = rc::NAU16;
  memset(&s_sea, 0, sizeof(s_sea));
  s_sea.sst10 = rc::NA16;
  store::putT(store::FORECAST, s_fc);
  store::putT(store::DAILY, s_daily);
  store::putT(store::AIR, s_air);
  store::putT(store::MARINE, s_sea);
  cache::forget(rc::BLOB_FORECAST);
  cache::forget(rc::BLOB_DAILY);
  cache::forget(rc::BLOB_AIR);
  cache::forget(rc::BLOB_MARINE);
}

int16_t fetchWeather(net::FetchStat& st) {
  Settings s = settings::get();
  char path[rc::OM_PATH_MAX];
  if (!rc::omWeatherPath(path, sizeof(path), s.lat, s.lon)) {
    st.code = http::E_BUILD;
    return st.code;
  }
  int16_t code = net::get(rc::OM_HOST_FORECAST, path, false, parseWeather, nullptr, st);
  if (code != 200) return code;
  uint32_t now = clk::synced() ? clk::nowUtc() : 0;
  s_fc.fetchedUtc = now;
  s_daily.fetchedUtc = now;
  store::putT(store::FORECAST, s_fc);
  store::putT(store::DAILY, s_daily);
  if (!now) return code;
  cache::save(rc::BLOB_FORECAST, &s_fc, sizeof(s_fc), now);
  cache::save(rc::BLOB_DAILY, &s_daily, sizeof(s_daily), now);
  return code;
}

int16_t fetchAir(net::FetchStat& st) {
  return run(rc::OM_HOST_AIR, rc::omAirPath, parseAir, s_air, store::AIR, rc::BLOB_AIR, airValid, false, st);
}

int16_t fetchMarine(net::FetchStat& st) {
  return run(rc::OM_HOST_MARINE, rc::omMarinePath, parseMarine, s_sea, store::MARINE, rc::BLOB_MARINE, seaValid, true,
             st);
}

}
