#pragma once

#include <Print.h>
#include <stdint.h>

#include "agents_proto.h"

namespace agents {

void head(const char* args);
void row(char* fields);
void loop();
const rc::Agents& state();
uint32_t version();
uint32_t spanS(const rc::AgentRow& r);
void report(Print& out);
bool demo(const char* what);

}
