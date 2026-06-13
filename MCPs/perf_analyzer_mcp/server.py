#!/usr/bin/env python3
"""
perf_analyzer_mcp — Analisi performance e memory leak C++/Python
=================================================================
Wrapper per valgrind, perf, callgrind e cProfile con output
già parsato e formattato per identificare hotspot e leak.

Tools:
  valgrind_memcheck  — rileva memory leak e accessi invalidi
  callgrind_profile  — CPU profiling con hotspot per funzione
  perf_stat          — hardware counters (cache miss, branch)
  python_profile     — cProfile + memory_profiler per script Python
  compare_builds     — confronta tempo di build Release vs Debug
  check_binary_size  — analizza dimensione sezioni del binario
"""
import sys, json, os, re, asyncio, subprocess
from pathlib import Path
from typing import Any

ROOT     = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
BUILD    = ROOT / "gui" / "build_gui"
BINARY   = BUILD / "Prismalux_GUI"
ANDROID_BIN = ROOT / "ANDROID" / "android_app" / "build-desktop" / "PrismaluxMobile"

def _run(cmd: list[str], timeout: int = 120, cwd: Path | None = None) -> tuple[int, str]:
    try:
        r = subprocess.run(cmd, capture_output=True, text=True,
                           timeout=timeout, cwd=str(cwd) if cwd else None)
        return r.returncode, (r.stdout + r.stderr).strip()
    except subprocess.TimeoutExpired:
        return -1, f"Timeout ({timeout}s) — il programma è ancora in esecuzione?"
    except FileNotFoundError:
        return -1, f"{cmd[0]} non trovato — installa: sudo apt install {cmd[0]}"

def _tool_ok(name: str) -> bool:
    c, _ = _run([name, "--version"])
    return c != -1

def _find_binary(binary_arg: str) -> Path | None:
    if binary_arg:
        p = Path(binary_arg) if Path(binary_arg).is_absolute() else ROOT / binary_arg
        return p if p.exists() else None
    for candidate in [BINARY, ANDROID_BIN]:
        if candidate.exists(): return candidate
    return None

# ─── handlers ────────────────────────────────────────────────────────────────

def handle_valgrind_memcheck(args: dict) -> str:
    binary = _find_binary(args.get("binary",""))
    vargs  = args.get("args", [])
    timeout = int(args.get("timeout_s", 60))

    if not _tool_ok("valgrind"):
        return "valgrind non installato.\n  sudo apt install valgrind"
    if not binary:
        return f"Binario non trovato. Compila prima con build('desktop').\nCercato: {BINARY}"

    cmd = [
        "valgrind",
        "--tool=memcheck",
        "--leak-check=full",
        "--show-leak-kinds=all",
        "--track-origins=yes",
        "--error-exitcode=1",
        "--xml=no",
        str(binary),
    ] + (vargs if isinstance(vargs, list) else vargs.split())

    code, out = _run(cmd, timeout=timeout)
    # Parsing output valgrind
    lines   = [f"🔍 valgrind memcheck — {binary.name}\n"]
    summary = re.search(r"LEAK SUMMARY:(.*?)(?=ERROR SUMMARY|$)", out, re.DOTALL)
    errors  = re.search(r"ERROR SUMMARY:\s*(\d+) errors", out)

    if errors:
        n = int(errors.group(1))
        lines.append(f"{'✅' if n==0 else '🔴'} {n} errori totali\n")

    if summary:
        for line in summary.group(1).splitlines():
            line = line.strip()
            if line and "bytes" in line:
                icon = "🔴" if "definitely lost" in line else ("🟡" if "possibly lost" in line else "🔵")
                lines.append(f"  {icon} {line}")

    # Estrai i leak più grandi
    big_leaks = re.findall(r"(\d+) bytes in \d+ blocks.*?\n.*?at .+\n((?:.*\n){0,5})", out)
    if big_leaks:
        lines.append("\nTop leak stacks:")
        for size, stack in big_leaks[:3]:
            lines.append(f"\n  {size} bytes:")
            for sl in stack.splitlines()[:4]:
                if sl.strip():
                    lines.append(f"    {sl.strip()}")

    if not summary and not errors:
        lines.append(out[:1500])
    return "\n".join(lines)


