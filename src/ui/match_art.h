#pragma once

#include <lvgl.h>
#include <stdint.h>

#if defined(RUMY_SCREEN_H) && RUMY_SCREEN_H >= 320
constexpr int32_t LEAGUE_PX = 26;
constexpr int32_t BALL_PX = 32;
#else
constexpr int32_t LEAGUE_PX = 18;
constexpr int32_t BALL_PX = 22;
#endif
constexpr int LEAGUE_LOGOS = 11;
constexpr int BALL_FRAMES = 8;
constexpr int BALL_STEP_DEG = 9;
extern const lv_image_dsc_t* const LEAGUE_IMAGES[LEAGUE_LOGOS];
extern const lv_image_dsc_t* const BALL_IMAGES[BALL_FRAMES];
