#include <string.h>
#include <unity.h>

#include "cache_fmt.h"
#include "openmeteo.h"

void setUp() {}
void tearDown() {}

static void test_crc32_known_vector() {
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, rc::crc32("123456789", 9));
  TEST_ASSERT_EQUAL_HEX32(0x00000000u, rc::crc32("", 0));
}

static void test_pack_unpack_roundtrip() {
  rc::Forecast f;
  rc::clearForecast(f);
  f.valid = 1;
  f.now.temp10 = 217;
  f.hours[5].pop = 40;
  static uint8_t buf[sizeof(rc::BlobHeader) + sizeof(rc::Forecast)];
  size_t n = rc::blobPack(rc::BLOB_FORECAST, 3, &f, sizeof(f), buf, sizeof(buf));
  TEST_ASSERT_EQUAL_UINT32(sizeof(buf), n);
  rc::Forecast back;
  TEST_ASSERT_TRUE(rc::blobUnpackInto(buf, n, rc::BLOB_FORECAST, 3, &back, sizeof(back)));
  TEST_ASSERT_EQUAL_INT16(217, back.now.temp10);
  TEST_ASSERT_EQUAL_UINT8(40, back.hours[5].pop);
  TEST_ASSERT_EQUAL_MEMORY(&f, &back, sizeof(f));
}

static void test_corruption_detected() {
  uint32_t payload[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  uint8_t buf[64];
  size_t n = rc::blobPack(rc::BLOB_AIR, 1, payload, sizeof(payload), buf, sizeof(buf));
  TEST_ASSERT_TRUE(n > 0);
  buf[20] ^= 0x01;
  uint32_t back[8];
  TEST_ASSERT_FALSE(rc::blobUnpackInto(buf, n, rc::BLOB_AIR, 1, back, sizeof(back)));
  buf[20] ^= 0x01;
  TEST_ASSERT_TRUE(rc::blobUnpackInto(buf, n, rc::BLOB_AIR, 1, back, sizeof(back)));
  TEST_ASSERT_FALSE(rc::blobUnpackInto(buf, n - 1, rc::BLOB_AIR, 1, back, sizeof(back)));
}

static void test_version_kind_and_size_mismatch() {
  uint16_t v = 7;
  uint8_t buf[32];
  size_t n = rc::blobPack(rc::BLOB_MARINE, 2, &v, sizeof(v), buf, sizeof(buf));
  uint16_t out = 0;
  TEST_ASSERT_FALSE(rc::blobUnpackInto(buf, n, rc::BLOB_MARINE, 3, &out, sizeof(out)));
  TEST_ASSERT_FALSE(rc::blobUnpackInto(buf, n, rc::BLOB_AIR, 2, &out, sizeof(out)));
  uint32_t wrongSize = 0;
  TEST_ASSERT_FALSE(rc::blobUnpackInto(buf, n, rc::BLOB_MARINE, 2, &wrongSize, sizeof(wrongSize)));
  TEST_ASSERT_TRUE(rc::blobUnpackInto(buf, n, rc::BLOB_MARINE, 2, &out, sizeof(out)));
  TEST_ASSERT_EQUAL_UINT16(7, out);
  TEST_ASSERT_EQUAL_UINT32(0, rc::blobPack(rc::BLOB_MARINE, 2, &v, sizeof(v), buf, 10));
}

static void test_change_gate_skips_identical_writes() {
  rc::ChangeGate g;
  int a[4] = {1, 2, 3, 4};
  TEST_ASSERT_TRUE(g.changed(a, sizeof(a)));
  g.remember(a, sizeof(a));
  TEST_ASSERT_FALSE(g.changed(a, sizeof(a)));
  a[2] = 9;
  TEST_ASSERT_TRUE(g.changed(a, sizeof(a)));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_crc32_known_vector);
  RUN_TEST(test_pack_unpack_roundtrip);
  RUN_TEST(test_corruption_detected);
  RUN_TEST(test_version_kind_and_size_mismatch);
  RUN_TEST(test_change_gate_skips_identical_writes);
  return UNITY_END();
}
