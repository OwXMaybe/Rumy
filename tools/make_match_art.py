"""Mac ekranlarinin kucuk resimlerini uretir: turnuva amblemleri (Super Lig, Sampiyonlar Ligi, Avrupa Ligi,
Konferans Ligi, Turkiye Kupasi, Super Kupa, Premier Lig, LaLiga, Serie A, Bundesliga, Ligue 1) ve gol animasyonundaki
donen topun kareleri.

Depoda hicbir kulup ya da turnuva logosu yoktur. tools/assets/lig_logolari/<ad>.png varsa (ad: super_lig, ucl, uel,
uecl, cup, super_cup, epl, laliga, serie_a, bundesliga, ligue1; git disi, kullanicinin kendi koydugu logolar) o resim
kullanilir; yoksa turnuvanin kisaltmasini tasiyan sade bir rozet cizilir (SL, ŞL, AL, KL, TK, SK, PL, LL, SA, BL, L1).
Rozet yazisi LVGL paketindeki Montserrat fontuyla (SIL OFL 1.1) cizilir; once bir kez 'pio run -e cyd' gerekir.
Hepsi 24 birimlik bir izgarada 8 kat buyuk cizilir, kucultulur (yumusak kenarlar) ve LVGL'nin RGB565A8 bicimiyle
(renk + saydamlik) src/ui/match_art.cpp dosyasina gomulur. Amblem CYD icin 18 px, 480x320 arayuz icin 26 px; top
22 / 32 px, 8 kare (desen 72 derecede bir kendini tekrarlar, kare basina 9 derece). Dosya RUMY_SCREEN_H'ye gore
yalnizca birini derler.

Kullanim:
    python tools/make_match_art.py [--preview dosya.png]
Gerekenler: Python Pillow
"""
import math
import os
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_H = os.path.join(ROOT, "src", "ui", "match_art.h")
OUT_CPP = os.path.join(ROOT, "src", "ui", "match_art.cpp")
FONT = os.path.join(ROOT, ".pio", "libdeps", "cyd", "lvgl", "scripts", "built_in_font", "Montserrat-Medium.ttf")
LOGO_DIR = os.path.join(ROOT, "tools", "assets", "lig_logolari")
SMALL_GUARD = "#if !defined(RUMY_SCREEN_H) || RUMY_SCREEN_H < 320"

PX = 18
BIG_PX = 26
BALL_PX = 22
BIG_BALL_PX = 32
BALL_FRAMES = 8
BALL_STEP = 9
DESIGN = 24
SS = 8

WHITE = (255, 255, 255, 255)
SILVER = (214, 222, 235, 255)
RED = (235, 20, 36, 255)
NAVY = (12, 30, 84, 255)
ORANGE = (255, 112, 0, 255)
GREEN = (0, 200, 90, 255)
GOLD = (255, 200, 50, 255)
INK = (28, 30, 38, 255)
PURPLE = (110, 40, 170, 255)
BLUE = (20, 110, 230, 255)
LIME = (215, 230, 40, 255)
DEEP = (16, 24, 60, 255)
YELLOW = (255, 210, 0, 255)

LEAGUES = ["super_lig", "ucl", "uel", "uecl", "cup", "super_cup", "epl", "laliga", "serie_a", "bundesliga", "ligue1"]
BADGES = {"super_lig": ("SL", RED, WHITE), "ucl": ("ŞL", (28, 58, 140, 255), WHITE), "uel": ("AL", ORANGE, WHITE),
          "uecl": ("KL", (0, 140, 70, 255), WHITE), "cup": ("TK", (180, 18, 40, 255), WHITE),
          "super_cup": ("SK", GOLD, INK), "epl": ("PL", PURPLE, WHITE), "laliga": ("LL", (230, 60, 50, 255), WHITE),
          "serie_a": ("SA", BLUE, WHITE), "bundesliga": ("BL", (200, 20, 30, 255), WHITE), "ligue1": ("L1", (34, 44, 92, 255), LIME)}


def canvas():
    img = Image.new("RGBA", (DESIGN * SS, DESIGN * SS), (0, 0, 0, 0))
    return img, ImageDraw.Draw(img)


def P(pts):
    return [(x * SS, y * SS) for x, y in pts]


def badge(name):
    if not os.path.exists(FONT):
        sys.exit("make_match_art: %s yok; once 'pio run -e cyd' calistirin" % os.path.relpath(FONT, ROOT))
    text, bg, fg = BADGES[name]
    img, d = canvas()
    d.ellipse(P([(0.6, 0.6), (23.4, 23.4)]), fill=bg)
    ring = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(ring).ellipse(P([(0.6, 0.6), (23.4, 23.4)]), outline=(255, 255, 255, 120), width=int(1.1 * SS))
    img.alpha_composite(ring)
    d = ImageDraw.Draw(img)
    font = ImageFont.truetype(FONT, int(10.5 * SS))
    stroke = int(0.35 * SS)
    box = d.textbbox((0, 0), text.replace("Ş", "S"), font=font, stroke_width=stroke)
    x = (DESIGN * SS - (box[2] - box[0])) / 2 - box[0]
    y = (DESIGN * SS - (box[3] - box[1])) / 2 - box[1]
    d.text((x, y), text, font=font, fill=fg, stroke_width=stroke, stroke_fill=fg)
    return img


