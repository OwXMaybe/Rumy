#include "wmo_tr.h"

namespace rc {

const char* wmoTextTr(uint8_t code) {
  switch (code) {
    case 0: return "Açık";
    case 1: return "Çoğunlukla açık";
    case 2: return "Parçalı bulutlu";
    case 3: return "Kapalı";
    case 45: return "Sisli";
    case 48: return "Kırağılı sis";
    case 51: return "Hafif çisenti";
    case 53: return "Çisenti";
    case 55: return "Yoğun çisenti";
    case 56: return "Hafif donan çisenti";
    case 57: return "Donan çisenti";
    case 61: return "Hafif yağmur";
    case 63: return "Yağmurlu";
    case 65: return "Şiddetli yağmur";
    case 66: return "Hafif donan yağmur";
    case 67: return "Donan yağmur";
    case 71: return "Hafif kar";
    case 73: return "Karlı";
    case 75: return "Yoğun kar";
    case 77: return "Kar taneleri";
    case 80: return "Hafif sağanak";
    case 81: return "Sağanak";
    case 82: return "Şiddetli sağanak";
    case 85: return "Hafif kar sağanağı";
    case 86: return "Kar sağanağı";
    case 95: return "Gök gürültülü fırtına";
    case 96: return "Dolulu fırtına";
    case 99: return "Şiddetli dolulu fırtına";
    default: return "Bilinmiyor";
  }
}

IconId wmoIcon(uint8_t code, bool isDay) {
  switch (code) {
    case 0: return isDay ? IC_CLEAR_DAY : IC_CLEAR_NIGHT;
    case 1:
    case 2: return isDay ? IC_PARTLY_DAY : IC_PARTLY_NIGHT;
    case 3: return IC_OVERCAST;
    case 45:
    case 48: return IC_FOG;
    case 51:
    case 53:
    case 55: return IC_DRIZZLE;
    case 56:
    case 57:
    case 66:
    case 67: return IC_SLEET;
    case 61:
    case 63:
    case 65: return IC_RAIN;
    case 71:
    case 73:
    case 75:
    case 77:
    case 85:
    case 86: return IC_SNOW;
    case 80:
    case 81:
    case 82: return isDay ? IC_SHOWERS_DAY : IC_SHOWERS_NIGHT;
    case 95: return IC_THUNDER;
    case 96:
    case 99: return IC_HAIL;
    default: return IC_OVERCAST;
  }
}

SkyClass wmoSky(uint8_t code, bool isDay) {
  if (code == 0 || code == 1) return isDay ? SKY_CLEAR_DAY : SKY_CLEAR_NIGHT;
  if (code == 2 || code == 3) return isDay ? SKY_CLOUDY_DAY : SKY_CLOUDY_NIGHT;
  if (code == 45 || code == 48) return SKY_FOG;
  if (code >= 95) return SKY_STORM;
  if (wmoIsSnow(code)) return SKY_SNOW;
  if (wmoIsWet(code)) return SKY_RAIN;
  return isDay ? SKY_CLOUDY_DAY : SKY_CLOUDY_NIGHT;
}

bool wmoIsWet(uint8_t code) { return (code >= 51 && code <= 67) || (code >= 71 && code <= 86) || code >= 95; }

bool wmoIsSnow(uint8_t code) { return (code >= 71 && code <= 77) || code == 85 || code == 86; }

const char* iconFileName(IconId id) {
  static const char* const kNames[IC_COUNT] = {
      "clear-day",   "clear-night", "partly-cloudy-day", "partly-cloudy-night", "overcast",   "fog",
      "drizzle",     "rain",        "sleet",             "snow",                "showers-day", "showers-night",
      "thunderstorm", "hail",       "sunrise",           "sunset",
  };
  if (id >= IC_COUNT) return "overcast";
  return kNames[id];
}

}
