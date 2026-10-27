#include "cache_fmt.h"

#include <string.h>

namespace rc {

uint32_t crc32(const void* data, size_t len, uint32_t seed) {
  static const uint32_t kNibble[16] = {0x00000000, 0x1DB71064, 0x3B6E20C8, 0x26D930AC, 0x76DC4190, 0x6B6B51F4,
                                       0x4DB26158, 0x5005713C, 0xEDB88320, 0xF00F9344, 0xD6D6A3E8, 0xCB61B38C,
                                       0x9B64C2B0, 0x86D3D2D4, 0xA00AE278, 0xBDBDF21C};
  const uint8_t* p = static_cast<const uint8_t*>(data);
  uint32_t crc = ~seed;
  for (size_t i = 0; i < len; i++) {
    crc = kNibble[(crc ^ p[i]) & 0x0F] ^ (crc >> 4);
    crc = kNibble[(crc ^ (p[i] >> 4)) & 0x0F] ^ (crc >> 4);
  }
  return ~crc;
}

size_t blobPack(uint16_t kind, uint16_t version, const void* payload, uint32_t size, uint8_t* out, size_t cap) {
  if (cap < sizeof(BlobHeader) + size) return 0;
  BlobHeader h;
  h.magic = BLOB_MAGIC;
  h.version = version;
  h.kind = kind;
  h.size = size;
  h.crc = crc32(payload, size);
  memcpy(out, &h, sizeof(h));
  memcpy(out + sizeof(h), payload, size);
  return sizeof(h) + size;
}

bool blobUnpack(const uint8_t* data, size_t len, uint16_t kind, uint16_t version, const uint8_t** payload,
                uint32_t* size) {
  if (!data || len < sizeof(BlobHeader)) return false;
  BlobHeader h;
  memcpy(&h, data, sizeof(h));
  if (h.magic != BLOB_MAGIC || h.kind != kind || h.version != version) return false;
  if (h.size > len - sizeof(BlobHeader)) return false;
  const uint8_t* p = data + sizeof(BlobHeader);
  if (crc32(p, h.size) != h.crc) return false;
  if (payload) *payload = p;
  if (size) *size = h.size;
  return true;
}

bool blobUnpackInto(const uint8_t* data, size_t len, uint16_t kind, uint16_t version, void* dst, uint32_t dstSize) {
  const uint8_t* p = nullptr;
  uint32_t n = 0;
  if (!blobUnpack(data, len, kind, version, &p, &n)) return false;
  if (n != dstSize) return false;
  memcpy(dst, p, n);
  return true;
}

bool ChangeGate::changed(const void* payload, size_t len) {
  uint32_t c = crc32(payload, len);
  return !known_ || c != crc_;
}

void ChangeGate::remember(const void* payload, size_t len) {
  crc_ = crc32(payload, len);
  known_ = true;
}

}
