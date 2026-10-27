#include "lvmem.h"

#include <lvgl.h>

#include "src/core/lv_global.h"

namespace lvmem {

namespace {

constexpr uint32_t CHUNK = 1024;
constexpr int MAX_CHUNKS = 32;

void* s_hold[MAX_CHUNKS] = {};
int s_count = 0;

}

void resetPeak() { LV_GLOBAL_DEFAULT()->tlsf_state.max_used = LV_GLOBAL_DEFAULT()->tlsf_state.cur_used; }

Snap snap() {
  lv_mem_monitor_t m;
  lv_mem_monitor(&m);
  Snap s;
  s.total = static_cast<uint32_t>(m.total_size);
  s.used = static_cast<uint32_t>(m.total_size - m.free_size);
  s.free = static_cast<uint32_t>(m.free_size);
  s.peak = static_cast<uint32_t>(m.max_used);
  s.biggest = static_cast<uint32_t>(m.free_biggest_size);
  s.frag = m.frag_pct;
  return s;
}

bool hold(uint32_t bytes) {
  while (s_count > 0) lv_free(s_hold[--s_count]);
  uint32_t want = (bytes + CHUNK - 1) / CHUNK;
  while (s_count < MAX_CHUNKS && static_cast<uint32_t>(s_count) < want) {
    void* p = lv_malloc(CHUNK);
    if (!p) return false;
    s_hold[s_count++] = p;
  }
  return static_cast<uint32_t>(s_count) >= want;
}

uint32_t held() { return static_cast<uint32_t>(s_count) * CHUNK; }

}
