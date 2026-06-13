#!/usr/bin/env python3
"""
cve_lookup_mcp — Ricerca CVE e advisory di sicurezza
=====================================================
Interroga NVD (NIST) e OSV.dev per trovare vulnerabilità
per software, librerie o CVE ID specifici.

Tools esposti:
  lookup_cve       — dettaglio completo di un CVE specifico
  search_cve       — cerca CVE per prodotto/keyword (NVD API)
  osv_search       — cerca in OSV.dev per pacchetto/ecosistema
  cvss_explain     — spiega un CVSS score vettoriale
  recent_critical  — CVE CRITICAL degli ultimi N giorni
  check_version    — controlla se una versione specifica è vulnerabile
"""

import sys, json, os, re, asyncio, logging, urllib.request, urllib.parse, urllib.error
from datetime import datetime, timedelta, timezone
from typing import Any

logging.basicConfig(level=getattr(logging, os.environ.get("PRISMALUX_LOG_LEVEL","WARNING")))
logger = logging.getLogger(__name__)

NVD_API  = "https://services.nvd.nist.gov/rest/json/cves/2.0"
OSV_API  = "https://api.osv.dev/v1"
NVD_KEY  = os.environ.get("NVD_API_KEY","")  # opzionale, aumenta rate limit

# ─── Helpers ─────────────────────────────────────────────────────────────────

def _http_get(url: str, timeout: int = 15) -> dict | None:
    headers = {"User-Agent": "Prismalux-CVE-MCP/1.0"}
    if NVD_KEY and "nvd.nist.gov" in url:
        headers["apiKey"] = NVD_KEY
    try:
        req = urllib.request.Request(url, headers=headers)
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return json.loads(resp.read())
    except urllib.error.HTTPError as e:
        logger.warning(f"HTTP {e.code}: {url}")
        return None
    except Exception as e:
        logger.warning(f"GET error: {e}")
        return None

def _http_post(url: str, payload: dict, timeout: int = 15) -> dict | None:
    data = json.dumps(payload).encode()
    try:
        req = urllib.request.Request(
            url, data=data,
            headers={"Content-Type":"application/json","User-Agent":"Prismalux-CVE-MCP/1.0"}
        )
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return json.loads(resp.read())
    except Exception as e:
        logger.warning(f"POST error: {e}")
        return None

def _severity_icon(score: float) -> str:
    if score >= 9.0: return "🔴 CRITICAL"
    if score >= 7.0: return "🟠 HIGH"
    if score >= 4.0: return "🟡 MEDIUM"
    return "🔵 LOW"

def _fmt_nvd_cve(item: dict) -> str:
    cve  = item.get("cve", {})
    cid  = cve.get("id","?")
    desc = next((d["value"] for d in cve.get("descriptions",[]) if d.get("lang")=="en"), "N/A")
    pub  = cve.get("published","")[:10]
    mod  = cve.get("lastModified","")[:10]

    # CVSS
    score_str = "N/A"
    vec_str   = ""
    metrics   = cve.get("metrics",{})
    for key in ("cvssMetricV31","cvssMetricV30","cvssMetricV2"):
        ms = metrics.get(key,[])
        if ms:
            cv = ms[0].get("cvssData",{})
            sc = cv.get("baseScore", 0)
            vec_str   = cv.get("vectorString","")
            score_str = f"{sc} ({_severity_icon(sc)})"
            break

    # Riferimenti
    refs = [r["url"] for r in cve.get("references",[]) if r.get("url")][:3]

    lines = [
        f"{'─'*60}",
        f"CVE     : {cid}",
        f"Pubbl.  : {pub}  (mod: {mod})",
        f"CVSS    : {score_str}",
    ]
    if vec_str:
        lines.append(f"Vettore : {vec_str}")
    lines.append(f"Desc    : {desc[:300]}")
    for r in refs:
        lines.append(f"Ref     : {r}")
    return "\n".join(lines)

# ─── Handlers ────────────────────────────────────────────────────────────────

def handle_lookup_cve(args: dict) -> str:
    cve_id = args.get("cve_id","").strip().upper()
    if not re.match(r"CVE-\d{4}-\d+", cve_id):
        return f"Formato non valido: '{cve_id}'. Usa CVE-YYYY-NNNNN."
    url  = f"{NVD_API}?cveId={cve_id}"
    data = _http_get(url)
    if not data:
        return f"Impossibile raggiungere NVD API. Verifica connessione internet."
    vulns = data.get("vulnerabilities",[])
    if not vulns:
        return f"{cve_id} non trovato in NVD."
    return _fmt_nvd_cve(vulns[0])


