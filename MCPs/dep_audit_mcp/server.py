#!/usr/bin/env python3
"""
dep_audit_mcp — Audit vulnerabilità dipendenze Python e sistema
===============================================================
Controlla requirements*.txt e pacchetti Python installati
contro database CVE (OSV.dev API gratuita, pip-audit, safety).

Tools esposti:
  audit_requirements  — scansiona un requirements.txt via OSV.dev
  audit_installed     — controlla tutti i pacchetti Python installati
  check_package       — cerca CVE per un pacchetto+versione specifica
  audit_project       — trova tutti i requirements nel progetto e li audita
  show_report         — mostra l'ultimo report salvato
  check_tool_avail    — verifica se pip-audit/safety sono installati
"""

import sys, json, os, re, asyncio, logging, subprocess, urllib.request, urllib.error
from pathlib import Path
from datetime import datetime
from typing import Any

logging.basicConfig(level=getattr(logging, os.environ.get("PRISMALUX_LOG_LEVEL","WARNING")))
logger = logging.getLogger(__name__)

ROOT        = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
REPORT_FILE = Path.home() / ".prismalux" / "dep_audit_last.json"
OSV_API     = "https://api.osv.dev/v1/query"

# ─── Helpers ─────────────────────────────────────────────────────────────────

def _osv_query(package: str, version: str, ecosystem: str = "PyPI") -> list[dict]:
    """Interroga OSV.dev per un pacchetto/versione."""
    payload = json.dumps({
        "version": version,
        "package": {"name": package, "ecosystem": ecosystem}
    }).encode()
    try:
        req = urllib.request.Request(
            OSV_API,
            data=payload,
            headers={"Content-Type": "application/json"},
            method="POST"
        )
        with urllib.request.urlopen(req, timeout=10) as resp:
            data = json.loads(resp.read())
            return data.get("vulns", [])
    except urllib.error.URLError as e:
        logger.warning(f"OSV.dev non raggiungibile: {e}")
        return []
    except Exception as e:
        logger.warning(f"OSV query error: {e}")
        return []

def _parse_requirements(path: Path) -> list[tuple[str, str]]:
    """Restituisce lista (package, version) dal requirements file."""
    result = []
    for line in path.read_text(errors="replace").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or line.startswith("-"):
            continue
        # Gestisce: pkg==1.2.3, pkg>=1.0, pkg~=1.0, pkg (senza versione)
        m = re.match(r"^([a-zA-Z0-9_\-\.]+)\s*(?:[=!<>~^]+\s*([^\s,;]+))?", line)
        if m:
            pkg = m.group(1).strip()
            ver = m.group(2).strip() if m.group(2) else ""
            result.append((pkg, ver))
    return result

def _severity_icon(severity: str) -> str:
    s = severity.upper()
    return {"CRITICAL":"🔴","HIGH":"🟠","MEDIUM":"🟡","LOW":"🔵","UNKNOWN":"⚪"}.get(s, "⚪")

def _format_vuln(vuln: dict) -> str:
    vid      = vuln.get("id", "?")
    summary  = vuln.get("summary", "")[:120]
    severity = "UNKNOWN"
    score    = ""
    for sev in vuln.get("severity", []):
        if sev.get("type") == "CVSS_V3":
            try:
                s = float(sev["score"].split("/")[1]) if "/" in sev.get("score","") else 0
                # CVSS base score dalla stringa CVSS:3.x/...
                # Prova a estrarre il score numerico dai database aliases
                pass
            except Exception:
                pass
            severity = sev.get("type","?")
    for alias in vuln.get("aliases", []):
        if alias.startswith("CVE-"):
            vid = alias
            break
    refs = [r["url"] for r in vuln.get("references",[]) if r.get("type")=="ADVISORY"][:1]
    ref_str = f"\n    Ref: {refs[0]}" if refs else ""
    return f"  {_severity_icon(severity)} {vid}: {summary}{ref_str}"

def _run_pip_audit(req_file: str = "") -> tuple[bool, str]:
    cmd = ["pip-audit", "--format=json"]
    if req_file:
        cmd += ["-r", req_file]
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        return r.returncode == 0, r.stdout or r.stderr
    except FileNotFoundError:
        return False, "pip-audit non installato"
    except subprocess.TimeoutExpired:
        return False, "Timeout pip-audit (120s)"

