"""Dayaniklilik testi kaydedici ve ozetleyici.

`pio run -e cyd_endurance -t upload` ile yuklenen surum her 60 sn'de bir "[diag]" satiri basar,
ekranlari 5 sn'de bir degistirir ve verileri 30 sn'de bir ceker. Bu betik seri porttan (ya da
kayitli bir log dosyasindan) bu satirlari okuyup CSV'ye yazar ve sonunda ozet cikarir.

Kullanim:
    python tools/endurance_log.py --port COM7 --hours 24 --csv dist/endurance.csv
    python tools/endurance_log.py --file kayit.log
Gerekenler: pyserial (sadece --port icin)

Ozet: en dusuk bos heap, heap egilimi (B/saat), en buyuk blok, LVGL parcalanmasi, TLS tepesi,
yigin (stack) en dusukleri, ag hatalari ve beklenmeyen yeniden baslatmalar.
"""
import argparse
import csv
import re
import sys
import time

DIAG = re.compile(
    r"up=(?P<up>\d+)s heap=(?P<heap>\d+) min=(?P<min>\d+) blok=(?P<blk>\d+) lvgl=(?P<lvu>\d+)%/(?P<lvf>\d+)% "
    r"fps=(?P<fps>\d+) yigin ui=(?P<ui>\d+) ag=(?P<ag>\d+) wiz=(?P<wiz>\d+) "
    r"tls=(?P<tls>\d+) ert=(?P<ert>\d+) wifi=(?P<wifi>-?\d+)")
BOOT = re.compile(r"Rumy (\S+) basliyor.*neden: (.+?), acilis #(\d+)")
NETERR = re.compile(r"net\s+(\S+) hata (-?\d+)")
FIELDS = ["zaman", "up", "heap", "min", "blk", "lvu", "lvf", "fps", "ui", "ag", "wiz", "tls", "ert", "wifi"]


def lines_from_serial(port, hours):
    import serial
    end = time.time() + hours * 3600
    with serial.Serial(port, 460800, timeout=1) as s:
        while time.time() < end:
            raw = s.readline()
            if raw:
                yield raw.decode("utf-8", "replace").rstrip()


def lines_from_file(path):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for ln in f:
            yield ln.rstrip()


def slope(xs, ys):
    n = len(xs)
    if n < 3:
        return 0.0
    sx, sy = sum(xs), sum(ys)
    sxx = sum(x * x for x in xs)
    sxy = sum(x * y for x, y in zip(xs, ys))
    den = n * sxx - sx * sx
    return (n * sxy - sx * sy) / den if den else 0.0


def main():
    ap = argparse.ArgumentParser()
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--port")
    src.add_argument("--file")
    ap.add_argument("--hours", type=float, default=24.0)
    ap.add_argument("--csv", default="endurance.csv")
    ap.add_argument("--echo", action="store_true", help="tum satirlari ekrana da yaz")
    args = ap.parse_args()

    rows = []
    boots = []
    errors = {}
    stream = lines_from_serial(args.port, args.hours) if args.port else lines_from_file(args.file)
    with open(args.csv, "w", newline="", encoding="utf-8") as out:
        w = csv.writer(out)
        w.writerow(FIELDS)
        try:
            for ln in stream:
                if args.echo:
                    print(ln)
                m = DIAG.search(ln)
                if m:
                    row = [time.strftime("%Y-%m-%d %H:%M:%S")] + [int(m.group(k)) for k in FIELDS[1:]]
                    rows.append(row)
                    w.writerow(row)
                    out.flush()
                    if not args.echo:
                        print("up %6ds  heap %6d  min %6d  blok %6d  lvgl %2d%%/%2d%%  fps %2d  tls %5d" % (
                            row[1], row[2], row[3], row[4], row[5], row[6], row[9], row[13]))
                    continue
                b = BOOT.search(ln)
                if b:
                    boots.append((b.group(1), b.group(2), int(b.group(3))))
                    print("ACILIS:", ln)
                    continue
                e = NETERR.search(ln)
                if e:
                    errors[e.group(1)] = errors.get(e.group(1), 0) + 1
        except KeyboardInterrupt:
            pass

    if not rows:
        print("Hic [diag] satiri bulunamadi.")
        return
    col = lambda i: [r[i] for r in rows]
    hours = [r[1] / 3600.0 for r in rows]
    trend = slope(hours, col(2))
    print("\n===== Dayaniklilik ozeti =====")
    print("Sure            : %.1f saat, %d olcum" % ((rows[-1][1] - rows[0][1]) / 3600.0, len(rows)))
    print("Bos heap        : ilk %d, son %d, en dusuk %d" % (rows[0][2], rows[-1][2], min(col(3))))
    print("Heap egilimi    : %+.0f B/saat %s" % (trend, "(sizinti supheli)" if trend < -300 else "(kararli)"))
    print("En buyuk blok   : en dusuk %d" % min(col(4)))
    print("LVGL            : en yuksek kullanim %%%d, en yuksek parcalanma %%%d" % (max(col(5)), max(col(6))))
    print("FPS             : %d..%d" % (min(col(9)), max(col(9))))
    print("Yigin bos (min) : ui %d, ag %d, wiz %d" % (min(col(10)), min(col(11)), min(col(12))))
    print("TLS tepe        : %d B, ertelenen %d" % (max(col(13)), max(col(14))))
    print("Ag hatalari     : %s" % (", ".join("%s=%d" % kv for kv in sorted(errors.items())) or "yok"))
    unexpected = [b for b in boots[1:] if b[1] not in ("yazılım",)]
    print("Yeniden baslama : %d (beklenmeyen %d)" % (max(0, len(boots) - 1), len(unexpected)))
    for b in unexpected:
        print("   ", b)
    print("CSV             : %s" % args.csv)


if __name__ == "__main__":
    main()
