#!/usr/bin/env python3
"""
export_mcp — Esporta il sorgente Prismalux come ZIP
====================================================
Crea uno ZIP strutturato in EXPORT/ escludendo cartelle pesanti
(build artifacts, modelli LLM, RAG, framework binari) ma mantenendo
la struttura delle cartelle con file .gitkeep placeholder.

Tools esposti:
  export_project  — crea EXPORT/Prismalux_v<ver>_Sorgenti_<data>.zip
  list_exports    — elenca gli ZIP già presenti in EXPORT/

Uso in ~/.claude/settings.json:
  {
    "mcpServers": {
      "prismalux-export": {
        "type": "stdio",
        "command": "python3",
        "args": ["/home/wildlux/Desktop/Prismalux/MCPs/export_mcp/server.py"]
      }
    }
  }
"""

import sys
import json
import os
import zipfile
import fnmatch
import logging
from pathlib import Path
from datetime import datetime
from typing import Any

logging.basicConfig(
    level=getattr(logging, os.environ.get("PRISMALUX_LOG_LEVEL", "WARNING")),
    format="%(asctime)s [%(name)s] %(levelname)s %(message)s",
)
logger = logging.getLogger(__name__)

ROOT    = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
EXPORT  = ROOT / "EXPORT"
VERSION = os.environ.get("PRISMALUX_VERSION", "3.0")

# ─── Cosa escludere ───────────────────────────────────────────────────────────

# Cartelle di primo livello (e EXPORT stesso per evitare ricorsione)
_HEAVY_ROOT = {
    "build_gui",
    "AppDir",
    "ENGINE_LLM",
    "Frameworks",
    "models",
    "RAG",
    "KNOWLEDGE_USER",
    "EXPORT",
}

# Sotto-percorsi specifici (relativi a ROOT)
_HEAVY_SUBPATHS = {
    Path("gui") / "build_gui",
    Path("Test") / "build_tests",
    Path("ANDROID") / "android_app",
    Path("Tools") / "compile_win" / "qt6_cross",
}

# Pattern nome file/cartella da saltare
_SKIP_PATTERNS = [
    "*.AppImage",
    "*.apk",
    "*.idsig",
    "__pycache__",
    "*.pyc",
    "*.pyo",
    "*.o",
    "*.a",
    "*.so",
    "*.dll",
    "*.exe",
    "Prismalux ottimizzazione.zip",
    "setup-x86_64.exe",
    ".git",
]

# ─── Helpers ─────────────────────────────────────────────────────────────────

def _is_heavy(rel: Path) -> bool:
    """True se il percorso relativo punta a una dir/file da escludere."""
    parts = rel.parts
    if len(parts) == 1 and parts[0] in _HEAVY_ROOT:
        return True
    for sp in _HEAVY_SUBPATHS:
        sp_parts = sp.parts
        if parts[: len(sp_parts)] == sp_parts:
            return True
    return False


def _skip_name(name: str) -> bool:
    return any(fnmatch.fnmatch(name, p) for p in _SKIP_PATTERNS)


def _build_zip(out_path: Path) -> dict[str, Any]:
    """Crea il ZIP e restituisce statistiche."""
    added        = 0
    placeholders: list[str] = []

    with zipfile.ZipFile(out_path, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
        for root_str, dirs, files in os.walk(ROOT):
            root_path = Path(root_str)
            rel_root  = root_path.relative_to(ROOT)

            # Siamo dentro una dir pesante? placeholder e skip.
            if rel_root != Path(".") and _is_heavy(rel_root):
                key = str(rel_root)
                if key not in placeholders:
                    zf.writestr(
                        str(rel_root / ".gitkeep"),
                        f"# Cartella esclusa dallo ZIP (pesante o privata)\n# Path: {rel_root}\n",
                    )
                    placeholders.append(key)
                dirs.clear()
                continue

            # Filtra sottocartelle
            keep: list[str] = []
            for d in dirs:
                if _skip_name(d):
                    continue
                sub = rel_root / d
                if _is_heavy(sub):
                    zf.writestr(
                        str(sub / ".gitkeep"),
                        f"# Cartella esclusa dallo ZIP (pesante o privata)\n# Path: {sub}\n",
                    )
                    placeholders.append(str(sub))
                    continue
                keep.append(d)
            dirs[:] = sorted(keep)

            # Aggiungi file
            for fname in files:
                fpath = root_path / fname
                frel  = rel_root / fname
                if _skip_name(fname):
                    continue
                try:
                    zf.write(fpath, frel)
                    added += 1
                except Exception as exc:
                    logger.warning("skip %s: %s", frel, exc)

    size_mb = out_path.stat().st_size / (1024 * 1024)
    return {
        "zip":          str(out_path),
        "files_added":  added,
        "size_mb":      round(size_mb, 1),
        "placeholders": placeholders,
    }


# ─── Tool definitions ────────────────────────────────────────────────────────

TOOL_DEFS: list[dict[str, Any]] = [
    {
        "name": "export_project",
        "description": (
            "Crea uno ZIP del sorgente Prismalux in EXPORT/. "
            "Esclude build artifacts, modelli LLM, framework binari, RAG e KNOWLEDGE_USER. "
            "Mantiene la struttura delle cartelle con file .gitkeep placeholder. "
            "Restituisce il percorso, il numero di file e la dimensione in MB."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "version": {
                    "type": "string",
                    "description": (
                        "Versione da includere nel nome file, es. '3.0'. "
                        "Default: valore di PRISMALUX_VERSION o '3.0'."
                    ),
                },
            },
        },
    },
    {
        "name": "list_exports",
        "description": "Elenca gli ZIP già presenti nella cartella EXPORT/ con data e dimensione.",
        "inputSchema": {"type": "object", "properties": {}},
    },
]

