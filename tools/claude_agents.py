"""Kod ajanlari -> Rumy (ClawLight uyarlamasi): Claude Code ve Codex oturumlarinin durumunu Rumy'ye iletir.

tools/claude_hook.py, Claude Code'un ve Codex'in hook'lariyla her oturumun durumunu ~/.claude/rumy/oturumlar/ altina
yazar. Bu modul (kopru acilinca kendiliginden baslar) klasoru 0,5 sn'de bir okur, oturumlari aciliyete gore siralar
(seni bekleyen > calisan > biten) ve degisince, degismese de 10 sn'de bir Rumy'ye yollar. Biten oturumlar 30 dk sonra,
24 saattir haber vermeyenler ve sureci kapanmis olanlar listeden duser. Codex oturumlarinin adi Codex'in kendi konusma
basligidir (~/.codex/state_*.sqlite, yalnizca okunur; yoksa session_index.jsonl); Claude oturumlarinin adi calisma
klasorudur.

Rumy'ye giden satirlar:
    @ajanlar <surum-hex> <satir> <calisan> <bekleyen> <bosta>
    @ajan<TAB>sira<TAB>kimlik-hex8<TAB>c|b|i<TAB>durum_sn<TAB>is_sn<TAB>claude|codex<TAB>proje<TAB>ayrinti
        c calisiyor (durum_sn: istegin baslangicindan beri), b seni bekliyor, i bitti (is_sn: son isin suresi)

Kurulum (hook'lar):
    Claude Code: ~/.claude/settings.json (10 olay, async, once yedek).
    Codex: ~/.codex/hooks.json (8 olay, async; baska araclarin hook'lari yerinden oynatilmaz, Codex hook guvenini sirayla
    tuttugu icin). Codex yeni hook'lari bir kez onaylamani ister (/hooks); Codex'in config.toml'una dokunulmaz.

Kullanim:
    python tools/claude_agents.py --kur [claude|codex]     hook'lari ekler (varsayilan ikisi)
    python tools/claude_agents.py --kaldir [claude|codex]  hook'lari kaldirir
    python tools/claude_agents.py --durum     oturumlari bir kez listeler
    python tools/claude_agents.py --deneme    Rumy'siz: Rumy'ye gidecek satirlari yazar
"""
import ctypes
import glob
import json
import os
import queue
import shutil
import sqlite3
import sys
import threading
import time
import unicodedata
import urllib.request
import zlib

HOME = os.path.expanduser("~")
DIR = os.path.join(HOME, ".claude", "rumy", "oturumlar")
SETTINGS = os.path.join(HOME, ".claude", "settings.json")
CODEX_HOME = os.environ.get("CODEX_HOME") or os.path.join(HOME, ".codex")
CODEX_HOOKS = os.path.join(CODEX_HOME, "hooks.json")
HOOK = os.path.join(os.path.dirname(os.path.abspath(__file__)), "claude_hook.py").replace("\\", "/")
EVENTS = ("SessionStart", "UserPromptSubmit", "PreToolUse", "PostToolUse", "PostToolUseFailure", "PermissionRequest",
          "Notification", "Stop", "StopFailure", "SessionEnd")
CODEX_EVENTS = ("SessionStart", "UserPromptSubmit", "PreToolUse", "PostToolUse", "PermissionRequest", "Stop",
                "Interrupt", "SessionEnd")
CODEX_TIMEOUT_S = 3
POLL_S = 0.5
KEEPALIVE_S = 10
MIN_GAP_S = 0.5
IDLE_KEEP_S = 30 * 60
STALE_S = 24 * 3600
DEAD_GRACE_S = 60
TITLE_RETRY_S = 30
ROWS = 6
PROJECT_BYTES = 27
DETAIL_BYTES = 39
ORDER = {"b": 0, "c": 1, "i": 2}
WORDS = {"b": "seni bekliyor", "c": "calisiyor", "i": "bitti"}
AGENTS = {"claude": "Claude", "codex": "Codex"}

OUT = queue.Queue(maxsize=64)


