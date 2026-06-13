#!/usr/bin/env python3
"""
secrets_scanner_mcp — Rilevatore di segreti hardcodati nel codebase
=====================================================================
Scansiona file/directory per trovare API key, token, password,
chiavi private e altri segreti prima che finiscano in un commit.

Tools esposti:
  scan_project    — scansiona tutto il progetto Prismalux
  scan_file       — scansiona un singolo file
  scan_staged     — scansiona solo i file in git staging area
  scan_diff       — scansiona le righe aggiunte nell'ultimo diff
  list_patterns   — mostra le categorie di pattern attive
  add_whitelist   — aggiunge una stringa alla whitelist (falsi positivi)
  show_whitelist  — mostra la whitelist corrente
"""

import sys, json, os, re, asyncio, logging, subprocess
from pathlib import Path
from typing import Any

logging.basicConfig(level=getattr(logging, os.environ.get("PRISMALUX_LOG_LEVEL","WARNING")))
logger = logging.getLogger(__name__)

ROOT = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
WHITELIST_FILE = Path.home() / ".prismalux" / "secrets_whitelist.txt"

# ─── Pattern library ─────────────────────────────────────────────────────────
# Ogni entry: (nome_categoria, regex, severity: HIGH/MEDIUM/LOW)

PATTERNS: list[tuple[str, re.Pattern, str]] = [
    # Chiavi private
    ("Private Key (PEM)",
     re.compile(r"-----BEGIN\s+(RSA\s+|EC\s+|DSA\s+|OPENSSH\s+)?PRIVATE KEY-----", re.I),
     "CRITICAL"),
    # AWS
    ("AWS Access Key ID",
     re.compile(r"(?<![A-Z0-9])(AKIA|AGPA|AROA|ASIA|AIPA|ANPA|ANVA|ASIA)[0-9A-Z]{16}(?![A-Z0-9])"),
     "CRITICAL"),
    ("AWS Secret Access Key",
     re.compile(r"aws[_\-\s]?secret[_\-\s]?access[_\-\s]?key\s*[=:]\s*['\"]?[a-zA-Z0-9/+]{40}['\"]?", re.I),
     "CRITICAL"),
    # Token GitHub/GitLab/Bitbucket
    ("GitHub Personal Access Token",
     re.compile(r"ghp_[a-zA-Z0-9]{36}|github_pat_[a-zA-Z0-9_]{82}"),
     "CRITICAL"),
    ("GitHub OAuth Token",
     re.compile(r"gho_[a-zA-Z0-9]{36}"),
     "CRITICAL"),
    ("GitLab Token",
     re.compile(r"glpat-[a-zA-Z0-9\-_]{20}"),
     "HIGH"),
    # Telegram Bot Token
    ("Telegram Bot Token",
     re.compile(r"\b\d{8,12}:[a-zA-Z0-9_-]{35}\b"),
     "HIGH"),
    # JWT (bearer tokens)
    ("JWT Token",
     re.compile(r"eyJ[a-zA-Z0-9_-]{10,}\.eyJ[a-zA-Z0-9_-]{10,}\.[a-zA-Z0-9_-]{10,}"),
     "HIGH"),
    # Google / GCP
    ("Google API Key",
     re.compile(r"AIza[0-9A-Za-z\-_]{35}"),
     "HIGH"),
    ("GCP Service Account",
     re.compile(r'"type"\s*:\s*"service_account"', re.I),
     "HIGH"),
    # Stripe
    ("Stripe Secret Key",
     re.compile(r"sk_live_[a-zA-Z0-9]{24,}"),
     "CRITICAL"),
    ("Stripe Publishable Key",
     re.compile(r"pk_live_[a-zA-Z0-9]{24,}"),
     "MEDIUM"),
    # SendGrid / Mailgun
    ("SendGrid API Key",
     re.compile(r"SG\.[a-zA-Z0-9_-]{22,}\.[a-zA-Z0-9_-]{43}"),
     "HIGH"),
    # Slack
    ("Slack Token",
     re.compile(r"xox[bprs]-[a-zA-Z0-9-]{10,}"),
     "HIGH"),
    ("Slack Webhook",
     re.compile(r"hooks\.slack\.com/services/T[a-zA-Z0-9_]{8}/B[a-zA-Z0-9_]{8}/[a-zA-Z0-9_]{24}"),
     "HIGH"),
    # SSH private key passphrase in config
    ("SSH Private Key Block",
     re.compile(r"-----BEGIN\s+OPENSSH PRIVATE KEY-----"),
     "CRITICAL"),
    # Generic segreti hardcodati: assegnazioni a variabili con nomi sospetti
    ("Hardcoded Password",
     re.compile(r"""(?:password|passwd|pwd|secret|passphrase)\s*[=:]\s*['"][^'"]{8,}['"]""", re.I),
     "HIGH"),
    ("Hardcoded API Key Assignment",
     re.compile(r"""(?:api[_\-]?key|apikey|api[_\-]?secret|auth[_\-]?token|access[_\-]?token)\s*[=:]\s*['"][a-zA-Z0-9+/._\-]{16,}['"]""", re.I),
     "HIGH"),
    ("Generic Secret Assignment",
     re.compile(r"""(?:secret|token|bearer)\s*[=:]\s*['"][a-zA-Z0-9+/._\-]{20,}['"]""", re.I),
     "MEDIUM"),
    # Chiave hex generica lunga (≥32 hex chars)
    ("Long Hex Secret",
     re.compile(r"""(?:key|secret|hash|token)\s*[=:]\s*['"]?[0-9a-f]{32,}['"]?""", re.I),
     "LOW"),
    # URL con credenziali
    ("URL with Credentials",
     re.compile(r"[a-zA-Z][a-zA-Z0-9+\-.]*://[^:@\s]+:[^@\s]{4,}@[^\s/]+"),
     "HIGH"),
    # Certificati
    ("Certificate Block",
     re.compile(r"-----BEGIN CERTIFICATE-----"),
     "LOW"),
]