def handle_search_cve(args: dict) -> str:
    keyword  = args.get("keyword","").strip()
    product  = args.get("product","").strip()
    n        = min(int(args.get("n", 10)), 20)
    min_sev  = args.get("severity","").upper()

    if not keyword and not product:
        return "Specifica 'keyword' o 'product'."

    params: dict[str, str] = {"resultsPerPage": str(n)}
    if keyword:
        params["keywordSearch"] = keyword
    if product:
        params["keywordSearch"] = (params.get("keywordSearch","") + " " + product).strip()
    if min_sev in ("CRITICAL","HIGH","MEDIUM","LOW"):
        params["cvssV3Severity"] = min_sev

    url   = NVD_API + "?" + urllib.parse.urlencode(params)
    data  = _http_get(url, timeout=20)
    if not data:
        return "NVD API non raggiungibile."

    vulns = data.get("vulnerabilities",[])
    total = data.get("totalResults",0)
    if not vulns:
        return f"Nessun CVE trovato per '{keyword or product}'."

    lines = [f"🔍 CVE per '{keyword or product}' — {total} totali, mostro {len(vulns)}:\n"]
    for v in vulns:
        cve   = v.get("cve",{})
        cid   = cve.get("id","?")
        pub   = cve.get("published","")[:10]
        desc  = next((d["value"] for d in cve.get("descriptions",[]) if d.get("lang")=="en"), "")[:100]
        score = 0.0
        for key in ("cvssMetricV31","cvssMetricV30"):
            ms = cve.get("metrics",{}).get(key,[])
            if ms:
                score = ms[0].get("cvssData",{}).get("baseScore",0)
                break
        icon = _severity_icon(score) if score else "⚪ N/A"
        lines.append(f"  {icon:18s} {cid:18s} {pub}  {desc}")
    lines.append(f"\nUsa lookup_cve(cve_id) per i dettagli completi.")
    return "\n".join(lines)


def handle_osv_search(args: dict) -> str:
    package   = args.get("package","").strip()
    ecosystem = args.get("ecosystem","PyPI").strip()
    version   = args.get("version","").strip()

    if not package:
        return "Specifica 'package'."

    payload: dict[str, Any] = {"package": {"name": package, "ecosystem": ecosystem}}
    if version:
        payload["version"] = version

    data = _http_post(f"{OSV_API}/query", payload)
    if data is None:
        return "OSV.dev non raggiungibile."

    vulns = data.get("vulns",[])
    if not vulns:
        ver_str = f"=={version}" if version else ""
        return f"✅ {package}{ver_str} ({ecosystem}): nessuna vulnerabilità in OSV.dev."

    ver_str = f"=={version}" if version else ""
    lines = [f"⚠️  {package}{ver_str} ({ecosystem}) — {len(vulns)} vulnerabilità OSV:\n"]
    for v in vulns[:8]:
        vid     = v.get("id","?")
        aliases = [a for a in v.get("aliases",[]) if a.startswith("CVE-")]
        summary = v.get("summary","")[:120]
        fixed   = []
        for aff in v.get("affected",[]):
            for rng in aff.get("ranges",[]):
                for ev in rng.get("events",[]):
                    if "fixed" in ev:
                        fixed.append(ev["fixed"])
        cve_str = f" [{aliases[0]}]" if aliases else ""
        fix_str = f" → fix: {', '.join(set(fixed[:3]))}" if fixed else ""
        lines.append(f"  · {vid}{cve_str}: {summary}{fix_str}")
    if len(vulns) > 8:
        lines.append(f"  ... e altri {len(vulns)-8}")
    return "\n".join(lines)


