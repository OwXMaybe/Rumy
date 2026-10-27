#pragma once

#include <stdint.h>

#include "app/shared.h"

namespace net {

void begin();
void refreshAll();
void refresh(NetSource s);
void locationChanged();
bool online();
uint32_t stackFree();

}
