#include <string.h>
#include <unity.h>

#include "media_proto.h"

void setUp() {}
void tearDown() {}

static void test_np_full_line() {
  char line[] = "1\t83500\t215000\t1a2b3c4d\tff8800\tSpotify\tŞarkı Adı\tSanatçı\tAlbüm";
  rc::NowPlaying np;
  rc::parseNowPlaying(line, np);
  TEST_ASSERT_TRUE(np.playing);
  TEST_ASSERT_TRUE(np.active);
  TEST_ASSERT_EQUAL_UINT32(83500, np.posMs);
  TEST_ASSERT_EQUAL_UINT32(215000, np.durMs);
  TEST_ASSERT_EQUAL_HEX32(0x1a2b3c4d, np.artHash);
  TEST_ASSERT_EQUAL_HEX32(0xff8800, np.accent);
  TEST_ASSERT_EQUAL_STRING("Spotify", np.app);
  TEST_ASSERT_EQUAL_STRING("Şarkı Adı", np.title);
  TEST_ASSERT_EQUAL_STRING("Sanatçı", np.artist);
  TEST_ASSERT_EQUAL_STRING("Albüm", np.album);
  TEST_ASSERT_EQUAL_HEX32(rc::trackHash("Şarkı Adı", "Sanatçı", "Albüm"), np.track);
}

static void test_np_short_and_idle() {
  char line[] = "0\t0\t0\t0\t0\tSpotify";
  rc::NowPlaying np;
  rc::parseNowPlaying(line, np);
  TEST_ASSERT_FALSE(np.playing);
  TEST_ASSERT_FALSE(np.active);
  TEST_ASSERT_EQUAL_UINT32(0, np.artHash);
  TEST_ASSERT_EQUAL_STRING("Spotify", np.app);
  TEST_ASSERT_EQUAL_STRING("", np.title);
  TEST_ASSERT_EQUAL_STRING("", np.album);
  char empty[] = "";
  rc::parseNowPlaying(empty, np);
  TEST_ASSERT_FALSE(np.playing);
  TEST_ASSERT_EQUAL_STRING("", np.app);
}

static void test_np_paused_with_empty_fields() {
  char line[] = "0\t1000\t2000\t\t\tChrome\tVideo\t\t";
  rc::NowPlaying np;
  rc::parseNowPlaying(line, np);
  TEST_ASSERT_FALSE(np.playing);
  TEST_ASSERT_TRUE(np.active);
  TEST_ASSERT_EQUAL_UINT32(0, np.artHash);
  TEST_ASSERT_EQUAL_STRING("Video", np.title);
  TEST_ASSERT_EQUAL_STRING("", np.artist);
}

static void test_track_hash_separates_fields() {
  TEST_ASSERT_NOT_EQUAL(rc::trackHash("ab", "c", ""), rc::trackHash("a", "bc", ""));
  TEST_ASSERT_NOT_EQUAL(rc::trackHash("a", "", "b"), rc::trackHash("a", "b", ""));
  TEST_ASSERT_EQUAL_HEX32(rc::trackHash("x", "y", "z"), rc::trackHash("x", "y", "z"));
}

static void test_art_offer() {
  rc::ArtOffer o;
  TEST_ASSERT_TRUE(rc::parseArtOffer("1a2b3c4d 144 144 41472 deadbeef", 0xB000, o));
  TEST_ASSERT_EQUAL_HEX32(0x1a2b3c4d, o.hash);
  TEST_ASSERT_EQUAL_UINT16(144, o.w);
  TEST_ASSERT_EQUAL_UINT16(144, o.h);
  TEST_ASSERT_EQUAL_UINT32(41472, o.len);
  TEST_ASSERT_EQUAL_HEX32(0xdeadbeef, o.crc);
  TEST_ASSERT_TRUE(rc::parseArtOffer("1 192 192 73728 0", 0x13000, o));
  TEST_ASSERT_FALSE(rc::parseArtOffer("1 192 192 73728 0", 0xB000, o));
  TEST_ASSERT_FALSE(rc::parseArtOffer("0 144 144 41472 0", 0xB000, o));
  TEST_ASSERT_FALSE(rc::parseArtOffer("1 144 144 41470 0", 0xB000, o));
  TEST_ASSERT_FALSE(rc::parseArtOffer("1 300 10 6000 0", 0x13000, o));
  TEST_ASSERT_FALSE(rc::parseArtOffer("1 0 144 0 0", 0xB000, o));
  TEST_ASSERT_FALSE(rc::parseArtOffer("", 0xB000, o));
}

