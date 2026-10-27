#pragma once

#include <stdint.h>

namespace bench {

enum Mode : uint8_t { ALL = 0, TRANSITIONS, SCROLLS, MEMORY };

void start(Mode mode);
bool active();
void setLive(bool on);
void frameReady();
void handlerDone(uint32_t us);
void tick();

}
