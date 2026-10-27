#include "fwupdate.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "app/config.h"
#include "bridge.h"

#if !RUMY_PI
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <mbedtls/sha256.h>

#include "app/app.h"
#include "board/sdcard.h"
#include "diag/diag.h"
#include "fw_image.h"
#include "media.h"
#endif

namespace fwupdate {

#if !RUMY_PI

namespace {

volatile Phase s_phase = FW_IDLE;
volatile uint32_t s_got = 0;
volatile uint32_t s_total = 0;
volatile uint32_t s_at = 0;
const char* volatile s_why = "";
uint32_t s_lastByte = 0;
uint32_t s_startMs = 0;
uint32_t s_okCount = 0;
uint32_t s_failCount = 0;
rc::FwOffer s_offer;
rc::FwInfo s_info;
uint8_t s_head[rc::FW_HEAD_BYTES];
bool s_headOk = false;
mbedtls_sha256_context s_sha;
mbedtls_sha256_context s_body;
uint8_t s_tail[rc::FW_SHA_BYTES];
bool s_shaOn = false;
bool s_toUpdater = false;
esp_ota_handle_t s_ota = 0;
bool s_otaOpen = false;
char s_lastElf[9] = "-";

const esp_partition_t* part(const char* label) {
  return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, label);
}

bool partElf(const esp_partition_t* p, uint8_t* elf) {
  esp_app_desc_t d;
  if (!p || esp_ota_get_partition_description(p, &d) != ESP_OK) return false;
  memcpy(elf, d.app_elf_sha256, 4);
  return true;
}

void cleanFiles() {
  if (sdcard::exists(rc::FW_PART_FILE)) sdcard::remove(rc::FW_PART_FILE);
  if (sdcard::exists(rc::FW_FILE)) sdcard::remove(rc::FW_FILE);
  if (sdcard::exists(rc::FW_SHA_FILE)) sdcard::remove(rc::FW_SHA_FILE);
}

void shaStop() {
  if (!s_shaOn) return;
  mbedtls_sha256_free(&s_sha);
  mbedtls_sha256_free(&s_body);
  s_shaOn = false;
}

void otaStop() {
  if (!s_otaOpen) return;
  esp_ota_abort(s_ota);
  s_otaOpen = false;
}

void refuse(const char* why) {
  bridge::printf("@guncelle hata %s\n", why);
  diag::note("guncelle", "guncelleme reddedildi: %s", why);
}

void fail(const char* why) {
  bool streaming = s_phase == FW_RX;
  shaStop();
  otaStop();
  s_why = why;
  s_at = millis();
  s_phase = FW_FAIL;
  s_failCount++;
  if (!s_toUpdater) cleanFiles();
  bridge::bulk(false);
  bridge::printf("@guncelle hata %s\n", why);
  if (streaming) media::discardUntilQuiet();
  diag::note("guncelle", "guncelleme alinamadi: %s (%lu/%lu B)", why, static_cast<unsigned long>(s_got),
             static_cast<unsigned long>(s_total));
}

bool checkHead() {
  if (!rc::fwImageInfo(s_head, sizeof(s_head), s_info)) {
    fail("imaj");
    return false;
  }
  uint8_t upd[4];
  if (!s_toUpdater && partElf(part(rc::FW_LABEL_UPDATER), upd) && !memcmp(upd, s_info.elf, sizeof(upd))) {
    fail("guncelleyici imaji");
    return false;
  }
  s_headOk = true;
  return true;
}

bool stage() {
  if (!sdcard::rename(rc::FW_PART_FILE, rc::FW_FILE)) {
    fail("sd ad");
    return false;
  }
  char text[2 * rc::FW_SHA_BYTES + 16];
  rc::fwBytesToHex(s_offer.sha, rc::FW_SHA_BYTES, text);
  int n = snprintf(text + 2 * rc::FW_SHA_BYTES, sizeof(text) - 2 * rc::FW_SHA_BYTES, " %lu\n",
                   static_cast<unsigned long>(s_offer.size));
  size_t len = 2 * rc::FW_SHA_BYTES + static_cast<size_t>(n > 0 ? n : 0);
  if (!sdcard::writeFileAtomic(rc::FW_SHA_FILE, reinterpret_cast<const uint8_t*>(text), len)) {
    fail("sd sha");
    return false;
  }
  if (esp_ota_set_boot_partition(part(rc::FW_LABEL_UPDATER)) != ESP_OK) {
    fail("acilis bolumu");
    return false;
  }
  return true;
}

void finish() {
  uint8_t sum[rc::FW_SHA_BYTES];
  uint8_t body[rc::FW_SHA_BYTES];
  mbedtls_sha256_finish_ret(&s_sha, sum);
  mbedtls_sha256_finish_ret(&s_body, body);
  shaStop();
  if (memcmp(sum, s_offer.sha, sizeof(sum)) != 0) {
    fail("sha");
    return;
  }
  if (memcmp(body, s_tail, sizeof(body)) != 0) {
    fail("imaj ozeti");
    return;
  }
  if (s_toUpdater) {
    s_otaOpen = false;
    if (esp_ota_end(s_ota) != ESP_OK) {
      fail("yazma");
      return;
    }
  } else if (!stage()) {
    return;
  }
  rc::fwBytesToHex(s_info.elf, sizeof(s_info.elf), s_lastElf);
  s_phase = FW_DONE;
  s_at = millis();
  s_okCount++;
  bridge::bulk(false);
  bridge::printf("@guncelle tamam %s\n", s_lastElf);
  diag::note("guncelle", "%s %s alindi (%lu B, %lu sn), yeniden baslatiliyor", s_toUpdater ? "guncelleyici" : "yazilim",
             s_lastElf, static_cast<unsigned long>(s_offer.size),
             static_cast<unsigned long>((s_at - s_startMs) / 1000));
  app::request(app::ACT_REBOOT, cfg::FW_REBOOT_MS);
}

bool prepareSd(const rc::FwOffer& o, const esp_partition_t* upd) {
  uint8_t elf[4];
  if (!partElf(upd, elf)) {
    refuse("guncelleyici yok");
    return false;
  }
  if (!sdcard::mounted()) {
    refuse("sd yok");
    return false;
  }
  uint64_t total = sdcard::totalBytes();
  uint64_t used = sdcard::usedBytes();
  if (used > total || total - used < o.size + cfg::FW_SD_SLACK) {
    refuse("sd dolu");
    return false;
  }
  cleanFiles();
  if (!sdcard::streamOpen(rc::FW_PART_FILE, false) || !sdcard::streamClose()) {
    refuse("sd yazma");
    return false;
  }
  return true;
}

bool put(const uint8_t* data, size_t n) {
  if (s_toUpdater) return esp_ota_write(s_ota, data, n) == ESP_OK;
  return sdcard::streamWrite(data, n);
}

}

