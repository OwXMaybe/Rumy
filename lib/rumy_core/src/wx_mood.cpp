#include "wx_mood.h"

namespace rc {

namespace {

bool between(uint8_t c, uint8_t a, uint8_t b) { return c >= a && c <= b; }

}

WxKind wxKind(const WxNow& n) {
  const uint8_t c = n.code;
  if (c == NA8) return WXK_NONE;
  if (between(c, 95, 99)) return WXK_STORM;
  if (between(c, 71, 77) || c == 85 || c == 86) return WXK_SNOW;
  if (between(c, 61, 67) || between(c, 80, 82)) return WXK_RAIN;
  if (between(c, 51, 57)) return WXK_DRIZZLE;
  if (c == 45 || c == 48) return WXK_FOG;
  if ((n.gust10 != NAU16 && n.gust10 >= WXK_GUST10) || (n.wind10 != NAU16 && n.wind10 >= WXK_WIND10)) return WXK_WINDY;
  const int16_t feels = n.feels10 != NA16 ? n.feels10 : n.temp10;
  if (feels != NA16 && feels <= WXK_COLD_FEELS10) return WXK_COLD;
  if (feels != NA16 && feels >= WXK_HOT_FEELS10) return WXK_HOT;
  const bool clear = c <= 1;
  if (clear && n.isDay) {
    bool hot = (feels != NA16 && feels >= WXK_SUNNY_HOT_FEELS10) || (n.uv10 != NA8 && n.uv10 >= WXK_SUNNY_HOT_UV10);
    return hot ? WXK_SUNNY_HOT : WXK_SUNNY;
  }
  if (clear) return WXK_CLEAR_NIGHT;
  if (c == 3) return WXK_CLOUDY;
  return WXK_NONE;
}

bool wxKindWet(WxKind k) { return k == WXK_DRIZZLE || k == WXK_RAIN || k == WXK_STORM; }

const char* wxKindName(WxKind k) {
  static const char* const kNames[WXK_COUNT] = {"parçalı bulutlu", "güneşli",   "güneşli ve sıcak", "bunaltıcı sıcak",
                                                "kapalı",          "sisli",     "çisenti",          "yağmurlu",
                                                "karlı",           "fırtınalı", "rüzgarlı",         "soğuk",
                                                "açık gece"};
  return k < WXK_COUNT ? kNames[k] : "?";
}

}
