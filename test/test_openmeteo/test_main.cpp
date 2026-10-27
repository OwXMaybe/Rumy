#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "../fixtures/fixtures.h"
#include "openmeteo.h"

static uint8_t g_mem[8192];
static rc::JsonArena g_arena(g_mem, sizeof(g_mem));

void setUp() {}
void tearDown() {}

static void test_weather_path() {
  char path[rc::OM_PATH_MAX];
  size_t n = rc::omWeatherPath(path, sizeof(path), 40.978f, 27.511f);
  TEST_ASSERT_TRUE(n > 100);
  TEST_ASSERT_NOT_NULL(strstr(path, "latitude=40.978&longitude=27.511"));
  TEST_ASSERT_NOT_NULL(strstr(path, "&current=temperature_2m,"));
  TEST_ASSERT_NOT_NULL(strstr(path, "forecast_hours=24"));
  TEST_ASSERT_NOT_NULL(strstr(path, "forecast_minutely_15=4"));
  TEST_ASSERT_NOT_NULL(strstr(path, "&daily=weather_code,"));
  TEST_ASSERT_NOT_NULL(strstr(path, "forecast_days=7"));
  TEST_ASSERT_NOT_NULL(strstr(path, "timeformat=unixtime"));
  TEST_ASSERT_NOT_NULL(strstr(path, "Europe%2FIstanbul"));
  TEST_ASSERT_TRUE(rc::omWeatherPath(path, sizeof(path), -89.999f, -179.999f) > n);
  char small[32];
  TEST_ASSERT_EQUAL_UINT32(0, rc::omWeatherPath(small, sizeof(small), 1, 2));
}

static void test_parse_weather_fixture() {
  static uint8_t mem[8192];
  rc::JsonArena arena(mem, sizeof(mem));
  rc::MemSource src(FX_OPENMETEO_WEATHER, strlen(FX_OPENMETEO_WEATHER));
  rc::Forecast f;
  rc::Daily d;
  TEST_ASSERT_TRUE(rc::omParseWeather(src, arena, f, d));
  TEST_ASSERT_EQUAL_UINT8(1, f.valid);
  TEST_ASSERT_EQUAL_UINT32(1790525700u, f.now.time);
  TEST_ASSERT_EQUAL_INT16(183, f.now.temp10);
  TEST_ASSERT_EQUAL_INT16(163, f.now.feels10);
  TEST_ASSERT_EQUAL_UINT8(75, f.now.rh);
  TEST_ASSERT_EQUAL_UINT8(0, f.now.isDay);
  TEST_ASSERT_EQUAL_UINT8(3, f.now.code);
  TEST_ASSERT_EQUAL_UINT16(10181, f.now.press10);
  TEST_ASSERT_EQUAL_UINT16(218, f.now.wind10);
  TEST_ASSERT_EQUAL_UINT16(16, f.now.windDir);
  TEST_ASSERT_EQUAL_UINT16(529, f.now.gust10);
  TEST_ASSERT_EQUAL_UINT16(3398, f.now.visDam);
  TEST_ASSERT_EQUAL_UINT8(4, f.m15.count);
  TEST_ASSERT_EQUAL_UINT32(1790525700u, f.m15.start);
  TEST_ASSERT_EQUAL_UINT8(24, f.hourCount);
  TEST_ASSERT_EQUAL_UINT32(1790524800u, f.hourStart);
  TEST_ASSERT_EQUAL_INT16(184, f.hours[0].temp10);
  TEST_ASSERT_EQUAL_INT16(186, f.hours[15].temp10);
  TEST_ASSERT_EQUAL_UINT8(50, f.hours[1].pop);
  TEST_ASSERT_EQUAL_UINT8(0, f.hours[0].isDay);
  TEST_ASSERT_EQUAL_UINT8(1, f.hours[13].isDay);
  TEST_ASSERT_EQUAL_UINT8(1, d.valid);
  TEST_ASSERT_EQUAL_UINT8(7, d.count);
  TEST_ASSERT_EQUAL_UINT32(1790456400u, d.days[0].date);
  TEST_ASSERT_EQUAL_INT16(213, d.days[0].tmax10);
  TEST_ASSERT_EQUAL_INT16(170, d.days[0].tmin10);
  TEST_ASSERT_EQUAL_UINT8(50, d.days[0].popMax);
  TEST_ASSERT_EQUAL_UINT32(1790481739u, d.days[0].sunrise);
  TEST_ASSERT_EQUAL_UINT32(1790524743u, d.days[0].sunset);
  TEST_ASSERT_EQUAL_UINT8(39, d.days[0].uvMax10);
  TEST_ASSERT_EQUAL_UINT8(80, d.days[2].code);
  TEST_ASSERT_EQUAL_UINT8(95, d.days[6].code);
  TEST_ASSERT_EQUAL_INT16(146, d.days[6].tmin10);
  TEST_ASSERT_EQUAL_UINT32(0, arena.used());
  TEST_ASSERT_EQUAL_UINT32(0, arena.failures());
  TEST_ASSERT_TRUE(arena.peak() < arena.capacity());
}

