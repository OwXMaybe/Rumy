#include "bench.h"

#include <Arduino.h>
#include <lvgl.h>
#include <string.h>

#include "board/display.h"
#include "lvmem.h"
#include "screens.h"
#include "slide.h"
#include "ui.h"
#if defined(RUMY_PROFILE) && RUMY_PROFILE
#include "diag/prof.h"
#endif

namespace bench {

namespace {

enum Kind : uint8_t { K_GO, K_SCROLL, K_SCROLL_TOP, K_STRIP, K_WAIT };

struct Step {
  const char* name;
  uint8_t kind;
  uint8_t screen;
  int8_t dir;
  uint8_t modes;
};

constexpr uint8_t MA = 1u << ALL;
constexpr uint8_t MT = 1u << TRANSITIONS;
constexpr uint8_t MS = 1u << SCROLLS;
constexpr uint8_t MM = 1u << MEMORY;
constexpr uint8_t QUIETLY = 0x80;

const Step kSteps[] = {
    {"gecis ana>hava", K_GO, ui::SCR_WEATHER, 1, MA | MT},
    {"hava sessiz", K_GO, ui::SCR_WEATHER, 0, MS | QUIETLY},
    {"hava asagi kaydir", K_SCROLL, 0, 0, MA | MS},
    {"hava asagi kaydir 2", K_SCROLL, 0, 0, MA | MS},
    {"hava yukari kaydir", K_SCROLL_TOP, 0, 0, MA | MS},
    {"saatlik serit", K_STRIP, 0, 0, MA | MS},
    {"gecis hava>ana", K_GO, ui::SCR_HOME, -1, MA | MT},
    {"ana sessiz", K_GO, ui::SCR_HOME, 0, MS | QUIETLY},
    {"gecis ana>isik", K_GO, ui::SCR_LIGHT, 1, MA | MT},
    {"gecis isik>ana", K_GO, ui::SCR_HOME, -1, MA | MT},
    {"gecis ana>ayarlar", K_GO, ui::SCR_SETTINGS, 1, MA | MT},
    {"ayarlar asagi kaydir", K_SCROLL, 0, 0, MA},
    {"ayarlar yukari kaydir", K_SCROLL_TOP, 0, 0, MA},
    {"gecis ayarlar>ana", K_GO, ui::SCR_HOME, -1, MA | MT},
    {"ana bekle", K_WAIT, 0, 0, MM},
    {"ana>hava", K_GO, ui::SCR_WEATHER, 1, MM},
    {"hava kaydir", K_SCROLL, 0, 0, MM},
    {"hava kaydir 2", K_SCROLL, 0, 0, MM},
    {"hava kaydir 3", K_SCROLL, 0, 0, MM},
    {"saatlik serit", K_STRIP, 0, 0, MM},
    {"hava yukari", K_SCROLL_TOP, 0, 0, MM},
    {"hava>ana", K_GO, ui::SCR_HOME, -1, MM},
    {"ana>isik", K_GO, ui::SCR_LIGHT, 1, MM},
    {"isik>ana", K_GO, ui::SCR_HOME, -1, MM},
    {"ana>ayarlar", K_GO, ui::SCR_SETTINGS, 1, MM},
    {"ayarlar kaydir", K_SCROLL, 0, 0, MM},
    {"ayarlar kaydir 2", K_SCROLL, 0, 0, MM},
    {"ayarlar yukari", K_SCROLL_TOP, 0, 0, MM},
    {"ayarlar>ekran", K_GO, ui::SCR_DISPLAY_CFG, 1, MM},
    {"ekran kaydir", K_SCROLL, 0, 0, MM},
    {"ekran>ayarlar", K_GO, ui::SCR_SETTINGS, -1, MM},
    {"ayarlar>gece modu", K_GO, ui::SCR_NIGHT_CFG, 1, MM},
    {"gece modu>ayarlar", K_GO, ui::SCR_SETTINGS, -1, MM},
    {"ayarlar>konum", K_GO, ui::SCR_LOCATION, 1, MM},
    {"konum>ayarlar", K_GO, ui::SCR_SETTINGS, -1, MM},
    {"ayarlar>ampul", K_GO, ui::SCR_PAIR, 1, MM},
    {"ampul>ayarlar", K_GO, ui::SCR_SETTINGS, -1, MM},
    {"ayarlar>tanilama", K_GO, ui::SCR_DIAG, 1, MM},
    {"tanilama kaydir", K_SCROLL, 0, 0, MM},
    {"tanilama>ayarlar", K_GO, ui::SCR_SETTINGS, -1, MM},
    {"ayarlar>hakkinda", K_GO, ui::SCR_INFO, 1, MM},
    {"hakkinda>ayarlar", K_GO, ui::SCR_SETTINGS, -1, MM},
    {"ayarlar>ana", K_GO, ui::SCR_HOME, -1, MM},
    {"ana>spotify", K_GO, ui::SCR_MEDIA, -1, MM},
    {"spotify bekle", K_WAIT, 0, 0, MM},
    {"spotify>yazici", K_GO, ui::SCR_PRINTER, -1, MM},
    {"yazici bekle", K_WAIT, 0, 0, MM},
    {"yazici>ana", K_GO, ui::SCR_HOME, 1, MM},
    {"ana>gece", K_GO, ui::SCR_NIGHT, 0, MM},
    {"gece bekle", K_WAIT, 0, 0, MM},
    {"gece>ana", K_GO, ui::SCR_HOME, 0, MM},
    {"ana>kutlama", K_GO, ui::SCR_EVENT, 0, MM},
    {"kutlama bekle", K_WAIT, 0, 0, MM},
    {"kutlama>ana", K_GO, ui::SCR_HOME, 0, MM},
    {"ana bekle son", K_WAIT, 0, 0, MM},
};
constexpr int STEP_COUNT = sizeof(kSteps) / sizeof(kSteps[0]);
constexpr uint32_t QUIET_MS = 250;
constexpr uint32_t STEP_TIMEOUT_MS = 4000;
constexpr uint32_t WAIT_MS = 2500;
constexpr uint32_t MEM_SETTLE_MS = 700;

struct Acc {
  uint32_t frames;
  uint32_t firstUs;
  uint32_t lastUs;
  uint32_t maxGapUs;
  uint32_t busyUs;
  uint32_t busyMaxUs;
};

bool s_active = false;
bool s_profiling = false;
Mode s_mode = ALL;
bool s_live = false;
bool s_framed = false;
int s_step = -1;
bool s_running = false;
uint32_t s_stepStart = 0;
uint32_t s_actUs = 0;
uint32_t s_liveMark = 0;
Acc s_acc;
Acc s_live_acc;
uint32_t s_sumFrames = 0;
uint32_t s_sumSpanUs = 0;
uint32_t s_minBig = 0;
uint32_t s_topPeak = 0;
uint32_t s_lowBig = 0;
const char* s_topStep = "";
const char* s_lowStep = "";

void reset(Acc& a) { memset(&a, 0, sizeof(a)); }

void addFrame(Acc& a, uint32_t now) {
  if (a.frames) {
    uint32_t gap = now - a.lastUs;
    if (gap > a.maxGapUs) a.maxGapUs = gap;
  } else {
    a.firstUs = now;
  }
  a.lastUs = now;
  a.frames++;
}

bool act(const Step& st) {
  lv_obj_t* scr = lv_screen_active();
  switch (st.kind) {
    case K_GO:
      ui::go(static_cast<ui::ScreenId>(st.screen), st.dir);
      return true;
    case K_SCROLL:
      if (!scr || !lv_obj_has_flag(scr, LV_OBJ_FLAG_SCROLLABLE)) return false;
      lv_obj_scroll_by(scr, 0, -200, LV_ANIM_ON);
      return true;
    case K_SCROLL_TOP:
      if (!scr || !lv_obj_has_flag(scr, LV_OBJ_FLAG_SCROLLABLE)) return false;
      lv_obj_scroll_to_y(scr, 0, LV_ANIM_ON);
      return true;
    case K_STRIP:
      return scr_weather::benchStrip();
    case K_WAIT:
      return true;
    default:
      return false;
  }
}

void memReport(const char* name) {
  lvmem::Snap m = lvmem::snap();
  if (m.biggest < s_minBig) s_minBig = m.biggest;
  Serial.printf("bellek %-20s kullanim %5u  tepe %5u  en buyuk bos %5u (en az %5u)  parca %%%u  / %u\n", name,
                static_cast<unsigned>(m.used), static_cast<unsigned>(m.peak), static_cast<unsigned>(m.biggest),
                static_cast<unsigned>(s_minBig), static_cast<unsigned>(m.frag), static_cast<unsigned>(m.total));
  if (m.peak > s_topPeak) {
    s_topPeak = m.peak;
    s_topStep = name;
  }
  if (!s_lowBig || s_minBig < s_lowBig) {
    s_lowBig = s_minBig;
    s_lowStep = name;
  }
}

void report(const char* name, const Acc& a, const display::FlushPerf& fp) {
  uint32_t span = a.frames > 1 ? a.lastUs - a.firstUs : 0;
  uint32_t fps10 = span ? static_cast<uint32_t>((static_cast<uint64_t>(a.frames - 1) * 10000000ULL) / span) : 0;
  uint32_t firstMs = a.frames ? (a.firstUs - s_actUs) / 1000 : 0;
  Serial.printf("bench %-22s %3u kare %5u ms  %3u.%u fps  ilk kare %3u ms  en uzun %3u ms  cizim ort %3u ms en cok %3u ms  "
                "aktarim %4u ms  dokunma %3u ms  %u px",
                name, static_cast<unsigned>(a.frames), static_cast<unsigned>(span / 1000), static_cast<unsigned>(fps10 / 10),
                static_cast<unsigned>(fps10 % 10), static_cast<unsigned>(firstMs), static_cast<unsigned>(a.maxGapUs / 1000),
                static_cast<unsigned>(a.frames ? a.busyUs / a.frames / 1000 : 0), static_cast<unsigned>(a.busyMaxUs / 1000),
                static_cast<unsigned>(fp.flushUs / 1000), static_cast<unsigned>(fp.touchUs / 1000),
                static_cast<unsigned>(fp.pixels));
  if (kSteps[s_step].kind == K_GO) {
    slide::Stats ss = slide::last();
    Serial.printf("  olustur %u ms  kayma %u kare %u ms en uzun %u ms", static_cast<unsigned>(ui::lastCreateUs() / 1000),
                  static_cast<unsigned>(ss.frames), static_cast<unsigned>(ss.totalMs),
                  static_cast<unsigned>(ss.maxFrameMs));
  }
  Serial.println();
  if (a.frames > 1) {
    s_sumFrames += a.frames - 1;
    s_sumSpanUs += span;
  }
}

int nextStep(int from) {
  for (int i = from; i < STEP_COUNT; i++) {
    if (kSteps[i].modes & (1u << s_mode)) return i;
  }
  return -1;
}

void begin(int i) {
  s_step = i;
  s_running = false;
  s_stepStart = millis();
}

void finish() {
  s_active = false;
  if (s_mode == MEMORY) {
    Serial.printf("bellek bitti: en yuksek tepe %u B (%s), en kucuk bos blok %u B (%s)\n",
                  static_cast<unsigned>(s_topPeak), s_topStep, static_cast<unsigned>(s_lowBig), s_lowStep);
    return;
  }
  uint32_t fps10 = s_sumSpanUs ? static_cast<uint32_t>((static_cast<uint64_t>(s_sumFrames) * 10000000ULL) / s_sumSpanUs) : 0;
  Serial.printf("bench bitti: hareketli karelerde ortalama %u.%u fps\n", static_cast<unsigned>(fps10 / 10),
                static_cast<unsigned>(fps10 % 10));
#if defined(RUMY_PROFILE) && RUMY_PROFILE
  prof::stop();
  prof::print(Serial);
#endif
}

void advance() {
  int n = nextStep(s_step + 1);
  if (n < 0) {
    finish();
    return;
  }
  begin(n);
}

}

void start(Mode mode) {
  if (s_active) return;
  s_active = true;
  s_mode = mode;
  s_profiling = false;
  s_sumFrames = 0;
  s_sumSpanUs = 0;
  s_topPeak = 0;
  s_lowBig = 0;
  s_topStep = "";
  s_lowStep = "";
  Serial.println(F("bench basliyor"));
  if (ui::current() != ui::SCR_HOME) ui::go(ui::SCR_HOME, 0);
  begin(-1);
}

bool active() { return s_active; }

void setLive(bool on) {
  s_live = on;
  reset(s_live_acc);
  display::takeFlushPerf();
  s_liveMark = millis();
}

void frameReady() {
  uint32_t now = micros();
  s_framed = true;
  if (s_active && s_running) addFrame(s_acc, now);
  if (s_live) addFrame(s_live_acc, now);
}

void handlerDone(uint32_t us) {
  if (!s_framed) return;
  s_framed = false;
  if (s_active && s_running) {
    s_acc.busyUs += us;
    if (us > s_acc.busyMaxUs) s_acc.busyMaxUs = us;
  }
  if (s_live) {
    s_live_acc.busyUs += us;
    if (us > s_live_acc.busyMaxUs) s_live_acc.busyMaxUs = us;
  }
}

void tick() {
  uint32_t now = millis();
  if (s_live && !s_active && now - s_liveMark >= 1000) {
    display::FlushPerf fp = display::takeFlushPerf();
    if (s_live_acc.frames) {
      Serial.printf("perf %u kare  cizim ort %u ms en cok %u ms  en uzun aralik %u ms  aktarim %u ms  dokunma %u ms  %u px\n",
                    static_cast<unsigned>(s_live_acc.frames),
                    static_cast<unsigned>(s_live_acc.busyUs / s_live_acc.frames / 1000),
                    static_cast<unsigned>(s_live_acc.busyMaxUs / 1000), static_cast<unsigned>(s_live_acc.maxGapUs / 1000),
                    static_cast<unsigned>(fp.flushUs / 1000), static_cast<unsigned>(fp.touchUs / 1000),
                    static_cast<unsigned>(fp.pixels));
    }
    reset(s_live_acc);
    s_liveMark = now;
  }
  if (!s_active) return;
  if (s_step < 0) {
    if (now - s_stepStart < 1200 || ui::busy()) return;
    int n = nextStep(0);
    if (n < 0) {
      finish();
      return;
    }
    begin(n);
  }
  const Step& st = kSteps[s_step];
  bool silent = (st.modes & QUIETLY) != 0;
  if (!s_running) {
    if (now - s_stepStart < 300 || ui::busy()) return;
#if defined(RUMY_PROFILE) && RUMY_PROFILE
    if (!silent && !s_profiling) {
      s_profiling = true;
      prof::start();
    }
#endif
    reset(s_acc);
    display::takeFlushPerf();
    s_actUs = micros();
    lv_display_trigger_activity(nullptr);
    if (s_mode == MEMORY) {
      lvmem::resetPeak();
      s_minBig = lvmem::snap().biggest;
    }
    if (!act(st)) {
      Serial.printf("bench %-22s atlandi\n", st.name);
      advance();
      return;
    }
    s_running = true;
    s_stepStart = now;
    return;
  }
  if (s_mode == MEMORY) {
    uint32_t big = lvmem::snap().biggest;
    if (big < s_minBig) s_minBig = big;
  }
  bool quiet = s_acc.frames && micros() - s_acc.lastUs > QUIET_MS * 1000UL;
  bool done = now - s_stepStart > STEP_TIMEOUT_MS;
  if (st.kind == K_WAIT) done = now - s_stepStart > WAIT_MS;
  else if (st.kind == K_GO) done = done || (!ui::busy() && now - s_stepStart > (s_mode == MEMORY ? MEM_SETTLE_MS : 100));
  else done = done || (now - s_stepStart > 200 && quiet);
  if (!done) return;
  s_running = false;
  display::FlushPerf fp = display::takeFlushPerf();
  if (s_mode == MEMORY) memReport(st.name);
  else if (!silent) report(st.name, s_acc, fp);
  advance();
}

}
