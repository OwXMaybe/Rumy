#pragma once

#include <arpa/inet.h>
#include <stdint.h>

class IPAddress {
 public:
  IPAddress() : a_(0) {}
  IPAddress(uint32_t a) : a_(a) {}
  operator uint32_t() const { return a_; }
  uint8_t operator[](int i) const { return static_cast<uint8_t>((a_ >> (8 * i)) & 0xFFu); }
  bool fromString(const char* s) {
    in_addr in;
    if (!s || inet_pton(AF_INET, s, &in) != 1) return false;
    a_ = in.s_addr;
    return true;
  }

 private:
  uint32_t a_;
};
