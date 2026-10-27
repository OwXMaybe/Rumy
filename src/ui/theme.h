#pragma once

#include <lvgl.h>
#include <stddef.h>

#include "app/config.h"
#include "wmo_tr.h"

namespace theme {

constexpr int32_t SW = cfg::SCREEN_W;
constexpr int32_t SH = cfg::SCREEN_H;

constexpr int32_t S(int32_t v) { return (v * 2 * SH + (v >= 0 ? 240 : -240)) / 480; }
constexpr int32_t R(int32_t x) { return SW - S(320 - x); }
constexpr int32_t CX(int32_t x) { return SW / 2 - S(160 - x); }
constexpr int32_t FW(int32_t w) { return SW - S(320 - w); }

constexpr uint32_t C_BG = 0x000000;
constexpr uint32_t C_SURFACE = 0x11151B;
constexpr uint32_t C_SURFACE2 = 0x1A2029;
constexpr uint32_t C_BORDER = 0x27303C;
constexpr uint32_t C_TEXT = 0xF2F5F9;
constexpr uint32_t C_TEXT2 = 0x9AA5B4;
constexpr uint32_t C_TEXT3 = 0x5E6978;
constexpr uint32_t C_EYE = 0x19D3F0;
constexpr uint32_t C_OK = 0x3DDC84;
constexpr uint32_t C_WARN = 0xFFB020;
constexpr uint32_t C_DANGER = 0xFF5A5F;
constexpr uint32_t C_BLUE = 0x5AB0FF;

constexpr uint32_t C_WHITE = 0xFFFFFF;
constexpr uint32_t C_NEON_GREEN = 0x2BFF6A;
constexpr uint32_t C_NEON_LIME = 0xB6FF3B;
constexpr uint32_t C_NEON_CYAN = 0x00E0FF;
constexpr uint32_t C_NEON_YELLOW = 0xFFD60A;
constexpr uint32_t C_NEON_ORANGE = 0xFF9F1C;
constexpr uint32_t C_NEON_RED = 0xFF4040;
constexpr uint32_t C_NEON_PINK = 0xFF4FA3;
constexpr uint32_t C_NEON_PURPLE = 0xB84DFF;
constexpr uint32_t C_NEON_BLUE = 0x1E6BFF;
constexpr uint32_t C_BAR_BG = 0x1B222B;

constexpr int32_t RADIUS_S = S(10);
constexpr int32_t RADIUS_M = S(14);
constexpr int32_t RADIUS_L = S(20);
constexpr int32_t PAD = S(8);
constexpr int32_t TOUCH_MIN = S(40);

constexpr int32_t ICON_MAIN = S(64);
constexpr int32_t ICON_HOME = S(48);
constexpr int32_t ICON_HOUR = S(28);
constexpr int32_t ICON_DAY = S(24);
constexpr int32_t ICON_ACC = S(32);

constexpr int32_t HOME_EYE = S(62);
constexpr int32_t EVENT_EYE = S(84);

inline lv_color_t col(uint32_t hex) { return lv_color_hex(hex); }

const lv_font_t* fontS();
const lv_font_t* fontM();
const lv_font_t* fontL();
const lv_font_t* fontNum();
const lv_font_t* fontTemp();
const lv_font_t* fontGoal();
const lv_font_t* fontHero();

void init();
void setTeamColors(uint32_t c1, uint32_t c2);
uint32_t team();
uint32_t teamBg();
uint32_t teamDeep();
uint32_t teamStripe();
uint32_t teamPale();
uint32_t teamSky();
lv_style_t* styleScreen();
lv_style_t* styleCard();
lv_style_t* styleCardGlass();
lv_style_t* styleButton();
lv_style_t* stylePressed();
lv_style_t* styleSliderMain();
lv_style_t* styleSliderIndicator();
lv_style_t* styleSliderKnob();
lv_style_t* styleSwitchMain();
lv_style_t* styleSwitchChecked();
lv_style_t* styleSwitchKnob();
lv_style_t* styleRollerMain();
lv_style_t* styleRollerSelected();

struct SkyGradient {
  uint32_t top;
  uint32_t bottom;
};

SkyGradient sky(rc::SkyClass s);
lv_color_t tempColor(int16_t temp10);

lv_obj_t* makeScreen();
lv_style_t* textStyle(const lv_font_t* font, uint32_t color);
lv_obj_t* makeLabel(lv_obj_t* parent, const lv_font_t* font, uint32_t color);
void oneLine(lv_obj_t* label);
const char* printable(const lv_font_t* font, const char* in, char* out, size_t cap, const char* fallback);
lv_obj_t* makeBox(lv_obj_t* parent);
lv_obj_t* makeButton(lv_obj_t* parent, const char* text, const lv_font_t* font);
void styleSlider(lv_obj_t* slider);
void styleSwitch(lv_obj_t* sw);
void styleRoller(lv_obj_t* roller);
void setText(lv_obj_t* label, const char* text);
lv_obj_t* makeCard(lv_obj_t* parent, int32_t w, int32_t h);
void styleScrollbar(lv_obj_t* obj);

}
