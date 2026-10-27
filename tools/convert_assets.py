"""Rumy gorsel varlik hatti.

Meteocons (Bas Milius, MIT) SVG -> hedef boyutta PNG (resvg) -> paletli PNG (Pillow)
-> LVGL 9 binary (LVGL'nin kendi scripts/LVGLImage.py betigi).

Uretilenler
  sd_card/rumy/img/64/*.bin   I8   hava ekrani ana ikon
  sd_card/rumy/img/48/*.bin   I8   ana ekran hava kutusu
  sd_card/rumy/img/28/*.bin   I4   saatlik serit (+ gun dogumu/batimi)
  sd_card/rumy/img/24/*.bin   I4   7 gunluk liste
  src/ui/icons_embedded.*          ayni ikonlar flash'a gomulu (embed_icons.py); cihaz SD'den okumaz
  src/rumy/accessories.*           Rumy aksesuarlari (bayrak, parti sapkasi, semsiye) I4 32 px
  docs/icon_ram.md                 her ikonun RAM maliyeti
  docs/icons_preview.png           kalite kontrolu icin onizleme

  src/ui/icons_big.cpp             480x320 arayuz icin 85/37/32 px ikonlar (64 px CYD'ninkiyle ortak)
  src/rumy/accessories_big.cpp     480x320 arayuz icin 43 px aksesuarlar
                                   (ikisi de yalnizca RUMY_SCREEN_H >= 320 derlemelerinde derlenir)

Kullanim:
    python tools/convert_assets.py --buyuk   yalnizca 480x320 ikonlari ve aksesuarlari
    python tools/convert_assets.py            (varsayilan: buyuk=fill, kucuk=flat)
    python tools/convert_assets.py --small-style fill
    python tools/convert_assets.py --small-format RGB565A8   (I4 kalitesi yetmezse)

Gerekenler: pip install resvg-py pypng lz4 pillow
"""
import argparse
import io
import math
import os
import shutil
import struct
import subprocess
import sys
import tarfile
import urllib.request

from PIL import Image, ImageDraw, features

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CACHE = os.path.join(ROOT, "tools", ".cache")
MC_URL = "https://registry.npmjs.org/@meteocons/svg-static/-/svg-static-0.1.0.tgz"
MC_DIR = os.path.join(CACHE, "meteocons")
SD_IMG = os.path.join(ROOT, "sd_card", "rumy", "img")
DOCS = os.path.join(ROOT, "docs")

ICONS = [
    ("clear-day", "clear-day"),
    ("clear-night", "clear-night"),
    ("partly-cloudy-day", "partly-cloudy-day"),
    ("partly-cloudy-night", "partly-cloudy-night"),
    ("overcast", "overcast"),
    ("fog", "fog"),
    ("drizzle", "drizzle"),
    ("rain", "rain"),
    ("sleet", "sleet"),
    ("snow", "snow"),
    ("showers-day", "partly-cloudy-day-rain"),
    ("showers-night", "partly-cloudy-night-rain"),
    ("thunderstorm", "thunderstorms-rain"),
    ("hail", "hail"),
    ("sunrise", "sunrise"),
    ("sunset", "sunset"),
]

def log(*a):
    print(*a, flush=True)


def ensure_meteocons():
    marker = os.path.join(MC_DIR, "package", "fill", "clear-day.svg")
    if os.path.exists(marker):
        return os.path.join(MC_DIR, "package")
    os.makedirs(MC_DIR, exist_ok=True)
    tgz = os.path.join(CACHE, "meteocons.tgz")
    if not os.path.exists(tgz):
        log("indiriliyor:", MC_URL)
        with urllib.request.urlopen(MC_URL, timeout=120) as r, open(tgz, "wb") as f:
            f.write(r.read())
    with tarfile.open(tgz, "r:gz") as t:
        t.extractall(MC_DIR)
    return os.path.join(MC_DIR, "package")


def find_lvglimage():
    cand = os.path.join(ROOT, ".pio", "libdeps", "cyd", "lvgl", "scripts", "LVGLImage.py")
    if os.path.exists(cand):
        return cand
    os.makedirs(CACHE, exist_ok=True)
    path = os.path.join(CACHE, "LVGLImage.py")
    if not os.path.exists(path):
        url = "https://raw.githubusercontent.com/lvgl/lvgl/v9.5.0/scripts/LVGLImage.py"
        log("indiriliyor:", url)
        with urllib.request.urlopen(url, timeout=60) as r, open(path, "wb") as f:
            f.write(r.read())
    return path


