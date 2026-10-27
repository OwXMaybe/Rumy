"""Gol ekranindaki oyuncu fotograflari (kopru tarafi).

Rumy bir gol oldugunda "@foto <kimlik> <px> <oyuncu adi>" ister; secili takimi da "@futbol <0|1> <takim> <renk1> <renk2>"
ile bildirir (takim ESPN numarasi, renkler uygulamanin kulup renkleri). Takimin adi ve ligi ESPN'den bir kez alinip
tools/.cache/foto/takim_<no>.json'da saklanir. Bu modul oyuncuyu once TheSportsDB'de adiyla arar (ucretsiz deneme
anahtari "3", kisisel kullanim, dakikada en cok 30 istek) ve arka plani kesilmis fotografin kucuk halini (/small,
~50 KB) kullanir. TheSportsDB'nin fotograflarinin bir kismi oyuncunun eski kulubunun formasiyla oldugundan formanin
rengine bakilir: govdenin alt kismindaki opak piksellerin en az %8'i takimin koyu renginde, %5'i acik renginde olmali
(beyaz, siyah gibi renksizler sayilmaz; ikisi de renksizse forma denetlenmez). Uymazsa Wikimedia Commons'ta oyuncunun
"<takim> ... (cropped)" fotografi aranir (Vikipedi editorlerinin yuze gore kirptigi mac fotograflari); o da yoksa eski
formali fotograf kullanilir. Hic resim yoksa Vikipedi'de (once Ingilizce, sonra Turkce) futbolcu sayfasinin fotografi
kullanilir. Bas ve omuzlar kare kirpilir, Rumy'nin gol ekranindaki koyu zemine oturtulur ve istenen boyutta RGB565
(little endian) veriye cevrilir; kart fotografi yuvarlak keserek cizer.

Yanlis kisi secilmesin diye ad eslesmesi siki tutulur: aksanlar ve buyuk/kucuk harf yok sayilir, soyad ayni olmali ve
kelimelerden biri digerinin icinde olmali ("Mert Yandas" = "Mert Hakan Yandas"). Ayni adda birden cok futbolcu varsa
secili takimin oyuncusu secilir, o da yoksa fotograf gonderilmez (kart bas harfleri gosterir).

Kullanicinin sectigi fotograflar her seyden once gelir: tools/oyuncu_foto/<ad_soyad>.jpg (ad, aksansiz kucuk harf ve
alt cizgiyle: romelu_lukaku), istege bagli kirpma kutusu tools/oyuncu_foto/kirpma.json'da {"romelu_lukaku": [x, y, kenar]}
(yoksa ortadan, ustten biraz asagidan kare). Bu fotograflar git'e girmez.

Islenen fotograflar ve kaynaklari tools/.cache/foto/ altinda saklanir (kaynak.json); ayni oyuncu ikinci kez internete
cikmadan gelir. Bulunamayan adlar 7 gun boyunca yeniden aranmaz.

Kullanim:
    python tools/player_photo.py "Kerem Akturkoglu" [px] [--takim 436]   tek oyuncu
    python tools/player_photo.py --kadro [px] --takim 436                 ESPN'deki guncel kadronun tamami:
                                                          fotograflari hazirlar, listeyi yazar,
                                                          tools/.cache/foto/kadro_<px>.png kontrol sayfasini cizer
    --renk 13235b ffd60a ile takimin iki rengi verilebilir (yoksa ESPN'in renkleri)
Gerekenler: Python Pillow
"""
import io
import json
import os
import re
import sys
import threading
import time
import unicodedata
import urllib.error
import urllib.parse
import urllib.request

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE = os.path.join(ROOT, "tools", ".cache", "foto")
MISS_FILE = os.path.join(CACHE, "bulunamayan.json")
META_FILE = os.path.join(CACHE, "kaynak.json")
OWN_DIR = os.path.join(ROOT, "tools", "oyuncu_foto")
OWN_CROPS = os.path.join(OWN_DIR, "kirpma.json")
SEARCH = "https://www.thesportsdb.com/api/v1/json/3/searchplayers.php?p=%s"
WIKI = "https://%s.wikipedia.org/w/rest.php/v1/search/title?q=%s&limit=8"
COMMONS_SEARCH = ("https://commons.wikimedia.org/w/api.php?action=query&list=search&srnamespace=6&srlimit=20"
                  "&format=json&srsearch=%s")
COMMONS_INFO = ("https://commons.wikimedia.org/w/api.php?action=query&prop=imageinfo&iiprop=url&iiurlwidth=250"
                "&format=json&titles=%s")
