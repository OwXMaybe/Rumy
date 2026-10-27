#pragma once

#include <stdint.h>

#include "json_arena.h"
#include "reader.h"

namespace net {

using ParseFn = bool (*)(rc::ByteSource& src, void* ctx);

enum Via : uint8_t {
  VIA_HTTP = 0,
  VIA_TLS,
  VIA_BRIDGE,
  VIA_KANDILLI,
};

struct FetchStat {
  int16_t code;
  uint8_t via;
  uint8_t deferred;
  uint32_t bytes;
  uint32_t durMs;
};

int16_t get(const char* host, const char* path, bool tls, ParseFn parse, void* ctx, FetchStat& st);
rc::JsonArena& arena();
bool tlsAllowed();

}
