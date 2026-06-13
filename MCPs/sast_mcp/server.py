#!/usr/bin/env python3
"""
sast_mcp — Static Application Security Testing per Prismalux
=============================================================
Esegue analisi statica di sicurezza sul codice C++ (gui/) e Python (MCPs/)
usando bandit, cppcheck e analisi regex custom.

Tools esposti:
  bandit_scan      — analisi sicurezza Python (bandit) su MCPs/ o file specifico
  cppcheck_scan    — analisi C++ (cppcheck) su gui/ o file specifico
  regex_audit      — pattern security custom: format string, strcpy, gets, system()
  check_includes   — verifica include pericolosi / deprecati in C++
  full_audit       — esegue tutti gli strumenti disponibili e genera report
  check_tool_avail — verifica quali tool SAST sono installati
"""

import sys, json, os, re, asyncio, logging, subprocess
from pathlib import Path
from datetime import datetime
from typing import Any

logging.basicConfig(level=getattr(logging, os.environ.get("PRISMALUX_LOG_LEVEL","WARNING")))
logger = logging.getLogger(__name__)

ROOT        = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
GUI_DIR     = ROOT / "gui"
MCPS_DIR    = ROOT / "MCPs"
REPORT_FILE = Path.home() / ".prismalux" / "sast_last_report.json"

# ─── Pattern SAST custom C++ ─────────────────────────────────────────────────

CPP_PATTERNS: list[tuple[str, re.Pattern, str, str]] = [
    # (nome, regex, severity, descrizione)
    ("gets() usage",
     re.compile(r"\bgets\s*\("),
     "CRITICAL", "gets() non controlla i bound → buffer overflow"),
    ("strcpy/strcat unsafe",
     re.compile(r"\b(strcpy|strcat|sprintf)\s*\("),
     "HIGH", "Usare strncpy/strncat/snprintf con size esplicita"),
    ("system() call",
     re.compile(r"\bsystem\s*\("),
     "HIGH", "system() esegue shell: rischio command injection; usare QProcess"),
    ("printf format string",
     re.compile(r'\bprintf\s*\(\s*(?!")[^)]+\)'),
     "MEDIUM", "printf con variabile come format string → format string attack"),
    ("scanf unsafe",
     re.compile(r"\bscanf\s*\("),
     "MEDIUM", "scanf senza width limit può causare buffer overflow"),
    ("SQL string concat",
     re.compile(r'(?:query|sql)\s*[+=]+\s*.*(?:user|input|param|arg)', re.I),
     "HIGH", "Possibile SQL injection: usare prepared statements"),
    ("hardcoded IP/port",
     re.compile(r'"(?:\d{1,3}\.){3}\d{1,3}(?::\d+)?"'),
     "LOW", "IP hardcodato: usare prismalux_paths.h (P::kOllamaPort, ecc.)"),
    ("TODO security",
     re.compile(r"//\s*TODO.*(?:security|auth|fix|vuln|hack|workaround)", re.I),
     "LOW", "TODO di sicurezza non risolto"),
    ("QProcess with shell=true equivalent",
     re.compile(r'QProcess.*(?:start|execute).*(?:"sh"|"/bin/sh"|"bash"|"cmd")'),
     "MEDIUM", "Shell tramite QProcess: rischio injection se argomenti non sanitizzati"),
    ("rand() for security",
     re.compile(r'\brand\s*\(\s*\)'),
     "MEDIUM", "rand() non è crittograficamente sicuro; usare QRandomGenerator::securelySeeded()"),
    ("MD5/SHA1 usage",
     re.compile(r'\b(?:MD5|SHA1|sha1|md5)\b'),
     "MEDIUM", "MD5/SHA1 deprecati per sicurezza; usare SHA-256+"),
    ("eval/exec in comments",
     re.compile(r'(?:eval|exec)\s*\(', re.I),
     "HIGH", "eval/exec: rischio code injection"),
    ("Uninitialized pointer pattern",
     re.compile(r'\b\w+\s*\*\s*\w+\s*;(?!\s*=|\s*//)'),
     "LOW", "Puntatore non inizializzato: rischio use-after-free se non azzerato"),
]

