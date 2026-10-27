#pragma once

#include <stddef.h>
#include <stdint.h>

#include "wiz_proto.h"

namespace rc {

bool macNormalize(const char* in, char out[13]);
bool macEqual(const char* a, const char* b);

struct FoundBulb {
  uint32_t ip;
  uint32_t seenMs;
  char mac[13];
  uint8_t reserved[3];
};

class BulbList {
 public:
  static constexpr int CAP = 8;
  void clear() { n_ = 0; }
  int add(uint32_t ip, const char* mac, uint32_t nowMs);
  int find(const char* mac) const;
  int count() const { return n_; }
  const FoundBulb& at(int i) const { return items_[i]; }

 private:
  FoundBulb items_[CAP] = {};
  int n_ = 0;
};

class SendThrottle {
 public:
  explicit SendThrottle(uint32_t minIntervalMs = 80) : interval_(minIntervalMs) {}
  bool shouldSend(uint32_t nowMs, const Pilot& desired, bool final) const;
  void markSent(uint32_t nowMs, const Pilot& p);
  uint32_t lastSendMs() const { return lastMs_; }
  bool everSent() const { return sent_; }
  const Pilot& lastSent() const { return last_; }

 private:
  uint32_t interval_;
  uint32_t lastMs_ = 0;
  bool sent_ = false;
  Pilot last_ = {};
};

bool syncAllowed(uint32_t nowMs, bool userTouching, uint32_t lastSendMs, bool everSent, uint32_t quietMs = 1000);

class OnlineTracker {
 public:
  explicit OnlineTracker(uint32_t replyTimeoutMs = 1500, uint8_t maxMisses = 2)
      : timeout_(replyTimeoutMs), maxMisses_(maxMisses) {}
  void onRequest(uint32_t nowMs);
  void onReply(uint32_t nowMs);
  void tick(uint32_t nowMs);
  bool online() const { return online_; }
  uint8_t misses() const { return misses_; }
  uint32_t lastReplyMs() const { return lastReply_; }

 private:
  uint32_t timeout_;
  uint8_t maxMisses_;
  uint32_t pendingSince_ = 0;
  bool pending_ = false;
  bool online_ = false;
  uint8_t misses_ = 0;
  uint32_t lastReply_ = 0;
};

}
