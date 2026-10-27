"""Rumy arayuzunu kart olmadan bilgisayarda cizer ve ekran goruntulerini PNG olarak kaydeder.

Gercek LVGL 9.5, gercek ekran kodu (src/ui, src/rumy), gercek SD ikonlari (sd_card/) ve test verileri kullanilir;
yalnizca donanima dokunan moduller (WiFi, WiZ, SD, saat, ekran surucusu) sim/sim_stubs.cpp ile taklit edilir.
Boylece yerlesim hatalari, tasan yazilar ve Rumy'nin ifadeleri kart gelmeden gorulebilir.

Kullanim:
    python tools/ui_preview.py            CYD (320x240)
    python tools/ui_preview.py --buyuk    Raspberry Pi (480x320) -> docs/preview_buyuk/, docs/ui_preview_buyuk.png
    python tools/ui_preview.py --tarih    ana ekran tarih senaryolari -> docs/preview_tarih/ (--buyuk ile birlikte de olur)
    python tools/ui_preview.py --cesit    uc durumlar (ikonlar, uzun adlar, eksik veri, emoji...) -> docs/preview_cesit/
    python tools/ui_preview.py --film     animasyonlar (gol ekrani, bildirimler, lig amblemleri) -> docs/preview_film/
                                          her animasyon bir GIF (<ad>.gif) ve inceleme seridi (<ad>_serit.png);
                                          her film icin kare basina yeniden cizilen ekran payi yazdirilir
    python tools/ui_preview.py --yuzler   telefon uygulamasinin "Gorunum" bolumu icin 13 ifadeyi web/yuzler.png
                                          seridine cizer (docs'a dokunmaz)
Her calismada otomatik denetim yapilir: ust uste binen yazi/ikon, kesilen yazi, fontta olmayan harf.
Bulgular <cikti>/denetim.txt dosyasina yazilir; bilerek boyle olanlar sim/denetim_izin.txt ile susturulur.
Gerekenler: MinGW-w64 g++ (PATH'te ya da RUMY_MINGW ortam degiskeninde), Python Pillow,
            en az bir kez 'pio run -e cyd' (LVGL ve ArduinoJson kaynaklari .pio/libdeps altina iner).
Ciktilar:
    docs/preview/NN_ad.png  her ekran
    docs/ui_preview.png     hepsi tek sayfada
"""
import concurrent.futures
import fnmatch
import configparser
import glob
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIG = "--buyuk" in sys.argv[1:]
DATES = "--tarih" in sys.argv[1:]
VARIANTS = "--cesit" in sys.argv[1:]
FACES = "--yuzler" in sys.argv[1:]
FILMS = "--film" in sys.argv[1:] or FACES
FACE_W, FACE_H, FACE_COLS = 80, 56, 4
MODE = "tarih" if DATES else ("cesit" if VARIANTS else ("film" if FILMS else ""))
BUILD = os.path.join(ROOT, ".pio", "sim_build_buyuk" if BIG else "sim_build")
OUT = os.path.join(ROOT, "docs", ("preview_" + MODE if MODE else "preview") + ("_buyuk" if BIG else ""))
SHEET = os.path.join(ROOT, "docs", ("ui_" + MODE if MODE else "ui_preview") + ("_buyuk" if BIG else "") + ".png")
ALLOW = os.path.join(ROOT, "sim", "denetim_izin.txt")
LIBDEPS = os.path.join(ROOT, ".pio", "libdeps", "cyd")
POOL_KB = int(os.environ.get("RUMY_SIM_POOL_KB", "96" if BIG else "36"))


def find_tool(name):
    base = os.environ.get("RUMY_MINGW")
    if base:
        cand = os.path.join(base, name + ".exe")
        if os.path.exists(cand):
            return cand
    found = shutil.which(name)
    if not found:
        sys.exit("%s bulunamadi: MinGW-w64'u PATH'e ekleyin ya da RUMY_MINGW ile bin klasorunu gosterin" % name)
    return found


def version():
    cp = configparser.ConfigParser(interpolation=None, strict=False)
    cp.read(os.path.join(ROOT, "platformio.ini"), encoding="utf-8")
    return cp.get("rumy", "version", fallback="dev")


