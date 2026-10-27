"""Rumy Spotify koprusu: bilgisayarda calan sarkiyi (kapak, ad, sanatci, sure) USB uzerinden Rumy'ye gonderir,
Rumy'deki dugmelerle (onceki / oynat-duraklat / sonraki / ses) bilgisayardaki calmayi yonetir.

Spotify hesabi ya da API anahtari gerekmez: bilgiler Windows'un medya oturumundan (SMTC) okunur; Spotify
masaustu uygulamasi sarki bilgisini ve kapagi zaten Windows'a bildirir. Fikir, seri port ile "now playing"
gonderen acik kaynak projelerden (or. RodgerE1/NowPLayingDisplay) ve CYD Spotify ekranlarindan
(witnessmenow/Spotify-Diy-Thing, MIT) uyarlanmistir.

Kullanim:
    python tools/spotify_bridge.py                 otomatik port, yalnizca Spotify
    python tools/spotify_bridge.py --any           Spotify yoksa diger oynaticilar da (tarayici, VLC...)
    python tools/spotify_bridge.py --port COM11 --log
    python tools/spotify_bridge.py --pi rumy-pi.local  Raspberry Pi'deki Rumy'ye WiFi ile (TCP 7777) baglan;
                                                     anahtar tools/.cache/pi_anahtar.txt dosyasindan okunur
    python tools/spotify_bridge.py --wifi 192.168.1.40  CYD'ye WiFi ile (TCP 7777) baglan, kablo yalnizca guc
    python tools/spotify_bridge.py --anahtar-yaz      ayni anahtari CYD'ye USB'den bir kez yaz (yoksa uretir);
                                                     CYD anahtari NVS'te saklar, anahtar ekrana yazilmaz
    python tools/spotify_bridge.py --autostart ac     Windows acilisinda konsolsuz (pythonw) bekciyle baslat; Gorev
                                                     Zamanlayici 5 dk'da bir bekcinin calistigini denetler
    python tools/spotify_bridge.py --autostart kapat  otomatik baslatmayi ve gorevi kaldir
    python tools/spotify_bridge.py --bekci --pi ...   kopruyu baslatir, kapanirsa yeniden acar (tek kopya)
    python tools/spotify_bridge.py --durdur           calisan bekciyi ve kopruyu kapatir
    python tools/spotify_bridge.py --spotify-giris <client_id>  calma listeleri icin Spotify hesabina bir kez giris
                                                     (tools/spotify_api.py; Premium ve gelistirici uygulamasi gerekir)
    python tools/spotify_bridge.py --guncelle [bin]  yeni yazilimi (varsayilan dist/Rumy.bin) WiFi'den Rumy'ye
                                                     yollar; kopru calisiyorsa ona iletir (tools/fw_update.py)
Gerekenler:
    python -m pip install pyserial pillow winrt-runtime winrt-Windows.Media.Control
        winrt-Windows.Storage.Streams winrt-Windows.Foundation winrt-Windows.Foundation.Collections
Not: kopru calisirken seri port mesguldur; karta yukleme yapmadan once kopruyu kapatin (Ctrl+C ya da --durdur).
Konsolsuz calisirken ciktilar tools/.cache/kopru.log dosyasina yazilir.

Protokol (satirlar '@' ile baslar, Rumy'nin gunluk satirlari ayni hattan gelir ve '@' ile baslamaz):
    PC -> Rumy  @np<TAB>calıyor<TAB>konum_ms<TAB>sure_ms<TAB>kapak_hash<TAB>renk<TAB>uygulama<TAB>ad<TAB>sanatci<TAB>albüm
    Rumy -> PC  @need <hash>           kapak onbellekte yok
    PC -> Rumy  @art <hash> <g> <y> <bayt> <crc32>, Rumy "@go <hash>" der; ardindan RGB565 (little endian) ham
                veri, 2048 baytlik parcalar halinde; Rumy her parcadan sonra "@ok <n>", sonunda "@art ok <hash>" ya da
                "@art err ..." der
    Kimligi ya da <n>'si beklenenle tutmayan "@go"/"@ok" eski bir aktarima aittir ve atilir. Rumy bir aktarim
    hatasindan sonra hat 300 ms susana kadar (en cok 2 sn) gelen baytlari yok sayar.
    Rumy -> PC  @cmd play|next|prev|volup|voldown|seek <ms>   (seek: Spotify ekranindaki cubuktan; Windows'un
                medya oturumuyla konum degistirilir, kopru 3 sn boyunca hedef konumu esas alir)
    PC -> Rumy  @vol <0-100>|-   Spotify'in ses seviyesi (- : Spotify'in ses oturumu yok)
    Rumy -> PC  @cmd pls                      calma listelerini iste (liste ekrani acilinca)
    PC -> Rumy  @pls <surum> <n> hazir|yukleniyor|giris|hata <karisik 0|1> [mesaj], ardindan n satir
                @pl<TAB>sira<TAB>sarki_sayisi<TAB>pafta_hash<TAB>hucre<TAB>ad ve @plnow <sira|-1> (calan liste)
                Kapaklar ART x ART boyunda 3x3 paftalardir (hucre ART/3, kapak ART*5/18 px, hucrenin sol ustunde);
                Rumy paftayi kapak gibi "@need <hash>" ile ister ve ayni onbellege yazar.
    Rumy -> PC  @cmd plplay <surum> <sira>    listeyi bilgisayardaki Spotify'da baslat (Web API)
    PC -> Rumy  @plst <sira> bas|tamam|hata [mesaj]
    Rumy -> PC  @cmd plmix 0|1                karisik calma tercihi (kopru saklar, baslatirken uygular)
    Rumy -> PC  @futbol <0|1> <takim> <renk1> <renk2>   mac takibi acik mi, secili takim (ESPN no, 0: yok) ve
                uygulamadaki iki kulup rengi (000000: yok); baglaninca ve ayar degisince gelir, dakikada bir
                yinelenir. Hizli gol izlemesi ve oyuncu fotograflari bu takima gore yapilir
    Rumy -> PC  @foto <hash> <px> <oyuncu adi>   gol ekrani icin oyuncu fotografi (tools/player_photo.py,
                TheSportsDB); kopru px x px kareyi kapak gibi "@art" ile yollar ya da "@foto yok <hash>" der
    PC -> Rumy  @gol <espn-no> <ev> <dep> <h|a> <g|p|o> <dakika> <oyuncu>  ve  @skor <espn-no> <ev> <dep>
                hizli gol bildirimi: LiveScore'dan ~20 sn'de (ESPN 50-100 sn gec), tools/fast_goals.py
    PC -> Rumy  @ajanlar <surum> <satir> <calisan> <bekleyen> <bosta>, ardindan satir kadar
                @ajan<TAB>sira<TAB>kimlik<TAB>c|b|i<TAB>durum_sn<TAB>is_sn<TAB>proje<TAB>ayrinti
                Claude Code oturumlarinin durumu (ClawLight uyarlamasi), tools/claude_agents.py
    PC -> Rumy  @guncelle <bayt> <sha256> [guncelleyici], Rumy "@guncelle git" der; imaj 2048 baytlik parcalar
                halinde, her parcadan sonra "@guncelle ok <n>", sonunda "@guncelle tamam <elf8>" ya da
                "@guncelle hata ..." (kablosuz guncelleme, tools/fw_update.py)
    Rumy -> PC  @merhaba <kapak> <kamera> <uygulama_elf8> <guncelleyici_elf8>   (WiFi baglantisi kabul edilince)
    Rumy -> PC  @veri afad <yol> | @veri nager <yil>   Rumy'nin bellegi HTTPS'e yetmedigi icin kopru AFAD ve Nager'i
                bilgisayarda ceker; PC -> Rumy  @deprem <ozet> | @bayram <yil> <ay> <gun> <ay> <gun> <0|1>
                (tools/veri_koprusu.py)

Ses: volup/voldown Windows'un ana sesini degil, ses karistiricisindaki Spotify oturumunu %5 degistirir (Spotify'in
kendi ses cubuguyla ayni deger; butun etkin cikislarda Spotify.exe oturumlari). pycaw yoksa Windows'un ses
tuslarina doner. Seviye degisince ve her saniyede bir Rumy'ye bildirilir; her volup/voldown'a tek bir @vol
yaniti gider. Rumy basili tutulan tusta ard arda komut yollar; oturum taramasi ~100 ms surdugu icin adimlarda
oturum listesi VOL_CACHE_S boyunca yeniden kullanilir (adim ~0,2 ms).
    python -m pip install pycaw
    PC -> Rumy  @tempo <vurus_ms> <sonraki_vurusa_ms>   (0 0: tempo bilinmiyor)

Ritim: numpy ve pyaudiowpatch kuruluysa kopru bilgisayarin ses cikisini (WASAPI loopback) dinler, vuruslari
bulur ve tempoyu Rumy'ye bildirir; Rumy kulakligiyla kafasini vuruslara gore sallar. Paketler yoksa ya da
--no-beat verilirse Rumy kendi yavas ritmiyle sallanir.
    python -m pip install numpy pyaudiowpatch

Yazici kamerasi (Creality K2): K2'nin kamerasi yalnizca WebRTC (H.264) verir; ESP32 bunu cozemez. Rumy yazici
ekranini acinca "@cam on <yazici_ip>" gonderir; kopru tools/go2rtc/go2rtc.exe'yi (go2rtc >= 1.9.10, K2 icin
"#format=creality") yalnizca bu bilgisayardan erisilebilir sekilde baslatir, goruntuyu PyAV ile cozer, 160x120
JPEG'e cevirip saniyede bir karta gonderir. Rumy yazici ekranindan cikinca ya da istek 12 sn gelmezse gonderim durur,
go2rtc 60 sn sonra kapatilir.
    python -m pip install av
    go2rtc: https://github.com/AlexxIT/go2rtc/releases (go2rtc_win64.zip) -> tools/go2rtc/go2rtc.exe
    Rumy -> PC  @cam on <ip> | @cam off
    PC -> Rumy  @cam <bayt> <crc32>, Rumy "@go <crc32>" der; JPEG 2048 baytlik parcalar halinde, her parcadan sonra
                "@ok <n>", sonunda "@cam ok" ya da "@cam err ..."
"""
import argparse
import asyncio
import atexit
import base64
import colorsys
import concurrent.futures
import ctypes
import faulthandler
import io
import os
import queue
import secrets
import socket
import subprocess
import sys
import threading
import time
import traceback
import unicodedata
import urllib.request
import zlib

