#!/usr/bin/env python3
"""
git_prismalux_mcp — Git intelligence per il progetto Prismalux
===============================================================
Strumenti Git ottimizzati per navigare la storia del progetto
senza riempire il contesto con output raw di git log/diff.

Tools esposti:
  status          — stato working tree (staged/unstaged/untracked)
  log             — ultimi N commit con riassunto (default: 10)
  diff            — diff leggibile di file o staged changes
  blame           — chi ha modificato quale riga in un file
  search_commits  — cerca keyword nei messaggi di commit
  show_commit     — dettaglio di un commit specifico
  branch_info     — branch corrente + diff vs main
  recent_files    — file modificati di recente (heat map)
"""

import sys, json, os, subprocess, asyncio, logging
from pathlib import Path
from typing import Any

logging.basicConfig(level=getattr(logging, os.environ.get("PRISMALUX_LOG_LEVEL","WARNING")))
logger = logging.getLogger(__name__)

ROOT = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))

# ─── Tool definitions ────────────────────────────────────────────────────────

TOOL_DEFS: list[dict[str, Any]] = [
    {
        "name": "status",
        "description": "Stato del working tree: file modificati, staged, untracked. Più leggibile di 'git status -s'.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "short": {
                    "type": "boolean",
                    "description": "Se true, output compatto (default: false)",
                    "default": False,
                },
            },
        },
    },
    {
        "name": "log",
        "description": "Ultimi N commit con hash breve, autore, data e messaggio. Opzionalmente filtrati per file o path.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "n": {
                    "type": "integer",
                    "description": "Numero di commit da mostrare (default: 10, max: 50)",
                    "default": 10,
                },
                "path": {
                    "type": "string",
                    "description": "Filtra commit che toccano questo path (relativo a root)",
                    "default": "",
                },
                "author": {
                    "type": "string",
                    "description": "Filtra per autore (es. 'wildlux')",
                    "default": "",
                },
            },
        },
    },
    {
        "name": "diff",
        "description": "Mostra il diff di un file specifico o dei cambiamenti staged. Limita output a N righe.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "file": {
                    "type": "string",
                    "description": "Path del file da differe (relativo a root, opzionale)",
                    "default": "",
                },
                "staged": {
                    "type": "boolean",
                    "description": "Se true, mostra diff staged (--cached). Default: false",
                    "default": False,
                },
                "commit": {
                    "type": "string",
                    "description": "Hash del commit da comparare vs HEAD (es. 'HEAD~1' o hash). Default: working tree.",
                    "default": "",
                },
                "max_lines": {
                    "type": "integer",
                    "description": "Numero massimo di righe output (default: 100)",
                    "default": 100,
                },
            },
        },
    },
    {
        "name": "blame",
        "description": "Mostra quale commit ha introdotto ogni riga di un file (range opzionale).",
        "inputSchema": {
            "type": "object",
            "properties": {
                "file":       {"type": "string", "description": "Path del file (relativo a root)"},
                "start_line": {"type": "integer", "description": "Prima riga (default: 1)", "default": 1},
                "end_line":   {"type": "integer", "description": "Ultima riga (default: 40)", "default": 40},
            },
            "required": ["file"],
        },
    },
    {
        "name": "search_commits",
        "description": "Cerca una keyword nei messaggi di commit dell'intera storia del progetto.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "keyword": {
                    "type": "string",
                    "description": "Keyword da cercare (case-insensitive)",
                },
                "n": {
                    "type": "integer",
                    "description": "Numero massimo di risultati (default: 15)",
                    "default": 15,
                },
                "grep_code": {
                    "type": "boolean",
                    "description": "Se true, cerca anche nelle modifiche al codice (--pickaxe), non solo nei messaggi",
                    "default": False,
                },
            },
            "required": ["keyword"],
        },
    },
    {
        "name": "show_commit",
        "description": "Mostra il dettaglio completo di un commit: messaggio, stats file, patch (troncata).",
        "inputSchema": {
            "type": "object",
            "properties": {
                "hash": {
                    "type": "string",
                    "description": "Hash del commit (può essere breve, es. 'df3263e', o 'HEAD', 'HEAD~2', ecc.)",
                },
                "show_patch": {
                    "type": "boolean",
                    "description": "Se true, include il diff (patch). Default: false",
                    "default": False,
                },
            },
            "required": ["hash"],
        },
    },
    {
        "name": "branch_info",
        "description": "Branch corrente, commit in avanti rispetto a main, e lista di file cambiati.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "base": {
                    "type": "string",
                    "description": "Branch di base da confrontare (default: 'master')",
                    "default": "master",
                },
            },
        },
    },
    {
        "name": "recent_files",
        "description": "File più modificati di recente (ultimi N commit). Utile per capire dove si sta lavorando.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "n": {
                    "type": "integer",
                    "description": "Quanti commit analizzare (default: 20)",
                    "default": 20,
                },
                "ext": {
                    "type": "string",
                    "description": "Filtra per estensione (es. '.cpp', '.py', vuoto = tutto)",
                    "default": "",
                },
            },
        },
    },
]

