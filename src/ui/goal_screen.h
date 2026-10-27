#pragma once

#include <lvgl.h>
#include <stdint.h>

#include "espn.h"

namespace goal_screen {

enum Variant : uint8_t { V_LETTERS = 0, V_NET, V_FIREWORKS, V_RUMY, V_COUNT };

void open(const rc::Match& m, const rc::MatchEvent& e, uint8_t variant, lv_event_cb_t clicked);
void leave();
void kill();
bool active();
bool leaving();
const char* variantName(uint8_t variant);

}
