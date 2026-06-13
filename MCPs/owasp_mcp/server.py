#!/usr/bin/env python3
"""
owasp_mcp — OWASP Top 10 specifico per Qt6 / Python / Prismalux
================================================================
Checklist OWASP A01-A10 adattata al codebase Qt6 + MCPs Python.
Complementa sast_mcp con pattern di sicurezza architetturali.

Tools:
  check_a01  — Broken Access Control (endpoint senza auth)
  check_a02  — Cryptographic Failures (MD5, HTTP, rand insicuro)
  check_a03  — Injection (QProcess, SQL concat, format string)
  check_a05  — Security Misconfiguration (debug, default creds)
  check_a07  — Auth & Session (token hardcodati, JWT deboli)
  check_a09  — Security Logging (password/token nei log)
  full_scan  — tutti i check A01-A09 con report unificato
  explain    — spiega una categoria OWASP con esempi Qt6
"""
import sys, json, os, re, asyncio
from pathlib import Path
from typing import Any

ROOT    = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
GUI_DIR = ROOT / "gui"
IGNORE  = {"build", "build_gui", "build_asan", ".git", "__pycache__", "node_modules"}

def _files(path: Path, exts=(".cpp",".h",".py")) -> list[Path]:
    return [f for f in path.rglob("*") if f.suffix in exts
            and not any(p in IGNORE for p in f.parts)]

def _rel(f: Path) -> str:
    try: return str(f.relative_to(ROOT))
    except: return str(f)

def _scan(path: Path, patterns: list[tuple[str, re.Pattern, str, str]],
          exts=(".cpp",".h",".py")) -> list[dict]:
    findings = []
    for f in _files(path, exts):
        try: lines = f.read_text(errors="replace").splitlines()
        except OSError: continue
        for ln, line in enumerate(lines, 1):
            if "nosec" in line.lower() or "noqa" in line.lower(): continue
            for sev, pat, category, desc in patterns:
                if pat.search(line):
                    findings.append({"sev":sev,"file":_rel(f),"line":ln,
                                     "cat":category,"desc":desc,
                                     "ctx":line.strip()[:100]})
    return findings

ICONS = {"CRITICAL":"🔴","HIGH":"🟠","MEDIUM":"🟡","LOW":"🔵"}
ORDER = {"CRITICAL":0,"HIGH":1,"MEDIUM":2,"LOW":3}

def _fmt(findings: list[dict], title: str) -> str:
    if not findings: return f"✅ {title}: nessun problema rilevato."
    findings.sort(key=lambda x: ORDER.get(x["sev"],3))
    counts = {}
    for f in findings: counts[f["sev"]] = counts.get(f["sev"],0)+1
    summ = " | ".join(f"{ICONS[s]} {s}: {n}" for s,n in counts.items())
    lines = [f"⚠️  {title} — {len(findings)} problemi", summ, ""]
    cur = None
    for f in findings:
        if f["file"] != cur:
            cur = f["file"]; lines.append(f"\n📄 {cur}")
        lines.append(f"  {ICONS[f['sev']]} L{f['line']:4d} [{f['cat']}] {f['desc']}")
        lines.append(f"         → {f['ctx']}")
    return "\n".join(lines)

# ─── A01: Broken Access Control ──────────────────────────────────────────────

A01_PATTERNS = [
    ("HIGH", re.compile(r'/api/\w+.*route|handleRequest|handlePost|handleGet', re.I), "Endpoint HTTP", "Verificare che ogni endpoint controlli token Bearer"),
    ("HIGH", re.compile(r'if\s*\(\s*!.*auth|!.*token|!.*authorized', re.I), "Auth check mancante", "Assicurarsi che il check sia sul percorso corretto"),
    ("MEDIUM", re.compile(r'readAll\(\)|readLine\(\).*socket|QTcpSocket.*read', re.I), "Lettura socket senza auth", "Dati da socket letti prima della verifica auth"),
    ("MEDIUM", re.compile(r'setWorldMatrixEnabled|QDir::setCurrent|QFile.*remove\(', re.I), "Operazione filesystem privilegiata", "Verificare che l'utente sia autorizzato"),
]

def handle_check_a01(args: dict) -> str:
    path = Path(args.get("path", str(GUI_DIR)))
    if not path.is_absolute(): path = ROOT / path
    findings = _scan(path, A01_PATTERNS)
    return _fmt(findings, "A01 Broken Access Control")

# ─── A02: Cryptographic Failures ─────────────────────────────────────────────

