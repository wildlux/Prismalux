#!/usr/bin/env python3
"""
changelog_mcp — Generatore CHANGELOG basato su Conventional Commits
====================================================================
Raggruppa i commit per tipo (feat/fix/docs/…), suggerisce semver bump,
formatta CHANGELOG.md e valida i messaggi non conformi.

Tools:
  list_unreleased    — commit dall'ultimo tag che non sono nel CHANGELOG
  generate_entry     — genera il blocco markdown per una release
  validate_commits   — verifica conformità Conventional Commits
  suggest_version    — suggerisce il prossimo numero semver
  format_release     — scrive/aggiorna CHANGELOG.md
"""
import sys, json, os, re, asyncio, subprocess
from pathlib import Path
from datetime import date
from typing import Any

ROOT = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
CHANGELOG = ROOT / "CHANGELOG.md"

# Conventional Commits type → sezione markdown
CC_MAP = {
    "feat":     ("✨ Nuove funzionalità",  "MINOR"),
    "fix":      ("🐛 Correzioni bug",      "PATCH"),
    "perf":     ("⚡ Performance",          "PATCH"),
    "refactor": ("♻️  Refactoring",         "PATCH"),
    "docs":     ("📝 Documentazione",       "PATCH"),
    "test":     ("🧪 Test",                 "PATCH"),
    "build":    ("🏗️  Build e dipendenze",  "PATCH"),
    "ci":       ("👷 CI/CD",                "PATCH"),
    "chore":    ("🔧 Manutenzione",         "PATCH"),
    "style":    ("💄 Stile",                "PATCH"),
    "revert":   ("⏪ Revert",               "PATCH"),
}
BREAKING_BUMP = "MAJOR"

CC_RE = re.compile(
    r'^(?P<type>\w+)(?:\((?P<scope>[^)]+)\))?(?P<breaking>!)?\s*:\s*(?P<desc>.+)$'
)

def _run(cmd: list[str], cwd: Path = ROOT) -> tuple[int, str]:
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, cwd=str(cwd), timeout=15)
        return r.returncode, (r.stdout + r.stderr).strip()
    except (FileNotFoundError, subprocess.TimeoutExpired) as e:
        return -1, str(e)

def _last_tag() -> str | None:
    code, out = _run(["git", "describe", "--tags", "--abbrev=0"])
    return out.strip() if code == 0 and out.strip() else None

def _commits_since(ref: str | None) -> list[dict]:
    if ref:
        fmt_cmd = ["git", "log", f"{ref}..HEAD", "--format=%H%x1f%s%x1f%b%x1e"]
    else:
        fmt_cmd = ["git", "log", "--format=%H%x1f%s%x1f%b%x1e"]
    code, out = _run(fmt_cmd)
    if code != 0 or not out.strip():
        return []
    records = []
    for block in out.split("\x1e"):
        block = block.strip()
        if not block: continue
        parts = block.split("\x1f")
        if len(parts) < 2: continue
        hash_, subject = parts[0].strip(), parts[1].strip()
        body = parts[2].strip() if len(parts) > 2 else ""
        records.append({"hash": hash_[:7], "subject": subject, "body": body})
    return records

def _parse_cc(subject: str) -> dict | None:
    m = CC_RE.match(subject)
    if not m: return None
    return {
        "type":     m.group("type"),
        "scope":    m.group("scope") or "",
        "breaking": bool(m.group("breaking")) or "BREAKING CHANGE" in subject,
        "desc":     m.group("desc"),
    }

def _group_commits(commits: list[dict]) -> dict:
    groups: dict[str, list[str]] = {k: [] for k in CC_MAP}
    groups["other"] = []
    breaking = []
    for c in commits:
        cc = _parse_cc(c["subject"])
        if not cc:
            groups["other"].append(c["subject"])
            continue
        if cc["breaking"]:
            breaking.append(f"**{cc['desc']}** ({c['hash']})")
        bucket = cc["type"] if cc["type"] in groups else "other"
        scope_str = f"**{cc['scope']}**: " if cc["scope"] else ""
        groups[bucket].append(f"{scope_str}{cc['desc']} ({c['hash']})")
    return {"groups": groups, "breaking": breaking}

def _semver_bump(groups: dict, breaking: list) -> str:
    code, out = _run(["git", "describe", "--tags", "--abbrev=0"])
    current = out.strip() if code == 0 else "v0.0.0"
    ver = re.search(r'(\d+)\.(\d+)\.(\d+)', current)
    major, minor, patch = (int(ver.group(i)) for i in (1, 2, 3)) if ver else (0, 0, 0)
    if breaking:          return f"v{major+1}.0.0"
    if groups.get("feat"): return f"v{major}.{minor+1}.0"
    return f"v{major}.{minor}.{patch+1}"

# ─── handlers ────────────────────────────────────────────────────────────────

def handle_list_unreleased(args: dict) -> str:
    limit = int(args.get("limit", 30))
    tag   = _last_tag()
    commits = _commits_since(tag)[:limit]
    if not commits:
        return f"Nessun commit unreleased{' dopo ' + tag if tag else ''}."

    lines = [f"📋 Commit unreleased{'  (dopo ' + tag + ')' if tag else '  (tutti)'}:\n"]
    for c in commits:
        cc = _parse_cc(c["subject"])
        if cc:
            icon = "✨" if cc["type"]=="feat" else ("🐛" if cc["type"]=="fix" else "🔧")
            brk  = " ⚠️ BREAKING" if cc["breaking"] else ""
            lines.append(f"  {icon} [{c['hash']}] {c['subject']}{brk}")
        else:
            lines.append(f"  ❓ [{c['hash']}] {c['subject']}  ← non Conventional Commit")
    return "\n".join(lines)


