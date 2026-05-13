#!/usr/bin/env python3
"""
FreeCAD MCP — Prismalux
Crea modelli 3D parametrici, esporta STL/STEP via bridge HTTP (porta 9876).
Prerequisiti: copiare freecad_bridge.py in ~/.FreeCAD/Mod/PrismaluxBridge/ e riavviare FreeCAD.
"""
import sys, json
import urllib.request, urllib.error

FREECAD_BASE = "http://localhost:9876"

def _fc(endpoint, data=None):
    url = FREECAD_BASE + endpoint
    body = json.dumps(data).encode() if data is not None else None
    req = urllib.request.Request(url, data=body, method="POST" if data is not None else "GET",
        headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=15) as r:
            return json.loads(r.read()), None
    except urllib.error.HTTPError as e:
        return None, f"HTTP {e.code}: {e.read().decode()[:300]}"
    except Exception as e:
        return None, f"{e}\nInstalla freecad_bridge.py in FreeCAD e riavvia l'app."

TOOLS = [
    {"name": "execute_python",
     "description": "Esegue codice Python direttamente in FreeCAD via bridge.",
     "inputSchema": {"type": "object", "required": ["code"],
        "properties": {"code": {"type": "string", "description": "Codice Python FreeCAD (usa import FreeCAD, Part, etc.)"}}}},
    {"name": "create_box",
     "description": "Crea un parallelepipedo nel documento FreeCAD corrente.",
     "inputSchema": {"type": "object", "properties": {
        "length": {"type": "number", "default": 10.0},
        "width":  {"type": "number", "default": 10.0},
        "height": {"type": "number", "default": 10.0},
        "name":   {"type": "string", "default": "Box"}}}},
    {"name": "create_cylinder",
     "description": "Crea un cilindro nel documento FreeCAD corrente.",
     "inputSchema": {"type": "object", "properties": {
        "radius": {"type": "number", "default": 5.0},
        "height": {"type": "number", "default": 10.0},
        "name":   {"type": "string", "default": "Cylinder"}}}},
    {"name": "export_model",
     "description": "Esporta il documento corrente in STL, STEP o OBJ.",
     "inputSchema": {"type": "object", "required": ["output_path"],
        "properties": {
            "output_path": {"type": "string", "description": "Percorso output (.stl/.step/.obj)"},
            "object_name": {"type": "string", "description": "Nome oggetto specifico (default: tutti)"}}}},
    {"name": "list_objects",
     "description": "Elenca gli oggetti nel documento FreeCAD corrente.",
     "inputSchema": {"type": "object", "properties": {}}},
    {"name": "boolean_operation",
     "description": "Esegui operazione booleana tra due oggetti (fuse, cut, common).",
     "inputSchema": {"type": "object", "required": ["obj1", "obj2", "operation"],
        "properties": {
            "obj1":      {"type": "string"},
            "obj2":      {"type": "string"},
            "operation": {"type": "string", "enum": ["fuse","cut","common"]},
            "name":      {"type": "string", "default": "BooleanResult"}}}},
]

def _exec(code):
    res, err = _fc("/exec", {"code": code})
    if err: return f"[Errore FreeCAD bridge] {err}"
    return res.get("output", res.get("result", "OK"))

def tool_execute_python(args):
    return _exec(args["code"])

def tool_create_box(args):
    code = (f"import FreeCAD, Part\n"
            f"doc = FreeCAD.activeDocument() or FreeCAD.newDocument('Prismalux')\n"
            f"box = doc.addObject('Part::Box', '{args.get('name','Box')}')\n"
            f"box.Length = {args.get('length', 10.0)}\n"
            f"box.Width  = {args.get('width',  10.0)}\n"
            f"box.Height = {args.get('height', 10.0)}\n"
            f"doc.recompute()\n"
            f"print('Box creato: L={args.get('length',10)} W={args.get('width',10)} H={args.get('height',10)}')")
    return _exec(code)

def tool_create_cylinder(args):
    code = (f"import FreeCAD, Part\n"
            f"doc = FreeCAD.activeDocument() or FreeCAD.newDocument('Prismalux')\n"
            f"cyl = doc.addObject('Part::Cylinder', '{args.get('name','Cylinder')}')\n"
            f"cyl.Radius = {args.get('radius', 5.0)}\n"
            f"cyl.Height = {args.get('height', 10.0)}\n"
            f"doc.recompute()\n"
            f"print('Cilindro creato: R={args.get('radius',5)} H={args.get('height',10)}')")
    return _exec(code)

def tool_export_model(args):
    ext = args["output_path"].split(".")[-1].lower()
    fmt_map = {"stl": "Mesh", "step": "Part", "stp": "Part", "obj": "Mesh"}
    code = (f"import FreeCAD, Mesh, Part, ImportGui\n"
            f"doc = FreeCAD.activeDocument()\n"
            f"if doc is None: print('[Errore] Nessun documento aperto.')\n"
            f"else:\n"
            f"    objs = [doc.getObject('{args.get('object_name','')}') ] if '{args.get('object_name','')}' else doc.Objects\n"
            f"    objs = [o for o in objs if o]\n"
            f"    Mesh.export(objs, '{args['output_path']}') if '{ext}' in ('stl','obj') else Part.export(objs, '{args['output_path']}')\n"
            f"    print('Esportato: {args[\"output_path\"]}')")
    return _exec(code)

def tool_list_objects(_):
    code = ("import FreeCAD\n"
            "doc = FreeCAD.activeDocument()\n"
            "if not doc: print('Nessun documento aperto.')\n"
            "else:\n"
            "    for o in doc.Objects:\n"
            "        print(f'  [{o.Name}] {o.Label} ({o.TypeId})')")
    return _exec(code)

def tool_boolean_operation(args):
    op_map = {"fuse": "Part::Fuse", "cut": "Part::Cut", "common": "Part::Common"}
    code = (f"import FreeCAD, Part\n"
            f"doc = FreeCAD.activeDocument()\n"
            f"o1 = doc.getObject('{args['obj1']}')\n"
            f"o2 = doc.getObject('{args['obj2']}')\n"
            f"result = doc.addObject('{op_map[args['operation']]}', '{args.get('name','BooleanResult')}')\n"
            f"result.Base = o1; result.Tool = o2\n"
            f"o1.Visibility = False; o2.Visibility = False\n"
            f"doc.recompute()\n"
            f"print('Operazione {args[\"operation\"]} completata: {args.get(\"name\",\"BooleanResult\")}')")
    return _exec(code)

HANDLERS = {"execute_python": tool_execute_python, "create_box": tool_create_box,
            "create_cylinder": tool_create_cylinder, "export_model": tool_export_model,
            "list_objects": tool_list_objects, "boolean_operation": tool_boolean_operation}

def _send(obj): sys.stdout.write(json.dumps(obj) + "\n"); sys.stdout.flush()
def _error(rid, c, m): _send({"jsonrpc":"2.0","id":rid,"error":{"code":c,"message":m}})
def _result(rid, r):   _send({"jsonrpc":"2.0","id":rid,"result":r})

def handle(req):
    m, rid, p = req.get("method",""), req.get("id"), req.get("params",{}) or {}
    if m == "initialize":
        _result(rid, {"protocolVersion":"2024-11-05","capabilities":{"tools":{}},"serverInfo":{"name":"freecad-mcp","version":"1.0.0"}})
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
