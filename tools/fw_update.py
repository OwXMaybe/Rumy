"""Rumy'ye WiFi (ya da USB) uzerinden yeni yazilim yollar: kablosuz guncelleme.

Kullanim:
    python tools/spotify_bridge.py --guncelle                 dist/Rumy.bin'i yollar (once: pio run)
    python tools/spotify_bridge.py --guncelle dist/Rumy-st7789.bin
    python tools/spotify_bridge.py --guncelle dist/Rumy-guncelleyici.bin   guncelleyicinin kendisini yeniler
    python tools/spotify_bridge.py --guncelle --zorla         Rumy'de ayni yazilim calissa da yollar

Kopru calisiyorsa istek ona iletilir (tools/.cache/guncelle_istek.txt) ve ilerleme buradan izlenir; calismiyorsa bu
komut --pi/--wifi adresine (ya da USB'ye) kendisi baglanir. Gondermeden once imajin basligi ve sonundaki SHA-256
denetlenir; bozuk imaj yollanmaz.
Ana yazilim: Rumy dosyayi SD karta (/rumy/guncelleme.bin) yazar, SHA-256'yi denetler, guncelleyiciyle (ota_1)
yeniden baslar; guncelleyici dosyayi yeniden dogrulayip ana bolume (ota_0) kopyalar.
Guncelleyici ("Rumy guncelleniyor" yazisini iceren imaj): Rumy onu SD'ye ugramadan dogrudan ota_1 bolumune yazar.
Rumy yeniden baglaninca "@merhaba" satirindaki ELF ozetlerinin ilk 8 hanesi beklenenle karsilastirilir.

Protokol:
    PC -> Rumy  @guncelle <bayt> <sha256> [guncelleyici]
    Rumy -> PC  @guncelle git | @guncelle hata <neden>
    PC -> Rumy  imaj, 2048 baytlik parcalar; Rumy her parcadan sonra "@guncelle ok <n>", sonunda
                "@guncelle tamam <elf8>" ya da "@guncelle hata <neden>"
    Rumy -> PC  @merhaba <kapak> <kamera> <uygulama_elf8> <guncelleyici_elf8>   (yeniden baglaninca)
"""
import hashlib
import os
import struct
import time

CHUNK = 2048
APP_LIMIT = 0x280000
UPDATER_LIMIT = 0x70000
UPDATER_MARK = "Rumy güncelleniyor".encode("utf-8")
IMAGE_MAGIC = 0xE9
DESC_OFFSET = 32
DESC_MAGIC = 0xABCD5432
HEAD_BYTES = DESC_OFFSET + 176
HASH_FLAG = 23
SHA_BYTES = 32
REPLY_S = 6.0
FIRST_REPLY_S = 10.0
VERIFY_S = 180.0
PICKUP_S = 30.0
STALL_S = 45.0
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE = os.path.join(ROOT, "tools", ".cache")
REQ_FILE = os.path.join(CACHE, "guncelle_istek.txt")
STATUS_FILE = os.path.join(CACHE, "guncelle_durum.txt")
EXPECT_FILE = os.path.join(CACHE, "guncelle_bekle.txt")
CURRENT_FILE = os.path.join(CACHE, "rumy_elf.txt")
DEFAULT_BIN = os.path.join(ROOT, "dist", "Rumy.bin")


def image_info(data):
    if len(data) < HEAD_BYTES or data[0] != IMAGE_MAGIC:
        return None
    chip = struct.unpack_from("<H", data, 12)[0]
    magic = struct.unpack_from("<I", data, DESC_OFFSET)[0]
    if chip != 0 or magic != DESC_MAGIC or data[HASH_FLAG] != 1:
        return None
    return {"elf8": data[DESC_OFFSET + 144:DESC_OFFSET + 148].hex(), "updater": UPDATER_MARK in data}


def load(path):
    try:
        with open(path, "rb") as f:
            data = f.read()
    except OSError as e:
        raise ValueError("dosya okunamadi: %s" % e)
    info = image_info(data)
    if info is None:
        raise ValueError("ESP32 uygulama imaji degil: %s" % path)
    limit = UPDATER_LIMIT if info["updater"] else APP_LIMIT
    if len(data) > limit:
        raise ValueError("imaj bolumune sigmiyor: %d > %d B" % (len(data), limit))
    if hashlib.sha256(data[:-SHA_BYTES]).digest() != data[-SHA_BYTES:]:
        raise ValueError("imaj bozuk: sonundaki SHA-256 icerikle tutmuyor: %s" % path)
    info["data"] = data
    info["sha"] = hashlib.sha256(data).hexdigest()
    info["path"] = os.path.abspath(path)
    info["when"] = time.strftime("%Y-%m-%d %H:%M", time.localtime(os.path.getmtime(path)))
    return info