def handle_validate_commits(args: dict) -> str:
    since = args.get("since", "")
    commits = _commits_since(since or _last_tag())
    if not commits:
        return "Nessun commit da validare."

    ok, bad = [], []
    for c in commits:
        if _parse_cc(c["subject"]): ok.append(c)
        else: bad.append(c)

    lines = [f"✅ Conformi: {len(ok)}   ❌ Non conformi: {len(bad)}\n"]
    if bad:
        lines.append("Formato atteso: <tipo>(<scope>): <descrizione>")
        lines.append("Tipi validi: " + ", ".join(CC_MAP.keys()) + "\n")
        for c in bad:
            lines.append(f"  ❌ [{c['hash']}] {c['subject']}")
    return "\n".join(lines)


def handle_suggest_version(args: dict) -> str:
    tag     = _last_tag()
    commits = _commits_since(tag)
    data    = _group_commits(commits)
    nxt     = _semver_bump(data["groups"], data["breaking"])

    lines = [f"🏷  Versione suggerita: {nxt}\n"]
    lines.append(f"  Tag corrente: {tag or 'nessuno'}")
    lines.append(f"  Commit analizzati: {len(commits)}")
    if data["breaking"]:
        lines.append(f"  ⚠️ {len(data['breaking'])} BREAKING CHANGE → bump MAJOR")
    elif data["groups"].get("feat"):
        lines.append(f"  ✨ {len(data['groups']['feat'])} feat → bump MINOR")
    else:
        lines.append("  🐛 solo fix/chore → bump PATCH")
    return "\n".join(lines)


def handle_generate_entry(args: dict) -> str:
    version = args.get("version", "").strip()
    tag     = _last_tag()
    commits = _commits_since(tag)
    data    = _group_commits(commits)

    if not version:
        version = _semver_bump(data["groups"], data["breaking"])

    today = date.today().isoformat()
    lines = [f"## [{version}] — {today}\n"]

    if data["breaking"]:
        lines.append("### ⚠️ BREAKING CHANGES\n")
        for b in data["breaking"]: lines.append(f"- {b}")
        lines.append("")

    for key, (title, _) in CC_MAP.items():
        items = data["groups"].get(key, [])
        if items:
            lines.append(f"### {title}\n")
            for item in items: lines.append(f"- {item}")
            lines.append("")

    others = data["groups"].get("other", [])
    if others:
        lines.append("### Altro\n")
        for o in others: lines.append(f"- {o}")
        lines.append("")

    return "\n".join(lines)


def handle_format_release(args: dict) -> str:
    version  = args.get("version", "").strip()
    dry_run  = bool(args.get("dry_run", True))

    entry = handle_generate_entry({"version": version})
    if dry_run:
        return f"[DRY RUN — non scritto su disco]\n\n{entry}"

    # Leggi CHANGELOG esistente e prepend
    existing = CHANGELOG.read_text(errors="replace") if CHANGELOG.exists() else "# CHANGELOG\n\n"
    # Rimuovi l'intestazione e riassembla
    body = re.sub(r'^# CHANGELOG\n+', '', existing)
    new_content = f"# CHANGELOG\n\n{entry}{body}"
    CHANGELOG.write_text(new_content, encoding="utf-8")

    tag     = _last_tag()
    commits = _commits_since(tag)
    data    = _group_commits(commits)
    ver     = version or _semver_bump(data["groups"], data["breaking"])
    return (f"✅ CHANGELOG.md aggiornato con {ver}.\n"
            f"Prossimo step:\n"
            f"  git add CHANGELOG.md\n"
            f"  git commit -m 'chore: update CHANGELOG for {ver}'\n"
            f"  git tag {ver}")


HANDLERS = {
    "list_unreleased":  handle_list_unreleased,
    "validate_commits": handle_validate_commits,
    "suggest_version":  handle_suggest_version,
    "generate_entry":   handle_generate_entry,
    "format_release":   handle_format_release,
}
TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"list_unreleased","description":"Elenca i commit unreleased dall'ultimo tag git con icona per tipo (feat/fix/…).","inputSchema":{"type":"object","properties":{"limit":{"type":"integer","default":30}}}},
    {"name":"validate_commits","description":"Verifica che i commit rispettino il formato Conventional Commits (tipo: descrizione).","inputSchema":{"type":"object","properties":{"since":{"type":"string","default":"","description":"Ref da cui partire (default: ultimo tag)"}}}},
    {"name":"suggest_version","description":"Suggerisce il prossimo numero di versione semver basandosi sui commit unreleased.","inputSchema":{"type":"object","properties":{}}},
    {"name":"generate_entry","description":"Genera il blocco markdown per il CHANGELOG della release corrente (o versione specificata).","inputSchema":{"type":"object","properties":{"version":{"type":"string","default":"","description":"Versione (es. v3.0.0). Default: calcolata automaticamente."}}}},
    {"name":"format_release","description":"Scrive il blocco release nel file CHANGELOG.md (dry_run=true di default per sicurezza).","inputSchema":{"type":"object","properties":{"version":{"type":"string","default":""},"dry_run":{"type":"boolean","default":True}}}}
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
            resp = _ok(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"changelog","version":"1.0.0"},"capabilities":{"tools":{}}})
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