SEVERITY_ORDER = {"CRITICAL":0,"HIGH":1,"MEDIUM":2,"LOW":3}
SEVERITY_ICON  = {"CRITICAL":"🔴","HIGH":"🟠","MEDIUM":"🟡","LOW":"🔵"}

# ─── Helpers ─────────────────────────────────────────────────────────────────

def _run(cmd: list[str], cwd: Path | None = None, timeout: int = 120) -> tuple[int, str]:
    try:
        r = subprocess.run(
            cmd, capture_output=True, text=True,
            cwd=str(cwd) if cwd else None, timeout=timeout
        )
        return r.returncode, (r.stdout + r.stderr).strip()
    except subprocess.TimeoutExpired:
        return -1, f"Timeout ({timeout}s)"
    except FileNotFoundError:
        return -1, f"{cmd[0]} non trovato"

def _tool_available(name: str) -> bool:
    code, _ = _run([name, "--version"])
    return code != -1

def _fmt_sast_results(findings: list[dict], title: str) -> str:
    if not findings:
        return f"✅ {title}\nNessun problema rilevato."

    findings.sort(key=lambda f: (SEVERITY_ORDER.get(f.get("severity","LOW"),3),
                                  f.get("file",""), f.get("line",0)))

    counts = {}
    for f in findings:
        s = f.get("severity","?")
        counts[s] = counts.get(s,0) + 1

    summary = " | ".join(f"{SEVERITY_ICON.get(s,'·')} {s}: {n}" for s,n in counts.items())
    lines   = [f"⚠️  {title} — {len(findings)} problemi", summary, ""]

    cur_file = None
    for f in findings:
        if f.get("file") != cur_file:
            cur_file = f.get("file","?")
            lines.append(f"\n📄 {cur_file}")
        sev  = f.get("severity","?")
        line = f.get("line","?")
        cat  = f.get("category","?")
        msg  = f.get("message","")[:120]
        ctx  = f.get("context","")[:100]
        icon = SEVERITY_ICON.get(sev,"·")
        lines.append(f"  {icon} L{str(line):4s}  [{cat}] {msg}")
        if ctx:
            lines.append(f"         → {ctx}")
    return "\n".join(lines)

# ─── Handlers ────────────────────────────────────────────────────────────────

def handle_bandit_scan(args: dict) -> str:
    target   = args.get("target","").strip()
    severity = args.get("severity","MEDIUM").upper()

    path = Path(target) if target and Path(target).is_absolute() else (ROOT / target if target else MCPS_DIR)
    if not path.exists():
        return f"Path non trovato: {path}"

    if not _tool_available("bandit"):
        return ("bandit non installato.\nInstalla con:\n"
                "  pip install bandit\n\nHai installato le dipendenze dei MCP?\n"
                "  pip install bandit safety pip-audit")

    sev_map = {"CRITICAL":"HIGH","HIGH":"HIGH","MEDIUM":"MEDIUM","LOW":"LOW"}
    bandit_sev = sev_map.get(severity,"MEDIUM")

    cmd = ["bandit", "-r", str(path), "-f", "json",
           f"-l{bandit_sev[0].lower()}", "--quiet"]
    code, out = _run(cmd, timeout=60)

    try:
        data   = json.loads(out)
        issues = data.get("results",[])
    except json.JSONDecodeError:
        return f"bandit output non parsabile:\n{out[:500]}"

    if not issues:
        return f"✅ bandit: nessun problema in {path.relative_to(ROOT) if ROOT in path.parents else path}"

    findings = []
    for issue in issues:
        findings.append({
            "file":     str(Path(issue.get("filename","?")).relative_to(ROOT) if ROOT in Path(issue.get("filename","?")).parents else issue.get("filename","?")),
            "line":     issue.get("line_number","?"),
            "severity": issue.get("issue_severity","?").upper(),
            "category": issue.get("test_id","?") + " " + issue.get("test_name",""),
            "message":  issue.get("issue_text",""),
            "context":  issue.get("code","").strip()[:80],
        })

    return _fmt_sast_results(findings, f"bandit — {path.name}")