static void test_art_slot_header() {
  TEST_ASSERT_EQUAL_UINT32(24, sizeof(rc::ArtSlotHdr));
  rc::ArtSlotHdr h = {rc::ART_MAGIC, 0x1234, 7, 144, 144, 0, 144 * 144 * 2};
  TEST_ASSERT_TRUE(rc::artSlotValid(h, 144, 0xB000));
  TEST_ASSERT_FALSE(rc::artSlotValid(h, 192, 0x13000));
  h.magic = 0xFFFFFFFFu;
  TEST_ASSERT_FALSE(rc::artSlotValid(h, 144, 0xB000));
  h.magic = rc::ART_MAGIC;
  h.size = 100;
  TEST_ASSERT_FALSE(rc::artSlotValid(h, 144, 0xB000));
  h.size = 144 * 144 * 2;
  h.h = 0;
  TEST_ASSERT_FALSE(rc::artSlotValid(h, 144, 0xB000));
  rc::ArtSlotHdr big = {rc::ART_MAGIC, 1, 1, 192, 192, 0, 192 * 192 * 2};
  TEST_ASSERT_TRUE(rc::artSlotValid(big, 192, 0x13000));
  TEST_ASSERT_FALSE(rc::artSlotValid(big, 192, 0xB000));
  rc::ArtSlotHdr face = {rc::ART_MAGIC, 2, 2, 84, 84, 0, 84 * 84 * 2};
  TEST_ASSERT_FALSE(rc::artSlotValid(face, 144, 0xB000));
  TEST_ASSERT_TRUE(rc::artSlotValid(face, 144, 0xB000, 84));
  face.h = 80;
  face.size = 84 * 80 * 2;
  TEST_ASSERT_FALSE(rc::artSlotValid(face, 144, 0xB000, 84));
  TEST_ASSERT_FALSE(rc::artSlotValid(h, 192, 0x13000, 112));
}

static void test_photo_hash() {
  uint32_t a = rc::photoHash("Kerem Aktürkoğlu", 84);
  TEST_ASSERT_EQUAL_HEX32(a, rc::photoHash("Kerem Aktürkoğlu", 84));
  TEST_ASSERT_NOT_EQUAL(a, rc::photoHash("Kerem Aktürkoğlu", 112));
  TEST_ASSERT_NOT_EQUAL(a, rc::photoHash("Kerem Akturkoglu", 84));
  TEST_ASSERT_NOT_EQUAL(0u, rc::photoHash("", 84));
  TEST_ASSERT_NOT_EQUAL(0u, rc::photoHash(nullptr, 84));
}

static void test_position_prediction() {
  TEST_ASSERT_EQUAL_UINT32(1000, rc::predictPosMs(1000, 5000, false, 200000));
  TEST_ASSERT_EQUAL_UINT32(6000, rc::predictPosMs(1000, 5000, true, 200000));
  TEST_ASSERT_EQUAL_UINT32(200000, rc::predictPosMs(199000, 5000, true, 200000));
  TEST_ASSERT_EQUAL_UINT32(204000, rc::predictPosMs(199000, 5000, true, 0));
  TEST_ASSERT_FALSE(rc::seekJumped(10000, 9000, 1500));
  TEST_ASSERT_FALSE(rc::seekJumped(9000, 10500, 1500));
  TEST_ASSERT_TRUE(rc::seekJumped(12000, 10000, 1500));
  TEST_ASSERT_TRUE(rc::seekJumped(0, 60000, 1500));
}

static void test_tempo_line() {
  rc::TempoMsg t;
  TEST_ASSERT_TRUE(rc::parseTempo("500 120", t));
  TEST_ASSERT_EQUAL_UINT32(500, t.periodMs);
  TEST_ASSERT_EQUAL_UINT32(120, t.nextMs);
  TEST_ASSERT_TRUE(rc::parseTempo("480 1000", t));
  TEST_ASSERT_EQUAL_UINT32(40, t.nextMs);
  TEST_ASSERT_TRUE(rc::parseTempo("250 0", t));
  TEST_ASSERT_TRUE(rc::parseTempo("2000 0", t));
  TEST_ASSERT_FALSE(rc::parseTempo("249 0", t));
  TEST_ASSERT_EQUAL_UINT32(0, t.periodMs);
  TEST_ASSERT_FALSE(rc::parseTempo("2001 0", t));
  TEST_ASSERT_FALSE(rc::parseTempo("0", t));
}

