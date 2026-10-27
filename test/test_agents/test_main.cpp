#include <string.h>
#include <unity.h>

#include "agents_proto.h"

void setUp() {}
void tearDown() {}

static rc::AgentRow row(uint32_t key, uint8_t st, uint32_t since, uint32_t work, const char* project) {
  rc::AgentRow r;
  memset(&r, 0, sizeof(r));
  r.key = key;
  r.status = st;
  r.sinceS = since;
  r.workS = work;
  strcpy(r.project, project);
  return r;
}

static rc::Agents agents(int n, const rc::AgentRow* rows) {
  rc::Agents a;
  rc::agentsClear(a);
  a.valid = true;
  a.n = static_cast<uint8_t>(n);
  for (int i = 0; i < n; i++) {
    a.rows[i] = rows[i];
    if (rows[i].status == rc::AG_WORK) a.work++;
    if (rows[i].status == rc::AG_WAIT) a.wait++;
    if (rows[i].status == rc::AG_IDLE) a.idle++;
  }
  return a;
}

static void test_head() {
  rc::AgentsHead h;
  TEST_ASSERT_TRUE(rc::parseAgentsHead("1f 3 2 1 4", h));
  TEST_ASSERT_EQUAL_HEX32(0x1f, h.ver);
  TEST_ASSERT_EQUAL_UINT8(3, h.n);
  TEST_ASSERT_EQUAL_UINT8(2, h.work);
  TEST_ASSERT_EQUAL_UINT8(1, h.wait);
  TEST_ASSERT_EQUAL_UINT8(4, h.idle);
  TEST_ASSERT_TRUE(rc::parseAgentsHead("2 9 9 0 0", h));
  TEST_ASSERT_EQUAL_UINT8(rc::AGENT_ROWS, h.n);
  TEST_ASSERT_FALSE(rc::parseAgentsHead("2 1 1 0", h));
  TEST_ASSERT_FALSE(rc::parseAgentsHead("", h));
}

static void test_row() {
  char line[] = "2\ta1b2c3d4\tb\t40\t150\tclaude\tRumy\tİzin istiyor: Bash";
  uint8_t idx = 0;
  rc::AgentRow r;
  TEST_ASSERT_TRUE(rc::parseAgentRow(line, idx, r));
  TEST_ASSERT_EQUAL_UINT8(2, idx);
  TEST_ASSERT_EQUAL_HEX32(0xa1b2c3d4, r.key);
  TEST_ASSERT_EQUAL_UINT8(rc::AG_WAIT, r.status);
  TEST_ASSERT_EQUAL_UINT8(rc::AK_CLAUDE, r.agent);
  TEST_ASSERT_EQUAL_UINT32(40, r.sinceS);
  TEST_ASSERT_EQUAL_UINT32(150, r.workS);
  TEST_ASSERT_EQUAL_STRING("Rumy", r.project);
  TEST_ASSERT_EQUAL_STRING("İzin istiyor: Bash", r.detail);
  char idle[] = "0\tff\ti\t3\t420\tcodex\tSatın alma PDF'si\t";
  TEST_ASSERT_TRUE(rc::parseAgentRow(idle, idx, r));
  TEST_ASSERT_EQUAL_UINT8(rc::AG_IDLE, r.status);
  TEST_ASSERT_EQUAL_UINT8(rc::AK_CODEX, r.agent);
  TEST_ASSERT_EQUAL_STRING("Satın alma PDF'si", r.project);
  TEST_ASSERT_EQUAL_STRING("", r.detail);
  TEST_ASSERT_EQUAL_STRING("Codex", rc::agentName(rc::AK_CODEX));
  TEST_ASSERT_EQUAL_STRING("Claude", rc::agentName(rc::AK_CLAUDE));
  char bad[] = "0\tff\tx\t3\t4\tclaude\tp\td";
  TEST_ASSERT_FALSE(rc::parseAgentRow(bad, idx, r));
  char far[] = "6\tff\tc\t3\t4\tclaude\tp\td";
  TEST_ASSERT_FALSE(rc::parseAgentRow(far, idx, r));
  char shortLine[] = "0\tff\tc\t3\t4\tclaude\tp";
  TEST_ASSERT_FALSE(rc::parseAgentRow(shortLine, idx, r));
}

static void test_row_clips_utf8() {
  char line[] = "0\t1\tc\t1\t1\tclaude\tçok çok çok çok uzun proje adı burada\td";
  uint8_t idx;
  rc::AgentRow r;
  TEST_ASSERT_TRUE(rc::parseAgentRow(line, idx, r));
  TEST_ASSERT_TRUE(strlen(r.project) < sizeof(r.project));
  size_t n = strlen(r.project);
  TEST_ASSERT_TRUE(n > 0);
  TEST_ASSERT_TRUE((static_cast<uint8_t>(r.project[n - 1]) & 0xC0) != 0xC0);
}

