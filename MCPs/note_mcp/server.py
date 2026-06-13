#!/usr/bin/env python3
"""
note_mcp — Note rapide con tag, ricerca e reminder
====================================================
Salva note di testo con tag e scadenza in SQLite.
Complementare a chat_memory_mcp: le note sono intenzionali
(cose da ricordare, TODO, idee), la chat_memory è automatica.

Tools:
  add_note      — aggiunge nota con titolo, corpo, tag, scadenza
  search_notes  — ricerca per keyword o tag
  get_note      — mostra nota per ID
  list_notes    — elenca note (filtro: tag, scadenza, recenti)
  update_note   — modifica corpo o tag di una nota
  delete_note   — elimina nota per ID
  due_today     — note con scadenza oggi o scadute
"""
import sys, json, os, re, asyncio, sqlite3
from pathlib import Path
from datetime import datetime, date, timedelta
from typing import Any

ROOT    = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
DB_DIR  = Path.home() / ".prismalux"
DB_PATH = DB_DIR / "notes.db"

def _conn() -> sqlite3.Connection:
    DB_DIR.mkdir(parents=True, exist_ok=True)
    c = sqlite3.connect(str(DB_PATH))
    c.row_factory = sqlite3.Row
    c.execute("""
        CREATE TABLE IF NOT EXISTS notes (
            id       INTEGER PRIMARY KEY AUTOINCREMENT,
            created  TEXT NOT NULL DEFAULT (datetime('now','localtime')),
            updated  TEXT NOT NULL DEFAULT (datetime('now','localtime')),
            title    TEXT NOT NULL,
            body     TEXT NOT NULL DEFAULT '',
            tags     TEXT NOT NULL DEFAULT '',
            due_date TEXT NOT NULL DEFAULT '',
            done     INTEGER NOT NULL DEFAULT 0
        )
    """)
    c.execute("CREATE INDEX IF NOT EXISTS idx_tags ON notes(tags)")
    c.execute("CREATE INDEX IF NOT EXISTS idx_due  ON notes(due_date)")
    c.commit()
    return c

def _parse_due(s: str) -> str:
    """Converte hint data in ISO (oggi, domani, YYYY-MM-DD, +Nd)."""
    s = s.strip().lower()
    if not s: return ""
    if s in ("oggi","today"):   return str(date.today())
    if s in ("domani","tomorrow"): return str(date.today() + timedelta(days=1))
    if s == "dopodomani":       return str(date.today() + timedelta(days=2))
    m = re.match(r'\+(\d+)([dg]?)', s)
    if m: return str(date.today() + timedelta(days=int(m.group(1))))
    # ISO o it: YYYY-MM-DD o DD/MM/YYYY
    for fmt in ("%Y-%m-%d","%d/%m/%Y","%d-%m-%Y"):
        try: return datetime.strptime(s, fmt).strftime("%Y-%m-%d")
        except: pass
    return s  # ritorna as-is

def _fmt_note(r, short: bool = False) -> str:
    done_icon = "✅" if r["done"] else "📝"
    due  = f"  📅 Scadenza: {r['due_date']}" if r["due_date"] else ""
    tags = f"  🏷️  {r['tags']}" if r["tags"] else ""
    body_preview = r["body"][:200] if short else r["body"]
    if short and len(r["body"]) > 200: body_preview += "..."
    lines = [f"{done_icon} #{r['id']} [{r['created'][:16]}] {r['title']}"]
    if r["body"]: lines.append(f"  {body_preview}")
    if due:  lines.append(due)
    if tags: lines.append(tags)
    return "\n".join(lines)

# ─── handlers ────────────────────────────────────────────────────────────────

def handle_add_note(args: dict) -> str:
    title    = args.get("title","").strip()
    body     = args.get("body","").strip()
    tags     = args.get("tags","").strip().lower()
    due_raw  = args.get("due","").strip()
    if not title: return "Specifica 'title'."
    due = _parse_due(due_raw)
    c   = _conn()
    cur = c.execute(
        "INSERT INTO notes (title, body, tags, due_date) VALUES (?,?,?,?)",
        (title, body, tags, due)
    )
    c.commit()
    rid = cur.lastrowid
    ts  = c.execute("SELECT created FROM notes WHERE id=?", (rid,)).fetchone()["created"]
    c.close()
    due_str = f"  Scadenza: {due}" if due else ""
    return (f"✅ Nota #{rid} salvata [{ts[:16]}]\n"
            f"  {title}\n"
            f"  {body[:80] if body else '(nessun corpo)'}"
            f"{due_str}")

