#include "fast.h"

#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/shared.h"
#include "net/net.h"

namespace fast {

namespace {

constexpr int SLOTS = 8;

Msg s_q[SLOTS];
volatile uint8_t s_head = 0;
volatile uint8_t s_tail = 0;
volatile uint32_t s_received = 0;
portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

uint8_t kindOf(char c) {
  if (c == 'p') return rc::MK_PEN_GOAL;
  if (c == 'o') return rc::MK_OWN_GOAL;
  return rc::MK_GOAL;
}

void post(const Msg& m) {
  portENTER_CRITICAL(&s_mux);
  uint8_t next = static_cast<uint8_t>((s_head + 1) % SLOTS);
  if (next == s_tail) s_tail = static_cast<uint8_t>((s_tail + 1) % SLOTS);
  s_q[s_head] = m;
  s_head = next;
  s_received = s_received + 1;
  portEXIT_CRITICAL(&s_mux);
}

}

bool parseLine(const char* text, Msg& out) {
  memset(&out, 0, sizeof(out));
  unsigned long id = 0;
  unsigned h = 0;
  unsigned a = 0;
  int used = 0;
  if (!strncmp(text, "skor ", 5)) {
    if (sscanf(text + 5, "%lu %u %u", &id, &h, &a) != 3) return false;
    out.type = T_SCORE;
  } else if (!strncmp(text, "gol ", 4)) {
    char side = 0;
    char kind = 0;
    char minute[sizeof(out.minute)] = "";
    if (sscanf(text + 4, "%lu %u %u %c %c %7s %n", &id, &h, &a, &side, &kind, minute, &used) < 6) return false;
    out.type = T_GOAL;
    out.homeSide = side == 'h' ? 1 : 0;
    out.kind = kindOf(kind);
    snprintf(out.minute, sizeof(out.minute), "%s", minute);
    if (used > 0) snprintf(out.player, sizeof(out.player), "%s", text + 4 + used);
  } else {
    return false;
  }
  if (!id || h > 30 || a > 30) return false;
  out.espnId = static_cast<uint32_t>(id);
  out.home = static_cast<uint8_t>(h);
  out.away = static_cast<uint8_t>(a);
  return true;
}

void line(const char* text) {
  Msg m;
  if (!parseLine(text, m)) return;
  post(m);
  net::refresh(SRC_MATCH);
}

bool take(Msg& out) {
  bool got = false;
  portENTER_CRITICAL(&s_mux);
  if (s_tail != s_head) {
    out = s_q[s_tail];
    s_tail = static_cast<uint8_t>((s_tail + 1) % SLOTS);
    got = true;
  }
  portEXIT_CRITICAL(&s_mux);
  return got;
}

uint32_t received() { return s_received; }

}
