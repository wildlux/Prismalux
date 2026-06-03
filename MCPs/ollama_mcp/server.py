#!/usr/bin/env python3
"""
Ollama MCP — Prismalux
Cache SQLite della lista modelli Ollama con ricerca e info dettagliate.
Riduce le chiamate HTTP a /api/tags ad ogni avvio: i modelli vengono
sincronizzati su richiesta e serviti dalla cache locale.

Protocollo: JSON-RPC 2.0 su stdio (standard MCP).

Tools esposti:
  list_models   — lista modelli dalla cache (o live se cache vuota)
  get_model_info — info dettagliate su un modello (dimensione, famiglia, quant)
  search_models  — cerca modelli per nome/dimensione/famiglia
  sync           — aggiorna la cache da Ollama (/api/tags)
  pull_model     — avvia il download di un modello (non bloccante)

Uso in ~/.claude/settings.json:
  {
    "mcpServers": {
      "ollama": {
        "type": "stdio",
        "command": "python3",
        "args": ["/path/to/Prismalux/MCPs/ollama_mcp/server.py"]
      }
    }
  }
"""

import sys
import json
import os
import sqlite3
import logging
import asyncio
import urllib.request
import urllib.error
from pathlib import Path
from datetime import datetime, timezone
from typing import Any

logging.basicConfig(
    level=getattr(logging, os.environ.get("PRISMALUX_LOG_LEVEL", "WARNING")),
    format="%(asctime)s [%(name)s] %(levelname)s %(message)s",
)
logger = logging.getLogger(__name__)

# ─── Configurazione ───────────────────────────────────────────────────────────

OLLAMA_HOST  = os.environ.get("OLLAMA_HOST", "http://localhost:11434")
DB_PATH      = Path.home() / ".prismalux" / "ollama_models_cache.db"
CACHE_TTL_S  = 300   # secondi — cache valida 5 minuti prima di richiedere sync

TOOL_DEFS: list[dict[str, Any]] = [
    {
        "name": "list_models",
        "description": (
            "Elenca i modelli Ollama disponibili localmente. "
            "Usa la cache SQLite per risposta rapida. "
            "Parametro opzionale 'force_sync' per aggiornare prima dalla rete."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "force_sync": {
                    "type": "boolean",
                    "description": "Se true, sincronizza prima da Ollama e poi restituisce la lista",
                    "default": False,
                }
            },
        },
    },
    {
        "name": "get_model_info",
        "description": "Restituisce informazioni dettagliate su un modello specifico (dimensione, famiglia, quantizzazione, parametri).",
        "inputSchema": {
            "type": "object",
            "properties": {
                "name": {
                    "type": "string",
                    "description": "Nome del modello (es. 'qwen3:8b', 'llama3.2:3b')",
                }
            },
            "required": ["name"],
        },
    },
    {
        "name": "search_models",
        "description": "Cerca modelli nella cache per nome, famiglia o dimensione massima in GB.",
        "inputSchema": {
            "type": "object",
            "properties": {
                "query": {
                    "type": "string",
                    "description": "Stringa di ricerca nel nome o nella famiglia del modello",
                    "default": "",
                },
                "max_size_gb": {
                    "type": "number",
                    "description": "Dimensione massima in GB (0 = nessun limite)",
                    "default": 0,
                },
            },
        },
    },
    {
        "name": "sync",
        "description": "Sincronizza la cache dei modelli da Ollama (/api/tags). Restituisce il numero di modelli trovati.",
        "inputSchema": {
            "type": "object",
            "properties": {},
        },
    },
    {
        "name": "pull_model",
        "description": "Avvia il download di un modello Ollama (non bloccante — il pull prosegue in background).",
        "inputSchema": {
            "type": "object",
            "properties": {
                "name": {
                    "type": "string",
                    "description": "Nome del modello da scaricare (es. 'qwen3:8b')",
                }
            },
            "required": ["name"],
        },
    },
]

# ─── Database ────────────────────────────────────────────────────────────────

def _init_db() -> sqlite3.Connection:
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(str(DB_PATH))
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA journal_mode=WAL")
    conn.execute("""
        CREATE TABLE IF NOT EXISTS models (
            name        TEXT PRIMARY KEY,
            size_bytes  INTEGER DEFAULT 0,
            family      TEXT DEFAULT '',
            format      TEXT DEFAULT '',
            quantization TEXT DEFAULT '',
            param_size  TEXT DEFAULT '',
            modified_at TEXT DEFAULT '',
            synced_at   TEXT DEFAULT ''
        )
    """)
    conn.execute("""
        CREATE TABLE IF NOT EXISTS meta (
            key   TEXT PRIMARY KEY,
            value TEXT
        )
    """)
    conn.commit()
    return conn


def _fetch_ollama_models() -> list[dict[str, Any]]:
    """Chiama /api/tags su Ollama e restituisce la lista raw."""
    try:
        url = f"{OLLAMA_HOST}/api/tags"
        req = urllib.request.Request(url, headers={"Accept": "application/json"})
        with urllib.request.urlopen(req, timeout=8) as resp:
            data = json.loads(resp.read())
            return data.get("models", [])
    except (urllib.error.URLError, OSError, json.JSONDecodeError) as e:
        logger.error("Ollama /api/tags non raggiungibile: %s", e)
        return []


