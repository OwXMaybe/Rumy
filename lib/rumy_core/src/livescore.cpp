#include "livescore.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json_stream.h"

namespace rc {

using namespace js;

const char* const LS_HOST = "prod-public-api.livescore.com";

namespace {

constexpr int LS_IT_GOAL = 36;
constexpr int LS_IT_PEN = 37;
constexpr int LS_IT_OWN = 39;
constexpr int LS_TOKENS = 8;

struct Fold2 {
  uint8_t lead;
  uint8_t lo;
  uint8_t hi;
  char to;
};

const Fold2 kFold[] = {
    {0xC3, 0x80, 0x85, 'a'}, {0xC3, 0x87, 0x87, 'c'}, {0xC3, 0x88, 0x8B, 'e'}, {0xC3, 0x8C, 0x8F, 'i'},
    {0xC3, 0x91, 0x91, 'n'}, {0xC3, 0x92, 0x96, 'o'}, {0xC3, 0x98, 0x98, 'o'}, {0xC3, 0x99, 0x9C, 'u'},
    {0xC3, 0x9D, 0x9D, 'y'}, {0xC3, 0x9F, 0x9F, 's'}, {0xC3, 0xA0, 0xA5, 'a'}, {0xC3, 0xA7, 0xA7, 'c'},
    {0xC3, 0xA8, 0xAB, 'e'}, {0xC3, 0xAC, 0xAF, 'i'}, {0xC3, 0xB1, 0xB1, 'n'}, {0xC3, 0xB2, 0xB6, 'o'},
    {0xC3, 0xB8, 0xB8, 'o'}, {0xC3, 0xB9, 0xBC, 'u'}, {0xC3, 0xBD, 0xBF, 'y'}, {0xC4, 0x80, 0x85, 'a'},
    {0xC4, 0x86, 0x8D, 'c'}, {0xC4, 0x8E, 0x91, 'd'}, {0xC4, 0x92, 0x9B, 'e'}, {0xC4, 0x9C, 0xA3, 'g'},
    {0xC4, 0xA8, 0xB1, 'i'}, {0xC4, 0xB9, 0xBF, 'l'}, {0xC5, 0x80, 0x82, 'l'}, {0xC5, 0x83, 0x88, 'n'},
    {0xC5, 0x8C, 0x91, 'o'}, {0xC5, 0x94, 0x99, 'r'}, {0xC5, 0x9A, 0xA1, 's'}, {0xC5, 0xA2, 0xA5, 't'},
    {0xC5, 0xA8, 0xB3, 'u'}, {0xC5, 0xB9, 0xBE, 'z'},
};

struct Tokens {
  char text[LS_TEAM * 2];
  const char* tok[LS_TOKENS];
  int n;
};

void tokens(const char* name, Tokens& t) {
  lsFold(name, t.text, sizeof(t.text));
  t.n = 0;
  char* p = t.text;
  while (*p && t.n < LS_TOKENS) {
    while (*p == ' ') p++;
    if (!*p) break;
    t.tok[t.n++] = p;
    while (*p && *p != ' ') p++;
    if (*p) *p++ = 0;
  }
}

bool prefix(const char* a, const char* b) { return !strncmp(a, b, strlen(a)); }

bool common(const char* tok) {
  static const char* const kCommon[] = {"real",     "club",      "united", "city",   "sporting", "athletic", "atletico",
                                        "deportivo", "union",    "olympique", "dynamo", "dinamo", "sport",  "football",
                                        "calcio",   "borussia", "racing", "saint",  "stade",    "inter",    "spor"};
  for (const char* c : kCommon) {
    if (!strcmp(tok, c)) return true;
  }
  return false;
}

bool subset(const Tokens& small, const Tokens& big) {
  if (!small.n) return false;
  for (int i = 0; i < small.n; i++) {
    bool hit = false;
    for (int k = 0; k < big.n && !hit; k++) hit = prefix(small.tok[i], big.tok[k]);
    if (!hit) return false;
  }
  return true;
}

void joined(const Tokens& t, char* out, size_t cap) {
  size_t k = 0;
  for (int i = 0; i < t.n; i++) {
    for (const char* p = t.tok[i]; *p && k + 1 < cap; p++) out[k++] = *p;
  }
  out[k] = 0;
}

uint8_t kindOf(int it) {
  if (it == LS_IT_PEN) return MK_PEN_GOAL;
  if (it == LS_IT_OWN) return MK_OWN_GOAL;
  return MK_GOAL;
}

void addGoal(LsMatch& out, JsonObjectConst x) {
  int it = x["IT"] | 0;
  if (it != LS_IT_GOAL && it != LS_IT_PEN && it != LS_IT_OWN) return;
  JsonArrayConst sc = x["Sc"];
  uint8_t h = static_cast<uint8_t>(sc[0] | 0);
  uint8_t a = static_cast<uint8_t>(sc[1] | 0);
  for (int i = 0; i < out.n; i++) {
    if (out.goals[i].home == h && out.goals[i].away == a) return;
  }
  if (out.n >= LS_GOALS) return;
  LsGoal& g = out.goals[out.n++];
  memset(&g, 0, sizeof(g));
  g.home = h;
  g.away = a;
  g.homeSide = (x["Nm"] | 0) == 1 ? 1 : 0;
  g.kind = kindOf(it);
  int min = x["Min"] | 0;
  int ex = x["MinEx"] | 0;
  if (ex) snprintf(g.minute, sizeof(g.minute), "%d+%d'", min, ex);
  else snprintf(g.minute, sizeof(g.minute), "%d'", min);
  copyText(g.player, sizeof(g.player), x["Pn"] | "");
}

void itemFilter(JsonObject f) {
  f["Min"] = true;
  f["MinEx"] = true;
  f["Nm"] = true;
  f["IT"] = true;
  f["Pn"] = true;
  f["Sc"] = true;
}

}

void lsFold(const char* in, char* out, size_t cap) {
  if (!cap) return;
  size_t k = 0;
  bool space = true;
  const uint8_t* p = reinterpret_cast<const uint8_t*>(in ? in : "");
  while (*p && k + 1 < cap) {
    char c = 0;
    if (*p < 0x80) {
      c = static_cast<char>(*p);
      if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
      if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) c = ' ';
      p++;
    } else if (p[1]) {
      for (const Fold2& f : kFold) {
        if (p[0] == f.lead && p[1] >= f.lo && p[1] <= f.hi) c = f.to;
      }
      p += (*p >= 0xF0) ? 4 : (*p >= 0xE0) ? 3 : 2;
      if (!c) continue;
    } else {
      break;
    }
    if (c == ' ') {
      if (!space) out[k++] = ' ';
      space = true;
      continue;
    }
    out[k++] = c;
    space = false;
  }
  while (k && out[k - 1] == ' ') k--;
  out[k] = 0;
}

