#!/usr/bin/env python3
"""
AIMemory MCP — Prismalux
Espone le API di AIMemory (memoria versionata Git) via JSON-RPC 2.0 stdio.

Tool disponibili:
  - learn(key, value)           — salva preferenza in profile/preferences.yaml
  - feedback(query, response, rating, reason) — registra 👍/👎 con motivo
  - get_context(query)          — legge contesto rilevante (preferenze + feedback recenti)
  - git_log(n)                  — ultimi n commit del repo memoria
  - save_interaction(query, response) — diario giornaliero conversazione

Backend: ~/.ai-memory/ (formato identico a gui/ai_memory.h)
Uso: comunicazione stdio JSON-RPC 2.0 (standard MCP).
"""

import sys
import json
import os
import subprocess
from pathlib import Path
from datetime import datetime
from typing import Any

# ─── Paths ────────────────────────────────────────────────────────────────────

MEM_ROOT = Path.home() / ".ai-memory"

def _ensure_dir(p: Path) -> None:
    p.mkdir(parents=True, exist_ok=True)

def _git(*args, cwd=MEM_ROOT) -> tuple[int, str]:
    try:
        r = subprocess.run(
            ["git", *args], cwd=str(cwd),
            capture_output=True, text=True, timeout=10
        )
        return r.returncode, r.stdout.strip()
    except Exception as e:
        return 1, str(e)

def _init_repo() -> None:
    if not (MEM_ROOT / ".git").exists():
        _ensure_dir(MEM_ROOT)
        _git("init", "-b", "main", cwd=MEM_ROOT)
        _git("config", "user.email", "ai-memory@prismalux.local", cwd=MEM_ROOT)
        _git("config", "user.name", "AIMemory", cwd=MEM_ROOT)

def _commit(message: str) -> None:
    _git("add", "-A")
    _git("commit", "-m", message, "--allow-empty-message")

def _read_file(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except FileNotFoundError:
        return ""

def _write_file(path: Path, content: str) -> None:
    _ensure_dir(path.parent)
    path.write_text(content, encoding="utf-8")

def _append_file(path: Path, content: str) -> None:
    _ensure_dir(path.parent)
    with path.open("a", encoding="utf-8") as f:
        f.write(content)

def _set_yaml_key(path: Path, key: str, value: str) -> None:
    content = _read_file(path)
    lines = content.splitlines()
    new_line = f"{key}: {json.dumps(value, ensure_ascii=False)}"
    for i, line in enumerate(lines):
        if line.startswith(key + ":"):
            lines[i] = new_line
            _write_file(path, "\n".join(lines) + "\n")
            return
    _append_file(path, new_line + "\n")

# ─── Tool implementations ─────────────────────────────────────────────────────

def _prefs_path() -> Path:
    return MEM_ROOT / "profile" / "preferences.yaml"

def _feedback_path() -> Path:
    d = datetime.now()
    return MEM_ROOT / "interactions" / d.strftime("%Y-%m") / "feedback.yaml"

def _interaction_path() -> Path:
    d = datetime.now()
    return MEM_ROOT / "interactions" / d.strftime("%Y-%m") / (d.strftime("%d") + ".md")

def tool_learn(key: str, value: str) -> str:
    _init_repo()
    _set_yaml_key(_prefs_path(), key, value)
    _commit(f"learn: {key}")
    return f"Preferenza '{key}' aggiornata."

def tool_feedback(query: str, response: str, rating: int, reason: str = "") -> str:
    _init_repo()
    ts = datetime.now().isoformat(timespec="seconds")
    thumbs = "\U0001f44d" if rating >= 0 else "\U0001f44e"
    entry = (
        f"- ts: {ts}\n"
        f"  query: {json.dumps(query[:200], ensure_ascii=False)}\n"
        f"  response_snippet: {json.dumps(response[:120], ensure_ascii=False)}\n"
        f"  rating: {thumbs}\n"
        + (f"  reason: {json.dumps(reason, ensure_ascii=False)}\n" if reason else "")
        + "\n"
    )
    _append_file(_feedback_path(), entry)
    _commit(f"feedback: {thumbs} su query '{query[:40]}'")
    return f"Feedback {thumbs} registrato."

def tool_get_context(query: str = "", max_feedback: int = 5) -> str:
    prefs = _read_file(_prefs_path())
    fb_path = _feedback_path()
    fb_lines = _read_file(fb_path).splitlines() if fb_path.exists() else []
    recent_fb = "\n".join(fb_lines[-(max_feedback * 5):]) if fb_lines else "(nessuno)"
    return (
        f"=== Preferenze ===\n{prefs or '(vuoto)'}\n\n"
        f"=== Feedback recenti ===\n{recent_fb}"
    )

def tool_save_interaction(query: str, response: str) -> str:
    _init_repo()
    ts = datetime.now().isoformat(timespec="seconds")
    entry = f"\n## [{ts}]\n**Q:** {query}\n\n**A:** {response[:500]}\n"
    _append_file(_interaction_path(), entry)
    _commit(f"interaction: {query[:40]}")
    return "Interazione salvata."

def tool_git_log(n: int = 20) -> str:
    _init_repo()
    _, out = _git("log", f"--oneline", f"-{n}")
    return out or "(repository vuoto)"

# ─── MCP dispatch ─────────────────────────────────────────────────────────────

TOOLS = {
    "learn": {
        "description": "Salva una preferenza appresa (chiave/valore) in profile/preferences.yaml con commit Git.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "key":   {"type": "string", "description": "Nome della preferenza (es. 'lingua', 'stile_risposta')"},
                "value": {"type": "string", "description": "Valore della preferenza"}
            },
            "required": ["key", "value"]
        }
    },
    "feedback": {
        "description": "Registra feedback 👍/👎 su una risposta AI con motivo opzionale.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "query":    {"type": "string", "description": "La domanda dell'utente"},
                "response": {"type": "string", "description": "La risposta dell'AI"},
                "rating":   {"type": "integer", "description": "1 = 👍, -1 = 👎"},
                "reason":   {"type": "string", "description": "Motivo opzionale del feedback"}
            },
            "required": ["query", "response", "rating"]
        }
    },
    "get_context": {
        "description": "Restituisce il contesto rilevante: preferenze salvate + feedback recenti.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "query":        {"type": "string", "description": "Query corrente (per eventuale filtraggio futuro)"},
                "max_feedback": {"type": "integer", "description": "Numero massimo di feedback recenti (default 5)"}
            },
            "required": []
        }
    },
    "save_interaction": {
        "description": "Salva una coppia query/risposta nel diario giornaliero con commit Git.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "query":    {"type": "string"},
                "response": {"type": "string"}
            },
            "required": ["query", "response"]
        }
    },
    "git_log": {
        "description": "Mostra gli ultimi N commit del repo memoria ~/.ai-memory.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "n": {"type": "integer", "description": "Numero di commit (default 20)"}
            },
            "required": []
        }
    }
}