def handle_cppcheck_scan(args: dict) -> str:
    target  = args.get("target","").strip()
    std     = args.get("std","c++17").strip()

    path = Path(target) if target and Path(target).is_absolute() else (ROOT / target if target else GUI_DIR)
    if not path.exists():
        return f"Path non trovato: {path}"

    if not _tool_available("cppcheck"):
        return "cppcheck non installato.\nInstalla con: sudo apt install cppcheck"

    cmd = [
        "cppcheck",
        "--enable=warning,style,performance,portability",
        f"--std={std}",
        "--template={file}:{line}:{severity}:{id}:{message}",
        "--suppress=missingIncludeSystem",
        "--quiet",
        str(path),
    ]
    code, out = _run(cmd, timeout=120)

    if not out.strip():
        return f"✅ cppcheck: nessun problema in {path.relative_to(ROOT) if ROOT in path.parents else path}"

    findings = []
    for line in out.splitlines():
        parts = line.split(":", 4)
        if len(parts) < 5:
            continue
        fpath, lineno, sev, check_id, msg = parts
        # Mappa severity cppcheck → nostra
        sev_map = {"error":"HIGH","warning":"MEDIUM","style":"LOW","performance":"LOW","portability":"LOW"}
        severity = sev_map.get(sev.strip(),"LOW")
        try:
            rel = str(Path(fpath).relative_to(ROOT))
        except ValueError:
            rel = fpath
        findings.append({
            "file":     rel,
            "line":     lineno.strip(),
            "severity": severity,
            "category": check_id.strip(),
            "message":  msg.strip(),
        })

    return _fmt_sast_results(findings, f"cppcheck — {path.name}")


def handle_regex_audit(args: dict) -> str:
    target   = args.get("target","").strip()
    severity = args.get("severity_min","LOW").upper()

    path = Path(target) if target and Path(target).is_absolute() else (ROOT / target if target else GUI_DIR)
    if not path.exists():
        return f"Path non trovato: {path}"

    min_idx = SEVERITY_ORDER.get(severity, 3)
    findings = []

    # Scansiona file .cpp e .h
    cpp_files = list(path.rglob("*.cpp")) + list(path.rglob("*.h")) if path.is_dir() else [path]
    cpp_files = [f for f in cpp_files if "build" not in str(f) and ".git" not in str(f)]

    for fpath in cpp_files:
        try:
            text  = fpath.read_text(errors="replace")
            lines = text.splitlines()
            for lineno, line in enumerate(lines, 1):
                if "// nosec" in line.lower() or "// nolint" in line.lower():
                    continue
                for name, pattern, severity_p, desc in CPP_PATTERNS:
                    if SEVERITY_ORDER.get(severity_p,3) > min_idx:
                        continue
                    if pattern.search(line):
                        try:
                            rel = str(fpath.relative_to(ROOT))
                        except ValueError:
                            rel = str(fpath)
                        findings.append({
                            "file":     rel,
                            "line":     lineno,
                            "severity": severity_p,
                            "category": name,
                            "message":  desc,
                            "context":  line.strip()[:80],
                        })
        except (OSError, PermissionError):
            pass

    return _fmt_sast_results(findings, f"Regex audit C++ — {path.name} (severity ≥ {severity})")