# ─── Helpers ─────────────────────────────────────────────────────────────────

def _git(args: list[str], cwd: Path = ROOT) -> tuple[int, str]:
    try:
        r = subprocess.run(
            ["git"] + args, capture_output=True, text=True,
            cwd=str(cwd), timeout=30
        )
        return r.returncode, (r.stdout + r.stderr).strip()
    except subprocess.TimeoutExpired:
        return -1, "Timeout (30s)"
    except FileNotFoundError:
        return -1, "git non trovato"

def _rel(path: str) -> str:
    """Converte path assoluto in relativo a ROOT."""
    try:
        return str(Path(path).relative_to(ROOT))
    except ValueError:
        return path

# ─── Handlers ────────────────────────────────────────────────────────────────

def handle_status(args: dict) -> str:
    short = bool(args.get("short", False))
    if short:
        code, out = _git(["status", "-s", "--short"])
        return out or "Working tree pulito."

    code, out = _git(["status"])
    return out if out else "Working tree pulito."


def handle_log(args: dict) -> str:
    n      = min(int(args.get("n", 10)), 50)
    path   = args.get("path", "").strip()
    author = args.get("author", "").strip()

    fmt = "%h│%an│%ar│%s"
    git_args = ["log", f"-{n}", f"--pretty=format:{fmt}", "--no-merges"]
    if author:
        git_args += [f"--author={author}"]
    if path:
        git_args += ["--", path]

    code, out = _git(git_args)
    if not out:
        return "Nessun commit trovato."

    lines = ["```", f"{'HASH':8}  {'AUTORE':12}  {'QUANDO':15}  MESSAGGIO"]
    for row in out.splitlines():
        parts = row.split("│", 3)
        if len(parts) == 4:
            h, an, ar, s = parts
            lines.append(f"{h:8}  {an[:12]:12}  {ar[:15]:15}  {s[:70]}")
    lines.append("```")
    return "\n".join(lines)


def handle_diff(args: dict) -> str:
    file_arg  = args.get("file", "").strip()
    staged    = bool(args.get("staged", False))
    commit    = args.get("commit", "").strip()
    max_lines = int(args.get("max_lines", 100))

    git_args = ["diff"]
    if staged:
        git_args.append("--cached")
    if commit:
        git_args.append(commit)
    if file_arg:
        git_args += ["--", file_arg]

    git_args = ["diff", "--stat"] + git_args[1:]  # prima stats
    code, stats = _git(git_args + (["--", file_arg] if file_arg else []))

    # poi patch
    patch_args = ["diff"]
    if staged:
        patch_args.append("--cached")
    if commit:
        patch_args.append(commit)
    if file_arg:
        patch_args += ["--", file_arg]
    code2, patch = _git(patch_args)

    patch_lines = patch.splitlines()[:max_lines]
    truncated   = len(patch.splitlines()) > max_lines

    result = []
    if stats.strip():
        result.append("**Stat:**\n```\n" + stats[:500] + "\n```\n")
    if patch_lines:
        result.append("**Patch:**\n```diff\n" + "\n".join(patch_lines) + "\n```")
        if truncated:
            result.append(f"*(troncato a {max_lines} righe — usa max_lines per aumentare)*")
    return "\n".join(result) if result else "Nessun cambiamento."


def handle_blame(args: dict) -> str:
    file_arg   = args.get("file", "").strip()
    start_line = int(args.get("start_line", 1))
    end_line   = int(args.get("end_line", 40))

    if not file_arg:
        return "Specifica il file da analizzare."

    end_line = max(end_line, start_line + 1)
    end_line = min(end_line, start_line + 100)

    code, out = _git(["blame", f"-L{start_line},{end_line}", "--abbrev=8", file_arg])
    if code != 0:
        return f"Errore git blame: {out}"
    return f"```\n{out}\n```" if out else "File vuoto o range non valido."


def handle_search_commits(args: dict) -> str:
    keyword    = args.get("keyword", "").strip()
    n          = min(int(args.get("n", 15)), 50)
    grep_code  = bool(args.get("grep_code", False))

    if not keyword:
        return "Specifica una keyword da cercare."

    if grep_code:
        git_args = ["log", f"-{n}", "--pretty=format:%h │ %ar │ %s",
                    f"-S{keyword}", "--pickaxe-regex"]
    else:
        git_args = ["log", f"-{n}", "--pretty=format:%h │ %ar │ %s",
                    f"--grep={keyword}", "--regexp-ignore-case"]

    code, out = _git(git_args)
    if not out:
        return f"Nessun commit trovato con '{keyword}'."

    label = "nel codice" if grep_code else "nel messaggio"
    lines = out.splitlines()
    return f"Commit con '{keyword}' ({label}) — {len(lines)} risultati:\n```\n" + "\n".join(lines) + "\n```"


