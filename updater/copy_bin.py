"""Guncelleyici derlemesinden sonra firmware.bin'i dist/Rumy-guncelleyici[-st7789].bin olarak kopyalar.

Ana derleme (tools/post_build.py) bu dosyayi tam imaja ve USB yuklemesine (ota_1 adresi) ekler.
"""
import os
import shutil

Import("env")


def after_build(source, target, env):
    root = os.path.dirname(env.subst("$PROJECT_DIR"))
    dist = os.path.join(root, "dist")
    os.makedirs(dist, exist_ok=True)
    name = "Rumy-guncelleyici" + ("" if env["PIOENV"] == "cyd" else "-" + env["PIOENV"].replace("cyd_", ""))
    app = os.path.join(env.subst("$BUILD_DIR"), "firmware.bin")
    shutil.copyfile(app, os.path.join(dist, name + ".bin"))
    print("guncelleyici: %s.bin %d B" % (name, os.path.getsize(app)))


env.AddPostAction("$BUILD_DIR/firmware.bin", after_build)
