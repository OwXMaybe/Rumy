"""Claude Code ve Codex hook'u: bir oturumun durumunu (calisiyor / seni bekliyor / bitti) Rumy icin dosyaya yazar.

Claude Code ve Codex her olayda (istek gonderildi, arac kullanilacak, izin istendi, is bitti...) bu betigi arka planda
(async) calistirir ve olayi JSON olarak stdin'den verir; ikisinin olay bicimi ayni. Codex'in kaydi betige "codex"
argumanini verir. Betik ~/.claude/rumy/oturumlar/<oturum>.json dosyasini gunceller; kopru (tools/claude_agents.py) bu
klasoru izleyip Rumy'ye iletir. Betik hicbir zaman hata vermez, cikti uretmez ve ajani bekletmez.
Kurulum ve kaldirma: python tools/claude_agents.py --kur | --kaldir

Durumlar: c = calisiyor, b = seni bekliyor, i = bitti/bosta. Alanlar: agent (claude | codex), t (son olay), turn
(istegin baslangici), since (bu durumun baslangici), work (son isin suresi, sn), detail (ne yaptigi), cwd, pid (ajanin
sureci: ata surecler icinde ilk claude.exe ya da codex.exe, bulunamazsa ust surec).
"""
import json
import os
import sys
import time

DIR = os.path.join(os.path.expanduser("~"), ".claude", "rumy", "oturumlar")
LOCK_STALE_S = 2
OWNERS = ("claude.exe", "codex.exe")
ASK = {"AskUserQuestion": "Sana bir soru sordu", "ExitPlanMode": "Plan onayını bekliyor"}
NOTE_WAIT = {"permission_prompt": "İzin istiyor", "elicitation_dialog": "Bilgi istiyor",
             "elicitation_url_dialog": "Bilgi istiyor", "agent_needs_input": "Seni bekliyor"}
ERRORS = {"rate_limit": "kullanım sınırı", "overloaded": "sunucu yoğun", "authentication_failed": "giriş gerekli",
          "billing_error": "ödeme sorunu", "server_error": "sunucu hatası", "max_output_tokens": "çıktı sınırı"}
VERBS = (
    (("Bash", "PowerShell", "BashOutput", "KillShell", "Monitor", "exec_command", "shell", "local_shell"),
     "Komut çalıştırıyor"),
    (("Edit", "Write", "MultiEdit", "NotebookEdit", "apply_patch"), "Dosya düzenliyor"),
    (("Read", "Glob", "Grep", "LS", "NotebookRead", "view_image", "read_file", "list_dir", "grep_files"), "Kod okuyor"),
    (("WebFetch", "WebSearch", "web_search"), "Web'e bakıyor"),
    (("Agent", "Task", "spawn_agent"), "Yardımcı ajan çalışıyor"),
    (("TodoWrite", "TaskCreate", "TaskUpdate", "update_plan"), "Plan yapıyor"),
    (("Skill",), "Beceri kullanıyor"),
)


def verb(tool):
    for names, text in VERBS:
        if tool in names:
            return text
    if "browser" in tool.lower() or "chrome" in tool.lower():
        return "Web'e bakıyor"
    if tool.startswith("mcp__"):
        return "Araç kullanıyor"
    return "Çalışıyor"


def short_tool(tool):
    if tool in ("apply_patch", "Edit", "Write", "MultiEdit"):
        return "dosya düzenleme"
    if tool.startswith("mcp__"):
        tool = tool.split("__")[-1]
    return tool[:18]


def owner_pid():
    ppid = os.getppid()
    if os.name != "nt":
        return ppid
    try:
        import ctypes
        from ctypes import wintypes

        class Entry(ctypes.Structure):
            _fields_ = [("dwSize", wintypes.DWORD), ("cntUsage", wintypes.DWORD), ("pid", wintypes.DWORD),
                        ("heap", ctypes.c_void_p), ("module", wintypes.DWORD), ("threads", wintypes.DWORD),
                        ("parent", wintypes.DWORD), ("prio", ctypes.c_long), ("flags", wintypes.DWORD),
                        ("exe", ctypes.c_wchar * 260)]

        k = ctypes.windll.kernel32
        k.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
        snap = k.CreateToolhelp32Snapshot(2, 0)
        if not snap or snap == wintypes.HANDLE(-1).value:
            return ppid
        procs = {}
        e = Entry()
        e.dwSize = ctypes.sizeof(Entry)
        ok = k.Process32FirstW(snap, ctypes.byref(e))
        while ok:
            procs[e.pid] = (e.parent, e.exe.lower())
            ok = k.Process32NextW(snap, ctypes.byref(e))
        k.CloseHandle(snap)
        pid = os.getpid()
        for _ in range(16):
            parent = procs.get(pid, (0, ""))[0]
            if not parent or parent not in procs:
                break
            if procs[parent][1] in OWNERS:
                return parent
            pid = parent
    except Exception:
        pass
    return ppid


