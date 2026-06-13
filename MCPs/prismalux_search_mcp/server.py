#!/usr/bin/env python3
"""
prismalux_search_mcp — Ricerca intelligente nel codice Prismalux
================================================================
Fornisce a Claude Code accesso rapido al codebase Prismalux senza
dover lanciare multiple chiamate Bash sequenziali.

Tools esposti:
  grep_symbol   — trova dove è definita/usata una classe/funzione/segnale
  find_file     — trova file per nome/pattern nel progetto
  read_file     — legge un range di righe da un file
  list_pages    — elenca tutte le pagine (desktop + Android) con info chiave
  get_context   — restituisce convenzioni critiche e struttura del progetto
  search_todo   — cerca nei file TODO/ROADMAP del progetto

Uso in ~/.claude/settings.json:
  {
    "mcpServers": {
      "prismalux-search": {
        "type": "stdio",
        "command": "python3",
        "args": ["/home/wildlux/Desktop/Prismalux/MCPs/prismalux_search_mcp/server.py"]
      }
    }
  }
"""

import sys
import json
import os
import subprocess
import asyncio
import logging
from pathlib import Path
from typing import Any

logging.basicConfig(
    level=getattr(logging, os.environ.get("PRISMALUX_LOG_LEVEL", "WARNING")),
    format="%(asctime)s [%(name)s] %(levelname)s %(message)s",
)
logger = logging.getLogger(__name__)

ROOT = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))

# ─── Tool definitions ────────────────────────────────────────────────────────

TOOL_DEFS: list[dict[str, Any]] = [
    {
        "name": "grep_symbol",
        "description": (
            "Cerca un simbolo C++ o Python nel codebase Prismalux usando ripgrep. "
            "Ottimo per trovare dove è definita una classe, funzione, segnale o slot. "
            "Restituisce file:riga:contenuto per i match più rilevanti."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "symbol": {
                    "type": "string",
                    "description": "Simbolo da cercare (es. 'ChatPage', 'onFinished', 'AiClient::chat')",
                },
                "path": {
                    "type": "string",
                    "description": "Sottocartella in cui cercare (es. 'gui/', 'ANDROID/android_app/'). Default: tutto il progetto.",
                    "default": "",
                },
                "definition_only": {
                    "type": "boolean",
                    "description": "Se true, cerca solo la definizione (class X, void X(, signals:, etc.)",
                    "default": False,
                },
                "max_results": {
                    "type": "integer",
                    "description": "Numero massimo di risultati da restituire (default 30)",
                    "default": 30,
                },
            },
            "required": ["symbol"],
        },
    },
    {
        "name": "find_file",
        "description": (
            "Trova file nel progetto Prismalux per nome o pattern glob. "
            "Utile per trovare l'header o l'implementazione di una pagina."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "pattern": {
                    "type": "string",
                    "description": "Pattern nome file (es. 'chat_page*', '*.h', 'CMakeLists.txt')",
                },
                "path": {
                    "type": "string",
                    "description": "Sottocartella di ricerca (default: root progetto)",
                    "default": "",
                },
            },
            "required": ["pattern"],
        },
    },
    {
        "name": "read_file",
        "description": (
            "Legge un range di righe da un file del progetto. "
            "Path può essere assoluto o relativo alla root Prismalux."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "path": {
                    "type": "string",
                    "description": "Percorso del file (es. 'gui/pages/chat_page.h' o path assoluto)",
                },
                "start": {
                    "type": "integer",
                    "description": "Riga di inizio (1-based). Default: 1",
                    "default": 1,
                },
                "end": {
                    "type": "integer",
                    "description": "Riga di fine (0 = tutto il file, max 200 righe alla volta)",
                    "default": 0,
                },
            },
            "required": ["path"],
        },
    },
    {
        "name": "list_pages",
        "description": (
            "Elenca tutte le pagine del progetto Prismalux (desktop GUI e Android mobile) "
            "con file, indice stack, e stato (se presente nel drawer). "
            "Utile prima di aggiungere una nuova pagina o trovare l'indice corretto."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "target": {
                    "type": "string",
                    "description": "'desktop' | 'android' | 'all' (default: 'all')",
                    "default": "all",
                },
            },
        },
    },
    {
        "name": "get_context",
        "description": (
            "Restituisce le convenzioni critiche del progetto Prismalux: "
            "naming, lambda rules, path costanti, DPI, emoji encoding, ecc. "
            "Da leggere all'inizio di ogni sessione per evitare errori comuni."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "topic": {
                    "type": "string",
                    "description": "'all' | 'lambda' | 'path' | 'dpi' | 'emoji' | 'build' | 'android' | 'test'",
                    "default": "all",
                },
            },
        },
    },
    {
        "name": "search_todo",
        "description": (
            "Cerca nei file TODO e ROADMAP del progetto. "
            "Restituisce task aperti (⬜), in corso (🔄) o filtrati per keyword."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "keyword": {
                    "type": "string",
                    "description": "Parola chiave da cercare nei TODO (vuoto = tutti i task aperti)",
                    "default": "",
                },
                "status": {
                    "type": "string",
                    "description": "'open' (⬜) | 'done' (✅) | 'all' (default: 'open')",
                    "default": "open",
                },
            },
        },
    },
]

