"""Derlenen imaja gomulen dosya yollarindan kullanici klasorunu cikarir (PlatformIO pre-script).

Arduino-ESP32 ve ESP-IDF'in bazi kaynaklari hata mesajlari icin __FILE__ kullanir; yol oldugu gibi kalirsa imajda
bilgisayardaki kullanici adi gorunur. -fmacro-prefix-map ile PlatformIO klasoru "~/.platformio", proje klasoru "."
olarak yazilir. platformio.ini'de extra_scripts = pre:tools/path_map.py olarak calisir.
"""
Import("env")

pairs = [(env.subst("$PROJECT_CORE_DIR"), "~/.platformio"), (env.subst("$PROJECT_DIR"), ".")]
flags = []
for src, dst in pairs:
    for form in {src, src.replace("\\", "/")}:
        flags.append("-fmacro-prefix-map=%s=%s" % (form, dst))
env.Append(CCFLAGS=flags)
