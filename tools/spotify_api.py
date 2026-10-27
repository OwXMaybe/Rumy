"""Spotify Web API istemcisi (Rumy koprusu icin): bir kez PKCE ile giris, calma listeleri, listeyi baslatma.

Kurulum (bir kez):
    1. https://developer.spotify.com/dashboard -> Create app; Redirect URI: http://127.0.0.1:8888/callback,
       API: Web API. Gelistirme modundaki uygulamalar icin hesap sahibinin Premium olmasi gerekir (Mart 2026'dan beri);
       listeyi baslatma ucu zaten yalnizca Premium'da calisir.
    2. python tools/spotify_bridge.py --spotify-giris <client_id>
       Tarayici acilir; izin verilince client_id ve yenileme anahtari tools/.cache/spotify_api.json dosyasina yazilir
       (git'e girmez, anahtarlar ekrana yazilmaz). Client secret gerekmez.
Kullanilan uclar: GET /me, GET /me/playlists, GET /me/tracks?limit=1 (Begenilen Sarkilar sayisi), GET /me/player,
GET /me/player/devices, PUT /me/player/play, PUT /me/player/shuffle. Her istekte 8 sn zaman asimi var.
"""
import base64
import hashlib
import http.server
import json
import os
import random
import secrets
import socket
import time
import urllib.error
import urllib.parse
import urllib.request
import webbrowser

ACCOUNTS = "https://accounts.spotify.com"
API = "https://api.spotify.com/v1"
REDIRECT_PORT = 8888
REDIRECT = "http://127.0.0.1:%d/callback" % REDIRECT_PORT
SCOPES = ("playlist-read-private playlist-read-collaborative user-library-read user-read-playback-state "
          "user-modify-playback-state")
STORE = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".cache", "spotify_api.json")
TIMEOUT_S = 8.0
LOGIN_WAIT_S = 900
MAX_LISTS = 30
DEVICE_WAIT_S = 15.0
LIKED = "liked"
LIKED_NAME = "Beğenilen Şarkılar"

MESSAGES = {
    "PREMIUM_REQUIRED": "Spotify Premium gerekli",
    "NO_ACTIVE_DEVICE": "Spotify cihazı bulunamadı",
    "RATE_LIMITED": "Spotify meşgul, biraz sonra dene",
    "giris": "Spotify hesabı bağlı değil",
    "ag": "Spotify'a ulaşılamadı",
    "cihaz": "Bilgisayarda Spotify açılamadı",
}


class ApiError(Exception):
    def __init__(self, status, reason, message=""):
        super().__init__("%s %s %s" % (status, reason, message))
        self.status = status
        self.reason = reason
        self.message = message

    def turkish(self):
        if self.reason in MESSAGES:
            return MESSAGES[self.reason]
        if self.status == 403:
            return "Spotify izin vermedi (403)"
        if self.status == 404:
            return "Liste ya da cihaz bulunamadı"
        return "Spotify hatası (%s)" % (self.status or self.reason)


def error_of(e):
    try:
        body = json.loads(e.read().decode("utf-8", "replace") or "{}")
    except (OSError, ValueError):
        body = {}
    err = body.get("error")
    if isinstance(err, dict):
        reason = err.get("reason") or ""
        if e.code == 429:
            reason = "RATE_LIMITED"
        return ApiError(e.code, reason, err.get("message") or "")
    return ApiError(e.code, "RATE_LIMITED" if e.code == 429 else str(err or ""), body.get("error_description") or "")


def pick_image(images):
    sized = [i for i in images or [] if i.get("url") and i.get("width")]
    big = [i for i in sized if i["width"] >= 60]
    if big:
        return min(big, key=lambda i: i["width"])["url"]
    plain = [i for i in images or [] if i.get("url")]
    return plain[0]["url"] if plain else None


def launch_spotify():
    try:
        os.startfile("spotify:")
        return True
    except OSError:
        return False