LOGO_TOP = {"bundesliga": 0.74, "ligue1": 0.56, "epl": 0.6, "serie_a": 0.62}
LOGO_KEY_WHITE = {"serie_a"}
LOGO_BOLD = {"ligue1": 11}
LOGO_HALO = {"epl": 9}


def logo(name, draw):
    path = os.path.join(LOGO_DIR, name + ".png")
    if not os.path.exists(path):
        return draw()
    src = Image.open(path).convert("RGBA")
    if name in LOGO_KEY_WHITE:
        r, g, b, a = src.split()
        light = ImageChops.darker(ImageChops.darker(r, g), b).point(lambda v: 255 if v > 225 else 0)
        src.putalpha(ImageChops.subtract(a, light))
    src = src.crop(src.getchannel("A").getbbox())
    if name in LOGO_TOP:
        src = src.crop((0, 0, src.size[0], int(src.size[1] * LOGO_TOP[name])))
        src = src.crop(src.getchannel("A").getbbox())
    if name in LOGO_BOLD:
        alpha = src.getchannel("A").filter(ImageFilter.MaxFilter(LOGO_BOLD[name]))
        rgb = src.convert("RGB").filter(ImageFilter.MaxFilter(LOGO_BOLD[name]))
        src = Image.merge("RGBA", (*rgb.split(), alpha))
    if name in LOGO_HALO:
        pad = LOGO_HALO[name]
        big = Image.new("RGBA", (src.size[0] + 2 * pad, src.size[1] + 2 * pad), (0, 0, 0, 0))
        big.paste(src, (pad, pad))
        halo = big.getchannel("A").filter(ImageFilter.MaxFilter(2 * pad + 1))
        out = Image.new("RGBA", big.size, (255, 255, 255, 0))
        out.putalpha(halo)
        out.alpha_composite(big)
        src = out
    side = max(src.size)
    sq = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    sq.paste(src, ((side - src.size[0]) // 2, (side - src.size[1]) // 2))
    return sq.resize((DESIGN * SS, DESIGN * SS), Image.LANCZOS)


def pentagon(cx, cy, r, rot):
    return [(cx + math.cos(math.radians(rot + k * 72)) * r, cy + math.sin(math.radians(rot + k * 72)) * r)
            for k in range(5)]


def ball(deg):
    img, d = canvas()
    c = 12
    d.ellipse(P([(c - 11.4, c - 11.4), (c + 11.4, c + 11.4)]), fill=WHITE)
    shade = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ds = ImageDraw.Draw(shade)
    ds.ellipse(P([(c - 11.4, c - 11.4), (c + 11.4, c + 11.4)]), outline=(150, 160, 178, 150), width=int(1.6 * SS))
    pat = Image.new("RGBA", img.size, (0, 0, 0, 0))
    dp = ImageDraw.Draw(pat)
    center = pentagon(c, c, 3.9, deg - 90)
    dp.polygon(P(center), fill=INK)
    for k in range(5):
        a = deg - 90 + k * 72
        ox = c + math.cos(math.radians(a)) * 10.0
        oy = c + math.sin(math.radians(a)) * 10.0
        dp.polygon(P(pentagon(ox, oy, 3.6, a + 180)), fill=INK)
        ix = c + math.cos(math.radians(a)) * 3.6
        iy = c + math.sin(math.radians(a)) * 3.6
        jx = c + math.cos(math.radians(a)) * 6.6
        jy = c + math.sin(math.radians(a)) * 6.6
        dp.line(P([(ix, iy), (jx, jy)]), fill=INK, width=int(0.8 * SS))
    clip = Image.new("L", img.size, 0)
    ImageDraw.Draw(clip).ellipse(P([(c - 11.0, c - 11.0), (c + 11.0, c + 11.0)]), fill=255)
    img.paste(pat, (0, 0), ImageChops.multiply(pat.getchannel("A"), clip))
    img.alpha_composite(shade)
    return img


DRAW = {n: (lambda n=n: logo(n, lambda: badge(n))) for n in LEAGUES}


def shrink(img, px):
    return img.convert("RGBa").resize((px, px), Image.BOX).convert("RGBA")


def rgb565a8(img):
    raw = img.tobytes()
    color = bytearray()
    alpha = bytearray()
    for i in range(0, len(raw), 4):
        r, g, b, a = raw[i], raw[i + 1], raw[i + 2], raw[i + 3]
        v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3) if a else 0
        color += bytes((v & 0xFF, v >> 8))
        alpha.append(a)
    return bytes(color + alpha)


def image(cpp, sym, img, px):
    data = rgb565a8(img)
    cpp.append("alignas(4) static const uint8_t %s_data[] = {" % sym)
    for i in range(0, len(data), 20):
        cpp.append("    " + ", ".join("0x%02x" % b for b in data[i:i + 20]) + ",")
    cpp.append("};")
    cpp.append("static const lv_image_dsc_t %s = {" % sym)
    cpp.append("    {LV_IMAGE_HEADER_MAGIC, LV_COLOR_FORMAT_RGB565A8, 0, %d, %d, %d, 0}," % (px, px, px * 2))
    cpp.append("    sizeof(%s_data)," % sym)
    cpp.append("    %s_data," % sym)
    cpp.append("    nullptr,")
    cpp.append("    nullptr,")
    cpp.append("};")
    return len(data)


def emit(cpp, px, bpx, logos, balls):
    total = 0
    for name in LEAGUES:
        total += image(cpp, "league_%s" % name, logos[name], px)
    for k, img in enumerate(balls):
        total += image(cpp, "ball_%d" % k, img, bpx)
    cpp.append("")
    cpp.append("const lv_image_dsc_t* const LEAGUE_IMAGES[LEAGUE_LOGOS] = {")
    cpp.append("    " + " ".join("&league_%s," % n for n in LEAGUES))
    cpp.append("};")
    cpp.append("const lv_image_dsc_t* const BALL_IMAGES[BALL_FRAMES] = {")
    cpp.append("    " + " ".join("&ball_%d," % k for k in range(len(balls))))
    cpp.append("};")
    return total


def preview(out, sets):
    bgs = [(0, 0, 0), (10, 30, 80), (26, 32, 41)]
    z = 4
    cell = BIG_BALL_PX * z + 12
    cols = max(len(LEAGUES), BALL_FRAMES)
    sheet = Image.new("RGB", (cols * cell + 12, (len(sets) * (len(bgs) + 1)) * cell + 12), (40, 40, 40))
    row = 0
    for px, logos, balls in sets:
        for bg in bgs:
            for i, name in enumerate(LEAGUES):
                tile = Image.new("RGBA", (px, px), bg + (255,))
                tile.alpha_composite(logos[name])
                tile = tile.resize((px * z, px * z), Image.NEAREST)
                sheet.paste(Image.new("RGB", (cell - 4, cell - 4), bg), (8 + i * cell, 8 + row * cell))
                sheet.paste(tile.convert("RGB"), (8 + i * cell + 4, 8 + row * cell + 4))
            row += 1
        bg = (10, 30, 80)
        for i, img in enumerate(balls):
            bp = img.size[0]
            tile = Image.new("RGBA", (bp, bp), bg + (255,))
            tile.alpha_composite(img)
            tile = tile.resize((bp * z, bp * z), Image.NEAREST)
            sheet.paste(Image.new("RGB", (cell - 4, cell - 4), bg), (8 + i * cell, 8 + row * cell))
            sheet.paste(tile.convert("RGB"), (8 + i * cell + 4, 8 + row * cell + 4))
        row += 1
    sheet.save(out)


def generate(out=None):
    big = {n: DRAW[n]() for n in LEAGUES}
    spin = [ball(k * BALL_STEP) for k in range(BALL_FRAMES)]
    small_set = ({n: shrink(big[n], PX) for n in LEAGUES}, [shrink(b, BALL_PX) for b in spin])
    big_set = ({n: shrink(big[n], BIG_PX) for n in LEAGUES}, [shrink(b, BIG_BALL_PX) for b in spin])
    h = ["#pragma once", "", "#include <lvgl.h>", "#include <stdint.h>", "",
         "#if defined(RUMY_SCREEN_H) && RUMY_SCREEN_H >= 320",
         "constexpr int32_t LEAGUE_PX = %d;" % BIG_PX,
         "constexpr int32_t BALL_PX = %d;" % BIG_BALL_PX,
         "#else",
         "constexpr int32_t LEAGUE_PX = %d;" % PX,
         "constexpr int32_t BALL_PX = %d;" % BALL_PX,
         "#endif",
         "constexpr int LEAGUE_LOGOS = %d;" % len(LEAGUES),
         "constexpr int BALL_FRAMES = %d;" % BALL_FRAMES,
         "constexpr int BALL_STEP_DEG = %d;" % BALL_STEP,
         "extern const lv_image_dsc_t* const LEAGUE_IMAGES[LEAGUE_LOGOS];",
         "extern const lv_image_dsc_t* const BALL_IMAGES[BALL_FRAMES];", ""]
    cpp = ["#include \"match_art.h\"", "", SMALL_GUARD, ""]
    small = emit(cpp, PX, BALL_PX, *small_set)
    cpp += ["", "#else", ""]
    large = emit(cpp, BIG_PX, BIG_BALL_PX, *big_set)
    cpp += ["", "#endif", ""]
    with open(OUT_H, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(h))
    with open(OUT_CPP, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(cpp))
    if out:
        preview(out, [(PX,) + small_set, (BIG_PX,) + big_set])
    print("make_match_art: %d amblem + %d top karesi, %d B (CYD) / %d B (480x320) flash -> %s"
          % (len(LEAGUES), BALL_FRAMES, small, large, os.path.relpath(OUT_CPP, ROOT)))


if __name__ == "__main__":
    args = sys.argv[1:]
    generate(args[1] if len(args) == 2 and args[0] == "--preview" else None)
