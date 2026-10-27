#include "media_proto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace rc {

namespace {

constexpr int NP_FIELDS = 9;
constexpr uint32_t FNV_BASIS = 2166136261u;
constexpr uint32_t FNV_PRIME = 16777619u;
constexpr int32_t LOCK_PERIOD_PCT = 4;
constexpr int32_t LOCK_PHASE_MS = 90;

uint32_t fnvField(uint32_t h, const char* s) {
  while (*s) h = (h ^ static_cast<uint8_t>(*s++)) * FNV_PRIME;
  return (h ^ 0x1Fu) * FNV_PRIME;
}

char* nextField(char*& p) {
  if (!p) return nullptr;
  char* out = p;
  char* tab = strchr(p, '\t');
  if (tab) {
    *tab = 0;
    p = tab + 1;
  } else {
    p = nullptr;
  }
  return out;
}

const char* skipSpaces(const char* p) {
  while (*p == ' ') p++;
  return p;
}

bool word(const char*& p, const char* w) {
  size_t n = strlen(w);
  if (strncmp(p, w, n) != 0 || (p[n] && p[n] != ' ')) return false;
  p = skipSpaces(p + n);
  return true;
}

}

bool parsePlaylistHead(char* args, PlaylistHead& out) {
  char* end = nullptr;
  uint32_t ver = strtoul(args, &end, 16);
  if (end == args) return false;
  char* nEnd = nullptr;
  unsigned long n = strtoul(end, &nEnd, 10);
  if (nEnd == end) return false;
  const char* p = skipSpaces(nEnd);
  uint8_t status;
  if (word(p, "hazir")) status = PLS_READY;
  else if (word(p, "yukleniyor")) status = PLS_LOADING;
  else if (word(p, "giris")) status = PLS_LOGIN;
  else if (word(p, "hata")) status = PLS_FAIL;
  else return false;
  if ((*p != '0' && *p != '1') || (p[1] && p[1] != ' ')) return false;
  out.ver = ver;
  out.status = status;
  out.shuffle = *p == '1';
  out.n = status != PLS_READY ? 0 : static_cast<uint8_t>(n > static_cast<unsigned long>(PL_MAX) ? PL_MAX : n);
  out.msg = skipSpaces(p + 1);
  return true;
}

bool parsePlaylistLine(char* fields, PlaylistLine& out) {
  constexpr int FIELDS = 5;
  char* f[FIELDS];
  char* p = fields;
  for (int i = 0; i < FIELDS; i++) {
    f[i] = nextField(p);
    if (!f[i]) return false;
  }
  char* end = nullptr;
  unsigned long index = strtoul(f[0], &end, 10);
  if (end == f[0] || index >= static_cast<unsigned long>(PL_MAX)) return false;
  unsigned long cell = strtoul(f[3], &end, 10);
  if (end == f[3] || cell >= static_cast<unsigned long>(PL_PER_SHEET)) return false;
  unsigned long total = strtoul(f[1], nullptr, 10);
  out.index = static_cast<uint8_t>(index);
  out.total = static_cast<uint16_t>(total > 65535UL ? 65535UL : total);
  out.sheet = strtoul(f[2], nullptr, 16);
  out.cell = static_cast<uint8_t>(cell);
  out.name = f[4];
  return true;
}

bool parsePlaylistReply(char* args, PlaylistReply& out) {
  char* end = nullptr;
  long index = strtol(args, &end, 10);
  if (end == args || index < -1 || index >= PL_MAX) return false;
  const char* p = skipSpaces(end);
  uint8_t result;
  if (word(p, "bas")) result = PLR_STARTING;
  else if (word(p, "tamam")) result = PLR_OK;
  else if (word(p, "hata")) result = PLR_FAIL;
  else return false;
  out.index = static_cast<int8_t>(index);
  out.result = result;
  out.msg = p;
  return true;
}

uint32_t trackHash(const char* title, const char* artist, const char* album) {
  return fnvField(fnvField(fnvField(FNV_BASIS, title), artist), album);
}

void parseNowPlaying(char* fields, NowPlaying& out) {
  static char empty[1] = "";
  char* f[NP_FIELDS];
  char* p = fields;
  for (int i = 0; i < NP_FIELDS; i++) {
    f[i] = nextField(p);
    if (!f[i]) f[i] = empty;
  }
  out.playing = f[0][0] == '1';
  out.posMs = strtoul(f[1], nullptr, 10);
  out.durMs = strtoul(f[2], nullptr, 10);
  out.artHash = strtoul(f[3], nullptr, 16);
  out.accent = strtoul(f[4], nullptr, 16);
  out.app = f[5];
  out.title = f[6];
  out.artist = f[7];
  out.album = f[8];
  out.active = f[6][0] != 0;
  out.track = trackHash(f[6], f[7], f[8]);
}

