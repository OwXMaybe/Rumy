"""Derleme sonrasi: dist/ klasorune Launcher ve USB icin .bin'leri kopyalar, boyut raporu yazar.

- dist/Rumy.bin            : sadece uygulama (Launcher SD'den bunu kurar; kablosuz guncelleme de bunu yollar)
- dist/Rumy-full-0x0.bin   : bootloader + bolum tablosu + uygulama + guncelleyici (esptool / web flasher, adres 0x0)
- dist/size_report.txt     : flash ve statik RAM kullanimi

Guncelleyici (updater/, ota_1 bolumu 0x290000) eksik ya da kaynaklarindan eskiyse once "pio run -d updater" ile
derlenir; USB yuklemesi (pio run -t upload) onu da ayni anda yazar.
"""
import glob
import os
import shutil
import subprocess

Import("env")

LAUNCHER_APP_LIMIT = 0x2A0000
APP_PART_LIMIT = 0x280000
UPDATER_OFFSET = 0x290000
UPDATER_LIMIT = 0x70000
TARGET_APP_LIMIT = 2048 * 1024


def _names(env):
    envname = env["PIOENV"]
    if envname == "cyd":
        return "Rumy"
    if envname == "cyd_st7789":
        return "Rumy-st7789"
    return "Rumy-" + envname


def _st7789(env):
    for d in env.get("CPPDEFINES", []):
        name = d[0] if isinstance(d, (list, tuple)) else d
        if name == "RUMY_PANEL_ST7789":
            return True
    return False


def _updater_bin(env):
    root = env.subst("$PROJECT_DIR")
    st = _st7789(env)
    path = os.path.join(root, "dist", "Rumy-guncelleyici%s.bin" % ("-st7789" if st else ""))
    sources = glob.glob(os.path.join(root, "updater", "src", "*.cpp")) + [
        os.path.join(root, "updater", "platformio.ini"),
        os.path.join(root, "lib", "rumy_core", "src", "fw_image.h"),
        os.path.join(root, "src", "board", "lgfx_cyd.h"),
        os.path.join(root, "src", "app", "portal_font.h"),
        os.path.join(root, "src", "app", "config.h"),
        os.path.join(root, "partitions", "rumy_4mb.csv"),
    ]
    newest = max(os.path.getmtime(p) for p in sources if os.path.exists(p))
    if not os.path.exists(path) or os.path.getmtime(path) < newest:
        cmd = [env.subst("$PYTHONEXE"), "-m", "platformio", "run", "-d", os.path.join(root, "updater"),
               "-e", "cyd_st7789" if st else "cyd"]
        print("post_build: guncelleyici derleniyor (%s)" % " ".join(cmd[3:]))
        try:
            subprocess.check_call(cmd, stdout=subprocess.DEVNULL)
        except Exception as exc:
            print("post_build: guncelleyici derlenemedi: %s" % exc)
    return path if os.path.exists(path) else None


UPDATER = _updater_bin(env)
if UPDATER and env.subst("$UPLOAD_PROTOCOL") == "esptool":
    env.Append(UPLOADERFLAGS=[hex(UPDATER_OFFSET), UPDATER])
elif not UPDATER:
    print("post_build: UYARI guncelleyici yok, kablosuz guncelleme calismayacak (pio run -d updater)")


def _section_sizes(env, elf):
    size_tool = env.subst("$SIZETOOL")
    try:
        out = subprocess.check_output([size_tool, "-A", elf], text=True)
    except Exception as exc:
        return {}, str(exc)
    sizes = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[0].startswith("."):
            try:
                sizes[parts[0]] = int(parts[1])
            except ValueError:
                pass
    return sizes, None


def after_build(source, target, env):
    root = env.subst("$PROJECT_DIR")
    build_dir = env.subst("$BUILD_DIR")
    dist = os.path.join(root, "dist")
    os.makedirs(dist, exist_ok=True)
    name = _names(env)
    app = os.path.join(build_dir, "firmware.bin")
    elf = os.path.join(build_dir, "firmware.elf")
    if not os.path.exists(app):
        return
    shutil.copyfile(app, os.path.join(dist, name + ".bin"))
    app_size = os.path.getsize(app)

    merged = os.path.join(dist, name + "-full-0x0.bin")
    boot = os.path.join(build_dir, "bootloader.bin")
    parts = os.path.join(build_dir, "partitions.bin")
    boot_app0 = None
    fw_dir = env.PioPlatform().get_package_dir("framework-arduinoespressif32")
    if fw_dir:
        cand = os.path.join(fw_dir, "tools", "partitions", "boot_app0.bin")
        if os.path.exists(cand):
            boot_app0 = cand
    esptool = os.path.join(env.PioPlatform().get_package_dir("tool-esptoolpy") or "", "esptool.py")
    if os.path.exists(boot) and os.path.exists(parts) and os.path.exists(esptool):
        cmd = [env.subst("$PYTHONEXE"), esptool, "--chip", "esp32", "merge_bin", "-o", merged,
               "--flash_mode", "dio", "--flash_freq", "40m", "--flash_size", "4MB",
               "0x1000", boot, "0x8000", parts]
        if boot_app0:
            cmd += ["0xe000", boot_app0]
        cmd += ["0x10000", app]
        if UPDATER:
            cmd += [hex(UPDATER_OFFSET), UPDATER]
        try:
            subprocess.check_call(cmd, stdout=subprocess.DEVNULL)
        except Exception as exc:
            print("post_build: merge_bin basarisiz: %s" % exc)

    upd_size = os.path.getsize(UPDATER) if UPDATER else 0
    sizes, err = _section_sizes(env, elf)
    dram = sum(v for k, v in sizes.items() if k in (".dram0.data", ".dram0.bss"))
    iram = sum(v for k, v in sizes.items() if k in (".iram0.text", ".iram0.vectors"))
    flash = sum(v for k, v in sizes.items() if k in (".flash.text", ".flash.rodata", ".flash.appdesc"))
    lines = [
        "Ortam            : %s" % env["PIOENV"],
        "Uygulama (.bin)  : %d B (%.2f MB)" % (app_size, app_size / 1048576.0),
        "Hedef < 2 MB     : %s" % ("OK" if app_size < TARGET_APP_LIMIT else "ASILDI"),
        "Uygulama bolumu  : %s (%d/%d B, %%%d)" % ("OK" if app_size <= APP_PART_LIMIT else "SIGMIYOR", app_size,
                                                APP_PART_LIMIT, app_size * 100 // APP_PART_LIMIT),
        "Guncelleyici     : %s" % ("%s (%d/%d B)" % ("OK" if upd_size <= UPDATER_LIMIT else "SIGMIYOR", upd_size,
                                                      UPDATER_LIMIT) if UPDATER else "YOK"),
        "Launcher alani   : %s (bos alan 0x%X)" % ("OK" if app_size < LAUNCHER_APP_LIMIT else "ASILDI", LAUNCHER_APP_LIMIT),
        "Statik DRAM      : %d B (.data + .bss)" % dram,
        "IRAM             : %d B" % iram,
        "Flash kod+veri   : %d B" % flash,
    ]
    if err:
        lines.append("size araci hatasi: %s" % err)
    for k in sorted(sizes):
        lines.append("  %-22s %8d" % (k, sizes[k]))
    report = "\n".join(lines)
    with open(os.path.join(dist, "size_report_%s.txt" % env["PIOENV"]), "w", encoding="utf-8") as f:
        f.write(report + "\n")
    print("\n" + report + "\n")


env.AddPostAction("$BUILD_DIR/firmware.bin", after_build)
