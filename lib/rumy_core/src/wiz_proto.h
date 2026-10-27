#pragma once

#include <stddef.h>
#include <stdint.h>

#include "color.h"
#include "json_arena.h"

namespace rc {

enum PilotMode : uint8_t {
  PILOT_RGB = 0,
  PILOT_TEMP = 1,
  PILOT_SCENE = 2,
};

struct Pilot {
  uint8_t valid;
  uint8_t on;
  uint8_t dim;
  uint8_t mode;
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t scene;
  uint16_t temp;
  uint16_t hue;
};

constexpr uint8_t WIZ_DIM_MIN = 10;
constexpr uint8_t WIZ_DIM_MAX = 100;
constexpr uint16_t WIZ_TEMP_MIN = 2200;
constexpr uint16_t WIZ_TEMP_MAX = 6500;
constexpr uint16_t WIZ_PORT = 38899;

Pilot pilotRgb(uint16_t hue, uint8_t dim);
Pilot pilotRgbRaw(uint8_t r, uint8_t g, uint8_t b, uint8_t dim);
Pilot pilotTemp(uint16_t kelvin, uint8_t dim);
Pilot pilotOff(const Pilot& base);
Pilot pilotClamp(const Pilot& p);
bool pilotSame(const Pilot& a, const Pilot& b);
Rgb pilotColor(const Pilot& p);

size_t wizBuildSetPilot(const Pilot& p, char* out, size_t cap);
size_t wizBuildGetPilot(char* out, size_t cap);
size_t wizBuildRegistration(char* out, size_t cap);

enum class WizMethod : uint8_t {
  Unknown = 0,
  Registration,
  GetPilot,
  SetPilot,
  SyncPilot,
};

constexpr int16_t WIZ_ERR_INVALID_PARAMS = -32602;

struct WizReply {
  WizMethod method;
  uint8_t success;
  uint8_t hasMac;
  uint8_t hasPilot;
  int16_t errorCode;
  char mac[13];
  Pilot pilot;
};

bool wizParseReply(const char* buf, size_t len, JsonArena& arena, WizReply& out);

}