import claude_agents
import fast_goals
import fw_update
import player_photo
import spotify_api
import veri_koprusu
from spotify_api import ApiError, SpotifyApi

try:
    import serial
    from serial.tools import list_ports
    from PIL import Image, ImageDraw
    from winrt.windows.media.control import GlobalSystemMediaTransportControlsSessionManager as MediaManager
    from winrt.windows.storage.streams import Buffer, InputStreamOptions
except ImportError as e:
    sys.exit("eksik paket (%s). Kurulum: python -m pip install pyserial pillow winrt-runtime "
             "winrt-Windows.Media.Control winrt-Windows.Storage.Streams winrt-Windows.Foundation "
             "winrt-Windows.Foundation.Collections" % e.name)

try:
    import numpy as np
    import pyaudiowpatch as pyaudio
except ImportError:
    np = None
    pyaudio = None

try:
    import av
except ImportError:
    av = None

try:
    import psutil
except ImportError:
    psutil = None

ART = 144
RADIUS = 12
BEAT_HOP = 1024
BEAT_WINDOW_S = 8.0
BEAT_LATENCY_S = 0.05
BEAT_BT_EXTRA_S = 0.15
BEAT_MIN_CONF = 0.12
BEAT_MIN_RMS = 0.0015
BEAT_HOLD = 8
CHUNK = 2048
POLL_S = 0.5
SEEK_HOLD_S = 3.0
BEAT_S = 1.0
VOL_STEP = 0.05
VOL_POLL_S = 1.0
VOL_CACHE_S = 2.0
VOL_APP = "spotify.exe"
LIST_FRESH_S = 300.0
LIST_PER_SHEET = 9
LIST_THUMB = 128
LIST_THUMBS_MAX = 200
LIST_NAME_BYTES = 39
LIST_MSG_BYTES = 44
LIST_BURST = 8
PLAYING = 4
VK_VOLUME_DOWN = 0xAE
VK_VOLUME_UP = 0xAF
EXTRA_CHARS = {0x131, 0x152, 0x153, 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026}
APP_NAMES = (("spotify", "Spotify"), ("chrome", "Chrome"), ("msedge", "Edge"), ("firefox", "Firefox"),
             ("opera", "Opera"), ("vlc", "VLC"), ("zunemusic", "Medya Oynatıcı"), ("youtube", "YouTube"))
GO2RTC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "go2rtc", "go2rtc.exe")
CMD_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".cache", "rumy_komut.txt")
SHOT_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".cache", "rumy_ekran.png")
PI_TOKEN_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".cache", "pi_anahtar.txt")
LOG_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".cache", "kopru.log")
LOG_MAX = 512 * 1024
PID_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".cache")
TASK_NAME = "Rumy kopru"
BRIDGE_MUTEX = "Local\\RumyKopru"
WATCH_MUTEX = "Local\\RumyKopruBekci"
EXIT_RUNNING = 3
WATCH_RETRY_S = 5
WATCH_BUSY_S = 60
PHOTO_PX = 84
PHOTO_PREFETCH_DELAY_S = 30
PHOTO_PREFETCH_EVERY_S = 24 * 3600
PI_PORT = 7777
CAM_W = 160
CAM_H = 120
CAM_MAX = 6144
CAM_PERIOD_S = 1.0
CAM_IDLE_S = 12.0
CAM_STOP_S = 60.0
CAM_RTSP_PORT = 18554
CAM_API_PORT = 11984
CREATE_NO_WINDOW = 0x08000000


def allowed(ch):
    cp = ord(ch)
    return 0x20 <= cp <= 0x7E or 0xA0 <= cp <= 0x17F or cp in EXTRA_CHARS


def clean(text, limit):
    out = []
    for ch in (text or "").replace("\t", " ").replace("\r", " ").replace("\n", " "):
        if allowed(ch):
            out.append(ch)
            continue
        base = "".join(c for c in unicodedata.normalize("NFKD", ch) if allowed(c))
        out.append(base)
    s = " ".join("".join(out).split())
    raw = s.encode("utf-8")
    if len(raw) > limit:
        s = raw[:limit - 3].decode("utf-8", "ignore").rstrip() + "…"
    return s


def app_name(model_id):
    low = (model_id or "").lower()
    for key, name in APP_NAMES:
        if key in low:
            return name
    return "Müzik"


def pixels(im):
    get = getattr(im, "get_flattened_data", None)
    return get() if get else im.getdata()


def rounded(im):
    mask = Image.new("L", im.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, im.size[0] - 1, im.size[1] - 1), RADIUS, fill=255)
    out = Image.new("RGB", im.size, (0, 0, 0))
    out.paste(im, (0, 0), mask)
    return out


def accent_of(im):
    small = im.resize((24, 24), Image.BILINEAR)
    best, best_score = None, -1.0
    counts = {}
    for r, g, b in pixels(small):
        key = (r >> 5, g >> 5, b >> 5)
        counts.setdefault(key, [0, 0, 0, 0])
        c = counts[key]
        c[0] += 1
        c[1] += r
        c[2] += g
        c[3] += b
    for n, r, g, b in counts.values():
        r, g, b = r / n / 255.0, g / n / 255.0, b / n / 255.0
        h, s, v = colorsys.rgb_to_hsv(r, g, b)
        score = n * (0.15 + s) * (0.2 + v)
        if s > 0.25 and v > 0.2 and score > best_score:
            best, best_score = (h, s, v), score
    if best is None:
        return 0x1ED760
    h, s, v = best
    r, g, b = colorsys.hsv_to_rgb(h, max(s, 0.65), max(v, 0.9))
    return (int(r * 255) << 16) | (int(g * 255) << 8) | int(b * 255)


def set_art_size(px):
    global ART
    px = max(64, min(256, px))
    if px != ART:
        ART = px
        print("kapak boyutu: %d px" % ART)


def set_cam_size(w, h):
    global CAM_W, CAM_H, CAM_MAX
    w = max(80, min(320, w))
    h = max(60, min(240, h))
    if (w, h) != (CAM_W, CAM_H):
        CAM_MAX = 6144 * w * h // (160 * 120)
        CAM_W, CAM_H = w, h
        print("kamera boyutu: %dx%d" % (CAM_W, CAM_H))


def to_rgb565(im):
    data = bytearray(ART * ART * 2)
    i = 0
    for r, g, b in pixels(im):
        v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        data[i] = v & 0xFF
        data[i + 1] = v >> 8
        i += 2
    return bytes(data)


