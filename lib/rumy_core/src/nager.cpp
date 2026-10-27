#include "nager.h"

#include <stdio.h>
#include <string.h>

#include "timeutil.h"

namespace rc {

const char* const NAGER_HOST = "date.nager.at";

size_t nagerPath(char* out, size_t cap, int year) {
  int n = snprintf(out, cap, "/api/v3/PublicHolidays/%d/TR", year);
  return (n < 0 || static_cast<size_t>(n) >= cap) ? 0 : static_cast<size_t>(n);
}

namespace {

struct TableRow {
  uint16_t year;
  uint8_t rm;
  uint8_t rd;
  uint8_t km;
  uint8_t kd;
};

constexpr TableRow kDiyanet[] = {
    {2026, 3, 20, 5, 27}, {2027, 3, 9, 5, 16}, {2028, 2, 26, 5, 5},
    {2029, 2, 14, 4, 24}, {2030, 2, 4, 4, 13},
};

bool startsWith(const char* s, const char* prefix) { return strncmp(s, prefix, strlen(prefix)) == 0; }

bool earlier(int m1, int d1, int m2, int d2) { return m1 < m2 || (m1 == m2 && d1 < d2); }

}

bool religiousTable(int year, Religious& out) {
  for (const TableRow& r : kDiyanet) {
    if (r.year == year) {
      out.year = r.year;
      out.ramazanMonth = r.rm;
      out.ramazanDay = r.rd;
      out.kurbanMonth = r.km;
      out.kurbanDay = r.kd;
      out.valid = 1;
      out.source = REL_TABLE;
      return true;
    }
  }
  return false;
}

bool nagerParse(ByteSource& src, JsonArena& arena, int year, Religious& out) {
  memset(&out, 0, sizeof(out));
  out.year = static_cast<uint16_t>(year);
  arena.reset();
  JsonDocument filter(&arena);
  filter["date"] = true;
  filter["localName"] = true;
  filter["name"] = true;
  size_t base = arena.mark();
  JsonDocument doc(&arena);
  if (!expectChar(src, '[')) {
    filter.clear();
    arena.reset();
    return false;
  }
  bool ok = true;
  bool tentative = false;
  int c = skipSpaces(src);
  if (c == ']') {
    src.read();
  } else {
    for (;;) {
      doc.clear();
      arena.rewind(base);
      if (deserializeJson(doc, src, DeserializationOption::Filter(filter))) {
        ok = false;
        break;
      }
      const char* local = doc["localName"] | "";
      const char* name = doc["name"] | "";
      const char* date = doc["date"] | "";
      int y, m, d;
      if (parseIsoDate(date, y, m, d) && y == year) {
        bool ramazan = startsWith(local, "Ramazan Bayram") || startsWith(name, "Eid al-Fitr");
        bool kurban = startsWith(local, "Kurban Bayram") || startsWith(name, "Eid al-Adha");
        if (strstr(local, "Tentative") || strstr(name, "Tentative")) {
          if (ramazan || kurban) tentative = true;
        }
        if (ramazan && (out.ramazanMonth == 0 || earlier(m, d, out.ramazanMonth, out.ramazanDay))) {
          out.ramazanMonth = static_cast<uint8_t>(m);
          out.ramazanDay = static_cast<uint8_t>(d);
        }
        if (kurban && (out.kurbanMonth == 0 || earlier(m, d, out.kurbanMonth, out.kurbanDay))) {
          out.kurbanMonth = static_cast<uint8_t>(m);
          out.kurbanDay = static_cast<uint8_t>(d);
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
  if (!ok) return false;
  out.valid = (out.ramazanMonth != 0 && out.kurbanMonth != 0) ? 1 : 0;
  out.source = tentative ? REL_NAGER_TENTATIVE : REL_NAGER;
  return out.valid != 0;
}

Religious religiousBest(int year, const Religious* fromNager) {
  Religious table{};
  bool hasTable = religiousTable(year, table);
  if (fromNager && fromNager->valid && fromNager->year == year) {
    if (fromNager->source == REL_NAGER_TENTATIVE && hasTable) return table;
    return *fromNager;
  }
  if (hasTable) return table;
  Religious none{};
  none.year = static_cast<uint16_t>(year);
  return none;
}

}
