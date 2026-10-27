#include "color.h"

#include <math.h>

namespace rc {

Rgb hsvToRgb(uint16_t hue, uint8_t sat, uint8_t val) {
  hue %= 360;
  uint32_t region = hue / 60;
  uint32_t rem = (hue % 60) * 255 / 60;
  uint32_t p = (static_cast<uint32_t>(val) * (255 - sat)) / 255;
  uint32_t q = (static_cast<uint32_t>(val) * (255 - (sat * rem) / 255)) / 255;
  uint32_t t = (static_cast<uint32_t>(val) * (255 - (sat * (255 - rem)) / 255)) / 255;
  Rgb c{0, 0, 0};
  switch (region) {
    case 0: c = {val, static_cast<uint8_t>(t), static_cast<uint8_t>(p)}; break;
    case 1: c = {static_cast<uint8_t>(q), val, static_cast<uint8_t>(p)}; break;
    case 2: c = {static_cast<uint8_t>(p), val, static_cast<uint8_t>(t)}; break;
    case 3: c = {static_cast<uint8_t>(p), static_cast<uint8_t>(q), val}; break;
    case 4: c = {static_cast<uint8_t>(t), static_cast<uint8_t>(p), val}; break;
    default: c = {val, static_cast<uint8_t>(p), static_cast<uint8_t>(q)}; break;
  }
  return c;
}

uint16_t rgbToHue(uint8_t r, uint8_t g, uint8_t b) {
  int mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
  int mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
  int d = mx - mn;
  if (d == 0) return 0;
  int h;
  if (mx == r) {
    h = (60 * (g - b)) / d;
    if (h < 0) h += 360;
  } else if (mx == g) {
    h = 120 + (60 * (b - r)) / d;
  } else {
    h = 240 + (60 * (r - g)) / d;
  }
  if (h < 0) h += 360;
  return static_cast<uint16_t>(h % 360);
}

uint8_t rgbToSat(uint8_t r, uint8_t g, uint8_t b) {
  int mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
  int mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
  if (mx == 0) return 0;
  return static_cast<uint8_t>(((mx - mn) * 255) / mx);
}

namespace {

struct KelvinPoint {
  uint16_t k;
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

constexpr KelvinPoint kKelvin[] = {
    {2200, 255, 147, 44},  {2700, 255, 169, 87},  {3000, 255, 180, 107},
    {4000, 255, 209, 163}, {5000, 255, 228, 206}, {6500, 214, 232, 255},
};

uint8_t lerp8(uint8_t a, uint8_t b, uint32_t num, uint32_t den) {
  int32_t d = static_cast<int32_t>(b) - static_cast<int32_t>(a);
  return static_cast<uint8_t>(a + (d * static_cast<int32_t>(num)) / static_cast<int32_t>(den));
}

}

Rgb kelvinToRgb(uint16_t kelvin) {
  const int n = sizeof(kKelvin) / sizeof(kKelvin[0]);
  if (kelvin <= kKelvin[0].k) return {kKelvin[0].r, kKelvin[0].g, kKelvin[0].b};
  if (kelvin >= kKelvin[n - 1].k) return {kKelvin[n - 1].r, kKelvin[n - 1].g, kKelvin[n - 1].b};
  for (int i = 1; i < n; i++) {
    if (kelvin <= kKelvin[i].k) {
      const KelvinPoint& a = kKelvin[i - 1];
      const KelvinPoint& b = kKelvin[i];
      uint32_t num = kelvin - a.k;
      uint32_t den = b.k - a.k;
      return {lerp8(a.r, b.r, num, den), lerp8(a.g, b.g, num, den), lerp8(a.b, b.b, num, den)};
    }
  }
  return {255, 255, 255};
}

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

uint32_t rgbHex(Rgb c) {
  return (static_cast<uint32_t>(c.r) << 16) | (static_cast<uint32_t>(c.g) << 8) | c.b;
}

Rgb scaleRgb(Rgb c, uint8_t percent) {
  if (percent > 100) percent = 100;
  return {static_cast<uint8_t>(c.r * percent / 100), static_cast<uint8_t>(c.g * percent / 100),
          static_cast<uint8_t>(c.b * percent / 100)};
}

Rgb mixRgb(Rgb a, Rgb b, uint8_t amount) {
  return {lerp8(a.r, b.r, amount, 255), lerp8(a.g, b.g, amount, 255), lerp8(a.b, b.b, amount, 255)};
}

namespace {

constexpr TeamPalette kDefaultPalette = {0xFFDE00, 0x0A1E50, 0x061234, 0x0E2660,
                                         0xFFF08A, 0x7FB8FF, {255, 200, 0}, {0, 20, 255}};
constexpr uint8_t kGraySat = 48;
constexpr float kLight = 0.30f;
constexpr float kMinContrast = 3.0f;

Rgb unpack(uint32_t c) {
  return {static_cast<uint8_t>(c >> 16), static_cast<uint8_t>(c >> 8), static_cast<uint8_t>(c)};
}

float channel(uint8_t v) {
  float c = static_cast<float>(v) / 255.0f;
  return c <= 0.03928f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
}

float luminance(uint32_t c) {
  Rgb x = unpack(c);
  return 0.2126f * channel(x.r) + 0.7152f * channel(x.g) + 0.0722f * channel(x.b);
}

uint8_t satOf(uint32_t c) {
  Rgb x = unpack(c);
  return rgbToSat(x.r, x.g, x.b);
}

uint16_t hueOf(uint32_t c) {
  Rgb x = unpack(c);
  return rgbToHue(x.r, x.g, x.b);
}

uint32_t tone(uint32_t c, uint8_t val) {
  uint8_t s = satOf(c);
  return rgbHex(hsvToRgb(hueOf(c), s > 230 ? 230 : s, val));
}

bool readable(uint32_t c, uint32_t bg) { return luminance(c) > 0.05f && contrastRatio(c, bg) >= kMinContrast; }

Rgb bulbOf(uint32_t c) { return satOf(c) < kGraySat ? Rgb{255, 255, 255} : hsvToRgb(hueOf(c), 255, 255); }

}

float contrastRatio(uint32_t a, uint32_t b) {
  float x = luminance(a);
  float y = luminance(b);
  return x > y ? (x + 0.05f) / (y + 0.05f) : (y + 0.05f) / (x + 0.05f);
}

TeamPalette teamPalette(uint32_t c1, uint32_t c2) {
  c1 &= 0xFFFFFF;
  c2 &= 0xFFFFFF;
  if (!c1 && !c2) return kDefaultPalette;
  uint32_t base = c1;
  uint32_t other = c2;
  if (luminance(c1) > kLight) {
    base = c2;
    other = c1;
  }
  bool neutral = luminance(base) > kLight || satOf(base) < kGraySat;
  TeamPalette p;
  if (neutral) {
    p.bg = 0x1A1A1E;
    p.deep = 0x0E0E11;
    p.stripe = 0x26262B;
    p.sky = 0xC8D0DC;
    if (satOf(base) > satOf(other)) {
      uint32_t t = base;
      base = other;
      other = t;
    }
  } else {
    p.bg = tone(base, 80);
    p.deep = tone(base, 52);
    p.stripe = tone(base, 97);
    p.sky = rgbHex(hsvToRgb(hueOf(base), 128, 255));
  }
  p.accent = readable(other, p.bg) ? other : 0xFFFFFF;
  p.pale = rgbHex(mixRgb(unpack(p.accent), {255, 255, 255}, 140));
  p.bulbA = bulbOf(c2);
  p.bulbB = bulbOf(c1);
  if (p.bulbA.r == p.bulbB.r && p.bulbA.g == p.bulbB.g && p.bulbA.b == p.bulbB.b) p.bulbB = kelvinToRgb(2200);
  return p;
}

}
