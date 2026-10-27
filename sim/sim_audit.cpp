#include "sim_audit.h"

#include <lvgl.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include "src/core/lv_global.h"
#include "src/draw/lv_draw_private.h"
#include "src/misc/lv_area_private.h"
#include "src/misc/lv_event_private.h"
#include "src/misc/lv_text_private.h"

namespace audit {

namespace {

struct Item {
  bool image;
  bool hidden;
  bool widget;
  bool moving;
  uint32_t seq;
  lv_area_t ink;
  lv_area_t clip;
  lv_area_t obj;
  std::string text;
  std::string key;
  const lv_obj_t* scr;
  bool hscroll;
  bool vscroll;
};

lv_draw_unit_t* s_unit = nullptr;
std::vector<Item> s_items;
std::vector<std::pair<uint32_t, lv_area_t>> s_fills;
uint32_t s_seq = 0;
std::vector<std::string> s_missing;
std::map<const void*, lv_area_t> s_inkCache;
bool s_on = false;
int s_total = 0;

int alphaAt(const lv_image_dsc_t* img, int32_t x, int32_t y) {
  uint32_t cf = img->header.cf;
  int32_t w = img->header.w;
  int32_t h = img->header.h;
  const uint8_t* p = img->data;
  uint32_t bpp = LV_COLOR_FORMAT_GET_BPP(cf);
  uint32_t stride = img->header.stride ? img->header.stride : (static_cast<uint32_t>(w) * bpp + 7) / 8;
  if (LV_COLOR_FORMAT_IS_INDEXED(cf)) {
    uint32_t entries = LV_COLOR_INDEXED_PALETTE_SIZE(cf);
    const lv_color32_t* pal = reinterpret_cast<const lv_color32_t*>(p);
    const uint8_t* idx = p + entries * 4;
    uint32_t bit = static_cast<uint32_t>(x) * bpp;
    uint8_t byte = idx[static_cast<uint32_t>(y) * stride + bit / 8];
    uint32_t shift = 8 - bpp - bit % 8;
    uint32_t v = (byte >> shift) & ((1u << bpp) - 1);
    return pal[v].alpha;
  }
  switch (cf) {
    case LV_COLOR_FORMAT_A8: return p[static_cast<uint32_t>(y) * stride + static_cast<uint32_t>(x)];
    case LV_COLOR_FORMAT_ARGB8888: return p[static_cast<uint32_t>(y) * stride + static_cast<uint32_t>(x) * 4 + 3];
    case LV_COLOR_FORMAT_RGB565A8: {
      const uint8_t* a = p + stride * static_cast<uint32_t>(h);
      return a[static_cast<uint32_t>(y) * (stride / 2) + static_cast<uint32_t>(x)];
    }
    default: return 255;
  }
}

lv_area_t imageInk(const lv_image_dsc_t* img) {
  auto it = s_inkCache.find(img);
  if (it != s_inkCache.end()) return it->second;
  int32_t w = img->header.w;
  int32_t h = img->header.h;
  lv_area_t box = {w, h, -1, -1};
  if (!img->data) {
    box = {0, 0, w - 1, h - 1};
  } else {
    for (int32_t y = 0; y < h; y++) {
      for (int32_t x = 0; x < w; x++) {
        if (alphaAt(img, x, y) <= 40) continue;
        box.x1 = LV_MIN(box.x1, x);
        box.y1 = LV_MIN(box.y1, y);
        box.x2 = LV_MAX(box.x2, x);
        box.y2 = LV_MAX(box.y2, y);
      }
    }
  }
  s_inkCache[img] = box;
  return box;
}

void scrollFlags(const lv_obj_t* obj, bool& h, bool& v) {
  h = v = false;
  for (const lv_obj_t* p = obj; p; p = lv_obj_get_parent(p)) {
    lv_obj_t* q = const_cast<lv_obj_t*>(p);
    if (!lv_obj_has_flag(q, LV_OBJ_FLAG_SCROLLABLE)) continue;
    lv_dir_t dir = lv_obj_get_scroll_dir(q);
    if ((dir & LV_DIR_VER) && (lv_obj_get_scroll_top(q) > 0 || lv_obj_get_scroll_bottom(q) > 0)) v = true;
    if ((dir & LV_DIR_HOR) && dir != LV_DIR_ALL && (lv_obj_get_scroll_left(q) > 0 || lv_obj_get_scroll_right(q) > 0)) h = true;
  }
}

bool ignorable(uint32_t c) { return c < 0x20 || c == 0x200B || c == 0x200D || c == 0xFE0F || c == 0xFEFF; }

std::string clipText(const lv_draw_label_dsc_t* d) {
  if (!d->text) return std::string();
  if (!d->text_length) return std::string(d->text);
  uint32_t i = 0;
  for (uint32_t n = 0; n < d->text_length && d->text[i]; n++) lv_text_encoded_next(d->text, &i);
  return std::string(d->text, i);
}

void addItem(Item&& it) {
  for (Item& o : s_items) {
    if (o.key != it.key) continue;
    o.clip.x1 = LV_MIN(o.clip.x1, it.clip.x1);
    o.clip.y1 = LV_MIN(o.clip.y1, it.clip.y1);
    o.clip.x2 = LV_MAX(o.clip.x2, it.clip.x2);
    o.clip.y2 = LV_MAX(o.clip.y2, it.clip.y2);
    return;
  }
  it.seq = ++s_seq;
  it.hidden = false;
  s_items.push_back(std::move(it));
}

std::vector<lv_obj_t*> s_cands;
size_t s_cursor = 0;

bool drawsItself(lv_obj_t* obj) {
  uint32_t n = lv_obj_get_event_count(obj);
  for (uint32_t i = 0; i < n; i++) {
    lv_event_dsc_t* d = lv_obj_get_event_dsc(obj, i);
    uint32_t f = d->filter & ~static_cast<uint32_t>(LV_EVENT_PREPROCESS);
    if (f >= LV_EVENT_DRAW_MAIN_BEGIN && f <= LV_EVENT_DRAW_POST_END) return true;
  }
  return false;
}

void collect(lv_obj_t* obj) {
  if (!obj || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
  if (drawsItself(obj)) s_cands.push_back(obj);
  uint32_t n = lv_obj_get_child_count(obj);
  for (uint32_t i = 0; i < n; i++) collect(lv_obj_get_child(obj, static_cast<int32_t>(i)));
}

bool holds(lv_obj_t* obj, const lv_area_t& clip) {
  lv_area_t c;
  lv_obj_get_coords(obj, &c);
  return lv_area_is_in(&clip, &c, 0);
}

const lv_obj_t* drawer(const lv_draw_task_t* t, const lv_obj_t* obj) {
  if (obj) return obj;
  for (size_t i = s_cursor; i < s_cands.size(); i++) {
    if (!holds(s_cands[i], t->clip_area)) continue;
    s_cursor = i;
    return s_cands[i];
  }
  for (size_t i = s_cands.size(); i-- > 0;) {
    if (holds(s_cands[i], t->clip_area)) return s_cands[i];
  }
  return nullptr;
}

bool moving(const lv_obj_t* obj) {
  for (const lv_obj_t* p = obj; p; p = lv_obj_get_parent(p)) {
    if (lv_anim_get(const_cast<lv_obj_t*>(p), nullptr)) return true;
  }
  return false;
}

void objArea(const lv_obj_t* obj, lv_area_t& out) {
  if (obj) lv_obj_get_coords(obj, &out);
  else out = {-10000, -10000, 10000, 10000};
}

void fill(lv_draw_task_t* t) {
  const lv_draw_fill_dsc_t* d = static_cast<const lv_draw_fill_dsc_t*>(t->draw_dsc);
  if (d->opa < LV_OPA_COVER || d->grad.dir != LV_GRAD_DIR_NONE) return;
  lv_area_t r;
  if (!lv_area_intersect(&r, &t->area, &t->clip_area)) return;
  for (auto& f : s_fills) {
    if (lv_area_is_in(&r, &f.second, 0)) return;
  }
  s_fills.push_back({++s_seq, r});
}

void label(lv_draw_task_t* t) {
  const lv_draw_label_dsc_t* d = static_cast<const lv_draw_label_dsc_t*>(t->draw_dsc);
  if (!d->font || d->opa <= LV_OPA_10) return;
  std::string txt = clipText(d);
  if (txt.empty()) return;
  const lv_font_t* f = d->font;
  int32_t lh = lv_font_get_line_height(f);
  int32_t base = f->base_line;
  const lv_area_t& a = t->area;
  int32_t maxW = (d->flag & LV_TEXT_FLAG_EXPAND) ? LV_COORD_MAX : lv_area_get_width(&a);
  lv_point_t sz;
  lv_text_get_size(&sz, txt.c_str(), f, d->letter_space, d->line_space, maxW, d->flag);
  int32_t aw = lv_area_get_width(&a);
  int32_t x = a.x1;
  if (d->align == LV_TEXT_ALIGN_CENTER) x = a.x1 + (aw - sz.x) / 2;
  else if (d->align == LV_TEXT_ALIGN_RIGHT) x = a.x2 + 1 - sz.x;
  x += d->ofs_x;
  int32_t y = a.y1 + d->ofs_y;
  int32_t asc = 0;
  int32_t desc = 0;
  bool any = false;
  uint32_t i = 0;
  const char* s = txt.c_str();
  while (s[i]) {
    uint32_t next = 0;
    uint32_t c = lv_text_encoded_next(s, &i);
    uint32_t j = i;
    if (s[j]) next = lv_text_encoded_next(s, &j);
    if (ignorable(c)) continue;
    lv_font_glyph_dsc_t g;
    if (!lv_font_get_glyph_dsc(f, &g, c, next)) {
      char buf[160];
      snprintf(buf, sizeof(buf), "eksik harf U+%04X '%s'", static_cast<unsigned>(c), s);
      s_missing.push_back(buf);
      continue;
    }
    if (!g.box_w || !g.box_h) continue;
    int32_t top = g.box_h + g.ofs_y;
    if (!any || top > asc) asc = top;
    if (!any || g.ofs_y < desc) desc = g.ofs_y;
    any = true;
  }
  if (!any) return;
  int32_t lines = LV_MAX(1, (sz.y + d->line_space) / (lh + d->line_space));
  Item it;
  it.image = false;
  it.ink.x1 = x;
  it.ink.x2 = x + sz.x - 1;
  it.ink.y1 = y + (lh - base) - asc;
  it.ink.y2 = y + (lines - 1) * (lh + d->line_space) + (lh - base) - desc - 1;
  it.clip = t->clip_area;
  it.text = txt;
  char key[64];
  snprintf(key, sizeof(key), "T%p:%d:%d:%d:", static_cast<const void*>(d->base.obj), static_cast<int>(x),
           static_cast<int>(y), static_cast<int>(a.x2));
  it.key = std::string(key) + txt;
  const lv_obj_t* obj = drawer(t, d->base.obj);
  if (obj && lv_obj_check_type(obj, &lv_roller_class)) return;
  if (d->base.obj && lv_obj_check_type(d->base.obj, &lv_label_class)) {
    lv_label_long_mode_t m = lv_label_get_long_mode(d->base.obj);
    if (m == LV_LABEL_LONG_MODE_SCROLL || m == LV_LABEL_LONG_MODE_SCROLL_CIRCULAR) return;
  }
  it.widget = d->base.obj != nullptr;
  it.scr = obj ? lv_obj_get_screen(obj) : nullptr;
  objArea(obj, it.obj);
  scrollFlags(obj, it.hscroll, it.vscroll);
  it.moving = moving(obj);
  addItem(std::move(it));
}

void image(lv_draw_task_t* t) {
  const lv_draw_image_dsc_t* d = static_cast<const lv_draw_image_dsc_t*>(t->draw_dsc);
  if (!d->src || lv_image_src_get_type(d->src) != LV_IMAGE_SRC_VARIABLE || d->opa <= LV_OPA_10) return;
  const lv_image_dsc_t* img = static_cast<const lv_image_dsc_t*>(d->src);
  const lv_area_t& a = t->area;
  Item it;
  it.image = true;
  if (d->rotation || d->scale_x != LV_SCALE_NONE || d->scale_y != LV_SCALE_NONE ||
      lv_area_get_width(&a) != img->header.w || lv_area_get_height(&a) != img->header.h) {
    it.ink = a;
  } else {
    lv_area_t in = imageInk(img);
    if (in.x2 < in.x1) return;
    it.ink = {a.x1 + in.x1, a.y1 + in.y1, a.x1 + in.x2, a.y1 + in.y2};
  }
  it.clip = t->clip_area;
  char key[96];
  snprintf(key, sizeof(key), "I%p:%p:%d:%d", static_cast<const void*>(d->base.obj), d->src, static_cast<int>(a.x1),
           static_cast<int>(a.y1));
  it.key = key;
  it.text = "[resim]";
  it.widget = d->base.obj != nullptr;
  const lv_obj_t* obj = drawer(t, d->base.obj);
  it.scr = obj ? lv_obj_get_screen(obj) : nullptr;
  objArea(obj, it.obj);
  scrollFlags(obj, it.hscroll, it.vscroll);
  it.moving = moving(obj);
  addItem(std::move(it));
}

int32_t evaluateCb(lv_draw_unit_t*, lv_draw_task_t* t) {
  if (!s_on) return 0;
  if (t->type == LV_DRAW_TASK_TYPE_LABEL) label(t);
  else if (t->type == LV_DRAW_TASK_TYPE_IMAGE) image(t);
  else if (t->type == LV_DRAW_TASK_TYPE_FILL) fill(t);
  return 0;
}

int32_t dispatchCb(lv_draw_unit_t*, lv_layer_t*) { return LV_DRAW_UNIT_IDLE; }

lv_area_t visibleOf(const Item& it) {
  lv_area_t r;
  if (!lv_area_intersect(&r, &it.ink, &it.clip)) r = {0, 0, -1, -1};
  return r;
}

std::string shortText(const std::string& s) {
  std::string out;
  uint32_t i = 0;
  int n = 0;
  while (s[i] && n < 40) {
    uint32_t a = i;
    uint32_t c = lv_text_encoded_next(s.c_str(), &i);
    if (c == 10) out += " / ";
    else if (c >= 0xE000 && c <= 0xF8FF) out += "<ikon>";
    else out.append(s, a, i - a);
    n++;
  }
  if (s[i]) out += "...";
  return out;
}

bool covered(const Item& it) {
  lv_area_t v = visibleOf(it);
  if (v.x2 < v.x1) return true;
  for (const auto& f : s_fills) {
    if (f.first > it.seq && lv_area_is_in(&v, &f.second, 0)) return true;
  }
  return false;
}

bool between(uint32_t s1, uint32_t s2, const lv_area_t& r) {
  uint32_t lo = LV_MIN(s1, s2);
  uint32_t hi = LV_MAX(s1, s2);
  for (const auto& f : s_fills) {
    if (f.first > lo && f.first < hi && lv_area_is_in(&r, &f.second, 0)) return true;
  }
  return false;
}

bool cutBy(int32_t cut, bool scroll) { return cut > 1 && !scroll; }

}

void begin() {
  static lv_draw_unit_t unit;
  memset(&unit, 0, sizeof(unit));
  lv_draw_global_info_t& info = LV_GLOBAL_DEFAULT()->draw_info;
  unit.next = info.unit_head;
  info.unit_head = &unit;
  info.unit_cnt++;
  unit.idx = static_cast<int32_t>(info.unit_cnt);
  s_unit = &unit;
  s_unit->name = "denetim";
  s_unit->evaluate_cb = evaluateCb;
  s_unit->dispatch_cb = dispatchCb;
}

void start() {
  s_items.clear();
  s_missing.clear();
  s_fills.clear();
  s_seq = 0;
  s_cands.clear();
  s_cursor = 0;
  collect(lv_screen_active());
  collect(lv_layer_top());
  collect(lv_layer_sys());
  s_on = true;
}

int finish(const char* shot, FILE* out) {
  s_on = false;
  std::vector<std::string> found;
  for (const std::string& m : s_missing) {
    bool dup = false;
    for (const std::string& f : found) dup = dup || f == m;
    if (!dup) found.push_back(m);
  }
  for (Item& it : s_items) it.hidden = covered(it);
  for (const Item& it : s_items) {
    if (it.image || it.hidden) continue;
    int32_t cutL = it.clip.x1 - it.ink.x1;
    int32_t cutR = it.ink.x2 - it.clip.x2;
    int32_t cutT = it.clip.y1 - it.ink.y1;
    int32_t cutB = it.ink.y2 - it.clip.y2;
    if (it.moving) continue;
    bool h = cutBy(cutL, it.hscroll) || cutBy(cutR, it.hscroll);
    bool v = cutBy(cutT, it.vscroll) || cutBy(cutB, it.vscroll);
    if (!h && !v) continue;
    char buf[320];
    snprintf(buf, sizeof(buf), "kesik '%s' (sol %d sag %d ust %d alt %d px; yazi %d,%d-%d,%d nesne %d,%d-%d,%d)",
             shortText(it.text).c_str(), static_cast<int>(LV_MAX(0, cutL)), static_cast<int>(LV_MAX(0, cutR)),
             static_cast<int>(LV_MAX(0, cutT)), static_cast<int>(LV_MAX(0, cutB)), static_cast<int>(it.ink.x1),
             static_cast<int>(it.ink.y1), static_cast<int>(it.ink.x2), static_cast<int>(it.ink.y2),
             static_cast<int>(it.obj.x1), static_cast<int>(it.obj.y1), static_cast<int>(it.obj.x2),
             static_cast<int>(it.obj.y2));
    found.push_back(buf);
  }
  for (size_t i = 0; i < s_items.size(); i++) {
    for (size_t j = i + 1; j < s_items.size(); j++) {
      const Item& a = s_items[i];
      const Item& b = s_items[j];
      if (a.image && b.image) continue;
      if (a.scr != b.scr || a.hidden || b.hidden) continue;
      if (!a.image && !b.image && a.text == b.text) continue;
      lv_area_t va = visibleOf(a);
      lv_area_t vb = visibleOf(b);
      lv_area_t r;
      if (va.x2 < va.x1 || vb.x2 < vb.x1 || !lv_area_intersect(&r, &va, &vb)) continue;
      int32_t area = lv_area_get_width(&r) * lv_area_get_height(&r);
      if (area < 6 || between(a.seq, b.seq, r)) continue;
      char buf[220];
      snprintf(buf, sizeof(buf), "ust uste '%s' x '%s' (%dx%d px @%d,%d)", shortText(a.text).c_str(),
               shortText(b.text).c_str(), static_cast<int>(lv_area_get_width(&r)),
               static_cast<int>(lv_area_get_height(&r)), static_cast<int>(r.x1), static_cast<int>(r.y1));
      found.push_back(buf);
    }
  }
  for (const std::string& f : found) {
    if (out) fprintf(out, "%s\t%s\n", shot, f.c_str());
  }
  if (!found.empty()) printf("denetim %s: %d bulgu\n", shot, static_cast<int>(found.size()));
  s_total += static_cast<int>(found.size());
  return static_cast<int>(found.size());
}

int total() { return s_total; }

}
