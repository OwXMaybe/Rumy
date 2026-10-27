#include "theme.h"

#include <string.h>

#include "src/misc/lv_text_private.h"
#include "color.h"

#if RUMY_SCREEN_H >= 320
extern "C" {
LV_FONT_DECLARE(font_rumy_b16)
LV_FONT_DECLARE(font_rumy_b21)
LV_FONT_DECLARE(font_rumy_b29)
LV_FONT_DECLARE(font_rumy_bnum)
LV_FONT_DECLARE(font_rumy_btemp)
LV_FONT_DECLARE(font_rumy_bgoal)
LV_FONT_DECLARE(font_rumy_bhero)
}
#else
extern "C" {
LV_FONT_DECLARE(font_rumy_temp)
LV_FONT_DECLARE(font_rumy_goal)
LV_FONT_DECLARE(font_rumy_hero)
}
#endif

namespace theme {

namespace {

lv_style_t s_screen;
lv_style_t s_card;
lv_style_t s_glass;
lv_style_t s_button;
lv_style_t s_pressed;
lv_style_t s_sliderMain;
lv_style_t s_sliderInd;
lv_style_t s_sliderKnob;
lv_style_t s_swMain;
lv_style_t s_swChecked;
lv_style_t s_swKnob;
lv_style_t s_rollerMain;
lv_style_t s_rollerSel;
lv_style_t s_scrollbar;
bool s_inited = false;

constexpr int TEXT_STYLES = 24;

struct TextStyle {
  const lv_font_t* font;
  uint32_t color;
  lv_style_t style;
};

TextStyle s_text[TEXT_STYLES];
int s_textCount = 0;

rc::TeamPalette s_team = rc::teamPalette(0, 0);
uint32_t s_teamC1 = 0;
uint32_t s_teamC2 = 0;

const SkyGradient kSky[rc::SKY_COUNT] = {
    {0x2E6FD8, 0x6FB6F7}, {0x0A1330, 0x24345E}, {0x50627A, 0x8FA0B6}, {0x151B27, 0x333D52},
    {0x2F3B4D, 0x5A6A80}, {0x7C8DA3, 0xB9C7D8}, {0x1E2230, 0x434A63}, {0x5E6672, 0x9AA1AB},
};

struct TempStop {
  int16_t t10;
  uint32_t color;
};

const TempStop kTemp[] = {
    {-100, 0x9AD4FF}, {0, 0x7EC8FF},   {50, 0x5AB0FF},  {100, 0x4FD1C5}, {150, 0x7BE07B},
    {200, 0xD8E35A},  {250, 0xFFC247}, {300, 0xFF8C42}, {350, 0xFF5A5F},
};

}

void setTeamColors(uint32_t c1, uint32_t c2) {
  if (c1 == s_teamC1 && c2 == s_teamC2) return;
  s_teamC1 = c1;
  s_teamC2 = c2;
  s_team = rc::teamPalette(c1, c2);
}

uint32_t team() { return s_team.accent; }
uint32_t teamBg() { return s_team.bg; }
uint32_t teamDeep() { return s_team.deep; }
uint32_t teamStripe() { return s_team.stripe; }
uint32_t teamPale() { return s_team.pale; }
uint32_t teamSky() { return s_team.sky; }

#if RUMY_SCREEN_H >= 320
const lv_font_t* fontS() { return &font_rumy_b16; }
const lv_font_t* fontM() { return &font_rumy_b21; }
const lv_font_t* fontL() { return &font_rumy_b29; }
const lv_font_t* fontNum() { return &font_rumy_bnum; }
const lv_font_t* fontTemp() { return &font_rumy_btemp; }
const lv_font_t* fontGoal() { return &font_rumy_bgoal; }
const lv_font_t* fontHero() { return &font_rumy_bhero; }
#else
const lv_font_t* fontS() { return &font_rumy_12; }
const lv_font_t* fontM() { return &font_rumy_16; }
const lv_font_t* fontL() { return &font_rumy_22; }
const lv_font_t* fontNum() { return &font_rumy_num; }
const lv_font_t* fontTemp() { return &font_rumy_temp; }
const lv_font_t* fontGoal() { return &font_rumy_goal; }
const lv_font_t* fontHero() { return &font_rumy_hero; }
#endif

void init() {
  if (s_inited) return;
  s_inited = true;

  lv_style_init(&s_screen);
  lv_style_set_bg_color(&s_screen, col(C_BG));
  lv_style_set_bg_opa(&s_screen, LV_OPA_COVER);
  lv_style_set_text_color(&s_screen, col(C_TEXT));
  lv_style_set_text_font(&s_screen, fontM());

  lv_style_init(&s_card);
  lv_style_set_bg_color(&s_card, col(C_SURFACE));
  lv_style_set_bg_opa(&s_card, LV_OPA_COVER);
  lv_style_set_radius(&s_card, RADIUS_M);
  lv_style_set_border_width(&s_card, 1);
  lv_style_set_border_color(&s_card, col(C_BORDER));
  lv_style_set_pad_all(&s_card, 0);

  lv_style_init(&s_glass);
  lv_style_set_bg_color(&s_glass, lv_color_black());
  lv_style_set_bg_opa(&s_glass, LV_OPA_30);
  lv_style_set_radius(&s_glass, RADIUS_M);
  lv_style_set_border_width(&s_glass, 0);

  lv_style_init(&s_button);
  lv_style_set_bg_color(&s_button, col(C_SURFACE2));
  lv_style_set_bg_opa(&s_button, LV_OPA_COVER);
  lv_style_set_radius(&s_button, RADIUS_S);
  lv_style_set_border_width(&s_button, 1);
  lv_style_set_border_color(&s_button, col(C_BORDER));
  lv_style_set_text_color(&s_button, col(C_TEXT));
  lv_style_set_pad_all(&s_button, S(4));

  lv_style_init(&s_pressed);
  lv_style_set_bg_color(&s_pressed, col(C_BORDER));
  lv_style_set_translate_y(&s_pressed, 1);

  lv_style_init(&s_sliderMain);
  lv_style_set_bg_color(&s_sliderMain, col(C_SURFACE2));
  lv_style_set_bg_opa(&s_sliderMain, LV_OPA_COVER);
  lv_style_set_radius(&s_sliderMain, LV_RADIUS_CIRCLE);
  lv_style_set_pad_ver(&s_sliderMain, 0);

  lv_style_init(&s_sliderInd);
  lv_style_set_bg_color(&s_sliderInd, col(C_EYE));
  lv_style_set_bg_opa(&s_sliderInd, LV_OPA_COVER);
  lv_style_set_radius(&s_sliderInd, LV_RADIUS_CIRCLE);

  lv_style_init(&s_sliderKnob);
  lv_style_set_bg_color(&s_sliderKnob, col(C_TEXT));
  lv_style_set_bg_opa(&s_sliderKnob, LV_OPA_COVER);
  lv_style_set_radius(&s_sliderKnob, LV_RADIUS_CIRCLE);
  lv_style_set_pad_all(&s_sliderKnob, S(7));
  lv_style_set_border_width(&s_sliderKnob, S(2));
  lv_style_set_border_color(&s_sliderKnob, col(C_BG));

  lv_style_init(&s_swMain);
  lv_style_set_bg_color(&s_swMain, col(C_SURFACE2));
  lv_style_set_bg_opa(&s_swMain, LV_OPA_COVER);
  lv_style_set_radius(&s_swMain, LV_RADIUS_CIRCLE);
  lv_style_set_border_width(&s_swMain, 1);
  lv_style_set_border_color(&s_swMain, col(C_BORDER));

  lv_style_init(&s_swChecked);
  lv_style_set_bg_color(&s_swChecked, col(C_EYE));
  lv_style_set_bg_opa(&s_swChecked, LV_OPA_COVER);
  lv_style_set_radius(&s_swChecked, LV_RADIUS_CIRCLE);

  lv_style_init(&s_swKnob);
  lv_style_set_bg_color(&s_swKnob, col(C_TEXT));
  lv_style_set_bg_opa(&s_swKnob, LV_OPA_COVER);
  lv_style_set_radius(&s_swKnob, LV_RADIUS_CIRCLE);
  lv_style_set_pad_all(&s_swKnob, -S(3));

  lv_style_init(&s_rollerMain);
  lv_style_set_bg_color(&s_rollerMain, col(C_SURFACE));
  lv_style_set_bg_opa(&s_rollerMain, LV_OPA_COVER);
  lv_style_set_text_color(&s_rollerMain, col(C_TEXT2));
  lv_style_set_text_font(&s_rollerMain, fontM());
  lv_style_set_radius(&s_rollerMain, RADIUS_M);
  lv_style_set_border_width(&s_rollerMain, 1);
  lv_style_set_border_color(&s_rollerMain, col(C_BORDER));
  lv_style_set_text_line_space(&s_rollerMain, S(14));
  lv_style_set_text_align(&s_rollerMain, LV_TEXT_ALIGN_CENTER);

  lv_style_init(&s_rollerSel);
  lv_style_set_bg_color(&s_rollerSel, col(C_SURFACE2));
  lv_style_set_bg_opa(&s_rollerSel, LV_OPA_COVER);
  lv_style_set_text_color(&s_rollerSel, col(C_EYE));
  lv_style_set_text_font(&s_rollerSel, fontM());

  lv_style_init(&s_scrollbar);
  lv_style_set_bg_color(&s_scrollbar, col(C_TEXT2));
  lv_style_set_bg_opa(&s_scrollbar, LV_OPA_40);
  lv_style_set_radius(&s_scrollbar, S(2));
  lv_style_set_width(&s_scrollbar, S(3));
  lv_style_set_pad_right(&s_scrollbar, S(2));
  lv_style_set_pad_ver(&s_scrollbar, S(4));
}

lv_style_t* styleScreen() { return &s_screen; }
lv_style_t* styleCard() { return &s_card; }
lv_style_t* styleCardGlass() { return &s_glass; }
lv_style_t* styleButton() { return &s_button; }
lv_style_t* stylePressed() { return &s_pressed; }
lv_style_t* styleSliderMain() { return &s_sliderMain; }
lv_style_t* styleSliderIndicator() { return &s_sliderInd; }
lv_style_t* styleSliderKnob() { return &s_sliderKnob; }
lv_style_t* styleSwitchMain() { return &s_swMain; }
lv_style_t* styleSwitchChecked() { return &s_swChecked; }
lv_style_t* styleSwitchKnob() { return &s_swKnob; }
lv_style_t* styleRollerMain() { return &s_rollerMain; }
lv_style_t* styleRollerSelected() { return &s_rollerSel; }

SkyGradient sky(rc::SkyClass s) { return kSky[s < rc::SKY_COUNT ? s : rc::SKY_CLOUDY_DAY]; }

lv_color_t tempColor(int16_t t10) {
  const int n = sizeof(kTemp) / sizeof(kTemp[0]);
  if (t10 <= kTemp[0].t10) return col(kTemp[0].color);
  if (t10 >= kTemp[n - 1].t10) return col(kTemp[n - 1].color);
  for (int i = 1; i < n; i++) {
    if (t10 <= kTemp[i].t10) {
      int32_t span = kTemp[i].t10 - kTemp[i - 1].t10;
      int32_t pos = t10 - kTemp[i - 1].t10;
      lv_opa_t mix = static_cast<lv_opa_t>((pos * 255) / span);
      return lv_color_mix(col(kTemp[i].color), col(kTemp[i - 1].color), mix);
    }
  }
  return col(C_TEXT);
}

lv_obj_t* makeScreen() {
  lv_obj_t* scr = lv_obj_create(nullptr);
  lv_obj_remove_style_all(scr);
  lv_obj_add_style(scr, &s_screen, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  return scr;
}

lv_style_t* textStyle(const lv_font_t* font, uint32_t color) {
  for (int i = 0; i < s_textCount; i++) {
    if (s_text[i].font == font && s_text[i].color == color) return &s_text[i].style;
  }
  if (s_textCount >= TEXT_STYLES) return nullptr;
  TextStyle& t = s_text[s_textCount++];
  t.font = font;
  t.color = color;
  lv_style_init(&t.style);
  lv_style_set_text_font(&t.style, font);
  lv_style_set_text_color(&t.style, col(color));
  return &t.style;
}

const char* printable(const lv_font_t* font, const char* in, char* out, size_t cap, const char* fallback) {
  if (!cap) return fallback;
  size_t n = 0;
  bool any = false;
  uint32_t i = 0;
  while (in && in[i]) {
    uint32_t start = i;
    uint32_t c = lv_text_encoded_next(in, &i);
    lv_font_glyph_dsc_t g;
    bool space = c == ' ' || c == 0xA0;
    if (!space && (c < 0x20 || !lv_font_get_glyph_dsc(font, &g, c, 0))) continue;
    if (space && (n == 0 || out[n - 1] == ' ')) continue;
    size_t len = space ? 1 : i - start;
    if (n + len + 1 > cap) break;
    if (space) out[n] = ' ';
    else memcpy(out + n, in + start, len);
    n += len;
    any = any || !space;
  }
  while (n > 0 && out[n - 1] == ' ') n--;
  out[n] = 0;
  return any ? out : fallback;
}

void oneLine(lv_obj_t* label) {
  lv_obj_set_height(label, lv_font_get_line_height(lv_obj_get_style_text_font(label, LV_PART_MAIN)));
  lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
}

lv_obj_t* makeLabel(lv_obj_t* parent, const lv_font_t* font, uint32_t color) {
  lv_obj_t* l = lv_label_create(parent);
  lv_style_t* st = textStyle(font, color);
  if (st) {
    lv_obj_add_style(l, st, 0);
  } else {
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, col(color), 0);
  }
  lv_label_set_text_static(l, "");
  return l;
}

lv_obj_t* makeBox(lv_obj_t* parent) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}

