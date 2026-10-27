"""Sahte WiZ ampul: gercek ampul olmadan Rumy'nin isik kontrolunu test etmek icin.

Bilgisayarin, Rumy ile ayni yerel agda olmasi gerekir. UDP 38899'u dinler ve WiZ protokolunun
Rumy'nin kullandigi kismini taklit eder (registration, getPilot, setPilot).

Kullanim:
    python tools/fake_bulb.py
    python tools/fake_bulb.py --mac a8bb50112233 --drop 20 --delay 150
Secenekler:
    --mac     ampulun MAC adresi (12 hex hane)
    --drop    yanitlarin yuzde kaci dusurulsun (cevrimdisi algilamayi denemek icin)
    --delay   her yanittan once bekleme (ms)
    --quiet   her paketi yazdirma, sadece ozet
Cikis: Ctrl+C. Cikista komut sikligi ozeti yazdirilir (80 ms kisitlamasini dogrulamak icin).
"""
import argparse
import json
import random
import socket
import sys
import time

PORT = 38899


def color_block(state):
    if not state["state"]:
        return "KAPALI"
    if "r" in state:
        r, g, b = state["r"], state["g"], state["b"]
        return "\x1b[48;2;%d;%d;%dm      \x1b[0m RGB(%d,%d,%d) %%%d" % (r, g, b, r, g, b, state["dimming"])
    if state.get("sceneId"):
        return "sahne %d %%%d" % (state["sceneId"], state["dimming"])
    return "%d K %%%d" % (state.get("temp", 2700), state["dimming"])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mac", default="a8bb50" + "%06x" % random.randrange(1 << 24))
    ap.add_argument("--drop", type=int, default=0)
    ap.add_argument("--delay", type=int, default=0)
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()
    mac = args.mac.lower().replace(":", "")
    if len(mac) != 12:
        sys.exit("MAC 12 hex hane olmali")

    state = {"state": True, "dimming": 60, "temp": 2700, "sceneId": 0}
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("0.0.0.0", PORT))
    print("Sahte WiZ ampul hazir: MAC %s, UDP %d" % (mac, PORT))
    print("Rumy'de Ayarlar > Ampul > Yeniden ara ile bulunabilir.\n")

    set_times = []
    counts = {}
    try:
        while True:
            data, addr = sock.recvfrom(2048)
            now = time.time()
            try:
                msg = json.loads(data.decode("utf-8"))
            except ValueError:
                continue
            method = msg.get("method", "")
            counts[method] = counts.get(method, 0) + 1
            params = msg.get("params", {}) or {}
            if method == "registration":
                reply = {"method": "registration", "env": "pro", "result": {"mac": mac, "success": True}}
            elif method == "getPilot":
                res = {"mac": mac, "rssi": -52, "src": ""}
                res.update(state)
                reply = {"method": "getPilot", "env": "pro", "result": res}
            elif method == "setPilot":
                set_times.append(now)
                if "state" in params:
                    state["state"] = bool(params["state"])
                if "dimming" in params:
                    state["dimming"] = int(params["dimming"])
                if "temp" in params:
                    state = {k: v for k, v in state.items() if k not in ("r", "g", "b")}
                    state["temp"] = int(params["temp"])
                    state["sceneId"] = 0
                if "r" in params:
                    state.pop("temp", None)
                    state["sceneId"] = 0
                    state["r"], state["g"], state["b"] = int(params["r"]), int(params.get("g", 0)), int(params.get("b", 0))
                if "sceneId" in params:
                    state = {k: v for k, v in state.items() if k not in ("r", "g", "b")}
                    state["sceneId"] = int(params["sceneId"])
                reply = {"method": "setPilot", "env": "pro", "result": {"success": True}}
            else:
                reply = {"method": method, "env": "pro", "error": {"code": -32601, "message": "Method not found"}}

            if not args.quiet:
                gap = ""
                if method == "setPilot" and len(set_times) > 1:
                    gap = " (+%d ms)" % round((set_times[-1] - set_times[-2]) * 1000)
                print("%s %-12s %s%s  ->  %s" % (time.strftime("%H:%M:%S"), method, addr[0], gap, color_block(state)))
            if args.drop and random.randrange(100) < args.drop:
                continue
            if args.delay:
                time.sleep(args.delay / 1000.0)
            sock.sendto(json.dumps(reply, separators=(",", ":")).encode(), addr)
    except KeyboardInterrupt:
        pass
    print("\nOzet:", ", ".join("%s=%d" % kv for kv in sorted(counts.items())))
    if len(set_times) > 1:
        gaps = [(b - a) * 1000 for a, b in zip(set_times, set_times[1:])]
        short = [g for g in gaps if g < 75]
        print("setPilot araliklari: en kisa %.0f ms, ortalama %.0f ms, 75 ms'den kisa %d adet" % (
            min(gaps), sum(gaps) / len(gaps), len(short)))


if __name__ == "__main__":
    main()
