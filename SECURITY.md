# Güvenlik

## Güven modeli

Rumy ev ağında, güvenilen cihazlarla birlikte kullanılmak üzere tasarlandı.

- **Uygulama (port 80):** telefon ve bilgisayar uygulaması şifresiz HTTP ile konuşur. Eşleştirme, Rumy'nin ekranında
  Ayarlar → Telefon açıkken görünen 4 haneli kodla yapılır; 5 hatalı denemede 1 dakika kilit vardır. Eşleşen cihaz
  128 bitlik rastgele bir belirteç alır (en çok 4 cihaz). Belirteçler ve ayarlar kartın NVS belleğinde durur.
- **Köprü (port 7777):** bilgisayardaki köprü WiFi ile bağlanırken 128 bitlik paylaşılan bir anahtar gönderir; anahtar
  kartın NVS'inde ve bilgisayarda `tools/.cache/pi_anahtar.txt` dosyasında (git dışı) durur. Bağlantı şifrelenmez.
  Köprü komutları ve kablosuz güncelleme WiFi'de yalnızca bu anahtarla kurulan bağlantıdan kabul edilir; USB'de
  karta fiziksel erişim gerekir.
- **WiFi şifresi** ilk kurulum portalında girilir ve yalnızca NVS'te tutulur; kodda ya da depoda yer almaz.
- **Spotify:** herkes kendi geliştirici uygulamasının Client ID'sini kullanır (PKCE, client secret yok); yenileme
  anahtarı yalnızca bilgisayarda `tools/.cache/spotify_api.json`'da durur.

## Öneriler

- 80 ve 7777 portlarını **internete açma** (modemde port yönlendirme yapma).
- Rumy'yi misafir ağı yerine kendi ağında tut; ağında güvenmediğin cihaz varsa köprüyü USB ile kullan.
- Seri port `rapor` çıktısını paylaşırken ağ adreslerini ve MAC adreslerini silebilirsin.

## Açık bildirme

Bir güvenlik açığı bulursan lütfen herkese açık bir issue açma. GitHub'daki depo sayfasında
**Security → Report a vulnerability** ile gizli olarak bildir. Açığın nasıl tekrarlanacağını ve etkisini yaz;
düzeltme yayınlanana kadar ayrıntıları paylaşmamanı rica ederiz.
