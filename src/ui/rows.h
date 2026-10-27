#pragma once

#include <lvgl.h>
#include <stdint.h>

#include "theme.h"

namespace rows {

constexpr int32_t W = theme::FW(308);
constexpr int32_t H = theme::S(44);

lv_obj_t* screen();
lv_obj_t* header(lv_obj_t* scr, const char* title, bool home, lv_event_cb_t cb);
lv_obj_t* section(lv_obj_t* scr, const char* text);
lv_obj_t* row(lv_obj_t* parent, const char* title, int32_t h);
lv_obj_t* value(lv_obj_t* row);
lv_obj_t* nav(lv_obj_t* parent, const char* title, lv_event_cb_t cb, lv_obj_t** valueOut);
lv_obj_t* slider(lv_obj_t* row, int32_t lo, int32_t hi, lv_event_cb_t cb);
lv_obj_t* toggle(lv_obj_t* row, lv_event_cb_t cb);
lv_obj_t* stepper(lv_obj_t* row, lv_event_cb_t cb);
int stepDir(lv_event_t* e);
void setChecked(lv_obj_t* o, bool on);
void setNavValue(lv_obj_t* label, const char* text);

}
