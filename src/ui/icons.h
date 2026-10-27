#pragma once

#include <lvgl.h>
#include <stdint.h>

#include "wmo_tr.h"

namespace icons {

enum Size : uint8_t {
  S64 = 0,
  S48,
  S28,
  S24,
  SIZE_COUNT,
};

const lv_image_dsc_t* get(rc::IconId id, Size s);
int32_t pixels(Size s);

}