static void test_playlist_head() {
  rc::PlaylistHead h;
  char ok[] = "1a2b 12 hazir 1";
  TEST_ASSERT_TRUE(rc::parsePlaylistHead(ok, h));
  TEST_ASSERT_EQUAL_HEX32(0x1a2b, h.ver);
  TEST_ASSERT_EQUAL_UINT8(12, h.n);
  TEST_ASSERT_EQUAL_UINT8(rc::PLS_READY, h.status);
  TEST_ASSERT_TRUE(h.shuffle);
  TEST_ASSERT_EQUAL_STRING("", h.msg);
  char many[] = "5 99 hazir 0";
  TEST_ASSERT_TRUE(rc::parsePlaylistHead(many, h));
  TEST_ASSERT_EQUAL_UINT8(rc::PL_MAX, h.n);
  TEST_ASSERT_FALSE(h.shuffle);
  char fail[] = "0 4 hata 1 Spotify'a ulaşılamadı";
  TEST_ASSERT_TRUE(rc::parsePlaylistHead(fail, h));
  TEST_ASSERT_EQUAL_UINT8(rc::PLS_FAIL, h.status);
  TEST_ASSERT_EQUAL_UINT8(0, h.n);
  TEST_ASSERT_EQUAL_STRING("Spotify'a ulaşılamadı", h.msg);
  char login[] = "0 0 giris 1";
  TEST_ASSERT_TRUE(rc::parsePlaylistHead(login, h));
  TEST_ASSERT_EQUAL_UINT8(rc::PLS_LOGIN, h.status);
  char loading[] = "0 0 yukleniyor 0";
  TEST_ASSERT_TRUE(rc::parsePlaylistHead(loading, h));
  TEST_ASSERT_EQUAL_UINT8(rc::PLS_LOADING, h.status);
  char badWord[] = "0 0 hazirr 1";
  TEST_ASSERT_FALSE(rc::parsePlaylistHead(badWord, h));
  char badMix[] = "0 0 hazir 2";
  TEST_ASSERT_FALSE(rc::parsePlaylistHead(badMix, h));
  char noMix[] = "0 0 hazir";
  TEST_ASSERT_FALSE(rc::parsePlaylistHead(noMix, h));
  char empty[] = "";
  TEST_ASSERT_FALSE(rc::parsePlaylistHead(empty, h));
}

static void test_playlist_line() {
  rc::PlaylistLine l;
  char ok[] = "3\t812\tdeadbeef\t3\tBeğenilen Şarkılar";
  TEST_ASSERT_TRUE(rc::parsePlaylistLine(ok, l));
  TEST_ASSERT_EQUAL_UINT8(3, l.index);
  TEST_ASSERT_EQUAL_UINT16(812, l.total);
  TEST_ASSERT_EQUAL_HEX32(0xdeadbeef, l.sheet);
  TEST_ASSERT_EQUAL_UINT8(3, l.cell);
  TEST_ASSERT_EQUAL_STRING("Beğenilen Şarkılar", l.name);
  char big[] = "0\t123456\t0\t0\tDev";
  TEST_ASSERT_TRUE(rc::parsePlaylistLine(big, l));
  TEST_ASSERT_EQUAL_UINT16(65535, l.total);
  TEST_ASSERT_EQUAL_HEX32(0, l.sheet);
  char emptyName[] = "1\t0\t0\t0\t";
  TEST_ASSERT_TRUE(rc::parsePlaylistLine(emptyName, l));
  TEST_ASSERT_EQUAL_STRING("", l.name);
  char badIndex[] = "30\t1\t0\t0\tx";
  TEST_ASSERT_FALSE(rc::parsePlaylistLine(badIndex, l));
  char badCell[] = "0\t1\t0\t9\tx";
  TEST_ASSERT_FALSE(rc::parsePlaylistLine(badCell, l));
  char shortLine[] = "0\t1\t0\t0";
  TEST_ASSERT_FALSE(rc::parsePlaylistLine(shortLine, l));
}

static void test_playlist_reply() {
  rc::PlaylistReply r;
  char start[] = "4 bas";
  TEST_ASSERT_TRUE(rc::parsePlaylistReply(start, r));
  TEST_ASSERT_EQUAL_INT8(4, r.index);
  TEST_ASSERT_EQUAL_UINT8(rc::PLR_STARTING, r.result);
  TEST_ASSERT_EQUAL_STRING("", r.msg);
  char ok[] = "0 tamam";
  TEST_ASSERT_TRUE(rc::parsePlaylistReply(ok, r));
  TEST_ASSERT_EQUAL_UINT8(rc::PLR_OK, r.result);
  char fail[] = "7 hata Premium gerekli";
  TEST_ASSERT_TRUE(rc::parsePlaylistReply(fail, r));
  TEST_ASSERT_EQUAL_INT8(7, r.index);
  TEST_ASSERT_EQUAL_UINT8(rc::PLR_FAIL, r.result);
  TEST_ASSERT_EQUAL_STRING("Premium gerekli", r.msg);
  char none[] = "-1 tamam";
  TEST_ASSERT_TRUE(rc::parsePlaylistReply(none, r));
  TEST_ASSERT_EQUAL_INT8(-1, r.index);
  char badIndex[] = "30 tamam";
  TEST_ASSERT_FALSE(rc::parsePlaylistReply(badIndex, r));
  char badWord[] = "1 tamamlandi";
  TEST_ASSERT_FALSE(rc::parsePlaylistReply(badWord, r));
  char noIndex[] = "tamam";
  TEST_ASSERT_FALSE(rc::parsePlaylistReply(noIndex, r));
}

