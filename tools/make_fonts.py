"""Rumy fontlarini uretir (lv_font_conv, 4 bpp, sadece gereken glifler).

Kaynaklar (hepsi acik lisansli):
  - Inter (SIL OFL 1.1)                 -> @fontsource/inter (jsDelivr)
  - Font Awesome 5 Free Solid (SIL OFL) -> LVGL paketindeki FontAwesome5-Solid+Brands+Regular.woff

Ciktilar (yorum satirlari temizlenir):
  src/ui/fonts/font_rumy_12.c, font_rumy_16.c, font_rumy_22.c, font_rumy_num.c, font_rumy_temp.c (29 px rakam),
                          font_rumy_goal.c (40 px, yalnizca "GOL!"; Pi icin font_rumy_bgoal.c 54 px),
                          font_rumy_hero.c (gol ekraninin buyuk GOOOL! yazisi, 60 px; Pi icin font_rumy_bhero.c 80 px)
  src/ui/fonts/font_rumy_b16.c, font_rumy_b21.c, font_rumy_b29.c, font_rumy_bnum.c, font_rumy_btemp.c
                          (480x320 arayuz icin 4/3 boy; yalnizca RUMY_SCREEN_H >= 320 derlemelerinde derlenir)
  src/app/portal_font.h   (ilk kurulum ekrani icin LovyanGFX VLW fontu)

Kullanim:
    python tools/make_fonts.py            hepsi
    python tools/make_fonts.py --buyuk    yalnizca 480x320 fontlari
Gerekenler: Node.js (npx), Python Pillow
"""
import os
import re
import shutil
import struct
import subprocess
import sys
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE = os.path.join(ROOT, "tools", ".cache", "fonts")
OUT_DIR = os.path.join(ROOT, "src", "ui", "fonts")
PORTAL_OUT = os.path.join(ROOT, "src", "app", "portal_font.h")
LV_FONT_CONV = "lv_font_conv@1.5.3"
INTER_BASE = "https://cdn.jsdelivr.net/npm/@fontsource/inter@5/files/"

LATIN = ("0x20-0x7E,0xA0-0xFF,0x131,0x152,0x153,0x2013,0x2014,0x2018,0x2019,0x201C,0x201D,0x2022,0x2026,"
         "0x2039,0x203A,0x2191,0x2193")
LATIN_EXT = "0x100-0x130,0x132-0x151,0x154-0x17F"
NUM = "0x20,0x2D,0x2E,0x30-0x3A,0xB0"

SYMBOLS = {
    "POWER": 0xF011, "SETTINGS": 0xF013, "OK": 0xF00C, "CLOSE": 0xF00D, "LEFT": 0xF053,
    "RIGHT": 0xF054, "REFRESH": 0xF021, "WARNING": 0xF071, "HOME": 0xF015, "WIFI": 0xF1EB,
    "SD": 0xF7C2, "EYE": 0xF06E, "BULB": 0xF0EB, "PIN": 0xF3C5, "MOON": 0xF186, "SUN": 0xF185,
    "DROP": 0xF043, "WIND": 0xF72E, "THERMO": 0xF2C9, "GAUGE": 0xF3FD, "LEAF": 0xF06C,
    "UMBRELLA": 0xF0E9, "WATER": 0xF773, "GLOBE": 0xF0AC, "INFO": 0xF05A, "CHART": 0xF080,
    "CALENDAR": 0xF133, "CLOCK": 0xF017, "HAND": 0xF25A, "STAR": 0xF005, "CAKE": 0xF1FD,
    "FLAG": 0xF024, "BELL": 0xF0F3, "PALETTE": 0xF53F, "BOLT": 0xF0E7, "TRASH": 0xF1F8,
    "PLUS": 0xF067, "SEARCH": 0xF002, "SIGNAL": 0xF012, "PLAY": 0xF04B, "PAUSE": 0xF04C,
    "NEXT": 0xF051, "PREV": 0xF048, "VOL_UP": 0xF028, "VOL_DOWN": 0xF027, "MUSIC": 0xF001,
    "SPOTIFY": 0xF1BC, "CUBE": 0xF1B2, "CAMERA": 0xF030,
    "LAPTOP": 0xF109, "BOOK": 0xF02D, "UTENSILS": 0xF2E7, "COUCH": 0xF4B8, "FILM": 0xF008, "FIRE": 0xF06D,
    "FUTBOL": 0xF1E3, "EXCHANGE": 0xF362, "LIST": 0xF03A, "RANDOM": 0xF074,
}