def alive(pid):
    if not pid or os.name != "nt":
        return True
    k = ctypes.windll.kernel32
    h = k.OpenProcess(0x1000, False, int(pid))
    if not h:
        return ctypes.GetLastError() == 5
    code = ctypes.c_ulong()
    ok = k.GetExitCodeProcess(h, ctypes.byref(code))
    k.CloseHandle(h)
    return not ok or code.value == 259


def clip(text, limit):
    text = "".join(c for c in unicodedata.normalize("NFC", text or "") if c.isprintable() and c not in "\t")
    if len(text.encode("utf-8")) <= limit:
        return text
    while text and len(text.encode("utf-8")) > limit - 3:
        text = text[:-1]
    return text.rstrip() + "…"


def project(cwd):
    parts = [p for p in (cwd or "").replace("\\", "/").split("/") if p]
    if len(parts) >= 3 and parts[-3] == ".claude" and parts[-2] == "worktrees":
        return parts[-1]
    return parts[-1] if parts else "?"


def codex_title(sid):
    dbs = sorted(glob.glob(os.path.join(CODEX_HOME, "state_*.sqlite")))
    for db in reversed(dbs):
        try:
            con = sqlite3.connect("file:%s?mode=ro" % urllib.request.pathname2url(db), uri=True, timeout=1)
            try:
                row = con.execute("select title from threads where id = ?", (sid,)).fetchone()
            finally:
                con.close()
        except sqlite3.Error:
            continue
        if row and row[0] and row[0].strip():
            return row[0].strip()
    name = ""
    try:
        with open(os.path.join(CODEX_HOME, "session_index.jsonl"), encoding="utf-8") as f:
            for line in f:
                try:
                    v = json.loads(line)
                except ValueError:
                    continue
                if v.get("id") == sid and (v.get("thread_name") or "").strip():
                    name = v["thread_name"].strip()
    except OSError:
        pass
    return name