def handle_cvss_explain(args: dict) -> str:
    vector = args.get("vector","").strip()
    if not vector:
        return "Specifica 'vector' (es. 'CVSS:3.1/AV:N/AC:L/PR:N/UI:N/S:U/C:H/I:H/A:H')."

    # Dizionari di decodifica CVSS v3
    AV  = {"N":"Network","A":"Adjacent","L":"Local","P":"Physical"}
    AC  = {"L":"Low","H":"High"}
    PR  = {"N":"None","L":"Low","H":"High"}
    UI  = {"N":"None","R":"Required"}
    S   = {"U":"Unchanged","C":"Changed"}
    CIA = {"N":"None","L":"Low","H":"High"}

    m = re.search(r"AV:([NALP])/AC:([LH])/PR:([NLH])/UI:([NR])/S:([UC])/C:([NLH])/I:([NLH])/A:([NLH])", vector)
    if not m:
        return f"Formato vettore non riconosciuto: {vector}\nEsempio: CVSS:3.1/AV:N/AC:L/PR:N/UI:N/S:U/C:H/I:H/A:H"

    av,ac,pr,ui,s,c,i,a = m.groups()
    av_desc = {"N":"Attacco via rete (più grave)","A":"Rete adiacente","L":"Accesso locale","P":"Accesso fisico"}.get(av,av)
    lines = [
        "📊 CVSS v3 — Analisi vettore:\n",
        f"  AV (Attack Vector)      : {AV.get(av,av)}  — {av_desc}",
        f"  AC (Attack Complexity)  : {AC.get(ac,ac)}  — {'Facile da sfruttare' if ac=='L' else 'Richiede condizioni specifiche'}",
        f"  PR (Privileges Required): {PR.get(pr,pr)}  — {'Nessun privilegio necessario' if pr=='N' else 'Richiede account '+PR.get(pr,pr).lower()}",
        f"  UI (User Interaction)   : {UI.get(ui,ui)}  — {'Nessuna interazione utente' if ui=='N' else 'Richiede azione utente'}",
        f"  S  (Scope)              : {S.get(s,s)}  — {'Impatto contenuto al componente' if s=='U' else 'Impatto si estende oltre il componente'}",
        f"  C  (Confidentiality)    : {CIA.get(c,c)}",
        f"  I  (Integrity)          : {CIA.get(i,i)}",
        f"  A  (Availability)       : {CIA.get(a,a)}",
    ]
    # Stima severity intuitiva
    if av == "N" and ac == "L" and pr == "N" and ui == "N":
        lines.append("\n⚠️  Vettore critico: attacco remoto, facile, senza auth, senza interazione utente.")
    return "\n".join(lines)


def handle_recent_critical(args: dict) -> str:
    days = min(int(args.get("days", 7)), 30)
    n    = min(int(args.get("n", 10)), 20)

    end   = datetime.now(timezone.utc)
    start = end - timedelta(days=days)
    fmt   = "%Y-%m-%dT%H:%M:%S.000"

    params = {
        "pubStartDate": start.strftime(fmt),
        "pubEndDate":   end.strftime(fmt),
        "cvssV3Severity": "CRITICAL",
        "resultsPerPage": str(n),
    }
    url  = NVD_API + "?" + urllib.parse.urlencode(params)
    data = _http_get(url, timeout=20)
    if not data:
        return "NVD API non raggiungibile."

    vulns = data.get("vulnerabilities",[])
    total = data.get("totalResults",0)
    if not vulns:
        return f"Nessun CVE CRITICAL negli ultimi {days} giorni."

    lines = [f"🔴 CVE CRITICAL ultimi {days} giorni — {total} totali:\n"]
    for v in vulns:
        cve  = v.get("cve",{})
        cid  = cve.get("id","?")
        pub  = cve.get("published","")[:10]
        desc = next((d["value"] for d in cve.get("descriptions",[]) if d.get("lang")=="en"),"")[:120]
        score = 0.0
        for key in ("cvssMetricV31","cvssMetricV30"):
            ms = cve.get("metrics",{}).get(key,[])
            if ms:
                score = ms[0].get("cvssData",{}).get("baseScore",0)
                break
        lines.append(f"  🔴 {score:.1f}  {cid}  {pub}  {desc}")
    lines.append(f"\nUsa lookup_cve(cve_id) per i dettagli.")
    return "\n".join(lines)


