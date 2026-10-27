#pragma once

#include <stddef.h>
#include <stdint.h>

namespace rc {

constexpr uint32_t BLOB_MAGIC = 0x594D5552u;

enum BlobKind : uint16_t {
  BLOB_FORECAST = 1,
  BLOB_DAILY = 2,
  BLOB_AIR = 3,
  BLOB_MARINE = 4,
  BLOB_QUAKE = 5,
  BLOB_RELIGIOUS = 6,
};

struct BlobHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t kind;
  uint32_t size;
  uint32_t crc;
};

static_assert(sizeof(BlobHeader) == 16, "BlobHeader 16 bayt olmali");

uint32_t crc32(const void* data, size_t len, uint32_t seed = 0);
size_t blobPack(uint16_t kind, uint16_t version, const void* payload, uint32_t size, uint8_t* out, size_t cap);
bool blobUnpack(const uint8_t* data, size_t len, uint16_t kind, uint16_t version, const uint8_t** payload,
                uint32_t* size);
bool blobUnpackInto(const uint8_t* data, size_t len, uint16_t kind, uint16_t version, void* dst, uint32_t dstSize);

class ChangeGate {
 public:
  bool changed(const void* payload, size_t len);
  void remember(const void* payload, size_t len);
  uint32_t last() const { return crc_; }

 private:
  uint32_t crc_ = 0;
  bool known_ = false;
};

}
