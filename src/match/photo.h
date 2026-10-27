#pragma once

#include <lvgl.h>

namespace photo {

void request(const char* player);
const lv_image_dsc_t* get(const char* player);

}
