#!/usr/bin/env python3
"""
GNS3 MCP — Prismalux  (Printing-Press concepts: SQLite cache + compound commands)

Novità rispetto alla v1:
  - SQLite locale per progetti/nodi/link/template → nessun round-trip ripetuto
  - sync: sincronizza GNS3 API → cache locale
  - get_topology: un solo tool restituisce progetto + nodi + link (compound command)
  - search_nodes: FTS5 sul nome dei nodi
  - compact=true: output ridotto (token-efficiente per agenti AI)

Cache: ~/.prismalux/gns3_cache.db
Prerequisiti: gns3server in esecuzione (porta 3080).
"""
import sys
import json
import sqlite3
import time
import urllib.request
import urllib.error
from pathlib import Path
from typing import Any

GNS3_BASE  = "http://localhost:3080/v2"
_CACHE_DIR = Path.home() / ".prismalux"
_CACHE_DB  = _CACHE_DIR / "gns3_cache.db"


# ─── SQLite helpers ───────────────────────────────────────────────────────────

def _db() -> sqlite3.Connection:
    _CACHE_DIR.mkdir(parents=True, exist_ok=True)
    con = sqlite3.connect(_CACHE_DB, timeout=5)
    con.row_factory = sqlite3.Row
    con.execute("PRAGMA journal_mode=WAL")
    con.execute("PRAGMA foreign_keys=ON")
    con.executescript("""
        CREATE TABLE IF NOT EXISTS projects (
            project_id TEXT PRIMARY KEY,
            name       TEXT NOT NULL,
            status     TEXT,
            synced_at  INTEGER
        );
        CREATE TABLE IF NOT EXISTS templates (
            template_id   TEXT PRIMARY KEY,
            name          TEXT NOT NULL,
            template_type TEXT,
            synced_at     INTEGER
        );
        CREATE TABLE IF NOT EXISTS nodes (
            node_id    TEXT PRIMARY KEY,
            project_id TEXT NOT NULL REFERENCES projects(project_id) ON DELETE CASCADE,
            name       TEXT NOT NULL,
            node_type  TEXT,
            status     TEXT,
            x          INTEGER DEFAULT 0,
            y          INTEGER DEFAULT 0,
            synced_at  INTEGER
        );
        CREATE TABLE IF NOT EXISTS links (
            link_id    TEXT PRIMARY KEY,
            project_id TEXT NOT NULL REFERENCES projects(project_id) ON DELETE CASCADE,
            node1_id   TEXT,
            node1_port INTEGER,
            node2_id   TEXT,
            node2_port INTEGER,
            synced_at  INTEGER
        );
        CREATE VIRTUAL TABLE IF NOT EXISTS nodes_fts USING fts5(
            name, node_type, node_id UNINDEXED, project_id UNINDEXED,
            content='nodes', content_rowid='rowid'
        );
        CREATE TRIGGER IF NOT EXISTS nodes_ai AFTER INSERT ON nodes BEGIN
            INSERT INTO nodes_fts(rowid, name, node_type, node_id, project_id)
            VALUES (new.rowid, new.name, new.node_type, new.node_id, new.project_id);
        END;
        CREATE TRIGGER IF NOT EXISTS nodes_au AFTER UPDATE ON nodes BEGIN
            INSERT INTO nodes_fts(nodes_fts, rowid, name, node_type, node_id, project_id)
            VALUES ('delete', old.rowid, old.name, old.node_type, old.node_id, old.project_id);
            INSERT INTO nodes_fts(rowid, name, node_type, node_id, project_id)
            VALUES (new.rowid, new.name, new.node_type, new.node_id, new.project_id);
        END;
        CREATE TRIGGER IF NOT EXISTS nodes_ad AFTER DELETE ON nodes BEGIN
            INSERT INTO nodes_fts(nodes_fts, rowid, name, node_type, node_id, project_id)
            VALUES ('delete', old.rowid, old.name, old.node_type, old.node_id, old.project_id);
        END;
    """)
    con.commit()
    return con


# ─── GNS3 API helpers ─────────────────────────────────────────────────────────

def _gns(method: str, path: str, data: Any = None) -> tuple[Any, str | None]:
    url  = GNS3_BASE + path
    body = json.dumps(data).encode() if data is not None else None
    req  = urllib.request.Request(url, data=body, method=method,
               headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=10) as r:
            return json.loads(r.read() or b"{}"), None
    except urllib.error.HTTPError as e:
        return None, f"HTTP {e.code}: {e.read().decode()[:300]}"
    except Exception as e:
        return None, f"{e}\nAssicurati che GNS3 server sia in esecuzione (porta 3080)."


