#!/usr/bin/env python3
"""
Godot MCP — Prismalux
Crea scene Godot, aggiunge nodi, esegue GDScript via bridge HTTP (porta 9080).
Prerequisiti: aggiungere godot_bridge.gd come AutoLoad nel progetto Godot.
"""
import sys, json
import urllib.request, urllib.error

GODOT_BASE = "http://localhost:9080"

def _gd(endpoint, data=None):
    url = GODOT_BASE + endpoint
    body = json.dumps(data).encode() if data is not None else None
    req = urllib.request.Request(url, data=body, method="POST" if data is not None else "GET",
        headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=10) as r:
            return json.loads(r.read()), None
    except urllib.error.HTTPError as e:
        return None, f"HTTP {e.code}: {e.read().decode()[:300]}"
    except Exception as e:
        return None, f"{e}\nAvvia Godot con godot_bridge.gd come AutoLoad (porta 9080)."

TOOLS = [
    {"name": "execute_gdscript",
     "description": "Esegue codice GDScript direttamente nel progetto Godot in esecuzione.",
     "inputSchema": {"type": "object", "required": ["code"],
        "properties": {"code": {"type": "string", "description": "Codice GDScript valido"}}}},
    {"name": "create_node",
     "description": "Aggiunge un nodo alla scena corrente.",
     "inputSchema": {"type": "object", "required": ["node_type", "name"],
        "properties": {
            "node_type": {"type": "string", "description": "Tipo nodo (es. 'Node3D','MeshInstance3D','Camera3D','RigidBody3D')"},
            "name":      {"type": "string"},
            "parent":    {"type": "string", "default": ".", "description": "Path del nodo padre (default: root)"}}}},
    {"name": "list_nodes",
     "description": "Elenca i nodi della scena corrente.",
     "inputSchema": {"type": "object", "properties": {
        "root": {"type": "string", "default": ".", "description": "Nodo radice da cui elencare"}}}},
    {"name": "set_property",
     "description": "Imposta una proprietà su un nodo della scena.",
     "inputSchema": {"type": "object", "required": ["node_path", "property", "value"],
        "properties": {
            "node_path": {"type": "string", "description": "Path del nodo (es. '/root/Main/Player')"},
            "property":  {"type": "string", "description": "Nome proprietà (es. 'position', 'scale', 'visible')"},
            "value":     {"description": "Valore da impostare (stringa, numero, array)"}}}},
    {"name": "save_scene",
     "description": "Salva la scena corrente su disco.",
     "inputSchema": {"type": "object", "properties": {
        "path": {"type": "string", "description": "Percorso .tscn (default: scena corrente)"}}}},
    {"name": "get_scene_info",
     "description": "Informazioni sulla scena corrente (nodi, frame rate, uptime).",
     "inputSchema": {"type": "object", "properties": {}}},
]

def tool_execute_gdscript(args):
    res, err = _gd("/exec", {"code": args["code"]})
    if err: return f"[Errore Godot bridge] {err}"
    return res.get("output", res.get("result", "OK"))

def tool_create_node(args):
    code = (f"var parent = get_node('{args.get('parent','.')}')\n"
            f"var node = {args['node_type']}.new()\n"
            f"node.name = '{args['name']}'\n"
            f"parent.add_child(node)\n"
            f"node.owner = get_tree().edited_scene_root\n"
            f"print('Nodo creato: {args[\"name\"]} ({args[\"node_type\"]})')")
    res, err = _gd("/exec", {"code": code})
    if err: return f"[Errore] {err}"
    return res.get("output", f"Nodo '{args['name']}' ({args['node_type']}) aggiunto.")

def tool_list_nodes(args):
    code = (f"func _list(node, depth):\n"
            f"    print('  ' * depth + node.name + ' [' + node.get_class() + ']')\n"
            f"    for c in node.get_children(): _list(c, depth+1)\n"
            f"_list(get_node('{args.get('root','.')}'), 0)")
    res, err = _gd("/exec", {"code": code})
    if err: return f"[Errore] {err}"
    return res.get("output", "OK")

def tool_set_property(args):
    val = args["value"]
    val_str = f'"{val}"' if isinstance(val, str) else str(val)
    code = (f"var node = get_node('{args['node_path']}')\n"
            f"node.set('{args['property']}', {val_str})\n"
            f"print('{args[\"node_path\"]}.{args[\"property\"]} = {val_str}')")
    res, err = _gd("/exec", {"code": code})
    if err: return f"[Errore] {err}"
    return res.get("output", "OK")

def tool_save_scene(args):
    path = args.get("path", "")
    code = (f"var scene = get_tree().edited_scene_root\n"
            f"var ps = PackedScene.new()\nps.pack(scene)\n"
            f"var path = '{path}' if '{path}' else scene.scene_file_path\n"
            f"ResourceSaver.save(ps, path)\nprint('Scena salvata: ' + path)")
    res, err = _gd("/exec", {"code": code})
    if err: return f"[Errore] {err}"
    return res.get("output", "OK")

def tool_get_scene_info(_):
    res, err = _gd("/info")
    if err: return f"[Errore] {err}"
    return json.dumps(res, indent=2)

HANDLERS = {"execute_gdscript": tool_execute_gdscript, "create_node": tool_create_node,
            "list_nodes": tool_list_nodes, "set_property": tool_set_property,
            "save_scene": tool_save_scene, "get_scene_info": tool_get_scene_info}

def _send(obj): sys.stdout.write(json.dumps(obj) + "\n"); sys.stdout.flush()
def _error(rid, c, m): _send({"jsonrpc":"2.0","id":rid,"error":{"code":c,"message":m}})
def _result(rid, r):   _send({"jsonrpc":"2.0","id":rid,"result":r})

def handle(req):
    m, rid, p = req.get("method",""), req.get("id"), req.get("params",{}) or {}
    if m == "initialize":
        _result(rid, {"protocolVersion":"2024-11-05","capabilities":{"tools":{}},"serverInfo":{"name":"godot-mcp","version":"1.0.0"}})
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