uint8_t lsTeamMatch(const char* a, const char* b) {
  Tokens ta;
  Tokens tb;
  tokens(a, ta);
  tokens(b, tb);
  if (!ta.n || !tb.n) return 0;
  char ja[LS_TEAM * 2];
  char jb[LS_TEAM * 2];
  joined(ta, ja, sizeof(ja));
  joined(tb, jb, sizeof(jb));
  if (!strcmp(ja, jb)) return 3;
  size_t la = strlen(ja);
  size_t lb = strlen(jb);
  if ((la >= 5 && lb >= 5) && (prefix(ja, jb) || prefix(jb, ja))) return 2;
  if (ta.n <= tb.n ? subset(ta, tb) : subset(tb, ta)) return 2;
  for (int i = 0; i < ta.n; i++) {
    for (int k = 0; k < tb.n; k++) {
      if (strlen(ta.tok[i]) >= 4 && !strcmp(ta.tok[i], tb.tok[k]) && !common(ta.tok[i])) return 1;
    }
  }
  return 0;
}

size_t lsIncidentsPath(char* out, size_t cap, uint32_t eid) {
  return fin(snprintf(out, cap, "/v1/api/app/incidents/soccer/%lu?locale=en", static_cast<unsigned long>(eid)), cap);
}

size_t lsLivePath(char* out, size_t cap) { return fin(snprintf(out, cap, "/v1/api/app/live/soccer/3?MD=1"), cap); }