def render_svg(path, px):
    import resvg_py
    data = resvg_py.svg_to_bytes(svg_path=path, width=px, height=px)
    return Image.open(io.BytesIO(bytes(data))).convert("RGBA")


def content_zoom(mc, style):
    worst = 0.0
    for _, src in ICONS + [("umbrella", "umbrella")]:
        im = render_svg(os.path.join(mc, style, src + ".svg"), 512)
        bbox = im.getchannel("A").point(lambda a: 255 if a > 8 else 0).getbbox()
        if not bbox:
            continue
        x0, y0, x1, y1 = bbox
        worst = max(worst, 256 - x0, x1 - 256, 256 - y0, y1 - 256)
    if worst <= 0:
        return 1.0
    return max(1.0, min(1.35, 256.0 / (worst + 6)))


def render_icon(mc, style, name, px, zoom):
    big = int(round(px * zoom))
    im = render_svg(os.path.join(mc, style, name + ".svg"), big)
    off = (big - px) // 2
    return im.crop((off, off, off + px, off + px))


def quantize(im, colors):
    method = Image.Quantize.LIBIMAGEQUANT if features.check_feature("libimagequant") else Image.Quantize.FASTOCTREE
    q = im.quantize(colors=colors, method=method, dither=Image.Dither.NONE)
    pal = q.getpalette(rawmode="RGBA") or []
    entries = [tuple(pal[i:i + 4]) for i in range(0, len(pal), 4)][:colors]
    while len(entries) < colors:
        entries.append((0, 0, 0, 0))
    return q, entries


def save_indexed_png(indices_img, entries, path):
    import png
    w, h = indices_img.size
    raw = indices_img.tobytes()
    rows = [list(raw[y * w:(y + 1) * w]) for y in range(h)]
    bitdepth = 8 if len(entries) > 16 else (4 if len(entries) > 4 else (2 if len(entries) > 2 else 1))
    writer = png.Writer(w, h, palette=entries, bitdepth=bitdepth)
    with open(path, "wb") as f:
        writer.write(f, rows)


def to_bin(lvimg, png_path, cf, outdir, name):
    os.makedirs(outdir, exist_ok=True)
    named = os.path.join(os.path.dirname(png_path), "named_" + os.path.basename(outdir), name + ".png")
    os.makedirs(os.path.dirname(named), exist_ok=True)
    shutil.copyfile(png_path, named)
    cmd = [sys.executable, lvimg, "--ofmt", "BIN", "--cf", cf, "--align", "1", "--compress", "NONE",
           "-o", outdir, named]
    subprocess.check_call(cmd, stdout=subprocess.DEVNULL)
    out = os.path.join(outdir, name + ".bin")
    if not os.path.exists(out):
        raise SystemExit("LVGLImage.py cikti uretmedi: " + out)
    return out


CF_NAMES = {0x07: "I1", 0x08: "I2", 0x09: "I4", 0x0A: "I8", 0x14: "RGB565A8", 0x0B: "A1", 0x0D: "A4", 0x0E: "A8"}


def read_bin(path):
    with open(path, "rb") as f:
        data = f.read()
    magic, cf, flags, w, h, stride, _ = struct.unpack("<BBHHHHH", data[:12])
    return {"magic": magic, "cf": cf, "flags": flags, "w": w, "h": h, "stride": stride, "payload": data[12:],
            "size": len(data)}