def sources():
    lv = os.path.join(LIBDEPS, "lvgl", "src")
    if not os.path.isdir(lv):
        sys.exit("LVGL kaynaklari yok: once 'pio run -e cyd' calistirin")
    out = []
    for path in glob.glob(os.path.join(lv, "**", "*.c"), recursive=True):
        rel = os.path.relpath(path, lv).replace("\\", "/")
        if rel.startswith(("drivers/", "libs/thorvg/", "others/vg_lite_tvg/")):
            continue
        out.append(path)
    out += glob.glob(os.path.join(ROOT, "lib", "rumy_core", "src", "*.cpp"))
    out += glob.glob(os.path.join(ROOT, "src", "ui", "*.cpp"))
    out += glob.glob(os.path.join(ROOT, "src", "ui", "fonts", "*.c"))
    out += glob.glob(os.path.join(ROOT, "src", "rumy", "*.cpp"))
    out += glob.glob(os.path.join(ROOT, "src", "agents", "*.cpp"))
    out += [os.path.join(ROOT, "src", "settings", "settings.cpp"), os.path.join(ROOT, "src", "settings", "provinces.cpp"),
            os.path.join(ROOT, "src", "app", "store.cpp")]
    out += glob.glob(os.path.join(ROOT, "sim", "*.cpp"))
    return out


def write_headers():
    os.makedirs(BUILD, exist_ok=True)
    sd = os.path.join(ROOT, "sd_card").replace("\\", "/")
    foto = os.path.join(BUILD, "foto").replace("\\", "/")
    fixtures = os.path.join(ROOT, "test", "fixtures").replace("\\", "/")
    files = {
        "sim_paths.h": '#pragma once\n#define RUMY_SIM_SD "%s"\n#define RUMY_SIM_FOTO "%s"\n#define RUMY_SIM_FIXTURES "%s"\n'
                       '#define RUMY_SIM_POOL_KB %d\n' % (sd, foto, fixtures, POOL_KB),
        "sim_defs.h": '#pragma once\n#define RUMY_VERSION "%s"\n' % version() +
                      ('#define RUMY_SCREEN_W 480\n#define RUMY_SCREEN_H 320\n' if BIG else ''),
    }
    for name, text in files.items():
        path = os.path.join(BUILD, name)
        old = open(path, encoding="utf-8").read() if os.path.exists(path) else None
        if old != text:
            with open(path, "w", encoding="utf-8", newline="\n") as f:
                f.write(text)


def includes():
    return ["-I" + os.path.join(ROOT, "sim"), "-I" + os.path.join(ROOT, "sim", "include"), "-I" + BUILD,
            "-I" + os.path.join(ROOT, "include"),
            "-I" + os.path.join(LIBDEPS, "lvgl"), "-I" + LIBDEPS, "-I" + os.path.join(ROOT, "src"),
            "-I" + os.path.join(ROOT, "lib", "rumy_core", "src"), "-I" + os.path.join(LIBDEPS, "ArduinoJson", "src"),
            "-include", os.path.join(BUILD, "sim_defs.h")]


def obj_path(src):
    rel = os.path.relpath(src, ROOT).replace("\\", "_").replace("/", "_").replace(":", "")
    return os.path.join(BUILD, rel + ".o")


def stale(src, obj):
    if not os.path.exists(obj):
        return True
    t = os.path.getmtime(obj)
    dep = obj[:-2] + ".d"
    if not os.path.exists(dep):
        return True
    text = open(dep, encoding="utf-8", errors="replace").read().replace("\\\n", " ")
    parts = text.split(":", 1)[1].split() if ":" in text else [src]
    for p in parts:
        p = p.replace("\\ ", " ")
        if os.path.exists(p) and os.path.getmtime(p) > t:
            return True
    return False


def compile_one(args):
    src, cc, cxx, env = args
    obj = obj_path(src)
    if not stale(src, obj):
        return obj, None
    if src.endswith(".c"):
        cmd = [cc, "-O1", "-std=gnu11", "-w", "-DLV_CONF_INCLUDE_SIMPLE", "-MMD"]
    else:
        cmd = [cxx, "-O1", "-std=gnu++17", "-Wall", "-Wextra", "-Wno-unused-parameter", "-DLV_CONF_INCLUDE_SIMPLE",
               "-DARDUINOJSON_USE_DOUBLE=0", "-DARDUINOJSON_USE_LONG_LONG=0", "-DARDUINOJSON_POOL_CAPACITY=32", "-MMD"]
    cmd += includes() + ["-c", src, "-o", obj]
    r = subprocess.run(cmd, capture_output=True, text=True, env=env, errors="replace")
    if r.returncode != 0:
        return obj, r.stderr or r.stdout
    warn = r.stderr.strip()
    return obj, ("UYARI\n" + warn) if warn and not src.endswith(".c") else None


