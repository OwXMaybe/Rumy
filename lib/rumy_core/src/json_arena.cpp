#include "json_arena.h"

#include <string.h>

namespace rc {

void JsonArena::attach(uint8_t* buffer, size_t capacity) {
  buf_ = buffer;
  cap_ = capacity & ~static_cast<size_t>(7u);
  top_ = 0;
  last_ = kNone;
  peak_ = 0;
  failures_ = 0;
}

size_t JsonArena::blockSize(size_t offset) const {
  uint32_t n;
  memcpy(&n, buf_ + offset, sizeof(n));
  return n;
}

void* JsonArena::allocate(size_t size) {
  if (!buf_) {
    failures_++;
    return nullptr;
  }
  size_t need = kHeader + roundUp(size);
  if (top_ + need > cap_) {
    failures_++;
    return nullptr;
  }
  uint32_t n = static_cast<uint32_t>(size);
  memcpy(buf_ + top_, &n, sizeof(n));
  last_ = top_;
  top_ += need;
  if (top_ > peak_) peak_ = top_;
  return buf_ + last_ + kHeader;
}

void JsonArena::deallocate(void* ptr) {
  if (!ptr || !buf_) return;
  uint8_t* p = static_cast<uint8_t*>(ptr);
  if (last_ != kNone && p == buf_ + last_ + kHeader) {
    top_ = last_;
    last_ = kNone;
  }
}

void* JsonArena::reallocate(void* ptr, size_t newSize) {
  if (!ptr) return allocate(newSize);
  uint8_t* p = static_cast<uint8_t*>(ptr);
  if (last_ != kNone && p == buf_ + last_ + kHeader) {
    size_t need = kHeader + roundUp(newSize);
    if (last_ + need > cap_) {
      failures_++;
      return nullptr;
    }
    uint32_t n = static_cast<uint32_t>(newSize);
    memcpy(buf_ + last_, &n, sizeof(n));
    top_ = last_ + need;
    if (top_ > peak_) peak_ = top_;
    return ptr;
  }
  size_t offset = static_cast<size_t>(p - buf_) - kHeader;
  size_t old = blockSize(offset);
  if (newSize <= old) {
    uint32_t n = static_cast<uint32_t>(newSize);
    memcpy(buf_ + offset, &n, sizeof(n));
    return ptr;
  }
  void* fresh = allocate(newSize);
  if (!fresh) return nullptr;
  memcpy(fresh, ptr, old < newSize ? old : newSize);
  return fresh;
}

void JsonArena::reset() {
  top_ = 0;
  last_ = kNone;
}

void JsonArena::rewind(size_t mark) {
  if (mark <= top_) top_ = mark;
  last_ = kNone;
}

}