# ─── Handlers ────────────────────────────────────────────────────────────────

def _resolve_path(p: str) -> Path:
    """Risolve path relativo alla root Prismalux se non assoluto."""
    path = Path(p)
    if not path.is_absolute():
        path = ROOT / p
    return path


def handle_grep_symbol(args: dict[str, Any]) -> str:
    symbol      = args.get("symbol", "").strip()
    subpath     = args.get("path", "").strip()
    def_only    = args.get("definition_only", False)
    max_results = int(args.get("max_results", 30))

    if not symbol:
        return "Specifica un simbolo da cercare."

    search_root = ROOT / subpath if subpath else ROOT

    # Escludi directory di build e cache
    rg_cmd = [
        "rg", "--no-heading", "-n",
        "--glob", "*.{cpp,h,py,cmake,txt}",
        "--glob", "!build*", "--glob", "!_deps",
        "--glob", "!*.moc", "--glob", "!*_autogen*",
        "-m", str(max_results),
    ]

    if def_only:
        # Pattern per definizioni: class X, void X(, X::, signals:X, Q_OBJECT ecc.
        pattern = rf"(class\s+{symbol}|void\s+{symbol}\s*\(|{symbol}::|\bslots\b|\bsignals\b.*{symbol})"
        rg_cmd += ["-e", pattern]
    else:
        rg_cmd += [symbol]

    rg_cmd.append(str(search_root))

    try:
        result = subprocess.run(
            rg_cmd,
            capture_output=True, text=True, timeout=15,
        )
        lines = result.stdout.strip().split("\n") if result.stdout.strip() else []

        # Rendi i path relativi alla root
        out_lines = []
        for line in lines[:max_results]:
            try:
                parts = line.split(":", 2)
                rel   = str(Path(parts[0]).relative_to(ROOT))
                out_lines.append(f"{rel}:{':'.join(parts[1:])}")
            except Exception:
                out_lines.append(line)

        if not out_lines:
            return f"Nessun risultato per '{symbol}' in {search_root.relative_to(ROOT) if subpath else 'tutto il progetto'}."
        header = f"Risultati per '{symbol}' ({len(out_lines)} match):\n"
        return header + "\n".join(out_lines)

    except subprocess.TimeoutExpired:
        return "Timeout ricerca. Prova a specificare un path più specifico."
    except Exception as e:
        return f"Errore ricerca: {e}"


def handle_find_file(args: dict[str, Any]) -> str:
    pattern  = args.get("pattern", "").strip()
    subpath  = args.get("path", "").strip()

    if not pattern:
        return "Specifica un pattern di nome file."

    search_root = ROOT / subpath if subpath else ROOT

    try:
        result = subprocess.run(
            ["find", str(search_root),
             "-name", pattern,
             "-not", "-path", "*/build*",
             "-not", "-path", "*/_deps*",
             "-not", "-path", "*autogen*",
             "-not", "-path", "*/.git*",
             ],
            capture_output=True, text=True, timeout=10,
        )
        found = [l.strip() for l in result.stdout.strip().split("\n") if l.strip()]
        if not found:
            return f"Nessun file trovato per pattern '{pattern}'."

        rel_paths = []
        for f in sorted(found):
            try:
                rel_paths.append(str(Path(f).relative_to(ROOT)))
            except Exception:
                rel_paths.append(f)

        return f"File trovati ({len(rel_paths)}):\n" + "\n".join(rel_paths)

    except Exception as e:
        return f"Errore find: {e}"


