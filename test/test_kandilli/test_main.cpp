#include <string.h>
#include <unity.h>

#include "kandilli.h"
#include "nager.h"
#include "timeutil.h"

void setUp() {}
void tearDown() {}

static const char kPage[] =
    "<HTML>\r\n<meta http-equiv=\"Content-Type\" content=\"text/html; charset=windows-1254\">\r\n<pre>\r\n"
    "..................T\xDDRK\xDDYE VE YAKIN \xDE" "EVRES\xDDNDEK\xDD SON DEPREMLER....................\r\n"
    "Tarih      Saat      Enlem(N)  Boylam(E) Derinlik(km)  MD   ML   Mw    Yer                                             "
    "\xC7\xF6z\xFCm Niteli\xF0i\r\n"
    "---------- --------  --------  -------   ----------    ------------    --------------                                  "
    "--------------\r\n"
    "2026.10.27 09:06:39  39.1782   28.1533        8.6      -.-  1.5  -.-   SINANDEDE-SINDIRGI (BALIKESIR)                    "
    "\xDDlksel\r\n"
    "2026.10.27 08:53:16  40.8508   28.9227        5.2      -.-  2.1  -.-   MARMARA DENIZI                                    "
    "\xDDlksel\r\n"
    "2026.10.27 08:46:11  38.0587   37.6917       19.9      -.-  4.0  -.-   BEGRE-DOGANSEHIR (MALATYA)                        "
    "\xDDlksel\r\n"
    "2026.10.27 08:17:32  39.7438   26.4547       13.1      -.-  2.5  2.6   YAHSIELI-BAYRAMIC (CANAKKALE)                     "
    "REVIZE01 (2026.10.27 08:20:11)\r\n"
    "2026.10.26 07:58:57  40.1800   28.1562       12.1      -.-  3.1  -.-   SINANDEDE-SINDIRGI (BALIKESIR)                    "
    "\xDDlksel\r\n"
    "2026.10.24 07:23:32  40.2328   28.9000        6.0      -.-  5.0  -.-   ESKI (BURSA)                                      "
    "\xDDlksel\r\n"
    "</pre>\r\n<p>son</p>\r\n";

static uint32_t utcOf(int y, int m, int d, int hh, int mm, int ss) {
  return rc::utcFromCivil(y, m, d, hh, mm, ss) - static_cast<uint32_t>(rc::TZ_OFFSET_SEC);
}

static void test_line_fields() {
  rc::Quake q;
  TEST_ASSERT_TRUE(rc::kandilliLine(
      "2026.10.27 09:06:39  39.1782   28.1533        8.6      -.-  1.5  -.-   SINANDEDE-SINDIRGI (BALIKESIR)       "
      "             Ilksel",
      q));
  TEST_ASSERT_EQUAL_UINT32(utcOf(2026, 10, 27, 9, 6, 39), q.timeUtc);
  TEST_ASSERT_EQUAL_INT16(15, q.mag10);
  TEST_ASSERT_EQUAL_INT16(86, q.depth10);
  TEST_ASSERT_EQUAL_INT32(391782, q.lat1e4);
  TEST_ASSERT_EQUAL_INT32(281533, q.lon1e4);
  TEST_ASSERT_EQUAL_STRING("Sindirgi (Balikesir)", q.place);
}

static void test_line_prefers_mw_and_rejects_headers() {
  rc::Quake q;
  TEST_ASSERT_TRUE(rc::kandilliLine(
      "2026.10.27 08:17:32  39.7438   26.4547       13.1      -.-  2.5  2.6   YAHSIELI-BAYRAMIC (CANAKKALE)  REVIZE01", q));
  TEST_ASSERT_EQUAL_INT16(26, q.mag10);
  TEST_ASSERT_EQUAL_STRING("Bayramic (Canakkale)", q.place);
  TEST_ASSERT_FALSE(rc::kandilliLine("Tarih      Saat      Enlem(N)  Boylam(E)", q));
  TEST_ASSERT_FALSE(rc::kandilliLine("2026.10.27 08:17:32  39.7438   26.4547  13.1  -.-  -.-  -.-   YER", q));
}

