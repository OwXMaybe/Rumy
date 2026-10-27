#pragma once

#include <stdint.h>

namespace lvmem {

struct Snap {
  uint32_t total;
  uint32_t used;
  uint32_t free;
  uint32_t peak;
  uint32_t biggest;
  uint8_t frag;
};

void resetPeak();
Snap snap();
bool hold(uint32_t bytes);
uint32_t held();

}