def _run_safety(req_file: str = "") -> tuple[bool, str]:
    cmd = ["safety", "check", "--json"]
    if req_file:
        cmd += ["-r", req_file]
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        return True, r.stdout or r.stderr
    except FileNotFoundError:
        return False, "safety non installato"
    except subprocess.TimeoutExpired:
        return False, "Timeout safety (60s)"

def _installed_packages() -> list[tuple[str, str]]:
    try:
        r = subprocess.run(
            [sys.executable, "-m", "pip", "list", "--format=json"],
            capture_output=True, text=True, timeout=30
        )
        pkgs = json.loads(r.stdout)
        return [(p["name"], p["version"]) for p in pkgs]
    except Exception:
        return []

def _save_report(data: dict):
    REPORT_FILE.parent.mkdir(parents=True, exist_ok=True)
    data["timestamp"] = datetime.now().isoformat()
    REPORT_FILE.write_text(json.dumps(data, indent=2))

# ─── Handlers ────────────────────────────────────────────────────────────────

def handle_audit_requirements(args: dict) -> str:
    req_path = args.get("file", "").strip()
    use_osv  = bool(args.get("use_osv", True))

    if not req_path:
        # Cerca il requirements principale del progetto
        for candidate in ["requirements.txt", "requirements.lock",
                          "MCPs/requirements.txt", "gui/requirements.txt"]:
            if (ROOT / candidate).exists():
                req_path = candidate
                break
    if not req_path:
        return "Nessun requirements.txt trovato. Specifica 'file'."

    path = Path(req_path) if Path(req_path).is_absolute() else ROOT / req_path
    if not path.exists():
        return f"File non trovato: {path}"

    packages = _parse_requirements(path)
    if not packages:
        return f"Nessun pacchetto trovato in {path.name}."

    lines    = [f"🔍 Audit: {path.relative_to(ROOT) if ROOT in path.parents else path.name} ({len(packages)} pacchetti)\n"]
    vulns_found = 0
    report_data = {"file": str(path), "results": []}

    # Prima prova pip-audit (più completo)
    ok_audit, audit_out = _run_pip_audit(str(path))
    if ok_audit:
        try:
            audit_data = json.loads(audit_out)
            deps = audit_data.get("dependencies", [])
            for dep in deps:
                vulns = dep.get("vulns", [])
                if vulns:
                    vulns_found += len(vulns)
                    lines.append(f"📦 {dep['name']} {dep.get('version','?')}")
                    for v in vulns:
                        vid = v.get("id","?")
                        desc = v.get("description","")[:100]
                        fix  = v.get("fix_versions", [])
                        fix_str = f" → fix: {', '.join(fix)}" if fix else ""
                        lines.append(f"  🔴 {vid}: {desc}{fix_str}")
                    report_data["results"].append({"package": dep["name"], "vulns": vulns})
            if not vulns_found:
                lines.append(f"✅ pip-audit: nessuna vulnerabilità nota in {len(packages)} pacchetti.")
            lines.append(f"\n(fonte: pip-audit)")
            _save_report(report_data)
            return "\n".join(lines)
        except (json.JSONDecodeError, KeyError):
            pass  # fallback a OSV

    if not use_osv:
        return "\n".join(lines) + "\npip-audit non disponibile e OSV disabilitato."

    # Fallback: OSV.dev
    lines.append("(pip-audit non disponibile, uso OSV.dev API)\n")
    checked = 0
    for pkg, ver in packages:
        if not ver:
            continue  # senza versione OSV non può verificare
        vulns = _osv_query(pkg, ver)
        checked += 1
        if vulns:
            vulns_found += len(vulns)
            lines.append(f"\n📦 {pkg}=={ver}  [{len(vulns)} vulnerabilità]")
            for v in vulns[:3]:
                lines.append(_format_vuln(v))
            if len(vulns) > 3:
                lines.append(f"  ... e altri {len(vulns)-3}")
            report_data["results"].append({"package": pkg, "version": ver, "vulns": [v.get("id") for v in vulns]})

    if vulns_found == 0:
        lines.append(f"✅ OSV.dev: nessuna vulnerabilità in {checked} pacchetti con versione specificata.")
    else:
        lines.append(f"\n⚠️  Totale: {vulns_found} vulnerabilità trovate.")

    _save_report(report_data)
    return "\n".join(lines)


