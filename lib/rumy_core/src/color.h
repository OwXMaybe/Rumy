#pragma once

#include <stdint.h>

namespace rc {

struct Rgb {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

Rgb hsvToRgb(uint16_t hue, uint8_t sat, uint8_t val);
uint16_t rgbToHue(uint8_t r, uint8_t g, uint8_t b);
uint8_t rgbToSat(uint8_t r, uint8_t g, uint8_t b);
Rgb kelvinToRgb(uint16_t kelvin);
uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);
uint32_t rgbHex(Rgb c);
Rgb scaleRgb(Rgb c, uint8_t percent);
Rgb mixRgb(Rgb a, Rgb b, uint8_t amount);

struct TeamPalette {
  uint32_t accent;
  uint32_t bg;
  uint32_t deep;
  uint32_t stripe;
  uint32_t pale;
  uint32_t sky;
  Rgb bulbA;
  Rgb bulbB;
};

float contrastRatio(uint32_t a, uint32_t b);
TeamPalette teamPalette(uint32_t c1, uint32_t c2);

}
