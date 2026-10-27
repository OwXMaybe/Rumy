#include "wiz_proto.h"

#include <stdio.h>
#include <string.h>

#include "pairing.h"

namespace rc {

namespace {

uint8_t clampDim(int v) {
  if (v < WIZ_DIM_MIN) return WIZ_DIM_MIN;
  if (v > WIZ_DIM_MAX) return WIZ_DIM_MAX;
  return static_cast<uint8_t>(v);
}

uint16_t clampTemp(int v) {
  if (v < WIZ_TEMP_MIN) return WIZ_TEMP_MIN;
  if (v > WIZ_TEMP_MAX) return WIZ_TEMP_MAX;
  return static_cast<uint16_t>(v);
}

uint8_t clamp8(int v) { return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v)); }

size_t finish(int n, size_t cap) {
  if (n < 0 || static_cast<size_t>(n) >= cap) return 0;
  return static_cast<size_t>(n);
}

}

Pilot pilotRgb(uint16_t hue, uint8_t dim) {
  Rgb c = hsvToRgb(hue % 360, 255, 255);
  Pilot p{};
  p.valid = 1;
  p.on = 1;
  p.dim = clampDim(dim);
  p.mode = PILOT_RGB;
  p.r = c.r;
  p.g = c.g;
  p.b = c.b;
  p.hue = hue % 360;
  p.temp = 0;
  return p;
}

Pilot pilotRgbRaw(uint8_t r, uint8_t g, uint8_t b, uint8_t dim) {
  Pilot p{};
  p.valid = 1;
  p.on = 1;
  p.dim = clampDim(dim);
  p.mode = PILOT_RGB;
  p.r = r;
  p.g = g;
  p.b = b;
  p.hue = rgbToHue(r, g, b);
  return p;
}

Pilot pilotTemp(uint16_t kelvin, uint8_t dim) {
  Pilot p{};
  p.valid = 1;
  p.on = 1;
  p.dim = clampDim(dim);
  p.mode = PILOT_TEMP;
  p.temp = clampTemp(kelvin);
  return p;
}

Pilot pilotOff(const Pilot& base) {
  Pilot p = base;
  p.valid = 1;
  p.on = 0;
  return p;
}

Pilot pilotClamp(const Pilot& in) {
  Pilot p = in;
  p.dim = clampDim(p.dim);
  if (p.mode == PILOT_TEMP) p.temp = clampTemp(p.temp);
  if (p.mode > PILOT_SCENE) p.mode = PILOT_RGB;
  p.hue %= 360;
  return p;
}

bool pilotSame(const Pilot& a, const Pilot& b) {
  if (a.valid != b.valid) return false;
  if (a.on != b.on) return false;
  if (!a.on) return true;
  if (a.dim != b.dim || a.mode != b.mode) return false;
  switch (a.mode) {
    case PILOT_RGB: return a.r == b.r && a.g == b.g && a.b == b.b;
    case PILOT_TEMP: return a.temp == b.temp;
    default: return a.scene == b.scene;
  }
}

Rgb pilotColor(const Pilot& p) {
  if (!p.valid || !p.on) return {70, 76, 88};
  if (p.mode == PILOT_TEMP) return kelvinToRgb(p.temp);
  if (p.mode == PILOT_SCENE) return {255, 190, 120};
  if (p.r == 0 && p.g == 0 && p.b == 0) return {255, 255, 255};
  return {p.r, p.g, p.b};
}

