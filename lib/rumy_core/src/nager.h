#pragma once

#include <stddef.h>
#include <stdint.h>

#include "json_arena.h"
#include "rc_types.h"
#include "reader.h"

namespace rc {

extern const char* const NAGER_HOST;

enum ReligiousSource : uint8_t {
  REL_NONE = 0,
  REL_NAGER = 1,
  REL_TABLE = 2,
  REL_NAGER_TENTATIVE = 3,
};

size_t nagerPath(char* out, size_t cap, int year);
bool nagerParse(ByteSource& src, JsonArena& arena, int year, Religious& out);
bool religiousTable(int year, Religious& out);
Religious religiousBest(int year, const Religious* fromNager);

}
