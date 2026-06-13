#!/usr/bin/env python3
"""
license_checker_mcp — Verifica licenze dipendenze Python + sistema
===================================================================
Recupera metadati licenza da PyPI, verifica compatibilità GPL/MIT/Apache,
genera file NOTICE con attribuzioni, segnala pacchetti senza licenza.

Tools:
  check_requirements   — licenze di tutti i pacchetti in requirements.txt
  check_installed      — licenze dei pacchetti installati nell'ambiente
  lookup_license       — licenza di un singolo pacchetto da PyPI
  check_compatibility  — rileva conflitti GPL/LGPL vs codice proprietario
  generate_notice      — genera NOTICE.txt con tutte le attribuzioni
"""
import sys, json, os, re, asyncio, subprocess, urllib.request, urllib.error
from pathlib import Path
from typing import Any

ROOT = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
REQ  = ROOT / "requirements.txt"

# Licenze note e loro categoria
LICENSE_CATEGORIES: dict[str, str] = {
    # Permissive
    "MIT":          "permissive",
    "BSD":          "permissive",
    "BSD-2-Clause": "permissive",
    "BSD-3-Clause": "permissive",
    "Apache":       "permissive",
    "Apache-2.0":   "permissive",
    "ISC":          "permissive",
    "Unlicense":    "permissive",
    "CC0":          "permissive",
    "PSF":          "permissive",
    "LGPL":         "weak-copyleft",
    "LGPL-2.0":     "weak-copyleft",
    "LGPL-2.1":     "weak-copyleft",
    "LGPL-3.0":     "weak-copyleft",
    "MPL":          "weak-copyleft",
    "MPL-2.0":      "weak-copyleft",
    "EPL":          "weak-copyleft",
    "GPL":          "copyleft",
    "GPL-2.0":      "copyleft",
    "GPL-3.0":      "copyleft",
    "AGPL":         "copyleft",
    "AGPL-3.0":     "copyleft",
}

COMPAT_NOTES = {
    "copyleft":      "🔴 GPL/AGPL: richiede che tutto il codice linkato sia open source. NON usare in prodotto commerciale chiuso.",
    "weak-copyleft": "🟡 LGPL/MPL: può essere linkato dinamicamente senza GPL contagio. Verifica le condizioni specifiche.",
    "permissive":    "✅ Licenza permissiva: uso libero anche in prodotti commerciali con attribuzione.",
    "unknown":       "⚪ Licenza non riconosciuta — verifica manualmente.",
}

def _pypi_info(pkg: str) -> dict:
    pkg_clean = re.sub(r'[>=<!\[\]~^].*$', '', pkg).strip()
    if not pkg_clean: return {}
    url = f"https://pypi.org/pypi/{pkg_clean}/json"
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "prismalux-license-checker/1.0"})
        with urllib.request.urlopen(req, timeout=8) as resp:
            data = json.loads(resp.read())
        info = data.get("info", {})
        return {
            "name":     info.get("name", pkg_clean),
            "version":  info.get("version", "?"),
            "license":  info.get("license", "") or "",
            "home":     info.get("home_page", "") or info.get("project_url", "") or "",
            "author":   info.get("author", "") or info.get("maintainer", "") or "",
            "summary":  info.get("summary", ""),
        }
    except Exception as e:
        return {"name": pkg_clean, "error": str(e)}

def _categorize(license_str: str) -> str:
    ls = license_str.upper()
    for key, cat in LICENSE_CATEGORIES.items():
        if key.upper() in ls: return cat
    return "unknown"

def _parse_requirements(path: Path) -> list[str]:
    pkgs = []
    for line in path.read_text(errors="replace").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or line.startswith("-r"): continue
        pkg = re.split(r'[>=<!\[\]~]', line)[0].strip()
        if pkg: pkgs.append(pkg)
    return pkgs

def _installed_licenses() -> list[dict]:
    code, out = subprocess.run(
        [sys.executable, "-m", "pip", "list", "--format=json"],
        capture_output=True, text=True, timeout=15
    ), None
    try:
        pkgs = json.loads(code.stdout)
    except Exception:
        return []
    # usa importlib.metadata se disponibile
    results = []
    try:
        import importlib.metadata as im
        for p in pkgs:
            name = p.get("name","")
            try:
                meta = im.metadata(name)
                lic  = meta.get("License","") or ""
                results.append({"name": name, "version": p.get("version",""), "license": lic})
            except Exception:
                results.append({"name": name, "version": p.get("version",""), "license": "?"})
    except ImportError:
        for p in pkgs:
            results.append({"name": p.get("name",""), "version": p.get("version",""), "license": "?"})
    return results