static void test_place_forms() {
  char out[56];
  const char* a = "ONIKI ADALAR (EGE DENIZI)";
  rc::kandilliPlace(a, strlen(a), out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("Oniki Adalar (Ege Denizi)", out);
  const char* b = "\xDDZM\xDDR K\xD6RFEZ\xDD";
  rc::kandilliPlace(b, strlen(b), out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("\xC4\xB0zmir K\xC3\xB6rfezi", out);
  char tiny[6];
  rc::kandilliPlace(a, strlen(a), tiny, sizeof(tiny));
  TEST_ASSERT_EQUAL_STRING("Oniki", tiny);
}

static void test_parse_marmara_window() {
  rc::MemSource src(kPage, strlen(kPage));
  rc::QuakeInfo info;
  uint32_t now = utcOf(2026, 10, 27, 9, 30, 0);
  TEST_ASSERT_TRUE(rc::kandilliParse(src, now, 2 * 86400u, rc::marmaraBox(), 20, info));
  TEST_ASSERT_EQUAL_UINT8(1, info.valid);
  TEST_ASSERT_EQUAL_UINT8(2, info.count24h);
  TEST_ASSERT_EQUAL_UINT32(utcOf(2026, 10, 27, 8, 53, 16), info.latest.timeUtc);
  TEST_ASSERT_EQUAL_STRING("Marmara Denizi", info.latest.place);
  TEST_ASSERT_EQUAL_INT16(26, info.strongest.mag10);
}

static void test_parse_stops_outside_window() {
  rc::MemSource src(kPage, strlen(kPage));
  rc::QuakeInfo info;
  uint32_t now = utcOf(2026, 10, 27, 9, 30, 0);
  rc::QuakeBox all = {30.0f, 45.0f, 20.0f, 45.0f};
  TEST_ASSERT_TRUE(rc::kandilliParse(src, now, 2 * 86400u, all, 30, info));
  TEST_ASSERT_EQUAL_UINT8(1, info.count24h);
  TEST_ASSERT_EQUAL_INT16(40, info.latest.mag10);
  TEST_ASSERT_EQUAL_INT16(40, info.strongest.mag10);
}

static void test_parse_empty_page() {
  const char* page = "<HTML><body>bakim</body></HTML>";
  rc::MemSource src(page, strlen(page));
  rc::QuakeInfo info;
  TEST_ASSERT_FALSE(rc::kandilliParse(src, 1000, 3600, rc::marmaraBox(), 20, info));
  TEST_ASSERT_EQUAL_UINT8(0, info.valid);
}

static void test_relay_quake_line() {
  rc::QuakeInfo info;
  TEST_ASSERT_TRUE(rc::relayQuakeLine(
      "3 1793080000 21 52 408508 289227 1793070000 34 -32768 401800 281562\tMarmara Denizi\tSındırgı (Balıkesir)", info));
  TEST_ASSERT_EQUAL_UINT8(3, info.count24h);
  TEST_ASSERT_EQUAL_UINT32(1793080000u, info.latest.timeUtc);
  TEST_ASSERT_EQUAL_INT16(21, info.latest.mag10);
  TEST_ASSERT_EQUAL_INT16(-32768, info.strongest.depth10);
  TEST_ASSERT_EQUAL_STRING("Marmara Denizi", info.latest.place);
  TEST_ASSERT_EQUAL_STRING("Sındırgı (Balıkesir)", info.strongest.place);
}

static void test_relay_quake_empty_and_bad() {
  rc::QuakeInfo info;
  TEST_ASSERT_TRUE(rc::relayQuakeLine("0 0 0 0 0 0 0 0 0 0 0\t\t", info));
  TEST_ASSERT_EQUAL_UINT8(1, info.valid);
  TEST_ASSERT_EQUAL_UINT32(0, info.latest.timeUtc);
  TEST_ASSERT_FALSE(rc::relayQuakeLine("hata zaman asimi", info));
  TEST_ASSERT_FALSE(rc::relayQuakeLine("300 0 0 0 0 0 0 0 0 0 0\t\t", info));
  TEST_ASSERT_FALSE(rc::relayQuakeLine("1 5 21 52 999999999 0 0 0 0 0 0\tx\ty", info));
}

static void test_relay_holiday_line() {
  rc::Religious r;
  TEST_ASSERT_TRUE(rc::relayHolidayLine("2027 3 9 5 16 0", 2027, r));
  TEST_ASSERT_EQUAL_UINT8(1, r.valid);
  TEST_ASSERT_EQUAL_UINT8(rc::REL_NAGER, r.source);
  TEST_ASSERT_EQUAL_UINT8(9, r.ramazanDay);
  TEST_ASSERT_EQUAL_UINT8(16, r.kurbanDay);
  TEST_ASSERT_TRUE(rc::relayHolidayLine("2027 3 9 5 16 1", 2027, r));
  TEST_ASSERT_EQUAL_UINT8(rc::REL_NAGER_TENTATIVE, r.source);
  TEST_ASSERT_FALSE(rc::relayHolidayLine("2026 3 9 5 16 0", 2027, r));
  TEST_ASSERT_FALSE(rc::relayHolidayLine("hata 2027", 2027, r));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_line_fields);
  RUN_TEST(test_line_prefers_mw_and_rejects_headers);
  RUN_TEST(test_place_forms);
  RUN_TEST(test_parse_marmara_window);
  RUN_TEST(test_parse_stops_outside_window);
  RUN_TEST(test_parse_empty_page);
  RUN_TEST(test_relay_quake_line);
  RUN_TEST(test_relay_quake_empty_and_bad);
  RUN_TEST(test_relay_holiday_line);
  return UNITY_END();
}
