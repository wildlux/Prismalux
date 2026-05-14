#!/usr/bin/env python3
"""
GNS3 MCP — Prismalux
Crea topologie di rete, gestisce nodi e link via GNS3 REST API (porta 3080).
Prerequisiti: gns3server in esecuzione (`gns3server` o GNS3 desktop aperto).
"""
import sys, json
import urllib.request, urllib.error
from typing import Any

GNS3_BASE = "http://localhost:3080/v2"

def _gns(method: str, path: str, data: Any = None) -> tuple[Any, str | None]:
    url = GNS3_BASE + path
    body = json.dumps(data).encode() if data is not None else None
    req = urllib.request.Request(url, data=body, method=method,
        headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=10) as r:
            return json.loads(r.read() or b"{}"), None
    except urllib.error.HTTPError as e:
        return None, f"HTTP {e.code}: {e.read().decode()[:300]}"
    except Exception as e:
        return None, f"{e}\nAssicurati che GNS3 server sia in esecuzione (porta 3080)."

TOOLS = [
    {"name": "list_projects",
     "description": "Elenca tutti i progetti GNS3.",
     "inputSchema": {"type": "object", "properties": {}}},
    {"name": "create_project",
     "description": "Crea un nuovo progetto GNS3.",
     "inputSchema": {"type": "object", "required": ["name"],
        "properties": {"name": {"type": "string"}}}},
    {"name": "list_templates",
     "description": "Elenca i template di nodi disponibili (router, switch, PC).",
     "inputSchema": {"type": "object", "properties": {}}},
    {"name": "add_node",
     "description": "Aggiunge un nodo (router, switch, PC) a un progetto.",
     "inputSchema": {"type": "object", "required": ["project_id", "name", "template_id"],
        "properties": {
            "project_id":  {"type": "string"},
            "name":        {"type": "string", "description": "Nome del nodo (es. 'R1', 'SW1')"},
            "template_id": {"type": "string", "description": "ID template da list_templates"},
            "x":           {"type": "integer", "default": 0},
            "y":           {"type": "integer", "default": 0}}}},
    {"name": "add_link",
     "description": "Collega due nodi con un link.",
     "inputSchema": {"type": "object", "required": ["project_id", "node1_id", "node2_id"],
        "properties": {
            "project_id": {"type": "string"},
            "node1_id":   {"type": "string"},
            "node1_port": {"type": "integer", "default": 0},
            "node2_id":   {"type": "string"},
            "node2_port": {"type": "integer", "default": 0}}}},
    {"name": "start_node",
     "description": "Avvia un nodo GNS3.",
     "inputSchema": {"type": "object", "required": ["project_id", "node_id"],
        "properties": {"project_id": {"type": "string"}, "node_id": {"type": "string"}}}},
    {"name": "stop_node",
     "description": "Ferma un nodo GNS3.",
     "inputSchema": {"type": "object", "required": ["project_id", "node_id"],
        "properties": {"project_id": {"type": "string"}, "node_id": {"type": "string"}}}},
    {"name": "list_nodes",
     "description": "Elenca i nodi di un progetto con stato (running/stopped).",
     "inputSchema": {"type": "object", "required": ["project_id"],
        "properties": {"project_id": {"type": "string"}}}},
]

def tool_list_projects(_: dict) -> str:
    res, err = _gns("GET", "/projects")
    if err: return f"[Errore GNS3] {err}"
    if not res: return "Nessun progetto GNS3."
    lines = [f"Progetti GNS3 ({len(res)}):"]
    for p in res:
        lines.append(f"  [{p['project_id'][:8]}...] {p['name']} — {p.get('status','?')}")
    return "\n".join(lines)

def tool_create_project(args: dict) -> str:
    res, err = _gns("POST", "/projects", {"name": args["name"]})
    if err: return f"[Errore] {err}"
    return f"Progetto '{args['name']}' creato.\n  ID: {res.get('project_id','?')}"

def tool_list_templates(_: dict) -> str:
    res, err = _gns("GET", "/templates")
    if err: return f"[Errore] {err}"
    if not res: return "Nessun template disponibile."
    lines = [f"Template disponibili ({len(res)}):"]
    for t in res:
        lines.append(f"  [{t['template_id'][:8]}...] {t['name']} ({t.get('template_type','?')})")
    return "\n".join(lines)

