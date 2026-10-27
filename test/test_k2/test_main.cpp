#include <string.h>
#include <unity.h>

#include "../fixtures/fixtures.h"
#include "k2.h"

static uint8_t g_mem[8192];
static rc::JsonArena g_arena(g_mem, sizeof(g_mem));

void setUp() {}
void tearDown() {}

static bool parse(const char* json, rc::K2Status& st, uint32_t& present) {
  rc::MemSource src(json, strlen(json));
  return rc::k2Parse(src, g_arena, st, present);
}

static void test_full_message_from_printer() {
  rc::K2Status st;
  rc::k2Clear(st);
  uint32_t present = 0;
  TEST_ASSERT_TRUE(parse(FX_K2_FULL, st, present));
  TEST_ASSERT_EQUAL_STRING("K2-YAZICI", st.host);
  TEST_ASSERT_EQUAL_STRING("F021", st.model);
  TEST_ASSERT_EQUAL_INT16(2, st.state);
  TEST_ASSERT_EQUAL_UINT8(100, st.progress);
  TEST_ASSERT_EQUAL_UINT32(0, st.leftS);
  TEST_ASSERT_EQUAL_UINT32(3413, st.jobS);
  TEST_ASSERT_EQUAL_UINT32(1790423916u, st.startUtc);
  TEST_ASSERT_EQUAL_STRING("Telefon_Standi_V2.stl_PLA_54m36s.gcode", st.file);
  TEST_ASSERT_EQUAL_INT16(233, st.nozzle10);
  TEST_ASSERT_EQUAL_INT16(219, st.bed10);
  TEST_ASSERT_EQUAL_INT16(230, st.box10);
  TEST_ASSERT_EQUAL_INT16(0, st.nozzleTarget10);
  TEST_ASSERT_EQUAL_INT32(0, st.errCode);
  TEST_ASSERT_EQUAL_UINT8(100, st.selfTest);
  TEST_ASSERT_EQUAL(rc::K2_DONE, rc::k2Phase(st));
  TEST_ASSERT_FALSE(rc::k2Problem(st));
  TEST_ASSERT_TRUE(present & rc::K2F_ERR);
  TEST_ASSERT_FALSE(present & rc::K2F_HEARTBEAT);
  TEST_ASSERT_TRUE(g_arena.peak() < sizeof(g_mem));
  TEST_ASSERT_EQUAL_UINT32(0, g_arena.failures());
}

static void test_partial_update_keeps_other_fields() {
  rc::K2Status st;
  rc::k2Clear(st);
  uint32_t present = 0;
  TEST_ASSERT_TRUE(parse(FX_K2_FULL, st, present));
  TEST_ASSERT_TRUE(parse(FX_K2_TEMPS, st, present));
  TEST_ASSERT_EQUAL_UINT32(rc::K2F_NOZZLE | rc::K2F_BED, present);
  TEST_ASSERT_EQUAL_INT16(233, st.nozzle10);
  TEST_ASSERT_EQUAL_INT16(210, st.bed10);
  TEST_ASSERT_EQUAL_STRING("K2-YAZICI", st.host);
  TEST_ASSERT_EQUAL_UINT32(3413, st.jobS);
}

static void test_printing_state() {
  rc::K2Status st;
  rc::k2Clear(st);
  uint32_t present = 0;
  TEST_ASSERT_TRUE(parse(FX_K2_PRINTING, st, present));
  TEST_ASSERT_EQUAL(rc::K2_PRINTING, rc::k2Phase(st));
  TEST_ASSERT_EQUAL_UINT8(45, st.progress);
  TEST_ASSERT_EQUAL_UINT32(4320, st.leftS);
  TEST_ASSERT_EQUAL_UINT16(120, st.layer);
  TEST_ASSERT_EQUAL_UINT16(340, st.totalLayer);
  TEST_ASSERT_EQUAL_INT16(2099, st.nozzle10);
  TEST_ASSERT_EQUAL_INT16(2100, st.nozzleTarget10);
  TEST_ASSERT_EQUAL_INT16(599, st.bed10);
  TEST_ASSERT_EQUAL_INT16(350, st.box10);
  TEST_ASSERT_EQUAL_UINT8(100, st.speedPct);
  TEST_ASSERT_FALSE(rc::k2Problem(st));
}

static void test_error_code() {
  rc::K2Status st;
  rc::k2Clear(st);
  uint32_t present = 0;
  TEST_ASSERT_TRUE(parse(FX_K2_PRINTING, st, present));
  TEST_ASSERT_TRUE(parse(FX_K2_ERROR, st, present));
  TEST_ASSERT_EQUAL(rc::K2_ERROR, rc::k2Phase(st));
  TEST_ASSERT_TRUE(rc::k2Problem(st));
  TEST_ASSERT_EQUAL_INT32(2845, st.errCode);
  TEST_ASSERT_EQUAL_INT32(3, st.errKey);
  TEST_ASSERT_EQUAL_STRING("E2845", st.errText);
  TEST_ASSERT_EQUAL_UINT8(45, st.progress);
}

static void test_error_cleared() {
  rc::K2Status st;
  rc::k2Clear(st);
  uint32_t present = 0;
  TEST_ASSERT_TRUE(parse(FX_K2_ERROR, st, present));
  TEST_ASSERT_TRUE(parse(FX_K2_PRINTING, st, present));
  TEST_ASSERT_EQUAL_INT32(0, st.errCode);
  TEST_ASSERT_EQUAL_STRING("", st.errText);
  TEST_ASSERT_EQUAL(rc::K2_PRINTING, rc::k2Phase(st));
}