# ─── Strumenti ────────────────────────────────────────────────────────────────

def tool_sync(args: dict) -> str:
    """Sincronizza progetti/template/nodi/link dalla GNS3 API → SQLite locale."""
    now = int(time.time())
    con = _db()
    errors: list[str] = []

    # Progetti
    projects, err = _gns("GET", "/projects")
    if err:
        return f"[Errore] sync projects: {err}"
    con.executemany(
        "INSERT OR REPLACE INTO projects(project_id, name, status, synced_at) VALUES(?,?,?,?)",
        [(p["project_id"], p["name"], p.get("status"), now) for p in projects]
    )

    # Template
    templates, err = _gns("GET", "/templates")
    if err:
        errors.append(f"templates: {err}")
    else:
        con.executemany(
            "INSERT OR REPLACE INTO templates(template_id, name, template_type, synced_at) VALUES(?,?,?,?)",
            [(t["template_id"], t["name"], t.get("template_type"), now) for t in templates]
        )

    # Nodi e link per ogni progetto
    node_count = link_count = 0
    for proj in projects:
        pid = proj["project_id"]

        nodes, err = _gns("GET", f"/projects/{pid}/nodes")
        if err:
            errors.append(f"nodes[{pid[:8]}]: {err}")
        else:
            for n in nodes:
                con.execute(
                    "INSERT OR REPLACE INTO nodes(node_id,project_id,name,node_type,status,x,y,synced_at) "
                    "VALUES(?,?,?,?,?,?,?,?)",
                    (n["node_id"], pid, n["name"], n.get("node_type"),
                     n.get("status"), n.get("x", 0), n.get("y", 0), now)
                )
            node_count += len(nodes)

        links, err = _gns("GET", f"/projects/{pid}/links")
        if err:
            errors.append(f"links[{pid[:8]}]: {err}")
        else:
            for lk in links:
                ns = lk.get("nodes", [])
                n1 = ns[0] if len(ns) > 0 else {}
                n2 = ns[1] if len(ns) > 1 else {}
                con.execute(
                    "INSERT OR REPLACE INTO links(link_id,project_id,node1_id,node1_port,node2_id,node2_port,synced_at) "
                    "VALUES(?,?,?,?,?,?,?)",
                    (lk["link_id"], pid, n1.get("node_id"), n1.get("port_number", 0),
                     n2.get("node_id"), n2.get("port_number", 0), now)
                )
            link_count += len(links)

    con.commit()
    con.close()

    summary = (f"Sync completato — {len(projects)} progetti, "
               f"{len(templates) if templates else 0} template, "
               f"{node_count} nodi, {link_count} link.")
    if errors:
        summary += "\nAvvisi: " + "; ".join(errors)
    return summary


def tool_get_topology(args: dict) -> str:
    """Compound command: restituisce progetto + nodi + link in un'unica chiamata (dalla cache locale)."""
    project_id = args.get("project_id", "").strip()
    compact    = args.get("compact", False)

    con = _db()

    # Se non c'è nessun dato, tenta sync automatico
    count = con.execute("SELECT COUNT(*) FROM projects").fetchone()[0]
    if count == 0:
        con.close()
        sync_msg = tool_sync({})
        con = _db()

    if project_id:
        proj = con.execute(
            "SELECT * FROM projects WHERE project_id=?", (project_id,)
        ).fetchone()
        if not proj:
            # Prova per nome (case-insensitive)
            proj = con.execute(
                "SELECT * FROM projects WHERE lower(name)=lower(?)", (project_id,)
            ).fetchone()
        if not proj:
            con.close()
            return f"[Errore] Progetto '{project_id}' non trovato nella cache. Esegui sync prima."
        projects = [proj]
    else:
        projects = con.execute("SELECT * FROM projects").fetchall()

    if not projects:
        con.close()
        return "Nessun progetto in cache. Esegui sync prima."

    parts: list[str] = []
    for proj in projects:
        pid   = proj["project_id"]
        pname = proj["name"]
        pstat = proj["status"] or "?"

        nodes = con.execute(
            "SELECT node_id, name, node_type, status, x, y FROM nodes WHERE project_id=?", (pid,)
        ).fetchall()
        links = con.execute(
            "SELECT link_id, node1_id, node1_port, node2_id, node2_port FROM links WHERE project_id=?", (pid,)
        ).fetchall()

        if compact:
            parts.append(
                f"PROJECT {pname} ({pid[:8]}) status={pstat} "
                f"nodes={len(nodes)} links={len(links)}"
            )
            for n in nodes:
                icon = "▶" if n["status"] == "started" else "■"
                parts.append(f"  {icon} {n['name']} type={n['node_type']} id={n['node_id'][:8]}")
            for lk in links:
                n1 = _node_name(con, lk["node1_id"])
                n2 = _node_name(con, lk["node2_id"])
                parts.append(f"  LINK {n1}:{lk['node1_port']} — {n2}:{lk['node2_port']}")
        else:
            parts.append(f"\n{'='*60}")
            parts.append(f"Progetto: {pname}")
            parts.append(f"  ID:     {pid}")
            parts.append(f"  Stato:  {pstat}")
            parts.append(f"  Nodi:   {len(nodes)}")
            parts.append(f"  Link:   {len(links)}")
            parts.append("  Nodi:")
            for n in nodes:
                icon = "🟢" if n["status"] == "started" else "🔴"
                parts.append(f"    {icon} {n['name']:<12} type={n['node_type']:<10} "
                             f"pos=({n['x']},{n['y']}) id={n['node_id'][:8]}")
            parts.append("  Link:")
            for lk in links:
                n1 = _node_name(con, lk["node1_id"])
                n2 = _node_name(con, lk["node2_id"])
                parts.append(f"    {n1}:{lk['node1_port']} ↔ {n2}:{lk['node2_port']}  "
                             f"id={lk['link_id'][:8]}")

    con.close()
    return "\n".join(parts).strip()


