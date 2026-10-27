#include "rumy.h"

#include <string.h>

#include "accessories.h"
#include "media_proto.h"
#include "rc_types.h"
#include "ui/theme.h"

namespace rumy {

namespace {

constexpr int Q = 4;
constexpr uint32_t FRAME_MS = 33;
constexpr int32_t GLOW = theme::S(6);
constexpr uint32_t SLEEP_FRAME_MS = 50;
constexpr uint32_t Z_PERIOD = 3600;
constexpr uint32_t Z_COUNT = 3;
constexpr int32_t Z_WOBBLE = theme::S(3);
constexpr uint32_t GROOVE_DEFAULT_MS = 1100;
constexpr uint32_t NOTE_PERIOD = 2400;
constexpr uint32_t NOTE_COUNT = 2;
constexpr int32_t NOTE_WOBBLE = theme::S(2);
constexpr uint32_t FX_SHIVER_MS = 700;
constexpr uint32_t FX_SHIVER_CYCLE = 2600;
constexpr uint32_t FX_STORM_CYCLE = 4800;
constexpr uint32_t FX_GLINT_CYCLE = 5200;
constexpr uint32_t FX_GLINT_MS = 450;
constexpr uint32_t FX_SWEAT_CYCLE = 3200;
constexpr uint32_t FX_SWEAT_MS = 2200;
constexpr int FX_BOLT = 8;
constexpr int FX_FLASH = 9;
constexpr uint32_t DOZE_CYCLE = 5600;
constexpr uint32_t TEAR_CYCLE = 4200;
constexpr uint32_t TEAR_MS = 2600;
constexpr uint16_t kSpread[10] = {120, 610, 330, 880, 470, 760, 210, 940, 540, 40};
constexpr uint16_t kPhase[10] = {0, 430, 170, 720, 560, 90, 840, 300, 650, 980};

struct Params {
  int16_t w;
  int16_t h;
  int16_t dx;
  int16_t dy;
  int16_t tired;
  int16_t angry;
  int16_t happy;
  int16_t flat;
  int16_t head;
  int16_t pinch;
};

struct Face {
  lv_timer_t* timer;
  lv_color_t color;
  lv_color_t bg;
  int16_t eye;
  uint8_t mood;
  uint8_t acc;
  uint8_t animated;
  uint8_t blinking;
  uint8_t music;
  uint8_t fx;
  Params cur;
  Params tgt;
  int16_t gazeX;
  int16_t gazeY;
  int16_t bounce;
  int16_t flicker;
  int16_t swayX;
  int16_t bobY;
  int16_t doze;
  int16_t wide;
  uint32_t now;
  uint32_t blinkStart;
  uint32_t nextBlink;
  uint32_t nextLook;
  uint32_t nextHop;
  uint32_t hopStart;
  uint32_t flickerUntil;
  uint32_t beatPeriod;
  uint32_t beatAnchor;
  uint32_t fxLast;
  lv_color_t phoneColor;
  lv_color_t noteColor;
  lv_area_t lastBox;
  uint32_t lastSig;
};

Face* faceOf(lv_obj_t* obj) { return static_cast<Face*>(lv_obj_get_user_data(obj)); }

int16_t scaleE(const Face& f, int32_t permille) { return static_cast<int16_t>((f.eye * Q * permille) / 1000); }

Params moodParams(const Face& f, uint8_t m) {
  Params p;
  memset(&p, 0, sizeof(p));
  p.w = scaleE(f, 1000);
  p.h = scaleE(f, 1000);
  switch (m) {
    case HAPPY:
      p.happy = scaleE(f, 620);
      break;
    case SLEEPY:
      p.h = scaleE(f, 740);
      p.tired = scaleE(f, 480);
      p.dy = scaleE(f, 100);
      break;
    case ASLEEP:
      p.h = static_cast<int16_t>(theme::S(6) * Q);
      p.dy = scaleE(f, 160);
      break;
    case SAD:
      p.w = scaleE(f, 900);
      p.h = scaleE(f, 700);
      p.tired = scaleE(f, 550);
      p.dy = scaleE(f, 180);
      break;
    case SURPRISED:
      p.w = scaleE(f, 1150);
      p.h = scaleE(f, 1360);
      break;
    case LOOKUP:
      p.dy = -scaleE(f, 190);
      p.h = scaleE(f, 900);
      p.w = scaleE(f, 960);
      break;
    case CELEBRATE:
      p.happy = scaleE(f, 640);
      p.w = scaleE(f, 1080);
      break;
    case CALM:
      p.flat = scaleE(f, 450);
      p.dy = scaleE(f, 50);
      break;
    case MUSIC:
      p.happy = scaleE(f, 560);
      p.head = scaleE(f, 120);
      break;
    case SQUINT:
      p.w = scaleE(f, 1060);
      p.h = scaleE(f, 520);
      p.angry = scaleE(f, 180);
      p.dy = scaleE(f, 40);
      break;
    case SCARED:
      p.w = scaleE(f, 950);
      p.h = scaleE(f, 1250);
      p.tired = scaleE(f, 320);
      p.dy = -scaleE(f, 40);
      p.pinch = scaleE(f, 80);
      break;
    case SQUEEZE:
      p.w = scaleE(f, 700);
      p.h = scaleE(f, 620);
      p.dy = scaleE(f, 20);
      break;
    default:
      break;
  }
  return p;
}

int16_t ease(int16_t cur, int16_t tgt) {
  int32_t d = tgt - cur;
  if (d == 0) return cur;
  int32_t step = d / 3;
  if (step == 0) step = d > 0 ? 1 : -1;
  return static_cast<int16_t>(cur + step);
}

void geometry(const Face& f, const lv_area_t& box, bool left, lv_area_t& eye, int32_t& radius) {
  int32_t w = f.cur.w / Q;
  int32_t h = f.cur.h / Q;
  if (f.blinking) {
    uint32_t t = f.now - f.blinkStart;
    int32_t k = t < 70 ? 100 - static_cast<int32_t>(t) * 90 / 70 : 10 + (static_cast<int32_t>(t) - 70) * 90 / 80;
    if (k > 100) k = 100;
    if (k < 8) k = 8;
    h = h * k / 100;
  }
  h = h * f.doze / 1000;
  w = w * f.wide / 1000;
  if (h < theme::S(4)) h = theme::S(4);
  int32_t gap = f.eye * 78 / 100;
  int32_t cx = (box.x1 + box.x2) / 2 + (left ? -(f.eye / 2 + gap / 2) : (f.eye / 2 + gap / 2));
  int32_t cy = (box.y1 + box.y2) / 2;
  cx += (f.cur.dx + f.gazeX + (left ? f.cur.pinch : -f.cur.pinch)) / Q + f.flicker + f.swayX;
  cy += (f.cur.dy + f.gazeY + f.cur.head) / Q + f.bounce + f.bobY;
  eye.x1 = cx - w / 2;
  eye.x2 = eye.x1 + w - 1;
  eye.y1 = cy - h / 2;
  eye.y2 = eye.y1 + h - 1;
  int32_t r = (w < h ? w : h) * 30 / 100;
  if (r > h / 2) r = h / 2;
  radius = r;
}

void rect(lv_layer_t* layer, const lv_area_t& a, lv_color_t c, lv_opa_t opa, int32_t r) {
  lv_draw_rect_dsc_t d;
  lv_draw_rect_dsc_init(&d);
  d.bg_color = c;
  d.bg_opa = opa;
  d.radius = r;
  d.border_width = 0;
  lv_draw_rect(layer, &d, &a);
}

void tri(lv_layer_t* layer, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, lv_color_t c,
         lv_opa_t opa = LV_OPA_COVER) {
  lv_draw_triangle_dsc_t t;
  lv_draw_triangle_dsc_init(&t);
  t.p[0].x = x0;
  t.p[0].y = y0;
  t.p[1].x = x1;
  t.p[1].y = y1;
  t.p[2].x = x2;
  t.p[2].y = y2;
  t.color = c;
  t.opa = opa;
  lv_draw_triangle(layer, &t);
}

void line(lv_layer_t* layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, lv_color_t c, lv_opa_t opa,
          int32_t w, bool round);

lv_color_t eyeColor(const Face& f) {
  return f.fx == FX_SHIVER ? lv_color_mix(lv_color_hex(0xCFF4FF), f.color, 120) : f.color;
}

void drawChevron(lv_layer_t* layer, const Face& f, const lv_area_t& e, bool left) {
  lv_color_t c = eyeColor(f);
  int32_t lw = LV_MAX(theme::S(4), f.eye * 16 / 100);
  int32_t x1 = e.x1 + lw / 2;
  int32_t x2 = e.x2 - lw / 2;
  int32_t y1 = e.y1 + lw / 2;
  int32_t y2 = e.y2 - lw / 2;
  int32_t cy = (e.y1 + e.y2) / 2;
  int32_t tipX = left ? x2 : x1;
  int32_t tailX = left ? x1 : x2;
  for (int pass = 0; pass < 2; pass++) {
    int32_t w = pass ? lw : lw + GLOW;
    lv_opa_t opa = pass ? LV_OPA_COVER : 40;
    line(layer, tailX, y1, tipX, cy, c, opa, w, true);
    line(layer, tipX, cy, tailX, y2, c, opa, w, true);
  }
}

void drawEye(lv_layer_t* layer, const Face& f, const lv_area_t& box, bool left) {
  lv_area_t e;
  int32_t r;
  geometry(f, box, left, e, r);
  if (f.mood == SQUEEZE) {
    drawChevron(layer, f, e, left);
    return;
  }
  lv_color_t col = eyeColor(f);
  lv_color_t bg = f.bg;
  lv_area_t g2 = {e.x1 - GLOW, e.y1 - GLOW, e.x2 + GLOW, e.y2 + GLOW};
  lv_area_t g1 = {e.x1 - GLOW / 2, e.y1 - GLOW / 2, e.x2 + GLOW / 2, e.y2 + GLOW / 2};
  rect(layer, g2, col, 22, r + GLOW);
  rect(layer, g1, col, 48, r + GLOW / 2);
  rect(layer, e, col, LV_OPA_COVER, r);

  int32_t h = e.y2 - e.y1 + 1;
  int32_t m = GLOW + 1;
  int32_t tired = f.cur.tired / Q * h / (f.cur.h / Q > 0 ? f.cur.h / Q : 1);
  int32_t angry = f.cur.angry / Q;
  int32_t happy = f.cur.happy / Q * h / (f.cur.h / Q > 0 ? f.cur.h / Q : 1);
  int32_t flat = f.cur.flat / Q * h / (f.cur.h / Q > 0 ? f.cur.h / Q : 1);
  if (tired > 1) {
    if (left) tri(layer, e.x1 - m, e.y1 - m, e.x2 + m, e.y1 - m, e.x1 - m, e.y1 + tired, bg);
    else tri(layer, e.x1 - m, e.y1 - m, e.x2 + m, e.y1 - m, e.x2 + m, e.y1 + tired, bg);
  }
  if (angry > 1) {
    if (left) tri(layer, e.x1 - m, e.y1 - m, e.x2 + m, e.y1 - m, e.x2 + m, e.y1 + angry, bg);
    else tri(layer, e.x1 - m, e.y1 - m, e.x2 + m, e.y1 - m, e.x1 - m, e.y1 + angry, bg);
  }
  if (flat > 1) {
    lv_area_t a = {e.x1 - m, e.y1 - m, e.x2 + m, e.y1 + flat};
    rect(layer, a, bg, LV_OPA_COVER, 0);
  }
  if (happy > 1) {
    lv_area_t a = {e.x1 - m, e.y2 - happy + 1, e.x2 + m, e.y2 + h + m};
    rect(layer, a, bg, LV_OPA_COVER, r + theme::S(2));
  }
}

struct Phones {
  lv_area_t cupL;
  lv_area_t cupR;
  int32_t top;
  int32_t base;
  int32_t r;
  int32_t w;
};

Phones phones(const Face& f, const lv_area_t& box) {
  int32_t e = f.eye;
  int32_t gap = e * 78 / 100;
  int32_t hx = (box.x1 + box.x2) / 2 + f.flicker + f.swayX;
  int32_t hy = (box.y1 + box.y2) / 2 + f.cur.head / Q + f.bounce + f.bobY;
  int32_t cw = LV_MAX(theme::S(9), e * 22 / 100);
  int32_t ch = e * 58 / 100;
  int32_t inner = e + gap / 2 + theme::S(3);
  Phones p;
  p.cupL = {hx - inner - cw, hy - ch / 2, hx - inner - 1, hy - ch / 2 + ch - 1};
  p.cupR = {hx + inner + 1, hy - ch / 2, hx + inner + cw, hy - ch / 2 + ch - 1};
  p.w = LV_MAX(theme::S(3), e * 7 / 100);
  p.top = hy - e / 2 - GLOW - p.w - 1;
  if (p.top < box.y1 + p.w / 2 + 1) p.top = box.y1 + p.w / 2 + 1;
  p.base = p.cupL.y1 + theme::S(4);
  p.r = LV_MAX(theme::S(8), p.base - p.top);
  return p;
}

struct ZPath {
  int32_t x;
  int32_t y;
  int32_t rise;
  int32_t drift;
  int32_t sMin;
  int32_t sMax;
};

ZPath zPath(const Face& f, const lv_area_t& box) {
  lv_area_t re;
  int32_t r;
  geometry(f, box, false, re, r);
  ZPath p;
  p.x = re.x2 - theme::S(4);
  p.y = re.y1 - theme::S(4);
  if (f.music) {
    Phones ph = phones(f, box);
    p.x = ph.cupR.x2 + theme::S(3);
    p.y = ph.cupR.y1 + theme::S(4);
  }
  p.rise = f.eye * 62 / 100;
  p.drift = f.eye * 45 / 100;
  p.sMin = f.eye * 9 / 100;
  p.sMax = f.eye * 25 / 100;
  if (p.sMin < theme::S(4)) p.sMin = theme::S(4);
  if (p.sMax < p.sMin + theme::S(4)) p.sMax = p.sMin + theme::S(4);
  return p;
}

void zBox(const Face& f, const lv_area_t& box, lv_area_t& out) {
  ZPath p = zPath(f, box);
  int32_t pad = Z_WOBBLE + theme::S(4) + GLOW;
  out.x1 = p.x - pad;
  out.x2 = p.x + p.drift + p.sMax + pad;
  out.y1 = p.y - p.rise - p.sMax - pad;
  out.y2 = p.y + pad;
}

void line(lv_layer_t* layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, lv_color_t c, lv_opa_t opa,
          int32_t w, bool round = true) {
  lv_draw_line_dsc_t d;
  lv_draw_line_dsc_init(&d);
  d.p1.x = x1;
  d.p1.y = y1;
  d.p2.x = x2;
  d.p2.y = y2;
  d.color = c;
  d.opa = opa;
  d.width = w;
  d.round_start = round ? 1 : 0;
  d.round_end = round ? 1 : 0;
  lv_draw_line(layer, &d);
}

void drawZ(lv_layer_t* layer, int32_t x, int32_t y, int32_t s, lv_color_t c, lv_opa_t opa, int32_t w) {
  line(layer, x, y, x + s, y, c, opa, w);
  line(layer, x + s, y, x, y + s, c, opa, w);
  line(layer, x, y + s, x + s, y + s, c, opa, w);
}

void drawZs(lv_layer_t* layer, const Face& f, const lv_area_t& box) {
  ZPath p = zPath(f, box);
  for (uint32_t i = 0; i < Z_COUNT; i++) {
    uint32_t t = (f.now + i * (Z_PERIOD / Z_COUNT)) % Z_PERIOD;
    int32_t k = static_cast<int32_t>(t * 1000 / Z_PERIOD);
    int32_t fade = LV_MIN(LV_MIN(k * 1000 / 150, (1000 - k) * 1000 / 380), 1000);
    if (fade <= 0) continue;
    int32_t s = p.sMin + (p.sMax - p.sMin) * k / 1000;
    int32_t wob = Z_WOBBLE * lv_trigo_sin(static_cast<int16_t>(k * 540 / 1000)) / LV_TRIGO_SIN_MAX;
    int32_t x = p.x + p.drift * k / 1000 + wob;
    int32_t y = p.y - p.rise * k / 1000 - s;
    int32_t w = theme::S(2) + s / 8;
    lv_opa_t opa = static_cast<lv_opa_t>(230 * fade / 1000);
    drawZ(layer, x, y, s, f.color, static_cast<lv_opa_t>(opa / 4), w + theme::S(4));
    drawZ(layer, x, y, s, f.color, opa, w);
  }
}

void arc(lv_layer_t* layer, int32_t cx, int32_t cy, int32_t r, int32_t w, int32_t a0, int32_t a1, lv_color_t c,
         lv_opa_t opa, bool round) {
  lv_draw_arc_dsc_t d;
  lv_draw_arc_dsc_init(&d);
  d.center.x = cx;
  d.center.y = cy;
  d.radius = static_cast<uint16_t>(r + w / 2);
  d.width = w;
  d.start_angle = a0;
  d.end_angle = a1;
  d.color = c;
  d.opa = opa;
  d.rounded = round ? 1 : 0;
  lv_draw_arc(layer, &d);
}

void band(lv_layer_t* layer, const Phones& p, int32_t w, lv_color_t c, lv_opa_t opa, bool round) {
  int32_t xl = (p.cupL.x1 + p.cupL.x2) / 2;
  int32_t xr = (p.cupR.x1 + p.cupR.x2) / 2;
  arc(layer, xl + p.r, p.base, p.r, w, 180, 270, c, opa, round);
  arc(layer, xr - p.r, p.base, p.r, w, 270, 360, c, opa, round);
  line(layer, xl + p.r, p.base - p.r, xr - p.r, p.base - p.r, c, opa, w, round);
}

void cup(lv_layer_t* layer, const lv_area_t& a, bool left, lv_color_t c) {
  int32_t w = a.x2 - a.x1 + 1;
  int32_t rad = w / 2;
  lv_area_t g2 = {a.x1 - GLOW, a.y1 - GLOW, a.x2 + GLOW, a.y2 + GLOW};
  lv_area_t g1 = {a.x1 - GLOW / 2, a.y1 - GLOW / 2, a.x2 + GLOW / 2, a.y2 + GLOW / 2};
  rect(layer, g2, c, 22, rad + GLOW);
  rect(layer, g1, c, 48, rad + GLOW / 2);
  rect(layer, a, c, LV_OPA_COVER, rad);
  int32_t s = LV_MAX(theme::S(3), w / 3);
  lv_area_t pad = left ? lv_area_t{a.x2 - s + 1, a.y1 + theme::S(4), a.x2, a.y2 - theme::S(4)}
                           : lv_area_t{a.x1, a.y1 + theme::S(4), a.x1 + s - 1, a.y2 - theme::S(4)};
  rect(layer, pad, lv_color_mix(c, lv_color_black(), 90), LV_OPA_COVER, s / 2);
  int32_t sx = left ? a.x1 + theme::S(2) : a.x2 - theme::S(3);
  lv_area_t shine = {sx, a.y1 + rad, sx + theme::S(2) - 1, a.y2 - rad};
  if (shine.y2 > shine.y1) rect(layer, shine, lv_color_white(), 120, 1);
}

void drawPhones(lv_layer_t* layer, const Face& f, const lv_area_t& box) {
  Phones p = phones(f, box);
  band(layer, p, p.w + theme::S(6), f.phoneColor, 40, false);
  band(layer, p, p.w, f.phoneColor, LV_OPA_COVER, true);
  cup(layer, p.cupL, true, f.phoneColor);
  cup(layer, p.cupR, false, f.phoneColor);
}

void phonesBox(const Face& f, const lv_area_t& box, lv_area_t& out) {
  Phones p = phones(f, box);
  out.x1 = p.cupL.x1 - GLOW - theme::S(2);
  out.x2 = p.cupR.x2 + GLOW + theme::S(2);
  out.y1 = p.top - p.w - theme::S(4);
  out.y2 = p.cupL.y2 + GLOW + theme::S(2);
}

struct NotePath {
  int32_t x;
  int32_t y;
  int32_t rise;
  int32_t drift;
  int32_t sMin;
  int32_t sMax;
};

NotePath notePath(const Face& f, const lv_area_t& box) {
  Phones p = phones(f, box);
  NotePath n;
  n.x = p.cupR.x2 + theme::S(3);
  n.y = p.cupR.y2 + theme::S(2);
  n.rise = f.eye * 90 / 100;
  n.drift = 0;
  n.sMin = LV_MAX(theme::S(5), f.eye * 11 / 100);
  n.sMax = LV_MAX(n.sMin + theme::S(2), f.eye * 17 / 100);
  return n;
}

void notesBox(const Face& f, const lv_area_t& box, lv_area_t& out) {
  NotePath n = notePath(f, box);
  int32_t tall = n.sMax / 2 + n.sMax * 21 / 10 + n.sMax / 3;
  out.x1 = n.x - NOTE_WOBBLE - theme::S(3);
  out.x2 = n.x + n.drift + NOTE_WOBBLE + n.sMax * 2 + theme::S(3);
  out.y1 = n.y - n.rise - tall - theme::S(3);
  out.y2 = n.y + theme::S(3);
}

void drawNote(lv_layer_t* layer, int32_t x, int32_t bottom, int32_t s, lv_color_t c, lv_opa_t opa) {
  int32_t hw = s * 135 / 100;
  int32_t lw = LV_MAX(theme::S(2), s / 3);
  int32_t stem = s * 21 / 10;
  lv_area_t head = {x, bottom - s + 1, x + hw - 1, bottom};
  rect(layer, head, c, opa, s / 2);
  int32_t sx = x + hw - 1 - lw / 2;
  int32_t ty = bottom - s / 2 - stem;
  line(layer, sx, bottom - s / 2, sx, ty, c, opa, lw);
  line(layer, sx, ty, sx + s * 6 / 10, ty + s * 8 / 10, c, opa, lw);
}

void drawNotes(lv_layer_t* layer, const Face& f, const lv_area_t& box) {
  NotePath n = notePath(f, box);
  for (uint32_t i = 0; i < NOTE_COUNT; i++) {
    uint32_t t = (f.now + i * (NOTE_PERIOD / NOTE_COUNT)) % NOTE_PERIOD;
    int32_t k = static_cast<int32_t>(t * 1000 / NOTE_PERIOD);
    int32_t fade = LV_MIN(LV_MIN(k * 1000 / 150, (1000 - k) * 1000 / 400), 1000);
    if (fade <= 0) continue;
    int32_t s = n.sMin + (n.sMax - n.sMin) * k / 1000;
    int32_t wob = NOTE_WOBBLE * lv_trigo_sin(static_cast<int16_t>(k * 720 / 1000 % 360)) / LV_TRIGO_SIN_MAX;
    int32_t x = n.x + n.drift * k / 1000 + wob;
    int32_t y = n.y - n.rise * k / 1000;
    drawNote(layer, x, y, s, f.noteColor, static_cast<lv_opa_t>(235 * fade / 1000));
  }
}

bool grooving(const Face& f) { return f.music && f.mood == MUSIC; }

void groove(Face& f) {
  uint32_t period = f.beatPeriod ? f.beatPeriod : GROOVE_DEFAULT_MS;
  int32_t P = static_cast<int32_t>(period);
  int32_t d = static_cast<int32_t>(f.now - f.beatAnchor);
  int32_t m = ((d % P) + P) % P;
  int32_t k = (d - m) / P;
  int32_t p = m * 1000 / P;
  int32_t sway = LV_MAX(theme::S(2), f.eye * 6 / 100);
  int32_t bob = LV_MAX(theme::S(2), f.eye * 8 / 100);
  int32_t ang = ((k & 1) ? 180 : 0) + p * 180 / 1000;
  f.swayX = static_cast<int16_t>(sway * lv_trigo_sin(static_cast<int16_t>(ang % 360)) / LV_TRIGO_SIN_MAX);
  int32_t q = (p + 100) % 1000;
  int32_t g;
  if (q < 200) {
    int32_t u = q * 5;
    g = (u * u / 1000) * (3000 - 2 * u) / 1000;
  } else {
    int32_t u = 1000 - (q - 200) * 1000 / 800;
    g = u * u / 1000;
  }
  f.bobY = static_cast<int16_t>(bob * g / 1000);
}

struct Zone {
  int32_t x1;
  int32_t x2;
  int32_t y1;
  int32_t y2;
  int32_t cx;
  int32_t cy;
  int32_t e;
};

Zone zone(const Face& f, const lv_area_t& box) {
  Zone z;
  z.e = f.eye;
  z.cx = (box.x1 + box.x2) / 2;
  z.cy = (box.y1 + box.y2) / 2;
  z.x1 = z.cx - f.eye * 23 / 10;
  z.x2 = z.cx + f.eye * 23 / 10;
  z.y1 = box.y1;
  z.y2 = box.y2;
  return z;
}

struct Part {
  int32_t x;
  int32_t y;
  int32_t s;
  int32_t w;
  int32_t k;
  lv_opa_t opa;
};

int partCount(uint8_t fx) {
  switch (fx) {
    case FX_DRIZZLE: return 5;
    case FX_RAIN: return 8;
    case FX_STORM: return FX_FLASH + 1;
    case FX_SNOW: return 8;
    case FX_FOG: return 3;
    case FX_WIND: return 5;
    case FX_STARS: return 6;
    case FX_SWEAT: return 4;
    case FX_TEAR: return 1;
    case FX_CONFETTI: return 7;
    case FX_SHIVER: return 2;
    case FX_CLOUD: return 1;
    case FX_SUNGLASSES: return 1;
    default: return 0;
  }
}

int32_t phaseK(uint32_t t, uint32_t period, int i) {
  return static_cast<int32_t>((t + kPhase[i] * period / 1000U) % period * 1000U / period);
}

int32_t fadeK(int32_t k, int32_t in, int32_t out) { return LV_MAX(0, LV_MIN(LV_MIN(k * 1000 / in, (1000 - k) * 1000 / out), 1000)); }

void lens(const Face& f, const lv_area_t& box, bool left, lv_area_t& out) {
  Face g = f;
  g.blinking = 0;
  lv_area_t e;
  int32_t r;
  geometry(g, box, left, e, r);
  int32_t cy = (e.y1 + e.y2) / 2;
  int32_t E = f.eye;
  out.x1 = e.x1 - E * 9 / 100;
  out.x2 = e.x2 + E * 9 / 100;
  out.y1 = cy - E * 36 / 100;
  out.y2 = cy + E * 30 / 100;
}

bool part(const Face& f, const lv_area_t& box, uint8_t fx, int i, uint32_t t, Part& p) {
  Zone z = zone(f, box);
  int32_t e = z.e;
  p.w = 0;
  p.k = 0;
  switch (fx) {
    case FX_DRIZZLE:
    case FX_RAIN:
    case FX_STORM: {
      if (fx == FX_STORM && i >= FX_BOLT) {
        uint32_t c = t % FX_STORM_CYCLE;
        if (!(c < 90 || (c >= 170 && c < 260))) return false;
        if (i == FX_FLASH) {
          p.x = box.x1;
          p.y = box.y1;
          p.s = box.x2;
          p.w = box.y2;
          p.opa = 46;
          return true;
        }
        p.x = z.cx - e * 175 / 100;
        p.y = z.y1 + theme::S(2);
        p.s = (z.y2 - z.y1) * 85 / 100;
        p.w = LV_MAX(theme::S(3), e / 14);
        p.opa = LV_OPA_COVER;
        return true;
      }
      uint32_t period = fx == FX_DRIZZLE ? 1000 : fx == FX_RAIN ? 650 : 520;
      int32_t k = phaseK(t, period, i);
      p.s = e * (fx == FX_DRIZZLE ? 20 : 30) / 100;
      p.w = LV_MAX(theme::S(2), e / 28);
      p.x = z.x1 + (z.x2 - z.x1) * kSpread[i] / 1000;
      p.y = z.y1 - p.s + (z.y2 - z.y1 + p.s) * k / 1000;
      p.k = k;
      p.opa = fx == FX_DRIZZLE ? 170 : 210;
      return true;
    }
    case FX_SNOW: {
      int32_t k = phaseK(t, 3200, i);
      p.s = LV_MAX(theme::S(4), e * 11 / 100);
      p.w = LV_MAX(1, p.s / 3);
      int32_t amp = e * 16 / 100;
      int32_t a = (k * 720 / 1000 + kPhase[i] * 360 / 1000) % 360;
      p.x = z.x1 + (z.x2 - z.x1) * kSpread[i] / 1000 + amp * lv_trigo_sin(static_cast<int16_t>(a)) / LV_TRIGO_SIN_MAX;
      p.y = z.y1 - p.s + (z.y2 - z.y1 + 2 * p.s) * k / 1000;
      p.opa = 235;
      return true;
    }
    case FX_FOG: {
      static const int16_t kRow[3] = {-30, 8, 40};
      int32_t k = phaseK(t, 9000, i * 3);
      if (i == 1) k = 1000 - k;
      p.s = e * 17 / 10;
      p.w = LV_MAX(theme::S(5), e * 17 / 100);
      p.x = z.x1 + (z.x2 - z.x1 - p.s) * k / 1000;
      p.y = z.cy + e * kRow[i] / 100 - p.w / 2;
      p.opa = static_cast<lv_opa_t>(130 * fadeK(k, 250, 250) / 1000);
      return p.opa > 0;
    }
    case FX_WIND: {
      if (i == 4) {
        int32_t k = phaseK(t, 3400, 3);
        p.s = LV_MAX(theme::S(5), e * 17 / 100);
        p.x = z.x2 + p.s - (z.x2 - z.x1 + 2 * p.s) * k / 1000;
        p.y = z.cy - e * 20 / 100 + e * 30 / 100 * lv_trigo_sin(static_cast<int16_t>(k * 540 / 1000 % 360)) / LV_TRIGO_SIN_MAX;
        p.k = k * 720 / 1000 % 360;
        p.opa = static_cast<lv_opa_t>(235 * fadeK(k, 120, 120) / 1000);
        return p.opa > 0;
      }
      static const int16_t kRow[4] = {-55, -10, 30, 65};
      int32_t k = phaseK(t, 1500, i * 2);
      p.s = e * 9 / 10;
      p.w = LV_MAX(theme::S(2), e / 30);
      p.x = z.x2 + p.s - (z.x2 - z.x1 + 2 * p.s) * k / 1000;
      p.y = z.cy + e * kRow[i] / 100;
      p.opa = static_cast<lv_opa_t>(160 * fadeK(k, 200, 250) / 1000);
      return p.opa > 0;
    }
    case FX_STARS: {
      if (i == 5) {
        p.s = e * 22 / 100;
        p.x = z.cx - e * 195 / 100;
        p.y = z.cy - e * 5 / 100;
        p.opa = LV_OPA_COVER;
        return true;
      }
      static const int16_t kX[5] = {60, 150, 860, 945, 500};
      static const int16_t kY[5] = {-40, 38, -55, 28, -80};
      int32_t k = phaseK(t, 2400, i * 2);
      int32_t v = (lv_trigo_sin(static_cast<int16_t>(k * 360 / 1000)) + LV_TRIGO_SIN_MAX) * 1000 / (2 * LV_TRIGO_SIN_MAX);
      int32_t s0 = LV_MAX(theme::S(4), e * 12 / 100);
      p.s = s0 * (55 + 45 * v / 1000) / 100;
      p.w = LV_MAX(1, s0 / 4);
      p.x = z.x1 + (z.x2 - z.x1) * kX[i] / 1000;
      p.y = z.cy + e * kY[i] / 100;
      if (p.y - s0 < z.y1) p.y = z.y1 + s0;
      p.opa = static_cast<lv_opa_t>(90 + 165 * v / 1000);
      return true;
    }
    case FX_SWEAT: {
      if (i > 0) {
        int32_t k = phaseK(t, 1800, (i - 1) * 3);
        p.s = e * 28 / 100;
        p.w = LV_MAX(theme::S(2), e / 24);
        p.x = z.cx + (i - 2) * e * 89 / 100;
        p.y = z.cy - e * 30 / 100 - e * 25 / 100 * k / 1000;
        p.k = static_cast<int32_t>(t % 900U * 360U / 900U);
        p.opa = static_cast<lv_opa_t>(220 * fadeK(k, 250, 450) / 1000);
        return p.opa > 0;
      }
      uint32_t c = t % FX_SWEAT_CYCLE;
      if (c >= FX_SWEAT_MS) return false;
      int32_t k = static_cast<int32_t>(c * 1000 / FX_SWEAT_MS);
      p.s = LV_MAX(theme::S(3), e * 9 / 100);
      p.x = z.cx + e * 147 / 100 + p.s;
      p.y = z.cy - e * 30 / 100 + e * 50 / 100 * k / 1000;
      p.opa = static_cast<lv_opa_t>(230 * fadeK(k, 150, 200) / 1000);
      return p.opa > 0;
    }
    case FX_SHIVER: {
      uint32_t c = t % FX_SHIVER_CYCLE;
      uint32_t start = FX_SHIVER_MS + static_cast<uint32_t>(i) * 260U;
      if (c < start || c >= start + 1100U) return false;
      int32_t u = static_cast<int32_t>((c - start) * 1000U / 1100U);
      p.s = e * 9 / 100 + e * 14 / 100 * u / 1000;
      p.x = z.cx + (i ? e * 12 / 100 : -e * 6 / 100);
      p.y = z.cy + e * 60 / 100 - e * 28 / 100 * u / 1000;
      p.opa = static_cast<lv_opa_t>(150 * (1000 - u) / 1000);
      return p.opa > 0;
    }
    case FX_CLOUD: {
      p.s = e * 85 / 100;
      p.w = p.s * 55 / 100;
      int32_t bob = theme::S(2) * lv_trigo_sin(static_cast<int16_t>(t % 4000U * 360U / 4000U)) / LV_TRIGO_SIN_MAX;
      int32_t drift = theme::S(10) * lv_trigo_sin(static_cast<int16_t>(t % 9000U * 360U / 9000U)) / LV_TRIGO_SIN_MAX;
      p.x = z.cx - e * 169 / 100 - p.s + drift;
      p.y = z.cy - e * 5 / 100 + bob;
      p.opa = LV_OPA_COVER;
      return true;
    }
    case FX_TEAR: {
      uint32_t c = t % TEAR_CYCLE;
      if (c >= TEAR_MS) return false;
      int32_t k = static_cast<int32_t>(c * 1000 / TEAR_MS);
      p.s = LV_MAX(theme::S(3), e * 10 / 100);
      p.x = z.cx - e * 52 / 100;
      p.y = z.cy + e * 48 / 100 + e * 30 / 100 * k / 1000;
      p.opa = static_cast<lv_opa_t>(230 * fadeK(k, 150, 250) / 1000);
      return p.opa > 0;
    }
    case FX_CONFETTI: {
      int32_t k = phaseK(t, 3000, i);
      p.s = LV_MAX(theme::S(3), e * 10 / 100);
      int32_t amp = e * 12 / 100;
      int32_t a = (k * 540 / 1000 + kPhase[i] * 360 / 1000) % 360;
      p.x = z.x1 + (z.x2 - z.x1) * kSpread[i] / 1000 + amp * lv_trigo_sin(static_cast<int16_t>(a)) / LV_TRIGO_SIN_MAX;
      p.y = z.y1 - p.s * 2 + (z.y2 - z.y1 + 4 * p.s) * k / 1000;
      p.w = static_cast<int32_t>(((t / 160U) + static_cast<uint32_t>(i)) & 1U);
      p.opa = LV_OPA_COVER;
      return true;
    }
    case FX_SUNGLASSES: {
      uint32_t c = t % FX_GLINT_CYCLE;
      if (c >= FX_GLINT_MS) return false;
      p.s = static_cast<int32_t>(c * 1000 / FX_GLINT_MS);
      p.x = 0;
      p.y = 0;
      p.opa = static_cast<lv_opa_t>(170 * fadeK(p.s, 250, 250) / 1000);
      return p.opa > 0;
    }
    default:
      return false;
  }
}

void partRect(const Face& f, const lv_area_t& box, uint8_t fx, int i, const Part& p, lv_area_t& a) {
  int32_t m = theme::S(2);
  int32_t e = f.eye;
  int32_t x1 = p.x, y1 = p.y, x2 = p.x, y2 = p.y;
  switch (fx) {
    case FX_DRIZZLE:
    case FX_RAIN:
    case FX_STORM:
      if (fx == FX_STORM && i == FX_FLASH) {
        x1 = p.x;
        y1 = p.y;
        x2 = p.s;
        y2 = p.w;
      } else if (fx == FX_STORM && i == FX_BOLT) {
        int32_t b = e * 45 / 100;
        x1 = p.x - b * 8 / 10 - 3 * p.w;
        x2 = p.x + 3 * p.w;
        y1 = p.y - 3 * p.w;
        y2 = p.y + p.s + 3 * p.w;
      } else {
        x1 = p.x - p.s / 4 - p.w;
        x2 = p.x + p.w;
        y1 = p.y - p.w;
        y2 = p.y + p.s + p.w;
        if (p.k > 800) {
          int32_t sp = e * 9 / 100;
          x1 -= sp;
          x2 += sp;
          y2 = LV_MAX(y2, box.y2);
        }
      }
      break;
    case FX_SNOW:
    case FX_SHIVER:
      x1 = p.x - p.s - p.w;
      x2 = p.x + p.s + p.w;
      y1 = p.y - p.s - p.w;
      y2 = p.y + p.s + p.w;
      break;
    case FX_FOG:
      x2 = p.x + p.s;
      y1 = p.y - p.w / 2;
      y2 = p.y + p.w + p.w / 2;
      break;
    case FX_WIND: {
      if (i == 4) {
        x1 = p.x - p.s;
        x2 = p.x + p.s;
        y1 = p.y - p.s;
        y2 = p.y + p.s;
        break;
      }
      int32_t r = e * 9 / 100;
      x1 = p.x - r - p.w;
      x2 = p.x + p.s + p.w;
      y1 = p.y - 2 * r - p.w;
      y2 = p.y + p.w;
      break;
    }
    case FX_STARS: {
      if (i == 5) {
        x1 = p.x - p.s * 3 / 2;
        x2 = p.x + p.s * 3 / 2;
        y1 = p.y - p.s * 3 / 2;
        y2 = p.y + p.s * 3 / 2;
        break;
      }
      int32_t s0 = LV_MAX(theme::S(4), e * 12 / 100);
      x1 = p.x - s0;
      x2 = p.x + s0;
      y1 = p.y - s0;
      y2 = p.y + s0;
      break;
    }
    case FX_SWEAT:
    case FX_TEAR:
      if (fx == FX_SWEAT && i > 0) {
        int32_t amp = e * 5 / 100 + p.w;
        x1 = p.x - amp;
        x2 = p.x + amp;
        y1 = p.y - p.s - p.w;
        y2 = p.y + p.w;
        break;
      }
      x1 = p.x - p.s;
      x2 = p.x + p.s;
      y1 = p.y - 2 * p.s;
      y2 = p.y + p.s;
      break;
    case FX_CONFETTI:
      x1 = p.x - p.s * 2;
      x2 = p.x + p.s * 2;
      y1 = p.y - p.s * 2;
      y2 = p.y + p.s * 2;
      break;
    case FX_CLOUD:
      x2 = p.x + p.s;
      y2 = p.y + p.w;
      break;
    case FX_SUNGLASSES: {
      lv_area_t l;
      lens(f, box, true, l);
      x1 = l.x1;
      x2 = l.x2;
      y1 = l.y1;
      y2 = l.y2;
      break;
    }
    default:
      break;
  }
  a.x1 = x1 - m;
  a.y1 = y1 - m;
  a.x2 = x2 + m;
  a.y2 = y2 + m;
}

void drawBolt(lv_layer_t* layer, const Part& p, int32_t e) {
  int32_t b = e * 45 / 100;
  const int32_t px[4] = {p.x, p.x - b * 6 / 10, p.x - b / 10, p.x - b * 8 / 10};
  const int32_t py[4] = {p.y, p.y + p.s * 48 / 100, p.y + p.s * 48 / 100, p.y + p.s};
  lv_color_t c = lv_color_hex(0xFFE14D);
  for (int g = 0; g < 2; g++) {
    int32_t w = g ? p.w : p.w * 3;
    lv_opa_t o = g ? LV_OPA_COVER : 70;
    for (int k = 0; k < 3; k++) line(layer, px[k], py[k], px[k + 1], py[k + 1], c, o, w);
  }
}

void drawGlasses(lv_layer_t* layer, const Face& f, const lv_area_t& box) {
  int32_t E = f.eye;
  lv_area_t l, r;
  lens(f, box, true, l);
  lens(f, box, false, r);
  lv_color_t frame = lv_color_hex(0x48525E);
  int32_t fw = LV_MAX(theme::S(2), E / 22);
  for (int s = 0; s < 2; s++) {
    const lv_area_t& a = s ? r : l;
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = lv_color_hex(0x0B0F14);
    d.bg_opa = 228;
    d.radius = E * 22 / 100;
    d.border_color = frame;
    d.border_width = fw;
    d.border_opa = LV_OPA_COVER;
    lv_draw_rect(layer, &d, &a);
    int32_t sx = a.x1 + E * 14 / 100;
    line(layer, sx, a.y1 + E * 30 / 100, sx + E * 14 / 100, a.y1 + E * 13 / 100, lv_color_white(), 80,
         LV_MAX(theme::S(2), E / 20));
  }
  int32_t by = l.y1 + E * 14 / 100;
  line(layer, l.x2, by, r.x1, by, frame, LV_OPA_COVER, fw, false);
  line(layer, l.x1, l.y1 + E * 10 / 100, l.x1 - E * 20 / 100, l.y1 + E * 4 / 100, frame, LV_OPA_COVER, fw);
  line(layer, r.x2, r.y1 + E * 10 / 100, r.x2 + E * 20 / 100, r.y1 + E * 4 / 100, frame, LV_OPA_COVER, fw);
}

void drawPart(lv_layer_t* layer, const Face& f, const lv_area_t& box, uint8_t fx, int i, const Part& p) {
  switch (fx) {
    case FX_DRIZZLE:
    case FX_RAIN:
    case FX_STORM:
      if (fx == FX_STORM && i == FX_FLASH) {
        lv_area_t a = {p.x, p.y, p.s, p.w};
        rect(layer, a, lv_color_white(), p.opa, 0);
      } else if (fx == FX_STORM && i == FX_BOLT) {
        drawBolt(layer, p, f.eye);
      } else {
        line(layer, p.x, p.y, p.x - p.s / 4, p.y + p.s, lv_color_hex(0x7CC4FF), p.opa, p.w);
        if (p.k > 800) {
          int32_t sp = f.eye * 9 / 100;
          int32_t xb = p.x - p.s / 4;
          int32_t yb = box.y2 - theme::S(2);
          lv_opa_t o = static_cast<lv_opa_t>(p.opa * (1000 - p.k) / 200);
          line(layer, xb, yb, xb - sp, yb - sp * 8 / 10, lv_color_hex(0x7CC4FF), o, p.w);
          line(layer, xb, yb, xb + sp, yb - sp * 8 / 10, lv_color_hex(0x7CC4FF), o, p.w);
        }
      }
      return;
    case FX_SNOW: {
      int32_t dx = p.s / 2;
      int32_t dy = p.s * 87 / 100;
      lv_color_t c = lv_color_white();
      line(layer, p.x - p.s, p.y, p.x + p.s, p.y, c, p.opa, p.w);
      line(layer, p.x - dx, p.y - dy, p.x + dx, p.y + dy, c, p.opa, p.w);
      line(layer, p.x - dx, p.y + dy, p.x + dx, p.y - dy, c, p.opa, p.w);
      return;
    }
    case FX_FOG: {
      lv_color_t c = lv_color_hex(0xC4CED8);
      lv_area_t halo = {p.x, p.y - p.w / 2, p.x + p.s, p.y + p.w + p.w / 2};
      rect(layer, halo, c, static_cast<lv_opa_t>(p.opa / 3), p.w);
      lv_area_t a = {p.x + p.w / 2, p.y, p.x + p.s - p.w / 2, p.y + p.w};
      rect(layer, a, c, p.opa, p.w / 2);
      return;
    }
    case FX_WIND: {
      if (i == 4) {
        int32_t L = p.s;
        int32_t W = p.s * 45 / 100;
        int32_t sn = lv_trigo_sin(static_cast<int16_t>(p.k));
        int32_t cs = lv_trigo_cos(static_cast<int16_t>(p.k));
        const int32_t ux[4] = {L, 0, -L, 0};
        const int32_t uy[4] = {0, W, 0, -W};
        int32_t qx[4];
        int32_t qy[4];
        for (int j = 0; j < 4; j++) {
          qx[j] = p.x + (ux[j] * cs - uy[j] * sn) / LV_TRIGO_SIN_MAX;
          qy[j] = p.y + (ux[j] * sn + uy[j] * cs) / LV_TRIGO_SIN_MAX;
        }
        lv_color_t c = lv_color_hex(0xA5E065);
        tri(layer, qx[0], qy[0], qx[1], qy[1], qx[2], qy[2], c, p.opa);
        tri(layer, qx[0], qy[0], qx[2], qy[2], qx[3], qy[3], c, p.opa);
        line(layer, qx[0], qy[0], qx[2], qy[2], lv_color_hex(0x5A8F2C), p.opa, LV_MAX(1, theme::S(1)));
        return;
      }
      int32_t r = f.eye * 9 / 100;
      line(layer, p.x, p.y, p.x + p.s, p.y, lv_color_white(), p.opa, p.w);
      arc(layer, p.x, p.y - r, r, p.w, 90, 270, lv_color_white(), p.opa, true);
      return;
    }
    case FX_STARS: {
      lv_color_t c = lv_color_hex(0xFFF1A8);
      if (i == 5) {
        int32_t g = p.s * 3 / 2;
        lv_area_t glow = {p.x - g, p.y - g, p.x + g, p.y + g};
        rect(layer, glow, c, 30, g);
        lv_area_t moon = {p.x - p.s, p.y - p.s, p.x + p.s, p.y + p.s};
        rect(layer, moon, c, LV_OPA_COVER, p.s);
        int32_t r2 = p.s * 85 / 100;
        int32_t bx = p.x + p.s * 45 / 100;
        int32_t by = p.y - p.s * 30 / 100;
        lv_area_t bite = {bx - r2, by - r2, bx + r2, by + r2};
        rect(layer, bite, f.bg, LV_OPA_COVER, r2);
        return;
      }
      line(layer, p.x - p.s, p.y, p.x + p.s, p.y, c, p.opa, p.w);
      line(layer, p.x, p.y - p.s, p.x, p.y + p.s, c, p.opa, p.w);
      lv_area_t dot = {p.x - p.w, p.y - p.w, p.x + p.w, p.y + p.w};
      rect(layer, dot, c, p.opa, p.w);
      return;
    }
    case FX_SWEAT:
    case FX_TEAR: {
      if (fx == FX_SWEAT && i > 0) {
        constexpr int N = 5;
        int32_t amp = f.eye * 5 / 100;
        int32_t px[N + 1];
        int32_t py[N + 1];
        for (int j = 0; j <= N; j++) {
          int32_t a = (j * 72 + p.k) % 360;
          px[j] = p.x + amp * lv_trigo_sin(static_cast<int16_t>(a)) / LV_TRIGO_SIN_MAX;
          py[j] = p.y - p.s * j / N;
        }
        for (int j = 0; j < N; j++) line(layer, px[j], py[j], px[j + 1], py[j + 1], lv_color_hex(0xFFC98A), p.opa, p.w);
        return;
      }
      lv_color_t c = lv_color_hex(fx == FX_TEAR ? 0x7CC4FF : 0x9AD9FF);
      lv_area_t a = {p.x - p.s, p.y - p.s, p.x + p.s, p.y + p.s};
      rect(layer, a, c, p.opa, p.s);
      tri(layer, p.x - p.s * 85 / 100, p.y - p.s / 2, p.x + p.s * 85 / 100, p.y - p.s / 2, p.x, p.y - p.s * 2, c, p.opa);
      int32_t g = LV_MAX(1, p.s / 3);
      lv_area_t shine = {p.x - p.s / 2, p.y - p.s / 4, p.x - p.s / 2 + g, p.y - p.s / 4 + g};
      rect(layer, shine, lv_color_white(), static_cast<lv_opa_t>(p.opa / 2), g);
      return;
    }
    case FX_SHIVER: {
      lv_area_t a = {p.x - p.s, p.y - p.s, p.x + p.s, p.y + p.s};
      rect(layer, a, lv_color_white(), p.opa, p.s);
      return;
    }
    case FX_CLOUD: {
      lv_color_t c = lv_color_hex(0xD3DBE5);
      int32_t W = p.s;
      int32_t H = p.w;
      lv_area_t base = {p.x, p.y + H * 45 / 100, p.x + W, p.y + H};
      rect(layer, base, c, p.opa, H * 28 / 100);
      int32_t r1 = H * 33 / 100;
      int32_t c1x = p.x + W * 28 / 100;
      int32_t c1y = p.y + H * 52 / 100;
      lv_area_t b1 = {c1x - r1, c1y - r1, c1x + r1, c1y + r1};
      rect(layer, b1, c, p.opa, r1);
      int32_t r2 = H * 45 / 100;
      int32_t c2x = p.x + W * 60 / 100;
      int32_t c2y = p.y + r2;
      lv_area_t b2 = {c2x - r2, c2y - r2, c2x + r2, c2y + r2};
      rect(layer, b2, c, p.opa, r2);
      return;
    }
    case FX_CONFETTI: {
      static const uint32_t kCol[5] = {0xFF4FA3, 0xFFD60A, 0x4ADE80, 0x38BDF8, 0xFB923C};
      int32_t a = p.s;
      int32_t b = p.s * 16 / 10;
      int32_t hw = p.w ? b : a;
      int32_t hh = p.w ? a : b;
      lv_area_t r = {p.x - hw / 2, p.y - hh / 2, p.x + hw / 2, p.y + hh / 2};
      rect(layer, r, lv_color_hex(kCol[i % 5]), p.opa, 1);
      return;
    }
    case FX_SUNGLASSES: {
      lv_area_t l;
      lens(f, box, true, l);
      int32_t m = LV_MAX(theme::S(3), f.eye * 8 / 100);
      int32_t h = l.y2 - l.y1;
      int32_t span = l.x2 - l.x1 - 2 * m - h / 2;
      int32_t x = l.x1 + m + span * p.s / 1000;
      line(layer, x, l.y2 - m, x + h / 2, l.y1 + m, lv_color_white(), p.opa, LV_MAX(theme::S(2), f.eye / 12));
      return;
    }
    default:
      return;
  }
}

uint8_t moodFx(uint8_t mood) {
  if (mood == SAD) return FX_TEAR;
  if (mood == CELEBRATE) return FX_CONFETTI;
  return FX_NONE;
}

void fxInvalidateOne(lv_obj_t* obj, const Face& f, const lv_area_t& box, uint8_t fx, uint32_t prev) {
  int n = partCount(fx);
  for (int i = 0; i < n; i++) {
    Part a, b;
    bool pa = part(f, box, fx, i, prev, a);
    bool pb = part(f, box, fx, i, f.now, b);
    if (!pa && !pb) continue;
    if (pa && pb && a.x == b.x && a.y == b.y && a.s == b.s && a.w == b.w && a.k == b.k && a.opa == b.opa) continue;
    lv_area_t ra, rb;
    if (pa) partRect(f, box, fx, i, a, ra);
    if (pb) partRect(f, box, fx, i, b, rb);
    lv_area_t u = pa ? ra : rb;
    if (pa && pb) {
      u.x1 = LV_MIN(ra.x1, rb.x1);
      u.y1 = LV_MIN(ra.y1, rb.y1);
      u.x2 = LV_MAX(ra.x2, rb.x2);
      u.y2 = LV_MAX(ra.y2, rb.y2);
    }
    lv_obj_invalidate_area(obj, &u);
  }
}

void fxInvalidate(lv_obj_t* obj, Face& f) {
  uint32_t prev = f.fxLast;
  f.fxLast = f.now;
  uint8_t extra = moodFx(f.mood);
  if (!partCount(f.fx) && !partCount(extra)) return;
  lv_area_t box;
  lv_obj_get_coords(obj, &box);
  fxInvalidateOne(obj, f, box, f.fx, prev);
  if (extra != f.fx) fxInvalidateOne(obj, f, box, extra, prev);
}

void drawFx(lv_layer_t* layer, const Face& f, const lv_area_t& box, uint8_t fx) {
  int n = partCount(fx);
  for (int i = 0; i < n; i++) {
    Part p;
    if (part(f, box, fx, i, f.now, p)) drawPart(layer, f, box, fx, i, p);
  }
}

void drawCheeks(lv_layer_t* layer, const Face& f, const lv_area_t& box) {
  Face g = f;
  g.blinking = 0;
  int32_t E = f.eye;
  int32_t cw = E * 42 / 100;
  int32_t ch = LV_MAX(theme::S(4), E * 15 / 100);
  for (int s = 0; s < 2; s++) {
    lv_area_t e;
    int32_t r;
    geometry(g, box, s == 0, e, r);
    int32_t vis = e.y2 - f.cur.happy / Q;
    int32_t cx = s == 0 ? e.x1 + (e.x2 - e.x1) * 22 / 100 : e.x2 - (e.x2 - e.x1) * 22 / 100;
    int32_t cy = vis + E * 26 / 100;
    lv_area_t halo = {cx - cw / 2 - GLOW / 2, cy - ch / 2 - GLOW / 2, cx + cw / 2 + GLOW / 2, cy + ch / 2 + GLOW / 2};
    rect(layer, halo, lv_color_hex(0xFF7AA8), 50, ch / 2 + GLOW / 2);
    lv_area_t a = {cx - cw / 2, cy - ch / 2, cx + cw / 2, cy + ch / 2};
    rect(layer, a, lv_color_hex(0xFF7AA8), 210, ch / 2);
  }
}

int16_t fxSway(const Face& f) {
  if (f.fx == FX_WIND) {
    int32_t amp = LV_MAX(theme::S(2), f.eye * 5 / 100);
    int32_t v = lv_trigo_sin(static_cast<int16_t>(f.now % 2400U * 360U / 2400U));
    return static_cast<int16_t>(-amp - amp * v / LV_TRIGO_SIN_MAX / 2);
  }
  if (f.fx == FX_SHIVER && f.now % FX_SHIVER_CYCLE < FX_SHIVER_MS)
    return static_cast<int16_t>(((f.now / 45) & 1) ? theme::S(2) : -theme::S(2));
  if (f.fx == FX_STORM && f.mood == SCARED && f.now % FX_STORM_CYCLE < 1100)
    return static_cast<int16_t>(((f.now / 60) & 1) ? theme::S(1) : -theme::S(1));
  return 0;
}

const lv_image_dsc_t* accImage(uint8_t acc) {
  switch (acc) {
    case rc::ACC_FLAG: return &ACC_FLAG_IMG;
    case rc::ACC_PARTY: return &ACC_PARTY_IMG;
    case rc::ACC_UMBRELLA: return &ACC_UMBRELLA_IMG;
    default: return nullptr;
  }
}

void drawCb(lv_event_t* e) {
  lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
  Face* f = faceOf(obj);
  if (!f) return;
  lv_layer_t* layer = lv_event_get_layer(e);
  lv_area_t box;
  lv_obj_get_coords(obj, &box);
  drawEye(layer, *f, box, true);
  drawEye(layer, *f, box, false);
  if (f->mood == HAPPY || f->mood == CELEBRATE) drawCheeks(layer, *f, box);
  if (f->fx == FX_SUNGLASSES) drawGlasses(layer, *f, box);
  if (f->music) drawPhones(layer, *f, box);

  if (f->mood == ASLEEP) drawZs(layer, *f, box);
  if (grooving(*f)) drawNotes(layer, *f, box);

  const lv_image_dsc_t* img = accImage(f->acc);
  if (img) {
    lv_area_t le, re;
    int32_t r;
    geometry(*f, box, true, le, r);
    geometry(*f, box, false, re, r);
    int32_t s = img->header.w;
    lv_area_t a;
    if (f->acc == rc::ACC_UMBRELLA) {
      a.x1 = re.x2 + theme::S(10);
      a.y1 = re.y1 - s / 2;
    } else if (f->acc == rc::ACC_PARTY) {
      a.x1 = (re.x1 + re.x2) / 2 - s / 2 + theme::S(6);
      a.y1 = re.y1 - s - theme::S(6);
    } else {
      a.x1 = le.x1 - s - theme::S(10);
      a.y1 = (le.y1 + le.y2) / 2 - s / 2 + theme::S(6);
    }
    if (a.x1 < box.x1) a.x1 = box.x1;
    if (a.y1 + s - 1 > box.y2) a.y1 = box.y2 - s + 1;
    if (a.y1 < box.y1) a.y1 = box.y1;
    if (a.x1 + s - 1 > box.x2) a.x1 = box.x2 - s + 1;
    a.x2 = a.x1 + s - 1;
    a.y2 = a.y1 + s - 1;
    lv_draw_image_dsc_t d;
    lv_draw_image_dsc_init(&d);
    d.src = img;
    lv_draw_image(layer, &d, &a);
  }
  drawFx(layer, *f, box, f->fx);
  uint8_t extra = moodFx(f->mood);
  if (extra != f->fx) drawFx(layer, *f, box, extra);
}

uint32_t signature(const Face& f) {
  uint32_t s = 2166136261u;
  const uint8_t* p = reinterpret_cast<const uint8_t*>(&f.cur);
  for (size_t i = 0; i < sizeof(f.cur); i++) s = (s ^ p[i]) * 16777619u;
  int32_t extra[10] = {f.gazeX, f.gazeY, f.bounce, f.flicker,
                       f.blinking ? static_cast<int32_t>(f.now - f.blinkStart) : -1, f.swayX, f.bobY, f.music,
                       f.doze, f.wide};
  const uint8_t* q = reinterpret_cast<const uint8_t*>(extra);
  for (size_t i = 0; i < sizeof(extra); i++) s = (s ^ q[i]) * 16777619u;
  return s ^ (static_cast<uint32_t>(f.mood) << 24) ^ (static_cast<uint32_t>(f.acc) << 16) ^
         (static_cast<uint32_t>(f.fx) << 8);
}

void faceBox(lv_obj_t* obj, const Face& f, lv_area_t& out) {
  lv_area_t box;
  lv_obj_get_coords(obj, &box);
  lv_area_t le, re;
  int32_t r;
  geometry(f, box, true, le, r);
  geometry(f, box, false, re, r);
  out.x1 = LV_MIN(le.x1, re.x1) - GLOW - theme::S(44);
  out.x2 = LV_MAX(le.x2, re.x2) + GLOW + theme::S(44);
  out.y1 = LV_MIN(le.y1, re.y1) - GLOW - theme::S(44);
  out.y2 = LV_MAX(le.y2, re.y2) + GLOW + theme::S(8);
  if (f.mood == ASLEEP) {
    lv_area_t z;
    zBox(f, box, z);
    out.x1 = LV_MIN(out.x1, z.x1);
    out.y1 = LV_MIN(out.y1, z.y1);
    out.x2 = LV_MAX(out.x2, z.x2);
    out.y2 = LV_MAX(out.y2, z.y2);
  }
  if (f.music) {
    lv_area_t a;
    phonesBox(f, box, a);
    out.x1 = LV_MIN(out.x1, a.x1);
    out.y1 = LV_MIN(out.y1, a.y1);
    out.x2 = LV_MAX(out.x2, a.x2);
    out.y2 = LV_MAX(out.y2, a.y2);
  }
  if (out.x1 < box.x1) out.x1 = box.x1;
  if (out.y1 < box.y1) out.y1 = box.y1;
  if (out.x2 > box.x2) out.x2 = box.x2;
  if (out.y2 > box.y2) out.y2 = box.y2;
}

void refresh(lv_obj_t* obj, Face& f, bool force) {
  uint32_t sig = signature(f);
  if (!force && sig == f.lastSig) return;
  f.lastSig = sig;
  lv_area_t nb;
  faceBox(obj, f, nb);
  if (f.lastBox.x2 > f.lastBox.x1) lv_obj_invalidate_area(obj, &f.lastBox);
  lv_obj_invalidate_area(obj, &nb);
  f.lastBox = nb;
}

void scheduleBlink(Face& f) {
  uint32_t base = f.mood == SLEEPY || f.mood == CALM ? 2600 : 3000;
  f.nextBlink = f.now + base + lv_rand(0, 4000);
}

void tick(lv_timer_t* t) {
  lv_obj_t* obj = static_cast<lv_obj_t*>(lv_timer_get_user_data(t));
  Face* fp = faceOf(obj);
  if (!fp) return;
  Face& f = *fp;
  f.now = lv_tick_get();

  if (f.mood != ASLEEP) {
    if (!f.blinking && f.mood != MUSIC && static_cast<int32_t>(f.now - f.nextBlink) >= 0) {
      f.blinking = 1;
      f.blinkStart = f.now;
    }
    if (f.blinking && f.now - f.blinkStart >= 150) {
      f.blinking = 0;
      scheduleBlink(f);
      if (lv_rand(0, 5) == 0) f.nextBlink = f.now + 180;
    }
    if (static_cast<int32_t>(f.now - f.nextLook) >= 0) {
      int32_t range = f.eye * Q * 18 / 100;
      if (f.mood == SAD || f.mood == SLEEPY || f.mood == CALM || f.mood == MUSIC || lv_rand(0, 3) == 0) {
        f.gazeX = 0;
        f.gazeY = 0;
      } else {
        f.gazeX = static_cast<int16_t>(lv_rand(0, static_cast<uint32_t>(range * 2)) - range);
        f.gazeY = static_cast<int16_t>(lv_rand(0, static_cast<uint32_t>(range)) - range / 2);
      }
      f.nextLook = f.now + 2500 + lv_rand(0, 5000);
    }
  } else {
    f.blinking = 0;
    f.gazeX = 0;
    f.gazeY = 0;
  }

  if (f.mood == HAPPY || f.mood == CELEBRATE) {
    if (static_cast<int32_t>(f.now - f.nextHop) >= 0) {
      f.hopStart = f.now;
      f.nextHop = f.now + (f.mood == CELEBRATE ? 900 : 4000 + lv_rand(0, 3000));
    }
    uint32_t ht = f.now - f.hopStart;
    int32_t amp = f.mood == CELEBRATE ? theme::S(7) : theme::S(4);
    if (ht < 420) {
      int32_t k = static_cast<int32_t>(ht) * 2 * 1000 / 420;
      int32_t y = k < 1000 ? k : 2000 - k;
      f.bounce = static_cast<int16_t>(-(amp * y) / 1000);
    } else {
      f.bounce = 0;
    }
  } else {
    f.bounce = 0;
  }

  bool jolt = f.fx == FX_STORM && f.now % FX_STORM_CYCLE < 260;
  if (jolt) f.flickerUntil = f.now + 80;
  f.wide = static_cast<int16_t>(jolt ? 1080 : 1000);
  f.doze = static_cast<int16_t>(jolt ? 1120 : 1000);
  if (f.mood == SLEEPY && !f.blinking) {
    uint32_t c = f.now % DOZE_CYCLE;
    if (c < 1500) f.doze = static_cast<int16_t>(1000 - 620 * static_cast<int32_t>(c) / 1500);
    else if (c < 1900) f.doze = 380;
    else if (c < 2050) f.doze = static_cast<int16_t>(380 + 620 * static_cast<int32_t>(c - 1900) / 150);
  }
  if (f.mood != ASLEEP) {
    int32_t range = f.eye * Q * 18 / 100;
    if (f.fx == FX_CLOUD) {
      f.gazeX = static_cast<int16_t>(-range * 6 / 10);
      f.gazeY = static_cast<int16_t>(-range * 4 / 10);
    } else if (f.fx == FX_FOG) {
      int32_t v = range * lv_trigo_sin(static_cast<int16_t>(f.now % 4400U * 360U / 4400U)) / LV_TRIGO_SIN_MAX;
      f.gazeX = static_cast<int16_t>(v / (2 * Q) * (2 * Q));
      f.gazeY = 0;
    }
  }
  if (static_cast<int32_t>(f.flickerUntil - f.now) > 0) f.flicker = static_cast<int16_t>(((f.now / 40) & 1) ? theme::S(2) : -theme::S(2));
  else f.flicker = 0;

  if (grooving(f)) {
    groove(f);
  } else {
    f.swayX = fxSway(f);
    f.bobY = 0;
  }

  f.cur.w = ease(f.cur.w, f.tgt.w);
  f.cur.h = ease(f.cur.h, f.tgt.h);
  f.cur.dx = ease(f.cur.dx, f.tgt.dx);
  f.cur.dy = ease(f.cur.dy, f.tgt.dy);
  f.cur.tired = ease(f.cur.tired, f.tgt.tired);
  f.cur.angry = ease(f.cur.angry, f.tgt.angry);
  f.cur.happy = ease(f.cur.happy, f.tgt.happy);
  f.cur.flat = ease(f.cur.flat, f.tgt.flat);
  f.cur.head = ease(f.cur.head, f.tgt.head);
  refresh(obj, f, false);
  if (f.mood == ASLEEP) {
    lv_area_t box;
    lv_area_t z;
    lv_obj_get_coords(obj, &box);
    zBox(f, box, z);
    lv_obj_invalidate_area(obj, &z);
  }
  if (grooving(f)) {
    lv_area_t box;
    lv_area_t n;
    lv_obj_get_coords(obj, &box);
    notesBox(f, box, n);
    lv_obj_invalidate_area(obj, &n);
  }
  fxInvalidate(obj, f);
}

uint32_t framePeriod(const Face& f) { return f.mood == ASLEEP ? SLEEP_FRAME_MS : FRAME_MS; }

void deleteCb(lv_event_t* e) {
  lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
  Face* f = faceOf(obj);
  if (!f) return;
  if (f->timer) lv_timer_delete(f->timer);
  lv_free(f);
  lv_obj_set_user_data(obj, nullptr);
}

void clickCb(lv_event_t* e) {
  lv_obj_t* obj = static_cast<lv_obj_t*>(lv_event_get_current_target(e));
  lv_point_t p;
  lv_indev_get_point(lv_indev_active(), &p);
  poke(obj, p.x, p.y);
}

}

lv_obj_t* create(lv_obj_t* parent, int32_t w, int32_t h, int32_t eye) {
  lv_obj_t* obj = lv_obj_create(parent);
  lv_obj_remove_style_all(obj);
  lv_obj_set_size(obj, w, h);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  Face* f = static_cast<Face*>(lv_malloc_zeroed(sizeof(Face)));
  if (!f) return obj;
  f->eye = static_cast<int16_t>(eye);
  f->color = theme::col(theme::C_EYE);
  f->bg = theme::col(theme::C_BG);
  f->mood = NORMAL;
  f->doze = 1000;
  f->wide = 1000;
  f->tgt = moodParams(*f, NORMAL);
  f->cur = f->tgt;
  f->now = lv_tick_get();
  scheduleBlink(*f);
  f->nextLook = f->now + 2000;
  f->nextHop = f->now + 1500;
  lv_obj_set_user_data(obj, f);
  lv_obj_add_event_cb(obj, drawCb, LV_EVENT_DRAW_MAIN, nullptr);
  lv_obj_add_event_cb(obj, deleteCb, LV_EVENT_DELETE, nullptr);
  lv_obj_add_event_cb(obj, clickCb, LV_EVENT_CLICKED, nullptr);
  return obj;
}

void setMood(lv_obj_t* obj, Mood m) {
  Face* f = faceOf(obj);
  if (!f || m >= MOOD_COUNT) return;
  if (f->mood == m) return;
  f->mood = m;
  f->tgt = moodParams(*f, m);
  if (m == SURPRISED) f->flickerUntil = lv_tick_get() + 900;
  if (f->animated && (m == SURPRISED || m == SCARED)) {
    f->cur.w = static_cast<int16_t>(f->tgt.w * 112 / 100);
    f->cur.h = static_cast<int16_t>(f->tgt.h * 118 / 100);
  }
  if (f->timer) lv_timer_set_period(f->timer, framePeriod(*f));
  if (!f->animated) {
    f->cur = f->tgt;
    f->blinking = 0;
    f->bounce = 0;
    f->flicker = 0;
    f->gazeX = 0;
    f->gazeY = 0;
  }
  refresh(obj, *f, true);
}

Mood mood(lv_obj_t* obj) {
  Face* f = faceOf(obj);
  return f ? static_cast<Mood>(f->mood) : NORMAL;
}

void setAccessory(lv_obj_t* obj, uint8_t acc) {
  Face* f = faceOf(obj);
  if (!f || f->acc == acc) return;
  f->acc = acc;
  lv_obj_invalidate(obj);
}

void setEffect(lv_obj_t* obj, Effect fx) {
  Face* f = faceOf(obj);
  if (!f || fx >= FX_COUNT || f->fx == fx) return;
  f->fx = fx;
  f->fxLast = f->now;
  if (fx != FX_WIND && fx != FX_SHIVER && fx != FX_STORM && !grooving(*f)) f->swayX = 0;
  lv_obj_invalidate(obj);
}

void setAnimated(lv_obj_t* obj, bool on) {
  Face* f = faceOf(obj);
  if (!f) return;
  if (on && !f->timer) {
    f->timer = lv_timer_create(tick, framePeriod(*f), obj);
    f->now = lv_tick_get();
    scheduleBlink(*f);
  }
  if (!on && f->timer) {
    lv_timer_delete(f->timer);
    f->timer = nullptr;
    f->cur = f->tgt;
    f->blinking = 0;
    f->bounce = 0;
    f->flicker = 0;
    f->gazeX = 0;
    f->gazeY = 0;
    f->swayX = 0;
    f->bobY = 0;
    f->doze = 1000;
    f->wide = 1000;
    refresh(obj, *f, true);
  }
  f->animated = on ? 1 : 0;
}

void setMusic(lv_obj_t* obj, bool on, lv_color_t phone, lv_color_t note) {
  Face* f = faceOf(obj);
  if (!f) return;
  uint8_t m = on ? 1 : 0;
  if (f->music == m && lv_color_eq(f->phoneColor, phone) && lv_color_eq(f->noteColor, note)) return;
  f->music = m;
  f->phoneColor = phone;
  f->noteColor = note;
  if (!on) {
    f->swayX = 0;
    f->bobY = 0;
    f->beatPeriod = 0;
  }
  lv_obj_invalidate(obj);
}

void setTempo(lv_obj_t* obj, uint32_t periodMs, uint32_t anchorTick) {
  Face* f = faceOf(obj);
  if (!f) return;
  rc::beatLock(f->beatPeriod, f->beatAnchor, periodMs, anchorTick, lv_tick_get());
}

void setColor(lv_obj_t* obj, lv_color_t c) {
  Face* f = faceOf(obj);
  if (!f || lv_color_eq(f->color, c)) return;
  f->color = c;
  lv_obj_invalidate(obj);
}

void setBackground(lv_obj_t* obj, lv_color_t c) {
  Face* f = faceOf(obj);
  if (!f || lv_color_eq(f->bg, c)) return;
  f->bg = c;
  lv_obj_invalidate(obj);
}

void poke(lv_obj_t* obj, int32_t x, int32_t y) {
  Face* f = faceOf(obj);
  if (!f || f->mood == ASLEEP) return;
  lv_area_t box;
  lv_obj_get_coords(obj, &box);
  int32_t cx = (box.x1 + box.x2) / 2;
  int32_t cy = (box.y1 + box.y2) / 2;
  int32_t range = f->eye * Q * 18 / 100;
  int32_t gx = (x - cx) * Q / 4;
  int32_t gy = (y - cy) * Q / 4;
  f->gazeX = static_cast<int16_t>(LV_CLAMP(-range, gx, range));
  f->gazeY = static_cast<int16_t>(LV_CLAMP(-range / 2, gy, range / 2));
  f->now = lv_tick_get();
  f->blinking = 1;
  f->blinkStart = f->now;
  f->nextLook = f->now + 2200;
  refresh(obj, *f, true);
}

const char* moodName(Mood m) {
  static const char* const kNames[MOOD_COUNT] = {"normal",  "mutlu",          "uykulu",  "uyuyor", "üzgün",
                                                 "şaşkın",  "yukarı bakıyor", "kutlama", "sakin",  "müzik dinliyor",
                                                 "gözleri kısık", "korkmuş", "gözleri sıkılı"};
  return m < MOOD_COUNT ? kNames[m] : "?";
}

Look weatherLook(rc::WxKind k) {
  static const Look kLooks[rc::WXK_COUNT] = {
      {NORMAL, rc::ACC_NONE, FX_NONE},       {HAPPY, rc::ACC_NONE, FX_NONE},
      {NORMAL, rc::ACC_NONE, FX_SUNGLASSES}, {SLEEPY, rc::ACC_NONE, FX_SWEAT},
      {CALM, rc::ACC_NONE, FX_CLOUD},        {SQUINT, rc::ACC_NONE, FX_FOG},
      {LOOKUP, rc::ACC_UMBRELLA, FX_DRIZZLE}, {SAD, rc::ACC_UMBRELLA, FX_RAIN},
      {HAPPY, rc::ACC_NONE, FX_SNOW},        {SCARED, rc::ACC_NONE, FX_STORM},
      {SQUINT, rc::ACC_NONE, FX_WIND},       {SQUEEZE, rc::ACC_NONE, FX_SHIVER},
      {CALM, rc::ACC_NONE, FX_STARS}};
  return k < rc::WXK_COUNT ? kLooks[k] : kLooks[rc::WXK_NONE];
}

}