class SpotifyApi:
    def __init__(self, path=STORE):
        self.path = path
        self.data = {}
        self.access = None
        self.expires = 0.0
        self.user_id = None
        self.load()

    def load(self):
        try:
            with open(self.path, encoding="utf-8") as f:
                self.data = json.load(f)
        except (OSError, ValueError):
            self.data = {}

    def save(self):
        os.makedirs(os.path.dirname(self.path), exist_ok=True)
        tmp = self.path + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(self.data, f)
        os.replace(tmp, self.path)

    def linked(self):
        if not (self.data.get("client_id") and self.data.get("refresh_token")):
            self.load()
        return bool(self.data.get("client_id") and self.data.get("refresh_token"))

    def shuffle(self):
        return bool(self.data.get("shuffle", True))

    def set_shuffle(self, on):
        self.data["shuffle"] = bool(on)
        self.save()

    def token(self, form):
        req = urllib.request.Request(ACCOUNTS + "/api/token", data=urllib.parse.urlencode(form).encode("ascii"),
                                     headers={"Content-Type": "application/x-www-form-urlencoded"})
        try:
            with urllib.request.urlopen(req, timeout=TIMEOUT_S) as r:
                tok = json.loads(r.read().decode("utf-8"))
        except urllib.error.HTTPError as e:
            err = error_of(e)
            if e.code == 400:
                self.data["refresh_token"] = ""
                self.save()
                raise ApiError(400, "giris", err.message)
            raise err
        except (urllib.error.URLError, OSError, ValueError) as e:
            raise ApiError(0, "ag", str(e))
        self.access = tok["access_token"]
        self.expires = time.time() + int(tok.get("expires_in", 3600)) - 60
        if tok.get("refresh_token"):
            self.data["refresh_token"] = tok["refresh_token"]
            self.save()

    def call(self, method, path, params=None, body=None, retry=True):
        if not self.linked():
            raise ApiError(0, "giris")
        if not self.access or time.time() >= self.expires:
            self.token({"grant_type": "refresh_token", "refresh_token": self.data["refresh_token"],
                        "client_id": self.data["client_id"]})
        url = API + path + ("?" + urllib.parse.urlencode(params) if params else "")
        headers = {"Authorization": "Bearer " + self.access}
        data = None
        if body is not None:
            data = json.dumps(body).encode("utf-8")
            headers["Content-Type"] = "application/json"
        elif method == "PUT":
            data = b""
        req = urllib.request.Request(url, data=data, method=method, headers=headers)
        try:
            with urllib.request.urlopen(req, timeout=TIMEOUT_S) as r:
                raw = r.read()
        except urllib.error.HTTPError as e:
            if e.code == 401 and retry:
                self.access = None
                return self.call(method, path, params, body, False)
            raise error_of(e)
        except (urllib.error.URLError, OSError) as e:
            raise ApiError(0, "ag", str(e))
        try:
            return json.loads(raw.decode("utf-8")) if raw.strip() else None
        except ValueError:
            return None

    def me(self):
        info = self.call("GET", "/me") or {}
        self.user_id = info.get("id")
        return info

    def playlists(self):
        out = []
        try:
            liked = self.call("GET", "/me/tracks", {"limit": 1}) or {}
            out.append({"name": LIKED_NAME, "uri": LIKED, "total": int(liked.get("total") or 0), "image": None})
        except ApiError as e:
            if e.reason in ("giris", "ag"):
                raise
        offset = 0
        while len(out) < MAX_LISTS:
            page = self.call("GET", "/me/playlists", {"limit": 50, "offset": offset}) or {}
            for p in page.get("items") or []:
                if not p or not p.get("uri"):
                    continue
                count = (p.get("items") or p.get("tracks") or {}).get("total") or 0
                out.append({"name": p.get("name") or "?", "uri": p["uri"], "total": int(count),
                            "image": pick_image(p.get("images"))})
            if not page.get("next"):
                break
            offset += 50
        return out[:MAX_LISTS]

    def context(self):
        state = self.call("GET", "/me/player") or {}
        ctx = state.get("context") or {}
        uri = ctx.get("uri") or ""
        if uri.endswith(":collection"):
            return LIKED
        return uri

    def device(self):
        devices = (self.call("GET", "/me/player/devices") or {}).get("devices") or []
        usable = [d for d in devices if d.get("id") and not d.get("is_restricted")]
        host = socket.gethostname().lower()
        tests = (lambda d: d.get("type") == "Computer" and (d.get("name") or "").lower() == host,
                 lambda d: d.get("type") == "Computer" and d.get("is_active"),
                 lambda d: d.get("type") == "Computer",
                 lambda d: d.get("is_active"))
        for test in tests:
            for d in usable:
                if test(d):
                    return d["id"]
        return None

    def wait_device(self):
        dev = self.device()
        if dev or not launch_spotify():
            return dev
        end = time.time() + DEVICE_WAIT_S
        while dev is None and time.time() < end:
            time.sleep(1.0)
            dev = self.device()
        return dev

    def play(self, item, shuffle):
        dev = self.wait_device()
        if dev is None:
            raise ApiError(0, "cihaz")
        uri = item["uri"]
        if uri == LIKED:
            if not self.user_id:
                self.me()
            uri = "spotify:user:%s:collection" % self.user_id
        body = {"context_uri": uri}
        total = int(item.get("total") or 0)
        if shuffle and total > 1:
            body["offset"] = {"position": random.randrange(min(total, 10000))}
        self.call("PUT", "/me/player/play", {"device_id": dev}, body)
        try:
            self.call("PUT", "/me/player/shuffle", {"state": "true" if shuffle else "false", "device_id": dev})
        except ApiError:
            pass


