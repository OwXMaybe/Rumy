#include <unity.h>

#include "pairing.h"

void setUp() {}
void tearDown() {}

static void test_mac_normalize() {
  char out[13];
  TEST_ASSERT_TRUE(rc::macNormalize("A8:BB:50:E1:C3:F2", out));
  TEST_ASSERT_EQUAL_STRING("a8bb50e1c3f2", out);
  TEST_ASSERT_TRUE(rc::macNormalize("a8bb50e1c3f2", out));
  TEST_ASSERT_FALSE(rc::macNormalize("a8bb50e1c3", out));
  TEST_ASSERT_FALSE(rc::macNormalize("a8bb50e1c3f2ff", out));
  TEST_ASSERT_FALSE(rc::macNormalize("zzbb50e1c3f2", out));
  TEST_ASSERT_FALSE(rc::macNormalize(nullptr, out));
  TEST_ASSERT_TRUE(rc::macEqual("A8BB50E1C3F2", "a8-bb-50-e1-c3-f2"));
}

static void test_list_dedupes_and_updates_ip() {
  rc::BulbList l;
  TEST_ASSERT_EQUAL_INT(0, l.add(0x0A00000A, "a8bb50e1c3f2", 100));
  TEST_ASSERT_EQUAL_INT(1, l.add(0x0B00000A, "a8bb50000001", 110));
  TEST_ASSERT_EQUAL_INT(0, l.add(0x0C00000A, "A8BB50E1C3F2", 120));
  TEST_ASSERT_EQUAL_INT(2, l.count());
  TEST_ASSERT_EQUAL_HEX32(0x0C00000A, l.at(0).ip);
  TEST_ASSERT_EQUAL_INT(1, l.find("a8:bb:50:00:00:01"));
  TEST_ASSERT_EQUAL_INT(-1, l.find("a8bb50000009"));
  TEST_ASSERT_EQUAL_INT(-1, l.add(1, "bad", 0));
}

static void test_list_capacity() {
  rc::BulbList l;
  char mac[13] = "a8bb5000000a";
  for (int i = 0; i < rc::BulbList::CAP + 3; i++) {
    mac[11] = "0123456789abcdef"[i % 16];
    mac[10] = "0123456789abcdef"[i / 16];
    l.add(static_cast<uint32_t>(i), mac, 0);
  }
  TEST_ASSERT_EQUAL_INT(rc::BulbList::CAP, l.count());
}

static void test_throttle_80ms() {
  rc::SendThrottle t(80);
  rc::Pilot a = rc::pilotRgb(10, 50);
  TEST_ASSERT_TRUE(t.shouldSend(1000, a, false));
  t.markSent(1000, a);
  rc::Pilot b = rc::pilotRgb(20, 50);
  TEST_ASSERT_FALSE(t.shouldSend(1040, b, false));
  TEST_ASSERT_TRUE(t.shouldSend(1080, b, false));
  TEST_ASSERT_TRUE(t.shouldSend(1040, b, true));
  TEST_ASSERT_FALSE(t.shouldSend(5000, a, true));
  TEST_ASSERT_FALSE(t.shouldSend(5000, a, false));
}

static void test_sync_suppression() {
  TEST_ASSERT_FALSE(rc::syncAllowed(5000, true, 0, false));
  TEST_ASSERT_TRUE(rc::syncAllowed(5000, false, 0, false));
  TEST_ASSERT_FALSE(rc::syncAllowed(5500, false, 5000, true));
  TEST_ASSERT_TRUE(rc::syncAllowed(6000, false, 5000, true));
  TEST_ASSERT_FALSE(rc::syncAllowed(10, false, 4294967000u, true));
  TEST_ASSERT_TRUE(rc::syncAllowed(800, false, 4294967000u, true));
}

static void test_online_tracker() {
  rc::OnlineTracker o(1500, 2);
  TEST_ASSERT_FALSE(o.online());
  o.onRequest(0);
  o.onReply(200);
  TEST_ASSERT_TRUE(o.online());
  o.onRequest(5000);
  o.tick(6600);
  TEST_ASSERT_TRUE(o.online());
  o.onRequest(10000);
  o.tick(11600);
  TEST_ASSERT_FALSE(o.online());
  o.onRequest(15000);
  o.onReply(15100);
  TEST_ASSERT_TRUE(o.online());
  TEST_ASSERT_EQUAL_UINT8(0, o.misses());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_mac_normalize);
  RUN_TEST(test_list_dedupes_and_updates_ip);
  RUN_TEST(test_list_capacity);
  RUN_TEST(test_throttle_80ms);
  RUN_TEST(test_sync_suppression);
  RUN_TEST(test_online_tracker);
  return UNITY_END();
}