bool parseArtOffer(const char* args, size_t slotBytes, ArtOffer& out) {
  char* end = nullptr;
  uint32_t hash = strtoul(args, &end, 16);
  unsigned long w = strtoul(end, &end, 10);
  unsigned long h = strtoul(end, &end, 10);
  unsigned long len = strtoul(end, &end, 10);
  uint32_t crc = strtoul(end, &end, 16);
  if (!hash || !w || !h || w > ART_MAX_SIDE || h > ART_MAX_SIDE || len != w * h * 2UL || ART_HDR + len > slotBytes)
    return false;
  out.hash = hash;
  out.w = static_cast<uint16_t>(w);
  out.h = static_cast<uint16_t>(h);
  out.len = static_cast<uint32_t>(len);
  out.crc = crc;
  return true;
}

bool artSlotValid(const ArtSlotHdr& h, uint16_t side, size_t slotBytes, uint16_t alt) {
  bool sized = h.w == side || (alt && h.w == alt && h.h == alt);
  return h.magic == ART_MAGIC && sized && h.h && h.size == static_cast<uint32_t>(h.w) * h.h * 2U &&
         ART_HDR + h.size <= slotBytes;
}

uint32_t photoHash(const char* name, uint16_t px) {
  uint32_t h = 2166136261u;
  for (const char* p = name ? name : ""; *p; p++) {
    h ^= static_cast<uint8_t>(*p);
    h *= 16777619u;
  }
  h ^= px;
  h *= 16777619u;
  return h ? h : 1;
}

size_t footballLine(char* out, size_t cap, bool on, uint32_t team, uint32_t c1, uint32_t c2) {
  if (!out || !cap) return 0;
  int n = snprintf(out, cap, "@futbol %d %lu %06lx %06lx\n", on ? 1 : 0, static_cast<unsigned long>(on ? team : 0),
                   static_cast<unsigned long>(c1 & 0xFFFFFF), static_cast<unsigned long>(c2 & 0xFFFFFF));
  if (n < 0 || static_cast<size_t>(n) >= cap) {
    out[0] = 0;
    return 0;
  }
  return static_cast<size_t>(n);
}

uint32_t predictPosMs(uint32_t posMs, uint32_t elapsedMs, bool playing, uint32_t durMs) {
  if (!playing) return posMs;
  uint32_t p = posMs + elapsedMs;
  return durMs && p > durMs ? durMs : p;
}

bool seekJumped(uint32_t posMs, uint32_t predictedMs, uint32_t slackMs) {
  int32_t drift = static_cast<int32_t>(posMs) - static_cast<int32_t>(predictedMs);
  return drift > static_cast<int32_t>(slackMs) || drift < -static_cast<int32_t>(slackMs);
}

bool parseTempo(const char* args, TempoMsg& out) {
  char* end = nullptr;
  unsigned long period = strtoul(args, &end, 10);
  unsigned long next = strtoul(end, &end, 10);
  out.periodMs = 0;
  out.nextMs = 0;
  if (period < TEMPO_MIN_MS || period > TEMPO_MAX_MS) return false;
  out.periodMs = static_cast<uint32_t>(period);
  out.nextMs = static_cast<uint32_t>(next % period);
  return true;
}

uint32_t lastBeat(uint32_t anchor, uint32_t period, uint32_t now) {
  int32_t P = static_cast<int32_t>(period);
  int32_t d = static_cast<int32_t>(now - anchor);
  int32_t m = ((d % P) + P) % P;
  return now - static_cast<uint32_t>(m);
}

void beatLock(uint32_t& period, uint32_t& anchor, uint32_t newPeriod, uint32_t newAnchor, uint32_t now) {
  if (!newPeriod) {
    period = 0;
    return;
  }
  uint32_t nb = lastBeat(newAnchor, newPeriod, now);
  if (period) {
    int32_t P = static_cast<int32_t>(newPeriod);
    int32_t dp = static_cast<int32_t>(newPeriod) - static_cast<int32_t>(period);
    if (dp < 0) dp = -dp;
    if (dp * 100 < static_cast<int32_t>(period) * LOCK_PERIOD_PCT) {
      uint32_t ob = lastBeat(anchor, period, now);
      int32_t err = static_cast<int32_t>(nb - ob);
      err = ((err % P) + P + P / 2) % P - P / 2;
      if (err < LOCK_PHASE_MS && err > -LOCK_PHASE_MS) {
        anchor = ob + static_cast<uint32_t>(err / 3);
        period = (period * 3 + newPeriod) / 4;
        return;
      }
    }
  }
  period = newPeriod;
  anchor = nb;
}

}
