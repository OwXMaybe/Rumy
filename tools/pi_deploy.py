"""Rumy'nin Raspberry Pi surumunu Pi'ye gonderir, orada derler ve istenirse yeniden baslatir.

Kaynaklar (src, lib/rumy_core, include, pi, platformio.ini) tar olarak SSH ile Pi'deki ~/rumy altina acilir.
--deps ile LVGL, ArduinoJson ve TJpgDec de .pio/libdeps/cyd altindan pi-deps klasorune gonderilir (ilk kurulumda).
--sd ile sd_card/ icerigi Pi'deki veri klasorune kopyalanir; Pi'de zaten olan dosyalarin ustune yazilmaz.

Kullanim:
    python tools/pi_deploy.py [--host rumy-pi.local] [--deps] [--sd] [--no-build] [--service] [--restart]
    python tools/pi_deploy.py --geri    (bir onceki ikiliye doner ve Rumy'yi yeniden baslatir; tekrari ileri alir)
Her derlemede calisan ikili pi-build/rumy.onceki olarak saklanir.
Adres sirasi: --host, RUMY_PI_HOST ortam degiskeni, rumy-pi.local. Kullanici: RUMY_PI_USER (varsayilan rumy).
Gerekenler: ~/.ssh/id_ed25519 anahtari Pi'de yetkili olmali; --deps icin en az bir kez 'pio run -e cyd'.
"""
import argparse
import io
import os
import subprocess
import sys
import tarfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIBDEPS = os.path.join(ROOT, ".pio", "libdeps", "cyd")
SKIP_DIRS = {"__pycache__", ".git"}


def ssh_base(host):
    key = os.path.join(os.path.expanduser("~"), ".ssh", "id_ed25519")
    user = os.environ.get("RUMY_PI_USER", "rumy")
    return ["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=15", "-o", "ServerAliveInterval=15",
            "-o", "ServerAliveCountMax=4", "-o", "HostKeyAlias=rumy-pi.local", "-i", key, "%s@%s" % (user, host)]


def add_tree(tar, src, arc, skip=()):
    for base, dirs, files in os.walk(src):
        rel = os.path.relpath(base, src).replace("\\", "/")
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS and not any(
            (rel + "/" + d).lstrip("./").startswith(s) for s in skip)]
        for name in files:
            full = os.path.join(base, name)
            inner = os.path.relpath(full, src).replace("\\", "/")
            tar.add(full, arcname=arc + "/" + inner)


def pack(deps, sd):
    buf = io.BytesIO()
    with tarfile.open(fileobj=buf, mode="w:gz") as tar:
        if sd:
            add_tree(tar, os.path.join(ROOT, "sd_card", "rumy"), "rumy")
        else:
            tar.add(os.path.join(ROOT, "platformio.ini"), arcname="platformio.ini")
            for d in ("src", "include", "pi"):
                add_tree(tar, os.path.join(ROOT, d), d)
            add_tree(tar, os.path.join(ROOT, "lib", "rumy_core", "src"), "lib/rumy_core/src")
            if deps:
                lv = os.path.join(LIBDEPS, "lvgl")
                if not os.path.isdir(lv):
                    sys.exit("LVGL kaynaklari yok: once 'pio run -e cyd' calistirin")
                for name in ("lvgl.h", "lv_version.h", "lvgl_private.h"):
                    tar.add(os.path.join(lv, name), arcname="pi-deps/lvgl/" + name)
                add_tree(tar, os.path.join(lv, "src"), "pi-deps/lvgl/src", ("libs/thorvg", "others/vg_lite_tvg"))
                add_tree(tar, os.path.join(LIBDEPS, "ArduinoJson", "src"), "pi-deps/ArduinoJson/src")
                util = os.path.join(LIBDEPS, "LovyanGFX", "src", "lgfx", "utility")
                for name in ("lgfx_tjpgd.c", "lgfx_tjpgd.h"):
                    tar.add(os.path.join(util, name), arcname="pi-deps/tjpgd/" + name)
    return buf.getvalue()


def run(cmd, data=None):
    r = subprocess.run(cmd, input=data)
    return r.returncode