void offer(const char* args) {
  if (s_phase == FW_RX || s_phase == FW_DONE) {
    bridge::println("@guncelle hata mesgul");
    return;
  }
  rc::FwOffer o;
  if (!rc::parseFwOffer(args, o)) return refuse("bicim");
  bool toUpdater = strstr(args, " guncelleyici") != nullptr;
  const esp_partition_t* app = part(rc::FW_LABEL_APP);
  const esp_partition_t* upd = part(rc::FW_LABEL_UPDATER);
  if (!app || !upd) return refuse("guncelleyici yok");
  if (esp_ota_get_running_partition() != app) return refuse("bolum");
  const esp_partition_t* dst = toUpdater ? upd : app;
  if (o.size < rc::FW_HEAD_BYTES || o.size > dst->size) return refuse("boyut");
  if (media::receiving()) return refuse("mesgul");
  if (toUpdater) {
    if (esp_ota_begin(upd, OTA_WITH_SEQUENTIAL_WRITES, &s_ota) != ESP_OK) return refuse("bolum");
    s_otaOpen = true;
  } else if (!prepareSd(o, upd)) {
    return;
  }
  s_toUpdater = toUpdater;
  s_offer = o;
  s_got = 0;
  s_total = o.size;
  s_why = "";
  s_headOk = false;
  memset(&s_info, 0, sizeof(s_info));
  mbedtls_sha256_init(&s_sha);
  mbedtls_sha256_starts_ret(&s_sha, 0);
  mbedtls_sha256_init(&s_body);
  mbedtls_sha256_starts_ret(&s_body, 0);
  memset(s_tail, 0, sizeof(s_tail));
  s_shaOn = true;
  s_startMs = millis();
  s_lastByte = s_startMs;
  s_at = s_startMs;
  s_phase = FW_RX;
  bridge::bulk(true);
  bridge::println("@guncelle git");
  diag::note("guncelle", "%s aliniyor: %lu B", toUpdater ? "guncelleyici" : "yeni yazilim",
             static_cast<unsigned long>(o.size));
}

bool receiving() { return s_phase == FW_RX; }

