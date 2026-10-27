#include <unity.h>

#include "phone_app.h"

void setUp() {}
void tearDown() {}

static void test_sleep_before_end_same_night() {
  uint32_t now = 1000000;
  uint32_t until = rc::sleepUntilUtc(now, 1 * 60 + 30, 15, 7 * 60);
  TEST_ASSERT_EQUAL_UINT32(now + (5 * 60 + 30) * 60 - 15, until);
}

static void test_sleep_evening_until_morning() {
  uint32_t now = 2000000;
  uint32_t until = rc::sleepUntilUtc(now, 23 * 60, 0, 7 * 60);
  TEST_ASSERT_EQUAL_UINT32(now + 8 * 3600, until);
}

static void test_sleep_at_end_minute_is_full_day() {
  uint32_t now = 3000000;
  TEST_ASSERT_EQUAL_UINT32(now + 1440 * 60 - 5, rc::sleepUntilUtc(now, 7 * 60, 5, 7 * 60));
}

static void test_sleep_active() {
  TEST_ASSERT_FALSE(rc::sleepActive(0, 100));
  TEST_ASSERT_TRUE(rc::sleepActive(200, 100));
  TEST_ASSERT_FALSE(rc::sleepActive(200, 200));
}

static void test_token_format() {
  TEST_ASSERT_TRUE(rc::phoneTokenWellFormed("0123456789abcdef0123456789abcdef"));
  TEST_ASSERT_FALSE(rc::phoneTokenWellFormed("0123456789ABCDEF0123456789abcdef"));
  TEST_ASSERT_FALSE(rc::phoneTokenWellFormed("0123456789abcdef0123456789abcde"));
  TEST_ASSERT_FALSE(rc::phoneTokenWellFormed("0123456789abcdef0123456789abcdef0"));
  TEST_ASSERT_FALSE(rc::phoneTokenWellFormed(nullptr));
}

static void test_token_equal() {
  TEST_ASSERT_TRUE(rc::phoneTokenEqual("0123456789abcdef0123456789abcdef", "0123456789abcdef0123456789abcdef"));
  TEST_ASSERT_FALSE(rc::phoneTokenEqual("0123456789abcdef0123456789abcdef", "0123456789abcdef0123456789abcdee"));
  TEST_ASSERT_FALSE(rc::phoneTokenEqual("", ""));
}

static void test_pair_code() {
  uint16_t v = 0;
  TEST_ASSERT_TRUE(rc::pairCodeParse("7305", v));
  TEST_ASSERT_EQUAL_UINT16(7305, v);
  TEST_ASSERT_FALSE(rc::pairCodeParse("0305", v));
  TEST_ASSERT_FALSE(rc::pairCodeParse("730", v));
  TEST_ASSERT_FALSE(rc::pairCodeParse("73051", v));
  TEST_ASSERT_FALSE(rc::pairCodeParse("73a5", v));
}

static void test_hex_color() {
  uint32_t c = 0;
  TEST_ASSERT_TRUE(rc::hexColorParse("13235b", c));
  TEST_ASSERT_EQUAL_HEX32(0x13235B, c);
  TEST_ASSERT_TRUE(rc::hexColorParse("FFD60A", c));
  TEST_ASSERT_EQUAL_HEX32(0xFFD60A, c);
  c = 7;
  TEST_ASSERT_FALSE(rc::hexColorParse("#13235b", c));
  TEST_ASSERT_FALSE(rc::hexColorParse("13235", c));
  TEST_ASSERT_FALSE(rc::hexColorParse("13235bb", c));
  TEST_ASSERT_FALSE(rc::hexColorParse("1323g5", c));
  TEST_ASSERT_FALSE(rc::hexColorParse(nullptr, c));
  TEST_ASSERT_EQUAL_UINT32(7, c);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_sleep_before_end_same_night);
  RUN_TEST(test_sleep_evening_until_morning);
  RUN_TEST(test_sleep_at_end_minute_is_full_day);
  RUN_TEST(test_sleep_active);
  RUN_TEST(test_token_format);
  RUN_TEST(test_token_equal);
  RUN_TEST(test_pair_code);
  RUN_TEST(test_hex_color);
  return UNITY_END();
}
