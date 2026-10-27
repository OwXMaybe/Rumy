#pragma once

#include <stdint.h>

#include "wiz_proto.h"

namespace wiz {

void begin();
void setDesired(const rc::Pilot& p, bool final);
void setInteracting(bool on);
void togglePower();
void celebrate();
void alert();
void scan();
void pair(const char* mac);
void forget();
uint32_t stackFree();

}