def handle_callgrind_profile(args: dict) -> str:
    binary  = _find_binary(args.get("binary",""))
    timeout = int(args.get("timeout_s", 30))

    if not _tool_ok("valgrind"):
        return "valgrind non installato.\n  sudo apt install valgrind"
    if not binary:
        return f"Binario non trovato: {BINARY}"

    out_file = Path("/tmp/callgrind.out.prismalux")
    cmd = ["valgrind", "--tool=callgrind",
           f"--callgrind-out-file={out_file}",
           "--collect-atstart=yes",
           str(binary), "--help"]  # --help per uscire subito

    _run(cmd, timeout=timeout)

    if not out_file.exists():
        return "callgrind non ha prodotto output. Il programma è uscito troppo velocemente?"

    # Usa callgrind_annotate se disponibile
    code, annotated = _run(["callgrind_annotate", "--auto=yes", str(out_file)], timeout=10)
    if code == 0 and annotated:
        # Estrai top funzioni
        lines = ["📊 callgrind — hotspot CPU (top 15 funzioni)\n"]
        func_lines = [l for l in annotated.splitlines()
                      if re.match(r'\s*\d[\d,]+\s+\d+\.\d+%', l)]
        for fl in func_lines[:15]:
            lines.append("  " + fl.strip())
        return "\n".join(lines)

    return f"callgrind output: {out_file}\nInstalla kcachegrind per visualizzarlo graficamente:\n  sudo apt install kcachegrind"


def handle_perf_stat(args: dict) -> str:
    binary  = _find_binary(args.get("binary",""))
    timeout = int(args.get("timeout_s", 15))

    if not _tool_ok("perf"):
        return ("perf non installato o non autorizzato.\n"
                "  sudo apt install linux-perf\n"
                "  sudo sysctl kernel.perf_event_paranoid=1")
    if not binary:
        return f"Binario non trovato: {BINARY}"

    cmd = ["perf", "stat", "-e",
           "cache-references,cache-misses,branch-instructions,branch-misses,instructions,cycles",
           str(binary), "--help"]
    code, out = _run(cmd, timeout=timeout)

    lines = [f"⚡ perf stat — {binary.name}\n"]
    metrics = {
        "cache-misses":       ("Cache miss rate", "🔴 > 10% → ottimizza accesso dati"),
        "branch-misses":      ("Branch misprediction", "🟡 > 5% → valuta branch-free"),
        "instructions":       ("Istruzioni totali", "info"),
        "cycles":             ("Cicli CPU", "info"),
    }
    for line in out.splitlines():
        for key, (label, note) in metrics.items():
            if key in line and "#" in line:
                pct = re.search(r'#\s*([\d\.]+\s*%)', line)
                val = re.search(r'^\s*([\d,\.]+)', line)
                if val:
                    pct_str = f"  {pct.group(1)}" if pct else ""
                    lines.append(f"  {label}: {val.group(1).replace(',','_')}{pct_str}")
                    if "🔴" in note or "🟡" in note:
                        lines.append(f"    → {note}")
    if len(lines) == 2:
        lines.append(out[:800])
    return "\n".join(lines)


def handle_python_profile(args: dict) -> str:
    script  = args.get("script","").strip()
    script_args = args.get("args", [])
    timeout = int(args.get("timeout_s", 30))

    if not script: return "Specifica 'script' (path del file .py)."
    path = Path(script) if Path(script).is_absolute() else ROOT / script
    if not path.exists(): return f"Script non trovato: {path}"

    # cProfile
    cmd = [sys.executable, "-m", "cProfile", "-s", "cumulative",
           str(path)] + (script_args if isinstance(script_args, list) else script_args.split())
    code, out = _run(cmd, timeout=timeout)

    lines = [f"🐍 cProfile — {path.name}\n"]
    profile_lines = [l for l in out.splitlines()
                     if re.match(r'\s+\d+\s+\d+\.\d+', l)]
    if profile_lines:
        lines.append(f"  {'ncalls':>8}  {'tottime':>8}  {'cumtime':>8}  function")
        for pl in profile_lines[:20]:
            lines.append("  " + pl.strip())
    else:
        lines.append(out[:600])

    # memory_profiler se disponibile
    try:
        import memory_profiler  # noqa: F401
        cmd2 = [sys.executable, "-m", "memory_profiler", str(path)]
        c2, out2 = _run(cmd2, timeout=timeout)
        if c2 == 0:
            lines.append("\n💾 memory_profiler:")
            for l in out2.splitlines()[:15]:
                if "MiB" in l: lines.append("  " + l.strip())
    except ImportError:
        lines.append("\n💡 Installa memory_profiler per l'analisi memoria: pip install memory-profiler")
    return "\n".join(lines)