def handle_show_commit(args: dict) -> str:
    hash_arg   = args.get("hash", "").strip()
    show_patch = bool(args.get("show_patch", False))

    if not hash_arg:
        return "Specifica l'hash del commit."

    # Header commit
    code, header = _git(["show", "--stat", "--format=full", hash_arg])
    if code != 0:
        return f"Commit non trovato: {hash_arg}\n{header}"

    if show_patch:
        code2, patch = _git(["show", hash_arg])
        lines = patch.splitlines()[:150]
        trunc = len(patch.splitlines()) > 150
        patch_out = "\n```diff\n" + "\n".join(lines) + "\n```"
        if trunc:
            patch_out += "\n*(troncato a 150 righe)*"
        return header[:1000] + patch_out

    return f"```\n{header[:1500]}\n```"


def handle_branch_info(args: dict) -> str:
    base = args.get("base", "master").strip() or "master"

    code, branch = _git(["branch", "--show-current"])
    if code != 0:
        return f"Errore: {branch}"
    branch = branch.strip()

    code2, ahead = _git(["rev-list", "--count", f"{base}..HEAD"])
    code3, behind = _git(["rev-list", "--count", f"HEAD..{base}"])
    code4, changed = _git(["diff", "--name-status", f"{base}...HEAD"])

    lines = [f"Branch corrente: **{branch}**"]
    if code2 == 0:
        lines.append(f"Commit avanti a {base}: {ahead.strip()}")
    if code3 == 0:
        lines.append(f"Commit indietro rispetto a {base}: {behind.strip()}")

    if code4 == 0 and changed:
        lines.append(f"\nFile modificati rispetto a {base}:")
        for row in changed.splitlines()[:30]:
            parts = row.split("\t")
            status = parts[0][0] if parts else "?"
            fname  = parts[1] if len(parts) > 1 else row
            icon   = {"A": "➕", "M": "✏️ ", "D": "❌", "R": "🔄"}.get(status, "·")
            lines.append(f"  {icon} {fname}")
    return "\n".join(lines)


def handle_recent_files(args: dict) -> str:
    n   = min(int(args.get("n", 20)), 100)
    ext = args.get("ext", "").strip()

    code, out = _git(["log", f"-{n}", "--name-only", "--pretty=format:"])
    if code != 0 or not out:
        return "Impossibile recuperare la lista dei file."

    from collections import Counter
    files = [l.strip() for l in out.splitlines() if l.strip()]
    if ext:
        files = [f for f in files if f.endswith(ext)]

    counts = Counter(files)
    top    = counts.most_common(20)
    if not top:
        return f"Nessun file con estensione '{ext}' negli ultimi {n} commit."

    lines = [f"File più modificati (ultimi {n} commit):\n"]
    for fname, cnt in top:
        bar = "█" * min(cnt, 20)
        lines.append(f"  {cnt:3}x {bar} {fname}")
    return "\n".join(lines)


HANDLERS = {
    "status":         handle_status,
    "log":            handle_log,
    "diff":           handle_diff,
    "blame":          handle_blame,
    "search_commits": handle_search_commits,
    "show_commit":    handle_show_commit,
    "branch_info":    handle_branch_info,
    "recent_files":   handle_recent_files,
}

# ─── JSON-RPC 2.0 loop ───────────────────────────────────────────────────────

def _rpc_result(req_id, result):
    return json.dumps({"jsonrpc":"2.0","id":req_id,"result":result})
def _rpc_error(req_id, code, message):
    return json.dumps({"jsonrpc":"2.0","id":req_id,"error":{"code":code,"message":message}})

async def _main_async():
    loop = asyncio.get_event_loop()
    async def readline(): return await loop.run_in_executor(None, sys.stdin.readline)
    while True:
        line = await readline()
        if not line: break
        line = line.strip()
        if not line: continue
        try:
            req = json.loads(line)
        except json.JSONDecodeError as e:
            sys.stdout.write(_rpc_error(None,-32700,f"Parse error: {e}")+"\n"); sys.stdout.flush(); continue
        req_id = req.get("id"); method = req.get("method",""); params = req.get("params",{})
        if method == "initialize":
            resp = _rpc_result(req_id,{"protocolVersion":"2024-11-05","serverInfo":{"name":"git-prismalux","version":"1.0.0"},"capabilities":{"tools":{}}})
        elif method == "tools/list":
            resp = _rpc_result(req_id,{"tools":TOOL_DEFS})
        elif method == "tools/call":
            name = params.get("name",""); targs = params.get("arguments",{})
            handler = HANDLERS.get(name)
            if not handler:
                resp = _rpc_error(req_id,-32601,f"Tool sconosciuto: {name}")
            else:
                try:
                    text = await asyncio.to_thread(handler, targs)
                    resp = _rpc_result(req_id,{"content":[{"type":"text","text":text}],"isError":False})
                except Exception as e:
                    resp = _rpc_result(req_id,{"content":[{"type":"text","text":f"Errore: {e}"}],"isError":True})
        elif method == "notifications/initialized":
            continue
        else:
            resp = _rpc_error(req_id,-32601,f"Metodo sconosciuto: {method}")
        sys.stdout.write(resp+"\n"); sys.stdout.flush()

def main(): asyncio.run(_main_async())
if __name__ == "__main__": main()