def is_exec(data):
    path = data.get("transcript_path") or ""
    try:
        with open(path, encoding="utf-8") as f:
            meta = json.loads(f.readline() or "{}")
    except (OSError, ValueError):
        return False
    if not isinstance(meta, dict):
        return False
    p = meta.get("payload") or {}
    return p.get("source") == "exec" or p.get("originator") == "codex_exec"


def read_input():
    stream = sys.stdin.buffer if sys.stdin is not None else None
    try:
        raw = stream.read() if stream else os.read(0, 1 << 20)
    except OSError:
        return {}
    try:
        data = json.loads(raw.decode("utf-8", "replace") or "{}")
    except ValueError:
        return {}
    return data if isinstance(data, dict) else {}


def load(path):
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return None


def save(path, st):
    tmp = "%s.%d.tmp" % (path, os.getpid())
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(st, f, ensure_ascii=False)
    for _ in range(5):
        try:
            os.replace(tmp, path)
            return
        except PermissionError:
            time.sleep(0.02)
    os.remove(tmp)


def remove(path):
    try:
        os.remove(path)
    except OSError:
        pass


def lock(path):
    name = path + ".lock"
    for _ in range(50):
        try:
            os.close(os.open(name, os.O_CREAT | os.O_EXCL | os.O_WRONLY))
            return name
        except FileExistsError:
            try:
                if time.time() - os.path.getmtime(name) > LOCK_STALE_S:
                    os.remove(name)
                    continue
            except OSError:
                pass
            time.sleep(0.01)
        except OSError:
            return None
    return None


def apply(st, ev, data, now):
    tool = data.get("tool_name") or ""
    status = st.get("st")

    def to(new, detail):
        if st.get("st") != new:
            st["since"] = now
        st["st"] = new
        st["detail"] = detail

    if ev == "SessionStart":
        if data.get("source") == "compact" and status:
            return True
        st["turn"] = now
        st["work"] = 0
        to("i", "Yeni oturum")
    elif ev == "UserPromptSubmit":
        st["turn"] = now
        to("c", "Düşünüyor")
    elif ev == "PreToolUse":
        if tool in ASK:
            to("b", ASK[tool])
        else:
            if status == "i" or "turn" not in st:
                st["turn"] = now
            to("c", verb(tool))
    elif ev in ("PostToolUse", "PostToolUseFailure"):
        to("c", "Düşünüyor")
    elif ev == "PermissionRequest":
        to("b", "İzin istiyor: %s" % short_tool(tool) if tool else "İzin istiyor")
    elif ev == "Notification":
        kind = data.get("notification_type") or ""
        if kind not in NOTE_WAIT:
            return False
        if status != "b":
            to("b", NOTE_WAIT[kind])
    elif ev in ("Stop", "Interrupt"):
        st["work"] = int(now - st.get("turn", now))
        to("i", "Durduruldu" if ev == "Interrupt" else "")
    elif ev == "StopFailure":
        err = data.get("error") or data.get("error_type") or data.get("reason") or ""
        if not isinstance(err, str):
            err = ""
        to("b", "API hatası" + (": " + ERRORS.get(err, err) if err else ""))
    else:
        return False
    return True


def main():
    now = time.time()
    agent = "codex" if len(sys.argv) > 1 and sys.argv[1] == "codex" else "claude"
    data = read_input()
    ev = data.get("hook_event_name") or ""
    sid = "".join(c for c in str(data.get("session_id") or "") if c.isalnum() or c in "-_")[:80]
    if not ev or not sid or os.environ.get("RUMY_HOOK_KAPALI"):
        return
    os.makedirs(DIR, exist_ok=True)
    path = os.path.join(DIR, sid + ".json")
    if ev == "SessionEnd" or (ev == "Stop" and agent == "codex" and is_exec(data)):
        remove(path)
        return
    held = lock(path)
    try:
        st = load(path) or {}
        if st.get("t", 0) > now or not apply(st, ev, data, now):
            return
        if "pid" not in st or ev in ("SessionStart", "UserPromptSubmit"):
            st["pid"] = owner_pid()
        st["t"] = now
        st["ev"] = ev
        st["sid"] = sid
        st["agent"] = agent
        st["cwd"] = data.get("cwd") or st.get("cwd") or ""
        save(path, st)
    finally:
        if held:
            remove(held)


if __name__ == "__main__":
    try:
        main()
    except Exception:
        pass