def photos():
    from PIL import Image
    px = 112 if BIG else 84
    out = os.path.join(BUILD, "foto")
    os.makedirs(out, exist_ok=True)
    for path in glob.glob(os.path.join(ROOT, "tools", ".cache", "foto", "*_%d.png" % px)):
        name = os.path.basename(path)[:-len("_%d.png" % px)]
        raw = Image.open(path).convert("RGB").tobytes()
        data = bytearray()
        for k in range(0, len(raw), 3):
            v = ((raw[k] & 0xF8) << 8) | ((raw[k + 1] & 0xFC) << 3) | (raw[k + 2] >> 3)
            data += bytes((v & 0xFF, v >> 8))
        with open(os.path.join(out, name + ".r565"), "wb") as f:
            f.write(data)


def to_png(ppm_dir):
    from PIL import Image, ImageDraw, ImageFont
    os.makedirs(OUT, exist_ok=True)
    for old in glob.glob(os.path.join(OUT, "*.png")) + glob.glob(os.path.join(OUT, "*.gif")):
        os.remove(old)
    shots = sorted(glob.glob(os.path.join(ppm_dir, "*.ppm")), key=lambda p: int(os.path.basename(p).split("_", 1)[0]))
    images = []
    for ppm in shots:
        name = os.path.splitext(os.path.basename(ppm))[0]
        img = Image.open(ppm).convert("RGB")
        img.save(os.path.join(OUT, name + ".png"))
        images.append((name, img))
    if not images:
        return 0
    cols = 3
    iw, ih = images[0][1].size
    cw, ch = iw + 16, ih + 30
    rows = (len(images) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * cw + 16, rows * ch + 16), (24, 28, 34))
    draw = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype("arial.ttf", 13)
    except OSError:
        font = ImageFont.load_default()
    for i, (name, img) in enumerate(images):
        x = 16 + (i % cols) * cw
        y = 16 + (i // cols) * ch
        sheet.paste(img, (x, y))
        draw.rectangle([x - 1, y - 1, x + iw, y + ih], outline=(60, 70, 84))
        draw.text((x, y + ih + 4), name.split("_", 1)[1].replace("_", " "), fill=(170, 180, 194), font=font)
    sheet.save(SHEET)
    return len(images)


def to_gif(ppm_dir):
    from PIL import Image
    count = 0
    for d in sorted(glob.glob(os.path.join(ppm_dir, "film_*"))):
        name = os.path.basename(d)[5:]
        frames = sorted(glob.glob(os.path.join(d, "*.ppm")))
        if not frames:
            continue
        stamps = [int(os.path.splitext(os.path.basename(f))[0].split("_")[1]) for f in frames]
        images = [Image.open(f).convert("RGB") for f in frames]
        durations = [max(20, stamps[i + 1] - stamps[i]) for i in range(len(stamps) - 1)] + [600]
        images[0].save(os.path.join(OUT, name + ".gif"), save_all=True, append_images=images[1:], duration=durations,
                       loop=0, optimize=False, disposal=1)
        picks = 12 if len(images) >= 12 else len(images)
        idx = [round(i * (len(images) - 1) / max(1, picks - 1)) for i in range(picks)]
        iw, ih = images[0].size
        cols = 4
        rows = (len(idx) + cols - 1) // cols
        sheet = Image.new("RGB", (cols * (iw + 6) + 6, rows * (ih + 6) + 6), (24, 28, 34))
        for k, i in enumerate(idx):
            sheet.paste(images[i], (6 + (k % cols) * (iw + 6), 6 + (k // cols) * (ih + 6)))
        sheet.save(os.path.join(OUT, name + "_serit.png"))
        count += 1
    return count


def make_faces(ppm_dir):
    from PIL import Image
    shots = glob.glob(os.path.join(ppm_dir, "*gorunum_yuzler.ppm"))
    if not shots:
        sys.exit("ifade goruntusu uretilemedi")
    img = Image.open(shots[0]).convert("RGB")
    sheet = Image.new("RGB", (FACE_W * 13, FACE_H))
    for m in range(13):
        x = (m % FACE_COLS) * FACE_W
        y = (m // FACE_COLS) * 60
        sheet.paste(img.crop((x, y, x + FACE_W, y + FACE_H)), (m * FACE_W, 0))
    out = os.path.join(ROOT, "web", "yuzler.png")
    sheet.quantize(colors=64).save(out, optimize=True)
    print("ui_preview: 13 ifade -> %s (%d B)" % (os.path.relpath(out, ROOT), os.path.getsize(out)))


def allow_rules():
    rules = []
    if not os.path.exists(ALLOW):
        return rules
    for line in open(ALLOW, encoding="utf-8").read().splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        parts = line.split("\t")
        if len(parts) >= 2:
            rules.append((parts[0], parts[1]))
    return rules


def allowed(line, rules):
    shot, _, finding = line.partition("\t")
    return any(fnmatch.fnmatch(shot, pat) and text in finding for pat, text in rules)


def main():
    cxx = find_tool("g++")
    cc = find_tool("gcc")
    env = dict(os.environ)
    env["PATH"] = os.path.dirname(cxx) + os.pathsep + env.get("PATH", "")
    if FACES:
        env["RUMY_SIM_ONLY"] = "gorunum"
    write_headers()
    photos()
    srcs = sources()
    jobs = [(s, cc, cxx, env) for s in srcs]
    objs = []
    errors = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as ex:
        for obj, msg in ex.map(compile_one, jobs):
            objs.append(obj)
            if msg:
                print(msg)
                if not msg.startswith("UYARI"):
                    errors += 1
    if errors:
        sys.exit("derleme hatasi: %d dosya" % errors)
    exe = os.path.join(BUILD, "rumy_sim.exe")
    rsp = os.path.join(BUILD, "objs.rsp")
    with open(rsp, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join('"%s"' % o.replace("\\", "/") for o in objs))
    r = subprocess.run([cxx, "-o", exe, "@" + rsp], capture_output=True, text=True, env=env, errors="replace")
    if r.returncode != 0:
        print(r.stderr)
        sys.exit("baglama hatasi")
    ppm_dir = os.path.join(BUILD, "shots")
    shutil.rmtree(ppm_dir, ignore_errors=True)
    os.makedirs(ppm_dir)
    r = subprocess.run([exe, ppm_dir] + ([MODE] if MODE else []), capture_output=True, text=True, env=env, errors="replace", timeout=300)
    with open(os.path.join(BUILD, "sim_run.log"), "w", encoding="utf-8") as f:
        f.write(r.stdout + r.stderr)
    for line in r.stdout.splitlines():
        if line.startswith(("bellek", "[Error]", "tamam", "denetim", "film")):
            print(line)
    if r.returncode != 0:
        print(r.stderr[-3000:])
        sys.exit("onizleme programi hata ile bitti (%d)" % r.returncode)
    if FACES:
        make_faces(ppm_dir)
        return
    n = to_png(ppm_dir)
    films = to_gif(ppm_dir)
    if films:
        print("ui_preview: %d animasyon -> %s" % (films, os.path.relpath(OUT, ROOT)))
    print("ui_preview: %d ekran -> %s, ozet -> %s" % (n, os.path.relpath(OUT, ROOT), os.path.relpath(SHEET, ROOT)))
    report = os.path.join(ppm_dir, "denetim.txt")
    if os.path.exists(report):
        lines = [l for l in open(report, encoding="utf-8", errors="replace").read().splitlines() if l.strip()]
        rules = allow_rules()
        kept = [l for l in lines if not allowed(l, rules)]
        out = os.path.join(OUT, "denetim.txt")
        with open(out, "w", encoding="utf-8", newline="\n") as f:
            f.write("".join(l + "\n" for l in kept))
        print("denetim: %d bulgu (%d izinli) -> %s" % (len(kept), len(lines) - len(kept), os.path.relpath(out, ROOT)))


if __name__ == "__main__":
    main()