ROSTER = "https://site.api.espn.com/apis/site/v2/sports/soccer/%s/teams/%d/roster"
TEAM_INFO = "https://site.api.espn.com/apis/site/v2/sports/soccer/all/teams/%d"
TEAM_DAYS = 7
COMMON = {"real", "club", "united", "city", "sporting", "athletic", "atletico", "deportivo", "union", "olympique", "fc",
          "sk", "afc", "cf", "ac", "as", "sc", "spor", "kulubu", "de", "la"}
TIMEOUT_S = 10
MISS_DAYS = 7
MAX_BYTES = 600 * 1024
SPORTSDB_GAP_S = 2.1
WIKI_GAP_S = 2.0
RETRY_429_S = 8
KIT_DARK_PCT = 8
KIT_LIGHT_PCT = 5
KIT_HUE = 18
BG_IN = (26, 54, 120)
BG_OUT = (6, 18, 52)

_lock = threading.Lock()
STATE = {"team": 0, "colors": None}
_last_sportsdb = [0.0]
_last_wiki = [0.0]


def fold(name):
    name = name.replace("ı", "i").replace("İ", "I").replace("ø", "o").replace("Ø", "O").replace("ß", "ss")
    name = unicodedata.normalize("NFKD", name)
    name = "".join(c for c in name if not unicodedata.combining(c))
    return re.sub(r"\s+", " ", name).strip()


def tokens(name):
    return re.sub(r"[^a-z0-9]+", " ", fold(name).lower()).split()


def slug(name):
    return "_".join(tokens(name)) or "oyuncu"


def same_person(a, b):
    ta, tb = tokens(a), tokens(b)
    if not ta or not tb:
        return False
    if ta == tb:
        return True
    if ta[-1] != tb[-1]:
        return False
    return set(ta) <= set(tb) or set(tb) <= set(ta)


def get(url, limit=MAX_BYTES):
    req = urllib.request.Request(url, headers={"User-Agent": "Rumy/1.2 (kisisel masa saati)"})
    try:
        r = urllib.request.urlopen(req, timeout=TIMEOUT_S)
    except urllib.error.HTTPError as e:
        if e.code != 429:
            raise
        time.sleep(RETRY_429_S)
        r = urllib.request.urlopen(req, timeout=TIMEOUT_S)
    with r:
        data = r.read(limit + 1)
    if len(data) > limit:
        raise OSError("dosya cok buyuk")
    return data


def get_json(url, limit=512 * 1024):
    return json.loads(get(url, limit).decode("utf-8"))


def read_json(path):
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


def write_json(path, data):
    os.makedirs(CACHE, exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=1)


def save_miss(key):
    misses = read_json(MISS_FILE)
    misses[key] = int(time.time())
    write_json(MISS_FILE, misses)


def recently_missed(key):
    t = read_json(MISS_FILE).get(key)
    return bool(t) and time.time() - t < MISS_DAYS * 86400


def save_meta(key, info):
    meta = read_json(META_FILE)
    meta[key] = info
    write_json(META_FILE, meta)


def sportsdb_wait():
    gap = time.time() - _last_sportsdb[0]
    if gap < SPORTSDB_GAP_S:
        time.sleep(SPORTSDB_GAP_S - gap)
    _last_sportsdb[0] = time.time()


def set_team(team, c1=None, c2=None):
    team = int(team or 0)
    colors = (c1, c2) if c1 and c2 else None
    if STATE["team"] != team or STATE["colors"] != colors:
        STATE.update(team=team, colors=colors)


def team_info(tid):
    if not tid:
        return {}
    path = os.path.join(CACHE, "takim_%d.json" % tid)
    info = read_json(path)
    if info and time.time() - info.get("t", 0) < TEAM_DAYS * 86400:
        return info
    try:
        t = get_json(TEAM_INFO % tid).get("team") or {}
    except (OSError, ValueError):
        return info
    lg = t.get("defaultLeague")
    info = {"ad": t.get("displayName") or "", "lig": lg.get("slug", "") if isinstance(lg, dict) else "",
            "renk": [t.get("color") or "", t.get("alternateColor") or ""], "t": int(time.time())}
    write_json(path, info)
    return info


def team():
    info = dict(team_info(STATE["team"]))
    if STATE["colors"]:
        info["renk"] = list(STATE["colors"])
    words = [w for w in tokens(info.get("ad") or "") if w not in COMMON and len(w) > 2]
    info["ipucu"] = max(words, key=len) if words else ""
    return info


def pick(players, name, hint):
    soccer = [p for p in players or [] if (p.get("strSport") or "").lower() == "soccer"]
    match = [p for p in soccer if same_person(p.get("strPlayer") or "", name)]
    ours = [p for p in match if hint and hint in fold(p.get("strTeam") or "").lower()]
    if ours:
        return ours[0]
    return match[0] if len(match) == 1 else None


