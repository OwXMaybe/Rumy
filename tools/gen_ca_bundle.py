"""Rumy icin kirpilmis TLS kok sertifika paketi (esp_crt_bundle bicimi) uretir.

Arduino-ESP32 2.0.x WiFiClientSecure::setCACertBundle() bu bicimi bekler:
  [2 bayt sertifika sayisi]
  her sertifika icin: [2 bayt ad uzunlugu][2 bayt anahtar uzunlugu][konu adi DER][SubjectPublicKeyInfo DER]
Sertifikalar konu adi DER baytlarina gore sirali olmalidir (cihaz ikili arama yapar).

Kaynak: Mozilla kok deposu (Python 'certifi' paketi; PlatformIO'nun kendi Python'unda hazir gelir)
veya --pem ile verilen bir PEM dosyasi.

Kullanim:
    %USERPROFILE%/.platformio/penv/Scripts/python.exe tools/gen_ca_bundle.py
    python tools/gen_ca_bundle.py --pem cacert.pem

Ciktilar:
    src/net/ca_bundle.h     (yorumsuz C dizisi)
    certs/rumy_roots.pem    (secilen kok sertifikalar, inceleme icin)

Hangi kok neden var:
    GlobalSign Root R46      -> servisnet.afad.gov.tr (AFAD) zincirinin koku
    GlobalSign Root CA (R1)  -> date.nager.at zincirindeki capraz imzali GTS Root R4'un koku.
                                Mozilla deposundan cikarildi; certs/extra/ altindan SHA-256 ile dogrulanip eklenir.
                                Kaynak: https://secure.globalsign.net/cacert/Root-R1.crt (2028-01-28'e kadar gecerli)
    GTS Root R1 / R4         -> Google Trust Services zinciri kisa gelirse
    ISRG Root X1 / X2        -> Let's Encrypt'e gecis olursa
    DigiCert, USERTrust, Amazon, GlobalSign R3 -> saglayici degisikligine karsi yedek
"""
import argparse
import base64
import os
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_H = os.path.join(ROOT, "src", "net", "ca_bundle.h")
OUT_PEM = os.path.join(ROOT, "certs", "rumy_roots.pem")

LABELS = [
    "GlobalSign Root R46",
    "GlobalSign Root CA - R3",
    "GTS Root R1",
    "GTS Root R4",
    "ISRG Root X1",
    "ISRG Root X2",
    "DigiCert Global Root G2",
    "USERTrust RSA Certification Authority",
    "Amazon Root CA 1",
]


EXTRA_DIR = os.path.join(ROOT, "certs", "extra")
EXTRA = {
    "globalsign_root_r1.crt": ("GlobalSign Root CA",
                               "ebd41040e4bb3ec742c9e381d31ef2a41a48b6685c96e7cef3c1df6cd4331c99"),
}


def read_tlv(data, pos):
    tag = data[pos]
    pos += 1
    first = data[pos]
    pos += 1
    if first < 0x80:
        length = first
    else:
        n = first & 0x7F
        length = int.from_bytes(data[pos:pos + n], "big")
        pos += n
    return tag, pos, length


def element(data, pos):
    start = pos
    tag, body, length = read_tlv(data, pos)
    end = body + length
    return tag, start, body, end


def children(data, body, end):
    out = []
    pos = body
    while pos < end:
        tag, start, b, e = element(data, pos)
        out.append((tag, start, b, e))
        pos = e
    return out


def subject_and_spki(der):
    tag, _, body, end = element(der, 0)
    if tag != 0x30:
        raise ValueError("sertifika SEQUENCE degil")
    cert_parts = children(der, body, end)
    tbs_tag, _, tbs_body, tbs_end = cert_parts[0]
    tbs = children(der, tbs_body, tbs_end)
    idx = 0
    if tbs[0][0] == 0xA0:
        idx = 1
    subject = tbs[idx + 4]
    spki = tbs[idx + 5]
    return der[subject[1]:subject[3]], der[spki[1]:spki[3]]