# ─── handlers ────────────────────────────────────────────────────────────────

def handle_check_requirements(args: dict) -> str:
    req_path = Path(args.get("requirements", str(REQ)))
    if not req_path.is_absolute(): req_path = ROOT / req_path
    if not req_path.exists(): return f"File non trovato: {req_path}"

    pkgs = _parse_requirements(req_path)
    if not pkgs: return "Nessun pacchetto trovato nel file."

    lines = [f"📦 Licenze — {req_path.name} ({len(pkgs)} pacchetti)\n"]
    problem_pkgs = []
    for pkg in pkgs:
        info = _pypi_info(pkg)
        if "error" in info:
            lines.append(f"  ⚪ {pkg:<30}  [errore fetch: {info['error'][:40]}]")
            continue
        lic  = info.get("license","") or "SCONOSCIUTA"
        cat  = _categorize(lic)
        icon = {"permissive":"✅","weak-copyleft":"🟡","copyleft":"🔴","unknown":"⚪"}.get(cat,"⚪")
        ver  = info.get("version","?")
        lines.append(f"  {icon} {pkg:<30}  {ver:<10}  {lic}")
        if cat in ("copyleft","unknown"): problem_pkgs.append((pkg, lic, cat))

    if problem_pkgs:
        lines.append(f"\n⚠️  {len(problem_pkgs)} pacchetti da verificare:")
        for p, l, c in problem_pkgs:
            lines.append(f"  → {p}: {l}")
            lines.append(f"     {COMPAT_NOTES[c]}")
    return "\n".join(lines)


def handle_check_installed(args: dict) -> str:
    installed = _installed_licenses()
    if not installed: return "Nessun pacchetto installato rilevato."

    copyleft = [p for p in installed if _categorize(p.get("license","")) == "copyleft"]
    unknown  = [p for p in installed if _categorize(p.get("license","")) == "unknown"]

    lines = [f"🔍 Pacchetti installati: {len(installed)}  🔴 GPL: {len(copyleft)}  ⚪ Sconosciute: {len(unknown)}\n"]
    if copyleft:
        lines.append("🔴 Licenze copyleft rilevate:")
        for p in copyleft:
            lines.append(f"  🔴 {p['name']:<30} {p['version']:<12} {p.get('license','')}")
    if unknown:
        lines.append("\n⚪ Licenze sconosciute (verifica manualmente):")
        for p in unknown[:15]:
            lines.append(f"  ⚪ {p['name']:<30} {p['version']:<12} {p.get('license','?')}")
    if not copyleft and not unknown:
        lines.append("✅ Tutte le licenze sono permissive o LGPL.")
    return "\n".join(lines)


def handle_lookup_license(args: dict) -> str:
    pkg = args.get("package","").strip()
    if not pkg: return "Specifica 'package'."
    info = _pypi_info(pkg)
    if "error" in info: return f"Errore PyPI: {info['error']}"
    lic  = info.get("license","") or "Non specificata"
    cat  = _categorize(lic)
    icon = {"permissive":"✅","weak-copyleft":"🟡","copyleft":"🔴","unknown":"⚪"}.get(cat,"⚪")
    return (f"{icon} {info['name']} v{info['version']}\n"
            f"  Licenza  : {lic}\n"
            f"  Categoria: {cat}\n"
            f"  Note     : {COMPAT_NOTES[cat]}\n"
            f"  Autore   : {info.get('author','')}\n"
            f"  Home page: {info.get('home','')}\n"
            f"  Sommario : {info.get('summary','')[:120]}")