def sportsdb(name, hint):
    seen = set()
    for q in (fold(name), name):
        q = q.replace(" ", "_")
        if q in seen:
            continue
        seen.add(q)
        sportsdb_wait()
        p = pick(get_json(SEARCH % urllib.parse.quote(q), 256 * 1024).get("player"), name, hint)
        if p:
            return p
    return None


def wikipedia(name):
    for lang, word in (("en", "footballer"), ("tr", "futbolcu")):
        wiki_wait()
        pages = get_json(WIKI % (lang, urllib.parse.quote(fold(name)))).get("pages") or []
        found = [p for p in pages if same_person(p.get("title") or "", name) and word in (p.get("description") or "")]
        if len(found) != 1:
            continue
        thumb = (found[0].get("thumbnail") or {}).get("url")
        if not thumb:
            continue
        url = ("https:" + thumb) if thumb.startswith("//") else thumb
        url = re.sub(r"/\d+px-", "/250px-", url.split("?")[0])
        return url, found[0].get("title"), "%s.wikipedia" % lang
    return None


def backdrop(px):
    bg = Image.new("RGB", (px, px), BG_OUT)
    d = ImageDraw.Draw(bg)
    steps = max(8, px // 2)
    for i in range(steps):
        t = i / steps
        r = int(px * 0.75 * (1 - t))
        c = tuple(int(BG_OUT[k] + (BG_IN[k] - BG_OUT[k]) * t) for k in range(3))
        cx, cy = px // 2, int(px * 0.38)
        d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=c)
    return bg