def decode_bin(info):
    cf = CF_NAMES.get(info["cf"], "?")
    w, h, stride, p = info["w"], info["h"], info["stride"], info["payload"]
    img = Image.new("RGBA", (w, h))
    px = img.load()
    if cf in ("I1", "I2", "I4", "I8"):
        bpp = {"I1": 1, "I2": 2, "I4": 4, "I8": 8}[cf]
        n = 1 << bpp
        pal = [(p[i * 4 + 2], p[i * 4 + 1], p[i * 4 + 0], p[i * 4 + 3]) for i in range(n)]
        data = p[n * 4:]
        for y in range(h):
            row = data[y * stride:(y + 1) * stride]
            for x in range(w):
                bit = x * bpp
                byte = row[bit // 8]
                shift = 8 - bpp - (bit % 8)
                idx = (byte >> shift) & (n - 1)
                px[x, y] = pal[idx]
    elif cf == "RGB565A8":
        rgb = p[:w * h * 2]
        a = p[w * h * 2:]
        for y in range(h):
            for x in range(w):
                v = rgb[(y * w + x) * 2] | (rgb[(y * w + x) * 2 + 1] << 8)
                r = ((v >> 11) & 0x1F) * 255 // 31
                g = ((v >> 5) & 0x3F) * 255 // 63
                b = (v & 0x1F) * 255 // 31
                px[x, y] = (r, g, b, a[y * w + x])
    return img


def c_array(name, info, comment_free=True):
    payload = info["payload"]
    lines = ["static const uint8_t %s_data[] = {" % name]
    for i in range(0, len(payload), 20):
        lines.append("    " + ", ".join("0x%02x" % b for b in payload[i:i + 20]) + ",")
    lines.append("};")
    cfname = {0x08: "LV_COLOR_FORMAT_I2", 0x09: "LV_COLOR_FORMAT_I4", 0x0A: "LV_COLOR_FORMAT_I8",
              0x14: "LV_COLOR_FORMAT_RGB565A8"}[info["cf"]]
    lines.append("const lv_image_dsc_t %s = {" % name)
    lines.append("    {LV_IMAGE_HEADER_MAGIC, %s, 0, %d, %d, %d, 0}," % (cfname, info["w"], info["h"], info["stride"]))
    lines.append("    sizeof(%s_data)," % name)
    lines.append("    %s_data," % name)
    lines.append("    nullptr,")
    lines.append("    nullptr,")
    lines.append("};")
    return "\n".join(lines)


def draw_flag(px):
    s = 256
    im = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((18, 20, 34, 240), radius=8, fill=(190, 196, 205, 255))
    fx0, fy0, fx1, fy1 = 34, 32, 246, 176
    d.rectangle((fx0, fy0, fx1, fy1), fill=(227, 10, 23, 255))
    cy = (fy0 + fy1) / 2
    fh = fy1 - fy0
    r1 = fh * 0.25
    cx1 = fx0 + fh * 0.5
    d.ellipse((cx1 - r1, cy - r1, cx1 + r1, cy + r1), fill=(255, 255, 255, 255))
    r2 = fh * 0.2
    cx2 = cx1 + fh * 0.0625
    d.ellipse((cx2 - r2, cy - r2, cx2 + r2, cy + r2), fill=(227, 10, 23, 255))
    sr = fh * 0.125
    scx = cx1 + fh * 0.333
    pts = []
    for i in range(10):
        ang = math.pi + i * math.pi / 5
        rr = sr if i % 2 == 0 else sr * 0.4
        pts.append((scx + rr * math.cos(ang), cy + rr * math.sin(ang)))
    d.polygon(pts, fill=(255, 255, 255, 255))
    return im.resize((px, px), Image.LANCZOS)


def draw_party(px):
    s = 256
    im = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    apex = (128, 30)
    base_l, base_r = (48, 230), (208, 230)
    d.polygon([apex, base_r, base_l], fill=(255, 92, 170, 255))
    for i, col in enumerate([(255, 214, 64, 255), (64, 224, 240, 255), (255, 214, 64, 255)]):
        t0 = 0.28 + i * 0.24
        t1 = t0 + 0.1
        def lerp(a, b, t):
            return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)
        d.polygon([lerp(apex, base_l, t0), lerp(apex, base_r, t0), lerp(apex, base_r, t1), lerp(apex, base_l, t1)],
                  fill=col)
    d.ellipse((104, 6, 152, 54), fill=(255, 255, 255, 255))
    d.rounded_rectangle((40, 222, 216, 246), radius=12, fill=(64, 224, 240, 255))
    return im.resize((px, px), Image.LANCZOS)


def preview_sheet(rows, out):
    cell = 76
    width = cell * max(len(r[1]) for r in rows) + 90
    height = cell * len(rows)
    sheet = Image.new("RGBA", (width, height), (0, 0, 0, 255))
    top, bot = (46, 111, 216), (20, 26, 40)
    for y in range(height):
        t = y / max(1, height - 1)
        c = tuple(int(top[i] * (1 - t) + bot[i] * t) for i in range(3)) + (255,)
        ImageDraw.Draw(sheet).line([(0, y), (width, y)], fill=c)
    d = ImageDraw.Draw(sheet)
    for ri, (label, imgs) in enumerate(rows):
        d.text((6, ri * cell + 30), label, fill=(255, 255, 255, 255))
        for ci, im in enumerate(imgs):
            x = 90 + ci * cell + (cell - im.width) // 2
            y = ri * cell + (cell - im.height) // 2
            sheet.alpha_composite(im, (x, y))
    sheet.convert("RGB").save(out)


BIG_GUARD = "#if defined(RUMY_SCREEN_H) && RUMY_SCREEN_H >= 320"
BIG_ACC = 43


def accessory_images(mc, px, zoom):
    umb = render_icon(mc, "fill", "umbrella", px, zoom)
    return (("ACC_FLAG_IMG", draw_flag(px)), ("ACC_PARTY_IMG", draw_party(px)), ("ACC_UMBRELLA_IMG", umb))


