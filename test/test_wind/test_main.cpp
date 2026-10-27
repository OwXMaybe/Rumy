#include <unity.h>

#include "wind_tr.h"

void setUp() {}
void tearDown() {}

static void test_cardinal_names() {
  TEST_ASSERT_EQUAL_STRING("Yıldız", rc::windNameTr(0));
  TEST_ASSERT_EQUAL_STRING("Yıldız", rc::windNameTr(360));
  TEST_ASSERT_EQUAL_STRING("Poyraz", rc::windNameTr(45));
  TEST_ASSERT_EQUAL_STRING("Gündoğusu", rc::windNameTr(90));
  TEST_ASSERT_EQUAL_STRING("Keşişleme", rc::windNameTr(135));
  TEST_ASSERT_EQUAL_STRING("Kıble", rc::windNameTr(180));
  TEST_ASSERT_EQUAL_STRING("Lodos", rc::windNameTr(225));
  TEST_ASSERT_EQUAL_STRING("Günbatısı", rc::windNameTr(270));
  TEST_ASSERT_EQUAL_STRING("Karayel", rc::windNameTr(315));
}

static void test_sector_boundaries() {
  TEST_ASSERT_EQUAL_UINT8(0, rc::windSector8(22));
  TEST_ASSERT_EQUAL_UINT8(1, rc::windSector8(23));
  TEST_ASSERT_EQUAL_UINT8(7, rc::windSector8(337));
  TEST_ASSERT_EQUAL_UINT8(0, rc::windSector8(338));
  TEST_ASSERT_EQUAL_UINT8(5, rc::windSector8(247));
  TEST_ASSERT_EQUAL_UINT8(6, rc::windSector8(248));
  TEST_ASSERT_EQUAL_UINT8(0, rc::windSector8(720));
}

static void test_abbreviations() {
  TEST_ASSERT_EQUAL_STRING("KD", rc::windAbbrTr(40));
  TEST_ASSERT_EQUAL_STRING("GB", rc::windAbbrTr(210));
  TEST_ASSERT_EQUAL_STRING("KB", rc::windAbbrTr(300));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_cardinal_names);
  RUN_TEST(test_sector_boundaries);
  RUN_TEST(test_abbreviations);
  return UNITY_END();
}