def crop_cutout(im):
    a = im.getchannel("A")
    box = a.getbbox()
    if not box:
        return None
    w, h = im.size
    top = box[1]
    band = a.crop((0, top, w, min(h, top + max(4, h // 4))))
    cols = band.resize((w, 1), Image.BOX).load()
    total = sum(cols[x, 0] for x in range(w))
    cx = sum(x * cols[x, 0] for x in range(w)) / total if total else w / 2
    side = int(min(w, h) * 0.84)
    x0 = int(max(0, min(w - side, cx - side / 2)))
    y0 = int(max(0, min(h - side, top - side * 0.12)))
    return im.crop((x0, y0, x0 + side, y0 + side))


def render(raw, px, cutout):
    im = Image.open(io.BytesIO(raw))
    im.load()
    if cutout and im.mode in ("RGBA", "LA", "P"):
        im = crop_cutout(im.convert("RGBA"))
        if im is None:
            return None
        im = im.resize((px, px), Image.LANCZOS)
        bg = backdrop(px).convert("RGBA")
        bg.alpha_composite(im)
        return bg.convert("RGB")
    im = im.convert("RGB")
    w, h = im.size
    side = min(w, h)
    x0 = (w - side) // 2
    y0 = int((h - side) * 0.12)
    return im.crop((x0, y0, x0 + side, y0 + side)).resize((px, px), Image.LANCZOS)


def to_rgb565(im):
    raw = im.tobytes()
    out = bytearray(len(raw) // 3 * 2)
    i = 0
    for k in range(0, len(raw), 3):
        v = ((raw[k] & 0xF8) << 8) | ((raw[k + 1] & 0xFC) << 3) | (raw[k + 2] >> 3)
        out[i] = v & 0xFF
        out[i + 1] = v >> 8
        i += 2
    return bytes(out)


def hsv(r, g, b):
    mx, mn = max(r, g, b), min(r, g, b)
    d = mx - mn
    if d == 0:
        h = 0
    elif mx == r:
        h = (60 * (g - b) / d) % 360
    elif mx == g:
        h = 60 * (b - r) / d + 120
    else:
        h = 60 * (r - g) / d + 240
    return h, (d / mx if mx else 0), mx / 255


def kit_colors(info):
    out = []
    for c in info.get("renk") or []:
        c = (c or "").strip().lstrip("#")
        if len(c) != 6:
            continue
        h, s, v = hsv(*(int(c[i:i + 2], 16) for i in (0, 2, 4)))
        if s >= 0.35 and v >= 0.15:
            out.append((h, v))
    return sorted(out, key=lambda x: x[1])


def kit_ok(raw, info):
    cols = kit_colors(info)
    if not cols:
        return None
    im = Image.open(io.BytesIO(raw)).convert("RGBA")
    box = im.getchannel("A").getbbox()
    if not box:
        return False
    zone = im.crop((box[0], box[1] + int((box[3] - box[1]) * 0.55), box[2], box[3])).tobytes()
    n = 0
    hits = [0] * len(cols)
    for k in range(0, len(zone), 4):
        if zone[k + 3] < 200:
            continue
        n += 1
        h, s, v = hsv(zone[k], zone[k + 1], zone[k + 2])
        if s < 0.35:
            continue
        for i, (ch, cv) in enumerate(cols):
            if min(abs(h - ch), 360 - abs(h - ch)) <= KIT_HUE and v >= cv * 0.5:
                hits[i] += 1
    need = [KIT_DARK_PCT, KIT_LIGHT_PCT] if len(cols) == 2 else [KIT_DARK_PCT]
    return n > 0 and all(hits[i] * 100 >= need[i] * n for i in range(len(cols)))


def wiki_wait():
    gap = time.time() - _last_wiki[0]
    if gap < WIKI_GAP_S:
        time.sleep(WIKI_GAP_S - gap)
    _last_wiki[0] = time.time()


def commons_team(name, info):
    if not info.get("ipucu"):
        return None
    wiki_wait()
    q = "%s %s cropped" % (fold(name), fold(info.get("ad") or ""))
    hits = get_json(COMMONS_SEARCH % urllib.parse.quote(q)).get("query", {})
    want = tokens(name)
    good = []
    for h in hits.get("search") or []:
        t = h.get("title") or ""
        tt = tokens(t)
        if info["ipucu"] in tt and "cropped" in tt and want and want[-1] in tt and all(w in tt for w in want):
            day = max([w for w in tt if len(w) == 8 and w.isdigit()] or ["0"])
            good.append((day, t))
    if not good:
        return None
    title = sorted(good, reverse=True)[0][1]
    wiki_wait()
    pages = get_json(COMMONS_INFO % urllib.parse.quote(title)).get("query", {}).get("pages", {})
    for page in pages.values():
        info = (page.get("imageinfo") or [{}])[0]
        if info.get("thumburl"):
            return info["thumburl"], title[5:] if title.startswith("File:") else title
    return None


def download(url, sportsdb_img):
    for suffix in (("/small", "") if sportsdb_img else ("",)):
        try:
            return get(url + suffix)
        except OSError:
            pass
    raise OSError("fotograf indirilemedi")


def find(name):
    t = team()
    try:
        p = sportsdb(name, t.get("ipucu"))
    except (OSError, ValueError) as e:
        print("foto: TheSportsDB kullanilamadi (%s), Commons ve Vikipedi deneniyor" % e)
        p = None
    spare = None
    if p and p.get("strCutout"):
        raw = download(p["strCutout"], True)
        kit = kit_ok(raw, t)
        info = {"kaynak": "TheSportsDB", "ad": p.get("strPlayer"), "takim": p.get("strTeam"), "dekupe": True,
                "forma": kit}
        if kit is not False:
            return raw, True, info
        spare = (raw, True, info)
    c = commons_team(name, t)
    if c:
        return download(c[0], False), False, {"kaynak": "Commons", "ad": c[1], "takim": t.get("ad") or "",
                                              "dekupe": False, "forma": True}
    if spare:
        return spare
    if p and p.get("strThumb"):
        return download(p["strThumb"], True), False, {"kaynak": "TheSportsDB", "ad": p.get("strPlayer"),
                                                      "takim": p.get("strTeam"), "dekupe": False}
    wiki_wait()
    w = wikipedia(name)
    if w:
        return download(w[0], False), False, {"kaynak": w[2], "ad": w[1], "takim": "", "dekupe": False}
    return None


def own_photo(key):
    for ext in (".jpg", ".jpeg", ".png", ".webp"):
        path = os.path.join(OWN_DIR, key + ext)
        if os.path.exists(path):
            return path
    return None


def render_own(path, key, px):
    im = Image.open(path).convert("RGB")
    box = read_json(OWN_CROPS).get(key)
    if box:
        x, y, side = box
        im = im.crop((x, y, x + side, y + side))
    else:
        w, h = im.size
        side = min(w, h)
        x0 = (w - side) // 2
        im = im.crop((x0, int((h - side) * 0.12), x0 + side, int((h - side) * 0.12) + side))
    return im.resize((px, px), Image.LANCZOS)


def photo(name, px):
    """Oyuncunun px x px RGB565 fotografini dondurur; bulunamazsa None. Ag hatasinda OSError atar."""
    name = (name or "").strip()
    if not name or not 32 <= px <= 200:
        return None
    with _lock:
        os.makedirs(CACHE, exist_ok=True)
        key = slug(name)
        done = os.path.join(CACHE, "%s_%d.png" % (key, px))
        own = own_photo(key)
        if own:
            im = render_own(own, key, px)
            im.save(done)
            save_meta(key, {"kaynak": "elle", "ad": os.path.basename(own), "takim": team().get("ad") or "",
                            "dekupe": False, "forma": True})
            return to_rgb565(im)
        if os.path.exists(done):
            return to_rgb565(Image.open(done).convert("RGB"))
        if recently_missed(key):
            return None
        try:
            hit = find(name)
        except ValueError as e:
            raise OSError("gecersiz yanit: %s" % e)
        if not hit:
            save_miss(key)
            return None
        raw, cutout, info = hit
        im = render(raw, px, cutout)
        if im is None:
            save_miss(key)
            return None
        im.save(done)
        info["bayt"] = len(raw)
        save_meta(key, info)
        print("foto: %s -> %s (%s, %s, %d B indirildi)" % (fold(name), fold(info["ad"] or "?"),
                                                          fold(info["takim"] or "-"), info["kaynak"], len(raw)))
        return to_rgb565(im)


def espn_name(name):
    words = []
    for w in name.split(" "):
        if "̇" in w:
            w = w.replace("̇", "")
            w = w[:1] + w[1:].replace("I", "ı").replace("İ", "i").lower()
        words.append(w)
    return " ".join(words)


def squad(px):
    """Guncel kadronun fotograflarini hazirlar; [(ad, durum, bilgi)] dondurur."""
    tid = STATE["team"]
    if not tid:
        return []
    athletes = get_json(ROSTER % (team().get("lig") or "tur.1", tid), 1024 * 1024).get("athletes") or []
    rows = []
    meta_before = read_json(META_FILE)
    for a in athletes:
        name = espn_name(a.get("displayName") or "")
        if not name:
            continue
        try:
            ok = photo(name, px) is not None
            status = "var" if ok else "yok"
        except OSError as e:
            status = "hata: %s" % e
        rows.append((name, status, read_json(META_FILE).get(slug(name)) or meta_before.get(slug(name)) or {}))
    return rows


def sheet(rows, px, path):
    cols = 6
    cw, ch = px + 16, px + 44
    im = Image.new("RGB", (cols * cw + 8, ((len(rows) + cols - 1) // cols) * ch + 8), (20, 24, 30))
    d = ImageDraw.Draw(im)
    try:
        font = ImageFont.truetype("arial.ttf", 11)
    except OSError:
        font = ImageFont.load_default()
    for i, (name, status, info) in enumerate(rows):
        x = 8 + (i % cols) * cw
        y = 8 + (i // cols) * ch
        p = os.path.join(CACHE, "%s_%d.png" % (slug(name), px))
        if status == "var" and os.path.exists(p):
            im.paste(Image.open(p).convert("RGB"), (x, y))
        else:
            d.rectangle((x, y, x + px - 1, y + px - 1), outline=(90, 90, 90))
            d.text((x + 6, y + px // 2 - 6), "yok", fill=(160, 160, 160), font=font)
        d.text((x, y + px + 2), name[:18], fill=(240, 240, 240), font=font)
        d.text((x, y + px + 16), (info.get("kaynak") or "-")[:18], fill=(255, 222, 0), font=font)
    im.save(path)


def cli_args(argv):
    rest = []
    i = 0
    while i < len(argv):
        if argv[i] == "--takim" and i + 1 < len(argv):
            STATE["team"] = int(argv[i + 1])
            i += 2
        elif argv[i] == "--renk" and i + 2 < len(argv):
            STATE["colors"] = (argv[i + 1], argv[i + 2])
            i += 3
        else:
            rest.append(argv[i])
            i += 1
    return rest


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.argv[1:] = cli_args(sys.argv[1:])
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    if sys.argv[1] == "--kadro":
        if not STATE["team"]:
            sys.exit("--kadro icin --takim <espn-no> verin (or. --takim 436)")
        size = int(sys.argv[2]) if len(sys.argv) > 2 else 84
        result = squad(size)
        for n, s, info in result:
            print("%-24s %-6s %-14s %s / %s" % (n, s, info.get("kaynak", "-"), info.get("ad", "-"), info.get("takim", "-")))
        out = os.path.join(CACHE, "kadro_%d.png" % size)
        sheet(result, size, out)
        print("%d oyuncu, %d fotograf -> %s" % (len(result), sum(1 for r in result if r[1] == "var"), out))
    else:
        size = int(sys.argv[2]) if len(sys.argv) > 2 else 84
        data = photo(sys.argv[1], size)
        print("bulunamadi" if data is None else "%d B -> %s" % (len(data), CACHE))
