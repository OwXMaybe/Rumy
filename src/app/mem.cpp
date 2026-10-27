#include "mem.h"

#include <esp_heap_caps.h>

#include "config.h"
#include "lv_conf.h"
#include "rumy_lv_pool.h"

static_assert(cfg::LVGL_POOL_BYTES == LV_MEM_SIZE, "LVGL havuz boyutu tutarsiz");

namespace {

uint8_t* g_lvPool = nullptr;
uint8_t* g_draw1 = nullptr;
uint8_t* g_draw2 = nullptr;
uint8_t* g_json = nullptr;
uint8_t* g_wiz = nullptr;

uint8_t* grab(size_t bytes, uint32_t caps) {
  return static_cast<uint8_t*>(heap_caps_aligned_alloc(4, bytes, caps));
}

}

extern "C" void* rumy_lv_pool_alloc(size_t size) {
  if (!g_lvPool) g_lvPool = grab(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  return g_lvPool;
}

namespace mem {

bool init() {
  if (!g_lvPool) g_lvPool = grab(cfg::LVGL_POOL_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!g_draw1) g_draw1 = grab(cfg::DRAW_BUF_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (!g_draw2) g_draw2 = grab(cfg::DRAW_BUF_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (!g_json) g_json = grab(cfg::JSON_ARENA_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!g_wiz) g_wiz = grab(cfg::WIZ_JSON_ARENA_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  return g_lvPool && g_draw1 && g_draw2 && g_json && g_wiz;
}

uint8_t* drawBuf1() { return g_draw1; }
uint8_t* drawBuf2() { return g_draw2; }
uint8_t* jsonArena() { return g_json; }
uint8_t* wizArena() { return g_wiz; }

uint32_t freeHeap() { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); }
uint32_t minFreeHeap() { return heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); }
uint32_t largestBlock() { return heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT); }

void report(Print& out) {
  out.printf("[mem] lvgl=%p/%u draw=%p,%p/%u json=%p/%u wiz=%p/%u\n", g_lvPool,
             static_cast<unsigned>(cfg::LVGL_POOL_BYTES), g_draw1, g_draw2, static_cast<unsigned>(cfg::DRAW_BUF_BYTES),
             g_json, static_cast<unsigned>(cfg::JSON_ARENA_BYTES), g_wiz, static_cast<unsigned>(cfg::WIZ_JSON_ARENA_BYTES));
  out.printf("[mem] free=%u min=%u largest=%u\n", static_cast<unsigned>(freeHeap()), static_cast<unsigned>(minFreeHeap()),
             static_cast<unsigned>(largestBlock()));
}

}