A02_PATTERNS = [
    ("CRITICAL", re.compile(r'\bMD5\b|QCryptographicHash::Md5', re.I), "MD5 deprecato", "MD5 non sicuro per hashing password/integrità"),
    ("CRITICAL", re.compile(r'\bSHA1\b|QCryptographicHash::Sha1(?!6)', re.I), "SHA1 deprecato", "SHA1 rotto dal 2017 — usare SHA-256+"),
    ("HIGH", re.compile(r'"http://(?!localhost|127\.0\.0\.1)', re.I), "HTTP non cifrato", "Comunicazione in chiaro — usare HTTPS"),
    ("HIGH", re.compile(r'\brand\(\s*\)|qrand\(\s*\)', re.I), "PRNG non sicuro", "rand()/qrand() non crittografici — usare QRandomGenerator::securelySeeded()"),
    ("HIGH", re.compile(r'ECB|AES_ECB|mode.*ECB', re.I), "AES-ECB insicuro", "ECB mode rivela pattern — usare AES-GCM o AES-CBC con IV"),
    ("MEDIUM", re.compile(r'QSslConfiguration::setProtocol\(QSsl::TlsV1_0|TlsV1_1\)', re.I), "TLS obsoleto", "TLS 1.0/1.1 deprecato — usare TLS 1.2+"),
    ("LOW", re.compile(r'setVerifyMode\(QSslSocket::VerifyNone\)', re.I), "SSL verify disabilitato", "VerifyNone sopprime la verifica del certificato server"),
]

def handle_check_a02(args: dict) -> str:
    path = Path(args.get("path", str(GUI_DIR)))
    if not path.is_absolute(): path = ROOT / path
    findings = _scan(path, A02_PATTERNS)
    return _fmt(findings, "A02 Cryptographic Failures")

# ─── A03: Injection ──────────────────────────────────────────────────────────

A03_PATTERNS = [
    ("CRITICAL", re.compile(r'QProcess\s*::\s*start\s*\(.*\+|QProcess\s*::\s*execute\s*\(.*\+'), "Command Injection", "Input utente concatenato in QProcess — usare lista argomenti separata"),
    ("CRITICAL", re.compile(r'QSqlQuery\s*\([^)]*\+|exec\s*\([^)]*\+.*sql', re.I), "SQL Injection", "Stringa SQL costruita con + — usare QSqlQuery::bindValue"),
    ("HIGH", re.compile(r'QString\s+query\s*=\s*.*\+\s*(?:user|input|param|arg|text)', re.I), "SQL Injection potenziale", "Concatenazione variabile in query SQL"),
    ("HIGH", re.compile(r'system\s*\('), "OS Command Injection", "system() esegue shell — usare QProcess con argomenti separati"),
    ("HIGH", re.compile(r'QWebEngineView.*evaluateJavaScript|page\(\)->runJavaScript\(.*\+'), "XSS/JS Injection", "JavaScript dinamico in QWebEngine — sanificare input"),
    ("MEDIUM", re.compile(r'fromUtf8\(.*getenv|QProcessEnvironment::value\(.*\+'), "Env Injection", "Variabile d'ambiente non sanificata inserita in contesto critico"),
    ("MEDIUM", re.compile(r'printf\s*\(\s*\w+\s*\)|fprintf\s*\(\s*\w+\s*,\s*\w+\s*\)'), "Format String", "Primo argomento printf è una variabile — format string attack"),
]

def handle_check_a03(args: dict) -> str:
    path = Path(args.get("path", str(GUI_DIR)))
    if not path.is_absolute(): path = ROOT / path
    findings = _scan(path, A03_PATTERNS)
    # Aggiungi check Python MCPs
    findings += _scan(ROOT / "MCPs", [
        ("HIGH", re.compile(r'subprocess\.call\(.*shell\s*=\s*True|os\.system\('), "Shell Injection Python", "shell=True con input esterno — usare lista argomenti"),
        ("HIGH", re.compile(r'eval\s*\(|exec\s*\('), "Code Injection Python", "eval/exec con input esterno"),
        ("MEDIUM", re.compile(r'f".*{.*input|f".*{.*param|f".*{.*user', re.I), "f-string injection", "f-string con dati utente non validati"),
    ], exts=(".py",))
    return _fmt(findings, "A03 Injection")

# ─── A05: Security Misconfiguration ──────────────────────────────────────────

A05_PATTERNS = [
    ("HIGH", re.compile(r'debug\s*=\s*true|DEBUG\s*=\s*1|setDebug\s*\(\s*true', re.I), "Debug attivo", "Debug mode attivo — disabilitare in produzione"),
    ("HIGH", re.compile(r'"password"\s*:\s*"[^"]{2,}"|password\s*=\s*"[^"]{4,}"', re.I), "Credenziale hardcodata", "Password hardcodata nel codice sorgente"),
    ("HIGH", re.compile(r'admin.*:.*admin|root.*:.*root|guest.*:.*guest', re.I), "Credenziale di default", "Credenziale di default — cambiare prima del deploy"),
    ("MEDIUM", re.compile(r'CORS.*\*|Access-Control-Allow-Origin.*\*'), "CORS wildcard", "CORS * permette richieste da qualsiasi origine"),
    ("MEDIUM", re.compile(r'setAllowedOrigins\s*\(\s*\*|allowAll\s*=\s*true', re.I), "CORS permissivo", "Controllare la whitelist di origini"),
    ("LOW", re.compile(r'QNetworkRequest.*setRawHeader.*X-Frame|X-Content-Type'), "Header sicurezza mancanti", "Aggiungere X-Frame-Options, X-Content-Type-Options"),
]