def handle_search_notes(args: dict) -> str:
    query = args.get("query","").strip()
    tag   = args.get("tag","").strip().lower()
    limit = min(int(args.get("limit", 20)), 100)
    done  = args.get("done", None)  # None=tutti, True=completate, False=aperte
    c     = _conn()
    clauses, params = [], []
    if query:
        clauses.append("(lower(title) LIKE ? OR lower(body) LIKE ?)")
        params += [f"%{query.lower()}%", f"%{query.lower()}%"]
    if tag:
        clauses.append("lower(tags) LIKE ?")
        params.append(f"%{tag}%")
    if done is not None:
        clauses.append("done = ?")
        params.append(1 if done else 0)
    where = ("WHERE " + " AND ".join(clauses)) if clauses else ""
    rows  = c.execute(
        f"SELECT * FROM notes {where} ORDER BY created DESC LIMIT ?",
        params + [limit]
    ).fetchall()
    c.close()
    if not rows: return f"Nessuna nota trovata per '{query or tag}'."
    lines = [f"🔍 {len(rows)} note trovate\n"]
    for r in rows: lines.append(_fmt_note(r, short=True)); lines.append("")
    return "\n".join(lines)

def handle_get_note(args: dict) -> str:
    nid = args.get("id")
    if nid is None: return "Specifica 'id' della nota."
    c   = _conn()
    r   = c.execute("SELECT * FROM notes WHERE id=?", (int(nid),)).fetchone()
    c.close()
    if not r: return f"Nota #{nid} non trovata."
    return _fmt_note(r, short=False)

def handle_list_notes(args: dict) -> str:
    limit     = min(int(args.get("limit", 20)), 100)
    tag       = args.get("tag","").strip().lower()
    show_done = bool(args.get("show_done", False))
    c         = _conn()
    clauses, params = [], []
    if not show_done:
        clauses.append("done = 0")
    if tag:
        clauses.append("lower(tags) LIKE ?")
        params.append(f"%{tag}%")
    where = ("WHERE " + " AND ".join(clauses)) if clauses else ""
    rows  = c.execute(
        f"SELECT * FROM notes {where} ORDER BY due_date ASC, created DESC LIMIT ?",
        params + [limit]
    ).fetchall()
    total = c.execute("SELECT COUNT(*) FROM notes WHERE done=0").fetchone()[0]
    c.close()
    if not rows: return "Nessuna nota aperta."
    lines = [f"📋 Note ({len(rows)} mostrate, {total} aperte totali)\n"]
    for r in rows: lines.append(_fmt_note(r, short=True)); lines.append("")
    return "\n".join(lines)

def handle_update_note(args: dict) -> str:
    nid      = args.get("id")
    new_body = args.get("body", None)
    new_tags = args.get("tags", None)
    new_title= args.get("title", None)
    done     = args.get("done", None)
    due      = args.get("due", None)
    if nid is None: return "Specifica 'id'."
    c   = _conn()
    r   = c.execute("SELECT * FROM notes WHERE id=?", (int(nid),)).fetchone()
    if not r:
        c.close(); return f"Nota #{nid} non trovata."
    now     = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    updates, params = ["updated=?"], [now]
    if new_body  is not None: updates.append("body=?");     params.append(new_body)
    if new_tags  is not None: updates.append("tags=?");     params.append(new_tags.lower())
    if new_title is not None: updates.append("title=?");    params.append(new_title)
    if done      is not None: updates.append("done=?");     params.append(1 if done else 0)
    if due       is not None: updates.append("due_date=?"); params.append(_parse_due(due))
    c.execute(f"UPDATE notes SET {', '.join(updates)} WHERE id=?", params + [int(nid)])
    c.commit()
    r2 = c.execute("SELECT * FROM notes WHERE id=?", (int(nid),)).fetchone()
    c.close()
    return f"✅ Nota #{nid} aggiornata\n{_fmt_note(r2, short=True)}"