size_t wizBuildSetPilot(const Pilot& in, char* out, size_t cap) {
  Pilot p = pilotClamp(in);
  int n;
  if (!p.on) {
    n = snprintf(out, cap, "{\"method\":\"setPilot\",\"params\":{\"state\":false}}");
  } else if (p.mode == PILOT_TEMP) {
    n = snprintf(out, cap, "{\"method\":\"setPilot\",\"params\":{\"state\":true,\"dimming\":%u,\"temp\":%u}}",
                 static_cast<unsigned>(p.dim), static_cast<unsigned>(p.temp));
  } else if (p.mode == PILOT_SCENE && p.scene > 0) {
    n = snprintf(out, cap, "{\"method\":\"setPilot\",\"params\":{\"state\":true,\"dimming\":%u,\"sceneId\":%u}}",
                 static_cast<unsigned>(p.dim), static_cast<unsigned>(p.scene));
  } else {
    n = snprintf(out, cap,
                 "{\"method\":\"setPilot\",\"params\":{\"state\":true,\"dimming\":%u,\"r\":%u,\"g\":%u,\"b\":%u}}",
                 static_cast<unsigned>(p.dim), static_cast<unsigned>(p.r), static_cast<unsigned>(p.g),
                 static_cast<unsigned>(p.b));
  }
  return finish(n, cap);
}

size_t wizBuildGetPilot(char* out, size_t cap) {
  return finish(snprintf(out, cap, "{\"method\":\"getPilot\",\"params\":{}}"), cap);
}

size_t wizBuildRegistration(char* out, size_t cap) {
  return finish(snprintf(out, cap,
                         "{\"method\":\"registration\",\"params\":{\"phoneMac\":\"AAAAAAAAAAAA\","
                         "\"register\":false,\"phoneIp\":\"1.2.3.4\",\"id\":\"1\"}}"),
                cap);
}

bool wizParseReply(const char* buf, size_t len, JsonArena& arena, WizReply& out) {
  memset(&out, 0, sizeof(out));
  arena.reset();
  JsonDocument doc(&arena);
  DeserializationError err = deserializeJson(doc, buf, len);
  if (err) {
    doc.clear();
    arena.reset();
    return false;
  }
  const char* method = doc["method"] | "";
  if (strcmp(method, "registration") == 0) out.method = WizMethod::Registration;
  else if (strcmp(method, "getPilot") == 0) out.method = WizMethod::GetPilot;
  else if (strcmp(method, "setPilot") == 0) out.method = WizMethod::SetPilot;
  else if (strcmp(method, "syncPilot") == 0) out.method = WizMethod::SyncPilot;
  else out.method = WizMethod::Unknown;

  JsonVariantConst result = doc["result"];
  if (out.method == WizMethod::SyncPilot) result = doc["params"];
  if (!result.is<JsonObjectConst>()) {
    bool errorReply = doc["error"].is<JsonObjectConst>();
    if (errorReply) out.errorCode = static_cast<int16_t>(doc["error"]["code"] | -1);
    doc.clear();
    arena.reset();
    return !errorReply && out.method != WizMethod::Unknown;
  }

  const char* mac = result["mac"] | "";
  if (macNormalize(mac, out.mac)) out.hasMac = 1;
  out.success = result["success"] | false;

  if (out.method == WizMethod::GetPilot || out.method == WizMethod::SyncPilot) {
    Pilot p{};
    p.valid = 1;
    p.on = result["state"] | false;
    p.dim = clampDim(result["dimming"] | 100);
    int scene = result["sceneId"] | 0;
    bool hasTemp = result["temp"].is<int>() && (result["temp"].as<int>() > 0);
    bool hasRgb = result["r"].is<int>() || result["g"].is<int>() || result["b"].is<int>();
    if (scene > 0 && scene != 11 && scene != 12 && scene != 13) {
      p.mode = PILOT_SCENE;
      p.scene = static_cast<uint8_t>(scene);
    } else if (hasTemp) {
      p.mode = PILOT_TEMP;
      p.temp = clampTemp(result["temp"].as<int>());
    } else if (hasRgb) {
      p.mode = PILOT_RGB;
      p.r = clamp8(result["r"] | 0);
      p.g = clamp8(result["g"] | 0);
      p.b = clamp8(result["b"] | 0);
      p.hue = rgbToHue(p.r, p.g, p.b);
    } else if (scene > 0) {
      p.mode = PILOT_TEMP;
      p.temp = scene == 11 ? 2700 : (scene == 12 ? 4200 : 6500);
    } else {
      p.mode = PILOT_TEMP;
      p.temp = 2700;
    }
    out.pilot = p;
    out.hasPilot = 1;
  }
  doc.clear();
  arena.reset();
  return true;
}

}
