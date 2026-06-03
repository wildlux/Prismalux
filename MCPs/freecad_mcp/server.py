#!/usr/bin/env python3
"""
FreeCAD MCP — Prismalux
Crea modelli 3D parametrici, esporta STL/STEP via bridge HTTP (porta 9876).
Prerequisiti: copiare freecad_bridge.py in ~/.FreeCAD/Mod/PrismaluxBridge/ e riavviare FreeCAD.
"""
import sys, json
import urllib.request, urllib.error

FREECAD_BASE = "http://localhost:9876"

import re as _re

def _safe_identifier(value: str, max_len: int = 64) -> str:
    """Valida un identificatore FreeCAD/Python: solo alfanumerico + underscore."""
    if not isinstance(value, str):
        raise ValueError(f"Valore non stringa: {value!r}")
    value = value.strip()
    if not value:
        raise ValueError("Identificatore vuoto.")
    if len(value) > max_len:
        raise ValueError(f"Identificatore troppo lungo (max {max_len}): {value!r}")
    if not _re.match(r'^[A-Za-z_][A-Za-z0-9_]*$', value):
        raise ValueError(f"Identificatore non valido (solo lettere/numeri/underscore): {value!r}")
    return value

def _safe_path_str(value: str, max_len: int = 512) -> str:
    """Valida un path file: niente quote singole/backslash che romperebbero l'f-string Python."""
    if not isinstance(value, str):
        raise ValueError(f"Path non stringa: {value!r}")
    value = value.strip()
    if not value:
        raise ValueError("Path vuoto.")
    if len(value) > max_len:
        raise ValueError(f"Path troppo lungo (max {max_len}).")
    # Caratteri che potrebbero fare code injection nelle f-string con quote singole
    for ch in ("'", "\\", "\n", "\r", "\0"):
        if ch in value:
            raise ValueError(f"Carattere non consentito nel path: {ch!r}")
    return value

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
    try:
        name = _safe_identifier(args.get("name", "Box"))
    except ValueError as e:
        return f"[Errore] {e}"
    length = float(args.get("length", 10.0))
    width  = float(args.get("width",  10.0))
    height = float(args.get("height", 10.0))
    code = (f"import FreeCAD, Part\n"
            f"doc = FreeCAD.activeDocument() or FreeCAD.newDocument('Prismalux')\n"
            f"box = doc.addObject('Part::Box', '{name}')\n"
            f"box.Length = {length}\n"
            f"box.Width  = {width}\n"
            f"box.Height = {height}\n"
            f"doc.recompute()\n"
            f"print('Box creato: L={length} W={width} H={height}')")
    return _exec(code)

def tool_create_cylinder(args):
    try:
        name = _safe_identifier(args.get("name", "Cylinder"))
    except ValueError as e:
        return f"[Errore] {e}"
    radius = float(args.get("radius", 5.0))
    height = float(args.get("height", 10.0))
    code = (f"import FreeCAD, Part\n"
            f"doc = FreeCAD.activeDocument() or FreeCAD.newDocument('Prismalux')\n"
            f"cyl = doc.addObject('Part::Cylinder', '{name}')\n"
            f"cyl.Radius = {radius}\n"
            f"cyl.Height = {height}\n"
            f"doc.recompute()\n"
            f"print('Cilindro creato: R={radius} H={height}')")
    return _exec(code)

def tool_export_model(args):
    try:
        out_path = _safe_path_str(args["output_path"])
    except ValueError as e:
        return f"[Errore] {e}"
    ext = out_path.split(".")[-1].lower()
    if ext not in ("stl", "step", "stp", "obj"):
        return "[Errore] Formato non supportato. Usa: .stl .step .stp .obj"
    obj_name = ""
    if args.get("object_name"):
        try:
            obj_name = _safe_identifier(args["object_name"])
        except ValueError as e:
            return f"[Errore] {e}"
    code = (f"import FreeCAD, Mesh, Part, ImportGui\n"
            f"doc = FreeCAD.activeDocument()\n"
            f"if doc is None: print('[Errore] Nessun documento aperto.')\n"
            f"else:\n"
            f"    objs = [doc.getObject('{obj_name}')] if '{obj_name}' else doc.Objects\n"
            f"    objs = [o for o in objs if o]\n"
            f"    Mesh.export(objs, '{out_path}') if '{ext}' in ('stl','obj') else Part.export(objs, '{out_path}')\n"
            f"    print('Esportato: {out_path}')")
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
    _op = args.get("operation", "")
    if _op not in op_map:
        return "[Errore] Operazione non valida. Usa: fuse, cut, common"
    try:
        obj1  = _safe_identifier(args["obj1"])
        obj2  = _safe_identifier(args["obj2"])
        bname = _safe_identifier(args.get("name", "BooleanResult"))
    except ValueError as e:
        return f"[Errore] {e}"
    code = (f"import FreeCAD, Part\n"
            f"doc = FreeCAD.activeDocument()\n"
            f"o1 = doc.getObject('{obj1}')\n"
            f"o2 = doc.getObject('{obj2}')\n"
            f"result = doc.addObject('{op_map[_op]}', '{bname}')\n"
            f"result.Base = o1; result.Tool = o2\n"
            f"o1.Visibility = False; o2.Visibility = False\n"
            f"doc.recompute()\n"
            f"print('Operazione {_op} completata: {bname}')")
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
