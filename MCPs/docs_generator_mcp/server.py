#!/usr/bin/env python3
"""
docs_generator_mcp — Documentazione automatica C++/Python
==========================================================
Genera HTML/PDF con Doxygen, verifica copertura commenti,
crea Doxyfile ottimizzato per Prismalux, trova API non documentate.

Tools:
  check_doxygen       — verifica installazione e versione
  generate_doxyfile   — crea Doxyfile con config Prismalux
  run_doxygen         — genera documentazione HTML
  check_coverage      — % funzioni pubbliche con commento Doxygen
  find_undocumented   — lista funzioni/classi senza commento
  open_docs           — apre la documentazione nel browser
"""
import sys, json, os, re, asyncio, subprocess
from pathlib import Path
from typing import Any

ROOT    = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
GUI_DIR = ROOT / "gui"
DOCS_OUT = ROOT / "docs" / "html"
DOXYFILE = ROOT / "docs" / "Doxyfile"

def _run(cmd: list[str], timeout: int = 120, cwd: Path = ROOT) -> tuple[int, str]:
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, cwd=str(cwd))
        return r.returncode, (r.stdout + r.stderr).strip()
    except (FileNotFoundError, subprocess.TimeoutExpired) as e:
        return -1, str(e)

def _cpp_files(path: Path) -> list[Path]:
    IGNORE = {"build", "build_gui", "build_asan", "build_dbg", "build_debug",
               "build_tests", ".git", "__pycache__"}
    return [f for f in path.rglob("*.h")
            if not any(p in IGNORE for p in f.parts)]

DOXYFILE_TEMPLATE = """\
# Doxyfile per Prismalux — auto-generato da docs_generator_mcp
PROJECT_NAME           = "Prismalux"
PROJECT_NUMBER         = "v2.9"
PROJECT_BRIEF          = "Desktop AI platform Qt6"
OUTPUT_DIRECTORY       = {docs_dir}
INPUT                  = {gui_dir}
RECURSIVE              = YES
EXCLUDE_PATTERNS       = */build*/* */tests/* */.git/*
FILE_PATTERNS          = *.h *.cpp
EXTRACT_ALL            = YES
EXTRACT_PRIVATE        = NO
EXTRACT_STATIC         = YES
GENERATE_HTML          = YES
HTML_OUTPUT            = html
HTML_COLORSTYLE        = DARK
GENERATE_LATEX         = NO
GENERATE_TREEVIEW      = YES
HAVE_DOT               = {have_dot}
CALL_GRAPH             = {have_dot}
CALLER_GRAPH           = {have_dot}
CLASS_DIAGRAMS         = YES
UML_LOOK               = YES
DOT_IMAGE_FORMAT       = svg
QUIET                  = YES
WARNINGS               = YES
WARN_IF_UNDOCUMENTED   = YES
WARN_NO_PARAMDOC       = YES
JAVADOC_AUTOBRIEF      = YES
QT_AUTOBRIEF           = YES
ENABLE_PREPROCESSING   = YES
MACRO_EXPANSION        = YES
PREDEFINED             = Q_OBJECT Q_SIGNALS Q_SLOTS Q_EMIT
"""

# ─── handlers ────────────────────────────────────────────────────────────────

def handle_check_doxygen(args: dict) -> str:
    c, v = _run(["doxygen", "--version"])
    if c != 0:
        return ("❌ Doxygen non installato.\n"
                "  sudo apt install doxygen doxygen-doc graphviz")
    lines = [f"✅ Doxygen {v.strip()}\n"]
    cd, _ = _run(["dot", "-V"])
    lines.append(f"  {'✅' if cd==0 else '⚠️ '} Graphviz (dot): {'disponibile — grafi abilitati' if cd==0 else 'non installato (sudo apt install graphviz)'}")
    lines.append(f"  Output previsto: {DOCS_OUT}")
    return "\n".join(lines)

