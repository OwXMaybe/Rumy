#include <string.h>
#include <unity.h>

#include "wx_mood.h"

void setUp() {}
void tearDown() {}

static rc::WxNow mild(uint8_t code, uint8_t isDay = 1) {
  rc::WxNow n;
  memset(&n, 0, sizeof(n));
  n.code = code;
  n.isDay = isDay;
  n.temp10 = 180;
  n.feels10 = 175;
  n.wind10 = 120;
  n.gust10 = 250;
  n.uv10 = 30;
  return n;
}

static void test_precipitation_codes() {
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_STORM, rc::wxKind(mild(95)));
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_STORM, rc::wxKind(mild(99)));
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_SNOW, rc::wxKind(mild(71)));
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_SNOW, rc::wxKind(mild(77)));
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_SNOW, rc::wxKind(mild(86)));
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_RAIN, rc::wxKind(mild(61)));
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_RAIN, rc::wxKind(mild(67)));
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_RAIN, rc::wxKind(mild(81)));
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_DRIZZLE, rc::wxKind(mild(51)));
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_DRIZZLE, rc::wxKind(mild(57)));
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_FOG, rc::wxKind(mild(45)));
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_FOG, rc::wxKind(mild(48)));
}

static void test_precipitation_beats_wind_and_temperature() {
  rc::WxNow n = mild(63);
  n.gust10 = 800;
  n.feels10 = -50;
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_RAIN, rc::wxKind(n));
  n.code = 73;
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_SNOW, rc::wxKind(n));
}

static void test_wind_thresholds() {
  rc::WxNow n = mild(2);
  n.gust10 = 499;
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_NONE, rc::wxKind(n));
  n.gust10 = 500;
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_WINDY, rc::wxKind(n));
  n.gust10 = rc::NAU16;
  n.wind10 = 350;
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_WINDY, rc::wxKind(n));
  n.wind10 = rc::NAU16;
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_NONE, rc::wxKind(n));
}

static void test_temperature_uses_feels_then_temp() {
  rc::WxNow n = mild(2);
  n.feels10 = 20;
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_COLD, rc::wxKind(n));
  n.feels10 = 21;
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_NONE, rc::wxKind(n));
  n.feels10 = 330;
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_HOT, rc::wxKind(n));
  n.feels10 = rc::NA16;
  n.temp10 = 10;
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_COLD, rc::wxKind(n));
  n.temp10 = rc::NA16;
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_NONE, rc::wxKind(n));
}

static void test_clear_sky_by_day_and_night() {
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_SUNNY, rc::wxKind(mild(0)));
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_SUNNY, rc::wxKind(mild(1)));
  rc::WxNow n = mild(0);
  n.feels10 = 270;
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_SUNNY_HOT, rc::wxKind(n));
  n = mild(1);
  n.uv10 = 60;
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_SUNNY_HOT, rc::wxKind(n));
  n.uv10 = rc::NA8;
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_SUNNY, rc::wxKind(n));
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_CLEAR_NIGHT, rc::wxKind(mild(0, 0)));
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_CLEAR_NIGHT, rc::wxKind(mild(1, 0)));
}

static void test_clouds_and_unknown() {
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_NONE, rc::wxKind(mild(2)));
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_CLOUDY, rc::wxKind(mild(3)));
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_CLOUDY, rc::wxKind(mild(3, 0)));
  TEST_ASSERT_EQUAL_UINT8(rc::WXK_NONE, rc::wxKind(mild(rc::NA8)));
}

static void test_wet_and_names() {
  TEST_ASSERT_TRUE(rc::wxKindWet(rc::WXK_DRIZZLE));
  TEST_ASSERT_TRUE(rc::wxKindWet(rc::WXK_RAIN));
  TEST_ASSERT_TRUE(rc::wxKindWet(rc::WXK_STORM));
  TEST_ASSERT_FALSE(rc::wxKindWet(rc::WXK_SNOW));
  TEST_ASSERT_FALSE(rc::wxKindWet(rc::WXK_FOG));
  TEST_ASSERT_EQUAL_STRING("karlı", rc::wxKindName(rc::WXK_SNOW));
  TEST_ASSERT_EQUAL_STRING("açık gece", rc::wxKindName(rc::WXK_CLEAR_NIGHT));
  TEST_ASSERT_EQUAL_STRING("?", rc::wxKindName(rc::WXK_COUNT));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_precipitation_codes);
  RUN_TEST(test_precipitation_beats_wind_and_temperature);
  RUN_TEST(test_wind_thresholds);
  RUN_TEST(test_temperature_uses_feels_then_temp);
  RUN_TEST(test_clear_sky_by_day_and_night);
  RUN_TEST(test_clouds_and_unknown);
  RUN_TEST(test_wet_and_names);
  return UNITY_END();
}
