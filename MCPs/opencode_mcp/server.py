#!/usr/bin/env python3
"""
OpenCode MCP Server — Prismalux
Permette a Claude di delegare task di codice a OpenCode (con qualsiasi modello Ollama).
Comunicazione: JSON-RPC 2.0 su stdio (protocollo MCP standard).

Uso con Claude Code:
  Aggiungi in ~/.claude/settings.json sotto "mcpServers":
  {
    "opencode": {
      "type": "stdio",
      "command": "python3",
      "args": ["/path/to/MCPs/opencode_mcp/server.py"],
      "env": {
        "OPENCODE_DEFAULT_MODEL": "ollama/qwen3.5:4b",
        "OPENCODE_DEFAULT_DIR":   "/home/wildlux/Desktop/Prismalux"
      }
    }
  }
"""

import sys
import json
import os
import subprocess
import threading
import time
from pathlib import Path

# ─── Configurazione ───────────────────────────────────────────────────────────
DEFAULT_MODEL = os.environ.get("OPENCODE_DEFAULT_MODEL", "ollama/qwen3.5:4b")
DEFAULT_DIR   = os.environ.get("OPENCODE_DEFAULT_DIR",   str(Path.home()))
OPENCODE_BIN  = os.environ.get("OPENCODE_BIN", "opencode")

# Timeout per opencode run (secondi) — modelli grandi possono essere lenti
RUN_TIMEOUT = int(os.environ.get("OPENCODE_TIMEOUT", "120"))


# ─── Strumenti esposti ────────────────────────────────────────────────────────
TOOLS = [
    {
        "name": "ask_opencode",
        "description": (
            "Invia un task o una domanda di programmazione a OpenCode, "
            "un agente AI specializzato nel codice. OpenCode può leggere file, "
            "scrivere codice, eseguire comandi e navigare nel filesystem.\n\n"
            "Usa questo strumento quando hai bisogno di:\n"
            "- Analizzare o scrivere codice in una directory specifica\n"
            "- Eseguire refactoring su file reali\n"
            "- Chiedere a un agente con accesso al filesystem di completare un task"
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "message": {
                    "type": "string",
                    "description": "Il task o la domanda da inviare a OpenCode. Sii specifico: includi nomi di file, linguaggi e obiettivi chiari."
                },
                "working_dir": {
                    "type": "string",
                    "description": f"Directory di lavoro per OpenCode. Default: {DEFAULT_DIR}"
                },
                "model": {
                    "type": "string",
                    "description": f"Modello AI in formato provider/modello. Default: {DEFAULT_MODEL}. Esempi: ollama/qwen3.5:4b, ollama/deepseek-coder:6.7b, ollama/llama3.2:3b"
                },
                "continue_session": {
                    "type": "boolean",
                    "description": "Se true, continua l'ultima sessione OpenCode invece di iniziarne una nuova. Utile per task multi-step."
                }
            },
            "required": ["message"]
        }
    },
    {
        "name": "opencode_models",
        "description": "Lista i modelli AI disponibili per OpenCode (Ollama locale + provider cloud configurati).",
        "inputSchema": {
            "type": "object",
            "properties": {
                "provider": {
                    "type": "string",
                    "description": "Filtra per provider (es. 'ollama'). Vuoto = tutti i provider."
                }
            }
        }
    },
    {
        "name": "opencode_sessions",
        "description": "Lista le sessioni OpenCode recenti con ID e riepilogo.",
        "inputSchema": {
            "type": "object",
            "properties": {}
        }
    }
]


# ─── Implementazione strumenti ────────────────────────────────────────────────

def _run_opencode(message: str, working_dir: str, model: str, continue_session: bool) -> str:
    """Esegue opencode run e cattura stdout/stderr."""
    cmd = [OPENCODE_BIN, "run", message]

    if model:
        cmd += ["--model", model]
    if continue_session:
        cmd += ["--continue"]

    env = os.environ.copy()
    env["TERM"] = "dumb"   # disabilita colori ANSI che sporcano l'output

    try:
        result = subprocess.run(
            cmd,
            cwd=working_dir,
            capture_output=True,
            text=True,
            timeout=RUN_TIMEOUT,
            env=env
        )
        output = result.stdout.strip()
        stderr = result.stderr.strip()

        if result.returncode != 0 and not output:
            # Opencode restituisce output su stderr in alcuni casi
            if stderr:
                return f"[OpenCode output]\n{stderr}"
            return f"[Errore] opencode exit code {result.returncode}: {stderr or 'nessun output'}"

        # Combina stdout (principale) e stderr se ha info utili
        if output and stderr and len(stderr) < 200:
            return f"{output}\n\n[Note]\n{stderr}"
        return output or f"[OpenCode completato senza output visibile] (exit {result.returncode})"

    except subprocess.TimeoutExpired:
        return f"[Timeout] OpenCode non ha risposto entro {RUN_TIMEOUT}s. Prova un modello più veloce o riduci il task."
    except FileNotFoundError:
        return (
            f"[Errore] OpenCode non trovato ({OPENCODE_BIN}). "
            "Installalo con: curl -fsSL https://opencode.ai/install | bash\n"
            "Poi configura la variabile OPENCODE_BIN nel MCP."
        )
    except Exception as e:
        return f"[Errore interno] {e}"