def tool_add_node(args: dict) -> str:
    payload = {"name": args["name"], "template_id": args["template_id"],
               "x": args.get("x", 0), "y": args.get("y", 0)}
    res, err = _gns("POST", f"/projects/{args['project_id']}/templates/{args['template_id']}", payload)
    if err: return f"[Errore] {err}"
    return f"Nodo '{args['name']}' aggiunto.\n  ID: {res.get('node_id','?')}"

def tool_add_link(args: dict) -> str:
    payload = {"nodes": [
        {"node_id": args["node1_id"], "port_number": args.get("node1_port", 0), "adapter_number": 0},
        {"node_id": args["node2_id"], "port_number": args.get("node2_port", 0), "adapter_number": 0}
    ]}
    res, err = _gns("POST", f"/projects/{args['project_id']}/links", payload)
    if err: return f"[Errore] {err}"
    return f"Link creato tra {args['node1_id'][:8]} e {args['node2_id'][:8]}.\n  ID: {res.get('link_id','?')}"

def tool_start_node(args: dict) -> str:
    _, err = _gns("POST", f"/projects/{args['project_id']}/nodes/{args['node_id']}/start")
    if err: return f"[Errore] {err}"
    return f"Nodo {args['node_id'][:8]}... avviato."

def tool_stop_node(args: dict) -> str:
    _, err = _gns("POST", f"/projects/{args['project_id']}/nodes/{args['node_id']}/stop")
    if err: return f"[Errore] {err}"
    return f"Nodo {args['node_id'][:8]}... fermato."

def tool_list_nodes(args: dict) -> str:
    res, err = _gns("GET", f"/projects/{args['project_id']}/nodes")
    if err: return f"[Errore] {err}"
    if not res: return "Nessun nodo nel progetto."
    lines = [f"Nodi ({len(res)}):"]
    for n in res:
        icon = "🟢" if n.get("status") == "started" else "🔴"
        lines.append(f"  {icon} [{n['node_id'][:8]}...] {n['name']} ({n.get('node_type','?')})")
    return "\n".join(lines)

HANDLERS: dict[str, Any] = {
    "list_projects": tool_list_projects, "create_project": tool_create_project,
    "list_templates": tool_list_templates, "add_node": tool_add_node,
    "add_link": tool_add_link, "start_node": tool_start_node,
    "stop_node": tool_stop_node, "list_nodes": tool_list_nodes,
}

def _send(obj: Any) -> None: sys.stdout.write(json.dumps(obj) + "\n"); sys.stdout.flush()
def _error(rid: Any, c: int, m: str) -> None: _send({"jsonrpc":"2.0","id":rid,"error":{"code":c,"message":m}})
def _result(rid: Any, r: Any) -> None:         _send({"jsonrpc":"2.0","id":rid,"result":r})

def handle(req: dict) -> None:
    m, rid, p = req.get("method",""), req.get("id"), req.get("params",{}) or {}
    if m == "initialize":
        _result(rid, {"protocolVersion":"2024-11-05","capabilities":{"tools":{}},"serverInfo":{"name":"gns3-mcp","version":"1.0.0"}})
    elif m == "tools/list": _result(rid, {"tools": TOOLS})
    elif m == "tools/call":
        name, args = p.get("name",""), p.get("arguments",{}) or {}
        h = HANDLERS.get(name)
        if not h: _error(rid, -32601, f"Strumento '{name}' non trovato."); return
        try: text = h(args)
        except Exception as e: text = f"[Errore] {e}"
        _result(rid, {"content":[{"type":"text","text":text}],"isError":text.startswith("[Errore")})
    elif rid is not None: _result(rid, {})

def main() -> None:
    for line in sys.stdin:
        line = line.strip()
        if not line: continue
        try: req = json.loads(line)
        except json.JSONDecodeError as e:
            _send({"jsonrpc":"2.0","id":None,"error":{"code":-32700,"message":str(e)}})
            continue
        try: handle(req)
        except Exception as e:
            if req.get("id"): _error(req["id"], -32603, str(e))

if __name__ == "__main__": main()