def handle_generate_doxyfile(args: dict) -> str:
    force = bool(args.get("force", False))
    if DOXYFILE.exists() and not force:
        return (f"Doxyfile già presente: {DOXYFILE}\n"
                f"Passa force=true per sovrascriverlo.")
    DOXYFILE.parent.mkdir(parents=True, exist_ok=True)
    cd, _ = _run(["dot", "-V"])
    have_dot = "YES" if cd == 0 else "NO"
    content = DOXYFILE_TEMPLATE.format(
        docs_dir=str(ROOT / "docs"),
        gui_dir=str(GUI_DIR),
        have_dot=have_dot,
    )
    DOXYFILE.write_text(content)
    return (f"✅ Doxyfile creato: {DOXYFILE}\n"
            f"  Graphviz: {have_dot}\n"
            f"  Output: {DOCS_OUT}\n\n"
            f"Genera la documentazione con: run_doxygen()")

def handle_run_doxygen(args: dict) -> str:
    doxyfile = args.get("doxyfile", str(DOXYFILE))
    df       = Path(doxyfile) if Path(doxyfile).is_absolute() else ROOT / doxyfile
    if not df.exists():
        return (f"Doxyfile non trovato: {df}\n"
                f"Crea prima con generate_doxyfile()")
    c, out = _run(["doxygen", str(df)], timeout=180)
    warnings = [l for l in out.splitlines() if "warning" in l.lower()]
    errors   = [l for l in out.splitlines() if "error" in l.lower()]
    icon = "✅" if c == 0 else "❌"
    lines = [f"{icon} Doxygen completato (exit={c})\n"]
    lines.append(f"  ⚠️ Warning: {len(warnings)}")
    lines.append(f"  ❌ Errori:  {len(errors)}")
    if warnings[:5]:
        lines.append("\nPrimi warning:")
        for w in warnings[:5]: lines.append(f"  {w}")
    if c == 0:
        index = DOCS_OUT / "index.html"
        lines.append(f"\n  📄 Documentazione: {index}")
        lines.append("  Apri con: open_docs()")
    return "\n".join(lines)

def handle_check_coverage(args: dict) -> str:
    path      = Path(args.get("path", str(GUI_DIR)))
    if not path.is_absolute(): path = ROOT / path
    min_pct   = float(args.get("min_pct", 50.0))

    DOXY_COMMENT = re.compile(r'/\*\*|///')
    PUBLIC_METHOD = re.compile(
        r'^\s*(?:virtual\s+)?(?:static\s+)?'
        r'(?:Q_INVOKABLE\s+)?'
        r'(?:void|bool|int|QString|QStringList|double|float|qint\d+|auto|QWidget\*)\s+'
        r'(\w+)\s*\('
    )

    files_data = []
    for f in _cpp_files(path):
        try:
            lines_text = f.read_text(errors="replace").splitlines()
            in_public  = False
            methods, documented = 0, 0
            for i, line in enumerate(lines_text):
                if "public:" in line:  in_public = True
                if "private:" in line or "protected:" in line: in_public = False
                if in_public and PUBLIC_METHOD.match(line):
                    methods += 1
                    prev = "\n".join(lines_text[max(0,i-3):i])
                    if DOXY_COMMENT.search(prev):
                        documented += 1
            if methods > 0:
                try: rel = f.relative_to(ROOT)
                except: rel = f
                pct = documented / methods * 100
                files_data.append((rel, methods, documented, pct))
        except OSError: pass

    files_data.sort(key=lambda x: x[3])
    lines = [f"📚 Copertura Doxygen (metodi pubblici)\n"]
    poor = [f for f in files_data if f[3] < min_pct]
    good = [f for f in files_data if f[3] >= min_pct]
    total_m = sum(f[1] for f in files_data)
    total_d = sum(f[2] for f in files_data)
    overall = total_d / max(total_m, 1) * 100
    lines.append(f"  Totale: {total_d}/{total_m} metodi documentati ({overall:.1f}%)\n")
    if poor:
        lines.append(f"  🔴 File sotto {min_pct:.0f}%:")
        for rel, m, d, pct in poor[:15]:
            lines.append(f"    {pct:5.0f}%  {d:3}/{m:3}  {rel}")
    if good:
        lines.append(f"\n  ✅ File sopra {min_pct:.0f}%: {len(good)}")
    return "\n".join(lines)

