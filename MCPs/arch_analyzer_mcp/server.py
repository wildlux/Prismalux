#!/usr/bin/env python3
"""
arch_analyzer_mcp — Analisi architetturale C++ per Prismalux
=============================================================
Analizza il grafo di dipendenze tra file, individua God Class,
accoppiamento, complessità ciclomatica e violazioni convenzioni.

Tools:
  analyze_dependencies  — grafo #include, dipendenze circolari
  find_god_classes      — file troppo grandi (righe + metodi)
  coupling_report       — accoppiamento afferente/efferente
  include_graph         — albero dipendenze di un singolo file
  check_conventions     — naming convention pages/widgets
  complexity_estimate   — complessità ciclomatica stimata per file
"""
import sys, json, os, re, asyncio, logging
from pathlib import Path
from collections import defaultdict, deque
from typing import Any

ROOT    = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
GUI_DIR = ROOT / "gui"
IGNORE  = {"build", "build_gui", "build_asan", ".git", "__pycache__"}

# ─── helpers ─────────────────────────────────────────────────────────────────

def _cpp_files(path: Path) -> list[Path]:
    files = []
    for f in path.rglob("*"):
        if f.suffix in (".cpp", ".h") and not any(p in IGNORE for p in f.parts):
            files.append(f)
    return files

def _includes(path: Path) -> list[str]:
    """Restituisce la lista di file inclusi localmente (non system)."""
    out = []
    try:
        for line in path.read_text(errors="replace").splitlines():
            m = re.match(r'^\s*#include\s+"([^"]+)"', line)
            if m:
                out.append(m.group(1))
    except OSError:
        pass
    return out

def _build_graph(path: Path) -> dict[str, list[str]]:
    """Mappa filename → lista di filename inclusi."""
    graph: dict[str, list[str]] = defaultdict(list)
    files = _cpp_files(path)
    name_map = {f.name: f for f in files}
    for f in files:
        key = f.name
        for inc in _includes(f):
            inc_name = Path(inc).name
            if inc_name in name_map:
                graph[key].append(inc_name)
    return dict(graph)

def _find_cycles(graph: dict[str, list[str]]) -> list[list[str]]:
    """DFS per trovare cicli nel grafo di dipendenze."""
    visited, in_stack = set(), set()
    cycles = []

    def dfs(node, stack):
        visited.add(node)
        in_stack.add(node)
        for nbr in graph.get(node, []):
            if nbr not in visited:
                dfs(nbr, stack + [nbr])
            elif nbr in in_stack:
                idx = (stack + [nbr]).index(nbr)
                cycle = (stack + [nbr])[idx:]
                if cycle not in cycles:
                    cycles.append(cycle)
        in_stack.discard(node)

    for node in graph:
        if node not in visited:
            dfs(node, [node])
    return cycles[:20]

def _count_complexity(text: str) -> int:
    """Complessità ciclomatica approssimata: conta branch points."""
    keywords = re.findall(r'\b(if|else if|for|while|switch|case|catch|\?\s*[^:])', text)
    return 1 + len(keywords)

def _count_methods(text: str) -> int:
    """Conta metodi/funzioni nel file (approssimazione)."""
    return len(re.findall(r'^\s*(?:void|bool|int|QString|QWidget\*|auto|static)\s+\w+\s*\(', text, re.MULTILINE))

SEV = {"CRITICAL": "🔴", "HIGH": "🟠", "MEDIUM": "🟡", "LOW": "🔵"}

# ─── handlers ────────────────────────────────────────────────────────────────

def handle_analyze_dependencies(args: dict) -> str:
    path = Path(args.get("path", str(GUI_DIR)))
    if not path.is_absolute(): path = ROOT / path
    graph = _build_graph(path)
    cycles = _find_cycles(graph)
    total = sum(len(v) for v in graph.values())

    lines = [f"📐 Analisi dipendenze: {path.name} ({len(graph)} file, {total} archi)\n"]
    if not cycles:
        lines.append("✅ Nessuna dipendenza circolare rilevata.")
    else:
        lines.append(f"🔴 {len(cycles)} dipendenze circolari:\n")
        for c in cycles:
            lines.append("  🔴 " + " → ".join(c) + f" → {c[0]}")

    # File più inclusi (più "popolari")
    counts: dict[str, int] = defaultdict(int)
    for deps in graph.values():
        for d in deps: counts[d] += 1
    top = sorted(counts.items(), key=lambda x: -x[1])[:8]
    if top:
        lines.append(f"\n📊 File più inclusi (hub di dipendenza):")
        for name, cnt in top:
            lines.append(f"  {cnt:3d}x  {name}")
    return "\n".join(lines)


