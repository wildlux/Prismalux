#!/usr/bin/env python3
"""
cloud_backup_mcp — Backup RAG/KNOWLEDGE su cloud con rclone
============================================================
Sync bidirezionale di RAG/ e KNOWLEDGE_USER/ su Google Drive,
Dropbox, S3, o qualsiasi provider rclone. Config con rclone.
dry_run=true di default per sicurezza.

Tools:
  check_rclone       — verifica installazione rclone
  list_providers     — elenca remote rclone configurati
  backup_rag         — carica RAG/ su cloud
  backup_knowledge   — carica KNOWLEDGE_USER/ su cloud
  restore_rag        — scarica RAG/ dal cloud (dry_run=true default)
  list_cloud_files   — elenca file nel cloud
  backup_all         — backup completo RAG + KNOWLEDGE + db
"""
import sys, json, os, re, asyncio, subprocess
from pathlib import Path
from datetime import datetime
from typing import Any

ROOT        = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
RAG_DIR     = ROOT / "RAG"
KNW_DIR     = ROOT / "KNOWLEDGE_USER"
DB_DIR      = Path.home() / ".prismalux"
BACKUP_LOG  = DB_DIR / "backup_history.json"

def _run(cmd: list[str], timeout: int = 120) -> tuple[int, str]:
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        return r.returncode, (r.stdout + r.stderr).strip()
    except (FileNotFoundError, subprocess.TimeoutExpired) as e:
        return -1, str(e)

def _rclone_ok() -> bool:
    c, _ = _run(["rclone", "version"])
    return c == 0

def _rclone_sync(src: str, dst: str, dry_run: bool = True, timeout: int = 120) -> tuple[int, str]:
    cmd = ["rclone", "sync", src, dst, "--progress", "--stats-one-line"]
    if dry_run: cmd.append("--dry-run")
    return _run(cmd, timeout=timeout)

def _log_backup(action: str, src: str, dst: str, ok: bool) -> None:
    history = []
    if BACKUP_LOG.exists():
        try: history = json.loads(BACKUP_LOG.read_text())
        except: pass
    history.append({
        "ts": datetime.now().isoformat(), "action": action,
        "src": src, "dst": dst, "ok": ok
    })
    history = history[-200:]
    DB_DIR.mkdir(parents=True, exist_ok=True)
    BACKUP_LOG.write_text(json.dumps(history, indent=2))

# ─── handlers ────────────────────────────────────────────────────────────────

def handle_check_rclone(args: dict) -> str:
    if not _rclone_ok():
        return ("❌ rclone non installato.\n"
                "  curl https://rclone.org/install.sh | sudo bash\n"
                "  # oppure:\n"
                "  sudo apt install rclone\n\n"
                "Configurazione provider:\n"
                "  rclone config  # wizard interattivo")
    c, v = _run(["rclone", "version"])
    lines = [f"✅ {v.splitlines()[0]}\n"]
    c2, remotes = _run(["rclone", "listremotes"])
    if c2 == 0 and remotes.strip():
        lines.append(f"Remote configurati:\n  {remotes.replace(chr(10), chr(10)+'  ')}")
    else:
        lines.append("Nessun remote configurato.\n"
                     "  Configura con: rclone config\n"
                     "  Guide: https://rclone.org/docs/")
    return "\n".join(lines)

def handle_list_providers(args: dict) -> str:
    if not _rclone_ok():
        return "rclone non installato. Vedi check_rclone()."
    c, out = _run(["rclone", "listremotes"])
    if c != 0 or not out.strip():
        return ("Nessun remote rclone configurato.\n"
                "  rclone config  # wizard interattivo per aggiungere Google Drive, Dropbox, S3...")
    lines = [f"☁️  Remote rclone disponibili\n"]
    for remote in out.splitlines():
        remote = remote.strip().rstrip(":")
        if not remote: continue
        # Prova a ottenere info
        c2, about = _run(["rclone", "about", f"{remote}:", "--json"], timeout=10)
        if c2 == 0:
            try:
                data  = json.loads(about)
                total = data.get("total", 0)
                used  = data.get("used", 0)
                free  = data.get("free", 0)
                lines.append(f"  ✅ {remote}")
                if total: lines.append(f"     Spazio: {used/1e9:.1f}GB usati / {total/1e9:.0f}GB totali ({free/1e9:.1f}GB liberi)")
            except: lines.append(f"  ✅ {remote}")
        else:
            lines.append(f"  ⚪ {remote}  (non raggiungibile)")
    return "\n".join(lines)