FONTS = [
    ("font_rumy_12", 12, "400", True),
    ("font_rumy_16", 16, "500", True),
    ("font_rumy_22", 22, "600", True),
]

BIG_FONTS = [
    ("font_rumy_b16", 16, "400", True),
    ("font_rumy_b21", 21, "500", True),
    ("font_rumy_b29", 29, "600", True),
]
BIG_NUM = ("font_rumy_bnum", 75)
BIG_TEMP = ("font_rumy_btemp", 39, "600")
TEMP = ("font_rumy_temp", 29, "600")
GOAL = ("font_rumy_goal", 40, "600")
BIG_GOAL = ("font_rumy_bgoal", 54, "600")
HERO = ("font_rumy_hero", 60, "600")
BIG_HERO = ("font_rumy_bhero", 80, "600")
GOAL_CHARS = "0x21,0x47,0x4C,0x4F"
BIG_GUARD = "#if defined(RUMY_SCREEN_H) && RUMY_SCREEN_H >= 320\n"


def fetch(name):
    os.makedirs(CACHE, exist_ok=True)
    path = os.path.join(CACHE, name)
    if not os.path.exists(path):
        url = INTER_BASE + name
        print("indiriliyor:", url)
        with urllib.request.urlopen(url, timeout=60) as r, open(path, "wb") as f:
            f.write(r.read())
    return path


def fa_path():
    cand = [
        os.path.join(ROOT, ".pio", "libdeps", "cyd", "lvgl", "scripts", "built_in_font",
                     "FontAwesome5-Solid+Brands+Regular.woff"),
    ]
    for c in cand:
        if os.path.exists(c):
            return c
    os.makedirs(CACHE, exist_ok=True)
    path = os.path.join(CACHE, "FontAwesome5-Solid+Brands+Regular.woff")
    if not os.path.exists(path):
        url = ("https://raw.githubusercontent.com/lvgl/lvgl/v9.5.0/scripts/built_in_font/"
               "FontAwesome5-Solid+Brands+Regular.woff")
        with urllib.request.urlopen(url, timeout=60) as r, open(path, "wb") as f:
            f.write(r.read())
    return path


def npx():
    exe = shutil.which("npx") or shutil.which("npx.cmd")
    if not exe:
        raise SystemExit("npx bulunamadi (Node.js gerekli)")
    return exe


def strip_comments(text):
    out = []
    i = 0
    n = len(text)
    in_str = None
    while i < n:
        c = text[i]
        if in_str:
            out.append(c)
            if c == "\\" and i + 1 < n:
                out.append(text[i + 1])
                i += 2
                continue
            if c == in_str:
                in_str = None
            i += 1
            continue
        if c in "\"'":
            in_str = c
            out.append(c)
            i += 1
            continue
        if text.startswith("/*", i):
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
            continue
        if text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
            continue
        out.append(c)
        i += 1
    lines = [ln.rstrip() for ln in "".join(out).splitlines()]
    cleaned = []
    for ln in lines:
        if ln == "" and cleaned and cleaned[-1] == "":
            continue
        cleaned.append(ln)
    return "\n".join(cleaned).strip() + "\n"


GLYPH_RE = re.compile(r"\{\.bitmap_index = (\d+), \.adv_w = (\d+), \.box_w = (\d+), \.box_h = (\d+), "
                      r"\.ofs_x = (-?\d+), \.ofs_y = (-?\d+)\}")


