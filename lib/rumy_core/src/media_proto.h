#pragma once

#include <stddef.h>
#include <stdint.h>

namespace rc {

constexpr uint32_t ART_MAGIC = 0x54524152u;
constexpr size_t ART_HDR = 32;
constexpr uint16_t ART_MAX_SIDE = 256;
constexpr uint32_t TEMPO_MIN_MS = 250;
constexpr uint32_t TEMPO_MAX_MS = 2000;

struct NowPlaying {
  bool playing;
  bool active;
  uint32_t posMs;
  uint32_t durMs;
  uint32_t artHash;
  uint32_t accent;
  uint32_t track;
  const char* app;
  const char* title;
  const char* artist;
  const char* album;
};

void parseNowPlaying(char* fields, NowPlaying& out);
uint32_t trackHash(const char* title, const char* artist, const char* album);

struct ArtOffer {
  uint32_t hash;
  uint16_t w;
  uint16_t h;
  uint32_t len;
  uint32_t crc;
};

bool parseArtOffer(const char* args, size_t slotBytes, ArtOffer& out);

struct ArtSlotHdr {
  uint32_t magic;
  uint32_t hash;
  uint32_t seq;
  uint16_t w;
  uint16_t h;
  uint32_t crc;
  uint32_t size;
};

bool artSlotValid(const ArtSlotHdr& h, uint16_t side, size_t slotBytes, uint16_t alt = 0);
uint32_t photoHash(const char* name, uint16_t px);
size_t footballLine(char* out, size_t cap, bool on, uint32_t team, uint32_t c1, uint32_t c2);

uint32_t predictPosMs(uint32_t posMs, uint32_t elapsedMs, bool playing, uint32_t durMs);
bool seekJumped(uint32_t posMs, uint32_t predictedMs, uint32_t slackMs);

struct TempoMsg {
  uint32_t periodMs;
  uint32_t nextMs;
};

bool parseTempo(const char* args, TempoMsg& out);

constexpr int PL_MAX = 30;
constexpr int PL_PER_SHEET = 9;
constexpr int PL_SHEETS = (PL_MAX + PL_PER_SHEET - 1) / PL_PER_SHEET;

enum PlStatus : uint8_t { PLS_NONE = 0, PLS_LOADING, PLS_READY, PLS_LOGIN, PLS_FAIL };
enum PlResult : uint8_t { PLR_NONE = 0, PLR_STARTING, PLR_OK, PLR_FAIL };

struct PlaylistHead {
  uint32_t ver;
  uint8_t n;
  uint8_t status;
  bool shuffle;
  const char* msg;
};

bool parsePlaylistHead(char* args, PlaylistHead& out);

struct PlaylistLine {
  uint8_t index;
  uint16_t total;
  uint32_t sheet;
  uint8_t cell;
  const char* name;
};

bool parsePlaylistLine(char* fields, PlaylistLine& out);

struct PlaylistReply {
  int8_t index;
  uint8_t result;
  const char* msg;
};

bool parsePlaylistReply(char* args, PlaylistReply& out);

uint32_t lastBeat(uint32_t anchor, uint32_t period, uint32_t now);
void beatLock(uint32_t& period, uint32_t& anchor, uint32_t newPeriod, uint32_t newAnchor, uint32_t now);

}
