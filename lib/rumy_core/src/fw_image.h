#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace rc {

constexpr const char* FW_FILE = "/rumy/guncelleme.bin";
constexpr const char* FW_PART_FILE = "/rumy/guncelleme.part";
constexpr const char* FW_SHA_FILE = "/rumy/guncelleme.sha";
constexpr const char* FW_LABEL_APP = "app0";
constexpr const char* FW_LABEL_UPDATER = "updater";
constexpr size_t FW_SHA_BYTES = 32;
constexpr size_t FW_DESC_OFFSET = 32;
constexpr size_t FW_HASH_FLAG_OFFSET = 23;
constexpr size_t FW_HEAD_BYTES = FW_DESC_OFFSET + 176;
constexpr uint8_t FW_IMAGE_MAGIC = 0xE9;
constexpr uint32_t FW_DESC_MAGIC = 0xABCD5432u;

struct FwOffer {
  uint32_t size;
  uint8_t sha[FW_SHA_BYTES];
};

struct FwInfo {
  char version[33];
  char project[33];
  char date[17];
  char time[17];
  uint8_t elf[4];
};

inline int fwHexDigit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

inline bool fwHexToBytes(const char* hex, uint8_t* out, size_t n) {
  for (size_t i = 0; i < n; i++) {
    int hi = fwHexDigit(hex[2 * i]);
    int lo = hi < 0 ? -1 : fwHexDigit(hex[2 * i + 1]);
    if (lo < 0) return false;
    out[i] = static_cast<uint8_t>(hi * 16 + lo);
  }
  return true;
}

inline void fwBytesToHex(const uint8_t* in, size_t n, char* out) {
  static const char kHex[] = "0123456789abcdef";
  for (size_t i = 0; i < n; i++) {
    out[2 * i] = kHex[in[i] >> 4];
    out[2 * i + 1] = kHex[in[i] & 15];
  }
  out[2 * n] = 0;
}

inline bool parseFwOffer(const char* args, FwOffer& out) {
  char* end = nullptr;
  unsigned long size = strtoul(args, &end, 10);
  if (end == args || *end != ' ' || !size) return false;
  const char* hex = end + 1;
  size_t len = 0;
  while (fwHexDigit(hex[len]) >= 0) len++;
  if (len != 2 * FW_SHA_BYTES || (hex[len] && hex[len] != ' ')) return false;
  if (!fwHexToBytes(hex, out.sha, FW_SHA_BYTES)) return false;
  out.size = static_cast<uint32_t>(size);
  return true;
}

inline void fwField(char* dst, size_t cap, const uint8_t* src, size_t n) {
  size_t k = 0;
  while (k < n && k + 1 < cap && src[k]) {
    dst[k] = static_cast<char>(src[k]);
    k++;
  }
  dst[k] = 0;
}

inline bool fwImageInfo(const uint8_t* head, size_t n, FwInfo& out) {
  memset(&out, 0, sizeof(out));
  if (n < FW_HEAD_BYTES || head[0] != FW_IMAGE_MAGIC) return false;
  uint16_t chip = static_cast<uint16_t>(head[12] | (head[13] << 8));
  if (chip != 0 || head[FW_HASH_FLAG_OFFSET] != 1) return false;
  const uint8_t* d = head + FW_DESC_OFFSET;
  uint32_t magic = static_cast<uint32_t>(d[0]) | (static_cast<uint32_t>(d[1]) << 8) | (static_cast<uint32_t>(d[2]) << 16) |
                   (static_cast<uint32_t>(d[3]) << 24);
  if (magic != FW_DESC_MAGIC) return false;
  fwField(out.version, sizeof(out.version), d + 16, 32);
  fwField(out.project, sizeof(out.project), d + 48, 32);
  fwField(out.time, sizeof(out.time), d + 80, 16);
  fwField(out.date, sizeof(out.date), d + 96, 16);
  memcpy(out.elf, d + 144, sizeof(out.elf));
  return true;
}

inline size_t fwBodyBytes(uint32_t off, size_t n, uint32_t size) {
  uint32_t body = size > FW_SHA_BYTES ? size - static_cast<uint32_t>(FW_SHA_BYTES) : 0;
  if (off >= body) return 0;
  return body - off < n ? body - off : n;
}

inline bool parseFwShaFile(const char* text, size_t n, FwOffer& out) {
  char buf[96];
  if (n >= sizeof(buf)) return false;
  memcpy(buf, text, n);
  buf[n] = 0;
  char* sp = strchr(buf, ' ');
  if (!sp || sp - buf != static_cast<long>(2 * FW_SHA_BYTES)) return false;
  if (!fwHexToBytes(buf, out.sha, FW_SHA_BYTES)) return false;
  char* end = nullptr;
  unsigned long size = strtoul(sp + 1, &end, 10);
  if (end == sp + 1 || !size) return false;
  out.size = static_cast<uint32_t>(size);
  return true;
}

}