static void test_weather_matches_parts() {
  static uint8_t memWhole[8192];
  static uint8_t memNow[8192];
  static uint8_t memDays[8192];
  rc::JsonArena aWhole(memWhole, sizeof(memWhole));
  rc::JsonArena aNow(memNow, sizeof(memNow));
  rc::JsonArena aDays(memDays, sizeof(memDays));
  rc::Forecast whole;
  rc::Daily wholeDaily;
  rc::MemSource a(FX_OPENMETEO_WEATHER, strlen(FX_OPENMETEO_WEATHER));
  TEST_ASSERT_TRUE(rc::omParseWeather(a, aWhole, whole, wholeDaily));
  rc::Forecast part;
  rc::MemSource b(FX_OPENMETEO_WEATHER, strlen(FX_OPENMETEO_WEATHER));
  TEST_ASSERT_TRUE(rc::omParseForecast(b, aNow, part));
  TEST_ASSERT_EQUAL_MEMORY(&part, &whole, sizeof(part));
  rc::Daily partDaily;
  rc::MemSource c(FX_OPENMETEO_WEATHER, strlen(FX_OPENMETEO_WEATHER));
  TEST_ASSERT_TRUE(rc::omParseDaily(c, aDays, partDaily));
  TEST_ASSERT_EQUAL_MEMORY(&partDaily, &wholeDaily, sizeof(partDaily));
  char msg[112];
  snprintf(msg, sizeof(msg), "cevap %u B; arena tepe: yalniz hava %u B, yalniz 7 gun %u B, birlesik %u B",
           static_cast<unsigned>(strlen(FX_OPENMETEO_WEATHER)), static_cast<unsigned>(aNow.peak()),
           static_cast<unsigned>(aDays.peak()), static_cast<unsigned>(aWhole.peak()));
  TEST_MESSAGE(msg);
}

static void test_parse_weather_needs_both_parts() {
  rc::Forecast f;
  rc::Daily d;
  rc::MemSource onlyNow(FX_OPENMETEO_FORECAST, strlen(FX_OPENMETEO_FORECAST));
  TEST_ASSERT_FALSE(rc::omParseWeather(onlyNow, g_arena, f, d));
  TEST_ASSERT_EQUAL_UINT8(0, d.valid);
  rc::MemSource onlyDays(FX_OPENMETEO_DAILY, strlen(FX_OPENMETEO_DAILY));
  TEST_ASSERT_FALSE(rc::omParseWeather(onlyDays, g_arena, f, d));
  TEST_ASSERT_EQUAL_UINT8(0, f.valid);
  rc::MemSource half(FX_OPENMETEO_WEATHER, strlen(FX_OPENMETEO_WEATHER) / 2);
  TEST_ASSERT_FALSE(rc::omParseWeather(half, g_arena, f, d));
  TEST_ASSERT_EQUAL_UINT32(0, g_arena.used());
}

