#pragma once

#include <stddef.h>
#include <stdint.h>

namespace rc {

class ByteSource {
 public:
  virtual int read() = 0;
  virtual int peek() = 0;
  virtual size_t readBytes(char* buffer, size_t length) = 0;

 protected:
  ~ByteSource() = default;
};

class MemSource final : public ByteSource {
 public:
  MemSource(const char* data, size_t len) : p_(data), n_(len), i_(0) {}
  int read() override { return i_ < n_ ? static_cast<unsigned char>(p_[i_++]) : -1; }
  int peek() override { return i_ < n_ ? static_cast<unsigned char>(p_[i_]) : -1; }
  size_t readBytes(char* buffer, size_t length) override {
    size_t k = 0;
    while (k < length && i_ < n_) buffer[k++] = p_[i_++];
    return k;
  }
  size_t position() const { return i_; }

 private:
  const char* p_;
  size_t n_;
  size_t i_;
};

inline bool isJsonSpace(int c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; }

inline int skipSpaces(ByteSource& s) {
  int c = s.peek();
  while (c >= 0 && isJsonSpace(c)) {
    s.read();
    c = s.peek();
  }
  return c;
}

inline bool expectChar(ByteSource& s, char want) {
  int c = skipSpaces(s);
  if (c != static_cast<unsigned char>(want)) return false;
  s.read();
  return true;
}

}
