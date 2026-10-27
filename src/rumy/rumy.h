#pragma once

#include <lvgl.h>
#include <stdint.h>

#include "wx_mood.h"

namespace rumy {

enum Mood : uint8_t {
  NORMAL = 0,
  HAPPY,
  SLEEPY,
  ASLEEP,
  SAD,
  SURPRISED,
  LOOKUP,
  CELEBRATE,
  CALM,
  MUSIC,
  SQUINT,
  SCARED,
  SQUEEZE,
  MOOD_COUNT,
};

enum Effect : uint8_t {
  FX_NONE = 0,
  FX_SUNGLASSES,
  FX_SWEAT,
  FX_CLOUD,
  FX_FOG,
  FX_DRIZZLE,
  FX_RAIN,
  FX_SNOW,
  FX_STORM,
  FX_WIND,
  FX_SHIVER,
  FX_STARS,
  FX_TEAR,
  FX_CONFETTI,
  FX_COUNT,
};

struct Look {
  Mood mood;
  uint8_t acc;
  Effect fx;
};

lv_obj_t* create(lv_obj_t* parent, int32_t w, int32_t h, int32_t eye);
void setMood(lv_obj_t* face, Mood m);
Mood mood(lv_obj_t* face);
void setAccessory(lv_obj_t* face, uint8_t acc);
void setEffect(lv_obj_t* face, Effect fx);
void setAnimated(lv_obj_t* face, bool on);
void setColor(lv_obj_t* face, lv_color_t c);
void setBackground(lv_obj_t* face, lv_color_t c);
void setMusic(lv_obj_t* face, bool on, lv_color_t phone, lv_color_t note);
void setTempo(lv_obj_t* face, uint32_t periodMs, uint32_t anchorTick);
void poke(lv_obj_t* face, int32_t x, int32_t y);
const char* moodName(Mood m);
Look weatherLook(rc::WxKind k);

}