def handle_check_a05(args: dict) -> str:
    path = ROOT
    findings = _scan(path, A05_PATTERNS)
    return _fmt(findings, "A05 Security Misconfiguration")

# ─── A07: Auth & Session ─────────────────────────────────────────────────────

A07_PATTERNS = [
    ("CRITICAL", re.compile(r'token\s*=\s*"[a-zA-Z0-9+/]{20,}"|bearer\s*=\s*"[^"]{10,}"', re.I), "Token hardcodato", "Token di autenticazione hardcodato nel sorgente"),
    ("HIGH", re.compile(r'jwt\.encode\(.*algorithm\s*=\s*"none"', re.I), "JWT algoritmo none", "JWT con algoritmo 'none' è privo di firma"),
    ("HIGH", re.compile(r'SECRET_KEY\s*=\s*"[^"]{0,20}"', re.I), "Secret key debole", "Secret JWT troppo corta (< 20 char)"),
    ("MEDIUM", re.compile(r'session\[.*\]\s*=|setcookie\s*\((?!.*httponly.*secure|.*secure.*httponly)', re.I), "Cookie insicuro", "Cookie senza HttpOnly o Secure flag"),
    ("LOW", re.compile(r'expir.*=.*3600\s*\*\s*24\s*\*\s*[7-9]\d|days\s*=\s*[7-9]\d', re.I), "Token longevo", "Token con scadenza > 7 giorni — ridurre a 24h"),
]

def handle_check_a07(args: dict) -> str:
    path = ROOT
    findings = _scan(path, A07_PATTERNS)
    return _fmt(findings, "A07 Identification & Authentication Failures")

# ─── A09: Security Logging ───────────────────────────────────────────────────

A09_PATTERNS = [
    ("HIGH", re.compile(r'qDebug\(\)\s*<<.*(?:password|token|secret|key|credential)', re.I), "Password in qDebug", "Credenziali loggate — rimuovere o mascherare"),
    ("HIGH", re.compile(r'qWarning\(\)\s*<<.*(?:password|token|secret)', re.I), "Secret in qWarning", "Dato sensibile nel log di warning"),
    ("HIGH", re.compile(r'print\s*\(.*(?:password|token|secret|api_key)', re.I), "Secret in print Python", "Dato sensibile stampato in output"),
    ("MEDIUM", re.compile(r'logging\.\w+\(.*(?:password|token|secret)', re.I), "Secret in logging Python", "Dato sensibile nel logger Python"),
    ("MEDIUM", re.compile(r'std::cout\s*<<.*(?:password|token|secret)', re.I), "Secret in cout", "Dato sensibile scritto su stdout C++"),
    ("LOW", re.compile(r'qDebug\(\)\s*<<\s*(?:m_token|m_key|m_secret|m_password)', re.I), "Membro privato logato", "Membro privato sensibile nel log di debug"),
]

def handle_check_a09(args: dict) -> str:
    path = ROOT
    findings = _scan(path, A09_PATTERNS)
    return _fmt(findings, "A09 Security Logging & Monitoring Failures")

# ─── Full scan ────────────────────────────────────────────────────────────────

def handle_full_scan(args: dict) -> str:
    checks = [
        ("A01 Broken Access Control",    handle_check_a01),
        ("A02 Cryptographic Failures",   handle_check_a02),
        ("A03 Injection",                handle_check_a03),
        ("A05 Security Misconfiguration",handle_check_a05),
        ("A07 Auth & Session Failures",  handle_check_a07),
        ("A09 Security Logging",         handle_check_a09),
    ]
    out = ["🔐 OWASP Top 10 — Full Scan Prismalux\n" + "═"*50]
    total = 0
    for title, fn in checks:
        out.append(f"\n{'─'*50}\n  {title}\n{'─'*50}")
        try:
            result = fn(args)
            out.append(result)
            if "problemi" in result:
                n = re.search(r"(\d+) problemi", result)
                if n: total += int(n.group(1))
        except Exception as e: out.append(f"Errore: {e}")
    out.append(f"\n{'═'*50}")
    out.append(f"  Totale: {total} problemi trovati")
    return "\n".join(out)

# ─── Explain ─────────────────────────────────────────────────────────────────

