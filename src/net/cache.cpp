#include "cache.h"

#include <stdio.h>
#include <string.h>

#include "app/config.h"
#include "board/sdcard.h"
#include "cache_fmt.h"

namespace cache {

namespace {

constexpr uint16_t VERSION = 1;
constexpr int KINDS = 8;
constexpr size_t MAX_PAYLOAD = 480;
constexpr uint32_t REWRITE_AFTER_S = 3600;
constexpr size_t STAMP_BYTES = 4;

rc::ChangeGate s_gates[KINDS];
uint32_t s_lastWrite[KINDS] = {};

void pathFor(uint16_t kind, char* out, size_t cap) { snprintf(out, cap, "%s/k%u.bin", cfg::SD_CACHE_DIR, kind); }

const uint8_t* body(const void* data, size_t size, size_t& len) {
  const uint8_t* p = static_cast<const uint8_t*>(data);
  if (size > STAMP_BYTES) {
    len = size - STAMP_BYTES;
    return p + STAMP_BYTES;
  }
  len = size;
  return p;
}

}

bool save(uint16_t kind, const void* data, size_t size, uint32_t nowUtc) {
  if (kind >= KINDS || size > MAX_PAYLOAD || !sdcard::mounted()) return false;
  size_t blen = 0;
  const uint8_t* b = body(data, size, blen);
  bool changed = s_gates[kind].changed(b, blen);
  bool stale = nowUtc > s_lastWrite[kind] + REWRITE_AFTER_S;
  if (!changed && !stale) return true;
  uint8_t buf[sizeof(rc::BlobHeader) + MAX_PAYLOAD];
  size_t n = rc::blobPack(kind, VERSION, data, static_cast<uint32_t>(size), buf, sizeof(buf));
  if (!n) return false;
  char path[48];
  pathFor(kind, path, sizeof(path));
  if (!sdcard::writeFileAtomic(path, buf, n)) return false;
  s_gates[kind].remember(b, blen);
  s_lastWrite[kind] = nowUtc;
  return true;
}

bool load(uint16_t kind, void* data, size_t size) {
  if (kind >= KINDS || size > MAX_PAYLOAD || !sdcard::mounted()) return false;
  uint8_t buf[sizeof(rc::BlobHeader) + MAX_PAYLOAD];
  size_t len = 0;
  char path[48];
  pathFor(kind, path, sizeof(path));
  if (!sdcard::readFile(path, buf, sizeof(buf), len)) return false;
  if (!rc::blobUnpackInto(buf, len, kind, VERSION, data, static_cast<uint32_t>(size))) return false;
  size_t blen = 0;
  const uint8_t* b = body(data, size, blen);
  s_gates[kind].remember(b, blen);
  uint32_t stamp = 0;
  if (size >= STAMP_BYTES) memcpy(&stamp, data, STAMP_BYTES);
  s_lastWrite[kind] = stamp;
  return true;
}

void forget(uint16_t kind) {
  if (kind >= KINDS) return;
  s_gates[kind] = rc::ChangeGate();
  s_lastWrite[kind] = 0;
  char path[48];
  pathFor(kind, path, sizeof(path));
  sdcard::remove(path);
}

}