lv_obj_t* makeButton(lv_obj_t* parent, const char* text, const lv_font_t* font) {
  lv_obj_t* b = lv_button_create(parent);
  lv_obj_remove_style_all(b);
  lv_obj_add_style(b, &s_button, 0);
  lv_obj_add_style(b, &s_pressed, LV_STATE_PRESSED);
  lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  if (text) {
    lv_obj_t* l = lv_label_create(b);
    lv_obj_set_style_text_font(l, font, 0);
    lv_label_set_text(l, text);
    lv_obj_center(l);
  }
  return b;
}

void styleSlider(lv_obj_t* s) {
  lv_obj_remove_style_all(s);
  lv_obj_add_style(s, &s_sliderMain, LV_PART_MAIN);
  lv_obj_add_style(s, &s_sliderInd, LV_PART_INDICATOR);
  lv_obj_add_style(s, &s_sliderKnob, LV_PART_KNOB);
  lv_obj_remove_flag(s, LV_OBJ_FLAG_GESTURE_BUBBLE);
  lv_obj_set_ext_click_area(s, S(14));
}

void styleSwitch(lv_obj_t* sw) {
  lv_obj_remove_style_all(sw);
  lv_obj_add_style(sw, &s_swMain, LV_PART_MAIN);
  lv_obj_add_style(sw, &s_swChecked, LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_add_style(sw, &s_swKnob, LV_PART_KNOB);
  lv_obj_set_size(sw, S(48), S(26));
  lv_obj_set_ext_click_area(sw, S(10));
}

void styleRoller(lv_obj_t* r) {
  lv_obj_remove_style_all(r);
  lv_obj_add_style(r, &s_rollerMain, LV_PART_MAIN);
  lv_obj_add_style(r, &s_rollerSel, LV_PART_SELECTED);
  lv_obj_remove_flag(r, LV_OBJ_FLAG_GESTURE_BUBBLE);
}

void setText(lv_obj_t* l, const char* t) {
  if (!l) return;
  const char* cur = lv_label_get_text(l);
  if (!cur || strcmp(cur, t) != 0) lv_label_set_text(l, t);
}

lv_obj_t* makeCard(lv_obj_t* parent, int32_t w, int32_t h) {
  lv_obj_t* c = makeBox(parent);
  lv_obj_add_style(c, &s_card, 0);
  lv_obj_set_size(c, w, h);
  return c;
}

void styleScrollbar(lv_obj_t* obj) {
  lv_obj_add_style(obj, &s_scrollbar, LV_PART_SCROLLBAR);
  lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_ACTIVE);
}

}
