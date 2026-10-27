"""Telefon/bilgisayar uygulamasinin takim listesini (web/index.html icindeki LEAGUES ve TEAMS) ESPN'den uretir.

Super Lig ve bes buyuk lig (Premier Lig, LaLiga, Serie A, Bundesliga, Ligue 1) icin ESPN'in takim listesini alir
(lig basina bir istek, sifresiz HTTP): ESPN kimligi, ad, kisaltma ve iki renk. Super Lig takimlarinin Turkce adlari ve
kisaltmalari kartin tablosundan (lib/rumy_core/src/espn.cpp, kTeams) okunur. ESPN Super Lig takimlarinin cogunun
rengini vermedigi (000000) ve birkac buyuk kulubun rengini yanlis verdigi icin kulup renkleri RENK tablosundan gelir;
uygulama bu iki rengi takim secilince karta yollar (gol ekrani ve ampul onlarla boyanir). Super Lig'de dort buyuk takim
basta, gerisi ada gore siralanir. Sonuc dosyadaki "const LEAGUES=" ve "const TEAMS=" satirlarina yazilir.

Ligler her sezon degistiginden sezon basinda yeniden calistirilir.

Kullanim:
    python tools/make_teams.py               web/index.html'i gunceller
    python tools/make_teams.py dosya.html    baska bir dosyayi (or. taslak) gunceller
"""
import json
import os
import re
import sys
import time
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
URL = "http://site.api.espn.com/apis/site/v2/sports/soccer/%s/teams"
UA = {"User-Agent": "Rumy/1.2 (kisisel masa saati)"}
LIGLER = [("tur.1", "Süper Lig"), ("eng.1", "Premier Lig"), ("esp.1", "LaLiga"), ("ita.1", "Serie A"),
          ("ger.1", "Bundesliga"), ("fra.1", "Ligue 1")]
BUYUKLER = ["436", "432", "1895", "997"]
RENK = {
    "436": ("13235b", "ffd60a"), "432": ("a6192e", "fdb913"), "1895": ("111111", "ffffff"),
    "997": ("7a1c34", "7fc6ef"), "7914": ("f26522", "1b2a4a"), "7656": ("008c45", "0057a8"),
    "20729": ("5b2c83", "ffd200"), "996": ("e30613", "111111"), "789": ("ffcc00", "e30613"),
    "6870": ("1d2f6f", "ffffff"), "7648": ("00843d", "ffffff"), "9078": ("f58220", "00843d"),
    "20070": ("d71920", "111111"), "11429": ("e30613", "ffffff"), "995": ("00843d", "111111"),
    "132335": ("008c45", "d71920"), "19267": ("0057a8", "ffffff"), "132334": ("d71920", "111111"),
    "83": ("a50044", "004d98"), "86": ("ffffff", "00529f"), "111": ("111111", "ffffff"),
}
TR_ABC = "abcçdefgğhıijklmnoöpqrsştuüvwxyz"


def get(url):
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=15) as r:
        return json.loads(r.read().decode("utf-8"))


def turkish_names():
    with open(os.path.join(ROOT, "lib", "rumy_core", "src", "espn.cpp"), encoding="utf-8") as f:
        src = f.read()
    block = src[src.index("kTeams[]"):]
    block = block[:block.index("};")]
    return {m.group(1): (m.group(2), m.group(3)) for m in re.finditer(r'\{"([^"]+)", "([^"]+)", "([^"]+)"\}', block)}


def hex6(c):
    c = (c or "").strip().lstrip("#").lower()
    return c if re.fullmatch(r"[0-9a-f]{6}", c) else ""


def colors(tid, team):
    if tid in RENK:
        return RENK[tid]
    a, b = hex6(team.get("color")), hex6(team.get("alternateColor"))
    if not a or a == b:
        return ("2c323b", "cfd6df")
    return (a, b or "ffffff")


def tr_key(name):
    low = name.replace("I", "ı").replace("İ", "i").lower()
    return [TR_ABC.index(ch) if ch in TR_ABC else 100 + ord(ch) for ch in low]


def fetch():
    tr = turkish_names()
    teams = []
    for idx, (slug, _) in enumerate(LIGLER):
        data = get(URL % slug)
        rows = []
        for item in data["sports"][0]["leagues"][0]["teams"]:
            t = item["team"]
            tid = str(t["id"])
            name, abbr = t.get("displayName") or t.get("shortDisplayName") or tid, t.get("abbreviation") or ""
            if slug == "tur.1" and name in tr:
                name, abbr = tr[name]
            c1, c2 = colors(tid, t)
            rows.append([int(tid), name, abbr[:4], c1, c2, idx])
        rows.sort(key=lambda r: (str(r[0]) not in BUYUKLER, BUYUKLER.index(str(r[0])) if str(r[0]) in BUYUKLER else 0,
                                 tr_key(r[1])))
        teams.extend(rows)
        print("%-6s %2d takim" % (slug, len(rows)))
        time.sleep(0.4)
    return teams


def write(path, teams):
    with open(path, encoding="utf-8") as f:
        html = f.read()
    leagues = "const LEAGUES=" + json.dumps([[s, n] for s, n in LIGLER], ensure_ascii=False, separators=(",", ":")) + ";"
    body = "const TEAMS=" + json.dumps(teams, ensure_ascii=False, separators=(",", ":")) + ";"
    pat = re.compile(r"(const LEAGUES=\[.*?\];\n)?const TEAMS=\[.*?\]\];", re.S)
    if not pat.search(html):
        sys.exit("make_teams: %s icinde 'const TEAMS=' bulunamadi" % path)
    html = pat.sub(lambda _: leagues + "\n" + body, html, count=1)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(html)
    print("yazildi: %s (%d takim, %d B)" % (path, len(teams), len(body)))


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "web", "index.html")
    write(path, fetch())


if __name__ == "__main__":
    main()
