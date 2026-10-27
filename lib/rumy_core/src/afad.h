#pragma once

#include <stddef.h>
#include <stdint.h>

#include "json_arena.h"
#include "rc_types.h"
#include "reader.h"

namespace rc {

extern const char* const AFAD_HOST;

struct QuakeBox {
  float minLat;
  float maxLat;
  float minLon;
  float maxLon;
};

QuakeBox marmaraBox();
QuakeBox boxAround(float lat, float lon);
size_t afadPath(char* out, size_t cap, uint32_t nowUtc, uint32_t windowSec, const QuakeBox& box, float minMag);
bool afadParse(ByteSource& src, JsonArena& arena, uint32_t nowUtc, QuakeInfo& out);
void afadShortPlace(const char* in, char* out, size_t cap);
size_t utf8Copy(char* out, size_t cap, const char* in, size_t inLen);

}
