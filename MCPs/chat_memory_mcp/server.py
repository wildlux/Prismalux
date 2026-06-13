#!/usr/bin/env python3
"""
chat_memory_mcp — Memoria cronologica conversazioni LLM
=======================================================
Salva scambi Q&A con timestamp e sessione in SQLite.
Permette di rispondere a domande come "cosa ho mangiato stamattina?"
cercando nella cronologia delle conversazioni.

Flusso tipico:
  1. save_exchange(user="ho mangiato una mela stamattina", assistant="Ok, annotato.")
  2. ask_history("cosa ho mangiato stamattina?")
     → "Hai detto: 'ho mangiato una mela stamattina' [oggi 08:32]"

Tools:
  save_exchange  — salva coppia (messaggio utente, risposta AI)
  ask_history    — cerca risposta a domanda nella cronologia
  search_history — ricerca per keyword negli scambi
  get_recent     — ultimi N scambi (opzionale per sessione)
  list_sessions  — sessioni disponibili con conteggio
  delete_old     — elimina scambi più vecchi di N giorni
  stats          — statistiche cronologia
"""
import sys, json, os, re, asyncio, sqlite3
from pathlib import Path
from datetime import datetime, timedelta
from typing import Any

ROOT   = Path(os.environ.get("PRISMALUX_ROOT", "/home/wildlux/Desktop/Prismalux"))
DB_DIR = Path.home() / ".prismalux"
DB_PATH = DB_DIR / "chat_memory.db"

# ─── DB setup ────────────────────────────────────────────────────────────────

def _conn() -> sqlite3.Connection:
    DB_DIR.mkdir(parents=True, exist_ok=True)
    c = sqlite3.connect(str(DB_PATH))
    c.row_factory = sqlite3.Row
    c.execute("""
        CREATE TABLE IF NOT EXISTS exchanges (
            id        INTEGER PRIMARY KEY AUTOINCREMENT,
            ts        TEXT NOT NULL DEFAULT (datetime('now','localtime')),
            session   TEXT NOT NULL DEFAULT 'default',
            user_msg  TEXT NOT NULL,
            asst_msg  TEXT NOT NULL DEFAULT '',
            tags      TEXT NOT NULL DEFAULT ''
        )
    """)
    c.execute("CREATE INDEX IF NOT EXISTS idx_ts ON exchanges(ts)")
    c.execute("CREATE INDEX IF NOT EXISTS idx_session ON exchanges(session)")
    c.commit()
    return c

# ─── keyword extraction ───────────────────────────────────────────────────────

STOPWORDS_IT = {
    "ho","hai","ha","abbiamo","avete","hanno","sono","sei","è","siamo",
    "io","tu","lui","lei","noi","voi","loro","mi","ti","si","ci","vi",
    "il","lo","la","i","gli","le","un","uno","una","di","da","in","con",
    "su","per","tra","fra","del","della","dello","dei","degli","delle",
    "al","alla","allo","ai","agli","alle","nel","nella","nello","nei",
    "negli","nelle","cosa","quando","dove","come","perché","chi","che",
    "questa","questo","questi","queste","quello","quella","quelli","quelle",
    "stamattina","stamani","oggi","ieri","l'altro","scorso","fa","alle","ore",
    "ma","e","o","se","non","anche","già","poi","ancora","più","meno",
    "detto","dici","dire","fatto","fare","mangiato","bevuto","visto","letto",
}

def _keywords(text: str) -> list[str]:
    words = re.findall(r'\b\w+\b', text.lower())
    return [w for w in words if len(w) > 2 and w not in STOPWORDS_IT]

def _time_hint(text: str) -> str | None:
    """Estrae hint temporale dalla domanda."""
    text_l = text.lower()
    now    = datetime.now()
    if any(w in text_l for w in ["stamattina","stamani","questa mattina"]):
        d = now.date()
        return f"date(ts) = '{d}' AND time(ts) BETWEEN '05:00' AND '12:00'"
    if any(w in text_l for w in ["oggi","stasera","stanotte","oggi pomeriggio"]):
        return f"date(ts) = '{now.date()}'"
    if "ieri" in text_l:
        d = (now - timedelta(days=1)).date()
        return f"date(ts) = '{d}'"
    if any(w in text_l for w in ["questa settimana","settimana scorsa"]):
        days_ago = 7 if "scorsa" in text_l else 0
        d = (now - timedelta(days=now.weekday() + days_ago)).date()
        return f"date(ts) >= '{d}'"
    return None

