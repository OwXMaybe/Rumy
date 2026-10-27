"""Hava ikonlarini flash'a gomer.

sd_card/rumy/img/<boyut>/<ad>.bin dosyalarini (LVGL 9 paletli ikonlar) okuyup
src/ui/icons_embedded.h ve src/ui/icons_embedded.cpp uretir. Ikonlar boylece SD'den okunmaz,
RAM'e de kopyalanmaz: LVGL onlari flash'tan satir satir cozerek cizer.

Kullanim:
    python tools/embed_icons.py
convert_assets.py ikonlari yeniden urettiginde bu betigi kendisi de cagirir.
"""
import os
import struct

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SD_IMG = os.path.join(ROOT, "sd_card", "rumy", "img")
OUT_H = os.path.join(ROOT, "src", "ui", "icons_embedded.h")
OUT_CPP = os.path.join(ROOT, "src", "ui", "icons_embedded.cpp")

NAMES = ["clear-day", "clear-night", "partly-cloudy-day", "partly-cloudy-night", "overcast", "fog", "drizzle", "rain",
         "sleet", "snow", "showers-day", "showers-night", "thunderstorm", "hail", "sunrise", "sunset"]
SIZES = [64, 48, 28, 24]
BIG_SIZES = [85, 37, 32]
CF = {0x08: "LV_COLOR_FORMAT_I2", 0x09: "LV_COLOR_FORMAT_I4", 0x0A: "LV_COLOR_FORMAT_I8",
      0x14: "LV_COLOR_FORMAT_RGB565A8"}


def read_bin(path):
    with open(path, "rb") as f:
        data = f.read()
    magic, cf, flags, w, h, stride, _ = struct.unpack("<BBHHHHH", data[:12])
    if magic != 0x19 or cf not in CF:
        raise SystemExit("gecersiz ikon: %s" % path)
    return {"cf": cf, "w": w, "h": h, "stride": stride, "payload": data[12:]}


def ident(px, name):
    return "ic%d_%s" % (px, name.replace("-", "_"))


def generate():
    cpp = ["#include \"icons_embedded.h\"", ""]
    total = 0
    for px in SIZES:
        for name in NAMES:
            info = read_bin(os.path.join(SD_IMG, str(px), name + ".bin"))
            if info["w"] != px or info["h"] != px:
                raise SystemExit("boyut tutmuyor: %d/%s" % (px, name))
            sym = ident(px, name)
            p = info["payload"]
            total += len(p)
            cpp.append("static const uint8_t %s_data[] = {" % sym)
            for i in range(0, len(p), 20):
                cpp.append("    " + ", ".join("0x%02x" % b for b in p[i:i + 20]) + ",")
            cpp.append("};")
            cpp.append("static const lv_image_dsc_t %s = {" % sym)
            cpp.append("    {LV_IMAGE_HEADER_MAGIC, %s, 0, %d, %d, %d, 0}," % (CF[info["cf"]], px, px, info["stride"]))
            cpp.append("    sizeof(%s_data)," % sym)
            cpp.append("    %s_data," % sym)
            cpp.append("    nullptr,")
            cpp.append("    nullptr,")
            cpp.append("};")
            cpp.append("")
    for px in SIZES:
        cpp.append("const lv_image_dsc_t* const ICONS_%d[rc::IC_COUNT] = {" % px)
        for name in NAMES:
            cpp.append("    &%s," % ident(px, name))
        cpp.append("};")
        cpp.append("")
    hdr = ["#pragma once", "", "#include <lvgl.h>", "", "#include \"wmo_tr.h\"", ""]
    hdr += ["extern const lv_image_dsc_t* const ICONS_%d[rc::IC_COUNT];" % px for px in SIZES]
    hdr += ["extern const lv_image_dsc_t* const ICONS_%d[rc::IC_COUNT];" % px for px in BIG_SIZES]
    hdr.append("")
    with open(OUT_H, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(hdr))
    with open(OUT_CPP, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(cpp))
    print("embed_icons: %d ikon, %d bayt flash -> src/ui/icons_embedded.cpp" % (len(SIZES) * len(NAMES), total))
    return total


if __name__ == "__main__":
    generate()