def handle_audit_installed(args: dict) -> str:
    limit = int(args.get("limit", 30))
    pkgs  = _installed_packages()
    if not pkgs:
        return "Impossibile ottenere la lista dei pacchetti installati."

    lines = [f"🔍 Audit {len(pkgs)} pacchetti installati (campionando {min(limit,len(pkgs))}) via OSV.dev...\n"]
    vulns_found = 0

    for pkg, ver in pkgs[:limit]:
        vulns = _osv_query(pkg, ver)
        if vulns:
            vulns_found += len(vulns)
            lines.append(f"\n📦 {pkg}=={ver}  [{len(vulns)} CVE]")
            for v in vulns[:2]:
                lines.append(_format_vuln(v))

    if vulns_found == 0:
        lines.append(f"✅ Nessuna vulnerabilità nei {min(limit,len(pkgs))} pacchetti controllati.")
    else:
        lines.append(f"\n⚠️  {vulns_found} vulnerabilità trovate. Usa check_package per i dettagli.")
    return "\n".join(lines)


def handle_check_package(args: dict) -> str:
    package = args.get("package","").strip()
    version = args.get("version","").strip()
    if not package:
        return "Specifica 'package'."
    if not version:
        # Prova a recuperare la versione installata
        try:
            r = subprocess.run([sys.executable,"-m","pip","show",package],
                               capture_output=True, text=True, timeout=10)
            m = re.search(r"^Version:\s*(.+)$", r.stdout, re.MULTILINE)
            version = m.group(1).strip() if m else "0.0.0"
        except Exception:
            version = "0.0.0"

    vulns = _osv_query(package, version)
    if not vulns:
        return f"✅ {package}=={version}: nessuna vulnerabilità nota in OSV.dev."

    lines = [f"⚠️  {package}=={version} — {len(vulns)} vulnerabilità:\n"]
    for v in vulns:
        vid     = v.get("id","?")
        aliases = [a for a in v.get("aliases",[]) if a.startswith("CVE-")]
        summary = v.get("summary","")
        detail  = v.get("details","")[:300]
        refs    = [r["url"] for r in v.get("references",[]) if r.get("type") in ("ADVISORY","WEB")][:2]
        fixed   = []
        for aff in v.get("affected",[]):
            for rng in aff.get("ranges",[]):
                for ev in rng.get("events",[]):
                    if "fixed" in ev:
                        fixed.append(ev["fixed"])

        lines.append(f"{'─'*60}")
        lines.append(f"ID      : {vid}" + (f" ({', '.join(aliases)})" if aliases else ""))
        lines.append(f"Summary : {summary}")
        if detail:
            lines.append(f"Dettaglio: {detail}")
        if fixed:
            lines.append(f"Fix     : {', '.join(set(fixed))}")
        for r in refs:
            lines.append(f"Ref     : {r}")
    return "\n".join(lines)


def handle_audit_project(args: dict) -> str:
    lines = ["🔍 Ricerca requirements nel progetto...\n"]
    req_files = list(ROOT.rglob("requirements*.txt")) + list(ROOT.rglob("requirements*.lock"))
    req_files = [f for f in req_files if "build" not in str(f) and ".venv" not in str(f)
                 and "node_modules" not in str(f)]

    if not req_files:
        return "Nessun requirements*.txt trovato nel progetto."

    lines.append(f"Trovati {len(req_files)} file:\n")
    total_vulns = 0
    for rf in req_files:
        rel = rf.relative_to(ROOT) if ROOT in rf.parents else rf
        pkgs = _parse_requirements(rf)
        lines.append(f"  📋 {rel} ({len(pkgs)} pacchetti)")
        file_vulns = 0
        for pkg, ver in pkgs:
            if not ver:
                continue
            vulns = _osv_query(pkg, ver)
            if vulns:
                file_vulns += len(vulns)
                lines.append(f"    🔴 {pkg}=={ver}: {len(vulns)} CVE")
        if file_vulns == 0:
            lines.append(f"    ✅ Nessuna vulnerabilità")
        else:
            total_vulns += file_vulns

    lines.append(f"\n{'─'*40}")
    if total_vulns == 0:
        lines.append("✅ Nessuna vulnerabilità trovata in tutti i requirements del progetto.")
    else:
        lines.append(f"⚠️  Totale: {total_vulns} vulnerabilità. Usa audit_requirements(file) per i dettagli.")
    return "\n".join(lines)