# ─── handlers ────────────────────────────────────────────────────────────────

def handle_save_exchange(args: dict) -> str:
    user_msg = args.get("user","").strip()
    asst_msg = args.get("assistant","").strip()
    session  = args.get("session","default").strip() or "default"
    tags     = args.get("tags","").strip()
    if not user_msg:
        return "Specifica 'user' (messaggio dell'utente)."
    c = _conn()
    cur = c.execute(
        "INSERT INTO exchanges (session, user_msg, asst_msg, tags) VALUES (?,?,?,?)",
        (session, user_msg, asst_msg, tags)
    )
    c.commit()
    rid = cur.lastrowid
    ts  = c.execute("SELECT ts FROM exchanges WHERE id=?", (rid,)).fetchone()["ts"]
    c.close()
    return f"✅ Salvato [#{rid}] [{session}] {ts[:16]}\n  U: {user_msg[:80]}"

def handle_ask_history(args: dict) -> str:
    question = args.get("question","").strip()
    session  = args.get("session","").strip()
    limit    = min(int(args.get("limit", 5)), 20)
    if not question:
        return "Specifica 'question'."

    c    = _conn()
    kws  = _keywords(question)
    time = _time_hint(question)

    # Costruisce query con filtro temporale + keyword
    clauses = []
    params  = []
    if time:
        clauses.append(f"({time})")
    if session:
        clauses.append("session = ?")
        params.append(session)
    # keyword: cerca in user_msg O asst_msg
    if kws:
        kw_clauses = []
        for kw in kws[:6]:
            kw_clauses.append("(lower(user_msg) LIKE ? OR lower(asst_msg) LIKE ?)")
            params += [f"%{kw}%", f"%{kw}%"]
        clauses.append("(" + " OR ".join(kw_clauses) + ")")

    where = ("WHERE " + " AND ".join(clauses)) if clauses else ""
    sql   = f"""
        SELECT id, ts, session, user_msg, asst_msg
        FROM exchanges {where}
        ORDER BY ts DESC LIMIT ?
    """
    rows = c.execute(sql, params + [limit]).fetchall()
    c.close()

    if not rows:
        # Fallback: ricerca più larga senza filtro temporale
        if time and kws:
            c2 = _conn()
            kw_clauses = []
            params2 = []
            for kw in kws[:4]:
                kw_clauses.append("(lower(user_msg) LIKE ? OR lower(asst_msg) LIKE ?)")
                params2 += [f"%{kw}%", f"%{kw}%"]
            sql2 = f"SELECT id,ts,session,user_msg,asst_msg FROM exchanges WHERE ({' OR '.join(kw_clauses)}) ORDER BY ts DESC LIMIT ?"
            rows = c2.execute(sql2, params2 + [limit]).fetchall()
            c2.close()

    if not rows:
        return (f"Nessun risultato in cronologia per: '{question}'\n"
                f"  Keywords cercate: {', '.join(kws[:6])}\n"
                f"  Usa save_exchange() per registrare conversazioni.")

    lines = [f"📖 Dalla cronologia ({len(rows)} risultati per: '{question}')\n"]
    for r in rows:
        ts_short = r["ts"][:16]
        lines.append(f"  [{ts_short}] [{r['session']}]")
        lines.append(f"  👤 {r['user_msg'][:120]}")
        if r["asst_msg"].strip():
            lines.append(f"  🤖 {r['asst_msg'][:120]}")
        lines.append("")
    return "\n".join(lines)

