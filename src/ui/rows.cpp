#include "rows.h"

#include <stdio.h>

#include "fonts/symbols.h"
#include "theme.h"
#include "ui.h"

namespace rows {

using theme::S;

lv_obj_t* screen() {
  lv_obj_t* s = theme::makeScreen();
  ui::attachGestures(s);
  lv_obj_add_flag(s, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(s, LV_DIR_VER);
  theme::styleScrollbar(s);
  lv_obj_set_flex_flow(s, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_top(s, S(6), 0);
  lv_obj_set_style_pad_bottom(s, S(12), 0);
  lv_obj_set_style_pad_row(s, S(6), 0);
  return s;
}

lv_obj_t* header(lv_obj_t* scr, const char* title, bool home, lv_event_cb_t cb) {
  lv_obj_t* head = theme::makeBox(scr);
  lv_obj_set_size(head, W, S(40));
  lv_obj_t* b = theme::makeButton(head, home ? RS_HOME : RS_LEFT, theme::fontM());
  lv_obj_set_size(b, S(44), S(36));
  lv_obj_set_pos(b, home ? W - S(44) : 0, S(2));
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* t = theme::makeLabel(head, theme::fontL(), theme::C_TEXT);
  lv_label_set_text_static(t, title);
  lv_obj_set_pos(t, home ? S(4) : S(56), S(6));
  return head;
}

lv_obj_t* section(lv_obj_t* scr, const char* text) {
  lv_obj_t* l = theme::makeLabel(scr, theme::fontS(), theme::C_TEXT3);
  lv_label_set_text_static(l, text);
  lv_obj_set_width(l, W - S(8));
  return l;
}

lv_obj_t* row(lv_obj_t* parent, const char* title, int32_t h) {
  lv_obj_t* r = theme::makeCard(parent, W, h);
  lv_obj_t* t = theme::makeLabel(r, theme::fontM(), theme::C_TEXT);
  lv_label_set_text_static(t, title);
  lv_obj_set_pos(t, S(12), S(12));
  return r;
}

lv_obj_t* value(lv_obj_t* r) {
  lv_obj_t* v = theme::makeLabel(r, theme::fontS(), theme::C_TEXT2);
  lv_obj_set_width(v, W - S(158));
  lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
  theme::oneLine(v);
  lv_obj_set_pos(v, S(146), S(15));
  return v;
}

lv_obj_t* nav(lv_obj_t* parent, const char* title, lv_event_cb_t cb, lv_obj_t** valueOut) {
  lv_obj_t* r = row(parent, title, H);
  lv_obj_add_flag(r, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_style(r, theme::stylePressed(), LV_STATE_PRESSED);
  lv_obj_add_event_cb(r, cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* v = value(r);
  lv_label_set_text_static(v, RS_RIGHT);
  if (valueOut) *valueOut = v;
  return r;
}

lv_obj_t* slider(lv_obj_t* r, int32_t lo, int32_t hi, lv_event_cb_t cb) {
  lv_obj_t* s = lv_slider_create(r);
  theme::styleSlider(s);
  lv_slider_set_range(s, lo, hi);
  lv_obj_set_pos(s, S(24), S(44));
  lv_obj_set_size(s, W - S(48), S(12));
  lv_obj_add_event_cb(s, cb, LV_EVENT_ALL, nullptr);
  return s;
}

lv_obj_t* toggle(lv_obj_t* r, lv_event_cb_t cb) {
  lv_obj_t* s = lv_switch_create(r);
  theme::styleSwitch(s);
  lv_obj_set_pos(s, W - S(60), S(9));
  lv_obj_add_event_cb(s, cb, LV_EVENT_VALUE_CHANGED, nullptr);
  return s;
}

lv_obj_t* stepper(lv_obj_t* r, lv_event_cb_t cb) {
  lv_obj_t* minus = theme::makeButton(r, "\xE2\x80\x93", theme::fontM());
  lv_obj_set_size(minus, S(38), S(30));
  lv_obj_set_pos(minus, W - S(134), S(7));
  lv_obj_add_event_cb(minus, cb, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(-1)));
  lv_obj_t* plus = theme::makeButton(r, "+", theme::fontM());
  lv_obj_set_size(plus, S(38), S(30));
  lv_obj_set_pos(plus, W - S(44), S(7));
  lv_obj_add_event_cb(plus, cb, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(1)));
  lv_obj_t* v = theme::makeLabel(r, theme::fontM(), theme::C_TEXT);
  lv_obj_set_width(v, S(52));
  lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(v, W - S(94), S(12));
  return v;
}

int stepDir(lv_event_t* e) { return static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e))); }

void setChecked(lv_obj_t* o, bool on) {
  if (!o) return;
  if (on) lv_obj_add_state(o, LV_STATE_CHECKED);
  else lv_obj_remove_state(o, LV_STATE_CHECKED);
}

void setNavValue(lv_obj_t* label, const char* text) {
  char buf[64];
  snprintf(buf, sizeof(buf), "%s  " RS_RIGHT, text);
  theme::setText(label, buf);
}

}
