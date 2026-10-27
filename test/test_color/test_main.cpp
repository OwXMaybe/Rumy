#include <stdlib.h>
#include <unity.h>

#include "color.h"
#include "wiz_proto.h"

void setUp() {}
void tearDown() {}

static void test_primary_hues() {
  rc::Rgb c = rc::hsvToRgb(0, 255, 255);
  TEST_ASSERT_EQUAL_UINT8(255, c.r);
  TEST_ASSERT_EQUAL_UINT8(0, c.g);
  TEST_ASSERT_EQUAL_UINT8(0, c.b);
  c = rc::hsvToRgb(120, 255, 255);
  TEST_ASSERT_EQUAL_UINT8(0, c.r);
  TEST_ASSERT_EQUAL_UINT8(255, c.g);
  c = rc::hsvToRgb(240, 255, 255);
  TEST_ASSERT_EQUAL_UINT8(255, c.b);
  TEST_ASSERT_EQUAL_UINT8(0, c.r);
  c = rc::hsvToRgb(360 + 60, 255, 255);
  TEST_ASSERT_EQUAL_UINT8(255, c.r);
  TEST_ASSERT_EQUAL_UINT8(255, c.g);
}

static void test_hue_roundtrip() {
  for (int h = 0; h < 360; h += 7) {
    rc::Rgb c = rc::hsvToRgb(static_cast<uint16_t>(h), 255, 255);
    int back = rc::rgbToHue(c.r, c.g, c.b);
    int diff = abs(back - h);
    if (diff > 180) diff = 360 - diff;
    TEST_ASSERT_TRUE_MESSAGE(diff <= 2, "hue roundtrip");
  }
}

static void test_gray_has_no_hue() {
  TEST_ASSERT_EQUAL_UINT16(0, rc::rgbToHue(128, 128, 128));
  TEST_ASSERT_EQUAL_UINT8(0, rc::rgbToSat(128, 128, 128));
  TEST_ASSERT_EQUAL_UINT8(255, rc::rgbToSat(255, 0, 0));
}

static void test_kelvin_endpoints_and_monotonic_blue() {
  rc::Rgb warm = rc::kelvinToRgb(2200);
  rc::Rgb cool = rc::kelvinToRgb(6500);
  TEST_ASSERT_EQUAL_UINT8(255, warm.r);
  TEST_ASSERT_TRUE(warm.b < 80);
  TEST_ASSERT_TRUE(cool.b > 240);
  int prev = -1;
  for (int k = 2200; k <= 6500; k += 100) {
    int b = rc::kelvinToRgb(static_cast<uint16_t>(k)).b;
    TEST_ASSERT_TRUE(b >= prev);
    prev = b;
  }
  rc::Rgb below = rc::kelvinToRgb(1000);
  TEST_ASSERT_EQUAL_UINT8(warm.g, below.g);
}

static void test_rgb565() {
  TEST_ASSERT_EQUAL_HEX16(0xF800, rc::rgb565(255, 0, 0));
  TEST_ASSERT_EQUAL_HEX16(0x07E0, rc::rgb565(0, 255, 0));
  TEST_ASSERT_EQUAL_HEX16(0x001F, rc::rgb565(0, 0, 255));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, rc::rgb565(255, 255, 255));
}

static void test_pilot_color() {
  rc::Pilot p = rc::pilotTemp(2200, 50);
  rc::Rgb c = rc::pilotColor(p);
  TEST_ASSERT_EQUAL_UINT8(255, c.r);
  rc::Pilot off = rc::pilotOff(p);
  rc::Rgb g = rc::pilotColor(off);
  TEST_ASSERT_TRUE(g.r < 100 && g.g < 100);
  TEST_ASSERT_EQUAL_HEX32(0xFF8000, rc::rgbHex({255, 128, 0}));
}

static void test_mix_and_scale() {
  rc::Rgb a{0, 0, 0}, b{200, 100, 50};
  rc::Rgb m = rc::mixRgb(a, b, 255);
  TEST_ASSERT_EQUAL_UINT8(200, m.r);
  m = rc::mixRgb(a, b, 0);
  TEST_ASSERT_EQUAL_UINT8(0, m.r);
  rc::Rgb s = rc::scaleRgb(b, 50);
  TEST_ASSERT_EQUAL_UINT8(100, s.r);
  TEST_ASSERT_EQUAL_UINT8(25, s.b);
}