class Watcher:
    def __init__(self, out=OUT, echo=False):
        self.out = out
        self.echo = echo
        self.cache = {}
        self.titles = {}
        self.seen = {}
        self.sig = None
        self.sent_at = 0.0
        self.ver = 0

    def read(self):
        found = {}
        try:
            names = os.listdir(DIR)
        except OSError:
            return found
        for name in names:
            if not name.endswith(".json"):
                continue
            path = os.path.join(DIR, name)
            try:
                mtime = os.path.getmtime(path)
            except OSError:
                continue
            hit = self.cache.get(path)
            if hit and hit[0] == mtime:
                found[path] = hit[1]
                continue
            try:
                with open(path, encoding="utf-8") as f:
                    st = json.load(f)
            except (OSError, ValueError):
                if hit:
                    found[path] = hit[1]
                continue
            self.cache[path] = (mtime, st)
            found[path] = st
        for path in list(self.cache):
            if path not in found:
                del self.cache[path]
        return found

    def name(self, st, now):
        if st.get("agent") != "codex":
            return project(st.get("cwd"))
        sid = st.get("sid", "")
        hit = self.titles.get(sid)
        if not hit or (not hit[1] and now - hit[0] > TITLE_RETRY_S):
            hit = (now, codex_title(sid))
            self.titles[sid] = hit
        return hit[1] or project(st.get("cwd"))

    def rows(self, now):
        out = []
        for path, st in self.read().items():
            status = st.get("st")
            if status not in ORDER:
                continue
            t = st.get("t", 0)
            if now - t > STALE_S:
                try:
                    os.remove(path)
                except OSError:
                    pass
                continue
            since = st.get("since", t)
            if status == "i" and now - since > IDLE_KEEP_S:
                continue
            if now - t > DEAD_GRACE_S and not alive(st.get("pid")):
                continue
            turn = st.get("turn", since)
            span = now - (turn if status == "c" else since)
            work = st.get("work", 0) if status == "i" else span
            agent = "codex" if st.get("agent") == "codex" else "claude"
            out.append({"key": "%08x" % (zlib.crc32(st.get("sid", path).encode()) & 0xFFFFFFFF), "st": status,
                        "span": max(0, int(span)), "work": max(0, int(work)), "agent": agent,
                        "project": clip(self.name(st, now), PROJECT_BYTES),
                        "detail": clip(st.get("detail", ""), DETAIL_BYTES), "since": since})
        out.sort(key=lambda r: (ORDER[r["st"]], r["since"] if r["st"] != "i" else -r["since"]))
        return out

    def note(self, rows):
        now_keys = {}
        for r in rows:
            now_keys[r["key"]] = r["st"]
            if self.seen.get(r["key"]) != r["st"]:
                print("%s: %s -> %s%s" % (r["agent"], r["project"], WORDS[r["st"]],
                                          (" (%s)" % r["detail"]) if r["detail"] else ""))
        self.seen = now_keys

    def lines(self, rows):
        count = {k: sum(1 for r in rows if r["st"] == k) for k in ORDER}
        shown = rows[:ROWS]
        self.ver += 1
        out = ["@ajanlar %x %d %d %d %d" % (self.ver, len(shown), count["c"], count["b"], count["i"])]
        for i, r in enumerate(shown):
            out.append("@ajan\t%d\t%s\t%s\t%d\t%d\t%s\t%s\t%s" % (i, r["key"], r["st"], r["span"], r["work"],
                                                               r["agent"], r["project"], r["detail"]))
        return out

    def emit(self, lines):
        for line in lines:
            if self.echo:
                print(line.replace("\t", " | "))
            try:
                self.out.put_nowait(line)
            except queue.Full:
                pass

    def kick(self):
        self.sig = None

    def step(self, now):
        rows = self.rows(now)
        sig = tuple((r["key"], r["st"], r["agent"], r["project"], r["detail"], r["work"] if r["st"] == "i" else 0)
                    for r in rows)
        changed = sig != self.sig
        if not changed and now - self.sent_at < KEEPALIVE_S:
            return
        if now - self.sent_at < MIN_GAP_S:
            return
        if changed:
            self.note(rows)
        self.sig = sig
        self.sent_at = now
        self.emit(self.lines(rows))

    def run(self):
        while True:
            try:
                self.step(time.time())
            except Exception as ex:
                print("ajan oturumlari: %s: %s" % (type(ex).__name__, ex))
            time.sleep(POLL_S)


WATCHER = None


def start():
    global WATCHER
    WATCHER = Watcher()
    threading.Thread(target=WATCHER.run, daemon=True).start()
    return WATCHER


def kick():
    if WATCHER:
        WATCHER.kick()


def pythonw():
    base = os.path.dirname(sys.executable)
    cand = os.path.join(base, "pythonw.exe")
    return (cand if os.path.exists(cand) else sys.executable).replace("\\", "/")


def ours(entry):
    return any("claude_hook.py" in " ".join([h.get("command", "")] + list(h.get("args") or []))
               for h in entry.get("hooks", []))


def load_json(path):
    if not os.path.exists(path):
        return {}
    with open(path, encoding="utf-8") as f:
        text = f.read()
    return json.loads(text) if text.strip() else {}


def save_json(path, cfg):
    if os.path.exists(path):
        backup = "%s.rumy-yedek-%s" % (path, time.strftime("%Y%m%d-%H%M%S"))
        shutil.copy2(path, backup)
        print("yedek: %s" % backup)
    tmp = path + ".rumy.tmp"
    with open(tmp, "w", encoding="utf-8", newline="\n") as f:
        json.dump(cfg, f, ensure_ascii=False, indent=2)
        f.write("\n")
    os.replace(tmp, path)


def install_claude():
    cfg = load_json(SETTINGS)
    hooks = cfg.setdefault("hooks", {})
    entry = {"hooks": [{"type": "command", "command": pythonw(), "args": ["-S", HOOK], "async": True}]}
    for ev in EVENTS:
        hooks[ev] = [h for h in hooks.get(ev, []) if not ours(h)] + [json.loads(json.dumps(entry))]
    save_json(SETTINGS, cfg)
    print("Claude Code hook'lari eklendi (%d olay): %s" % (len(EVENTS), SETTINGS))


