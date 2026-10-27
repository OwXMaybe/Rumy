#!/bin/bash
set -euo pipefail

if [ "$(id -u)" -ne 0 ]; then
  echo "Bu betik yonetici olarak calismali: sudo bash $0"
  exit 1
fi

KULLANICI="${SUDO_USER:-rumy}"
BOOT=/boot/firmware
TARIH=$(date +%Y%m%d-%H%M%S)

echo "== 1/6 Ekran surucusu (config.txt)"
cp "$BOOT/config.txt" "$BOOT/config.txt.yedek-$TARIH"
if ! grep -q '^dtoverlay=piscreen' "$BOOT/config.txt"; then
  printf '\n[all]\ndtparam=spi=on\ndtoverlay=piscreen,drm,speed=24000000\n' >> "$BOOT/config.txt"
  echo "   eklendi: dtparam=spi=on, dtoverlay=piscreen,drm,speed=24000000"
else
  echo "   zaten var"
fi

echo "== 2/6 Konsol yazilari ekrana karismasin (cmdline.txt)"
cp "$BOOT/cmdline.txt" "$BOOT/cmdline.txt.yedek-$TARIH"
SATIR=$(tr -d '\n' < "$BOOT/cmdline.txt")
for EK in fbcon=map:9 consoleblank=0 vt.global_cursor_default=0; do
  case " $SATIR " in
    *" $EK "*) ;;
    *) SATIR="$SATIR $EK" ;;
  esac
done
printf '%s\n' "$SATIR" > "$BOOT/cmdline.txt"
echo "   $SATIR"

echo "== 3/6 Rumy kullanici servisi acilista baslasin (linger)"
loginctl enable-linger "$KULLANICI"
echo "   $KULLANICI icin acik"

echo "== 4/6 Gunlukler yeniden baslatmadan sonra da kalsin (en fazla 50 MB)"
mkdir -p /etc/systemd/journald.conf.d /var/log/journal
printf '[Journal]\nStorage=persistent\nSystemMaxUse=50M\n' > /etc/systemd/journald.conf.d/rumy.conf
systemd-tmpfiles --create --prefix /var/log/journal
systemctl restart systemd-journald
echo "   tamam"

echo "== 5/6 cloud-init kapatilsin (ilk kurulum bitti; acilisi yaklasik 30 sn kisaltir)"
touch /etc/cloud/cloud-init.disabled
echo "   tamam"

echo "== 6/6 Imager'in WiFi ve sifre dosyalari herkesin okuyabildigi boot bolumunden kaldirilsin"
mkdir -p /root/imager-yedek
chmod 700 /root/imager-yedek
for F in user-data network-config; do
  if [ -f "$BOOT/$F" ]; then
    mv "$BOOT/$F" "/root/imager-yedek/$F-$TARIH"
    echo "   $F -> /root/imager-yedek/ (yalnizca yonetici okuyabilir)"
  fi
done

echo
echo "Bitti. Simdi yeniden baslat: sudo reboot"
