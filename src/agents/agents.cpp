#include "agents.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "app/config.h"

namespace agents {

namespace {

rc::Agents s_cur;
rc::Agents s_rx;
uint8_t s_got = 0;
bool s_rxOn = false;
uint32_t s_ver = 0;
uint32_t s_headAt = 0;
uint32_t s_heads = 0;
uint32_t s_bad = 0;

void commit() {
  s_rx.valid = true;
  s_rx.atMs = millis();
  s_cur = s_rx;
  s_rxOn = false;
  s_ver++;
}

}

void head(const char* args) {
  rc::AgentsHead h;
  if (!rc::parseAgentsHead(args, h)) {
    s_bad++;
    return;
  }
  s_heads++;
  s_headAt = millis() | 1u;
  rc::agentsClear(s_rx);
  s_rx.ver = h.ver;
  s_rx.n = h.n;
  s_rx.work = h.work;
  s_rx.wait = h.wait;
  s_rx.idle = h.idle;
  s_got = 0;
  s_rxOn = true;
  if (!h.n) commit();
}

void row(char* fields) {
  uint8_t idx;
  rc::AgentRow r;
  if (!s_rxOn || !rc::parseAgentRow(fields, idx, r) || idx >= s_rx.n) {
    s_bad++;
    return;
  }
  s_rx.rows[idx] = r;
  s_got |= static_cast<uint8_t>(1u << idx);
  if (s_got == static_cast<uint8_t>((1u << s_rx.n) - 1u)) commit();
}

void loop() {
  if (!s_cur.valid || !s_headAt || millis() - s_headAt < cfg::AGENT_LINK_MS) return;
  rc::agentsClear(s_cur);
  s_rxOn = false;
  s_ver++;
}

const rc::Agents& state() { return s_cur; }

uint32_t version() { return s_ver; }

uint32_t spanS(const rc::AgentRow& r) { return r.sinceS + (millis() - s_cur.atMs) / 1000U; }

bool demo(const char* what) {
  static const struct {
    const char* name;
    const char* head;
    const char* row;
  } kDemo[] = {
      {"calisiyor", "d1 1 1 0 0", "0\tdeadbeef\tc\t200\t200\tclaude\tdeneme\tKomut çalıştırıyor"},
      {"bekliyor", "d2 1 0 1 0", "0\tdeadbeef\tb\t0\t200\tclaude\tdeneme\tİzin istiyor: Bash"},
      {"bitti", "d3 1 0 0 1", "0\tdeadbeef\ti\t0\t300\tclaude\tdeneme\t"},
      {"codex", "d5 1 0 1 0", "0\tfeedbeef\tb\t0\t90\tcodex\tdeneme\tİzin istiyor: Bash"},
      {"kapat", "d4 0 0 0 0", nullptr},
  };
  for (const auto& d : kDemo) {
    if (strcmp(what, d.name)) continue;
    head(d.head);
    if (d.row) {
      char buf[96];
      snprintf(buf, sizeof(buf), "%s", d.row);
      row(buf);
    }
    return true;
  }
  return false;
}

void report(Print& out) {
  const rc::Agents& a = s_cur;
  out.printf("[ajan] %s · calisan %u · bekleyen %u · bosta %u · mesaj %lu · hatali %lu\n",
             a.valid ? "bagli" : "yok", static_cast<unsigned>(a.work), static_cast<unsigned>(a.wait),
             static_cast<unsigned>(a.idle), static_cast<unsigned long>(s_heads), static_cast<unsigned long>(s_bad));
  static const char* const kSt[] = {"-", "bosta", "calisiyor", "bekliyor"};
  for (int i = 0; i < a.n; i++) {
    const rc::AgentRow& r = a.rows[i];
    out.printf("  %08lx %-9s %5lu sn  %s · %s\n", static_cast<unsigned long>(r.key), kSt[r.status & 3],
               static_cast<unsigned long>(spanS(r)), r.project, r.detail);
  }
}

}