def handle_backup_rag(args: dict) -> str:
    remote   = args.get("remote","").strip()
    dry_run  = bool(args.get("dry_run", True))
    timeout  = int(args.get("timeout_s", 180))
    if not remote: return "Specifica 'remote' (es. 'gdrive', 'dropbox'). Usa list_providers()."
    if not _rclone_ok(): return "rclone non installato."
    if not RAG_DIR.exists():
        return f"RAG/ non trovata: {RAG_DIR}"
    dst = f"{remote}:Prismalux/RAG"
    c, out = _rclone_sync(str(RAG_DIR), dst, dry_run=dry_run, timeout=timeout)
    ok  = c == 0
    _log_backup("backup_rag", str(RAG_DIR), dst, ok)
    pfx = "[DRY RUN] " if dry_run else ""
    icon = "✅" if ok else "❌"
    # Conta file
    file_count = len(list(RAG_DIR.rglob("*")))
    return (f"{pfx}{icon} Backup RAG/ → {dst}\n"
            f"  File locali: {file_count}\n\n"
            f"{out[:800]}")

def handle_backup_knowledge(args: dict) -> str:
    remote  = args.get("remote","").strip()
    dry_run = bool(args.get("dry_run", True))
    timeout = int(args.get("timeout_s", 60))
    if not remote: return "Specifica 'remote'."
    if not _rclone_ok(): return "rclone non installato."
    if not KNW_DIR.exists():
        return f"KNOWLEDGE_USER/ non trovata: {KNW_DIR}"
    dst = f"{remote}:Prismalux/KNOWLEDGE_USER"
    c, out = _rclone_sync(str(KNW_DIR), dst, dry_run=dry_run, timeout=timeout)
    ok  = c == 0
    _log_backup("backup_knowledge", str(KNW_DIR), dst, ok)
    pfx  = "[DRY RUN] " if dry_run else ""
    icon = "✅" if ok else "❌"
    return f"{pfx}{icon} Backup KNOWLEDGE_USER/ → {dst}\n\n{out[:600]}"

def handle_restore_rag(args: dict) -> str:
    remote  = args.get("remote","").strip()
    dry_run = bool(args.get("dry_run", True))
    timeout = int(args.get("timeout_s", 180))
    if not remote: return "Specifica 'remote'."
    if not _rclone_ok(): return "rclone non installato."
    src = f"{remote}:Prismalux/RAG"
    RAG_DIR.mkdir(parents=True, exist_ok=True)
    c, out = _rclone_sync(src, str(RAG_DIR), dry_run=dry_run, timeout=timeout)
    ok  = c == 0
    _log_backup("restore_rag", src, str(RAG_DIR), ok)
    pfx  = "[DRY RUN] " if dry_run else ""
    icon = "✅" if ok else "❌"
    return (f"{pfx}{icon} Restore RAG ← {src}\n"
            f"  ⚠️ I file locali NON in cloud verranno eliminati (sync unidirezionale)\n\n"
            f"{out[:600]}")

def handle_list_cloud_files(args: dict) -> str:
    remote  = args.get("remote","").strip()
    path_arg= args.get("path","Prismalux").strip()
    if not remote: return "Specifica 'remote'."
    if not _rclone_ok(): return "rclone non installato."
    c, out = _run(["rclone", "ls", f"{remote}:{path_arg}"], timeout=30)
    if c != 0:
        return (f"❌ Impossibile listare {remote}:{path_arg}\n{out[:200]}\n\n"
                f"Verifica che il remote esista con list_providers()")
    if not out.strip():
        return f"Nessun file in {remote}:{path_arg}"
    lines = [f"☁️  {remote}:{path_arg}\n"]
    files = sorted(out.splitlines(), key=lambda x: x.strip())
    for f in files[:50]:
        lines.append(f"  {f}")
    if len(files) > 50:
        lines.append(f"  … e altri {len(files)-50} file")
    return "\n".join(lines)