# ─── Dispatcher ──────────────────────────────────────────────────────────────

def _handle(tool: str, args: dict[str, Any]) -> Any:
    if tool == "export_project":
        ver  = args.get("version") or VERSION
        date = datetime.now().strftime("%Y-%m-%d")
        name = f"Prismalux_v{ver}_Sorgenti_{date}.zip"
        out  = EXPORT / name
        EXPORT.mkdir(parents=True, exist_ok=True)
        stats = _build_zip(out)
        lines = [
            f"ZIP creato: {stats['zip']}",
            f"File inclusi: {stats['files_added']}",
            f"Dimensione: {stats['size_mb']} MB",
            "",
            f"Cartelle escluse ({len(stats['placeholders'])} placeholder .gitkeep):",
        ]
        for p in sorted(stats["placeholders"]):
            lines.append(f"  \U0001f4c1 {p}/")
        return "\n".join(lines)

    if tool == "list_exports":
        if not EXPORT.exists():
            return "Cartella EXPORT/ non trovata."
        zips = sorted(EXPORT.glob("*.zip"), key=lambda p: p.stat().st_mtime, reverse=True)
        if not zips:
            return "Nessuno ZIP trovato in EXPORT/."
        lines = ["ZIP in EXPORT/ (dal più recente):"]
        for z in zips:
            mb   = z.stat().st_size / (1024 * 1024)
            mtime = datetime.fromtimestamp(z.stat().st_mtime).strftime("%Y-%m-%d %H:%M")
            lines.append(f"  {z.name}  ({mb:.1f} MB)  [{mtime}]")
        return "\n".join(lines)

    return f"Tool sconosciuto: {tool}"


# ─── JSON-RPC 2.0 loop ───────────────────────────────────────────────────────

def _reply(id_: Any, result: Any) -> None:
    msg = json.dumps({"jsonrpc": "2.0", "id": id_, "result": result})
    sys.stdout.write(msg + "\n")
    sys.stdout.flush()


def _error(id_: Any, code: int, message: str) -> None:
    msg = json.dumps({"jsonrpc": "2.0", "id": id_, "error": {"code": code, "message": message}})
    sys.stdout.write(msg + "\n")
    sys.stdout.flush()


def main() -> None:
    logger.info("export_mcp avviato (root=%s)", ROOT)
    for raw in sys.stdin:
        raw = raw.strip()
        if not raw:
            continue
        try:
            req = json.loads(raw)
        except json.JSONDecodeError as exc:
            _error(None, -32700, f"Parse error: {exc}")
            continue

        id_    = req.get("id")
        method = req.get("method", "")
        params = req.get("params") or {}

        if method == "initialize":
            _reply(id_, {
                "protocolVersion": "2024-11-05",
                "capabilities":    {"tools": {}},
                "serverInfo":      {"name": "export_mcp", "version": "1.0.0"},
            })

        elif method == "tools/list":
            _reply(id_, {"tools": TOOL_DEFS})

        elif method == "tools/call":
            tool = params.get("name", "")
            args = params.get("arguments") or {}
            try:
                result = _handle(tool, args)
                _reply(id_, {"content": [{"type": "text", "text": str(result)}]})
            except Exception as exc:
                logger.exception("Errore in tool %s", tool)
                _error(id_, -32603, f"Errore interno: {exc}")

        elif method == "notifications/initialized":
            pass  # notifica — nessuna risposta

        else:
            _error(id_, -32601, f"Method not found: {method}")


if __name__ == "__main__":
    main()
