"""Rumy'nin HTTPS isteyen verilerini (AFAD depremleri, Nager bayram tarihleri) bilgisayarda ceker, ozetini tek satirla yollar.

spotify_bridge.py icinden kullanilir: Rumy "@veri afad <yol>" ya da "@veri nager <yil>" der, kopru
"@deprem <24sa-sayi> <son: zaman buyukluk derinlik enlem boylam> <en-buyuk: ...>\\t<yer>\\t<yer>" ya da
"@bayram <yil> <ramazan-ay> <gun> <kurban-ay> <gun> <kesin-degil 0/1>" ile cevap verir; hata olursa
"@deprem hata ..." / "@bayram hata ...". Rumy'nin bellegi HTTPS'e yetmedigi icin bu yol kullanilir.

Tek basina deneme (Marmara, son 2 gun, M2,0 ve uzeri / verilen yilin bayramlari):
    python tools/veri_koprusu.py afad
    python tools/veri_koprusu.py nager 2027
"""
import json
import math
import queue
import sys
import threading
import time
import urllib.parse
import urllib.request
from datetime import datetime, timedelta, timezone

AFAD_HOST = "https://servisnet.afad.gov.tr"
AFAD_PREFIX = "/apigateway/deprem/apiv2/event/filter?"
NAGER_URL = "https://date.nager.at/api/v3/PublicHolidays/%d/TR"
USER_AGENT = "Rumy/1.2 (kisisel masa saati)"
TIMEOUT_S = 10
NA16 = -32768
PLACE_BYTES = 55

OUT = queue.Queue()
_IN = queue.Queue()
_started = False


def _get_json(url):
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT, "Accept": "application/json"})
    with urllib.request.urlopen(req, timeout=TIMEOUT_S) as r:
        return json.load(r)


def _lround(v):
    return int(math.copysign(math.floor(abs(v) + 0.5), v))


def _num(v):
    try:
        f = float(v)
    except (TypeError, ValueError):
        return None
    return None if math.isnan(f) else f


def _utf8_cut(text, limit):
    b = text.encode("utf-8")[:limit]
    return b.decode("utf-8", "ignore")


def short_place(text):
    text = (text or "").replace("\t", " ").replace("\n", " ")
    dash = text.find(" - [")
    close = text.find("] ", dash) if dash >= 0 else -1
    if dash >= 0 and close >= 0:
        text = text[:dash] + ", " + text[close + 2:]
    return _utf8_cut(text, PLACE_BYTES)


def _parse_time(text):
    try:
        return int(datetime.strptime((text or "")[:19], "%Y-%m-%dT%H:%M:%S").replace(tzinfo=timezone.utc).timestamp())
    except ValueError:
        return None


def quake_line(events, now):
    latest = strongest = None
    count = 0
    for e in events:
        t = _parse_time(e.get("date"))
        mag = _num(e.get("magnitude"))
        if t is None or mag is None:
            continue
        depth = _num(e.get("depth"))
        lat = _num(e.get("latitude"))
        lon = _num(e.get("longitude"))
        q = (t, _lround(mag * 10), NA16 if depth is None else _lround(depth * 10),
             0 if lat is None else _lround(lat * 10000), 0 if lon is None else _lround(lon * 10000),
             short_place(e.get("location")))
        if latest is None or t > latest[0]:
            latest = q
        if t + 86400 >= now:
            count = min(count + 1, 255)
            if strongest is None or q[1] > strongest[1]:
                strongest = q
    empty = (0, 0, 0, 0, 0, "")
    a = latest or empty
    b = strongest or empty
    return "@deprem %d %d %d %d %d %d %d %d %d %d %d\t%s\t%s" % (
        count, a[0], a[1], a[2], a[3], a[4], b[0], b[1], b[2], b[3], b[4], a[5], b[5])


def holiday_line(items, year):
    rm = rd = km = kd = 0
    tentative = False
    for h in items:
        local = h.get("localName") or ""
        name = h.get("name") or ""
        try:
            d = datetime.strptime((h.get("date") or "")[:10], "%Y-%m-%d")
        except ValueError:
            continue
        if d.year != year:
            continue
        ramazan = local.startswith("Ramazan Bayram") or name.startswith("Eid al-Fitr")
        kurban = local.startswith("Kurban Bayram") or name.startswith("Eid al-Adha")
        if (ramazan or kurban) and ("Tentative" in local or "Tentative" in name):
            tentative = True
        if ramazan and (rm == 0 or (d.month, d.day) < (rm, rd)):
            rm, rd = d.month, d.day
        if kurban and (km == 0 or (d.month, d.day) < (km, kd)):
            km, kd = d.month, d.day
    if not rm or not km:
        return "@bayram hata %d bulunamadi" % year
    return "@bayram %d %d %d %d %d %d" % (year, rm, rd, km, kd, 1 if tentative else 0)


def answer(text):
    kind, _, arg = text.strip().partition(" ")
    arg = arg.strip()
    if kind == "afad":
        if not arg.startswith(AFAD_PREFIX) or any(c in arg for c in " \t\r\n#"):
            return "@deprem hata yol"
        try:
            events = _get_json(AFAD_HOST + arg)
        except Exception as e:
            return "@deprem hata %s" % type(e).__name__
        if not isinstance(events, list):
            return "@deprem hata bicim"
        return quake_line(events, int(time.time()))
    if kind == "nager":
        if not arg.isdigit() or not 2000 <= int(arg) <= 2100:
            return "@bayram hata yil"
        try:
            items = _get_json(NAGER_URL % int(arg))
        except Exception as e:
            return "@bayram hata %s %s" % (arg, type(e).__name__)
        if not isinstance(items, list):
            return "@bayram hata %s bicim" % arg
        return holiday_line(items, int(arg))
    return None


def _worker():
    while True:
        text = _IN.get()
        line = answer(text)
        if line:
            OUT.put(line)


def start():
    global _started
    if _started:
        return
    _started = True
    threading.Thread(target=_worker, daemon=True, name="veri").start()


def request(text):
    start()
    if _IN.qsize() < 8:
        _IN.put(text)


def _marmara_path():
    now = datetime.now(timezone.utc)
    fmt = "%Y-%m-%dT%H:%M:%S"
    q = {"start": (now - timedelta(days=2)).strftime(fmt), "end": (now + timedelta(hours=1)).strftime(fmt),
         "minlat": "39.50", "maxlat": "41.80", "minlon": "26.00", "maxlon": "30.50", "minmag": "2.0",
         "orderby": "timedesc", "limit": "500", "format": "json"}
    return AFAD_PREFIX + urllib.parse.urlencode(q, safe=":")


if __name__ == "__main__":
    sys.stdout.reconfigure(errors="replace")
    if len(sys.argv) > 1 and sys.argv[1] == "afad":
        print(answer("afad " + _marmara_path()))
    elif len(sys.argv) > 2 and sys.argv[1] == "nager":
        print(answer("nager " + sys.argv[2]))
    else:
        print(__doc__)