def handle_backup_all(args: dict) -> str:
    remote  = args.get("remote","").strip()
    dry_run = bool(args.get("dry_run", True))
    if not remote: return "Specifica 'remote'."
    results = []
    # RAG
    r1 = handle_backup_rag({"remote": remote, "dry_run": dry_run, "timeout_s": 180})
    results.append(f"=== RAG/ ===\n{r1[:200]}")
    # KNOWLEDGE
    r2 = handle_backup_knowledge({"remote": remote, "dry_run": dry_run, "timeout_s": 60})
    results.append(f"\n=== KNOWLEDGE_USER/ ===\n{r2[:200]}")
    # DB
    if DB_DIR.exists():
        dst = f"{remote}:Prismalux/db"
        c, out = _rclone_sync(str(DB_DIR), dst, dry_run=dry_run, timeout=30)
        results.append(f"\n=== ~/.prismalux/ (DB) ===\n{'✅' if c==0 else '❌'} {out[:100]}")
    pfx = "[DRY RUN] " if dry_run else ""
    return f"{pfx}Backup completo → {remote}:Prismalux\n\n" + "\n".join(results)

HANDLERS = {
    "check_rclone":       handle_check_rclone,
    "list_providers":     handle_list_providers,
    "backup_rag":         handle_backup_rag,
    "backup_knowledge":   handle_backup_knowledge,
    "restore_rag":        handle_restore_rag,
    "list_cloud_files":   handle_list_cloud_files,
    "backup_all":         handle_backup_all,
}
TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"check_rclone","description":"Verifica installazione rclone e lista i remote configurati.","inputSchema":{"type":"object","properties":{}}},
    {"name":"list_providers","description":"Elenca i remote rclone disponibili con spazio usato/libero.","inputSchema":{"type":"object","properties":{}}},
    {"name":"backup_rag","description":"Carica RAG/ su cloud con rclone sync (dry_run=true di default).","inputSchema":{"type":"object","properties":{"remote":{"type":"string"},"dry_run":{"type":"boolean","default":True},"timeout_s":{"type":"integer","default":180}},"required":["remote"]}},
    {"name":"backup_knowledge","description":"Carica KNOWLEDGE_USER/ su cloud (dry_run=true di default).","inputSchema":{"type":"object","properties":{"remote":{"type":"string"},"dry_run":{"type":"boolean","default":True},"timeout_s":{"type":"integer","default":60}},"required":["remote"]}},
    {"name":"restore_rag","description":"Scarica RAG/ dal cloud (ATTENZIONE: sovrascrive file locali, dry_run=true di default).","inputSchema":{"type":"object","properties":{"remote":{"type":"string"},"dry_run":{"type":"boolean","default":True},"timeout_s":{"type":"integer","default":180}},"required":["remote"]}},
    {"name":"list_cloud_files","description":"Elenca i file in un path cloud (default: Prismalux/).","inputSchema":{"type":"object","properties":{"remote":{"type":"string"},"path":{"type":"string","default":"Prismalux"}},"required":["remote"]}},
    {"name":"backup_all","description":"Backup completo: RAG/ + KNOWLEDGE_USER/ + DB SQLite (dry_run=true di default).","inputSchema":{"type":"object","properties":{"remote":{"type":"string"},"dry_run":{"type":"boolean","default":True}},"required":["remote"]}},
]

def _ok(rid, r): return json.dumps({"jsonrpc":"2.0","id":rid,"result":r})
def _err(rid, c, m): return json.dumps({"jsonrpc":"2.0","id":rid,"error":{"code":c,"message":m}})
async def _main():
    loop = asyncio.get_event_loop()
    rl   = lambda: loop.run_in_executor(None, sys.stdin.readline)
    while True:
        line = (await rl()).strip()
        if not line: continue
        try: req = json.loads(line)
        except: sys.stdout.write(_err(None,-32700,"parse")+"\n"); sys.stdout.flush(); continue
        rid, method, params = req.get("id"), req.get("method",""), req.get("params",{})
        if method == "initialize":
            resp = _ok(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"cloud-backup","version":"1.0.0"},"capabilities":{"tools":{}}})
        elif method == "tools/list": resp = _ok(rid,{"tools":TOOL_DEFS})
        elif method == "tools/call":
            name, targs = params.get("name",""), params.get("arguments",{})
            h = HANDLERS.get(name)
            if not h: resp = _err(rid,-32601,f"unknown: {name}")
            else:
                try:
                    text = await asyncio.to_thread(h, targs)
                    resp = _ok(rid,{"content":[{"type":"text","text":text}],"isError":False})
                except Exception as e:
                    resp = _ok(rid,{"content":[{"type":"text","text":str(e)}],"isError":True})
        elif method == "notifications/initialized": continue
        else: resp = _err(rid,-32601,method)
        sys.stdout.write(resp+"\n"); sys.stdout.flush()
def main(): asyncio.run(_main())
if __name__ == "__main__": main()