def login(client_id, path=STORE):
    client_id = (client_id or "").strip()
    if not client_id or not client_id.isalnum():
        raise SystemExit("client_id gecersiz: Spotify gelistirici panelindeki Client ID'yi verin")
    verifier = secrets.token_urlsafe(64)
    challenge = base64.urlsafe_b64encode(hashlib.sha256(verifier.encode("ascii")).digest()).rstrip(b"=").decode()
    state = secrets.token_urlsafe(16)
    result = {}

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            url = urllib.parse.urlparse(self.path)
            if url.path != "/callback":
                self.send_response(404)
                self.end_headers()
                return
            result.update({k: v[0] for k, v in urllib.parse.parse_qs(url.query).items()})
            ok = result.get("state") == state and "code" in result
            text = ("Rumy: Spotify girisi tamam, bu sekmeyi kapatabilirsin." if ok
                    else "Rumy: Spotify girisi olmadi (%s)." % (result.get("error") or "gecersiz yanit"))
            body = ("<!doctype html><meta charset=utf-8><title>Rumy</title><p style='font:18px sans-serif'>%s</p>"
                    % text).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, *args):
            pass

    try:
        server = http.server.HTTPServer(("127.0.0.1", REDIRECT_PORT), Handler)
    except OSError as e:
        raise SystemExit("127.0.0.1:%d acilamadi (%s); baska bir program kullaniyor olabilir" % (REDIRECT_PORT, e))
    server.timeout = 1.0
    url = ACCOUNTS + "/authorize?" + urllib.parse.urlencode({
        "client_id": client_id, "response_type": "code", "redirect_uri": REDIRECT, "scope": SCOPES,
        "code_challenge_method": "S256", "code_challenge": challenge, "state": state})
    print("Tarayicida Spotify izin sayfasi aciliyor. Acilmazsa bu adresi ac:\n%s" % url)
    webbrowser.open(url)
    end = time.time() + LOGIN_WAIT_S
    try:
        while "state" not in result and "error" not in result and time.time() < end:
            server.handle_request()
    finally:
        server.server_close()
    if result.get("state") != state or "code" not in result:
        raise SystemExit("giris olmadi: %s" % (result.get("error") or "zaman asimi ya da gecersiz yanit"))
    api = SpotifyApi(path)
    api.data["client_id"] = client_id
    api.data["refresh_token"] = ""
    api.token({"grant_type": "authorization_code", "code": result["code"], "redirect_uri": REDIRECT,
               "client_id": client_id, "code_verifier": verifier})
    if not api.linked():
        raise SystemExit("giris olmadi: Spotify yenileme anahtari vermedi")
    try:
        info = api.me()
        lists = api.playlists()
    except ApiError as e:
        print("giris kaydedildi ama deneme istegi olmadi: %s" % e.turkish())
        return
    print("giris tamam: %s (%s) · %d liste" % (info.get("display_name") or info.get("id"),
                                              info.get("product") or "?", len(lists)))
