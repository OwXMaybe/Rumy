#pragma once

#include <memory>
#include <stddef.h>
#include <stdint.h>

#include "Arduino.h"
#include "IPAddress.h"

enum WlStatus : uint8_t {
  WL_IDLE_STATUS = 0,
  WL_CONNECTED = 3,
  WL_DISCONNECTED = 6,
};

class PiWiFi {
 public:
  WlStatus status();
  int8_t RSSI();
  IPAddress localIP();
  IPAddress subnetMask();
};

extern PiWiFi WiFi;

class WiFiClient {
 public:
  WiFiClient() = default;
  explicit WiFiClient(int fd);
  explicit operator bool() const { return fd_ && *fd_ >= 0; }
  int available();
  int read();
  int read(uint8_t* buf, size_t n);
  bool connected();
  size_t write(const uint8_t* buf, size_t n);
  void flush() {}
  void stop();

 private:
  std::shared_ptr<int> fd_;
};

class WiFiServer {
 public:
  explicit WiFiServer(uint16_t port) : port_(port) {}
  void begin();
  void end();
  void setNoDelay(bool on) { noDelay_ = on; }
  WiFiClient available();

 private:
  uint16_t port_;
  int fd_ = -1;
  bool noDelay_ = false;
};