def handle_search_history(args: dict) -> str:
    query   = args.get("query","").strip()
    session = args.get("session","").strip()
    limit   = min(int(args.get("limit", 20)), 100)
    field   = args.get("field","both")  # "user", "assistant", "both"
    if not query:
        return "Specifica 'query'."

    c      = _conn()
    params = [f"%{query}%", f"%{query}%"]
    if field == "user":
        where_kw = "lower(user_msg) LIKE ?"
        params   = [f"%{query.lower()}%"]
    elif field == "assistant":
        where_kw = "lower(asst_msg) LIKE ?"
        params   = [f"%{query.lower()}%"]
    else:
        where_kw = "(lower(user_msg) LIKE ? OR lower(asst_msg) LIKE ?)"
        params   = [f"%{query.lower()}%", f"%{query.lower()}%"]

    session_clause = "AND session = ?" if session else ""
    if session: params.append(session)

    rows = c.execute(
        f"SELECT id,ts,session,user_msg,asst_msg FROM exchanges WHERE {where_kw} {session_clause} ORDER BY ts DESC LIMIT ?",
        params + [limit]
    ).fetchall()
    c.close()

    if not rows:
        return f"Nessun risultato per '{query}'."
    lines = [f"🔍 Ricerca '{query}' — {len(rows)} risultati\n"]
    for r in rows:
        # Evidenzia keyword
        ts_short = r["ts"][:16]
        um = r["user_msg"][:100].replace(query, f"**{query}**")
        am = r["asst_msg"][:80].replace(query, f"**{query}**")
        lines.append(f"  #{r['id']} [{ts_short}] [{r['session']}]")
        lines.append(f"    👤 {um}")
        if am.strip(): lines.append(f"    🤖 {am}")
    return "\n".join(lines)

def handle_get_recent(args: dict) -> str:
    n       = min(int(args.get("n", 10)), 50)
    session = args.get("session","").strip()
    c       = _conn()
    where   = "WHERE session=?" if session else ""
    params  = ([session] if session else []) + [n]
    rows    = c.execute(
        f"SELECT id,ts,session,user_msg,asst_msg FROM exchanges {where} ORDER BY ts DESC LIMIT ?",
        params
    ).fetchall()
    c.close()
    if not rows:
        return "Nessuno scambio salvato ancora."
    lines = [f"📋 Ultimi {n} scambi{' — sessione: ' + session if session else ''}\n"]
    for r in reversed(rows):
        lines.append(f"  #{r['id']} [{r['ts'][:16]}] [{r['session']}]")
        lines.append(f"    👤 {r['user_msg'][:100]}")
        if r["asst_msg"].strip():
            lines.append(f"    🤖 {r['asst_msg'][:80]}")
        lines.append("")
    return "\n".join(lines)

def handle_list_sessions(args: dict) -> str:
    c    = _conn()
    rows = c.execute(
        "SELECT session, COUNT(*) as cnt, MIN(ts) as first, MAX(ts) as last "
        "FROM exchanges GROUP BY session ORDER BY last DESC"
    ).fetchall()
    c.close()
    if not rows:
        return "Nessuno scambio salvato. Usa save_exchange() per iniziare."
    lines = [f"📂 Sessioni ({len(rows)})\n"]
    lines.append(f"  {'Sessione':<25} {'Scambi':>7}  Prima       Ultima")
    lines.append("  " + "─" * 65)
    for r in rows:
        lines.append(f"  {r['session']:<25} {r['cnt']:>7}  {r['first'][:16]}  {r['last'][:16]}")
    return "\n".join(lines)

def handle_delete_old(args: dict) -> str:
    days    = int(args.get("days", 30))
    session = args.get("session","").strip()
    dry_run = bool(args.get("dry_run", True))
    c       = _conn()
    cutoff  = (datetime.now() - timedelta(days=days)).strftime("%Y-%m-%d %H:%M:%S")
    where   = "ts < ?" + (" AND session=?" if session else "")
    params  = [cutoff] + ([session] if session else [])
    count   = c.execute(f"SELECT COUNT(*) FROM exchanges WHERE {where}", params).fetchone()[0]
    if not dry_run and count > 0:
        c.execute(f"DELETE FROM exchanges WHERE {where}", params)
        c.commit()
    c.close()
    pfx = "[DRY RUN] " if dry_run else ""
    return (f"{pfx}{'✅ Eliminati' if not dry_run else 'Eliminerei'} {count} scambi "
            f"più vecchi di {days} giorni (prima del {cutoff[:10]})")