def handle_find_god_classes(args: dict) -> str:
    path = Path(args.get("path", str(GUI_DIR)))
    if not path.is_absolute(): path = ROOT / path
    max_lines   = int(args.get("max_lines",   800))
    max_methods = int(args.get("max_methods",  25))

    offenders = []
    for f in _cpp_files(path):
        try:
            text  = f.read_text(errors="replace")
            nlines = len(text.splitlines())
            nmeths = _count_methods(text)
            if nlines > max_lines or nmeths > max_methods:
                try: rel = f.relative_to(ROOT)
                except ValueError: rel = f
                offenders.append((rel, nlines, nmeths))
        except OSError:
            pass

    offenders.sort(key=lambda x: -(x[1] + x[2] * 10))
    lines = [f"🔍 God Class detector (soglie: >{max_lines} righe o >{max_methods} metodi)\n"]
    if not offenders:
        lines.append("✅ Nessun God Class rilevato.")
    else:
        lines.append(f"⚠️  {len(offenders)} file da refactorare:\n")
        for rel, nl, nm in offenders:
            sev = "🔴" if nl > max_lines * 1.5 or nm > max_methods * 2 else "🟠"
            lines.append(f"  {sev} {str(rel):<55}  {nl:5d} righe  {nm:3d} metodi")
        lines.append("\n💡 Regola: una page/widget = una responsabilità. Spezza in sub-widget.")
    return "\n".join(lines)


def handle_coupling_report(args: dict) -> str:
    path = Path(args.get("path", str(GUI_DIR)))
    if not path.is_absolute(): path = ROOT / path
    top_n = int(args.get("top", 15))

    graph = _build_graph(path)
    afferent: dict[str, int] = defaultdict(int)   # chi mi include
    efferent: dict[str, int] = defaultdict(int)   # quanti includo

    for src, deps in graph.items():
        efferent[src] = len(deps)
        for d in deps: afferent[d] += 1

    all_files = set(graph) | set(afferent)
    report = []
    for f in all_files:
        ca = afferent.get(f, 0)
        ce = efferent.get(f, 0)
        instability = ce / (ca + ce) if (ca + ce) > 0 else 0
        report.append((f, ca, ce, instability))

    report.sort(key=lambda x: -(x[1] + x[2]))
    lines = [f"🔗 Accoppiamento (top {top_n} per Ca+Ce)\n",
             f"  {'File':<45} {'Ca':>4} {'Ce':>4} {'Instab':>7}",
             "  " + "─" * 62]
    for name, ca, ce, inst in report[:top_n]:
        flag = "🔴" if inst > 0.8 else ("🟡" if inst > 0.5 else "✅")
        lines.append(f"  {flag} {name:<43} {ca:>4} {ce:>4} {inst:>7.2f}")
    lines.append("\nCa=afferente (chi dipende da me) · Ce=efferente (da chi dipendo)")
    lines.append("Instabilità = Ce/(Ca+Ce): 0=stabile, 1=instabile")
    return "\n".join(lines)


def handle_include_graph(args: dict) -> str:
    filename = args.get("file", "").strip()
    depth    = min(int(args.get("depth", 3)), 5)
    if not filename: return "Specifica 'file'."

    graph = _build_graph(GUI_DIR)
    visited: set[str] = set()
    lines = [f"🌳 Include graph: {filename} (depth={depth})\n"]

    def dfs(node, d, prefix=""):
        if d > depth or node in visited: return
        visited.add(node)
        lines.append(f"{prefix}{node}")
        for dep in graph.get(node, []):
            dfs(dep, d+1, prefix + "  └─ ")

    dfs(Path(filename).name, 0)
    if len(lines) == 2:
        lines.append("  (nessuna dipendenza locale trovata)")
    return "\n".join(lines)


