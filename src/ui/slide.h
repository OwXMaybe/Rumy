#pragma once

#include <lvgl.h>
#include <stdint.h>

namespace slide {

struct Stats {
  uint16_t frames;
  uint16_t maxFrameMs;
  uint16_t totalMs;
};

void run(lv_obj_t* scr, int dir);
Stats last();
void setInvert(bool on);

}