static void test_last_beat() {
  TEST_ASSERT_EQUAL_UINT32(1000, rc::lastBeat(0, 500, 1200));
  TEST_ASSERT_EQUAL_UINT32(1500, rc::lastBeat(0, 500, 1500));
  TEST_ASSERT_EQUAL_UINT32(1100, rc::lastBeat(2100, 500, 1200));
  TEST_ASSERT_EQUAL_UINT32(204u, rc::lastBeat(4294967000u, 500, 204));
  TEST_ASSERT_EQUAL_UINT32(4294967000u, rc::lastBeat(4294967000u, 500, 100));
}

static void test_beat_lock_first_and_reset() {
  uint32_t period = 0, anchor = 0;
  rc::beatLock(period, anchor, 500, 10120, 10000);
  TEST_ASSERT_EQUAL_UINT32(500, period);
  TEST_ASSERT_EQUAL_UINT32(9620, anchor);
  rc::beatLock(period, anchor, 0, 0, 10100);
  TEST_ASSERT_EQUAL_UINT32(0, period);
}

static void test_beat_lock_smooths_small_errors() {
  uint32_t period = 500, anchor = 10000;
  rc::beatLock(period, anchor, 510, 11030, 11200);
  TEST_ASSERT_EQUAL_UINT32(502, period);
  TEST_ASSERT_EQUAL_UINT32(11000 + (rc::lastBeat(11030, 510, 11200) - 11000) / 3, anchor);
  uint32_t p2 = 500, a2 = 10000;
  rc::beatLock(p2, a2, 500, 10960, 11200);
  TEST_ASSERT_EQUAL_UINT32(500, p2);
  TEST_ASSERT_EQUAL_UINT32(10987, a2);
}

static void test_beat_lock_jumps_on_big_change() {
  uint32_t period = 500, anchor = 10000;
  rc::beatLock(period, anchor, 600, 11050, 11200);
  TEST_ASSERT_EQUAL_UINT32(600, period);
  TEST_ASSERT_EQUAL_UINT32(11050, anchor);
  uint32_t p2 = 500, a2 = 10000;
  rc::beatLock(p2, a2, 500, 11250, 11300);
  TEST_ASSERT_EQUAL_UINT32(500, p2);
  TEST_ASSERT_EQUAL_UINT32(11250, a2);
}

static void test_football_line() {
  char buf[48];
  TEST_ASSERT_EQUAL_UINT32(28, rc::footballLine(buf, sizeof(buf), true, 436, 0x13235B, 0xFFD60A));
  TEST_ASSERT_EQUAL_STRING("@futbol 1 436 13235b ffd60a\n", buf);
  TEST_ASSERT_EQUAL_UINT32(26, rc::footballLine(buf, sizeof(buf), false, 436, 0x13235B, 0xFFD60A));
  TEST_ASSERT_EQUAL_STRING("@futbol 0 0 13235b ffd60a\n", buf);
  rc::footballLine(buf, sizeof(buf), true, 132335, 0, 0);
  TEST_ASSERT_EQUAL_STRING("@futbol 1 132335 000000 000000\n", buf);
  char tiny[16];
  TEST_ASSERT_EQUAL_UINT32(0, rc::footballLine(tiny, sizeof(tiny), true, 436, 0, 0));
  TEST_ASSERT_EQUAL_STRING("", tiny);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_np_full_line);
  RUN_TEST(test_np_short_and_idle);
  RUN_TEST(test_np_paused_with_empty_fields);
  RUN_TEST(test_track_hash_separates_fields);
  RUN_TEST(test_art_offer);
  RUN_TEST(test_art_slot_header);
  RUN_TEST(test_position_prediction);
  RUN_TEST(test_tempo_line);
  RUN_TEST(test_playlist_head);
  RUN_TEST(test_playlist_line);
  RUN_TEST(test_playlist_reply);
  RUN_TEST(test_last_beat);
  RUN_TEST(test_beat_lock_first_and_reset);
  RUN_TEST(test_beat_lock_smooths_small_errors);
  RUN_TEST(test_beat_lock_jumps_on_big_change);
  RUN_TEST(test_photo_hash);
  RUN_TEST(test_football_line);
  return UNITY_END();
}
