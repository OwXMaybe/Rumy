#pragma once

#include <stdint.h>

#include "rc_types.h"

namespace rc {

enum WxKind : uint8_t {
  WXK_NONE = 0,
  WXK_SUNNY,
  WXK_SUNNY_HOT,
  WXK_HOT,
  WXK_CLOUDY,
  WXK_FOG,
  WXK_DRIZZLE,
  WXK_RAIN,
  WXK_SNOW,
  WXK_STORM,
  WXK_WINDY,
  WXK_COLD,
  WXK_CLEAR_NIGHT,
  WXK_COUNT,
};

constexpr int16_t WXK_HOT_FEELS10 = 330;
constexpr int16_t WXK_SUNNY_HOT_FEELS10 = 270;
constexpr uint8_t WXK_SUNNY_HOT_UV10 = 60;
constexpr int16_t WXK_COLD_FEELS10 = 20;
constexpr uint16_t WXK_WIND10 = 350;
constexpr uint16_t WXK_GUST10 = 500;

WxKind wxKind(const WxNow& n);
bool wxKindWet(WxKind k);
const char* wxKindName(WxKind k);

}
