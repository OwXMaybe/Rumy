#!/bin/bash
# Rumy Pi ucuncu kurulum: servis bekcisi. Rumy takilirsa (30 sn bildirim gelmezse) systemd onu yeniden baslatir.
# Bir kez calistirilir:  sudo bash ~/rumy/pi/kurulum3.sh
# Tekrar calistirmak zararsizdir. Geri almak icin: sudo bash ~/rumy/pi/kurulum3.sh geri
set -euo pipefail

if [ "$(id -u)" -ne 0 ]; then
  echo "Bu betik yonetici olarak calismali: sudo bash $0"
  exit 1
fi

UNIT=/etc/systemd/system/rumy.service
EK=/etc/systemd/system/rumy.service.d/90-rumy-bekci.conf

if [ "${1:-}" = "geri" ]; then
  rm -f "$EK"
  rmdir --ignore-fail-on-non-empty "$(dirname "$EK")" 2>/dev/null || true
  systemctl daemon-reload
  systemctl restart rumy.service || true
  echo "Geri alindi; bekci kapali."
  exit 0
fi

if [ ! -f "$UNIT" ]; then
  echo "$UNIT yok; once kurulum2.sh calistirilmali"
  exit 1
fi
IKILI=$(sed -n 's/^ExecStart=//p' "$UNIT" | head -n 1)
if [ -z "$IKILI" ] || ! grep -q 'systemd bekcisi' "$IKILI"; then
  echo "Yuklu Rumy ($IKILI) bekciyi desteklemiyor; once yeni surum yuklenmeli (pi_deploy.py). Hicbir sey degismedi."
  exit 1
fi

echo "== 1/1 Servis bekcisi (WatchdogSec=30)"
mkdir -p "$(dirname "$EK")"
printf '[Service]\nWatchdogSec=30\nNotifyAccess=main\n' > "$EK"
systemctl daemon-reload
systemctl restart rumy.service
sleep 3
echo "   $EK yazildi; servis yeniden basladi"
echo "   $(systemctl show rumy.service -p WatchdogUSec)"
journalctl -u rumy.service -n 40 --no-pager | grep -m1 '\[bekci\]' || echo "   UYARI: Rumy bekci satirini yazmadi (yeni surum yuklu mu?)"

echo
echo "Bitti. Yeniden baslatma gerekmez."