def handle_check_compatibility(args: dict) -> str:
    req_path = Path(args.get("requirements", str(REQ)))
    if not req_path.is_absolute(): req_path = ROOT / req_path
    if not req_path.exists(): return f"File non trovato: {req_path}"

    pkgs = _parse_requirements(req_path)
    lines = [f"⚖️  Analisi compatibilità licenze — {req_path.name}\n"]

    copyleft_list, weak_list = [], []
    for pkg in pkgs:
        info = _pypi_info(pkg)
        lic  = info.get("license","") or "?"
        cat  = _categorize(lic)
        if cat == "copyleft":    copyleft_list.append((pkg, lic))
        elif cat == "weak-copyleft": weak_list.append((pkg, lic))

    if not copyleft_list and not weak_list:
        lines.append("✅ Nessun conflitto di licenza rilevato. Tutte le dipendenze sono permissive.")
        return "\n".join(lines)

    if copyleft_list:
        lines.append(f"🔴 ATTENZIONE — {len(copyleft_list)} dipendenze GPL/AGPL:")
        for p, l in copyleft_list:
            lines.append(f"  🔴 {p}: {l}")
        lines.append("")
        lines.append("  → Se Prismalux è distribuito come software proprietario/commerciale,")
        lines.append("    queste dipendenze DEVONO essere sostituite con alternative permissive.")
        lines.append("  → Se Prismalux rimane open source (GPL-compatible), nessuna azione.")
        lines.append("")

    if weak_list:
        lines.append(f"🟡 VERIFICARE — {len(weak_list)} dipendenze LGPL/MPL:")
        for p, l in weak_list:
            lines.append(f"  🟡 {p}: {l}")
        lines.append("")
        lines.append("  → LGPL: ok se linkato dinamicamente (.so/.dll), non se compilato staticamente.")
        lines.append("  → MPL: modifica ai file MPL vanno condivise, resto del codice privato ok.")

    return "\n".join(lines)


def handle_generate_notice(args: dict) -> str:
    req_path = Path(args.get("requirements", str(REQ)))
    dry_run  = bool(args.get("dry_run", True))
    if not req_path.is_absolute(): req_path = ROOT / req_path
    if not req_path.exists(): return f"File non trovato: {req_path}"

    pkgs  = _parse_requirements(req_path)
    today = __import__("datetime").date.today().isoformat()
    lines = [
        f"NOTICE — Prismalux v2.9",
        f"Generato: {today}",
        f"Sorgente: {req_path.name}",
        "=" * 60,
        "",
        "Prismalux include le seguenti dipendenze open source:",
        "",
    ]

    for pkg in pkgs:
        info = _pypi_info(pkg)
        if "error" in info:
            lines.append(f"  {pkg}: [informazioni non disponibili]")
            continue
        lic  = info.get("license","Licenza non specificata") or "Licenza non specificata"
        auth = info.get("author","") or ""
        home = info.get("home","") or ""
        lines.append(f"{info.get('name', pkg)} {info.get('version','')}")
        lines.append(f"  Licenza: {lic}")
        if auth: lines.append(f"  Autore:  {auth}")
        if home: lines.append(f"  Home:    {home}")
        lines.append("")

    notice_text = "\n".join(lines)
    if dry_run:
        return f"[DRY RUN — non scritto su disco]\n\n{notice_text}"

    notice_path = ROOT / "NOTICE.txt"
    notice_path.write_text(notice_text, encoding="utf-8")
    return f"✅ NOTICE.txt scritto ({len(pkgs)} pacchetti documentati).\n  Path: {notice_path}"


HANDLERS = {
    "check_requirements":  handle_check_requirements,
    "check_installed":     handle_check_installed,
    "lookup_license":      handle_lookup_license,
    "check_compatibility": handle_check_compatibility,
    "generate_notice":     handle_generate_notice,
}
TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"check_requirements","description":"Recupera la licenza di ogni pacchetto in requirements.txt tramite PyPI e segnala licenze copyleft/sconosciute.","inputSchema":{"type":"object","properties":{"requirements":{"type":"string","default":"","description":"Path requirements.txt (default: Prismalux/requirements.txt)"}}}},
    {"name":"check_installed","description":"Elenca le licenze dei pacchetti Python installati nell'ambiente corrente, evidenziando GPL e licenze sconosciute.","inputSchema":{"type":"object","properties":{}}},
    {"name":"lookup_license","description":"Recupera la licenza di un singolo pacchetto da PyPI con spiegazione delle implicazioni.","inputSchema":{"type":"object","properties":{"package":{"type":"string"}},"required":["package"]}},
    {"name":"check_compatibility","description":"Analizza i conflitti di compatibilità tra le licenze delle dipendenze (GPL vs codice proprietario).","inputSchema":{"type":"object","properties":{"requirements":{"type":"string","default":""}}}},
    {"name":"generate_notice","description":"Genera il file NOTICE.txt con tutte le attribuzioni delle dipendenze (dry_run=true di default).","inputSchema":{"type":"object","properties":{"requirements":{"type":"string","default":""},"dry_run":{"type":"boolean","default":True}}}},
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
            resp = _ok(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"license-checker","version":"1.0.0"},"capabilities":{"tools":{}}})
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