static void test_parse_forecast_fixture() {
  rc::MemSource src(FX_OPENMETEO_FORECAST, strlen(FX_OPENMETEO_FORECAST));
  rc::Forecast f;
  TEST_ASSERT_TRUE(rc::omParseForecast(src, g_arena, f));
  TEST_ASSERT_EQUAL_UINT8(1, f.valid);
  TEST_ASSERT_EQUAL_UINT32(1790369100u, f.now.time);
  TEST_ASSERT_EQUAL_INT16(154, f.now.temp10);
  TEST_ASSERT_EQUAL_INT16(150, f.now.feels10);
  TEST_ASSERT_EQUAL_UINT8(83, f.now.rh);
  TEST_ASSERT_EQUAL_UINT8(0, f.now.isDay);
  TEST_ASSERT_EQUAL_UINT8(3, f.now.code);
  TEST_ASSERT_EQUAL_UINT16(10193, f.now.press10);
  TEST_ASSERT_EQUAL_UINT16(79, f.now.wind10);
  TEST_ASSERT_EQUAL_UINT16(360, f.now.windDir);
  TEST_ASSERT_EQUAL_UINT16(238, f.now.gust10);
  TEST_ASSERT_EQUAL_UINT16(3080, f.now.visDam);
  TEST_ASSERT_EQUAL_UINT8(4, f.m15.count);
  TEST_ASSERT_EQUAL_UINT32(1790369100u, f.m15.start);
  TEST_ASSERT_EQUAL_UINT16(0, f.m15.precip100[3]);
  TEST_ASSERT_EQUAL_UINT8(24, f.hourCount);
  TEST_ASSERT_EQUAL_UINT32(1790366400u, f.hourStart);
  TEST_ASSERT_EQUAL_INT16(158, f.hours[0].temp10);
  TEST_ASSERT_EQUAL_INT16(246, f.hours[15].temp10);
  TEST_ASSERT_EQUAL_UINT8(1, f.hours[0].code);
  TEST_ASSERT_EQUAL_UINT8(1, f.hours[10].isDay);
  TEST_ASSERT_EQUAL_UINT8(0, f.hours[23].isDay);
  TEST_ASSERT_EQUAL_UINT32(0, g_arena.used());
  TEST_ASSERT_TRUE(g_arena.peak() < 8192);
}

static void test_parse_forecast_nulls() {
  const char* j =
      "{\"current\":{\"time\":100,\"temperature_2m\":-3.46,\"relative_humidity_2m\":null,\"weather_code\":71,"
      "\"is_day\":1},\"minutely_15\":{\"time\":[100,1000],\"precipitation\":[0.3,null]},"
      "\"hourly\":{\"time\":[0,3600],\"temperature_2m\":[-3.4,null],\"weather_code\":[71,73],"
      "\"precipitation_probability\":[null,90],\"is_day\":[1,1]}}";
  rc::MemSource src(j, strlen(j));
  rc::Forecast f;
  TEST_ASSERT_TRUE(rc::omParseForecast(src, g_arena, f));
  TEST_ASSERT_EQUAL_INT16(-35, f.now.temp10);
  TEST_ASSERT_EQUAL_UINT8(rc::NA8, f.now.rh);
  TEST_ASSERT_EQUAL_UINT16(rc::NAU16, f.now.press10);
  TEST_ASSERT_EQUAL_UINT16(30, f.m15.precip100[0]);
  TEST_ASSERT_EQUAL_UINT16(rc::NAU16, f.m15.precip100[1]);
  TEST_ASSERT_EQUAL_INT16(rc::NA16, f.hours[1].temp10);
  TEST_ASSERT_EQUAL_UINT8(rc::NA8, f.hours[0].pop);
  TEST_ASSERT_EQUAL_UINT8(90, f.hours[1].pop);
  TEST_ASSERT_EQUAL_UINT8(2, f.hourCount);
}

static void test_parse_forecast_truncated() {
  size_t half = strlen(FX_OPENMETEO_FORECAST) / 2;
  rc::MemSource src(FX_OPENMETEO_FORECAST, half);
  rc::Forecast f;
  TEST_ASSERT_FALSE(rc::omParseForecast(src, g_arena, f));
  TEST_ASSERT_EQUAL_UINT32(0, g_arena.used());
}

