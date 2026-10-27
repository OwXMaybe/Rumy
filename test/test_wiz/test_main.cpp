#include <string.h>
#include <unity.h>

#include "wiz_proto.h"

static uint8_t g_mem[4096];
static rc::JsonArena g_arena(g_mem, sizeof(g_mem));

void setUp() {}
void tearDown() {}

static void test_set_pilot_rgb() {
  char buf[160];
  rc::Pilot p = rc::pilotRgbRaw(255, 10, 0, 60);
  size_t n = rc::wizBuildSetPilot(p, buf, sizeof(buf));
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_EQUAL_STRING("{\"method\":\"setPilot\",\"params\":{\"state\":true,\"dimming\":60,\"r\":255,\"g\":10,\"b\":0}}", buf);
  TEST_ASSERT_NULL(strstr(buf, "temp"));
}

static void test_set_pilot_temp_has_no_rgb() {
  char buf[160];
  rc::Pilot p = rc::pilotTemp(2700, 35);
  rc::wizBuildSetPilot(p, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("{\"method\":\"setPilot\",\"params\":{\"state\":true,\"dimming\":35,\"temp\":2700}}", buf);
  TEST_ASSERT_NULL(strstr(buf, "\"r\""));
}

static void test_set_pilot_clamps() {
  char buf[160];
  rc::Pilot p = rc::pilotTemp(9000, 3);
  rc::wizBuildSetPilot(p, buf, sizeof(buf));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"dimming\":10"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"temp\":6500"));
  p = rc::pilotTemp(1000, 250);
  rc::wizBuildSetPilot(p, buf, sizeof(buf));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"dimming\":100"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "\"temp\":2200"));
}