def _node_name(con: sqlite3.Connection, node_id: str | None) -> str:
    if not node_id:
        return "?"
    row = con.execute("SELECT name FROM nodes WHERE node_id=?", (node_id,)).fetchone()
    return row["name"] if row else node_id[:8]


def tool_search_nodes(args: dict) -> str:
    """Cerca nodi per nome (FTS5). Più veloce di list_nodes per topologie grandi."""
    query   = args.get("query", "").strip()
    compact = args.get("compact", False)
    if not query:
        return "[Errore] Il campo 'query' è obbligatorio."

    con = _db()
    rows = con.execute(
        "SELECT n.node_id, n.name, n.node_type, n.status, n.project_id "
        "FROM nodes_fts f JOIN nodes n ON n.node_id = f.node_id "
        "WHERE nodes_fts MATCH ? ORDER BY rank LIMIT 50",
        (query,)
    ).fetchall()
    con.close()

    if not rows:
        return f"Nessun nodo trovato per '{query}'."

    if compact:
        return "\n".join(
            f"{r['name']} type={r['node_type']} status={r['status']} "
            f"proj={r['project_id'][:8]} id={r['node_id'][:8]}"
            for r in rows
        )

    lines = [f"Nodi trovati per '{query}' ({len(rows)}):"]
    for r in rows:
        icon = "🟢" if r["status"] == "started" else "🔴"
        lines.append(f"  {icon} {r['name']:<12} type={r['node_type']:<10} "
                     f"proj={r['project_id'][:8]} id={r['node_id'][:8]}")
    return "\n".join(lines)


def tool_list_projects(_: dict) -> str:
    # Prima prova dalla cache, se vuota chiama API
    con = _db()
    rows = con.execute("SELECT project_id, name, status, synced_at FROM projects").fetchall()
    con.close()
    if rows:
        lines = [f"Progetti GNS3 in cache ({len(rows)}) — usa sync per aggiornare:"]
        for p in rows:
            age = int(time.time()) - (p["synced_at"] or 0)
            lines.append(f"  [{p['project_id'][:8]}...] {p['name']} — {p['status']} (cache {age}s fa)")
        return "\n".join(lines)

    # Fallback API live
    res, err = _gns("GET", "/projects")
    if err:
        return f"[Errore GNS3] {err}"
    if not res:
        return "Nessun progetto GNS3."
    lines = [f"Progetti GNS3 ({len(res)}):"]
    for p in res:
        lines.append(f"  [{p['project_id'][:8]}...] {p['name']} — {p.get('status','?')}")
    return "\n".join(lines)


def tool_create_project(args: dict) -> str:
    res, err = _gns("POST", "/projects", {"name": args["name"]})
    if err:
        return f"[Errore] {err}"
    return f"Progetto '{args['name']}' creato.\n  ID: {res.get('project_id','?')}"


