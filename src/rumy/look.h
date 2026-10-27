#pragma once

#include <stdint.h>

namespace look {

struct Pick {
  uint8_t mood;
  uint8_t acc;
  uint8_t fx;
  uint8_t mine;
};

bool set(uint8_t mood, uint8_t acc, uint8_t fx);
void clear();
bool request(Pick& out, uint32_t& seq);
void drop(uint32_t seq);
void publish(const Pick& shown);
Pick shown();

}