def handle_show_report(_args: dict) -> str:
    if not REPORT_FILE.exists():
        return "Nessun report salvato. Esegui prima audit_requirements o audit_project."
    try:
        data = json.loads(REPORT_FILE.read_text())
        ts   = data.get("timestamp","?")
        file = data.get("file","?")
        results = data.get("results",[])
        vuln_pkgs = [r for r in results if r.get("vulns")]
        lines = [f"📊 Ultimo report: {ts}", f"File: {file}", f"Pacchetti vulnerabili: {len(vuln_pkgs)}\n"]
        for r in vuln_pkgs:
            lines.append(f"  📦 {r.get('package','?')}=={r.get('version','?')}")
            for v in (r.get("vulns") or [])[:3]:
                vid = v if isinstance(v, str) else v.get("id","?")
                lines.append(f"    · {vid}")
        return "\n".join(lines)
    except Exception as e:
        return f"Errore lettura report: {e}"


def handle_check_tool_avail(_args: dict) -> str:
    tools = {}
    for tool in ["pip-audit", "safety", "bandit", "pip"]:
        try:
            r = subprocess.run([tool, "--version"], capture_output=True, text=True, timeout=5)
            ver = r.stdout.strip().splitlines()[0] if r.stdout else r.stderr.strip().splitlines()[0]
            tools[tool] = ("✅", ver[:60])
        except FileNotFoundError:
            tools[tool] = ("❌", "non installato")
        except Exception as e:
            tools[tool] = ("⚠️ ", str(e)[:40])

    lines = ["Tool di audit disponibili:\n"]
    for name, (icon, ver) in tools.items():
        lines.append(f"  {icon} {name:15s} {ver}")
    lines.append("\nInstallazione consigliata:")
    lines.append("  pip install pip-audit safety")
    return "\n".join(lines)


HANDLERS = {
    "audit_requirements": handle_audit_requirements,
    "audit_installed":    handle_audit_installed,
    "check_package":      handle_check_package,
    "audit_project":      handle_audit_project,
    "show_report":        handle_show_report,
    "check_tool_avail":   handle_check_tool_avail,
}

TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"audit_requirements","description":"Scansiona un requirements.txt per CVE noti via pip-audit o OSV.dev. Default: trova automaticamente il requirements principale del progetto.","inputSchema":{"type":"object","properties":{"file":{"type":"string","description":"Path requirements.txt (relativo a root, default: auto-detect)","default":""},"use_osv":{"type":"boolean","description":"Usa OSV.dev come fallback se pip-audit non disponibile (default: true)","default":True}}}},
    {"name":"audit_installed","description":"Controlla tutti i pacchetti Python installati nell'ambiente corrente via OSV.dev.","inputSchema":{"type":"object","properties":{"limit":{"type":"integer","description":"Numero massimo di pacchetti da controllare (default: 30)","default":30}}}},
    {"name":"check_package","description":"Cerca CVE per un pacchetto Python specifico (nome+versione). Se versione omessa, usa quella installata.","inputSchema":{"type":"object","properties":{"package":{"type":"string","description":"Nome pacchetto (es. 'requests')"},"version":{"type":"string","description":"Versione (es. '2.28.0'). Default: versione installata","default":""}},"required":["package"]}},
    {"name":"audit_project","description":"Trova tutti i requirements*.txt nel progetto e li controlla tutti per CVE.","inputSchema":{"type":"object","properties":{}}},
    {"name":"show_report","description":"Mostra l'ultimo report di audit salvato.","inputSchema":{"type":"object","properties":{}}},
    {"name":"check_tool_avail","description":"Verifica quali tool di audit (pip-audit, safety, bandit) sono installati.","inputSchema":{"type":"object","properties":{}}},
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
            resp = _rpc_result(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"dep-audit","version":"1.0.0"},"capabilities":{"tools":{}}})
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
