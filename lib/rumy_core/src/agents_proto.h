#pragma once

#include <stddef.h>
#include <stdint.h>

namespace rc {

constexpr int AGENT_ROWS = 6;

enum AgentStatus : uint8_t { AG_NONE = 0, AG_IDLE, AG_WORK, AG_WAIT };

enum AgentKind : uint8_t { AK_CLAUDE = 0, AK_CODEX };

struct AgentRow {
  uint32_t key;
  uint8_t status;
  uint8_t agent;
  uint32_t sinceS;
  uint32_t workS;
  char project[28];
  char detail[40];
};

struct Agents {
  bool valid;
  uint32_t ver;
  uint32_t atMs;
  uint8_t n;
  uint8_t work;
  uint8_t wait;
  uint8_t idle;
  AgentRow rows[AGENT_ROWS];
};

struct AgentsHead {
  uint32_t ver;
  uint8_t n;
  uint8_t work;
  uint8_t wait;
  uint8_t idle;
};

bool parseAgentsHead(const char* args, AgentsHead& out);
bool parseAgentRow(char* fields, uint8_t& index, AgentRow& out);
void agentsClear(Agents& a);
uint8_t agentsTop(const Agents& a);
uint8_t agentsTopCount(const Agents& a);

enum AgentNoteKind : uint8_t { AN_NONE = 0, AN_WAIT, AN_DONE };

struct AgentNote {
  uint8_t kind;
  AgentRow row;
};

int agentsNotes(const Agents& before, const Agents& after, uint32_t doneMinS, AgentNote* out, int cap);
void agentSpanTr(uint32_t s, char* out, size_t cap);
const char* agentName(uint8_t agent);

}
