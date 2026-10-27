#pragma once

#include <Client.h>
#include <stddef.h>
#include <stdint.h>

#include "reader.h"

namespace http {

enum Err : int16_t {
  E_CONNECT = -1,
  E_TIMEOUT = -2,
  E_PROTO = -3,
  E_WRITE = -4,
  E_TLS_MEM = -5,
  E_PARSE = -6,
  E_NO_WIFI = -7,
  E_BUILD = -8,
  E_ENCODING = -9,
};

class Response final : public rc::ByteSource {
 public:
  Response(Client& client, uint32_t timeoutMs);
  int begin(const char* host, const char* path);
  int read() override;
  int peek() override;
  size_t readBytes(char* buffer, size_t length) override;
  uint32_t bytes() const { return total_; }
  bool timedOut() const { return timedOut_; }
  bool complete() const { return eof_ && !timedOut_; }
  int32_t contentLength() const { return contentLength_; }

 private:
  bool fill();
  int raw();
  int next();
  bool readLine(char* out, size_t cap);
  bool startChunk();

  Client& c_;
  uint32_t timeoutMs_;
  uint32_t deadline_ = 0;
  uint32_t total_ = 0;
  int32_t contentLength_ = -1;
  int32_t left_ = -1;
  int32_t chunkLeft_ = 0;
  int peeked_ = -2;
  size_t pos_ = 0;
  size_t len_ = 0;
  bool chunked_ = false;
  bool firstChunk_ = true;
  bool eof_ = false;
  bool timedOut_ = false;
  uint8_t buf_[512];
};

const char* errText(int code);

}
