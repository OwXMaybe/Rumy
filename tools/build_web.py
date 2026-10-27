"""Web arayuzunu (web/index.html) gzip'leyip, ana ekran simgesini (web/icon.png) ve ifade seridini (web/yuzler.png)
oldugu gibi C++ dizisi olarak uretir.

PlatformIO pre-script olarak calisir; ayrica tek basina da calistirilabilir:
    python tools/build_web.py
Uretilen dosyalar: src/webui/web_assets.h, web_icon.h, web_faces.h  (yorum icermez)
"""
import gzip
import io
import os
import sys


def project_dir():
    try:
        Import("env")
        return env.subst("$PROJECT_DIR")
    except NameError:
        return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def write_array(out, name, data):
    lines = ["#pragma once", "#include <stdint.h>", "#include <stddef.h>", "",
             "static const uint8_t %s[] = {" % name]
    for i in range(0, len(data), 20):
        chunk = ", ".join("0x%02x" % b for b in data[i:i + 20])
        lines.append("    " + chunk + ",")
    lines.append("};")
    lines.append("static const size_t %s_LEN = %d;" % (name, len(data)))
    lines.append("")
    text = "\n".join(lines)
    old = None
    if os.path.exists(out):
        with open(out, "r", encoding="utf-8") as f:
            old = f.read()
    if old != text:
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)


def build(root):
    src = os.path.join(root, "web", "index.html")
    out = os.path.join(root, "src", "webui", "web_assets.h")
    if not os.path.exists(src):
        print("build_web: web/index.html yok, atlandi")
        return
    with open(src, "rb") as f:
        raw = f.read()
    buf = io.BytesIO()
    with gzip.GzipFile(fileobj=buf, mode="wb", compresslevel=9, mtime=0) as gz:
        gz.write(raw)
    data = buf.getvalue()
    write_array(out, "WEB_INDEX_GZ", data)
    icon = os.path.join(root, "web", "icon.png")
    with open(icon, "rb") as f:
        png = f.read()
    write_array(os.path.join(root, "src", "webui", "web_icon.h"), "WEB_ICON_PNG", png)
    with open(os.path.join(root, "web", "yuzler.png"), "rb") as f:
        faces = f.read()
    write_array(os.path.join(root, "src", "webui", "web_faces.h"), "WEB_FACES_PNG", faces)
    print("build_web: %d B html -> %d B gzip, simge %d B, ifadeler %d B" % (len(raw), len(data), len(png), len(faces)))


build(project_dir())
