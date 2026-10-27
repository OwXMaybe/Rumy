#!/bin/bash
# Ekranin SPI hizini degistirir. Yeniden baslattiktan sonra ekranda bozulma (renkli nokta, kayik satir) var mi bakilir.
#   sudo bash ~/rumy/pi/spi_hiz.sh 32 && sudo reboot      bozulma varsa: sudo bash ~/rumy/pi/spi_hiz.sh 24 && sudo reboot
# Pi Zero W'da cekirdek saati 400 MHz iken gercek hiz: 24->22,2  28->25,0  32->28,6  36->33,3  40->40,0 MHz.
set -euo pipefail

if [ "$(id -u)" -ne 0 ]; then
  echo "Bu betik yonetici olarak calismali: sudo bash $0 <MHz>"
  exit 1
fi

MHZ="${1:-}"
if ! [[ "$MHZ" =~ ^[0-9]+$ ]] || [ "$MHZ" -lt 10 ] || [ "$MHZ" -gt 50 ]; then
  echo "kullanim: sudo bash $0 <10-50 arasi MHz>"
  exit 1
fi

BOOT=/boot/firmware
if ! grep -q '^dtoverlay=piscreen,drm' "$BOOT/config.txt"; then
  echo "config.txt'de dtoverlay=piscreen,drm satiri yok; once kurulum.sh calistirilmali"
  exit 1
fi
cp "$BOOT/config.txt" "$BOOT/config.txt.yedek-$(date +%Y%m%d-%H%M%S)"
sed -i -E "s/^(dtoverlay=piscreen,drm)(,speed=[0-9]+)?/\1,speed=${MHZ}000000/" "$BOOT/config.txt"
grep '^dtoverlay=piscreen' "$BOOT/config.txt"
echo "Simdi yeniden baslat: sudo reboot"
