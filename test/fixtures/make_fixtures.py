"""test/fixtures/*.json dosyalarindan testlerin kullandigi fixtures.h basligini uretir.

    python test/fixtures/make_fixtures.py
"""
import glob
import os

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    lines = ["#pragma once", ""]
    for path in sorted(glob.glob(os.path.join(HERE, "*.json"))):
        name = os.path.splitext(os.path.basename(path))[0].upper()
        with open(path, "r", encoding="utf-8") as f:
            text = f.read().strip()
        if ")JSON\"" in text:
            raise SystemExit("gecersiz ayrac: " + path)
        lines.append('static const char FX_%s[] = R"JSON(%s)JSON";' % (name, text))
        lines.append("")
    out = os.path.join(HERE, "fixtures.h")
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    print("yazildi:", out)


if __name__ == "__main__":
    main()
