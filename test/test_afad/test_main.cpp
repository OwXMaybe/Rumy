#include <string.h>
#include <unity.h>

#include "../fixtures/fixtures.h"
#include "afad.h"
#include "timeutil.h"

static uint8_t g_mem[8192];
static rc::JsonArena g_arena(g_mem, sizeof(g_mem));

void setUp() {}
void tearDown() {}

static void test_path_uses_utc_window() {
  char path[400];
  uint32_t now = rc::utcFromCivil(2026, 9, 25, 20, 0, 0);
  size_t n = rc::afadPath(path, sizeof(path), now, 3 * 86400, rc::marmaraBox(), 2.0f);
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_NOT_NULL(strstr(path, "start=2026-09-22T20:00:00"));
  TEST_ASSERT_NOT_NULL(strstr(path, "end=2026-09-25T21:00:00"));
  TEST_ASSERT_NOT_NULL(strstr(path, "minlat=39.50&maxlat=41.80&minlon=26.00&maxlon=30.50"));
  TEST_ASSERT_NOT_NULL(strstr(path, "minmag=2.0"));
  TEST_ASSERT_EQUAL_STRING_LEN("/apigateway/deprem/apiv2/event/filter?", path, 38);
}

static void test_parse_fixture_picks_latest_and_strongest() {
  rc::MemSource src(FX_AFAD_MARMARA, strlen(FX_AFAD_MARMARA));
  rc::QuakeInfo q;
  uint32_t now = rc::utcFromCivil(2026, 8, 26, 18, 0, 0);
  TEST_ASSERT_TRUE(rc::afadParse(src, g_arena, now, q));
  TEST_ASSERT_EQUAL_UINT8(1, q.valid);
  TEST_ASSERT_EQUAL_UINT32(rc::utcFromCivil(2026, 8, 26, 16, 49, 49), q.latest.timeUtc);
  TEST_ASSERT_EQUAL_INT16(21, q.latest.mag10);
  TEST_ASSERT_EQUAL_STRING("Biga (Çanakkale)", q.latest.place);
  TEST_ASSERT_EQUAL_INT16(32, q.strongest.mag10);
  TEST_ASSERT_EQUAL_STRING("Marmara Denizi, Armutlu (Yalova)", q.strongest.place);
  TEST_ASSERT_EQUAL_UINT8(3, q.count24h);
  TEST_ASSERT_EQUAL_INT32(403438, q.latest.lat1e4);
  TEST_ASSERT_EQUAL_INT16(70, q.latest.depth10);
  TEST_ASSERT_EQUAL_UINT32(0, g_arena.used());
}

static void test_empty_array() {
  const char* j = "  [ ]  ";
  rc::MemSource src(j, strlen(j));
  rc::QuakeInfo q;
  TEST_ASSERT_TRUE(rc::afadParse(src, g_arena, 0, q));
  TEST_ASSERT_EQUAL_UINT8(1, q.valid);
  TEST_ASSERT_EQUAL_UINT32(0, q.latest.timeUtc);
}

static void test_numeric_fields_and_big_quake() {
  const char* j =
      "[{\"date\":\"2026-09-25T10:00:00\",\"magnitude\":4.6,\"depth\":10.2,\"latitude\":40.8,\"longitude\":28.1,"
      "\"location\":\"Marmara Denizi - [12.00 km] Silivri (İstanbul)\"},"
      "{\"date\":\"2026-09-25T11:00:00\",\"magnitude\":\"1.9\",\"location\":\"Gemlik (Bursa)\"}]";
  rc::MemSource src(j, strlen(j));
  rc::QuakeInfo q;
  TEST_ASSERT_TRUE(rc::afadParse(src, g_arena, rc::utcFromCivil(2026, 9, 25, 12, 0, 0), q));
  TEST_ASSERT_EQUAL_INT16(46, q.strongest.mag10);
  TEST_ASSERT_EQUAL_STRING("Marmara Denizi, Silivri (İstanbul)", q.strongest.place);
  TEST_ASSERT_EQUAL_INT16(19, q.latest.mag10);
  TEST_ASSERT_EQUAL_INT16(rc::NA16, q.latest.depth10);
}

static void test_old_events_not_counted_for_24h() {
  const char* j = "[{\"date\":\"2026-09-20T10:00:00\",\"magnitude\":\"5.1\",\"location\":\"X\"}]";
  rc::MemSource src(j, strlen(j));
  rc::QuakeInfo q;
  TEST_ASSERT_TRUE(rc::afadParse(src, g_arena, rc::utcFromCivil(2026, 9, 25, 12, 0, 0), q));
  TEST_ASSERT_EQUAL_UINT8(0, q.count24h);
  TEST_ASSERT_EQUAL_INT16(51, q.latest.mag10);
  TEST_ASSERT_EQUAL_UINT32(0, q.strongest.timeUtc);
}

static void test_malformed() {
  const char* j = "[{\"date\":\"2026-09-25T10:00:00\",\"magnitude\":\"2.0\"} {\"x\":1}]";
  rc::MemSource src(j, strlen(j));
  rc::QuakeInfo q;
  TEST_ASSERT_FALSE(rc::afadParse(src, g_arena, 0, q));
  const char* notArray = "{\"error\":\"x\"}";
  rc::MemSource s2(notArray, strlen(notArray));
  TEST_ASSERT_FALSE(rc::afadParse(s2, g_arena, 0, q));
  TEST_ASSERT_EQUAL_UINT32(0, g_arena.used());
}

static void test_utf8_safe_truncation() {
  char out[6];
  rc::utf8Copy(out, sizeof(out), "abcdçe", strlen("abcdçe"));
  TEST_ASSERT_EQUAL_STRING("abcd", out);
  char out2[16];
  rc::afadShortPlace("Gemlik (Bursa)", out2, sizeof(out2));
  TEST_ASSERT_EQUAL_STRING("Gemlik (Bursa)", out2);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_path_uses_utc_window);
  RUN_TEST(test_parse_fixture_picks_latest_and_strongest);
  RUN_TEST(test_empty_array);
  RUN_TEST(test_numeric_fields_and_big_quake);
  RUN_TEST(test_old_events_not_counted_for_24h);
  RUN_TEST(test_malformed);
  RUN_TEST(test_utf8_safe_truncation);
  return UNITY_END();
}
