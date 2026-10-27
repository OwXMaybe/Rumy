#pragma once

#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>

#include "agents_proto.h"

namespace agents_ui {

void changed();
void tick();
void close();
bool busy();
bool reaction(uint8_t& mood);
uint32_t color(uint8_t status);
void drawBeacon(lv_layer_t* layer, int32_t cx, int32_t cy, int32_t r, uint8_t status, uint32_t ms);
bool animated(uint8_t status);
void lineText(const rc::AgentRow& r, char* out, size_t cap);
void summary(const rc::Agents& a, char* out, size_t cap);

}
