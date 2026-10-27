#!/bin/bash
# Rumy Pi ikinci kurulum: acilisi hizlandirir ve ekran hizini sabitler. Bir kez calistirilir:
#   sudo bash ~/rumy/pi/kurulum2.sh && sudo reboot
# Tekrar calistirmak zararsizdir. Geri almak icin: sudo bash ~/rumy/pi/kurulum2.sh geri
set -euo pipefail

if [ "$(id -u)" -ne 0 ]; then
  echo "Bu betik yonetici olarak calismali: sudo bash $0"
  exit 1
fi

KULLANICI="${SUDO_USER:-rumy}"
EV=$(getent passwd "$KULLANICI" | cut -d: -f6)
KID=$(id -u "$KULLANICI")
BOOT=/boot/firmware
TARIH=$(date +%Y%m%d-%H%M%S)
UNIT=/etc/systemd/system/rumy.service
TAKAS=/etc/rpi/swap.conf.d/90-rumy.conf
GUNLUK=/etc/systemd/journald.conf.d/90-rumy-sik.conf
MODUL=/etc/modules-load.d/rumy-ekran.conf

kullanici_servisi() {
  systemctl --machine="$KULLANICI@.host" --user "$@" rumy.service 2>/dev/null ||
    runuser -u "$KULLANICI" -- env XDG_RUNTIME_DIR="/run/user/$KID" systemctl --user "$@" rumy.service 2>/dev/null ||
    true
}

if [ "${1:-}" = "geri" ]; then
  systemctl disable --now rumy.service 2>/dev/null || true
  rm -f "$UNIT" "$TAKAS" "$GUNLUK" "$MODUL"
  systemctl daemon-reload
  sed -i '/^core_freq_min=400$/d' "$BOOT/config.txt"
  kullanici_servisi enable --now
  echo "Geri alindi. Yeniden baslat: sudo reboot"
  exit 0
fi

echo "== 1/5 Rumy acilista erken baslasin (ag beklenmeden, yaklasik 16. saniyede)"
cat > "$UNIT" <<EOF
[Unit]
Description=Rumy masaustu arkadasi
DefaultDependencies=no
After=systemd-remount-fs.service systemd-journald.socket
Conflicts=shutdown.target
Before=shutdown.target
StartLimitIntervalSec=300
StartLimitBurst=10

[Service]
User=$KULLANICI
Group=$KULLANICI
WorkingDirectory=$EV
Environment=HOME=$EV
ExecStart=$EV/rumy/pi-build/rumy
Restart=always
RestartSec=2
RestartPreventExitStatus=75
TimeoutStopSec=5

[Install]
WantedBy=multi-user.target
EOF
kullanici_servisi disable --now
systemctl daemon-reload
systemctl enable rumy.service
if systemctl restart rumy.service; then
  echo "   $UNIT kuruldu ve basladi; kullanici servisi kapatildi"
else
  echo "   UYARI: rumy.service baslatilamadi, ayrinti: journalctl -u rumy -n 30"
fi

echo "== 2/5 Ekran hizi sabit kalsin (cekirdek saati 400 MHz'in altina inmesin)"
cp "$BOOT/config.txt" "$BOOT/config.txt.yedek-$TARIH"
if ! grep -q '^core_freq_min=' "$BOOT/config.txt"; then
  printf 'core_freq_min=400\n' >> "$BOOT/config.txt"
  echo "   eklendi: core_freq_min=400"
else
  echo "   zaten var: $(grep '^core_freq_min=' "$BOOT/config.txt")"
fi

echo "== 3/5 Takas yalnizca bellekte (zram); acilista 19 sn suren takas dosyasi hazirligi kalkar"
mkdir -p "$(dirname "$TAKAS")"
printf '[Main]\nMechanism=zram\n' > "$TAKAS"
echo "   $TAKAS yazildi"

echo "== 4/5 Gunluk diske dakikada bir yazilsin (elektrik kesilince son dakikalar kaybolmasin)"
mkdir -p "$(dirname "$GUNLUK")"
printf '[Journal]\nSyncIntervalSec=60\n' > "$GUNLUK"
systemctl restart systemd-journald
echo "   $GUNLUK yazildi"

echo "== 5/5 Ekran ve dokunmatik suruculeri acilisin basinda yuklensin (ekran ~20 sn erken gelir)"
printf 'spi_bcm2835\nili9486\nads7846\n' > "$MODUL"
echo "   $MODUL yazildi"

echo
echo "Bitti. Simdi yeniden baslat: sudo reboot"
