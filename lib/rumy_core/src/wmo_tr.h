#pragma once

#include <stdint.h>

namespace rc {

enum IconId : uint8_t {
  IC_CLEAR_DAY = 0,
  IC_CLEAR_NIGHT,
  IC_PARTLY_DAY,
  IC_PARTLY_NIGHT,
  IC_OVERCAST,
  IC_FOG,
  IC_DRIZZLE,
  IC_RAIN,
  IC_SLEET,
  IC_SNOW,
  IC_SHOWERS_DAY,
  IC_SHOWERS_NIGHT,
  IC_THUNDER,
  IC_HAIL,
  IC_SUNRISE,
  IC_SUNSET,
  IC_COUNT,
  IC_NONE = 0xFF,
};

enum SkyClass : uint8_t {
  SKY_CLEAR_DAY = 0,
  SKY_CLEAR_NIGHT,
  SKY_CLOUDY_DAY,
  SKY_CLOUDY_NIGHT,
  SKY_RAIN,
  SKY_SNOW,
  SKY_STORM,
  SKY_FOG,
  SKY_COUNT,
};

const char* wmoTextTr(uint8_t code);
IconId wmoIcon(uint8_t code, bool isDay);
SkyClass wmoSky(uint8_t code, bool isDay);
bool wmoIsWet(uint8_t code);
bool wmoIsSnow(uint8_t code);
const char* iconFileName(IconId id);

}