def uninstall_claude():
    cfg = load_json(SETTINGS)
    hooks = cfg.get("hooks", {})
    removed = 0
    for ev in list(hooks):
        keep = [h for h in hooks[ev] if not ours(h)]
        removed += len(hooks[ev]) - len(keep)
        if keep:
            hooks[ev] = keep
        else:
            del hooks[ev]
    if not hooks:
        cfg.pop("hooks", None)
    save_json(SETTINGS, cfg)
    print("%d Claude Code hook'u kaldirildi: %s" % (removed, SETTINGS))


def quoted(path):
    return '"%s"' % path if " " in path else path


def placeholder(group):
    return isinstance(group, dict) and set(group) <= {"hooks"} and group.get("hooks") == []


def codex_group():
    command = "%s -S %s codex" % (quoted(pythonw()), quoted(HOOK))
    return {"hooks": [{"type": "command", "command": command, "async": True, "timeout": CODEX_TIMEOUT_S}]}


def install_codex():
    if not os.path.isdir(CODEX_HOME):
        print("Codex bulunamadi (%s yok), Codex hook'lari eklenmedi" % CODEX_HOME)
        return
    cfg = load_json(CODEX_HOOKS)
    hooks = cfg.setdefault("hooks", {})
    group = codex_group()
    for ev in CODEX_EVENTS:
        groups = hooks.setdefault(ev, [])
        mine = next((i for i, g in enumerate(groups) if ours(g)), None)
        if mine is None:
            mine = next((i for i, g in enumerate(groups) if placeholder(g)), None)
        if mine is None:
            groups.append(json.loads(json.dumps(group)))
        else:
            groups[mine] = json.loads(json.dumps(group))
    save_json(CODEX_HOOKS, cfg)
    print("Codex hook'lari eklendi (%d olay): %s" % (len(CODEX_EVENTS), CODEX_HOOKS))
    print("Codex yeni hook'lari bir kez onaylamani ister: Codex'te /hooks (uygulamada hook uyarisi) -> guven")


def uninstall_codex():
    if not os.path.exists(CODEX_HOOKS):
        return
    cfg = load_json(CODEX_HOOKS)
    hooks = cfg.get("hooks", {})
    removed = 0
    for ev in list(hooks):
        groups = hooks[ev]
        last = max((i for i, g in enumerate(groups) if not ours(g) and not placeholder(g)), default=-1)
        for i in range(last + 1):
            if ours(groups[i]):
                groups[i] = {"hooks": []}
                removed += 1
        removed += sum(1 for g in groups[last + 1:] if ours(g))
        del groups[last + 1:]
        if not groups:
            del hooks[ev]
    save_json(CODEX_HOOKS, cfg)
    print("%d Codex hook'u kaldirildi: %s" % (removed, CODEX_HOOKS))


def status():
    w = Watcher(out=queue.Queue())
    rows = w.rows(time.time())
    if not rows:
        print("acik Claude ya da Codex oturumu yok (%s)" % DIR)
    for r in rows:
        print("%-7s %-24s %-13s %6d sn  %s" % (AGENTS[r["agent"]], r["project"], WORDS[r["st"]], r["span"], r["detail"]))


if __name__ == "__main__":
    sys.stdout.reconfigure(errors="replace")
    arg = sys.argv[1] if len(sys.argv) > 1 else ""
    which = sys.argv[2] if len(sys.argv) > 2 else ""
    if arg == "--kur":
        if which in ("", "claude"):
            install_claude()
        if which in ("", "codex"):
            install_codex()
    elif arg == "--kaldir":
        if which in ("", "claude"):
            uninstall_claude()
        if which in ("", "codex"):
            uninstall_codex()
    elif arg == "--durum":
        status()
    elif arg == "--deneme":
        Watcher(out=queue.Queue(), echo=True).run()
    else:
        print(__doc__)