def tabular_digits(text, first_codepoint=0x20, codepoints=None):
    glyphs = list(GLYPH_RE.finditer(text))
    if not glyphs or codepoints is None:
        return text
    entries = glyphs[1:]
    mapping = dict(zip(codepoints, entries))
    digits = [mapping[cp] for cp in range(0x30, 0x3A) if cp in mapping]
    if not digits:
        return text
    max_adv = max(int(m.group(2)) for m in digits)
    repl = {}
    for m in digits:
        adv = int(m.group(2))
        shift = int(round((max_adv - adv) / 32.0))
        new = "{.bitmap_index = %s, .adv_w = %d, .box_w = %s, .box_h = %s, .ofs_x = %d, .ofs_y = %s}" % (
            m.group(1), max_adv, m.group(3), m.group(4), int(m.group(5)) + shift, m.group(6))
        repl[m.span()] = new
    out = []
    last = 0
    for (a, b), new in sorted(repl.items()):
        out.append(text[last:a])
        out.append(new)
        last = b
    out.append(text[last:])
    return "".join(out)


def tighten_lines(text):
    glyphs = list(GLYPH_RE.finditer(text))[1:]
    if not glyphs:
        return text
    top = max(int(m.group(6)) + int(m.group(4)) for m in glyphs)
    bottom = min(int(m.group(6)) for m in glyphs)
    desc = max(0, -bottom) + 1
    line_h = top + desc + 1
    text = re.sub(r"\.line_height = \d+,", ".line_height = %d," % line_h, text)
    text = re.sub(r"\.base_line = -?\d+,", ".base_line = %d," % desc, text)
    return text


def expand(ranges):
    cps = []
    for part in ranges.split(","):
        if "-" in part:
            a, b = part.split("-")
            cps.extend(range(int(a, 16), int(b, 16) + 1))
        else:
            cps.append(int(part, 16))
    return cps


def run_conv(name, size, args):
    out = os.path.join(OUT_DIR, name + ".c")
    cmd = [npx(), "--yes", LV_FONT_CONV, "--bpp", "4", "--size", str(size), "--format", "lvgl",
           "--no-compress", "--lv-include", "lvgl.h", "--lv-font-name", name, "-o", out] + args
    print(" ".join(os.path.basename(c) if i == 0 else c for i, c in enumerate(cmd)))
    subprocess.check_call(cmd, shell=(os.name == "nt"))
    return out


def guard(text, big):
    return BIG_GUARD + text + "#endif\n" if big else text


def build_lvgl_fonts(fonts, num_name, num_size, big=False):
    os.makedirs(OUT_DIR, exist_ok=True)
    fa = fa_path()
    sym = ",".join("0x%X" % v for v in SYMBOLS.values())
    for name, size, weight, with_symbols in fonts:
        latin = fetch("inter-latin-%s-normal.woff" % weight)
        latin_ext = fetch("inter-latin-ext-%s-normal.woff" % weight)
        args = ["--font", latin, "-r", LATIN, "--font", latin_ext, "-r", LATIN_EXT]
        if with_symbols:
            args += ["--font", fa, "-r", sym]
        out = run_conv(name, size, args)
        with open(out, "r", encoding="utf-8") as f:
            text = f.read()
        with open(out, "w", encoding="utf-8", newline="\n") as f:
            f.write(guard(strip_comments(text), big))

    latin = fetch("inter-latin-500-normal.woff")
    out = run_conv(num_name, num_size, ["--font", latin, "-r", NUM])
    with open(out, "r", encoding="utf-8") as f:
        text = f.read()
    text = strip_comments(text)
    text = tabular_digits(text, codepoints=sorted(set(expand(NUM))))
    text = tighten_lines(text)
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write(guard(text, big))


def build_num_font(name, size, weight, big, rng=NUM):
    latin = fetch("inter-latin-%s-normal.woff" % weight)
    out = run_conv(name, size, ["--font", latin, "-r", rng])
    with open(out, "r", encoding="utf-8") as f:
        text = strip_comments(f.read())
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write(guard(text, big))


