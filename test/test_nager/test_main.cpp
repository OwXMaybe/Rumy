#include <string.h>
#include <unity.h>

#include "../fixtures/fixtures.h"
#include "nager.h"

static uint8_t g_mem[8192];
static rc::JsonArena g_arena(g_mem, sizeof(g_mem));

void setUp() {}
void tearDown() {}

static void test_path() {
  char p[64];
  rc::nagerPath(p, sizeof(p), 2027);
  TEST_ASSERT_EQUAL_STRING("/api/v3/PublicHolidays/2027/TR", p);
}

static void test_parse_2026() {
  rc::MemSource src(FX_NAGER_2026, strlen(FX_NAGER_2026));
  rc::Religious r;
  TEST_ASSERT_TRUE(rc::nagerParse(src, g_arena, 2026, r));
  TEST_ASSERT_EQUAL_UINT8(3, r.ramazanMonth);
  TEST_ASSERT_EQUAL_UINT8(20, r.ramazanDay);
  TEST_ASSERT_EQUAL_UINT8(5, r.kurbanMonth);
  TEST_ASSERT_EQUAL_UINT8(27, r.kurbanDay);
  TEST_ASSERT_EQUAL_UINT8(rc::REL_NAGER, r.source);
  TEST_ASSERT_EQUAL_UINT32(0, g_arena.used());
}

static void test_tentative_prefers_diyanet_table() {
  rc::MemSource src(FX_NAGER_2027, strlen(FX_NAGER_2027));
  rc::Religious r;
  TEST_ASSERT_TRUE(rc::nagerParse(src, g_arena, 2027, r));
  TEST_ASSERT_EQUAL_UINT8(rc::REL_NAGER_TENTATIVE, r.source);
  TEST_ASSERT_EQUAL_UINT8(10, r.ramazanDay);
  rc::Religious best = rc::religiousBest(2027, &r);
  TEST_ASSERT_EQUAL_UINT8(rc::REL_TABLE, best.source);
  TEST_ASSERT_EQUAL_UINT8(9, best.ramazanDay);
  TEST_ASSERT_EQUAL_UINT8(16, best.kurbanDay);
}

static void test_table_fallback() {
  rc::Religious best = rc::religiousBest(2029, nullptr);
  TEST_ASSERT_EQUAL_UINT8(1, best.valid);
  TEST_ASSERT_EQUAL_UINT8(2, best.ramazanMonth);
  TEST_ASSERT_EQUAL_UINT8(14, best.ramazanDay);
  TEST_ASSERT_EQUAL_UINT8(4, best.kurbanMonth);
  TEST_ASSERT_EQUAL_UINT8(24, best.kurbanDay);
  rc::Religious b30 = rc::religiousBest(2030, nullptr);
  TEST_ASSERT_EQUAL_UINT8(4, b30.ramazanDay);
  TEST_ASSERT_EQUAL_UINT8(13, b30.kurbanDay);
  rc::Religious none = rc::religiousBest(2040, nullptr);
  TEST_ASSERT_EQUAL_UINT8(0, none.valid);
}

static void test_nager_beyond_table_is_used() {
  const char* j =
      "[{\"date\":\"2031-01-24\",\"localName\":\"Ramazan Bayramı 1. Gün\",\"name\":\"Eid al-Fitr\"},"
      "{\"date\":\"2031-04-02\",\"localName\":\"Kurban Bayramı 1. Gün\",\"name\":\"Eid al-Adha\"}]";
  rc::MemSource src(j, strlen(j));
  rc::Religious r;
  TEST_ASSERT_TRUE(rc::nagerParse(src, g_arena, 2031, r));
  rc::Religious best = rc::religiousBest(2031, &r);
  TEST_ASSERT_EQUAL_UINT8(rc::REL_NAGER, best.source);
  TEST_ASSERT_EQUAL_UINT8(24, best.ramazanDay);
}

static void test_missing_bayram_invalid() {
  const char* j = "[{\"date\":\"2026-01-01\",\"localName\":\"Yılbaşı\",\"name\":\"New Year's Day\"}]";
  rc::MemSource src(j, strlen(j));
  rc::Religious r;
  TEST_ASSERT_FALSE(rc::nagerParse(src, g_arena, 2026, r));
  rc::Religious best = rc::religiousBest(2026, &r);
  TEST_ASSERT_EQUAL_UINT8(rc::REL_TABLE, best.source);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_path);
  RUN_TEST(test_parse_2026);
  RUN_TEST(test_tentative_prefers_diyanet_table);
  RUN_TEST(test_table_fallback);
  RUN_TEST(test_nager_beyond_table_is_used);
  RUN_TEST(test_missing_bayram_invalid);
  return UNITY_END();
}