def what(info):
    return "guncelleyici" if info["updater"] else "yazilim"


def describe(info):
    return "%s%s · %d B · elf %s · %s" % (os.path.basename(info["path"]), " (guncelleyici)" if info["updater"] else "",
                                         len(info["data"]), info["elf8"], info["when"])


def status(text):
    os.makedirs(CACHE, exist_ok=True)
    with open(STATUS_FILE, "a", encoding="utf-8") as f:
        f.write(text + "\n")
    print("guncelleme: " + text)


def wait_update(link, pending, timeout):
    end = time.time() + timeout
    while True:
        left = end - time.time()
        if left <= 0:
            return None
        r = link.wait(("guncelle ",), left, pending)
        if r is None:
            return None
        if r.startswith(("guncelle ok", "guncelle git", "guncelle tamam", "guncelle hata")):
            return r[9:]


def send(link, info, pending, report):
    data = info["data"]
    t0 = time.time()
    tail = " guncelleyici" if info["updater"] else ""
    if not link.write(("@guncelle %d %s%s\n" % (len(data), info["sha"], tail)).encode()):
        return False, "baglanti yazilamadi"
    r = wait_update(link, pending, FIRST_REPLY_S)
    if r is None:
        return False, "Rumy yanit vermedi (eski yazilim olabilir: ilk kez USB ile yukleyin)"
    if r.startswith("hata"):
        return False, "Rumy reddetti: %s" % r[5:]
    if r != "git":
        return False, "beklenmeyen yanit: %s" % r
    off = 0
    shown = 0
    while off < len(data):
        part = data[off:off + CHUNK]
        if not link.write(part):
            return False, "baglanti koptu (%d/%d B)" % (off, len(data))
        off += len(part)
        want = "ok %d" % off if off < len(data) else None
        r = wait_update(link, pending, REPLY_S)
        if r is None:
            return False, "Rumy yanit vermedi (%d/%d B)" % (off, len(data))
        if r.startswith("hata"):
            return False, "Rumy aktarimi kesti: %s (%d/%d B)" % (r[5:], off, len(data))
        if want and r != want:
            return False, "beklenmeyen yanit: %s" % r
        pct = off * 100 // len(data)
        if pct // 10 != shown:
            shown = pct // 10
            dt = max(time.time() - t0, 0.001)
            report("ilerleme %%%d · %d KB · %.0f KB/sn" % (pct, off // 1024, off / 1024.0 / dt))
    if not r.startswith("tamam"):
        return False, "beklenmeyen son yanit: %s" % r
    got = r[6:].strip()
    if got != info["elf8"]:
        return False, "Rumy farkli bir imaj gordu: %s (beklenen %s)" % (got, info["elf8"])
    after = "guncelleyici yazildi, Rumy yeniden basliyor" if info["updater"] else \
        "Rumy guncelleyiciyle yeniden basliyor"
    return True, "%d B %.0f sn'de aktarildi, %s" % (len(data), time.time() - t0, after)


def expect(info):
    os.makedirs(CACHE, exist_ok=True)
    with open(EXPECT_FILE, "w", encoding="ascii") as f:
        f.write("%s %d %s\n" % (info["elf8"], int(time.time()), what(info)))


def seen(link):
    return getattr(link, "elf", None), getattr(link, "upd_elf", None)


def matches(info, app, upd):
    return (upd if info["updater"] else app) == info["elf8"]


def connected(app, upd):
    if app and app != "-":
        os.makedirs(CACHE, exist_ok=True)
        with open(CURRENT_FILE, "w", encoding="ascii") as f:
            f.write("%s %s" % (app, upd or "-"))
    try:
        with open(EXPECT_FILE, "r", encoding="ascii") as f:
            want, since, kind = f.read().split()
    except (OSError, ValueError):
        return
    if time.time() - int(since) > VERIFY_S * 2:
        os.remove(EXPECT_FILE)
        return
    got = upd if kind == "guncelleyici" else app
    if not got or got == "-":
        return
    os.remove(EXPECT_FILE)
    if got == want:
        status("SON DOGRULANDI Rumy yeni %s ile calisiyor (elf %s)" % (kind, got))
    else:
        status("SON UYUSMUYOR Rumy'de eski %s duruyor (elf %s, beklenen %s); ekranindaki hataya bakin" % (kind, got, want))


def take_request():
    if not os.path.exists(REQ_FILE):
        return None
    try:
        with open(REQ_FILE, "r", encoding="utf-8") as f:
            path = f.read().strip()
        os.remove(REQ_FILE)
    except OSError:
        return None
    return path or None


def run_request(link, path, pending):
    try:
        info = load(path)
    except ValueError as e:
        status("SON HATA %s" % e)
        return
    status("BASLADI %s" % describe(info))
    ok, msg = send(link, info, pending, status)
    if ok:
        expect(info)
        status("AKTARILDI %s" % msg)
    else:
        status("SON HATA %s" % msg)


def follow(timeout_pickup):
    pos = 0
    t0 = time.time()
    started = False
    sent_at = None
    heard = t0
    while True:
        try:
            with open(STATUS_FILE, "r", encoding="utf-8") as f:
                f.seek(pos)
                chunk = f.read()
                pos = f.tell()
        except OSError:
            chunk = ""
        for line in chunk.splitlines():
            heard = time.time()
            word, _, rest = line.partition(" ")
            if word == "SON":
                kind, _, text = rest.partition(" ")
                print(("TAMAM: " if kind == "DOGRULANDI" else "HATA: ") + text)
                return kind == "DOGRULANDI"
            started = True
            if word == "AKTARILDI":
                sent_at = time.time()
                print(rest)
                print("Rumy yeniden baglanip yeni surumu bildirene kadar bekleniyor...")
            else:
                print(rest if word in ("BASLADI", "ilerleme") else line)
        if not started and time.time() - t0 > timeout_pickup:
            if os.path.exists(REQ_FILE):
                os.remove(REQ_FILE)
            print("HATA: kopru istegi almadi (Rumy'ye bagli degil olabilir)")
            return False
        if started and not sent_at and time.time() - heard > STALL_S:
            print("HATA: kopruden %d sn haber yok (kapanmis olabilir); Rumy eski yazilimla devam eder" % STALL_S)
            return False
        if sent_at and time.time() - sent_at > VERIFY_S:
            print("HATA: Rumy %d sn icinde yeni surumle baglanmadi; ekranina bakin" % VERIFY_S)
            return False
        time.sleep(0.3)


def via_bridge(info):
    os.makedirs(CACHE, exist_ok=True)
    with open(STATUS_FILE, "w", encoding="utf-8"):
        pass
    tmp = REQ_FILE + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        f.write(info["path"])
    os.replace(tmp, REQ_FILE)
    print("calisan kopruye iletildi, ilerleme izleniyor...")
    return follow(PICKUP_S)


def standalone(info, make_link, force):
    link = make_link()
    if not link.open():
        print("HATA: Rumy'ye baglanilamadi")
        return False
    app, upd = seen(link)
    if not force and matches(info, app, upd):
        link.close()
        print("Rumy'de zaten bu %s var (elf %s); yine de yollamak icin --zorla" % (what(info), info["elf8"]))
        return True
    pending = []
    try:
        ok, msg = send(link, info, pending, lambda t: print(t))
    finally:
        link.close()
    if not ok:
        print("HATA: " + msg)
        return False
    print(msg)
    if not hasattr(link, "token"):
        print("USB'de dogrulama yok; Rumy acilinca 'rapor' ciktisindaki [guncelleme] satirina bakin (elf %s)"
              % info["elf8"])
        return True
    print("Rumy yeniden baglanip yeni surumu bildirene kadar bekleniyor...")
    time.sleep(15)
    end = time.time() + VERIFY_S
    while time.time() < end:
        link = make_link()
        if link.open():
            app, upd = seen(link)
            link.close()
            got = upd if info["updater"] else app
            if got == info["elf8"]:
                print("TAMAM: Rumy yeni %s ile calisiyor (elf %s)" % (what(info), got))
                return True
            print("HATA: Rumy'de eski %s duruyor (elf %s, beklenen %s); ekranina bakin" % (what(info), got, info["elf8"]))
            return False
        time.sleep(5)
    print("HATA: Rumy %d sn icinde yeniden baglanmadi; ekranina bakin" % VERIFY_S)
    return False


def last_seen():
    try:
        with open(CURRENT_FILE, "r", encoding="ascii") as f:
            parts = f.read().split()
    except OSError:
        return None, None
    return (parts + [None, None])[0], (parts + [None, None])[1]


def cli(path, force, bridge_running, make_link):
    try:
        info = load(path or DEFAULT_BIN)
    except ValueError as e:
        print("HATA: %s" % e)
        return False
    print("yeni %s: %s" % (what(info), describe(info)))
    if not bridge_running:
        return standalone(info, make_link, force)
    app, upd = last_seen()
    if not force and matches(info, app, upd):
        print("Rumy'de zaten bu %s var (elf %s); yine de yollamak icin --zorla" % (what(info), info["elf8"]))
        return True
    return via_bridge(info)
