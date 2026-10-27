#include <string.h>
#include <unity.h>

#include "fw_image.h"

void setUp() {}
void tearDown() {}

static const char kSha[] = "81f8cf9b2977177a00112233445566778899aabbccddeeff0102030405060708";

static void put32(uint8_t* p, uint32_t v) {
  p[0] = static_cast<uint8_t>(v);
  p[1] = static_cast<uint8_t>(v >> 8);
  p[2] = static_cast<uint8_t>(v >> 16);
  p[3] = static_cast<uint8_t>(v >> 24);
}

static void head(uint8_t* h) {
  memset(h, 0, rc::FW_HEAD_BYTES);
  h[0] = rc::FW_IMAGE_MAGIC;
  h[1] = 6;
  h[rc::FW_HASH_FLAG_OFFSET] = 1;
  uint8_t* d = h + rc::FW_DESC_OFFSET;
  put32(d, rc::FW_DESC_MAGIC);
  memcpy(d + 16, "esp-idf: v4.4.7", 15);
  memcpy(d + 48, "arduino-lib-builder", 19);
  memcpy(d + 80, "07:14:52", 8);
  memcpy(d + 96, "Jul 12 2024", 11);
  d[144] = 0xda;
  d[145] = 0x11;
  d[146] = 0x3d;
  d[147] = 0xe0;
}

static void test_offer() {
  char line[96];
  strcpy(line, "2150512 ");
  strcat(line, kSha);
  rc::FwOffer o;
  TEST_ASSERT_TRUE(rc::parseFwOffer(line, o));
  TEST_ASSERT_EQUAL_UINT32(2150512, o.size);
  TEST_ASSERT_EQUAL_HEX8(0x81, o.sha[0]);
  TEST_ASSERT_EQUAL_HEX8(0x7a, o.sha[7]);
  TEST_ASSERT_EQUAL_HEX8(0x08, o.sha[31]);
  strcat(line, " sonra");
  TEST_ASSERT_TRUE(rc::parseFwOffer(line, o));
}

static void test_offer_upper_hex() {
  rc::FwOffer o;
  TEST_ASSERT_TRUE(rc::parseFwOffer("10 81F8CF9B2977177A00112233445566778899AABBCCDDEEFF0102030405060708", o));
  TEST_ASSERT_EQUAL_HEX8(0xf8, o.sha[1]);
}

static void test_offer_rejects() {
  rc::FwOffer o;
  char line[128];
  TEST_ASSERT_FALSE(rc::parseFwOffer("", o));
  TEST_ASSERT_FALSE(rc::parseFwOffer("2150512", o));
  strcpy(line, "0 ");
  strcat(line, kSha);
  TEST_ASSERT_FALSE(rc::parseFwOffer(line, o));
  strcpy(line, "abc ");
  strcat(line, kSha);
  TEST_ASSERT_FALSE(rc::parseFwOffer(line, o));
  strcpy(line, "100 ");
  strncat(line, kSha, 62);
  TEST_ASSERT_FALSE(rc::parseFwOffer(line, o));
  strcpy(line, "100 ");
  strcat(line, kSha);
  strcat(line, "ab");
  TEST_ASSERT_FALSE(rc::parseFwOffer(line, o));
  strcpy(line, "100 ");
  strcat(line, kSha);
  line[10] = 'g';
  TEST_ASSERT_FALSE(rc::parseFwOffer(line, o));
  strcpy(line, "100  ");
  strcat(line, kSha);
  TEST_ASSERT_FALSE(rc::parseFwOffer(line, o));
}

static void test_image_info() {
  uint8_t h[rc::FW_HEAD_BYTES];
  head(h);
  rc::FwInfo info;
  TEST_ASSERT_TRUE(rc::fwImageInfo(h, sizeof(h), info));
  TEST_ASSERT_EQUAL_STRING("esp-idf: v4.4.7", info.version);
  TEST_ASSERT_EQUAL_STRING("arduino-lib-builder", info.project);
  TEST_ASSERT_EQUAL_STRING("07:14:52", info.time);
  TEST_ASSERT_EQUAL_STRING("Jul 12 2024", info.date);
  char hex[9];
  rc::fwBytesToHex(info.elf, sizeof(info.elf), hex);
  TEST_ASSERT_EQUAL_STRING("da113de0", hex);
}

