#!/usr/bin/env python3
"""
Godot MCP — Prismalux
Crea scene Godot, aggiunge nodi, esegue GDScript via bridge HTTP (porta 9080).
Prerequisiti: aggiungere godot_bridge.gd come AutoLoad nel progetto Godot.
"""
import sys, json
import urllib.request, urllib.error

GODOT_BASE = "http://localhost:9080"

import re as _re

def _safe_godot_str(value: str, max_len: int = 256, label: str = "valore") -> str:
    """Valida una stringa usata dentro quote singole GDScript, bloccando code injection."""
    if not isinstance(value, str):
        raise ValueError(f"{label} non è una stringa.")
    value = value.strip()
    if not value:
        raise ValueError(f"{label} vuoto.")
    if len(value) > max_len:
        raise ValueError(f"{label} troppo lungo (max {max_len}).")
    for ch in ("'", "\\", "\n", "\r", "\0"):
        if ch in value:
            raise ValueError(f"Carattere non consentito in {label}: {ch!r}")
    return value

_GODOT_NODE_TYPES = {
    "Node", "Node2D", "Node3D", "Sprite2D", "Sprite3D",
    "MeshInstance3D", "Camera3D", "Camera2D", "RigidBody3D", "RigidBody2D",
    "CharacterBody3D", "CharacterBody2D", "StaticBody3D", "StaticBody2D",
    "Area3D", "Area2D", "CollisionShape3D", "CollisionShape2D",
    "DirectionalLight3D", "SpotLight3D", "OmniLight3D",
    "Label", "Button", "LineEdit", "TextEdit", "Panel", "Control",
    "AudioStreamPlayer", "AudioStreamPlayer2D", "AudioStreamPlayer3D",
    "AnimationPlayer", "AnimationTree", "Skeleton3D",
    "CSGBox3D", "CSGSphere3D", "CSGCylinder3D", "CSGCombiner3D",
    "Path3D", "PathFollow3D", "Timer", "Tween",
    "VBoxContainer", "HBoxContainer", "GridContainer",
}

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
    try:
        node_type = args.get("node_type", "")
        if node_type not in _GODOT_NODE_TYPES:
            return (f"[Errore] Tipo nodo '{node_type}' non consentito. "
                    f"Tipi validi: {', '.join(sorted(_GODOT_NODE_TYPES))}")
        name   = _safe_godot_str(args["name"], label="name")
        parent = _safe_godot_str(args.get("parent", "."), label="parent")
    except ValueError as e:
        return f"[Errore] {e}"
    code = (f"var parent = get_node('{parent}')\n"
            f"var node = {node_type}.new()\n"
            f"node.name = '{name}'\n"
            f"parent.add_child(node)\n"
            f"node.owner = get_tree().edited_scene_root\n"
            f"print('Nodo creato: {name} ({node_type})')")
    res, err = _gd("/exec", {"code": code})
    if err: return f"[Errore] {err}"
    return res.get("output", f"Nodo '{name}' ({node_type}) aggiunto.")

def tool_list_nodes(args):
    try:
        root = _safe_godot_str(args.get("root", "."), label="root")
    except ValueError as e:
        return f"[Errore] {e}"
    code = (f"func _list(node, depth):\n"
            f"    print('  ' * depth + node.name + ' [' + node.get_class() + ']')\n"
            f"    for c in node.get_children(): _list(c, depth+1)\n"
            f"_list(get_node('{root}'), 0)")
    res, err = _gd("/exec", {"code": code})
    if err: return f"[Errore] {err}"
    return res.get("output", "OK")

def tool_set_property(args):
    try:
        node_path = _safe_godot_str(args["node_path"], max_len=256, label="node_path")
        prop      = _safe_godot_str(args["property"], max_len=128, label="property")
    except ValueError as e:
        return f"[Errore] {e}"
    val = args["value"]
    # Serializza il valore in modo sicuro senza espansione shell
    if isinstance(val, str):
        # Valida la stringa per evitare injection nel GDScript
        try:
            val_safe = _safe_godot_str(val, max_len=512, label="value")
            val_str  = f'"{val_safe}"'
        except ValueError as e:
            return f"[Errore] {e}"
    elif isinstance(val, bool):
        val_str = "true" if val else "false"
    elif isinstance(val, (int, float)):
        val_str = str(val)
    elif isinstance(val, list) and all(isinstance(x, (int, float)) for x in val):
        val_str = str(val)  # lista di numeri — sicura
    else:
        return "[Errore] Tipo valore non supportato. Usa: stringa, numero, booleano, array di numeri."
    code = (f"var node = get_node('{node_path}')\n"
            f"node.set('{prop}', {val_str})\n"
            f"print('{node_path}.{prop} = {val_str}')")
    res, err = _gd("/exec", {"code": code})
    if err: return f"[Errore] {err}"
    return res.get("output", "OK")

def tool_save_scene(args):
    path = args.get("path", "")
    if path:
        try:
            path = _safe_godot_str(path, max_len=512, label="path")
        except ValueError as e:
            return f"[Errore] {e}"
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