def symbols_header():
    lines = ["#pragma once", ""]
    for k, v in SYMBOLS.items():
        ch = chr(v).encode("utf-8")
        esc = "".join("\\x%02X" % b for b in ch)
        lines.append('#define RS_%s "%s"' % (k, esc))
    lines.append("")
    path = os.path.join(OUT_DIR, "symbols.h")
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))


def build_vlw():
    from PIL import Image, ImageDraw, ImageFont
    size = 18
    latin = fetch("inter-latin-500-normal.woff")
    latin_ext = fetch("inter-latin-ext-500-normal.woff")
    f_lat = ImageFont.truetype(latin, size)
    f_ext = ImageFont.truetype(latin_ext, size)
    cps = sorted(set(list(range(0x21, 0x7F)) + [0xB0, 0xB7, 0xC7, 0xD6, 0xDC, 0xE7, 0xF6, 0xFC, 0x131,
                                                    0x11E, 0x11F, 0x130, 0x15E, 0x15F, 0x2022, 0x2026]))
    ascent, descent = f_lat.getmetrics()
    glyphs = []
    for cp in cps:
        font = f_ext if cp in (0x11E, 0x11F, 0x130, 0x15E, 0x15F) else f_lat
        ch = chr(cp)
        bbox = font.getbbox(ch)
        adv = int(round(font.getlength(ch)))
        if bbox is None or bbox[2] <= bbox[0] or bbox[3] <= bbox[1]:
            glyphs.append((cp, 0, 0, adv, 0, 0, b""))
            continue
        x0, y0, x1, y1 = bbox
        w, h = x1 - x0, y1 - y0
        img = Image.new("L", (w, h), 0)
        d = ImageDraw.Draw(img)
        d.text((-x0, -y0), ch, font=font, fill=255)
        dy = ascent - y0
        glyphs.append((cp, h, w, adv, dy, x0, img.tobytes()))
    header = struct.pack(">6i", len(glyphs), 11, size, 0, ascent, descent)
    table = b"".join(struct.pack(">7i", g[0], g[1], g[2], g[3], g[4], g[5], 0) for g in glyphs)
    bitmaps = b"".join(g[6] for g in glyphs)
    data = header + table + bitmaps
    lines = ["#pragma once", "#include <stdint.h>", "", "static const uint8_t PORTAL_FONT_VLW[] = {"]
    for i in range(0, len(data), 24):
        lines.append("    " + ", ".join("0x%02x" % b for b in data[i:i + 24]) + ",")
    lines.append("};")
    lines.append("")
    os.makedirs(os.path.dirname(PORTAL_OUT), exist_ok=True)
    with open(PORTAL_OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    print("VLW:", len(data), "bayt ->", PORTAL_OUT)


def main():
    if "--buyuk" not in sys.argv[1:]:
        build_lvgl_fonts(FONTS, "font_rumy_num", 56)
        build_num_font(TEMP[0], TEMP[1], TEMP[2], big=False)
        build_num_font(GOAL[0], GOAL[1], GOAL[2], big=False, rng=GOAL_CHARS)
        build_num_font(HERO[0], HERO[1], HERO[2], big=False, rng=GOAL_CHARS)
        symbols_header()
        build_vlw()
    build_lvgl_fonts(BIG_FONTS, BIG_NUM[0], BIG_NUM[1], big=True)
    build_num_font(BIG_TEMP[0], BIG_TEMP[1], BIG_TEMP[2], big=True)
    build_num_font(BIG_GOAL[0], BIG_GOAL[1], BIG_GOAL[2], big=True, rng=GOAL_CHARS)
    build_num_font(BIG_HERO[0], BIG_HERO[1], BIG_HERO[2], big=True, rng=GOAL_CHARS)
    total = sum(os.path.getsize(os.path.join(OUT_DIR, n)) for n in os.listdir(OUT_DIR) if n.endswith(".c"))
    print("font kaynaklari toplam:", total, "bayt (C metni)")


if __name__ == "__main__":
    sys.exit(main())