def _sync_to_db(conn: sqlite3.Connection) -> int:
    """Sincronizza i modelli da Ollama nel DB. Restituisce il conteggio."""
    models = _fetch_ollama_models()
    if not models:
        return 0
    now = datetime.now(timezone.utc).isoformat()
    for m in models:
        details = m.get("details", {})
        conn.execute("""
            INSERT OR REPLACE INTO models
              (name, size_bytes, family, format, quantization, param_size, modified_at, synced_at)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            m.get("name", ""),
            m.get("size", 0),
            details.get("family", ""),
            details.get("format", ""),
            details.get("quantization_level", ""),
            details.get("parameter_size", ""),
            m.get("modified_at", ""),
            now,
        ))
    conn.execute("INSERT OR REPLACE INTO meta VALUES ('last_sync', ?)", (now,))
    conn.commit()
    return len(models)


def _cache_age_seconds(conn: sqlite3.Connection) -> float:
    """Restituisce l'età della cache in secondi (inf se mai sincronizzata)."""
    row = conn.execute("SELECT value FROM meta WHERE key='last_sync'").fetchone()
    if not row:
        return float("inf")
    try:
        last = datetime.fromisoformat(row["value"])
        now  = datetime.now(timezone.utc)
        # Normalizza entrambi a offset-aware
        if last.tzinfo is None:
            last = last.replace(tzinfo=timezone.utc)
        return (now - last).total_seconds()
    except Exception:
        return float("inf")


def _fmt_size(size_bytes: int) -> str:
    gb = size_bytes / 1_073_741_824
    return f"{gb:.1f} GB" if gb >= 1 else f"{size_bytes / 1_048_576:.0f} MB"


# ─── Tool handlers ───────────────────────────────────────────────────────────

def handle_list_models(args: dict[str, Any]) -> str:
    force = args.get("force_sync", False)
    conn  = _init_db()
    try:
        if force or _cache_age_seconds(conn) > CACHE_TTL_S:
            _sync_to_db(conn)
        rows = conn.execute(
            "SELECT name, size_bytes, family, quantization, param_size "
            "FROM models ORDER BY size_bytes ASC"
        ).fetchall()
        if not rows:
            return "Nessun modello in cache. Usa sync per aggiornare da Ollama."
        lines = ["Modelli Ollama disponibili:\n"]
        for r in rows:
            icon = "🟢"
            if r["size_bytes"] > 20_000_000_000:
                icon = "🔴"
            elif r["size_bytes"] > 8_000_000_000:
                icon = "🟡"
            fam  = f" [{r['family']}]" if r["family"] else ""
            qnt  = f" {r['quantization']}" if r["quantization"] else ""
            sz   = _fmt_size(r["size_bytes"]) if r["size_bytes"] else "?"
            lines.append(f"{icon} {r['name']}{fam}{qnt} — {sz}")
        lines.append(f"\nTotale: {len(rows)} modelli | Cache aggiornata < {CACHE_TTL_S}s")
        return "\n".join(lines)
    finally:
        conn.close()


def handle_get_model_info(args: dict[str, Any]) -> str:
    name = args.get("name", "").strip()
    if not name:
        return "Specifica il nome del modello (es. 'qwen3:8b')."
    conn = _init_db()
    try:
        # Prima cerca in cache
        row = conn.execute(
            "SELECT * FROM models WHERE name = ? OR name LIKE ?",
            (name, f"{name}%")
        ).fetchone()
        if not row:
            # Prova a sincronizzare e ricercare di nuovo
            _sync_to_db(conn)
            row = conn.execute(
                "SELECT * FROM models WHERE name = ? OR name LIKE ?",
                (name, f"{name}%")
            ).fetchone()
        if not row:
            return f"Modello '{name}' non trovato. Verifica che sia installato con `ollama list`."
        lines = [
            f"📦 Modello: {row['name']}",
            f"   Dimensione:     {_fmt_size(row['size_bytes'])}",
            f"   Famiglia:       {row['family'] or 'N/A'}",
            f"   Formato:        {row['format'] or 'N/A'}",
            f"   Quantizzazione: {row['quantization'] or 'N/A'}",
            f"   Parametri:      {row['param_size'] or 'N/A'}",
            f"   Modificato:     {row['modified_at'][:10] if row['modified_at'] else 'N/A'}",
        ]
        return "\n".join(lines)
    finally:
        conn.close()


def handle_search_models(args: dict[str, Any]) -> str:
    query      = args.get("query", "").strip().lower()
    max_size   = args.get("max_size_gb", 0)
    max_bytes  = int(max_size * 1_073_741_824) if max_size > 0 else 0
    conn = _init_db()
    try:
        if _cache_age_seconds(conn) > CACHE_TTL_S:
            _sync_to_db(conn)
        sql  = "SELECT name, size_bytes, family, quantization FROM models WHERE 1=1"
        params: list[Any] = []
        if query:
            sql += " AND (LOWER(name) LIKE ? OR LOWER(family) LIKE ?)"
            params += [f"%{query}%", f"%{query}%"]
        if max_bytes > 0:
            sql += " AND size_bytes <= ?"
            params.append(max_bytes)
        sql += " ORDER BY size_bytes ASC"
        rows = conn.execute(sql, params).fetchall()
        if not rows:
            return f"Nessun modello trovato per '{query}'" + (
                f" entro {max_size} GB" if max_size else "") + "."
        lines = [f"Risultati ricerca '{query}':\n"]
        for r in rows:
            sz  = _fmt_size(r["size_bytes"]) if r["size_bytes"] else "?"
            fam = f" [{r['family']}]" if r["family"] else ""
            lines.append(f"• {r['name']}{fam} — {sz}")
        lines.append(f"\n{len(rows)} modelli trovati")
        return "\n".join(lines)
    finally:
        conn.close()


def handle_sync(_args: dict[str, Any]) -> str:
    conn = _init_db()
    try:
        count = _sync_to_db(conn)
        if count == 0:
            return "⚠️ Ollama non raggiungibile o nessun modello installato. Avvia: ollama serve"
        return f"✅ Sincronizzazione completata: {count} modelli in cache."
    finally:
        conn.close()


import re as _re

def _validate_model_name(name: str) -> str:
    """Valida il nome modello Ollama: solo caratteri sicuri (alfanumerico, ., :, -, _)."""
    if not isinstance(name, str):
        raise ValueError("Il nome modello deve essere una stringa.")
    name = name.strip()
    if not name:
        raise ValueError("Nome modello vuoto.")
    if len(name) > 128:
        raise ValueError("Nome modello troppo lungo (max 128 caratteri).")
    if not _re.match(r'^[A-Za-z0-9._:\-/]+$', name):
        raise ValueError(f"Nome modello non valido: {name!r}. Usa solo lettere, numeri, '.', ':', '-', '/'")
    return name


def handle_pull_model(args: dict[str, Any]) -> str:
    import subprocess
    name = args.get("name", "").strip()
    if not name:
        return "Specifica il nome del modello da scaricare."
    try:
        name = _validate_model_name(name)
    except ValueError as e:
        return f"[Errore] {e}"
    try:
        subprocess.Popen(
            ["ollama", "pull", name],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        return (
            f"⬇️ Download di '{name}' avviato in background.\n"
            "Usa `ollama list` per verificare il progresso."
        )
    except FileNotFoundError:
        return "❌ Comando 'ollama' non trovato. Installa Ollama da https://ollama.com"


HANDLERS = {
    "list_models":    handle_list_models,
    "get_model_info": handle_get_model_info,
    "search_models":  handle_search_models,
    "sync":           handle_sync,
    "pull_model":     handle_pull_model,
}


# ─── JSON-RPC 2.0 loop ───────────────────────────────────────────────────────

def _rpc_result(req_id: Any, result: Any) -> str:
    return json.dumps({"jsonrpc": "2.0", "id": req_id, "result": result})


def _rpc_error(req_id: Any, code: int, message: str) -> str:
    return json.dumps({
        "jsonrpc": "2.0",
        "id": req_id,
        "error": {"code": code, "message": message},
    })


async def _main_async() -> None:
    loop = asyncio.get_event_loop()

    async def readline() -> str:
        return await loop.run_in_executor(None, sys.stdin.readline)

    while True:
        line = await readline()
        if not line:
            break
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except json.JSONDecodeError as e:
            sys.stdout.write(_rpc_error(None, -32700, f"Parse error: {e}") + "\n")
            sys.stdout.flush()
            continue

        req_id  = req.get("id")
        method  = req.get("method", "")
        params  = req.get("params", {})

        if method == "initialize":
            resp = _rpc_result(req_id, {
                "protocolVersion": "2024-11-05",
                "serverInfo": {"name": "ollama-mcp", "version": "1.0.0"},
                "capabilities": {"tools": {}},
            })
        elif method == "tools/list":
            resp = _rpc_result(req_id, {"tools": TOOL_DEFS})
        elif method == "tools/call":
            tool_name = params.get("name", "")
            tool_args = params.get("arguments", {})
            handler   = HANDLERS.get(tool_name)
            if not handler:
                resp = _rpc_error(req_id, -32601, f"Tool sconosciuto: {tool_name}")
            else:
                try:
                    text = await asyncio.to_thread(handler, tool_args)
                    resp = _rpc_result(req_id, {
                        "content": [{"type": "text", "text": text}],
                        "isError": False,
                    })
                except Exception as e:
                    logger.error("Tool %s error: %s", tool_name, e)
                    resp = _rpc_result(req_id, {
                        "content": [{"type": "text", "text": f"Errore: {e}"}],
                        "isError": True,
                    })
        elif method == "notifications/initialized":
            continue
        else:
            resp = _rpc_error(req_id, -32601, f"Metodo sconosciuto: {method}")

        sys.stdout.write(resp + "\n")
        sys.stdout.flush()


def main() -> None:
    asyncio.run(_main_async())


if __name__ == "__main__":
    main()