def handle_read_file(args: dict[str, Any]) -> str:
    path  = args.get("path", "").strip()
    start = max(1, int(args.get("start", 1)))
    end   = int(args.get("end", 0))

    if not path:
        return "Specifica il path del file."

    file_path = _resolve_path(path)
    if not file_path.exists():
        return f"File non trovato: {file_path}"

    try:
        lines = file_path.read_text(encoding="utf-8", errors="replace").splitlines()
        total = len(lines)

        if end <= 0 or end > total:
            end = total
        # Limita a 200 righe per request
        if end - start + 1 > 200:
            end = start + 199

        selected = lines[start - 1:end]
        numbered = [f"{start + i:4d}  {l}" for i, l in enumerate(selected)]
        header   = f"// {path} — righe {start}-{end} di {total}\n"
        return header + "\n".join(numbered)

    except Exception as e:
        return f"Errore lettura: {e}"


def handle_list_pages(args: dict[str, Any]) -> str:
    target = args.get("target", "all").lower()
    lines  = []

    if target in ("desktop", "all"):
        lines.append("=== Desktop GUI (gui/mainwindow.cpp) ===")
        desktop_pages = [
            ( 0, "🤖 AI",           "agenti_page",           "Alt+1"),
            ( 1, "🛠 Strumenti",     "pratico_page",          "Alt+2"),
            ( 2, "🎬 Multimedia",    "multimedia_page",       ""),
            ( 3, "📁 File AI",       "strumenti_page",        ""),
            ( 4, "💻 Programmazione","programmazione_page",   "Alt+3"),
            ( 5, "π Matematica",     "matematica_page",       "Alt+4"),
            ( 6, "🔬 Ricerca",       "ricerca_page",          "Alt+5"),
            ( 7, "🕹 AppController", "main_app_controller",   "Alt+6"),
            ( 8, "🌐 LAN & WAN",     "lan_wan_page",          ""),
            ( 9, "🕸️ Multi-Agente", "agenti_multi_page",     ""),
        ]
        for idx, name, file, shortcut in desktop_pages:
            sc = f"  [{shortcut}]" if shortcut else ""
            lines.append(f"  [{idx}] {name}{sc}  →  gui/pages/{file}.h/cpp")

    if target in ("android", "all"):
        lines.append("\n=== Android Mobile (ANDROID/android_app/) ===")
        android_pages = [
            ( 0, "🤖 Chat AI",       "chat_page"),
            ( 1, "📚 Studio",        "studio_page"),
            ( 2, "💼 Lavoro AI",     "lavoro_page"),
            ( 3, "🎬 OBS",           "obs_page"),
            ( 4, "📐 Misure",        "misure_page"),
            ( 5, "📷 Camera",        "camera_page"),
            ( 6, "🔌 MCP",           "mcpaddons_page"),
            ( 7, "🔋 BLE",           "ble_page"),
            ( 8, "🎙️ Audio",        "audio_page"),
            ( 9, "⚙️ Impostazioni", "settings_page"),
            (10, "ℹ️ Info",          "info_page"),
            (11, "🎓 Impara",        "impara_page"),
            (12, "🔬 Ricerca",       "ricerca_page"),
            (13, "π Matematica",     "matematica_page"),
            (14, "🎵 Sintetizzatore","sintetizzatore_page"),
            (15, "🛠 Assistente",    "assistente_page"),
            (16, "🔮 Oracolo",       "oracle_page"),
            (17, "🧠 Hermes",        "hermes_page"),
            (18, "📁 File AI",       "file_ai_page"),
            (19, "💰 Finanza",       "finanza_page"),
            (20, "🤖 Simulatore",    "simulatore_page"),
            (21, "🌐 WAN Client",    "wan_client_page"),
            (22, "🔒 Sicurezza",     "security_page"),
            (23, "🕸️ Multi-Agente", "multi_agent_page"),
            (24, "📈 Grafico",       "chart_page"),
        ]
        for idx, name, file in android_pages:
            lines.append(f"  [{idx:2d}] {name:20s}  →  pages/{file}.h/cpp")

    lines.append("\nNota: aggiungere nuova pagina = nuovo indice alla fine, voce nel drawer, entry in onTabChanged()")
    return "\n".join(lines)


