"""Rumy uygulamasini bilgisayarda kendi penceresinde acan "Rumy" kisayolunu kurar ya da kaldirir.

Kisayol Edge'i adres cubugu ve sekmeler olmadan (--app) Rumy'nin adresiyle acar; masaustune ve Baslat menusune
konur, simgesi web/icon.png'den uretilen tools/assets/rumy.ico. Ilk acilista Rumy'deki Ayarlar > Telefon kodu girilir.
    python tools/pc_kisayol.py --kur [--adres http://192.168.1.40/]
    python tools/pc_kisayol.py --kaldir
"""
import argparse
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ICON_PNG = os.path.join(ROOT, "web", "icon.png")
ICON_ICO = os.path.join(ROOT, "tools", "assets", "rumy.ico")
NAME = "Rumy.lnk"
EDGE = [r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
        r"C:\Program Files\Microsoft\Edge\Application\msedge.exe"]

PS_FOLDERS = "[Environment]::GetFolderPath('Desktop'); [Environment]::GetFolderPath('Programs')"
PS_MAKE = ("$s = (New-Object -ComObject WScript.Shell).CreateShortcut($env:RUMY_LNK); $s.TargetPath = $env:RUMY_EXE; "
           "$s.Arguments = $env:RUMY_ARGS; $s.IconLocation = $env:RUMY_ICO + ',0'; "
           "$s.WorkingDirectory = Split-Path $env:RUMY_EXE; $s.Description = 'Rumy ayarlari'; $s.Save()")


def powershell(script, env=None):
    r = subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-Command", script], capture_output=True,
                       text=True, env=env, timeout=30)
    if r.returncode != 0:
        raise SystemExit("PowerShell hatasi: %s" % r.stderr.strip())
    return r.stdout


def folders():
    return [line.strip() for line in powershell(PS_FOLDERS).splitlines() if line.strip()]


def make_icon():
    from PIL import Image
    os.makedirs(os.path.dirname(ICON_ICO), exist_ok=True)
    img = Image.open(ICON_PNG).convert("RGBA")
    img.save(ICON_ICO, sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (180, 180)])


def install(address):
    edge = next((p for p in EDGE if os.path.exists(p)), None)
    if not edge:
        raise SystemExit("Microsoft Edge bulunamadi")
    if not os.path.exists(ICON_ICO):
        make_icon()
    args = "--app=%s --window-size=1180,800" % address
    for folder in folders():
        path = os.path.join(folder, NAME)
        env = dict(os.environ, RUMY_LNK=path, RUMY_EXE=edge, RUMY_ARGS=args, RUMY_ICO=ICON_ICO)
        powershell(PS_MAKE, env)
        print("kuruldu: %s" % path)


def remove():
    for folder in folders():
        path = os.path.join(folder, NAME)
        if os.path.exists(path):
            os.remove(path)
            print("kaldirildi: %s" % path)


def main():
    ap = argparse.ArgumentParser(description="Rumy bilgisayar kisayolu")
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--kur", action="store_true")
    g.add_argument("--kaldir", action="store_true")
    ap.add_argument("--adres", default="http://rumy.local/")
    a = ap.parse_args()
    if a.kur:
        if not a.adres.startswith("http://"):
            raise SystemExit("adres http:// ile baslamali")
        install(a.adres)
    else:
        remove()


if __name__ == "__main__":
    sys.stdout.reconfigure(errors="replace")
    main()
