#include "look.h"

#include <freertos/FreeRTOS.h>

#include "rc_types.h"
#include "rumy.h"

namespace look {

namespace {

portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
Pick s_req = {};
uint32_t s_seq = 0;
Pick s_shown = {};

}

bool set(uint8_t mood, uint8_t acc, uint8_t fx) {
  if (mood >= rumy::MOOD_COUNT || acc > rc::ACC_UMBRELLA || fx >= rumy::FX_COUNT) return false;
  portENTER_CRITICAL(&s_mux);
  s_req = {mood, acc, fx, 1};
  s_seq++;
  portEXIT_CRITICAL(&s_mux);
  return true;
}

void clear() {
  portENTER_CRITICAL(&s_mux);
  s_req.mine = 0;
  s_seq++;
  portEXIT_CRITICAL(&s_mux);
}

bool request(Pick& out, uint32_t& seq) {
  portENTER_CRITICAL(&s_mux);
  out = s_req;
  seq = s_seq;
  portEXIT_CRITICAL(&s_mux);
  return out.mine != 0;
}

void drop(uint32_t seq) {
  portENTER_CRITICAL(&s_mux);
  if (s_seq == seq) s_req.mine = 0;
  portEXIT_CRITICAL(&s_mux);
}

void publish(const Pick& shown) {
  portENTER_CRITICAL(&s_mux);
  s_shown = shown;
  portEXIT_CRITICAL(&s_mux);
}

Pick shown() {
  portENTER_CRITICAL(&s_mux);
  Pick p = s_shown;
  portEXIT_CRITICAL(&s_mux);
  return p;
}

}
