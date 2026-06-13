#!/usr/bin/env python3
"""
prismalux_build_mcp — Build e test automatizzati per Prismalux
==============================================================
Permette di compilare e testare il progetto senza uscire dalla
sessione Claude Code. Restituisce errori strutturati pronti per
essere analizzati e corretti.

Tools esposti:
  build          — compila desktop GUI o Android (desktop o tests)
  run_tests      — esegue ctest con filtro opzionale
  cmake_config   — riconfigura cmake (necessario dopo modifica CMakeLists)
  get_build_log  — restituisce l'ultimo log di build salvato
  check_compile  — rapido controllo se un file compila (senza linkare)

Uso in ~/.claude/settings.json:
  {
    "mcpServers": {
      "prismalux-build": {
        "type": "stdio",
        "command": "python3",
        "args": ["/home/wildlux/Desktop/Prismalux/MCPs/prismalux_build_mcp/server.py"]
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
import re
import tempfile
from pathlib import Path
from typing import Any

logging.basicConfig(
    level=getattr(logging, os.environ.get("PRISMALUX_LOG_LEVEL", "WARNING")),
    format="%(asctime)s [%(name)s] %(levelname)s %(message)s",
)
logger = logging.getLogger(__name__)

ROOT = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))

# Percorsi build
_TARGETS = {
    "desktop":        ROOT / "gui" / "build_gui",
    "android_desktop": ROOT / "ANDROID" / "android_app" / "build-desktop",
    "android_tests":  ROOT / "ANDROID" / "android_app" / "tests" / "build",
}

_CMAKE_SRCS = {
    "desktop":        ROOT / "gui",
    "android_desktop": ROOT / "ANDROID" / "android_app",
    "android_tests":  ROOT / "ANDROID" / "android_app" / "tests",
}

_LOG_FILE = Path.home() / ".prismalux" / "last_build.log"

# ─── Tool definitions ────────────────────────────────────────────────────────

TOOL_DEFS: list[dict[str, Any]] = [
    {
        "name": "build",
        "description": (
            "Compila il progetto Prismalux. "
            "Restituisce un riepilogo strutturato: errori con file:riga:messaggio, "
            "warning importanti, e se la build è riuscita."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "target": {
                    "type": "string",
                    "description": (
                        "'desktop'          — GUI Qt6 desktop (gui/build_gui)\n"
                        "'android_desktop'  — Android app compilata per Linux desktop (per test rapidi)\n"
                        "'android_tests'    — Suite di test Android (ANDROID/android_app/tests/build)\n"
                        "Default: 'desktop'"
                    ),
                    "default": "desktop",
                },
                "jobs": {
                    "type": "integer",
                    "description": "Numero di job paralleli (default: nproc)",
                    "default": 0,
                },
                "verbose": {
                    "type": "boolean",
                    "description": "Se true, include output completo cmake (molto lungo)",
                    "default": False,
                },
            },
        },
    },
    {
        "name": "run_tests",
        "description": (
            "Esegue i test del progetto con ctest o direttamente l'eseguibile di test. "
            "Restituisce PASS/FAIL per ogni suite con eventuale output di failure."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "target": {
                    "type": "string",
                    "description": (
                        "'android_tests' — test Qt Android (test_mobile_logic)\n"
                        "'desktop'       — test desktop GUI (ctest)\n"
                        "Default: 'android_tests'"
                    ),
                    "default": "android_tests",
                },
                "filter": {
                    "type": "string",
                    "description": "Pattern ctest -R per filtrare suite (es. 'GraphMemory', 'Finanza')",
                    "default": "",
                },
                "output_on_failure": {
                    "type": "boolean",
                    "description": "Se true, mostra output completo dei test falliti",
                    "default": True,
                },
            },
        },
    },
    {
        "name": "cmake_config",
        "description": (
            "Riconfigura cmake per un target. Necessario dopo modifiche a CMakeLists.txt "
            "o aggiunta di nuovi file sorgente. Più lento di 'build' ma obbligatorio in questi casi."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "target": {
                    "type": "string",
                    "description": "'desktop' | 'android_desktop' | 'android_tests'",
                    "default": "desktop",
                },
                "extra_flags": {
                    "type": "string",
                    "description": "Flag cmake aggiuntivi (es. '-DBUILD_TESTS=ON')",
                    "default": "",
                },
            },
        },
    },
    {
        "name": "get_build_log",
        "description": (
            "Restituisce l'ultimo log di build salvato. "
            "Utile per analizzare errori di una build precedente."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "lines": {
                    "type": "integer",
                    "description": "Numero di righe da restituire (default: 100, 0 = tutto)",
                    "default": 100,
                },
                "errors_only": {
                    "type": "boolean",
                    "description": "Se true, filtra solo le righe con 'error:' o 'fatal:'",
                    "default": True,
                },
            },
        },
    },
    {
        "name": "check_compile",
        "description": (
            "Verifica rapidamente se un file .cpp/.h ha errori di compilazione "
            "senza fare il link dell'intera app. Più veloce di una build completa."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "file": {
                    "type": "string",
                    "description": "Path del file relativo alla root Prismalux (es. 'gui/pages/chat_page.cpp')",
                },
            },
            "required": ["file"],
        },
    },
]

# ─── Helpers ─────────────────────────────────────────────────────────────────

def _nproc() -> int:
    try:
        import multiprocessing
        return max(1, multiprocessing.cpu_count())
    except Exception:
        return 4


def _parse_errors(output: str) -> list[str]:
    """Estrae righe error:/fatal: dal cmake output."""
    errors = []
    for line in output.splitlines():
        if "error:" in line.lower() or "fatal error" in line.lower():
            # Rendi i path relativi alla root
            try:
                m = re.match(r"(/[^:]+):(.*)", line)
                if m:
                    rel = str(Path(m.group(1)).relative_to(ROOT))
                    line = rel + ":" + m.group(2)
            except Exception:
                pass
            errors.append(line.strip())
    return errors


def _parse_warnings(output: str, max_w: int = 5) -> list[str]:
    warnings = []
    for line in output.splitlines():
        if "warning:" in line.lower() and len(warnings) < max_w:
            try:
                m = re.match(r"(/[^:]+):(.*)", line)
                if m:
                    rel = str(Path(m.group(1)).relative_to(ROOT))
                    line = rel + ":" + m.group(2)
            except Exception:
                pass
            warnings.append(line.strip())
    return warnings


def _save_log(content: str) -> None:
    try:
        _LOG_FILE.parent.mkdir(parents=True, exist_ok=True)
        _LOG_FILE.write_text(content, encoding="utf-8")
    except Exception:
        pass


# ─── Handlers ────────────────────────────────────────────────────────────────

def handle_build(args: dict[str, Any]) -> str:
    target  = args.get("target", "desktop").lower().strip()
    jobs    = int(args.get("jobs", 0)) or _nproc()
    verbose = bool(args.get("verbose", False))

    build_dir = _TARGETS.get(target)
    if not build_dir:
        return f"Target sconosciuto: '{target}'. Valori: {', '.join(_TARGETS)}"
    if not build_dir.exists():
        return (
            f"Directory build non trovata: {build_dir}\n"
            f"Esegui prima cmake_config per '{target}'."
        )

    cmd = ["cmake", "--build", str(build_dir), "-j", str(jobs)]
    if verbose:
        cmd += ["--", "VERBOSE=1"]

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=300,
            cwd=str(ROOT),
        )
        full_output = result.stdout + result.stderr
        _save_log(full_output)

        errors   = _parse_errors(full_output)
        warnings = _parse_warnings(full_output)
        success  = result.returncode == 0

        lines = []
        if success:
            lines.append(f"✅ Build '{target}' completata con successo.")
            if warnings:
                lines.append(f"\n⚠️  {len(warnings)} warning (primi {len(warnings)}):")
                lines.extend(f"  {w}" for w in warnings)
        else:
            lines.append(f"❌ Build '{target}' FALLITA (codice {result.returncode}).")
            if errors:
                lines.append(f"\n🔴 Errori ({len(errors)}):")
                lines.extend(f"  {e}" for e in errors[:20])
            if len(errors) > 20:
                lines.append(f"  ... e altri {len(errors)-20} errori. Usa get_build_log per il log completo.")
            if warnings:
                lines.append(f"\n⚠️  Warning ({len(warnings)}):")
                lines.extend(f"  {w}" for w in warnings[:5])

        # Ultima riga di cmake (spesso "Built target X" o "Error")
        last_meaningful = [l for l in full_output.splitlines() if l.strip() and not l.startswith(" ")]
        if last_meaningful:
            lines.append(f"\nUltimo output: {last_meaningful[-1].strip()}")

        return "\n".join(lines)

    except subprocess.TimeoutExpired:
        return f"❌ Timeout (300s) durante build '{target}'."
    except Exception as e:
        return f"❌ Errore avvio build: {e}"


def handle_run_tests(args: dict[str, Any]) -> str:
    target   = args.get("target", "android_tests").lower().strip()
    filt     = args.get("filter", "").strip()
    on_fail  = bool(args.get("output_on_failure", True))

    build_dir = _TARGETS.get(target)
    if not build_dir or not build_dir.exists():
        return f"Build dir non trovata per '{target}'. Compila prima con build()."

    if target == "android_tests":
        # Esegui direttamente l'eseguibile QtTest
        exe = build_dir / "test_mobile_logic"
        if not exe.exists():
            return f"Eseguibile non trovato: {exe}\nEsegui prima build('android_tests')."
        cmd = [str(exe), "-v2"] if on_fail else [str(exe)]
    else:
        # ctest
        cmd = ["ctest", "--test-dir", str(build_dir), "-j", str(_nproc())]
        if filt:
            cmd += ["-R", filt]
        if on_fail:
            cmd += ["--output-on-failure"]

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=120,
            cwd=str(ROOT),
        )
        output = result.stdout + result.stderr
        _save_log(output)

        # Parse risultato
        total_re  = re.search(r"Totals:\s*(\d+)\s*passed,\s*(\d+)\s*failed", output)
        ctest_re  = re.search(r"(\d+)%\s+tests passed.*out of\s+(\d+)", output)

        lines = []
        if total_re:
            passed = int(total_re.group(1))
            failed = int(total_re.group(2))
            icon   = "✅" if failed == 0 else "❌"
            lines.append(f"{icon} {passed} PASS, {failed} FAIL")
        elif ctest_re:
            pct    = int(ctest_re.group(1))
            total  = int(ctest_re.group(2))
            icon   = "✅" if pct == 100 else "❌"
            lines.append(f"{icon} {pct}% ({total} suite)")
        else:
            lines.append("Output test:")

        if result.returncode != 0:
            # Mostra le righe FAIL
            fail_lines = [l for l in output.splitlines()
                         if "FAIL" in l or "failed" in l.lower()]
            if fail_lines:
                lines.append("\nTest falliti:")
                lines.extend(f"  {l.strip()}" for l in fail_lines[:20])
        else:
            lines.append("Tutti i test superati.")

        if on_fail and result.returncode != 0:
            # Includi output dettagliato (max 50 righe)
            detail = [l for l in output.splitlines()
                     if "FAIL!" in l or "QCOMPARE" in l or "QVERIFY" in l or "Actual" in l]
            if detail:
                lines.append("\nDettaglio fallimenti:")
                lines.extend(f"  {l.strip()}" for l in detail[:20])

        return "\n".join(lines)

    except subprocess.TimeoutExpired:
        return "❌ Timeout (120s) durante esecuzione test."
    except Exception as e:
        return f"❌ Errore avvio test: {e}"


def handle_cmake_config(args: dict[str, Any]) -> str:
    target      = args.get("target", "desktop").lower().strip()
    extra_flags = args.get("extra_flags", "").strip()

    src_dir   = _CMAKE_SRCS.get(target)
    build_dir = _TARGETS.get(target)

    if not src_dir:
        return f"Target sconosciuto: '{target}'. Valori: {', '.join(_TARGETS)}"

    build_dir.mkdir(parents=True, exist_ok=True)

    cmd = [
        "cmake", "-B", str(build_dir), str(src_dir),
        "-DCMAKE_BUILD_TYPE=Release",
    ]
    if extra_flags:
        cmd += extra_flags.split()

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=120,
            cwd=str(ROOT),
        )
        output = result.stdout + result.stderr
        _save_log(output)

        if result.returncode == 0:
            # Estrai info utili da cmake output
            status_lines = [l for l in output.splitlines()
                           if l.strip().startswith("--") or "enabled" in l.lower()
                           or "disabled" in l.lower() or "abilitato" in l.lower()]
            summary = "\n".join(status_lines[:20])
            return f"✅ cmake configurato per '{target}'.\n\nStatus moduli:\n{summary}"
        else:
            errors = _parse_errors(output)
            lines  = [f"❌ cmake config fallito per '{target}'."]
            if errors:
                lines.append("\nErrori:")
                lines.extend(f"  {e}" for e in errors[:10])
            else:
                # Ultime 10 righe
                last = output.strip().splitlines()[-10:]
                lines.extend(last)
            return "\n".join(lines)

    except subprocess.TimeoutExpired:
        return "❌ Timeout (120s) durante cmake config."
    except Exception as e:
        return f"❌ Errore: {e}"


def handle_get_build_log(args: dict[str, Any]) -> str:
    n_lines    = int(args.get("lines", 100))
    errors_only = bool(args.get("errors_only", True))

    if not _LOG_FILE.exists():
        return "Nessun log di build salvato. Esegui prima build()."

    try:
        content = _LOG_FILE.read_text(encoding="utf-8", errors="replace")
        lines   = content.splitlines()

        if errors_only:
            lines = [l for l in lines
                    if "error:" in l.lower() or "fatal" in l.lower() or "warning:" in l.lower()]

        if n_lines > 0:
            lines = lines[-n_lines:] if len(lines) > n_lines else lines

        if not lines:
            return "Log presente ma nessun errore/warning trovato."

        return f"Build log ({len(lines)} righe):\n" + "\n".join(lines)

    except Exception as e:
        return f"Errore lettura log: {e}"


def handle_check_compile(args: dict[str, Any]) -> str:
    file_rel = args.get("file", "").strip()
    if not file_rel:
        return "Specifica il path del file."

    file_path = ROOT / file_rel
    if not file_path.exists():
        return f"File non trovato: {file_path}"

    suffix = file_path.suffix.lower()
    if suffix not in (".cpp", ".h", ".c"):
        return f"Supportati solo .cpp/.h/.c, trovato: {suffix}"

    # Troviamo il build dir appropriato
    if "ANDROID" in file_rel:
        build_dir = _TARGETS["android_desktop"]
        inc_dirs = [
            str(ROOT / "ANDROID" / "android_app"),
            str(ROOT / "ANDROID" / "android_app" / "pages"),
        ]
    else:
        build_dir = _TARGETS["desktop"]
        inc_dirs = [
            str(ROOT / "gui"),
            str(ROOT / "gui" / "pages"),
            str(ROOT / "gui" / "widgets"),
        ]

    if not build_dir.exists():
        return f"Build dir non trovata. Esegui cmake_config prima."

    # Cerca compile_commands.json per trovare i flag reali
    cc_json = build_dir / "compile_commands.json"
    if cc_json.exists():
        try:
            import json as json_mod
            commands = json_mod.loads(cc_json.read_text())
            for entry in commands:
                if file_rel in entry.get("file", "") or str(file_path) == entry.get("file", ""):
                    cmd = entry["command"].split()
                    # Sostituisci -c e -o per solo controllare la sintassi
                    try:
                        o_idx = cmd.index("-o")
                        cmd[o_idx+1] = "/dev/null"
                    except ValueError:
                        pass
                    result = subprocess.run(
                        cmd,
                        capture_output=True, text=True, timeout=30,
                        cwd=entry.get("directory", str(ROOT))
                    )
                    if result.returncode == 0:
                        return f"✅ {file_rel} compila correttamente."
                    errors = _parse_errors(result.stderr + result.stdout)
                    return f"❌ Errori in {file_rel}:\n" + "\n".join(errors[:15])
        except Exception:
            pass

    # Fallback: g++ diretto
    cmd = ["g++", "-std=c++17", "-fsyntax-only", str(file_path)]
    for d in inc_dirs:
        cmd += ["-I", d]

    # Aggiungi Qt include dirs
    qt_inc = Path("/usr/include/x86_64-linux-gnu/qt6")
    if qt_inc.exists():
        cmd += ["-I", str(qt_inc)]
        for subdir in qt_inc.iterdir():
            if subdir.is_dir():
                cmd += ["-I", str(subdir)]

    try:
        result = subprocess.run(
            cmd,
            capture_output=True, text=True, timeout=30,
        )
        if result.returncode == 0:
            return f"✅ {file_rel} — sintassi corretta (controllo base senza Qt MOC)."
        errors = _parse_errors(result.stderr)
        return (
            f"❌ {file_rel} — errori sintassi:\n"
            + "\n".join(errors[:10])
            + "\n\n(Nota: controllo senza MOC/Qt flags completi — usa build() per verifica definitiva)"
        )
    except subprocess.TimeoutExpired:
        return "❌ Timeout controllo sintassi."
    except Exception as e:
        return f"Errore: {e}"


HANDLERS = {
    "build":         handle_build,
    "run_tests":     handle_run_tests,
    "cmake_config":  handle_cmake_config,
    "get_build_log": handle_get_build_log,
    "check_compile": handle_check_compile,
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
                "serverInfo": {"name": "prismalux-build", "version": "1.0.0"},
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
