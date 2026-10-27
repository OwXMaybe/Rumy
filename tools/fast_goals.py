"""Hizli gol bildirimi (kopru tarafi): LiveScore'u izleyip golu Rumy'ye aninda iletir.

ESPN golu 50-100 sn gec yayinliyor (13-17 Ekim olcumu, golun gercek saati ESPN'in "wallclock" alanindan); LiveScore'un
uygulamasinin kullandigi acik veri ~20 sn'de veriyor. Bu modul ESPN'den bugunun Super Lig ve Avrupa kupasi maclarini ve
Rumy'nin bildirdigi takimin maclarini alir ("@futbol <0|1> <ESPN no>"; mac takibi kapaliyken hicbir sey izlenmez, takim
secili degilse yalnizca liglere bakilir), mac saatinde LiveScore'un canli listesine 6 sn'de bir bakar, maclari
takim adlarindan eslestirir (aksansiz, kelime onekleri), skor artinca macin olay listesinden golcuyu ve dakikayi alip
Rumy'ye yollar. Skor duserse (VAR) yeni skoru yollar. 30 sn'de bir canli maclarin skoru tekrar yollanir; Rumy bundan
koprunun calistigini anlar ve kendi LiveScore sorgusunu durdurur.

Rumy'ye giden satirlar (ESPN mac numarasiyla):
    @gol <espn-no> <ev> <dep> <h|a: golu atan taraf> <g|p|o: gol, penalti, kendi kalesine> <dakika> <oyuncu>
    @skor <espn-no> <ev> <dep>

LiveScore olay kodlari (17 Ekim'de ESPN kayitlariyla karsilastirildi): 36 gol, 37 penalti golu, 39 kendi kalesine (golu
kazanan taraf yazili), 38 iptal edilen gol, 62 kacan penalti, 43 sari, 44 ikinci sari, 45 kirmizi, 63 asist.

Kullanim:
    kopru (tools/spotify_bridge.py) kendiliginden baslatir
    python tools/fast_goals.py --deneme [--takim <espn-no>] [lig ...]
                                                   Rumy'siz deneme: takimi ve ek ligleri (or. esp.1 ita.1) izler,
                                                   satirlari yazar
"""
import json
import queue
import sys
import threading
import time
import unicodedata
import urllib.request
from datetime import datetime, timedelta, timezone

ESPN_BOARD = "http://site.api.espn.com/apis/site/v2/sports/soccer/%s/scoreboard?dates=%s"
ESPN_TEAM = "http://site.api.espn.com/apis/site/v2/sports/soccer/all/teams/%d/schedule?fixture=true"
LS_LIVE = "https://prod-public-api.livescore.com/v1/api/app/live/soccer/3?MD=1"
LS_INCIDENTS = "https://prod-public-api.livescore.com/v1/api/app/incidents/soccer/%s?locale=en"
LEAGUES = ("tur.1", "uefa.champions", "uefa.europa", "uefa.europa.conf")
UA = {"User-Agent": "Rumy/1.2 (kisisel masa saati)"}
TR = timezone(timedelta(hours=3))
ESPN_EVERY_S = 60
TEAM_EVERY_S = 3600
LIVE_EVERY_S = 5
IDLE_S = 30
KEEPALIVE_S = 30
NAME_WAIT_S = 90
LEAD_S = 600
AFTER_S = 3 * 3600
IT_KIND = {36: "g", 37: "p", 39: "o"}
COMMON = {"real", "club", "united", "city", "sporting", "athletic", "atletico", "deportivo", "union", "olympique",
          "dynamo", "dinamo", "sport", "football", "calcio", "borussia", "racing", "saint", "stade", "inter", "spor"}

OUT = queue.Queue(maxsize=64)
STATE = {"on": True, "team": 0, "ver": 0}


def set_team(on, team):
    on, team = bool(on), int(team) if on else 0
    if STATE["on"] == on and STATE["team"] == team:
        return
    STATE.update(on=on, team=team, ver=STATE["ver"] + 1)
    print("hizli gol: %s" % (("takim %d" % team if team else "takim yok, yalnizca ligler") if on else "mac takibi kapali"))


def get(url):
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=15) as r:
        return json.loads(r.read().decode("utf-8"))


def fold(text):
    text = (text or "").replace("ı", "i").replace("İ", "I")
    text = unicodedata.normalize("NFKD", text)
    text = "".join(c for c in text if not unicodedata.combining(c)).lower()
    return " ".join("".join(c if c.isalnum() and c.isascii() else " " for c in text).split())


