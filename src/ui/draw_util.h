#pragma once

#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>

namespace du {

void text(lv_layer_t* layer, const lv_font_t* font, lv_color_t color, lv_text_align_t align, int32_t x1, int32_t y1,
          int32_t x2, const char* txt, lv_opa_t opa = LV_OPA_COVER);
void box(lv_layer_t* layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, lv_color_t color, lv_opa_t opa,
         int32_t radius);
void hgrad(lv_layer_t* layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, lv_color_t left, lv_color_t right,
           int32_t radius);
void hfade(lv_layer_t* layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, lv_color_t color, lv_opa_t left,
           lv_opa_t right);
void image(lv_layer_t* layer, const lv_image_dsc_t* img, int32_t x, int32_t y, lv_opa_t opa = LV_OPA_COVER);
bool visible(lv_layer_t* layer, int32_t y1, int32_t y2);
int32_t width(const lv_font_t* font, const char* txt);
const char* fit(const lv_font_t* font, const char* txt, int32_t maxW, char* buf, size_t cap, size_t from = 0,
                size_t to = SIZE_MAX);

class Clip {
 public:
  Clip(lv_layer_t* layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2);
  ~Clip();
  bool empty() const { return empty_; }
  Clip(const Clip&) = delete;
  Clip& operator=(const Clip&) = delete;

 private:
  lv_layer_t* layer_;
  lv_area_t saved_;
  bool empty_;
};

}
