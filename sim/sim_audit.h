#pragma once

#include <stdio.h>

namespace audit {

void begin();
void start();
int finish(const char* shot, FILE* out);
int total();

}
