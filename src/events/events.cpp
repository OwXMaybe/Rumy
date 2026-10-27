#include "events.h"

#include <stdio.h>
#include <string.h>

#include "app/config.h"
#include "app/mem.h"
#include "app/store.h"
#include "board/sdcard.h"
#include "cache_fmt.h"
#include "clock/clock.h"
#include "events_calc.h"
#include "json_arena.h"
#include "nager.h"
#include "net/http_min.h"
#include "net/relay.h"
#include "reader.h"
#include "timeutil.h"

namespace events {

namespace {

constexpr size_t SCRATCH_TEXT = 4096;
constexpr uint16_t REL_VERSION = 1;
constexpr int32_t TENTATIVE_RETRY_DAYS = 7;

rc::PersonalEvent s_personal[rc::MAX_PERSONAL];
int s_personalN = 0;
rc::Religious s_rel[2];
bool s_final[2] = {false, false};
int32_t s_tried[2] = {-100000, -100000};
int s_relYear = 0;
int32_t s_lastDay = -1;
bool s_dirty = true;

struct NagerCtx {
  rc::Religious* out;
  int year;
};

void holidayPath(int year, char* out, size_t cap) { snprintf(out, cap, "%s/%d.bin", cfg::SD_HOLIDAY_DIR, year); }

bool loadYear(int year, rc::Religious& out) {
  uint8_t buf[sizeof(rc::BlobHeader) + sizeof(rc::Religious)];
  size_t len = 0;
  char path[48];
  holidayPath(year, path, sizeof(path));
  if (!sdcard::readFile(path, buf, sizeof(buf), len)) return false;
  if (!rc::blobUnpackInto(buf, len, rc::BLOB_RELIGIOUS, REL_VERSION, &out, sizeof(out))) return false;
  return out.valid && out.year == year;
}

bool saveYear(const rc::Religious& r) {
  uint8_t buf[sizeof(rc::BlobHeader) + sizeof(rc::Religious)];
  size_t n = rc::blobPack(rc::BLOB_RELIGIOUS, REL_VERSION, &r, sizeof(r), buf, sizeof(buf));
  if (!n) return false;
  char path[48];
  holidayPath(r.year, path, sizeof(path));
  return sdcard::writeFileAtomic(path, buf, n);
}

void ensureYears(int y) {
  if (s_relYear == y) return;
  s_relYear = y;
  for (int i = 0; i < 2; i++) {
    rc::Religious sd = {};
    bool have = loadYear(y + i, sd);
    s_final[i] = have && sd.source == rc::REL_NAGER;
    s_rel[i] = rc::religiousBest(y + i, have ? &sd : nullptr);
    s_tried[i] = -100000;
  }
  s_dirty = true;
}

void loadPersonal() {
  s_personalN = 0;
  uint8_t* scratch = mem::jsonArena();
  if (!scratch) return;
  size_t len = 0;
  if (!sdcard::readFile(cfg::SD_EVENTS_FILE, scratch, SCRATCH_TEXT - 1, len)) return;
  rc::JsonArena a(scratch + SCRATCH_TEXT, cfg::JSON_ARENA_BYTES - SCRATCH_TEXT);
  rc::MemSource src(reinterpret_cast<const char*>(scratch), len);
  int n = 0;
  if (rc::personalParse(src, a, s_personal, rc::MAX_PERSONAL, n)) s_personalN = n;
}

bool parseNager(rc::ByteSource& src, void* ctx) {
  NagerCtx* c = static_cast<NagerCtx*>(ctx);
  return rc::nagerParse(src, net::arena(), c->year, *c->out);
}

int dueIndex(int32_t today) {
  for (int i = 0; i < 2; i++) {
    if (s_final[i]) continue;
    if (today - s_tried[i] < (s_rel[i].source == rc::REL_NAGER_TENTATIVE ? TENTATIVE_RETRY_DAYS : 1)) continue;
    return i;
  }
  return -1;
}

}

void begin() {
  loadPersonal();
  s_dirty = true;
}

void tick() {
  if (!clk::synced()) return;
  int32_t today = clk::todayDayNum();
  rc::CivilDate cd = rc::civilFromDays(today);
  ensureYears(cd.year);
  if (today != s_lastDay) {
    s_lastDay = today;
    s_dirty = true;
  }
  if (!s_dirty) return;
  s_dirty = false;
  rc::EventsSnapshot snap;
  rc::buildSnapshot(s_rel, 2, s_personal, s_personalN, today, snap);
  store::putT(store::EVENTS, snap);
}

bool holidaysNeeded() {
  if (!clk::synced() || !sdcard::mounted() || s_relYear == 0) return false;
  return dueIndex(clk::todayDayNum()) >= 0;
}

int16_t fetchHolidays(net::FetchStat& st) {
  if (!holidaysNeeded()) {
    st.code = 0;
    return 0;
  }
  int32_t today = clk::todayDayNum();
  int i = dueIndex(today);
  s_tried[i] = today;
  int year = s_relYear + i;
  char path[48];
  if (!rc::nagerPath(path, sizeof(path), year)) {
    st.code = http::E_BUILD;
    return st.code;
  }
  rc::Religious got = {};
  int16_t code;
  if (relay::holidays(year, got)) {
    code = 200;
    st.code = code;
    st.via = net::VIA_BRIDGE;
    st.durMs = relay::lastMs();
  } else {
    NagerCtx ctx{&got, year};
    code = net::get(rc::NAGER_HOST, path, true, parseNager, &ctx, st);
  }
  if (code != 200) return code;
  if (!got.valid) {
    st.code = http::E_PARSE;
    return st.code;
  }
  saveYear(got);
  s_final[i] = got.source == rc::REL_NAGER;
  s_rel[i] = rc::religiousBest(year, &got);
  s_dirty = true;
  return code;
}

size_t personalJson(char* out, size_t cap) { return rc::personalSerialize(s_personal, s_personalN, out, cap); }

bool setPersonal(const char* json, size_t len) {
  rc::PersonalEvent tmp[rc::MAX_PERSONAL];
  int n = 0;
  uint8_t* scratch = mem::jsonArena();
  if (!scratch || !sdcard::mounted()) return false;
  {
    rc::JsonArena a(scratch + SCRATCH_TEXT, cfg::JSON_ARENA_BYTES - SCRATCH_TEXT);
    rc::MemSource src(json, len);
    if (!rc::personalParse(src, a, tmp, rc::MAX_PERSONAL, n)) return false;
  }
  char* out = reinterpret_cast<char*>(scratch + SCRATCH_TEXT);
  size_t w = rc::personalSerialize(tmp, n, out, cfg::JSON_ARENA_BYTES - SCRATCH_TEXT);
  if (!w && n > 0) return false;
  if (!sdcard::writeFileAtomic(cfg::SD_EVENTS_FILE, reinterpret_cast<const uint8_t*>(out), w)) return false;
  memcpy(s_personal, tmp, sizeof(tmp[0]) * static_cast<size_t>(n));
  s_personalN = n;
  s_dirty = true;
  return true;
}

int personalCount() { return s_personalN; }

}