# Estensioni da scansionare
SCAN_EXTS = {
    ".py", ".cpp", ".h", ".js", ".ts", ".json", ".yaml", ".yml",
    ".env", ".cfg", ".conf", ".ini", ".toml", ".sh", ".bat", ".txt",
    ".xml", ".md", ".cmake",
}

# Path da ignorare sempre
IGNORE_PATHS = {
    "build_gui", "build-desktop", "build", ".git", "__pycache__",
    "node_modules", ".venv", "venv", ".mypy_cache",
    "secrets_whitelist.txt",  # evita auto-detect della propria whitelist
}

# ─── Whitelist ────────────────────────────────────────────────────────────────

def _load_whitelist() -> set[str]:
    if not WHITELIST_FILE.exists():
        return set()
    return {l.strip() for l in WHITELIST_FILE.read_text().splitlines() if l.strip()}

def _save_whitelist(wl: set[str]):
    WHITELIST_FILE.parent.mkdir(parents=True, exist_ok=True)
    WHITELIST_FILE.write_text("\n".join(sorted(wl)) + "\n")

# ─── Scanner core ─────────────────────────────────────────────────────────────

def _is_ignored(path: Path) -> bool:
    parts = set(path.parts)
    return bool(parts & IGNORE_PATHS)

def _scan_text(text: str, filepath: str, whitelist: set[str]) -> list[dict]:
    findings = []
    lines = text.splitlines()
    for lineno, line in enumerate(lines, 1):
        # Salta righe di commento esplicito di sicurezza
        if "nosec" in line.lower() or "noqa" in line.lower():
            continue
        for name, pattern, severity in PATTERNS:
            m = pattern.search(line)
            if m:
                matched = m.group(0)
                if any(w in matched for w in whitelist):
                    continue
                findings.append({
                    "file":     filepath,
                    "line":     lineno,
                    "severity": severity,
                    "category": name,
                    "match":    matched[:80],
                    "context":  line.strip()[:120],
                })
    return findings