def handle_check_includes(args: dict) -> str:
    target = args.get("target","").strip()
    path   = Path(target) if target and Path(target).is_absolute() else (ROOT / target if target else GUI_DIR)

    # Include pericolosi o deprecati
    dangerous = {
        "<cstring>":       ("LOW",    "Preferire std::string e QByteArray"),
        "<string.h>":      ("MEDIUM", "Header C puro: usare <cstring> o Qt equivalenti"),
        "<stdio.h>":       ("LOW",    "Preferire <iostream> o QTextStream"),
        "<stdlib.h>":      ("LOW",    "Preferire equivalenti C++ o Qt"),
        "<unistd.h>":      ("LOW",    "Dipendente da POSIX, non portabile su Windows"),
        "<windows.h>":     ("LOW",    "Limita la portabilità cross-platform"),
        "<alloca.h>":      ("MEDIUM", "alloca() può causare stack overflow"),
        "openssl/md5.h":   ("HIGH",   "MD5 deprecato per sicurezza"),
        "openssl/sha.h":   ("MEDIUM", "SHA1 deprecato, assicurarsi di usare SHA-256+"),
    }

    findings = []
    cpp_files = list(path.rglob("*.h")) + list(path.rglob("*.cpp")) if path.is_dir() else [path]
    cpp_files = [f for f in cpp_files if "build" not in str(f)]

    for fpath in cpp_files:
        try:
            text = fpath.read_text(errors="replace")
            for lineno, line in enumerate(text.splitlines(), 1):
                m = re.match(r'\s*#include\s*[<"]([^>"]+)[>"]', line)
                if m:
                    inc = "<" + m.group(1) + ">"
                    if inc in dangerous:
                        sev, desc = dangerous[inc]
                        try: rel = str(fpath.relative_to(ROOT))
                        except ValueError: rel = str(fpath)
                        findings.append({
                            "file":     rel,
                            "line":     lineno,
                            "severity": sev,
                            "category": f"include: {inc}",
                            "message":  desc,
                        })
        except (OSError, PermissionError):
            pass

    return _fmt_sast_results(findings, f"Include audit — {path.name}")


def handle_full_audit(args: dict) -> str:
    lines   = [f"🔐 SAST Full Audit — {datetime.now().strftime('%Y-%m-%d %H:%M')}\n"]
    report  = {"timestamp": datetime.now().isoformat(), "sections": {}}

    sections = [
        ("bandit (Python MCPs/)", lambda: handle_bandit_scan({"target":"MCPs","severity":"MEDIUM"})),
        ("cppcheck (C++ gui/)",    lambda: handle_cppcheck_scan({"target":"gui"})),
        ("regex audit C++ gui/",   lambda: handle_regex_audit({"target":"gui","severity_min":"MEDIUM"})),
        ("include audit gui/",     lambda: handle_check_includes({"target":"gui"})),
    ]

    for name, fn in sections:
        lines.append(f"\n{'='*50}")
        lines.append(f"  {name}")
        lines.append(f"{'='*50}")
        try:
            result = fn()
            lines.append(result)
            report["sections"][name] = result[:500]
        except Exception as e:
            lines.append(f"Errore: {e}")

    REPORT_FILE.parent.mkdir(parents=True, exist_ok=True)
    REPORT_FILE.write_text(json.dumps(report, indent=2))
    lines.append(f"\n\n📊 Report salvato in: {REPORT_FILE}")
    return "\n".join(lines)


def handle_check_tool_avail(_args: dict) -> str:
    tools = {
        "bandit":    "pip install bandit",
        "cppcheck":  "sudo apt install cppcheck",
        "flake8":    "pip install flake8",
        "mypy":      "pip install mypy",
        "semgrep":   "pip install semgrep",
    }
    lines = ["Tool SAST disponibili:\n"]
    for tool, install in tools.items():
        avail = _tool_available(tool)
        icon  = "✅" if avail else "❌"
        lines.append(f"  {icon} {tool:15s} {'(disponibile)' if avail else '→ '+install}")
    lines.append("\nConsigliato minimo: bandit + cppcheck")
    return "\n".join(lines)


HANDLERS = {
    "bandit_scan":      handle_bandit_scan,
    "cppcheck_scan":    handle_cppcheck_scan,
    "regex_audit":      handle_regex_audit,
    "check_includes":   handle_check_includes,
    "full_audit":       handle_full_audit,
    "check_tool_avail": handle_check_tool_avail,
}

TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"bandit_scan","description":"Esegue bandit (Python SAST) su MCPs/ o un file specifico. Riporta vulnerability per severity.","inputSchema":{"type":"object","properties":{"target":{"type":"string","description":"Path da analizzare (default: MCPs/)","default":""},"severity":{"type":"string","description":"Severity minima: CRITICAL|HIGH|MEDIUM|LOW (default: MEDIUM)","default":"MEDIUM"}}}},
    {"name":"cppcheck_scan","description":"Esegue cppcheck su gui/ (C++) o un file specifico. Rileva buffer overflow, use-after-free, code style.","inputSchema":{"type":"object","properties":{"target":{"type":"string","description":"Path da analizzare (default: gui/)","default":""},"std":{"type":"string","description":"Standard C++: c++17|c++20|c++14 (default: c++17)","default":"c++17"}}}},
    {"name":"regex_audit","description":"Analisi regex custom sul codice C++: gets, strcpy, system(), format string, MD5, rand() insicuro, ecc.","inputSchema":{"type":"object","properties":{"target":{"type":"string","description":"Path da analizzare (default: gui/)","default":""},"severity_min":{"type":"string","description":"Severity minima: CRITICAL|HIGH|MEDIUM|LOW (default: LOW)","default":"LOW"}}}},
    {"name":"check_includes","description":"Verifica include C++ pericolosi o deprecati (stdio.h, string.h, openssl/md5.h, ecc.).","inputSchema":{"type":"object","properties":{"target":{"type":"string","description":"Path da analizzare (default: gui/)","default":""}}}},
    {"name":"full_audit","description":"Esegue tutti gli strumenti SAST disponibili (bandit + cppcheck + regex + include) e salva un report.","inputSchema":{"type":"object","properties":{}}},
    {"name":"check_tool_avail","description":"Verifica quali tool SAST sono installati (bandit, cppcheck, flake8, mypy, semgrep).","inputSchema":{"type":"object","properties":{}}},
]

# ─── JSON-RPC 2.0 loop ───────────────────────────────────────────────────────

def _rpc_result(rid, result): return json.dumps({"jsonrpc":"2.0","id":rid,"result":result})
def _rpc_error(rid, code, msg): return json.dumps({"jsonrpc":"2.0","id":rid,"error":{"code":code,"message":msg}})

async def _main_async():
    loop = asyncio.get_event_loop()
    async def readline(): return await loop.run_in_executor(None, sys.stdin.readline)
    while True:
        line = await readline()
        if not line: break
        line = line.strip()
        if not line: continue
        try: req = json.loads(line)
        except json.JSONDecodeError as e:
            sys.stdout.write(_rpc_error(None,-32700,str(e))+"\n"); sys.stdout.flush(); continue
        rid = req.get("id"); method = req.get("method",""); params = req.get("params",{})
        if method == "initialize":
            resp = _rpc_result(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"sast","version":"1.0.0"},"capabilities":{"tools":{}}})
        elif method == "tools/list":
            resp = _rpc_result(rid,{"tools":TOOL_DEFS})
        elif method == "tools/call":
            name = params.get("name",""); targs = params.get("arguments",{})
            h = HANDLERS.get(name)
            if not h: resp = _rpc_error(rid,-32601,f"Tool sconosciuto: {name}")
            else:
                try:
                    text = await asyncio.to_thread(h, targs)
                    resp = _rpc_result(rid,{"content":[{"type":"text","text":text}],"isError":False})
                except Exception as e:
                    resp = _rpc_result(rid,{"content":[{"type":"text","text":f"Errore: {e}"}],"isError":True})
        elif method == "notifications/initialized": continue
        else: resp = _rpc_error(rid,-32601,f"Metodo sconosciuto: {method}")
        sys.stdout.write(resp+"\n"); sys.stdout.flush()

def main(): asyncio.run(_main_async())
if __name__ == "__main__": main()
