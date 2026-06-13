#!/usr/bin/env python3
"""
sqlite_inspector_mcp — Ispezione database SQLite Prismalux
===========================================================
Accede a graph_memory.db, rag_graph.db e qualsiasi altro DB
in ~/.prismalux/ senza aprire tool esterni.

Tools:
  list_databases   — elenca tutti i DB Prismalux
  schema           — struttura tabelle di un DB
  query            — esegue SELECT (read-only, no DDL/DML)
  stats            — statistiche nodi/archi GraphMemory
  vacuum           — VACUUM + ANALYZE per ottimizzare
  export_csv       — esporta una tabella in CSV
"""
import sys, json, os, re, asyncio, sqlite3, csv, io
from pathlib import Path
from typing import Any

ROOT    = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
DB_DIR  = Path.home() / ".prismalux"
KNOWN   = {
    "graph_memory": DB_DIR / "graph_memory.db",
    "rag_graph":    DB_DIR / "rag_graph.db",
}

def _resolve_db(name: str) -> Path | None:
    if not name:
        # Primo DB trovato
        for p in KNOWN.values():
            if p.exists(): return p
        return None
    p = Path(name) if Path(name).is_absolute() else DB_DIR / name
    if p.exists(): return p
    # Prova con .db
    p2 = DB_DIR / (name + ".db")
    return p2 if p2.exists() else None

def _conn(path: Path) -> sqlite3.Connection:
    c = sqlite3.connect(f"file:{path}?mode=ro", uri=True)
    c.row_factory = sqlite3.Row
    return c

def _safe_sql(sql: str) -> bool:
    """Blocca qualsiasi statement non SELECT."""
    s = sql.strip().upper()
    return s.startswith("SELECT") or s.startswith("PRAGMA") or s.startswith("WITH")

# ─── handlers ────────────────────────────────────────────────────────────────

def handle_list_databases(args: dict) -> str:
    lines = [f"💾 Database Prismalux ({DB_DIR})\n"]
    found = False
    for name, path in KNOWN.items():
        if path.exists():
            found = True
            size = path.stat().st_size / 1024
            conn = _conn(path)
            tables = [r[0] for r in conn.execute("SELECT name FROM sqlite_master WHERE type='table'")]
            conn.close()
            lines.append(f"  ✅ {name:<18} {size:>8.1f} KB   tabelle: {', '.join(tables)}")
        else:
            lines.append(f"  ⚪ {name:<18} (non esiste ancora)")
    # Cerca altri DB nella cartella
    for p in DB_DIR.glob("*.db"):
        if p not in KNOWN.values():
            found = True
            size = p.stat().st_size / 1024
            lines.append(f"  🔵 {p.stem:<18} {size:>8.1f} KB   (extra)")
    if not found:
        lines.append("  Nessun database trovato. Avvia Prismalux almeno una volta per crearli.")
    return "\n".join(lines)

