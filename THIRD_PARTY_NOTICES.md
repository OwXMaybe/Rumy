# Üçüncü taraf bileşenler ve notlar

Rumy'nin kendi kodu MIT lisanslıdır (`LICENSE`). Aşağıdaki bileşenler kendi lisanslarıyla kullanılır; lisans
metinleri `LICENSES/` klasöründedir.

## Kart yazılımına derlenenler

Bu kütüphaneler depoda yoktur; PlatformIO derlemede sabit sürümleriyle indirir (`platformio.ini`). Releases'teki
derlenmiş `.bin` dosyaları bunları içerir.

| Bileşen | Sürüm | Lisans | Metin |
|---|---|---|---|
| [LVGL](https://github.com/lvgl/lvgl) | 9.5.0 | MIT, Copyright (c) LVGL Kft | `LICENSES/LVGL-MIT.txt` |
| [LovyanGFX](https://github.com/lovyan03/LovyanGFX) | 1.2.30 | FreeBSD (BSD-2), Copyright (c) lovyan03; içindeki uyarlanmış kodlar MIT/BSD | `LICENSES/LovyanGFX.txt` |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | 7.4.3 | MIT, Copyright (c) Benoit Blanchon | `LICENSES/ArduinoJson-MIT.txt` |
| [WiFiManager](https://github.com/tzapu/WiFiManager) | 2.0.17 | MIT, Copyright (c) tzapu | `LICENSES/WiFiManager-MIT.txt` |
| [Arduino-ESP32](https://github.com/espressif/arduino-esp32) | platform espressif32 6.13.0 | LGPL-2.1 | `LICENSES/LGPL-2.1.txt` |
| ESP-IDF, mbedTLS, lwIP ve diğer Espressif bileşenleri (Arduino-ESP32 içinde) | – | Apache-2.0 / BSD | `LICENSES/Apache-2.0.txt` |

Arduino-ESP32 LGPL-2.1 ile lisanslıdır: derlenmiş imajı yeniden oluşturmak için gereken bütün kaynak (bu depo ve
`platformio.ini`'deki sabit sürümler) açıktır; kütüphanenin kaynağı yukarıdaki bağlantıdadır.

## Projede gömülü varlıklar

| Varlık | Nerede | Lisans |
|---|---|---|
| [Inter](https://github.com/rsms/inter) yazı tipi — Copyright (c) 2016 The Inter Project Authors | `src/ui/fonts/*.c`, `src/app/portal_font.h` (yalnızca gereken glifler, `tools/make_fonts.py`) | SIL OFL 1.1, `LICENSES/OFL-1.1.txt` |
| [Font Awesome Free](https://fontawesome.com) 5 simgeleri — Copyright (c) Fonticons, Inc. | `src/ui/fonts/*.c` (simge glifleri) | Simgeler CC BY 4.0, yazı tipi SIL OFL 1.1; `LICENSES/FontAwesome-Free.txt` |
| [Meteocons](https://github.com/basmilius/weather-icons) — Copyright (c) 2020-present Bas Milius | `src/ui/icons_embedded.cpp`, `src/ui/icons_big.cpp`, `sd_card/rumy/img/` | MIT, `LICENSES/Meteocons-MIT.txt` |
| [Montserrat](https://github.com/JulietaUla/Montserrat) — Copyright 2011 The Montserrat Project Authors | Lig rozetlerinin harfleri `src/ui/match_art.cpp` içinde resim olarak (`tools/make_match_art.py`, LVGL paketindeki dosyadan) | SIL OFL 1.1, `LICENSES/OFL-1.1.txt` |
| Mozilla kök sertifika deposu (certifi) ve GlobalSign Root CA | `src/net/ca_bundle.h`, `certs/` | Herkese açık kök sertifikalar; seçimi `tools/gen_ca_bundle.py` |

Yazı tipleri dönüştürülürken yeniden adlandırılmıştır (`font_rumy_*`); Font Awesome'ın ayrılmış adı kullanılmaz.

Rumy'nin yüzü, aksesuarları, dönen plak ve futbol topu kareleri, uygulama simgesi ve lig rozetleri bu projede
çizilmiştir ve MIT lisansına dahildir. Depoda hiçbir kulüp ya da turnuva logosu ve oyuncu fotoğrafı yoktur.

## Çalışırken kullanılan servisler

Bunlar Rumy'nin ya da bilgisayardaki köprünün çalışırken veri aldığı servislerdir; depoda verileri yoktur (testlerdeki
küçük örnek cevaplar hariç, `test/fixtures/`).

| Servis | Kullanım | Not |
|---|---|---|
| [Open-Meteo](https://open-meteo.com) | Hava, hava kalitesi, deniz | Veri CC BY 4.0; atıf: "Weather data by Open-Meteo.com" |
| AFAD | Deprem listesi (köprü üzerinden HTTPS) | T.C. İçişleri Bakanlığı Afet ve Acil Durum Yönetimi Başkanlığı, kamuya açık servis |
| Kandilli Rasathanesi ve Deprem Araştırma Enstitüsü | Deprem listesi (yedek) | Boğaziçi Üniversitesi, kamuya açık sayfa |
| [Nager.Date](https://date.nager.at) | Resmî tatiller | Servis MIT lisanslı açık kaynak |
| ESPN | Fikstür, skor, olaylar, puan durumu, takım listesi | Herkese açık ama belgelenmemiş uç noktalar; kişisel kullanım |
| LiveScore | Hızlı gol bildirimi (köprü) | Herkese açık ama belgelenmemiş uç noktalar; kişisel kullanım |
| TheSportsDB, Wikimedia Commons, Vikipedi | Gol ekranındaki oyuncu fotoğrafı (köprü) | Fotoğraflar yalnızca kullanıcının bilgisayarındaki önbellekte durur; Commons fotoğraflarının lisansı her dosyanın sayfasındadır |
| Spotify Web API | Çalma listeleri (köprü) | Her kullanıcı kendi geliştirici uygulamasını açar; Spotify Developer Terms geçerlidir |
| WiZ (yerel UDP), Creality K2 (yerel WebSocket) | Ampul ve yazıcı | Yerel ağ protokolleri, resmî olmayan kullanım |

## Esinlenilen projeler

- [Claw Light](https://clawlight.dev): kod ajanı oturumlarını bir ışıkla gösterme fikri (Rumy'deki uygulama kendi
  kodudur).
- [NowPLayingDisplay](https://github.com/RodgerE1/NowPLayingDisplay) ve
  [Spotify-Diy-Thing](https://github.com/witnessmenow/Spotify-Diy-Thing): seri porttan "şimdi çalıyor" gönderme ve CYD
  Spotify ekranı fikri.
- [Launcher](https://github.com/bmorcelli/Launcher): SD karttan uygulama kurma (Rumy'nin `Rumy.bin` dosyası onunla
  uyumludur; depoda Launcher kodu yoktur).

## Bağlantı yok

Rumy bağımsız bir hobi projesidir. ESPN, LiveScore, TheSportsDB, UEFA, TFF ve adı geçen ligler ve kulüpler, Spotify,
WiZ (Signify), Creality, Espressif, Anthropic (Claude Code), OpenAI (Codex), Claw Light, AFAD, Kandilli Rasathanesi ve
Open-Meteo ile bağlantılı değildir ve onlar tarafından desteklenmez. Adlar ve markalar yalnızca uyumluluğu anlatmak
için geçer ve sahiplerine aittir.