def _scan_path(path: Path, whitelist: set[str], max_size_kb: int = 512) -> list[dict]:
    if _is_ignored(path):
        return []
    findings = []
    if path.is_file():
        if path.suffix.lower() not in SCAN_EXTS and path.name not in {".env", ".envrc"}:
            return []
        try:
            if path.stat().st_size > max_size_kb * 1024:
                return []
            text = path.read_text(errors="replace")
            rel  = str(path.relative_to(ROOT)) if ROOT in path.parents else str(path)
            findings.extend(_scan_text(text, rel, whitelist))
        except (OSError, PermissionError):
            pass
    elif path.is_dir():
        for child in path.rglob("*"):
            if not _is_ignored(child):
                findings.extend(_scan_path(child, whitelist, max_size_kb))
    return findings

def _format_findings(findings: list[dict], title: str) -> str:
    if not findings:
        return f"✅ {title}\nNessun segreto rilevato."

    sev_order = {"CRITICAL": 0, "HIGH": 1, "MEDIUM": 2, "LOW": 3}
    findings.sort(key=lambda f: (sev_order.get(f["severity"], 9), f["file"], f["line"]))

    icons = {"CRITICAL": "🔴", "HIGH": "🟠", "MEDIUM": "🟡", "LOW": "🔵"}
    counts = {}
    for f in findings:
        counts[f["severity"]] = counts.get(f["severity"], 0) + 1

    summary = " | ".join(f"{icons[s]} {s}: {n}" for s, n in counts.items() if n)
    lines   = [f"⚠️  {title} — {len(findings)} segreti rilevati", summary, ""]

    current_file = None
    for f in findings:
        if f["file"] != current_file:
            current_file = f["file"]
            lines.append(f"\n📄 {current_file}")
        icon = icons.get(f["severity"], "·")
        lines.append(f"  {icon} L{f['line']:4d}  [{f['category']}]")
        lines.append(f"         Match  : {f['match']}")
        lines.append(f"         Contesto: {f['context'][:100]}")

    lines.append("\n💡 Falso positivo? Usa add_whitelist(string) per escluderlo.")
    return "\n".join(lines)

# ─── Handlers ─────────────────────────────────────────────────────────────────

def handle_scan_project(args: dict) -> str:
    severity_min = args.get("severity_min", "LOW").upper()
    sev_order    = {"CRITICAL":0,"HIGH":1,"MEDIUM":2,"LOW":3}
    min_idx      = sev_order.get(severity_min, 3)
    whitelist    = _load_whitelist()

    findings = _scan_path(ROOT, whitelist)
    findings = [f for f in findings if sev_order.get(f["severity"], 3) <= min_idx]
    return _format_findings(findings, f"Scansione progetto Prismalux (severity ≥ {severity_min})")


def handle_scan_file(args: dict) -> str:
    filepath = args.get("file", "").strip()
    if not filepath:
        return "Specifica 'file'."
    path = Path(filepath) if Path(filepath).is_absolute() else ROOT / filepath
    if not path.exists():
        return f"File non trovato: {path}"
    whitelist = _load_whitelist()
    findings  = _scan_path(path, whitelist)
    return _format_findings(findings, f"Scansione {path.name}")


def handle_scan_staged(args: dict) -> str:
    try:
        r = subprocess.run(
            ["git", "diff", "--cached", "--name-only"],
            capture_output=True, text=True, cwd=str(ROOT), timeout=10
        )
        staged = [l.strip() for l in r.stdout.splitlines() if l.strip()]
    except Exception as e:
        return f"Errore git: {e}"

    if not staged:
        return "Nessun file in staging area."

    whitelist = _load_whitelist()
    all_findings = []
    for rel in staged:
        p = ROOT / rel
        if p.exists():
            all_findings.extend(_scan_path(p, whitelist))

    return _format_findings(all_findings, f"Scansione staging ({len(staged)} file)")