def call_tool(name: str, args: dict) -> Any:
    if name == "learn":
        return tool_learn(args["key"], args["value"])
    if name == "feedback":
        return tool_feedback(
            args["query"], args["response"], args["rating"],
            args.get("reason", "")
        )
    if name == "get_context":
        return tool_get_context(args.get("query", ""), args.get("max_feedback", 5))
    if name == "save_interaction":
        return tool_save_interaction(args["query"], args["response"])
    if name == "git_log":
        return tool_git_log(args.get("n", 20))
    raise ValueError(f"Tool sconosciuto: {name}")

def respond(req_id: Any, result: Any) -> None:
    print(json.dumps({"jsonrpc": "2.0", "id": req_id, "result": result}), flush=True)

def respond_error(req_id: Any, code: int, message: str) -> None:
    print(json.dumps({
        "jsonrpc": "2.0", "id": req_id,
        "error": {"code": code, "message": message}
    }), flush=True)

def main() -> None:
    for raw in sys.stdin:
        raw = raw.strip()
        if not raw:
            continue
        try:
            req = json.loads(raw)
        except json.JSONDecodeError as e:
            respond_error(None, -32700, f"Parse error: {e}")
            continue

        req_id = req.get("id")
        method = req.get("method", "")

        if method == "initialize":
            respond(req_id, {
                "protocolVersion": "2024-11-05",
                "capabilities": {"tools": {}},
                "serverInfo": {"name": "ai_memory_mcp", "version": "1.0.0"}
            })
        elif method == "tools/list":
            respond(req_id, {"tools": [
                {"name": k, "description": v["description"], "inputSchema": v["inputSchema"]}
                for k, v in TOOLS.items()
            ]})
        elif method == "tools/call":
            params = req.get("params", {})
            tool_name = params.get("name", "")
            tool_args = params.get("arguments", {})
            try:
                result = call_tool(tool_name, tool_args)
                respond(req_id, {"content": [{"type": "text", "text": str(result)}]})
            except Exception as e:
                respond_error(req_id, -32603, str(e))
        elif method == "notifications/initialized":
            pass  # no response needed
        else:
            respond_error(req_id, -32601, f"Method not found: {method}")

if __name__ == "__main__":
    main()