static void test_image_full_fields_terminate() {
  uint8_t h[rc::FW_HEAD_BYTES];
  head(h);
  memset(h + rc::FW_DESC_OFFSET + 16, 'v', 32);
  memset(h + rc::FW_DESC_OFFSET + 48, 'p', 32);
  rc::FwInfo info;
  TEST_ASSERT_TRUE(rc::fwImageInfo(h, sizeof(h), info));
  TEST_ASSERT_EQUAL_size_t(32, strlen(info.version));
  TEST_ASSERT_EQUAL_size_t(32, strlen(info.project));
}

static void test_image_rejects() {
  uint8_t h[rc::FW_HEAD_BYTES];
  rc::FwInfo info;
  head(h);
  TEST_ASSERT_FALSE(rc::fwImageInfo(h, sizeof(h) - 1, info));
  h[0] = 0xE8;
  TEST_ASSERT_FALSE(rc::fwImageInfo(h, sizeof(h), info));
  head(h);
  h[12] = 9;
  TEST_ASSERT_FALSE(rc::fwImageInfo(h, sizeof(h), info));
  head(h);
  h[rc::FW_HASH_FLAG_OFFSET] = 0;
  TEST_ASSERT_FALSE(rc::fwImageInfo(h, sizeof(h), info));
  head(h);
  h[rc::FW_DESC_OFFSET] ^= 1;
  TEST_ASSERT_FALSE(rc::fwImageInfo(h, sizeof(h), info));
  TEST_ASSERT_EQUAL_STRING("", info.version);
}

static void test_sha_file_roundtrip() {
  rc::FwOffer a;
  char line[96];
  strcpy(line, "421280 ");
  strcat(line, kSha);
  TEST_ASSERT_TRUE(rc::parseFwOffer(line, a));
  char text[96];
  rc::fwBytesToHex(a.sha, rc::FW_SHA_BYTES, text);
  TEST_ASSERT_EQUAL_STRING(kSha, text);
  strcat(text, " 421280\n");
  rc::FwOffer b;
  TEST_ASSERT_TRUE(rc::parseFwShaFile(text, strlen(text), b));
  TEST_ASSERT_EQUAL_UINT32(421280, b.size);
  TEST_ASSERT_EQUAL_MEMORY(a.sha, b.sha, rc::FW_SHA_BYTES);
}

static void test_sha_file_rejects() {
  rc::FwOffer o;
  char text[128];
  TEST_ASSERT_FALSE(rc::parseFwShaFile("", 0, o));
  strcpy(text, kSha);
  TEST_ASSERT_FALSE(rc::parseFwShaFile(text, strlen(text), o));
  strcat(text, " 0");
  TEST_ASSERT_FALSE(rc::parseFwShaFile(text, strlen(text), o));
  strcpy(text, kSha + 2);
  strcat(text, " 100");
  TEST_ASSERT_FALSE(rc::parseFwShaFile(text, strlen(text), o));
  memset(text, 'a', 100);
  TEST_ASSERT_FALSE(rc::parseFwShaFile(text, 100, o));
}

static void test_body_bytes() {
  TEST_ASSERT_EQUAL_size_t(2048, rc::fwBodyBytes(0, 2048, 100000));
  TEST_ASSERT_EQUAL_size_t(256, rc::fwBodyBytes(99000, 256, 100000));
  TEST_ASSERT_EQUAL_size_t(224, rc::fwBodyBytes(99744, 256, 100000));
  TEST_ASSERT_EQUAL_size_t(0, rc::fwBodyBytes(99968, 32, 100000));
  TEST_ASSERT_EQUAL_size_t(0, rc::fwBodyBytes(99990, 10, 100000));
  TEST_ASSERT_EQUAL_size_t(0, rc::fwBodyBytes(0, 20, 20));
  size_t body = 0;
  for (uint32_t off = 0; off < 5000; off += 256) body += rc::fwBodyBytes(off, off + 256 <= 5000 ? 256 : 5000 - off, 5000);
  TEST_ASSERT_EQUAL_size_t(5000 - rc::FW_SHA_BYTES, body);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_offer);
  RUN_TEST(test_offer_upper_hex);
  RUN_TEST(test_offer_rejects);
  RUN_TEST(test_image_info);
  RUN_TEST(test_image_full_fields_terminate);
  RUN_TEST(test_image_rejects);
  RUN_TEST(test_sha_file_roundtrip);
  RUN_TEST(test_sha_file_rejects);
  RUN_TEST(test_body_bytes);
  return UNITY_END();
}
