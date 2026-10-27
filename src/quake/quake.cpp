#include "quake.h"

#include "afad.h"
#include "kandilli.h"
#include "app/config.h"
#include "app/store.h"
#include "cache_fmt.h"
#include "clock/clock.h"
#include "net/cache.h"
#include "net/http_min.h"
#include "net/net.h"
#include "net/relay.h"
#include "settings/settings.h"

namespace quake {

namespace {

rc::QuakeInfo s_q;

struct KandilliCtx {
  rc::QuakeInfo* out;
  rc::QuakeBox box;
  int16_t minMag10;
};

bool parse(rc::ByteSource& src, void* ctx) {
  return rc::afadParse(src, net::arena(), clk::nowUtc(), *static_cast<rc::QuakeInfo*>(ctx));
}

bool parseKandilli(rc::ByteSource& src, void* ctx) {
  KandilliCtx* k = static_cast<KandilliCtx*>(ctx);
  return rc::kandilliParse(src, clk::nowUtc(), cfg::QUAKE_WINDOW_S, k->box, k->minMag10, *k->out);
}

bool inside(const rc::QuakeBox& b, float lat, float lon) {
  return lat >= b.minLat && lat <= b.maxLat && lon >= b.minLon && lon <= b.maxLon;
}

}

void loadCache() {
  if (cache::load(rc::BLOB_QUAKE, &s_q, sizeof(s_q)) && s_q.valid) store::putT(store::QUAKE, s_q);
}

int16_t fetch(net::FetchStat& st) {
  if (!clk::synced()) {
    st.code = http::E_BUILD;
    return st.code;
  }
  uint32_t now = clk::nowUtc();
  Settings s = settings::get();
  rc::QuakeBox box = rc::marmaraBox();
  if (!inside(box, s.lat, s.lon)) box = rc::boxAround(s.lat, s.lon);
  char path[400];
  if (!rc::afadPath(path, sizeof(path), now, cfg::QUAKE_WINDOW_S, box, s.quakeMinMag10 / 10.0f)) {
    st.code = http::E_BUILD;
    return st.code;
  }
  rc::QuakeInfo got;
  uint8_t source = rc::QS_AFAD;
  int16_t code;
  if (relay::quake(path, got)) {
    code = 200;
    st.code = code;
    st.via = net::VIA_BRIDGE;
    st.durMs = relay::lastMs();
  } else if (net::tlsAllowed()) {
    code = net::get(rc::AFAD_HOST, path, true, parse, &got, st);
  } else {
    KandilliCtx k{&got, box, static_cast<int16_t>(s.quakeMinMag10)};
    code = net::get(rc::KANDILLI_HOST, rc::KANDILLI_PATH, false, parseKandilli, &k, st);
    st.via = net::VIA_KANDILLI;
    source = rc::QS_KANDILLI;
  }
  if (code != 200) return code;
  if (!got.valid) {
    st.code = http::E_PARSE;
    return st.code;
  }
  s_q = got;
  s_q.fetchedUtc = now;
  s_q.source = source;
  store::putT(store::QUAKE, s_q);
  cache::save(rc::BLOB_QUAKE, &s_q, sizeof(s_q), now);
  return code;
}

}
