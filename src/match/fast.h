#pragma once

#include <stdint.h>

#include "espn.h"

namespace fast {

enum MsgType : uint8_t { T_NONE = 0, T_GOAL, T_SCORE };

struct Msg {
  uint32_t espnId;
  uint8_t type;
  uint8_t home;
  uint8_t away;
  uint8_t homeSide;
  uint8_t kind;
  uint8_t reserved[3];
  char minute[8];
  char player[rc::MATCH_PLAYER];
};

bool parseLine(const char* line, Msg& out);
void line(const char* text);
bool take(Msg& out);
uint32_t received();

}
