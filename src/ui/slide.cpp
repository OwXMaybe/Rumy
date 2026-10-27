#include "slide.h"

#include <Arduino.h>

#include "app/config.h"
#include "board/display.h"
#include "src/display/lv_display_private.h"

namespace slide {

namespace {

Stats s_last = {};
bool s_invert = false;

uint16_t line(int32_t v, int32_t w) {
  int32_t l = ((v % w) + w) % w;
  if (s_invert) l = (w - l) % w;
  return static_cast<uint16_t>(l);
}

int32_t eased(uint32_t elapsed, uint32_t total, int32_t span) {
  if (elapsed >= total) return span;
  uint32_t p = (elapsed << 10) / total;
  uint32_t inv = 1024 - p;
  uint32_t cube = (inv * inv / 1024) * inv / 1024;
  return static_cast<int32_t>((static_cast<uint32_t>(span) * (1024 - cube)) >> 10);
}

void drawColumns(lv_display_t* disp, lv_obj_t* scr, int32_t x1, int32_t x2) {
  disp->inv_p = 0;
  lv_area_t a = {x1, 0, x2, cfg::SCREEN_H - 1};
  lv_obj_invalidate_area(scr, &a);
  lv_display_refr_timer(lv_display_get_refr_timer(disp));
}

}

void run(lv_obj_t* scr, int dir) {
  lv_display_t* disp = lv_display_get_default();
  lv_timer_t* refr = lv_display_get_refr_timer(disp);
  const int32_t w = cfg::SCREEN_W;
  const bool hw = display::hardwareScroll();
  lv_timer_pause(refr);
  lv_screen_load(scr);
  lv_obj_update_layout(scr);
  uint32_t t0 = millis();
  uint32_t lastMs = t0;
  Stats st = {};
  int32_t done = 0;
  while (done < w) {
    uint32_t now = millis();
    int32_t target = eased(now - t0, cfg::SLIDE_MS, w);
    if (target - done < cfg::SLIDE_MIN_STEP && target < w) {
      vTaskDelay(1);
      continue;
    }
    if (!hw) {
      if (dir > 0) drawColumns(disp, scr, w - target, w - done - 1);
      else drawColumns(disp, scr, done, target - 1);
    } else {
      if (dir > 0) drawColumns(disp, scr, done, target - 1);
      else drawColumns(disp, scr, w - target, w - done - 1);
      display::setScroll(line(dir > 0 ? target : w - target, w));
    }
    done = target;
    uint32_t end = millis();
    uint32_t frame = end - lastMs;
    if (frame > st.maxFrameMs) st.maxFrameMs = static_cast<uint16_t>(frame);
    lastMs = end;
    st.frames++;
  }
  st.totalMs = static_cast<uint16_t>(millis() - t0);
  s_last = st;
  lv_timer_resume(refr);
  display::ignoreUntilRelease();
}

Stats last() { return s_last; }

void setInvert(bool on) { s_invert = on; }

}