_CONVENTIONS = {
    "lambda": """\
REGOLA LAMBDA in connect():
  ✅ connect(btn, &QPushButton::clicked, this, [this]{ ... });   // context = this
  ✅ connect(ai, &AiClient::token, bar, [=](const QString& t){ ... }); // context = bar
  ❌ connect(reply, &QNetworkReply::finished, [this, reply]{ ... }); // no context → UAF
  ❌ static QMetaObject::Connection c;  // condivisa → zombie

  Logica > 2 righe → slot nominato. Context object sempre 4° arg.
  Per pattern one-shot: QMetaObject::Connection come membro, disconnect esplicito.""",

    "path": """\
PATH — mai hardcode, usare sempre:
  #include "prismalux_paths.h"
  namespace P = PrismaluxPaths;
  P::root()              → /home/wildlux/Desktop/Prismalux
  P::kOllamaPort         → 11434
  P::kLlamaServerPort    → 8081
  P::kWanComputePort     → 11600
  m_ai->backend()        → AiClient::Backend enum
  m_ai->host() / port()  → host e porta correnti""",

    "dpi": """\
DPI — usare dpiScale() per TUTTE le dimensioni strutturali:
  #include "dpi_utils.h"
  setFixedWidth(dpiScale(80));   // non setFixedWidth(80)
  setFixedHeight(dpiScale(36));  // non setFixedHeight(36)
  dpiScale() è no-op a 96dpi, scala su HiDPI/Wayland 2×""",

    "emoji": """\
EMOJI in C++:
  ✅ QString::fromUtf8("\\xf0\\x9f\\xa4\\x96")           // 🤖
  ✅ "\\xe2\\x80\\x9c" "Testo"    // ← concatena SEPARATO se seguono hex digits
  ❌ "\\xe2\\x80\\x9cTesto"       // C è hex valida → warning / bug
  Regola: se il char dopo la sequenza hex è [0-9a-fA-F], separa il literal.""",

    "build": """\
BUILD COMANDI:
  Desktop:        cmake --build gui/build_gui -j$(nproc)
  Android desk:   cmake --build ANDROID/android_app/build-desktop -j$(nproc)
  Android tests:  cmake --build ANDROID/android_app/tests/build && ./test_mobile_logic
  Desktop tests:  ctest --test-dir gui/Test/build_tests -j4
  Riconfigura:    cmake -B gui/build_gui gui/ -DCMAKE_BUILD_TYPE=Release

  PRIMA di modificare CMakeLists.txt → rifare cmake -B (non solo --build)""",

    "android": """\
ANDROID SPECIFICO:
  Stack indici: 0-24 (vedere list_pages tool)
  Drawer: buildDrawer() in mainwindow.cpp
  onTabChanged(): mappa indice → titolo header
  Conditional: HAVE_TTS, HAVE_MULTIMEDIA, HAVE_BLE, HAVE_SQL, HAVE_LOCAL_LLM
  Emoji Android: usare \\xNN\\xNN escape, non QString::fromUtf8 diretto
  File CMakeLists: ANDROID/android_app/CMakeLists.txt
  Build desktop per test: build-desktop/ (non build_android!)""",

    "test": """\
TEST STRUTTURA:
  Desktop: gui/tests/ + gui/Test/build_tests/ — 53 suite, ctest
  Android: ANDROID/android_app/tests/ — QtTest, logica pura (no UI, no NDK)
  Pattern test: estrarre logica in struct helper, testare quella (vedi AttachState in test_mobile_logic.cpp)
  Aggiungere test desktop: gui/CMakeLists.txt (copia struttura test_programmazione_page)
  CAT-A: costruzione (no Ollama) | CAT-B: logica pura | CAT-C: processo esterno (QSKIP) | CAT-D: Ollama reale""",
}