OWASP_DOCS = {
    "a01": """**A01 — Broken Access Control**
Scenario Qt6: il LAN server risponde a richieste /api/* senza verificare il token Bearer.
Fix: `if (!verifyToken(req.header("Authorization"))) { reply(401); return; }`
Ref: https://owasp.org/Top10/A01_2021-Broken_Access_Control/""",
    "a02": """**A02 — Cryptographic Failures**
Scenario Qt6: hash password con MD5 o comunicazione HTTP in chiaro.
Fix: `QCryptographicHash::hash(data, QCryptographicHash::Sha3_256)`
Fix TLS: `QSslConfiguration::defaultConfiguration().setProtocol(QSsl::TlsV1_3)`
Ref: https://owasp.org/Top10/A02_2021-Cryptographic_Failures/""",
    "a03": """**A03 — Injection**
Scenario Qt6 (command): `QProcess::execute("ffmpeg " + userInput)` → injection.
Fix: `QProcess p; p.start("ffmpeg", QStringList() << arg1 << arg2);`
Scenario SQL: `query.exec("SELECT * FROM t WHERE id=" + id)` → SQLi.
Fix: `query.prepare("SELECT * FROM t WHERE id=:id"); query.bindValue(":id", id);`
Ref: https://owasp.org/Top10/A03_2021-Injection/""",
    "a05": """**A05 — Security Misconfiguration**
Scenario: `#define DEBUG 1` lasciato in build Release, o password hardcodata.
Fix: usare CMake `-DCMAKE_BUILD_TYPE=Release` + `#ifdef QT_DEBUG` per le stampe.
Ref: https://owasp.org/Top10/A05_2021-Security_Misconfiguration/""",
    "a07": """**A07 — Identification & Authentication Failures**
Scenario: token JWT con scadenza 30 giorni o secret key "password123".
Fix: secret >= 32 bytes casuali, exp = now + 3600. Rotazione token ogni login.
Ref: https://owasp.org/Top10/A07_2021-Identification_and_Authentication_Failures/""",
    "a09": """**A09 — Security Logging Failures**
Scenario: `qDebug() << "Token:" << m_token;` traccia il segreto nei log.
Fix: log solo il prefisso `qDebug() << "Token:" << m_token.left(4) << "****";`
Ref: https://owasp.org/Top10/A09_2021-Security_Logging_and_Monitoring_Failures/""",
}

def handle_explain(args: dict) -> str:
    cat = args.get("category", "").lower().replace(" ","").replace("-","")
    for key in OWASP_DOCS:
        if key in cat or cat in key:
            return OWASP_DOCS[key]
    return "Categorie disponibili: a01, a02, a03, a05, a07, a09\nEsempio: explain(category='a03')"


HANDLERS = {
    "check_a01":  handle_check_a01,
    "check_a02":  handle_check_a02,
    "check_a03":  handle_check_a03,
    "check_a05":  handle_check_a05,
    "check_a07":  handle_check_a07,
    "check_a09":  handle_check_a09,
    "full_scan":  handle_full_scan,
    "explain":    handle_explain,
}
TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"check_a01","description":"OWASP A01 Broken Access Control: endpoint HTTP senza verifica auth, operazioni filesystem non protette.","inputSchema":{"type":"object","properties":{"path":{"type":"string","default":""}}}},
    {"name":"check_a02","description":"OWASP A02 Cryptographic Failures: MD5/SHA1, HTTP non cifrato, rand() insicuro, TLS obsoleto, AES-ECB.","inputSchema":{"type":"object","properties":{"path":{"type":"string","default":""}}}},
    {"name":"check_a03","description":"OWASP A03 Injection: QProcess con input concatenato, SQL injection, system(), XSS in QWebEngine, eval Python.","inputSchema":{"type":"object","properties":{"path":{"type":"string","default":""}}}},
    {"name":"check_a05","description":"OWASP A05 Security Misconfiguration: debug attivo, credenziali hardcodate/di default, CORS wildcard.","inputSchema":{"type":"object","properties":{}}},
    {"name":"check_a07","description":"OWASP A07 Auth Failures: token hardcodati, JWT algoritmo none, secret key debole, cookie insicuri.","inputSchema":{"type":"object","properties":{}}},
    {"name":"check_a09","description":"OWASP A09 Security Logging: password/token loggati con qDebug, print Python, std::cout.","inputSchema":{"type":"object","properties":{}}},
    {"name":"full_scan","description":"Esegue tutti i check OWASP A01-A09 in sequenza con report unificato.","inputSchema":{"type":"object","properties":{"path":{"type":"string","default":""}}}},
    {"name":"explain","description":"Spiega una categoria OWASP con scenario concreto Qt6 e fix. Es: explain(category='a03').","inputSchema":{"type":"object","properties":{"category":{"type":"string"}},"required":["category"]}},
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
            resp = _ok(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"owasp","version":"1.0.0"},"capabilities":{"tools":{}}})
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
