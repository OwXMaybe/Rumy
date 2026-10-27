#include "pairing.h"

#include <string.h>

namespace rc {

bool macNormalize(const char* in, char out[13]) {
  out[0] = 0;
  if (!in) return false;
  int k = 0;
  for (const char* p = in; *p; ++p) {
    char c = *p;
    if (c == ':' || c == '-' || c == '.') continue;
    if (c >= 'A' && c <= 'F') c = static_cast<char>(c - 'A' + 'a');
    bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    if (!hex || k >= 12) {
      out[0] = 0;
      return false;
    }
    out[k++] = c;
  }
  if (k != 12) {
    out[0] = 0;
    return false;
  }
  out[12] = 0;
  return true;
}

bool macEqual(const char* a, const char* b) {
  char x[13], y[13];
  if (!macNormalize(a, x) || !macNormalize(b, y)) return false;
  return memcmp(x, y, 12) == 0;
}

int BulbList::add(uint32_t ip, const char* mac, uint32_t nowMs) {
  char m[13];
  if (!macNormalize(mac, m)) return -1;
  for (int i = 0; i < n_; i++) {
    if (memcmp(items_[i].mac, m, 12) == 0) {
      items_[i].ip = ip;
      items_[i].seenMs = nowMs;
      return i;
    }
  }
  if (n_ >= CAP) return -1;
  FoundBulb& f = items_[n_];
  f.ip = ip;
  f.seenMs = nowMs;
  memcpy(f.mac, m, 13);
  return n_++;
}

int BulbList::find(const char* mac) const {
  char m[13];
  if (!macNormalize(mac, m)) return -1;
  for (int i = 0; i < n_; i++) {
    if (memcmp(items_[i].mac, m, 12) == 0) return i;
  }
  return -1;
}

bool SendThrottle::shouldSend(uint32_t nowMs, const Pilot& desired, bool final) const {
  if (!desired.valid) return false;
  if (sent_ && pilotSame(desired, last_)) return false;
  if (final) return true;
  if (!sent_) return true;
  return static_cast<uint32_t>(nowMs - lastMs_) >= interval_;
}

void SendThrottle::markSent(uint32_t nowMs, const Pilot& p) {
  last_ = p;
  lastMs_ = nowMs;
  sent_ = true;
}

bool syncAllowed(uint32_t nowMs, bool userTouching, uint32_t lastSendMs, bool everSent, uint32_t quietMs) {
  if (userTouching) return false;
  if (everSent && static_cast<uint32_t>(nowMs - lastSendMs) < quietMs) return false;
  return true;
}

void OnlineTracker::onRequest(uint32_t nowMs) {
  if (!pending_) {
    pending_ = true;
    pendingSince_ = nowMs;
  }
}

void OnlineTracker::onReply(uint32_t nowMs) {
  pending_ = false;
  misses_ = 0;
  online_ = true;
  lastReply_ = nowMs;
}

void OnlineTracker::tick(uint32_t nowMs) {
  if (pending_ && static_cast<uint32_t>(nowMs - pendingSince_) >= timeout_) {
    pending_ = false;
    if (misses_ < 255) misses_++;
    if (misses_ >= maxMisses_) online_ = false;
  }
}

}