static void near(uint32_t want, uint32_t got, int slack) {
  for (int sh = 0; sh < 24; sh += 8) {
    int a = static_cast<int>((want >> sh) & 255);
    int b = static_cast<int>((got >> sh) & 255);
    TEST_ASSERT_INT_WITHIN(slack, a, b);
  }
}

static void test_palette_default_is_todays_look() {
  rc::TeamPalette p = rc::teamPalette(0, 0);
  TEST_ASSERT_EQUAL_HEX32(0xFFDE00, p.accent);
  TEST_ASSERT_EQUAL_HEX32(0x0A1E50, p.bg);
  TEST_ASSERT_EQUAL_HEX32(0x061234, p.deep);
  TEST_ASSERT_EQUAL_HEX32(0x0E2660, p.stripe);
  TEST_ASSERT_EQUAL_UINT8(255, p.bulbA.r);
  TEST_ASSERT_EQUAL_UINT8(255, p.bulbB.b);
}

static void test_palette_fenerbahce_close_to_default() {
  rc::TeamPalette d = rc::teamPalette(0, 0);
  rc::TeamPalette p = rc::teamPalette(0x13235B, 0xFFD60A);
  near(d.accent, p.accent, 12);
  near(d.bg, p.bg, 8);
  near(d.deep, p.deep, 8);
  near(d.stripe, p.stripe, 8);
}

static void test_palette_light_first_color_uses_second_as_background() {
  rc::TeamPalette p = rc::teamPalette(0xFFFFFF, 0x00529F);
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFF, p.accent);
  TEST_ASSERT_TRUE((p.bg & 0xFF) > ((p.bg >> 16) & 0xFF));
  TEST_ASSERT_TRUE(rc::contrastRatio(p.accent, p.bg) >= 3.0f);
}

static void test_palette_black_white_is_neutral() {
  rc::TeamPalette p = rc::teamPalette(0x111111, 0xFFFFFF);
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFF, p.accent);
  TEST_ASSERT_EQUAL_HEX32(0x1A1A1E, p.bg);
  TEST_ASSERT_FALSE(p.bulbA.r == p.bulbB.r && p.bulbA.g == p.bulbB.g && p.bulbA.b == p.bulbB.b);
}

static void test_palette_unreadable_second_color_falls_back_to_white() {
  rc::TeamPalette p = rc::teamPalette(0xE20520, 0x003399);
  TEST_ASSERT_EQUAL_HEX32(0xFFFFFF, p.accent);
  TEST_ASSERT_EQUAL_UINT8(255, p.bulbB.r);
  TEST_ASSERT_EQUAL_UINT8(255, p.bulbA.b);
}

static void test_palette_always_readable() {
  const uint32_t cs[] = {0x000000, 0xFFFFFF, 0xFFEE00, 0x272726, 0xA50044, 0x004D98, 0x7FC6EF, 0x2C323B, 0xFF0000, 0x00FF00};
  for (uint32_t a : cs) {
    for (uint32_t b : cs) {
      if (!a && !b) continue;
      rc::TeamPalette p = rc::teamPalette(a, b);
      TEST_ASSERT_TRUE(rc::contrastRatio(p.accent, p.bg) >= 3.0f);
      TEST_ASSERT_TRUE(rc::contrastRatio(p.accent, 0x000000) >= 3.0f);
      TEST_ASSERT_TRUE(((p.bg >> 16) & 0xFF) < 0x60 && ((p.bg >> 8) & 0xFF) < 0x60 && (p.bg & 0xFF) < 0x60);
    }
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_primary_hues);
  RUN_TEST(test_hue_roundtrip);
  RUN_TEST(test_gray_has_no_hue);
  RUN_TEST(test_kelvin_endpoints_and_monotonic_blue);
  RUN_TEST(test_rgb565);
  RUN_TEST(test_pilot_color);
  RUN_TEST(test_mix_and_scale);
  RUN_TEST(test_palette_default_is_todays_look);
  RUN_TEST(test_palette_fenerbahce_close_to_default);
  RUN_TEST(test_palette_light_first_color_uses_second_as_background);
  RUN_TEST(test_palette_black_white_is_neutral);
  RUN_TEST(test_palette_unreadable_second_color_falls_back_to_white);
  RUN_TEST(test_palette_always_readable);
  return UNITY_END();
}