def handle_get_context(args: dict[str, Any]) -> str:
    topic = args.get("topic", "all").lower()

    if topic == "all":
        sections = []
        for t, content in _CONVENTIONS.items():
            sections.append(f"{'='*60}\n[{t.upper()}]\n{content}")
        sections.append("\n" + "="*60)
        sections.append("STRUTTURA CARTELLE CHIAVE:\n"
            "  gui/pages/           — pagine desktop (settings_*.cpp, main_*.h)\n"
            "  gui/widgets/         — widget riutilizzabili\n"
            "  ANDROID/android_app/ — app mobile Qt6\n"
            "  MCPs/                — 20+ plugin MCP Python\n"
            "  RAG/                 — documenti RAG (non in git)\n"
            "  gui/build_gui/       — cmake build desktop\n"
            "  ANDROID/android_app/build-desktop/ — cmake build Android desk")
        return "\n\n".join(sections)

    content = _CONVENTIONS.get(topic)
    if content:
        return f"[{topic.upper()}]\n{content}"
    return f"Topic sconosciuto: '{topic}'. Valori: {', '.join(_CONVENTIONS.keys())}, all"


def handle_search_todo(args: dict[str, Any]) -> str:
    keyword = args.get("keyword", "").strip().lower()
    status  = args.get("status", "open").lower()

    # File TODO da cercare
    todo_files = [
        ROOT / "ANDROID" / "TODO_ANDROID_ROADMAP.md",
        ROOT / "BEST_PRACTICE_&_GOAL" / "TODO.md",
    ]
    # Cerca anche altri TODO nella root
    todo_files += list(ROOT.glob("TODO*.md"))
    todo_files += list(ROOT.glob("BEST_PRACTICE*/*.md"))

    results = []
    status_markers = {"open": ["⬜", "🔄"], "done": ["✅"], "all": ["⬜", "🔄", "✅", "🔴", "🟡", "🟢"]}
    markers = status_markers.get(status, ["⬜", "🔄"])

    for todo_file in set(todo_files):
        if not todo_file.exists():
            continue
        try:
            lines = todo_file.read_text(encoding="utf-8", errors="replace").splitlines()
            rel   = str(todo_file.relative_to(ROOT))
            file_hits = []
            for i, line in enumerate(lines, 1):
                has_marker = any(m in line for m in markers)
                has_kw     = (not keyword) or (keyword in line.lower())
                if has_marker and has_kw:
                    file_hits.append(f"  {rel}:{i}  {line.strip()}")
            if file_hits:
                results.extend(file_hits)
        except Exception:
            continue

    if not results:
        kw_msg = f" con keyword '{keyword}'" if keyword else ""
        return f"Nessun task{kw_msg} con status '{status}'."

    header = f"Task trovati ({len(results)}{' | keyword: '+keyword if keyword else ''}):\n"
    return header + "\n".join(results[:80])


HANDLERS = {
    "grep_symbol":  handle_grep_symbol,
    "find_file":    handle_find_file,
    "read_file":    handle_read_file,
    "list_pages":   handle_list_pages,
    "get_context":  handle_get_context,
    "search_todo":  handle_search_todo,
}

# ─── JSON-RPC 2.0 loop ───────────────────────────────────────────────────────

def _rpc_result(req_id: Any, result: Any) -> str:
    return json.dumps({"jsonrpc": "2.0", "id": req_id, "result": result})

def _rpc_error(req_id: Any, code: int, message: str) -> str:
    return json.dumps({"jsonrpc": "2.0", "id": req_id,
                       "error": {"code": code, "message": message}})

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

        req_id = req.get("id")
        method = req.get("method", "")
        params = req.get("params", {})

        if method == "initialize":
            resp = _rpc_result(req_id, {
                "protocolVersion": "2024-11-05",
                "serverInfo": {"name": "prismalux-search", "version": "1.0.0"},
                "capabilities": {"tools": {}},
            })
        elif method == "tools/list":
            resp = _rpc_result(req_id, {"tools": TOOL_DEFS})
        elif method == "tools/call":
            name    = params.get("name", "")
            targs   = params.get("arguments", {})
            handler = HANDLERS.get(name)
            if not handler:
                resp = _rpc_error(req_id, -32601, f"Tool sconosciuto: {name}")
            else:
                try:
                    text = await asyncio.to_thread(handler, targs)
                    resp = _rpc_result(req_id, {
                        "content": [{"type": "text", "text": text}],
                        "isError": False,
                    })
                except Exception as e:
                    logger.error("Tool %s error: %s", name, e)
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
