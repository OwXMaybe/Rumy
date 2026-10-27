#include "kandilli.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nager.h"
#include "timeutil.h"

namespace rc {

const char* const KANDILLI_HOST = "www.koeri.boun.edu.tr";
const char* const KANDILLI_PATH = "/scripts/lst0.asp";

namespace {

constexpr size_t LINE_CAP = 160;

const char* cp1254(unsigned char c, bool upper) {
  switch (c) {
    case 0xC7:
    case 0xE7: return upper ? "\xC3\x87" : "\xC3\xA7";
    case 0xD0:
    case 0xF0: return upper ? "\xC4\x9E" : "\xC4\x9F";
    case 0xDD: return upper ? "\xC4\xB0" : "i";
    case 0xFD: return upper ? "I" : "\xC4\xB1";
    case 0xD6:
    case 0xF6: return upper ? "\xC3\x96" : "\xC3\xB6";
    case 0xDE:
    case 0xFE: return upper ? "\xC5\x9E" : "\xC5\x9F";
    case 0xDC:
    case 0xFC: return upper ? "\xC3\x9C" : "\xC3\xBC";
    default: return nullptr;
  }
}

bool magField(const char* s, int16_t& mag10) {
  if (!s[0] || s[0] == '-') return false;
  char* end = nullptr;
  double v = strtod(s, &end);
  if (end == s || v <= 0.0 || v > 10.0) return false;
  mag10 = static_cast<int16_t>(lround(v * 10.0));
  return true;
}

bool inside(const QuakeBox& b, int32_t lat1e4, int32_t lon1e4) {
  float lat = lat1e4 / 10000.0f;
  float lon = lon1e4 / 10000.0f;
  return lat >= b.minLat && lat <= b.maxLat && lon >= b.minLon && lon <= b.maxLon;
}

bool parseQuake(const char*& p, Quake& q) {
  char* end = nullptr;
  unsigned long t = strtoul(p, &end, 10);
  if (end == p) return false;
  p = end;
  long m = strtol(p, &end, 10);
  if (end == p) return false;
  p = end;
  long d = strtol(p, &end, 10);
  if (end == p) return false;
  p = end;
  long la = strtol(p, &end, 10);
  if (end == p) return false;
  p = end;
  long lo = strtol(p, &end, 10);
  if (end == p) return false;
  p = end;
  if (m < -50 || m > 100 || la < -900000 || la > 900000 || lo < -1800000 || lo > 1800000) return false;
  if (d < -32768 || d > 32767) return false;
  memset(&q, 0, sizeof(q));
  q.timeUtc = static_cast<uint32_t>(t);
  q.mag10 = static_cast<int16_t>(m);
  q.depth10 = static_cast<int16_t>(d);
  q.lat1e4 = static_cast<int32_t>(la);
  q.lon1e4 = static_cast<int32_t>(lo);
  return true;
}

}

void kandilliPlace(const char* in, size_t inLen, char* out, size_t cap) {
  if (!cap) return;
  const char* paren = static_cast<const char*>(memchr(in, '(', inLen));
  size_t head = paren ? static_cast<size_t>(paren - in) : inLen;
  const char* dash = static_cast<const char*>(memchr(in, '-', head));
  size_t from = dash ? static_cast<size_t>(dash - in) + 1 : 0;
  size_t k = 0;
  bool wordStart = true;
  for (size_t i = from; i < inLen && in[i]; i++) {
    unsigned char c = static_cast<unsigned char>(in[i]);
    const char* tr = c >= 0x80 ? cp1254(c, wordStart) : nullptr;
    if (c >= 0x80 && !tr) continue;
    if (tr) {
      size_t n = strlen(tr);
      if (k + n + 1 > cap) break;
      memcpy(out + k, tr, n);
      k += n;
      wordStart = false;
      continue;
    }
    char ch = static_cast<char>(c);
    if (ch >= 'A' && ch <= 'Z' && !wordStart) ch = static_cast<char>(ch - 'A' + 'a');
    if (ch >= 'a' && ch <= 'z' && wordStart) ch = static_cast<char>(ch - 'a' + 'A');
    if (k + 2 > cap) break;
    out[k++] = ch;
    wordStart = ch == ' ' || ch == '(' || ch == '-' || ch == '/';
  }
  while (k > 0 && out[k - 1] == ' ') k--;
  out[k] = 0;
}

bool kandilliLine(const char* line, Quake& q) {
  int y, mo, d, h, mi, s, used = 0;
  float lat, lon, depth;
  char md[8], ml[8], mw[8];
  if (sscanf(line, "%4d.%2d.%2d %2d:%2d:%2d %f %f %f %7s %7s %7s %n", &y, &mo, &d, &h, &mi, &s, &lat, &lon, &depth,
             md, ml, mw, &used) < 12 ||
      used <= 0) {
    return false;
  }
  if (y < 2000 || mo < 1 || mo > 12 || d < 1 || d > 31 || h > 23 || mi > 59 || s > 60) return false;
  int16_t mag10 = 0;
  if (!magField(mw, mag10) && !magField(ml, mag10) && !magField(md, mag10)) return false;
  memset(&q, 0, sizeof(q));
  uint32_t local = utcFromCivil(y, mo, d, h, mi, s);
  q.timeUtc = local - static_cast<uint32_t>(TZ_OFFSET_SEC);
  q.mag10 = mag10;
  q.depth10 = static_cast<int16_t>(lroundf(depth * 10.0f));
  q.lat1e4 = static_cast<int32_t>(lroundf(lat * 10000.0f));
  q.lon1e4 = static_cast<int32_t>(lroundf(lon * 10000.0f));
  const char* p = line + used;
  const char* stop = strstr(p, "  ");
  size_t n = stop ? static_cast<size_t>(stop - p) : strlen(p);
  kandilliPlace(p, n, q.place, sizeof(q.place));
  return true;
}

bool kandilliParse(ByteSource& src, uint32_t nowUtc, uint32_t windowSec, const QuakeBox& box, int16_t minMag10,
                   QuakeInfo& out) {
  memset(&out, 0, sizeof(out));
  char line[LINE_CAP];
  size_t n = 0;
  bool seen = false;
  bool done = false;
  for (;;) {
    int c = src.read();
    if (c < 0 || c == '\n') {
      line[n] = 0;
      Quake q;
      if (kandilliLine(line, q)) {
        seen = true;
        if (nowUtc && q.timeUtc + windowSec < nowUtc) done = true;
        else if (q.mag10 >= minMag10 && inside(box, q.lat1e4, q.lon1e4)) {
          if (out.latest.timeUtc == 0 || q.timeUtc > out.latest.timeUtc) out.latest = q;
          if (nowUtc == 0 || q.timeUtc + 86400u >= nowUtc) {
            if (out.count24h < 255) out.count24h++;
            if (out.strongest.timeUtc == 0 || q.mag10 > out.strongest.mag10) out.strongest = q;
          }
        }
      } else if (seen && strstr(line, "</pre>")) {
        done = true;
      }
      n = 0;
      if (c < 0 || done) break;
      continue;
    }
    if (c == '\r') continue;
    if (n + 1 < sizeof(line)) line[n++] = static_cast<char>(c);
  }
  out.valid = seen ? 1 : 0;
  return seen;
}

bool relayQuakeLine(const char* text, QuakeInfo& out) {
  memset(&out, 0, sizeof(out));
  if (!text) return false;
  char* end = nullptr;
  unsigned long count = strtoul(text, &end, 10);
  if (end == text || count > 255) return false;
  const char* p = end;
  Quake a, b;
  if (!parseQuake(p, a) || !parseQuake(p, b)) return false;
  const char* tab1 = strchr(p, '\t');
  if (!tab1) return false;
  const char* tab2 = strchr(tab1 + 1, '\t');
  size_t n1 = tab2 ? static_cast<size_t>(tab2 - tab1 - 1) : strlen(tab1 + 1);
  utf8Copy(a.place, sizeof(a.place), tab1 + 1, n1);
  if (tab2) utf8Copy(b.place, sizeof(b.place), tab2 + 1, strlen(tab2 + 1));
  if (a.timeUtc) out.latest = a;
  if (b.timeUtc) out.strongest = b;
  out.count24h = static_cast<uint8_t>(count);
  out.valid = 1;
  return true;
}

bool relayHolidayLine(const char* text, int year, Religious& out) {
  memset(&out, 0, sizeof(out));
  if (!text) return false;
  int y, rm, rd, km, kd, tent;
  if (sscanf(text, "%d %d %d %d %d %d", &y, &rm, &rd, &km, &kd, &tent) != 6) return false;
  if (y != year || rm < 1 || rm > 12 || rd < 1 || rd > 31 || km < 1 || km > 12 || kd < 1 || kd > 31) return false;
  out.year = static_cast<uint16_t>(y);
  out.ramazanMonth = static_cast<uint8_t>(rm);
  out.ramazanDay = static_cast<uint8_t>(rd);
  out.kurbanMonth = static_cast<uint8_t>(km);
  out.kurbanDay = static_cast<uint8_t>(kd);
  out.valid = 1;
  out.source = tent ? REL_NAGER_TENTATIVE : REL_NAGER;
  return true;
}

}
