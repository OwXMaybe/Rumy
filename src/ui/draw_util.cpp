#include "draw_util.h"

#include <stdio.h>
#include <string.h>

namespace du {

void text(lv_layer_t* layer, const lv_font_t* font, lv_color_t color, lv_text_align_t align, int32_t x1, int32_t y1,
          int32_t x2, const char* txt, lv_opa_t opa) {
  if (!txt || !txt[0]) return;
  int32_t lh = lv_font_get_line_height(font);
  if (!visible(layer, y1, y1 + lh)) return;
  lv_draw_label_dsc_t d;
  lv_draw_label_dsc_init(&d);
  d.font = font;
  d.color = color;
  d.opa = opa;
  d.align = align;
  d.text = txt;
  d.text_local = 1;
  d.flag = LV_TEXT_FLAG_EXPAND;
  lv_area_t a = {x1, y1, x2, y1 + lh - 1};
  lv_draw_label(layer, &d, &a);
}

void box(lv_layer_t* layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, lv_color_t color, lv_opa_t opa,
         int32_t radius) {
  if (x2 < x1 || y2 < y1 || !visible(layer, y1, y2)) return;
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = color;
  d.bg_opa = opa;
  d.radius = radius;
  d.border_width = 0;
  lv_area_t a = {x1, y1, x2, y2};
  lv_draw_rect(layer, &d, &a);
}

void hgrad(lv_layer_t* layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, lv_color_t left, lv_color_t right,
           int32_t radius) {
  if (x2 < x1 || y2 < y1 || !visible(layer, y1, y2)) return;
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = left;
  d.bg_opa = LV_OPA_COVER;
  d.radius = radius;
  d.border_width = 0;
  d.bg_grad.dir = LV_GRAD_DIR_HOR;
  d.bg_grad.stops_count = 2;
  d.bg_grad.stops[0].color = left;
  d.bg_grad.stops[0].opa = LV_OPA_COVER;
  d.bg_grad.stops[0].frac = 0;
  d.bg_grad.stops[1].color = right;
  d.bg_grad.stops[1].opa = LV_OPA_COVER;
  d.bg_grad.stops[1].frac = 255;
  lv_area_t a = {x1, y1, x2, y2};
  lv_draw_rect(layer, &d, &a);
}

void hfade(lv_layer_t* layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, lv_color_t color, lv_opa_t left,
           lv_opa_t right) {
  if (x2 < x1 || y2 < y1 || !visible(layer, y1, y2)) return;
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = color;
  d.bg_opa = LV_OPA_COVER;
  d.border_width = 0;
  d.bg_grad.dir = LV_GRAD_DIR_HOR;
  d.bg_grad.stops_count = 2;
  d.bg_grad.stops[0].color = color;
  d.bg_grad.stops[0].opa = left;
  d.bg_grad.stops[0].frac = 0;
  d.bg_grad.stops[1].color = color;
  d.bg_grad.stops[1].opa = right;
  d.bg_grad.stops[1].frac = 255;
  lv_area_t a = {x1, y1, x2, y2};
  lv_draw_rect(layer, &d, &a);
}

void image(lv_layer_t* layer, const lv_image_dsc_t* img, int32_t x, int32_t y, lv_opa_t opa) {
  if (!img) return;
  int32_t w = img->header.w;
  int32_t h = img->header.h;
  if (!visible(layer, y, y + h - 1)) return;
  lv_draw_image_dsc_t d;
  lv_draw_image_dsc_init(&d);
  d.src = img;
  d.opa = opa;
  lv_area_t a = {x, y, x + w - 1, y + h - 1};
  lv_draw_image(layer, &d, &a);
}

int32_t width(const lv_font_t* font, const char* txt) {
  lv_point_t p;
  lv_text_get_size(&p, txt, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_EXPAND);
  return p.x;
}

const char* fit(const lv_font_t* font, const char* txt, int32_t maxW, char* buf, size_t cap, size_t from, size_t to) {
  if (!txt || width(font, txt) <= maxW || cap < 8) return txt;
  size_t len = strlen(txt);
  if (to > len) to = len;
  if (from > to) from = to;
  const char* tail = txt + to;
  size_t tailLen = len - to;
  size_t mid = to;
  while (mid > from) {
    do {
      mid--;
    } while (mid > from && (static_cast<uint8_t>(txt[mid]) & 0xC0) == 0x80);
    size_t cut = mid;
    while (cut > from && txt[cut - 1] == ' ') cut--;
    if (cut + 3 + tailLen + 1 > cap) continue;
    snprintf(buf, cap, "%.*s\xE2\x80\xA6%.*s", static_cast<int>(cut), txt, static_cast<int>(tailLen), tail);
    if (width(font, buf) <= maxW) return buf;
  }
  snprintf(buf, cap, "%.*s\xE2\x80\xA6", static_cast<int>(from), txt);
  return buf;
}

bool visible(lv_layer_t* layer, int32_t y1, int32_t y2) {
  return !(y2 < layer->_clip_area.y1 || y1 > layer->_clip_area.y2);
}

Clip::Clip(lv_layer_t* layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2) : layer_(layer) {
  saved_ = layer->_clip_area;
  lv_area_t res = {LV_MAX(x1, saved_.x1), LV_MAX(y1, saved_.y1), LV_MIN(x2, saved_.x2), LV_MIN(y2, saved_.y2)};
  empty_ = res.x1 > res.x2 || res.y1 > res.y2;
  if (!empty_) layer->_clip_area = res;
}

Clip::~Clip() { layer_->_clip_area = saved_; }

}