static void test_top() {
  rc::Agents a;
  rc::agentsClear(a);
  TEST_ASSERT_EQUAL_UINT8(rc::AG_NONE, rc::agentsTop(a));
  a.valid = true;
  TEST_ASSERT_EQUAL_UINT8(rc::AG_NONE, rc::agentsTop(a));
  a.idle = 2;
  TEST_ASSERT_EQUAL_UINT8(rc::AG_IDLE, rc::agentsTop(a));
  a.work = 3;
  TEST_ASSERT_EQUAL_UINT8(rc::AG_WORK, rc::agentsTop(a));
  TEST_ASSERT_EQUAL_UINT8(3, rc::agentsTopCount(a));
  a.wait = 1;
  TEST_ASSERT_EQUAL_UINT8(rc::AG_WAIT, rc::agentsTop(a));
  TEST_ASSERT_EQUAL_UINT8(1, rc::agentsTopCount(a));
}

static void test_notes_wait_and_done() {
  rc::AgentRow b1[] = {row(1, rc::AG_WORK, 10, 10, "a"), row(2, rc::AG_WORK, 200, 200, "b")};
  rc::AgentRow a1[] = {row(1, rc::AG_WAIT, 0, 12, "a"), row(2, rc::AG_IDLE, 0, 205, "b")};
  rc::Agents before = agents(2, b1);
  rc::Agents after = agents(2, a1);
  rc::AgentNote out[4];
  int n = rc::agentsNotes(before, after, 120, out, 4);
  TEST_ASSERT_EQUAL_INT(2, n);
  TEST_ASSERT_EQUAL_UINT8(rc::AN_WAIT, out[0].kind);
  TEST_ASSERT_EQUAL_HEX32(1, out[0].row.key);
  TEST_ASSERT_EQUAL_UINT8(rc::AN_DONE, out[1].kind);
  TEST_ASSERT_EQUAL_HEX32(2, out[1].row.key);
}

static void test_notes_quiet_cases() {
  rc::AgentRow b1[] = {row(1, rc::AG_WORK, 10, 30, "kisa"), row(2, rc::AG_WAIT, 5, 5, "zaten")};
  rc::AgentRow a1[] = {row(1, rc::AG_IDLE, 0, 30, "kisa"), row(2, rc::AG_WAIT, 9, 9, "zaten")};
  rc::Agents before = agents(2, b1);
  rc::Agents after = agents(2, a1);
  rc::AgentNote out[4];
  TEST_ASSERT_EQUAL_INT(0, rc::agentsNotes(before, after, 120, out, 4));
  rc::Agents none;
  rc::agentsClear(none);
  TEST_ASSERT_EQUAL_INT(0, rc::agentsNotes(none, after, 120, out, 4));
  rc::AgentRow fresh[] = {row(7, rc::AG_WAIT, 0, 0, "yeni")};
  rc::Agents add = agents(1, fresh);
  TEST_ASSERT_EQUAL_INT(1, rc::agentsNotes(before, add, 120, out, 4));
  TEST_ASSERT_EQUAL_UINT8(rc::AN_WAIT, out[0].kind);
  rc::AgentRow idleNew[] = {row(8, rc::AG_IDLE, 0, 600, "acildi")};
  TEST_ASSERT_EQUAL_INT(0, rc::agentsNotes(before, agents(1, idleNew), 120, out, 4));
}

static void test_notes_cap() {
  rc::AgentRow b1[] = {row(1, rc::AG_WORK, 1, 1, "a"), row(2, rc::AG_WORK, 1, 1, "b"), row(3, rc::AG_WORK, 1, 1, "c")};
  rc::AgentRow a1[] = {row(1, rc::AG_WAIT, 0, 1, "a"), row(2, rc::AG_WAIT, 0, 1, "b"), row(3, rc::AG_WAIT, 0, 1, "c")};
  rc::AgentNote out[2];
  TEST_ASSERT_EQUAL_INT(2, rc::agentsNotes(agents(3, b1), agents(3, a1), 120, out, 2));
}

static void test_span() {
  char buf[24];
  rc::agentSpanTr(44, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("44 sn", buf);
  rc::agentSpanTr(185, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("3 dk", buf);
  rc::agentSpanTr(3600, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("1 sa", buf);
  rc::agentSpanTr(3900, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("1 sa 5 dk", buf);
  rc::agentSpanTr(90000, buf, sizeof(buf));
  TEST_ASSERT_EQUAL_STRING("25 sa", buf);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_head);
  RUN_TEST(test_row);
  RUN_TEST(test_row_clips_utf8);
  RUN_TEST(test_top);
  RUN_TEST(test_notes_wait_and_done);
  RUN_TEST(test_notes_quiet_cases);
  RUN_TEST(test_notes_cap);
  RUN_TEST(test_span);
  return UNITY_END();
}