def output(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
    return r.stdout if r.returncode == 0 else None


def build(base):
    start = ("cd ~/rumy/pi && rm -f ~/rumy/derleme.kod && find ~/rumy/pi-build -name '*.tmp' -delete 2>/dev/null; "
             "(setsid nohup sh -c 'make -j2 > ~/rumy/derleme.log 2>&1; echo $? > ~/rumy/derleme.kod' "
             "< /dev/null > /dev/null 2>&1 &)")
    if run(base + [start]) != 0:
        sys.exit("derleme baslatilamadi")
    print("pi_deploy: Pi'de derleniyor (baglanti koparsa derleme surer)")
    deadline = time.time() + 3 * 3600
    while time.time() < deadline:
        time.sleep(10)
        st = output(base + ["cat ~/rumy/derleme.kod 2>/dev/null || (pgrep -x make >/dev/null && echo yok || echo dur)"])
        if st is None:
            continue
        st = st.strip()
        if st == "yok":
            continue
        if st == "dur":
            sys.exit("derleme yarida kaldi (Pi yeniden basladi mi?); yeniden calistirin, bitmis dosyalar korunur")
        log = output(base + ["grep -E 'error|warning|Error' ~/rumy/derleme.log | head -n 30"]) or ""
        if log.strip():
            print(log.rstrip())
        if st != "0":
            sys.exit("derleme hatasi (kod %s), ayrinti: ~/rumy/derleme.log" % st)
        print("pi_deploy: derleme tamam")
        return
    sys.exit("derleme 3 saatte bitmedi")


RESTART = ("if systemctl is-active -q rumy.service 2>/dev/null; then pkill -x rumy; "
           "elif loginctl show-user $USER -p Linger 2>/dev/null | grep -q yes; then "
           "systemctl --user restart rumy; else pkill -x rumy; sleep 1; "
           "setsid nohup ~/rumy/pi-build/rumy >> ~/rumy/rumy.log 2>&1 < /dev/null & fi")


def rollback(base):
    swap = ("cd ~/rumy/pi-build && [ -f rumy.onceki ] || { echo 'onceki surum yok'; exit 3; }; "
            "if [ \"$(systemctl show rumy.service -p WatchdogUSec --value 2>/dev/null || echo 0)\" != 0 ] && "
            "! grep -q 'systemd bekcisi' rumy.onceki; then echo 'onceki surum bekciyi desteklemiyor; "
            "once: sudo bash ~/rumy/pi/kurulum3.sh geri'; exit 4; fi; "
            "ln -f rumy rumy.takas && mv -f rumy.onceki rumy && mv -f rumy.takas rumy.onceki && "
            "ls -l --time-style=+%Y-%m-%d_%H:%M rumy rumy.onceki | awk '{print $6, $7}'")
    if run(base + [swap]) != 0:
        sys.exit("geri alinamadi")
    if run(base + [RESTART]) != 0:
        sys.exit("yeniden baslatilamadi")
    print("pi_deploy: onceki surume donuldu ve Rumy yeniden baslatildi (ileri almak icin yine --geri)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--geri", action="store_true")
    ap.add_argument("--host", default=os.environ.get("RUMY_PI_HOST", "rumy-pi.local"))
    ap.add_argument("--deps", action="store_true")
    ap.add_argument("--sd", action="store_true")
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--restart", action="store_true")
    ap.add_argument("--service", action="store_true")
    a = ap.parse_args()
    base = ssh_base(a.host)
    if a.geri:
        rollback(base)
        return
    subprocess.run([sys.executable, os.path.join(ROOT, "tools", "build_web.py")], check=True)
    data = pack(a.deps, False)
    print("pi_deploy: %d KB gonderiliyor -> %s" % (len(data) // 1024, a.host))
    if run(base + ["mkdir -p ~/rumy && tar -xzf - -C ~/rumy"], data) != 0:
        sys.exit("gonderilemedi")
    if a.sd:
        sd = pack(False, True)
        if run(base + ["d=~/.local/share/rumy/sd && mkdir -p $d && tar -xzf - --skip-old-files -C $d"], sd) != 0:
            sys.exit("sd icerigi gonderilemedi")
    if not a.no_build:
        build(base)
    if a.service:
        if run(base + ["if [ -f /etc/systemd/system/rumy.service ]; then "
                       "echo 'pi_deploy: sistem servisi kurulu (kurulum2.sh), kullanici servisi atlandi'; else "
                       "mkdir -p ~/.config/systemd/user && cp ~/rumy/pi/rumy.service ~/.config/systemd/user/ && "
                       "systemctl --user daemon-reload && systemctl --user enable rumy && "
                       "echo 'pi_deploy: rumy.service kuruldu ve acilista baslayacak (linger acikken)'; fi"]) != 0:
            sys.exit("servis kurulamadi")
    if a.restart:
        if run(base + [RESTART]) != 0:
            sys.exit("yeniden baslatilamadi")
        print("pi_deploy: Rumy yeniden baslatildi")


if __name__ == "__main__":
    main()