def handle_delete_note(args: dict) -> str:
    nid     = args.get("id")
    dry_run = bool(args.get("dry_run", True))
    if nid is None: return "Specifica 'id'."
    c = _conn()
    r = c.execute("SELECT * FROM notes WHERE id=?", (int(nid),)).fetchone()
    if not r:
        c.close(); return f"Nota #{nid} non trovata."
    if not dry_run:
        c.execute("DELETE FROM notes WHERE id=?", (int(nid),))
        c.commit()
    c.close()
    pfx = "[DRY RUN] " if dry_run else ""
    return f"{pfx}{'✅ Eliminata' if not dry_run else 'Eliminerei'} nota #{nid}: {r['title']}"

def handle_due_today(args: dict) -> str:
    today = str(date.today())
    c     = _conn()
    rows  = c.execute(
        "SELECT * FROM notes WHERE due_date != '' AND due_date <= ? AND done=0 ORDER BY due_date ASC",
        (today,)
    ).fetchall()
    c.close()
    if not rows: return f"✅ Nessuna nota in scadenza oggi ({today})."
    overdue = [r for r in rows if r["due_date"] < today]
    due_now = [r for r in rows if r["due_date"] == today]
    lines   = [f"📅 Note in scadenza ({today})\n"]
    if overdue:
        lines.append(f"⚠️  SCADUTE ({len(overdue)}):")
        for r in overdue: lines.append("  " + _fmt_note(r, short=True).replace("\n","\n  ")); lines.append("")
    if due_now:
        lines.append(f"📌 OGGI ({len(due_now)}):")
        for r in due_now: lines.append("  " + _fmt_note(r, short=True).replace("\n","\n  ")); lines.append("")
    return "\n".join(lines)

HANDLERS = {
    "add_note":     handle_add_note,
    "search_notes": handle_search_notes,
    "get_note":     handle_get_note,
    "list_notes":   handle_list_notes,
    "update_note":  handle_update_note,
    "delete_note":  handle_delete_note,
    "due_today":    handle_due_today,
}
TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"add_note","description":"Aggiunge una nota con titolo, corpo, tag e scadenza opzionale (es. due='domani', 'oggi', '+3d', '2026-07-01').","inputSchema":{"type":"object","properties":{"title":{"type":"string"},"body":{"type":"string","default":""},"tags":{"type":"string","default":""},"due":{"type":"string","default":""}},"required":["title"]}},
    {"name":"search_notes","description":"Cerca note per keyword nel titolo/corpo, o per tag.","inputSchema":{"type":"object","properties":{"query":{"type":"string","default":""},"tag":{"type":"string","default":""},"limit":{"type":"integer","default":20},"done":{"type":"boolean","nullable":True}}}},
    {"name":"get_note","description":"Mostra il contenuto completo di una nota per ID.","inputSchema":{"type":"object","properties":{"id":{"type":"integer"}},"required":["id"]}},
    {"name":"list_notes","description":"Elenca le note aperte (o tutte), opzionalmente filtrate per tag, ordinate per scadenza.","inputSchema":{"type":"object","properties":{"limit":{"type":"integer","default":20},"tag":{"type":"string","default":""},"show_done":{"type":"boolean","default":False}}}},
    {"name":"update_note","description":"Aggiorna titolo, corpo, tag, scadenza o stato (done) di una nota.","inputSchema":{"type":"object","properties":{"id":{"type":"integer"},"title":{"type":"string"},"body":{"type":"string"},"tags":{"type":"string"},"due":{"type":"string"},"done":{"type":"boolean"}},"required":["id"]}},
    {"name":"delete_note","description":"Elimina una nota per ID (dry_run=true di default).","inputSchema":{"type":"object","properties":{"id":{"type":"integer"},"dry_run":{"type":"boolean","default":True}},"required":["id"]}},
    {"name":"due_today","description":"Mostra le note in scadenza oggi e quelle già scadute.","inputSchema":{"type":"object","properties":{}}},
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
            resp = _ok(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"note","version":"1.0.0"},"capabilities":{"tools":{}}})
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