def handle_find_undocumented(args: dict) -> str:
    path  = Path(args.get("path", str(GUI_DIR)))
    limit = int(args.get("limit", 40))
    if not path.is_absolute(): path = ROOT / path

    PUBLIC_DECL = re.compile(
        r'^\s*(?:virtual\s+|static\s+|explicit\s+)?'
        r'(?:Q_INVOKABLE\s+)?'
        r'(?:void|bool|int|QString|QStringList|double|float|auto|QWidget\*|explicit)\s+'
        r'(\w+)\s*\('
    )
    DOXY_COMMENT = re.compile(r'/\*\*|///')
    results = []
    for f in _cpp_files(path):
        if not f.name.endswith(".h"): continue
        try:
            lines_text = f.read_text(errors="replace").splitlines()
            in_public  = False
            for i, line in enumerate(lines_text):
                if "public:" in line:   in_public = True
                if "private:" in line or "protected:" in line: in_public = False
                if not in_public: continue
                m = PUBLIC_DECL.match(line)
                if not m: continue
                func = m.group(1)
                prev = "\n".join(lines_text[max(0,i-2):i])
                if not DOXY_COMMENT.search(prev):
                    try: rel = f.relative_to(ROOT)
                    except: rel = f
                    results.append(f"  {rel}:{i+1}  {func}()")
        except OSError: pass

    lines = [f"📋 Funzioni pubbliche senza commento Doxygen ({len(results)} trovate)\n"]
    for r in results[:limit]: lines.append(r)
    if len(results) > limit:
        lines.append(f"  … e altre {len(results)-limit}")
    if not results:
        lines.append("✅ Tutte le funzioni pubbliche hanno un commento!")
    lines.append("\nFormato Doxygen:\n"
                 "  /// @brief Descrizione breve\n"
                 "  /// @param name Descrizione parametro\n"
                 "  /// @return Valore ritornato")
    return "\n".join(lines)

def handle_open_docs(args: dict) -> str:
    index = DOCS_OUT / "index.html"
    if not index.exists():
        return (f"Documentazione non trovata: {index}\n"
                f"Genera prima con run_doxygen()")
    c, _ = _run(["xdg-open", str(index)], timeout=5)
    if c == 0:
        return f"✅ Aperto: {index}"
    return f"💡 Apri manualmente: file://{index}"

HANDLERS = {
    "check_doxygen":    handle_check_doxygen,
    "generate_doxyfile":handle_generate_doxyfile,
    "run_doxygen":      handle_run_doxygen,
    "check_coverage":   handle_check_coverage,
    "find_undocumented":handle_find_undocumented,
    "open_docs":        handle_open_docs,
}
TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"check_doxygen","description":"Verifica installazione Doxygen e Graphviz.","inputSchema":{"type":"object","properties":{}}},
    {"name":"generate_doxyfile","description":"Crea il file Doxyfile ottimizzato per Prismalux in docs/Doxyfile.","inputSchema":{"type":"object","properties":{"force":{"type":"boolean","default":False}}}},
    {"name":"run_doxygen","description":"Genera la documentazione HTML da Doxygen.","inputSchema":{"type":"object","properties":{"doxyfile":{"type":"string","default":""}}}},
    {"name":"check_coverage","description":"Calcola la percentuale di metodi pubblici C++ con commento Doxygen.","inputSchema":{"type":"object","properties":{"path":{"type":"string","default":""},"min_pct":{"type":"number","default":50}}}},
    {"name":"find_undocumented","description":"Elenca le funzioni pubbliche C++ prive di commento Doxygen (/// o /**)","inputSchema":{"type":"object","properties":{"path":{"type":"string","default":""},"limit":{"type":"integer","default":40}}}},
    {"name":"open_docs","description":"Apre la documentazione generata nel browser di sistema.","inputSchema":{"type":"object","properties":{}}},
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
            resp = _ok(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"docs-generator","version":"1.0.0"},"capabilities":{"tools":{}}})
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