def handle_scan_diff(args: dict) -> str:
    commit = args.get("commit", "HEAD").strip() or "HEAD"
    try:
        r = subprocess.run(
            ["git", "diff", commit, "--unified=0"],
            capture_output=True, text=True, cwd=str(ROOT), timeout=15
        )
        diff_text = r.stdout
    except Exception as e:
        return f"Errore git diff: {e}"

    if not diff_text:
        return "Nessun cambiamento rispetto a HEAD."

    whitelist = _load_whitelist()
    # Estrae solo le righe aggiunte (+) con il loro nome file
    findings  = []
    current_file = "unknown"
    for line in diff_text.splitlines():
        if line.startswith("+++ b/"):
            current_file = line[6:]
        elif line.startswith("+") and not line.startswith("+++"):
            added_line = line[1:]
            hits = _scan_text(added_line, current_file, whitelist)
            findings.extend(hits)

    return _format_findings(findings, f"Scansione diff vs {commit}")


def handle_list_patterns(_args: dict) -> str:
    icons = {"CRITICAL": "🔴", "HIGH": "🟠", "MEDIUM": "🟡", "LOW": "🔵"}
    lines = [f"Pattern di rilevamento attivi ({len(PATTERNS)} totali):\n"]
    for name, _, severity in PATTERNS:
        lines.append(f"  {icons[severity]} {severity:8s}  {name}")
    lines.append(f"\nEstensioni scansionate: {', '.join(sorted(SCAN_EXTS))}")
    lines.append(f"Path esclusi: {', '.join(sorted(IGNORE_PATHS))}")
    return "\n".join(lines)


def handle_add_whitelist(args: dict) -> str:
    string = args.get("string", "").strip()
    if not string:
        return "Specifica 'string' da aggiungere alla whitelist."
    wl = _load_whitelist()
    wl.add(string)
    _save_whitelist(wl)
    return f"✅ Aggiunto alla whitelist: '{string}'\nWhitelist contiene ora {len(wl)} voci."


def handle_show_whitelist(_args: dict) -> str:
    wl = _load_whitelist()
    if not wl:
        return "Whitelist vuota (nessun falso positivo registrato)."
    return f"Whitelist ({len(wl)} voci):\n" + "\n".join(f"  · {w}" for w in sorted(wl))


HANDLERS = {
    "scan_project":   handle_scan_project,
    "scan_file":      handle_scan_file,
    "scan_staged":    handle_scan_staged,
    "scan_diff":      handle_scan_diff,
    "list_patterns":  handle_list_patterns,
    "add_whitelist":  handle_add_whitelist,
    "show_whitelist": handle_show_whitelist,
}

TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"scan_project","description":"Scansiona tutto il progetto Prismalux per segreti hardcodati (API key, token, password, chiavi private). Filtrabile per severity minima.","inputSchema":{"type":"object","properties":{"severity_min":{"type":"string","description":"Severity minima da riportare: CRITICAL|HIGH|MEDIUM|LOW (default: LOW)","default":"LOW"}}}},
    {"name":"scan_file","description":"Scansiona un singolo file per segreti hardcodati.","inputSchema":{"type":"object","properties":{"file":{"type":"string","description":"Path del file (relativo a root Prismalux o assoluto)"}},"required":["file"]}},
    {"name":"scan_staged","description":"Scansiona solo i file attualmente in git staging area (pre-commit check).","inputSchema":{"type":"object","properties":{}}},
    {"name":"scan_diff","description":"Scansiona solo le righe aggiunte nel diff rispetto a un commit (default: HEAD).","inputSchema":{"type":"object","properties":{"commit":{"type":"string","description":"Commit di riferimento (default: HEAD, es. 'HEAD~1', hash)","default":"HEAD"}}}},
    {"name":"list_patterns","description":"Mostra tutte le categorie di pattern di rilevamento attivi con severity.","inputSchema":{"type":"object","properties":{}}},
    {"name":"add_whitelist","description":"Aggiunge una stringa alla whitelist per escludere falsi positivi dalle future scansioni.","inputSchema":{"type":"object","properties":{"string":{"type":"string","description":"Stringa da whitelistare (es. un placeholder noto come 'YOUR_API_KEY_HERE')"}},"required":["string"]}},
    {"name":"show_whitelist","description":"Mostra la whitelist corrente dei falsi positivi.","inputSchema":{"type":"object","properties":{}}},
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
            resp = _rpc_result(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"secrets-scanner","version":"1.0.0"},"capabilities":{"tools":{}}})
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
