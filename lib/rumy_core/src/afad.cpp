#include "afad.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "timeutil.h"

namespace rc {

const char* const AFAD_HOST = "servisnet.afad.gov.tr";

QuakeBox marmaraBox() { return {39.5f, 41.8f, 26.0f, 30.5f}; }

QuakeBox boxAround(float lat, float lon) { return {lat - 1.5f, lat + 1.2f, lon - 1.8f, lon + 3.0f}; }

size_t afadPath(char* out, size_t cap, uint32_t nowUtc, uint32_t windowSec, const QuakeBox& box, float minMag) {
  uint32_t startUtc = nowUtc > windowSec ? nowUtc - windowSec : 0;
  uint32_t endUtc = nowUtc + 3600;
  LocalTime s = toLocal(startUtc, 0);
  LocalTime e = toLocal(endUtc, 0);
  int n = snprintf(out, cap,
                   "/apigateway/deprem/apiv2/event/filter?start=%04d-%02d-%02dT%02d:%02d:%02d"
                   "&end=%04d-%02d-%02dT%02d:%02d:%02d&minlat=%.2f&maxlat=%.2f&minlon=%.2f&maxlon=%.2f"
                   "&minmag=%.1f&orderby=timedesc&limit=500&format=json",
                   s.year, s.month, s.day, s.hour, s.minute, s.second, e.year, e.month, e.day, e.hour, e.minute,
                   e.second, static_cast<double>(box.minLat), static_cast<double>(box.maxLat),
                   static_cast<double>(box.minLon), static_cast<double>(box.maxLon), static_cast<double>(minMag));
  return (n < 0 || static_cast<size_t>(n) >= cap) ? 0 : static_cast<size_t>(n);
}

size_t utf8Copy(char* out, size_t cap, const char* in, size_t inLen) {
  if (cap == 0) return 0;
  size_t n = inLen < cap - 1 ? inLen : cap - 1;
  while (n > 0 && n < inLen && (static_cast<unsigned char>(in[n]) & 0xC0) == 0x80) n--;
  memcpy(out, in, n);
  out[n] = 0;
  return n;
}

void afadShortPlace(const char* in, char* out, size_t cap) {
  if (!in) {
    if (cap) out[0] = 0;
    return;
  }
  const char* dash = strstr(in, " - [");
  const char* close = dash ? strstr(dash, "] ") : nullptr;
  if (dash && close) {
    char tmp[96];
    size_t a = static_cast<size_t>(dash - in);
    size_t used = utf8Copy(tmp, sizeof(tmp), in, a);
    if (used + 2 < sizeof(tmp)) {
      tmp[used++] = ',';
      tmp[used++] = ' ';
      tmp[used] = 0;
      const char* rest = close + 2;
      utf8Copy(tmp + used, sizeof(tmp) - used, rest, strlen(rest));
    }
    utf8Copy(out, cap, tmp, strlen(tmp));
    return;
  }
  utf8Copy(out, cap, in, strlen(in));
}

namespace {

float numberOrString(JsonVariantConst v) {
  if (v.is<const char*>()) return static_cast<float>(atof(v.as<const char*>()));
  if (v.isNull()) return NAN;
  return v.as<float>();
}

}

bool afadParse(ByteSource& src, JsonArena& arena, uint32_t nowUtc, QuakeInfo& out) {
  memset(&out, 0, sizeof(out));
  arena.reset();
  JsonDocument filter(&arena);
  filter["date"] = true;
  filter["magnitude"] = true;
  filter["depth"] = true;
  filter["latitude"] = true;
  filter["longitude"] = true;
  filter["location"] = true;
  size_t base = arena.mark();
  JsonDocument doc(&arena);

  if (!expectChar(src, '[')) {
    filter.clear();
    arena.reset();
    return false;
  }
  bool ok = true;
  int c = skipSpaces(src);
  if (c == ']') {
    src.read();
  } else {
    for (;;) {
      doc.clear();
      arena.rewind(base);
      DeserializationError err = deserializeJson(doc, src, DeserializationOption::Filter(filter));
      if (err) {
        ok = false;
        break;
      }
      uint32_t t = 0;
      const char* ds = doc["date"] | "";
      float mag = numberOrString(doc["magnitude"]);
      if (parseIsoDateTime(ds, t) && !isnan(mag)) {
        Quake q{};
        q.timeUtc = t;
        q.mag10 = static_cast<int16_t>(lroundf(mag * 10.0f));
        float depth = numberOrString(doc["depth"]);
        q.depth10 = isnan(depth) ? NA16 : static_cast<int16_t>(lroundf(depth * 10.0f));
        float lat = numberOrString(doc["latitude"]);
        float lon = numberOrString(doc["longitude"]);
        q.lat1e4 = isnan(lat) ? 0 : static_cast<int32_t>(lroundf(lat * 10000.0f));
        q.lon1e4 = isnan(lon) ? 0 : static_cast<int32_t>(lroundf(lon * 10000.0f));
        afadShortPlace(doc["location"] | "", q.place, sizeof(q.place));
        if (out.latest.timeUtc == 0 || q.timeUtc > out.latest.timeUtc) out.latest = q;
        bool recent = nowUtc == 0 || (q.timeUtc + 86400u >= nowUtc);
        if (recent) {
          if (out.count24h < 255) out.count24h++;
          if (out.strongest.timeUtc == 0 || q.mag10 > out.strongest.mag10) out.strongest = q;
        }
      }
      c = skipSpaces(src);
      if (c == ',') {
        src.read();
        continue;
      }
      if (c == ']') {
        src.read();
        break;
      }
      ok = false;
      break;
    }
  }
  doc.clear();
  filter.clear();
  arena.reset();
  out.valid = ok ? 1 : 0;
  return ok;
}

}
