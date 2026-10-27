#include "rumy_prof.h"

#if defined(RUMY_PROFILE) && RUMY_PROFILE

#include <Arduino.h>
#include <string.h>

#include "prof.h"

namespace {

constexpr int SLOTS = 96;
constexpr int DEPTH = 32;

struct Slot {
  const char* tag;
  uint32_t total;
  uint32_t self;
  uint32_t count;
};

struct Frame {
  const char* tag;
  uint32_t t0;
  uint32_t child;
};

Slot s_slots[SLOTS];
int s_slotN = 0;
Frame s_stack[DEPTH];
int s_depth = 0;
bool s_on = false;
TaskHandle_t s_owner = nullptr;

Slot* slot(const char* tag) {
  for (int i = 0; i < s_slotN; i++) {
    if (s_slots[i].tag == tag) return &s_slots[i];
  }
  if (s_slotN >= SLOTS) return nullptr;
  Slot* s = &s_slots[s_slotN++];
  s->tag = tag;
  s->total = s->self = s->count = 0;
  return s;
}

}

extern "C" void rumy_prof_begin(const char* tag) {
  if (!s_on || xTaskGetCurrentTaskHandle() != s_owner || s_depth >= DEPTH) return;
  Frame& f = s_stack[s_depth++];
  f.tag = tag;
  f.child = 0;
  f.t0 = micros();
}

extern "C" void rumy_prof_end(const char* tag) {
  if (!s_on || xTaskGetCurrentTaskHandle() != s_owner) return;
  uint32_t now = micros();
  while (s_depth > 0) {
    Frame f = s_stack[--s_depth];
    uint32_t dt = now - f.t0;
    if (s_depth > 0) s_stack[s_depth - 1].child += dt;
    if (f.tag != tag && strcmp(f.tag, tag) != 0) continue;
    Slot* s = slot(f.tag);
    if (s) {
      s->total += dt;
      s->self += dt > f.child ? dt - f.child : 0;
      s->count++;
    }
    return;
  }
}

namespace prof {

void start() {
  s_owner = xTaskGetCurrentTaskHandle();
  s_slotN = 0;
  s_depth = 0;
  s_on = true;
}

void stop() { s_on = false; }

void print(Print& out) {
  bool was = s_on;
  s_on = false;
  int idx[SLOTS];
  for (int i = 0; i < s_slotN; i++) idx[i] = i;
  for (int i = 0; i < s_slotN; i++) {
    for (int j = i + 1; j < s_slotN; j++) {
      if (s_slots[idx[j]].self > s_slots[idx[i]].self) {
        int t = idx[i];
        idx[i] = idx[j];
        idx[j] = t;
      }
    }
  }
  out.println(F("profil  (kendi ms | toplam ms | sayi | islev)"));
  for (int k = 0; k < s_slotN && k < 40; k++) {
    const Slot& s = s_slots[idx[k]];
    out.printf("  %7u | %7u | %6u | %s\n", static_cast<unsigned>(s.self / 1000), static_cast<unsigned>(s.total / 1000),
               static_cast<unsigned>(s.count), s.tag);
  }
  s_on = was;
}

}

#else

extern "C" void rumy_prof_begin(const char*) {}
extern "C" void rumy_prof_end(const char*) {}

#endif