def handle_check_version(args: dict) -> str:
    software = args.get("software","").strip()
    version  = args.get("version","").strip()
    if not software or not version:
        return "Specifica 'software' e 'version' (es. software='openssl', version='3.0.1')."

    # Cerca in NVD per software+versione
    url  = f"{NVD_API}?keywordSearch={urllib.parse.quote(software+' '+version)}&resultsPerPage=5"
    data = _http_get(url, timeout=20)

    lines = [f"🔍 Controllo {software} {version}:\n"]
    if data:
        vulns = data.get("vulnerabilities",[])
        if vulns:
            for v in vulns:
                cve   = v.get("cve",{})
                cid   = cve.get("id","?")
                desc  = next((d["value"] for d in cve.get("descriptions",[]) if d.get("lang")=="en"),"")[:100]
                score = 0.0
                for key in ("cvssMetricV31","cvssMetricV30"):
                    ms = cve.get("metrics",{}).get(key,[])
                    if ms:
                        score = ms[0].get("cvssData",{}).get("baseScore",0)
                        break
                lines.append(f"  {_severity_icon(score)}  {cid}: {desc}")
        else:
            lines.append(f"✅ Nessun CVE noto per {software} {version} in NVD.")
    else:
        lines.append("NVD non raggiungibile.")

    # Cerca anche in OSV se è un pacchetto Python
    osv_ecosystems = ["PyPI","npm","Go","Maven","RubyGems","NuGet","crates.io"]
    for eco in ["PyPI"]:  # default PyPI per Prismalux
        payload = {"version": version, "package": {"name": software, "ecosystem": eco}}
        osv_data = _http_post(f"{OSV_API}/query", payload)
        if osv_data and osv_data.get("vulns"):
            n = len(osv_data["vulns"])
            lines.append(f"\n  OSV.dev ({eco}): {n} vulnerabilità aggiuntive")

    return "\n".join(lines)


HANDLERS = {
    "lookup_cve":      handle_lookup_cve,
    "search_cve":      handle_search_cve,
    "osv_search":      handle_osv_search,
    "cvss_explain":    handle_cvss_explain,
    "recent_critical": handle_recent_critical,
    "check_version":   handle_check_version,
}

TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"lookup_cve","description":"Mostra il dettaglio completo di un CVE (CVSS, descrizione, riferimenti) dal database NVD/NIST.","inputSchema":{"type":"object","properties":{"cve_id":{"type":"string","description":"ID CVE (es. 'CVE-2024-12345')"}},"required":["cve_id"]}},
    {"name":"search_cve","description":"Cerca CVE per prodotto o keyword. Filtrabile per severity. Usa NVD API.","inputSchema":{"type":"object","properties":{"keyword":{"type":"string","description":"Keyword di ricerca (es. 'Qt6', 'openssl buffer overflow')","default":""},"product":{"type":"string","description":"Nome prodotto (es. 'nginx', 'python')","default":""},"n":{"type":"integer","description":"Numero risultati (default: 10, max: 20)","default":10},"severity":{"type":"string","description":"Filtra per severity: CRITICAL|HIGH|MEDIUM|LOW (default: nessun filtro)","default":""}}}},
    {"name":"osv_search","description":"Cerca vulnerabilità in OSV.dev per pacchetto ed ecosistema (PyPI, npm, Go, ecc.).","inputSchema":{"type":"object","properties":{"package":{"type":"string","description":"Nome pacchetto"},"ecosystem":{"type":"string","description":"Ecosistema: PyPI|npm|Go|Maven|RubyGems|NuGet|crates.io (default: PyPI)","default":"PyPI"},"version":{"type":"string","description":"Versione specifica (opzionale)","default":""}},"required":["package"]}},
    {"name":"cvss_explain","description":"Spiega in linguaggio naturale un vettore CVSS v3 (es. 'CVSS:3.1/AV:N/AC:L/PR:N/UI:N/S:U/C:H/I:H/A:H').","inputSchema":{"type":"object","properties":{"vector":{"type":"string","description":"Stringa vettore CVSS v3"}},"required":["vector"]}},
    {"name":"recent_critical","description":"Mostra i CVE con severity CRITICAL pubblicati negli ultimi N giorni.","inputSchema":{"type":"object","properties":{"days":{"type":"integer","description":"Giorni da guardare indietro (default: 7, max: 30)","default":7},"n":{"type":"integer","description":"Numero risultati (default: 10)","default":10}}}},
    {"name":"check_version","description":"Controlla se una versione specifica di un software/libreria ha CVE noti (NVD + OSV).","inputSchema":{"type":"object","properties":{"software":{"type":"string","description":"Nome software (es. 'openssl', 'requests', 'Qt')"},"version":{"type":"string","description":"Versione da controllare (es. '3.0.1')"}},"required":["software","version"]}},
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
            resp = _rpc_result(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"cve-lookup","version":"1.0.0"},"capabilities":{"tools":{}}})
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
