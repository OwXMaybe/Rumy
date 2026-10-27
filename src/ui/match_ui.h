#pragma once

#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>

#include "espn.h"
#include "espn_board.h"

namespace match_ui {

void changed();
void tick();
void close();
bool isGoal(uint8_t kind);
int32_t drawIcon(lv_layer_t* layer, const rc::MatchEvent& e, int32_t x, int32_t y, int32_t h);
const char* side(const rc::Match& m, bool home, bool full);
void scoreText(const rc::Match& m, char* out, size_t cap);
void kickoffText(const rc::Match& m, char* out, size_t cap);
void forceGoalVariant(int8_t variant);
bool reaction(uint8_t& mood);
bool busy();
const lv_image_dsc_t* leagueLogo(uint8_t league);
void drawGameRow(lv_layer_t* layer, int32_t x1, int32_t x2, int32_t y, int32_t h, const rc::BoardGame& g);
int32_t drawLeague(lv_layer_t* layer, uint8_t league, int32_t x, int32_t y, int32_t h, lv_color_t fallback);

}