def tool_list_templates(_: dict) -> str:
    con = _db()
    rows = con.execute("SELECT template_id, name, template_type FROM templates").fetchall()
    con.close()
    if rows:
        lines = [f"Template in cache ({len(rows)}):"]
        for t in rows:
            lines.append(f"  [{t['template_id'][:8]}...] {t['name']} ({t['template_type']})")
        return "\n".join(lines)

    res, err = _gns("GET", "/templates")
    if err:
        return f"[Errore] {err}"
    if not res:
        return "Nessun template disponibile."
    lines = [f"Template disponibili ({len(res)}):"]
    for t in res:
        lines.append(f"  [{t['template_id'][:8]}...] {t['name']} ({t.get('template_type','?')})")
    return "\n".join(lines)


def tool_add_node(args: dict) -> str:
    payload = {"name": args["name"], "template_id": args["template_id"],
               "x": args.get("x", 0), "y": args.get("y", 0)}
    res, err = _gns("POST",
        f"/projects/{args['project_id']}/templates/{args['template_id']}", payload)
    if err:
        return f"[Errore] {err}"
    return f"Nodo '{args['name']}' aggiunto.\n  ID: {res.get('node_id','?')}"


def tool_add_link(args: dict) -> str:
    payload = {"nodes": [
        {"node_id": args["node1_id"], "port_number": args.get("node1_port", 0), "adapter_number": 0},
        {"node_id": args["node2_id"], "port_number": args.get("node2_port", 0), "adapter_number": 0},
    ]}
    res, err = _gns("POST", f"/projects/{args['project_id']}/links", payload)
    if err:
        return f"[Errore] {err}"
    return (f"Link creato tra {args['node1_id'][:8]} e {args['node2_id'][:8]}.\n"
            f"  ID: {res.get('link_id','?')}")


def tool_start_node(args: dict) -> str:
    _, err = _gns("POST", f"/projects/{args['project_id']}/nodes/{args['node_id']}/start")
    if err:
        return f"[Errore] {err}"
    return f"Nodo {args['node_id'][:8]}... avviato."


def tool_stop_node(args: dict) -> str:
    _, err = _gns("POST", f"/projects/{args['project_id']}/nodes/{args['node_id']}/stop")
    if err:
        return f"[Errore] {err}"
    return f"Nodo {args['node_id'][:8]}... fermato."


def tool_list_nodes(args: dict) -> str:
    pid = args["project_id"]
    # Prova cache prima
    con = _db()
    rows = con.execute(
        "SELECT node_id, name, node_type, status FROM nodes WHERE project_id=?", (pid,)
    ).fetchall()
    con.close()
    if rows:
        lines = [f"Nodi da cache ({len(rows)}):"]
        for n in rows:
            icon = "🟢" if n["status"] == "started" else "🔴"
            lines.append(f"  {icon} [{n['node_id'][:8]}...] {n['name']} ({n['node_type']})")
        return "\n".join(lines)

    # Fallback API live
    res, err = _gns("GET", f"/projects/{pid}/nodes")
    if err:
        return f"[Errore] {err}"
    if not res:
        return "Nessun nodo nel progetto."
    lines = [f"Nodi ({len(res)}):"]
    for n in res:
        icon = "🟢" if n.get("status") == "started" else "🔴"
        lines.append(f"  {icon} [{n['node_id'][:8]}...] {n['name']} ({n.get('node_type','?')})")
    return "\n".join(lines)


# ─── Definizioni strumenti (schema MCP) ───────────────────────────────────────