def handle_check_conventions(args: dict) -> str:
    pages_dir  = GUI_DIR / "pages"
    widgets_dir = GUI_DIR / "widgets"
    ALLOWED_PREFIXES = {"main_", "settings_", "widget_", "dialog_", "agenti_",
                        "simulatore_", "matematica_", "ricerca_", "strumenti_",
                        "multimedia_", "programmazione_", "lan_wan_", "pratici_",
                        "pratico_", "security_", "app_controller_"}
    violations = []
    for d in [pages_dir, widgets_dir]:
        if not d.exists(): continue
        for f in d.iterdir():
            if f.suffix not in (".h", ".cpp"): continue
            name = f.stem
            if name.endswith("_page"):
                violations.append((f.relative_to(ROOT), "🔴 CRITICO: suffisso '_page' vietato (REGOLE_IRREMOVIBILI)"))
            elif not any(name.startswith(p) for p in ALLOWED_PREFIXES):
                violations.append((f.relative_to(ROOT), "🟡 Prefisso non riconosciuto — usare main_/settings_/widget_/dialog_"))

    lines = [f"📏 Naming convention check: {pages_dir.name}/ + {widgets_dir.name}/\n"]
    if not violations:
        lines.append("✅ Tutte le convenzioni rispettate.")
    else:
        for path, msg in violations:
            lines.append(f"  {msg}\n     → {path}")
    return "\n".join(lines)


def handle_complexity_estimate(args: dict) -> str:
    path = Path(args.get("path", str(GUI_DIR)))
    if not path.is_absolute(): path = ROOT / path
    threshold = int(args.get("threshold", 15))

    results = []
    for f in _cpp_files(path):
        try:
            text  = f.read_text(errors="replace")
            score = _count_complexity(text)
            if score >= threshold:
                try: rel = f.relative_to(ROOT)
                except ValueError: rel = f
                results.append((rel, score))
        except OSError: pass

    results.sort(key=lambda x: -x[1])
    lines = [f"🧮 Complessità ciclomatica (soglia ≥ {threshold})\n"]
    if not results:
        lines.append(f"✅ Nessun file supera la soglia {threshold}.")
    else:
        for rel, score in results[:20]:
            icon = "🔴" if score > threshold*2 else ("🟠" if score > threshold*1.5 else "🟡")
            lines.append(f"  {icon} {score:5d}  {rel}")
        lines.append("\n💡 Complessità > 30 suggerisce necessità di refactoring in metodi privati.")
    return "\n".join(lines)


HANDLERS = {
    "analyze_dependencies": handle_analyze_dependencies,
    "find_god_classes":     handle_find_god_classes,
    "coupling_report":      handle_coupling_report,
    "include_graph":        handle_include_graph,
    "check_conventions":    handle_check_conventions,
    "complexity_estimate":  handle_complexity_estimate,
}
TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"analyze_dependencies","description":"Analizza il grafo di dipendenze #include tra file C++, trova dipendenze circolari e file hub.","inputSchema":{"type":"object","properties":{"path":{"type":"string","description":"Directory da analizzare (default: gui/)","default":""}}}},
    {"name":"find_god_classes","description":"Trova file C++ troppo grandi (God Class): troppi metodi o troppe righe.","inputSchema":{"type":"object","properties":{"path":{"type":"string","default":""},"max_lines":{"type":"integer","default":800},"max_methods":{"type":"integer","default":25}}}},
    {"name":"coupling_report","description":"Calcola accoppiamento afferente/efferente e indice di instabilità per ogni file C++.","inputSchema":{"type":"object","properties":{"path":{"type":"string","default":""},"top":{"type":"integer","default":15}}}},
    {"name":"include_graph","description":"Mostra l'albero di dipendenze #include di un singolo file C++.","inputSchema":{"type":"object","properties":{"file":{"type":"string"},"depth":{"type":"integer","default":3}},"required":["file"]}},
    {"name":"check_conventions","description":"Verifica le naming convention su gui/pages/ e gui/widgets/ (no *_page, prefissi corretti).","inputSchema":{"type":"object","properties":{}}},
    {"name":"complexity_estimate","description":"Stima la complessità ciclomatica di ogni file C++ (conta branch points: if/for/while/switch).","inputSchema":{"type":"object","properties":{"path":{"type":"string","default":""},"threshold":{"type":"integer","default":15}}}},
]

# ─── JSON-RPC 2.0 ────────────────────────────────────────────────────────────
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
            resp = _ok(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"arch-analyzer","version":"1.0.0"},"capabilities":{"tools":{}}})
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
        else: resp = _err(rid,-32601,f"unknown method: {method}")
        sys.stdout.write(resp+"\n"); sys.stdout.flush()
def main(): asyncio.run(_main())
if __name__ == "__main__": main()
