"""Ana ekrandaki "simdi caliyor" plaginin donus karelerini uretir.

Plak 17x17 px (cizim olculeri 25 px'e gore, boya gore olceklenir); siyah zemin uzerinde 8 kat buyuk cizilip kucultulur (yumusak kenarlar) ve LVGL'nin RGB565
bicimiyle src/ui/record_frames.h ve src/ui/record_frames.cpp dosyalarina gomulur. Desen 180 derecede bir
kendini tekrarladigi icin yarim tur 24 kareyle (7,5 derece) cizilir; kartta her karede tek bir kucuk resim
cizilir, plak her karede yeniden hesaplanmaz.

480x320 arayuz icin 22 px'lik kareler src/ui/record_frames_big.cpp dosyasina yazilir; iki dosya da
RUMY_SCREEN_H'ye gore yalnizca birini derler.

Kullanim:
    python tools/make_record.py [--buyuk] [--preview dosya.png]
Gerekenler: Python Pillow
"""
import math
import os
import sys

from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_H = os.path.join(ROOT, "src", "ui", "record_frames.h")
OUT_CPP = os.path.join(ROOT, "src", "ui", "record_frames.cpp")
OUT_BIG_CPP = os.path.join(ROOT, "src", "ui", "record_frames_big.cpp")
SMALL_GUARD = "#if !defined(RUMY_SCREEN_H) || RUMY_SCREEN_H < 320"
BIG_GUARD = "#if defined(RUMY_SCREEN_H) && RUMY_SCREEN_H >= 320"

PX = 17
BIG_PX = 22
DESIGN_PX = 25
FRAMES = 24
PERIOD = 180
SS = 8

RIM = (0x3C, 0x3C, 0x46)
BODY = (0x13, 0x13, 0x18)
GROOVE = (0x30, 0x30, 0x3A)
GLINT = (0x9A, 0xA5, 0xB4, 190)
LABEL = (0xB8, 0x4D, 0xFF)
MARK = (0xEB, 0xCD, 0xFF, 220)
HOLE = (0x00, 0x00, 0x00)


def circle(draw, c, r, fill):
    draw.ellipse([c - r * SS, c - r * SS, c + r * SS, c + r * SS], fill=fill)


def ring(draw, c, r, w, fill):
    draw.ellipse([c - r * SS, c - r * SS, c + r * SS, c + r * SS], outline=fill, width=max(1, round(w * SS)))


def band(size, c, r_out, w, a0, a1, color):
    layer = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    rgb = color[:3] + (255,)
    d.arc([c - r_out * SS, c - r_out * SS, c + r_out * SS, c + r_out * SS], a0, a1, fill=rgb, width=round(w * SS))
    mid = r_out - w / 2
    for a in (a0, a1):
        x = c + math.cos(math.radians(a)) * mid * SS
        y = c + math.sin(math.radians(a)) * mid * SS
        d.ellipse([x - w / 2 * SS, y - w / 2 * SS, x + w / 2 * SS, y + w / 2 * SS], fill=rgb)
    alpha = layer.getchannel("A").point(lambda v: v * color[3] // 255)
    layer.putalpha(alpha)
    return layer


def frame(deg, px=PX):
    k = px / DESIGN_PX
    size = px * SS
    c = size / 2
    img = Image.new("RGBA", (size, size), (0, 0, 0, 255))
    d = ImageDraw.Draw(img)
    circle(d, c, 12.4 * k, RIM)
    circle(d, c, 11.3 * k, BODY)
    ring(d, c, 9.4 * k, 0.8 * k, GROOVE)
    ring(d, c, 7.4 * k, 0.8 * k, GROOVE)
    for base in (deg, deg + 180):
        img.alpha_composite(band(size, c, 10.6 * k, 2.2 * k, base - 22, base + 22, GLINT))
    d = ImageDraw.Draw(img)
    circle(d, c, 5.3 * k, LABEL)
    for base in (deg + 90, deg + 270):
        img.alpha_composite(band(size, c, 4.3 * k, 1.6 * k, base - 30, base + 30, MARK))
    d = ImageDraw.Draw(img)
    circle(d, c, 1.3 * k, HOLE)
    return img.convert("RGB").resize((px, px), Image.BOX)


def rgb565(img):
    raw = img.tobytes()
    out = bytearray()
    for i in range(0, len(raw), 3):
        r, g, b = raw[i], raw[i + 1], raw[i + 2]
        v = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        out += bytes((v & 0xFF, v >> 8))
    return bytes(out)


def generate(preview=None, big=False):
    px = BIG_PX if big else PX
    frames = [frame(k * PERIOD / FRAMES, px) for k in range(FRAMES)]
    h = ["#pragma once", "", "#include <lvgl.h>", "#include <stdint.h>", "",
         "#if defined(RUMY_SCREEN_H) && RUMY_SCREEN_H >= 320",
         "constexpr int32_t RECORD_PX = %d;" % BIG_PX,
         "#else",
         "constexpr int32_t RECORD_PX = %d;" % PX,
         "#endif",
         "constexpr int32_t RECORD_FRAMES = %d;" % FRAMES,
         "constexpr int32_t RECORD_PERIOD_DEG = %d;" % PERIOD,
         "extern const lv_image_dsc_t* const RECORD_IMAGES[RECORD_FRAMES];", ""]
    cpp = [BIG_GUARD if big else SMALL_GUARD, "#include \"record_frames.h\"", ""]
    total = 0
    for k, img in enumerate(frames):
        data = rgb565(img)
        total += len(data)
        sym = "record_%02d" % k
        cpp.append("alignas(4) static const uint8_t %s_data[] = {" % sym)
        for i in range(0, len(data), 20):
            cpp.append("    " + ", ".join("0x%02x" % b for b in data[i:i + 20]) + ",")
        cpp.append("};")
        cpp.append("static const lv_image_dsc_t %s = {" % sym)
        cpp.append("    {LV_IMAGE_HEADER_MAGIC, LV_COLOR_FORMAT_RGB565, 0, %d, %d, %d, 0}," % (px, px, px * 2))
        cpp.append("    sizeof(%s_data)," % sym)
        cpp.append("    %s_data," % sym)
        cpp.append("    nullptr,")
        cpp.append("    nullptr,")
        cpp.append("};")
    cpp.append("")
    cpp.append("const lv_image_dsc_t* const RECORD_IMAGES[RECORD_FRAMES] = {")
    for i in range(0, FRAMES, 6):
        cpp.append("    " + " ".join("&record_%02d," % k for k in range(i, min(i + 6, FRAMES))))
    cpp.append("};")
    cpp.append("")
    cpp.append("#endif")
    cpp.append("")
    out_cpp = OUT_BIG_CPP if big else OUT_CPP
    with open(OUT_H, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(h))
    with open(out_cpp, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(cpp))
    if preview:
        sheet = Image.new("RGB", (FRAMES * (px + 2) * 4, (px + 2) * 4), (0, 0, 0))
        for k, img in enumerate(frames):
            sheet.paste(img.resize((px * 4, px * 4), Image.NEAREST), (k * (px + 2) * 4, 4))
        sheet.save(preview)
    print("make_record: %d kare, %d B flash -> %s" % (FRAMES, total, os.path.relpath(out_cpp, ROOT)))


if __name__ == "__main__":
    out = None
    big = "--buyuk" in sys.argv[1:]
    args = [a for a in sys.argv[1:] if a != "--buyuk"]
    if len(args) == 2 and args[0] == "--preview":
        out = args[1]
    generate(out, big)
