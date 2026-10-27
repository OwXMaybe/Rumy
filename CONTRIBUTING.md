# Katkı

Rumy'ye katkı için teşekkürler. Hata bildirimi ve öneriler GitHub Issues'tan gelir; kod değişiklikleri pull request
olarak. Proje Türkçe yürür, ama İngilizce yazılan issue ve PR'lar da memnuniyetle okunur.

## Hata bildirirken

- Kartın panel tipi (ILI9341 ya da ST7789) ve Rumy'nin sürümü (Tanılama ekranı ya da seri port `rapor`).
- Mümkünse seri porttan (460800 baud) `rapor` çıktısı. Çıktıda WiFi şifresi yoktur, ama ağ adresleri ve ampulün MAC
  adresi olabilir; paylaşmadan önce silebilirsin.
- Ne yaptın, ne bekliyordun, ne oldu.

## Kod kuralları

Bunlar projenin başından beri uyulan kurallardır; PR'lar da bunlara göre incelenir.

- **C++ kodunda yorum satırı yok.** Açıklamalar README'de ve `Rumy_Proje_Dokumani.md`'de; kod adlandırmayla okunur
  tutulur.
- **Python araçlarında** yalnızca dosya başında, kullanımı anlatan açıklama metni olur.
- **LVGL çağrıları yalnızca UI görevinde** yapılır; diğer görevler veriyi `store` üzerinden yayınlar.
- Çalışma sırasında büyük `malloc` yok, döngülerde `String` birleştirme yok, bloklayan `delay()` yok, her ağ işleminde
  zaman aşımı var.
- **Kişisel ayarlar kodda değil NVS'te** durur. Kişisel geliştirme ayarları (`secrets*.ini`) git dışıdır.
- Kart ve köprü dışındaki servislere giden isteklerde `User-Agent` projeyi tanıtır ve kişisel bilgi taşımaz.

## Göndermeden önce

```bash
pio run -e cyd
pio run -e cyd_st7789
pio run -e cyd_endurance
pio run -e cyd_prof
pio test -e native
```

- Dört ortam da `-Wall -Wextra` ile **uyarısız** derlenmeli; bütün native testler geçmeli.
- Arayüzü değiştiren her şey önce **simülatörde** denenir: `python tools/ui_preview.py` (gerekirse `--buyuk`,
  `--film`, `--cesit`, `--tarih`). Denetim `denetim: 0 bulgu` vermeli; bilerek böyle olan bir bulgu varsa nedeniyle
  birlikte `sim/denetim_izin.txt`'ye eklenir. Simülatör MinGW-w64 g++ ve Pillow ister.
- Arayüz değişikliklerinde PR'a önce/sonra ekran görüntüsü ekle.
- Yeni bir ayrıştırıcı ya da protokol satırı `lib/rumy_core` altına yazılır ve `test/` altında testi eklenir.
- README'yi ve gerekiyorsa `Rumy_Proje_Dokumani.md`'yi güncelle; kullanıcıyı ilgilendiren değişiklikleri
  `CHANGELOG.md`'ye ekle.

## Varlıklar ve lisanslar

- Depoya kulüp ya da turnuva logosu, oyuncu fotoğrafı veya lisansı belirsiz görsel eklenmez.
- Yeni bir kütüphane, yazı tipi ya da ikon eklersen lisansını `THIRD_PARTY_NOTICES.md`'ye ve gerekiyorsa metnini
  `LICENSES/` altına koy.
- Katkıların projenin MIT lisansıyla yayınlanır.
