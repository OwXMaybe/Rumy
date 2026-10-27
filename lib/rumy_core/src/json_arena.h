#pragma once

#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>

namespace rc {

class JsonArena final : public ArduinoJson::Allocator {
 public:
  JsonArena() = default;
  JsonArena(uint8_t* buffer, size_t capacity) { attach(buffer, capacity); }
  virtual ~JsonArena() = default;

  void attach(uint8_t* buffer, size_t capacity);
  void* allocate(size_t size) override;
  void deallocate(void* ptr) override;
  void* reallocate(void* ptr, size_t newSize) override;

  void reset();
  size_t mark() const { return top_; }
  void rewind(size_t mark);

  size_t used() const { return top_; }
  size_t peak() const { return peak_; }
  size_t capacity() const { return cap_; }
  uint32_t failures() const { return failures_; }
  bool attached() const { return buf_ != nullptr; }

 private:
  static constexpr size_t kHeader = 8;
  static constexpr size_t kNone = static_cast<size_t>(-1);
  static size_t roundUp(size_t n) { return (n + 7u) & ~static_cast<size_t>(7u); }
  size_t blockSize(size_t offset) const;

  uint8_t* buf_ = nullptr;
  size_t cap_ = 0;
  size_t top_ = 0;
  size_t last_ = kNone;
  size_t peak_ = 0;
  uint32_t failures_ = 0;
};

}