bool receive() {
  if (s_phase != FW_RX) return false;
  uint32_t got = s_got;
  uint32_t need = static_cast<uint32_t>(cfg::FW_CHUNK - got % cfg::FW_CHUNK);
  if (need > s_total - got) need = s_total - got;
  if (static_cast<uint32_t>(bridge::available()) < need) return false;
  if (!s_toUpdater && !sdcard::streamOpen(rc::FW_PART_FILE, true)) {
    fail("sd acma");
    return true;
  }
  uint8_t buf[256];
  const char* why = nullptr;
  while (need) {
    size_t n = bridge::read(buf, need < sizeof(buf) ? need : sizeof(buf));
    if (!n) {
      why = "okuma";
      break;
    }
    if (got < sizeof(s_head)) {
      size_t k = sizeof(s_head) - got;
      memcpy(s_head + got, buf, n < k ? n : k);
    }
    if (!put(buf, n)) {
      why = s_toUpdater ? "yazma" : "sd yazma";
      break;
    }
    mbedtls_sha256_update_ret(&s_sha, buf, n);
    size_t body = rc::fwBodyBytes(got, n, s_total);
    if (body) mbedtls_sha256_update_ret(&s_body, buf, body);
    if (body < n) memcpy(s_tail + (got + body - (s_total - rc::FW_SHA_BYTES)), buf + body, n - body);
    got += static_cast<uint32_t>(n);
    need -= static_cast<uint32_t>(n);
  }
  if (!s_toUpdater && !sdcard::streamClose() && !why) why = "sd yazma";
  s_got = got;
  if (why) {
    fail(why);
    return true;
  }
  s_lastByte = millis();
  if (!s_headOk && !checkHead()) return true;
  if (got >= s_total) {
    finish();
    return true;
  }
  bridge::printf("@guncelle ok %lu\n", static_cast<unsigned long>(got));
  return true;
}

void loop() {
  uint32_t now = millis();
  if (s_phase == FW_RX && now - s_lastByte > cfg::FW_RX_TIMEOUT_MS) fail("zaman asimi");
  else if (s_phase == FW_FAIL && now - s_at > cfg::FW_FAIL_SHOW_MS) s_phase = FW_IDLE;
}

Progress progress() {
  Progress p;
  p.phase = s_phase;
  p.got = s_got;
  p.total = s_total;
  p.atMs = s_at;
  p.why = s_why;
  return p;
}

void runningElf(char* out, size_t cap) {
  if (esp_ota_get_app_elf_sha256(out, cap) == 0 && cap) out[0] = 0;
}

void updaterElf(char* out, size_t cap) {
  uint8_t u[4];
  if (cap < 9 || !partElf(part(rc::FW_LABEL_UPDATER), u)) {
    if (cap) out[0] = 0;
    return;
  }
  rc::fwBytesToHex(u, sizeof(u), out);
}

void report(Print& out) {
  static const char* const kPhase[] = {"bekliyor", "aliniyor", "hazir", "hata"};
  char elf[9];
  char ue[9];
  runningElf(elf, sizeof(elf));
  updaterElf(ue, sizeof(ue));
  const esp_partition_t* upd = part(rc::FW_LABEL_UPDATER);
  const esp_partition_t* run = esp_ota_get_running_partition();
  out.printf("[guncelleme] %s%s %lu/%lu B · calisan %s (%s) · guncelleyici %s (%u KB) · basarili %lu · hata %lu · son %s",
             kPhase[s_phase], s_toUpdater ? " (guncelleyici)" : "", static_cast<unsigned long>(s_got),
             static_cast<unsigned long>(s_total), elf, run ? run->label : "?", upd ? (ue[0] ? ue : "bos") : "yok",
             upd ? static_cast<unsigned>(upd->size / 1024) : 0U, static_cast<unsigned long>(s_okCount),
             static_cast<unsigned long>(s_failCount), s_lastElf);
  if (s_phase == FW_FAIL || s_failCount) out.printf(" · neden %s", s_why[0] ? s_why : "-");
  out.println();
}

#else

void offer(const char*) { bridge::println("@guncelle hata pi"); }

bool receiving() { return false; }

bool receive() { return false; }

void loop() {}

Progress progress() {
  Progress p = {FW_IDLE, 0, 0, 0, ""};
  return p;
}

void runningElf(char* out, size_t cap) {
  if (cap) out[0] = 0;
}

void updaterElf(char* out, size_t cap) {
  if (cap) out[0] = 0;
}

void report(Print& out) { out.println("[guncelleme] pi: kablosuz guncelleme yok"); }

#endif

}