static void test_filament_runout() {
  rc::K2Status st;
  rc::k2Clear(st);
  uint32_t present = 0;
  TEST_ASSERT_TRUE(parse(FX_K2_RUNOUT, st, present));
  TEST_ASSERT_EQUAL(rc::K2_PAUSED, rc::k2Phase(st));
  TEST_ASSERT_TRUE(rc::k2Runout(st));
  TEST_ASSERT_TRUE(rc::k2Problem(st));
}

static void test_heartbeat() {
  rc::K2Status st;
  rc::k2Clear(st);
  uint32_t present = 0;
  TEST_ASSERT_TRUE(parse(FX_K2_HEARTBEAT, st, present));
  TEST_ASSERT_EQUAL_UINT32(rc::K2F_HEARTBEAT, present);
  TEST_ASSERT_EQUAL_UINT32(0, st.seen);
  TEST_ASSERT_EQUAL(rc::K2_IDLE, rc::k2Phase(st));
}

static void test_not_json() {
  rc::K2Status st;
  rc::k2Clear(st);
  uint32_t present = 0;
  TEST_ASSERT_FALSE(parse("ok", st, present));
  TEST_ASSERT_FALSE(parse("[1,2]", st, present));
  TEST_ASSERT_EQUAL_UINT32(0, present);
}

static void test_job_name() {
  char name[48];
  char mat[12];
  rc::k2JobName("Telefon_Standi_V2.stl_PLA_54m36s.gcode", name, sizeof(name), mat, sizeof(mat));
  TEST_ASSERT_EQUAL_STRING("Telefon Standi V2", name);
  TEST_ASSERT_EQUAL_STRING("PLA", mat);
  rc::k2JobName("/mnt/UDISK/printer_data/gcodes/Benchy_Test.3mf_PETG_1h12m5s.gcode", name, sizeof(name), mat,
                sizeof(mat));
  TEST_ASSERT_EQUAL_STRING("Benchy Test", name);
  TEST_ASSERT_EQUAL_STRING("PETG", mat);
  rc::k2JobName("Vazo_PLA_2h3m.gcode", name, sizeof(name), mat, sizeof(mat));
  TEST_ASSERT_EQUAL_STRING("Vazo", name);
  TEST_ASSERT_EQUAL_STRING("PLA", mat);
  rc::k2JobName("plain_part.gcode", name, sizeof(name), mat, sizeof(mat));
  TEST_ASSERT_EQUAL_STRING("plain part", name);
  TEST_ASSERT_EQUAL_STRING("", mat);
  rc::k2JobName("", name, sizeof(name), mat, sizeof(mat));
  TEST_ASSERT_EQUAL_STRING("", name);
}

static void test_request_and_texts() {
  char buf[64];
  TEST_ASSERT_TRUE(rc::k2BuildStatusRequest(buf, sizeof(buf)) > 0);
  TEST_ASSERT_EQUAL_STRING("{\"method\":\"get\",\"params\":{\"ReqPrinterPara\":1}}", buf);
  TEST_ASSERT_EQUAL_UINT32(0, rc::k2BuildStatusRequest(buf, 8));
  TEST_ASSERT_EQUAL_STRING("Yazdırılıyor", rc::k2PhaseTr(rc::K2_PRINTING));
  TEST_ASSERT_EQUAL_STRING("Hata", rc::k2PhaseTr(rc::K2_ERROR));
}

static void test_host_valid() {
  TEST_ASSERT_TRUE(rc::k2HostValid("192.168.1.19"));
  TEST_ASSERT_TRUE(rc::k2HostValid("10.0.0.1"));
  TEST_ASSERT_TRUE(rc::k2HostValid("255.255.255.255"));
  TEST_ASSERT_TRUE(rc::k2HostValid("0.0.0.0"));
  TEST_ASSERT_FALSE(rc::k2HostValid("K2-YAZICI"));
  TEST_ASSERT_FALSE(rc::k2HostValid("k2-yazici.local"));
  TEST_ASSERT_FALSE(rc::k2HostValid(""));
  TEST_ASSERT_FALSE(rc::k2HostValid(nullptr));
  TEST_ASSERT_FALSE(rc::k2HostValid("192.168.1"));
  TEST_ASSERT_FALSE(rc::k2HostValid("192.168.1.256"));
  TEST_ASSERT_FALSE(rc::k2HostValid("192.168.01.5"));
  TEST_ASSERT_FALSE(rc::k2HostValid("1.2.3.4.5"));
  TEST_ASSERT_FALSE(rc::k2HostValid("1..2.3"));
  TEST_ASSERT_FALSE(rc::k2HostValid("1234.1.1.1"));
  TEST_ASSERT_FALSE(rc::k2HostValid("192.168.1.5 "));
  TEST_ASSERT_FALSE(rc::k2HostValid("-1.2.3.4"));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_full_message_from_printer);
  RUN_TEST(test_partial_update_keeps_other_fields);
  RUN_TEST(test_printing_state);
  RUN_TEST(test_error_code);
  RUN_TEST(test_error_cleared);
  RUN_TEST(test_filament_runout);
  RUN_TEST(test_heartbeat);
  RUN_TEST(test_not_json);
  RUN_TEST(test_job_name);
  RUN_TEST(test_request_and_texts);
  RUN_TEST(test_host_valid);
  return UNITY_END();
}
