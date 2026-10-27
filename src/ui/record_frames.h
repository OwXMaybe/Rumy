#pragma once

#include <lvgl.h>
#include <stdint.h>

#if defined(RUMY_SCREEN_H) && RUMY_SCREEN_H >= 320
constexpr int32_t RECORD_PX = 22;
#else
constexpr int32_t RECORD_PX = 17;
#endif
constexpr int32_t RECORD_FRAMES = 24;
constexpr int32_t RECORD_PERIOD_DEG = 180;
extern const lv_image_dsc_t* const RECORD_IMAGES[RECORD_FRAMES];