TOOLS = [
    {"name": "sync",
     "description": (
         "Sincronizza tutti i progetti/template/nodi/link da GNS3 API → cache SQLite locale. "
         "Esegui prima di usare get_topology o search_nodes. "
         "Dopo il primo sync le operazioni di lettura sono instant (no round-trip API)."
     ),
     "inputSchema": {"type": "object", "properties": {}}},

    {"name": "get_topology",
     "description": (
         "COMPOUND: restituisce progetto + tutti i nodi + tutti i link in una sola chiamata "
         "(dalla cache locale — zero round-trip API). "
         "Sostituisce la sequenza list_projects → list_nodes → list_links. "
         "Se project_id è omesso mostra tutte le topologie."
     ),
     "inputSchema": {"type": "object", "properties": {
         "project_id": {"type": "string",
                        "description": "ID o nome del progetto. Ometti per tutte le topologie."},
         "compact":    {"type": "boolean", "default": False,
                        "description": "true = output ridotto per agenti AI (meno token)."},
     }}},

    {"name": "search_nodes",
     "description": "Cerca nodi per nome usando FTS5 (full-text). Istantaneo dalla cache locale.",
     "inputSchema": {"type": "object", "required": ["query"], "properties": {
         "query":   {"type": "string", "description": "Termine di ricerca (es. 'R1', 'SW', 'PC')"},
         "compact": {"type": "boolean", "default": False},
     }}},

    {"name": "list_projects",
     "description": "Elenca tutti i progetti GNS3 (dalla cache se disponibile, altrimenti API live).",
     "inputSchema": {"type": "object", "properties": {}}},

    {"name": "create_project",
     "description": "Crea un nuovo progetto GNS3.",
     "inputSchema": {"type": "object", "required": ["name"],
        "properties": {"name": {"type": "string"}}}},

    {"name": "list_templates",
     "description": "Elenca i template nodi (dalla cache se disponibile, altrimenti API).",
     "inputSchema": {"type": "object", "properties": {}}},

    {"name": "add_node",
     "description": "Aggiunge un nodo (router, switch, PC) a un progetto.",
     "inputSchema": {"type": "object", "required": ["project_id", "name", "template_id"],
        "properties": {
            "project_id":  {"type": "string"},
            "name":        {"type": "string"},
            "template_id": {"type": "string"},
            "x":           {"type": "integer", "default": 0},
            "y":           {"type": "integer", "default": 0},
        }}},

    {"name": "add_link",
     "description": "Collega due nodi con un link.",
     "inputSchema": {"type": "object", "required": ["project_id", "node1_id", "node2_id"],
        "properties": {
            "project_id": {"type": "string"},
            "node1_id":   {"type": "string"},
            "node1_port": {"type": "integer", "default": 0},
            "node2_id":   {"type": "string"},
            "node2_port": {"type": "integer", "default": 0},
        }}},

    {"name": "start_node",
     "description": "Avvia un nodo GNS3.",
     "inputSchema": {"type": "object", "required": ["project_id", "node_id"],
        "properties": {"project_id": {"type": "string"}, "node_id": {"type": "string"}}}},

    {"name": "stop_node",
     "description": "Ferma un nodo GNS3.",
     "inputSchema": {"type": "object", "required": ["project_id", "node_id"],
        "properties": {"project_id": {"type": "string"}, "node_id": {"type": "string"}}}},

    {"name": "list_nodes",
     "description": "Elenca i nodi di un progetto (dalla cache se disponibile).",
     "inputSchema": {"type": "object", "required": ["project_id"],
        "properties": {"project_id": {"type": "string"}}}},
]

HANDLERS: dict[str, Any] = {
    "sync":             tool_sync,
    "get_topology":     tool_get_topology,
    "search_nodes":     tool_search_nodes,
    "list_projects":    tool_list_projects,
    "create_project":   tool_create_project,
    "list_templates":   tool_list_templates,
    "add_node":         tool_add_node,
    "add_link":         tool_add_link,
    "start_node":       tool_start_node,
    "stop_node":        tool_stop_node,
    "list_nodes":       tool_list_nodes,
}


# ─── JSON-RPC 2.0 server ──────────────────────────────────────────────────────

def _send(obj: Any) -> None:
    sys.stdout.write(json.dumps(obj) + "\n")
    sys.stdout.flush()

def _error(rid: Any, c: int, m: str) -> None:
    _send({"jsonrpc": "2.0", "id": rid, "error": {"code": c, "message": m}})

def _result(rid: Any, r: Any) -> None:
    _send({"jsonrpc": "2.0", "id": rid, "result": r})


def handle(req: dict) -> None:
    m, rid, p = req.get("method", ""), req.get("id"), req.get("params", {}) or {}
    if m == "initialize":
        _result(rid, {
            "protocolVersion": "2024-11-05",
            "capabilities": {"tools": {}},
            "serverInfo": {"name": "gns3-mcp", "version": "2.0.0"},
        })
    elif m == "tools/list":
        _result(rid, {"tools": TOOLS})
    elif m == "tools/call":
        name, args = p.get("name", ""), p.get("arguments", {}) or {}
        h = HANDLERS.get(name)
        if not h:
            _error(rid, -32601, f"Strumento '{name}' non trovato.")
            return
        try:
            text = h(args)
        except Exception as e:
            text = f"[Errore] {e}"
        _result(rid, {
            "content": [{"type": "text", "text": text}],
            "isError": text.startswith("[Errore"),
        })
    elif rid is not None:
        _result(rid, {})


def main() -> None:
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
        except json.JSONDecodeError as e:
            _send({"jsonrpc": "2.0", "id": None,
                   "error": {"code": -32700, "message": str(e)}})
            continue
        try:
            handle(req)
        except Exception as e:
            if req.get("id"):
                _error(req["id"], -32603, str(e))


if __name__ == "__main__":
    main()