def handle_schema(args: dict) -> str:
    db = _resolve_db(args.get("db",""))
    if not db: return f"DB non trovato in {DB_DIR}. Usa list_databases() per vedere quelli disponibili."
    conn = _conn(db)
    lines = [f"🗂  Schema — {db.name}\n"]
    tables = [r[0] for r in conn.execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name")]
    for tbl in tables:
        lines.append(f"  📋 {tbl}")
        cols = conn.execute(f"PRAGMA table_info({tbl})")
        for col in cols:
            pk = " PK" if col["pk"] else ""
            nn = " NOT NULL" if col["notnull"] else ""
            lines.append(f"      {col['name']:<20} {col['type']:<12}{pk}{nn}")
        count = conn.execute(f"SELECT COUNT(*) FROM \"{tbl}\"").fetchone()[0]
        lines.append(f"      → {count} righe\n")
    conn.close()
    return "\n".join(lines)

def handle_query(args: dict) -> str:
    db   = _resolve_db(args.get("db",""))
    sql  = args.get("sql","").strip()
    limit = min(int(args.get("limit", 50)), 200)
    if not db: return "DB non trovato. Specifica 'db' oppure usa list_databases()."
    if not sql: return "Specifica 'sql' (solo SELECT/PRAGMA/WITH)."
    if not _safe_sql(sql):
        return "⛔ Solo SELECT, PRAGMA e WITH sono permessi (modalità read-only)."
    # Aggiungi LIMIT se non presente
    if "LIMIT" not in sql.upper():
        sql = sql.rstrip(";") + f" LIMIT {limit}"
    try:
        conn = _conn(db)
        cur  = conn.execute(sql)
        rows = cur.fetchall()
        if not rows: return "Query OK — nessun risultato."
        cols = [d[0] for d in cur.description]
        # Formatta tabella
        widths = [max(len(c), max((len(str(r[i])) for r in rows), default=0)) for i, c in enumerate(cols)]
        widths = [min(w, 40) for w in widths]
        sep    = "  ".join("-" * w for w in widths)
        header = "  ".join(c[:w].ljust(w) for c, w in zip(cols, widths))
        lines  = [f"  {header}", f"  {sep}"]
        for row in rows:
            lines.append("  " + "  ".join(str(row[i])[:w].ljust(w) for i, w in enumerate(widths)))
        lines.append(f"\n  {len(rows)} righe")
        conn.close()
        return "\n".join(lines)
    except sqlite3.Error as e:
        return f"Errore SQLite: {e}"

def handle_stats(args: dict) -> str:
    db = _resolve_db(args.get("db", "graph_memory"))
    if not db: return "graph_memory.db non trovato. Avvia Prismalux una volta."
    try:
        conn = _conn(db)
        lines = [f"📊 Statistiche — {db.name}\n"]
        # Nodi
        if "nodes" in [r[0] for r in conn.execute("SELECT name FROM sqlite_master WHERE type='table'")]:
            n_total = conn.execute("SELECT COUNT(*) FROM nodes").fetchone()[0]
            n_types = conn.execute("SELECT type, COUNT(*) as c FROM nodes GROUP BY type ORDER BY c DESC").fetchall()
            lines.append(f"  Nodi totali: {n_total}")
            for r in n_types[:8]:
                lines.append(f"    {r['type']:<20} {r['c']}")
        # Archi
        if "edges" in [r[0] for r in conn.execute("SELECT name FROM sqlite_master WHERE type='table'")]:
            e_total = conn.execute("SELECT COUNT(*) FROM edges").fetchone()[0]
            e_types = conn.execute("SELECT relation, COUNT(*) as c FROM edges GROUP BY relation ORDER BY c DESC").fetchall()
            lines.append(f"\n  Archi totali: {e_total}")
            for r in e_types[:8]:
                lines.append(f"    {r['relation']:<20} {r['c']}")
        # Nodi più importanti
        try:
            top = conn.execute("SELECT label, type, importance FROM nodes ORDER BY importance DESC LIMIT 5").fetchall()
            if top:
                lines.append("\n  Top nodi per importanza:")
                for r in top:
                    lines.append(f"    [{r['type']}] {r['label'][:40]}  ({r['importance']:.2f})")
        except: pass
        conn.close()
        return "\n".join(lines)
    except sqlite3.Error as e:
        return f"Errore: {e}"

def handle_vacuum(args: dict) -> str:
    db      = _resolve_db(args.get("db",""))
    dry_run = bool(args.get("dry_run", False))
    if not db: return "DB non trovato."
    if dry_run:
        size = db.stat().st_size / 1024
        return f"[DRY RUN] Eseguirei VACUUM su {db.name} ({size:.0f} KB)"
    before = db.stat().st_size
    # VACUUM richiede modalità read-write
    conn = sqlite3.connect(str(db))
    conn.execute("VACUUM")
    conn.execute("ANALYZE")
    conn.close()
    after = db.stat().st_size
    saved = (before - after) / 1024
    return (f"✅ VACUUM + ANALYZE completato su {db.name}\n"
            f"  Prima: {before/1024:.0f} KB  →  Dopo: {after/1024:.0f} KB  (−{saved:.0f} KB)")

def handle_export_csv(args: dict) -> str:
    db      = _resolve_db(args.get("db",""))
    table   = args.get("table","").strip()
    output  = args.get("output","").strip()
    limit   = min(int(args.get("limit", 5000)), 50000)
    if not db: return "DB non trovato."
    if not table: return "Specifica 'table'."
    out_path = Path(output) if output else Path.home() / "Desktop" / f"{db.stem}_{table}.csv"
    try:
        conn = _conn(db)
        rows = conn.execute(f"SELECT * FROM \"{table}\" LIMIT {limit}").fetchall()
        if not rows: return f"Tabella '{table}' vuota o non trovata."
        cols = rows[0].keys()
        buf  = io.StringIO()
        w    = csv.writer(buf)
        w.writerow(cols)
        for r in rows: w.writerow(list(r))
        out_path.write_text(buf.getvalue(), encoding="utf-8")
        conn.close()
        return f"✅ Esportato {len(rows)} righe → {out_path}"
    except sqlite3.Error as e:
        return f"Errore: {e}"

HANDLERS = {
    "list_databases": handle_list_databases,
    "schema":         handle_schema,
    "query":          handle_query,
    "stats":          handle_stats,
    "vacuum":         handle_vacuum,
    "export_csv":     handle_export_csv,
}
TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"list_databases","description":"Elenca tutti i database SQLite Prismalux (~/.prismalux/*.db) con dimensione e tabelle.","inputSchema":{"type":"object","properties":{}}},
    {"name":"schema","description":"Mostra la struttura (tabelle, colonne, tipi) di un database SQLite Prismalux.","inputSchema":{"type":"object","properties":{"db":{"type":"string","default":"","description":"Nome DB (es. 'graph_memory') o path assoluto"}}}},
    {"name":"query","description":"Esegue una query SELECT su un DB Prismalux (read-only, no INSERT/UPDATE/DELETE).","inputSchema":{"type":"object","properties":{"db":{"type":"string","default":""},"sql":{"type":"string"},"limit":{"type":"integer","default":50}},"required":["sql"]}},
    {"name":"stats","description":"Statistiche nodi/archi GraphMemory: tipi, importanza, distribuzione relazioni.","inputSchema":{"type":"object","properties":{"db":{"type":"string","default":"graph_memory"}}}},
    {"name":"vacuum","description":"Esegue VACUUM + ANALYZE sul DB per recuperare spazio e aggiornare le statistiche.","inputSchema":{"type":"object","properties":{"db":{"type":"string","default":""},"dry_run":{"type":"boolean","default":False}}}},
    {"name":"export_csv","description":"Esporta una tabella SQLite in CSV sul Desktop.","inputSchema":{"type":"object","properties":{"db":{"type":"string","default":""},"table":{"type":"string"},"output":{"type":"string","default":""},"limit":{"type":"integer","default":5000}},"required":["table"]}},
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
            resp = _ok(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"sqlite-inspector","version":"1.0.0"},"capabilities":{"tools":{}}})
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
