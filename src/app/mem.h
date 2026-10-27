#pragma once

#include <Print.h>
#include <stddef.h>
#include <stdint.h>

namespace mem {

bool init();
uint8_t* drawBuf1();
uint8_t* drawBuf2();
uint8_t* jsonArena();
uint8_t* wizArena();
void report(Print& out);
uint32_t freeHeap();
uint32_t minFreeHeap();
uint32_t largestBlock();

}