static void test_set_pilot_off() {
  char buf[160];
  rc::Pilot p = rc::pilotOff(rc::pilotRgb(120, 80));
  rc::wizBuildSetPilot(p, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("{\"method\":\"setPilot\",\"params\":{\"state\":false}}", buf);
}

static void test_small_buffer_fails() {
  char buf[16];
  TEST_ASSERT_EQUAL_UINT32(0, rc::wizBuildSetPilot(rc::pilotRgb(0, 50), buf, sizeof(buf)));
}

static void test_registration_and_get() {
  char buf[200];
  rc::wizBuildRegistration(buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING(
      "{\"method\":\"registration\",\"params\":{\"phoneMac\":\"AAAAAAAAAAAA\",\"register\":false,\"phoneIp\":\"1.2.3.4\","
      "\"id\":\"1\"}}",
      buf);
  rc::wizBuildGetPilot(buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("{\"method\":\"getPilot\",\"params\":{}}", buf);
}

static void test_parse_registration() {
  const char* msg = "{\"method\":\"registration\",\"env\":\"pro\",\"result\":{\"mac\":\"A8BB50E1C3F2\",\"success\":true}}";
  rc::WizReply r;
  TEST_ASSERT_TRUE(rc::wizParseReply(msg, strlen(msg), g_arena, r));
  TEST_ASSERT_TRUE(r.method == rc::WizMethod::Registration);
  TEST_ASSERT_EQUAL_UINT8(1, r.hasMac);
  TEST_ASSERT_EQUAL_STRING("a8bb50e1c3f2", r.mac);
  TEST_ASSERT_EQUAL_UINT8(1, r.success);
}

static void test_parse_getpilot_rgb() {
  const char* msg =
      "{\"method\":\"getPilot\",\"env\":\"pro\",\"result\":{\"mac\":\"a8bb50e1c3f2\",\"rssi\":-61,\"state\":true,"
      "\"sceneId\":0,\"r\":0,\"g\":0,\"b\":255,\"c\":0,\"w\":0,\"dimming\":42}}";
  rc::WizReply r;
  TEST_ASSERT_TRUE(rc::wizParseReply(msg, strlen(msg), g_arena, r));
  TEST_ASSERT_EQUAL_UINT8(1, r.hasPilot);
  TEST_ASSERT_EQUAL_UINT8(1, r.pilot.on);
  TEST_ASSERT_EQUAL_UINT8(rc::PILOT_RGB, r.pilot.mode);
  TEST_ASSERT_EQUAL_UINT8(255, r.pilot.b);
  TEST_ASSERT_EQUAL_UINT8(42, r.pilot.dim);
  TEST_ASSERT_EQUAL_UINT16(240, r.pilot.hue);
}

static void test_parse_getpilot_temp_and_off() {
  const char* msg =
      "{\"method\":\"getPilot\",\"env\":\"pro\",\"result\":{\"mac\":\"a8bb50e1c3f2\",\"state\":false,\"sceneId\":0,"
      "\"temp\":3000,\"dimming\":5}}";
  rc::WizReply r;
  TEST_ASSERT_TRUE(rc::wizParseReply(msg, strlen(msg), g_arena, r));
  TEST_ASSERT_EQUAL_UINT8(0, r.pilot.on);
  TEST_ASSERT_EQUAL_UINT8(rc::PILOT_TEMP, r.pilot.mode);
  TEST_ASSERT_EQUAL_UINT16(3000, r.pilot.temp);
  TEST_ASSERT_EQUAL_UINT8(10, r.pilot.dim);
}

static void test_parse_getpilot_scene() {
  const char* msg =
      "{\"method\":\"getPilot\",\"result\":{\"mac\":\"a8bb50e1c3f2\",\"state\":true,\"sceneId\":4,\"speed\":100,"
      "\"dimming\":80}}";
  rc::WizReply r;
  TEST_ASSERT_TRUE(rc::wizParseReply(msg, strlen(msg), g_arena, r));
  TEST_ASSERT_EQUAL_UINT8(rc::PILOT_SCENE, r.pilot.mode);
  TEST_ASSERT_EQUAL_UINT8(4, r.pilot.scene);
}

static void test_parse_garbage() {
  rc::WizReply r;
  const char* bad = "{\"method\":\"getPilot\",\"result\":{";
  TEST_ASSERT_FALSE(rc::wizParseReply(bad, strlen(bad), g_arena, r));
  const char* err = "{\"method\":\"setPilot\",\"error\":{\"code\":-32600,\"message\":\"Invalid Request\"}}";
  TEST_ASSERT_FALSE(rc::wizParseReply(err, strlen(err), g_arena, r));
  TEST_ASSERT_EQUAL_INT16(-32600, r.errorCode);
  TEST_ASSERT_EQUAL_UINT32(0, g_arena.used());
  const char* ok = "{\"method\":\"setPilot\",\"result\":{\"success\":true}}";
  TEST_ASSERT_TRUE(rc::wizParseReply(ok, strlen(ok), g_arena, r));
  TEST_ASSERT_EQUAL_INT16(0, r.errorCode);
  TEST_ASSERT_EQUAL_UINT8(1, r.success);
}

static void test_parse_signed_firmware() {
  const char* denied =
      "{\"method\":\"setPilot\",\"env\":\"pro\",\"error\":{\"code\":-32602,\"message\":\"Invalid params\","
      "\"sigTs\":1790784151},\"hmac\":\"ou/G0NXlEijd9ZcNjmRMxk9NsQgK4Qu6Uu/GJsHeidI=\"}";
  rc::WizReply r;
  TEST_ASSERT_FALSE(rc::wizParseReply(denied, strlen(denied), g_arena, r));
  TEST_ASSERT_TRUE(r.method == rc::WizMethod::SetPilot);
  TEST_ASSERT_EQUAL_INT16(rc::WIZ_ERR_INVALID_PARAMS, r.errorCode);
  TEST_ASSERT_EQUAL_UINT32(0, g_arena.used());
  const char* state =
      "{\"method\":\"getPilot\",\"env\":\"pro\",\"result\":{\"mac\":\"a8bb5012d4f0\",\"rssi\":-53,\"state\":true,"
      "\"sceneId\":14,\"sigTs\":1790784151},\"hmac\":\"cjeHm/dx11t927vjmCRbf+13n1OchrbcVagni+oJRbI=\"}";
  TEST_ASSERT_TRUE(rc::wizParseReply(state, strlen(state), g_arena, r));
  TEST_ASSERT_EQUAL_INT16(0, r.errorCode);
  TEST_ASSERT_EQUAL_STRING("a8bb5012d4f0", r.mac);
  TEST_ASSERT_EQUAL_UINT8(1, r.pilot.on);
  TEST_ASSERT_EQUAL_UINT8(rc::PILOT_SCENE, r.pilot.mode);
  TEST_ASSERT_EQUAL_UINT8(14, r.pilot.scene);
}

static void test_pilot_same() {
  rc::Pilot a = rc::pilotRgb(10, 50);
  rc::Pilot b = rc::pilotRgb(10, 50);
  TEST_ASSERT_TRUE(rc::pilotSame(a, b));
  b.dim = 51;
  TEST_ASSERT_FALSE(rc::pilotSame(a, b));
  rc::Pilot offA = rc::pilotOff(a);
  rc::Pilot offB = rc::pilotOff(rc::pilotTemp(3000, 20));
  TEST_ASSERT_TRUE(rc::pilotSame(offA, offB));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_set_pilot_rgb);
  RUN_TEST(test_set_pilot_temp_has_no_rgb);
  RUN_TEST(test_set_pilot_clamps);
  RUN_TEST(test_set_pilot_off);
  RUN_TEST(test_small_buffer_fails);
  RUN_TEST(test_registration_and_get);
  RUN_TEST(test_parse_registration);
  RUN_TEST(test_parse_getpilot_rgb);
  RUN_TEST(test_parse_getpilot_temp_and_off);
  RUN_TEST(test_parse_getpilot_scene);
  RUN_TEST(test_parse_garbage);
  RUN_TEST(test_parse_signed_firmware);
  RUN_TEST(test_pilot_same);
  return UNITY_END();
}