bool lsParseIncidents(ByteSource& src, JsonArena& arena, LsMatch& out) {
  memset(&out, 0, sizeof(out));
  arena.reset();
  JsonDocument filter(&arena);
  filter["Eid"] = true;
  filter["Tr1"] = true;
  filter["Tr2"] = true;
  static const char* const kPeriods[] = {"1", "2", "3", "4", "5"};
  for (const char* key : kPeriods) {
    JsonObject item = filter["Incs"][key].add<JsonObject>();
    itemFilter(item);
    itemFilter(item["Incs"].add<JsonObject>());
  }
  JsonDocument doc(&arena);
  bool ok = !deserializeJson(doc, src, DeserializationOption::Filter(filter),
                             DeserializationOption::NestingLimit(ESPN_NESTING));
  if (ok) {
    out.eid = idOf(doc["Eid"]);
    out.home = static_cast<uint8_t>(atoi(doc["Tr1"] | "0"));
    out.away = static_cast<uint8_t>(atoi(doc["Tr2"] | "0"));
    for (const char* key : kPeriods) {
      for (JsonObjectConst x : doc["Incs"][key].as<JsonArrayConst>()) {
        JsonArrayConst group = x["Incs"];
        if (group.size()) {
          for (JsonObjectConst y : group) addGoal(out, y);
        } else {
          addGoal(out, x);
        }
      }
    }
    out.valid = out.eid ? 1 : 0;
  }
  doc.clear();
  filter.clear();
  arena.reset();
  return ok && out.valid;
}

bool lsFindLive(ByteSource& src, JsonArena& arena, const char* home, const char* away, LsEvent& out) {
  memset(&out, 0, sizeof(out));
  arena.reset();
  JsonDocument filter(&arena);
  JsonObject ev = filter["Events"].add<JsonObject>();
  ev["Eid"] = true;
  ev["Tr1"] = true;
  ev["Tr2"] = true;
  ev["T1"].add<JsonObject>()["Nm"] = true;
  ev["T2"].add<JsonObject>()["Nm"] = true;
  size_t base = arena.mark();
  JsonDocument doc(&arena);
  TopKeys keys(src);
  char key[24];
  bool ok = false;
  while (keys.next(key, sizeof(key))) {
    if (strcmp(key, "Stages")) continue;
    ok = eachElement(src, arena, doc, filter, base, [&](JsonObjectConst stage) {
      for (JsonObjectConst e : stage["Events"].as<JsonArrayConst>()) {
        const char* t1 = e["T1"][0]["Nm"] | "";
        const char* t2 = e["T2"][0]["Nm"] | "";
        uint8_t h = lsTeamMatch(home, t1);
        uint8_t a = lsTeamMatch(away, t2);
        uint8_t hs = lsTeamMatch(home, t2);
        uint8_t as = lsTeamMatch(away, t1);
        bool swapped = (hs && as && hs + as > h + a) || !(h && a);
        uint8_t q1 = swapped ? hs : h;
        uint8_t q2 = swapped ? as : a;
        if (!q1 || !q2) continue;
        uint8_t q = static_cast<uint8_t>(q1 + q2);
        if (q <= out.quality) continue;
        out.eid = idOf(e["Eid"]);
        out.quality = q;
        out.swapped = swapped ? 1 : 0;
        uint8_t s1 = static_cast<uint8_t>(atoi(e["Tr1"] | "0"));
        uint8_t s2 = static_cast<uint8_t>(atoi(e["Tr2"] | "0"));
        out.home = swapped ? s2 : s1;
        out.away = swapped ? s1 : s2;
      }
    });
    break;
  }
  doc.clear();
  filter.clear();
  arena.reset();
  return ok && out.eid;
}

}
