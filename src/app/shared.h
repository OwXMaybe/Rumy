#pragma once

#include <stdint.h>

#include "events_calc.h"
#include "k2.h"
#include "pairing.h"
#include "rc_types.h"
#include "wiz_proto.h"

enum NetSource : uint8_t {
  SRC_FORECAST = 0,
  SRC_MARINE,
  SRC_AIR,
  SRC_QUAKE,
  SRC_HOLIDAY,
  SRC_MATCH,
  SRC_BOARD,
  SRC_LEAGUE,
  SRC_COUNT,
};

struct SourceStat {
  int16_t lastCode;
  uint8_t lastOk;
  uint8_t via;
  uint16_t fails;
  uint16_t deferred;
  uint32_t lastTryUtc;
  uint32_t lastOkUtc;
  uint32_t durMs;
  uint32_t nextInS;
  uint32_t bytes;
};

struct NetStatus {
  uint8_t wifi;
  uint8_t timeSynced;
  int8_t rssi;
  uint8_t webActive;
  uint32_t ip;
  uint16_t webPin;
  uint16_t reserved;
  uint32_t tlsPeak;
  uint32_t tlsDeferred;
  uint32_t lastFullOkUtc;
  SourceStat src[SRC_COUNT];
};

struct BulbState {
  uint8_t paired;
  uint8_t found;
  uint8_t online;
  uint8_t scanning;
  uint32_t ip;
  uint32_t lastSeenMs;
  char mac[13];
  uint8_t candidateCount;
  uint8_t writeDenied;
  uint8_t reserved;
  rc::Pilot pilot;
  rc::FoundBulb candidates[rc::BulbList::CAP];
};

struct PrinterState {
  uint8_t configured;
  uint8_t connected;
  uint8_t phase;
  uint8_t problem;
  uint32_t ip;
  uint32_t updatedMs;
  uint32_t msgs;
  uint16_t reconnects;
  uint16_t parseFails;
  uint16_t arenaPeak;
  uint16_t reserved;
  rc::K2Status st;
};

struct LvglStats {
  uint32_t total;
  uint32_t used;
  uint32_t maxUsed;
  uint32_t biggestFree;
  uint8_t usedPct;
  uint8_t fragPct;
  uint8_t reserved[2];
  uint32_t uiStackFree;
  uint32_t fps;
};