def team_match(a, b):
    ta = fold(a).split()
    tb = fold(b).split()
    if not ta or not tb:
        return 0
    ja = "".join(ta)
    jb = "".join(tb)
    if ja == jb:
        return 3
    if len(ja) >= 5 and len(jb) >= 5 and (ja.startswith(jb) or jb.startswith(ja)):
        return 2
    small, big = (ta, tb) if len(ta) <= len(tb) else (tb, ta)
    if all(any(t.startswith(s) for t in big) for s in small):
        return 2
    if any(len(t) >= 4 and t in tb and t not in COMMON for t in ta):
        return 1
    return 0


def iso(text):
    return datetime.strptime(text[:16], "%Y-%m-%dT%H:%M").replace(tzinfo=timezone.utc).timestamp()


class FastGoals:
    def __init__(self, leagues=LEAGUES, out=OUT, echo=False):
        self.leagues = leagues
        self.out = out
        self.echo = echo
        self.games = {}
        self.team_games = {}
        self.links = {}
        self.score = {}
        self.sent = set()
        self.beat = {}
        self.nameless = {}
        self.next_espn = 0.0
        self.next_team = 0.0
        self.ver = -1

    def emit(self, line):
        if self.echo:
            print(time.strftime("%H:%M:%S"), line)
        try:
            self.out.put_nowait(line)
        except queue.Full:
            pass

    def add(self, e):
        c = e["competitions"][0]
        side = {x["homeAway"]: x["team"] for x in c["competitors"]}
        if "home" not in side or "away" not in side:
            return
        state = e.get("status", c.get("status", {})).get("type", {}).get("state", "pre")
        self.games[e["id"]] = {"home": side["home"]["displayName"], "away": side["away"]["displayName"],
                               "kick": iso(e["date"]), "state": state}

    def refresh(self, now):
        today = datetime.now(TR).strftime("%Y%m%d")
        found = {}
        for lg in self.leagues:
            try:
                j = get(ESPN_BOARD % (lg, today))
            except Exception as ex:
                print("hizli gol: ESPN %s alinamadi: %s" % (lg, ex))
                continue
            for e in j.get("events", []):
                found[e["id"]] = e
        team = STATE["team"]
        if not team:
            self.team_games = {}
        elif now >= self.next_team:
            self.next_team = now + TEAM_EVERY_S
            try:
                j = get(ESPN_TEAM % team)
                self.team_games = {e["id"]: e for e in j.get("events", [])
                                   if abs(iso(e["date"]) - now) < 24 * 3600}
            except Exception as ex:
                print("hizli gol: takim fiksturu alinamadi: %s" % ex)
        found.update(self.team_games)
        self.games = {}
        for e in found.values():
            try:
                self.add(e)
            except (KeyError, IndexError, ValueError):
                continue

    def watched(self, now):
        return {k: g for k, g in self.games.items()
                if g["state"] == "in" or (g["state"] == "pre" and g["kick"] - LEAD_S <= now <= g["kick"] + AFTER_S)}

    def link(self, gid, game, events):
        best = None
        for e in events:
            t1 = e.get("T1", [{}])[0].get("Nm", "")
            t2 = e.get("T2", [{}])[0].get("Nm", "")
            h, a = team_match(game["home"], t1), team_match(game["away"], t2)
            hs, as_ = team_match(game["home"], t2), team_match(game["away"], t1)
            swapped = hs and as_ and hs + as_ > h + a
            q = (hs + as_) if swapped else (h + a)
            if (hs and as_) if swapped else (h and a):
                if best is None or q > best[0]:
                    best = (q, e["Eid"], bool(swapped))
        if best:
            self.links[gid] = (best[1], best[2])
            print("hizli gol: %s - %s -> LiveScore %s%s" % (game["home"], game["away"], best[1],
                                                            " (ters)" if best[2] else ""))

    def goals(self, eid, swapped):
        j = get(LS_INCIDENTS % eid)
        rows = []
        for per in sorted((j.get("Incs") or {}).keys()):
            for x in j["Incs"][per]:
                for y in (x.get("Incs") or [x]):
                    kind = IT_KIND.get(y.get("IT"))
                    sc = y.get("Sc") or []
                    if not kind or len(sc) != 2:
                        continue
                    h, a = (sc[1], sc[0]) if swapped else (sc[0], sc[1])
                    home_side = (y.get("Nm") == 1) != swapped
                    minute = "%d'" % y.get("Min", 0) if not y.get("MinEx") else "%d+%d'" % (y.get("Min", 0), y["MinEx"])
                    rows.append((h, a, "h" if home_side else "a", kind, minute, (y.get("Pn") or "").strip()))
        return rows

    def scan(self, now):
        watch = self.watched(now)
        if not watch:
            return False
        live = get(LS_LIVE)
        events = [e for st in live.get("Stages", []) for e in st.get("Events", [])]
        by_id = {e.get("Eid"): e for e in events}
        for gid, game in watch.items():
            if gid not in self.links or self.links[gid][0] not in by_id:
                self.link(gid, game, events)
            if gid not in self.links or self.links[gid][0] not in by_id:
                continue
            eid, swapped = self.links[gid]
            e = by_id[eid]
            s1, s2 = int(e.get("Tr1") or 0), int(e.get("Tr2") or 0)
            cur = (s2, s1) if swapped else (s1, s2)
            prev = self.score.get(gid)
            self.score[gid] = cur
            if prev is None:
                self.emit("@skor %s %d %d" % (gid, cur[0], cur[1]))
                self.beat[gid] = time.time()
                for g in self.goals(eid, swapped):
                    self.sent.add((gid, g[0], g[1]))
                continue
            if cur == prev:
                if time.time() - self.beat.get(gid, 0) >= KEEPALIVE_S:
                    self.emit("@skor %s %d %d" % (gid, cur[0], cur[1]))
                    self.beat[gid] = time.time()
                continue
            if sum(cur) > sum(prev):
                rows = []
                try:
                    rows = self.goals(eid, swapped)
                except Exception as ex:
                    print("hizli gol: olay listesi alinamadi: %s" % ex)
                news = [g for g in rows if (gid, g[0], g[1]) not in self.sent and g[0] + g[1] > sum(prev)]
                if not any((g[0], g[1]) == cur for g in news):
                    side = "h" if cur[0] > prev[0] else "a"
                    news.append((cur[0], cur[1], side, "g", e.get("Eps") or "", ""))
                for g in news:
                    self.sent.add((gid, g[0], g[1]))
                    self.emit(("@gol %s %d %d %s %s %s %s" % (gid, g[0], g[1], g[2], g[3], g[4] or "-", g[5])).rstrip())
                    if not g[5]:
                        self.nameless[(gid, g[0], g[1])] = (now, eid, swapped)
            else:
                self.sent = {s for s in self.sent if not (s[0] == gid and (s[1] > cur[0] or s[2] > cur[1]))}
                self.emit("@skor %s %d %d" % (gid, cur[0], cur[1]))
            self.beat[gid] = time.time()
        self.names(now)
        return True

    def names(self, now):
        for key, (since, eid, swapped) in list(self.nameless.items()):
            if now - since > NAME_WAIT_S:
                del self.nameless[key]
                continue
            try:
                rows = self.goals(eid, swapped)
            except Exception:
                continue
            for g in rows:
                if (key[0], g[0], g[1]) == key and g[5]:
                    self.emit("@gol %s %d %d %s %s %s %s" % (key[0], g[0], g[1], g[2], g[3], g[4], g[5]))
                    del self.nameless[key]
                    break

    def run(self):
        while True:
            now = time.time()
            if STATE["ver"] != self.ver:
                self.ver = STATE["ver"]
                self.next_espn = self.next_team = 0.0
                self.games, self.team_games, self.links, self.score = {}, {}, {}, {}
            if not STATE["on"]:
                time.sleep(IDLE_S)
                continue
            try:
                if now >= self.next_espn:
                    self.refresh(now)
                    self.next_espn = now + ESPN_EVERY_S
                busy = self.scan(now)
            except Exception as ex:
                print("hizli gol: %s: %s" % (type(ex).__name__, ex))
                busy = False
            time.sleep(LIVE_EVERY_S if busy else IDLE_S)


def start(echo=False):
    fg = FastGoals(echo=echo)
    threading.Thread(target=fg.run, daemon=True).start()
    return fg


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--deneme":
        sys.stdout.reconfigure(errors="replace")
        rest = sys.argv[2:]
        if len(rest) >= 2 and rest[0] == "--takim":
            set_team(True, int(rest[1]))
            rest = rest[2:]
        FastGoals(leagues=LEAGUES + tuple(rest), out=queue.Queue(), echo=True).run()
    else:
        print(__doc__)