static void test_parse_daily_fixture() {
  rc::MemSource src(FX_OPENMETEO_DAILY, strlen(FX_OPENMETEO_DAILY));
  rc::Daily d;
  TEST_ASSERT_TRUE(rc::omParseDaily(src, g_arena, d));
  TEST_ASSERT_EQUAL_UINT8(7, d.count);
  TEST_ASSERT_EQUAL_UINT32(1790370000u, d.days[0].date);
  TEST_ASSERT_EQUAL_INT16(246, d.days[0].tmax10);
  TEST_ASSERT_EQUAL_INT16(152, d.days[0].tmin10);
  TEST_ASSERT_EQUAL_UINT8(5, d.days[0].popMax);
  TEST_ASSERT_EQUAL_UINT32(1790395278u, d.days[0].sunrise);
  TEST_ASSERT_EQUAL_UINT32(1790438446u, d.days[0].sunset);
  TEST_ASSERT_EQUAL_UINT8(53, d.days[0].uvMax10);
  TEST_ASSERT_EQUAL_UINT8(2, d.days[5].code);
  TEST_ASSERT_EQUAL_INT16(91, d.days[6].tmin10);
}

static void test_parse_air_and_marine() {
  rc::MemSource a(FX_OPENMETEO_AIR, strlen(FX_OPENMETEO_AIR));
  rc::AirQ q;
  TEST_ASSERT_TRUE(rc::omParseAir(a, g_arena, q));
  TEST_ASSERT_EQUAL_UINT16(26, q.eaqi);
  TEST_ASSERT_EQUAL_UINT16(72, q.pm25x10);
  TEST_ASSERT_EQUAL_UINT16(86, q.pm10x10);
  rc::MemSource m(FX_OPENMETEO_MARINE, strlen(FX_OPENMETEO_MARINE));
  rc::Marine s;
  TEST_ASSERT_TRUE(rc::omParseMarine(m, g_arena, s));
  TEST_ASSERT_EQUAL_INT16(217, s.sst10);
  const char* land = "{\"current\":{\"time\":1,\"sea_surface_temperature\":null}}";
  rc::MemSource l(land, strlen(land));
  TEST_ASSERT_FALSE(rc::omParseMarine(l, g_arena, s));
  TEST_ASSERT_EQUAL_INT16(rc::NA16, s.sst10);
}

static void test_small_arena_fails_gracefully() {
  static uint8_t tiny[256];
  rc::JsonArena small(tiny, sizeof(tiny));
  rc::MemSource src(FX_OPENMETEO_FORECAST, strlen(FX_OPENMETEO_FORECAST));
  rc::Forecast f;
  TEST_ASSERT_FALSE(rc::omParseForecast(src, small, f));
  TEST_ASSERT_TRUE(small.failures() > 0);
  TEST_ASSERT_EQUAL_UINT32(0, small.used());
  rc::MemSource whole(FX_OPENMETEO_WEATHER, strlen(FX_OPENMETEO_WEATHER));
  rc::Daily d;
  uint32_t before = small.failures();
  TEST_ASSERT_FALSE(rc::omParseWeather(whole, small, f, d));
  TEST_ASSERT_TRUE(small.failures() > before);
  TEST_ASSERT_EQUAL_UINT32(0, small.used());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_weather_path);
  RUN_TEST(test_parse_weather_fixture);
  RUN_TEST(test_weather_matches_parts);
  RUN_TEST(test_parse_weather_needs_both_parts);
  RUN_TEST(test_parse_forecast_fixture);
  RUN_TEST(test_parse_forecast_nulls);
  RUN_TEST(test_parse_forecast_truncated);
  RUN_TEST(test_parse_daily_fixture);
  RUN_TEST(test_parse_air_and_marine);
  RUN_TEST(test_small_arena_fails_gracefully);
  return UNITY_END();
}
