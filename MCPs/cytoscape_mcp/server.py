#!/usr/bin/env python3
"""
Cytoscape MCP — Prismalux
Crea e gestisce reti biologiche via CyREST API nativa Cytoscape (porta 1234).
Prerequisiti: Cytoscape desktop in esecuzione.
"""
import sys, json
import urllib.request, urllib.error

CY_BASE = "http://localhost:1234/v1"

def _cy(method, path, data=None):
    url = CY_BASE + path
    body = json.dumps(data).encode() if data is not None else None
    req = urllib.request.Request(url, data=body, method=method,
        headers={"Content-Type": "application/json", "Accept": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=10) as r:
            return json.loads(r.read()), None
    except urllib.error.HTTPError as e:
        return None, f"HTTP {e.code}: {e.read().decode()[:300]}"
    except Exception as e:
        return None, f"{e}\nAssicurati che Cytoscape sia aperto con CyREST attivo (porta 1234)."

TOOLS = [
    {"name": "list_networks",
     "description": "Elenca le reti aperte in Cytoscape.",
     "inputSchema": {"type": "object", "properties": {}}},
    {"name": "create_network",
     "description": "Crea una nuova rete biologica con nodi e archi.",
     "inputSchema": {"type": "object", "required": ["name", "nodes", "edges"],
        "properties": {
            "name":  {"type": "string"},
            "nodes": {"type": "array", "items": {"type": "object"},
                      "description": "[{'id':'A','label':'Gene A','type':'gene'}, ...]"},
            "edges": {"type": "array", "items": {"type": "object"},
                      "description": "[{'source':'A','target':'B','interaction':'activates'}, ...]"}}}},
    {"name": "add_nodes",
     "description": "Aggiunge nodi a una rete esistente.",
     "inputSchema": {"type": "object", "required": ["network_id", "nodes"],
        "properties": {
            "network_id": {"type": "integer"},
            "nodes": {"type": "array", "items": {"type": "object"}}}}},
    {"name": "apply_layout",
     "description": "Applica un algoritmo di layout alla rete.",
     "inputSchema": {"type": "object", "required": ["network_id", "algorithm"],
        "properties": {
            "network_id": {"type": "integer"},
            "algorithm":  {"type": "string", "enum": ["force-directed","circular","hierarchical","grid","kamada-kawai"], "default": "force-directed"}}}},
    {"name": "export_network",
     "description": "Esporta la rete come immagine PNG o file SIF/XGMML.",
     "inputSchema": {"type": "object", "required": ["network_id", "output_path"],
        "properties": {
            "network_id":  {"type": "integer"},
            "output_path": {"type": "string"},
            "format":      {"type": "string", "enum": ["png","svg","pdf","sif","xgmml"], "default": "png"}}}},
    {"name": "get_network_info",
     "description": "Restituisce statistiche di una rete (nodi, archi, componenti).",
     "inputSchema": {"type": "object", "required": ["network_id"],
        "properties": {"network_id": {"type": "integer"}}}},
]

def tool_list_networks(_):
    res, err = _cy("GET", "/networks")
    if err: return f"[Errore CyREST] {err}"
    if not res: return "Nessuna rete aperta in Cytoscape."
    lines = [f"Reti aperte ({len(res)}):"]
    for nid in res[:20]:
        info, _ = _cy("GET", f"/networks/{nid}")
        name = info.get("data", {}).get("name", "?") if info else "?"
        lines.append(f"  [{nid}] {name}")
    return "\n".join(lines)

def tool_create_network(args):
    nodes = [{"data": {"id": n.get("id", str(i)), "name": n.get("label", n.get("id", str(i))),
                       **{k:v for k,v in n.items() if k not in ("id","label")}}}
             for i, n in enumerate(args["nodes"])]
    edges = [{"data": {"source": e["source"], "target": e["target"],
                       "interaction": e.get("interaction", "interacts"), **{k:v for k,v in e.items() if k not in ("source","target","interaction")}}}
             for e in args["edges"]]
    payload = {"data": {"name": args["name"]}, "elements": {"nodes": nodes, "edges": edges}}
    res, err = _cy("POST", "/networks?collection=Prismalux", data=payload)
    if err: return f"[Errore] {err}"
    nid = res.get("networkSUID") if res else "?"
    return f"Rete '{args['name']}' creata (SUID={nid}).\n  {len(nodes)} nodi, {len(edges)} archi."

def tool_add_nodes(args):
    nodes = [{"data": {"id": n.get("id", str(i)), "name": n.get("label", n.get("id", str(i)))}}
             for i, n in enumerate(args["nodes"])]
    res, err = _cy("POST", f"/networks/{args['network_id']}/nodes", data=nodes)
    if err: return f"[Errore] {err}"
    return f"Aggiunti {len(nodes)} nodi alla rete {args['network_id']}."

def tool_apply_layout(args):
    algo_map = {"force-directed": "force-directed", "circular": "circular",
                "hierarchical": "hierarchical", "grid": "grid", "kamada-kawai": "kamada-kawai"}
    algo = algo_map.get(args["algorithm"], "force-directed")
    _, err = _cy("GET", f"/apply/layouts/{algo}/{args['network_id']}")
    if err: return f"[Errore] {err}"
    return f"Layout '{args['algorithm']}' applicato alla rete {args['network_id']}."

def tool_export_network(args):
    fmt = args.get("format", "png").upper()
    if fmt in ("PNG", "SVG", "PDF"):
        payload = {"outputFormat": fmt, "fileName": args["output_path"]}
        _, err = _cy("POST", f"/networks/{args['network_id']}/views/first.{fmt.lower()}", data=payload)
    else:
        payload = {"options": [{"name": "exportFormat", "value": fmt}], "OutputFile": args["output_path"]}
        _, err = _cy("POST", f"/commands/network/export", data=payload)
    if err: return f"[Errore export] {err}"
    return f"Rete {args['network_id']} esportata: {args['output_path']}"

def tool_get_network_info(args):
    res, err = _cy("GET", f"/networks/{args['network_id']}")
    if err: return f"[Errore] {err}"
    data = res.get("data", {}) if res else {}
    nodes, _ = _cy("GET", f"/networks/{args['network_id']}/nodes/count")
    edges, _ = _cy("GET", f"/networks/{args['network_id']}/edges/count")
    nc = nodes.get("count", "?") if nodes else "?"
    ec = edges.get("count", "?") if edges else "?"
    return (f"Rete [{args['network_id']}]: {data.get('name','?')}\n"
            f"  Nodi: {nc} | Archi: {ec}")

HANDLERS = {"list_networks": tool_list_networks, "create_network": tool_create_network,
            "add_nodes": tool_add_nodes, "apply_layout": tool_apply_layout,
            "export_network": tool_export_network, "get_network_info": tool_get_network_info}

def _send(obj): sys.stdout.write(json.dumps(obj) + "\n"); sys.stdout.flush()
def _error(rid, c, m): _send({"jsonrpc":"2.0","id":rid,"error":{"code":c,"message":m}})
def _result(rid, r):   _send({"jsonrpc":"2.0","id":rid,"result":r})

def handle(req):
    m, rid, p = req.get("method",""), req.get("id"), req.get("params",{}) or {}
    if m == "initialize":
        _result(rid, {"protocolVersion":"2024-11-05","capabilities":{"tools":{}},"serverInfo":{"name":"cytoscape-mcp","version":"1.0.0"}})
    elif m == "tools/list": _result(rid, {"tools": TOOLS})
    elif m == "tools/call":
        name, args = p.get("name",""), p.get("arguments",{}) or {}
        h = HANDLERS.get(name)
        if not h: _error(rid, -32601, f"Strumento '{name}' non trovato."); return
        try: text = h(args)
        except Exception as e: text = f"[Errore] {e}"
        _result(rid, {"content":[{"type":"text","text":text}],"isError":text.startswith("[Errore")})
    elif rid is not None: _result(rid, {})

def main():
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