def load_pem_blocks(text):
    blocks = []
    label = None
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        ln = lines[i]
        m = re.match(r'^# Label: "(.*)"', ln)
        if m:
            label = m.group(1)
        if ln.startswith("-----BEGIN CERTIFICATE-----"):
            j = i + 1
            b64 = []
            while not lines[j].startswith("-----END CERTIFICATE-----"):
                b64.append(lines[j].strip())
                j += 1
            pem = "\n".join(lines[i:j + 1]) + "\n"
            blocks.append((label, base64.b64decode("".join(b64)), pem))
            label = None
            i = j
        i += 1
    return blocks


def source_text(pem_path):
    if pem_path:
        with open(pem_path, "r", encoding="utf-8") as f:
            return f.read()
    try:
        import certifi
    except ImportError:
        sys.exit("certifi yok: PlatformIO Python'u ile calistirin ya da --pem verin")
    with open(certifi.where(), "r", encoding="utf-8") as f:
        return f.read()


def load_extra():
    import hashlib
    out = []
    for fname, (label, sha) in EXTRA.items():
        path = os.path.join(EXTRA_DIR, fname)
        if not os.path.exists(path):
            print("UYARI: ek kok yok:", fname)
            continue
        with open(path, "rb") as f:
            raw = f.read()
        if raw.startswith(b"-----BEGIN"):
            b64 = b"".join(ln.strip() for ln in raw.splitlines() if ln and not ln.startswith(b"-----"))
            der = base64.b64decode(b64)
        else:
            der = raw
        got = hashlib.sha256(der).hexdigest()
        if got != sha:
            sys.exit("HATA: %s parmak izi uyusmuyor (%s)" % (fname, got))
        enc = base64.b64encode(der).decode()
        body = [enc[i:i + 64] for i in range(0, len(enc), 64)]
        pem = "\n".join(["-----BEGIN CERTIFICATE-----"] + body + ["-----END CERTIFICATE-----", ""])
        out.append((label, der, pem))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--pem", help="Mozilla bicimli PEM paketi (etiket satirlariyla)")
    args = ap.parse_args()
    blocks = load_pem_blocks(source_text(args.pem))
    chosen = []
    missing = []
    for want in LABELS:
        hit = [b for b in blocks if b[0] == want]
        if not hit:
            missing.append(want)
            continue
        chosen.append(hit[0])
    if missing:
        print("UYARI: bulunamayan kokler:", ", ".join(missing))
    chosen.extend(load_extra())
    entries = []
    for label, der, pem in chosen:
        name, key = subject_and_spki(der)
        entries.append((name, key, label, pem))
    entries.sort(key=lambda e: e[0])
    blob = struct.pack(">H", len(entries))
    for name, key, _, _ in entries:
        blob += struct.pack(">HH", len(name), len(key)) + name + key

    lines = ["#pragma once", "#include <stddef.h>", "#include <stdint.h>", "",
             "static const uint8_t RUMY_CA_BUNDLE[] = {"]
    for i in range(0, len(blob), 20):
        lines.append("    " + ", ".join("0x%02x" % b for b in blob[i:i + 20]) + ",")
    lines.append("};")
    lines.append("static const size_t RUMY_CA_BUNDLE_LEN = %d;" % len(blob))
    lines.append("static const uint8_t RUMY_CA_COUNT = %d;" % len(entries))
    lines.append("")
    os.makedirs(os.path.dirname(OUT_H), exist_ok=True)
    with open(OUT_H, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))
    os.makedirs(os.path.dirname(OUT_PEM), exist_ok=True)
    with open(OUT_PEM, "w", encoding="utf-8", newline="\n") as f:
        for _, _, label, pem in entries:
            f.write("# %s\n" % label)
            f.write(pem)
    print("gen_ca_bundle: %d kok, %d bayt -> %s" % (len(entries), len(blob), os.path.relpath(OUT_H, ROOT)))
    for _, _, label, _ in entries:
        print("  -", label)


if __name__ == "__main__":
    main()
