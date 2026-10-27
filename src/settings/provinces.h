#pragma once

#include <stdint.h>

namespace provinces {

struct Province {
  uint8_t plate;
  const char* name;
  float lat;
  float lon;
};

constexpr int COUNT = 81;

const Province* byPlate(int plate);
const Province& at(int index);
int indexOfPlate(int plate);
int findByName(const char* name);

}