def handle_stats(args: dict) -> str:
    c    = _conn()
    total = c.execute("SELECT COUNT(*) FROM exchanges").fetchone()[0]
    if not total:
        return f"Nessuno scambio salvato.\nDB: {DB_PATH}"
    sessions = c.execute("SELECT COUNT(DISTINCT session) FROM exchanges").fetchone()[0]
    first    = c.execute("SELECT MIN(ts) FROM exchanges").fetchone()[0]
    last     = c.execute("SELECT MAX(ts) FROM exchanges").fetchone()[0]
    today    = c.execute("SELECT COUNT(*) FROM exchanges WHERE date(ts)=date('now','localtime')").fetchone()[0]
    avg_len  = c.execute("SELECT AVG(LENGTH(user_msg)) FROM exchanges").fetchone()[0] or 0
    c.close()
    return (f"📊 Chat Memory Statistics\n\n"
            f"  Scambi totali:  {total}\n"
            f"  Sessioni:       {sessions}\n"
            f"  Scambi oggi:    {today}\n"
            f"  Periodo:        {str(first)[:10]} → {str(last)[:10]}\n"
            f"  Lunghezza media msg: {avg_len:.0f} caratteri\n"
            f"  DB:             {DB_PATH} ({DB_PATH.stat().st_size/1024:.0f} KB)" if DB_PATH.exists() else "")

HANDLERS = {
    "save_exchange":  handle_save_exchange,
    "ask_history":    handle_ask_history,
    "search_history": handle_search_history,
    "get_recent":     handle_get_recent,
    "list_sessions":  handle_list_sessions,
    "delete_old":     handle_delete_old,
    "stats":          handle_stats,
}
TOOL_DEFS: list[dict[str, Any]] = [
    {"name":"save_exchange","description":"Salva una coppia (messaggio utente, risposta AI) nella cronologia SQLite.","inputSchema":{"type":"object","properties":{"user":{"type":"string","description":"Messaggio dell'utente"},"assistant":{"type":"string","default":"","description":"Risposta dell'AI (opzionale)"},"session":{"type":"string","default":"default"},"tags":{"type":"string","default":""}},"required":["user"]}},
    {"name":"ask_history","description":"Cerca nella cronologia la risposta a una domanda (es. 'cosa ho mangiato stamattina?'). Applica filtro temporale automatico se la domanda contiene 'stamattina', 'ieri', 'oggi', ecc.","inputSchema":{"type":"object","properties":{"question":{"type":"string"},"session":{"type":"string","default":""},"limit":{"type":"integer","default":5}},"required":["question"]}},
    {"name":"search_history","description":"Ricerca per keyword negli scambi salvati (user_msg e/o asst_msg).","inputSchema":{"type":"object","properties":{"query":{"type":"string"},"session":{"type":"string","default":""},"limit":{"type":"integer","default":20},"field":{"type":"string","enum":["user","assistant","both"],"default":"both"}},"required":["query"]}},
    {"name":"get_recent","description":"Mostra gli ultimi N scambi nella cronologia, opzionalmente filtrati per sessione.","inputSchema":{"type":"object","properties":{"n":{"type":"integer","default":10},"session":{"type":"string","default":""}}}},
    {"name":"list_sessions","description":"Elenca le sessioni disponibili con numero di scambi e date.","inputSchema":{"type":"object","properties":{}}},
    {"name":"delete_old","description":"Elimina scambi più vecchi di N giorni (dry_run=true di default).","inputSchema":{"type":"object","properties":{"days":{"type":"integer","default":30},"session":{"type":"string","default":""},"dry_run":{"type":"boolean","default":True}}}},
    {"name":"stats","description":"Statistiche cronologia: totale scambi, sessioni, periodo, scambi oggi.","inputSchema":{"type":"object","properties":{}}},
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
            resp = _ok(rid,{"protocolVersion":"2024-11-05","serverInfo":{"name":"chat-memory","version":"1.0.0"},"capabilities":{"tools":{}}})
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