def rgb565_to_rgb(raw):
    out = bytearray(len(raw) // 2 * 3)
    j = 0
    for i in range(0, len(raw) - 1, 2):
        v = (raw[i] << 8) | raw[i + 1]
        out[j] = ((v >> 11) & 0x1F) * 255 // 31
        out[j + 1] = ((v >> 5) & 0x3F) * 255 // 63
        out[j + 2] = (v & 0x1F) * 255 // 31
        j += 3
    return out


def square(im, px):
    w, h = im.size
    side = min(w, h)
    im = im.crop(((w - side) // 2, (h - side) // 2, (w - side) // 2 + side, (h - side) // 2 + side))
    return im.resize((px, px), Image.LANCZOS)


def fetch_thumb(url):
    try:
        with urllib.request.urlopen(url, timeout=spotify_api.TIMEOUT_S) as r:
            raw = r.read()
        return square(Image.open(io.BytesIO(raw)).convert("RGB"), LIST_THUMB)
    except (OSError, ValueError):
        return None


def liked_cover(px):
    s = px * 4
    im = Image.new("RGB", (s, s))
    d = ImageDraw.Draw(im)
    for y in range(s):
        t = y / s
        d.line([(0, y), (s, y)], fill=(int(72 + 60 * t), int(36 + 40 * t), int(220 - 70 * t)))
    r = s * 0.13
    cx, cy = s / 2, s * 0.44
    white = (255, 255, 255)
    d.ellipse([cx - 2 * r, cy - r, cx, cy + r], fill=white)
    d.ellipse([cx, cy - r, cx + 2 * r, cy + r], fill=white)
    d.polygon([(cx - 1.94 * r, cy + 0.35 * r), (cx + 1.94 * r, cy + 0.35 * r), (cx, cy + 2.3 * r)], fill=white)
    return im.resize((px, px), Image.LANCZOS)


def blank_cover(px):
    im = Image.new("RGB", (px, px), (40, 46, 56))
    d = ImageDraw.Draw(im)
    d.ellipse([px * 0.22, px * 0.22, px * 0.78, px * 0.78], fill=(62, 70, 82))
    d.ellipse([px * 0.42, px * 0.42, px * 0.58, px * 0.58], fill=(40, 46, 56))
    return im


def round_cover(im):
    mask = Image.new("L", (im.size[0] * 4, im.size[1] * 4), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, mask.size[0] - 1, mask.size[1] - 1), max(4, im.size[0] // 3),
                                           fill=255)
    out = Image.new("RGB", im.size, (0, 0, 0))
    out.paste(im, (0, 0), mask.resize(im.size, Image.LANCZOS))
    return out


class Playlists:
    def __init__(self, api):
        self.api = api
        self.pool = concurrent.futures.ThreadPoolExecutor(max_workers=1)
        self.play_pool = concurrent.futures.ThreadPoolExecutor(max_workers=1)
        self.items = []
        self.cells = []
        self.sheets = {}
        self.thumbs = {}
        self.ver = 0
        self.art = 0
        self.at = 0.0
        self.playing = -1
        self.busy = False

    def fresh(self):
        return bool(self.items) and self.art == ART and time.time() - self.at < LIST_FRESH_S

    def build(self, items):
        cover = ART * 5 // 18
        cell = ART // 3
        urls = [it["image"] for it in items if it.get("image") and it["image"] not in self.thumbs]
        if urls:
            with concurrent.futures.ThreadPoolExecutor(max_workers=6) as ex:
                for url, im in zip(urls, ex.map(fetch_thumb, urls)):
                    if im is not None:
                        self.thumbs[url] = im
        if len(self.thumbs) > LIST_THUMBS_MAX:
            keep = {it.get("image") for it in items}
            self.thumbs = {k: v for k, v in self.thumbs.items() if k in keep}
        sheets, cells = {}, []
        for start in range(0, len(items), LIST_PER_SHEET):
            group = items[start:start + LIST_PER_SHEET]
            im = Image.new("RGB", (ART, ART), (0, 0, 0))
            for k, it in enumerate(group):
                src = self.thumbs.get(it.get("image") or "")
                if src is not None:
                    pic = src.resize((cover, cover), Image.LANCZOS)
                elif it["uri"] == spotify_api.LIKED:
                    pic = liked_cover(cover)
                else:
                    pic = blank_cover(cover)
                im.paste(round_cover(pic), ((k % 3) * cell, (k // 3) * cell))
            data = to_rgb565(im)
            h = zlib.crc32(data) or 1
            sheets[h] = (data, 0)
            cells += [(h, k) for k in range(len(group))]
        return sheets, cells

    def refresh(self, full):
        if full:
            items = self.api.playlists()
            sheets, cells = self.build(items)
            sig = "\n".join("%s\t%s\t%d\t%08x\t%d" % (it["name"], it["uri"], it["total"], h, c)
                            for it, (h, c) in zip(items, cells))
            self.items, self.sheets, self.cells = items, sheets, cells
            self.ver = zlib.crc32(sig.encode("utf-8")) or 1
            self.art = ART
            self.at = time.time()
        try:
            ctx = self.api.context()
        except ApiError:
            ctx = None
        if ctx is not None:
            self.playing = next((i for i, it in enumerate(self.items) if it["uri"] == ctx), -1)


def prepare_art(raw):
    im = Image.open(io.BytesIO(raw)).convert("RGB")
    w, h = im.size
    side = min(w, h)
    im = im.crop(((w - side) // 2, (h - side) // 2, (w - side) // 2 + side, (h - side) // 2 + side))
    im = im.resize((ART, ART), Image.LANCZOS)
    return to_rgb565(rounded(im)), accent_of(im)


def press(vk, times=2):
    user32 = ctypes.windll.user32
    for _ in range(times):
        user32.keybd_event(vk, 0, 0, 0)
        user32.keybd_event(vk, 0, 2, 0)


def find_port():
    for p in list_ports.comports():
        desc = "%s %s" % (p.description, p.hwid)
        if "1A86" in desc.upper() or "CH340" in desc.upper() or "CP210" in desc.upper():
            return p.device
    return None


class BeatTracker:
    def __init__(self, rate, offset_s=BEAT_LATENCY_S):
        self.rate = rate
        self.offset = offset_s
        self.fps = rate / BEAT_HOP
        n = int(BEAT_WINDOW_S * self.fps)
        self.env = np.zeros(n)
        self.times = np.zeros(n)
        self.prev = np.zeros(BEAT_HOP)
        self.prev_spec = None
        self.win = np.hanning(BEAT_HOP * 2)
        freqs = np.fft.rfftfreq(BEAT_HOP * 2, 1.0 / rate)
        self.band = freqs < 3000
        self.low = freqs[self.band] < 200
        self.rms = 0.0
        self.lock = threading.Lock()
        self.period = 0.0
        self.next_beat = 0.0
        self.conf = 0.0
        self.misses = 0
        self.candidate = 0.0
        self.votes = 0
        self.ac_avg = None

    def feed(self, x, t_end):
        frame = np.concatenate([self.prev, x])
        self.prev = x
        spec = np.log1p(40.0 * np.abs(np.fft.rfft(frame * self.win))[self.band])
        if self.prev_spec is None:
            self.prev_spec = spec
        diff = np.maximum(spec - self.prev_spec, 0.0)
        self.prev_spec = spec
        self.env = np.roll(self.env, -1)
        self.env[-1] = diff.sum() + 4.0 * diff[self.low].sum()
        self.times = np.roll(self.times, -1)
        self.times[-1] = t_end - len(x) / self.rate
        self.rms = 0.9 * self.rms + 0.1 * float(np.sqrt(np.mean(x * x)))

    def estimate(self, now):
        period, next_beat, conf = self.analyse(now)
        with self.lock:
            self.conf = conf
            if not period:
                self.misses += 1
                if self.misses >= BEAT_HOLD:
                    self.period = 0.0
                return
            self.misses = 0
            if not self.period or abs(period / self.period - 1) < 0.06:
                self.period = period if not self.period else 0.7 * self.period + 0.3 * period
                self.next_beat = next_beat
                self.candidate, self.votes = 0.0, 0
                return
            if self.candidate and abs(period / self.candidate - 1) < 0.06:
                self.votes += 1
            else:
                self.candidate, self.votes = period, 1
            if self.votes >= 3:
                self.period = period
                self.next_beat = next_beat
                self.candidate, self.votes = 0.0, 0

    def analyse(self, now):
        if self.rms < BEAT_MIN_RMS or not self.times[0]:
            return 0.0, 0.0, 0.0
        e = self.env - np.convolve(self.env, np.ones(8) / 8.0, mode="same")
        e = np.maximum(e, 0.0)
        sd = e.std()
        if sd < 1e-9:
            return 0.0, 0.0, 0.0
        e = (e - e.mean()) / sd
        n = len(e)
        ac = np.correlate(e, e, mode="full")[n - 1:]
        ac = ac / ac[0]
        if self.ac_avg is None or len(self.ac_avg) != len(ac):
            self.ac_avg = ac
        else:
            self.ac_avg = 0.75 * self.ac_avg + 0.25 * ac
        ac = self.ac_avg
        lags = np.arange(int(self.fps * 60 / 200), int(self.fps * 60 / 60) + 1)
        bpm = 60.0 * self.fps / lags
        weight = np.exp(-0.5 * (np.log2(bpm / 120.0) / 0.75) ** 2)
        i = int(np.argmax(ac[lags] * weight))
        lag = int(lags[i])
        if bpm[i] < 80 or bpm[i] > 170:
            j = lag // 2 if bpm[i] < 80 else lag * 2
            near = np.arange(max(int(lags[0]), j - 1), min(int(lags[-1]), j + 1) + 1)
            if len(near):
                k = int(near[np.argmax(ac[near])])
                if ac[k] > 0.5 * ac[lag]:
                    lag = k
        i = lag - int(lags[0])
        off = 0.0
        if 0 < i < len(lags) - 1:
            a, b, c = ac[lag - 1], ac[lag], ac[lag + 1]
            den = a - 2 * b + c
            if den:
                off = max(-0.5, min(0.5, 0.5 * (a - c) / den))
        step = lag + off
        conf = float(ac[lag])
        if conf < BEAT_MIN_CONF:
            return 0.0, 0.0, conf
        best_phi, best = 0, -1e9
        span = min(n - 1, int(4 * self.fps))
        for phi in range(int(round(step))):
            idx = np.round(n - 1 - phi - np.arange(0, span - phi, step)).astype(int)
            s = e[idx[idx >= 0]].sum()
            if s > best:
                best, best_phi = s, phi
        period = step / self.fps
        last = self.times[n - 1 - best_phi] + self.offset
        next_beat = last + period * max(1, np.ceil((now - last) / period))
        return period, float(next_beat), conf

    def line(self):
        with self.lock:
            period, next_beat = self.period, self.next_beat
        if not period:
            return "tempo 0 0"
        wait = (next_beat - time.time()) % period
        return "tempo %d %d" % (int(period * 1000), int(wait * 1000))

    def bpm(self):
        with self.lock:
            return 60.0 / self.period if self.period else 0.0


class BeatSource:
    def __init__(self):
        self.tracker = None

    def line(self):
        return self.tracker.line() if self.tracker else "tempo 0 0"

    def bpm(self):
        return self.tracker.bpm() if self.tracker else 0.0


class Loopback:
    def __init__(self, pa, dev):
        self.name = dev["name"].replace(" [Loopback]", "")
        self.rate = int(dev["defaultSampleRate"])
        self.ch = int(dev["maxInputChannels"]) or 2
        self.stream = pa.open(format=pyaudio.paFloat32, channels=self.ch, rate=self.rate, input=True,
                              input_device_index=dev["index"], frames_per_buffer=BEAT_HOP)
        self.level = 0.0
        self.seen = 0.0

    def drain(self, now):
        n = self.stream.get_read_available() // BEAT_HOP
        blocks = []
        for i in range(n):
            data = self.stream.read(BEAT_HOP, exception_on_overflow=False)
            x = np.frombuffer(data, dtype=np.float32).reshape(-1, self.ch).mean(axis=1)
            self.level = 0.9 * self.level + 0.1 * float(np.sqrt(np.mean(x * x)))
            blocks.append((x, now - (n - 1 - i) * BEAT_HOP / self.rate))
        if blocks:
            self.seen = now
        return blocks


def bluetooth_names():
    cmd = ["powershell", "-NoProfile", "-Command",
           "[Console]::OutputEncoding = [Text.Encoding]::UTF8; "
           "Get-PnpDevice -Class Bluetooth -PresentOnly | Select-Object -ExpandProperty FriendlyName"]
    try:
        raw = subprocess.run(cmd, capture_output=True, timeout=20).stdout or b""
    except (OSError, subprocess.SubprocessError):
        return set()
    out = raw.decode("utf-8", "replace")
    return {line.strip() for line in out.splitlines() if line.strip()}


def start_beat_tracker(offset_s):
    if np is None:
        print("ritim takibi kapali (numpy ve pyaudiowpatch kurulu degil)")
        return None
    source = BeatSource()
    bt = bluetooth_names() if offset_s is None else set()

    def offset_for(name):
        if offset_s is not None:
            return offset_s
        wireless = any("(%s)" % n in name for n in bt)
        return BEAT_LATENCY_S + (BEAT_BT_EXTRA_S if wireless else 0.0)

    def run():
        pa = pyaudio.PyAudio()
        outs = []
        for dev in pa.get_loopback_device_info_generator():
            try:
                outs.append(Loopback(pa, dev))
            except OSError:
                pass
        if not outs:
            print("dinlenebilecek ses cikisi yok, ritim takibi kapali")
            return
        print("ritim takibi acik, dinlenen cikislar: %s" % ", ".join(o.name for o in outs))
        active = None
        tracker = None
        last = 0.0
        while True:
            now = time.time()
            fed = False
            for o in outs:
                for x, t in o.drain(now):
                    fed = True
                    if o is active:
                        tracker.feed(x, t)
            if now - last >= 1.0:
                last = now
                live = [o for o in outs if now - o.seen < 0.5 and o.level > BEAT_MIN_RMS]
                best = max(live, key=lambda o: o.level) if live else None
                if active is not None and now - active.seen < 2.0:
                    best = active
                if best is not None and best is not active:
                    active = best
                    offset = offset_for(best.name)
                    tracker = BeatTracker(best.rate, offset)
                    source.tracker = tracker
                    print("ritim kaynagi: %s (gecikme payi %d ms)" % (best.name, offset * 1000))
                if tracker is not None:
                    tracker.estimate(now)
            if not fed:
                time.sleep(0.005)

    def guarded():
        try:
            run()
        except OSError as e:
            print("ses dinleme durdu: %s" % e)
            source.tracker = None

    threading.Thread(target=guarded, daemon=True).start()
    return source


class _IoCounters(ctypes.Structure):
    _fields_ = [(n, ctypes.c_ulonglong) for n in ("read_ops", "write_ops", "other_ops", "read_bytes", "write_bytes",
                                                   "other_bytes")]


class _BasicLimits(ctypes.Structure):
    _fields_ = [("user_time", ctypes.c_longlong), ("job_user_time", ctypes.c_longlong), ("flags", ctypes.c_uint32),
                ("min_ws", ctypes.c_size_t), ("max_ws", ctypes.c_size_t), ("active", ctypes.c_uint32),
                ("affinity", ctypes.c_size_t), ("priority", ctypes.c_uint32), ("scheduling", ctypes.c_uint32)]


class _ExtendedLimits(ctypes.Structure):
    _fields_ = [("basic", _BasicLimits), ("io", _IoCounters), ("process_mem", ctypes.c_size_t),
                ("job_mem", ctypes.c_size_t), ("peak_process_mem", ctypes.c_size_t), ("peak_job_mem", ctypes.c_size_t)]


def kill_with_parent(proc):
    try:
        k32 = ctypes.windll.kernel32
        k32.CreateJobObjectW.restype = ctypes.c_void_p
        job = k32.CreateJobObjectW(None, None)
        if not job:
            return None
        info = _ExtendedLimits()
        info.basic.flags = 0x2000
        k32.SetInformationJobObject.argtypes = (ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32)
        k32.AssignProcessToJobObject.argtypes = (ctypes.c_void_p, ctypes.c_void_p)
        if not k32.SetInformationJobObject(job, 9, ctypes.byref(info), ctypes.sizeof(info)):
            return None
        if not k32.AssignProcessToJobObject(job, int(proc._handle)):
            return None
        return job
    except (AttributeError, OSError):
        return None


class PrinterCamera:
    def __init__(self):
        self.ip = None
        self.asked = 0.0
        self.released = 0.0
        self.proc = None
        self.thread = None
        self.running = False
        self.lock = threading.Lock()
        self.frame = None
        self.frame_seq = 0
        self.sent_seq = 0
        self.last_send = 0.0
        self.warned = False
        self.sent = 0
        self.job = None

    def available(self):
        if av is not None and os.path.exists(GO2RTC):
            return True
        if not self.warned:
            self.warned = True
            print("kamera kullanilamiyor: %s" % ("'python -m pip install av' gerekli" if av is None else
                                                  "tools/go2rtc/go2rtc.exe bulunamadi"))
        return False

    def request(self, ip):
        self.asked = time.time()
        if ip != self.ip:
            self.stop()
            self.ip = ip
        if not self.available():
            return
        if self.proc is None or self.proc.poll() is not None:
            self.start_go2rtc()
        if self.thread is None or not self.thread.is_alive():
            self.running = True
            self.thread = threading.Thread(target=self.grab, daemon=True)
            self.thread.start()

    def release(self):
        if self.asked:
            self.released = time.time()
        self.asked = 0.0

    def wanted(self):
        return self.asked and time.time() - self.asked < CAM_IDLE_S

    def due(self):
        return self.wanted() and time.time() - self.last_send >= CAM_PERIOD_S

    def tick(self):
        if self.asked and not self.wanted():
            self.release()
        if self.proc is not None and not self.asked and time.time() - self.released > CAM_STOP_S:
            self.stop()

    def start_go2rtc(self):
        cfg = os.path.join(os.path.dirname(GO2RTC), "rumy_go2rtc.yaml")
        with open(cfg, "w", encoding="utf-8") as f:
            f.write("api:\n  listen: \"127.0.0.1:%d\"\n" % CAM_API_PORT)
            f.write("rtsp:\n  listen: \"127.0.0.1:%d\"\n" % CAM_RTSP_PORT)
            f.write("webrtc:\n  listen: \"\"\n")
            f.write("streams:\n  k2: \"webrtc:http://%s:8000/call/webrtc_local#format=creality\"\n" % self.ip)
            f.write("log:\n  level: warn\n")
        self.proc = subprocess.Popen([GO2RTC, "-config", cfg], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                                     creationflags=CREATE_NO_WINDOW)
        self.job = kill_with_parent(self.proc)
        print("kamera: go2rtc baslatildi (yazici %s)%s" % (self.ip, "" if self.job else ", iş nesnesine baglanamadi"))

    def grab(self):
        url = "rtsp://127.0.0.1:%d/k2" % CAM_RTSP_PORT
        while self.running:
            try:
                c = av.open(url, options={"rtsp_transport": "tcp"}, timeout=10)
            except Exception as e:
                print("kamera: yayin acilamadi (%s), yeniden deneniyor" % e)
                time.sleep(2)
                continue
            try:
                for frame in c.decode(c.streams.video[0]):
                    with self.lock:
                        self.frame = frame
                        self.frame_seq += 1
                    if not self.running:
                        break
            except Exception as e:
                print("kamera: yayin koptu (%s)" % e)
            finally:
                c.close()
            time.sleep(1)

    def next_jpeg(self):
        with self.lock:
            if self.frame is None or self.frame_seq == self.sent_seq:
                return None
            frame = self.frame
            seq = self.frame_seq
        w = max(CAM_W, round(frame.width * CAM_H / frame.height))
        img = frame.to_image(width=w, height=CAM_H)
        left = (w - CAM_W) // 2
        img = img.crop((left, 0, left + CAM_W, CAM_H))
        for q in (80, 70, 60, 50, 40, 30):
            b = io.BytesIO()
            img.save(b, "JPEG", quality=q)
            if b.tell() <= CAM_MAX:
                self.sent_seq = seq
                return b.getvalue()
        return None

    def stop(self):
        self.running = False
        if self.proc is not None:
            try:
                self.proc.terminate()
                self.proc.wait(timeout=5)
            except Exception:
                pass
            self.proc = None
            print("kamera: go2rtc kapatildi")
        if self.job:
            ctypes.windll.kernel32.CloseHandle.argtypes = (ctypes.c_void_p,)
            ctypes.windll.kernel32.CloseHandle(self.job)
            self.job = None
        if self.thread is not None:
            self.thread.join(timeout=3)
            self.thread = None
        with self.lock:
            self.frame = None


class Link:
    def __init__(self, port, baud, show_log):
        self.port = port
        self.baud = baud
        self.show_log = show_log
        self.ser = None
        self.lines = queue.Queue()
        self.alive = False
        self.elf = None
        self.upd_elf = None

    def open(self):
        port = self.port or find_port()
        if not port:
            return False
        s = serial.Serial()
        s.port = port
        s.baudrate = self.baud
        s.timeout = 0.2
        s.dtr = False
        s.rts = False
        try:
            s.open()
        except serial.SerialException as e:
            print("port acilamadi (%s): %s" % (port, e))
            return False
        self.ser = s
        self.alive = True
        threading.Thread(target=self._reader, daemon=True).start()
        print("Rumy'ye baglanildi: %s @ %d" % (port, self.baud))
        return True

    def _read_chunk(self):
        return self.ser.read(self.ser.in_waiting or 1)

    def _reader(self):
        buf = b""
        while self.alive:
            try:
                chunk = self._read_chunk()
            except (serial.SerialException, OSError):
                self.alive = False
                break
            if chunk is None:
                self.alive = False
                break
            if not chunk:
                continue
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                text = line.decode("utf-8", "replace").rstrip("\r")
                if text.startswith("@"):
                    self.lines.put(text[1:])
                elif self.show_log and text:
                    print("  rumy| " + text)

    def write(self, data):
        try:
            self.ser.write(data)
            return True
        except (serial.SerialException, OSError):
            self.alive = False
            return False

    def close(self):
        self.alive = False
        try:
            self.ser.close()
        except Exception:
            pass

    def wait(self, prefixes, timeout, pending):
        end = time.time() + timeout
        while time.time() < end and self.alive:
            try:
                line = self.lines.get(timeout=0.01)
            except queue.Empty:
                continue
            if any(line.startswith(p) for p in prefixes):
                return line
            pending.append(line)
        return None


class TcpLink(Link):
    def __init__(self, target, token, show_log):
        super().__init__(None, 0, show_log)
        host, _, port = target.partition(":")
        self.host = host
        self.port_no = int(port) if port else PI_PORT
        self.token = token
        self.sock = None

    def open(self):
        try:
            s = socket.create_connection((self.host, self.port_no), timeout=5)
        except OSError as e:
            print("Rumy'ye WiFi ile baglanilamadi (%s:%d): %s" % (self.host, self.port_no, e))
            return False
        s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        s.settimeout(0.2)
        self.sock = s
        self.alive = True
        threading.Thread(target=self._reader, daemon=True).start()
        if self.token:
            self.write(("@anahtar %s\n" % self.token).encode("utf-8"))
            hello = self.wait(("merhaba",), 5, [])
            if not hello:
                print("Rumy baglantiyi kabul etmedi (anahtar yanlis ya da Rumy'de anahtar yok olabilir)")
                self.close()
                return False
            parts = hello.split()
            if len(parts) > 3:
                self.elf = parts[3]
            if len(parts) > 4:
                self.upd_elf = parts[4]
            if len(parts) > 1 and parts[1].isdigit():
                set_art_size(int(parts[1]))
            if len(parts) > 2 and "x" in parts[2]:
                w, _, h = parts[2].partition("x")
                if w.isdigit() and h.isdigit():
                    set_cam_size(int(w), int(h))
        print("Rumy'ye baglanildi: %s:%d (WiFi)" % (self.host, self.port_no))
        return True

    def _read_chunk(self):
        try:
            data = self.sock.recv(4096)
        except socket.timeout:
            return b""
        return data if data else None

    def write(self, data):
        try:
            self.sock.sendall(data)
            return True
        except OSError:
            self.alive = False
            return False

    def close(self):
        self.alive = False
        try:
            self.sock.close()
        except Exception:
            pass


def pi_token(arg):
    if arg:
        return arg
    env = os.environ.get("RUMY_PI_ANAHTAR")
    if env:
        return env.strip()
    try:
        with open(PI_TOKEN_FILE, "r", encoding="utf-8") as f:
            return f.read().strip()
    except OSError:
        return ""


def write_key(port_name, arg):
    token = pi_token(arg)
    if not token:
        token = secrets.token_hex(16)
        os.makedirs(os.path.dirname(PI_TOKEN_FILE), exist_ok=True)
        with open(PI_TOKEN_FILE, "w", encoding="utf-8") as f:
            f.write(token + "\n")
        print("yeni anahtar uretildi: %s" % PI_TOKEN_FILE)
    if len(token) != 32 or any(c not in "0123456789abcdefABCDEF" for c in token):
        print("anahtar 32 onaltilik karakter olmali")
        return False
    port_name = port_name or find_port()
    if not port_name:
        print("Rumy'nin seri portu bulunamadi; USB veri kablosuyla bagla")
        return False
    s = serial.Serial()
    s.port = port_name
    s.baudrate = 460800
    s.timeout = 0.1
    s.dtr = False
    s.rts = False
    try:
        s.open()
    except serial.SerialException as e:
        print("port acilamadi (%s): %s" % (port_name, e))
        return False
    try:
        s.reset_input_buffer()
        s.write(("kopru anahtar %s\n" % token).encode("ascii"))
        end = time.time() + 3.0
        buf = b""
        while time.time() < end:
            buf += s.read(256)
            if b"kopru anahtari" in buf:
                break
    finally:
        s.close()
    ok = b"kopru anahtari kaydedildi" in buf
    print("anahtar Rumy'ye yazildi (%s)" % port_name if ok else "Rumy anahtari kabul etmedi")
    return ok


class AppVolume:
    def __init__(self, exe):
        self.exe = exe
        self.pool = concurrent.futures.ThreadPoolExecutor(max_workers=1)
        self.names = {}
        self.ok = psutil is not None
        self.warned = False
        self.cache = []
        self.cache_at = 0.0

    def is_app(self, pid):
        name = self.names.get(pid)
        if name is None:
            try:
                name = psutil.Process(pid).name().lower()
            except (psutil.Error, OSError):
                name = ""
            self.names[pid] = name
        return name == self.exe

    def controls(self):
        from pycaw.constants import DEVICE_STATE, EDataFlow
        from pycaw.pycaw import AudioUtilities, IAudioSessionControl2, ISimpleAudioVolume
        out = []
        for dev in AudioUtilities.GetAllDevices(EDataFlow.eRender.value, DEVICE_STATE.ACTIVE.value):
            try:
                sessions = dev.AudioSessionManager.GetSessionEnumerator()
                count = sessions.GetCount()
            except Exception:
                continue
            for i in range(count):
                ctl = sessions.GetSession(i)
                if ctl is None:
                    continue
                pid = ctl.QueryInterface(IAudioSessionControl2).GetProcessId()
                if pid and self.is_app(pid):
                    out.append(ctl.QueryInterface(ISimpleAudioVolume))
        return out

    def level(self, ctls):
        if not ctls:
            return None
        if all(c.GetMute() for c in ctls):
            return 0
        return int(round(max(c.GetMasterVolume() for c in ctls) * 100))

    def fresh(self):
        self.cache = self.controls()
        self.cache_at = time.monotonic()
        return self.cache

    def do_read(self):
        return self.level(self.fresh())

    def do_step(self, delta):
        ctls = self.cache if time.monotonic() - self.cache_at < VOL_CACHE_S else self.fresh()
        if not ctls:
            return None
        cur = max(c.GetMasterVolume() for c in ctls)
        new = min(1.0, max(0.0, round((cur + delta) / VOL_STEP) * VOL_STEP))
        for c in ctls:
            c.SetMasterVolume(new, None)
            if delta > 0 and c.GetMute():
                c.SetMute(0, None)
        return self.level(ctls)

    async def call(self, fn, *args):
        if not self.ok:
            return None
        try:
            return await asyncio.get_running_loop().run_in_executor(self.pool, fn, *args)
        except ImportError as e:
            self.ok = False
            print("Spotify sesi icin pycaw yok (%s); Windows'un ses tuslari kullanilacak" % e.name)
        except Exception as e:
            if not self.warned:
                self.warned = True
                print("Spotify sesi okunamadi: %s: %s" % (type(e).__name__, e))
        return None

    async def read(self):
        return await self.call(self.do_read)

    async def step(self, delta):
        return await self.call(self.do_step, delta)


class Bridge:
    def __init__(self, link, any_app, tracker, camera, lists):
        self.link = link
        self.any_app = any_app
        self.tracker = tracker
        self.camera = camera
        self.lists = lists
        self.bpm_shown = 0.0
        self.manager = None
        self.session = None
        self.arts = {}
        self.photos = {}
        self.photo_busy = set()
        self.photo_pool = concurrent.futures.ThreadPoolExecutor(max_workers=1)
        self.tasks = set()
        self.thumb_key = None
        self.art_hash = 0
        self.accent = 0
        self.last_line = None
        self.last_beat = 0.0
        self.pending = []
        self.stale = 0
        self.seek_hold = None
        self.title_seen = None
        self.shot = None
        self.volume = AppVolume(VOL_APP)
        self.vol = None
        self.vol_sent = False
        self.vol_next = 0.0

    async def pick_session(self):
        if self.manager is None:
            self.manager = await MediaManager.request_async()
        sessions = list(self.manager.get_sessions())
        spotify = [s for s in sessions if "spotify" in (s.source_app_user_model_id or "").lower()]
        if spotify:
            return spotify[0]
        if self.any_app:
            return self.manager.get_current_session()
        return None

    async def read_thumb(self, props):
        if not props.thumbnail:
            return None
        stream = await props.thumbnail.open_read_async()
        buf = Buffer(stream.size)
        await stream.read_async(buf, buf.capacity, InputStreamOptions.READ_AHEAD)
        return bytes(buf)

    async def snapshot(self):
        s = await self.pick_session()
        self.session = s
        if s is None:
            return "np\t0\t0\t0\t0\t0\t\t\t\t"
        props = await s.try_get_media_properties_async()
        info = s.get_playback_info()
        tl = s.get_timeline_properties()
        if props is None or info is None or tl is None or tl.end_time is None or tl.position is None:
            return self.last_line or "np\t0\t0\t0\t0\t0\t\t\t\t"
        playing = info.playback_status == PLAYING
        dur = int(tl.end_time.total_seconds() * 1000)
        pos = int(tl.position.total_seconds() * 1000)
        if playing and tl.last_updated_time:
            try:
                age = time.time() - tl.last_updated_time.timestamp()
                if 0 < age < 3600:
                    pos += int(age * 1000)
            except (OverflowError, OSError, ValueError):
                pass
        if self.seek_hold:
            target, t0 = self.seek_hold
            age = time.time() - t0
            want = target + (int(age * 1000) if playing else 0)
            if age > SEEK_HOLD_S or abs(pos - want) < 1500:
                self.seek_hold = None
            else:
                pos = want
        pos = max(0, min(pos, dur)) if dur else max(0, pos)
        title = clean(props.title, 95)
        artist = clean(props.artist, 63)
        album = clean(props.album_title, 63)
        key = (props.title, props.artist, props.album_title)
        if key != self.thumb_key:
            self.thumb_key = key
            self.art_hash = 0
            self.accent = 0
            raw = None
            try:
                raw = await self.read_thumb(props)
            except OSError:
                raw = None
            if raw:
                h = zlib.crc32(raw) or 1
                if h not in self.arts:
                    try:
                        self.arts[h] = prepare_art(raw)
                    except OSError:
                        h = 0
                if h:
                    self.art_hash = h
                    self.accent = self.arts[h][1]
                if len(self.arts) > 64:
                    for old in list(self.arts)[:16]:
                        if old != self.art_hash:
                            del self.arts[old]
            if title and title != self.title_seen:
                self.title_seen = title
                print("calan: %s - %s" % (artist or "?", title))
        return "np\t%d\t%d\t%d\t%x\t%x\t%s\t%s\t%s\t%s" % (1 if playing else 0, pos, dur, self.art_hash, self.accent,
                                                         app_name(s.source_app_user_model_id), title, artist, album)

    def wait_reply(self, want, fails, timeout):
        end = time.time() + timeout
        while True:
            left = end - time.time()
            if left <= 0:
                return None
            r = self.link.wait(("go", "ok ") + fails, left, self.pending)
            if r is None or r in want or r.startswith(fails):
                return r
            self.stale += 1
            print("eski cevap atildi (%d): %s" % (self.stale, r))

    def send_art(self, h):
        entry = self.arts.get(h) or self.lists.sheets.get(h) or self.photos.get(h)
        if entry is None:
            return
        data = entry[0]
        side = entry[2] if len(entry) > 2 else ART
        t0 = time.time()
        self.link.write(("@art %08x %d %d %d %08x\n" % (h, side, side, len(data), zlib.crc32(data))).encode())
        r = self.wait_reply(("go %08x" % h, "go"), ("art ",), 3.0)
        if not r or r.startswith("art err"):
            print("kapak reddedildi: %s" % r)
            return
        if r.startswith("art ok"):
            return
        off = 0
        while off < len(data):
            part = data[off:off + CHUNK]
            if not self.link.write(part):
                return
            off += len(part)
            if off < len(data):
                r = self.wait_reply(("ok %d" % off,), ("art err",), 3.0)
                if not r or r.startswith("art err"):
                    print("kapak aktarimi kesildi: %s" % r)
                    return
        r = self.link.wait(("art ",), 3.0, self.pending)
        print("kapak gonderildi (%d B, %.1f sn): %s" % (len(data), time.time() - t0, r))

    def send_cam(self, data):
        crc = zlib.crc32(data)
        self.link.write(("@cam %d %08x\n" % (len(data), crc)).encode())
        r = self.wait_reply(("go %08x" % crc, "go"), ("cam err", "cam ok"), 2.0)
        if not r or not r.startswith("go"):
            return False
        off = 0
        while off < len(data):
            part = data[off:off + CHUNK]
            if not self.link.write(part):
                return False
            off += len(part)
            if off < len(data):
                r = self.wait_reply(("ok %d" % off,), ("cam err",), 2.0)
                if not r or r.startswith("cam err"):
                    return False
        r = self.link.wait(("cam ok", "cam err"), 2.0, self.pending)
        if r == "cam ok":
            self.camera.sent += 1
            if self.camera.sent == 1:
                print("kamera: ilk kare gonderildi (%d B)" % len(data))
            return True
        return False

    def write_list_head(self, status, msg=""):
        mix = 1 if self.lists.api.shuffle() else 0
        line = "@pls %x 0 %s %d%s\n" % (self.lists.ver, status, mix, (" " + clean(msg, LIST_MSG_BYTES)) if msg else "")
        self.link.write(line.encode("utf-8"))

    async def write_lists(self):
        pl = self.lists
        mix = 1 if pl.api.shuffle() else 0
        if not self.link.write(("@pls %x %d hazir %d\n" % (pl.ver, len(pl.items), mix)).encode("utf-8")):
            return
        for i, (it, (h, cell)) in enumerate(zip(pl.items, pl.cells)):
            line = "@pl\t%d\t%d\t%08x\t%d\t%s\n" % (i, it["total"], h, cell, clean(it["name"], LIST_NAME_BYTES))
            if not self.link.write(line.encode("utf-8")):
                return
            if i % LIST_BURST == LIST_BURST - 1:
                await asyncio.sleep(0.02)
        self.link.write(("@plnow %d\n" % pl.playing).encode("utf-8"))

    async def send_lists(self):
        pl = self.lists
        if not pl.api.linked():
            self.write_list_head("giris")
            return
        cached = bool(pl.items) and pl.art == ART
        if cached:
            await self.write_lists()
        else:
            self.write_list_head("yukleniyor")
        if pl.busy:
            return
        pl.busy = True
        before = (pl.ver, pl.playing)
        t0 = time.time()
        try:
            full = not pl.fresh()
            await asyncio.get_running_loop().run_in_executor(pl.pool, pl.refresh, full)
            if full:
                print("calma listeleri: %d liste, %d pafta (%.1f sn)" % (len(pl.items), len(pl.sheets),
                                                                       time.time() - t0))
        except ApiError as e:
            print("calma listeleri alinamadi: %s" % e)
            if not cached:
                self.write_list_head("giris" if e.reason == "giris" else "hata", e.turkish())
            return
        finally:
            pl.busy = False
        if not cached or (pl.ver, pl.playing) != before:
            await self.write_lists()

    async def play_list(self, args):
        pl = self.lists
        parts = args.split()
        try:
            ver, idx = int(parts[0], 16), int(parts[1])
        except (IndexError, ValueError):
            return
        if not 0 <= idx < 30:
            return
        if ver != pl.ver or idx >= len(pl.items):
            self.link.write(("@plst %d hata Liste değişti, yeniden dene\n" % idx).encode("utf-8"))
            asyncio.ensure_future(self.send_lists())
            return
        item = pl.items[idx]
        self.link.write(("@plst %d bas\n" % idx).encode("utf-8"))
        print("liste baslatiliyor: %s" % item["name"])
        t0 = time.time()
        try:
            await asyncio.get_running_loop().run_in_executor(pl.play_pool, pl.api.play, item, pl.api.shuffle())
        except ApiError as e:
            print("liste baslatilamadi: %s" % e)
            self.link.write(("@plst %d hata %s\n" % (idx, clean(e.turkish(), LIST_MSG_BYTES))).encode("utf-8"))
            return
        pl.playing = idx
        print("liste basladi (%.1f sn)" % (time.time() - t0))
        self.link.write(("@plst %d tamam\n" % idx).encode("utf-8"))

    async def command(self, name):
        print("komut: %s" % name)
        s = self.session
        if name == "pls":
            asyncio.ensure_future(self.send_lists())
        elif name.startswith("plplay "):
            asyncio.ensure_future(self.play_list(name[7:]))
        elif name.startswith("plmix "):
            self.lists.api.set_shuffle(name[6:].strip() == "1")
        elif name in ("volup", "voldown"):
            up = name == "volup"
            v = await self.volume.step(VOL_STEP if up else -VOL_STEP)
            if self.volume.ok:
                if v is None:
                    print("Spotify'in ses oturumu yok (Spotify bir sey calinca olusur)")
                self.set_vol(v, True)
            else:
                press(VK_VOLUME_UP if up else VK_VOLUME_DOWN)
        elif s is not None:
            if name == "play":
                await s.try_toggle_play_pause_async()
            elif name == "next":
                await s.try_skip_next_async()
            elif name == "prev":
                await s.try_skip_previous_async()
            elif name.startswith("seek ") and name[5:].isdigit():
                ms = int(name[5:])
                ok = await s.try_change_playback_position_async(ms * 10000)
                print("konum: %d:%02d %s" % (ms // 60000, ms // 1000 % 60, "tamam" if ok else "reddedildi"))
                if ok:
                    self.seek_hold = (ms, time.time())
        self.last_beat = 0.0

    def set_vol(self, v, force=False):
        if v == self.vol and self.vol_sent and not force:
            return
        self.vol = v
        self.vol_sent = self.link.write(("@vol %s\n" % ("-" if v is None else v)).encode("utf-8"))

    async def poll_vol(self, now):
        if now < self.vol_next or not self.volume.ok:
            return
        self.vol_next = now + VOL_POLL_S
        self.set_vol(await self.volume.read())

    def send_tempo(self, playing):
        if self.tracker is None:
            return
        text = self.tracker.line() if playing else "tempo 0 0"
        self.link.write(("@" + text + "\n").encode())
        bpm = self.tracker.bpm() if playing else 0.0
        if abs(bpm - self.bpm_shown) > 3 or (bpm == 0) != (self.bpm_shown == 0):
            self.bpm_shown = bpm
            print("tempo: %s" % ("%.0f BPM" % bpm if bpm else "yok"))

    async def fetch_photo(self, h, px, name):
        if h in self.photos:
            self.send_art(h)
            return
        if h in self.photo_busy:
            return
        self.photo_busy.add(h)
        data = None
        try:
            data = await asyncio.get_running_loop().run_in_executor(self.photo_pool, player_photo.photo, name, px)
        except Exception as e:
            print("foto alinamadi (%s): %s" % (name, e))
        finally:
            self.photo_busy.discard(h)
        if data is None:
            print("foto yok: %s" % name)
            self.link.write(("@foto yok %08x\n" % h).encode())
            return
        self.photos[h] = (data, 0, px)
        if len(self.photos) > 40:
            for old in list(self.photos)[:10]:
                del self.photos[old]
        self.send_art(h)

    async def handle(self, line):
        if line.startswith("need "):
            try:
                self.send_art(int(line[5:].strip(), 16))
            except ValueError:
                pass
        elif line.startswith("foto "):
            parts = line[5:].split(" ", 2)
            try:
                h, px, name = int(parts[0], 16), int(parts[1]), parts[2].strip()
            except (ValueError, IndexError):
                return
            task = asyncio.create_task(self.fetch_photo(h, px, name))
            self.tasks.add(task)
            task.add_done_callback(self.tasks.discard)
        elif line.startswith("futbol "):
            parts = line[7:].split()
            try:
                on, team = int(parts[0]) != 0, int(parts[1])
            except (ValueError, IndexError):
                return
            colors = [c for c in parts[2:4] if len(c) == 6 and c != "000000"]
            fast_goals.set_team(on, team)
            player_photo.set_team(team if on else 0, *(colors if len(colors) == 2 else ()))
        elif line.startswith("cmd "):
            await self.command(line[4:].strip())
        elif line.startswith("veri "):
            veri_koprusu.request(line[5:])
        elif line.startswith("cam on "):
            self.camera.request(line[7:].strip())
        elif line.startswith("cam off"):
            self.camera.release()
        elif line.startswith("shot "):
            w, h = (int(v) for v in line[5:].split())
            self.shot = (w, h, bytearray(w * h * 2))
        elif line.startswith("px ") and self.shot:
            y, x, b64 = line[3:].split(" ", 2)
            data = base64.b64decode(b64)
            off = (int(y) * self.shot[0] + int(x)) * 2
            self.shot[2][off:off + len(data)] = data
        elif line == "shotend" and self.shot:
            w, h, raw = self.shot
            self.shot = None
            img = Image.frombytes("RGB", (w, h), bytes(rgb565_to_rgb(raw)))
            os.makedirs(os.path.dirname(SHOT_FILE), exist_ok=True)
            img.save(SHOT_FILE)
            print("ekran goruntusu kaydedildi: %s" % SHOT_FILE)

    def update_from_file(self):
        path = fw_update.take_request()
        if path:
            fw_update.run_request(self.link, path, self.pending)

    def commands_from_file(self):
        if not os.path.exists(CMD_FILE):
            return
        try:
            with open(CMD_FILE, "r", encoding="utf-8-sig") as f:
                cmds = [c.strip() for c in f.read().splitlines() if c.strip()]
            os.remove(CMD_FILE)
        except OSError:
            return
        for c in cmds:
            print("komut dosyasindan: %s" % c)
            self.link.write((c + "\n").encode("utf-8"))

    async def run(self):
        while self.link.alive:
            try:
                line = await self.snapshot()
            except OSError as e:
                print("medya oturumu okunamadi: %s" % e)
                line = "np\t0\t0\t0\t0\t0\t\t\t\t"
            except Exception as e:
                print("medya bilgisi beklenmedik bicimde: %s: %s" % (type(e).__name__, e))
                line = self.last_line or "np\t0\t0\t0\t0\t0\t\t\t\t"
            now = time.time()
            if line != self.last_line or now - self.last_beat >= BEAT_S:
                if not self.link.write(("@" + line + "\n").encode("utf-8")):
                    break
                self.last_line = line
                self.last_beat = now
                self.send_tempo(line.startswith("np\t1"))
                self.vol_sent = False
            await self.poll_vol(now)
            self.commands_from_file()
            self.update_from_file()
            end = time.time() + POLL_S
            while time.time() < end and self.link.alive:
                items, self.pending = self.pending, []
                while True:
                    try:
                        items.append(self.link.lines.get_nowait())
                    except queue.Empty:
                        break
                for it in items:
                    await self.handle(it)
                while True:
                    try:
                        goal = fast_goals.OUT.get_nowait()
                    except queue.Empty:
                        break
                    if not self.link.write((goal + "\n").encode("utf-8")):
                        break
                    print("hizli gol -> Rumy: %s" % goal)
                while True:
                    try:
                        line = claude_agents.OUT.get_nowait()
                    except queue.Empty:
                        break
                    if not self.link.write((line + "\n").encode("utf-8")):
                        break
                while True:
                    try:
                        line = veri_koprusu.OUT.get_nowait()
                    except queue.Empty:
                        break
                    if not self.link.write((line + "\n").encode("utf-8")):
                        break
                    print("veri -> Rumy: %s" % line.split("\t")[0])
                self.camera.tick()
                if self.camera.due():
                    self.camera.last_send = time.time()
                    data = self.camera.next_jpeg()
                    if data:
                        self.send_cam(data)
                await asyncio.sleep(0.03)


def hold_mutex(name):
    k = ctypes.windll.kernel32
    k.CreateMutexW.restype = ctypes.c_void_p
    h = k.CreateMutexW(None, False, name)
    if not h or k.GetLastError() == 183:
        return None
    return h


def pid_file(kind):
    return os.path.join(PID_DIR, "%s.pid" % kind)


def write_pid(kind):
    os.makedirs(PID_DIR, exist_ok=True)
    with open(pid_file(kind), "w", encoding="ascii") as f:
        f.write(str(os.getpid()))


def photo_prefetch():
    time.sleep(PHOTO_PREFETCH_DELAY_S)
    while True:
        try:
            rows = player_photo.squad(PHOTO_PX)
            print("kadro fotograflari: %d/%d oyuncu hazir" % (sum(1 for r in rows if r[1] == "var"), len(rows)))
        except Exception as e:
            print("kadro fotograflari hazirlanamadi: %s" % e)
        time.sleep(PHOTO_PREFETCH_EVERY_S)


def stop_all():
    stopped = 0
    for kind in ("bekci", "kopru"):
        try:
            with open(pid_file(kind), "r", encoding="ascii") as f:
                pid = int(f.read().strip())
        except (OSError, ValueError):
            continue
        r = subprocess.run(["taskkill", "/PID", str(pid), "/T", "/F"], capture_output=True, text=True,
                           creationflags=0x08000000)
        if r.returncode == 0:
            stopped += 1
        try:
            os.remove(pid_file(kind))
        except OSError:
            pass
    print("kapatildi" if stopped else "calisan kopru yok")


def watch(child_args):
    if not hold_mutex(WATCH_MUTEX):
        print("bekci zaten calisiyor")
        return
    write_pid("bekci")
    script = os.path.abspath(__file__)
    env = dict(os.environ, RUMY_KOPRU_COCUK="1")
    print("bekci basladi")
    while True:
        started = time.time()
        p = subprocess.Popen([sys.executable, script] + child_args, env=env, cwd=os.path.dirname(script),
                             creationflags=0x08000000)
        code = p.wait()
        if code == EXIT_RUNNING:
            time.sleep(WATCH_BUSY_S)
            continue
        print("kopru kapandi (kod %s, %d sn calisti), %d sn sonra yeniden baslatiliyor"
              % (code, int(time.time() - started), WATCH_RETRY_S))
        time.sleep(WATCH_RETRY_S)


TASK_XML = """<?xml version="1.0" encoding="UTF-16"?>
<Task version="1.2" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">
  <Triggers>
    <TimeTrigger>
      <Repetition><Interval>PT5M</Interval><StopAtDurationEnd>false</StopAtDurationEnd></Repetition>
      <StartBoundary>2026-01-01T00:00:00</StartBoundary>
      <Enabled>true</Enabled>
    </TimeTrigger>
  </Triggers>
  <Principals>
    <Principal id="Author"><UserId>{user}</UserId><LogonType>InteractiveToken</LogonType><RunLevel>LeastPrivilege</RunLevel></Principal>
  </Principals>
  <Settings>
    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>
    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>
    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>
    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>
    <StartWhenAvailable>true</StartWhenAvailable>
    <Enabled>true</Enabled>
  </Settings>
  <Actions Context="Author">
    <Exec><Command>{exe}</Command><Arguments>{args}</Arguments><WorkingDirectory>{cwd}</WorkingDirectory></Exec>
  </Actions>
</Task>
"""


def schedule(on, exe, args):
    flags = 0x08000000
    if not on:
        subprocess.run(["schtasks", "/Delete", "/TN", TASK_NAME, "/F"], capture_output=True, creationflags=flags)
        return True
    from xml.sax.saxutils import escape
    user = "%s\\%s" % (os.environ.get("USERDOMAIN", ""), os.environ.get("USERNAME", ""))
    xml = TASK_XML.format(user=escape(user), exe=escape(exe), args=escape(args),
                          cwd=escape(os.path.dirname(os.path.abspath(__file__))))
    path = os.path.join(PID_DIR, "gorev.xml")
    os.makedirs(PID_DIR, exist_ok=True)
    with open(path, "w", encoding="utf-16") as f:
        f.write(xml)
    r = subprocess.run(["schtasks", "/Create", "/TN", TASK_NAME, "/XML", path, "/F"], capture_output=True, text=True,
                       creationflags=flags)
    try:
        os.remove(path)
    except OSError:
        pass
    if r.returncode != 0:
        print("gorev eklenemedi: %s" % (r.stderr or r.stdout).strip())
        return False
    return True


def startup_path():
    return os.path.join(os.environ.get("APPDATA", ""), "Microsoft", "Windows", "Start Menu", "Programs", "Startup",
                        "rumy_spotify_bridge.cmd")


def autostart(mode, extra):
    path = startup_path()
    if mode == "kapat":
        if os.path.exists(path):
            os.remove(path)
        schedule(False, "", "")
        print("otomatik baslatma ve gorev kaldirildi")
        return
    exe = sys.executable
    pyw = os.path.join(os.path.dirname(exe), "pythonw.exe")
    if os.path.exists(pyw):
        exe = pyw
    script = os.path.abspath(__file__)
    args = " ".join(["--bekci"] + extra)
    with open(path, "w", encoding="utf-8") as f:
        f.write('@start "" "%s" "%s" %s\n' % (exe, script, args))
    print("otomatik baslatma eklendi: %s" % path)
    if schedule(True, exe, '"%s" %s' % (script, args)):
        print("gorev eklendi: '%s' 5 dk'da bir bekciyi denetler" % TASK_NAME)


async def main():
    ap = argparse.ArgumentParser(description="Rumy Spotify koprusu")
    ap.add_argument("--port", help="seri port (varsayilan: CH340'i otomatik bul)")
    ap.add_argument("--pi", "--wifi", dest="pi",
                    help="Rumy'ye (Pi ya da CYD) WiFi ile baglan: adres[:port] (or. 192.168.1.40)")
    ap.add_argument("--anahtar", help="WiFi baglanti anahtari (varsayilan: RUMY_PI_ANAHTAR ya da tools/.cache/pi_anahtar.txt)")
    ap.add_argument("--anahtar-yaz", action="store_true", help="anahtari USB'den CYD'ye yaz ve cik")
    ap.add_argument("--baud", type=int, default=460800)
    ap.add_argument("--any", action="store_true", help="Spotify yoksa diger oynaticilari da goster")
    ap.add_argument("--log", action="store_true", help="Rumy'nin gunluk satirlarini da yazdir")
    ap.add_argument("--autostart", choices=("ac", "kapat"), help="Windows acilisinda otomatik baslatma")
    ap.add_argument("--bekci", action="store_true", help="kopruyu baslat, kapanirsa yeniden ac")
    ap.add_argument("--durdur", action="store_true", help="calisan bekciyi ve kopruyu kapat")
    ap.add_argument("--send", action="append", default=[], help="baglaninca Rumy'ye seri komut gonder (or. \"rapor\")")
    ap.add_argument("--no-beat", action="store_true", help="ses cikisindan ritim takibini kapat")
    ap.add_argument("--spotify-giris", metavar="CLIENT_ID",
                    help="calma listeleri icin Spotify hesabina bir kez giris yap (tarayici acilir) ve cik")
    ap.add_argument("--beat-offset", type=int, default=None,
                    help="kafa sallamayi bu kadar ms geciktir (varsayilan: kablolu 50, Bluetooth 200)")
    ap.add_argument("--guncelle", nargs="?", const="", metavar="BIN",
                    help="yeni yazilimi WiFi'den Rumy'ye yolla (varsayilan dist/Rumy.bin) ve cik")
    ap.add_argument("--zorla", action="store_true", help="--guncelle: Rumy ayni yazilimla calissa da yolla")
    args = ap.parse_args()
    if args.guncelle is not None:
        running = not hold_mutex(BRIDGE_MUTEX)
        token = pi_token(args.anahtar) if args.pi else ""
        if args.pi:
            make = lambda: TcpLink(args.pi, token, args.log)
        else:
            make = lambda: Link(args.port, args.baud, args.log)
        sys.exit(0 if fw_update.cli(args.guncelle or None, args.zorla, running, make) else 1)
    if args.anahtar_yaz:
        write_key(args.port, args.anahtar)
        return
    if args.spotify_giris:
        spotify_api.login(args.spotify_giris)
        return
    if args.durdur:
        stop_all()
        return
    if args.bekci:
        watch([a for a in sys.argv[1:] if a != "--bekci"])
        return
    if args.autostart:
        extra = (["--any"] if args.any else []) + (["--port", args.port] if args.port else [])
        extra += ["--pi", args.pi] if args.pi else []
        extra += ["--no-beat"] if args.no_beat else []
        extra += ["--beat-offset", str(args.beat_offset)] if args.beat_offset is not None else []
        autostart(args.autostart, extra)
        return
    if not hold_mutex(BRIDGE_MUTEX):
        print("kopru zaten calisiyor")
        sys.exit(EXIT_RUNNING)
    write_pid("kopru")
    offset = None if args.beat_offset is None else args.beat_offset / 1000.0
    tracker = None if args.no_beat else start_beat_tracker(offset)
    camera = PrinterCamera()
    atexit.register(camera.stop)
    token = pi_token(args.anahtar) if args.pi else ""
    lists = Playlists(SpotifyApi())
    threading.Thread(target=photo_prefetch, daemon=True).start()
    fast_goals.start()
    claude_agents.start()
    veri_koprusu.start()
    while True:
        link = TcpLink(args.pi, token, args.log) if args.pi else Link(args.port, args.baud, args.log)
        if not link.open():
            time.sleep(3)
            continue
        for cmd in args.send:
            link.write((cmd + "\n").encode("utf-8"))
        fw_update.connected(link.elf, link.upd_elf)
        claude_agents.kick()
        try:
            await Bridge(link, args.any, tracker, camera, lists).run()
        except Exception:
            print("kopru hatasi, yeniden baslatiliyor:")
            traceback.print_exc()
        finally:
            link.close()
            camera.release()
        print("baglanti koptu, yeniden deneniyor...")
        time.sleep(2)


class StampedLog:
    def __init__(self, path):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        try:
            if not os.environ.get("RUMY_KOPRU_COCUK") and os.path.getsize(path) > LOG_MAX:
                os.replace(path, path + ".1")
        except OSError:
            pass
        self.f = open(path, "a", encoding="utf-8", errors="replace", buffering=1)
        self.lock = threading.Lock()
        self.fresh = True

    def write(self, text):
        with self.lock:
            for part in text.splitlines(True):
                if self.fresh:
                    self.f.write(time.strftime("%Y-%m-%d %H:%M:%S "))
                self.f.write(part)
                self.fresh = part.endswith("\n")
        return len(text)

    def flush(self):
        self.f.flush()


if __name__ == "__main__":
    if sys.stdout is None or os.environ.get("RUMY_KOPRU_COCUK"):
        log = StampedLog(LOG_FILE)
        sys.stdout = sys.stderr = log
        faulthandler.enable(file=log.f)
        if "--durdur" not in sys.argv and "--bekci" not in sys.argv:
            print("kopru basladi (pid %d): %s" % (os.getpid(), " ".join(sys.argv[1:])))
    else:
        sys.stdout.reconfigure(errors="replace")
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("kopru kapatildi")
    except SystemExit:
        raise
    except BaseException:
        print("kopru beklenmedik sekilde kapandi:")
        traceback.print_exc()
        raise
