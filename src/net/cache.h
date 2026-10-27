#pragma once

#include <stddef.h>
#include <stdint.h>

namespace cache {

bool save(uint16_t kind, const void* data, size_t size, uint32_t nowUtc);
bool load(uint16_t kind, void* data, size_t size);
void forget(uint16_t kind);

}