def handle_compare_builds(args: dict) -> str:
    import time
    results = []
    configs = [("Release", ["-DCMAKE_BUILD_TYPE=Release"]),
               ("Debug",   ["-DCMAKE_BUILD_TYPE=Debug", "-DBUILD_TESTS=ON"])]

    lines = ["⏱  Confronto build Release vs Debug\n"]
    for name, cmake_flags in configs:
        build_dir = Path(f"/tmp/prismalux_bench_{name.lower()}")
        cmake_cmd = ["cmake", "-B", str(build_dir), str(ROOT/"gui")] + cmake_flags + ["-G","Ninja"]
        _run(cmake_cmd, timeout=60)
        t0 = time.time()
        code, out = _run(["cmake","--build",str(build_dir),"-j4","--clean-first"], timeout=300)
        elapsed = time.time() - t0
        status = "✅" if code == 0 else "❌"
        size   = sum(f.stat().st_size for f in build_dir.rglob("Prismalux*") if f.is_file())
        lines.append(f"  {status} {name:8s} — {elapsed:.0f}s  ({size/1_048_576:.0f} MB output)")
        results.append((name, elapsed, code))

    if len(results) == 2 and all(r[2]==0 for r in results):
        ratio = results[1][1] / max(results[0][1], 1)
        lines.append(f"\n  Debug è {ratio:.1f}× più lento di Release")
    return "\n".join(lines)


def handle_check_binary_size(args: dict) -> str:
    binary = _find_binary(args.get("binary",""))
    if not binary: return f"Binario non trovato: {BINARY}"

    lines = [f"📦 Dimensione binario — {binary.name} ({binary.stat().st_size/1_048_576:.1f} MB)\n"]

    # size: sezioni ELF
    code, out = _run(["size", str(binary)])
    if code == 0:
        lines.append("Sezioni ELF:")
        for l in out.splitlines():
            lines.append("  " + l)

    # Symbols più grandi
    code2, out2 = _run(["nm", "--size-sort", "-S", str(binary)])
    if code2 == 0:
        syms = [l for l in out2.splitlines() if l.strip()][-20:]
        lines.append("\nSymbol più grandi (top 20):")
        for s in reversed(syms):
            parts = s.split()
            if len(parts) >= 4:
                size_hex = parts[1]
                name     = parts[-1]
                try: size_b = int(size_hex, 16)
                except: continue
                lines.append(f"  {size_b:8d} B  {name[:80]}")

    lines.append("\n💡 Ridurre dimensione: -Os, link-time optimization (-flto), strip debug symbols")
    return "\n".join(lines)


HANDLERS = {
    "valgrind_memcheck": handle_valgrind_memcheck,
    "callgrind_profile": handle_callgrind_profile,
    "perf_stat":         handle_perf_stat,
    "python_profile":    handle_python_profile,
    "compare_builds":    handle_compare_builds,
    "check_binary_size": handle_check_binary_size,
}
TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"valgrind_memcheck","description":"Esegue valgrind --tool=memcheck sul binario Prismalux per rilevare memory leak e accessi invalidi.","inputSchema":{"type":"object","properties":{"binary":{"type":"string","default":""},"args":{"type":"array","items":{"type":"string"},"default":[]},"timeout_s":{"type":"integer","default":60}}}},
    {"name":"callgrind_profile","description":"CPU profiling con valgrind callgrind: mostra le funzioni che consumano più tempo.","inputSchema":{"type":"object","properties":{"binary":{"type":"string","default":""},"timeout_s":{"type":"integer","default":30}}}},
    {"name":"perf_stat","description":"Hardware performance counters con 'perf stat': cache miss, branch misprediction, IPC.","inputSchema":{"type":"object","properties":{"binary":{"type":"string","default":""},"timeout_s":{"type":"integer","default":15}}}},
    {"name":"python_profile","description":"cProfile + memory_profiler su uno script Python: hotspot CPU e picco memoria.","inputSchema":{"type":"object","properties":{"script":{"type":"string"},"args":{"type":"array","items":{"type":"string"},"default":[]},"timeout_s":{"type":"integer","default":30}},"required":["script"]}},
    {"name":"compare_builds","description":"Confronta tempo di compilazione e dimensione output tra Release e Debug.","inputSchema":{"type":"object","properties":{}}},
    {"name":"check_binary_size","description":"Analizza le sezioni ELF e i symbol più grandi del binario compilato.","inputSchema":{"type":"object","properties":{"binary":{"type":"string","default":""}}}},
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
            resp = _ok(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"perf-analyzer","version":"1.0.0"},"capabilities":{"tools":{}}})
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
