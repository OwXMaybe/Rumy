#pragma once

#include <stddef.h>
#include <stdint.h>

#include "json_arena.h"
#include "reader.h"

namespace rc {

enum K2Phase : uint8_t {
  K2_OFFLINE = 0,
  K2_IDLE,
  K2_PRINTING,
  K2_PAUSED,
  K2_DONE,
  K2_STOPPED,
  K2_ERROR,
  K2_SELFTEST,
};

enum K2Field : uint32_t {
  K2F_STATE = 1u << 0,
  K2F_PROGRESS = 1u << 1,
  K2F_LEFT = 1u << 2,
  K2F_JOB = 1u << 3,
  K2F_FILE = 1u << 4,
  K2F_LAYER = 1u << 5,
  K2F_TOTAL_LAYER = 1u << 6,
  K2F_NOZZLE = 1u << 7,
  K2F_NOZZLE_TARGET = 1u << 8,
  K2F_BED = 1u << 9,
  K2F_BED_TARGET = 1u << 10,
  K2F_BOX = 1u << 11,
  K2F_ERR = 1u << 12,
  K2F_SELFTEST = 1u << 13,
  K2F_MATERIAL = 1u << 14,
  K2F_HOST = 1u << 15,
  K2F_MODEL = 1u << 16,
  K2F_START = 1u << 17,
  K2F_SPEED = 1u << 18,
  K2F_HEARTBEAT = 1u << 31,
};

constexpr uint32_t K2_WS_PORT = 9999;
constexpr int16_t K2_NO_TEMP = -32768;

struct K2Status {
  uint32_t seen;
  int16_t state;
  uint8_t progress;
  uint8_t selfTest;
  uint8_t runout;
  uint8_t speedPct;
  uint16_t layer;
  uint16_t totalLayer;
  uint32_t leftS;
  uint32_t jobS;
  uint32_t startUtc;
  int16_t nozzle10;
  int16_t nozzleTarget10;
  int16_t bed10;
  int16_t bedTarget10;
  int16_t box10;
  int16_t reserved;
  int32_t errCode;
  int32_t errKey;
  char errText[48];
  char file[72];
  char host[24];
  char model[12];
};

void k2Clear(K2Status& st);
bool k2Parse(ByteSource& src, JsonArena& arena, K2Status& st, uint32_t& present);
K2Phase k2Phase(const K2Status& st);
bool k2Runout(const K2Status& st);
bool k2Problem(const K2Status& st);
void k2JobName(const char* file, char* name, size_t cap, char* material, size_t mcap);
size_t k2BuildStatusRequest(char* out, size_t cap);
bool k2HostValid(const char* host);
const char* k2PhaseTr(K2Phase p);

}
