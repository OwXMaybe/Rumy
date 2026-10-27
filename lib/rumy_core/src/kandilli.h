#pragma once

#include <stddef.h>
#include <stdint.h>

#include "afad.h"
#include "rc_types.h"
#include "reader.h"

namespace rc {

extern const char* const KANDILLI_HOST;
extern const char* const KANDILLI_PATH;

bool kandilliLine(const char* line, Quake& q);
void kandilliPlace(const char* in, size_t inLen, char* out, size_t cap);
bool kandilliParse(ByteSource& src, uint32_t nowUtc, uint32_t windowSec, const QuakeBox& box, int16_t minMag10,
                   QuakeInfo& out);

bool relayQuakeLine(const char* text, QuakeInfo& out);
bool relayHolidayLine(const char* text, int year, Religious& out);

}