def tool_ask_opencode(args: dict) -> str:
    message        = args.get("message", "").strip()
    working_dir    = args.get("working_dir", DEFAULT_DIR).strip() or DEFAULT_DIR
    model          = args.get("model", DEFAULT_MODEL).strip() or DEFAULT_MODEL
    cont           = args.get("continue_session", False)

    if not message:
        return "[Errore] Il campo 'message' è obbligatorio."

    # Normalizza la directory
    working_dir = str(Path(working_dir).expanduser().resolve())
    if not Path(working_dir).is_dir():
        return f"[Errore] Directory non trovata: {working_dir}"

    return _run_opencode(message, working_dir, model, cont)


def tool_opencode_models(args: dict) -> str:
    provider_filter = args.get("provider", "").strip().lower()
    try:
        result = subprocess.run(
            [OPENCODE_BIN, "models"],
            capture_output=True, text=True, timeout=15
        )
        output = result.stdout.strip()
        if provider_filter:
            lines = [l for l in output.splitlines() if provider_filter in l.lower()]
            output = "\n".join(lines) or f"Nessun modello trovato per provider '{provider_filter}'."
        return output or "[Nessun modello trovato]"
    except FileNotFoundError:
        return "[Errore] OpenCode non trovato nel PATH."
    except Exception as e:
        return f"[Errore] {e}"


def tool_opencode_sessions(args: dict) -> str:
    try:
        result = subprocess.run(
            [OPENCODE_BIN, "session", "list"],
            capture_output=True, text=True, timeout=10
        )
        return result.stdout.strip() or "[Nessuna sessione trovata]"
    except FileNotFoundError:
        return "[Errore] OpenCode non trovato nel PATH."
    except Exception as e:
        return f"[Errore] {e}"


TOOL_HANDLERS = {
    "ask_opencode":      tool_ask_opencode,
    "opencode_models":   tool_opencode_models,
    "opencode_sessions": tool_opencode_sessions,
}


# ─── Protocollo MCP (JSON-RPC 2.0 su stdio) ──────────────────────────────────

def _send(obj: dict):
    """Scrive un JSON su stdout (una riga) e flushes."""
    sys.stdout.write(json.dumps(obj) + "\n")
    sys.stdout.flush()


def _error(req_id, code: int, message: str, data=None):
    err = {"code": code, "message": message}
    if data is not None:
        err["data"] = data
    _send({"jsonrpc": "2.0", "id": req_id, "error": err})


def _result(req_id, result):
    _send({"jsonrpc": "2.0", "id": req_id, "result": result})


def handle(request: dict):
    method = request.get("method", "")
    req_id = request.get("id")
    params = request.get("params", {}) or {}

    # ── initialize ──────────────────────────────────────────────────
    if method == "initialize":
        _result(req_id, {
            "protocolVersion": "2024-11-05",
            "capabilities": {"tools": {}},
            "serverInfo": {
                "name":    "opencode-mcp",
                "version": "1.0.0"
            }
        })

    # ── initialized (notification, no id) ──────────────────────────
    elif method == "notifications/initialized":
        pass   # nessuna risposta per le notifiche

    # ── tools/list ─────────────────────────────────────────────────
    elif method == "tools/list":
        _result(req_id, {"tools": TOOLS})

    # ── tools/call ─────────────────────────────────────────────────
    elif method == "tools/call":
        name      = params.get("name", "")
        tool_args = params.get("arguments", {}) or {}
        handler   = TOOL_HANDLERS.get(name)
        if not handler:
            _error(req_id, -32601, f"Strumento '{name}' non trovato.")
            return
        try:
            text = handler(tool_args)
        except Exception as e:
            text = f"[Errore strumento] {e}"
        _result(req_id, {
            "content": [{"type": "text", "text": text}],
            "isError": text.startswith("[Errore")
        })

    # ── ping ───────────────────────────────────────────────────────
    elif method == "ping":
        _result(req_id, {})

    # ── metodo sconosciuto ─────────────────────────────────────────
    elif req_id is not None:
        _error(req_id, -32601, f"Metodo '{method}' non trovato.")


def main():
    """Loop principale: legge JSON-RPC da stdin riga per riga."""
    for raw_line in sys.stdin:
        raw_line = raw_line.strip()
        if not raw_line:
            continue
        try:
            request = json.loads(raw_line)
        except json.JSONDecodeError as e:
            _send({
                "jsonrpc": "2.0",
                "id": None,
                "error": {"code": -32700, "message": f"Parse error: {e}"}
            })
            continue
        try:
            handle(request)
        except Exception as e:
            req_id = request.get("id")
            if req_id is not None:
                _error(req_id, -32603, f"Internal error: {e}")


if __name__ == "__main__":
    main()