def build_big(mc, lvimg, work, args, zoom_large, zoom_small):
    import embed_icons
    plan = [
        (85, "I8", args.large_style, zoom_large),
        (37, args.small_format, args.small_style, zoom_small),
        (32, args.small_format, args.small_style, zoom_small),
    ]
    cpp = [BIG_GUARD, "#include \"icons_embedded.h\"", ""]
    total = 0
    preview_rows = []
    for px, cf, style, zoom in plan:
        outdir = os.path.join(work, "big", str(px))
        previews = []
        for ours, src in ICONS:
            im = render_icon(mc, style, src, px, zoom)
            pngp = os.path.join(work, "big_%d_%s.png" % (px, ours))
            if cf in ("I4", "I8"):
                q, entries = quantize(im, 16 if cf == "I4" else 256)
                save_indexed_png(q, entries, pngp)
            else:
                im.save(pngp)
            info = read_bin(to_bin(lvimg, pngp, cf, outdir, ours))
            previews.append(decode_bin(info))
            sym = embed_icons.ident(px, ours)
            total += len(info["payload"])
            cpp.append(c_array(sym, info).replace("const lv_image_dsc_t %s =" % sym,
                                                  "static const lv_image_dsc_t %s =" % sym))
            cpp.append("")
        preview_rows.append(("%dpx %s" % (px, cf), previews))
        cpp.append("const lv_image_dsc_t* const ICONS_%d[rc::IC_COUNT] = {" % px)
        for ours, _ in ICONS:
            cpp.append("    &%s," % embed_icons.ident(px, ours))
        cpp.append("};")
        cpp.append("")
    cpp.append("#endif")
    cpp.append("")
    with open(os.path.join(ROOT, "src", "ui", "icons_big.cpp"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(cpp))

    acc_cpp = [BIG_GUARD, "#include \"accessories.h\"", ""]
    acc_preview = []
    for name, im in accessory_images(mc, BIG_ACC, zoom_large):
        q, entries = quantize(im, 16)
        pngp = os.path.join(work, "big_" + name + ".png")
        save_indexed_png(q, entries, pngp)
        info = read_bin(to_bin(lvimg, pngp, "I4", os.path.join(work, "big", "acc"), name.lower()))
        acc_preview.append(decode_bin(info))
        acc_cpp.append(c_array(name, info).replace("static const uint8_t %s_data" % name,
                                                   "static const uint8_t %s_data" % name.lower())
                       .replace("sizeof(%s_data)" % name, "sizeof(%s_data)" % name.lower())
                       .replace("    %s_data," % name, "    %s_data," % name.lower()))
        acc_cpp.append("")
    acc_cpp.append("#endif")
    acc_cpp.append("")
    with open(os.path.join(ROOT, "src", "rumy", "accessories_big.cpp"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(acc_cpp))
    preview_rows.append(("aksesuar %d I4" % BIG_ACC, acc_preview))
    preview_sheet(preview_rows, os.path.join(DOCS, "icons_preview_buyuk.png"))
    log("480x320 ikonlari: %d bayt flash -> src/ui/icons_big.cpp, aksesuarlar -> src/rumy/accessories_big.cpp"
        % total)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--large-style", default="fill")
    ap.add_argument("--small-style", default="flat")
    ap.add_argument("--small-format", default="I4", choices=["I4", "I8", "RGB565A8"])
    ap.add_argument("--buyuk", action="store_true", help="yalnizca 480x320 ikonlari ve aksesuarlari")
    args = ap.parse_args()

    if args.buyuk:
        mc = ensure_meteocons()
        lvimg = find_lvglimage()
        work = os.path.join(CACHE, "work_big")
        shutil.rmtree(work, ignore_errors=True)
        os.makedirs(work, exist_ok=True)
        os.makedirs(DOCS, exist_ok=True)
        build_big(mc, lvimg, work, args, content_zoom(mc, args.large_style), content_zoom(mc, args.small_style))
        return

    mc = ensure_meteocons()
    lvimg = find_lvglimage()
    work = os.path.join(CACHE, "work")
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work, exist_ok=True)
    os.makedirs(DOCS, exist_ok=True)

    zoom_large = content_zoom(mc, args.large_style)
    zoom_small = content_zoom(mc, args.small_style)
    log("yakinlastirma: buyuk=%.3f kucuk=%.3f" % (zoom_large, zoom_small))

    plan = [
        (64, "I8", args.large_style, zoom_large),
        (48, "I8", args.large_style, zoom_large),
        (28, args.small_format, args.small_style, zoom_small),
        (24, args.small_format, args.small_style, zoom_small),
    ]
    table = []
    preview_rows = []
    for px, cf, style, zoom in plan:
        outdir = os.path.join(SD_IMG, str(px))
        shutil.rmtree(outdir, ignore_errors=True)
        os.makedirs(outdir, exist_ok=True)
        previews = []
        for ours, src in ICONS:
            im = render_icon(mc, style, src, px, zoom)
            pngp = os.path.join(work, "%d_%s.png" % (px, ours))
            if cf in ("I4", "I8"):
                q, entries = quantize(im, 16 if cf == "I4" else 256)
                save_indexed_png(q, entries, pngp)
            else:
                im.save(pngp)
            binp = to_bin(lvimg, pngp, cf, outdir, ours)
            info = read_bin(binp)
            previews.append(decode_bin(info))
            table.append((px, ours, CF_NAMES.get(info["cf"], "?"), info["size"], len(info["payload"]), px * px * 4))
        preview_rows.append(("%dpx %s" % (px, cf), previews))

    import embed_icons
    embed_icons.generate()

    acc = []
    acc_preview = []
    umb = render_icon(mc, "fill", "umbrella", 32, zoom_large)
    for name, im in (("ACC_FLAG_IMG", draw_flag(32)), ("ACC_PARTY_IMG", draw_party(32)), ("ACC_UMBRELLA_IMG", umb)):
        q, entries = quantize(im, 16)
        pngp = os.path.join(work, name + ".png")
        save_indexed_png(q, entries, pngp)
        binp = to_bin(lvimg, pngp, "I4", os.path.join(work, "acc"), name.lower())
        info = read_bin(binp)
        acc.append((name, info))
        acc_preview.append(decode_bin(info))
        table.append((32, name, "I4 (flash)", info["size"], 0, 32 * 32 * 4))
    preview_rows.append(("aksesuar 32 I4", acc_preview))
    cpp = ["#if !defined(RUMY_SCREEN_H) || RUMY_SCREEN_H < 320", "#include \"accessories.h\"", ""]
    for name, info in acc:
        cpp.append(c_array(name, info).replace("static const uint8_t %s_data" % name,
                                                 "static const uint8_t %s_data" % name.lower())
                   .replace("sizeof(%s_data)" % name, "sizeof(%s_data)" % name.lower())
                   .replace("    %s_data," % name, "    %s_data," % name.lower()))
        cpp.append("")
    cpp.append("#endif")
    cpp.append("")
    hdr = ["#pragma once", "", "#include <lvgl.h>", ""] + ["extern const lv_image_dsc_t %s;" % n for n, _ in acc] + [""]
    with open(os.path.join(ROOT, "src", "rumy", "accessories.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(hdr))
    with open(os.path.join(ROOT, "src", "rumy", "accessories.cpp"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(cpp))

    preview_sheet(preview_rows, os.path.join(DOCS, "icons_preview.png"))

    lines = ["# İkon boyutları", "",
             "Bütün ikonlar flash'a gömülüdür ve oradan satır satır çözülerek çizilir; RAM maliyeti yoktur.",
             "Flash sütunu: ikonun flash'ta kapladığı bayt (dosya başlığı hariç).",
             "LVGL varsayılan: aynı ikon LVGL'nin yerleşik decoder'ı ile tam ARGB8888'e açılsaydı gereken RAM.", "",
             "| Boyut | İkon | Format | Dosya (B) | Flash (B) | LVGL varsayılan (B) |",
             "|---:|---|---|---:|---:|---:|"]
    for px, name, cf, size, payload, argb in table:
        lines.append("| %d | %s | %s | %d | %d | %d |" % (px, name, cf, size, payload, argb))
    sums = {}
    for px, name, cf, size, payload, argb in table:
        if payload:
            sums.setdefault(px, []).append(payload)
    lines.append("")
    lines.append("| Boyut | İkon başına en fazla (B) |")
    lines.append("|---:|---:|")
    for px in sorted(sums, reverse=True):
        lines.append("| %d | %d |" % (px, max(sums[px])))
    lines.append("")
    with open(os.path.join(DOCS, "icon_ram.md"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))

    total = sum(os.path.getsize(os.path.join(dp, fn)) for dp, _, fns in os.walk(SD_IMG) for fn in fns)
    log("SD ikonlari: %d dosya, %d bayt" % (sum(len(f) for _, _, f in os.walk(SD_IMG)), total))
    for px in sorted(sums, reverse=True):
        log("  %dpx: ikon basina en fazla %d B" % (px, max(sums[px])))


if __name__ == "__main__":
    main()
